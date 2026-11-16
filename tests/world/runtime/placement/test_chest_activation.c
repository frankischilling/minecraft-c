/* Actual GAME/Store/Chunk/TileEntity inputs at E's activation boundary.
 * This tests the World body and its synchronous GS.a(LW) operand, not a GUI. */
#define MC_COLLISION_BOXES_MAIN mc_chest_unused_boxes_main
#include "../collision/test_boxes.c"
#undef MC_COLLISION_BOXES_MAIN

#include "mc/entity/player/player.h"
#include "mc/inventory/chest.h"
#include "mc/world/runtime/placement.h"
#include "mc/world/runtime/tile_entities/tile_entities.h"

enum { CHEST_CENTER = 0, CHEST_WEST, CHEST_EAST, CHEST_NORTH, CHEST_SOUTH, CHEST_CELLS };

typedef struct chest_inputs {
    fixture *base;
    mc_entity *player;
    mc_tile_entity *tiles[CHEST_CELLS];
    mc_item_stack *first[CHEST_CELLS], *last[CHEST_CELLS];
} chest_inputs;

typedef struct chest_observer {
    mc_entity *player;
    mc_inventory_chest *held;
    const mc_item_stack *first[2], *last[2];
    int32_t size;
    size_t calls;
    bool retain, refuse, busy;
} chest_observer;

static const int32_t chest_xs[CHEST_CELLS] = {8, 7, 9, 8, 8};
static const int32_t chest_zs[CHEST_CELLS] = {8, 8, 8, 7, 9};

static void chest_cells(chest_inputs *inputs, int neighbor) {
    fixture *f = inputs->base;
    boxes_air(f, 0);
    /* Authored loaded Chunk bytes, before the invocation. Actual World
     * getters and lazy TileEntity construction are never replaced. */
    f->states[0]->data->blocks[boxes_index(8, 64, 8)] = 54;
    if (neighbor > 0)
        f->states[0]->data->blocks[boxes_index(chest_xs[neighbor], 64, chest_zs[neighbor])] = 54;
}

static bool chest_get_is(mc_inventory_chest *inventory, int32_t index,
                         const mc_item_stack *expected) {
    mc_item_stack *held = NULL;
    const mc_inventory_status status = mc_inventory_chest_get(inventory, index, &held, NULL);
    const bool same = status == MC_INVENTORY_OK && held == expected;
    mc_item_stack_release(held);
    return same;
}

static mc_world_storage_status chest_capture(void *context, mc_entity *player,
                                             mc_inventory_chest *inventory,
                                             mc_world_placement_error *error) {
    chest_observer *o = context;
    ++o->calls;
    int32_t size = -1;
    const char *name = NULL;
    bool valid = player == o->player && inventory != NULL &&
                 mc_inventory_chest_size(inventory, &size, NULL) == MC_INVENTORY_OK &&
                 size == o->size &&
                 mc_inventory_chest_name(inventory, &name, NULL) == MC_INVENTORY_OK &&
                 name != NULL && strcmp(name, size == 27 ? "Chest" : "Large chest") == 0;
    for (int32_t group = 0; valid && group < size / 27; ++group)
        valid = group < 2 && chest_get_is(inventory, group * 27, o->first[group]) &&
                chest_get_is(inventory, group * 27 + 26, o->last[group]);
    /* This real synchronous callback cannot consume its active parent. */
    o->busy =
        !mc_world_placement_error_retire(error) && error->cleanup.status == MC_WORLD_STORAGE_BUSY;
    valid = valid && o->busy;
    if (valid && o->retain) {
        valid = o->held == NULL && mc_inventory_chest_retain(inventory, NULL) == MC_INVENTORY_OK;
        if (valid)
            o->held = inventory;
    }
    if (!valid || o->refuse) {
        error->status = valid ? MC_WORLD_STORAGE_UNSUPPORTED_STATE : MC_WORLD_STORAGE_INVALID_DATA;
        error->operation = valid ? "authored Chest destination refusal"
                                 : "Chest actual GS.a inventory operand contract";
        error->native_policy = true;
        return error->status;
    }
    return MC_WORLD_STORAGE_OK;
}

