/* Constructed native-root controls for the World collector. Actual World,
 * LocalPlayer, CD, JS and OG owners; no HP/ID.b execution or whole-App terminal
 * authority is claimed. Player.e below is an explicit fixture source input. */
#define MC_COLLISION_BOXES_MAIN mc_chest_terminal_unused_boxes_main
#include "../collision/test_boxes.c"
#undef MC_COLLISION_BOXES_MAIN

#include "entity/player/player_internal.h"
#include "entity/player/terminal_inventory.h"
#include "inventory/chest_internal.h"
#include "mc/entity/player/player.h"
#include "world/runtime/terminal_inventory_internal.h"
#include "world/runtime/tile_entities/tile_entities_internal.h"

typedef enum chest_terminal_case {
    CHEST_TERMINAL_SINGLE,
    CHEST_TERMINAL_DOUBLE,
    CHEST_TERMINAL_SHARED,
    CHEST_TERMINAL_FOREIGN
} chest_terminal_case;

typedef struct chest_terminal_fixture {
    fixture *base;
    mc_entity *player;
    mc_container *cd, *active;
    mc_inventory_chest *root, *leaves[2], *extra_root, *extra_leaf;
    mc_tile_entity *tiles[2];
    mc_item_stack *payload;
} chest_terminal_fixture;

typedef struct chest_terminal_observation {
    mc_local_player_snapshot player;
    mc_java_random player_random, world_random;
    mc_item_stack_fields payload;
    inv_chest_node_view nodes[4];
    size_t node_count, player_holds, world_bindings;
    mc_container *source_container;
} chest_terminal_observation;

static bool chest_terminal_player(chest_terminal_fixture *f) {
    bool ok = false;
    mc_player_session_classes *sessions = NULL;
    mc_player_session *session = NULL;
    mc_player_string *username = NULL;
    const uint16_t name[] = {'C', 'h', 'e', 's', 't', 'R', 'o', 'o', 't'};
    fixture *base = f->base;
    CHECK(mc_registry_classes_initialize_statistics(base->registry[0], NULL) == MC_REGISTRY_OK &&
          mc_player_session_classes_create(base->entity_classes[0], base->registry[0], &sessions,
                                           NULL) == MC_PLAYER_OK &&
          mc_player_string_create(name, COUNT(name), &username) == MC_PLAYER_OK &&
          mc_player_session_create(sessions, username, NULL, &session, NULL) == MC_PLAYER_OK &&
          mc_local_player_allocate(base->entity_classes[0], &f->player, NULL) == MC_ENTITY_OK &&
          mc_local_player_construct(f->player, base->worlds[0], NULL, session, 0, NULL, NULL) ==
              MC_PLAYER_OK);
    ok = true;
cleanup:
    mc_player_session_retire(&session);
    mc_player_string_release(username);
    if (mc_player_session_classes_free(&sessions, NULL) != MC_PLAYER_OK)
        ok = false;
    return ok;
}

