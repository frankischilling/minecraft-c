#define main mc_redstone_publication_fixture_main
#include "../test_entity_publication.c"
#undef main

#include "../../../../src/world/runtime/mutation/mutation_internal.h"
#include "mc/world/runtime/mutations.h"

/* Prepared substrate uses the actual loaded Chunk bytes. Every operation
 * under test enters the real notifying World setters and concrete hooks. */
static bool redstone_cell(fixture *f, size_t side, int32_t x, int32_t y, int32_t z, uint8_t id,
                          int32_t metadata) {
    if (x < 0 || x >= 16 || y < 0 || y >= 128 || z < 0 || z >= 16)
        return false;
    f->states[side]->data->blocks[((size_t)x << 11u) | ((size_t)z << 7u) | (size_t)y] = id;
    bool ignored;
    return mc_world_runtime_metadata_set_quiet(f->worlds[side], x, y, z, metadata, &ignored,
                                               NULL) == MC_WORLD_STORAGE_OK;
}

static bool redstone_value(fixture *f, size_t side, int32_t x, int32_t y, int32_t z, int32_t id,
                           int32_t metadata) {
    int32_t observed_id = -1, observed_metadata = -1;
    return mc_world_runtime_block_get(f->worlds[side], x, y, z, &observed_id, NULL) ==
               MC_WORLD_STORAGE_OK &&
           observed_id == id &&
           mc_world_runtime_metadata_get(f->worlds[side], x, y, z, &observed_metadata, NULL) ==
               MC_WORLD_STORAGE_OK &&
           observed_metadata == metadata;
}

