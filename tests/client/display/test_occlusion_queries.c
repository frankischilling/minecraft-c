/* Exact canonical Display body, actual ARB addresses and GPU. No query
 * callback/capability/result is replaced. Ge buffers are genuine owners. */
#include "../../../src/client/display/display.c"
#include "client/render_engine/internal.h"
#include "client/render_engine/nw.h"
#include <SDL3/SDL_main.h>
#include <inttypes.h>
#include <stdio.h>

static unsigned int checks;
#define CHECK(value)                                                                               \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(value)) {                                                                            \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #value);                            \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

static uint32_t word(const mc_client_direct_int_buffer *buffer, size_t index) {
    uint32_t value;
    memcpy(&value, buffer->bytes->data + index * sizeof(value), sizeof(value));
    return value;
}

static bool span(mc_client_direct_int_buffer *buffer, void **storage, size_t *remaining,
                 mc_client_graphics_error *error) {
    const mc_client_graphics_site caller = {"native-test", "query-span", "()V", -1};
    return mc_client_ge_int_position(buffer, 1, caller, error) &&
           mc_client_ge_int_limit(buffer, 2, caller, error) &&
           mc_client_ge_int_writable_remaining(buffer, storage, remaining, caller, error);
}

static bool query_ended(mc_client_display_active_query *query, bool ended) {
    mc_client_display_active_query_state state;
    return mc_client_display_active_query_inspect(query, &state) && state.begin_issued &&
           state.source_ended == ended && !state.native_aborted;
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    int result = 1;
    mc_client_display *display = NULL, *other = NULL;
    mc_client_render_engine_classes *classes = NULL;
    mc_client_render_arena arena = {.options = {1024u * 1024u, SIZE_MAX}};
    mc_client_direct_int_buffer *ids = NULL, *output = NULL;
    mc_client_display_query_names *names = NULL;
    mc_client_display_active_query *query = NULL, *second_query = NULL;
    mc_client_display_context_lease *lease = NULL;
    mc_client_display_error error = {0};
    mc_client_graphics_error graphics = {0};
    uint8_t *vertices = NULL;
    bool supported = false;
    void *storage = NULL;
    size_t remaining = 0;
    int32_t id = 0;
    uint32_t initial_available = UINT32_MAX, visible_available = UINT32_MAX, visible = 0;
    GLint counter_bits = 0;
    const mc_client_graphics_site caller = {"native-test", "query", "()V", -1};
    CHECK(
        mc_client_display_create("actual ARB query issue/result", 64, 64, true, &display, &error));
    CHECK(mc_client_display_source_arb_occlusion_query(display, &supported, &error));
    CHECK(mc_client_render_engine_classes_allocate_context(NULL, NULL, NULL, &classes) ==
          MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_ge_d(classes, &arena, 4, caller, &ids, &graphics));
    CHECK(mc_client_ge_d(classes, &arena, 4, caller, &output, &graphics));
    for (int i = 0; i < 4; ++i) {
        CHECK(mc_client_ge_int_put(ids, 0x13579bdf, caller, &graphics));
        CHECK(mc_client_ge_int_put(output, 0x2468ace, caller, &graphics));
    }
    CHECK(mc_client_ge_int_position(ids, 1, caller, &graphics));
    CHECK(mc_client_ge_int_limit(ids, 3, caller, &graphics));
    CHECK(mc_client_ge_int_writable_remaining(ids, &storage, &remaining, caller, &graphics) &&
          remaining == 2);
    if (!supported) {
        CHECK(!mc_client_display_source_gen_queries(display, storage, remaining, &names, &error) &&
              names == NULL && error.status == MC_CLIENT_DISPLAY_UNSUPPORTED_GL);
        CHECK(word(ids, 0) == UINT32_C(0x13579bdf) && word(ids, 3) == UINT32_C(0x13579bdf));
        result = 0;
        goto cleanup;
    }
    CHECK(mc_client_display_source_gen_queries(display, storage, remaining, &names, &error));
    display->occlusion.GetQueryiv((GLenum)35092, (GLenum)34916, &counter_bits);
    CHECK(counter_bits >= 0);
    CHECK(ids->position == 1 && ids->limit == 3 && word(ids, 0) == UINT32_C(0x13579bdf) &&
          word(ids, 3) == UINT32_C(0x13579bdf));
    CHECK(mc_client_ge_int_get(ids, 1, &id, caller, &graphics) && id != 0);
    CHECK(span(output, &storage, &remaining, &graphics) && remaining == 1);
    const uint32_t sentinel = word(output, 1);
    CHECK(!mc_client_display_source_query_result(display, names, id, 34919, storage, 0, &error) &&
          error.status == MC_CLIENT_DISPLAY_BUFFER_TOO_SMALL && word(output, 1) == sentinel &&
          output->position == 1 && output->limit == 2);

    /* A genuine sticky enum error survives only these actual query wrappers.
     * There is no rendering/gl_check operation in this control. */
    CHECK(display->gl.GetError() == GL_NO_ERROR);
    display->gl.Enable((GLenum)UINT32_MAX);
    CHECK(mc_client_display_source_begin_query(display, names, id, &query, &error));
    GLint current_id = 0;
    display->occlusion.GetQueryiv((GLenum)35092, (GLenum)34917, &current_id);
    CHECK((uint32_t)current_id == (uint32_t)id && query_ended(query, false));
    CHECK(!mc_client_display_source_begin_query(display, names, id, &second_query, &error) &&
          error.status == MC_CLIENT_DISPLAY_BUSY && second_query == NULL);
    CHECK(!mc_client_display_query_names_retire(&names, &error) && names != NULL &&
          error.status == MC_CLIENT_DISPLAY_BUSY);
    CHECK(!mc_client_display_destroy(display, &error) && error.status == MC_CLIENT_DISPLAY_BUSY);
    CHECK(mc_client_display_source_end_query(query, &error) && query_ended(query, true));
    CHECK(!mc_client_display_source_end_query(query, &error) &&
          error.status == MC_CLIENT_DISPLAY_INVALID_ARGUMENT && query_ended(query, true));
    CHECK(mc_client_display_source_query_result(display, names, id, 34919, storage, remaining,
                                                &error));
    initial_available = word(output, 1);
    CHECK(initial_available <= 1 && output->position == 1 && output->limit == 2 &&
          word(output, 0) == UINT32_C(0x02468ace) && word(output, 2) == UINT32_C(0x02468ace) &&
          word(output, 3) == UINT32_C(0x02468ace));
    CHECK(mc_client_display_source_query_result(display, names, id, 34918, storage, remaining,
                                                &error) &&
          word(output, 1) == 0);
    CHECK(display->gl.GetError() == GL_INVALID_ENUM && display->gl.GetError() == GL_NO_ERROR);
    CHECK(!mc_client_display_query_names_retire(&names, &error) && names != NULL &&
          error.status == MC_CLIENT_DISPLAY_BUSY);
    CHECK(mc_client_display_active_query_retire(&query, &error) && query == NULL);
    mc_client_display_query_state name_state;
    CHECK(mc_client_display_query_names_inspect(names, &name_state) && name_state.query_holds == 0);

    /* Declared native admission fault, not a simulated GPU/query result.
     * A pending handle is published before source_enter rejects this flag.
     * Its cleanup must release rights without issuing an unmatched GL End. */
    display->context_lost = true;
    const bool rejected_begin =
        mc_client_display_source_begin_query(display, names, id, &query, &error);
    display->context_lost = false;
    mc_client_display_active_query_state pending_state;
    CHECK(!rejected_begin && query != NULL && error.status == MC_CLIENT_DISPLAY_CONTEXT_LOST &&
          mc_client_display_active_query_inspect(query, &pending_state) &&
          !pending_state.begin_issued && !pending_state.source_ended &&
          !pending_state.native_aborted);
    CHECK(!mc_client_display_query_names_retire(&names, &error) &&
          error.status == MC_CLIENT_DISPLAY_BUSY);
    CHECK(mc_client_display_active_query_retire(&query, &error) && query == NULL);
    current_id = -1;
    display->occlusion.GetQueryiv((GLenum)35092, (GLenum)34917, &current_id);
    CHECK(current_id == 0 && display->gl.GetError() == GL_NO_ERROR);

    /* Begin survived a real native list failure, then context unavailability.
     * Checked retirement ends the owning context's query as native abort. */
    CHECK(mc_client_display_source_begin_query(display, names, id, &query, &error));
    const int32_t undefined = INT32_MAX;
    CHECK(!mc_client_display_lists_call(display, &undefined, 1, &error) &&
          error.status == MC_CLIENT_DISPLAY_UNSUPPORTED_GL && query_ended(query, false));
    CHECK(mc_client_display_create("actual other query context", 16, 16, true, &other, &error));
    CHECK(!mc_client_display_source_end_query(query, &error) &&
          error.status == MC_CLIENT_DISPLAY_INVALID_ARGUMENT && query_ended(query, false));
    CHECK(!mc_client_display_source_query_result(display, names, id, 34919, storage, remaining,
                                                 &error) &&
          error.status == MC_CLIENT_DISPLAY_INVALID_ARGUMENT && word(output, 1) == 0);
    CHECK(mc_client_display_active_query_retire(&query, &error) && query == NULL);
    CHECK(SDL_GL_GetCurrentContext() == display->context);
    current_id = -1;
    display->occlusion.GetQueryiv((GLenum)35092, (GLenum)34917, &current_id);
    CHECK(current_id == 0 && mc_client_display_query_names_inspect(names, &name_state) &&
          name_state.query_holds == 0);

    /* Real source-format geometry exercises asynchronous availability and the
     * unsigned samples result. The first availability value is observed, not
     * forced: a sufficiently fast implementation may already report1. */
    const int32_t count = 8190;
    const size_t bytes = (size_t)count * 32;
    vertices = calloc(1, bytes);
    CHECK(vertices != NULL);
    const float positions[3][3] = {{-1, -1, 0}, {1, -1, 0}, {-1, 1, 0}};
    for (int32_t i = 0; i < count; ++i)
        memcpy(vertices + (size_t)i * 32, positions[i % 3], sizeof(positions[0]));
    CHECK(mc_client_display_context_acquire(display, &lease, &error));
    CHECK(mc_client_display_source_pointer(lease, MC_CLIENT_SOURCE_VERTEX_ARRAY, vertices, 0,
                                           &error));
    CHECK(mc_client_display_source_array(display, MC_CLIENT_SOURCE_VERTEX_ARRAY, true, &error));
    display->gl.Viewport(0, 0, 64, 64);
    display->gl.Disable(GL_DEPTH_TEST);
    display->gl.Disable(GL_CULL_FACE);
    display->gl.Disable(GL_SCISSOR_TEST);
    display->gl.Disable(GL_BLEND);
    CHECK(mc_client_display_source_color_mask(display, false, false, false, false, &error));
    CHECK(mc_client_display_source_begin_query(display, names, id, &query, &error));
    CHECK(mc_client_display_source_batch(lease, vertices, vertices, bytes, count, GL_TRIANGLES,
                                         &error));
    CHECK(mc_client_display_source_end_query(query, &error));
    CHECK(mc_client_display_source_query_result(display, names, id, 34919, storage, remaining,
                                                &error));
    visible_available = word(output, 1);
    CHECK(visible_available <= 1);
    CHECK(mc_client_display_source_query_result(display, names, id, 34918, storage, remaining,
                                                &error));
    visible = word(output, 1);
    CHECK((counter_bits == 0 ? visible == 0 : visible > 0) && output->position == 1 &&
          output->limit == 2 && word(output, 0) == UINT32_C(0x02468ace) &&
          word(output, 2) == UINT32_C(0x02468ace) && word(output, 3) == UINT32_C(0x02468ace));
    CHECK(mc_client_display_source_query_result(display, names, id, 34919, storage, remaining,
                                                &error) &&
          word(output, 1) == 1);
    CHECK(mc_client_display_active_query_retire(&query, &error) && query == NULL);
    CHECK(display->gl.GetError() == GL_NO_ERROR);
    result = 0;
cleanup:
    if (result != 0)
        fprintf(stderr, "Display status%d: %s (%s)\n", (int)error.status,
                error.operation == NULL ? "none" : error.operation, error.detail);
    if (!mc_client_display_active_query_retire(&query, &error))
        result = 1;
    if (!mc_client_display_active_query_retire(&second_query, &error))
        result = 1;
    if (!mc_client_display_query_names_retire(&names, &error))
        result = 1;
    if (!mc_client_display_context_release(&lease, &error))
        result = 1;
    free(vertices);
    mc_client_render_arena_free(&arena);
    if (classes != NULL &&
        mc_client_render_engine_classes_release(classes) != MC_CLIENT_RENDER_ENGINE_OK)
        result = 1;
    if (!mc_client_display_destroy(other, &error))
        result = 1;
    if (!mc_client_display_destroy(display, &error))
        result = 1;
    SDL_Quit();
    if (result == 0)
        printf("ARB query device: %u checks PASS capability=%d empty_available=%" PRIu32
               " visible_available=%" PRIu32 " visible_samples=%" PRIu32 " counter_bits=%d\n",
               checks, supported, initial_available, visible_available, visible, counter_bits);
    return result;
}
