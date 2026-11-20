/* Reuse real Store/Registry/Entity construction and checked retirement.
 * Earlier fixture mains are renamed and never run. */
#define MC_PRESSURE_MAIN_NAME mc_ladder_pressure_fixture_main
#include "test_pressure_plate.c"
#undef MC_PRESSURE_MAIN_NAME

#include "../../../src/world/chunk/object_state_internal.h"

static bool ladder_cell(mc_world_runtime *world, int32_t id, int32_t metadata) {
    int32_t current_id = -1, current_metadata = -1;
    return mc_world_runtime_block_get(world, 12, 64, 8, &current_id, NULL) == MC_WORLD_STORAGE_OK &&
           mc_world_runtime_metadata_get(world, 12, 64, 8, &current_metadata, NULL) ==
               MC_WORLD_STORAGE_OK &&
           current_id == id && current_metadata == metadata;
}

static bool ladder_set(mc_world_runtime *world, int32_t x, int32_t y, int32_t z, int32_t id,
                       int32_t metadata) {
    bool changed = false;
    return mc_world_runtime_block_metadata_set_quiet(world, x, y, z, id, metadata, &changed,
                                                     NULL) == MC_WORLD_STORAGE_OK;
}

static bool ladder_call(mc_world_runtime *world, const mc_block *block,
                        mc_world_mutation_error *error) {
    if (mc_world_runtime_call_enter(world, NULL) != MC_WORLD_STORAGE_OK)
        return false;
    mc_world_mutation_error_init(error);
    const bool done = mc_world_mutation_block_neighbor(world, block, 12, 64, 8, 923, error);
    mc_world_runtime_call_leave(world);
    return done;
}

typedef struct ladder_observer {
    size_t added, cells;
    bool drop_before_air;
} ladder_observer;

