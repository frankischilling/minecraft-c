#include "../../platform/files/url_path.h"
#include "../../world/storage/java_file_internal.h"
#include "url_internal.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#define URL_MAX_UNITS 65536u

mc_java_url_status mc_url_fail(mc_java_url_error *e, mc_java_url_status s, const char *op,
                               int code) {
    if (e != NULL)
        *e = (mc_java_url_error){
            .status = s,
            .operation = op,
            .native_error = code,
            .source_exception = s == MC_JAVA_URL_SOURCE_MALFORMED || s == MC_JAVA_URL_SOURCE_IO ||
                                s == MC_JAVA_URL_SOURCE_NOT_FOUND || s == MC_JAVA_URL_SOURCE_NULL,
            .exception_class = s == MC_JAVA_URL_SOURCE_MALFORMED ? "java/net/MalformedURLException"
                               : s == MC_JAVA_URL_SOURCE_NOT_FOUND ? "java/io/FileNotFoundException"
                               : s == MC_JAVA_URL_SOURCE_IO        ? "java/io/IOException"
                               : s == MC_JAVA_URL_SOURCE_NULL ? "java/lang/NullPointerException"
                                                              : NULL};
    return s;
}

static char *copy_bytes(const char *s, size_t n) {
    char *r = malloc(n + 1u);
    if (r != NULL) {
        memcpy(r, s, n);
        r[n] = 0;
    }
    return r;
}

static mc_java_url_status file_error(mc_java_url_error *e, mc_java_file_status s,
                                     mc_java_file_error f) {
    mc_java_url_status mapped = s == MC_JAVA_FILE_BUSY                   ? MC_JAVA_URL_BUSY
                                : s == MC_JAVA_FILE_LIMIT                ? MC_JAVA_URL_LIMIT
                                : s == MC_JAVA_FILE_ALLOCATION_FAILED    ? MC_JAVA_URL_ALLOCATION
                                : s == MC_JAVA_FILE_UNSUPPORTED_PROVIDER ? MC_JAVA_URL_PROVIDER
                                                                         : MC_JAVA_URL_NATIVE;
    mc_url_fail(e, mapped, f.operation, f.system_error);
    if (e != NULL)
        e->file = f;
    return mapped;
}

mc_java_url_status mc_java_file_to_uri(mc_java_file *f, mc_java_uri **out, mc_java_url_error *e) {
    if (e == NULL)
        return MC_JAVA_URL_ARGUMENT;
    if (out == NULL)
        return mc_url_fail(e, MC_JAVA_URL_ARGUMENT, "URI output", 0);
    if (f == NULL)
        return mc_url_fail(e, MC_JAVA_URL_SOURCE_NULL, "File.toURI receiver", 0);
    mc_java_uri *u = calloc(1, sizeof(*u));
    if (u == NULL)
        return mc_url_fail(e, MC_JAVA_URL_ALLOCATION, "URI native owner", 0);
    u->holds = 1;
    /* Output owns the failed native prefix too; success is the returned status. */
    *out = u;
    mc_java_file_error fe = {0};
    mc_java_file_status s = mc_java_file_get_absolute_file(f, &u->absolute, &fe);
    if (s != MC_JAVA_FILE_OK)
        return file_error(e, s, fe);
    /* getPath is a source read, followed by actual independent isDirectory. */
    const mc_world_info_name *source_path = mc_java_file_path(u->absolute);
    (void)source_path;
    bool directory = false;
    s = mc_java_file_is_directory(u->absolute, &directory, &fe);
    if (s != MC_JAVA_FILE_OK)
        return file_error(e, s, fe);
    s = mc_file_utf8(u->absolute, &u->path, &fe);
    if (s != MC_JAVA_FILE_OK)
        return file_error(e, s, fe);
    size_t length = strlen(u->path);
    if (length > URL_MAX_UNITS)
        return mc_url_fail(e, MC_JAVA_URL_LIMIT, "URI native text bound", 0);
    u->url_path = malloc((length * 3u + 6u) * sizeof(*u->url_path));
    if (u->url_path == NULL)
        return mc_url_fail(e, MC_JAVA_URL_ALLOCATION, "URI text", 0);
    if (!mc_url_path_encode(u->path, length, directory, u->url_path, length * 3u + 6u,
                            &u->url_length))
        return mc_url_fail(e, MC_JAVA_URL_PROVIDER, "URI native encoding", 0);
    return mc_url_fail(e, MC_JAVA_URL_OK, NULL, 0);
}

