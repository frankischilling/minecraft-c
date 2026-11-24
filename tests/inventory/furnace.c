/* Actual SK/IX/BM owners. The BM rows below come from unchanged original
 * constructors/direct methods; native adapter controls are identified separately. */
#include "mc/world/runtime/tile_entities/furnace.h"
#include "../../src/inventory/container/interaction_internal.h"
#include "../../src/inventory/internal.h"
#include "../../src/inventory/player/source_internal.h"
#include "../../src/item/stack/source_internal.h"
#include "../reference/inventory/furnace/native-observations.h"
#include "mc/resources/files/files.h"
#include "mc/world/runtime/construction.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(v)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(v)) {                                                                                \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #v);                                \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

typedef struct aliases {
    const char *names[128];
    const mc_item_stack *values[128];
    size_t count;
} aliases;

static mc_item_stack *lookup(const aliases *a, const char *name) {
    if (name != NULL)
        for (size_t i = 0; i < a->count; ++i)
            if (strcmp(name, a->names[i]) == 0)
                return (mc_item_stack *)a->values[i];
    return NULL;
}

static bool observed(aliases *a, const mc_furnace_stack_reference *s, const mc_item_stack *actual) {
    if (s->identity == NULL)
        return actual == NULL;
    mc_item_stack_fields f;
    if (actual == NULL || mc_item_stack_fields_get(actual, &f) != MC_ITEM_STACK_OK ||
        f.id != s->id || f.count != s->count || f.damage != s->damage ||
        f.animation != s->animation)
        return false;
    const mc_item_stack *known = lookup(a, s->identity);
    if (known != NULL)
        return known == actual;
    for (size_t i = 0; i < a->count; ++i)
        if (a->values[i] == actual)
            return false;
    if (a->count == sizeof a->names / sizeof *a->names)
        return false;
    a->names[a->count] = s->identity;
    a->values[a->count++] = actual;
    return true;
}

static bool exception_is(const char *java, const char *native) {
    if (java == NULL || native == NULL)
        return java == native;
    while (*java != 0 && *native != 0) {
        if ((*java == '.' ? '/' : *java) != *native)
            return false;
        ++java;
        ++native;
    }
    return *java == *native;
}