static mc_world_storage_status ladder_added(void *context, mc_world_runtime *world, mc_entity *item,
                                            mc_world_storage_error *error) {
    (void)item;
    (void)error;
    ladder_observer *observer = context;
    ++observer->added;
    observer->drop_before_air = ladder_cell(world, 65, 2);
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status ladder_cell_failure(void *context, mc_world_runtime *world,
                                                   int32_t x, int32_t y, int32_t z,
                                                   mc_world_storage_error *error) {
    ladder_observer *observer = context;
    ++observer->cells;
    *error =
        (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR, "Ladder reached cell callback", 57};
    if (x != 12 || y != 64 || z != 8 || !ladder_cell(world, 0, 0))
        error->status = MC_WORLD_STORAGE_INVALID_DATA;
    return error->status;
}

static bool ladder_payload(mc_entity *item) {
    mc_item_stack *stack = NULL;
    mc_item_stack_fields fields;
    const bool same = mc_item_entity_stack_get(item, &stack) == MC_ENTITY_OK &&
                      mc_item_stack_fields_get(stack, &fields) == MC_ITEM_STACK_OK &&
                      fields.id == 65 && fields.count == 1 && fields.damage == 0;
    mc_item_stack_release(stack);
    return same;
}

static bool ladder_row(fixture *f, int32_t metadata, int32_t missing, bool returned,
                       uint64_t random_before) {
    mc_world_runtime *world = f->worlds[0];
    int32_t id_after = -1, metadata_after = -1;
    if (mc_world_runtime_block_get(world, 12, 64, 8, &id_after, NULL) != MC_WORLD_STORAGE_OK ||
        mc_world_runtime_metadata_get(world, 12, 64, 8, &metadata_after, NULL) !=
            MC_WORLD_STORAGE_OK)
        return false;
    return printf("LADDER_NEIGHBOR {\"profile\":\"%s%d\",\"metadata\":%d,\"missing\":%d,"
                  "\"returned\":%s,\"exception\":null,\"id_after\":%d,\"metadata_after\":%d,"
                  "\"dirty_after\":%s,\"world_random_unchanged\":%s}\n",
                  missing < 0 ? "all-walls-" : "missing-wall-", metadata, metadata, missing,
                  returned ? "true" : "false", id_after, metadata_after,
                  f->states[0]->dirty ? "true" : "false",
                  world->fields.r->state == random_before ? "true" : "false") > 0;
}

static bool ladder_contracts(fixture *f) {
    bool ok = false, registered = false;
    mc_world_runtime *world = f->worlds[0];
    const mc_block *ladder = f->registry[0]->blocks.m[65];
    mc_world_mutation_error *error = malloc(sizeof(*error));
    mc_world_access *access = NULL;
    ladder_observer observer = {0};
    const int32_t xs[4] = {12, 12, 13, 11};
    const int32_t zs[4] = {9, 7, 8, 8};
    CHECK(error != NULL && ladder != NULL && ladder->kind == MC_BLOCK_LADDER);
    CHECK(ladder_set(world, 12, 64, 9, 1, 0));
    CHECK(ladder_set(world, 12, 64, 8, 65, 2));
    CHECK(ladder_call(world, ladder, error) && ladder_cell(world, 65, 2));
    for (size_t i = 0; i < COUNT(xs); ++i)
        CHECK(ladder_set(world, xs[i], 64, zs[i], 1, 0));
    /* Full metadata equality, not metadata&7: invalid orientations detach even
     * when all four walls are present. B skips only the inherited drop body. */
    world->fields.B = true;
    for (int32_t metadata = 0; metadata < 16; ++metadata) {
        CHECK(ladder_set(world, 12, 64, 8, 65, metadata));
        const mc_java_random before = *world->fields.r;
        f->states[0]->dirty = false;
        const bool returned = ladder_call(world, ladder, error);
        CHECK(returned && ladder_row(f, metadata, -1, returned, before.state));
        const bool supported = metadata >= 2 && metadata <= 5;
        CHECK(ladder_cell(world, supported ? 65 : 0, supported ? metadata : 0) &&
              error->status == MC_WORLD_STORAGE_OK && error->hook_class == NULL &&
              error->hook_method == NULL && error->hook_bytecode == 0 &&
              world->fields.r->state == before.state && world->fields.b->count == 0 &&
              world->active_calls == 0 && world->local_leases == 0);
    }
    for (size_t i = 0; i < COUNT(xs); ++i) {
        CHECK(ladder_set(world, xs[i], 64, zs[i], 0, 0));
        CHECK(ladder_set(world, 12, 64, 8, 65, (int32_t)i + 2));
        const uint64_t random_before = world->fields.r->state;
        f->states[0]->dirty = false;
        const bool returned = ladder_call(world, ladder, error);
        CHECK(returned && ladder_cell(world, 0, 0) && world->fields.b->count == 0 &&
              ladder_row(f, (int32_t)i + 2, (int32_t)i, returned, random_before));
        CHECK(ladder_set(world, xs[i], 64, zs[i], 1, 0));
    }
    world->fields.B = false;
    CHECK(ladder_set(world, 12, 64, 8, 65, 2));
    mc_world_chunk_nibble *saved = f->chunks[0]->e;
    f->chunks[0]->e = NULL;
    const bool failed_metadata = ladder_call(world, ladder, error);
    f->chunks[0]->e = saved;
    CHECK(!failed_metadata && error->status == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          strcmp(error->hook_class, "dp") == 0 && error->hook_bytecode == 5 &&
          !error->ladder_source_null && ladder_cell(world, 65, 2) && world->fields.b->count == 0 &&
          world->active_calls == 0 && world->local_leases == 0);
    mc_world_mutation_error_init(error);
    CHECK(!mc_world_mutation_ladder_neighbor(NULL, ladder, 12, 64, 8, 923, error) &&
          error->ladder_source_null && error->hook_bytecode == 5);

    /* Actual drop selection, Item construction and membership precede removal. */
    CHECK(ladder_set(world, 12, 64, 9, 0, 0));
    CHECK(ladder_call(world, ladder, error) && ladder_cell(world, 0, 0) &&
          world->fields.b->count == 1 && ladder_payload(world->fields.b->items[0]));
    CHECK(ladder_set(world, 12, 64, 8, 65, 2));
    const mc_world_access_callbacks callbacks = {
        .cell = ladder_cell_failure, .range = plate_sound_range, .entity_added = ladder_added};
    CHECK(mc_world_access_create(&callbacks, &observer, &access, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(world, access, NULL) == MC_WORLD_STORAGE_OK);
    registered = true;
    CHECK(!ladder_call(world, ladder, error) && error->status == MC_WORLD_STORAGE_IO_ERROR &&
          error->cause.system_error == 57 && strcmp(error->hook_class, "dp") == 0 &&
          error->hook_bytecode == 123 && observer.added == 1 && observer.cells == 1 &&
          observer.drop_before_air && ladder_cell(world, 0, 0) && world->fields.b->count == 2 &&
          ladder_payload(world->fields.b->items[1]) && world->active_calls == 0 &&
          world->local_leases == 0);
    ok = true;
cleanup:
    if (world != NULL)
        world->fields.B = false;
    if (registered) {
        bool removed = false;
        if (mc_world_runtime_remove_world_access(world, access, &removed, NULL) !=
                MC_WORLD_STORAGE_OK ||
            !removed)
            ok = false;
    }
    if (mc_world_access_free(access, NULL) != MC_WORLD_STORAGE_OK)
        ok = false;
    free(error);
    return ok;
}

int main(int argc, char **argv) {
    int result = 1;
    fixture *f = NULL;
    char root[8192];
    const uint16_t title[] = {'L', 'a', 'd', 'd', 'e', 'r'};
    if (argc != 3 || strcmp(argv[1], "world-ladder") != 0 ||
        !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 1;
    f = calloc(1, sizeof(*f));
    CHECK(f != NULL && mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_materials_create(&f->materials));
    CHECK(mc_java_random_profile_create(4u * 1024u * 1024u, &f->profile) ==
          MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
          MC_WORLD_INFO_OK);
    CHECK(mc_item_stack_allocate(&f->stack) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(f->stack, 262, 5, 7) == MC_ITEM_STACK_OK);
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
        CHECK(mc_registry_classes_initialize_blocks(f->registry[side], NULL) == MC_REGISTRY_OK &&
              r->opens == 3 && r->closes == 1 && !r->bad);
    }
    for (size_t slot = 0; slot < WORLD_COUNT; ++slot)
        CHECK(plate_open_store(f, slot, root) && plate_world(f, slot));
    CHECK(ladder_contracts(f));
    result = 0;
cleanup:
    if (!plate_cleanup(f)) {
        fputs("Ladder cleanup refused; surviving owners retained\n", stderr);
        return 1;
    }
    if (result == 0 && printf("PASS: %u Ladder checks (world-ladder)\n", checks) < 0)
        result = 1;
    return result;
}
