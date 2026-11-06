#include "url_internal.h"
#include <curl/curl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
#include <time.h>

struct mc_java_http_input {
    size_t holds, reads, source_closes, disconnects;
    uint64_t received, consumed;
    mc_java_url *original, *current, *pending;
    mc_java_http_options options;
    const atomic_bool *stop;
    CURL *easy;
    CURLM *multi;
    struct curl_slist *request_headers;
    bool attached, global_hold, source_open, completed, active, paused, header_done;
    bool connection_ready, disconnected;
    long response;
    unsigned int redirects;
    uint8_t *buffer;
    size_t begin, end;
    char location[65537];
    size_t location_length;
    mc_java_url_error failure;
};

static once_flag global_once = ONCE_FLAG_INIT;
static mtx_t global_mutex;
static bool global_mutex_ready, global_quarantine;
static size_t global_holds;

static void global_setup(void) {
    global_mutex_ready = mtx_init(&global_mutex, mtx_plain) == thrd_success;
}

static mc_java_url_status global_take(mc_java_http_input *s, mc_java_url_error *e) {
    call_once(&global_once, global_setup);
    if (!global_mutex_ready || mtx_lock(&global_mutex) != thrd_success)
        return mc_url_fail(e, MC_JAVA_URL_NATIVE, "curl global native lock", 0);
    mc_java_url_status result = MC_JAVA_URL_OK;
    if (global_quarantine)
        result = mc_url_fail(e, MC_JAVA_URL_NATIVE, "curl global quarantine", 0);
    else if (global_holds == SIZE_MAX)
        result = mc_url_fail(e, MC_JAVA_URL_LIMIT, "curl global holds", 0);
    else if (global_holds == 0 && curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK)
        result = mc_url_fail(e, MC_JAVA_URL_NATIVE, "curl global initialization", 0);
    else {
        ++global_holds;
        s->global_hold = true;
    }
    if (mtx_unlock(&global_mutex) != thrd_success) {
        global_quarantine = true;
        return mc_url_fail(e, MC_JAVA_URL_NATIVE, "curl global unlock quarantine", 0);
    }
    return result;
}

static mc_java_url_status global_drop(mc_java_http_input *s, mc_java_url_error *e) {
    if (!s->global_hold)
        return MC_JAVA_URL_OK;
    if (mtx_lock(&global_mutex) != thrd_success)
        return mc_url_fail(e, MC_JAVA_URL_NATIVE, "curl final global lock", 0);
    if (global_quarantine) {
        mtx_unlock(&global_mutex);
        return mc_url_fail(e, MC_JAVA_URL_NATIVE, "curl global quarantine", 0);
    }
    --global_holds;
    s->global_hold = false;
    if (global_holds == 0)
        curl_global_cleanup();
    if (mtx_unlock(&global_mutex) != thrd_success) {
        global_quarantine = true;
        return mc_url_fail(e, MC_JAVA_URL_NATIVE, "curl final global unlock", 0);
    }
    return MC_JAVA_URL_OK;
}

mc_java_http_options mc_java_http_default_options(void) {
    return (mc_java_http_options){65536u, UINT64_C(268435456), 30000u};
}

static mc_java_url_status remember(mc_java_http_input *s, mc_java_url_error *e,
                                   mc_java_url_status status, const char *operation, int code) {
    mc_url_fail(e, status, operation, code);
    if (e != NULL) {
        e->response_code = s->response;
        s->failure = *e;
    }
    return status;
}

static size_t body(char *bytes, size_t size, size_t count, void *context) {
    mc_java_http_input *s = context;
    if (size != 1u || count > s->options.max_buffer_bytes)
        return 0;
    if (s->received > s->options.max_body_bytes ||
        (uint64_t)count > s->options.max_body_bytes - s->received) {
        remember(s, &s->failure, MC_JAVA_URL_LIMIT, "HTTP received-body bound", 0);
        return 0;
    }
    if (s->end - s->begin > 0 && count > s->options.max_buffer_bytes - s->end) {
        s->paused = true;
        return CURL_WRITEFUNC_PAUSE;
    }
    if (s->end == s->begin)
        s->begin = s->end = 0;
    if (count > s->options.max_buffer_bytes - s->end) {
        s->paused = true;
        return CURL_WRITEFUNC_PAUSE;
    }
    if (count != 0)
        memcpy(s->buffer + s->end, bytes, count);
    s->end += count;
    s->received += (uint64_t)count;
    return count;
}