static bool chest_prepare(chest_inputs *inputs) {
    bool ok = false;
    fixture *f = inputs->base;
    mc_live_tile_entity_error error;
    boxes_air(f, 0);
    for (size_t i = 0; i < CHEST_CELLS; ++i) {
        f->states[0]->data->blocks[boxes_index(chest_xs[i], 64, chest_zs[i])] = 54;
        CHECK(mc_live_tile_entities_get(f->worlds[0], chest_xs[i], 64, chest_zs[i],
                                        &inputs->tiles[i], &error) == MC_WORLD_STORAGE_OK &&
              inputs->tiles[i] != NULL);
        mc_tile_entity_snapshot state;
        CHECK(mc_tile_entity_get_snapshot(inputs->tiles[i], &state) == MC_TILE_ENTITY_OK &&
              state.kind == MC_TILE_ENTITY_CHEST && state.world == f->worlds[0] &&
              state.x == chest_xs[i] && state.y == 64 && state.z == chest_zs[i]);
        CHECK(
            mc_item_stack_allocate(&inputs->first[i]) == MC_ITEM_STACK_OK &&
            mc_item_stack_construct(inputs->first[i], 262, (int32_t)i + 3, (int32_t)i + 40) ==
                MC_ITEM_STACK_OK &&
            mc_item_stack_allocate(&inputs->last[i]) == MC_ITEM_STACK_OK &&
            mc_item_stack_construct(inputs->last[i], 264, (int32_t)i + 1, 0) == MC_ITEM_STACK_OK &&
            mc_tile_entity_chest_set(inputs->tiles[i], 0, inputs->first[i]) == MC_TILE_ENTITY_OK &&
            mc_tile_entity_chest_set(inputs->tiles[i], 26, inputs->last[i]) == MC_TILE_ENTITY_OK);
    }
    ok = true;
cleanup:
    return ok;
}

static bool chest_open_controls(chest_inputs *inputs) {
    bool ok = false;
    fixture *f = inputs->base;
    mc_world_runtime *world = f->worlds[0];
    const mc_block *block = NULL;
    mc_world_placement_error e = {0};
    chest_observer o = {0};
    const mc_world_block_activation_context context = {.context = &o, .chest = chest_capture};
    CHECK(mc_registry_block_get(f->registry[0], 54, &block, NULL) == MC_REGISTRY_OK &&
          block != NULL);
    for (int neighbor = 0; neighbor < CHEST_CELLS; ++neighbor) {
        chest_cells(inputs, neighbor);
        const bool prepend = neighbor == CHEST_WEST || neighbor == CHEST_NORTH;
        const size_t first = prepend ? (size_t)neighbor : CHEST_CENTER;
        const size_t second = prepend ? CHEST_CENTER : (size_t)neighbor;
        o = (chest_observer){.player = inputs->player,
                             .first = {inputs->first[first], inputs->first[second]},
                             .last = {inputs->last[first], inputs->last[second]},
                             .size = neighbor == 0 ? 27 : 54,
                             .retain = true};
        const size_t bindings = world->persistent_bindings;
        bool value = false;
        CHECK(mc_world_block_activate_with_context(f->registry[0], block, world, 8, 64, 8,
                                                   inputs->player, &context, &value,
                                                   &e) == MC_WORLD_STORAGE_OK &&
              value && e.returned && e.chest_entered && e.chest_returned && !e.chest_blocked &&
              o.calls == 1 && o.busy && o.held == e.chest_inventory && e.remote_read && !e.remote &&
              e.site.pc == 353 && world->persistent_bindings == bindings + 1);
        /* The destination's same LW occurrence outlives the original local
         * variables and the entire placement call, with Stack identity intact. */
        CHECK(mc_world_placement_error_retire(&e) && world->persistent_bindings == bindings &&
              chest_get_is(o.held, 0, inputs->first[first]) &&
              chest_get_is(o.held, o.size - 1, inputs->last[neighbor == 0 ? first : second]) &&
              mc_inventory_chest_retire(&o.held, NULL) == MC_INVENTORY_OK && o.held == NULL);
    }
    ok = true;
cleanup:
    (void)mc_world_placement_error_retire(&e);
    (void)mc_inventory_chest_retire(&o.held, NULL);
    return ok;
}

