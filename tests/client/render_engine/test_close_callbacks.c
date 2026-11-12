/* Authored genuine-filesystem native ownership controls. Source only: this
 * successor has not compiled or executed this test. No fake READY font,
 * Display, image or completed close is supplied by a provider. */
#include "client/gui/bitmap_font_internal.h"
#include "client/render_engine/internal.h"
#include "mc/resources/files/files.h"

#include <stdio.h>
#include <string.h>

static unsigned checks;
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "close callback check %u line%d: %s\n", checks, __LINE__, #condition); \
            result = 1;                                                                            \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

typedef struct close_control {
    const mc_registry_resources *real;
    mc_registry_resources wrapped;
    mc_client_class_stream **stream_slot;
    mc_client_bitmap_font **font_slot;
    mc_java_properties_input saved_input;
    const char *case_name;
    size_t entries, forwarded_ok, nested_stream, nested_read, nested_font, nested_retain;
    size_t injected_failures;
    bool inside, reenter, fail_once, failed;
} close_control;

static mc_registry_status resource_open(void *context, const char *name,
                                        mc_java_properties_input *out) {
    close_control *c = context;
    return c->real->open(c->real->context, name, out);
}

static bool resource_ready(void *context, const mc_java_properties_input *input) {
    close_control *c = context;
    return c->real->ready != NULL && c->real->ready(c->real->context, input);
}

static mc_registry_status resource_close(void *context, const mc_java_properties_input *input) {
    close_control *c = context;
    ++c->entries;
    c->saved_input = *input;
    /* Always issue the real filesystem close, including an unexpected nested
     * callback. That provider owns this context until resource_files_free. */
    const mc_registry_status closed = c->real->close(c->real->context, input);
    if (closed == MC_REGISTRY_OK)
        ++c->forwarded_ok;
    printf("CLOSE %s %zu %d\n", c->case_name, c->entries, (int)closed);
    if (c->inside || c->entries > 2) {
        c->failed = true;
        return closed;
    }
    c->inside = true;
    if (closed != MC_REGISTRY_OK)
        c->failed = true;
    if (c->reenter) {
        c->reenter = false;
        mc_client_class_stream *before = *c->stream_slot;
        const mc_client_image_io_input view = mc_client_class_stream_image_input(before);
        uint8_t byte = 0;
        size_t count = 99;
        mc_client_image_io_error image_error = {0};
        const mc_client_image_io_status read =
            view.read(view.context, &byte, 1, &count, &image_error);
        ++c->nested_read;
        if (read != MC_CLIENT_IMAGE_IO_BUSY || count != 0)
            c->failed = true;
        mc_client_class_stream_error stream_error = {0};
        const mc_client_class_stream_status stream =
            mc_client_class_stream_retire(c->stream_slot, &stream_error);
        ++c->nested_stream;
        const bool stream_same = *c->stream_slot == before;
        printf("NESTED_STREAM %s %d %d\n", c->case_name, (int)stream, stream_same ? 1 : 0);
        if (stream != MC_CLIENT_CLASS_STREAM_BUSY || !stream_same) {
            /* Old source can already have freed the owner. Do not inspect or
             * reenter that stale identity again. The outer old continuation
             * remains the genuine bounded lifetime red, never a passed reject. */
            c->failed = true;
            c->inside = false;
            return MC_REGISTRY_IO_FAILED;
        }
        if (c->font_slot != NULL) {
            mc_client_bitmap_font *font = *c->font_slot;
            mc_client_bitmap_font_state state;
            if (!mc_client_bitmap_font_inspect(font, &state) || state.holds != 1 || !state.active ||
                state.phase != MC_CLIENT_BITMAP_FONT_RETIRING)
                c->failed = true;
            mc_client_bitmap_font_error font_error = {0};
            const mc_client_bitmap_font_status release =
                mc_client_bitmap_font_release(c->font_slot, &font_error);
            ++c->nested_font;
            const bool font_same = *c->font_slot == font;
            printf("NESTED_FONT %s %d %d\n", c->case_name, (int)release, font_same ? 1 : 0);
            if (release != MC_CLIENT_BITMAP_FONT_BUSY || !font_same ||
                font_error.status != MC_CLIENT_BITMAP_FONT_BUSY || !font_error.native_policy) {
                c->failed = true;
                c->inside = false;
                return MC_REGISTRY_IO_FAILED;
            }
            const mc_client_bitmap_font_status retained = mc_client_bitmap_font_retain(font);
            ++c->nested_retain;
            printf("NESTED_RETAIN %s %d\n", c->case_name, (int)retained);
            if (retained != MC_CLIENT_BITMAP_FONT_BUSY ||
                !mc_client_bitmap_font_inspect(font, &state) || state.holds != 1 || !state.active)
                c->failed = true;
        }
    }
    c->inside = false;
    if (closed == MC_REGISTRY_OK && c->fail_once) {
        c->fail_once = false;
        ++c->injected_failures;
        /* Deliberate native-provider fault AFTER actual successful close.
         * This is not an observed OS fclose failure or Java IOException. */
        return MC_REGISTRY_IO_FAILED;
    }
    return c->failed ? MC_REGISTRY_IO_FAILED : closed;
}

