/* CPU HP/CD/ID construction and inherited close over genuine native owners.
 * Nullable Player Inventory is an explicit constructor input; no App startup,
 * physical input, graphical frame or original pixel comparison is claimed. */
#include "client/application_state.h"
#include "client/gui/source_chest_screen_internal.h"
#include "client/gui/source_screen_transition_receipts_internal.h"
#ifdef MC_TEST_CHEST_DRAWING
#include "client/gui/source_inventory_drawing_internal.h"
#endif
#include "mc/world/runtime/construction.h"
#include "mc/world/runtime/tile_entities/tile_entities.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int checks;
static mc_math_trig trig;
#define CHECK(value)                                                                               \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(value)) {                                                                            \
            fprintf(stderr, "Chest screen check%u line%d: %s\n", checks, __LINE__, #value);        \
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
    mc_client_screen_error error;
    mc_inventory_error inventory_error;
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
    mc_client_source_hp *screen = NULL;
    CHECK(mc_client_screen_chest_allocate(classes, models, NULL, &screen, &error) ==
              MC_CLIENT_SCREEN_OK &&
          screen != NULL && mc_client_screen_inventory_item_renderer(classes) != NULL);
    bool pauses = true;
    CHECK(mc_client_screen_pauses_game(mc_client_screen_chest_base(screen), &pauses, &error) ==
              MC_CLIENT_SCREEN_OK &&
          !pauses && strcmp(error.site.class_name, "id") == 0);
    CHECK(mc_client_screen_chest_construct(screen, NULL, NULL, &error) ==
              MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE &&
          error.source_exception && !error.native_policy && error.site.pc == 7 &&
          strcmp(error.site.class_name, "hp") == 0);
    mc_client_chest_screen_state state;
    mc_container_state container;
    CHECK(mc_client_screen_chest_inspect(screen, &state, &error) == MC_CLIENT_SCREEN_OK &&
          state.phase == MC_CLIENT_SCREEN_FAILED && state.container_owned &&
          state.constructor_container != NULL && state.id.j == NULL && state.id.a == 0 &&
          state.id.i == 0 && state.hp.l == NULL && state.hp.m == NULL && state.hp.n == 0 &&
          mc_container_read_state(state.constructor_container, &container) &&
          container.kind == MC_CONTAINER_CHEST && container.constructor_failed &&
          !container.constructed);
    const mc_client_screen_error failed = *state.failure;
    mc_container *actual = state.constructor_container;
    CHECK(mc_client_screen_chest_construct(screen, NULL, NULL, &error) ==
              MC_CLIENT_SCREEN_INVALID_PHASE &&
          screen->container.captured_container == actual &&
          screen->container.base.failure.site.pc == failed.site.pc);
    CHECK(mc_client_screen_chest_retire(&screen, &error) == MC_CLIENT_SCREEN_OK && screen == NULL);

    mc_tile_entity *tiles[2] = {0};
    mc_inventory_chest *chests[2] = {0}, *joined = NULL;
    for (size_t i = 0; i < 2; ++i) {
        CHECK(mc_tile_entity_allocate_in_classes(world_classes, MC_TILE_ENTITY_CHEST, &tiles[i]) ==
                  MC_TILE_ENTITY_OK &&
              mc_tile_entity_construct(tiles[i]) == MC_TILE_ENTITY_OK &&
              mc_inventory_chest_from_tile(MC_REGISTRY_CLIENT, tiles[i], NULL, &chests[i],
                                           &inventory_error) == MC_INVENTORY_OK);
    }
    CHECK(mc_inventory_chest_large_create(MC_REGISTRY_CLIENT, "Large chest", chests[0], chests[1],
                                          NULL, &joined, &inventory_error) == MC_INVENTORY_OK);
    for (size_t i = 0; i < 2; ++i) {
        mc_inventory_chest *chest = i == 0 ? chests[0] : joined;
        const int32_t rows = i == 0 ? 3 : 6, count = rows * 9;
        CHECK(mc_client_screen_chest_allocate(classes, models, NULL, &screen, &error) ==
                  MC_CLIENT_SCREEN_OK &&
              mc_client_screen_chest_construct(screen, NULL, chest, &error) ==
                  MC_CLIENT_SCREEN_OK &&
              mc_client_screen_chest_inspect(screen, &state, &error) == MC_CLIENT_SCREEN_OK &&
              state.phase == MC_CLIENT_SCREEN_READY && state.hp.l == NULL && state.hp.m == chest &&
              state.hp.n == rows && state.id.a == 176 && state.id.i == 114 + rows * 18 &&
              state.id.j == state.constructor_container && state.chest_held &&
              !screen->container.base.fields.f && state.last_pc == 70 &&
              mc_container_read_state(state.id.j, &container) &&
              container.slots == (size_t)(count + 36) && container.chest_rows == rows);
        const unsigned int stores[] = {15, 20, 25, 30, 53, 67};
        const mc_client_source_da *base = mc_client_screen_chest_base(screen);
        CHECK(base->store_count >= sizeof(stores) / sizeof(*stores));
        for (size_t store = 0; store < sizeof(stores) / sizeof(*stores); ++store) {
            const mc_client_screen_site site =
                base->stores[base->store_count - sizeof(stores) / sizeof(*stores) + store];
            CHECK(strcmp(site.class_name, "hp") == 0 && strcmp(site.method, "<init>") == 0 &&
                  strcmp(site.descriptor, "(Llw;Llw;)V") == 0 && site.pc == stores[store]);
        }
#ifdef MC_TEST_CHEST_DRAWING
        /* Enter the real HP.k override with this genuine JS/OG receiver and
         * its actual nullable Font field. It reaches Font.b19 after the name
         * query and stops before any glyph/GL operation; no frame is claimed. */
        mc_client_inventory_draw_call *draw = calloc(1, sizeof(*draw));
        CHECK(draw != NULL && screen->container.base.fields.g == NULL);
        draw->owner = &screen->container;
        CHECK(!mc_chest_draw_foreground(draw) &&
              draw->error.screen.status == MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE &&
              !draw->error.screen.native_policy && !draw->state.foreground_returned &&
              strcmp(draw->error.screen.site.class_name, "hp") == 0 &&
              strcmp(draw->error.screen.site.method, "k") == 0 &&
              strcmp(draw->error.screen.site.descriptor, "()V") == 0 &&
              draw->error.screen.site.pc == 19);
        CHECK(draw->error.screen.source_exception && !draw->error.screen.source_error &&
              draw->error.screen.exception_class != NULL &&
              strcmp(draw->error.screen.exception_class, "java/lang/NullPointerException") == 0);
        free(draw);
#endif
        for (int32_t slot = 0; slot < count + 36; ++slot) {
            mc_container_slot_state value;
            CHECK(mc_container_slot_read(state.id.j, slot, &value, &inventory_error) ==
                  MC_INVENTORY_OK);
            if (slot < count)
                CHECK(value.inventory_kind == MC_SLOT_INVENTORY_CHEST && value.inventory == chest &&
                      value.inventory_index == slot && value.x == 8 + slot % 9 * 18 &&
                      value.y == 18 + slot / 9 * 18);
            else {
                const int32_t local = slot - count, offset = (rows - 4) * 18;
                CHECK(value.inventory_kind == MC_SLOT_INVENTORY_PLAYER && value.inventory == NULL &&
                      value.inventory_index == (local < 27 ? local + 9 : local - 27) &&
                      value.x == 8 + local % 9 * 18 &&
                      value.y == (local < 27 ? 103 + local / 9 * 18 : 161) + offset);
            }
        }
        /* HP has no OO.h override. With the declared nullable App.h the real
         * inherited ID.h returns at10, without a second direct close call. */
        client_application *app = calloc(1, sizeof(*app));
        CHECK(app != NULL);
        screen->container.base.fields.b = app;
        CHECK(mc_client_screen_closed(mc_client_screen_chest_base(screen), &error) ==
                  MC_CLIENT_SCREEN_OK &&
              screen->container.inherited_closed_returned && screen->container.source_calls == 0 &&
              screen->container.closed_direct == NULL);
        screen->container.base.fields.b = NULL;
        free(app);
        CHECK(mc_client_screen_retain(mc_client_screen_chest_base(screen), &error) ==
                  MC_CLIENT_SCREEN_OK &&
              mc_client_screen_chest_retire(&screen, &error) == MC_CLIENT_SCREEN_BUSY &&
              screen != NULL);
        mc_client_source_da *alias = mc_client_screen_chest_base(screen);
        CHECK(mc_client_screen_release(&alias, &error) == MC_CLIENT_SCREEN_OK && alias == NULL);
        /* Declared native diagnostic inputs test the two-phase borrow boundary;
         * this does not manufacture a source invocation or JS dirty failure. */
        mc_live_tile_entity_error *view = calloc(1, sizeof(*view));
        mc_screen_transition_receipts *receipt = calloc(1, sizeof(*receipt));
        CHECK(view != NULL && receipt != NULL);
        mc_client_inventory_screen_error *detail = &screen->container.detail;
        detail->ownership.chest = detail->cleanup_ownership.chest = view;
        detail->container.interaction.inventory.chest = view;
        detail->cleanup_container.interaction.inventory.chest = view;
        detail->inherited_close.interaction.inventory.chest = view;
        detail->direct_close.interaction.inventory.chest = view;
        detail->ownership.source_exception = true;
        detail->ownership.exception_class = "java/lang/IllegalStateException";
        mc_client_screen_error outer = {.inventory = detail};
        CHECK(mc_screen_transition_inventory_capture(receipt, &outer, detail) &&
              receipt->inventory.ownership.chest == view);
        CHECK(mc_screen_lock(classes, &error));
        const bool prepared =
            mc_screen_container_terminal_prepare_locked(&screen->container, &error);
        mtx_unlock(&classes->mutex);
        CHECK(prepared && screen->container.base.phase == MC_CLIENT_SCREEN_RETIRING &&
              screen->container.source_calls == 0 && screen->fields.m == chest);
        mc_screen_transition_inventory_after(receipt, detail);
        CHECK(receipt->inventory.ownership.chest == NULL &&
              receipt->inventory.cleanup_ownership.chest == NULL &&
              receipt->inventory.container.interaction.inventory.chest == NULL &&
              receipt->inventory.cleanup_container.interaction.inventory.chest == NULL &&
              receipt->inventory.inherited_close.interaction.inventory.chest == NULL &&
              receipt->inventory.direct_close.interaction.inventory.chest == NULL &&
              receipt->inventory.ownership.source_exception &&
              strcmp(receipt->inventory.ownership.exception_class,
                     "java/lang/IllegalStateException") == 0);
        CHECK(mc_client_screen_chest_inspect(screen, &state, &error) == MC_CLIENT_SCREEN_OK &&
              state.id.j == NULL && state.hp.l == NULL && state.hp.m == NULL && state.hp.n == rows);
        CHECK(mc_client_screen_chest_retire(&screen, &error) == MC_CLIENT_SCREEN_OK &&
              screen == NULL);
        mc_screen_transition_inventory_after(receipt, NULL);
        free(receipt);
        free(view);
    }
    CHECK(mc_inventory_chest_retire(&joined, &inventory_error) == MC_INVENTORY_OK);
    for (size_t i = 0; i < 2; ++i) {
        CHECK(mc_inventory_chest_retire(&chests[i], &inventory_error) == MC_INVENTORY_OK);
        mc_tile_entity_release(tiles[i]);
    }
    mc_world_storage_error world_error;
    const mc_world_storage_status world_status =
        mc_world_runtime_classes_free(world_classes, &world_error);
    if (world_status != MC_WORLD_STORAGE_OK)
        fprintf(stderr, "Chest World classes cleanup status=%d operation=%s\n", (int)world_status,
                world_error.operation == NULL ? "none" : world_error.operation);
    CHECK(world_status == MC_WORLD_STORAGE_OK);
    CHECK(mc_client_screen_classes_release(&classes, &error) == MC_CLIENT_SCREEN_OK &&
          classes == NULL);
    CHECK(mc_client_render_classes_free(models) == MC_CLIENT_MODEL_OK);
    /* World-class Entity binding also installs the actual Registry binding;
     * freeing that World namespace does not end the Registry's occurrence. */
    CHECK(mc_entity_classes_free(entities) == MC_ENTITY_BUSY);
    CHECK(mc_registry_classes_free(registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_entity_classes_free(entities) == MC_ENTITY_OK);
    mc_materials_destroy(materials);
    mc_java_random_profile_free(profile);
#ifdef MC_TEST_CHEST_DRAWING
    puts("Entered HP foreground: nullable Font reaches k19 source NPE; no GL frame");
#endif
    printf("Chest screen CPU: %u checks; HP/CD/ID, 27/54-slot layout, NULL prefix, inherited "
           "close, checked retirement\n",
           checks);
    return 0;
}
