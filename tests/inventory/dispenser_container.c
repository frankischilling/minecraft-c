/* Authored CPU controls for actual SH layout/inherited DW/ordinary GP bodies.
 * Real AZ/IX/Stack owners are used. The declared GS.c/GS.b host below isolates
 * Container behavior; it is not a Player, GUI, device or gameplay qualification.
 * Layout constants are transcribed from unchanged SH Code174, not a modern
 * Dispenser transfer convention. Root separately compares the original JVM. */
#include "../../src/inventory/container/interaction_internal.h"
#include "../../src/inventory/dispenser_internal.h"
#include "../../src/inventory/player/source_internal.h"
#include "mc/resources/files/files.h"
#include "mc/statistics/registry/registry.h"
#include "mc/world/runtime/construction.h"
#include "mc/world/runtime/tile_entities/tile_entities.h"

#include <stdio.h>
#include <string.h>

static unsigned checks;
#define CHECK(value)                                                                               \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(value)) {                                                                            \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #value);                            \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

typedef struct fixture {
    mc_tile_entity *tile;
    mc_inventory_dispenser *dispenser;
    mc_inventory_player *inventory;
    mc_container *container;
    mc_container_interaction_call *call;
    mc_item_stack *a, *b, *cursor, *returned;
    unsigned inventory_calls, cursor_calls;
    bool observed_cursor_store, fail_cursor;
} fixture;

static bool fields(const mc_item_stack *stack, int32_t id, int32_t count, int32_t damage,
                   int32_t animation) {
    mc_item_stack_fields f;
    return stack != NULL && mc_item_stack_fields_get(stack, &f) == MC_ITEM_STACK_OK && f.id == id &&
           f.count == count && f.damage == damage && f.animation == animation;
}

static mc_item_stack *stack(int32_t id, int32_t count, int32_t damage, int32_t animation) {
    mc_item_stack *value = NULL;
    if (mc_item_stack_allocate(&value) != MC_ITEM_STACK_OK)
        return NULL;
    if (mc_item_stack_construct(value, id, count, damage) != MC_ITEM_STACK_OK ||
        mc_item_stack_source_animation_set(value, animation) != MC_ITEM_STACK_OK) {
        mc_item_stack_release(value);
        return NULL;
    }
    return value;
}

static bool setup(fixture *f, mc_registry_classes *registry, mc_world_runtime_classes *classes,
                  bool player, bool dispenser) {
    *f = (fixture){0};
    if (dispenser && (mc_tile_entity_allocate_in_classes(classes, MC_TILE_ENTITY_DISPENSER,
                                                         &f->tile) != MC_TILE_ENTITY_OK ||
                      mc_tile_entity_construct(f->tile) != MC_TILE_ENTITY_OK ||
                      mc_inventory_dispenser_from_tile(MC_REGISTRY_CLIENT, f->tile, NULL,
                                                       &f->dispenser, NULL) != MC_INVENTORY_OK))
        return false;
    if (player && mc_inventory_player_create(MC_REGISTRY_CLIENT, NULL, NULL, &f->inventory, NULL) !=
                      MC_INVENTORY_OK)
        return false;
    return mc_container_dispenser_create(MC_REGISTRY_CLIENT, registry, f->inventory, f->dispenser,
                                         NULL, &f->container, NULL) == MC_INVENTORY_OK;
}

static bool destroy(fixture *f) {
    bool ok = mc_container_interaction_retire(&f->call, NULL) == MC_INVENTORY_OK;
    if (!ok)
        return false;
    ok = mc_container_retire(&f->container, NULL) == MC_INVENTORY_OK && ok;
    ok = mc_inventory_player_retire(&f->inventory, NULL) == MC_INVENTORY_OK && ok;
    ok = mc_inventory_dispenser_end_calls(f->dispenser, NULL) == MC_INVENTORY_OK && ok;
    mc_inventory_dispenser_release(f->dispenser);
    mc_tile_entity_release(f->tile);
    mc_item_stack_release(f->a);
    mc_item_stack_release(f->b);
    mc_item_stack_release(f->cursor);
    mc_item_stack_release(f->returned);
    *f = (fixture){0};
    return ok;
}

static mc_inventory_status inventory(void *context, mc_entity *player, mc_inventory_player **out,
                                     mc_container_interaction_error *error) {
    fixture *f = context;
    (void)player;
    (void)error;
    ++f->inventory_calls;
    *out = f->inventory;
    return MC_INVENTORY_OK;
}

