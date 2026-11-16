#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#define MC_PICKING_BODY_MAIN_NAME mc_chest_eligibility_picking_fixture_main
int mc_chest_eligibility_picking_fixture_main(int argc, char **argv);
#include "../../client/world/test_picking.c"
#undef main

#include "entity/player/active_internal.h"
#include "mc/inventory/chest.h"
#include "mc/world/runtime/tile_entities/tile_entities.h"
#include <math.h>

static bool chest_eligibility_controls(picking_fixture *f) {
    mc_entity *player = f->actors[0];
    mc_active_scope scope = {.player = player, .registry = f->base.registry};
    mc_player_tick_error *e = calloc(1, sizeof(*e));
    CHECK(e != NULL);
    mc_tile_entity *tile = NULL, *unbound = NULL;
    mc_inventory_chest *chest = NULL, *unbound_view = NULL;
    mc_container *cd = NULL, *unbound_cd = NULL;
    mc_live_tile_entity_error world_error;
    const size_t cell = (8u << 11u) | (8u << 7u) | 64u;
    f->chunks[0]->data->blocks[cell] = 54;
    CHECK(mc_live_tile_entities_get(f->base.world, 8, 64, 8, &tile, &world_error) ==
              MC_WORLD_STORAGE_OK &&
          tile != NULL);
    CHECK(mc_inventory_chest_from_tile(MC_REGISTRY_CLIENT, tile, NULL, &chest, NULL) ==
          MC_INVENTORY_OK);
    CHECK(mc_container_chest_create(MC_REGISTRY_CLIENT, f->base.registry,
                                    player->local_player->fields.player.c, chest, NULL, &cd,
                                    NULL) == MC_INVENTORY_OK);
    const double old_x = player->base.x, old_y = player->base.y, old_z = player->base.z;
    const double positions[] = {8.5, .5, .25, NAN};
    const bool expected[] = {true, true, false, false};
    for (size_t i = 0; i < sizeof positions / sizeof *positions; ++i) {
        mc_player_tick_error_init(e);
        e->player_bytecode = 136;
        player->base.x = positions[i];
        player->base.y = 64.5;
        player->base.z = 8.5;
        bool eligible = !expected[i];
        const bool returned = mc_active_container(&scope, cd, &eligible, e);
        player->base.x = old_x;
        player->base.y = old_y;
        player->base.z = old_z;
        CHECK(returned && eligible == expected[i] && e->status == MC_ENTITY_OK &&
              e->player_bytecode == 136 && e->container_bytecode == 10 &&
              e->child == MC_PLAYER_TICK_CHILD_NONE && e->child_error.container.chest == NULL);
        CHECK(mc_player_tick_error_retire(e));
    }
    /* A real World acquisition failure retains the complete checked LW child,
     * including its exact borrowed diagnostic, rather than a generic frontier. */
    mc_player_tick_error_init(e);
    e->player_bytecode = 136;
    mc_chunk_provider *provider = f->base.world->fields.v;
    f->base.world->fields.v = NULL;
    bool untouched = true;
    const bool failed = mc_active_container(&scope, cd, &untouched, e);
    f->base.world->fields.v = provider;
    CHECK(!failed && untouched && e->status != MC_ENTITY_OK && strcmp(e->source_class, "cd") == 0 &&
          strcmp(e->source_method, "b(Lgs;)Z") == 0 && e->player_bytecode == 136 &&
          e->container_bytecode == 5 && e->child == MC_PLAYER_TICK_CHILD_CONTAINER &&
          e->child_error.container.chest != NULL && e->child_error.container.frames[0].pc == 5 &&
          strcmp(e->child_error.container.frames[0].class_name, "cd") == 0 &&
          e->child_error.container.chest->status != MC_WORLD_STORAGE_OK);
    const mc_entity_status first_status = e->status;
    const unsigned int first_site = e->container_bytecode;
    CHECK(mc_registry_classes_retain(f->base.registry, MC_REGISTRY_CLIENT, f->base.profile, NULL) ==
          MC_REGISTRY_OK);
    e->retained_registry = f->base.registry;
    const unsigned int entered = f->base.registry->entered;
    f->base.registry->entered = entered + 1;
    const bool retired = mc_player_tick_error_retire(e);
    f->base.registry->entered = entered;
    CHECK(!retired && e->cleanup.child == MC_PLAYER_TICK_CLEANUP_REGISTRY &&
          e->child_error.container.chest == NULL && e->retained_registry != NULL &&
          e->status == first_status && e->container_bytecode == first_site);
    CHECK(mc_player_tick_error_retire(e) && e->retained_registry == NULL &&
          e->child_error.container.chest == NULL && e->status == first_status);
    CHECK(mc_inventory_chest_end_calls(chest, NULL) == MC_INVENTORY_OK);
    /* Source null flags and the JS World-read frame survive the Player join. */
    CHECK(mc_tile_entity_allocate_in_classes(f->base.world_classes, MC_TILE_ENTITY_CHEST,
                                             &unbound) == MC_TILE_ENTITY_OK &&
          mc_tile_entity_construct(unbound) == MC_TILE_ENTITY_OK);
    CHECK(mc_inventory_chest_from_tile(MC_REGISTRY_CLIENT, unbound, NULL, &unbound_view, NULL) ==
              MC_INVENTORY_OK &&
          mc_container_chest_create(MC_REGISTRY_CLIENT, f->base.registry,
                                    player->local_player->fields.player.c, unbound_view, NULL,
                                    &unbound_cd, NULL) == MC_INVENTORY_OK);
    mc_player_tick_error_init(e);
    CHECK(!mc_active_container(&scope, unbound_cd, &untouched, e) &&
          e->child == MC_PLAYER_TICK_CHILD_CONTAINER && e->child_error.container.source_exception &&
          strcmp(e->child_error.container.exception_class, "java/lang/NullPointerException") == 0 &&
          e->child_error.container.frame_count == 2 && e->child_error.container.frames[0].pc == 5 &&
          e->child_error.container.frames[1].pc == 16 && e->container_bytecode == 5);
    CHECK(mc_player_tick_error_retire(e) && e->child_error.container.source_exception &&
          e->child_error.container.frames[1].pc == 16);
    CHECK(mc_container_retire(&unbound_cd, NULL) == MC_INVENTORY_OK);
    CHECK(mc_inventory_chest_retire(&unbound_view, NULL) == MC_INVENTORY_OK);
    mc_tile_entity_release(unbound);
    unbound = NULL;
    /* Once the actual Tile identity differs, JS returns false before touching
     * even a NULL Player argument. This preserves the real source query order. */
    f->chunks[0]->data->blocks[cell] = 0;
    CHECK(mc_live_tile_entities_remove(f->base.world, 8, 64, 8, &world_error) ==
          MC_WORLD_STORAGE_OK);
    mc_player_tick_error_init(e);
    scope.player = NULL;
    bool eligible = true;
    CHECK(mc_active_container(&scope, cd, &eligible, e) && !eligible &&
          e->container_bytecode == 10 && e->child == MC_PLAYER_TICK_CHILD_NONE);
    CHECK(mc_player_tick_error_retire(e));
    CHECK(mc_container_retire(&cd, NULL) == MC_INVENTORY_OK &&
          mc_inventory_chest_retire(&chest, NULL) == MC_INVENTORY_OK);
    mc_tile_entity_release(tile);
    tile = NULL;
    free(e);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3)
        return 2;
    picking_fixture *f = calloc(1, sizeof(*f));
    if (f == NULL || !picking_open(f, argv[1]) || !picking_case_open(f, argv[2]) ||
        !chest_eligibility_controls(f) || !picking_case_close(f) || !picking_close(f))
        return 1;
    free(f);
    printf("Player Chest eligibility controls passed: %u\n", checks);
    return 0;
}
