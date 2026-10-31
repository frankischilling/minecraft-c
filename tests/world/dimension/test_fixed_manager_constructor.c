/* Reuse genuine Registry/resource construction and its complete retirement. */
#define main fixed_manager_existing_runtime_main
#include "test_fixed_biome_runtime.c"
#undef main

#include "../../../src/core/java/random_profile_internal.h"

#include <emmintrin.h>

static uint64_t double_bits(double value) {
    uint64_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static double double_from_bits(uint64_t value) {
    double result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static bool unchanged_provider(const mc_world_provider_fields *before,
                               const mc_world_provider_fields *after) {
    return before->a == after->a && before->b == after->b && before->c == after->c &&
           before->d == after->d && before->e == after->e && before->f == after->f &&
           before->g == after->g && before->h == after->h;
}

static bool empty_manager(const mc_world_chunk_manager_fields *fields,
                          const mc_world_chunk_manager_game_fields *game) {
    return fields->fixed && fields->noise_backed == NULL && fields->a != NULL &&
           fields->a->data == NULL && fields->a->length == 0 && fields->b != NULL &&
           fields->b->data == NULL && fields->b->length == 0 && fields->c != NULL &&
           fields->c->data == NULL && fields->c->length == 0 && fields->d != NULL &&
           fields->d->data == NULL && fields->d->length == 0 && game->d == NULL;
}

static bool fixed_constructor_side(mc_registry_side side) {
    bool ok = false;
    fixture *f = calloc(1, sizeof(*f));
    mc_world_chunk_manager *first = NULL, *plains = NULL, *tundra = NULL, *foreign = NULL;
    mc_world_provider_fields before = {0}, after = {0};
    mc_world_provider_state state_before = {0}, state_after = {0};
    mc_world_chunk_manager_fields fields = {0};
    mc_world_chunk_manager_game_fields game = {0};
    mc_biome_registry_view biomes = {0};
    mc_world_provider_error error = {0};
    size_t provider_prefix_bytes = 0;
    CHECK(f != NULL && mc_materials_create(&f->materials));
    CHECK(mc_java_random_profile_create(PROFILE_BUDGET, &f->profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(registry_create(f, 0, side, false) && registry_create(f, 1, side, false) &&
          registry_create(f, 2, side, false));
    const mc_world_provider_registry_binding binding = {f->registries[0], side, f->profile};
    const mc_world_provider_registry_binding other = {f->registries[1], side, f->profile};
    CHECK(mc_world_provider_create(0, NULL, &f->dimensions[0], NULL) == MC_WORLD_PROVIDER_OK &&
          mc_world_provider_create(0, NULL, &f->dimensions[1], NULL) == MC_WORLD_PROVIDER_OK);
    CHECK(mc_world_provider_fields_get(f->dimensions[0], &before) == MC_WORLD_PROVIDER_OK &&
          mc_world_provider_state_get(f->dimensions[0], &state_before) == MC_WORLD_PROVIDER_OK);
    provider_prefix_bytes = state_before.direct_allocation_bytes;

    /* No arithmetic is in HD/ET construction: preserve negative zero and a
     * NaN payload even under a nondefault rounding mode. */
    const uint64_t temperature = UINT64_C(0x8000000000000000);
    const uint64_t rainfall = UINT64_C(0x7ff8000000001234);
    const uint64_t entropy = atomic_load_explicit(&f->profile->uniquifier, memory_order_relaxed);
    const unsigned int saved_csr = _mm_getcsr();
    _mm_setcsr(saved_csr ^ 0x2000u);
    const mc_world_provider_status constructed = mc_world_chunk_manager_fixed_create_game(
        f->dimensions[0], &binding, NULL, double_from_bits(temperature), double_from_bits(rainfall),
        &first, &error);
    _mm_setcsr(saved_csr);
    CHECK(constructed == MC_WORLD_PROVIDER_OK && error.bytecode == 20 && first != NULL);
    CHECK(mc_world_provider_fields_get(f->dimensions[0], &after) == MC_WORLD_PROVIDER_OK &&
          unchanged_provider(&before, &after));
    CHECK(mc_world_provider_state_get(f->dimensions[0], &state_after) == MC_WORLD_PROVIDER_OK &&
          state_after.direct_allocation_bytes > state_before.direct_allocation_bytes &&
          state_after.constructor_pc == state_before.constructor_pc &&
          state_after.bind_pc == state_before.bind_pc &&
          state_after.setup_pc == state_before.setup_pc && state_after.holds == state_before.holds);
    CHECK(mc_world_chunk_manager_fields_get(first, &fields) == MC_WORLD_PROVIDER_OK &&
          mc_world_chunk_manager_game_fields_get(first, &game) == MC_WORLD_PROVIDER_OK &&
          empty_manager(&fields, &game) && fields.holds == 1 && game.registry == binding.classes &&
          game.fixed_e == NULL && fields.fixed_e == MC_BIOME_NONE &&
          double_bits(fields.fixed_f) == temperature && double_bits(fields.fixed_g) == rainfall);
    CHECK(mc_biome_registry_inspect(binding.classes, &biomes, NULL) == MC_REGISTRY_OK &&
          biomes.phase == MC_REGISTRY_COLD && no_biomes(&biomes) &&
          resources_unread(&f->sources[0]) &&
          atomic_load_explicit(&f->profile->uniquifier, memory_order_relaxed) == entropy);
    CHECK(mc_registry_classes_free(binding.classes, NULL) == MC_REGISTRY_BUSY);
    CHECK(mc_world_provider_assign_chunk_manager_game(f->dimensions[0], first, &error) ==
          MC_WORLD_PROVIDER_OK);
    CHECK(mc_world_chunk_manager_fields_get(first, &fields) == MC_WORLD_PROVIDER_OK &&
          fields.holds == 2);
    CHECK(mc_world_provider_assign_chunk_manager_game(f->dimensions[0], first, &error) ==
          MC_WORLD_PROVIDER_OK);
    CHECK(mc_world_chunk_manager_fields_get(first, &fields) == MC_WORLD_PROVIDER_OK &&
          fields.holds == 2);
    mc_world_provider_release(f->dimensions[0]);
    f->dimensions[0] = NULL;
    const mc_biome *point = NULL;
    CHECK(mc_world_chunk_manager_point(first, INT32_MIN, INT32_MAX, &point, NULL) ==
              MC_WORLD_PROVIDER_OK &&
          point == NULL &&
          mc_world_chunk_manager_fields_get(first, &fields) == MC_WORLD_PROVIDER_OK &&
          fields.holds == 1);
    double raw = 1.0;
    CHECK(mc_world_chunk_manager_temperature_raw(first, 0, 0, &raw, NULL) == MC_WORLD_PROVIDER_OK &&
          double_bits(raw) == temperature);

    /* Active static arguments are evaluated by the caller. Use the existing
     * real fixed-provider setup to run that bootstrap and observe its source
     * publication order through the resource fixture. */
    CHECK(mc_world_provider_create(-1, NULL, &f->dimensions[0], NULL) == MC_WORLD_PROVIDER_OK);
    f->sources[0].observed_dimension = f->dimensions[0];
    CHECK(mc_world_provider_bind_game(f->dimensions[0], NULL, &binding, &error) ==
              MC_WORLD_PROVIDER_OK &&
          resources_complete(&f->sources[0]));
    const mc_biome *plain_biome = NULL, *tundra_biome = NULL;
    CHECK(mc_biome_static_get(binding.classes, 'i', &plain_biome, NULL) == MC_REGISTRY_OK &&
          mc_biome_static_get(binding.classes, 'k', &tundra_biome, NULL) == MC_REGISTRY_OK &&
          plain_biome != NULL && tundra_biome != NULL && plain_biome != tundra_biome &&
          !mc_biome_fields_get(plain_biome)->v && mc_biome_fields_get(tundra_biome)->v);
    CHECK(mc_world_chunk_manager_fixed_create_game(f->dimensions[1], &binding, plain_biome, .5, .5,
                                                   &plains, &error) == MC_WORLD_PROVIDER_OK &&
          mc_world_chunk_manager_fixed_create_game(f->dimensions[1], &binding, tundra_biome, .5, .5,
                                                   &tundra, &error) == MC_WORLD_PROVIDER_OK);
    CHECK(mc_world_chunk_manager_fields_get(plains, &fields) == MC_WORLD_PROVIDER_OK &&
          mc_world_chunk_manager_game_fields_get(plains, &game) == MC_WORLD_PROVIDER_OK &&
          empty_manager(&fields, &game) && game.fixed_e == plain_biome &&
          fields.fixed_e == MC_BIOME_PLAINS && fields.fixed_f == .5 && fields.fixed_g == .5);
    CHECK(mc_world_chunk_manager_fields_get(tundra, &fields) == MC_WORLD_PROVIDER_OK &&
          mc_world_chunk_manager_game_fields_get(tundra, &game) == MC_WORLD_PROVIDER_OK &&
          empty_manager(&fields, &game) && game.fixed_e == tundra_biome &&
          fields.fixed_e == MC_BIOME_TUNDRA && fields.fixed_f == .5 && fields.fixed_g == .5);
    CHECK(mc_world_provider_fields_get(f->dimensions[0], &before) == MC_WORLD_PROVIDER_OK &&
          mc_world_provider_state_get(f->dimensions[0], &state_before) == MC_WORLD_PROVIDER_OK);
    CHECK(mc_world_provider_assign_chunk_manager_game(f->dimensions[0], plains, &error) ==
          MC_WORLD_PROVIDER_OK);
    CHECK(mc_world_provider_fields_get(f->dimensions[0], &after) == MC_WORLD_PROVIDER_OK &&
          after.b == plains && after.a == before.a && after.c == before.c && after.d == before.d &&
          after.e == before.e && after.f == before.f && after.g == before.g && after.h == before.h);
    CHECK(mc_world_provider_state_get(f->dimensions[0], &state_after) == MC_WORLD_PROVIDER_OK &&
          state_after.constructor_pc == state_before.constructor_pc &&
          state_after.bind_pc == state_before.bind_pc &&
          state_after.setup_pc == state_before.setup_pc);
    CHECK(mc_world_provider_assign_chunk_manager_game(f->dimensions[0], tundra, &error) ==
              MC_WORLD_PROVIDER_OK &&
          mc_world_chunk_manager_fields_get(plains, &fields) == MC_WORLD_PROVIDER_OK &&
          fields.holds == 1);
    CHECK(mc_world_chunk_manager_fixed_create_game(f->dimensions[1], &other, NULL, 1., 2., &foreign,
                                                   &error) == MC_WORLD_PROVIDER_OK);
    CHECK(mc_world_provider_assign_chunk_manager_game(f->dimensions[0], foreign, &error) ==
              MC_WORLD_PROVIDER_INVALID_ARGUMENT &&
          mc_world_provider_fields_get(f->dimensions[0], &after) == MC_WORLD_PROVIDER_OK &&
          after.b == tundra &&
          mc_world_chunk_manager_fields_get(foreign, &fields) == MC_WORLD_PROVIDER_OK &&
          fields.holds == 1);
    mc_world_chunk_manager *unchanged = plains;
    CHECK(mc_world_chunk_manager_fixed_create_game(f->dimensions[1], &other, plain_biome, .5, .5,
                                                   &unchanged,
                                                   &error) == MC_WORLD_PROVIDER_INVALID_ARGUMENT &&
          unchanged == plains);
    const mc_world_provider_registry_binding wrong_profile = {binding.classes, side, NULL};
    CHECK(mc_world_chunk_manager_fixed_create_game(f->dimensions[1], &wrong_profile, NULL, .5, .5,
                                                   &unchanged,
                                                   &error) == MC_WORLD_PROVIDER_INVALID_ARGUMENT &&
          unchanged == plains);

    /* Measure the real constructed provider prefix; a factory allocation
     * cannot fit into precisely that budget. Its temporary Registry borrow
     * must not keep the cold class owner live after native failure. */
    mc_world_provider_limits limits = mc_world_provider_default_limits();
    limits.max_direct_allocation_bytes = provider_prefix_bytes;
    CHECK(mc_world_provider_create(0, &limits, &f->dimensions[2], NULL) == MC_WORLD_PROVIDER_OK);
    const mc_world_provider_registry_binding limited = {f->registries[2], side, f->profile};
    CHECK(mc_world_chunk_manager_fixed_create_game(f->dimensions[2], &limited, NULL, .5, .5,
                                                   &unchanged, &error) == MC_WORLD_PROVIDER_LIMIT &&
          unchanged == plains && error.bytecode == 0 && resources_unread(&f->sources[2]));
    CHECK(mc_registry_classes_free(f->registries[2], NULL) == MC_REGISTRY_OK);
    f->registries[2] = NULL;
    CHECK(mc_world_provider_allocate(MC_WORLD_PROVIDER_OVERWORLD, NULL, &f->dimensions[3], NULL) ==
              MC_WORLD_PROVIDER_OK &&
          mc_world_provider_assign_chunk_manager_game(f->dimensions[3], plains, NULL) ==
              MC_WORLD_PROVIDER_INVALID_PHASE);
    CHECK(mc_world_provider_assign_chunk_manager_game(f->dimensions[0], NULL, NULL) ==
          MC_WORLD_PROVIDER_OK);
    CHECK(mc_world_provider_fields_get(f->dimensions[0], &after) == MC_WORLD_PROVIDER_OK &&
          after.b == NULL);
    mc_world_provider_release(f->dimensions[1]);
    f->dimensions[1] = NULL;
    point = NULL;
    CHECK(mc_world_chunk_manager_chunk(tundra, INT32_MIN, INT32_MAX, &point, NULL) ==
              MC_WORLD_PROVIDER_OK &&
          point == tundra_biome &&
          mc_registry_classes_free(f->registries[0], NULL) == MC_REGISTRY_BUSY);
    CHECK(mc_biome_registry_inspect(other.classes, &biomes, NULL) == MC_REGISTRY_OK &&
          biomes.phase == MC_REGISTRY_COLD && no_biomes(&biomes) &&
          resources_unread(&f->sources[1]));
    ok = true;
cleanup:
    mc_world_chunk_manager_release(first);
    mc_world_chunk_manager_release(plains);
    mc_world_chunk_manager_release(tundra);
    mc_world_chunk_manager_release(foreign);
    if (!cleanup_fixture(f))
        ok = false;
    return ok;
}

int main(void) {
    for (int side = MC_REGISTRY_CLIENT; side <= MC_REGISTRY_SERVER; ++side)
        if (!fixed_constructor_side((mc_registry_side)side))
            return 1;
    printf("fixed manager constructor: %u checks\n", checks);
    return 0;
}
