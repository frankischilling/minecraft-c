/* Actual App/Writer/LocalPlayer construction; its observation main is not run.
 * BM publication is an explicitly authored native root scope. No HP/OV/GUI
 * publication or original full-App execution is claimed by this component. */
#define main mc_furnace_output_unused_statistics_main
#include "../../client/statistics/test_player_dispatch.c"
#undef main

#include "../../../src/entity/player/container_interaction.h"
#include "../../../src/entity/player/terminal_inventory.h"
#include "../../../src/inventory/internal.h"
#include "../../../src/item/stack/source_internal.h"
#include "mc/world/runtime/tile_entities/furnace.h"

typedef enum output_fault {
    OUTPUT_NORMAL,
    OUTPUT_NULL_STATS,
    OUTPUT_NULL_CLIENT,
    OUTPUT_NULL_ITEMS,
    OUTPUT_NULL_FISH,
    OUTPUT_FISH_AS_IRON,
    OUTPUT_MAP_FRONTIER,
    OUTPUT_BAD_STACK_ID
} output_fault;

static bool output_value(fixture *f, const mc_statistic *statistic, int32_t *out) {
    CHECK(statistic != NULL && mc_client_stats_writer_value(f->app.stats_writer, statistic, out,
                                                            NULL) == MC_CLIENT_STATS_OK);
    return true;
}

static bool output_achievements(fixture *f, int32_t out[16]) {
    for (size_t i = 0; i < 16; ++i)
        CHECK(output_value(f, f->app.registry->achievements.named[i], &out[i]));
    return true;
}

static bool output_cursor_to_slot(fixture *f, int32_t slot) {
    mc_player_container_call *call = NULL;
    mc_player_container_error e = {0};
    mc_inventory_player *ix = f->player->local_player->fields.player.c;
    CHECK(ix->cursor != NULL && ix->main_slots[slot + 6] == NULL);
    CHECK(mc_player_container_click_begin(f->app.registry, NULL, f->player, 0, slot, 0, false,
                                          &call, &e) == MC_ENTITY_OK &&
          call != NULL);
    mc_player_container_state state;
    CHECK(mc_player_container_inspect(call, &state) && state.returned && ix->cursor == NULL);
    CHECK(mc_player_container_retire(&call, &e) == MC_ENTITY_OK && call == NULL);
    return true;
}