static mc_inventory_status cursor_changed(void *context, mc_entity *player, mc_item_stack *value,
                                          mc_container_interaction_error *error) {
    fixture *f = context;
    mc_inventory_player_state state;
    (void)player;
    ++f->cursor_calls;
    f->observed_cursor_store =
        mc_inventory_player_read_state(f->inventory, &state) && state.cursor == value;
    if (f->fail_cursor) {
        error->status = MC_INVENTORY_UNIMPLEMENTED_DEPENDENCY;
        error->operation = "authored GS.b observer failure after actual IX cursor store";
        error->native_policy = true;
        return error->status;
    }
    return MC_INVENTORY_OK;
}

static mc_inventory_status click(fixture *f, int32_t slot, int32_t button, bool shift,
                                 mc_container_interaction_error *error) {
    const mc_container_interaction_host host = {
        .context = f, .inventory = inventory, .cursor_changed = cursor_changed};
    return mc_container_click_begin(f->container, NULL, slot, button, shift, &host, &f->call,
                                    error);
}

static bool layout_controls(mc_registry_classes *registry, mc_world_runtime_classes *classes) {
    /* inventory index, x, y: original SH's 9+27+9 loop output. */
    static const int32_t layout[45][3] = {
        {0, 62, 17},    {1, 80, 17},    {2, 98, 17},   {3, 62, 35},    {4, 80, 35},
        {5, 98, 35},    {6, 62, 53},    {7, 80, 53},   {8, 98, 53},    {9, 8, 84},
        {10, 26, 84},   {11, 44, 84},   {12, 62, 84},  {13, 80, 84},   {14, 98, 84},
        {15, 116, 84},  {16, 134, 84},  {17, 152, 84}, {18, 8, 102},   {19, 26, 102},
        {20, 44, 102},  {21, 62, 102},  {22, 80, 102}, {23, 98, 102},  {24, 116, 102},
        {25, 134, 102}, {26, 152, 102}, {27, 8, 120},  {28, 26, 120},  {29, 44, 120},
        {30, 62, 120},  {31, 80, 120},  {32, 98, 120}, {33, 116, 120}, {34, 134, 120},
        {35, 152, 120}, {0, 8, 142},    {1, 26, 142},  {2, 44, 142},   {3, 62, 142},
        {4, 80, 142},   {5, 98, 142},   {6, 116, 142}, {7, 134, 142},  {8, 152, 142}};
    bool ok = false;
    fixture f = {0};
    mc_inventory_error e;
    for (unsigned arguments = 0; arguments < 4; ++arguments) {
        CHECK(setup(&f, registry, classes, (arguments & 1u) != 0, (arguments & 2u) != 0));
        mc_container_state state;
        CHECK(mc_container_read_state(f.container, &state) &&
              state.kind == MC_CONTAINER_DISPENSER && state.constructed &&
              state.constructor_entered && !state.constructor_failed &&
              state.constructor_pc == 173 && !state.active && !state.local && state.slots == 45 &&
              state.snapshots == 45 && state.listeners == 0 && state.blocked_players == 0 &&
              state.crafting == NULL && state.result == NULL && state.world == NULL &&
              state.dispenser_inventory == f.dispenser && state.player_inventory == f.inventory);
        CHECK(state.slot_list != state.snapshot_list && state.slot_list != state.listener_list &&
              state.listener_list != state.snapshot_list && state.blocked_player_set != NULL);
        for (int32_t index = 0; index < 45; ++index) {
            mc_container_slot_state s;
            CHECK(mc_container_slot_read(f.container, index, &s, &e) == MC_INVENTORY_OK &&
                  s.kind == MC_SLOT_NORMAL && s.container_index == index &&
                  s.inventory_kind ==
                      (index < 9 ? MC_SLOT_INVENTORY_DISPENSER : MC_SLOT_INVENTORY_PLAYER) &&
                  s.inventory ==
                      (index < 9 ? (const void *)f.dispenser : (const void *)f.inventory) &&
                  s.inventory_index == layout[index][0] && s.x == layout[index][1] &&
                  s.y == layout[index][2] && s.player == NULL && s.crafting_input == NULL &&
                  s.armor_container == NULL && f.container->snapshots->values[index] == NULL);
        }
        const size_t before = state.allocation_bytes;
        CHECK(mc_container_dispenser_construct(f.container, f.inventory, f.dispenser, &e) ==
                  MC_INVENTORY_INVALID_ARGUMENT &&
              mc_container_read_state(f.container, &state) && state.allocation_bytes == before &&
              state.slots == 45 && state.constructor_pc == 173);
        CHECK(mc_container_inventory_changed(f.container, &e) == MC_INVENTORY_OK);
        bool usable = true;
        CHECK(mc_container_dispenser_usable(f.container, NULL, &usable, &e) ==
                  MC_INVENTORY_NULL_REFERENCE &&
              usable && e.source_exception && e.frame_count >= 1 &&
              strcmp(e.frames[0].class_name, "sh") == 0 && e.frames[0].pc == 5);
        if (f.dispenser == NULL)
            CHECK(e.frame_count == 1); /* Null SH field never enters AZ. */
        else {
            CHECK(e.frame_count >= 2 && strcmp(e.frames[1].class_name, "az") == 0 &&
                  e.frames[1].pc == 16);
            mc_inventory_dispenser_state adapter;
            CHECK(mc_inventory_dispenser_read_state(f.dispenser, &adapter) &&
                  adapter.native_holds == 2 && adapter.tile == f.tile && !adapter.active);
        }
        CHECK(destroy(&f));
    }
    ok = true;
cleanup:
    (void)destroy(&f);
    return ok;
}

