/* Real GAME World/Registry/Chunk owners and input regions. The included
 * fixture's fluid entry is not executed. All plants/light are serialized
 * before acquisition; callbacks mutate the actual loaded state. */
#define main mc_plant_fluid_fixture_main
#include "../test_fluids.c"
#undef main

#include "../../../../src/world/runtime/mutation/plants/plants_internal.h"

typedef struct plant_spec {
    int32_t id, support;
} plant_spec;

static const plant_spec species[] = {{6, 2},  {31, 2}, {32, 12}, {37, 2},  {38, 2},
                                     {39, 1}, {40, 1}, {59, 60}, {81, 12}, {83, 3}};

static input_cell plant_input(int32_t x, int32_t y, int32_t z) {
    if ((x == 28 || x == 29) && z == 28 && y >= 1)
        return (input_cell){(uint8_t)(x == 28 ? 81 : 83), 0};
    for (size_t i = 0; i < COUNT(species); ++i) {
        const int32_t px = 5 + (int32_t)i * 2;
        const int32_t pz = species[i].id == 39 || species[i].id == 40 ? 25 : 5;
        if (x == px && z == pz) {
            if (y == 12 || y == 24 || y == 32)
                return (input_cell){(uint8_t)species[i].id, (uint8_t)(species[i].id == 59 ? 7 : 2)};
            if (y == 11)
                return (input_cell){(uint8_t)species[i].support, 0};
        }
        if (species[i].id == 83 && x == px + 1 && z == pz && y == 11)
            return (input_cell){9, 0};
    }
    /* Three-block unsupported columns cross a Chunk border. */
    if ((x == 15 || x == 16) && z == 12 && y >= 40 && y <= 42)
        return (input_cell){(uint8_t)(x == 15 ? 81 : 83), 3};
    /* Growth age, height cap, blocked air, invalid support and partial write. */
    if (z == 9) {
        if (x == 5 || x == 10) {
            if (y == 49)
                return (input_cell){(uint8_t)(x == 5 ? 3 : 12), 0};
            if (y == 50)
                return (input_cell){(uint8_t)(x == 5 ? 83 : 81), 14};
        }
        if (x == 6 && y == 49)
            return (input_cell){9, 0};
        if (x == 15 && y >= 48 && y <= 50)
            return (input_cell){81, 15};
        if (x == 20 && (y == 50 || y == 51))
            return (input_cell){(uint8_t)(y == 50 ? 83 : 1), 15};
        if (x == 25 && y == 50)
            return (input_cell){81, 14};
        if (x == 10 && (y == 79 || y == 80))
            return (input_cell){(uint8_t)(y == 79 ? 12 : 81), (uint8_t)(y == 80 ? 15 : 0)};
        if (x == 11 && y == 81)
            return (input_cell){51, 0};
        if (x == 5 && (y == 126 || y == 127))
            return (input_cell){(uint8_t)(y == 126 ? 12 : 81), (uint8_t)(y == 127 ? 15 : 0)};
    }
    /* Two isolated mushrooms; their full candidate planes stay dark. */
    if (z >= 23 && z <= 29 && x >= 4 && x <= 12 && y == 59)
        return (input_cell){1, 0};
    if ((x == 6 || x == 10) && y == 60 && z == 26)
        return (input_cell){39, 0};
    /* Crop: wet 3x3 farmland, dry isolated, mature and dark branches. */
    if (y == 69 && x >= 4 && x <= 6 && z >= 14 && z <= 16)
        return (input_cell){60, 7};
    if (y == 69 && x == 10 && z == 15)
        return (input_cell){60, 0};
    if (y == 69 && (x == 15 || x == 20) && z == 15)
        return (input_cell){60, 7};
    if (y == 70 && z == 15 && (x == 5 || x == 10 || x == 15 || x == 20))
        return (input_cell){59, (uint8_t)(x == 15 ? 7 : 0)};
    /* Saplings: two-stage, blocked restoration for four species, actual trees. */
    if (z == 20 && (x == 5 || x == 10 || x == 15 || x == 20 || x == 25)) {
        if (y == 89 || y == 99)
            return (input_cell){2, 0};
        if (y == 90)
            return (input_cell){6, (uint8_t)(x == 5 ? 2 : 8 + (x / 5 - 2))};
        if (x != 5 && y == 91)
            return (input_cell){1, 0};
        if (x <= 20 && y == 100)
            return (input_cell){6, (uint8_t)(8 + (x / 5 - 1))};
    }
    if (z == 15 && y == 110 && (x == 5 || x == 10 || x == 15 || x == 20 || x == 25))
        return (input_cell){60, (uint8_t)(x == 5 ? 3 : x == 15 ? 1 : 0)};
    if (x == 1 && y == 110 && z == 11)
        return (input_cell){9, 0};
    if (y == 111 && z == 15 && (x == 10 || x == 25))
        return (input_cell){(uint8_t)(x == 10 ? 1 : 59), 0};
    if (x == 5 && y == 120 && z == 15)
        return (input_cell){60, 3};
    return (input_cell){0, 0};
}