static bool chest_blocked_controls(chest_inputs *inputs) {
    bool ok = false;
    fixture *f = inputs->base;
    mc_world_runtime *world = f->worlds[0];
    const mc_block *block = NULL;
    mc_world_placement_error e = {0};
    CHECK(mc_registry_block_get(f->registry[0], 54, &block, NULL) == MC_REGISTRY_OK &&
          block != NULL);
    const size_t bindings = world->persistent_bindings;
    for (int neighbor = 0; neighbor < CHEST_CELLS; ++neighbor) {
        chest_cells(inputs, neighbor);
        f->states[0]->data->blocks[boxes_index(chest_xs[neighbor], 65, chest_zs[neighbor])] = 3;
        bool value = false;
        CHECK(mc_world_block_activate(f->registry[0], block, world, 8, 64, 8, NULL, &value, &e) ==
                  MC_WORLD_STORAGE_OK &&
              value && e.returned && e.chest_blocked && !e.remote_read && !e.chest_entered &&
              mc_world_placement_error_retire(&e) && world->persistent_bindings == bindings);
    }
    /* A solid neighbor's top does not block this Chest if that neighbor is
     * not another Chest. Glass is nonopaque and does not block its own top. */
    chest_cells(inputs, -1);
    f->states[0]->data->blocks[boxes_index(7, 64, 8)] = 3;
    f->states[0]->data->blocks[boxes_index(7, 65, 8)] = 3;
    f->states[0]->data->blocks[boxes_index(8, 65, 8)] = 20;
    bool value = false;
    CHECK(mc_world_block_activate(f->registry[0], block, world, 8, 64, 8, NULL, &value, &e) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          !value && !e.returned && !e.chest_blocked && e.remote_read && !e.remote &&
          !e.chest_entered && e.source_exception && !e.native_policy && e.site.pc == 349 &&
          e.exception_class != NULL &&
          strcmp(e.exception_class, "java/lang/NullPointerException") == 0 &&
          mc_world_placement_error_retire(&e) && world->persistent_bindings == bindings);
    /* The first TileEntity getter/cast precedes even the first obstruction.
     * Observe real lazy construction through the native Map, before a later
     * getter could create the missing receiver itself. */
    boxes_air(f, 0);
    f->states[0]->data->blocks[boxes_index(11, 64, 8)] = 54;
    f->states[0]->data->blocks[boxes_index(11, 65, 8)] = 3;
    const size_t maps = f->chunks[0]->l->count;
    value = false;
    CHECK(mc_world_block_activate(f->registry[0], block, world, 11, 64, 8, NULL, &value, &e) ==
              MC_WORLD_STORAGE_OK &&
          value && e.chest_blocked && !e.remote_read && f->chunks[0]->l->count == maps + 1 &&
          mc_world_placement_error_retire(&e));
    ok = true;
cleanup:
    (void)mc_world_placement_error_retire(&e);
    return ok;
}

static bool chest_remote_and_failure_controls(chest_inputs *inputs) {
    bool ok = false;
    fixture *f = inputs->base;
    mc_world_runtime *world = f->worlds[0];
    const bool original_remote = world->fields.B;
    const mc_block *block = NULL;
    mc_world_placement_error e = {0};
    chest_observer o = {.player = inputs->player,
                        .first = {inputs->first[0], inputs->first[CHEST_EAST]},
                        .last = {inputs->last[0], inputs->last[CHEST_EAST]},
                        .size = 54};
    const mc_world_block_activation_context context = {.context = &o, .chest = chest_capture};
    CHECK(mc_registry_block_get(f->registry[0], 54, &block, NULL) == MC_REGISTRY_OK &&
          block != NULL);
    chest_cells(inputs, CHEST_EAST);
    world->fields.B = true; /* Declared source input, outside the call. */
    bool value = false;
    CHECK(mc_world_block_activate_with_context(f->registry[0], block, world, 8, 64, 8, NULL,
                                               &context, &value, &e) == MC_WORLD_STORAGE_OK &&
          value && e.returned && e.remote_read && e.remote && !e.chest_entered && o.calls == 0 &&
          e.chest_inventory != NULL && chest_get_is(e.chest_inventory, 0, inputs->first[0]) &&
          chest_get_is(e.chest_inventory, 27, inputs->first[CHEST_EAST]) &&
          mc_world_placement_error_retire(&e));
    world->fields.B = false;
    value = false;
    CHECK(mc_world_block_activate_with_context(f->registry[0], block, world, 8, 64, 8, NULL,
                                               &context, &value,
                                               &e) == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          !value && !e.returned && e.remote_read && !e.remote && !e.chest_entered && o.calls == 0 &&
          e.source_exception && e.site.pc == 349 && mc_world_placement_error_retire(&e));
    o.refuse = o.retain = true;
    const size_t bindings = world->persistent_bindings;
    value = false;
    CHECK(mc_world_block_activate_with_context(f->registry[0], block, world, 8, 64, 8,
                                               inputs->player, &context, &value,
                                               &e) == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          !value && !e.returned && e.chest_entered && !e.chest_returned && o.calls == 1 && o.busy &&
          e.caller.pc == 349 && e.operation != NULL &&
          strcmp(e.operation, "authored Chest destination refusal") == 0 &&
          mc_world_placement_error_retire(&e) && world->persistent_bindings == bindings &&
          chest_get_is(o.held, 0, inputs->first[0]) &&
          mc_inventory_chest_retire(&o.held, NULL) == MC_INVENTORY_OK);
    value = true;
    CHECK(mc_world_block_activate(f->registry[0], block, NULL, 8, 64, 8, NULL, &value, &e) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          value && !e.returned && e.source_exception && !e.native_policy && e.site.pc == 5 &&
          mc_world_placement_error_retire(&e));
    ok = true;
cleanup:
    (void)mc_world_placement_error_retire(&e);
    (void)mc_inventory_chest_retire(&o.held, NULL);
    world->fields.B = original_remote;
    return ok;
}

