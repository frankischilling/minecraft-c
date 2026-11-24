/* CPU OV/BM/ID source prefixes over genuine SK and Inventory owners.
 * Authored field inputs below exercise actual SK scale methods, not World ticks. */
#include "client/application_state.h"
#include "client/gui/source_furnace_screen_internal.h"
#include "client/gui/source_screen_transition_receipts_internal.h"
#ifdef MC_TEST_FURNACE_DRAWING
#include "client/gui/source_inventory_drawing_internal.h"
#endif
#include "mc/world/runtime/construction.h"
#include "mc/world/runtime/tile_entities/furnace.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int checks;
static mc_math_trig trig;
#define CHECK(value)                                                                               \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(value)) {                                                                            \
            fprintf(stderr, "Furnace screen check%u line%d: %s\n", checks, __LINE__, #value);      \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

int main(void) {
    mc_java_random_profile *profile = NULL;
    mc_materials *materials = NULL;
    mc_registry_classes *registry = NULL;
    mc_entity_classes *entities = NULL;
    mc_client_render_classes *models = NULL;
    mc_client_screen_classes *classes = NULL;
    mc_world_runtime_classes *world_classes = NULL;
    mc_client_screen_error error = {0};
    mc_inventory_error inventory_error = {0};
    CHECK(mc_java_random_profile_create(SIZE_MAX, &profile) == MC_JAVA_RANDOM_PROFILE_OK &&
          mc_materials_create(&materials));
    const mc_registry_options options = {
        .side = MC_REGISTRY_CLIENT, .random_profile = profile, .materials = materials};
    CHECK(mc_registry_classes_allocate(&options, &registry, NULL) == MC_REGISTRY_OK &&
          mc_entity_classes_create(MC_ENTITY_CLIENT, profile, NULL, &entities) == MC_ENTITY_OK &&
          mc_client_render_classes_create(entities, profile, &models) == MC_CLIENT_MODEL_OK &&
          mc_client_screen_classes_create(profile, NULL, registry, &classes, &error) ==
              MC_CLIENT_SCREEN_OK &&
          mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, profile, &trig, &world_classes,
                                          NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_classes_bind_registry(world_classes, registry, NULL) == MC_REGISTRY_OK &&
          mc_world_runtime_classes_bind_entities(world_classes, entities) == MC_ENTITY_OK);
    mc_client_source_ov *screen = NULL;
    CHECK(mc_client_screen_furnace_allocate(classes, models, NULL, &screen, &error) ==
          MC_CLIENT_SCREEN_OK);
    bool pauses = true;
    CHECK(mc_client_screen_pauses_game(mc_client_screen_furnace_base(screen), &pauses, &error) ==
              MC_CLIENT_SCREEN_OK &&
          !pauses && error.source_returned && strcmp(error.site.class_name, "id") == 0);
    CHECK(mc_client_screen_furnace_construct(screen, NULL, NULL, &error) ==
              MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE &&
          error.source_exception && !error.native_policy && error.site.pc == 7 &&
          strcmp(error.site.class_name, "ov") == 0);
    mc_client_furnace_screen_state state;
    mc_container_state container;
    CHECK(mc_client_screen_furnace_inspect(screen, &state, &error) == MC_CLIENT_SCREEN_OK &&
          state.phase == MC_CLIENT_SCREEN_FAILED && state.container_owned && state.id.j == NULL &&
          state.id.a == 0 && state.id.i == 0 && state.ov.l == NULL &&
          state.detail->child.pc == 64 && strcmp(state.detail->child.class_name, "bm") == 0 &&
          mc_container_read_state(state.constructor_container, &container) &&
          container.kind == MC_CONTAINER_FURNACE && container.constructor_failed &&
          container.slots == 2);
    mc_container *failed_container = state.constructor_container;
    CHECK(mc_client_screen_furnace_construct(screen, NULL, NULL, &error) ==
              MC_CLIENT_SCREEN_INVALID_PHASE &&
          screen->container.captured_container == failed_container &&
          screen->container.base.failure.site.pc == 7);
    CHECK(mc_client_screen_furnace_retire(&screen, &error) == MC_CLIENT_SCREEN_OK &&
          screen == NULL);

    mc_inventory_player *inventory = NULL;
    mc_tile_entity *tile = NULL;
    mc_inventory_furnace *furnace = NULL;
    CHECK(mc_inventory_player_create(MC_REGISTRY_CLIENT, NULL, NULL, &inventory,
                                     &inventory_error) == MC_INVENTORY_OK &&
          mc_tile_entity_allocate_in_classes(world_classes, MC_TILE_ENTITY_FURNACE, &tile) ==
              MC_TILE_ENTITY_OK &&
          mc_tile_entity_construct(tile) == MC_TILE_ENTITY_OK &&
          mc_inventory_furnace_from_tile(MC_REGISTRY_CLIENT, tile, NULL, &furnace,
                                         &inventory_error) == MC_INVENTORY_OK);
    for (unsigned int pass = 0; pass < 2; ++pass) {
        mc_inventory_furnace *actual = pass == 0 ? NULL : furnace;
        CHECK(mc_client_screen_furnace_allocate(classes, models, NULL, &screen, &error) ==
                  MC_CLIENT_SCREEN_OK &&
              mc_client_screen_furnace_construct(screen, inventory, actual, &error) ==
                  MC_CLIENT_SCREEN_OK &&
              mc_client_screen_furnace_inspect(screen, &state, &error) == MC_CLIENT_SCREEN_OK &&
              state.phase == MC_CLIENT_SCREEN_READY && state.ov.l == actual && state.id.a == 176 &&
              state.id.i == 166 && state.id.j == state.constructor_container &&
              state.container_owned && state.furnace_held == (actual != NULL) &&
              state.last_pc == 18 && mc_container_read_state(state.id.j, &container) &&
              container.slots == 39 && container.furnace_inventory == actual);
        const mc_client_source_da *base = mc_client_screen_furnace_base(screen);
        CHECK(base->kind == MC_CLIENT_SCREEN_FURNACE && base->store_count >= 1 &&
              strcmp(base->stores[base->store_count - 1].class_name, "ov") == 0 &&
              base->stores[base->store_count - 1].pc == 15 &&
              strcmp(base->stores[base->store_count - 1].descriptor, "(Lix;Lsk;)V") == 0);
        for (int32_t slot = 0; slot < 39; ++slot) {
            mc_container_slot_state value;
            CHECK(mc_container_slot_read(state.id.j, slot, &value, &inventory_error) ==
                  MC_INVENTORY_OK);
            if (slot < 3) {
                const int32_t xs[] = {56, 56, 116}, ys[] = {17, 53, 35};
                CHECK(value.inventory_kind == MC_SLOT_INVENTORY_FURNACE &&
                      value.inventory == actual && value.inventory_index == slot &&
                      value.x == xs[slot] && value.y == ys[slot]);
            } else {
                const int32_t local = slot - 3;
                CHECK(value.inventory_kind == MC_SLOT_INVENTORY_PLAYER &&
                      value.inventory == inventory &&
                      value.inventory_index == (local < 27 ? local + 9 : local - 27) &&
                      value.x == 8 + local % 9 * 18 &&
                      value.y == (local < 27 ? 84 + local / 9 * 18 : 142));
            }
        }
        if (actual != NULL) {
            mc_inventory_furnace_state adapter;
            mc_tile_entity *alias = NULL;
            CHECK(mc_inventory_furnace_read_state(actual, &adapter) && adapter.native_holds == 3 &&
                  mc_inventory_furnace_tile_borrow(state.ov.l, &alias, &inventory_error) ==
                      MC_INVENTORY_OK &&
                  alias == tile &&
                  mc_tile_entity_furnace_field_set(tile, MC_FURNACE_BURN_REMAINING, 100) ==
                      MC_TILE_ENTITY_OK &&
                  mc_tile_entity_furnace_field_set(tile, MC_FURNACE_FUEL_DURATION, 0) ==
                      MC_TILE_ENTITY_OK &&
                  mc_tile_entity_furnace_field_set(tile, MC_FURNACE_COOK_PROGRESS, 199) ==
                      MC_TILE_ENTITY_OK);
            bool burning = false;
            int32_t burn = -1, cook = -1;
            mc_furnace_fields fields;
            CHECK(mc_inventory_furnace_burning(state.ov.l, &burning, &inventory_error) ==
                      MC_INVENTORY_OK &&
                  burning &&
                  mc_inventory_furnace_burn_scale(state.ov.l, 12, &burn, &inventory_error) ==
                      MC_INVENTORY_OK &&
                  burn == 6 &&
                  mc_inventory_furnace_cook_scale(state.ov.l, 24, &cook, &inventory_error) ==
                      MC_INVENTORY_OK &&
                  cook == 23 &&
                  mc_tile_entity_furnace_fields_get(tile, &fields) == MC_TILE_ENTITY_OK &&
                  fields.fuel_duration == 200 && fields.burn_remaining == 100 &&
                  fields.cook_progress == 199);
        }
#ifdef MC_TEST_FURNACE_DRAWING
        mc_client_inventory_draw_call *draw = calloc(1, sizeof(*draw));
        CHECK(draw != NULL);
        draw->owner = &screen->container;
        CHECK(!mc_furnace_draw_foreground(draw) && draw->error.screen.source_exception &&
              !draw->error.screen.native_policy &&
              draw->error.screen.status == MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE &&
              strcmp(draw->error.screen.site.class_name, "ov") == 0 &&
              draw->error.screen.site.pc == 12 &&
              strcmp(draw->error.screen.exception_class, "java/lang/NullPointerException") == 0);
        free(draw);
#endif
        client_application *app = calloc(1, sizeof(*app));
        CHECK(app != NULL);
        screen->container.base.fields.b = app; /* Declared nullable App.h for entered ID.h. */
        CHECK(mc_client_screen_closed(mc_client_screen_furnace_base(screen), &error) ==
                  MC_CLIENT_SCREEN_OK &&
              screen->container.inherited_closed_returned &&
              !screen->container.direct_closed_returned &&
              screen->container.closed_direct == NULL && screen->container.source_calls == 0);
        screen->container.base.fields.b = NULL;
        free(app);
        mc_client_source_da *external = mc_client_screen_furnace_base(screen);
        CHECK(mc_client_screen_retain(external, &error) == MC_CLIENT_SCREEN_OK &&
              mc_client_screen_furnace_retire(&screen, &error) == MC_CLIENT_SCREEN_BUSY &&
              screen != NULL &&
              mc_client_screen_release(&external, &error) == MC_CLIENT_SCREEN_OK &&
              external == NULL);
        mc_live_tile_entity_error *view = calloc(1, sizeof(*view));
        mc_screen_transition_receipts *receipt = calloc(1, sizeof(*receipt));
        CHECK(view != NULL && receipt != NULL);
        screen->container.detail.ownership.furnace =
            view; /* Explicit native diagnostic input only. */
        screen->container.detail.ownership.source_exception = true;
        screen->container.detail.ownership.exception_class = "java/lang/IllegalStateException";
        mc_client_screen_error retained = {.inventory = &screen->container.detail};
        CHECK(
            mc_screen_transition_inventory_capture(receipt, &retained, &screen->container.detail));
        CHECK(mc_screen_lock(classes, &error));
        const bool prepared =
            mc_screen_container_terminal_prepare_locked(&screen->container, &error);
        mtx_unlock(&classes->mutex);
        CHECK(prepared && screen->container.base.phase == MC_CLIENT_SCREEN_RETIRING);
        mc_screen_transition_inventory_after(receipt, &screen->container.detail);
        CHECK(retained.inventory->ownership.furnace == NULL &&
              retained.inventory->ownership.source_exception &&
              strcmp(retained.inventory->ownership.exception_class,
                     "java/lang/IllegalStateException") == 0 &&
              mc_client_screen_furnace_inspect(screen, &state, &error) == MC_CLIENT_SCREEN_OK &&
              state.id.j == NULL && state.ov.l == NULL);
        CHECK(mc_client_screen_furnace_retire(&screen, &error) == MC_CLIENT_SCREEN_OK &&
              screen == NULL);
        mc_screen_transition_inventory_after(receipt, NULL);
        free(receipt);
        free(view);
    }
    CHECK(mc_inventory_furnace_retire(&furnace, &inventory_error) == MC_INVENTORY_OK &&
          mc_inventory_player_retire(&inventory, &inventory_error) == MC_INVENTORY_OK);
    mc_tile_entity_release(tile);
    CHECK(mc_world_runtime_classes_free(world_classes, NULL) == MC_WORLD_STORAGE_OK &&
          mc_client_screen_classes_release(&classes, &error) == MC_CLIENT_SCREEN_OK &&
          mc_client_render_classes_free(models) == MC_CLIENT_MODEL_OK &&
          mc_registry_classes_free(registry, NULL) == MC_REGISTRY_OK &&
          mc_entity_classes_free(entities) == MC_ENTITY_OK);
    mc_materials_destroy(materials);
    mc_java_random_profile_free(profile);
    printf("Furnace screen CPU: %u checks; OV/BM/ID,39 slots,nullable SK/IX prefixes,shared SK "
           "scales,inherited close,checked retirement\n",
           checks);
    return 0;
}
