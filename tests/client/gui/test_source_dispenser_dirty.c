/* Authored diagnostic lifetime controls over actual App/World/Player/AZ/GQ
 * owners. The genuine OB/DW/GP call fails at its first Slot.changed prefix.
 * ID's production projection helper is entered separately with that returned
 * child. No SDK mouse/keyboard entry or graphical frame is claimed. */
#define main mc_dispenser_dirty_unused_statistics_main
#include "../statistics/test_player_dispatch.c"
#undef main

#include "client/gui/source_dispenser_screen_internal.h"
#include "client/gui/source_screen_initialize.h"
#include "client/gui/source_screen_transition_receipts_internal.h"
#include "inventory/player/source_internal.h"
#include "mc/world/runtime/tile_entities/dispenser.h"
#include "world/runtime/world_access_internal.h"

typedef struct gqd_producer {
    mc_world_access *self;
    mc_world_access_receipt *receipt;
    mc_world_runtime *world;
    mc_tile_entity *tile;
    char *name;
    size_t calls;
    bool observed;
} gqd_producer;

static mc_world_storage_status dirty(void *context, mc_world_runtime *world, int32_t x, int32_t y,
                                     int32_t z, mc_tile_entity *tile, mc_world_storage_error *cause,
                                     mc_world_access_failure *failure) {
    gqd_producer *p = context;
    ++p->calls;
    p->observed =
        p->calls == 1 && world == p->world && tile == p->tile && x == 8 && y == 64 && z == 8;
    bool removed = false;
    mc_world_storage_status status =
        mc_world_runtime_remove_world_access(world, p->self, &removed, cause);
    p->observed &= status == MC_WORLD_STORAGE_OK && removed &&
                   mc_world_access_free(p->self, NULL) == MC_WORLD_STORAGE_BUSY;
    if (status != MC_WORLD_STORAGE_OK || !removed)
        return status == MC_WORLD_STORAGE_OK ? MC_WORLD_STORAGE_INVALID_DATA : status;
    const char name[] = "test/OwnedDispenserScreenFailure";
    p->name = malloc(sizeof(name));
    if (p->name == NULL) {
        *cause = (mc_world_storage_error){MC_WORLD_STORAGE_ALLOCATION_FAILED,
                                          "GQ authored producer name allocation", 0};
        return cause->status;
    }
    memcpy(p->name, name, sizeof(name));
    status = mc_world_access_receipt_create(p->self, NULL, &p->receipt, failure, cause);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    p->receipt->exception_class = p->name; /* Actual producing receiver owns this storage. */
    *cause = (mc_world_storage_error){MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                      "GQ authored dirty producer failure", 93};
    return cause->status;
}