static bool chest_cast_controls(chest_inputs *inputs) {
    bool ok = false;
    fixture *f = inputs->base;
    mc_world_runtime *world = f->worlds[0];
    const bool remote = world->fields.B;
    const mc_block *block = NULL;
    mc_tile_entity *spawner = NULL;
    mc_live_tile_entity_error tile;
    mc_world_placement_error e = {0};
    CHECK(mc_registry_block_get(f->registry[0], 54, &block, NULL) == MC_REGISTRY_OK &&
          block != NULL);
    chest_cells(inputs, -1);
    f->states[0]->data->blocks[boxes_index(8, 65, 8)] = 3;
    CHECK(mc_live_tile_entities_new(world, MC_TILE_ENTITY_SPAWNER, &spawner, &tile) ==
              MC_WORLD_STORAGE_OK &&
          mc_live_tile_entities_set(world, 8, 64, 8, spawner, &tile) == MC_WORLD_STORAGE_OK);
    world->fields.B = true;
    bool value = false;
    CHECK(mc_world_block_activate(f->registry[0], block, world, 8, 64, 8, NULL, &value, &e) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          !value && !e.returned && !e.chest_blocked && !e.remote_read && !e.chest_entered &&
          e.source_exception && !e.native_policy && e.site.pc == 8 && e.exception_class != NULL &&
          strcmp(e.exception_class, "java/lang/ClassCastException") == 0 &&
          mc_world_placement_error_retire(&e));
    ok = true;
cleanup:
    (void)mc_world_placement_error_retire(&e);
    mc_tile_entity_release(spawner);
    world->fields.B = remote;
    return ok;
}

int main(int argc, char **argv) {
    int result = 1;
    fixture *f = NULL;
    chest_inputs inputs = {0};
    mc_player_session_classes *sessions = NULL;
    mc_player_session *session = NULL;
    mc_player_string *username = NULL;
    char root[8192];
    const uint16_t title[] = {'C', 'h', 'e', 's', 't'}, name[] = {'C', 'h', 'e', 's', 't', 'P'};
    if (argc != 3 || strcmp(argv[1], "world-chest-activation") != 0 ||
        !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 1;
    f = calloc(1, sizeof(*f));
    inputs.base = f;
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
          mc_local_player_allocate(f->entity_classes[0], &inputs.player, NULL) == MC_ENTITY_OK &&
          mc_local_player_construct(inputs.player, f->worlds[0], NULL, session, 0, NULL, NULL) ==
              MC_PLAYER_OK);
    CHECK(chest_prepare(&inputs) && chest_open_controls(&inputs) &&
          chest_blocked_controls(&inputs) && chest_remote_and_failure_controls(&inputs) &&
          chest_cast_controls(&inputs));
    result = 0;
cleanup:
    mc_entity_release(inputs.player);
    for (size_t i = 0; i < CHEST_CELLS; ++i) {
        mc_tile_entity_release(inputs.tiles[i]);
        mc_item_stack_release(inputs.first[i]);
        mc_item_stack_release(inputs.last[i]);
    }
    mc_player_session_retire(&session);
    mc_player_string_release(username);
    if (mc_player_session_classes_free(&sessions, NULL) != MC_PLAYER_OK || !cleanup_fixture(f)) {
        fputs("Chest activation cleanup refused; surviving owners retained\n", stderr);
        return 1;
    }
    if (result == 0)
        printf("PASS: %u World Chest activation/operand/ownership checks\n", checks);
    return result;
}