static bool equal_header(const char *p, size_t n, const char *name) {
    size_t length = strlen(name);
    if (n < length)
        return false;
    for (size_t i = 0; i < length; ++i) {
        char a = p[i], b = name[i];
        if (a >= 'A' && a <= 'Z')
            a = (char)(a + ('a' - 'A'));
        if (a != b)
            return false;
    }
    return true;
}

static size_t header(char *bytes, size_t size, size_t count, void *context) {
    mc_java_http_input *s = context;
    if (size != 1u || count > 65536u)
        return 0;
    if (count >= 12u && memcmp(bytes, "HTTP/", 5u) == 0) {
        const char *space = memchr(bytes, ' ', count);
        if (space != NULL && (size_t)(bytes + count - space) >= 4u && space[1] >= '0' &&
            space[1] <= '9' && space[2] >= '0' && space[2] <= '9' && space[3] >= '0' &&
            space[3] <= '9') {
            s->response = (long)(space[1] - '0') * 100L + (long)(space[2] - '0') * 10L +
                          (long)(space[3] - '0');
            s->header_done = false;
            s->location_length = 0;
        }
    } else if (count == 2u && bytes[0] == '\r' && bytes[1] == '\n') {
        if (s->response >= 200L)
            s->header_done = true;
    } else if (equal_header(bytes, count, "location:")) {
        size_t first = 9u, last = count;
        while (first < last && (bytes[first] == ' ' || bytes[first] == '\t'))
            ++first;
        while (last > first && (bytes[last - 1u] == '\r' || bytes[last - 1u] == '\n' ||
                                bytes[last - 1u] == ' ' || bytes[last - 1u] == '\t'))
            --last;
        if (last - first > 65536u)
            return 0;
        memcpy(s->location, bytes + first, last - first);
        s->location_length = last - first;
        s->location[s->location_length] = 0;
    }
    return count;
}

static mc_java_url_status transfer_cleanup(mc_java_http_input *s, mc_java_url_error *e) {
    if (s->attached) {
        CURLMcode c = curl_multi_remove_handle(s->multi, s->easy);
        if (c != CURLM_OK)
            return remember(s, e, MC_JAVA_URL_NATIVE, "curl remove issued handle", (int)c);
        s->attached = false;
    }
    if (s->easy != NULL) {
        curl_easy_cleanup(s->easy);
        s->easy = NULL;
    }
    if (s->multi != NULL) {
        CURLMcode c = curl_multi_cleanup(s->multi);
        if (c != CURLM_OK)
            return remember(s, e, MC_JAVA_URL_NATIVE, "curl multi cleanup", (int)c);
        s->multi = NULL;
    }
    curl_slist_free_all(s->request_headers);
    s->request_headers = NULL;
    return MC_JAVA_URL_OK;
}