mc_java_url_status mc_java_uri_release(mc_java_uri **slot, mc_java_url_error *e) {
    if (e == NULL)
        return MC_JAVA_URL_ARGUMENT;
    if (slot == NULL)
        return mc_url_fail(e, MC_JAVA_URL_ARGUMENT, "URI occurrence", 0);
    mc_java_uri *u = *slot;
    if (u == NULL)
        return mc_url_fail(e, MC_JAVA_URL_OK, NULL, 0);
    if (u->holds > 1) {
        --u->holds;
        *slot = NULL;
        return mc_url_fail(e, MC_JAVA_URL_OK, NULL, 0);
    }
    mc_java_file_error fe = {0};
    mc_java_file_status s = mc_java_file_release(&u->absolute, &fe);
    if (s != MC_JAVA_FILE_OK)
        return file_error(e, s, fe);
    free(u->path);
    free(u->url_path);
    free(u);
    *slot = NULL;
    return mc_url_fail(e, MC_JAVA_URL_OK, NULL, 0);
}

mc_java_url_status mc_java_uri_to_url(mc_java_uri *u, mc_java_url **out, mc_java_url_error *e) {
    if (e == NULL)
        return MC_JAVA_URL_ARGUMENT;
    if (out == NULL)
        return mc_url_fail(e, MC_JAVA_URL_ARGUMENT, "URL output", 0);
    if (u == NULL)
        return mc_url_fail(e, MC_JAVA_URL_SOURCE_NULL, "URI.toURL receiver", 0);
    if (u->path == NULL || u->url_path == NULL)
        return mc_url_fail(e, MC_JAVA_URL_STATE, "URI not constructed", 0);
    mc_java_url *v = calloc(1, sizeof(*v));
    if (v == NULL)
        return mc_url_fail(e, MC_JAVA_URL_ALLOCATION, "File URL owner", 0);
    v->holds = 1;
    v->phase = 1;
    *out = v;
    v->file = true;
    v->file_path = copy_bytes(u->path, strlen(u->path));
    v->url_path = malloc((u->url_length + 1u) * sizeof(*v->url_path));
    if (v->file_path == NULL || v->url_path == NULL) {
        v->phase = 3;
        return mc_url_fail(e, MC_JAVA_URL_ALLOCATION, "File URL strings", 0);
    }
    memcpy(v->url_path, u->url_path, (u->url_length + 1u) * sizeof(*v->url_path));
    v->url_length = u->url_length;
    v->phase = 2;
    *out = v;
    return mc_url_fail(e, MC_JAVA_URL_OK, NULL, 0);
}

/* The HTTP source profile admits ASCII URL text (the real fixed endpoint and
 * percent-escaped ordinary S3 keys). Other encodings/protocol handlers remain
 * typed provider frontiers, never synthesized MalformedURL failures. */
static mc_java_url_status url_ascii(mc_world_info_name *name, char **out, mc_java_url_error *e) {
    if (name == NULL)
        return mc_url_fail(e, MC_JAVA_URL_SOURCE_NULL, "URL String is null", 0);
    mc_nbt_string t = mc_world_info_name_text(name);
    if (t.length > URL_MAX_UNITS)
        return mc_url_fail(e, MC_JAVA_URL_LIMIT, "URL String bound", 0);
    char *s = malloc(t.length + 1u);
    if (s == NULL)
        return mc_url_fail(e, MC_JAVA_URL_ALLOCATION, "URL ASCII storage", 0);
    for (size_t i = 0; i < t.length; ++i) {
        if (t.units[i] == 0 || t.units[i] > 127u) {
            free(s);
            return mc_url_fail(e, MC_JAVA_URL_PROVIDER, "URL non-ASCII/control provider", 0);
        }
        s[i] = (char)t.units[i];
    }
    s[t.length] = 0;
    *out = s;
    return MC_JAVA_URL_OK;
}

mc_java_url_status mc_java_url_allocate(mc_java_url **out, mc_java_url_error *e) {
    if (e == NULL)
        return MC_JAVA_URL_ARGUMENT;
    if (out == NULL)
        return mc_url_fail(e, MC_JAVA_URL_ARGUMENT, "URL RAW output", 0);
    mc_java_url *u = calloc(1, sizeof(*u));
    if (u == NULL)
        return mc_url_fail(e, MC_JAVA_URL_ALLOCATION, "URL RAW owner", 0);
    u->holds = 1;
    *out = u;
    return mc_url_fail(e, MC_JAVA_URL_OK, NULL, 0);
}

