/* Whole N submission on genuine owners/device; prepared graph inputs. */
#include "client/application_state.h"
#include "client/display/camera_surface.h"
#include "client/display/fog_surface.h"
#include "client/display/startup_surface.h"
#include "client/display/world_frame_surface.h"
#include "client/render/cx.h"
#include "client/render_engine/internal.h"
#include "client/world/render_batch_internal.h"
#include "client/world/source_terrain_internal.h"
#include "entity/entity_internal.h"
#include "mc/entity/player/player.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int checks;
#define CHECK(value)                                                                               \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(value)) {                                                                            \
            fprintf(stderr, "N submit check%u at%d: %s\n", checks, __LINE__, #value);              \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)

static const char *const names[] = {"empty_pass0",      "groups_pass0",     "groups_pass1",
                                    "counter_priority", "visibility_pass1", "null_current_view",
                                    "occlusion16"};

static uint32_t bits(float value) {
    uint32_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static uint64_t buffer_hash(const mc_client_direct_int_buffer *b) {
    uint64_t result = UINT64_C(0xcbf29ce484222325);
    for (size_t i = 0; i < b->capacity; ++i) {
        uint32_t word;
        memcpy(&word, b->bytes->data + i * sizeof(word), sizeof(word));
        result = (result ^ word) * UINT64_C(1099511628211);
    }
    return result;
}

static void ids(const mc_client_render_list *list, mc_client_source_dk *const actors[3]) {
    if (list->count == 0) {
        putchar('-');
        return;
    }
    for (size_t i = 0; i < list->count; ++i) {
        unsigned int id = 0;
        for (unsigned int j = 0; j < 3; ++j)
            if (list->entries[i] == actors[j])
                id = j + 1;
        CHECK(id != 0);
        if (i != 0)
            putchar(',');
        printf("%u", id);
    }
}

static void define_list(mc_client_render_global *n, int32_t id, int32_t local_x, int32_t pass) {
    mc_client_graphics_error error;
    mc_client_display_error de;
    mc_client_tessellator *t = NULL;
    CHECK(mc_client_display_lists_begin(n->display, id, &de));
    CHECK(mc_client_nw_active_use(
        n->classes, (mc_client_graphics_site){"fixture", "pass_list", "()V", -1}, &t, &error));
    CHECK(mc_client_nw_translation(t, 0, 0, 0, &error));
    CHECK(mc_client_nw_begin_quads(t, &error));
    CHECK(mc_client_nw_color(t, pass == 0 ? 255 : 0, pass == 0 ? 0 : 255, 0, 255, &error));
    CHECK(mc_client_nw_vertex(t, (double)local_x - .5, -.5, 0, &error));
    CHECK(mc_client_nw_vertex(t, (double)local_x + .5, -.5, 0, &error));
    CHECK(mc_client_nw_vertex(t, (double)local_x + .5, .5, 0, &error));
    CHECK(mc_client_nw_vertex(t, (double)local_x - .5, .5, 0, &error));
    CHECK(mc_client_nw_draw(t, n->display, &error));
    CHECK(mc_client_display_lists_end(n->display, &de));
}

static void short_result_control(mc_client_render_global *n, mc_client_source_dk *dk) {
    const mc_client_graphics_site prepared = {"fixture", "zero_result", "()V", -1};
    mc_client_render_global_error e;
    mc_client_direct_int_buffer *zero = NULL;
    CHECK(mc_client_ge_d(n->classes, &n->arena, 0, prepared, &zero, &e.graphics));
    void *storage = NULL;
    size_t remaining = 1;
    CHECK(mc_client_ge_int_writable_remaining(zero, &storage, &remaining, prepared, &e.graphics));
    CHECK(storage != NULL && remaining == 0 && (uintptr_t)storage % _Alignof(uint32_t) == 0);
    CHECK(mc_client_ge_d(n->classes, &n->arena, 1, prepared, &n->fields.v, &e.graphics));
    CHECK(mc_client_ge_int_writable_remaining(n->fields.v, &storage, &remaining, prepared,
                                              &e.graphics));
    CHECK(remaining == 1 && n->queries == NULL);
    CHECK(mc_client_display_source_gen_queries(n->display, storage, remaining, &n->queries,
                                               &e.display));
    int32_t id;
    CHECK(mc_client_ge_int_get(n->fields.v, 0, &id, prepared, &e.graphics));
    mc_client_direct_int_buffer *saved_buffer = n->fields.c;
    const int32_t saved_id = dk->fields.z;
    const bool saved_y = dk->fields.y, saved_x = dk->fields.x;
    n->fields.c = zero;
    dk->fields.z = id;
    dk->fields.y = true;
    for (unsigned int policy = 0; policy < 2; ++policy) {
        mc_client_display_query_names *query_names = n->queries;
        if (policy != 0)
            n->queries = NULL;
        mc_client_render_global_submit_call child = {0};
        child.state.owner = n;
        CHECK(mc_terrain_push(&child, "n", "a", "(Lls;ID)I"));
        n->active = child.state.active = true;
        CHECK(!mc_terrain_poll(&child, 0, 1, 357));
        n->active = child.state.active = false;
        CHECK(child.state.source_pc == 42 && child.depth == 2 && child.frames[0].pc == 357 &&
              child.frames[1].pc == 42 && dk->fields.y && dk->fields.x == saved_x &&
              zero->position == 0 && zero->limit == 0 && child.state.query_polls == 0);
        CHECK(child.failure.status == (policy == 0 ? MC_CLIENT_RENDER_GLOBAL_SOURCE_ILLEGAL_ARGUMENT
                                                   : MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED));
        CHECK(child.failure.native_policy == (policy != 0));
        CHECK(child.failure.display.status == (policy == 0 ? MC_CLIENT_DISPLAY_BUFFER_TOO_SMALL
                                                           : MC_CLIENT_DISPLAY_INVALID_ARGUMENT));
        n->queries = query_names;
    }
    n->fields.c = saved_buffer;
    dk->fields.z = saved_id;
    dk->fields.y = saved_y;
}

int main(int argc, char **argv) {
    if (argc != 2)
        return 1;
    char *end = NULL;
    const long number = strtol(argv[1], &end, 10);
    CHECK(end != argv[1] && *end == '\0' && number >= 0 && number < 7);
    const unsigned int mode = (unsigned int)number;
    const bool query_profile = mode == 6;
    const int32_t pass = mode == 2 || mode == 4 ? 1 : 0;
    const char *name = names[mode];
    mc_java_random_profile *profile = NULL;
    mc_entity_classes *entities = NULL;
    mc_math_classes *math = NULL;
    mc_client_render_engine_classes *graphics = NULL;
    mc_client_display *display = NULL;
    mc_client_display_error de;
    CHECK(mc_java_random_profile_create(SIZE_MAX, &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, profile, NULL, &entities) == MC_ENTITY_OK);
    CHECK(mc_math_classes_create(SIZE_MAX, &math, NULL) == MC_MATH_CLASS_OK);
    CHECK(mc_math_classes_ensure(math, NULL) == MC_MATH_CLASS_OK); /* Explicit fixture priming. */
    CHECK(mc_client_render_engine_classes_create(NULL, &graphics) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_display_create("Source N terrain submission", 64, 64, true, &display, &de));
    mc_client_framebuffer_info format;
    CHECK(mc_client_display_framebuffer_info_get(display, &format, &de));
    CHECK(format.red_bits == 8 && format.green_bits == 8 && format.blue_bits == 8 &&
          format.alpha_bits == 8);
    printf("SUBMIT_FORMAT 8 8 8 8 %d %d\n", format.depth_bits, format.stencil_bits);
    mc_client_cx_classes *cx_classes = NULL;
    mc_client_source_cx *cx = NULL;
    bool capability = false;
    if (query_profile) {
        CHECK(mc_client_cx_classes_create(&cx_classes, NULL) == MC_CLIENT_CX_OK);
        CHECK(mc_client_cx_allocate(cx_classes, &cx, NULL) == MC_CLIENT_CX_OK);
        CHECK(mc_client_cx_construct(cx, NULL) == MC_CLIENT_CX_OK);
        CHECK(mc_client_cx_supported(cx, display, &capability, NULL) == MC_CLIENT_CX_OK);
        printf("SUBMIT_CAPABILITY %s %u\n", name, capability ? 1u : 0u);
    }
    mc_game_settings *settings = NULL;
    CHECK(mc_game_settings_create_defaults(NULL, &settings, NULL) == MC_GAME_SETTINGS_OK);
    mc_game_settings_state settings_state;
    CHECK(mc_game_settings_read_state(settings, &settings_state));
    settings_state.values.render_distance = 3;
    settings_state.values.advanced_opengl = query_profile;
    settings_state.values.anaglyph = false;
    CHECK(mc_game_settings_set_values(settings, &settings_state.values, NULL) ==
          MC_GAME_SETTINGS_OK);
    mc_entity *camera = NULL, *view = NULL;
    CHECK(mc_local_player_allocate(entities, &camera, NULL) == MC_ENTITY_OK);
    CHECK(mc_local_player_allocate(entities, &view, NULL) == MC_ENTITY_OK);
    camera->base.bl = 19;
    camera->base.bm = 29;
    camera->base.bn = 39;
    camera->base.x = 20;
    camera->base.y = 30;
    camera->base.z = 40;
    view->base.x = .5;
    view->base.y = .25;
    view->base.z = -.5;
    client_application *app = calloc(1, sizeof(*app));
    CHECK(app != NULL);
    app->settings = settings;
    app->entities = entities;
    app->i = mode == 5 ? NULL : view;
    mc_client_render_global *n = NULL;
    mc_client_render_global_error error;
    CHECK(mc_client_render_global_allocate(graphics, NULL, &n, &error) ==
          MC_CLIENT_RENDER_GLOBAL_OK);
    n->display = display;
    n->fields.t = app;
    n->fields.H = 3;
    n->fields.f = 20;
    n->fields.g = 30;
    n->fields.h = 40;
    n->fields.R = -1;
    n->fields.w = query_profile ? capability : mode == 4;
    n->fields.M = 11;
    n->fields.N = 12;
    n->fields.O = 13;
    n->fields.P = 14;
    n->fields.Q = 15;
    const mc_client_graphics_site prepared = {"fixture", "submit_inputs", "()V", -1};
    n->fields.m = mc_render_global_allocate(n, sizeof(*n->fields.m), prepared, &error);
    n->fields.S = mc_render_global_allocate(n, sizeof(*n->fields.S), prepared, &error);
    mc_client_render_list *tiles = mc_render_global_allocate(n, sizeof(*tiles), prepared, &error);
    CHECK(n->fields.m != NULL && n->fields.S != NULL && tiles != NULL);
    CHECK(mc_client_ge_d(graphics, &n->arena, 64, prepared, &n->fields.c, &error.graphics));
    n->fields.T = mc_render_global_allocate(n, 4 * sizeof(*n->fields.T), prepared, &error);
    CHECK(n->fields.T != NULL);
    n->t_length = 4;
    for (size_t i = 0; i < 4; ++i) {
        n->fields.T[i] = mc_render_global_allocate(n, sizeof(*n->fields.T[i]), prepared, &error);
        CHECK(n->fields.T[i] != NULL && mc_render_batch_construct(n, n->fields.T[i], 0));
    }
    static const int32_t xs[] = {0, 16, 1024};
    mc_client_source_dk *actors[3];
    mc_client_display_list_range *ranges[3] = {0};
    for (size_t i = 0; i < 3; ++i) {
        int32_t base;
        CHECK(mc_client_display_lists_generate(display, 3, 128, &ranges[i], &base, &de));
        actors[i] = mc_source_dk_allocate(n, &error);
        CHECK(actors[i] != NULL &&
              mc_source_dk_construct(actors[i], NULL, tiles, xs[i], 0, 0, 16, base, &error));
        define_list(n, base, xs[i] & 1023, 0);
        define_list(n, mc_java_i32_add(base, 1), xs[i] & 1023, 1);
        actors[i]->fields.p[0] = false;
        actors[i]->fields.p[1] = i == 2;
        actors[i]->fields.o = !(i == 1 && mode == 3);
        actors[i]->fields.x = !(i == 0 && mode == 4);
        actors[i]->fields.u = i != 1;
        actors[i]->fields.E = true;
        if (i == 2 && mode == 3)
            actors[i]->fields.p[0] = actors[i]->fields.p[1] = true;
    }
    CHECK(mc_render_global_pointer_append(n, n->fields.m, actors[0], prepared, &error));
    n->fields.o = mc_render_global_allocate(n, 3 * sizeof(*n->fields.o), prepared, &error);
    CHECK(n->fields.o != NULL);
    n->o_length = 3;
    memcpy(n->fields.o, actors, sizeof(actors));
    n->n_length = mode == 0 ? 0u : query_profile ? 16u : 3u;
    n->fields.n =
        mc_render_global_allocate(n, n->n_length * sizeof(*n->fields.n), prepared, &error);
    CHECK(n->fields.n != NULL);
    for (size_t i = 0; i < n->n_length; ++i)
        n->fields.n[i] = query_profile ? actors[0] : actors[i];
    mc_client_startup_projection_stage stage;
    CHECK(mc_client_display_startup_projection(display, 64, 64, &stage, &de));
    CHECK(mc_client_display_source_viewport(display, 64, 64, &de));
    CHECK(mc_client_display_source_matrix_mode(display, 5889, &de) &&
          mc_client_display_source_load_identity(display, &de));
    CHECK(mc_client_display_source_matrix_mode(display, 5888, &de) &&
          mc_client_display_source_load_identity(display, &de));
    CHECK(mc_client_display_source_texture_enable(display, false, &de));
    CHECK(mc_client_display_source_disable_unlit_feature(display, MC_CLIENT_SOURCE_LIGHTING, &de));
    CHECK(mc_client_display_source_color_material_enable(display, false, &de));
    CHECK(mc_client_display_source_fog_enable(display, false, &de));
    CHECK(mc_client_display_source_blend_enable(display, false, &de));
    CHECK(mc_client_display_source_alpha_test(display, false, &de));
    CHECK(mc_client_display_source_depth_test(display, false, &de));
    CHECK(mc_client_display_source_cull_test(display, false, &de));
    CHECK(mc_client_display_source_clear_color(display, 0, 0, 0, 0, &de));
    CHECK(mc_client_display_source_clear(display, UINT32_C(0x4100), &de));
    mc_client_render_global_submit_call *call = NULL;
    CHECK(mc_client_render_global_submit_begin(n, camera, pass, .5, math, &call, &error) ==
          (mode == 5 ? MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE : MC_CLIENT_RENDER_GLOBAL_OK));
    mc_client_render_global_submit_state state;
    CHECK(call != NULL && mc_client_render_global_submit_inspect(call, &state));
    CHECK(state.returned == (mode != 5) && state.dirty_polls == 10 && state.queued_chunks == 1 &&
          state.camera == camera && state.batch_camera == app->i && state.render_x == 19.5 &&
          state.render_y == 29.5 && state.render_z == 39.5 && !state.query_active &&
          !state.matrix_pushed);
    CHECK(state.occlusion == (query_profile && capability));
    if (mode == 5) {
        CHECK(state.source_pc == 888 && state.frame_count == 2 && state.frames[1].pc == 235 &&
              !state.failure->native_policy && !state.failure->source_returned);
    } else {
        CHECK(state.source_pc == 896 && state.frame_count == 0 &&
              state.subsets == (query_profile && capability ? 2u : 1u));
    }
    float projection[16], modelview[16];
    CHECK(mc_client_display_camera_matrices_inspect(display, projection, modelview));
    mc_client_capture image = {0};
    CHECK(mc_client_display_capture_rgba(display, &image, &de));
    CHECK(image.width == 64 && image.height == 64);
    const uint8_t *pixel = image.pixels + 32u * image.stride + 32u * 4u;
    printf("SUBMIT %s %s %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32
           " %" PRId32 " ",
           name, state.returned ? "OK" : "NULL", state.returned ? state.result : -1, n->fields.R,
           n->fields.M, n->fields.N, n->fields.O, n->fields.P, n->fields.Q);
    ids(n->fields.m, actors);
    putchar(' ');
    ids(n->fields.S, actors);
    putchar(' ');
    for (size_t i = 0; i < 16; ++i) {
        if (i != 0)
            putchar(',');
        printf("%08" PRIx32, bits(modelview[i]));
    }
    printf(" %u %u %u %u\n", pixel[0], pixel[1], pixel[2], pixel[3]);
    uint64_t hashes[4];
    for (size_t i = 0; i < 4; ++i) {
        const mc_client_render_batch_state *b = &n->fields.T[i]->state;
        const mc_client_direct_int_buffer *g = b->g;
        hashes[i] = buffer_hash(g);
        CHECK(!b->matrix_pushed);
        printf("SUBMIT_TG %s %zu %u %u %" PRId32 " %" PRId32 " %" PRId32 " %08" PRIx32 " %08" PRIx32
               " %08" PRIx32 " %zu %zu %zu %016" PRIx64 "\n",
               name, i, b->h ? 1u : 0u, b->i ? 1u : 0u, b->a, b->b, b->c, bits(b->d), bits(b->e),
               bits(b->f), g->position, g->limit, g->capacity, hashes[i]);
    }
    mc_client_capture_free(&image);
    CHECK(mc_client_render_global_retire(&n, &error) == MC_CLIENT_RENDER_GLOBAL_BUSY);
    CHECK(mc_client_render_global_submit_retire(&call, &error) == MC_CLIENT_RENDER_GLOBAL_OK &&
          call == NULL);
    if (query_profile && capability)
        short_result_control(n, actors[0]);
    for (size_t i = 0; i < 4; ++i)
        CHECK(buffer_hash(n->fields.T[i]->state.g) == hashes[i]);
    for (size_t i = 0; i < 3; ++i)
        CHECK(mc_client_display_lists_retire(&ranges[i], &de));
    CHECK(mc_client_render_global_retire(&n, &error) == MC_CLIENT_RENDER_GLOBAL_OK);
    CHECK(mc_client_render_engine_classes_release(graphics) == MC_CLIENT_RENDER_ENGINE_OK);
    if (cx != NULL)
        CHECK(mc_client_cx_release(&cx, NULL) == MC_CLIENT_CX_OK);
    if (cx_classes != NULL)
        CHECK(mc_client_cx_classes_release(&cx_classes, NULL) == MC_CLIENT_CX_OK);
    CHECK(mc_client_display_destroy(display, &de));
    mc_entity_release(camera);
    mc_entity_release(view);
    CHECK(mc_entity_classes_free(entities) == MC_ENTITY_OK);
    CHECK(mc_math_classes_release(&math, NULL) == MC_MATH_CLASS_OK);
    CHECK(mc_game_settings_retire(&settings, NULL) == MC_GAME_SETTINGS_OK);
    mc_java_random_profile_free(profile);
    free(app);
    printf("Source N submit: %s; %u checks\n", name, checks);
    return 0;
}
