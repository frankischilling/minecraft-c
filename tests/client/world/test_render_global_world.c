/* Reuse the genuine whole-App/startup fixture. These are explicit direct
 * callee controls, not a substitute selected-World/Minecraft caller path. */
int mc_render_world_startup_fixture_main(int argc, char **argv);
#define main mc_render_world_startup_fixture_main
#include "../startup/test_application_startup.c"
#undef main

#include "client/render_engine/chunk_statistics.h"
#include "client/render_engine/internal.h"
#include "client/world/render_global_access.h"
#include "client/world/source_chunk_renderer_internal.h"
#include "mc/client/world/frustum_culling.h"
#include "mc/world/dimension/world_provider.h"
#include "mc/world/runtime/presentation.h"
#include "mc/world/runtime/world_access.h"
#include "world/runtime/construction_internal.h"
#include "world/runtime/runtime_internal.h"
#include "world/runtime/world_access_internal.h"

static int terminal_admission_closed(mc_client_render_global *owner,
                                     mc_client_render_global_error *error) {
    CHECK(owner->phase == MC_CLIENT_RENDER_GLOBAL_RETIRING && owner->world_retiring &&
          owner->terminal_preserve_manager_world && owner->holds == 1);
    CHECK(mc_client_render_global_retain(owner, error) == MC_CLIENT_RENDER_GLOBAL_BUSY &&
          owner->holds == 1);
    mc_client_render_global_world_call *call = NULL;
    CHECK(mc_client_render_global_world_begin(owner, NULL, &call) == MC_CLIENT_RENDER_GLOBAL_BUSY &&
          call == NULL && owner->world_call == NULL);
    mc_client_culling_call *cull = NULL;
    mc_client_culling_error culling = {0};
    CHECK(mc_client_render_global_cull_begin(owner, NULL, 0.0f, &cull, &culling) ==
              MC_CLIENT_FRUSTUM_BUSY &&
          cull == NULL && owner->cull_call == NULL);
    CHECK(mc_render_global_mark_cell(owner, 0, 64, 0, error) == MC_CLIENT_RENDER_GLOBAL_BUSY);
    return 0;
}

static int raw_world_admission(void) {
    mc_client_render_engine_classes *classes = NULL;
    mc_client_render_global *owner = NULL;
    mc_client_render_global_error *error = calloc(1, sizeof(*error));
    CHECK(error != NULL);
    CHECK(mc_client_render_engine_classes_create(NULL, &classes) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_render_global_allocate(classes, NULL, &owner, error) ==
          MC_CLIENT_RENDER_GLOBAL_OK);
    mc_client_render_global_world_call *call = NULL;
    CHECK(mc_client_render_global_world_begin(owner, NULL, &call) == MC_CLIENT_RENDER_GLOBAL_BUSY &&
          call == NULL);
    CHECK(owner->phase == MC_CLIENT_RENDER_GLOBAL_RAW && owner->arena.bytes == 0 &&
          owner->fields.k == NULL && owner->fields.u == NULL && owner->access == NULL &&
          owner->world_call == NULL && !owner->world_retiring);
    mc_client_render_engine_phase phase;
    const mc_client_tessellator *tessellator = NULL;
    int32_t updates = -19;
    CHECK(mc_client_chunk_statistics_inspect(classes, &phase, &tessellator, &updates) &&
          phase == MC_CLIENT_RENDER_ENGINE_RAW && tessellator == NULL && updates == 0);
    CHECK(mc_client_render_global_world_retire(&call) == MC_CLIENT_RENDER_GLOBAL_OK &&
          call == NULL);
    CHECK(mc_client_render_global_terminal_detach(NULL, error) ==
          MC_CLIENT_RENDER_GLOBAL_INVALID_ARGUMENT);
    CHECK(mc_client_render_global_terminal_detach(owner, NULL) ==
              MC_CLIENT_RENDER_GLOBAL_INVALID_ARGUMENT &&
          owner->phase == MC_CLIENT_RENDER_GLOBAL_RAW && !owner->world_retiring &&
          !owner->terminal_preserve_manager_world);
    mc_client_render_global *external = owner;
    CHECK(mc_client_render_global_retain(external, error) == MC_CLIENT_RENDER_GLOBAL_OK);
    CHECK(mc_client_render_global_terminal_detach(owner, error) == MC_CLIENT_RENDER_GLOBAL_BUSY &&
          owner->phase == MC_CLIENT_RENDER_GLOBAL_RAW && !owner->world_retiring &&
          !owner->terminal_preserve_manager_world);
    CHECK(mc_client_render_global_release(&external, error) == MC_CLIENT_RENDER_GLOBAL_OK &&
          external == NULL);
    CHECK(mc_client_render_global_terminal_detach(owner, error) == MC_CLIENT_RENDER_GLOBAL_OK &&
          owner->fields.k == NULL && owner->fields.u == NULL && owner->access == NULL &&
          owner->world_occurrence == NULL && owner->manager_world_occurrence == NULL &&
          owner->world_manager == NULL);
    CHECK(terminal_admission_closed(owner, error) == 0);
    CHECK(mc_client_render_global_terminal_detach(owner, error) == MC_CLIENT_RENDER_GLOBAL_OK);
    CHECK(mc_client_render_global_world_detach(owner, error) == MC_CLIENT_RENDER_GLOBAL_OK &&
          owner->terminal_preserve_manager_world);
    CHECK(mc_client_render_global_retire(&owner, error) == MC_CLIENT_RENDER_GLOBAL_OK &&
          owner == NULL);
    /* A genuinely entered legacy detach cannot later acquire the preserve-TH
     * policy, even on a real RAW owner with no World or device dependencies. */
    CHECK(mc_client_render_global_allocate(classes, NULL, &owner, error) ==
          MC_CLIENT_RENDER_GLOBAL_OK);
    CHECK(mc_client_render_global_world_detach(owner, error) == MC_CLIENT_RENDER_GLOBAL_OK &&
          owner->world_retiring && !owner->terminal_preserve_manager_world);
    CHECK(mc_client_render_global_terminal_detach(owner, error) == MC_CLIENT_RENDER_GLOBAL_BUSY &&
          owner->world_retiring && !owner->terminal_preserve_manager_world);
    CHECK(mc_client_render_global_retire(&owner, error) == MC_CLIENT_RENDER_GLOBAL_OK &&
          owner == NULL);
    CHECK(mc_client_render_engine_classes_release(classes) == MC_CLIENT_RENDER_ENGINE_OK);
    free(error);
    return 0;
}