static mc_java_url_status construct_text(mc_java_url *v, mc_world_info_name *name,
                                         mc_java_url_error *e) {
    char *s = NULL;
    mc_java_url_status status = url_ascii(name, &s, e);
    if (status != MC_JAVA_URL_OK)
        return status;
    size_t length = strlen(s);
    size_t begin = 0;
    while (begin < length && (unsigned char)s[begin] <= 32u)
        ++begin;
    while (length > begin && (unsigned char)s[length - 1u] <= 32u)
        --length;
    s[length] = 0;
    const char *colon = strchr(s + begin, ':');
    if (colon == NULL) {
        free(s);
        return mc_url_fail(e, MC_JAVA_URL_SOURCE_MALFORMED, "URL missing protocol", 0);
    }
    size_t protocol = (size_t)(colon - (s + begin));
    bool http = protocol == 4 && (s[begin] == 'h' || s[begin] == 'H') &&
                (s[begin + 1] == 't' || s[begin + 1] == 'T') &&
                (s[begin + 2] == 't' || s[begin + 2] == 'T') &&
                (s[begin + 3] == 'p' || s[begin + 3] == 'P');
    bool https = protocol == 5 && (s[begin] == 'h' || s[begin] == 'H') &&
                 (s[begin + 1] == 't' || s[begin + 1] == 'T') &&
                 (s[begin + 2] == 't' || s[begin + 2] == 'T') &&
                 (s[begin + 3] == 'p' || s[begin + 3] == 'P') &&
                 (s[begin + 4] == 's' || s[begin + 4] == 'S');
    if (!http && !https) {
        free(s);
        return mc_url_fail(e, MC_JAVA_URL_PROVIDER,
                           "URL protocol handler outside HTTP/HTTPS/File profile", 0);
    }
    if (colon[1] != '/' || colon[2] != '/') {
        free(s);
        return mc_url_fail(e, MC_JAVA_URL_PROVIDER, "HTTP URL non-authority form", 0);
    }
    const char *host = colon + 3;
    const char *host_end = host + strcspn(host, "/?#");
    if (host_end == host) {
        free(s);
        return mc_url_fail(e, MC_JAVA_URL_PROVIDER, "HTTP empty-host connection provider", 0);
    }
    for (const char *at = host; at < host_end; ++at)
        if (*at == '@' || *at == '[' || *at == ']') {
            free(s);
            return mc_url_fail(e, MC_JAVA_URL_PROVIDER, "HTTP userinfo/IPv6 provider", 0);
        }
    const char *port = memchr(host, ':', (size_t)(host_end - host));
    if (port != NULL) {
        uint32_t value = 0;
        for (const char *at = port + 1; at < host_end; ++at) {
            if (*at < '0' || *at > '9' || value > (UINT32_C(65535) - (uint32_t)(*at - '0')) / 10u) {
                free(s);
                return mc_url_fail(e, MC_JAVA_URL_SOURCE_MALFORMED, "URL invalid port", 0);
            }
            value = value * 10u + (uint32_t)(*at - '0');
        }
    }

    v->http = http;
    if (mc_world_info_name_retain(name) != MC_WORLD_INFO_OK) {
        free(s);
        return mc_url_fail(e, MC_JAVA_URL_LIMIT, "URL source argument hold", 0);
    }
    v->text = name;
    v->request = copy_bytes(s + begin, length - begin);
    free(s);
    if (v->request == NULL) {
        return mc_url_fail(e, MC_JAVA_URL_ALLOCATION, "URL request", 0);
    }
    return mc_url_fail(e, MC_JAVA_URL_OK, NULL, 0);
}

mc_java_url_status mc_java_url_construct(mc_java_url *u, mc_world_info_name *name,
                                         mc_java_url_error *e) {
    if (e == NULL)
        return MC_JAVA_URL_ARGUMENT;
    if (u == NULL || u->phase != 0)
        return mc_url_fail(e, MC_JAVA_URL_STATE, "URL constructor one-shot", 0);
    u->phase = 1;
    mc_java_url_status s = construct_text(u, name, e);
    u->phase = s == MC_JAVA_URL_OK ? 2u : 3u;
    return s;
}

mc_java_url_status mc_java_url_create(mc_world_info_name *name, mc_java_url **out,
                                      mc_java_url_error *e) {
    mc_java_url_status s = mc_java_url_allocate(out, e);
    return s == MC_JAVA_URL_OK ? mc_java_url_construct(*out, name, e) : s;
}

