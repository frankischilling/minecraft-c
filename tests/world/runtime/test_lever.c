/* Reuse genuine Store/Registry/Entity construction and retirement helpers.
 * The renamed pressure main is not run. These are native source contracts,
 * separate from an original-JAR comparison or an interactive client run. */
#define MC_PRESSURE_MAIN_NAME mc_lever_pressure_fixture_main
#include "test_pressure_plate.c"
#undef MC_PRESSURE_MAIN_NAME

static bool lever_cell(mc_world_runtime *world, int32_t id, int32_t metadata) {
    int32_t actual_id = -1, actual_metadata = -1;
    return mc_world_runtime_block_get(world, 12, 64, 8, &actual_id, NULL) == MC_WORLD_STORAGE_OK &&
           mc_world_runtime_metadata_get(world, 12, 64, 8, &actual_metadata, NULL) ==
               MC_WORLD_STORAGE_OK &&
           actual_id == id && actual_metadata == metadata;
}

static bool lever_set(mc_world_runtime *world, int32_t x, int32_t y, int32_t z, int32_t id,
                      int32_t metadata) {
    bool changed = false;
    return mc_world_runtime_block_metadata_set(world, x, y, z, id, metadata, &changed, NULL) ==
           MC_WORLD_STORAGE_OK;
}

static bool lever_call(mc_world_runtime *world, const mc_block *lever, bool removed,
                       mc_world_mutation_error *error) {
    if (mc_world_runtime_call_enter(world, NULL) != MC_WORLD_STORAGE_OK)
        return false;
    *error = mc_world_mutation_empty_error();
    const bool completed =
        removed ? mc_world_mutation_block_removed(world, lever, 12, 64, 8, error)
                : mc_world_mutation_block_neighbor(world, lever, 12, 64, 8, 923, error);
    mc_world_runtime_call_leave(world);
    return completed;
}

