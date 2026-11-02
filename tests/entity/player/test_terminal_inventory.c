/* Authored partial-App fixture with genuine Registry/World/Player/OO/DA/ID.b
 * construction. These private local-owner controls do not establish the Root
 * caller's terminal authority or claim whole-App/Source-GC equivalence. */
#define main mc_terminal_inventory_unused_statistics_main
#include "../../client/statistics/test_player_dispatch.c"
#undef main

#include "client/gui/source_screen_initialize.h"
#include "client/gui/source_workbench_screen_internal.h"
#include "entity/player/terminal_inventory.h"
#include "inventory/internal.h"
#include "inventory/player/terminal_internal.h"
#include "mc/client/controller/singleplayer.h"
#include "world/runtime/runtime_internal.h"

typedef struct terminal_player_fixture {
    fixture base;
    mc_math_classes *math;
    mc_client_screen_classes *screens;
    mc_client_source_oo *screen;
    mc_item_stack *payload;
} terminal_player_fixture;

typedef struct terminal_player_observation {
    mc_local_player_snapshot player;
    mc_java_random player_random, world_random;
    mc_client_screen_id_fields id;
    mc_inventory_player_terminal owner;
    mc_entity *app_player;
    mc_client_source_da *app_screen;
    size_t holds, bindings, inventory_bindings;
} terminal_player_observation;

static bool observe(terminal_player_fixture *f, terminal_player_observation *out) {
    mc_entity *player = f->base.player;
    memcpy(&out->player, &player->local_player->fields, sizeof(out->player));
    memcpy(&out->player_random, player->random, sizeof(out->player_random));
    memcpy(&out->world_random, f->base.world->fields.r, sizeof(out->world_random));
    memcpy(&out->id, &f->screen->container.id, sizeof(out->id));
    CHECK(mc_player_terminal_inventory_inspect(player, &out->owner));
    out->app_player = f->base.app.h;
    out->app_screen = f->base.app.r;
    out->holds = player->holds;
    out->bindings = f->base.world->persistent_bindings;
    out->inventory_bindings = player->local_player->fields.player.c->containers;
    return true;
}

static bool same_source(terminal_player_fixture *f, const terminal_player_observation *before,
                        bool same_owner) {
    mc_entity *player = f->base.player;
    mc_inventory_player_terminal owner;
    CHECK(mc_player_terminal_inventory_inspect(player, &owner));
    CHECK(
        memcmp(&before->player, &player->local_player->fields, sizeof(before->player)) == 0 &&
        memcmp(&before->player_random, player->random, sizeof(before->player_random)) == 0 &&
        memcmp(&before->world_random, f->base.world->fields.r, sizeof(before->world_random)) == 0 &&
        memcmp(&before->id, &f->screen->container.id, sizeof(before->id)) == 0 &&
        f->base.app.h == before->app_player && f->base.app.r == before->app_screen &&
        player->holds == before->holds && f->base.world->persistent_bindings == before->bindings &&
        player->local_player->fields.player.c->containers == before->inventory_bindings);
    if (same_owner)
        CHECK(owner.inventory == before->owner.inventory &&
              owner.default_container == before->owner.default_container &&
              owner.active_container == before->owner.active_container &&
              owner.player == before->owner.player && owner.sealed == before->owner.sealed);
    return true;
}