static input_cell plant_lights(int32_t x, int32_t y, int32_t z) {
    const bool dark = z >= 23 || (x >= 19 && x <= 21 && y >= 69 && y <= 72 && z == 15);
    return (input_cell){(uint8_t)(dark ? 0 : 15), 0};
}

static bool plant_call(mc_world_runtime *world, const mc_block *block, int32_t x, int32_t y,
                       int32_t z, bool neighbor, mc_java_random *random,
                       mc_world_mutation_error *error) {
    if (mc_world_runtime_call_enter(world, NULL) != MC_WORLD_STORAGE_OK)
        return false;
    *error = mc_world_mutation_empty_error();
    const bool completed =
        neighbor ? mc_world_mutation_block_neighbor(world, block, x, y, z, INT32_MIN, error)
                 : mc_world_mutation_block_tick(world, block, x, y, z, random, error);
    mc_world_runtime_call_leave(world);
    return completed;
}

static bool plant_success(const mc_world_mutation_error *error) {
    return error->status == MC_WORLD_STORAGE_OK && error->hook_class == NULL &&
           error->hook_method == NULL && error->hook_bytecode == 0 &&
           error->plant.status == MC_WORLD_STORAGE_OK && !error->plant_tree.reached;
}

static bool plant_neighbors(fixture *f, size_t slot) {
    bool ok = false;
    mc_world_runtime *world = f->worlds[slot];
    mc_registry_classes *registry = f->registry[slot % 2u];
    mc_world_mutation_error error;
    for (size_t i = 0; i < COUNT(species); ++i) {
        const int32_t x = 5 + (int32_t)i * 2;
        const int32_t z = species[i].id == 39 || species[i].id == 40 ? 25 : 5;
        const mc_block *block = NULL;
        CHECK(mc_registry_block_get(registry, species[i].id, &block, NULL) == MC_REGISTRY_OK);
        mc_java_random *const random = world->fields.r;
        world->fields.r = NULL;
        const bool supported = plant_call(world, block, x, 12, z, true, NULL, &error);
        world->fields.r = random;
        CHECK(supported && plant_success(&error) &&
              at(world, x, 12, z, species[i].id, species[i].id == 59 ? 7 : 2));
        const mc_java_random before = *world->fields.r;
        const size_t count = world->fields.b->count;
        const bool remote = world->fields.B;
        world->fields.B = true;
        const bool removed = plant_call(world, block, x, 24, z, true, NULL, &error);
        world->fields.B = remote;
        CHECK(removed && plant_success(&error) && at(world, x, 24, z, 0, 0) &&
              world->fields.b->count == count &&
              mc_test_java_random_equal(&before, world->fields.r));
        CHECK(plant_call(world, block, x, 32, z, true, NULL, &error) && plant_success(&error) &&
              at(world, x, 32, z, 0, 0));
    }
    const mc_block *cactus = NULL, *reed = NULL, *flower = NULL;
    CHECK(mc_registry_block_get(registry, 81, &cactus, NULL) == MC_REGISTRY_OK &&
          mc_registry_block_get(registry, 83, &reed, NULL) == MC_REGISTRY_OK &&
          mc_registry_block_get(registry, 37, &flower, NULL) == MC_REGISTRY_OK);
    const size_t count = world->fields.b->count;
    CHECK(plant_call(world, cactus, 15, 40, 12, true, NULL, &error) && plant_success(&error) &&
          world->fields.b->count == count + 6u);
    for (int32_t y = 40; y <= 42; ++y)
        CHECK(at(world, 15, y, 12, 0, 0) && at(world, 16, y, 12, 0, 0));
    /* The captured reed receiver still drops on a later direct callback even
     * though the recursive notification has already removed its cell. */
    CHECK(plant_call(world, reed, 16, 40, 12, true, NULL, &error) && plant_success(&error) &&
          world->fields.b->count == count + 7u);
    /* Source drops precede removal. A failed World Random access leaves the
     * actual unsupported flower in place and reports the reached drop body. */
    bool changed;
    CHECK(mc_world_runtime_block_set_quiet(world, 8, 35, 8, 37, &changed, &error) ==
              MC_WORLD_STORAGE_OK &&
          changed);
    mc_java_random *const random = world->fields.r;
    world->fields.r = NULL;
    const bool failure = plant_call(world, flower, 8, 35, 8, true, NULL, &error);
    world->fields.r = random;
    CHECK(!failure && error.status == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          error.hook_bytecode == 17 && error.cause_hook_bytecode == 26 &&
          error.drop_bytecode == 32 && at(world, 8, 35, 8, 37, 0));
    ok = true;
cleanup:
    return ok;
}