static bool reference_bm(mc_registry_classes *registry, mc_world_runtime_classes *classes,
                         const mc_furnace_reference *r) {
    bool ok = false;
    mc_tile_entity *tile = NULL;
    mc_inventory_furnace *furnace = NULL;
    mc_inventory_player *inventory = NULL;
    mc_container *bm = NULL;
    mc_container_interaction_call *call = NULL;
    mc_item_stack *inputs[8] = {0};
    aliases identities = {0};
    mc_inventory_error e = {0};
    mc_container_interaction_error ie = {0};
    mc_inventory_status status;
    CHECK(!r->array_null);
    if (!r->tile_null) {
        CHECK(mc_tile_entity_allocate_in_classes(classes, MC_TILE_ENTITY_FURNACE, &tile) ==
                  MC_TILE_ENTITY_OK &&
              mc_tile_entity_construct(tile) == MC_TILE_ENTITY_OK &&
              mc_tile_entity_set_coordinates(tile, r->xyz[0], r->xyz[1], r->xyz[2]) ==
                  MC_TILE_ENTITY_OK &&
              mc_tile_entity_furnace_field_set(tile, MC_FURNACE_BURN_REMAINING, r->fields[0]) ==
                  MC_TILE_ENTITY_OK &&
              mc_tile_entity_furnace_field_set(tile, MC_FURNACE_FUEL_DURATION, r->fields[1]) ==
                  MC_TILE_ENTITY_OK &&
              mc_tile_entity_furnace_field_set(tile, MC_FURNACE_COOK_PROGRESS, r->fields[2]) ==
                  MC_TILE_ENTITY_OK &&
              mc_inventory_furnace_from_tile(MC_REGISTRY_CLIENT, tile, NULL, &furnace, &e) ==
                  MC_INVENTORY_OK);
    }
    if (!r->player_null)
        CHECK(mc_inventory_player_create(MC_REGISTRY_CLIENT, NULL, NULL, &inventory, &e) ==
              MC_INVENTORY_OK);
    for (size_t i = 0; i < r->input_stack_count; ++i) {
        const mc_furnace_stack_reference *s = &r->input_stacks[i];
        CHECK(mc_item_stack_allocate(&inputs[i]) == MC_ITEM_STACK_OK &&
              mc_item_stack_construct(inputs[i], s->id, s->count, s->damage) == MC_ITEM_STACK_OK &&
              mc_item_stack_source_animation_set(inputs[i], s->animation) == MC_ITEM_STACK_OK &&
              observed(&identities, s, inputs[i]));
    }
    if (inventory != NULL) {
        for (int32_t i = 0; i < 36; ++i)
            CHECK(mc_inventory_player_source_set(inventory, i,
                                                 lookup(&identities, r->before_main[i].identity),
                                                 &e) == MC_INVENTORY_OK);
        inventory->changed = false; /* Declared genuine IX initial source field. */
    }
    if (tile != NULL)
        for (int32_t i = 0; i < 3; ++i)
            CHECK(mc_inventory_furnace_set(furnace, i,
                                           lookup(&identities, r->before_tile[i].identity),
                                           &e) == MC_INVENTORY_OK);
    CHECK(mc_container_furnace_allocate(MC_REGISTRY_CLIENT, registry, NULL, &bm, &e) ==
          MC_INVENTORY_OK);
    status = mc_container_furnace_construct(bm, inventory, furnace, &e);
    if (status == MC_INVENTORY_OK) {
        if (strcmp(r->operation, "bm-shift") == 0) {
            status = mc_container_furnace_transfer_begin(bm, r->index, NULL, &call, &ie);
            CHECK(call != NULL);
        } else if (strcmp(r->operation, "bm-progress") == 0)
            status = mc_container_furnace_progress(bm, r->index, r->amount, &e);
        else if (strcmp(r->operation, "bm-update") == 0)
            status = mc_container_furnace_detect_changes(bm, &e);
        else if (strcmp(r->operation, "bm-usable") == 0) {
            bool usable = false;
            status = mc_container_furnace_usable(bm, NULL, &usable, &e);
        } else
            CHECK(strcmp(r->operation, "bm-layout") == 0);
    }
    CHECK((status == MC_INVENTORY_OK) == r->returned);
    if (!r->returned) {
        CHECK(exception_is(r->exception, call == NULL ? e.exception_class : ie.exception_class));
        if (r->player_null)
            CHECK(bm->constructor_failed && bm->constructor_pc == 64 && bm->slots->size == 2);
        if (call != NULL && r->index == 2)
            CHECK(ie.source_exception && strcmp(ie.child.class_name, "vq") == 0 &&
                  ie.child.pc == 5);
    }
    if (call != NULL) {
        mc_container_interaction_state state;
        CHECK(mc_container_interaction_inspect(call, &state) && state.returned == r->returned);
        if (r->returned) {
            if (r->result_kind == 0)
                CHECK(state.result == NULL);
            else {
                CHECK(r->result_kind == 3 && r->result_text != NULL && state.result != NULL);
                const mc_furnace_stack_reference *expected = NULL;
                for (size_t i = 0; i < r->tracked_stack_count; ++i)
                    if (strcmp(r->tracked_stacks[i].identity, r->result_text) == 0)
                        expected = &r->tracked_stacks[i];
                CHECK(expected != NULL && observed(&identities, expected, state.result));
            }
        }
    }
    if (inventory != NULL) {
        CHECK(inventory->changed == r->player_changed);
        for (size_t i = 0; i < 36; ++i)
            CHECK(observed(&identities, &r->after_main[i], inventory->main_slots[i]));
    }
    if (tile != NULL) {
        mc_furnace_fields fields;
        CHECK(mc_tile_entity_furnace_fields_get(tile, &fields) == MC_TILE_ENTITY_OK &&
              fields.burn_remaining == r->after_fields[0] &&
              fields.fuel_duration == r->after_fields[1] &&
              fields.cook_progress == r->after_fields[2]);
        for (int32_t i = 0; i < 3; ++i) {
            const mc_item_stack *value = NULL;
            CHECK(mc_inventory_furnace_borrow(furnace, i, &value, &e) == MC_INVENTORY_OK &&
                  observed(&identities, &r->after_tile[i], value));
        }
    }
    for (size_t i = 0; i < r->tracked_stack_count; ++i)
        CHECK(observed(&identities, &r->tracked_stacks[i],
                       lookup(&identities, r->tracked_stacks[i].identity)));
    if (r->layout_count != 0) {
        CHECK(bm->slots->size == r->layout_count);
        for (size_t i = 0; i < r->layout_count; ++i) {
            mc_container_slot_state s;
            const int32_t *expected = r->layout[i];
            CHECK(mc_container_slot_read(bm, (int32_t)i, &s, &e) == MC_INVENTORY_OK &&
                  s.container_index == expected[0] &&
                  s.inventory_kind ==
                      (expected[1] == 1 ? MC_SLOT_INVENTORY_FURNACE : MC_SLOT_INVENTORY_PLAYER) &&
                  s.inventory ==
                      (expected[1] == 1 ? (const void *)furnace : (const void *)inventory) &&
                  s.inventory_index == expected[2] && s.x == expected[3] && s.y == expected[4] &&
                  s.kind == (expected[5] == 2 ? MC_SLOT_FURNACE : MC_SLOT_NORMAL));
        }
    }
    if (r->has_bm_caches)
        CHECK(bm->furnace_cook == r->bm_caches[0] && bm->furnace_burn == r->bm_caches[1] &&
              bm->furnace_fuel == r->bm_caches[2]);
    ok = true;
cleanup:
    if (!ok)
        fprintf(stderr, "Original BM profile failed: %s\n", r->profile);
    (void)mc_container_interaction_retire(&call, NULL);
    (void)mc_container_retire(&bm, NULL);
    (void)mc_inventory_player_retire(&inventory, NULL);
    mc_inventory_furnace_release(furnace);
    mc_tile_entity_release(tile);
    for (size_t i = 0; i < 8; ++i)
        mc_item_stack_release(inputs[i]);
    return ok;
}