mc_java_url_status mc_java_url_resolve(mc_java_url *base, mc_world_info_name *name,
                                       mc_java_url **out, mc_java_url_error *e) {
    if (e == NULL)
        return MC_JAVA_URL_ARGUMENT;
    mc_java_url_status allocated = mc_java_url_allocate(out, e);
    if (allocated != MC_JAVA_URL_OK)
        return allocated;
    mc_java_url *raw = *out;
    if (base == NULL)
        return mc_java_url_construct(raw, name, e);
    if (base->phase != 2)
        return mc_url_fail(e, MC_JAVA_URL_STATE, "relative URL context is not constructed", 0);
    if (!base->http) {
        raw->phase = 3;
        return mc_url_fail(e, MC_JAVA_URL_PROVIDER, "relative URL handler", 0);
    }
    char *part = NULL;
    mc_java_url_status s = url_ascii(name, &part, e);
    if (s != MC_JAVA_URL_OK)
        return s;
    const char *colon = strchr(part, ':');
    const char *slash = strchr(part, '/');
    if (colon != NULL && (slash == NULL || colon < slash)) {
        free(part);
        return mc_java_url_construct(raw, name, e);
    }
    /* Native provider gate for Java URL corner forms until their real parse
     * rules are ported; ordinary S3 names contain neither dot segments nor URL
     * query/fragment operands. No wrong normalized request is issued. */
    if (strstr(part, "../") || strstr(part, "./") || strchr(part, '?') || strchr(part, '#') ||
        strncmp(part, "//", 2) == 0) {
        free(part);
        return mc_url_fail(e, MC_JAVA_URL_PROVIDER, "relative URL dot/query/authority provider", 0);
    }
    const char *b = base->request;
    const char *authority = strstr(b, "://");
    authority += 3;
    const char *path = strchr(authority, '/');
    size_t keep;
    if (part[0] == '/')
        keep = path == NULL ? strlen(b) : (size_t)(path - b);
    else {
        const char *last = strrchr(b, '/');
        keep = last != NULL && last >= authority ? (size_t)(last - b) + 1u : strlen(b);
    }
    bool add_slash = part[0] != '/' && (keep == strlen(b)) && (path == NULL);
    size_t n = strlen(part);
    if (keep + n + 1u > URL_MAX_UNITS) {
        free(part);
        return mc_url_fail(e, MC_JAVA_URL_LIMIT, "relative URL bound", 0);
    }
    uint16_t *units = malloc((keep + n + 1u) * sizeof(*units));
    if (units == NULL) {
        free(part);
        return mc_url_fail(e, MC_JAVA_URL_ALLOCATION, "relative URL text", 0);
    }
    size_t used = 0;
    for (size_t i = 0; i < keep; ++i)
        units[used++] = (unsigned char)b[i];
    if (add_slash)
        units[used++] = '/';
    for (size_t i = 0; i < n; ++i)
        units[used++] = (unsigned char)part[i];
    free(part);
    mc_world_info_name *resolved = NULL;
    mc_world_info_status ns =
        mc_world_info_name_copy((mc_nbt_string){units, used}, SIZE_MAX, &resolved);
    free(units);
    if (ns != MC_WORLD_INFO_OK)
        return mc_url_fail(e, MC_JAVA_URL_ALLOCATION, "relative URL source String", 0);
    s = mc_java_url_construct(raw, resolved, e);
    mc_world_info_name_release(resolved);
    return s;
}

mc_java_url_status mc_java_url_retain(mc_java_url *u, mc_java_url_error *e) {
    if (e == NULL)
        return MC_JAVA_URL_ARGUMENT;
    if (u == NULL)
        return mc_url_fail(e, MC_JAVA_URL_ARGUMENT, "URL retain", 0);
    if (u->holds == SIZE_MAX)
        return mc_url_fail(e, MC_JAVA_URL_LIMIT, "URL holds", 0);
    ++u->holds;
    return mc_url_fail(e, MC_JAVA_URL_OK, NULL, 0);
}

mc_java_url_status mc_java_url_release(mc_java_url **slot, mc_java_url_error *e) {
    mc_java_url_error ignored;
    if (e == NULL)
        e = &ignored;
    if (slot == NULL)
        return mc_url_fail(e, MC_JAVA_URL_ARGUMENT, "URL occurrence", 0);
    mc_java_url *u = *slot;
    if (u != NULL) {
        if (--u->holds == 0) {
            mc_world_info_name_release(u->text);
            free(u->request);
            free(u->file_path);
            free(u->url_path);
            free(u);
        }
        *slot = NULL;
    }
    return mc_url_fail(e, MC_JAVA_URL_OK, NULL, 0);
}

bool mc_java_url_inspect(const mc_java_url *u, mc_java_url_view *v) {
    if (u == NULL || v == NULL)
        return false;
    *v = (mc_java_url_view){.text = u->text,
                            .request = u->request,
                            .native_file = u->file_path,
                            .file_url_path = u->url_path,
                            .file_url_length = u->url_length,
                            .http = u->http,
                            .file = u->file,
                            .phase = u->phase};
    return true;
}
