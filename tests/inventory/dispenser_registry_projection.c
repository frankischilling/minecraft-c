/* Actual AZ eligibility's lazy World getter reaches first/cached Registry
 * failure before its null Player distance call. Container projection below is
 * an explicit returned-VALUE control, not a scripted Source callee or GUI run. */
int mc_dispenser_registry_projection_unused_boxes_main(int, char **);
#define MC_COLLISION_BOXES_MAIN mc_dispenser_registry_projection_unused_boxes_main
#include "../world/runtime/collision/test_boxes.c"
#undef MC_COLLISION_BOXES_MAIN

#include "inventory/container/interaction_internal.h"
#include "mc/inventory/dispenser.h"
#include "mc/world/runtime/tile_entities/dispenser.h"

static mc_registry_status projection_missing_stream(void *context, const char *name,
                                                    mc_java_properties_input *out) {
    resources *r = context;
    if (name == NULL || out == NULL || strcmp(name, "/lang/en_US.lang") != 0) {
        r->bad = true;
        return MC_REGISTRY_INVALID_ARGUMENT;
    }
    ++r->opens;
    return MC_REGISTRY_NULL_REFERENCE; /* Actual absent class-loader resource. */
}

/* Same genuine COLD World setup as the Registry failure controls. The two
 * fixtures supply missing-resource and absent-native-capability inputs at
 * Registry allocation; no initializer phase/result or class field is patched. */
static bool projection_world(fixture *f, const char *root, size_t slot, bool source) {
    bool ok = false;
    const uint16_t title[] = {'A', 'Z', 'R', 'e', 'g', 'i', 's', 't', 'r', 'y'};
    CHECK(mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK &&
          mc_materials_create(&f->materials) &&
          mc_java_random_profile_create(8u * 1024u * 1024u, &f->profile) ==
              MC_JAVA_RANDOM_PROFILE_OK &&
          mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
              MC_WORLD_INFO_OK);
    resources *r = &f->sources[0];
    r->api = (mc_registry_resources){r, projection_missing_stream, NULL, NULL};
    const mc_registry_options options = {MC_REGISTRY_CLIENT, f->profile, f->materials, 0,
                                         source ? &r->api : NULL};
    CHECK(mc_registry_classes_allocate(&options, &f->registry[0], NULL) == MC_REGISTRY_OK &&
          mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile, &trig, &f->classes[0],
                                          NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_classes_bind_registry(f->classes[0], f->registry[0], NULL) ==
              MC_REGISTRY_OK &&
          mc_entity_classes_create(MC_ENTITY_CLIENT, f->profile, NULL, &f->entity_classes[0]) ==
              MC_ENTITY_OK &&
          mc_world_runtime_classes_bind_entities(f->classes[0], f->entity_classes[0]) ==
              MC_ENTITY_OK &&
          boxes_open_store(f, slot, root) && boxes_create_world(f, slot) &&
          f->registry[0]->blocks_phase == MC_REGISTRY_COLD);
    ok = true;
cleanup:
    return ok;
}