static bool adapter_controls(mc_world_runtime_classes *classes) {
    bool ok = false;
    mc_tile_entity *tile = NULL;
    mc_inventory_furnace *furnace = NULL;
    mc_item_stack *stack = NULL, *removed = NULL;
    mc_inventory_error e = {0};
    CHECK(mc_tile_entity_allocate_in_classes(classes, MC_TILE_ENTITY_FURNACE, &tile) ==
              MC_TILE_ENTITY_OK &&
          mc_tile_entity_construct(tile) == MC_TILE_ENTITY_OK &&
          mc_inventory_furnace_from_tile(MC_REGISTRY_CLIENT, tile, NULL, &furnace, &e) ==
              MC_INVENTORY_OK);
    CHECK(mc_item_stack_allocate(&stack) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(stack, 15, 90, 0) == MC_ITEM_STACK_OK &&
          mc_inventory_furnace_set(furnace, 0, stack, &e) == MC_INVENTORY_OK && e.furnace == NULL);
    mc_item_stack_fields fields;
    CHECK(mc_item_stack_fields_get(stack, &fields) == MC_ITEM_STACK_OK && fields.count == 64);
    CHECK(mc_inventory_furnace_split(furnace, 0, -1, &removed, &e) == MC_INVENTORY_OK &&
          removed != stack && mc_item_stack_fields_get(stack, &fields) == MC_ITEM_STACK_OK &&
          fields.count == 65);
    mc_item_stack_release(removed);
    removed = NULL;
    CHECK(mc_inventory_furnace_split(furnace, 0, 100, &removed, &e) == MC_INVENTORY_OK &&
          removed == stack);
    const mc_item_stack *preserved = stack;
    CHECK(mc_inventory_furnace_borrow(furnace, 3, &preserved, &e) ==
              MC_INVENTORY_INDEX_OUT_OF_BOUNDS &&
          preserved == stack && e.source_exception && e.frames[0].pc == 5);
    bool usable = true;
    CHECK(mc_inventory_furnace_usable(furnace, NULL, &usable, &e) == MC_INVENTORY_NULL_REFERENCE &&
          usable && e.source_exception && e.frames[0].pc == 16);
    inv_furnace_node_view node;
    CHECK(inv_furnace_node_get(furnace, &node, &e) == MC_INVENTORY_OK && node.tile == tile &&
          node.native_holds == 1 &&
          inv_furnace_terminal_admit(furnace, false, &e) == MC_INVENTORY_OK);
    inv_furnace_terminal_seal(furnace);
    CHECK(mc_inventory_furnace_set(furnace, 0, stack, &e) == MC_INVENTORY_BUSY &&
          inv_furnace_terminal_admit(furnace, true, &e) == MC_INVENTORY_OK &&
          mc_inventory_furnace_end_calls(furnace, &e) == MC_INVENTORY_OK &&
          mc_inventory_furnace_retire(&furnace, &e) == MC_INVENTORY_OK && furnace == NULL);
    ok = true;
cleanup:
    mc_inventory_furnace_release(furnace);
    mc_tile_entity_release(tile);
    mc_item_stack_release(stack);
    mc_item_stack_release(removed);
    return ok;
}