static bool plant_upright(fixture *f, size_t slot) {
    bool ok = false;
    mc_world_runtime *world = f->worlds[slot];
    mc_registry_classes *registry = f->registry[slot % 2u];
    const mc_block *cactus = NULL, *reed = NULL;
    mc_world_mutation_error error;
    mc_java_random supplied, expected;
    mc_java_random_seed(&supplied, 23);
    expected = supplied;
    CHECK(mc_registry_block_get(registry, 81, &cactus, NULL) == MC_REGISTRY_OK &&
          mc_registry_block_get(registry, 83, &reed, NULL) == MC_REGISTRY_OK);
    CHECK(plant_call(world, reed, 5, 50, 9, false, &supplied, &error) && plant_success(&error) &&
          at(world, 5, 50, 9, 83, 15));
    CHECK(plant_call(world, reed, 5, 50, 9, false, NULL, &error) && plant_success(&error) &&
          at(world, 5, 50, 9, 83, 0) && at(world, 5, 51, 9, 83, 0));
    CHECK(plant_call(world, cactus, 10, 50, 9, false, NULL, &error) && plant_success(&error) &&
          at(world, 10, 50, 9, 81, 15));
    CHECK(plant_call(world, cactus, 10, 50, 9, false, NULL, &error) && plant_success(&error) &&
          at(world, 10, 50, 9, 81, 0) && at(world, 10, 51, 9, 81, 0));
    CHECK(plant_call(world, cactus, 15, 50, 9, false, NULL, &error) && plant_success(&error) &&
          at(world, 15, 50, 9, 81, 15) && at(world, 15, 51, 9, 0, 0));
    CHECK(plant_call(world, reed, 20, 50, 9, false, NULL, &error) && plant_success(&error) &&
          at(world, 20, 50, 9, 83, 15));
    CHECK(plant_call(world, cactus, 25, 50, 9, false, NULL, &error) && plant_success(&error) &&
          at(world, 25, 50, 9, 81, 15));
    CHECK(plant_call(world, cactus, 5, 127, 9, false, NULL, &error) && plant_success(&error) &&
          at(world, 5, 127, 9, 81, 0));
    /* The upper write completes before a fire neighbor fails. Age reset has
     * not run, and the newly placed upper block must remain published. */
    CHECK(!plant_call(world, cactus, 10, 80, 9, false, NULL, &error) &&
          error.status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && error.hook_bytecode == 74 &&
          at(world, 10, 80, 9, 81, 15) && at(world, 10, 81, 9, 81, 0));
    CHECK(mc_test_java_random_equal(&supplied, &expected));
    ok = true;
cleanup:
    return ok;
}

