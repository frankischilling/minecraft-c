/* Native retained-hook regressions, not original-JVM observations. The cold
 * defining Registry, actual no-sky Dimension projection, raw Chunk bytes and
 * checked Registry quota are declared fixture inputs. No replacement World,
 * getter, factory result or Java exception is supplied by this fixture. */
#include "mc/resources/translation/translation.h"
#include "world/chunk/object_state_internal.h"
#include "world/runtime/mutation/chunk_execution_internal.h"

typedef struct chest_nullable_resource {
    const mc_registry_resources *delegate;
    mc_world_mutation_chunk_execution *setter;
    mc_world_mutation_error *error;
    mc_world_chunk *chunk;
    mc_world_runtime *world;
    size_t opens, rebinds;
    bool armed, valid;
} chest_nullable_resource;

static mc_registry_status chest_nullable_open(void *context, const char *name,
                                              mc_java_properties_input *out) {
    chest_nullable_resource *r = context;
    ++r->opens;
    if (r->armed && r->rebinds == 0) {
        const size_t cell = ((size_t)8 * 16u + 8u) * 128u + 64u;
        const uint8_t packed = r->chunk->data->metadata[cell >> 1u];
        const int32_t metadata = (int32_t)((packed >> ((cell & 1u) * 4u)) & 15u);
        r->valid = r->valid && r->setter->active && r->setter->lease_held &&
                   r->setter->binding_held && r->error->chunk_bytecode == 279 &&
                   r->setter->setter_body == MC_WORLD_MUTATION_CHUNK_ID_METADATA &&
                   r->chunk->d == r->world && r->chunk->data->blocks[cell] == 54 && metadata == 7;
        r->chunk->d = NULL; /* Real resource callback, between m279 and d286. */
        ++r->rebinds;
    }
    return r->delegate->open(r->delegate->context, name, out);
}

static mc_registry_status chest_nullable_close(void *context,
                                               const mc_java_properties_input *input) {
    chest_nullable_resource *r = context;
    return r->delegate->close == NULL ? MC_REGISTRY_OK
                                      : r->delegate->close(r->delegate->context, input);
}

static bool chest_nullable_ready(void *context, const mc_java_properties_input *input) {
    chest_nullable_resource *r = context;
    return r->delegate->ready != NULL && r->delegate->ready(r->delegate->context, input);
}

