/* Genuine GAME Registry/World/Store/Chunk constructors. The included collision
 * controls have a distinct entry point and are never executed by this test. */
#define MC_COLLISION_BOXES_MAIN mc_furnace_tick_unused_collision_main
#include "../collision/test_boxes.c"
#undef MC_COLLISION_BOXES_MAIN

#include "../../../../src/item/stack/source_internal.h"
#include "../../../../src/world/runtime/mutation/furnace/furnace_internal.h"
#include "../../../../src/world/runtime/mutation/mutation_internal.h"
#include "../../../../src/world/runtime/tile_entities/tile_entities_internal.h"
#include "mc/world/runtime/entity_tick.h"
#include "mc/world/runtime/tile_entities/furnace_tick.h"

static mc_item_stack *furnace_stack(int32_t id, int32_t count) {
    mc_item_stack *s = NULL;
    if (mc_item_stack_allocate(&s) != MC_ITEM_STACK_OK)
        return NULL;
    if (mc_item_stack_construct(s, id, count, 0) != MC_ITEM_STACK_OK) {
        mc_item_stack_release(s);
        return NULL;
    }
    return s;
}

static bool furnace_fields(mc_tile_entity *tile, int32_t burn, int32_t fuel, int32_t cook) {
    mc_furnace_fields actual;
    return mc_tile_entity_furnace_fields_get(tile, &actual) == MC_TILE_ENTITY_OK &&
           actual.burn_remaining == burn && actual.fuel_duration == fuel &&
           actual.cook_progress == cook;
}

static bool furnace_count(mc_tile_entity *tile, int32_t cell, int32_t id, int32_t count) {
    mc_item_stack *stack = NULL;
    if (mc_tile_entity_furnace_get(tile, cell, &stack) != MC_TILE_ENTITY_OK)
        return false;
    mc_item_stack_fields actual;
    const bool ok = stack != NULL && mc_item_stack_fields_get(stack, &actual) == MC_ITEM_STACK_OK &&
                    actual.id == id && actual.count == count;
    mc_item_stack_release(stack);
    return ok;
}

static bool furnace_open(fixture *f, const char *root) {
    bool ok = false;
    const uint16_t title[] = {'F', 'u', 'r', 'n', 'a', 'c', 'e'};
    CHECK(mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK &&
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
          boxes_open_store(f, 0, root) && boxes_create_world(f, 0));
    boxes_air(f, 0);
    ok = true;
cleanup:
    return ok;
}

static bool furnace_pre_world(fixture *f) {
    bool ok = false;
    mc_tile_entity *tile = NULL;
    mc_furnace_tick_error error = {0};
    CHECK(mc_tile_entity_allocate_in_classes(f->classes[0], MC_TILE_ENTITY_FURNACE, &tile) ==
              MC_TILE_ENTITY_OK &&
          mc_tile_entity_construct(tile) == MC_TILE_ENTITY_OK &&
          mc_tile_entity_furnace_field_set(tile, MC_FURNACE_BURN_REMAINING, 7) ==
              MC_TILE_ENTITY_OK);
    CHECK(mc_tile_entity_furnace_tick(tile, &error) == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          error.source_null && error.bytecode == 36 && error.work == NULL &&
          furnace_fields(tile, 6, 0, 0) && mc_tile_entity_furnace_tick_retire(&error));
    CHECK(mc_tile_entity_furnace_tick(tile, NULL) == MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          furnace_fields(tile, 6, 0, 0));
    ok = true;
cleanup:
    (void)mc_tile_entity_furnace_tick_retire(&error);
    mc_tile_entity_release(tile);
    return ok;
}