static bool prepare(terminal_player_fixture *f, const char *resources, const char *directory) {
    CHECK(setup(&f->base, resources, directory, true));
    CHECK(mc_math_classes_create(SIZE_MAX, &f->math, NULL) == MC_MATH_CLASS_OK);
    /* Declared App operands only. Actual ID.b below performs e=j itself. */
    f->base.app.h = f->base.player;
    f->base.app.active_world = f->base.world;
    f->base.app.source_math_classes = f->math;
    CHECK(mc_client_singleplayer_controller_allocate(f->base.app.entities, f->base.app.profile, 0,
                                                     &f->base.app.controller,
                                                     NULL) == MC_SINGLEPLAYER_CONTROLLER_OK &&
          mc_client_singleplayer_controller_construct(f->base.app.controller, &f->base.app, NULL) ==
              MC_SINGLEPLAYER_CONTROLLER_OK);
    CHECK(mc_client_screen_classes_create(
              f->base.app.profile, mc_resource_files_source(f->base.app.files),
              f->base.app.registry, &f->screens, NULL) == MC_CLIENT_SCREEN_OK &&
          mc_client_screen_workbench_allocate(f->screens, f->base.app.render_classes, NULL,
                                              &f->screen, NULL) == MC_CLIENT_SCREEN_OK &&
          mc_client_screen_workbench_construct(
              f->screen, f->base.player->local_player->fields.player.c, f->base.world, 8, 64, 8,
              NULL) == MC_CLIENT_SCREEN_OK);
    mc_client_screen_init_error initialization;
    CHECK(mc_client_screen_initialize(mc_client_screen_workbench_base(f->screen), &f->base.app, 427,
                                      240, &initialization) == MC_CLIENT_SCREEN_OK);
    mc_client_workbench_screen_state screen;
    CHECK(mc_client_screen_workbench_inspect(f->screen, &screen, NULL) == MC_CLIENT_SCREEN_OK &&
          screen.phase == MC_CLIENT_SCREEN_READY && screen.container_owned &&
          screen.id.j == screen.constructor_container &&
          screen.id.j == f->base.player->local_player->fields.player.e &&
          screen.id.j != f->base.player->local_player->fields.player.d);
    CHECK(mc_item_stack_allocate(&f->payload) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(f->payload, 3, 7, 5) == MC_ITEM_STACK_OK &&
          mc_inventory_player_set(f->base.player->local_player->fields.player.c, 0, f->payload,
                                  NULL) == MC_INVENTORY_OK &&
          mc_inventory_crafting_set(screen.id.j->crafting, 0, f->payload, NULL) == MC_INVENTORY_OK);
    return true;
}

static bool refused(terminal_player_fixture *f, mc_entity *player, mc_registry_classes *registry,
                    mc_world_runtime *world, mc_container **slot, mc_entity_status expected) {
    terminal_player_observation before;
    CHECK(observe(f, &before));
    mc_container *value = slot == NULL ? NULL : *slot;
    mc_player_terminal_inventory_error error;
    CHECK(mc_player_terminal_inventory_adopt(player, registry, world, slot, &error) == expected &&
          error.status == expected && error.operation != NULL && error.inventory.frame_count == 0 &&
          (slot == NULL || *slot == value) && same_source(f, &before, true));
    CHECK(mc_player_terminal_inventory_adopt(player, registry, world, slot, NULL) == expected &&
          (slot == NULL || *slot == value) && same_source(f, &before, true));
    return true;
}

