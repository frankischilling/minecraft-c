/* Reuse the actual whole-App fixture and its concrete services, not its main
 * dispatcher or fabricated READY children. The startup prefix publishes its
 * own g before this control constructs a separate callee owner. */
int mc_render_global_fixture_main(int argc, char **argv);
#define main mc_render_global_fixture_main
#include "../startup/test_application_startup.c"
#undef main

#include "client/display/camera_surface.h"
#include "client/display/fog_surface.h"
#include "client/display/source_surface.h"
#include "client/render_engine/internal.h"
#include "client/world/render_batch_internal.h"
#include "client/world/render_global_internal.h"
#include "mc/client/world/render_global_submit.h"

#include <xmmintrin.h>

static uint64_t replay_double_bits(double value) {
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static double replay_unused_snan(void) {
    const uint64_t bits = UINT64_C(0x7ff0000000000001);
    double value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static int raw_replay_control(mc_client_render_global *owner,
                              mc_client_render_global_error *error) {
    CHECK(owner->phase == MC_CLIENT_RENDER_GLOBAL_RAW && owner->fields.T == NULL &&
          owner->fields.t == NULL && owner->display == NULL);
    unsigned char fields[sizeof(owner->fields)];
    memcpy(fields, &owner->fields, sizeof(fields));
    const size_t holds = owner->holds;
    mc_client_render_global_submit_call *call = NULL;
    const unsigned int saved = _mm_getcsr(), unsupported = saved | 0x8000u;
    _mm_setcsr(unsupported);
    const mc_client_render_global_status status =
        mc_client_render_global_replay_begin(owner, INT32_MIN, replay_unused_snan(), &call, error);
    const unsigned int observed = _mm_getcsr();
    _mm_setcsr(saved); /* Balanced even if a later assertion fails. */
    mc_client_render_global_submit_state state;
    CHECK(status == MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE && observed == unsupported &&
          call != NULL && mc_client_render_global_submit_inspect(call, &state) && !state.active &&
          !state.returned && !state.retiring && state.source_pc == 9 && state.frame_count == 1 &&
          state.frames[0].pc == 9 && state.last_store_pc == 0 &&
          strcmp(state.frames[0].descriptor, "(ID)V") == 0 && state.failure != error &&
          state.failure->status == status && !state.failure->native_policy &&
          !state.failure->source_returned && state.failure->source_pc == 9 &&
          strcmp(state.failure->source_descriptor, "(ID)V") == 0 &&
          state.failure->child.class_name == NULL && state.camera == NULL &&
          state.batch_camera == NULL && state.math == NULL && state.rebuild == NULL &&
          state.pass == INT32_MIN &&
          replay_double_bits(state.partial) == UINT64_C(0x7ff0000000000001) &&
          state.dirty_polls == 0 && state.subsets == 0 && state.selected_chunks == 0 &&
          state.drawn_batches == 0 && owner->submit_call == call && owner->holds == holds + 1 &&
          memcmp(fields, &owner->fields, sizeof(fields)) == 0);
    mc_client_render_global_submit_call *other = NULL;
    CHECK(mc_client_render_global_replay_begin(owner, 0, 0, &other, error) ==
              MC_CLIENT_RENDER_GLOBAL_BUSY &&
          other == NULL);
    CHECK(mc_client_render_global_submit_begin(owner, NULL, 0, 0, NULL, &other, error) ==
              MC_CLIENT_RENDER_GLOBAL_BUSY &&
          other == NULL && state.failure->source_pc == 9 &&
          state.failure->status == MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE);
    mc_client_render_global *blocked = owner;
    CHECK(mc_client_render_global_retire(&blocked, error) == MC_CLIENT_RENDER_GLOBAL_BUSY &&
          blocked == owner);
    CHECK(mc_client_render_global_submit_retire(&call, error) == MC_CLIENT_RENDER_GLOBAL_OK &&
          call == NULL && owner->submit_call == NULL && owner->holds == holds &&
          owner->phase == MC_CLIENT_RENDER_GLOBAL_RAW &&
          memcmp(fields, &owner->fields, sizeof(fields)) == 0);
    CHECK(mc_client_render_global_submit_retire(&call, error) == MC_CLIENT_RENDER_GLOBAL_OK);
    return 0;
}

static bool replay_batch_preserved(const mc_client_render_batch_state *a,
                                   const mc_client_render_batch_state *b) {
    return a->phase == b->phase && a->a == b->a && a->b == b->b && a->c == b->c &&
           memcmp(&a->d, &b->d, sizeof(a->d)) == 0 && memcmp(&a->e, &b->e, sizeof(a->e)) == 0 &&
           memcmp(&a->f, &b->f, sizeof(a->f)) == 0 && a->g == b->g && a->h == b->h &&
           a->i == b->i && a->last_store_pc == b->last_store_pc &&
           a->matrix_pushed == b->matrix_pushed;
}

static int source_replay_controls(mc_client_render_global *owner,
                                  mc_client_render_global_error *error) {
    CHECK(owner->phase == MC_CLIENT_RENDER_GLOBAL_READY && owner->source_returned &&
          owner->t_length == 4 && owner->fields.T != NULL && owner->submit_call == NULL);
    unsigned char fields[sizeof(owner->fields)];
    memcpy(fields, &owner->fields, sizeof(fields));
    mc_client_render_batch_state batches[4];
    unsigned char *contents[4] = {0};
    for (size_t i = 0; i < 4; ++i) {
        CHECK(owner->fields.T[i] != NULL);
        batches[i] = owner->fields.T[i]->state;
        const mc_client_direct_int_buffer *buffer = batches[i].g;
        CHECK(!batches[i].h && !batches[i].i && buffer != NULL && buffer->capacity == 65536 &&
              buffer->position == 0 && buffer->limit == buffer->capacity);
        contents[i] = malloc(buffer->capacity * sizeof(int32_t));
        CHECK(contents[i] != NULL);
        memcpy(contents[i], buffer->bytes->data, buffer->capacity * sizeof(int32_t));
    }
    float projection[16], modelview[16], after_projection[16], after_modelview[16];
    CHECK(mc_client_display_camera_matrices_inspect(owner->display, projection, modelview));
    mc_client_render_global_submit_call *call = NULL;
    const unsigned int saved = _mm_getcsr(), unsupported = saved | 0x8000u;
    _mm_setcsr(unsupported);
    const mc_client_render_global_status status =
        mc_client_render_global_replay_begin(owner, INT32_MAX, replay_unused_snan(), &call, error);
    const unsigned int observed = _mm_getcsr();
    _mm_setcsr(saved);
    mc_client_render_global_submit_state state;
    CHECK(status == MC_CLIENT_RENDER_GLOBAL_OK && observed == unsupported && call != NULL &&
          mc_client_render_global_submit_inspect(call, &state) && state.returned &&
          state.source_pc == 29 && state.frame_count == 0 && state.failure->source_returned &&
          state.camera == NULL && state.batch_camera == NULL && state.math == NULL &&
          state.rebuild == NULL && state.pass == INT32_MAX &&
          replay_double_bits(state.partial) == UINT64_C(0x7ff0000000000001) &&
          state.dirty_polls == 0 && state.queued_chunks == 0 && state.subsets == 0 &&
          state.selected_chunks == 0 && state.configured_batches == 0 &&
          state.appended_lists == 0 && state.drawn_batches == 4 && state.query_polls == 0 &&
          state.queries_issued == 0 && !state.matrix_pushed && !state.query_active &&
          state.last_store_pc == 0 && memcmp(fields, &owner->fields, sizeof(fields)) == 0);
    for (size_t i = 0; i < 4; ++i) {
        const mc_client_render_batch_state *batch = &owner->fields.T[i]->state;
        const mc_client_direct_int_buffer *buffer = batch->g;
        CHECK(batch->source_pc == 7 && replay_batch_preserved(batch, &batches[i]) &&
              buffer->position == 0 && buffer->limit == buffer->capacity &&
              memcmp(contents[i], buffer->bytes->data, buffer->capacity * sizeof(int32_t)) == 0);
    }
    CHECK(mc_client_display_camera_matrices_inspect(owner->display, after_projection,
                                                    after_modelview) &&
          memcmp(projection, after_projection, sizeof(projection)) == 0 &&
          memcmp(modelview, after_modelview, sizeof(modelview)) == 0);

    /* Actual authored list input, real constructor-created TG and canonical
     * TG methods under the retained call's actual N guard. No N array, phase,
     * hold, counter or cursor is installed. The inspected borrowed TG views
     * expire on these source input mutations before that receipt is retired. */
    mc_client_display_error display_error;
    mc_client_display_list_range *range = NULL;
    int32_t id;
    CHECK(mc_client_display_lists_generate(owner->display, 1, 16, &range, &id, &display_error) &&
          mc_client_display_lists_begin(owner->display, id, &display_error) &&
          mc_client_display_source_color(owner->display, .25f, .5f, .75f, 1, &display_error) &&
          mc_client_display_source_translate(owner->display, .125f, .25f, .5f, &display_error) &&
          mc_client_display_lists_end(owner->display, &display_error));
    mc_client_render_batch *active = owner->fields.T[0];
    CHECK(mc_render_batch_configure(owner, active, 0, 0, 0, 0, 0, 0, error) &&
          mc_render_batch_append(owner, active, id, error) && active->state.h && !active->state.i &&
          active->state.g == batches[0].g && active->state.g->position == 1 &&
          active->state.g->limit == active->state.g->capacity);
    memcpy(contents[0], active->state.g->bytes->data, active->state.g->capacity * sizeof(int32_t));
    CHECK(mc_client_render_global_submit_retire(&call, error) == MC_CLIENT_RENDER_GLOBAL_OK &&
          call == NULL);
    for (unsigned int iteration = 0; iteration < 2; ++iteration) {
        CHECK(mc_client_display_source_color(owner->display, 1, 0, 0, 1, &display_error));
        CHECK(mc_client_render_global_replay_begin(owner, iteration == 0 ? INT32_MIN : INT32_MAX,
                                                   replay_unused_snan(), &call,
                                                   error) == MC_CLIENT_RENDER_GLOBAL_OK &&
              mc_client_render_global_submit_inspect(call, &state) && state.returned &&
              state.source_pc == 29 && state.frame_count == 0 && state.drawn_batches == 4 &&
              state.dirty_polls == 0 && state.subsets == 0 && state.configured_batches == 0 &&
              state.appended_lists == 0 && state.camera == NULL && state.math == NULL &&
              memcmp(fields, &owner->fields, sizeof(fields)) == 0 && active->state.i &&
              active->state.source_pc == 84 && !active->state.matrix_pushed &&
              active->state.g->position == 0 && active->state.g->limit == 1);
        float color[5] = {-1, -1, -1, -1, 19};
        CHECK(mc_client_display_source_current_attribute(owner->display, 2816, color, 4,
                                                         &display_error) &&
              color[0] == .25f && color[1] == .5f && color[2] == .75f && color[3] == 1 &&
              color[4] == 19 &&
              mc_client_display_camera_matrices_inspect(owner->display, after_projection,
                                                        after_modelview) &&
              memcmp(projection, after_projection, sizeof(projection)) == 0 &&
              memcmp(modelview, after_modelview, sizeof(modelview)) == 0);
        for (size_t i = 0; i < 4; ++i) {
            const mc_client_direct_int_buffer *buffer = owner->fields.T[i]->state.g;
            CHECK(buffer == batches[i].g && memcmp(contents[i], buffer->bytes->data,
                                                   buffer->capacity * sizeof(int32_t)) == 0);
        }
        CHECK(mc_client_render_global_submit_retire(&call, error) == MC_CLIENT_RENDER_GLOBAL_OK &&
              call == NULL);
    }
    CHECK(mc_client_display_lists_retire(&range, &display_error) && range == NULL);
    for (size_t i = 0; i < 4; ++i)
        free(contents[i]);
    return 0;
}

/* Actual nw b=true expands [0,1,2,3] to [0,1,2,0,2,3]. This
 * position-only profile does not copy or consume disabled attribute slots. */
static int triangle_copies(const mc_client_source_batch_view *batch) {
    CHECK(batch != NULL && batch->vertices > 0 && batch->vertices % 6 == 0 && batch->mode == 4 &&
          batch->bytes != NULL && batch->byte_count == (size_t)batch->vertices * 32 &&
          batch->arrays == (1u << MC_CLIENT_SOURCE_VERTEX_ARRAY));
    for (size_t first = 0; first < (size_t)batch->vertices; first += 6) {
        CHECK(memcmp(batch->bytes + first * 32, batch->bytes + (first + 3) * 32, 12) == 0);
        CHECK(memcmp(batch->bytes + (first + 2) * 32, batch->bytes + (first + 4) * 32, 12) == 0);
    }
    return 0;
}

static int global_device(const char *resources, const char *assets) {
    fixture f;
    startup_fixture startup;
    CHECK(fixture_open(&f, resources) == 0);
    CHECK(startup_services(&startup) == 0);
    CHECK(startup_icons(&startup, assets) == 0);
    client_application *app = NULL;
    CHECK(actual_app(&f, false, &app) == 0);
    CHECK(statistics_session(app) == 0);
    mc_client_application_startup *attempt = NULL;
    mc_client_startup_options options = mc_client_application_startup_default_options();
    options.hidden = true;
    CHECK(mc_client_application_startup_allocate(app, &startup.services, &options, &attempt,
                                                 startup.error) == MC_CLIENT_STARTUP_OK);
    CHECK(mc_client_application_run_prefix(attempt, startup.error) ==
              MC_CLIENT_STARTUP_UNIMPLEMENTED &&
          startup.error->caller.pc == 32 && app->S != NULL && app->g != NULL);
    CHECK(statistics_retire_resources(attempt) == 0);
    mc_client_render_global *published_global = app->g;
    const size_t scene_before = f.graphics->scene_lease_count;
    CHECK(scene_before == 1);
    const size_t ledger_before = f.graphics->a->count;
    mc_client_render_global *owner = NULL;
    mc_client_render_global_error *error = calloc(1, sizeof(*error));
    CHECK(error != NULL);
    CHECK(mc_client_render_global_allocate(f.graphics, NULL, &owner, error) ==
          MC_CLIENT_RENDER_GLOBAL_OK);
    CHECK(raw_replay_control(owner, error) == 0);
    typedef void(APIENTRY * enable_fn)(GLenum);
    enable_fn enable = NULL;
    const SDL_FunctionPointer p = SDL_GL_GetProcAddress("glEnable");
    _Static_assert(sizeof(enable) == sizeof(p), "actual SDL function pointer bridge");
    memcpy(&enable, &p, sizeof(enable));
    CHECK(enable != NULL);
    enable((GLenum)UINT32_MAX); /* Actual sticky error, never a fake return/provider. */
    CHECK(mc_client_display_source_viewport(app->display, app->d, app->e, &error->display));
    typedef void(APIENTRY * get_integer_fn)(GLenum, GLint *);
    get_integer_fn get_integer = NULL;
    const SDL_FunctionPointer get_symbol = SDL_GL_GetProcAddress("glGetIntegerv");
    _Static_assert(sizeof(get_integer) == sizeof(get_symbol), "actual GL query pointer bridge");
    memcpy(&get_integer, &get_symbol, sizeof(get_integer));
    CHECK(get_integer != NULL);
    GLint rectangle[4] = {0};
    get_integer(GL_VIEWPORT, rectangle);
    CHECK(rectangle[0] == 0 && rectangle[1] == 0 && rectangle[2] == app->d &&
          rectangle[3] == app->e);
    CHECK(mc_client_render_global_construct(owner, app, app->render_engine, app->display, error) ==
          MC_CLIENT_RENDER_GLOBAL_OK);
    uint32_t pending = 0;
    CHECK(mc_client_display_source_get_error(app->display, &pending, &error->display) &&
          pending == GL_INVALID_ENUM); /* Constructor performed no GetError/drain. */
    mc_client_render_global_state state;
    CHECK(mc_client_render_global_inspect(owner, &state));
    CHECK(state.phase == MC_CLIENT_RENDER_GLOBAL_READY && state.source_returned &&
          state.last_constructor_pc == 651 && state.last_store_pc == 489 && state.star_pc == 412 &&
          !state.list_open && !state.matrix_pushed && state.fields.t == app &&
          state.fields.l == app->render_engine && state.fields.k == NULL &&
          state.fields.u == NULL && state.fields.n == NULL && state.fields.o == NULL &&
          state.fields.H == -1 && state.fields.I == 2 && state.fields.T != NULL &&
          state.published_batches == 4 && state.constructed_batches == 4 &&
          state.pending_T == NULL && state.pending_batch == NULL && state.star_random_created &&
          state.star_attempts == 1500 && state.accepted_stars > 0 &&
          state.star_vertices == state.accepted_stars * 4 && state.top_quads == 169 &&
          state.bottom_quads == 169 && state.fields.z == state.fields.y + 1 &&
          state.fields.A == state.fields.y + 2 && state.fields.e != state.fields.s &&
          state.fields.s != state.fields.y && f.graphics->a->count == ledger_before + 6 &&
          f.graphics->scene_lease_count == scene_before + 1 && app->g == published_global &&
          attempt->last_startup_store == 705);
    if (state.fields.w) {
        mc_client_display_query_state queries;
        CHECK(state.queries_issued && state.fields.v != NULL && owner->queries != NULL &&
              state.fields.v->position == 0 && state.fields.v->limit == 262144 &&
              state.fields.v->capacity == 262144 && state.fields.c->position == 0 &&
              state.fields.c->limit == 64 &&
              mc_client_display_query_names_inspect(owner->queries, &queries) && queries.issued &&
              !queries.deleted && queries.count == 262144);
    } else
        CHECK(!state.queries_issued && state.fields.v == NULL && owner->queries == NULL);
    CHECK(f.graphics->nw.b); /* Original nw <clinit>1, not a native option. */
    const size_t star_triangle_vertices = state.accepted_stars * 6;
    mc_client_source_list_state stars, top, bottom;
    CHECK(mc_client_display_list_inspect(app->display, state.fields.y, &stars) && stars.defined &&
          !stars.building && stars.draws == 1 && stars.vertices == star_triangle_vertices);
    CHECK(mc_client_display_list_inspect(app->display, state.fields.z, &top) && top.defined &&
          !top.building && top.draws == 169 && top.vertices == 1014);
    CHECK(mc_client_display_list_inspect(app->display, state.fields.A, &bottom) && bottom.defined &&
          !bottom.building && bottom.draws == 1 && bottom.vertices == 1014);
    mc_client_source_batch_view batch;
    CHECK(mc_client_display_list_batch(app->display, state.fields.y, 0, &batch) &&
          batch.vertices == (int32_t)star_triangle_vertices && batch.mode == 4 &&
          batch.byte_count == star_triangle_vertices * 32 && batch.bytes != NULL);
    CHECK(triangle_copies(&batch) == 0);
    /* Inspect actual immutable nw records, not regenerated geometry answers. */
    for (size_t i = 0; i < star_triangle_vertices; ++i) {
        for (size_t component = 0; component < 3; ++component) {
            uint32_t bits;
            float position;
            memcpy(&bits, batch.bytes + i * 32 + component * 4, sizeof(bits));
            memcpy(&position, &bits, sizeof(position));
            CHECK((bits & UINT32_C(0x7f800000)) != UINT32_C(0x7f800000) && position >= -101.0f &&
                  position <= 101.0f);
        }
    }
    for (size_t quad = 0; quad < 169; ++quad) {
        CHECK(mc_client_display_list_batch(app->display, state.fields.z, quad, &batch) &&
              batch.vertices == 6 && triangle_copies(&batch) == 0);
    }
    CHECK(mc_client_display_list_batch(app->display, state.fields.A, 0, &batch) &&
          batch.vertices == 1014 && triangle_copies(&batch) == 0);
    CHECK(source_replay_controls(owner, error) == 0);
    mc_client_render_texture_error *terminal = calloc(1, sizeof(*terminal));
    CHECK(terminal != NULL);
    CHECK(mc_client_render_engine_retire_resources(f.graphics, terminal) ==
              MC_CLIENT_RENDER_TEXTURE_BUSY &&
          !f.graphics->resource_retirement_started);
    mc_client_render_global *external = owner;
    CHECK(mc_client_render_global_retain(external, error) == MC_CLIENT_RENDER_GLOBAL_OK);
    CHECK(mc_client_render_global_retire(&owner, error) == MC_CLIENT_RENDER_GLOBAL_BUSY &&
          owner == external && f.graphics->scene_lease_count == scene_before + 1);
    CHECK(mc_client_render_global_release(&owner, error) == MC_CLIENT_RENDER_GLOBAL_OK &&
          owner == NULL && f.graphics->scene_lease_count == scene_before + 1);
    CHECK(mc_client_render_global_release(&external, error) == MC_CLIENT_RENDER_GLOBAL_OK &&
          external == NULL && f.graphics->scene_lease_count == scene_before &&
          app->g == published_global);
    /* A genuine independent namespace shares THIS Display allocator. Null
     * App is reached only after its real buffers/ranges/source t/l stores.
     * End the primary class occurrence first, exercising retryable final-class
     * cleanup without inventing an App, resetting state or replaying source. */
    mc_client_render_engine_classes *other_classes = NULL;
    CHECK(mc_client_render_engine_classes_create(NULL, &other_classes) ==
          MC_CLIENT_RENDER_ENGINE_OK);
    mc_client_render_global *failed = NULL;
    CHECK(mc_client_render_global_allocate(other_classes, NULL, &failed, error) ==
          MC_CLIENT_RENDER_GLOBAL_OK);
    CHECK(mc_client_render_global_construct(failed, NULL, NULL, app->display, error) ==
              MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE &&
          error->source_pc == 190 && failed->last_store_pc == 185 && failed->fields.t == NULL &&
          failed->fields.l == NULL && failed->fields.T != NULL &&
          failed->fields.s > state.fields.A && failed->fields.e > state.fields.A &&
          !failed->source_returned && failed->star_random == NULL && failed->queries == NULL);
    CHECK(mc_client_render_engine_classes_release(other_classes) == MC_CLIENT_RENDER_ENGINE_OK);
    other_classes = NULL;
    CHECK(mc_client_render_global_release(&failed, error) == MC_CLIENT_RENDER_GLOBAL_BUSY &&
          failed != NULL && failed->phase == MC_CLIENT_RENDER_GLOBAL_RETIRING &&
          !failed->scene_guard && failed->classes->scene_lease_count == 0 &&
          failed->fields.T != NULL && failed->failure.source_pc == 190);
    CHECK(mc_client_render_global_retain(failed, error) == MC_CLIENT_RENDER_GLOBAL_BUSY);
    CHECK(mc_client_render_engine_retire_resources(failed->classes, terminal) ==
          MC_CLIENT_RENDER_TEXTURE_OK);
    CHECK(mc_client_render_global_release(&failed, error) == MC_CLIENT_RENDER_GLOBAL_OK &&
          failed == NULL);
    free(error);
    mc_client_render_manager_classes *manager = attempt->manager_classes;
    CHECK(mc_client_application_startup_retire(&attempt, startup.error) == MC_CLIENT_STARTUP_BUSY);
    CHECK(mc_client_render_manager_retire_hands(manager, &startup.error->manager) ==
          MC_CLIENT_MANAGER_OK);
    CHECK(mc_client_render_manager_classes_retire(&manager, &startup.error->manager) ==
          MC_CLIENT_MANAGER_OK);
    CHECK(mc_client_application_startup_retire(&attempt, startup.error) == MC_CLIENT_STARTUP_BUSY);
    CHECK(mc_client_render_engine_retire_resources(f.graphics, terminal) ==
          MC_CLIENT_RENDER_TEXTURE_OK);
    free(terminal);
    CHECK(mc_client_application_startup_retire(&attempt, startup.error) == MC_CLIENT_STARTUP_OK &&
          attempt == NULL);
    CHECK(statistics_end_app(&f, &startup, &app) == 0);
    return 0;
}

int main(int argc, char **argv) {
    CHECK(argc == 3);
    CHECK(global_device(argv[1], argv[2]) == 0);
    printf("Actual RenderGlobal device contracts: %u checks\n", checks);
    return 0;
}