static bool controls(const char *resources) {
    fixture *f = calloc(1, sizeof(*f));
    mc_math_classes *math = NULL;
    mc_client_screen_classes *screens = NULL;
    mc_client_source_gq *gq = NULL;
    mc_client_source_da *external = NULL;
    mc_tile_entity *tile = NULL;
    mc_inventory_dispenser *adapter = NULL;
    mc_item_stack *stack = NULL;
    mc_player_container_call *call = NULL;
    mc_player_container_error child = {0}, cleanup = {0};
    mc_client_screen_error projected = {0}, screen_cleanup = {0};
    mc_client_screen_init_error initialization = {0}, nested_copy = {0};
    mc_screen_transition_receipts direct = {0}, nested = {0};
    gqd_producer producer = {0};
    CHECK(f != NULL && setup(f, resources, "dispenser-screen-dirty", false));
    CHECK(mc_math_classes_create(SIZE_MAX, &math, NULL) == MC_MATH_CLASS_OK &&
          mc_client_screen_classes_create(f->app.profile, mc_resource_files_source(f->app.files),
                                          f->app.registry, &screens, NULL) == MC_CLIENT_SCREEN_OK &&
          mc_tile_entity_allocate(f->world, MC_TILE_ENTITY_DISPENSER, &tile) == MC_TILE_ENTITY_OK &&
          mc_tile_entity_construct(tile) == MC_TILE_ENTITY_OK &&
          mc_tile_entity_bind_world(tile, f->world) == MC_TILE_ENTITY_OK &&
          mc_tile_entity_set_coordinates(tile, 8, 64, 8) == MC_TILE_ENTITY_OK &&
          mc_inventory_dispenser_from_tile(MC_REGISTRY_CLIENT, tile, NULL, &adapter, NULL) ==
              MC_INVENTORY_OK &&
          mc_item_stack_allocate(&stack) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(stack, 1, 7, 3) == MC_ITEM_STACK_OK &&
          mc_inventory_dispenser_set(adapter, 0, stack, NULL) == MC_INVENTORY_OK &&
          mc_client_screen_dispenser_allocate(screens, f->app.render_classes, NULL, &gq, NULL) ==
              MC_CLIENT_SCREEN_OK &&
          mc_client_screen_dispenser_construct(gq, f->player->local_player->fields.player.c,
                                               adapter, NULL) == MC_CLIENT_SCREEN_OK);
    /* Explicit public App operands. Actual DA/ID.b below publishes Player.e;
     * no private constructor/phase/Inventory/Player source field is repaired. */
    f->app.h = f->player;
    f->app.active_world = f->world;
    f->app.source_math_classes = math;
    CHECK(mc_client_screen_initialize(mc_client_screen_dispenser_base(gq), &f->app, 427, 240,
                                      &initialization) == MC_CLIENT_SCREEN_OK &&
          f->player->local_player->fields.player.e == gq->container.id.j &&
          f->player->local_player->fields.player.e != f->player->local_player->fields.player.d);
    mc_inventory_dispenser_state adapter_state = {0};
    CHECK(mc_inventory_dispenser_read_state(adapter, &adapter_state) &&
          adapter_state.native_holds == 3 && adapter_state.diagnostic_owner_id != 0);
    const uintptr_t id = adapter_state.diagnostic_owner_id;
    producer.world = f->world;
    producer.tile = tile;
    const mc_world_access_source_callbacks callbacks = {.tile_entity_dirty = dirty};
    CHECK(mc_world_access_source_create(&callbacks, &producer, &producer.self, NULL) ==
              MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(f->world, producer.self, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_player_container_click_begin(f->app.registry, math, f->player, 0, 0, 0, false, &call,
                                          &child) == MC_ENTITY_STATE_ERROR &&
          call != NULL && producer.calls == 1 && producer.observed && child.source_exception &&
          !child.native_policy && !child.source_error && child.exception_class == producer.name &&
          child.exception_class_dispenser_owner_id == id && child.site.pc == 11 &&
          child.interaction.site.pc == 210 &&
          child.interaction.exception_class_dispenser_owner_id == id &&
          child.interaction.inventory.dispenser_diagnostic_owner_id == id &&
          child.interaction.inventory.exception_class_from_dispenser &&
          child.interaction.inventory.frames[0].pc == 24 &&
          strcmp(child.interaction.inventory.frames[0].class_name, "ow") == 0 &&
          child.interaction.inventory.dispenser != NULL &&
          child.interaction.inventory.dispenser->cause.system_error == 93 &&
          producer.receipt->holds == 2);
    const mc_item_stack *borrowed = NULL, *cursor = NULL;
    CHECK(mc_inventory_dispenser_borrow(adapter, 0, &borrowed, NULL) == MC_INVENTORY_OK &&
          borrowed == stack &&
          mc_inventory_player_source_cursor_get(f->player->local_player->fields.player.c, &cursor,
                                                NULL) == MC_INVENTORY_OK &&
          cursor == NULL); /* DW210 failed before reading/moving the Stack. */
    CHECK(mc_screen_lock(screens, &screen_cleanup));
    const bool returned = mc_screen_container_player_failure(&gq->container, &child, "id", "a",
                                                             "(III)V", 179, &projected);
    mtx_unlock(&screens->mutex);
    CHECK(!returned && projected.status == MC_CLIENT_SCREEN_APP_FAILED &&
          projected.source_exception && !projected.native_policy &&
          projected.exception_class == producer.name &&
          projected.exception_class_dispenser_owner_id == id && projected.site.pc == 179 &&
          projected.inventory == &gq->container.detail &&
          mc_screen_transition_inventory_capture(&direct, &projected, &gq->container.detail));
    /* Native nested diagnostic copy only: no failed Source initialization is
     * invented. This exercises the same owned receipt/copy expiry route. */
    nested_copy.screen = projected;
    nested_copy.screen.inventory = &gq->container.detail;
    mc_client_screen_error outer = projected;
    outer.inventory = NULL;
    outer.initialization = &nested_copy;
    CHECK(mc_screen_transition_inventory_capture(&nested, &outer, &gq->container.detail));
    external = mc_client_screen_dispenser_base(gq);
    CHECK(mc_client_screen_retain(external, NULL) == MC_CLIENT_SCREEN_OK &&
          mc_client_screen_dispenser_retire(&gq, &screen_cleanup) == MC_CLIENT_SCREEN_BUSY &&
          gq != NULL && gq->container.ended_dispenser_diagnostic_owner_id == 0 &&
          projected.exception_class == producer.name && producer.receipt->holds == 2 &&
          mc_world_access_receipt_retire(&producer.receipt, NULL) == MC_WORLD_STORAGE_BUSY &&
          mc_client_screen_release(&external, NULL) == MC_CLIENT_SCREEN_OK && external == NULL);
    /* Execute the real GS.r body to displace this owned SH before native GQ
     * retirement. The failed click itself never restored the active Container. */
    CHECK(mc_player_container_restore_default(f->player, NULL) == MC_ENTITY_OK &&
          mc_player_container_retire(&call, &cleanup) == MC_ENTITY_OK && call == NULL &&
          mc_client_screen_dispenser_retire(&gq, &screen_cleanup) == MC_CLIENT_SCREEN_OK &&
          gq == NULL && mc_inventory_dispenser_read_state(adapter, &adapter_state) &&
          adapter_state.native_holds == 1 && producer.receipt->holds == 2);
    mc_screen_transition_inventory_after(&direct, NULL);
    mc_screen_transition_inventory_expire_dispenser(&direct, &projected, id);
    mc_screen_transition_inventory_after(&nested, NULL);
    mc_screen_transition_inventory_expire_dispenser(&nested, &outer, id);
    mc_client_screen_error_expire_dispenser(&nested_copy.screen, id);
    nested_copy.screen.inventory = NULL; /* Its actual GQ diagnostic occurrence has ended. */
    mc_player_container_error_expire_dispenser(&child, id);
    CHECK(projected.source_exception && projected.site.pc == 179 &&
          projected.exception_class == NULL && projected.exception_class_dispenser_owner_id == 0 &&
          projected.inventory == &direct.inventory && direct.inventory.container.source_exception &&
          direct.inventory.container.exception_class == NULL &&
          direct.inventory.container.interaction.exception_class == NULL &&
          direct.inventory.container.interaction.inventory.exception_class == NULL &&
          direct.inventory.container.interaction.inventory.dispenser == NULL &&
          outer.source_exception && outer.exception_class == NULL &&
          outer.initialization == &nested.initialization &&
          nested.initialization.screen.source_exception &&
          nested.initialization.screen.exception_class == NULL &&
          nested.initialization.screen.site.pc == 179 &&
          nested_copy.screen.exception_class == NULL && nested_copy.screen.inventory == NULL &&
          nested.inventory.container.interaction.site.pc == 210);
    CHECK(mc_inventory_dispenser_end_calls(adapter, NULL) == MC_INVENTORY_OK &&
          producer.receipt->holds == 1 &&
          mc_world_access_receipt_retire(&producer.receipt, NULL) == MC_WORLD_STORAGE_OK);
    free(producer.name);
    producer.name = NULL;
    CHECK(projected.exception_class == NULL && child.source_exception &&
          child.exception_class == NULL &&
          mc_world_access_free(producer.self, NULL) == MC_WORLD_STORAGE_OK);
    producer.self = NULL;
    mc_item_stack_release(stack);
    CHECK(mc_inventory_dispenser_retire(&adapter, NULL) == MC_INVENTORY_OK && adapter == NULL);
    mc_tile_entity_release(tile);
    CHECK(mc_client_screen_classes_release(&screens, NULL) == MC_CLIENT_SCREEN_OK &&
          screens == NULL && mc_math_classes_release(&math, NULL) == MC_MATH_CLASS_OK &&
          math == NULL);
    f->app.h = NULL;
    f->app.active_world = NULL;
    f->app.source_math_classes = NULL; /* End the fixture's public App views. */
    CHECK(teardown(f));
    free(f);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3 || CD(argv[2]) != 0 || !mc_math_trig_init(&trig))
        return 2;
    if (!controls(argv[1]))
        return 1;
    printf("Dispenser screen diagnostic ownership passed: %zu checks\n", checks);
    return 0;
}