static bool furnace_game(fixture *f) {
    bool ok = false;
    mc_tile_entity *tile = NULL, *mapped = NULL;
    mc_item_stack *ore = NULL, *coal = NULL;
    mc_world_mutation_error *mutation = calloc(1, sizeof(*mutation));
    mc_world_entity_tick_error *tick = calloc(1, sizeof(*tick));
    mc_live_tile_entity_error tile_error;
    mc_furnace_tick_error direct = {0};
    mc_world_runtime *world = f->worlds[0];
    CHECK(mutation != NULL && tick != NULL);
    bool changed = false;
    CHECK(mc_world_runtime_block_metadata_set(world, 8, 64, 8, 61, 0, &changed, mutation) ==
              MC_WORLD_STORAGE_OK &&
          changed);
    int32_t metadata = -1;
    CHECK(mc_world_runtime_metadata_get(world, 8, 64, 8, &metadata, NULL) == MC_WORLD_STORAGE_OK &&
          metadata == 3 &&
          mc_live_tile_entities_get(world, 8, 64, 8, &tile, &tile_error) == MC_WORLD_STORAGE_OK &&
          tile != NULL && tile->kind == MC_TILE_ENTITY_FURNACE && !tile->invalid);
    CHECK(mc_world_runtime_metadata_set(world, 8, 64, 8, 5, mutation) == MC_WORLD_STORAGE_OK);
    /* A captured UT singleton and the live cell's actual SK are distinct
     * source operands. Its private instanceof must return ordinary NULL. */
    const mc_block *moving = NULL;
    mc_entity_bounds *shape = NULL;
    mc_world_collision_error *shape_error = malloc(sizeof(*shape_error));
    CHECK(shape_error != NULL &&
          mc_registry_block_static_get(f->registry[0], "ad", &moving, NULL) == MC_REGISTRY_OK &&
          moving != NULL);
    *shape_error = mc_world_collision_empty_error();
    const mc_world_storage_status shaped = mc_world_collision_block_box_in_classes_held(
        f->classes[0], world, moving, 8, 64, 8, &shape, shape_error);
    free(shape_error);
    CHECK(shaped == MC_WORLD_STORAGE_OK && shape == NULL && !tile->invalid);
    ore = furnace_stack(15, 2);
    coal = furnace_stack(263, 2);
    CHECK(ore != NULL && coal != NULL &&
          mc_tile_entity_furnace_set(tile, 0, ore) == MC_TILE_ENTITY_OK &&
          mc_tile_entity_furnace_set(tile, 1, coal) == MC_TILE_ENTITY_OK);
    const mc_world_storage_status ignition =
        mc_world_runtime_tick_entities(world, NULL, NULL, tick);
    if (ignition != MC_WORLD_STORAGE_OK)
        fprintf(stderr, "Furnace ignition status=%d operation=%s child=%d tile-pc=%u\n", ignition,
                tick->operation == NULL ? "(none)" : tick->operation, tick->child,
                tick->tile_bytecode);
    CHECK(ignition == MC_WORLD_STORAGE_OK && furnace_fields(tile, 1600, 1600, 1) &&
          furnace_count(tile, 1, 263, 1) && !world->fields.L && world->fields.G->count == 0 &&
          !f->registry[0]->tc_c &&
          mc_live_tile_entities_get(world, 8, 64, 8, &mapped, &tile_error) == MC_WORLD_STORAGE_OK &&
          mapped == tile && !tile->invalid &&
          f->states[0]->data->blocks[boxes_index(8, 64, 8)] == 62 &&
          mc_world_runtime_metadata_get(world, 8, 64, 8, &metadata, NULL) == MC_WORLD_STORAGE_OK &&
          metadata == 5);
    mc_tile_entity_release(mapped);
    mapped = NULL;
    CHECK(mc_world_runtime_tick_entities(world, NULL, NULL, tick) == MC_WORLD_STORAGE_OK &&
          furnace_fields(tile, 1599, 1600, 2));
    for (int32_t i = 0; i < 198; ++i)
        CHECK(mc_world_runtime_tick_entities(world, NULL, NULL, tick) == MC_WORLD_STORAGE_OK);
    CHECK(furnace_fields(tile, 1401, 1600, 0) && furnace_count(tile, 0, 15, 1) &&
          furnace_count(tile, 2, 265, 1) && furnace_count(tile, 1, 263, 1) &&
          world->fields.c->count == 2 && world->fields.G->count == 0 &&
          world->fields.c->items[0] == tile && world->fields.c->items[1] != tile &&
          !((mc_tile_entity *)world->fields.c->items[1])->invalid);
    /* Explicit input changes isolate ordinary blocked output/no-input behavior;
     * real tick decides the countdown/reset/dirty behavior afterward. */
    CHECK(mc_tile_entity_furnace_set(tile, 0, NULL) == MC_TILE_ENTITY_OK &&
          mc_tile_entity_furnace_field_set(tile, MC_FURNACE_COOK_PROGRESS, 67) ==
              MC_TILE_ENTITY_OK &&
          mc_tile_entity_furnace_tick(tile, &direct) == MC_WORLD_STORAGE_OK &&
          direct.work == NULL && furnace_fields(tile, 1400, 1600, 0));
    CHECK(mc_tile_entity_furnace_field_set(tile, MC_FURNACE_BURN_REMAINING, 1) ==
              MC_TILE_ENTITY_OK &&
          mc_world_runtime_tick_entities(world, NULL, NULL, tick) == MC_WORLD_STORAGE_OK &&
          furnace_fields(tile, 0, 1600, 0) &&
          f->states[0]->data->blocks[boxes_index(8, 64, 8)] == 61 &&
          mc_live_tile_entities_get(world, 8, 64, 8, &mapped, &tile_error) == MC_WORLD_STORAGE_OK &&
          mapped == tile && !tile->invalid && !f->registry[0]->tc_c);
    mc_tile_entity_release(mapped);
    mapped = NULL;
    CHECK(mc_world_runtime_tick_entities(world, NULL, NULL, tick) == MC_WORLD_STORAGE_OK &&
          world->fields.c->count == 3 && world->fields.G->count == 0 &&
          world->fields.c->items[0] == tile &&
          !((mc_tile_entity *)world->fields.c->items[1])->invalid &&
          !((mc_tile_entity *)world->fields.c->items[2])->invalid);
    ok = true;
cleanup:
    if (tick != NULL && !mc_world_entity_tick_error_retire(tick))
        ok = false;
    if (!mc_tile_entity_furnace_tick_retire(&direct))
        ok = false;
    mc_tile_entity_release(mapped);
    mc_tile_entity_release(tile);
    mc_item_stack_release(ore);
    mc_item_stack_release(coal);
    free(mutation);
    free(tick);
    return ok;
}

