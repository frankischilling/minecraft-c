/* Focused private-getter regression. Original rows are direct client leaves;
 * paired native Registry and nullable Material controls are authored inputs. */
#define main mc_piston_mobility_unused_publication_main
#include "../test_entity_publication.c"
#undef main

/* Keep the production getter private. The actual translation unit is compiled
 * under distinct public names only for this test, with no substitute getter. */
#define mc_world_mutation_piston_world_event mc_test_mobility_world_event
#define mc_world_mutation_piston_base_added mc_test_mobility_base_added
#define mc_world_mutation_piston_base_neighbor mc_test_mobility_base_neighbor
#define mc_world_mutation_piston_head_removed mc_test_mobility_head_removed
#define mc_world_mutation_piston_head_neighbor mc_test_mobility_head_neighbor
#include "../../../../src/world/runtime/mutation/piston.c"
#undef mc_world_mutation_piston_world_event
#undef mc_world_mutation_piston_base_added
#undef mc_world_mutation_piston_base_neighbor
#undef mc_world_mutation_piston_head_removed
#undef mc_world_mutation_piston_head_neighbor

#include "../../../reference/block/piston_mobility/native-observations.h"

static bool mobility_overrides(int32_t id) {
    return id == 26 || id == 27 || id == 28 || id == 64 || id == 66 || id == 70 || id == 71 ||
           id == 72 || id == 79;
}

static bool mobility_side(mc_registry_classes *registry, mc_world_mutation_error *error) {
    bool ok = false;
    piston_scratch *scratch = NULL;
    const piston_frame frame = {
        .registry = registry, .error = error, .scratch = &scratch, .method = P_CAN_PUSH};
    mc_registry_block_m_view array;
    CHECK(mc_registry_block_m_get(registry, &array, NULL) == MC_REGISTRY_OK);
    size_t occupied = 0;
    for (int32_t i = 0; i < 256; ++i) {
        const mc_block *block = NULL;
        CHECK(mc_registry_block_m_at(&array, i, &block, NULL) == MC_REGISTRY_OK);
        if (block != NULL)
            ++occupied;
    }
    CHECK(occupied == MC_PISTON_MOBILITY_REFERENCE_COUNT);
    for (size_t i = 0; i < MC_PISTON_MOBILITY_REFERENCE_COUNT; ++i) {
        const mc_piston_mobility_reference_row *row = &mc_piston_mobility_reference[i];
        const mc_block *block = NULL;
        CHECK(mc_registry_block_m_at(&array, row->block_id, &block, NULL) == MC_REGISTRY_OK &&
              block != NULL && block->fields.bn == row->block_id);
        const mc_block before = *block;
        for (size_t repeat = 0; repeat < 2; ++repeat) {
            int32_t actual = INT32_C(-19073);
            mc_world_mutation_error_init(error);
            CHECK(piston_mobility(&frame, block, 69, &actual) && actual == row->mobility &&
                  !error->piston_source_null && scratch == NULL &&
                  memcmp(block, &before, sizeof(before)) == 0);
        }
        /* Authored pre-call local receiver only: never edit Registry singleton
         * fields or claim original execution of a NULL Material receiver. */
        mc_block nullable = *block;
        nullable.fields.bA = NULL;
        int32_t actual = INT32_C(-19073);
        mc_world_mutation_error_init(error);
        if (mobility_overrides(row->block_id)) {
            CHECK(piston_mobility(&frame, &nullable, 69, &actual) && actual == row->mobility &&
                  !error->piston_source_null && error->cause_hook_class == NULL &&
                  nullable.fields.bA == NULL && scratch == NULL);
        } else {
            CHECK(
                !piston_mobility(&frame, &nullable, 69, &actual) && actual == INT32_C(-19073) &&
                error->piston_source_null && error->status == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
                strcmp(error->cause_hook_class, "uu") == 0 &&
                strcmp(error->cause_hook_method, "h()I") == 0 && error->cause_hook_bytecode == 4 &&
                error->piston.can_push == 69 && error->hook_bytecode == 69 && scratch == NULL);
        }
        CHECK(memcmp(block, &before, sizeof(before)) == 0);
    }
    int32_t actual = INT32_C(-19073);
    mc_world_mutation_error_init(error);
    CHECK(!piston_mobility(&frame, NULL, 69, &actual) && actual == INT32_C(-19073) &&
          error->piston_source_null && error->cause_hook_class == NULL &&
          error->piston.can_push == 69 && scratch == NULL);
    ok = true;
cleanup:
    free(scratch);
    return ok;
}

int main(void) {
    int result = 1;
    fixture *f = calloc(1, sizeof(*f));
    mc_world_mutation_error *error = calloc(1, sizeof(*error));
    CHECK(f != NULL && error != NULL && mc_materials_create(&f->materials));
    CHECK(mc_java_random_profile_create(4u * 1024u * 1024u, &f->profile) ==
          MC_JAVA_RANDOM_PROFILE_OK);
    for (size_t side = 0; side < 2; ++side) {
        resources *r = &f->sources[side];
        r->streams[0] = (stream){.bytes = language, .length = sizeof(language) - 1u};
        r->streams[1] = (stream){.bytes = language};
        r->streams[2] = (stream){.bytes = language};
        r->api = (mc_registry_resources){r, open_resource, close_resource, ready_resource};
        const mc_registry_options options = {side == 0 ? MC_REGISTRY_CLIENT : MC_REGISTRY_SERVER,
                                             f->profile, f->materials, 0, &r->api};
        CHECK(mc_registry_classes_allocate(&options, &f->registry[side], NULL) == MC_REGISTRY_OK);
        CHECK(mc_entity_classes_create(side == 0 ? MC_ENTITY_CLIENT : MC_ENTITY_SERVER, f->profile,
                                       NULL, &f->entity_classes[side]) == MC_ENTITY_OK);
        CHECK(mc_registry_classes_bind_entities(f->registry[side], f->entity_classes[side], NULL) ==
              MC_REGISTRY_OK);
        CHECK(mc_registry_classes_initialize_blocks(f->registry[side], NULL) == MC_REGISTRY_OK &&
              r->opens == 3 && r->closes == 1 && !r->bad);
        CHECK(mobility_side(f->registry[side], error));
    }
    result = 0;
cleanup:
    free(error);
    if (!cleanup_fixture(f))
        return 1;
    if (result == 0)
        printf("PASS: Piston virtual mobility; original-rows=96; native-sides=2; checks=%u; "
               "cleanup=1\n",
               checks);
    return result;
}
