/* Actual XQ activation over genuine GAME/Store/Chunk/Player/Tile owners.
 * Authored loaded cells below precede the calls; no placement/orientation body
 * is invoked or claimed. Callback controls observe only the Player operand. */
#define MC_COLLISION_BOXES_MAIN mc_dispenser_activation_unused_boxes_main
#include "../collision/test_boxes.c"
#undef MC_COLLISION_BOXES_MAIN

#include "mc/entity/player/player.h"
#include "mc/inventory/dispenser.h"
#include "mc/statistics/registry/registry.h"
#include "mc/world/runtime/placement.h"
#include "mc/world/runtime/tile_entities/dispenser.h"

typedef struct dispenser_observer {
    mc_entity *player;
    mc_tile_entity *tile;
    mc_item_stack *first, *last;
    mc_inventory_dispenser *held;
    size_t calls;
    bool nullable, refuse, retain, busy;
} dispenser_observer;

static mc_world_storage_status observe_dispenser(void *context, mc_entity *player,
                                                 mc_inventory_dispenser *inventory,
                                                 mc_world_placement_error *error) {
    dispenser_observer *o = context;
    ++o->calls;
    bool valid = player == o->player && error->dispenser_tile_returned &&
                 error->dispenser_cast_returned && error->dispenser_entered &&
                 !error->dispenser_returned && error->site.pc == 26;
    if (o->nullable)
        valid = valid && inventory == NULL;
    else {
        int32_t size = -1, limit = -1;
        const char *name = NULL;
        mc_tile_entity *actual = NULL;
        const mc_item_stack *first = NULL, *last = NULL;
        valid =
            valid && inventory != NULL &&
            mc_inventory_dispenser_size(inventory, &size, NULL) == MC_INVENTORY_OK && size == 9 &&
            mc_inventory_dispenser_limit(inventory, &limit, NULL) == MC_INVENTORY_OK &&
            limit == 64 && mc_inventory_dispenser_name(inventory, &name, NULL) == MC_INVENTORY_OK &&
            name != NULL && strcmp(name, "Trap") == 0 &&
            mc_inventory_dispenser_tile_borrow(inventory, &actual, NULL) == MC_INVENTORY_OK &&
            actual == o->tile &&
            mc_inventory_dispenser_borrow(inventory, 0, &first, NULL) == MC_INVENTORY_OK &&
            first == o->first &&
            mc_inventory_dispenser_borrow(inventory, 8, &last, NULL) == MC_INVENTORY_OK &&
            last == o->last;
    }
    o->busy =
        !mc_world_placement_error_retire(error) && error->cleanup.status == MC_WORLD_STORAGE_BUSY;
    valid = valid && o->busy;
    if (valid && o->retain) {
        valid =
            o->held == NULL && mc_inventory_dispenser_retain(inventory, NULL) == MC_INVENTORY_OK;
        if (valid)
            o->held = inventory;
    }
    if (!valid || o->refuse) {
        error->status = valid ? MC_WORLD_STORAGE_UNSUPPORTED_STATE : MC_WORLD_STORAGE_INVALID_DATA;
        error->operation = valid ? "authored Dispenser destination refusal"
                                 : "actual XQ Player Dispenser operand contract";
        error->native_policy = true;
        return error->status;
    }
    return MC_WORLD_STORAGE_OK;
}