typedef struct furnace_observer {
    unsigned int reached;
} furnace_observer;

static mc_world_storage_status furnace_fail_cell(void *context, mc_world_runtime *world, int32_t x,
                                                 int32_t y, int32_t z,
                                                 mc_world_storage_error *error) {
    furnace_observer *observer = context;
    (void)world;
    (void)x;
    (void)y;
    (void)z;
    ++observer->reached;
    *error = (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR,
                                      "authored Furnace orientation observer failure", 0};
    return MC_WORLD_STORAGE_IO_ERROR;
}

static mc_world_storage_status furnace_range(void *context, mc_world_runtime *world, int32_t x,
                                             int32_t y, int32_t z, int32_t x1, int32_t y1,
                                             int32_t z1, mc_world_storage_error *error) {
    (void)context;
    (void)world;
    (void)x;
    (void)y;
    (void)z;
    (void)x1;
    (void)y1;
    (void)z1;
    (void)error;
    return MC_WORLD_STORAGE_OK;
}

static bool furnace_failed_switch(fixture *f) {
    bool ok = false;
    mc_world_runtime *world = f->worlds[0];
    mc_tile_entity *tile = NULL;
    mc_item_stack *ore = furnace_stack(15, 1), *coal = furnace_stack(263, 1);
    mc_live_tile_entity_error child;
    mc_world_entity_tick_error *tick = calloc(1, sizeof(*tick));
    mc_world_access *access = NULL;
    furnace_observer observer = {0};
    const mc_world_access_callbacks callbacks = {.cell = furnace_fail_cell, .range = furnace_range};
    CHECK(tick != NULL && ore != NULL && coal != NULL &&
          mc_live_tile_entities_get(world, 8, 64, 8, &tile, &child) == MC_WORLD_STORAGE_OK &&
          tile != NULL && mc_tile_entity_furnace_set(tile, 0, ore) == MC_TILE_ENTITY_OK &&
          mc_tile_entity_furnace_set(tile, 1, coal) == MC_TILE_ENTITY_OK &&
          mc_world_access_create(&callbacks, &observer, &access, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(world, access, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_tick_entities(world, NULL, NULL, tick) == MC_WORLD_STORAGE_IO_ERROR &&
          tick->child == MC_WORLD_ENTITY_TICK_CHILD_FURNACE &&
          tick->child_error.furnace.bytecode == 226 && tick->child_error.furnace.work != NULL &&
          !tick->child_error.furnace.returned && observer.reached == 1 && world->fields.L &&
          f->registry[0]->tc_c && f->states[0]->data->blocks[boxes_index(8, 64, 8)] == 62 &&
          furnace_fields(tile, 1600, 1600, 1) && tile->invalid);
    const size_t active = world->fields.c->count, deferred = world->fields.G->count;
    const mc_furnace_fields source = {tile->state.furnace.burn_remaining,
                                      tile->state.furnace.fuel_duration,
                                      tile->state.furnace.cook_progress};
    CHECK(mc_world_entity_tick_error_retire(tick) && world->fields.L && f->registry[0]->tc_c &&
          world->fields.c->count == active && world->fields.G->count == deferred && tile->invalid &&
          furnace_fields(tile, source.burn_remaining, source.fuel_duration, source.cook_progress) &&
          observer.reached == 1);
    ok = true;
cleanup:
    if (tick != NULL && !mc_world_entity_tick_error_retire(tick))
        ok = false;
    bool removed = false;
    if (access != NULL && (mc_world_runtime_remove_world_access(world, access, &removed, NULL) !=
                               MC_WORLD_STORAGE_OK ||
                           !removed || mc_world_access_free(access, NULL) != MC_WORLD_STORAGE_OK))
        ok = false;
    mc_tile_entity_release(tile);
    mc_item_stack_release(ore);
    mc_item_stack_release(coal);
    free(tick);
    return ok;
}

#ifndef MC_FURNACE_TICK_MAIN
#define MC_FURNACE_TICK_MAIN main
#endif
int MC_FURNACE_TICK_MAIN(int argc, char **argv) {
    int code = 1;
    fixture *f = NULL;
    char root[8192];
    if (argc != 3 || strcmp(argv[1], "world-furnace-tick") != 0 ||
        !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 1;
    f = calloc(1, sizeof(*f));
    CHECK(f != NULL && furnace_open(f, root) && furnace_pre_world(f) && furnace_game(f) &&
          furnace_failed_switch(f));
    code = 0;
cleanup:
    if (!cleanup_fixture(f)) {
        fputs("Furnace tick cleanup refused; surviving owners retained\n", stderr);
        return 1;
    }
    if (code == 0)
        printf("PASS: %u actual World Furnace tick, same-Tile and deferred-list checks\n", checks);
    return code;
}
