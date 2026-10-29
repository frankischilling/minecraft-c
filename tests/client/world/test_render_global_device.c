/* Reuse the actual whole-App fixture and its concrete services, not its main
 * dispatcher or fabricated READY children. The startup prefix publishes its
 * own g before this control constructs a separate callee owner. */
int mc_render_global_fixture_main(int argc, char **argv);
#define main mc_render_global_fixture_main
#include "../startup/test_application_startup.c"
#undef main

#include "client/render_engine/internal.h"
#include "client/world/render_global_internal.h"

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
          startup.error->caller.pc == 697 && app->S != NULL && app->g != NULL);
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
          attempt->last_startup_store == 676);
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