static bool constructor_prefix_controls(mc_registry_classes *registry,
                                        mc_world_runtime_classes *classes) {
    bool ok = false;
    fixture f = {0};
    mc_inventory_error e;
    static const size_t bounds[] = {8, 9, 44};
    static const int32_t pcs[] = {55, 121, 164};
    CHECK(setup(&f, registry, classes, true, true));
    CHECK(mc_container_retire(&f.container, &e) == MC_INVENTORY_OK);
    for (size_t attempt = 0; attempt < sizeof bounds / sizeof *bounds; ++attempt) {
        mc_inventory_limits limits = mc_inventory_default_limits();
        limits.max_slots = bounds[attempt];
        CHECK(mc_container_dispenser_allocate(MC_REGISTRY_CLIENT, registry, &limits, &f.container,
                                              &e) == MC_INVENTORY_OK);
        CHECK(mc_container_dispenser_construct(f.container, f.inventory, f.dispenser, &e) ==
                  MC_INVENTORY_LIMIT &&
              f.container->constructor_entered && f.container->constructor_failed &&
              !f.container->constructed && !f.container->active &&
              f.container->constructor_pc == pcs[attempt] &&
              f.container->slots->size == bounds[attempt] &&
              f.container->snapshots->size == bounds[attempt] && f.inventory->containers == 1 &&
              e.frame_count == 2 && strcmp(e.frames[0].class_name, "sh") == 0 &&
              e.frames[0].pc == pcs[attempt] && strcmp(e.frames[1].class_name, "dw") == 0 &&
              e.frames[1].pc == 18);
        size_t allocated = 0;
        for (const inv_slot *slot = f.container->allocated_slots; slot != NULL;
             slot = slot->allocated_next)
            ++allocated;
        CHECK(allocated == bounds[attempt] + 1); /* Unpublished NEW GP remains owned. */
        mc_inventory_dispenser_state adapter;
        CHECK(mc_inventory_dispenser_read_state(f.dispenser, &adapter) &&
              adapter.native_holds == 2);
        CHECK(mc_container_dispenser_construct(f.container, f.inventory, f.dispenser, &e) ==
                  MC_INVENTORY_INVALID_ARGUMENT &&
              f.container->slots->size == bounds[attempt]);
        CHECK(mc_container_retire(&f.container, &e) == MC_INVENTORY_OK &&
              f.inventory->containers == 0 &&
              mc_inventory_dispenser_read_state(f.dispenser, &adapter) &&
              adapter.native_holds == 1);
        CHECK(mc_container_dispenser_create(MC_REGISTRY_CLIENT, registry, f.inventory, f.dispenser,
                                            &limits, &f.container, &e) == MC_INVENTORY_LIMIT &&
              f.container == NULL && f.inventory->containers == 0 &&
              mc_inventory_dispenser_read_state(f.dispenser, &adapter) &&
              adapter.native_holds == 1);
    }
    ok = true;
cleanup:
    (void)destroy(&f);
    return ok;
}