static bool output_case(fixture *f, mc_inventory_furnace *furnace, int32_t id, int32_t amount,
                        bool shift, output_fault fault, int32_t stash) {
    mc_registry_classes *registry = f->app.registry;
    mc_inventory_player *ix = f->player->local_player->fields.player.c;
    mc_item_stack *stack = NULL;
    mc_player_container_call *call = NULL;
    mc_player_container_error error = {0};
    CHECK(ix->cursor == NULL && mc_item_stack_allocate(&stack) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(stack, id, amount, 0) == MC_ITEM_STACK_OK &&
          mc_item_stack_source_animation_set(stack, 7) == MC_ITEM_STACK_OK &&
          mc_inventory_furnace_set(furnace, 2, stack, NULL) == MC_INVENTORY_OK);
    int32_t before[16], after[16], crafted_before = 0, crafted_after = 0;
    CHECK(output_achievements(f, before));
    const mc_statistic *crafted = id >= 0 && id < 32000 ? registry->statistics.D[id] : NULL;
    if (crafted != NULL)
        CHECK(output_value(f, crafted, &crafted_before));
    size_t fish_index = SIZE_MAX;
    for (size_t i = 0; i < 106; ++i)
        if (strcmp(mc_registry_item_static_name(i), "aT") == 0)
            fish_index = i;
    CHECK(fish_index != SIZE_MAX && registry->items.named[fish_index] != NULL);
    mc_statistic **const statistics = registry->statistics.D;
    mc_registry_item_array *const items = registry->items.c;
    client_application *const client = f->player->local_player->fields.minecraft;
    mc_item *const fish = registry->items.named[fish_index];
    const int32_t fish_id = fish->fields.bf;
    const size_t player_holds = f->player->holds;
    if (fault == OUTPUT_NULL_STATS)
        registry->statistics.D = NULL;
    else if (fault == OUTPUT_NULL_CLIENT)
        f->player->local_player->fields.minecraft = NULL;
    else if (fault == OUTPUT_NULL_ITEMS)
        registry->items.c = NULL;
    else if (fault == OUTPUT_NULL_FISH)
        registry->items.named[fish_index] = NULL;
    else if (fault == OUTPUT_FISH_AS_IRON)
        fish->fields.bf = 265;
    const mc_entity_status status =
        mc_player_container_click_begin(registry, NULL, f->player, 0, 2, 0, shift, &call, &error);
    /* Source projections end before assertions; the owning call still keeps
     * its complete reached diagnostic and all actual mutation prefixes. */
    registry->statistics.D = statistics;
    registry->items.c = items;
    f->player->local_player->fields.minecraft = client;
    registry->items.named[fish_index] = fish;
    fish->fields.bf = fish_id;
    const bool success = fault == OUTPUT_NORMAL || fault == OUTPUT_FISH_AS_IRON;
    CHECK(call != NULL && (status == MC_ENTITY_OK) == success);
    mc_player_container_state state;
    CHECK(mc_player_container_inspect(call, &state) && state.returned == success &&
          state.container == f->player->local_player->fields.player.e && state.interaction != NULL);
    const mc_item_stack *slot = stack;
    CHECK(mc_inventory_furnace_borrow(furnace, 2, &slot, NULL) == MC_INVENTORY_OK && slot == NULL);
    mc_item_stack_fields fields;
    CHECK(mc_item_stack_fields_get(stack, &fields) == MC_ITEM_STACK_OK && fields.id == id &&
          fields.count == (shift ? 0 : amount) && fields.animation == 7 &&
          (shift ? ix->cursor == NULL : ix->cursor == stack));
    if (success) {
        CHECK(state.result != NULL &&
              mc_item_stack_fields_get(state.result, &fields) == MC_ITEM_STACK_OK &&
              fields.id == id && fields.count == amount && fields.animation == 0);
        /* VQ reaches GP only after IZ and both independent award tests. */
        CHECK(state.interaction->error != NULL &&
              strcmp(state.interaction->error->child.class_name, "gp") == 0 &&
              (shift || strcmp(state.interaction->error->child.method, "c") == 0));
    } else {
        CHECK(!state.returned && state.result == NULL);
        if (fault == OUTPUT_NULL_STATS || fault == OUTPUT_BAD_STACK_ID) {
            CHECK(error.source_exception && strcmp(error.child.class_name, "iz") == 0 &&
                  error.child.pc == 8 &&
                  strcmp(error.exception_class, fault == OUTPUT_NULL_STATS
                                                    ? "java/lang/NullPointerException"
                                                    : "java/lang/ArrayIndexOutOfBoundsException") ==
                      0);
        } else if (fault == OUTPUT_NULL_CLIENT) {
            CHECK(error.source_exception &&
                  error.statistic.status == MC_PLAYER_STATISTIC_NULL_REFERENCE &&
                  error.statistic.site.pc == 85 && strcmp(error.child.class_name, "dc") == 0);
        } else if (fault == OUTPUT_NULL_ITEMS) {
            CHECK(error.source_exception && strcmp(error.child.class_name, "iz") == 0 &&
                  error.child.pc == 23 && error.registry.status == MC_REGISTRY_NULL_REFERENCE);
        } else if (fault == OUTPUT_NULL_FISH) {
            CHECK(error.source_exception && strcmp(error.child.class_name, "vq") == 0 &&
                  error.child.pc == 46);
        } else {
            CHECK(fault == OUTPUT_MAP_FRONTIER && error.native_policy && !error.source_exception &&
                  strcmp(error.child.class_name, "wr") == 0 && error.child.pc == 4);
        }
    }
    CHECK(output_achievements(f, after));
    for (size_t i = 0; i < 16; ++i) {
        const bool iron = i == 5 && id == 265 && (success || fault == OUTPUT_NULL_FISH);
        const bool cooked_fish =
            i == 10 && ((id == 350 && success) || fault == OUTPUT_FISH_AS_IRON);
        CHECK(after[i] == before[i] + (iron || cooked_fish ? 1 : 0));
    }
    if (crafted != NULL) {
        CHECK(output_value(f, crafted, &crafted_after));
        const int32_t increment = fault == OUTPUT_NULL_STATS || fault == OUTPUT_NULL_CLIENT ? 0
                                  : shift                                                   ? 0
                                          : amount;
        CHECK(crafted_after == crafted_before + increment);
    }
    const mc_item_stack_fields prefix = {shift ? 0 : amount, 7, id, 0};
    CHECK(mc_player_container_retire(&call, &error) == MC_ENTITY_OK && call == NULL &&
          f->player->holds == player_holds &&
          mc_item_stack_fields_get(stack, &fields) == MC_ITEM_STACK_OK &&
          fields.count == prefix.count && fields.animation == prefix.animation &&
          fields.id == prefix.id);
    if (!shift && fault != OUTPUT_BAD_STACK_ID)
        CHECK(output_cursor_to_slot(f, stash)); /* A fresh ordinary click, no retry. */
    mc_item_stack_release(stack);
    return true;
}