static bool activation_controls(fixture *f, mc_entity *player) {
    bool ok = false;
    mc_world_runtime *world = f->worlds[0];
    mc_world_runtime_classes *foreign_target = NULL;
    const mc_block *block = NULL;
    mc_tile_entity *tile = NULL, *wrong = NULL;
    mc_item_stack *first = NULL, *last = NULL;
    mc_world_placement_error e = {0};
    mc_live_tile_entity_error te;
    dispenser_observer o = {.player = player};
    mc_world_block_activation_context context = {
        .context = &o, .block_code_classes = f->classes[0], .dispenser = observe_dispenser};
    const bool remote = world->fields.B;
    CHECK(mc_registry_block_get(f->registry[0], 23, &block, NULL) == MC_REGISTRY_OK &&
          block != NULL);
    bool value = true;
    CHECK(mc_world_block_activate_with_context(f->registry[0], block, NULL, 8, 64, 8, NULL,
                                               &context, &value, &e) != MC_WORLD_STORAGE_OK &&
          !e.returned && e.source_exception && !e.native_policy && e.site.pc == 1 &&
          !e.remote_read && !e.dispenser_tile_entered && o.calls == 0 && value &&
          mc_world_placement_error_retire(&e));
    world->fields.B = true;
    value = false;
    CHECK(mc_world_block_activate_with_context(f->registry[0], block, world, 8, 64, 8, NULL, NULL,
                                               &value, &e) == MC_WORLD_STORAGE_OK &&
          e.returned && value && e.remote_read && e.remote && e.site.pc == 8 &&
          !e.dispenser_tile_entered && !e.dispenser_entered && o.calls == 0 &&
          mc_world_placement_error_retire(&e));
    world->fields.B = false;

    boxes_air(f, 0);
    o.nullable = true;
    value = false;
    CHECK(mc_world_block_activate_with_context(f->registry[0], block, world, 8, 64, 8, player,
                                               &context, &value, &e) == MC_WORLD_STORAGE_OK &&
          value && e.returned && !e.remote && e.query_count == 1 && e.acquisition == e.queries[0] &&
          e.dispenser_inventory == NULL && e.dispenser_returned && e.site.pc == 30 &&
          o.calls == 1 && o.busy && mc_world_placement_error_retire(&e));
    value = true;
    CHECK(mc_world_block_activate_with_context(f->registry[0], block, world, 8, 64, 8, NULL,
                                               &context, &value, &e) != MC_WORLD_STORAGE_OK &&
          !e.returned && e.dispenser_tile_returned && e.dispenser_cast_returned &&
          !e.dispenser_entered && e.source_exception && !e.native_policy && e.site.pc == 26 &&
          o.calls == 1 && value && mc_world_placement_error_retire(&e));

    f->states[0]->data->blocks[boxes_index(8, 64, 8)] = 23;
    CHECK(mc_live_tile_entities_new(world, MC_TILE_ENTITY_DISPENSER, &tile, &te) ==
              MC_WORLD_STORAGE_OK &&
          mc_live_tile_entities_set(world, 8, 64, 8, tile, &te) == MC_WORLD_STORAGE_OK &&
          mc_item_stack_allocate(&first) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(first, 262, 7, 3) == MC_ITEM_STACK_OK &&
          mc_item_stack_allocate(&last) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(last, 264, 2, 1) == MC_ITEM_STACK_OK &&
          mc_tile_entity_dispenser_set(tile, 0, first) == MC_TILE_ENTITY_OK &&
          mc_tile_entity_dispenser_set(tile, 8, last) == MC_TILE_ENTITY_OK);
    o.nullable = false;
    o.tile = tile;
    o.first = first;
    o.last = last;
    o.retain = true;
    value = false;
    CHECK(mc_world_block_activate_with_context(f->registry[0], block, world, 8, 64, 8, player,
                                               &context, &value, &e) == MC_WORLD_STORAGE_OK &&
          value && e.returned && e.dispenser_returned && e.site.pc == 30 &&
          e.dispenser_inventory == o.held && o.held != NULL && o.calls == 2 &&
          mc_world_placement_error_retire(&e));
    mc_tile_entity *borrowed = NULL;
    CHECK(mc_inventory_dispenser_tile_borrow(o.held, &borrowed, NULL) == MC_INVENTORY_OK &&
          borrowed == tile && mc_inventory_dispenser_retire(&o.held, NULL) == MC_INVENTORY_OK);

    o.retain = false;
    o.refuse = true;
    value = true;
    CHECK(mc_world_block_activate_with_context(f->registry[0], block, world, 8, 64, 8, player,
                                               &context, &value,
                                               &e) == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          !e.returned && !e.dispenser_returned && e.dispenser_entered && e.native_policy &&
          !e.source_exception && e.site.pc == 26 && e.caller.pc == 26 && o.calls == 3 && value &&
          mc_world_placement_error_retire(&e));
    o.refuse = false;
    mc_world_block_activation_context missing = context;
    missing.dispenser = NULL;
    CHECK(mc_world_block_activate_with_context(f->registry[0], block, world, 8, 64, 8, player,
                                               &missing, &value, &e) != MC_WORLD_STORAGE_OK &&
          e.dispenser_tile_returned && e.dispenser_cast_returned && !e.dispenser_entered &&
          e.site.pc == 26 && e.native_policy && !e.source_exception && o.calls == 3 &&
          mc_world_placement_error_retire(&e));
    missing = context;
    missing.block_code_classes = NULL;
    CHECK(mc_world_block_activate_with_context(f->registry[0], block, world, 8, 64, 8, player,
                                               &missing, &value, &e) != MC_WORLD_STORAGE_OK &&
          e.dispenser_tile_returned && !e.dispenser_cast_returned && !e.dispenser_entered &&
          e.site.pc == 17 && e.native_policy && !e.source_exception && o.calls == 3 &&
          mc_world_placement_error_retire(&e));

    /* Same Registry and concrete AZ kind, but distinct canonical Class owners.
     * A resolved checkcast compares identities without initializing AZ. */
    CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile, &trig, &foreign_target,
                                          NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_classes_bind_registry(foreign_target, f->registry[0], NULL) ==
              MC_REGISTRY_OK);
    missing = context;
    missing.block_code_classes = foreign_target;
    value = true;
    CHECK(mc_world_block_activate_with_context(f->registry[0], block, world, 8, 64, 8, player,
                                               &missing, &value, &e) != MC_WORLD_STORAGE_OK &&
          e.dispenser_tile_returned && !e.dispenser_cast_returned && !e.dispenser_entered &&
          e.source_exception && !e.native_policy && e.site.pc == 17 && e.exception_class != NULL &&
          strcmp(e.exception_class, "java/lang/ClassCastException") == 0 && o.calls == 3 && value &&
          mc_world_placement_error_retire(&e));
    CHECK(mc_world_runtime_classes_free(foreign_target, NULL) == MC_WORLD_STORAGE_OK);
    foreign_target = NULL;

    CHECK(mc_live_tile_entities_new(world, MC_TILE_ENTITY_SPAWNER, &wrong, &te) ==
              MC_WORLD_STORAGE_OK &&
          mc_live_tile_entities_set(world, 8, 64, 8, wrong, &te) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_block_activate_with_context(f->registry[0], block, world, 8, 64, 8, player,
                                               &context, &value, &e) != MC_WORLD_STORAGE_OK &&
          e.dispenser_tile_returned && !e.dispenser_cast_returned && !e.dispenser_entered &&
          e.source_exception && !e.native_policy && e.site.pc == 17 &&
          strcmp(e.exception_class, "java/lang/ClassCastException") == 0 && o.calls == 3 &&
          mc_world_placement_error_retire(&e));
    ok = true;