static bool inherited_controls(mc_registry_classes *registry, mc_world_runtime_classes *classes) {
    bool ok = false;
    fixture f = {0};
    mc_inventory_error e;
    mc_container_interaction_error ie;
    mc_container_interaction_state state;
    const mc_item_stack *borrow = NULL;
    CHECK(setup(&f, registry, classes, true, true));
    f.a = stack(1, 7, 3, 11);
    f.b = stack(4, 6, 2, 5);
    f.cursor = stack(5, 2, 1, 9);
    mc_entity *notify = NULL;
    CHECK(f.a != NULL && f.b != NULL && f.cursor != NULL &&
          mc_inventory_dispenser_set(f.dispenser, 0, f.a, &e) == MC_INVENTORY_OK &&
          mc_inventory_player_source_set(f.inventory, 9, f.b, &e) == MC_INVENTORY_OK &&
          mc_inventory_player_source_cursor_store(f.inventory, f.cursor, &notify, &e) ==
              MC_INVENTORY_OK);
    f.inventory->changed = false; /* Declared actual IX prestate, not a click repair. */
    CHECK(mc_container_dispenser_transfer_begin(f.container, 0, &f.call, &ie) == MC_INVENTORY_OK &&
          mc_container_interaction_inspect(f.call, &state) && state.returned &&
          state.result == f.a && state.shift_calls == 1 && state.source_pc == 22 &&
          ie.transfer.pc == 22 && strcmp(ie.transfer.class_name, "dw") == 0 &&
          f.inventory_calls == 0);
    CHECK(mc_container_retire(&f.container, &e) == MC_INVENTORY_BUSY &&
          mc_container_interaction_take_result(f.call, &f.returned) == MC_INVENTORY_OK &&
          f.returned == f.a);
    mc_item_stack_release(f.returned);
    f.returned = NULL;
    CHECK(mc_container_interaction_retire(&f.call, &ie) == MC_INVENTORY_OK);
    const int32_t selected[] = {0, 9};
    for (size_t index = 0; index < sizeof selected / sizeof *selected; ++index) {
        mc_item_stack *original = selected[index] == 0 ? f.a : f.b;
        CHECK(click(&f, selected[index], 0, true, &ie) == MC_INVENTORY_OK &&
              mc_container_interaction_inspect(f.call, &state) && state.returned &&
              state.result != NULL && state.result != original && state.shift_calls == 1 &&
              fields(state.result, selected[index] == 0 ? 1 : 4, selected[index] == 0 ? 7 : 6,
                     selected[index] == 0 ? 3 : 2, 0));
        CHECK(fields(f.a, 1, 7, 3, 11) && fields(f.b, 4, 6, 2, 5) && fields(f.cursor, 5, 2, 1, 9) &&
              f.inventory->cursor == f.cursor && !f.inventory->changed && f.cursor_calls == 0 &&
              mc_inventory_dispenser_borrow(f.dispenser, 0, &borrow, &e) == MC_INVENTORY_OK &&
              borrow == f.a && f.inventory->main_slots[9] == f.b);
        CHECK(mc_container_interaction_take_result(f.call, &f.returned) == MC_INVENTORY_OK);
        CHECK(mc_container_interaction_retire(&f.call, &ie) == MC_INVENTORY_OK);
        mc_item_stack_release(f.returned);
        f.returned = NULL;
    }
    CHECK(click(&f, 1, 0, true, &ie) == MC_INVENTORY_OK &&
          mc_container_interaction_inspect(f.call, &state) && state.returned &&
          state.result == NULL && state.shift_calls == 1);
    CHECK(mc_container_interaction_retire(&f.call, &ie) == MC_INVENTORY_OK);
    CHECK(mc_container_dispenser_transfer_begin(f.container, -1, &f.call, &ie) ==
              MC_INVENTORY_INDEX_OUT_OF_BOUNDS &&
          ie.source_exception &&
          strcmp(ie.exception_class, "java/lang/IndexOutOfBoundsException") == 0 &&
          ie.transfer.pc == 5 && mc_container_interaction_inspect(f.call, &state) &&
          !state.returned);
    CHECK(mc_container_interaction_retire(&f.call, &ie) == MC_INVENTORY_OK);
    ok = true;
cleanup:
    (void)destroy(&f);
    return ok;
}