static bool chest_terminal_open(chest_terminal_fixture *f, const char *root,
                                chest_terminal_case kind) {
    bool ok = false;
    const uint16_t title[] = {'C', 'h', 'e', 's', 't', 'R', 'o', 'o', 't'};
    f->base = calloc(1, sizeof(*f->base));
    fixture *base = f->base;
    CHECK(base != NULL &&
          mc_world_regions_create(NULL, &base->regions, NULL) == MC_WORLD_STORAGE_OK &&
          mc_materials_create(&base->materials) &&
          mc_java_random_profile_create(8u * 1024u * 1024u, &base->profile) ==
              MC_JAVA_RANDOM_PROFILE_OK &&
          mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &base->name) ==
              MC_WORLD_INFO_OK);
    resources *r = &base->sources[0];
    r->streams[0] = (stream){.bytes = language, .length = sizeof(language) - 1u};
    r->streams[1] = r->streams[2] = (stream){.bytes = language};
    r->api = (mc_registry_resources){r, open_resource, close_resource, ready_resource};
    const mc_registry_options options = {MC_REGISTRY_CLIENT, base->profile, base->materials, 0,
                                         &r->api};
    CHECK(mc_registry_classes_allocate(&options, &base->registry[0], NULL) == MC_REGISTRY_OK &&
          mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, base->profile, &trig,
                                          &base->classes[0], NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_classes_bind_registry(base->classes[0], base->registry[0], NULL) ==
              MC_REGISTRY_OK &&
          mc_entity_classes_create(MC_ENTITY_CLIENT, base->profile, NULL,
                                   &base->entity_classes[0]) == MC_ENTITY_OK &&
          mc_world_runtime_classes_bind_entities(base->classes[0], base->entity_classes[0]) ==
              MC_ENTITY_OK &&
          mc_registry_classes_initialize_blocks(base->registry[0], NULL) == MC_REGISTRY_OK &&
          boxes_open_store(base, 0, root) && boxes_create_world(base, 0) &&
          chest_terminal_player(f));
    if (kind == CHEST_TERMINAL_FOREIGN)
        CHECK(boxes_open_store(base, 2, root) && boxes_create_world(base, 2));
    const size_t tile_world = kind == CHEST_TERMINAL_FOREIGN ? 2u : 0u;
    const size_t tile_count = kind == CHEST_TERMINAL_DOUBLE ? 2u : 1u;
    boxes_air(base, tile_world);
    mc_live_tile_entity_error tile_error;
    for (size_t i = 0; i < tile_count; ++i) {
        const int32_t x = 8 + (int32_t)i;
        base->states[tile_world]->data->blocks[boxes_index(x, 64, 8)] = 54;
        CHECK(mc_live_tile_entities_get(base->worlds[tile_world], x, 64, 8, &f->tiles[i],
                                        &tile_error) == MC_WORLD_STORAGE_OK &&
              f->tiles[i] != NULL &&
              mc_inventory_chest_from_tile(MC_REGISTRY_CLIENT, f->tiles[i], NULL, &f->leaves[i],
                                           NULL) == MC_INVENTORY_OK);
    }
    CHECK(mc_item_stack_allocate(&f->payload) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(f->payload, 264, 7, 11) == MC_ITEM_STACK_OK &&
          mc_tile_entity_chest_set(f->tiles[0], 0, f->payload) == MC_TILE_ENTITY_OK &&
          mc_inventory_player_set(f->player->local_player->fields.player.c, 5, f->payload, NULL) ==
              MC_INVENTORY_OK);
    if (kind == CHEST_TERMINAL_DOUBLE || kind == CHEST_TERMINAL_SHARED) {
        CHECK(mc_inventory_chest_large_create(MC_REGISTRY_CLIENT, "Large chest", f->leaves[0],
                                              kind == CHEST_TERMINAL_SHARED ? f->leaves[0]
                                                                            : f->leaves[1],
                                              NULL, &f->root, NULL) == MC_INVENTORY_OK);
    } else {
        f->root = f->leaves[0]; /* Move this actual native owning occurrence. */
        f->leaves[0] = NULL;
    }
    if (kind == CHEST_TERMINAL_DOUBLE) {
        /* These existing rights must remain visible after the graph seals.
         * They are not manufactured by changing an owner count. */
        CHECK(mc_inventory_chest_retain(f->root, NULL) == MC_INVENTORY_OK &&
              mc_inventory_chest_retain(f->leaves[0], NULL) == MC_INVENTORY_OK);
        f->extra_root = f->root;
        f->extra_leaf = f->leaves[0];
    }
    CHECK(mc_container_chest_create(MC_REGISTRY_CLIENT, base->registry[0],
                                    f->player->local_player->fields.player.c, f->root, NULL, &f->cd,
                                    NULL) == MC_INVENTORY_OK);
    f->active = f->cd;
    /* Explicit constructed native-root input, not an emulated ID.b call.
     * cd is the distinct actual native owning slot supplied to the adopter. */
    f->player->local_player->fields.player.e = f->cd;
    for (size_t i = 0; i < COUNT(f->leaves); ++i) {
        mc_inventory_chest_release(f->leaves[i]);
        f->leaves[i] = NULL;
        mc_tile_entity_release(f->tiles[i]);
        f->tiles[i] = NULL;
    }
    mc_inventory_chest_release(f->root);
    f->root = NULL;
    /* End the genuine caller's provider occurrence, retaining World.t. */
    mc_world_provider_release(base->dimensions[0]);
    base->dimensions[0] = NULL;
    CHECK(mc_world_provider_hold_count(base->worlds[0]->fields.t) == 1);
    ok = true;
cleanup:
    return ok;
}