static bool plant_growth(fixture *f, size_t slot) {
    bool ok = false;
    mc_world_runtime *world = f->worlds[slot];
    mc_registry_classes *registry = f->registry[slot % 2u];
    const mc_block *mushroom = NULL, *crop = NULL, *sapling = NULL;
    mc_world_mutation_error error;
    mc_java_random supplied, expected;
    int32_t ignored;
    CHECK(mc_registry_block_get(registry, 39, &mushroom, NULL) == MC_REGISTRY_OK &&
          mc_registry_block_get(registry, 59, &crop, NULL) == MC_REGISTRY_OK &&
          mc_registry_block_get(registry, 6, &sapling, NULL) == MC_REGISTRY_OK);
    mc_java_random_seed(&supplied, 18);
    expected = supplied;
    const int32_t bounds[] = {100, 3, 2, 2, 3, 3, 3};
    for (size_t i = 0; i < COUNT(bounds); ++i)
        CHECK(mc_java_random_next_bounded(&expected, bounds[i], &ignored));
    CHECK(plant_call(world, mushroom, 6, 60, 26, false, &supplied, &error) &&
          plant_success(&error) && at(world, 7, 60, 27, 39, 0) &&
          mc_test_java_random_equal(&supplied, &expected));
    /* First candidate is occupied on the repeat; skip both dead-local draws. */
    mc_java_random_seed(&supplied, 18);
    expected = supplied;
    for (size_t i = 0; i < 5; ++i)
        CHECK(mc_java_random_next_bounded(&expected, bounds[i], &ignored));
    CHECK(plant_call(world, mushroom, 6, 60, 26, false, &supplied, &error) &&
          plant_success(&error) && mc_test_java_random_equal(&supplied, &expected));
    CHECK(!plant_call(world, mushroom, 10, 60, 26, false, NULL, &error) &&
          error.hook_bytecode == 4);
    /* Wet isolated farmland yields factor10 and bound10. Dry isolated yields
     * factor2 and bound50. Existing numerical tests qualify the Random helper. */
    mc_java_random_seed(&supplied, 0);
    expected = supplied;
    CHECK(mc_java_random_next_bounded(&expected, 10, &ignored) && ignored == 0);
    const bool crop_completed = plant_call(world, crop, 5, 70, 15, false, &supplied, &error);
    if (!crop_completed)
        fprintf(stderr, "crop failure: %s, hook %s:%u, cause %s:%u, query %u, plant %u\n",
                error.operation != NULL ? error.operation : "none",
                error.hook_class != NULL ? error.hook_class : "none", error.hook_bytecode,
                error.cause_hook_class != NULL ? error.cause_hook_class : "none",
                error.cause_hook_bytecode, error.query.bytecode, error.plant.bytecode);
    CHECK(crop_completed);
    CHECK(plant_success(&error));
    CHECK(at(world, 5, 70, 15, 59, 1));
    CHECK(mc_test_java_random_equal(&supplied, &expected));
    mc_java_random_seed(&supplied, 0);
    expected = supplied;
    CHECK(mc_java_random_next_bounded(&expected, 50, &ignored));
    CHECK(plant_call(world, crop, 10, 70, 15, false, &supplied, &error) && plant_success(&error) &&
          at(world, 10, 70, 15, 59, ignored == 0 ? 1 : 0) &&
          mc_test_java_random_equal(&supplied, &expected));
    CHECK(plant_call(world, crop, 15, 70, 15, false, NULL, &error) && plant_success(&error));
    CHECK(plant_call(world, crop, 20, 70, 15, false, NULL, &error) && plant_success(&error));
    /* A remote sapling skips even survival and a NULL Random. Other plants do
     * not share this subclass guard. */
    const bool remote = world->fields.B;
    world->fields.B = true;
    const bool skipped = plant_call(world, sapling, 5, 90, 20, false, NULL, &error);
    world->fields.B = remote;
    CHECK(skipped && plant_success(&error) && at(world, 5, 90, 20, 6, 2));
    mc_java_random_seed(&supplied, 0);
    expected = supplied;
    CHECK(mc_java_random_next_bounded(&expected, 30, &ignored) && ignored == 0);
    CHECK(plant_call(world, sapling, 5, 90, 20, false, &supplied, &error) &&
          plant_success(&error) && at(world, 5, 90, 20, 6, 10) &&
          mc_test_java_random_equal(&supplied, &expected));
    /* All species restore only after an ordinary rejected tree, and lose the
     * growth-stage bit. These are actual existing tree bodies, not adapters. */
    for (int32_t x = 10; x <= 25; x += 5) {
        mc_java_random_seed(&supplied, 0);
        CHECK(plant_call(world, sapling, x, 90, 20, false, &supplied, &error) &&
              plant_success(&error) && at(world, x, 90, 20, 6, x / 5 - 2));
    }
    for (int32_t x = 5; x <= 20; x += 5) {
        mc_java_random_seed(&supplied, 0);
        CHECK(plant_call(world, sapling, x, 100, 20, false, &supplied, &error) &&
              plant_success(&error) &&
              at(world, x, 100, 20, 17,
                 (x == 10   ? 1
                  : x == 15 ? 2
                            : 0)));
    }
    /* A direct growth call selects big oak with its first seed0 draw. No
     * population scale call may change its constructor leaf distance4. */
    bool changed;
    CHECK(mc_world_runtime_block_metadata_set_quiet(world, 25, 100, 20, 6, 8, &changed, &error) ==
              MC_WORLD_STORAGE_OK &&
          changed);
    mc_java_random_seed(&supplied, 0);
    CHECK(mc_world_runtime_call_enter(world, NULL) == MC_WORLD_STORAGE_OK);
    error = mc_world_mutation_empty_error();
    const bool big = mc_plant_sapling_grow(world, sapling, 25, 100, 20, &supplied, &error);
    mc_world_runtime_call_leave(world);
    CHECK(big && plant_success(&error) && at(world, 25, 100, 20, 17, 0));
    /* A late dirt-static failure occurs after the quiet sapling removal. It
     * is a native failure, so the ordinary rejected-tree restore must not run. */
    CHECK(mc_world_runtime_block_set_quiet(world, 15, 39, 20, 2, &changed, &error) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_block_metadata_set_quiet(world, 15, 40, 20, 6, 9, &changed, &error) ==
          MC_WORLD_STORAGE_OK);
    size_t dirt = 0;
    while (dirt < COUNT(registry->blocks.named) &&
           strcmp(mc_registry_block_static_name(dirt), "w") != 0)
        ++dirt;
    CHECK(dirt < COUNT(registry->blocks.named));
    mc_block *const saved_dirt = registry->blocks.named[dirt];
    registry->blocks.named[dirt] = NULL;
    mc_java_random_seed(&supplied, 0);
    CHECK(mc_world_runtime_call_enter(world, NULL) == MC_WORLD_STORAGE_OK);
    error = mc_world_mutation_empty_error();
    const bool tree_failure = mc_plant_sapling_grow(world, sapling, 15, 40, 20, &supplied, &error);
    mc_world_runtime_call_leave(world);
    registry->blocks.named[dirt] = saved_dirt;
    CHECK(!tree_failure);
    CHECK(error.plant_tree.reached && error.hook_bytecode == 98);
    CHECK(error.plant_tree.bytecode == 281 && error.registry.status == MC_REGISTRY_NULL_REFERENCE &&
          at(world, 15, 40, 20, 0, 0));
    CHECK(world->active_calls == 0 && world->local_leases == 0);
    ok = true;
cleanup:
    return ok;
}