typedef struct render_world_fixture {
    mc_world_regions *regions;
    mc_world_store *store;
    mc_world_runtime_classes *classes;
    mc_world_runtime *world;
    mc_world_info_name *name;
    mc_world_provider *dimension;
} render_world_fixture;

static int direct_world(fixture *runtime, startup_fixture *startup, const char *path,
                        render_world_fixture *f) {
    memset(f, 0, sizeof(*f));
    uint8_t map[256];
    for (size_t i = 0; i < sizeof(map); ++i)
        map[i] = (uint8_t)i;
    const uint16_t title[] = {'R', 'e', 'n', 'd', 'e', 'r'};
    CHECK(mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_store_options options = {.session_token = 137,
                                            .block_map = map,
                                            .block_map_length = sizeof(map),
                                            .limits = mc_world_store_default_limits()};
    CHECK(mc_world_store_open(path, f->regions, &options, &f->store, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_info_name_copy((mc_nbt_string){title, sizeof(title) / sizeof(*title)}, SIZE_MAX,
                                  &f->name) == MC_WORLD_INFO_OK);
    CHECK(mc_world_provider_create(0, NULL, &f->dimension, NULL) == MC_WORLD_PROVIDER_OK);
    CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, runtime->services.profile,
                                          startup->effect_trig, &f->classes,
                                          NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_classes_bind_registry(f->classes, runtime->services.registry, NULL) ==
          MC_REGISTRY_OK);
    CHECK(mc_world_runtime_classes_bind_entities(f->classes, runtime->services.entities) ==
          MC_ENTITY_OK);
    /* Actual fd direct-metadata constructor. No loaded/generated Chunk,
     * World field patch, clock override or READY publication is injected. */
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA, f->classes, NULL,
                                          &f->world, NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_runtime_world_arguments args = {
        .store = f->store, .name = f->name, .seed = 23, .dimension = f->dimension};
    CHECK(mc_world_runtime_construct_world(f->world, &args, NULL) == MC_WORLD_STORAGE_OK);
    mc_world_runtime_construction state;
    CHECK(mc_world_runtime_construction_get(f->world, &state) == MC_WORLD_STORAGE_OK &&
          state.entry == MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA &&
          state.phase == MC_WORLD_RUNTIME_READY && state.contexts_bound && state.cache_registered &&
          mc_world_store_chunk_count(f->store) == 0);
    return 0;
}

static int direct_world_close(render_world_fixture *f) {
    CHECK(mc_world_runtime_retire_entity_containers(f->world, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_retire_world(f->world, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_free(f->world, NULL) == MC_WORLD_STORAGE_OK);
    f->world = NULL;
    mc_world_provider_release(f->dimension);
    CHECK(mc_world_runtime_classes_free(f->classes, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_store_free_checked(&f->store, NULL) == MC_WORLD_STORAGE_OK && f->store == NULL);
    mc_world_regions_free(f->regions);
    mc_world_info_name_release(f->name);
    return 0;
}

static int distance(client_application *app, int32_t value) {
    mc_game_settings_state settings;
    CHECK(mc_game_settings_read_state(app->settings, &settings));
    settings.values.render_distance = value;
    CHECK(mc_game_settings_set_values(app->settings, &settings.values, NULL) ==
          MC_GAME_SETTINGS_OK);
    return 0;
}

static int grid(mc_client_render_global *owner, mc_world_runtime *world, int32_t extent,
                size_t count) {
    CHECK(owner->fields.k == world && owner->fields.u != NULL && owner->fields.u->c == world &&
          owner->fields.p == extent && owner->fields.q == 8 && owner->fields.r == extent &&
          owner->fields.o != NULL && owner->fields.n != NULL &&
          owner->fields.o != owner->fields.n && owner->o_length == count &&
          owner->n_length == count && owner->fields.a != owner->fields.m &&
          owner->fields.a != owner->fields.S && owner->fields.m != owner->fields.S &&
          owner->fields.a->count == 0 && owner->fields.m->count == count);
    size_t ordinal = 0;
    for (int32_t x = 0; x < extent; ++x)
        for (int32_t y = 0; y < 8; ++y)
            for (int32_t z = 0; z < extent; ++z, ++ordinal) {
                const size_t index = (size_t)((z * 8 + y) * extent + x);
                mc_client_source_dk *chunk = owner->fields.o[index];
                mc_client_source_dk_state state;
                CHECK(chunk != NULL && owner->fields.n[index] == chunk &&
                      owner->fields.m->entries[ordinal] == chunk &&
                      mc_client_source_dk_inspect(chunk, &state));
                CHECK(state.owner == owner && state.phase == MC_CLIENT_RENDER_GLOBAL_READY &&
                      state.constructor_returned && !state.list_open &&
                      state.pending_pooled_bounds == NULL && state.fields.a == world &&
                      state.fields.F == owner->fields.a && state.fields.B != NULL &&
                      state.fields.B != owner->fields.a && state.fields.B != owner->fields.m &&
                      state.fields.B->count == 0 && state.fields.c == x * 16 &&
                      state.fields.d == y * 16 && state.fields.e == z * 16 &&
                      state.fields.q == x * 16 + 8 && state.fields.r == y * 16 + 8 &&
                      state.fields.s == z * 16 + 8 && state.fields.f == 16 &&
                      state.fields.g == 16 && state.fields.h == 16 &&
                      state.fields.C == owner->fields.s + (int32_t)ordinal * 3 &&
                      state.fields.w == (int32_t)ordinal && state.fields.x && !state.fields.y &&
                      state.fields.o && state.fields.u && !state.fields.E &&
                      state.fields.p != NULL && state.fields.p[0] && state.fields.p[1] &&
                      state.fields.v != NULL && state.fields.v->min_x == (double)(x * 16 - 6) &&
                      state.fields.v->min_y == (double)(y * 16 - 6) &&
                      state.fields.v->min_z == (double)(z * 16 - 6) &&
                      state.fields.v->max_x == (double)(x * 16 + 22) &&
                      state.fields.v->max_y == (double)(y * 16 + 22) &&
                      state.fields.v->max_z == (double)(z * 16 + 22));
                mc_client_source_list_state list;
                mc_client_source_batch_view batch;
                CHECK(mc_client_display_list_inspect(owner->display, state.fields.C + 2, &list) &&
                      list.defined && !list.building && list.commands == 1 && list.draws == 1 &&
                      list.vertices == 36 &&
                      mc_client_display_list_batch(owner->display, state.fields.C + 2, 0, &batch) &&
                      batch.vertices == 36 && batch.mode == 4 && batch.byte_count == 36u * 32u &&
                      batch.arrays == (1u << MC_CLIENT_SOURCE_VERTEX_ARRAY));
                if (owner->fields.w) {
                    int32_t query;
                    memcpy(&query, owner->fields.v->bytes->data + ordinal * sizeof(query),
                           sizeof(query));
                    CHECK(state.fields.z == query && owner->fields.v->position == 0);
                }
            }
    CHECK(ordinal == count);
    return 0;
}

static int attach(mc_client_render_global *owner, mc_world_runtime *world, int32_t extent,
                  size_t count, size_t *bytes) {
    mc_client_render_global_world_call *call = NULL;
    CHECK(mc_client_render_global_world_begin(owner, world, &call) == MC_CLIENT_RENDER_GLOBAL_OK &&
          call != NULL);
    mc_client_render_global_world_state state;
    CHECK(mc_client_render_global_world_inspect(call, &state) && state.owner == owner &&
          state.argument == world && state.source_returned && state.source_pc == 73 &&
          state.rebuild_returned && state.rebuild_pc == 699 && state.last_store_pc == 696 &&
          state.constructed_chunks == count && state.published_chunks == count &&
          state.queued_chunks == count && state.pending_chunk == NULL && state.registered &&
          state.manager_returned && state.k_stored && state.cv_constructed && state.u_stored);
    CHECK(grid(owner, world, extent, count) == 0);
    mc_client_render_global_error *error = calloc(1, sizeof(*error));
    CHECK(error != NULL);
    mc_client_render_global *original = owner;
    CHECK(mc_client_render_global_retire(&original, error) == MC_CLIENT_RENDER_GLOBAL_BUSY &&
          original == owner && !owner->world_retiring);
    CHECK(mc_client_render_global_terminal_detach(owner, error) == MC_CLIENT_RENDER_GLOBAL_BUSY &&
          owner->phase == MC_CLIENT_RENDER_GLOBAL_READY && !owner->world_retiring &&
          !owner->terminal_preserve_manager_world && owner->world_call == call);
    CHECK(mc_client_render_global_world_retire(&call) == MC_CLIENT_RENDER_GLOBAL_OK &&
          call == NULL);
    mc_client_render_global_state actual;
    CHECK(mc_client_render_global_inspect(owner, &actual) && actual.holds == 1 &&
          actual.arena_bytes <= 8u * 1024u * 1024u);
    *bytes = actual.arena_bytes;
    const int32_t ids[] = {owner->fields.s + 2, owner->fields.s + (int32_t)(count - 1u) * 3 + 2};
    /* Explicit bbox-execution control: the actual render caller disables
     * GL_TEXTURE_2D at n.a(md,ID)433 before DK.d753. This does not run that
     * containing renderer or add a state reset to the DK constructor. */
    CHECK(mc_client_display_source_texture_enable(owner->display, false, &error->display));
    const bool drawn = mc_client_display_lists_call(owner->display, ids, 2, &error->display);
    if (!drawn)
        fprintf(stderr, "BBox execution status=%d operation=%s detail=%s gl=0x%x\n",
                error->display.status,
                error->display.operation == NULL ? "NULL" : error->display.operation,
                error->display.detail, error->display.gl_error);
    CHECK(drawn);
    CHECK(mc_client_display_source_texture_enable(owner->display, true, &error->display));
    uint32_t gl_error = UINT32_MAX;
    CHECK(mc_client_display_source_get_error(owner->display, &gl_error, &error->display) &&
          gl_error == 0);
    free(error);
    return 0;
}

static int quota_prefix(fixture *runtime, client_application *app, mc_world_runtime *world,
                        size_t constructor_allocations) {
    mc_client_render_global *owner = NULL;
    mc_client_render_global_error *error = calloc(1, sizeof(*error));
    CHECK(error != NULL);
    const mc_client_render_engine_options options = {8u * 1024u * 1024u,
                                                     constructor_allocations + 5u};
    CHECK(mc_client_render_global_allocate(runtime->graphics, &options, &owner, error) ==
          MC_CLIENT_RENDER_GLOBAL_OK);
    CHECK(mc_client_render_global_construct(owner, app, app->render_engine, app->display, error) ==
          MC_CLIENT_RENDER_GLOBAL_OK);
    mc_client_render_global_world_call *call = NULL;
    CHECK(mc_client_render_global_world_begin(owner, world, &call) ==
          MC_CLIENT_RENDER_GLOBAL_LIMIT);
    mc_client_render_global_world_state state;
    mc_client_source_dk_state chunk;
    CHECK(call != NULL && mc_client_render_global_world_inspect(call, &state) &&
          !state.source_returned && state.source_pc == 70 && state.rebuild_pc == 347 &&
          state.registered && state.pending_chunk != NULL && state.constructed_chunks == 0 &&
          state.published_chunks == 0 && state.queued_chunks == 0 &&
          state.failure->status == MC_CLIENT_RENDER_GLOBAL_LIMIT &&
          state.failure->source_pc == 32 && state.failure->parent.pc == 70 &&
          mc_client_source_dk_inspect(state.pending_chunk, &chunk) &&
          chunk.phase == MC_CLIENT_RENDER_GLOBAL_FAILED && !chunk.constructor_returned &&
          chunk.fields.p != NULL && chunk.fields.B == NULL && chunk.fields.a == NULL &&
          chunk.last_store_pc == 28 && chunk.fields.C == -1 && !chunk.fields.o &&
          !chunk.fields.p[0] && !chunk.fields.p[1] && chunk.fields.x && !chunk.fields.E);
    mc_client_render_global_world_call *retry = NULL;
    CHECK(mc_client_render_global_world_begin(owner, world, &retry) ==
              MC_CLIENT_RENDER_GLOBAL_BUSY &&
          retry == NULL && owner->world_call == call && owner->fields.o[0] == NULL);
    CHECK(mc_client_render_global_world_retire(&call) == MC_CLIENT_RENDER_GLOBAL_OK &&
          call == NULL);
    CHECK(mc_client_render_global_retire(&owner, error) == MC_CLIENT_RENDER_GLOBAL_OK &&
          owner == NULL);
    free(error);
    return 0;
}

typedef struct terminal_graph_snapshot {
    mc_world_runtime_fields world_fields;
    mc_client_render_global_fields other_fields;
    const mc_chunk_acquire_context *acquisition;
    mc_chunk_cache cache;
    mc_chunk_pending pending;
    const void *store_info;
    size_t stored_chunks;
    mc_world_regions_usage regions;
    int64_t token;
} terminal_graph_snapshot;

static int terminal_graph_capture(render_world_fixture *world, mc_client_render_global *other,
                                  terminal_graph_snapshot *snapshot) {
    snapshot->acquisition = mc_world_runtime_acquisition(world->world);
    CHECK(snapshot->acquisition != NULL && snapshot->acquisition->cache != NULL &&
          snapshot->acquisition->pending != NULL);
    /* Copy actual canonical subobjects, including their existing padding, for
     * unchanged-byte checks. WorldAccess membership counts are in their own
     * List storage and are checked separately below. */
    memcpy(&snapshot->world_fields, &world->world->fields, sizeof(snapshot->world_fields));
    memcpy(&snapshot->other_fields, &other->fields, sizeof(snapshot->other_fields));
    memcpy(&snapshot->cache, snapshot->acquisition->cache, sizeof(snapshot->cache));
    memcpy(&snapshot->pending, snapshot->acquisition->pending, sizeof(snapshot->pending));
    snapshot->store_info = mc_world_store_info(world->store);
    snapshot->stored_chunks = mc_world_store_chunk_count(world->store);
    snapshot->regions = mc_world_regions_get_usage(world->regions);
    CHECK(mc_world_store_session_token_get(world->store, &snapshot->token) == MC_WORLD_STORAGE_OK);
    CHECK(snapshot->stored_chunks == 0 && snapshot->cache.count == 0 &&
          snapshot->pending.count == 0);
    return 0;
}

static int terminal_graph_same(render_world_fixture *world, mc_client_render_global *other,
                               const terminal_graph_snapshot *snapshot) {
    mc_world_runtime_construction construction;
    CHECK(mc_world_runtime_construction_get(world->world, &construction) == MC_WORLD_STORAGE_OK &&
          construction.phase == MC_WORLD_RUNTIME_READY && construction.contexts_bound &&
          construction.cache_registered &&
          mc_world_provider_world(world->dimension) == world->world);
    CHECK(memcmp(&world->world->fields, &snapshot->world_fields, sizeof(snapshot->world_fields)) ==
              0 &&
          mc_world_runtime_acquisition(world->world) == snapshot->acquisition &&
          memcmp(snapshot->acquisition->cache, &snapshot->cache, sizeof(snapshot->cache)) == 0 &&
          memcmp(snapshot->acquisition->pending, &snapshot->pending, sizeof(snapshot->pending)) ==
              0);
    const mc_world_regions_usage regions = mc_world_regions_get_usage(world->regions);
    int64_t token = 0;
    CHECK(mc_world_store_info(world->store) == snapshot->store_info &&
          mc_world_store_chunk_count(world->store) == snapshot->stored_chunks &&
          regions.count == snapshot->regions.count && regions.bytes == snapshot->regions.bytes &&
          mc_world_store_session_token_get(world->store, &token) == MC_WORLD_STORAGE_OK &&
          token == snapshot->token);
    CHECK(other->phase == MC_CLIENT_RENDER_GLOBAL_READY && !other->world_retiring &&
          memcmp(&other->fields, &snapshot->other_fields, sizeof(snapshot->other_fields)) == 0 &&
          other->fields.k == world->world && other->fields.u->c == world->world &&
          other->fields.o[0]->fields.a == world->world && other->fields.o[0]->world_held &&
          other->world_occurrence == world->world &&
          other->manager_world_occurrence == world->world);
    return 0;
}

static int terminal_manager_same(mc_client_render_manager_classes *classes,
                                 const mc_client_manager_state *before, size_t ended_holds) {
    mc_client_manager_state after;
    CHECK(mc_client_render_manager_inspect(classes, &after) &&
          after.phase == MC_CLIENT_MANAGER_READY && after.source_views_available &&
          after.a == before->a && after.candidate == before->candidate &&
          after.native_consumers == before->native_consumers &&
          after.manager_holds + ended_holds == before->manager_holds && after.entries == 28 &&
          after.completed_bw_stores == 28);
    const mc_client_manager_fields *a = &before->fields, *b = &after.fields;
    CHECK(a->o == b->o && a->p == b->p && a->e == b->e && a->f == b->f && a->g == b->g &&
          a->h == b->h && a->k == b->k && memcmp(&a->i, &b->i, sizeof(a->i)) == 0 &&
          memcmp(&a->j, &b->j, sizeof(a->j)) == 0 && memcmp(&a->l, &b->l, sizeof(a->l)) == 0 &&
          memcmp(&a->m, &b->m, sizeof(a->m)) == 0 && memcmp(&a->n, &b->n, sizeof(a->n)) == 0 &&
          memcmp(&before->b, &after.b, sizeof(after.b)) == 0 &&
          memcmp(&before->c, &after.c, sizeof(after.c)) == 0 &&
          memcmp(&before->d, &after.d, sizeof(after.d)) == 0);
    return 0;
}

static int terminal_preserves_manager(mc_client_render_global **slot,
                                      mc_client_render_global *other, render_world_fixture *world,
                                      mc_client_render_manager_classes *classes) {
    /* Native quiesce seam only, using two genuine N owners of ONE actual App.
     * This does not qualify a two-App or full Load/Screen terminal coordinator. */
    mc_client_render_global *owner = *slot;
    mc_client_render_global_error *error = calloc(1, sizeof(*error));
    CHECK(error != NULL && world->world->fields.u->count == 2);
    mc_client_render_manager *manager = NULL;
    CHECK(mc_client_render_manager_static_get(classes, &manager, &error->manager) ==
              MC_CLIENT_MANAGER_OK &&
          manager != NULL && owner->world_manager == manager && other->world_manager == manager &&
          owner->manager_world_occurrence == world->world);
    /* A later genuine source TH bind stores the SAME pointer. Equality with
     * N's earlier captured publication is insufficient terminal authority. */
    CHECK(mc_client_render_manager_change_world(manager, world->world, &error->manager) ==
          MC_CLIENT_MANAGER_OK);
    mc_client_manager_consumer consumer = {0};
    CHECK(mc_client_render_manager_consumer_attach(classes, &consumer, &error->manager) ==
          MC_CLIENT_MANAGER_OK);
    mc_client_manager_state before;
    CHECK(mc_client_render_manager_inspect(classes, &before) && before.fields.g == world->world &&
          before.native_consumers != 0 && consumer.attached && consumer.classes == classes);
    mc_client_render_manager_classes *retirement_slot = classes;
    CHECK(mc_client_render_manager_classes_retire(&retirement_slot, &error->manager) ==
              MC_CLIENT_MANAGER_BUSY &&
          retirement_slot == classes);

    mc_world_access *identity = world->world->world_accesses.entries[1];
    CHECK(identity != NULL && identity != world->world->world_accesses.entries[0] &&
          mc_world_runtime_add_world_access(world->world, identity, NULL) == MC_WORLD_STORAGE_OK);
    const uint64_t foreign = world->world->world_accesses.identities[2];
    mc_world_storage_error child_error = {0};
    mc_world_access_failure original = {0}, captured = {0};
    /* Actual registered receiver and real callback body, as a direct callee
     * control. No replacement callback or successful World caller is supplied. */
    CHECK(mc_world_access_call_sound(identity, world->world, NULL, 0, 64, 0, 1, 1, &child_error,
                                     &original) == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          original.receiver == identity && original.render_global != NULL &&
          original.render_global->status == MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE &&
          original.render_global->source_pc == 30 &&
          mc_world_access_failure_retain(&original, &captured) == MC_WORLD_STORAGE_OK);
    mc_render_global_access_state access;
    CHECK(mc_render_global_access_inspect(owner->access, &access) && access.registrations == 1 &&
          access.active_callbacks == 0 && access.retained_failures == 1 &&
          access.failure == original.render_global);
    terminal_graph_snapshot graph;
    CHECK(terminal_graph_capture(world, other, &graph) == 0);
    mc_client_render_global *external = owner;
    CHECK(mc_client_render_global_retain(external, error) == MC_CLIENT_RENDER_GLOBAL_OK);
    CHECK(mc_client_render_global_terminal_detach(owner, error) == MC_CLIENT_RENDER_GLOBAL_BUSY &&
          !owner->world_retiring && !owner->terminal_preserve_manager_world &&
          owner->phase == MC_CLIENT_RENDER_GLOBAL_READY && world->world->fields.u->count == 3);
    CHECK(terminal_manager_same(classes, &before, 0) == 0 &&
          terminal_graph_same(world, other, &graph) == 0);
    CHECK(mc_client_render_global_release(&external, error) == MC_CLIENT_RENDER_GLOBAL_OK &&
          external == NULL);

    /* Captured real failure refuses cleanup AFTER terminal admission latches.
     * Both legacy retry paths must retain that policy and every remaining right. */
    CHECK(mc_client_render_global_terminal_detach(owner, error) == MC_CLIENT_RENDER_GLOBAL_BUSY &&
          owner->world_occurrence == world->world &&
          owner->manager_world_occurrence == world->world && owner->world_manager == manager &&
          owner->fields.k == world->world && owner->fields.u->c == world->world &&
          world->world->fields.u->count == 3);
    CHECK(terminal_admission_closed(owner, error) == 0);
    CHECK(mc_client_render_global_world_detach(owner, error) == MC_CLIENT_RENDER_GLOBAL_BUSY &&
          owner->terminal_preserve_manager_world);
    CHECK(mc_client_render_global_retire(slot, error) == MC_CLIENT_RENDER_GLOBAL_BUSY &&
          *slot == owner);
    CHECK(captured.render_global == original.render_global &&
          captured.render_global->source_pc == 30);
    CHECK(terminal_manager_same(classes, &before, 0) == 0 &&
          terminal_graph_same(world, other, &graph) == 0);
    mc_world_access_failure_release(&captured);
    /* The legacy owner-retire entry now consumes the remaining local World
     * rights itself. The real foreign membership stops only final bridge free. */
    CHECK(mc_client_render_global_retire(slot, error) == MC_CLIENT_RENDER_GLOBAL_BUSY &&
          *slot == owner && owner->terminal_preserve_manager_world &&
          owner->phase == MC_CLIENT_RENDER_GLOBAL_RETIRING && owner->world_occurrence == NULL &&
          owner->manager_world_occurrence == NULL && owner->world_manager == NULL &&
          owner->fields.k == NULL && owner->fields.u->c == NULL);
    for (mc_client_source_dk *chunk = owner->chunk_owners; chunk != NULL; chunk = chunk->next)
        CHECK(!chunk->world_held && chunk->fields.a == NULL &&
              chunk->phase == MC_CLIENT_RENDER_GLOBAL_RETIRING);
    CHECK(world->world->fields.u->count == 2 &&
          world->world->world_accesses.entries[1] == identity &&
          world->world->world_accesses.identities[1] == foreign);
    CHECK(terminal_manager_same(classes, &before, 1) == 0 &&
          terminal_graph_same(world, other, &graph) == 0);
    CHECK(terminal_admission_closed(owner, error) == 0);
    CHECK(mc_client_render_global_world_detach(owner, error) == MC_CLIENT_RENDER_GLOBAL_OK &&
          owner->terminal_preserve_manager_world);
    CHECK(mc_client_render_global_terminal_detach(owner, error) == MC_CLIENT_RENDER_GLOBAL_OK);
    CHECK(mc_client_render_global_retire(slot, error) == MC_CLIENT_RENDER_GLOBAL_BUSY &&
          *slot == owner && original.render_global->source_pc == 30);
    CHECK(terminal_manager_same(classes, &before, 1) == 0 &&
          terminal_graph_same(world, other, &graph) == 0);
    bool removed = false;
    CHECK(mc_world_runtime_remove_world_access(world->world, identity, &removed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          removed && world->world->fields.u->count == 1);
    CHECK(mc_client_render_global_retire(slot, error) == MC_CLIENT_RENDER_GLOBAL_OK &&
          *slot == NULL);
    CHECK(terminal_manager_same(classes, &before, 1) == 0 &&
          terminal_graph_same(world, other, &graph) == 0);
    CHECK(consumer.attached && consumer.classes == classes);
    CHECK(mc_client_render_manager_consumer_detach(&consumer, &error->manager) ==
              MC_CLIENT_MANAGER_OK &&
          !consumer.attached && consumer.classes == NULL);
    mc_client_manager_state after;
    CHECK(mc_client_render_manager_inspect(classes, &after) && after.fields.g == world->world &&
          after.native_consumers + 1 == before.native_consumers);
    printf("Terminal N quiesce: one actual App, same-World TH preserved; other "
           "N/World/provider/cache/storage live\n");
    free(error);
    return 0;
}

static int lifetime(mc_client_render_global **slot, mc_world_runtime *world) {
    mc_client_render_global *owner = *slot, *external = owner;
    mc_client_render_global_error *error = calloc(1, sizeof(*error));
    mc_world_mutation_error *mutation = calloc(1, sizeof(*mutation));
    CHECK(error != NULL && mutation != NULL && world->fields.u->count == 1);
    mc_world_access *identity = world->world_accesses.entries[0];
    CHECK(identity != NULL &&
          mc_world_runtime_add_world_access(world, identity, NULL) == MC_WORLD_STORAGE_OK);
    const uint64_t foreign = world->world_accesses.identities[1];
    CHECK(mc_client_render_global_retain(external, error) == MC_CLIENT_RENDER_GLOBAL_OK);
    CHECK(mc_client_render_global_world_detach(owner, error) == MC_CLIENT_RENDER_GLOBAL_BUSY &&
          !owner->world_retiring && world->fields.u->count == 2);
    CHECK(mc_client_render_global_retire(slot, error) == MC_CLIENT_RENDER_GLOBAL_BUSY &&
          *slot == owner && !owner->world_retiring);
    CHECK(mc_client_render_global_release(&external, error) == MC_CLIENT_RENDER_GLOBAL_OK &&
          external == NULL);
    CHECK(mc_world_runtime_sound(world, 0, 64, 0, NULL, 1, 1, mutation) ==
          MC_WORLD_STORAGE_UNSUPPORTED_STATE);
    const mc_world_access_failure original = mutation->world_access_failure;
    mc_world_access_failure captured = {0};
    CHECK(original.receiver == identity && original.render_global != NULL &&
          original.render_global->source_pc == 30 &&
          original.render_global->status == MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE &&
          mc_world_access_failure_retain(&original, &captured) == MC_WORLD_STORAGE_OK);
    CHECK(mc_client_render_global_world_detach(owner, error) == MC_CLIENT_RENDER_GLOBAL_BUSY &&
          world->fields.u->count == 2 && captured.render_global == original.render_global &&
          captured.render_global->source_pc == 30);
    mc_world_access_failure_release(&captured);
    CHECK(mc_client_render_global_world_detach(owner, error) == MC_CLIENT_RENDER_GLOBAL_OK &&
          world->fields.u->count == 1 && world->world_accesses.entries[0] == identity &&
          world->world_accesses.identities[0] == foreign && owner->fields.k == NULL);
    CHECK(mc_client_render_global_terminal_detach(owner, error) == MC_CLIENT_RENDER_GLOBAL_BUSY &&
          owner->world_retiring && !owner->terminal_preserve_manager_world);
    CHECK(mc_client_render_global_retire(slot, error) == MC_CLIENT_RENDER_GLOBAL_BUSY &&
          *slot == owner && owner->access != NULL && original.render_global->source_pc == 30);
    CHECK(mc_world_runtime_retire_entity_containers(world, NULL) == MC_WORLD_STORAGE_OK &&
          world->fields.u->count == 0);
    CHECK(mc_client_render_global_retire(slot, error) == MC_CLIENT_RENDER_GLOBAL_OK &&
          *slot == NULL);
    free(mutation);
    free(error);
    return 0;
}

static int world_device(const char *resources, const char *assets, const char *world_path) {
    fixture runtime;
    startup_fixture startup;
    CHECK(fixture_open(&runtime, resources) == 0 && startup_services(&startup) == 0 &&
          startup_icons(&startup, assets) == 0);
    client_application *app = NULL;
    CHECK(actual_app(&runtime, false, &app) == 0 && statistics_session(app) == 0);
    mc_client_application_startup *attempt = NULL;
    mc_client_startup_options options = mc_client_application_startup_default_options();
    options.hidden = true;
    CHECK(mc_client_application_startup_allocate(app, &startup.services, &options, &attempt,
                                                 startup.error) == MC_CLIENT_STARTUP_OK);
    CHECK(mc_client_application_run_prefix(attempt, startup.error) ==
              MC_CLIENT_STARTUP_UNIMPLEMENTED &&
          startup.error->caller.pc == 32 && app->g != NULL && app->i == NULL &&
          app->active_world == NULL);
    CHECK(statistics_retire_resources(attempt) == 0);
    render_world_fixture world;
    CHECK(direct_world(&runtime, &startup, world_path, &world) == 0);
    mc_client_render_global_error *error = calloc(1, sizeof(*error));
    CHECK(error != NULL);
    mc_client_render_global *owner = NULL;
    CHECK(mc_client_render_global_allocate(runtime.graphics, NULL, &owner, error) ==
              MC_CLIENT_RENDER_GLOBAL_OK &&
          mc_client_render_global_construct(owner, app, app->render_engine, app->display, error) ==
              MC_CLIENT_RENDER_GLOBAL_OK);
    const size_t constructor_allocations = owner->arena.allocations;
    mc_client_render_list *const tiles = owner->fields.a, *const dirty = owner->fields.m,
                                 *const sky = owner->fields.S;
    CHECK(distance(app, 3) == 0);
    size_t small_bytes = 0, large_bytes = 0;
    CHECK(attach(owner, world.world, 5, 200, &small_bytes) == 0);
    const mc_client_tessellator *tessellator = NULL;
    mc_client_render_engine_phase phase;
    int32_t before = -1, after = -1;
    CHECK(mc_client_chunk_statistics_inspect(runtime.graphics, &phase, &tessellator, &before) &&
          phase == MC_CLIENT_RENDER_ENGINE_READY && tessellator != NULL && before == 0);
    mc_client_source_dk **old_array = owner->fields.o;
    mc_client_source_dk *first = old_array[0];
    mc_client_render_blocks *old_cv = owner->fields.u;
    mc_client_render_global_world_call *unload = NULL;
    CHECK(mc_client_render_global_world_begin(owner, NULL, &unload) == MC_CLIENT_RENDER_GLOBAL_OK);
    mc_client_render_global_world_state unloaded;
    CHECK(mc_client_render_global_world_inspect(unload, &unloaded) && unloaded.source_returned &&
          unloaded.source_pc == 73 && unloaded.old_removed && !unloaded.registered &&
          !unloaded.rebuild_returned && owner->fields.k == NULL && owner->fields.u != old_cv &&
          owner->fields.u->c == NULL && owner->fields.o == old_array &&
          first->fields.a == world.world && owner->fields.m == dirty && owner->fields.a == tiles &&
          owner->fields.S == sky && world.world->fields.u->count == 0);
    CHECK(mc_client_render_global_world_retire(&unload) == MC_CLIENT_RENDER_GLOBAL_OK);
    CHECK(attach(owner, world.world, 5, 200, &small_bytes) == 0 && first->fields.a == NULL &&
          owner->fields.o != old_array && owner->fields.m == dirty && owner->fields.a == tiles &&
          owner->fields.S == sky);
    mc_client_render_global *large = NULL;
    CHECK(mc_client_render_global_allocate(runtime.graphics, NULL, &large, error) ==
              MC_CLIENT_RENDER_GLOBAL_OK &&
          mc_client_render_global_construct(large, app, app->render_engine, app->display, error) ==
              MC_CLIENT_RENDER_GLOBAL_OK);
    CHECK(distance(app, 0) == 0 && attach(large, world.world, 26, 5408, &large_bytes) == 0);
    CHECK(large_bytes > small_bytes &&
          mc_client_chunk_statistics_inspect(runtime.graphics, &phase, &tessellator, &after) &&
          after == before);
    printf("Actual n/DK arena bytes: H3 after repeat=%zu H0=%zu; sizeof(DK)=%zu\n", small_bytes,
           large_bytes, sizeof(mc_client_source_dk));
    CHECK(terminal_preserves_manager(&large, owner, &world, attempt->manager_classes) == 0 &&
          large == NULL);
    CHECK(distance(app, 3) == 0 &&
          quota_prefix(&runtime, app, world.world, constructor_allocations) == 0);
    CHECK(lifetime(&owner, world.world) == 0 && direct_world_close(&world) == 0);
    free(error);
    mc_client_render_texture_error *terminal = calloc(1, sizeof(*terminal));
    CHECK(terminal != NULL);
    mc_client_render_manager_classes *manager = attempt->manager_classes;
    CHECK(mc_client_application_startup_retire(&attempt, startup.error) == MC_CLIENT_STARTUP_BUSY);
    CHECK(mc_client_render_manager_retire_hands(manager, &startup.error->manager) ==
          MC_CLIENT_MANAGER_OK);
    CHECK(mc_client_render_manager_classes_retire(&manager, &startup.error->manager) ==
          MC_CLIENT_MANAGER_OK);
    CHECK(mc_client_application_startup_retire(&attempt, startup.error) == MC_CLIENT_STARTUP_BUSY);
    CHECK(mc_client_render_engine_retire_resources(runtime.graphics, terminal) ==
          MC_CLIENT_RENDER_TEXTURE_OK);
    free(terminal);
    CHECK(mc_client_application_startup_retire(&attempt, startup.error) == MC_CLIENT_STARTUP_OK &&
          attempt == NULL);
    CHECK(statistics_end_app(&runtime, &startup, &app) == 0);
    return 0;
}

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "--cpu") == 0)
        CHECK(raw_world_admission() == 0);
    else {
        CHECK(argc == 4);
        CHECK(world_device(argv[1], argv[2], argv[3]) == 0);
    }
    printf("Actual RenderGlobal World contracts: %u checks\n", checks);
    return 0;
}