static bool ordinary_click_controls(mc_registry_classes *registry,
                                    mc_world_runtime_classes *classes, bool fail_cursor) {
    bool ok = false;
    fixture f = {0};
    mc_inventory_error e;
    mc_container_interaction_error ie;
    mc_container_interaction_state state;
    const mc_item_stack *borrow = NULL;
    CHECK(setup(&f, registry, classes, true, true));
    f.a = stack(1, 7, 3, 11);
    CHECK(f.a != NULL && mc_inventory_dispenser_set(f.dispenser, 0, f.a, &e) == MC_INVENTORY_OK);
    f.fail_cursor = fail_cursor;
    const mc_inventory_status expected =
        fail_cursor ? MC_INVENTORY_UNIMPLEMENTED_DEPENDENCY : MC_INVENTORY_OK;
    CHECK(click(&f, 0, 1, false, &ie) == expected && f.observed_cursor_store &&
          f.cursor_calls == 1 && mc_container_interaction_inspect(f.call, &state) &&
          state.returned == !fail_cursor && fields(f.a, 1, 3, 3, 11) &&
          fields(f.inventory->cursor, 1, 4, 3, 0) &&
          mc_inventory_dispenser_borrow(f.dispenser, 0, &borrow, &e) == MC_INVENTORY_OK &&
          borrow == f.a);
    CHECK(mc_container_interaction_retire(&f.call, &ie) == MC_INVENTORY_OK &&
          fields(f.a, 1, 3, 3, 11) && fields(f.inventory->cursor, 1, 4, 3, 0));
    if (fail_cursor) {
        ok =
            true; /* Actual completed split/cursor prefix survives native failure and retirement. */
        goto cleanup;
    }
    CHECK(click(&f, 1, 0, false, &ie) == MC_INVENTORY_OK && f.observed_cursor_store &&
          f.inventory->cursor == NULL &&
          mc_inventory_dispenser_borrow(f.dispenser, 1, &borrow, &e) == MC_INVENTORY_OK &&
          fields(borrow, 1, 4, 3, 0));
    CHECK(mc_container_interaction_retire(&f.call, &ie) == MC_INVENTORY_OK);
    CHECK(click(&f, 1, 0, false, &ie) == MC_INVENTORY_OK &&
          mc_inventory_dispenser_borrow(f.dispenser, 1, &borrow, &e) == MC_INVENTORY_OK &&
          borrow == NULL && fields(f.inventory->cursor, 1, 4, 3, 0));
    CHECK(mc_container_interaction_retire(&f.call, &ie) == MC_INVENTORY_OK);
    CHECK(click(&f, 9, 0, false, &ie) == MC_INVENTORY_OK && f.inventory->cursor == NULL &&
          fields(f.inventory->main_slots[9], 1, 4, 3, 0) && f.inventory->changed);
    CHECK(mc_container_interaction_retire(&f.call, &ie) == MC_INVENTORY_OK &&
          fields(f.a, 1, 3, 3, 11));
    ok = true;
cleanup:
    (void)destroy(&f);
    return ok;
}

static bool null_slot_controls(mc_registry_classes *registry) {
    bool ok = false;
    fixture f = {0};
    mc_container_interaction_error e;
    mc_inventory_slot *slot = NULL;
    mc_container_interaction_state state;
    CHECK(mc_container_dispenser_create(MC_REGISTRY_CLIENT, registry, NULL, NULL, NULL,
                                        &f.container, NULL) == MC_INVENTORY_OK);
    CHECK(mc_container_source_slot(f.container, 0, &slot, &e) == MC_INVENTORY_OK);
    int32_t icon = 123;
    CHECK(mc_inventory_slot_source_background(slot, &icon, &e) == MC_INVENTORY_OK && icon == -1);
    CHECK(mc_container_dispenser_transfer_begin(f.container, 0, &f.call, &e) ==
              MC_INVENTORY_NULL_REFERENCE &&
          e.source_exception && strcmp(e.child.class_name, "gp") == 0 && e.child.pc == 8 &&
          e.transfer.pc == 19 && e.inventory.frame_count == 0 &&
          mc_container_interaction_inspect(f.call, &state) && !state.returned);
    CHECK(mc_container_interaction_retire(&f.call, &e) == MC_INVENTORY_OK);
    CHECK(mc_container_dispenser_transfer_begin(f.container, 9, &f.call, &e) ==
              MC_INVENTORY_NULL_REFERENCE &&
          e.child.pc == 8 && e.transfer.pc == 19 && e.inventory.frame_count == 0);
    CHECK(mc_container_interaction_retire(&f.call, &e) == MC_INVENTORY_OK);
    ok = true;
cleanup:
    (void)destroy(&f);
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
        mc_world_runtime_classes_bind_registry(classes, registry, NULL) != MC_REGISTRY_OK ||
        !layout_controls(registry, classes) || !constructor_prefix_controls(registry, classes) ||
        !inherited_controls(registry, classes) ||
        !ordinary_click_controls(registry, classes, false) ||
        !ordinary_click_controls(registry, classes, true) || !null_slot_controls(registry))
        goto cleanup;
    printf("Dispenser Container CPU controls passed: %u\n", checks);
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