static bool controls(const char *resources) {
    terminal_player_fixture *f = calloc(1, sizeof(*f));
    CHECK(f != NULL && prepare(f, resources, "player-terminal-owner"));
    mc_entity *player = f->base.player;
    mc_registry_classes *registry = f->base.app.registry;
    mc_world_runtime *world = f->base.world;
    mc_container **slot = &f->screen->container.captured_container;
    mc_container *actual_it = *slot;
    mc_inventory_player *actual_ix = player->local_player->fields.player.c;
    mc_container *actual_aa = player->local_player->fields.player.d;
    mc_inventory_player_terminal owner;
    CHECK(mc_player_terminal_inventory_inspect(player, &owner) && !owner.sealed &&
          owner.inventory == NULL && owner.default_container == NULL &&
          owner.active_container == NULL && owner.player == NULL &&
          !mc_local_player_terminal_sealed(player));
    CHECK(mc_player_terminal_inventory_admit(player, registry, world, NULL) == MC_ENTITY_OK);
    memset(&owner, 0xa5, sizeof(owner));
    unsigned char sentinel[sizeof(owner)];
    memcpy(sentinel, &owner, sizeof(owner));
    CHECK(!mc_player_terminal_inventory_inspect(NULL, &owner) &&
          memcmp(&owner, sentinel, sizeof(owner)) == 0 &&
          !mc_player_terminal_inventory_inspect(player, NULL) &&
          !mc_local_player_terminal_sealed(NULL));
    CHECK(refused(f, NULL, registry, world, slot, MC_ENTITY_INVALID_ARGUMENT) &&
          refused(f, player, NULL, world, slot, MC_ENTITY_INVALID_ARGUMENT) &&
          refused(f, player, registry, NULL, slot, MC_ENTITY_INVALID_ARGUMENT) &&
          refused(f, player, registry, world, NULL, MC_ENTITY_INVALID_ARGUMENT) &&
          refused(f, player, registry, world, &player->local_player->fields.player.e,
                  MC_ENTITY_INVALID_ARGUMENT) &&
          refused(f, player, registry, world, &player->local_player->fields.player.d,
                  MC_ENTITY_INVALID_ARGUMENT));

    mc_entity *raw = NULL, *ordinary = NULL;
    CHECK(mc_local_player_allocate(f->base.app.entities, &raw, NULL) == MC_ENTITY_OK &&
          mc_falling_entity_allocate(f->base.app.entities, &ordinary, NULL) == MC_ENTITY_OK &&
          mc_falling_entity_construct_world(ordinary, NULL, NULL) == MC_ENTITY_OK &&
          refused(f, raw, registry, world, slot, MC_ENTITY_INVALID_PHASE) &&
          refused(f, ordinary, registry, world, slot, MC_ENTITY_INVALID_ARGUMENT));
    mc_entity_release(raw);
    mc_entity_release(ordinary);

    mc_registry_classes *foreign = NULL;
    const mc_registry_options options = {MC_REGISTRY_CLIENT, f->base.app.profile,
                                         f->base.app.materials, 0,
                                         mc_resource_files_source(f->base.app.files)};
    CHECK(mc_registry_classes_allocate(&options, &foreign, NULL) == MC_REGISTRY_OK &&
          refused(f, player, foreign, world, slot, MC_ENTITY_INVALID_ARGUMENT) &&
          foreign->blocks_phase == MC_REGISTRY_COLD && foreign->entities == NULL &&
          mc_registry_classes_free(foreign, NULL) == MC_REGISTRY_OK);

    mc_world_runtime *other_world = NULL;
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA,
                                          f->base.app.world_classes, NULL, &other_world,
                                          NULL) == MC_WORLD_STORAGE_OK &&
          refused(f, player, registry, other_world, slot, MC_ENTITY_INVALID_ARGUMENT) &&
          mc_world_runtime_free(other_world, NULL) == MC_WORLD_STORAGE_OK);

    /* Existing legal Container rights refuse before any ownership move. */
    CHECK(mc_container_retain(actual_it, NULL) == MC_INVENTORY_OK &&
          refused(f, player, registry, world, slot, MC_ENTITY_BUSY) &&
          mc_container_release(actual_it, NULL) == MC_INVENTORY_OK &&
          mc_container_retain(actual_aa, NULL) == MC_INVENTORY_OK &&
          refused(f, player, registry, world, slot, MC_ENTITY_BUSY) &&
          mc_container_release(actual_aa, NULL) == MC_INVENTORY_OK);
    mc_container *extra_bound = NULL;
    CHECK(mc_container_workbench_create(MC_REGISTRY_CLIENT, registry, actual_ix, world, 1, 2, 3,
                                        NULL, &extra_bound, NULL) == MC_INVENTORY_OK &&
          actual_ix->containers == 3 && refused(f, player, registry, world, slot, MC_ENTITY_BUSY) &&
          mc_container_retire(&extra_bound, NULL) == MC_INVENTORY_OK && actual_ix->containers == 2);

    /* A second real owned IT uses a different genuine IX, not a copied owner
     * claim and not a second binding of the Player's soon-to-be-sealed IX. */
    mc_inventory_player *other_ix = NULL;
    mc_container *other_it = NULL;
    CHECK(mc_inventory_player_create(MC_REGISTRY_CLIENT, NULL, NULL, &other_ix, NULL) ==
              MC_INVENTORY_OK &&
          mc_container_workbench_create(MC_REGISTRY_CLIENT, registry, other_ix, world, 1, 2, 3,
                                        NULL, &other_it, NULL) == MC_INVENTORY_OK);
    CHECK(mc_entity_retain(player) == MC_ENTITY_OK); /* Real surviving external Player hold. */
    terminal_player_observation before;
    CHECK(observe(f, &before));
    mc_player_terminal_inventory_error error;
    CHECK(mc_player_terminal_inventory_adopt(player, registry, world, slot, &error) ==
              MC_ENTITY_OK &&
          error.status == MC_ENTITY_OK && error.operation == NULL &&
          error.inventory.frame_count == 0 && *slot == NULL && same_source(f, &before, false) &&
          mc_player_terminal_inventory_inspect(player, &owner) && owner.sealed &&
          owner.inventory == actual_ix && owner.default_container == actual_aa &&
          owner.active_container == actual_it && owner.player == player &&
          mc_local_player_terminal_sealed(player));
    CHECK(refused(f, player, registry, world, &other_it, MC_ENTITY_BUSY) && other_it != NULL &&
          mc_container_retire(&other_it, NULL) == MC_INVENTORY_OK &&
          mc_inventory_player_retire(&other_ix, NULL) == MC_INVENTORY_OK);
    CHECK(observe(f, &before) &&
          mc_player_terminal_inventory_admit(player, registry, world, &error) == MC_ENTITY_OK &&
          error.status == MC_ENTITY_OK &&
          mc_player_terminal_inventory_admit(player, registry, world, NULL) == MC_ENTITY_OK &&
          mc_entity_retain(player) == MC_ENTITY_BUSY &&
          mc_local_player_add_statistic(player, NULL, 1, NULL) ==
              MC_PLAYER_STATISTIC_ENTITY_FAILED &&
          same_source(f, &before, true));
    mc_local_player_snapshot snapshot;
    CHECK(mc_local_player_get_snapshot(player, &snapshot) == MC_ENTITY_OK &&
          snapshot.player.c == actual_ix && snapshot.player.d == actual_aa &&
          snapshot.player.e == actual_it && player->phase == MC_ENTITY_READY &&
          f->screen->container.id.j == actual_it);

    /* The trusted caller ends only its transferred native owning flag. This
     * models the private GUI adapter, never a Source j/e/d/c/r store or close. */
    f->screen->container.container_owned = false;
    CHECK(mc_client_screen_workbench_retire(&f->screen, NULL) == MC_CLIENT_SCREEN_OK &&
          f->screen == NULL &&
          mc_client_screen_classes_release(&f->screens, NULL) == MC_CLIENT_SCREEN_OK &&
          mc_client_singleplayer_controller_free(&f->base.app.controller, NULL) ==
              MC_SINGLEPLAYER_CONTROLLER_OK &&
          mc_math_classes_release(&f->math, NULL) == MC_MATH_CLASS_OK &&
          player->local_player->fields.player.e == actual_it &&
          mc_player_terminal_inventory_admit(player, registry, world, NULL) == MC_ENTITY_OK);
    mc_entity_release(player); /* End the actual external hold after the Screen is gone. */
    const size_t live = f->base.app.entities->live_entities, bindings = world->persistent_bindings;
    mc_entity_release(f->base.player); /* Infallible sealed IT -> AA -> IX disposal. */
    f->base.player = NULL;
    mc_item_stack_fields payload;
    CHECK(f->base.app.entities->live_entities + 1 == live &&
          world->persistent_bindings + 1 == bindings &&
          mc_item_stack_fields_get(f->payload, &payload) == MC_ITEM_STACK_OK && payload.id == 3 &&
          payload.count == 7 && payload.damage == 5);
    mc_item_stack_release(f->payload);
    f->payload = NULL;
    CHECK(teardown(&f->base));
    free(f);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3 || CD(argv[2]) != 0 || !mc_math_trig_init(&trig))
        return 2;
    const bool passed = controls(argv[1]);
    printf("Player terminal Inventory: %zu checks, %s\n", checks, passed ? "pass" : "fail");
    return passed ? 0 : 1;
}