static bool redstone_line(fixture *f, size_t side, mc_world_mutation_error *error) {
    bool ok = false;
    mc_world_runtime *world = f->worlds[side];
    const mc_java_random before = *world->fields.r;
    for (int32_t x = 2; x <= 13; ++x)
        CHECK(redstone_cell(f, side, x, 63, 8, 1, 0));
    CHECK(redstone_cell(f, side, 2, 64, 8, 70, 1));
    for (int32_t x = 3; x <= 12; ++x)
        CHECK(redstone_cell(f, side, x, 64, 8, 55, 0));
    CHECK(mc_world_runtime_metadata_set(world, 2, 64, 8, 1, error) == MC_WORLD_STORAGE_OK);
    for (int32_t x = 3; x <= 12; ++x)
        CHECK(redstone_value(f, side, x, 64, 8, 55, 18 - x));
    mc_registry_redstone_set_view view;
    const mc_block *redstone = f->registry[side]->blocks.m[55];
    CHECK(redstone->subtype.sm.a && !world->fields.o &&
          mc_registry_redstone_set_inspect(redstone->subtype.sm.b, &view) == MC_REGISTRY_OK &&
          view.size == 0 && f->registry[side]->redstone_snapshot_count == 0);
    CHECK(mc_world_runtime_metadata_set(world, 2, 64, 8, 0, error) == MC_WORLD_STORAGE_OK);
    for (int32_t x = 3; x <= 12; ++x)
        CHECK(redstone_value(f, side, x, 64, 8, 55, 0));
    CHECK(redstone->subtype.sm.a && !world->fields.o &&
          mc_registry_redstone_set_inspect(redstone->subtype.sm.b, &view) == MC_REGISTRY_OK &&
          view.size == 0);
    CHECK(mc_test_java_random_equal(world->fields.r, &before));

    /* Real addition/removal, followed by a second rising edge. */
    bool changed = false;
    CHECK(mc_world_runtime_block_set(world, 7, 64, 8, 0, &changed, error) == MC_WORLD_STORAGE_OK &&
          changed);
    CHECK(redstone_value(f, side, 7, 64, 8, 0, 0));
    CHECK(mc_world_runtime_metadata_set(world, 2, 64, 8, 1, error) == MC_WORLD_STORAGE_OK);
    CHECK(redstone_value(f, side, 6, 64, 8, 55, 12) && redstone_value(f, side, 8, 64, 8, 55, 0));
    CHECK(mc_world_runtime_block_set(world, 7, 64, 8, 55, &changed, error) == MC_WORLD_STORAGE_OK &&
          changed);
    for (int32_t x = 3; x <= 12; ++x)
        CHECK(redstone_value(f, side, x, 64, 8, 55, 18 - x));
    CHECK(mc_world_runtime_metadata_set(world, 2, 64, 8, 0, error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_test_java_random_equal(world->fields.r, &before) && world->active_calls == 0 &&
          world->local_leases == 0 && f->registry[side]->redstone_snapshot_count == 0);
    ok = true;
cleanup:
    if (!ok)
        fprintf(stderr, "redstone line: status=%d operation=%s hook=%s pc=%u update=%u drain=%u\n",
                error->status, error->operation != NULL ? error->operation : "none",
                error->hook_class != NULL ? error->hook_class : "none", error->hook_bytecode,
                error->redstone.update, error->redstone.drain);
    return ok;
}

static void redstone_empty(fixture *f, size_t side) {
    memset(f->states[side]->data->blocks, 0, MC_CHUNK_BLOCKS);
    memset(f->states[side]->data->metadata, 0, MC_CHUNK_NIBBLE_BYTES);
}

static bool redstone_long_path(fixture *f, size_t side, mc_world_mutation_error *error) {
    bool ok = false;
    int32_t xs[25], zs[25];
    size_t length = 0;
    redstone_empty(f, side);
    for (int32_t x = 3; x <= 12; ++x) {
        xs[length] = x;
        zs[length++] = 4;
    }
    for (int32_t z = 5; z <= 10; ++z) {
        xs[length] = 12;
        zs[length++] = z;
    }
    for (int32_t x = 11; x >= 3; --x) {
        xs[length] = x;
        zs[length++] = 10;
    }
    CHECK(length == COUNT(xs));
    CHECK(redstone_cell(f, side, 2, 63, 4, 1, 0) && redstone_cell(f, side, 2, 64, 4, 70, 1));
    for (size_t i = 0; i < length; ++i)
        CHECK(redstone_cell(f, side, xs[i], 63, zs[i], 1, 0) &&
              redstone_cell(f, side, xs[i], 64, zs[i], 55, 0));
    const mc_java_random before = *f->worlds[side]->fields.r;
    CHECK(mc_world_runtime_metadata_set(f->worlds[side], 2, 64, 4, 1, error) ==
          MC_WORLD_STORAGE_OK);
    for (size_t i = 0; i < length; ++i)
        CHECK(redstone_value(f, side, xs[i], 64, zs[i], 55, i < 15 ? 15 - (int32_t)i : 0));
    CHECK(mc_world_runtime_metadata_set(f->worlds[side], 2, 64, 4, 0, error) ==
          MC_WORLD_STORAGE_OK);
    for (size_t i = 0; i < length; ++i)
        CHECK(redstone_value(f, side, xs[i], 64, zs[i], 55, 0));
    CHECK(mc_test_java_random_equal(f->worlds[side]->fields.r, &before));
    ok = true;
cleanup:
    return ok;
}

static bool redstone_steps_and_loop(fixture *f, size_t side, mc_world_mutation_error *error) {
    bool ok = false;
    mc_world_runtime *world = f->worlds[side];
    for (unsigned int blocked = 0; blocked < 2; ++blocked) {
        redstone_empty(f, side);
        CHECK(redstone_cell(f, side, 2, 63, 4, 1, 0) && redstone_cell(f, side, 3, 63, 4, 1, 0) &&
              redstone_cell(f, side, 4, 64, 4, 1, 0) && redstone_cell(f, side, 5, 64, 4, 1, 0));
        CHECK(redstone_cell(f, side, 2, 64, 4, 70, 1) && redstone_cell(f, side, 3, 64, 4, 55, 0) &&
              redstone_cell(f, side, 4, 65, 4, 55, 0) && redstone_cell(f, side, 5, 65, 4, 55, 0));
        if (blocked)
            CHECK(redstone_cell(f, side, 3, 65, 4, 1, 0));
        CHECK(mc_world_runtime_metadata_set(world, 2, 64, 4, 1, error) == MC_WORLD_STORAGE_OK);
        CHECK(redstone_value(f, side, 3, 64, 4, 55, 15) &&
              redstone_value(f, side, 4, 65, 4, 55, blocked ? 0 : 14) &&
              redstone_value(f, side, 5, 65, 4, 55, blocked ? 0 : 13));
        CHECK(mc_world_runtime_metadata_set(world, 2, 64, 4, 0, error) == MC_WORLD_STORAGE_OK);
        CHECK(redstone_value(f, side, 3, 64, 4, 55, 0) && redstone_value(f, side, 4, 65, 4, 55, 0));
    }
    redstone_empty(f, side);
    CHECK(redstone_cell(f, side, 2, 63, 4, 1, 0) && redstone_cell(f, side, 2, 64, 4, 70, 1));
    for (int32_t x = 3; x <= 4; ++x)
        for (int32_t z = 4; z <= 5; ++z)
            CHECK(redstone_cell(f, side, x, 63, z, 1, 0) &&
                  redstone_cell(f, side, x, 64, z, 55, 0));
    CHECK(mc_world_runtime_metadata_set(world, 2, 64, 4, 1, error) == MC_WORLD_STORAGE_OK);
    CHECK(redstone_value(f, side, 3, 64, 4, 55, 15) && redstone_value(f, side, 4, 64, 4, 55, 14) &&
          redstone_value(f, side, 3, 64, 5, 55, 14) && redstone_value(f, side, 4, 64, 5, 55, 13));
    CHECK(mc_world_runtime_metadata_set(world, 2, 64, 4, 0, error) == MC_WORLD_STORAGE_OK);
    for (int32_t x = 3; x <= 4; ++x)
        for (int32_t z = 4; z <= 5; ++z)
            CHECK(redstone_value(f, side, x, 64, z, 55, 0));
    ok = true;
cleanup:
    return ok;
}

static bool redstone_prefixes(fixture *f, size_t side, mc_world_mutation_error *error) {
    bool ok = false;
    mc_world_runtime *world = f->worlds[side];
    mc_registry_classes *classes = f->registry[side];
    mc_block *redstone = classes->blocks.m[55];
    redstone_empty(f, side);
    CHECK(redstone_cell(f, side, 6, 63, 8, 1, 0) && redstone_cell(f, side, 7, 63, 8, 1, 0) &&
          redstone_cell(f, side, 6, 64, 8, 70, 1) && redstone_cell(f, side, 7, 64, 8, 55, 0));
    CHECK(mc_world_runtime_call_enter(world, NULL) == MC_WORLD_STORAGE_OK);
    mc_chunk_provider *const provider = world->fields.v;
    world->fields.B = true;
    world->fields.v = NULL;
    mc_world_mutation_error_init(error);
    const bool remote_added = mc_world_mutation_redstone_added(world, redstone, 7, 64, 8, error);
    const bool remote_removed =
        mc_world_mutation_redstone_removed(world, redstone, 7, 64, 8, error);
    const bool remote_neighbor =
        mc_world_mutation_redstone_neighbor(world, redstone, 7, 64, 8, 70, error);
    world->fields.B = false;
    world->fields.v = provider;
    mc_world_runtime_call_leave(world);
    CHECK(remote_added && remote_removed && remote_neighbor && redstone->subtype.sm.a &&
          redstone_value(f, side, 7, 64, 8, 55, 0));

    /* Native nullable inputs are restored before assertions. No successful
     * alternate Registry, WorldAccess or World service is installed. */
    CHECK(mc_world_runtime_call_enter(world, NULL) == MC_WORLD_STORAGE_OK);
    mc_block **const array = classes->blocks.m;
    classes->blocks.m = NULL;
    mc_world_mutation_error_init(error);
    const bool power_failed = mc_world_mutation_redstone_added(world, redstone, 7, 64, 8, error);
    classes->blocks.m = array;
    mc_world_runtime_call_leave(world);
    CHECK(!power_failed && error->status == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          error->redstone.added == 23 && error->redstone.drain == 10 &&
          error->redstone.update == 23 && !redstone->subtype.sm.a && !world->fields.o &&
          redstone_value(f, side, 7, 64, 8, 55, 0));
    redstone->subtype.sm.a = true;

    CHECK(mc_world_runtime_call_enter(world, NULL) == MC_WORLD_STORAGE_OK);
    mc_world_reference_list *const world_accesses = world->fields.u;
    world->fields.u = NULL;
    mc_world_mutation_error_init(error);
    const bool world_access_failed =
        mc_world_mutation_redstone_added(world, redstone, 7, 64, 8, error);
    world->fields.u = world_accesses;
    mc_world_runtime_call_leave(world);
    CHECK(!world_access_failed && error->status == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          error->redstone.update == (classes->blocks.t[55] ? 285u : 297u) &&
          error->world_access_bytecode == 9 && world->fields.o && redstone->subtype.sm.a &&
          redstone_value(f, side, 7, 64, 8, 55, 15));
    world->fields.o = false;

    CHECK(redstone_cell(f, side, 7, 64, 8, 55, 0));
    const size_t maximum = classes->options.max_allocation_bytes;
    classes->options.max_allocation_bytes =
        classes->allocation_bytes + classes->managed_current_bytes;
    CHECK(mc_world_runtime_call_enter(world, NULL) == MC_WORLD_STORAGE_OK);
    mc_world_mutation_error_init(error);
    const bool limited = mc_world_mutation_redstone_added(world, redstone, 7, 64, 8, error);
    mc_world_runtime_call_leave(world);
    classes->options.max_allocation_bytes = maximum;
    CHECK(!limited && error->registry.status == MC_REGISTRY_LIMIT &&
          error->redstone.update == 526 && !world->fields.o && redstone->subtype.sm.a &&
          redstone_value(f, side, 7, 64, 8, 55, 15) && classes->redstone_snapshot_count == 0 &&
          world->active_calls == 0 && world->local_leases == 0);
    ok = true;
cleanup:
    return ok;
}

static bool redstone_support_drop(fixture *f, size_t side, mc_world_mutation_error *error) {
    bool ok = false;
    mc_item_stack *stack = NULL;
    redstone_empty(f, side);
    CHECK(redstone_cell(f, side, 7, 64, 8, 55, 0));
    mc_world_runtime *world = f->worlds[side];
    const size_t count = world->fields.b->count;
    CHECK(mc_world_runtime_metadata_set(world, 6, 64, 8, 0, error) == MC_WORLD_STORAGE_OK);
    CHECK(redstone_value(f, side, 7, 64, 8, 0, 0) && world->fields.b->count == count + 1);
    mc_entity *item = world->fields.b->items[count];
    mc_item_stack_fields payload;
    CHECK(item != NULL && mc_item_entity_stack_get(item, &stack) == MC_ENTITY_OK && stack != NULL);
    CHECK(mc_item_stack_fields_get(stack, &payload) == MC_ITEM_STACK_OK && payload.id == 331 &&
          payload.count == 1 && payload.damage == 0);
    CHECK(world->active_calls == 0 && world->local_leases == 0 &&
          f->registry[side]->redstone_snapshot_count == 0);
    ok = true;
cleanup:
    mc_item_stack_release(stack);
    return ok;
}

int main(int argc, char **argv) {
    int result = 1;
    fixture *f = NULL;
    mc_world_mutation_error *error = NULL;
    char root[8192];
    const uint16_t title[] = {'W', 'i', 'r', 'e'};
    if (argc != 3 || strcmp(argv[1], "world-redstone") != 0 ||
        !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 1;
    error = malloc(sizeof(*error));
    f = calloc(1, sizeof(*f));
    CHECK(f != NULL && error != NULL);
    mc_world_mutation_error_init(error);
    CHECK(mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_materials_create(&f->materials));
    CHECK(mc_java_random_profile_create(4u * 1024u * 1024u, &f->profile) ==
          MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
          MC_WORLD_INFO_OK);
    for (size_t side = 0; side < 2; ++side) {
        resources *r = &f->sources[side];
        r->streams[0] = (stream){.bytes = language, .length = sizeof(language) - 1u};
        r->streams[1] = (stream){.bytes = language};
        r->streams[2] = (stream){.bytes = language};
        r->api = (mc_registry_resources){r, open_resource, close_resource, ready_resource};
        const mc_registry_options options = {side == 0 ? MC_REGISTRY_CLIENT : MC_REGISTRY_SERVER,
                                             f->profile, f->materials, 0, &r->api};
        CHECK(mc_registry_classes_allocate(&options, &f->registry[side], NULL) == MC_REGISTRY_OK);
        CHECK(mc_world_runtime_classes_create(
                  side == 0 ? MC_WORLD_INFO_CLIENT : MC_WORLD_INFO_SERVER, f->profile, &trig,
                  &f->classes[side], NULL) == MC_WORLD_STORAGE_OK);
        CHECK(mc_world_runtime_classes_bind_registry(f->classes[side], f->registry[side], NULL) ==
              MC_REGISTRY_OK);
        CHECK(mc_entity_classes_create(side == 0 ? MC_ENTITY_CLIENT : MC_ENTITY_SERVER, f->profile,
                                       NULL, &f->entity_classes[side]) == MC_ENTITY_OK);
        CHECK(mc_world_runtime_classes_bind_entities(f->classes[side], f->entity_classes[side]) ==
              MC_ENTITY_OK);
        CHECK(mc_registry_classes_initialize_blocks(f->registry[side], NULL) == MC_REGISTRY_OK);
        CHECK(open_store(f, side, root) && create_world(f, side));
        CHECK(redstone_line(f, side, error));
        CHECK(redstone_long_path(f, side, error) && redstone_steps_and_loop(f, side, error) &&
              redstone_prefixes(f, side, error) && redstone_support_drop(f, side, error));
    }
    result = 0;
cleanup:
    free(error);
    if (!cleanup_fixture(f))
        return 1;
    if (result == 0)
        printf("PASS: %u synchronous redstone checks\n", checks);
    return result;
}