static bool chest_nullable_late_setter(fixture *f) {
    CHECK(mc_world_store_chunk_count(f->store) == 0);
    mc_registry_classes *registry = NULL;
    mc_world_runtime_classes *classes = NULL;
    mc_world_provider *dimension = NULL, *no_sky = NULL;
    mc_world_runtime *world = NULL;
    mc_world_chunk *chunk = NULL;
    mc_entity_tick_execution *execution = calloc(1, sizeof(*execution));
    mc_world_runtime_execution_result *owned = calloc(1, sizeof(*owned));
    mc_world_mutation_chunk_execution *setter = calloc(1, sizeof(*setter));
    mc_world_mutation_error *error = calloc(1, sizeof(*error));
    CHECK(execution != NULL && owned != NULL && setter != NULL && error != NULL);
    chest_nullable_resource resource = {.delegate = mc_resource_files_source(f->files),
                                        .setter = setter,
                                        .error = error,
                                        .valid = true};
    const mc_registry_resources resources = {&resource, chest_nullable_open, chest_nullable_close,
                                             chest_nullable_ready};
    const mc_registry_options options = {MC_REGISTRY_CLIENT, f->profile, f->materials, 0,
                                         &resources};
    CHECK(mc_registry_classes_allocate(&options, &registry, NULL) == MC_REGISTRY_OK &&
          mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile, &trig, &classes,
                                          NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_classes_bind_registry(classes, registry, NULL) == MC_REGISTRY_OK &&
          mc_world_provider_create(0, NULL, &dimension, NULL) == MC_WORLD_PROVIDER_OK);
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_provider_chunks = 1;
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA, classes, &limits,
                                          &world, NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_runtime_world_arguments args = {
        .store = f->store, .name = f->name, .seed = 5143, .dimension = dimension};
    CHECK(mc_world_runtime_construct_world(world, &args, NULL) == MC_WORLD_STORAGE_OK);
    uint8_t *blocks = calloc(MC_CHUNK_BLOCKS, 1);
    CHECK(blocks != NULL &&
          mc_world_store_create_generated(f->store, world, 0, 0, 0, &blocks, MC_CHUNK_BLOCKS,
                                          &chunk, NULL) == MC_WORLD_STORAGE_OK &&
          blocks == NULL);
    mc_chunk_state *state = mc_world_chunk_state(chunk);
    mc_registry_block_view before;
    mc_tile_entity_piston_class_snapshot tile_before;
    CHECK(state != NULL &&
          mc_registry_classes_inspect_blocks(registry, &before, NULL) == MC_REGISTRY_OK &&
          before.phase == MC_REGISTRY_COLD && resource.opens == 0 &&
          mc_tile_entity_piston_class_get_snapshot(classes, &tile_before) == MC_TILE_ENTITY_OK &&
          tile_before.base_phase == MC_TILE_ENTITY_CLASS_COLD);
    /* This supported numerical Dimension bind is an explicit native fixture
     * projection. It avoids an earlier Block.q active use; it does not claim
     * a completed Nether World or a source-published World.t assignment. */
    CHECK(mc_world_provider_create(-1, NULL, &no_sky, NULL) == MC_WORLD_PROVIDER_OK &&
          mc_world_provider_bind(no_sky, world, NULL) == MC_WORLD_PROVIDER_OK);
    mc_world_provider_fields no_sky_fields;
    CHECK(mc_world_provider_fields_get(no_sky, &no_sky_fields) == MC_WORLD_PROVIDER_OK &&
          no_sky_fields.e && no_sky_fields.a == world && world->fields.t == dimension);
    world->fields.t = no_sky;
    const size_t bindings = world->persistent_bindings;
    const size_t listed = world->fields.c->count, deferred = world->fields.G->count;
    resource.world = world;
    resource.chunk = chunk;
    resource.armed = true;
    mc_entity_tick_execution_init(execution, NULL);
    mc_world_runtime_execution_result_init(owned);
    mc_world_mutation_error_init(error);
    const mc_entity_tick_execution_site site = {"fd", "a", "(IIIII)Z", 68};
    mc_entity_tick_execution_world_token root;
    CHECK(mc_entity_tick_execution_push_world(execution, world, site, &root) ==
          MC_WORLD_STORAGE_OK);
    const mc_world_runtime_execution_code code = {{"lm", "e", "()V", 6}, {"lm", "e", "()V", 33}};
    mc_world_mutation_chunk_execution_init(setter, execution, code, classes, registry, site, root,
                                           owned);
    bool changed = false;
    mc_world_storage_status raw = MC_WORLD_STORAGE_OK;
    const bool completed = mc_world_mutation_chunk_block_metadata_in_execution(
        setter, world, f->store, state, chunk, 8, 64, 8, 54, 7, &changed, error, &raw);
    /* End the authored source projection before any assertion or native
     * retirement. The exact scope and error retain the reached NULL value. */
    chunk->d = world;
    world->fields.t = dimension;
    resource.armed = false;
    mc_entity_tick_execution_site null_site = {0};
    mc_world_storage_status child_status = MC_WORLD_STORAGE_OK;
    const mc_live_tile_entity_error *child =
        mc_world_mutation_execution_failed_tile_added(setter->hook, &child_status);
    mc_tile_entity_piston_class_snapshot tile_after;
    mc_registry_block_view after;
    size_t map_count = SIZE_MAX;
    CHECK(!completed && !changed && setter->entered && !setter->active &&
          !setter->source_returned && !setter->source_null && resource.valid &&
          resource.opens != 0 && resource.rebinds == 1 && setter->hook != NULL &&
          raw != MC_WORLD_STORAGE_OK && child != NULL && child_status == raw &&
          child->caller_bytecode == 0 && child->bytecode == 18 &&
          child->registry.status == MC_REGISTRY_OK &&
          child->tile_entity.status == MC_TILE_ENTITY_OK &&
          owned->ownership_status == MC_WORLD_STORAGE_OK &&
          mc_world_mutation_execution_source_null(setter->hook, &null_site) &&
          null_site.class_name != NULL && strcmp(null_site.class_name, "rw") == 0 &&
          null_site.method != NULL && strcmp(null_site.method, "c") == 0 &&
          null_site.descriptor != NULL && strcmp(null_site.descriptor, "(Lfd;III)V") == 0 &&
          null_site.pc == 18 &&
          mc_registry_classes_inspect_blocks(registry, &after, NULL) == MC_REGISTRY_OK &&
          after.phase == MC_REGISTRY_READY &&
          mc_tile_entity_piston_class_get_snapshot(classes, &tile_after) == MC_TILE_ENTITY_OK &&
          tile_after.base_phase == MC_TILE_ENTITY_CLASS_READY && tile_after.base_name_count == 8 &&
          tile_after.base_class_count == 8 && tile_after.live_tile_entities == 0 &&
          world->fields.c->count == listed && world->fields.G->count == deferred &&
          mc_world_chunk_tile_entity_count(chunk->l, &map_count) == MC_WORLD_STORAGE_OK &&
          map_count == 0);
    const size_t cell = ((size_t)8 * 16u + 8u) * 128u + 64u;
    CHECK(state->data->blocks[cell] == 54 &&
          ((state->data->metadata[cell >> 1u] >> ((cell & 1u) * 4u)) & 15) == 7);
    const mc_live_tile_entity_error *const held_child = child;
    memset(error, 0, sizeof(*error));
    CHECK(mc_world_mutation_execution_failed_tile_added(setter->hook, &child_status) ==
              held_child &&
          child_status == raw &&
          mc_world_mutation_chunk_native_close(setter) == MC_WORLD_STORAGE_OK &&
          mc_world_mutation_chunk_children_retire(setter) == MC_WORLD_STORAGE_OK &&
          setter->hook == NULL &&
          mc_entity_tick_execution_pop_world(execution, &root, true) == MC_WORLD_STORAGE_OK &&
          mc_entity_tick_execution_retire(execution) == MC_WORLD_STORAGE_OK &&
          world->persistent_bindings == bindings);
    mc_world_provider_unbind(no_sky);
    mc_world_provider_release(no_sky);
    CHECK(mc_world_runtime_retire_entity_containers(world, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_store_retire_chunk(f->store, state, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_retire_world(world, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_free(world, NULL) == MC_WORLD_STORAGE_OK);
    mc_world_provider_release(dimension);
    CHECK(mc_world_runtime_classes_free(classes, NULL) == MC_WORLD_STORAGE_OK &&
          mc_registry_classes_free(registry, NULL) == MC_REGISTRY_OK &&
          mc_world_store_chunk_count(f->store) == 0);
    free(error);
    free(setter);
    free(owned);
    free(execution);
    return true;
}

static bool chest_nullable_factory_priority(fixture *f) {
    mc_registry_classes *registry = NULL;
    mc_world_runtime_classes *classes = NULL;
    const mc_registry_options options = {MC_REGISTRY_CLIENT, f->profile, f->materials, 0,
                                         mc_resource_files_source(f->files)};
    const mc_block *block = NULL;
    CHECK(mc_registry_classes_allocate(&options, &registry, NULL) == MC_REGISTRY_OK &&
          mc_registry_block_get(registry, 54, &block, NULL) == MC_REGISTRY_OK && block != NULL &&
          mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile, &trig, &classes,
                                          NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_classes_bind_registry(classes, registry, NULL) == MC_REGISTRY_OK);
    mc_tile_entity_piston_class_snapshot before, after;
    CHECK(mc_tile_entity_piston_class_get_snapshot(classes, &before) == MC_TILE_ENTITY_OK &&
          before.base_phase == MC_TILE_ENTITY_CLASS_COLD);
    /* Consume the real immutable native quota with actual checked arena
     * allocations, without changing a limit, byte counter or class phase.
     * This exercises factory active-use failure priority, not Java OOM timing. */
    const size_t allocation_before = registry->allocation_bytes;
    const size_t managed_before = registry->managed_current_bytes;
    CHECK(mc_registry_allocate(registry, 1) != NULL &&
          registry->allocation_bytes > allocation_before + 1u &&
          registry->managed_current_bytes == managed_before);
    const size_t overhead = registry->allocation_bytes - allocation_before - 1u;
    const size_t remaining = registry->options.max_allocation_bytes - registry->allocation_bytes -
                             registry->managed_current_bytes;
    CHECK(remaining > overhead && mc_registry_allocate(registry, remaining - overhead) != NULL &&
          registry->allocation_bytes + registry->managed_current_bytes ==
              registry->options.max_allocation_bytes);
    mc_entity_tick_execution *execution = calloc(1, sizeof(*execution));
    mc_world_runtime_execution_result *owned = calloc(1, sizeof(*owned));
    mc_world_mutation_error *error = calloc(1, sizeof(*error));
    CHECK(execution != NULL && owned != NULL && error != NULL);
    mc_entity_tick_execution_init(execution, NULL);
    mc_world_runtime_execution_result_init(owned);
    const mc_entity_tick_execution_site site = {"rw", "c", "(Lfd;III)V", 18};
    mc_entity_tick_execution_world_token root;
    CHECK(mc_entity_tick_execution_push_world(execution, NULL, site, &root) == MC_WORLD_STORAGE_OK);
    const mc_world_mutation_execution_context context = {
        .execution = execution,
        .code_classes = classes,
        .code = {{"lm", "e", "()V", 6}, {"lm", "e", "()V", 33}},
        .result = owned,
        .world = root};
    mc_world_mutation_execution_call *call = NULL;
    const bool completed =
        mc_world_mutation_execution_added(&context, NULL, block, 8, 64, 8, &call, error, site);
    mc_world_storage_status raw = MC_WORLD_STORAGE_OK;
    const mc_live_tile_entity_error *child =
        mc_world_mutation_execution_failed_tile_added(call, &raw);
    mc_entity_tick_execution_site null_site = {0};
    CHECK(!completed && call != NULL && child != NULL && raw == MC_WORLD_STORAGE_LIMIT &&
          child->bytecode == 15 && child->caller_bytecode == 0 &&
          child->initializer_bytecode == 22 && child->registry.status == MC_REGISTRY_LIMIT &&
          error->hook_bytecode == 15 && owned->ownership_status == MC_WORLD_STORAGE_OK &&
          !mc_world_mutation_execution_source_null(call, &null_site) &&
          mc_tile_entity_piston_class_get_snapshot(classes, &after) == MC_TILE_ENTITY_OK &&
          after.base_phase == MC_TILE_ENTITY_CLASS_FAILED &&
          after.base_failure == MC_TILE_ENTITY_LIMIT && after.base_name_count == 0 &&
          after.base_class_count == 0 && after.live_tile_entities == 0);
    memset(error, 0, sizeof(*error));
    CHECK(mc_world_mutation_execution_native_close(call) == MC_WORLD_STORAGE_OK &&
          mc_world_mutation_execution_retire(&call) == MC_WORLD_STORAGE_OK && call == NULL &&
          mc_entity_tick_execution_pop_world(execution, &root, true) == MC_WORLD_STORAGE_OK &&
          mc_entity_tick_execution_retire(execution) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_classes_free(classes, NULL) == MC_WORLD_STORAGE_OK &&
          mc_registry_classes_free(registry, NULL) == MC_REGISTRY_OK);
    free(error);
    free(owned);
    free(execution);
    return true;
}

static bool chest_use_nullable_controls(fixture *f) {
    CHECK(chest_nullable_late_setter(f) && chest_nullable_factory_priority(f));
    return true;
}