static bool projection_controls(fixture *f, size_t slot, bool source) {
    bool ok = false;
    mc_tile_entity *tile = NULL;
    mc_inventory_dispenser *adapter = NULL;
    mc_inventory_error inventory[2] = {{0}};
    mc_container_interaction_error container[2];
    const char *names[2] = {NULL, NULL};
    mc_inventory_dispenser_state state = {0};
    boxes_air(f, slot);
    CHECK(boxes_cell(f, slot, 8, 64, 8, 23, 0) &&
          mc_tile_entity_allocate(f->worlds[slot], MC_TILE_ENTITY_DISPENSER, &tile) ==
              MC_TILE_ENTITY_OK &&
          mc_tile_entity_construct(tile) == MC_TILE_ENTITY_OK &&
          mc_tile_entity_bind_world(tile, f->worlds[slot]) == MC_TILE_ENTITY_OK &&
          mc_tile_entity_set_coordinates(tile, 8, 64, 8) == MC_TILE_ENTITY_OK &&
          mc_inventory_dispenser_from_tile(MC_REGISTRY_CLIENT, tile, NULL, &adapter, NULL) ==
              MC_INVENTORY_OK &&
          mc_inventory_dispenser_read_state(adapter, &state) && state.diagnostic_owner_id != 0 &&
          f->registry[0]->blocks_phase == MC_REGISTRY_COLD);
    for (size_t attempt = 0; attempt < 2; ++attempt) {
        bool usable = true; /* Failure must preserve this caller-owned output. */
        CHECK(mc_inventory_dispenser_usable(adapter, NULL, &usable, &inventory[attempt]) !=
                  MC_INVENTORY_OK &&
              usable && inventory[attempt].dispenser != NULL &&
              inventory[attempt].dispenser_diagnostic_owner_id == state.diagnostic_owner_id &&
              !inventory[attempt].exception_class_from_dispenser &&
              inventory[attempt].source_error == source && !inventory[attempt].source_exception &&
              inventory[attempt].registry_status == inventory[attempt].registry.status &&
              inventory[attempt].registry.status ==
                  (attempt == 0 ? (source ? MC_REGISTRY_NULL_REFERENCE
                                          : MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY)
                                : MC_REGISTRY_FAILED_INITIALIZATION) &&
              mc_world_registry_failure_matches(&inventory[attempt].dispenser->registry_failure,
                                                &inventory[attempt].registry) &&
              inventory[attempt].dispenser->registry_failure.kind ==
                  (source ? MC_WORLD_REGISTRY_FAILURE_SOURCE_ERROR
                          : MC_WORLD_REGISTRY_FAILURE_NATIVE));
        names[attempt] = inventory[attempt].exception_class;
        if (source)
            CHECK(names[attempt] != NULL &&
                  strcmp(names[attempt], attempt == 0 ? "java/lang/ExceptionInInitializerError"
                                                      : "java/lang/NoClassDefFoundError") == 0);
        else
            CHECK(names[attempt] == NULL);
        /* Project the returned diagnostic VALUE through the actual shared
         * Container helper. This invokes no replacement Inventory/Source host. */
        mc_container_interaction_error_init(&container[attempt]);
        container[attempt].inventory = inventory[attempt];
        CHECK(!inv_interaction_inventory(&container[attempt], inventory[attempt].status) &&
              container[attempt].source_error == source && !container[attempt].source_exception &&
              container[attempt].native_policy == !source &&
              container[attempt].exception_class == names[attempt] &&
              container[attempt].exception_class_dispenser_owner_id == 0 &&
              container[attempt].inventory.dispenser == inventory[attempt].dispenser &&
              container[attempt].dependency.pc == inventory[attempt].frames[0].pc);
    }
    CHECK(f->registry[0]->blocks_phase == MC_REGISTRY_FAILED &&
          f->sources[0].opens == (source ? 1u : 0u) && !f->sources[0].bad &&
          mc_inventory_dispenser_end_calls(adapter, NULL) == MC_INVENTORY_OK);
    for (size_t attempt = 0; attempt < 2; ++attempt) {
        mc_inventory_dispenser_error_expire(&inventory[attempt], state.diagnostic_owner_id);
        mc_container_interaction_error_expire_dispenser(&container[attempt],
                                                        state.diagnostic_owner_id);
        CHECK(inventory[attempt].dispenser == NULL &&
              inventory[attempt].dispenser_diagnostic_owner_id == 0 &&
              !inventory[attempt].exception_class_from_dispenser &&
              inventory[attempt].source_error == source && !inventory[attempt].source_exception &&
              inventory[attempt].exception_class == names[attempt] &&
              container[attempt].inventory.dispenser == NULL &&
              container[attempt].source_error == source && !container[attempt].source_exception &&
              container[attempt].exception_class == names[attempt] &&
              container[attempt].exception_class_dispenser_owner_id == 0);
    }
    ok = true;
cleanup:
    if (mc_inventory_dispenser_retire(&adapter, NULL) != MC_INVENTORY_OK)
        return false;
    mc_tile_entity_release(tile);
    return ok;
}

int main(int argc, char **argv) {
    int result = 1;
    fixture *source = NULL, *native = NULL;
    char root[8192];
    if (argc != 3 || strcmp(argv[1], "inventory-dispenser-registry-projection") != 0 ||
        !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 1;
    source = calloc(1, sizeof(*source));
    native = calloc(1, sizeof(*native));
    CHECK(source != NULL && native != NULL && projection_world(source, root, 0, true) &&
          projection_world(native, root, 2, false) && projection_controls(source, 0, true) &&
          projection_controls(native, 2, false));
    result = 0;
cleanup:
    if (!cleanup_fixture(source) || !cleanup_fixture(native)) {
        fputs("Dispenser Registry projection cleanup refused\n", stderr);
        return 1;
    }
    if (result == 0)
        printf("Dispenser Registry projection CPU controls passed: %u\n", checks);
    return result;
}