static bool furnace_output(fixture *f) {
    mc_tile_entity *tile = NULL;
    mc_inventory_furnace *furnace = NULL;
    mc_container *bm = NULL;
    mc_inventory_error e = {0};
    mc_inventory_player *ix = f->player->local_player->fields.player.c;
    CHECK(mc_tile_entity_allocate_in_classes(f->app.world_classes, MC_TILE_ENTITY_FURNACE, &tile) ==
              MC_TILE_ENTITY_OK &&
          mc_tile_entity_construct(tile) == MC_TILE_ENTITY_OK &&
          mc_inventory_furnace_from_tile(MC_REGISTRY_CLIENT, tile, NULL, &furnace, &e) ==
              MC_INVENTORY_OK &&
          mc_container_furnace_create(MC_REGISTRY_CLIENT, f->app.registry, ix, furnace, NULL, &bm,
                                      &e) == MC_INVENTORY_OK);
    /* Explicit actual native BM owning root; e is its source alias. */
    f->player->local_player->fields.player.e = bm;
    for (size_t i = 0; i <= 4; ++i)
        CHECK(mc_local_player_add_statistic(f->player, f->app.registry->achievements.named[i], 1,
                                            NULL) == MC_PLAYER_STATISTIC_OK);
    CHECK(output_case(f, furnace, 265, 3, false, OUTPUT_NORMAL, 3));
    CHECK(output_case(f, furnace, 350, 2, false, OUTPUT_NORMAL, 4));
    CHECK(output_case(f, furnace, 58, 1, false, OUTPUT_NORMAL, 5));
    CHECK(output_case(f, furnace, 61, 1, false, OUTPUT_NORMAL, 6));
    CHECK(output_case(f, furnace, 270, 1, false, OUTPUT_NORMAL, 7));
    CHECK(output_case(f, furnace, 265, 4, true, OUTPUT_NORMAL, 0));
    CHECK(output_case(f, furnace, 265, 2, false, OUTPUT_NULL_FISH, 8));
    CHECK(output_case(f, furnace, 265, 2, false, OUTPUT_FISH_AS_IRON, 9));
    CHECK(output_case(f, furnace, 265, 3, false, OUTPUT_NULL_STATS, 10));
    CHECK(output_case(f, furnace, 265, 2, false, OUTPUT_NULL_CLIENT, 11));
    CHECK(output_case(f, furnace, 265, 4, false, OUTPUT_NULL_ITEMS, 12));
    CHECK(output_case(f, furnace, 358, 1, false, OUTPUT_MAP_FRONTIER, 13));
    CHECK(output_case(f, furnace, 32767, 3, false, OUTPUT_BAD_STACK_ID, 0));
    /* End this explicitly authored native root scope through the real local
     * terminal handoff. The failed cursor/Source e remain untouched; no close,
     * drop or repaired default-container assignment hides the final prefix. */
    mc_player_terminal_inventory_error terminal;
    mc_container *const source_e = f->player->local_player->fields.player.e;
    mc_item_stack *const cursor = ix->cursor;
    CHECK(cursor != NULL &&
          mc_player_terminal_inventory_adopt(f->player, f->app.registry, f->world, &bm,
                                             &terminal) == MC_ENTITY_OK &&
          bm == NULL && f->player->local_player->fields.player.e == source_e &&
          ix->cursor == cursor);
    CHECK(mc_player_terminal_inventory_end_calls(f->player, f->app.registry, f->world, &terminal) ==
          MC_ENTITY_OK);
    mc_inventory_furnace_release(furnace); /* Existing external right may release after seal. */
    mc_tile_entity_release(tile);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3 || CD(argv[2]) != 0 || !mc_math_trig_init(&trig))
        return 2;
    fixture *f = calloc(1, sizeof(*f));
    if (f == NULL)
        return 1;
    if (!setup(f, argv[1], "furnace-output", true) || !furnace_output(f) || !teardown(f))
        return 1;
    free(f);
    printf("Player Furnace output: %zu checks\n", checks);
    return 0;
}
