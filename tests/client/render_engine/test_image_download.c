#include "client/render_engine/image_buffer.h"
#include "client/render_engine/image_download.h"
#include "mc/client/render_engine/render_engine.h"
#include "mc/core/java/url.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

static unsigned int checks;
#define CHECK(expression)                                                                          \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(expression)) {                                                                       \
            fprintf(stderr, "Image download line%d: %s\n", __LINE__, #expression);                 \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

typedef struct fixture {
    mc_client_render_engine_classes *classes;
    mc_client_default_font *font;
    mc_client_render_engine *engine;
} fixture;

static bool pause_once(void) {
    const struct timespec duration = {0, 1000000};
    return thrd_sleep(&duration, NULL) == 0;
}

static bool barrier(const char *operation, const char *name, unsigned int count) {
    printf("%s %s %u\n", operation, name, count);
    fflush(stdout);
    char reply[16];
    CHECK(fgets(reply, sizeof(reply), stdin) != NULL && strcmp(reply, "OK\n") == 0);
    return true;
}

static bool open_fixture(fixture *f, const mc_client_image_download_options *options) {
    mc_client_render_engine_error error;
    CHECK(mc_client_render_engine_classes_create(NULL, &f->classes) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_default_font_create(32u * 1024u * 1024u, &f->font) == MC_CLIENT_FONT_OK);
    CHECK(mc_client_render_engine_allocate(f->classes, f->font, NULL, &f->engine) ==
          MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_render_engine_construct(f->engine, NULL, NULL, &error) ==
          MC_CLIENT_RENDER_ENGINE_OK);
    if (options != NULL)
        CHECK(mc_client_render_image_downloads_configure(f->engine, options) ==
              MC_CLIENT_IMAGE_DOWNLOAD_OK);
    return true;
}

static bool retire_downloads(fixture *f) {
    mc_client_image_download_error error;
    for (unsigned int i = 0; i < 30000u; ++i) {
        const mc_client_image_download_status status =
            mc_client_render_image_downloads_retire(f->engine, &error);
        if (status == MC_CLIENT_IMAGE_DOWNLOAD_OK)
            return true;
        CHECK(status == MC_CLIENT_IMAGE_DOWNLOAD_BUSY && pause_once());
    }
    fprintf(stderr, "Image download native retirement did not complete\n");
    return false;
}

static bool close_fixture(fixture *f) {
    CHECK(retire_downloads(f));
    mc_client_render_engine_error error;
    CHECK(mc_client_render_engine_retire(&f->engine, &error) == MC_CLIENT_RENDER_ENGINE_OK &&
          f->engine == NULL);
    CHECK(mc_client_default_font_release(f->font) == MC_CLIENT_FONT_OK);
    mc_client_render_engine_classes_release(f->classes);
    return true;
}

static bool make_string(const char *base, const char *path, mc_player_string **out) {
    char text[1024];
    const int count = snprintf(text, sizeof(text), "%s/%s", base, path);
    CHECK(count > 0 && (size_t)count < sizeof(text));
    uint16_t units[1024];
    for (int i = 0; i < count; ++i)
        units[i] = (unsigned char)text[i];
    CHECK(mc_player_string_create(units, (size_t)count, out) == MC_PLAYER_OK);
    return true;
}

static bool map_count(fixture *f, size_t expected) {
    mc_client_render_engine_state state;
    size_t count = SIZE_MAX;
    CHECK(mc_client_render_engine_inspect(f->engine, &state) &&
          mc_client_render_map_count(state.fields.h, &count) && count == expected);
    return true;
}

static bool terminal_admission(fixture *f, mc_player_string *key) {
    const mc_client_image_download_options options = mc_client_image_download_default_options();
    CHECK(mc_client_render_image_downloads_configure(f->engine, &options) ==
          MC_CLIENT_IMAGE_DOWNLOAD_STATE);
    mc_client_image_download_call *call = NULL;
    CHECK(mc_client_image_download_request_begin(f->engine, key, NULL, &call) ==
          MC_CLIENT_IMAGE_DOWNLOAD_STATE);
    mc_client_image_download_call_state state;
    CHECK(mc_client_image_download_call_inspect(call, &state) && !state.active &&
          !state.source_returned && !state.map_published && state.source_pc == 0 &&
          state.engine == NULL && state.url == NULL && state.processor == NULL &&
          state.lookup == NULL && state.candidate == NULL &&
          state.failure->status == MC_CLIENT_IMAGE_DOWNLOAD_STATE && state.failure->native_policy &&
          !state.failure->source_exception);
    CHECK(mc_client_image_download_call_retire(&call) == MC_CLIENT_IMAGE_DOWNLOAD_OK &&
          call == NULL);
    mc_client_image_download_error error;
    CHECK(mc_client_render_image_download_release(f->engine, key, &error) ==
              MC_CLIENT_IMAGE_DOWNLOAD_STATE &&
          error.native_policy && !error.source_exception && !error.source_returned);
    mc_client_render_engine_state engine;
    CHECK(mc_client_render_engine_inspect(f->engine, &engine) && engine.native_holds == 1 &&
          map_count(f, 0));
    CHECK(mc_client_render_image_downloads_retire(f->engine, &error) ==
          MC_CLIENT_IMAGE_DOWNLOAD_OK);
    return true;
}

static bool empty_terminal_case(const char *base) {
    mc_player_string *key = NULL;
    CHECK(make_string(base, "no-worker", &key));
    fixture f = {0};
    CHECK(open_fixture(&f, NULL));
    CHECK(mc_client_render_engine_retain(f.engine) == MC_CLIENT_RENDER_ENGINE_OK);
    mc_client_image_download_error error;
    CHECK(mc_client_render_image_downloads_retire(f.engine, &error) ==
          MC_CLIENT_IMAGE_DOWNLOAD_BUSY);
    const mc_client_image_download_options options = mc_client_image_download_default_options();
    /* Refusing a foreign Engine hold must also preserve configure admission
     * before any sidecar has been created. */
    CHECK(mc_client_render_image_downloads_configure(f.engine, &options) ==
          MC_CLIENT_IMAGE_DOWNLOAD_OK);
    CHECK(mc_client_render_engine_release(f.engine) == MC_CLIENT_RENDER_ENGINE_OK &&
          retire_downloads(&f) && terminal_admission(&f, key) && close_fixture(&f));
    f = (fixture){0};
    /* A never-created sidecar still has a persistent terminal Engine state. */
    CHECK(open_fixture(&f, NULL) && retire_downloads(&f) && terminal_admission(&f, key) &&
          close_fixture(&f));
    mc_player_string_release(key);
    return true;
}

static bool wait_worker(const mc_client_image_download *entry,
                        mc_client_image_download_state *state) {
    for (unsigned int i = 0; i < 30000u; ++i) {
        CHECK(mc_client_image_download_inspect(entry, state));
        if (state->thread_exited)
            return true;
        CHECK(pause_once());
    }
    fprintf(stderr, "Image download worker did not exit\n");
    return false;
}

static bool https_case(const char *base) {
    CHECK(strncmp(base, "http://", 7) == 0);
    char secure_base[1024];
    const int count = snprintf(secure_base, sizeof(secure_base), "HTTPS://%s", base + 7);
    CHECK(count > 0 && (size_t)count < sizeof(secure_base));
    fixture f = {0};
    mc_player_string *key = NULL;
    mc_client_image_download_call *call = NULL;
    CHECK(open_fixture(&f, NULL) && make_string(secure_base, "no-worker", &key));
    CHECK(mc_client_image_download_request_begin(f.engine, key, NULL, &call) ==
          MC_CLIENT_IMAGE_DOWNLOAD_OK);
    mc_client_image_download_call_state request;
    CHECK(mc_client_image_download_call_inspect(call, &request) && request.source_returned &&
          request.source_pc == 52 && request.lookup == NULL && request.candidate != NULL &&
          request.map_published && request.failure->status == MC_CLIENT_IMAGE_DOWNLOAD_OK);
    mc_client_image_download_state state;
    CHECK(wait_worker(request.candidate, &state) && state.thread_started && state.a == NULL &&
          !state.worker_source_returned && !state.print_stack_trace_pending &&
          state.worker_pc == 15 && state.connection == NULL &&
          state.worker_error->status == MC_CLIENT_IMAGE_DOWNLOAD_UNIMPLEMENTED &&
          state.worker_error->native_policy && !state.worker_error->source_exception &&
          state.worker_error->exception_class == NULL &&
          strcmp(state.worker_error->site.class_name, "tj") == 0 &&
          strcmp(state.worker_error->site.method, "run") == 0 &&
          strcmp(state.worker_error->site.descriptor, "()V") == 0 &&
          state.worker_error->site.pc == 15 && state.worker_error->caller.class_name == NULL &&
          state.worker_error->url.status == MC_JAVA_URL_PROVIDER &&
          !state.worker_error->url.source_exception &&
          state.worker_error->url.exception_class == NULL &&
          state.worker_error->url.operation != NULL &&
          strcmp(state.worker_error->operation, state.worker_error->url.operation) == 0);
    mc_player_string_release(key);
    CHECK(mc_client_image_download_call_retire(&call) == MC_CLIENT_IMAGE_DOWNLOAD_OK &&
          call == NULL && close_fixture(&f));
    return true;
}

static bool connection_state(const mc_client_image_download_state *state, long code, bool input,
                             size_t disconnects) {
    mc_java_http_state http;
    CHECK(state->thread_exited && state->connection != NULL &&
          mc_java_http_inspect(state->connection, &http) && http.response_code == code &&
          http.source_open == input && http.disconnects == disconnects && http.source_closes == 0);
    if (disconnects != 0)
        CHECK(http.disconnected && !http.native_open);
    return true;
}

static bool image_case(const char *base, const char *path, bool processor, int expected) {
    fixture f = {0};
    CHECK(open_fixture(&f, NULL));
    mc_player_string *key = NULL;
    mc_client_image_buffer *rr = NULL;
    if (processor)
        CHECK(mc_client_image_buffer_create(NULL, &rr, NULL) == MC_CLIENT_IMAGE_BUFFER_OK);
    CHECK(make_string(base, path, &key));
    mc_client_image_download_call *call = NULL;
    CHECK(mc_client_image_download_request_begin(f.engine, key, rr, &call) ==
          MC_CLIENT_IMAGE_DOWNLOAD_OK);
    mc_client_image_download_call_state request;
    CHECK(mc_client_image_download_call_inspect(call, &request) && request.source_returned &&
          request.map_published && request.source_pc == 52 && request.lookup == NULL &&
          request.candidate != NULL && request.url == key && request.processor == rr &&
          request.failure->status == MC_CLIENT_IMAGE_DOWNLOAD_OK && map_count(&f, 1));
    mc_client_image_download_state state;
    CHECK(wait_worker(request.candidate, &state) && state.thread_constructed &&
          state.thread_started && !state.thread_joined && state.constructor_pc == 32 &&
          state.b == 1 && state.c == -1 && !state.d && state.url == key && state.processor == rr);
    /* Worker failure never changes the already-returned factory invocation. */
    CHECK(mc_client_image_download_call_inspect(call, &request) && request.source_returned &&
          request.failure->status == MC_CLIENT_IMAGE_DOWNLOAD_OK);
    if (expected == 404) {
        CHECK(state.a == NULL && state.worker_source_returned && state.worker_pc == 51 &&
              !state.print_stack_trace_pending &&
              state.worker_error->status == MC_CLIENT_IMAGE_DOWNLOAD_OK &&
              connection_state(&state, 404, false, 1));
    } else if (expected == 500) {
        CHECK(state.a == NULL && !state.worker_source_returned && state.worker_pc == 108 &&
              state.print_stack_trace_pending &&
              state.worker_error->status == MC_CLIENT_IMAGE_DOWNLOAD_UNIMPLEMENTED &&
              state.worker_error->site.pc == 108 &&
              state.worker_error->caller.pc == (processor ? 85u : 64u) &&
              state.worker_error->url.source_exception &&
              state.worker_error->url.response_code == 500 &&
              state.worker_error->url.status == MC_JAVA_URL_SOURCE_IO &&
              connection_state(&state, 500, false, 0));
    } else {
        CHECK(state.worker_source_returned && state.worker_pc == 125 &&
              !state.print_stack_trace_pending &&
              state.worker_error->status == MC_CLIENT_IMAGE_DOWNLOAD_OK &&
              connection_state(&state, 200, false, 1));
        if (expected == 0)
            CHECK(state.a == NULL);
        else {
            mc_client_argb_image_state image;
            CHECK(mc_client_argb_image_inspect(state.a, &image) && image.constructed &&
                  image.type == 2);
            if (processor)
                CHECK(image.width == 64 && image.height == 32 && image.pixel_count == 2048 &&
                      image.pixels[0] == UINT32_C(0xff123456) &&
                      image.pixels[8u * 64u + 40u] == UINT32_C(0x7f123456) &&
                      image.pixels[31u * 64u + 63u] == UINT32_C(0xff123456));
            else
                CHECK(image.width == 2 && image.height == 2 && image.pixel_count == 4 &&
                      image.pixels[0] == UINT32_C(0xffff0000) &&
                      image.pixels[1] == UINT32_C(0x800000ff) &&
                      image.pixels[2] == UINT32_C(0x00010203) &&
                      image.pixels[3] == UINT32_C(0xff00ff00));
        }
    }
    if (rr != NULL) {
        mc_client_image_buffer_state buffer;
        CHECK(mc_client_image_buffer_inspect(rr, &buffer) && buffer.native_holds == 3);
        if (expected == 0 || expected == 404 || expected == 500)
            CHECK(buffer.a == NULL && buffer.b == 0 && buffer.c == 0 && buffer.array_owner == NULL);
        else
            CHECK(buffer.b == 64 && buffer.c == 32 && buffer.array_owner == state.a);
    }
    mc_client_image_download_error retirement;
    CHECK(mc_client_render_image_downloads_retire(f.engine, &retirement) ==
              MC_CLIENT_IMAGE_DOWNLOAD_BUSY &&
          mc_client_image_download_call_retire(&call) == MC_CLIENT_IMAGE_DOWNLOAD_OK &&
          call == NULL);
    if (rr != NULL)
        CHECK(mc_client_image_buffer_release(&rr, NULL) == MC_CLIENT_IMAGE_BUFFER_OK);
    mc_player_string_release(key);
    CHECK(close_fixture(&f));
    return true;
}

static bool cache_and_gate(const char *base) {
    fixture f = {0};
    CHECK(open_fixture(&f, NULL));
    mc_player_string *a = NULL, *equal = NULL;
    CHECK(make_string(base, "gate", &a) && make_string(base, "gate", &equal) && a != equal);
    mc_client_image_buffer *rr = NULL, *ignored = NULL;
    CHECK(mc_client_image_buffer_create(NULL, &rr, NULL) == MC_CLIENT_IMAGE_BUFFER_OK &&
          mc_client_image_buffer_create(NULL, &ignored, NULL) == MC_CLIENT_IMAGE_BUFFER_OK);
    mc_client_image_download_call *first = NULL, *hit = NULL, *again = NULL;
    CHECK(mc_client_image_download_request_begin(f.engine, a, rr, &first) ==
          MC_CLIENT_IMAGE_DOWNLOAD_OK);
    mc_client_image_download_call_state one, two, three;
    CHECK(mc_client_image_download_call_inspect(first, &one) && one.lookup == NULL &&
          one.candidate != NULL && one.map_published && one.source_returned);
    CHECK(barrier("SYNC", "gate", 1));
    CHECK(mc_client_image_download_request_begin(f.engine, equal, ignored, &hit) ==
          MC_CLIENT_IMAGE_DOWNLOAD_OK);
    CHECK(mc_client_image_download_call_inspect(hit, &two) && two.lookup == one.candidate &&
          two.candidate == NULL && !two.map_published && two.source_returned && two.url == equal &&
          two.processor == ignored && map_count(&f, 1));
    mc_client_image_download_state old;
    CHECK(mc_client_image_download_inspect(one.candidate, &old) && old.b == 2 && old.url == a &&
          old.processor == rr && old.thread_started && !old.thread_exited && old.a == NULL);
    mc_player_string_view text;
    mc_player_string_release(a);
    mc_player_string_release(equal);
    /* These locals now borrow the exact retained call/entry String identities;
     * no second owning release is performed below. */
    CHECK(mc_player_string_read(one.url, &text) && text.length != 0 &&
          mc_player_string_read(two.url, &text) && text.length != 0);
    mc_client_image_download_error error;
    CHECK(mc_client_render_image_download_release(f.engine, equal, &error) ==
              MC_CLIENT_IMAGE_DOWNLOAD_OK &&
          error.source_returned && map_count(&f, 1));
    CHECK(mc_client_image_download_inspect(one.candidate, &old) && old.b == 1);
    CHECK(mc_client_render_image_download_release(f.engine, a, &error) ==
              MC_CLIENT_IMAGE_DOWNLOAD_OK &&
          map_count(&f, 0));
    CHECK(mc_client_image_download_inspect(one.candidate, &old) && old.b == 0 &&
          !old.thread_exited && !old.native_stop_requested && old.a == NULL);
    CHECK(mc_client_image_download_request_begin(f.engine, equal, NULL, &again) ==
          MC_CLIENT_IMAGE_DOWNLOAD_OK);
    CHECK(mc_client_image_download_call_inspect(again, &three) && three.lookup == NULL &&
          three.candidate != NULL && three.candidate != one.candidate && three.map_published &&
          three.source_returned && map_count(&f, 1));
    CHECK(barrier("SYNC", "gate", 2));
    CHECK(mc_client_image_download_inspect(one.candidate, &old) && !old.thread_exited &&
          !old.native_stop_requested && old.b == 0);
    mc_client_image_download_state fresh;
    CHECK(mc_client_image_download_inspect(three.candidate, &fresh) && !fresh.thread_exited &&
          fresh.b == 1 && fresh.processor == NULL);
    CHECK(mc_client_render_image_downloads_retire(f.engine, &error) ==
          MC_CLIENT_IMAGE_DOWNLOAD_BUSY);
    CHECK(barrier("OPEN", "gate", 2));
    CHECK(wait_worker(one.candidate, &old) && wait_worker(three.candidate, &fresh) &&
          old.worker_source_returned && fresh.worker_source_returned && old.a != NULL &&
          fresh.a != NULL && old.a != fresh.a && old.b == 0 && fresh.b == 1);
    CHECK(connection_state(&old, 200, false, 1) && connection_state(&fresh, 200, false, 1));
    CHECK(mc_client_image_buffer_release(&rr, NULL) == MC_CLIENT_IMAGE_BUFFER_OK &&
          mc_client_image_buffer_release(&ignored, NULL) == MC_CLIENT_IMAGE_BUFFER_OK);
    CHECK(mc_client_image_download_call_retire(&first) == MC_CLIENT_IMAGE_DOWNLOAD_OK &&
          mc_client_image_download_call_retire(&hit) == MC_CLIENT_IMAGE_DOWNLOAD_OK &&
          mc_client_image_download_call_retire(&again) == MC_CLIENT_IMAGE_DOWNLOAD_OK);
    CHECK(close_fixture(&f));
    return true;
}

static bool policy_case(const char *base, bool map_failure) {
    fixture f = {0};
    mc_client_image_download_options options = mc_client_image_download_default_options();
    if (map_failure)
        options.max_map_entries = 0;
    else
        options.max_active_workers = 0;
    CHECK(open_fixture(&f, &options));
    mc_player_string *key = NULL;
    CHECK(make_string(base, map_failure ? "map-failure" : "no-worker", &key));
    mc_client_image_download_call *call = NULL;
    CHECK(mc_client_image_download_request_begin(f.engine, key, NULL, &call) ==
          MC_CLIENT_IMAGE_DOWNLOAD_LIMIT);
    mc_client_image_download_call_state request;
    CHECK(mc_client_image_download_call_inspect(call, &request) && request.candidate != NULL &&
          request.lookup == NULL && !request.source_returned && !request.map_published &&
          request.source_pc == (map_failure ? 32u : 29u) && request.failure->native_policy &&
          !request.failure->source_exception && !request.failure->source_returned &&
          map_count(&f, 0));
    mc_client_image_download_state state;
    CHECK(mc_client_image_download_inspect(request.candidate, &state) && state.b == 1 &&
          state.c == -1 && !state.d && state.thread_constructed &&
          state.thread_started == map_failure && state.url == key);
    if (map_failure) {
        CHECK(barrier("SYNC", "map-failure", 1));
        CHECK(wait_worker(request.candidate, &state) && state.worker_source_returned &&
              state.worker_pc == 51 && connection_state(&state, 404, false, 1));
    }
    mc_client_image_download_error error;
    mc_client_render_engine_error owner_error;
    CHECK(mc_client_render_image_downloads_retire(f.engine, &error) ==
              MC_CLIENT_IMAGE_DOWNLOAD_BUSY &&
          mc_client_render_engine_retire(&f.engine, &owner_error) == MC_CLIENT_RENDER_ENGINE_BUSY &&
          f.engine != NULL);
    mc_player_string_release(key);
    CHECK(mc_client_image_download_call_retire(&call) == MC_CLIENT_IMAGE_DOWNLOAD_OK &&
          call == NULL && close_fixture(&f));
    return true;
}

static bool stop_case(const char *base) {
    fixture f = {0};
    CHECK(open_fixture(&f, NULL));
    mc_player_string *key = NULL;
    CHECK(make_string(base, "stop", &key));
    mc_client_image_download_call *call = NULL;
    CHECK(mc_client_image_download_request_begin(f.engine, key, NULL, &call) ==
          MC_CLIENT_IMAGE_DOWNLOAD_OK);
    CHECK(barrier("SYNC", "stop", 1));
    mc_client_image_download_call_state request;
    mc_client_image_download_state state;
    CHECK(mc_client_image_download_call_inspect(call, &request) && request.candidate != NULL &&
          mc_client_image_download_inspect(request.candidate, &state) && !state.thread_exited &&
          !state.native_stop_requested);
    const mc_client_image_download *entry = request.candidate;
    CHECK(mc_client_render_engine_retain(f.engine) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_image_download_call_retire(&call) == MC_CLIENT_IMAGE_DOWNLOAD_OK);
    mc_client_image_download_error error;
    CHECK(mc_client_render_image_downloads_retire(f.engine, &error) ==
              MC_CLIENT_IMAGE_DOWNLOAD_BUSY &&
          mc_client_image_download_inspect(entry, &state) && !state.native_stop_requested &&
          !state.thread_exited);
    /* Rejected terminal admission must not latch native retirement either. */
    CHECK(mc_client_image_download_request_begin(f.engine, key, NULL, &call) ==
          MC_CLIENT_IMAGE_DOWNLOAD_OK);
    CHECK(mc_client_image_download_call_inspect(call, &request) && request.lookup == entry &&
          request.source_returned && request.candidate == NULL &&
          mc_client_image_download_inspect(entry, &state) && state.b == 2 &&
          !state.native_stop_requested);
    CHECK(mc_client_render_image_download_release(f.engine, key, &error) ==
              MC_CLIENT_IMAGE_DOWNLOAD_OK &&
          mc_client_image_download_call_retire(&call) == MC_CLIENT_IMAGE_DOWNLOAD_OK &&
          mc_client_render_engine_release(f.engine) == MC_CLIENT_RENDER_ENGINE_OK);
    /* The server body is STILL gated. Completion must use native stop/join,
     * never waiting for a successful body or a peer-release acknowledgement. */
    CHECK(retire_downloads(&f));
    CHECK(barrier("STOPPED", "stop", 1));
    CHECK(terminal_admission(&f, key) && close_fixture(&f));
    mc_player_string_release(key);
    return true;
}

static bool http_case(const char *base, const char *path, long response) {
    mc_player_string *string = NULL;
    CHECK(make_string(base, path, &string));
    mc_player_string_view text;
    CHECK(mc_player_string_read(string, &text));
    mc_world_info_name *name = NULL;
    CHECK(mc_world_info_name_copy((mc_nbt_string){text.units, text.length}, SIZE_MAX, &name) ==
          MC_WORLD_INFO_OK);
    mc_java_url *url = NULL;
    mc_java_http_input *connection = NULL;
    mc_java_url_error error;
    CHECK(mc_java_url_create(name, &url, &error) == MC_JAVA_URL_OK &&
          mc_java_http_connect(url, NULL, NULL, &connection, &error) == MC_JAVA_URL_OK);
    mc_java_http_state state;
    long code = -1;
    CHECK(mc_java_http_inspect(connection, &state) && state.connection_ready &&
          !state.source_open && !state.disconnected && state.disconnects == 0 &&
          state.source_closes == 0 && state.reads == 0 &&
          mc_java_http_response_code(connection, &code, &error) == MC_JAVA_URL_OK &&
          code == response);
    const mc_java_url_status expected = response == 404   ? MC_JAVA_URL_SOURCE_NOT_FOUND
                                        : response == 500 ? MC_JAVA_URL_SOURCE_IO
                                                          : MC_JAVA_URL_OK;
    CHECK(mc_java_http_get_input(connection, &error) == expected);
    CHECK(mc_java_http_inspect(connection, &state) && state.source_open == (response == 200) &&
          state.response_code == response && state.disconnects == 0 && state.source_closes == 0);
    if (response == 200) {
        uint8_t bytes[8] = {0};
        size_t total = 0;
        for (;;) {
            int32_t count = 0;
            CHECK(mc_java_http_read(connection, bytes + total, sizeof(bytes) - total, &count,
                                    &error) == MC_JAVA_URL_OK);
            if (count < 0)
                break;
            CHECK(count > 0 && (size_t)count <= sizeof(bytes) - total);
            total += (size_t)count;
        }
        CHECK(total == 5 && memcmp(bytes, "HELLO", 5) == 0);
    } else
        CHECK(error.source_exception && error.response_code == response);
    CHECK(mc_java_http_disconnect(connection, &error) == MC_JAVA_URL_OK &&
          mc_java_http_inspect(connection, &state) && state.disconnected && !state.source_open &&
          !state.native_open && state.disconnects == 1 && state.source_closes == 0);
    CHECK(mc_java_http_release(&connection, &error) == MC_JAVA_URL_OK && connection == NULL);
    if (response == 404) {
        CHECK(mc_java_http_open(url, NULL, NULL, &connection, &error) ==
                  MC_JAVA_URL_SOURCE_NOT_FOUND &&
              connection != NULL && error.source_exception && error.response_code == 404 &&
              mc_java_http_inspect(connection, &state) && !state.source_open &&
              state.disconnects == 0);
        CHECK(mc_java_http_release(&connection, &error) == MC_JAVA_URL_OK);
    }
    CHECK(mc_java_url_release(&url, &error) == MC_JAVA_URL_OK);
    mc_world_info_name_release(name);
    mc_player_string_release(string);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 2)
        return 2;
    (void)setvbuf(stdout, NULL, _IONBF, 0);
    if (!http_case(argv[1], "http-ok", 200) || !http_case(argv[1], "http-404", 404) ||
        !http_case(argv[1], "http-500", 500) || !image_case(argv[1], "rgba", false, 1) ||
        !image_case(argv[1], "rr", true, 1) || !image_case(argv[1], "unknown", false, 0) ||
        !image_case(argv[1], "unknown-rr", true, 0) || !image_case(argv[1], "missing", true, 404) ||
        !image_case(argv[1], "server-error", false, 500) || !cache_and_gate(argv[1]) ||
        !policy_case(argv[1], true) || !policy_case(argv[1], false) || !stop_case(argv[1]) ||
        !empty_terminal_case(argv[1]) || !https_case(argv[1]))
        return 1;
    printf("Image download contracts: %u checks\n", checks);
    return 0;
}
