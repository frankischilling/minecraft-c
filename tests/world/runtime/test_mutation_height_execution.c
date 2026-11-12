/* Reuse the constructed GAME World, Store and normal Chunk fixture. */
int mc_mutation_height_unused_main(int, char **);
#define main mc_mutation_height_unused_main
#include "test_mutations.c"
#undef main

#include "../../../src/world/runtime/mutation/chunk_execution_internal.h"
#include "../../../src/world/runtime/world_access_internal.h"
#include "mc/world/chunk/nibbles.h"

typedef enum height_mode { HEIGHT_RISE, HEIGHT_FALL, HEIGHT_THROW, HEIGHT_SCOPE_DRIFT } height_mode;

typedef struct height_attempt {
    mc_entity_tick_execution execution;
    mc_world_runtime_execution_result owned;
    mc_world_mutation_chunk_execution call;
    mc_world_mutation_error error;
    mc_entity_tick_execution_world_token root, external;
    mc_world_runtime *world;
    mc_chunk_state *state;
    mc_world_access *listener;
    mc_world_access_receipt *receipt;
    int32_t x, z;
    size_t light_count, ranges;
    uint64_t occurrence;
    height_mode mode;
    bool root_open, external_open, registered, valid;
} height_attempt;

static bool same_world_token(mc_entity_tick_execution_world_token a,
                             mc_entity_tick_execution_world_token b) {
    return a.owner == b.owner && a.slot == b.slot && a.generation == b.generation;
}

static mc_world_storage_status height_range(void *context, mc_world_runtime *world, int32_t min_x,
                                            int32_t min_y, int32_t min_z, int32_t max_x,
                                            int32_t max_y, int32_t max_z,
                                            mc_world_storage_error *cause,
                                            mc_world_access_failure *failure) {
    height_attempt *a = context;
    mc_entity_tick_execution_world_token current = {0};
    mc_world_runtime *captured = NULL;
    mc_world_light_list_view list = {0};
    const bool falling = a->mode == HEIGHT_FALL;
    const size_t target = cell(a->x, 4, a->z);
    const size_t column = (size_t)a->z * 16u + (size_t)a->x;
    a->valid = ++a->ranges == 1 && world == a->world && min_x == a->x && max_x == a->x &&
               min_z == a->z && max_z == a->z && min_y == 2 && max_y == 5 &&
               a->state->data->heightmap[column] == (falling ? 5 : 2) &&
               a->state->data->blocks[target] == (falling ? 0 : 1) &&
               nibble(a->state->data->metadata, target) == 0 && !a->state->dirty &&
               a->call.active && a->call.lease_held && a->call.binding_held &&
               a->call.height_observer.entered && a->call.height_observer.range &&
               mc_entity_tick_execution_current_world(&a->execution, &current) &&
               same_world_token(current, a->call.height_observer.world) &&
               !same_world_token(current, a->root) &&
               mc_entity_tick_execution_read_world(&a->execution, &current, &captured) &&
               captured == world && a->call.height_observer.receiver_site.pc == 83 &&
               mc_world_runtime_light_list_get(world, &list, NULL) == MC_WORLD_STORAGE_OK &&
               list.count == a->light_count;
    if (!a->valid)
        return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    if (a->mode == HEIGHT_THROW) {
        const mc_world_storage_status status =
            mc_world_access_receipt_create(a->listener, NULL, &a->receipt, failure, cause);
        if (status != MC_WORLD_STORAGE_OK)
            return status;
        a->receipt->exception_class = "java/lang/IllegalStateException";
        *cause = (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR, "Height range observer", 19};
        return cause->status;
    }
    if (a->mode == HEIGHT_SCOPE_DRIFT) {
        const mc_entity_tick_execution_site site = {"test", "height_range", "()V", 0};
        const mc_world_storage_status status =
            mc_entity_tick_execution_push_world(&a->execution, world, site, &a->external);
        a->external_open = status == MC_WORLD_STORAGE_OK;
        return status;
    }
    return MC_WORLD_STORAGE_OK;
}