cleanup:
    (void)mc_world_placement_error_retire(&e);
    if (mc_world_runtime_classes_free(foreign_target, NULL) != MC_WORLD_STORAGE_OK)
        ok = false;
    (void)mc_inventory_dispenser_retire(&o.held, NULL);
    mc_item_stack_release(first);
    mc_item_stack_release(last);
    mc_tile_entity_release(tile);
    mc_tile_entity_release(wrong);
    world->fields.B = remote;
    return ok;
}

int main(int argc, char **argv) {
    int result = 1;
    fixture *f = NULL;
    mc_entity *player = NULL;
    mc_player_session_classes *sessions = NULL;
    mc_player_session *session = NULL;
    mc_player_string *username = NULL;
    char root[8192];
    const uint16_t title[] = {'D', 'i', 's', 'p', 'e', 'n', 's', 'e', 'r'};
    const uint16_t name[] = {'D', 'i', 's', 'p', 'e', 'n', 's', 'e', 'r', 'P'};
    if (argc != 3 || strcmp(argv[1], "world-dispenser-activation") != 0 ||
        !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 1;
    f = calloc(1, sizeof(*f));
    CHECK(f != NULL && mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK &&
          mc_materials_create(&f->materials) &&
          mc_java_random_profile_create(8u * 1024u * 1024u, &f->profile) ==
              MC_JAVA_RANDOM_PROFILE_OK &&
          mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
              MC_WORLD_INFO_OK);
    resources *r = &f->sources[0];
    r->streams[0] = (stream){.bytes = language, .length = sizeof(language) - 1u};
    r->streams[1] = r->streams[2] = (stream){.bytes = language};
    r->api = (mc_registry_resources){r, open_resource, close_resource, ready_resource};
    const mc_registry_options options = {MC_REGISTRY_CLIENT, f->profile, f->materials, 0, &r->api};
    CHECK(mc_registry_classes_allocate(&options, &f->registry[0], NULL) == MC_REGISTRY_OK &&
          mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile, &trig, &f->classes[0],
                                          NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_classes_bind_registry(f->classes[0], f->registry[0], NULL) ==
              MC_REGISTRY_OK &&
          mc_entity_classes_create(MC_ENTITY_CLIENT, f->profile, NULL, &f->entity_classes[0]) ==
              MC_ENTITY_OK &&
          mc_world_runtime_classes_bind_entities(f->classes[0], f->entity_classes[0]) ==
              MC_ENTITY_OK &&
          mc_registry_classes_initialize_blocks(f->registry[0], NULL) == MC_REGISTRY_OK &&
          boxes_open_store(f, 0, root) && boxes_create_world(f, 0) &&
          mc_registry_classes_initialize_statistics(f->registry[0], NULL) == MC_REGISTRY_OK &&
          mc_player_session_classes_create(f->entity_classes[0], f->registry[0], &sessions, NULL) ==
              MC_PLAYER_OK &&
          mc_player_string_create(name, COUNT(name), &username) == MC_PLAYER_OK &&
          mc_player_session_create(sessions, username, NULL, &session, NULL) == MC_PLAYER_OK &&
          mc_local_player_allocate(f->entity_classes[0], &player, NULL) == MC_ENTITY_OK &&
          mc_local_player_construct(player, f->worlds[0], NULL, session, 0, NULL, NULL) ==
              MC_PLAYER_OK &&
          activation_controls(f, player));
    result = 0;
cleanup:
    mc_entity_release(player);
    (void)mc_player_session_retire(&session);
    mc_player_string_release(username);
    if (mc_player_session_classes_free(&sessions, NULL) != MC_PLAYER_OK || !cleanup_fixture(f)) {
        fputs("Dispenser activation cleanup refused\n", stderr);
        return 1;
    }
    if (result == 0)
        printf("Dispenser activation CPU controls passed: %u\n", checks);
    return result;
}