static bool chest_terminal_observe(chest_terminal_fixture *f, chest_terminal_observation *out) {
    bool ok = false;
    memset(out, 0, sizeof(*out));
    memcpy(&out->player, &f->player->local_player->fields, sizeof(out->player));
    CHECK(mc_item_stack_fields_get(f->payload, &out->payload) == MC_ITEM_STACK_OK &&
          inv_chest_node_count(f->active->chest, &out->node_count, NULL) == MC_INVENTORY_OK &&
          out->node_count <= COUNT(out->nodes));
    for (size_t i = 0; i < out->node_count; ++i)
        CHECK(inv_chest_node_at(f->active->chest, i, &out->nodes[i], NULL) == MC_INVENTORY_OK);
    memcpy(&out->player_random, f->player->random, sizeof(out->player_random));
    memcpy(&out->world_random, f->base->worlds[0]->fields.r, sizeof(out->world_random));
    out->source_container = f->player->local_player->fields.player.e;
    out->player_holds = f->player->holds;
    out->world_bindings = f->base->worlds[0]->persistent_bindings;
    ok = true;
cleanup:
    return ok;
}

static bool chest_terminal_same(chest_terminal_fixture *f,
                                const chest_terminal_observation *before) {
    chest_terminal_observation after;
    if (!chest_terminal_observe(f, &after) || before->node_count != after.node_count)
        return false;
    for (size_t i = 0; i < before->node_count; ++i) {
        const inv_chest_node_view *a = &before->nodes[i], *b = &after.nodes[i];
        if (a->owner != b->owner || a->left != b->left || a->right != b->right ||
            a->tile != b->tile || a->native_holds != b->native_holds || a->large != b->large ||
            a->active != b->active || a->terminal_sealed != b->terminal_sealed ||
            a->phase != b->phase)
            return false;
    }
    return memcmp(&before->player, &after.player, sizeof(after.player)) == 0 &&
           mc_test_java_random_equal(&before->player_random, &after.player_random) &&
           mc_test_java_random_equal(&before->world_random, &after.world_random) &&
           memcmp(&before->payload, &after.payload, sizeof(after.payload)) == 0 &&
           before->source_container == after.source_container &&
           before->player_holds == after.player_holds &&
           before->world_bindings == after.world_bindings;
}

static bool chest_terminal_attempt(chest_terminal_fixture *f, mc_world_storage_status expected,
                                   const char *reason, mc_world_terminal_inventory *success) {
    bool ok = false;
    chest_terminal_observation before;
    CHECK(chest_terminal_observe(f, &before));
    mc_world_terminal_inventory inventory;
    memset(&inventory, 0xa5, sizeof(inventory));
    unsigned char sentinel[sizeof(inventory)];
    memcpy(sentinel, &inventory, sizeof(inventory));
    const mc_world_terminal_entity_anchor entity = {&f->player};
    const mc_world_terminal_anchors roots = {&entity, 1, NULL, 0};
    mc_world_storage_error error;
    CHECK(mc_world_runtime_terminal_admit_owned(f->base->worlds[0], &roots, NULL, &inventory,
                                                &error) == expected &&
          error.status == expected && chest_terminal_same(f, &before));
    if (expected == MC_WORLD_STORAGE_OK) {
        CHECK(error.operation == NULL);
        if (success != NULL)
            *success = inventory;
    } else {
        CHECK(error.operation != NULL && (reason == NULL || strcmp(error.operation, reason) == 0) &&
              memcmp(sentinel, &inventory, sizeof(inventory)) == 0);
    }
    ok = true;
cleanup:
    return ok;
}

static bool chest_terminal_close(chest_terminal_fixture *f) {
    mc_inventory_chest_release(f->extra_root);
    mc_inventory_chest_release(f->extra_leaf);
    f->extra_root = f->extra_leaf = NULL;
    for (size_t i = 0; i < COUNT(f->leaves); ++i) {
        mc_inventory_chest_release(f->leaves[i]);
        mc_tile_entity_release(f->tiles[i]);
    }
    mc_inventory_chest_release(f->root);
    if (f->cd != NULL && f->player != NULL && f->player->local_player->fields.player.e == f->cd) {
        /* A failed preparation keeps its original owner graph for inspection.
         * Do not repair a source alias to force destruction. */
        fputs("Chest terminal unadopted root retained after failure\n", stderr);
        return false;
    }
    if (mc_container_retire(&f->cd, NULL) != MC_INVENTORY_OK)
        return false;
    mc_entity_release(f->player);
    f->player = NULL;
    mc_item_stack_release(f->payload);
    f->payload = NULL;
    const bool closed = cleanup_fixture(f->base);
    f->base = NULL;
    return closed;
}