static bool notify_and_output_controls(mc_registry_classes *registry,
                                       mc_world_runtime_classes *classes) {
    bool ok = false;
    mc_tile_entity *tile = NULL;
    mc_inventory_furnace *furnace = NULL;
    mc_inventory_player *inventory = NULL;
    mc_container *bm = NULL;
    mc_item_stack *input = NULL;
    mc_inventory_error e = {0};
    CHECK(mc_tile_entity_allocate_in_classes(classes, MC_TILE_ENTITY_FURNACE, &tile) ==
              MC_TILE_ENTITY_OK &&
          mc_tile_entity_construct(tile) == MC_TILE_ENTITY_OK &&
          mc_inventory_furnace_from_tile(MC_REGISTRY_CLIENT, tile, NULL, &furnace, &e) ==
              MC_INVENTORY_OK &&
          mc_inventory_player_create(MC_REGISTRY_CLIENT, NULL, NULL, &inventory, &e) ==
              MC_INVENTORY_OK &&
          mc_container_furnace_create(MC_REGISTRY_CLIENT, registry, inventory, furnace, NULL, &bm,
                                      &e) == MC_INVENTORY_OK);
    /* An authored native caller scope invokes the real VQ validity leaf.
     * No Player, UI input, packet or award result is claimed here. */
    mc_container_interaction_call caller = {.state = {.container = bm}};
    mc_container_interaction_error_init(&caller.error);
    bool valid = true;
    CHECK(inv_interaction_slot_valid(&caller, bm->slots->values[2], NULL, &valid) && !valid &&
          strcmp(caller.error.child.class_name, "vq") == 0 && caller.error.child.pc == 1);
    CHECK(mc_item_stack_allocate(&input) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(input, 15, 3, 2) == MC_ITEM_STACK_OK &&
          mc_inventory_furnace_set(furnace, 0, input, &e) == MC_INVENTORY_OK);
    /* A real null EC occurrence is a legal List element. DW first clones and
     * stores its snapshot, then fails at its actual interface invocation113. */
    bm->listeners->values = inv_alloc(&bm->budget, sizeof(*bm->listeners->values), &e);
    CHECK(bm->listeners->values != NULL);
    bm->listeners->capacity = bm->listeners->size = 1;
    bm->listeners->values[0] = NULL;
    CHECK(mc_container_furnace_detect_changes(bm, &e) == MC_INVENTORY_NULL_REFERENCE &&
          e.source_exception && bm->snapshots->values[0] != NULL &&
          bm->snapshots->values[0] != input && bm->furnace_cook == 0 && bm->furnace_burn == 0 &&
          bm->furnace_fuel == 0);
    bool dw = false, bm_parent = false;
    for (size_t i = 0; i < e.frame_count; ++i) {
        dw |= strcmp(e.frames[i].class_name, "dw") == 0 && e.frames[i].pc == 113;
        bm_parent |= strcmp(e.frames[i].class_name, "bm") == 0 && e.frames[i].pc == 1;
    }
    CHECK(dw && bm_parent);
    ok = true;
cleanup:
    (void)mc_container_retire(&bm, NULL);
    (void)mc_inventory_player_retire(&inventory, NULL);
    mc_inventory_furnace_release(furnace);
    mc_tile_entity_release(tile);
    mc_item_stack_release(input);
    return ok;
}

int main(int argc, char **argv) {
    int result = 1;
    mc_resource_files *files = NULL;
    mc_materials *materials = NULL;
    mc_java_random_profile *profile = NULL;
    mc_registry_classes *registry = NULL;
    mc_world_runtime_classes *classes = NULL;
    static mc_math_trig trig;
    if (argc != 2 || !mc_math_trig_init(&trig) ||
        mc_resource_files_create(argv[1], &files) != MC_REGISTRY_OK ||
        !mc_materials_create(&materials) ||
        mc_java_random_profile_create(SIZE_MAX, &profile) != MC_JAVA_RANDOM_PROFILE_OK)
        goto cleanup;
    const mc_registry_options options = {MC_REGISTRY_CLIENT, profile, materials, 0,
                                         mc_resource_files_source(files)};
    if (mc_registry_classes_allocate(&options, &registry, NULL) != MC_REGISTRY_OK ||
        mc_registry_classes_initialize_statistics(registry, NULL) != MC_REGISTRY_OK ||
        mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, profile, &trig, &classes, NULL) !=
            MC_WORLD_STORAGE_OK ||
        mc_world_runtime_classes_bind_registry(classes, registry, NULL) != MC_REGISTRY_OK)
        goto cleanup;
    for (size_t i = 0; i < sizeof mc_furnace_bm_references / sizeof *mc_furnace_bm_references; ++i)
        if (!reference_bm(registry, classes, mc_furnace_bm_references[i]))
            goto cleanup;
    if (!adapter_controls(classes) || !notify_and_output_controls(registry, classes))
        goto cleanup;
    printf("Furnace backend controls passed: %u (27 original BM profiles)\n", checks);
    result = 0;
cleanup:
    if (classes != NULL && mc_world_runtime_classes_free(classes, NULL) != MC_WORLD_STORAGE_OK)
        result = 1;
    if (registry != NULL && mc_registry_classes_free(registry, NULL) != MC_REGISTRY_OK)
        result = 1;
    if (files != NULL && mc_resource_files_free(files) != MC_REGISTRY_OK)
        result = 1;
    mc_materials_destroy(materials);
    mc_java_random_profile_free(profile);
    return result;
}