static bool plant_farmland(fixture *f, size_t slot) {
    bool ok = false;
    mc_world_runtime *world = f->worlds[slot];
    const mc_block *farmland = NULL;
    mc_world_mutation_error error;
    mc_java_random supplied, expected;
    int32_t ignored;
    CHECK(mc_registry_block_get(f->registry[slot % 2u], 60, &farmland, NULL) == MC_REGISTRY_OK);
    CHECK(plant_call(world, farmland, 10, 110, 15, true, NULL, &error) && plant_success(&error) &&
          at(world, 10, 110, 15, 3, 0));
    CHECK(!plant_call(world, farmland, 15, 110, 15, false, NULL, &error) &&
          error.hook_bytecode == 3 && at(world, 15, 110, 15, 60, 1));
    for (int32_t x = 5; x <= 25; x += 5) {
        if (x == 10)
            continue;
        mc_java_random_seed(&supplied, 0);
        expected = supplied;
        CHECK(mc_java_random_next_bounded(&expected, 5, &ignored) && ignored == 0);
        CHECK(plant_call(world, farmland, x, 110, 15, false, &supplied, &error) &&
              plant_success(&error) && mc_test_java_random_equal(&supplied, &expected));
        CHECK(at(world, x, 110, 15, x == 20 ? 3 : 60, x == 5 ? 7 : 0));
    }
    /* Step-on uses World.r and ignores its nullable Entity. Inside contact is
     * a distinct, empty inherited overload and cannot perform this work. */
    bool stepped = false;
    for (int64_t seed = 0; !stepped && seed < 10000; ++seed) {
        mc_java_random_seed(&expected, seed);
        CHECK(mc_java_random_next_bounded(&expected, 4, &ignored));
        if (ignored != 0)
            continue;
        mc_java_random_reseed(world->fields.r, seed);
        CHECK(mc_world_runtime_call_enter(world, NULL) == MC_WORLD_STORAGE_OK);
        error = mc_world_mutation_empty_error();
        stepped = mc_plant_farmland_contact(world, farmland, 5, 120, 15, NULL, &error);
        mc_world_runtime_call_leave(world);
        CHECK(stepped && plant_success(&error) && at(world, 5, 120, 15, 3, 0) &&
              mc_test_java_random_equal(world->fields.r, &expected));
    }
    CHECK(stepped && world->active_calls == 0 && world->local_leases == 0);
    ok = true;
cleanup:
    return ok;
}