static int one_case(const char *root, bool font_case, bool fail_once) {
    int result = 0;
    mc_resource_files *files = NULL;
    mc_player_string *name = NULL;
    mc_client_class_stream *stream = NULL;
    mc_client_bitmap_font *font = NULL;
    mc_client_loading_classes *gui = NULL;
    mc_client_render_engine_classes *classes = NULL;
    close_control c = {0};
    c.case_name = font_case ? (fail_once ? "font-retry" : "font-success")
                            : (fail_once ? "stream-retry" : "stream-success");
    const uint16_t units[] = {'/', 'n', 'o', 't', '-', 'i', 'm', 'a', 'g', 'e', '.', 'b', 'i', 'n'};
    CHECK(mc_resource_files_create(root, &files) == MC_REGISTRY_OK);
    c.real = mc_resource_files_source(files);
    c.wrapped = (mc_registry_resources){&c, resource_open, resource_close, resource_ready};
    CHECK(mc_player_string_create(units, sizeof units / sizeof *units, &name) == MC_PLAYER_OK);
    if (font_case) {
        CHECK(mc_client_render_engine_classes_allocate_context(NULL, NULL, NULL, &classes) ==
              MC_CLIENT_RENDER_ENGINE_OK);
        CHECK(mc_client_loading_classes_create(classes, &gui) == MC_CLIENT_LOADING_OK);
        CHECK(mc_client_loading_classes_bind_resources(gui, &c.wrapped, NULL) ==
              MC_CLIENT_LOADING_OK);
        CHECK(mc_client_bitmap_font_allocate(gui, NULL, &font) == MC_CLIENT_BITMAP_FONT_OK);
        mc_client_bitmap_font_error error = {0};
        CHECK(mc_client_bitmap_font_construct(font, NULL, name, NULL, NULL, &error) ==
              MC_CLIENT_BITMAP_FONT_NULL_REFERENCE);
        CHECK(error.pc == 56 && font->phase == MC_CLIENT_BITMAP_FONT_FAILED &&
              font->stream != NULL && font->image == NULL);
        CHECK(classes->bound_display == NULL && classes->context_lease == NULL && font->holds == 1);
        c.stream_slot = &font->stream; /* Observe the actual member, never replace it. */
        c.font_slot = &font;
    } else {
        CHECK(mc_client_class_stream_open(&c.wrapped, name, 1024, 1024, &stream, NULL) ==
                  MC_CLIENT_CLASS_STREAM_OK &&
              stream != NULL);
        c.stream_slot = &stream;
    }
    c.reenter = true;
    c.fail_once = fail_once;
    if (font_case) {
        mc_client_bitmap_font *saved = font;
        mc_client_bitmap_font_error error = {0};
        const mc_client_bitmap_font_status status = mc_client_bitmap_font_release(&font, &error);
        if (fail_once) {
            CHECK(status == MC_CLIENT_BITMAP_FONT_RENDER_FAILED && font == saved &&
                  error.stream.status == MC_CLIENT_CLASS_STREAM_NATIVE_CLOSE_FAILED &&
                  error.stream.close == MC_REGISTRY_IO_FAILED &&
                  error.stream.native_close_attempted && error.native_policy && error.pc == -1 &&
                  font->stream != NULL);
            CHECK(font->phase == MC_CLIENT_BITMAP_FONT_RETIRING && !font->active &&
                  font->holds == 1);
            CHECK(mc_client_bitmap_font_retain(font) == MC_CLIENT_BITMAP_FONT_INVALID_PHASE &&
                  font->holds == 1);
            c.reenter = true;
            CHECK(mc_client_bitmap_font_release(&font, &error) == MC_CLIENT_BITMAP_FONT_OK &&
                  font == NULL);
        } else
            CHECK(status == MC_CLIENT_BITMAP_FONT_OK && font == NULL);
    } else {
        mc_client_class_stream *saved = stream;
        mc_client_class_stream_error error = {0};
        const mc_client_class_stream_status status = mc_client_class_stream_retire(&stream, &error);
        if (fail_once) {
            CHECK(status == MC_CLIENT_CLASS_STREAM_NATIVE_CLOSE_FAILED && stream == saved &&
                  error.close == MC_REGISTRY_IO_FAILED && error.native_close_attempted);
            c.reenter = true;
            CHECK(mc_client_class_stream_retire(&stream, &error) == MC_CLIENT_CLASS_STREAM_OK &&
                  stream == NULL);
        } else
            CHECK(status == MC_CLIENT_CLASS_STREAM_OK && stream == NULL);
    }
    c.stream_slot = NULL;
    c.font_slot = NULL;
    CHECK(!c.failed && c.entries == (fail_once ? 2u : 1u) && c.forwarded_ok == c.entries &&
          c.nested_stream == c.entries && c.nested_read == c.entries &&
          c.nested_font == (font_case ? c.entries : 0u) && c.nested_retain == c.nested_font &&
          c.injected_failures == (fail_once ? 1u : 0u));
    uint8_t byte;
    size_t count = 99;
    CHECK(c.saved_input.read(c.saved_input.context, &byte, 1, &count) ==
              MC_JAVA_PROPERTIES_IO_ERROR &&
          count == 0);
    CHECK(c.real->ready == NULL || !c.real->ready(c.real->context, &c.saved_input));
    printf("CASE_OK %s %zu %zu %zu %zu\n", c.case_name, c.entries, c.nested_stream, c.nested_font,
           c.nested_retain);
cleanup:
    /* Only ordinary API retirement. No slot replacement or private-field
     * repair can turn an old-source lifetime red into success. */
    c.reenter = false;
    c.fail_once = false;
    if (c.failed)
        result = 1;
    c.failed = false; /* Remove fixture faults for ordinary cleanup only. */
    if (font != NULL && mc_client_bitmap_font_release(&font, NULL) != MC_CLIENT_BITMAP_FONT_OK)
        result = 1;
    if (stream != NULL && mc_client_class_stream_retire(&stream, NULL) != MC_CLIENT_CLASS_STREAM_OK)
        result = 1;
    if (gui != NULL && mc_client_loading_classes_release(gui) != MC_CLIENT_LOADING_OK)
        result = 1;
    if (classes != NULL &&
        mc_client_render_engine_classes_release(classes) != MC_CLIENT_RENDER_ENGINE_OK)
        result = 1;
    mc_player_string_release(name);
    if (mc_resource_files_free(files) != MC_REGISTRY_OK)
        result = 1;
    return result;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fputs("Close callback input: resource-directory\n", stderr);
        return 2;
    }
    (void)setvbuf(stdout, NULL, _IONBF, 0);
    if (one_case(argv[1], false, false) || one_case(argv[1], false, true) ||
        one_case(argv[1], true, false) || one_case(argv[1], true, true))
        return 1;
    if (printf("Source close callbacks: %u checks;4 genuine-filesystem cases\n", checks) < 0)
        return 1;
    return 0;
}