static bool height_region(mc_world_runtime *world, size_t index, mc_light_kind kind,
                          const mc_light_bounds *expected) {
    const mc_world_light_region *region = NULL;
    if (mc_world_runtime_light_at(world, index, &region, NULL) != MC_WORLD_STORAGE_OK)
        return false;
    const mc_world_light_region_fields *fields = mc_world_runtime_light_region_fields_get(region);
    const mc_world_light_type_fields *type =
        fields == NULL ? NULL : mc_world_runtime_light_type_fields_get(fields->a);
    return type != NULL && type->kind == kind && fields->bounds.min_x == expected->min_x &&
           fields->bounds.min_y == expected->min_y && fields->bounds.min_z == expected->min_z &&
           fields->bounds.max_x == expected->max_x && fields->bounds.max_y == expected->max_y &&
           fields->bounds.max_z == expected->max_z;
}

static bool finish_height(height_attempt *a) {
    if (a->external_open) {
        if (mc_entity_tick_execution_pop_world(&a->execution, &a->external, false) !=
            MC_WORLD_STORAGE_OK)
            return false;
        a->external_open = false;
    }
    if (a->call.execution != NULL &&
        mc_world_mutation_chunk_native_close(&a->call) != MC_WORLD_STORAGE_OK)
        return false;
    if (a->root_open && mc_entity_tick_execution_pop_world(
                            &a->execution, &a->root, !a->call.completed) != MC_WORLD_STORAGE_OK)
        return false;
    a->call = (mc_world_mutation_chunk_execution){0};
    a->owned = (mc_world_runtime_execution_result){0};
    mc_world_mutation_error_init(&a->error);
    if (mc_entity_tick_execution_retire(&a->execution) != MC_WORLD_STORAGE_OK)
        return false;
    if (a->registered) {
        bool removed = false;
        uint64_t identity = 0;
        if (mc_world_access_remove_receipted(a->world, a->listener, &removed, &identity, NULL) !=
                MC_WORLD_STORAGE_OK ||
            !removed || identity != a->occurrence)
            return false;
    }
    return mc_world_access_receipt_retire(&a->receipt, NULL) == MC_WORLD_STORAGE_OK &&
           mc_world_access_free(a->listener, NULL) == MC_WORLD_STORAGE_OK;
}