static bool lever_contracts(fixture *f) {
    bool ok = false;
    mc_world_runtime *world = f->worlds[0];
    mc_block *lever = f->registry[0]->blocks.m[69];
    mc_world_mutation_error error = mc_world_mutation_empty_error();
    const int32_t xs[6] = {11, 13, 12, 12, 12, 12};
    const int32_t ys[6] = {64, 64, 64, 64, 63, 63};
    const int32_t zs[6] = {8, 8, 7, 9, 8, 8};
    CHECK(lever != NULL && lever->kind == MC_BLOCK_LEVER);
    for (size_t i = 0; i < 5; ++i)
        CHECK(lever_set(world, xs[i], ys[i], zs[i], 1, 0));
    for (int32_t powered = 0; powered <= 8; powered += 8)
        for (int32_t orientation = 1; orientation <= 6; ++orientation) {
            const size_t i = (size_t)orientation - 1u;
            CHECK(lever_set(world, 12, 64, 8, 69, powered + orientation) &&
                  lever_cell(world, 69, powered + orientation));
            CHECK(lever_call(world, lever, false, &error) && error.hook_class == NULL &&
                  error.hook_method == NULL && error.hook_bytecode == 0 &&
                  lever_cell(world, 69, powered + orientation));
            CHECK(lever_call(world, lever, true, &error) && error.hook_class == NULL &&
                  error.hook_method == NULL && error.hook_bytecode == 0 &&
                  lever_cell(world, 69, powered + orientation));
            /* Change the real support quietly, then enter actual neighbor
             * dispatch. Other supports stay present, so private h returns true
             * and the later orientation-specific sequence must detach it.
             * B skips the inherited drop; it is not a Lever neighbor guard. */
            bool changed = false;
            CHECK(mc_world_runtime_block_set_quiet(world, xs[i], ys[i], zs[i], 0, &changed,
                                                   &error) == MC_WORLD_STORAGE_OK &&
                  changed);
            world->fields.B = true;
            const bool detached = lever_call(world, lever, false, &error);
            world->fields.B = false;
            CHECK(detached && lever_cell(world, 0, 0) && world->fields.b->count == 0 &&
                  world->local_leases == 0 && world->active_calls == 0);
            CHECK(lever_set(world, xs[i], ys[i], zs[i], 1, 0));
        }
    /* A surviving alternate support does not make orientation zero or seven
     * detach. The original compares only its six explicit orientation values. */
    const int32_t unbound[] = {0, 7, 8, 15};
    for (size_t i = 0; i < COUNT(unbound); ++i) {
        const int32_t metadata = unbound[i];
        CHECK(lever_set(world, 12, 64, 8, 69, metadata));
        CHECK(lever_call(world, lever, false, &error) && lever_cell(world, 69, metadata));
    }
    CHECK(lever_set(world, 12, 64, 8, 0, 0));
    for (size_t i = 0; i < 5; ++i)
        CHECK(lever_set(world, xs[i], ys[i], zs[i], 0, 0));
    CHECK(lever_set(world, 12, 64, 8, 69, 5));
    world->fields.B = true;
    const bool unsupported = lever_call(world, lever, false, &error);
    world->fields.B = false;
    CHECK(unsupported && lever_cell(world, 0, 0) && world->fields.b->count == 0);

    bool survived = true;
    CHECK(lever_set(world, 12, 64, 8, 69, 5));
    CHECK(mc_world_runtime_call_enter(world, NULL) == MC_WORLD_STORAGE_OK);
    world->fields.B = true;
    const bool survival_returned =
        mc_world_mutation_lever_survive(world, lever, 12, 64, 8, &survived, &error);
    world->fields.B = false;
    mc_world_runtime_call_leave(world);
    CHECK(survival_returned && !survived && lever_cell(world, 0, 0));

    /* NULL and missing-provider failures preserve out and actual current/deep
     * hooks. No substitute World, source support or successful result is used. */
    bool ordinary = true;
    error = mc_world_mutation_empty_error();
    CHECK(!mc_world_mutation_lever_can_place(NULL, lever, 12, 64, 8, &ordinary, &error) &&
          ordinary && strcmp(error.hook_class, "xr") == 0 &&
          strcmp(error.hook_method, "a(Lfd;III)Z") == 0 && error.hook_bytecode == 7 &&
          error.lever_source_null);
    error = mc_world_mutation_empty_error();
    CHECK(!mc_world_mutation_lever_neighbor(NULL, lever, 12, 64, 8, 0, &error) &&
          strcmp(error.hook_class, "xr") == 0 && strcmp(error.hook_method, "b(Lfd;IIII)V") == 0 &&
          error.hook_bytecode == 6 && strcmp(error.cause_hook_class, "xr") == 0 &&
          strcmp(error.cause_hook_method, "a(Lfd;III)Z") == 0 && error.cause_hook_bytecode == 7);
    error = mc_world_mutation_empty_error();
    CHECK(!mc_world_mutation_lever_removed(NULL, lever, 12, 64, 8, &error) &&
          strcmp(error.hook_class, "xr") == 0 && strcmp(error.hook_method, "b(Lfd;III)V") == 0 &&
          error.hook_bytecode == 5);
    mc_chunk_provider *const provider = world->fields.v;
    world->fields.v = NULL;
    const bool missing_provider = lever_call(world, lever, false, &error);
    world->fields.v = provider;
    CHECK(!missing_provider && error.hook_bytecode == 6 && error.cause_hook_bytecode == 7 &&
          error.nearby.query.provider_bytecode == 6 && world->local_leases == 0 &&
          world->active_calls == 0);

    /* Real inherited drop publication after support loss. Source Random and
     * constructors run normally; this control does not assert their entropy. */
    CHECK(lever_set(world, 12, 63, 8, 1, 0));
    CHECK(lever_set(world, 12, 64, 8, 69, 5));
    CHECK(lever_set(world, 12, 63, 8, 0, 0) && lever_cell(world, 0, 0) &&
          world->fields.b->count == 1 && world->local_leases == 0 && world->active_calls == 0);
    ok = true;
cleanup:
    return ok;
}

int main(int argc, char **argv) {
    int result = 1;
    fixture *f = NULL;
    char root[8192];
    const uint16_t title[] = {'L', 'e', 'v', 'e', 'r'};
    if (argc != 3 || strcmp(argv[1], "world-lever") != 0 ||
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
    CHECK(lever_contracts(f));
    result = 0;
cleanup:
    if (!plate_cleanup(f)) {
        fputs("Lever cleanup refused; surviving owners retained\n", stderr);
        return 1;
    }
    if (result == 0 && printf("PASS: %u Lever checks (world-lever)\n", checks) < 0)
        result = 1;
    return result;
}