static bool plant_full_height_removal(fixture *f, size_t slot) {
    bool ok = false;
    mc_world_runtime *world = f->worlds[slot];
    const mc_block *cactus = NULL;
    mc_world_mutation_error error;
    const size_t before = world->fields.b->count;
    CHECK(mc_registry_block_get(f->registry[slot % 2u], 81, &cactus, NULL) == MC_REGISTRY_OK);
    CHECK(plant_call(world, cactus, 28, 1, 28, true, NULL, &error) && plant_success(&error));
    for (int32_t y = 1; y < 128; ++y)
        CHECK(at(world, 28, y, 28, 0, 0) && at(world, 29, y, 28, 0, 0));
    CHECK(world->fields.b->count == before + 254u && world->active_calls == 0 &&
          world->local_leases == 0);
    ok = true;
cleanup:
    return ok;
}

int main(int argc, char **argv) {
    int result = 1;
    fixture *f = NULL;
    char root[8192];
    const uint16_t title[] = {'P', 'l', 'a', 'n', 't'};
    if (argc != 3 || strcmp(argv[1], "world-plants") != 0 ||
        !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 1;
    f = calloc(1, sizeof(*f));
    CHECK(f != NULL && mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK &&
          mc_materials_create(&f->materials));
    CHECK(mc_java_random_profile_create(4u * 1024u * 1024u, &f->profile) ==
          MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
          MC_WORLD_INFO_OK);
    for (size_t i = 0; i < COUNT(f->stores); ++i)
        CHECK(open_store(f, i, root));
    for (size_t side = 0; side < 2; ++side) {
        resources *r = &f->sources[side];
        r->streams[0] = (stream){.bytes = language, .length = sizeof(language) - 1u};
        r->streams[1] = (stream){.bytes = language};
        r->streams[2] = (stream){.bytes = language};
        r->api = (mc_registry_resources){r, open_resource, close_resource, ready_resource};
        const mc_registry_options options = {side == 0 ? MC_REGISTRY_CLIENT : MC_REGISTRY_SERVER,
                                             f->profile, f->materials, 0, &r->api};
        CHECK(mc_registry_classes_allocate(&options, &f->registry[side], NULL) == MC_REGISTRY_OK &&
              mc_entity_classes_create(side == 0 ? MC_ENTITY_CLIENT : MC_ENTITY_SERVER, f->profile,
                                       NULL, &f->entities[side]) == MC_ENTITY_OK &&
              mc_world_runtime_classes_create(
                  side == 0 ? MC_WORLD_INFO_CLIENT : MC_WORLD_INFO_SERVER, f->profile, &trig,
                  &f->classes[side], NULL) == MC_WORLD_STORAGE_OK);
        CHECK(mc_world_runtime_classes_bind_registry(f->classes[side], f->registry[side], NULL) ==
                  MC_REGISTRY_OK &&
              mc_world_runtime_classes_bind_entities(f->classes[side], f->entities[side]) ==
                  MC_ENTITY_OK &&
              mc_registry_classes_initialize_blocks(f->registry[side], NULL) == MC_REGISTRY_OK);
    }
    for (size_t slot = 0; slot < COUNT(f->worlds); ++slot) {
        CHECK(create_world_with_scheduled_capacity(f, slot, 512));
        for (int32_t x = 0; x < 2; ++x)
            for (int32_t z = 0; z < 2; ++z)
                CHECK(seed_record_from(f, slot, x, z, plant_input, plant_lights));
        CHECK(load_seeded_square(f, slot));
    }
    for (size_t slot = 0; slot < COUNT(f->worlds); ++slot)
        CHECK(plant_neighbors(f, slot) && plant_upright(f, slot) && plant_growth(f, slot) &&
              plant_farmland(f, slot) && plant_full_height_removal(f, slot));
    result = 0;
cleanup:
    if (!cleanup_fixture(f)) {
        fputs("plant cleanup refused; live dependencies retained\n", stderr);
        return 1;
    }
    if (result == 0 && printf("PASS: %u World plant checks (world-plants)\n", checks) < 0)
        result = 1;
    return result;
}