static bool run_height(fixture *f, int32_t x, int32_t z, height_mode mode) {
    bool ok = false;
    height_attempt *a = calloc(1, sizeof(*a));
    CHECK(a != NULL);
    mc_entity_tick_execution_init(&a->execution, NULL);
    a->world = f->worlds[0];
    a->state = f->chunks[0].state;
    a->x = x;
    a->z = z;
    a->mode = mode;
    const bool falling = mode == HEIGHT_FALL;
    const bool ordinary = mode == HEIGHT_RISE || falling;
    const size_t target = cell(x, 4, z), column = (size_t)z * 16u + (size_t)x;
    mc_chunk_data *const data = a->state->data;
    a->state->dirty = false;
    if (!falling) {
        data->blocks[cell(x, 1, z)] = 1;
        data->heightmap[column] = 2;
        CHECK(mc_chunk_nibbles_set(data->metadata, MC_CHUNK_NIBBLE_BYTES, x, 4, z, 7));
    }
    if (ordinary) {
        const uint8_t adjacent = falling ? 2 : 5;
        data->heightmap[column - 1] = data->heightmap[column + 1] = adjacent;
        data->heightmap[column - 16] = data->heightmap[column + 16] = adjacent;
    }
    const uint8_t prior_sky = nibble(data->skylight, target);
    mc_world_light_list_view before = {0}, after = {0};
    CHECK(mc_world_runtime_light_list_get(a->world, &before, NULL) == MC_WORLD_STORAGE_OK);
    a->light_count = before.count;
    const mc_world_access_source_callbacks callbacks = {.range = height_range};
    CHECK(mc_world_access_source_create(&callbacks, a, &a->listener, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_access_add_receipted(a->world, a->listener, &a->occurrence, NULL) ==
          MC_WORLD_STORAGE_OK);
    a->registered = true;
    const mc_entity_tick_execution_site caller = {"fd", "c", "(IIII)Z", 66};
    const mc_world_runtime_execution_code code = {{"lm", "e", "()V", 6}, {"lm", "e", "()V", 33}};
    CHECK(mc_entity_tick_execution_push_world(&a->execution, a->world, caller, &a->root) ==
          MC_WORLD_STORAGE_OK);
    a->root_open = true;
    mc_world_mutation_chunk_execution_init(&a->call, &a->execution, code, f->classes[0],
                                           f->registry[0], caller, a->root, &a->owned);
    mc_world_mutation_error_init(&a->error);
    mc_world_storage_status raw = MC_WORLD_STORAGE_LIMIT;
    bool changed = !ordinary;
    const bool completed = mc_world_mutation_chunk_block_in_execution(
        &a->call, a->world, f->stores[0], a->state, f->chunks[0].object, x, 4, z, falling ? 0 : 1,
        &changed, &a->error, &raw);
    CHECK(a->valid && a->ranges == 1 && a->call.height_observer.callbacks_returned == 1 &&
          mc_world_runtime_light_list_get(a->world, &after, NULL) == MC_WORLD_STORAGE_OK &&
          before.identity == after.identity && data->blocks[target] == (falling ? 0 : 1) &&
          nibble(data->metadata, target) == 0);
    if (ordinary) {
        const int32_t wx = a->state->x * 16 + x, wz = a->state->z * 16 + z;
        const mc_light_bounds wide = {wx - 1, falling ? 0 : 2, wz - 1,
                                      wx + 1, falling ? 2 : 5, wz + 1};
        const mc_light_bounds vertical = {wx, 2, wz, wx, 5, wz};
        const mc_light_bounds block = {wx, 4, wz, wx, 4, wz};
        CHECK(completed && changed && raw == MC_WORLD_STORAGE_OK && a->call.source_returned &&
              a->call.completed && !a->call.lease_held && !a->call.binding_held &&
              a->call.world_captures == (falling ? 9u : 10u) &&
              a->owned.ownership_status == MC_WORLD_STORAGE_OK && !a->owned.root_pending &&
              !a->owned.selection_ready && data->heightmap[column] == (falling ? 2 : 5) &&
              a->state->dirty && nibble(data->skylight, target) == (falling ? 15 : 0) &&
              after.count == before.count + (falling ? 1u : 3u));
        CHECK(height_region(a->world, before.count + (falling ? 0u : 1u), MC_LIGHT_SKY, &wide));
        if (!falling)
            CHECK(height_region(a->world, before.count, MC_LIGHT_SKY, &vertical) &&
                  height_region(a->world, before.count + 2u, MC_LIGHT_BLOCK, &block));
    } else {
        const mc_world_notification_execution *observer =
            mc_world_mutation_chunk_failure_observer(&a->call);
        CHECK(!completed && changed && !a->call.source_returned && a->call.world_captures == 1 &&
              observer == &a->call.height_observer && observer->range &&
              a->error.chunk_bytecode == 156 && a->error.height_bytecode == 92 &&
              a->error.world_access_bytecode == 40 && data->heightmap[column] == 2 &&
              nibble(data->skylight, target) == prior_sky && !a->state->dirty &&
              before.count == after.count);
        if (mode == HEIGHT_THROW) {
            const mc_world_access_failure *held = NULL;
            CHECK(raw == MC_WORLD_STORAGE_IO_ERROR && observer->callback_status == raw &&
                  a->owned.ownership_status == MC_WORLD_STORAGE_OK && a->call.scope.consumed &&
                  !a->call.lease_held && a->error.cause.system_error == 19 &&
                  mc_world_access_failure_lease_inspect(observer->lease, &held) && held != NULL &&
                  held->receipt == a->receipt &&
                  a->error.world_access_failure.receipt == a->receipt &&
                  mc_world_access_receipt_retire(&a->receipt, NULL) == MC_WORLD_STORAGE_BUSY);
        } else {
            CHECK(raw == MC_WORLD_STORAGE_OK && observer->callback_status == MC_WORLD_STORAGE_OK &&
                  a->owned.ownership_status == MC_WORLD_STORAGE_BUSY && a->external_open &&
                  a->call.scope.attempted && !a->call.scope.consumed && a->call.lease_held &&
                  a->call.scope.initial_close_status == MC_WORLD_STORAGE_BUSY &&
                  mc_world_mutation_chunk_execution_owns_world(&a->call, &observer->world) &&
                  !mc_world_mutation_chunk_execution_owns_world(&a->call, &a->external) &&
                  mc_world_mutation_chunk_native_close(&a->call) == MC_WORLD_STORAGE_BUSY &&
                  a->call.lease_held);
        }
    }
    ok = true;
cleanup:
    if (a != NULL) {
        if (!finish_height(a)) {
            fputs("height execution cleanup refused\n", stderr);
            return false;
        }
        free(a);
    }
    return ok;
}

int main(int argc, char **argv) {
    bool ok = false, published = false;
    fixture *f = NULL;
    char root[8192];
    const uint16_t title[] = {'H', 'e', 'i', 'g', 'h', 't'};
    if (argc != 3 || strcmp(argv[1], "world-mutation-height-execution") != 0 ||
        !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 1;
    f = calloc(1, sizeof(*f));
    CHECK(f != NULL);
    CHECK(mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(open_store(f, 0, root, "/mutations") && mc_materials_create(&f->materials));
    CHECK(mc_java_random_profile_create(4u * 1024u * 1024u, &f->profile) ==
          MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
          MC_WORLD_INFO_OK);
    resource_source *r = &f->resources[0];
    r->streams[0] = (byte_stream){.data = language, .length = sizeof(language) - 1u};
    r->streams[1] = r->streams[2] = (byte_stream){.data = language};
    r->api = (mc_registry_resources){r, open_resource, close_resource, resource_ready};
    const mc_registry_options options = {MC_REGISTRY_CLIENT, f->profile, f->materials, 0, &r->api};
    CHECK(mc_registry_classes_allocate(&options, &f->registry[0], NULL) == MC_REGISTRY_OK);
    r->registry = f->registry[0];
    CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile, &trig, &f->classes[0],
                                          NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_classes_bind_registry(f->classes[0], f->registry[0], NULL) ==
          MC_REGISTRY_OK);
    CHECK(create_world(f, 0) && new_chunk(f, 0, 0));
    mc_chunk_state *state = f->chunks[0].state;
    CHECK(mc_chunk_cache_publish(f->provider[0].cache, state->x, state->z, state));
    published = true;
    CHECK(run_height(f, 8, 8, HEIGHT_RISE) && run_height(f, 8, 8, HEIGHT_FALL) &&
          run_height(f, 4, 4, HEIGHT_THROW) && run_height(f, 12, 12, HEIGHT_SCOPE_DRIFT) &&
          !r->bad);
    ok = true;
cleanup:
    if (published) {
        if (!drain_world(f, 0))
            return 1;
        f->chunks[0] = (chunk_slot){0}; /* The actual cache drain now tracks this identity. */
    }
    if (!cleanup_fixture(f) || !ok)
        return 1;
    return printf("PASS: %u owned incremental height checks (world-mutation-height-execution)\n",
                  checks) < 0;
}