static bool chest_terminal_exercise(const char *root, chest_terminal_case kind) {
    bool ok = false;
    chest_terminal_fixture f = {0};
    CHECK(chest_terminal_open(&f, root, kind));
    chest_terminal_observation before;
    CHECK(chest_terminal_observe(&f, &before));
    mc_player_terminal_inventory_error detail;
    CHECK(mc_player_terminal_inventory_adopt(f.player, f.base->registry[0], f.base->worlds[0],
                                             &f.cd, &detail) == MC_ENTITY_OK &&
          f.cd == NULL && mc_local_player_terminal_sealed(f.player));
    /* Adoption changes native seals/owning slots only. Source fields, Stack
     * counts and entropy remain exact; the Player still publishes this CD. */
    mc_local_player_snapshot player_after;
    mc_item_stack_fields payload_after;
    memcpy(&player_after, &f.player->local_player->fields, sizeof(player_after));
    CHECK(memcmp(&before.player, &player_after, sizeof(player_after)) == 0 &&
          mc_item_stack_fields_get(f.payload, &payload_after) == MC_ITEM_STACK_OK &&
          memcmp(&before.payload, &payload_after, sizeof(payload_after)) == 0 &&
          mc_test_java_random_equal(&before.player_random, f.player->random) &&
          mc_test_java_random_equal(&before.world_random, f.base->worlds[0]->fields.r) &&
          f.player->local_player->fields.player.e == f.active &&
          mc_inventory_chest_retain(f.active->chest, NULL) == MC_INVENTORY_BUSY &&
          mc_entity_retain(f.player) == MC_ENTITY_BUSY);
    mc_world_terminal_inventory admitted;
    if (kind == CHEST_TERMINAL_FOREIGN) {
        CHECK(chest_terminal_attempt(&f, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                     "World terminal Chest Inventory actual World", NULL));
    } else {
        if (kind == CHEST_TERMINAL_DOUBLE) {
            CHECK(chest_terminal_attempt(&f, MC_WORLD_STORAGE_BUSY,
                                         "World terminal external Chest inventory hold", NULL) &&
                  mc_inventory_chest_retire(&f.extra_root, NULL) == MC_INVENTORY_OK &&
                  f.extra_root == NULL &&
                  chest_terminal_attempt(&f, MC_WORLD_STORAGE_BUSY,
                                         "World terminal external Chest inventory hold", NULL) &&
                  mc_inventory_chest_retire(&f.extra_leaf, NULL) == MC_INVENTORY_OK &&
                  f.extra_leaf == NULL);
        }
        CHECK(chest_terminal_attempt(&f, MC_WORLD_STORAGE_OK, NULL, &admitted) &&
              admitted.entities == 1 && admitted.entity_occurrences == 1 &&
              admitted.tile_entities == (kind == CHEST_TERMINAL_DOUBLE ? 2u : 1u) &&
              admitted.expected_world_bindings == admitted.tile_entities + 2u &&
              f.base->worlds[0]->persistent_bindings == admitted.expected_world_bindings);
    }
    /* Final native Player disposal releases its sealed CD/OG/JS graph. No
     * source Container close, Player.e repair, Tile removal or World detach. */
    mc_entity_release(f.player);
    f.player = NULL;
    f.active = NULL;
    CHECK(mc_item_stack_fields_get(f.payload, &payload_after) == MC_ITEM_STACK_OK &&
          payload_after.id == 264 && payload_after.count == 7 && payload_after.damage == 11 &&
          mc_world_runtime_terminal_admit(f.base->worlds[0], NULL, &admitted, NULL) ==
              MC_WORLD_STORAGE_OK &&
          admitted.entities == 0);
    ok = true;
cleanup:
    if (!chest_terminal_close(&f))
        ok = false;
    return ok;
}

int main(int argc, char **argv) {
    char root[8192], child[8192];
    if (argc != 3 || strcmp(argv[1], "world-chest-terminal") != 0 ||
        !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 1;
    for (int kind = CHEST_TERMINAL_SINGLE; kind <= CHEST_TERMINAL_FOREIGN; ++kind) {
        const int written = snprintf(child, sizeof(child), "%s/case%d", root, kind);
        if (written <= 0 || (size_t)written >= sizeof(child) ||
            !chest_terminal_exercise(child, (chest_terminal_case)kind))
            return 1;
    }
    printf("PASS: %u native World Chest terminal ownership checks\n", checks);
    return 0;
}