static mc_java_url_status begin_transfer(mc_java_http_input *s, mc_java_url_error *e) {
    s->easy = curl_easy_init();
    s->multi = curl_multi_init();
    if (s->easy == NULL || s->multi == NULL)
        return remember(s, e, MC_JAVA_URL_ALLOCATION, "curl transfer owners", 0);
    s->response = 0;
    s->header_done = false;
    s->location_length = 0;
    s->completed = false;
    s->paused = false;
    s->begin = s->end = 0;
    struct curl_slist *headers =
        curl_slist_append(NULL, "Accept: text/html, image/gif, image/jpeg, */*; q=0.2");
    if (headers == NULL)
        return remember(s, e, MC_JAVA_URL_ALLOCATION, "HTTP request header", 0);
    s->request_headers = headers;
#define SET(option, value)                                                                         \
    do {                                                                                           \
        CURLcode c = curl_easy_setopt(s->easy, option, value);                                     \
        if (c != CURLE_OK)                                                                         \
            return remember(s, e, MC_JAVA_URL_NATIVE, "curl explicit HTTP profile", (int)c);       \
    } while (0)
    SET(CURLOPT_URL, s->current->request);
    SET(CURLOPT_PROXY, "");
    SET(CURLOPT_FOLLOWLOCATION, 0L);
    SET(CURLOPT_NOSIGNAL, 1L);
    SET(CURLOPT_TIMEOUT_MS, (long)s->options.native_operation_millis);
    SET(CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
    SET(CURLOPT_PROTOCOLS_STR, "http");
    SET(CURLOPT_NETRC, CURL_NETRC_IGNORED);
    SET(CURLOPT_USERAGENT, "Java/17.0.17");
    SET(CURLOPT_HTTPHEADER, s->request_headers);
    SET(CURLOPT_HTTP_CONTENT_DECODING, 0L);
    SET(CURLOPT_WRITEFUNCTION, body);
    SET(CURLOPT_WRITEDATA, s);
    SET(CURLOPT_HEADERFUNCTION, header);
    SET(CURLOPT_HEADERDATA, s);
#undef SET
    CURLMcode added = curl_multi_add_handle(s->multi, s->easy);
    if (added != CURLM_OK)
        return remember(s, e, MC_JAVA_URL_NATIVE, "curl add real HTTP handle", (int)added);
    s->attached = true;
    return MC_JAVA_URL_OK;
}

static mc_java_url_status drive(mc_java_http_input *s, bool opening, mc_java_url_error *e) {
    for (;;) {
        if (s->stop != NULL && atomic_load_explicit(s->stop, memory_order_acquire))
            return remember(s, e, MC_JAVA_URL_NATIVE_CANCELLED, "HTTP native retirement stop", 0);
        if (s->paused && s->begin == s->end) {
            s->paused = false;
            CURLcode c = curl_easy_pause(s->easy, CURLPAUSE_CONT);
            if (c != CURLE_OK)
                return remember(s, e, MC_JAVA_URL_NATIVE, "curl unpause real stream", (int)c);
        }
        int running = 0;
        CURLMcode c = curl_multi_perform(s->multi, &running);
        if (c != CURLM_OK)
            return remember(s, e, MC_JAVA_URL_NATIVE, "curl multi perform", (int)c);
        int messages = 0;
        CURLMsg *message;
        while ((message = curl_multi_info_read(s->multi, &messages)) != NULL)
            if (message->msg == CURLMSG_DONE) {
                s->completed = true;
                if (message->data.result != CURLE_OK && s->failure.status == MC_JAVA_URL_OK)
                    remember(s, &s->failure,
                             message->data.result == CURLE_OUT_OF_MEMORY ? MC_JAVA_URL_ALLOCATION
                             : message->data.result == CURLE_OPERATION_TIMEDOUT
                                 ? MC_JAVA_URL_LIMIT
                                 : MC_JAVA_URL_SOURCE_IO,
                             "actual HTTP transfer", (int)message->data.result);
            }
        if (opening && s->header_done)
            return MC_JAVA_URL_OK;
        if (!opening && s->end > s->begin)
            return MC_JAVA_URL_OK;
        if (s->failure.status != MC_JAVA_URL_OK) {
            if (e != NULL)
                *e = s->failure;
            return s->failure.status;
        }
        if (s->completed)
            return MC_JAVA_URL_OK;
        int descriptors = 0;
        c = curl_multi_poll(s->multi, NULL, 0, 100, &descriptors);
        if (c != CURLM_OK)
            return remember(s, e, MC_JAVA_URL_NATIVE, "curl actual socket wait", (int)c);
    }
}

mc_java_url_status mc_java_http_connect(mc_java_url *url, const mc_java_http_options *options,
                                        const atomic_bool *stop, mc_java_http_input **out,
                                        mc_java_url_error *e) {
    if (e == NULL)
        return MC_JAVA_URL_ARGUMENT;
    if (out == NULL || url == NULL)
        return mc_url_fail(e, MC_JAVA_URL_ARGUMENT, "HTTP open native arguments", 0);
    if (url->phase != 2)
        return mc_url_fail(e, MC_JAVA_URL_STATE, "URL.openStream RAW/failed receiver", 0);
    if (!url->http)
        return mc_url_fail(e, MC_JAVA_URL_PROVIDER, "URL.openStream protocol provider", 0);
    mc_java_http_options o = options == NULL ? mc_java_http_default_options() : *options;
    if (o.max_buffer_bytes < CURL_MAX_WRITE_SIZE || o.max_buffer_bytes > 1048576u ||
        o.native_operation_millis == 0 || o.native_operation_millis > (unsigned int)INT_MAX)
        return mc_url_fail(e, MC_JAVA_URL_ARGUMENT, "HTTP native work bounds", 0);
    mc_java_http_input *s = calloc(1, sizeof(*s));
    if (s == NULL)
        return mc_url_fail(e, MC_JAVA_URL_ALLOCATION, "HTTP input owner", 0);
    s->holds = 1;
    s->options = o;
    s->stop = stop;
    *out = s;
    if (mc_java_url_retain(url, e) != MC_JAVA_URL_OK)
        return e->status;
    s->original = url;
    if (mc_java_url_retain(url, e) != MC_JAVA_URL_OK)
        return e->status;
    s->current = url;
    s->buffer = malloc(o.max_buffer_bytes);
    if (s->buffer == NULL)
        return remember(s, e, MC_JAVA_URL_ALLOCATION, "HTTP read buffer", 0);
    if (global_take(s, e) != MC_JAVA_URL_OK)
        return e->status;
    for (;;) {
        if (begin_transfer(s, e) != MC_JAVA_URL_OK || drive(s, true, e) != MC_JAVA_URL_OK)
            return e->status;
        if (s->response == 305L)
            return remember(s, e, MC_JAVA_URL_PROVIDER, "JDK HTTP305 proxy redirect", 0);
        bool redirect = s->response >= 300L && s->response <= 307L && s->response != 304L &&
                        s->response != 306L && s->location_length != 0;
        if (!redirect)
            break;
        uint16_t *units = malloc(s->location_length * sizeof(*units));
        if (units == NULL)
            return remember(s, e, MC_JAVA_URL_ALLOCATION, "HTTP Location String", 0);
        for (size_t i = 0; i < s->location_length; ++i)
            units[i] = (unsigned char)s->location[i];
        mc_world_info_name *name = NULL;
        mc_world_info_status ns =
            mc_world_info_name_copy((mc_nbt_string){units, s->location_length}, SIZE_MAX, &name);
        free(units);
        if (ns != MC_WORLD_INFO_OK)
            return remember(s, e, MC_JAVA_URL_ALLOCATION, "HTTP Location source text", 0);
        mc_java_url *next = NULL;
        mc_java_url_status status = mc_java_url_resolve(s->current, name, &next, e);
        s->pending = next;
        mc_world_info_name_release(name);
        if (status != MC_JAVA_URL_OK)
            return status;
        if (!next->http) {
            mc_java_url_release(&s->pending, NULL);
            next = NULL;
            break;
        }
        if (++s->redirects >= 20u) {
            mc_java_url_release(&s->pending, NULL);
            next = NULL;
            return remember(s, e, MC_JAVA_URL_SOURCE_IO, "actual JDK HTTP redirect limit", 0);
        }
        if (transfer_cleanup(s, e) != MC_JAVA_URL_OK) {
            mc_java_url_release(&s->pending, NULL);
            next = NULL;
            return e->status;
        }
        mc_java_url_release(&s->current, NULL);
        s->current = next;
        s->pending = NULL;
    }
    s->connection_ready = true;
    return mc_url_fail(e, MC_JAVA_URL_OK, NULL, 0);
}

mc_java_url_status mc_java_http_response_code(mc_java_http_input *s, long *out,
                                              mc_java_url_error *e) {
    if (e == NULL)
        return MC_JAVA_URL_ARGUMENT;
    if (s == NULL)
        return mc_url_fail(e, MC_JAVA_URL_SOURCE_NULL, "HTTP response receiver", 0);
    if (out == NULL)
        return mc_url_fail(e, MC_JAVA_URL_ARGUMENT, "HTTP response output", 0);
    if (s->active)
        return mc_url_fail(e, MC_JAVA_URL_BUSY, "HTTP response reentry", 0);
    if (!s->connection_ready || s->disconnected)
        return mc_url_fail(e, MC_JAVA_URL_STATE, "HTTP response connection state", 0);
    *out = s->response;
    return mc_url_fail(e, MC_JAVA_URL_OK, NULL, 0);
}

mc_java_url_status mc_java_http_get_input(mc_java_http_input *s, mc_java_url_error *e) {
    if (e == NULL)
        return MC_JAVA_URL_ARGUMENT;
    if (s == NULL)
        return mc_url_fail(e, MC_JAVA_URL_SOURCE_NULL, "HTTP input receiver", 0);
    if (s->active)
        return mc_url_fail(e, MC_JAVA_URL_BUSY, "HTTP input acknowledgement reentry", 0);
    if (!s->connection_ready || s->disconnected)
        return mc_url_fail(e, MC_JAVA_URL_STATE, "HTTP input connection state", 0);
    if (s->response >= 400L)
        return remember(s, e,
                        (s->response == 404L || s->response == 410L) ? MC_JAVA_URL_SOURCE_NOT_FOUND
                                                                     : MC_JAVA_URL_SOURCE_IO,
                        "actual HTTP response error", 0);
    s->source_open = true;
    return mc_url_fail(e, MC_JAVA_URL_OK, NULL, 0);
}

mc_java_url_status mc_java_http_open(mc_java_url *url, const mc_java_http_options *options,
                                     const atomic_bool *stop, mc_java_http_input **out,
                                     mc_java_url_error *e) {
    const mc_java_url_status status = mc_java_http_connect(url, options, stop, out, e);
    return status == MC_JAVA_URL_OK ? mc_java_http_get_input(*out, e) : status;
}

mc_java_url_status mc_java_http_disconnect(mc_java_http_input *s, mc_java_url_error *e) {
    if (e == NULL)
        return MC_JAVA_URL_ARGUMENT;
    if (s == NULL)
        return mc_url_fail(e, MC_JAVA_URL_SOURCE_NULL, "HTTP disconnect receiver", 0);
    if (s->active)
        return mc_url_fail(e, MC_JAVA_URL_BUSY, "HTTP disconnect reentry", 0);
    if (s->disconnects == SIZE_MAX)
        return remember(s, e, MC_JAVA_URL_LIMIT, "HTTP disconnect counter", 0);
    ++s->disconnects;
    const mc_java_url_status status = transfer_cleanup(s, e);
    if (status == MC_JAVA_URL_OK) {
        s->source_open = false;
        s->disconnected = true;
        mc_url_fail(e, MC_JAVA_URL_OK, NULL, 0);
    }
    return status;
}

mc_java_url_status mc_java_http_read(mc_java_http_input *s, uint8_t *bytes, size_t capacity,
                                     int32_t *out, mc_java_url_error *e) {
    if (e == NULL)
        return MC_JAVA_URL_ARGUMENT;
    if (s == NULL || bytes == NULL || out == NULL || capacity > INT32_MAX)
        return mc_url_fail(e, MC_JAVA_URL_ARGUMENT, "HTTP input read native arguments", 0);
    if (s->active)
        return mc_url_fail(e, MC_JAVA_URL_BUSY, "HTTP input reentry", 0);
    if (!s->source_open)
        return remember(s, e, MC_JAVA_URL_SOURCE_IO, "actual closed HTTP input", 0);
    if (s->reads == SIZE_MAX)
        return remember(s, e, MC_JAVA_URL_LIMIT, "HTTP read counter", 0);
    ++s->reads;
    if (capacity == 0) {
        *out = 0;
        return mc_url_fail(e, MC_JAVA_URL_OK, NULL, 0);
    }
    s->active = true;
    mc_java_url_status status = drive(s, false, e);
    if (status == MC_JAVA_URL_OK) {
        size_t count = s->end - s->begin;
        if (count > capacity)
            count = capacity;
        if (count > 0) {
            memcpy(bytes, s->buffer + s->begin, count);
            s->begin += count;
            s->consumed += (uint64_t)count;
            *out = (int32_t)count;
        } else
            *out = -1;
    }
    s->active = false;
    return status;
}

mc_java_url_status mc_java_http_source_close(mc_java_http_input *s, mc_java_url_error *e) {
    if (e == NULL)
        return MC_JAVA_URL_ARGUMENT;
    if (s == NULL)
        return mc_url_fail(e, MC_JAVA_URL_ARGUMENT, "HTTP close receiver", 0);
    if (s->active)
        return mc_url_fail(e, MC_JAVA_URL_BUSY, "HTTP close reentry", 0);
    if (s->source_closes == SIZE_MAX)
        return remember(s, e, MC_JAVA_URL_LIMIT, "HTTP close counter", 0);
    ++s->source_closes;
    mc_java_url_status status = transfer_cleanup(s, e);
    if (status == MC_JAVA_URL_OK) {
        s->source_open = false;
        mc_url_fail(e, MC_JAVA_URL_OK, NULL, 0);
        if (e != NULL)
            e->source_closed = true;
    }
    return status;
}

mc_java_url_status mc_java_http_release(mc_java_http_input **slot, mc_java_url_error *e) {
    if (e == NULL)
        return MC_JAVA_URL_ARGUMENT;
    if (slot == NULL)
        return mc_url_fail(e, MC_JAVA_URL_ARGUMENT, "HTTP input occurrence", 0);
    mc_java_http_input *s = *slot;
    if (s == NULL)
        return mc_url_fail(e, MC_JAVA_URL_OK, NULL, 0);
    if (s->holds > 1) {
        --s->holds;
        *slot = NULL;
        return mc_url_fail(e, MC_JAVA_URL_OK, NULL, 0);
    }
    if (s->active)
        return mc_url_fail(e, MC_JAVA_URL_BUSY, "HTTP active final occurrence", 0);
    if (transfer_cleanup(s, e) != MC_JAVA_URL_OK || global_drop(s, e) != MC_JAVA_URL_OK)
        return e->status;
    mc_java_url_release(&s->pending, NULL);
    mc_java_url_release(&s->current, NULL);
    mc_java_url_release(&s->original, NULL);
    free(s->buffer);
    free(s);
    *slot = NULL;
    return mc_url_fail(e, MC_JAVA_URL_OK, NULL, 0);
}

bool mc_java_http_inspect(const mc_java_http_input *s, mc_java_http_state *out) {
    if (s == NULL || out == NULL)
        return false;
    *out = (mc_java_http_state){
        s->holds,       s->reads,        s->source_closes,    s->received,    s->consumed,
        s->source_open, s->easy != NULL, s->completed,        s->active,      s->response,
        s->failure,     s->disconnects,  s->connection_ready, s->disconnected};
    return true;
}

mc_java_url_status mc_java_http_retain(mc_java_http_input *s, mc_java_url_error *e) {
    if (e == NULL)
        return MC_JAVA_URL_ARGUMENT;
    if (s == NULL)
        return mc_url_fail(e, MC_JAVA_URL_ARGUMENT, "HTTP input retain", 0);
    if (s->active)
        return mc_url_fail(e, MC_JAVA_URL_BUSY, "HTTP active retain", 0);
    if (s->holds == SIZE_MAX)
        return mc_url_fail(e, MC_JAVA_URL_LIMIT, "HTTP holds", 0);
    ++s->holds;
    return mc_url_fail(e, MC_JAVA_URL_OK, NULL, 0);
}
