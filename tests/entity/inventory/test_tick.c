/* Real GAME/Registry/Store/loaded-Chunk fixture; imported main is not run. */
#define MC_COLLISION_BOXES_MAIN mc_inventory_tick_boxes_main
#include "../../world/runtime/collision/test_boxes.c"
#undef MC_COLLISION_BOXES_MAIN
#include "../../../src/entity/player/player_internal.h"
#include "../../../src/inventory/internal.h"
#include "../../../src/item/stack/source_internal.h"
#include "mc/inventory/runtime/tick.h"
#include "mc/item/registry/runtime.h"
#include "mc/world/runtime/saved_data.h"
#include "mc/world/saved_data/storage.h"
#include <inttypes.h>

static bool stack_new(int32_t id, int32_t count, int32_t damage, int32_t animation,
                      mc_item_stack **out) {
    return mc_item_stack_allocate(out) == MC_ITEM_STACK_OK &&
           mc_item_stack_construct(*out, id, count, damage) == MC_ITEM_STACK_OK &&
           mc_item_stack_source_animation_set(*out, animation) == MC_ITEM_STACK_OK;
}

static bool fields(const mc_item_stack *s, int32_t animation) {
    mc_item_stack_fields f;
    return mc_item_stack_fields_get(s, &f) == MC_ITEM_STACK_OK && f.animation == animation;
}

static bool inventory_cases(fixture *f, mc_entity *player) {
    bool ok = false;
    mc_inventory_tick_error e;
    mc_item_stack *stack = NULL;
    mc_inventory_player *inventory = player->local_player->fields.player.c, *empty = NULL;
    CHECK(mc_inventory_player_create(MC_REGISTRY_CLIENT, NULL, NULL, &empty, NULL) ==
          MC_INVENTORY_OK);
    CHECK(mc_inventory_player_tick(empty, NULL, NULL, &e) == MC_INVENTORY_TICK_OK);
    CHECK(stack_new(262, 3, 7, 7, &stack));
    CHECK(mc_inventory_player_set(inventory, 0, stack, NULL) == MC_INVENTORY_OK &&
          mc_inventory_player_set(inventory, 5, stack, NULL) == MC_INVENTORY_OK &&
          mc_inventory_player_set(inventory, 36, stack, NULL) == MC_INVENTORY_OK);
    mc_item_stack **main = inventory->main_slots, **armor = inventory->armor_slots;
    inventory->changed = true;
    inventory->selected = -1;
    for (int c = 0; c < 4; ++c) {
        if (c == 2)
            CHECK(mc_inventory_player_set(inventory, 0, NULL, NULL) == MC_INVENTORY_OK);
        if (c == 3)
            CHECK(mc_inventory_player_set(inventory, 5, NULL, NULL) == MC_INVENTORY_OK);
        CHECK(mc_inventory_player_tick(inventory, f->registry[0], NULL, &e) ==
              MC_INVENTORY_TICK_OK);
        const int32_t animation = c == 0 ? 5 : c == 1 ? 3 : 2;
        CHECK(fields(stack, animation) && inventory->main_slots == main &&
              inventory->armor_slots == armor && inventory->armor_slots[0] == stack &&
              inventory->changed && inventory->selected == -1 && !inventory->active);
        printf("CMP INV %d %" PRId32 " %d %d %d\n", c, animation, inventory->main_slots[0] == stack,
               inventory->main_slots[5] == stack, inventory->armor_slots[0] == stack);
    }
    CHECK(mc_inventory_player_set(empty, 0, stack, NULL) == MC_INVENTORY_OK);
    CHECK(mc_item_stack_source_animation_set(stack, 9) == MC_ITEM_STACK_OK);
    CHECK(mc_inventory_player_tick(empty, f->registry[0], NULL, &e) ==
              MC_INVENTORY_TICK_NULL_REFERENCE &&
          e.inventory_bytecode == 30 && fields(stack, 9) && !empty->active);
    printf("CMP NULLPLAYER %d\n", 9);
    mc_item_stack **array = empty->main_slots;
    empty->main_slots = NULL;
    const mc_inventory_tick_status null_array = mc_inventory_player_tick(empty, NULL, NULL, &e);
    empty->main_slots = array;
    CHECK(null_array == MC_INVENTORY_TICK_NULL_REFERENCE && e.inventory_bytecode == 7 &&
          !empty->active);
    CHECK(mc_inventory_player_set(inventory, 0, stack, NULL) == MC_INVENTORY_OK);
    mc_world_runtime *world = player->base.world;
    player->base.world = NULL;
    const mc_inventory_tick_status null_world =
        mc_inventory_player_tick(inventory, f->registry[0], NULL, &e);
    player->base.world = world;
    CHECK(null_world == MC_INVENTORY_TICK_OK && fields(stack, 8));
    printf("CMP NULLWORLD %d\n", 8);
    CHECK(mc_inventory_player_set(inventory, 0, NULL, NULL) == MC_INVENTORY_OK &&
          mc_inventory_player_set(inventory, 36, NULL, NULL) == MC_INVENTORY_OK);
    ok = true;
cleanup:
    mc_item_stack_release(stack);
    if (mc_inventory_player_retire(&empty, NULL) != MC_INVENTORY_OK)
        ok = false;
    return ok;
}

static bool item_cases(fixture *f) {
    bool ok = false;
    mc_item_stack *stack = NULL;
    mc_inventory_tick_error e;
    const mc_registry_item_array *array;
    CHECK(mc_registry_item_c_get(f->registry[0], &array, NULL) == MC_REGISTRY_OK);
    const size_t length = mc_registry_item_array_length(array);
    size_t kinds = 0;
    for (size_t i = 0; i < length; ++i) {
        const mc_item *item = NULL;
        CHECK(mc_registry_item_array_get(array, i, &item, NULL) == MC_REGISTRY_OK);
        if (item == NULL || item->kind == MC_ITEM_MAP)
            continue;
        CHECK(stack_new((int32_t)i, -3, 0x13579, (int32_t)(i % 3), &stack));
        CHECK(mc_item_inventory_update(item, NULL, NULL, NULL, INT32_MIN, true, NULL, &e) ==
              MC_INVENTORY_TICK_OK);
        CHECK(mc_item_stack_inventory_update(stack, f->registry[0], NULL, NULL, INT32_MIN, true,
                                             NULL, &e) == MC_INVENTORY_TICK_OK);
        mc_item_stack_fields values;
        CHECK(mc_item_stack_fields_get(stack, &values) == MC_ITEM_STACK_OK);
        CHECK(values.animation == (int32_t)(i % 3 == 0 ? 0 : i % 3 - 1) && values.count == -3 &&
              values.damage == 0x13579);
        printf("CMP ITEM %zu %" PRId32 " %" PRId32 " %" PRId32 "\n", i, values.animation,
               values.count, values.damage);
        ++kinds;
        mc_item_stack_release(stack);
        stack = NULL;
    }
    CHECK(kinds > 100);
    const int32_t animations[] = {INT32_MIN, -1, 0, 1, INT32_MAX};
    for (size_t i = 0; i < COUNT(animations); ++i) {
        CHECK(stack_new(262, 1, 0, animations[i], &stack));
        CHECK(mc_item_stack_inventory_update(stack, f->registry[0], NULL, NULL, 0, false, NULL,
                                             &e) == MC_INVENTORY_TICK_OK);
        mc_item_stack_fields values;
        CHECK(mc_item_stack_fields_get(stack, &values) == MC_ITEM_STACK_OK);
        CHECK(values.animation == (animations[i] > 0 ? animations[i] - 1 : animations[i]));
        printf("CMP ANIMATION %zu %" PRId32 "\n", i, values.animation);
        mc_item_stack_release(stack);
        stack = NULL;
    }
    for (int c = 0; c < 4; ++c) {
        CHECK(stack_new(c == 0   ? -1
                        : c == 1 ? -1
                        : c == 2 ? 500
                                 : (int32_t)length,
                        1, 0, 2, &stack));
        mc_registry_item_array *current = f->registry[0]->items.c;
        if (c == 0)
            f->registry[0]->items.c = NULL;
        const mc_inventory_tick_status status =
            mc_item_stack_inventory_update(stack, f->registry[0], NULL, NULL, 0, false, NULL, &e);
        f->registry[0]->items.c = current;
        CHECK(status != MC_INVENTORY_TICK_OK && fields(stack, 1));
        CHECK(c == 2 ? e.stack_bytecode == 31 && status == MC_INVENTORY_TICK_NULL_REFERENCE
                     : e.stack_bytecode == 24 &&
                           e.registry.status == (c == 0 ? MC_REGISTRY_NULL_REFERENCE
                                                        : MC_REGISTRY_INDEX_OUT_OF_RANGE));
        printf("CMP FAILURE %d %d\n", c, 1);
        mc_item_stack_release(stack);
        stack = NULL;
    }
    CHECK(stack_new(262, 1, 0, 5, &stack));
    CHECK(mc_item_stack_inventory_update(stack, NULL, NULL, NULL, 0, false, NULL, &e) ==
              MC_INVENTORY_TICK_NAMESPACE &&
          e.stack_bytecode == 17 && fields(stack, 4) && e.animation_stored);
    ok = true;
cleanup:
    mc_item_stack_release(stack);
    return ok;
}

static bool name_equals(mc_world_info_name *name, const char *s) {
    const mc_nbt_string t = mc_world_info_name_text(name);
    if (t.length != strlen(s))
        return false;
    for (size_t i = 0; i < t.length; ++i)
        if (t.units[i] != (unsigned char)s[i])
            return false;
    return true;
}

static bool construction_prefix_controls(fixture *f) {
    bool ok = false;
    mc_item_stack *raw = NULL, *failed = NULL;
    mc_saved_data_value *value = NULL;
    const mc_registry_item_array *array = NULL;
    const mc_item *map = NULL;
    mc_inventory_tick_error e;
    mc_item_stack_phase phase;
    mc_item_stack_fields before, after;
    bool stored = false;
    CHECK(mc_registry_item_c_get(f->registry[0], &array, NULL) == MC_REGISTRY_OK &&
          mc_registry_item_c_at(f->registry[0], array, 358, &map, NULL) == MC_REGISTRY_OK);
    CHECK(mc_item_stack_allocate(&raw) == MC_ITEM_STACK_OK &&
          mc_item_stack_source_animation_set(raw, 3) == MC_ITEM_STACK_OK &&
          mc_item_stack_source_inventory_animation_tick(raw, &stored) == MC_ITEM_STACK_OK &&
          stored && mc_item_stack_source_damage_set(raw, 777) == MC_ITEM_STACK_OK);
    CHECK(mc_item_stack_fields_get(raw, &before) == MC_ITEM_STACK_OK && before.animation == 2 &&
          before.damage == 777);
    CHECK(mc_item_stack_damage_set(raw, -1) == MC_ITEM_STACK_INVALID_PHASE && fields(raw, 2));
    CHECK(mc_item_map_data_get(map, raw, f->worlds[0], NULL, &value, &e) == MC_INVENTORY_TICK_OK &&
          value != NULL);
    CHECK(mc_item_stack_fields_get(raw, &after) == MC_ITEM_STACK_OK && after.damage == 5 &&
          after.id == before.id && after.count == before.count &&
          after.animation == before.animation && mc_item_stack_source(raw) == NULL &&
          mc_item_stack_phase_get(raw, &phase) == MC_ITEM_STACK_OK && phase == MC_ITEM_STACK_RAW);
    mc_saved_data_value_release(value);
    value = NULL;
    const uint16_t id_name[] = {'i', 'd'}, count_name[] = {'C', 'o', 'u', 'n', 't'};
    const mc_nbt_tag id = {.type = MC_NBT_SHORT, .name = {id_name, 2}, .value.short_value = 358};
    const mc_nbt_tag bad_count = {
        .type = MC_NBT_STRING, .name = {count_name, 5}, .value.string = {NULL, 0}};
    const mc_nbt_tag *children[] = {&id, &bad_count};
    const mc_nbt_tag compound = {.type = MC_NBT_COMPOUND, .value.compound = {children, 2}};
    CHECK(mc_item_stack_allocate(&failed) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct_nbt(failed, &compound, NULL) == MC_ITEM_STACK_INVALID_FIELD);
    CHECK(mc_item_stack_source_damage_set(failed, 888) == MC_ITEM_STACK_OK &&
          mc_item_stack_damage_set(failed, -1) == MC_ITEM_STACK_INVALID_PHASE &&
          mc_item_stack_source_animation_set(failed, 3) == MC_ITEM_STACK_OK);
    CHECK(mc_item_map_data_get(map, failed, f->worlds[0], NULL, &value, &e) ==
              MC_INVENTORY_TICK_OK &&
          value != NULL);
    CHECK(mc_item_stack_fields_get(failed, &after) == MC_ITEM_STACK_OK && after.id == 358 &&
          after.count == 0 && after.damage == 6 && after.animation == 3 &&
          mc_item_stack_phase_get(failed, &phase) == MC_ITEM_STACK_OK &&
          phase == MC_ITEM_STACK_FAILED_CONSTRUCTION);
    CHECK(mc_item_stack_inventory_update(failed, f->registry[0], f->worlds[0], NULL, 0, false, NULL,
                                         &e) == MC_INVENTORY_TICK_OK &&
          fields(failed, 2) && mc_item_stack_phase_get(failed, &phase) == MC_ITEM_STACK_OK &&
          phase == MC_ITEM_STACK_FAILED_CONSTRUCTION);
    ok = true;
cleanup:
    mc_saved_data_value_release(value);
    mc_item_stack_release(raw);
    mc_item_stack_release(failed);
    return ok;
}

static bool map_row(int c, mc_item_stack *stack, mc_saved_data_value *value,
                    mc_world_saved_data *manager, bool same) {
    mc_item_stack_fields sf;
    mc_saved_data_map_fields m;
    mc_world_saved_data_view v;
    if (mc_item_stack_fields_get(stack, &sf) != MC_ITEM_STACK_OK ||
        mc_saved_data_map_fields_get(value, &m) != MC_WORLD_STORAGE_OK ||
        mc_world_saved_data_inspect(manager, &v) != MC_WORLD_STORAGE_OK)
        return false;
    printf("CMP MAP %d %" PRId32 " %" PRId32 " %" PRId32 " %d %d %d %zu %zu %zu %zu %d %d\n", c,
           sf.damage, m.x_center, m.z_center, (int)m.scale, (int)m.dimension, m.dirty,
           mc_nbt_byte_array_length(m.colors), v.named_count, v.ordered_count, v.id_count, same,
           m.trackers != m.decorations && m.trackers != m.player_trackers &&
               m.decorations != m.player_trackers);
    return true;
}

static bool map_cases(fixture *f, mc_entity *player) {
    bool ok = false;
    mc_item_stack *stack = NULL;
    mc_saved_data_value *first = NULL, *second = NULL;
    mc_entity *falling = NULL;
    mc_item_stack *later = NULL;
    mc_inventory_tick_error e;
    mc_world_runtime *world = f->worlds[0];
    const mc_item *map = NULL;
    const mc_registry_item_array *array = NULL;
    char *directory = NULL;
    mc_world_saved_data *loaded = NULL;
    CHECK(mc_registry_item_c_get(f->registry[0], &array, NULL) == MC_REGISTRY_OK &&
          mc_registry_item_c_at(f->registry[0], array, 358, &map, NULL) == MC_REGISTRY_OK &&
          map->kind == MC_ITEM_MAP);
    CHECK(mc_ws_path_join(mc_world_store_root(f->stores[0]), "data", &directory, NULL) ==
              MC_WORLD_STORAGE_OK &&
          mc_ws_mkdirs(directory, NULL) == MC_WORLD_STORAGE_OK);
    free(directory);
    directory = NULL;
    CHECK(stack_new(358, 1, 73, 4, &stack));
    world->fields.B = true;
    const mc_inventory_tick_status remote_null =
        mc_item_inventory_update(map, NULL, world, NULL, 0, true, NULL, &e);
    const mc_inventory_tick_status remote_stack =
        mc_item_stack_inventory_update(stack, f->registry[0], world, NULL, 0, true, NULL, &e);
    world->fields.B = false;
    CHECK(remote_null == MC_INVENTORY_TICK_OK && remote_stack == MC_INVENTORY_TICK_OK &&
          fields(stack, 3));
    printf("CMP REMOTE %d\n", 3);
    CHECK(mc_world_info_spawn_set(mc_world_runtime_info(world), 17, 64, -33) == MC_WORLD_INFO_OK);
    CHECK(mc_item_map_data_get(map, stack, world, NULL, &first, &e) == MC_INVENTORY_TICK_OK &&
          first != NULL && name_equals(mc_saved_data_value_name(first), "map_0"));
    CHECK(map_row(0, stack, first, world->fields.z, false));
    CHECK(mc_saved_data_value_dirty_set(first, false) == MC_WORLD_STORAGE_OK);
    CHECK(mc_item_map_data_get(map, stack, world, NULL, &second, &e) == MC_INVENTORY_TICK_OK &&
          second == first);
    CHECK(map_row(1, stack, second, world->fields.z, true));
    mc_saved_data_value_release(second);
    second = NULL;
    CHECK(mc_item_stack_damage_set(stack, INT32_MIN) == MC_ITEM_STACK_OK &&
          mc_world_info_spawn_set(mc_world_runtime_info(world), 99, 64, 101) == MC_WORLD_INFO_OK);
    CHECK(mc_item_map_data_get(map, stack, world, NULL, &second, &e) == MC_INVENTORY_TICK_OK &&
          second != first && name_equals(mc_saved_data_value_name(second), "map_1"));
    CHECK(map_row(2, stack, second, world->fields.z, false));
    /* Real MapData lookup prefix, followed by the exact missing tracker call. */
    CHECK(mc_item_stack_inventory_update(stack, f->registry[0], world, player, 4, false, NULL,
                                         &e) == MC_INVENTORY_TICK_UNIMPLEMENTED_DEPENDENCY &&
          e.item_bytecode == 34 && e.tracker_bytecode == 0 && e.pending_map == second &&
          fields(stack, 2));
    mc_saved_data_map_fields m;
    CHECK(mc_saved_data_map_fields_get(e.pending_map, &m) == MC_WORLD_STORAGE_OK &&
          m.tracker_count == 0 && m.player_tracker_count == 0);
    CHECK(mc_falling_entity_allocate(f->entity_classes[0], &falling, NULL) == MC_ENTITY_OK &&
          mc_falling_entity_construct(falling, world, 8, 8, 8, 1, NULL) == MC_ENTITY_OK);
    CHECK(mc_item_inventory_update(map, stack, world, falling, 0, false, NULL, &e) ==
          MC_INVENTORY_TICK_OK);
    mc_item_stack_fields unselected;
    CHECK(mc_item_stack_fields_get(stack, &unselected) == MC_ITEM_STACK_OK &&
          unselected.animation == 2);
    printf("CMP NONPLAYER %" PRId32 "\n", unselected.animation);
    CHECK(mc_item_inventory_update(map, stack, world, falling, 0, true, NULL, &e) ==
              MC_INVENTORY_TICK_UNIMPLEMENTED_DEPENDENCY &&
          e.item_bytecode == 47);
    /* Actual owned Player inventory, with a genuine earlier alias and later
     * cell. Reached Map tracking stops the source loop before that later cell. */
    mc_inventory_player *inventory = player->local_player->fields.player.c;
    CHECK(stack_new(262, 1, 0, 7, &later) &&
          mc_item_stack_source_animation_set(stack, 5) == MC_ITEM_STACK_OK);
    CHECK(mc_inventory_player_set(inventory, 0, later, NULL) == MC_INVENTORY_OK &&
          mc_inventory_player_set(inventory, 2, stack, NULL) == MC_INVENTORY_OK &&
          mc_inventory_player_set(inventory, 3, later, NULL) == MC_INVENTORY_OK);
    CHECK(mc_inventory_player_tick(inventory, f->registry[0], NULL, &e) ==
              MC_INVENTORY_TICK_UNIMPLEMENTED_DEPENDENCY &&
          e.inventory_bytecode == 51 && e.stack_bytecode == 31 && e.item_bytecode == 34 &&
          e.slot == 2 && e.completed_slots == 2 && e.pending_map == second && fields(stack, 4) &&
          fields(later, 6) && !inventory->active);
    CHECK(mc_inventory_player_set(inventory, 0, NULL, NULL) == MC_INVENTORY_OK &&
          mc_inventory_player_set(inventory, 2, NULL, NULL) == MC_INVENTORY_OK &&
          mc_inventory_player_set(inventory, 3, NULL, NULL) == MC_INVENTORY_OK);
    /* Real storage save and a distinct real manager disk load, no fake cache. */
    CHECK(mc_world_saved_data_save(world->fields.z, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_saved_data_create_for_side(f->stores[0], MC_WORLD_INFO_CLIENT, NULL, &loaded,
                                              NULL) == MC_WORLD_STORAGE_OK);
    mc_world_saved_data *manager = world->fields.z;
    world->fields.z = loaded;
    mc_saved_data_value *disk = NULL;
    const mc_inventory_tick_status disk_status =
        mc_item_map_data_get(map, stack, world, NULL, &disk, &e);
    world->fields.z = manager;
    CHECK(disk_status == MC_INVENTORY_TICK_OK && disk != second);
    CHECK(map_row(3, stack, disk, loaded, false));
    mc_saved_data_value_release(disk);
    disk = NULL;
    /* No fabricated successful operation at zero resource budget. */
    CHECK(mc_item_stack_damage_set(stack, 72) == MC_ITEM_STACK_OK);
    mc_inventory_tick_limits limits = mc_inventory_tick_default_limits();
    limits.max_map_allocation_bytes = 0;
    CHECK(mc_item_map_data_get(map, stack, world, &limits, &disk, &e) == MC_INVENTORY_TICK_LIMIT &&
          e.map_lookup_bytecode == 99 && !e.map.created && !e.map.published);
    mc_item_stack_fields sf;
    CHECK(mc_item_stack_fields_get(stack, &sf) == MC_ITEM_STACK_OK && sf.damage == 2);
    CHECK(mc_item_stack_damage_set(stack, 999) == MC_ITEM_STACK_OK);
    mc_world_info *info = world->fields.x;
    world->fields.x = NULL;
    const mc_inventory_tick_status null_info =
        mc_item_map_data_get(map, stack, world, NULL, &disk, &e);
    world->fields.x = info;
    CHECK(null_info == MC_INVENTORY_TICK_NULL_REFERENCE && e.map_lookup_bytecode == 110 &&
          e.map.created && !e.map.published && !e.map.dirty && e.map.x_center == 0 &&
          e.map.z_center == 0 && e.map.scale == 0 && e.pending_map == NULL && disk == NULL);
    CHECK(mc_item_stack_fields_get(stack, &sf) == MC_ITEM_STACK_OK && sf.damage == 3);
    CHECK(mc_item_stack_damage_set(stack, 999) == MC_ITEM_STACK_OK);
    mc_world_provider *dimension = world->fields.t;
    world->fields.t = NULL;
    const mc_inventory_tick_status null_dimension =
        mc_item_map_data_get(map, stack, world, NULL, &disk, &e);
    world->fields.t = dimension;
    CHECK(null_dimension == MC_INVENTORY_TICK_NULL_REFERENCE && e.map_lookup_bytecode == 140 &&
          e.map.created && !e.map.published && !e.map.dirty && e.map.x_center == 99 &&
          e.map.z_center == 101 && e.map.scale == 3 && e.pending_map == NULL && disk == NULL);
    CHECK(mc_item_stack_fields_get(stack, &sf) == MC_ITEM_STACK_OK && sf.damage == 4);
    CHECK(mc_item_inventory_update(map, stack, NULL, player, 0, false, NULL, &e) ==
              MC_INVENTORY_TICK_NULL_REFERENCE &&
          e.item_bytecode == 1);
    ok = true;
cleanup:
    mc_entity_release(falling);
    mc_item_stack_release(later);
    free(directory);
    mc_world_saved_data_free(loaded);
    mc_saved_data_value_release(second);
    mc_saved_data_value_release(first);
    mc_item_stack_release(stack);
    return ok;
}

typedef struct tick_resource_probe {
    resources source;
    mc_inventory_player *inventory;
    mc_item_stack **first;
    mc_item_stack *second;
    size_t attempts;
    mc_inventory_status retirement;
} tick_resource_probe;

static mc_registry_status tick_resource_open(void *context, const char *name,
                                             mc_java_properties_input *out) {
    tick_resource_probe *p = context;
    if (p->attempts++ == 0) {
        mc_inventory_player *attempt = p->inventory;
        p->retirement = mc_inventory_player_retire(&attempt, NULL);
        if (p->retirement != MC_INVENTORY_BUSY || attempt != p->inventory)
            return MC_REGISTRY_INVALID_ARGUMENT;
        if (mc_inventory_player_set(p->inventory, 0, NULL, NULL) != MC_INVENTORY_OK ||
            mc_inventory_player_set(p->inventory, 1, p->second, NULL) != MC_INVENTORY_OK)
            return MC_REGISTRY_INVALID_ARGUMENT;
        /* Drop both pre-call occurrences. The runtime's actual captured Stack
         * hold must protect both a later id read and a failing active-use unwind. */
        mc_item_stack_release(*p->first);
        *p->first = NULL;
    }
    return open_resource(&p->source, name, out);
}

static bool lifetime_controls(fixture *f, mc_entity *player) {
    bool ok = false;
    mc_registry_classes *cold = NULL, *missing = NULL;
    mc_inventory_player *inventory = NULL;
    mc_item_stack *first = NULL, *second = NULL;
    mc_inventory_tick_error e;
    tick_resource_probe probe = {0};
    probe.source.streams[0] = (stream){.bytes = language, .length = sizeof(language) - 1};
    probe.source.streams[1] = probe.source.streams[2] = (stream){.bytes = language};
    probe.source.api =
        (mc_registry_resources){&probe, tick_resource_open, close_resource, ready_resource};
    const mc_registry_options options = {MC_REGISTRY_CLIENT, f->profile, f->materials, 0,
                                         &probe.source.api};
    CHECK(mc_registry_classes_allocate(&options, &cold, NULL) == MC_REGISTRY_OK);
    CHECK(mc_inventory_player_create(MC_REGISTRY_CLIENT, player, NULL, &inventory, NULL) ==
          MC_INVENTORY_OK);
    CHECK(stack_new(262, 1, 0, 5, &first) && stack_new(262, 1, 0, 6, &second));
    CHECK(mc_inventory_player_set(inventory, 0, first, NULL) == MC_INVENTORY_OK);
    probe.inventory = inventory;
    probe.first = &first;
    probe.second = second;
    const mc_inventory_tick_status probed = mc_inventory_player_tick(inventory, cold, NULL, &e);
    if (probed != MC_INVENTORY_TICK_REGISTRY_FAILED || probe.attempts == 0 || first != NULL ||
        !fields(second, 6))
        fprintf(
            stderr,
            "lifetime probe: status=%d attempts=%zu retired=%d first=%d second=%d op=%s reg=%s\n",
            (int)probed, probe.attempts, (int)probe.retirement, first != NULL, fields(second, 6),
            e.operation == NULL ? "-" : e.operation,
            e.registry.message == NULL ? "-" : e.registry.message);
    CHECK(probed == MC_INVENTORY_TICK_REGISTRY_FAILED && e.stack_bytecode == 17 &&
          e.inventory_bytecode == 51 && e.completed_slots == 0 && e.slot == 0 &&
          e.animation_stored && probe.attempts > 0 && probe.retirement == MC_INVENTORY_BUSY &&
          first == NULL && inventory->main_slots[0] == NULL && inventory->main_slots[1] == second &&
          fields(second, 6) && !inventory->active && !probe.source.bad &&
          cold->runtime_borrows == 0 && cold->entered == 0);
    const size_t calls = probe.attempts;
    CHECK(mc_inventory_player_tick(inventory, cold, NULL, &e) ==
              MC_INVENTORY_TICK_REGISTRY_FAILED &&
          e.completed_slots == 1 && e.slot == 1 && e.stack_bytecode == 17 && fields(second, 5) &&
          probe.attempts == calls);
    const mc_registry_options absent = {MC_REGISTRY_CLIENT, f->profile, f->materials, 0, NULL};
    CHECK(mc_registry_classes_allocate(&absent, &missing, NULL) == MC_REGISTRY_OK);
    mc_registry_item_view before, after;
    CHECK(mc_registry_classes_inspect_items(missing, &before, NULL) == MC_REGISTRY_OK &&
          before.phase == MC_REGISTRY_COLD);
    CHECK(mc_item_stack_source_animation_set(second, 5) == MC_ITEM_STACK_OK);
    CHECK(mc_item_stack_inventory_update(second, missing, NULL, NULL, 0, false, NULL, &e) !=
              MC_INVENTORY_TICK_OK &&
          e.stack_bytecode == 17 && fields(second, 4) && e.registry.status != MC_REGISTRY_OK &&
          e.animation_stored);
    const mc_registry_error original = e.registry;
    CHECK(mc_item_stack_inventory_update(second, missing, NULL, NULL, 0, false, NULL, &e) !=
              MC_INVENTORY_TICK_OK &&
          e.stack_bytecode == 17 && fields(second, 3) &&
          e.registry.status == MC_REGISTRY_FAILED_INITIALIZATION &&
          e.registry.initializer_pc == original.initializer_pc &&
          e.registry.constructor_pc == original.constructor_pc && missing->runtime_borrows == 0 &&
          missing->entered == 0);
    CHECK(mc_registry_classes_inspect_items(missing, &after, NULL) == MC_REGISTRY_OK &&
          after.phase == MC_REGISTRY_FAILED);
    ok = true;
cleanup:
    if (mc_inventory_player_retire(&inventory, NULL) != MC_INVENTORY_OK)
        ok = false;
    mc_item_stack_release(first);
    mc_item_stack_release(second);
    if (mc_registry_classes_free(cold, NULL) != MC_REGISTRY_OK ||
        mc_registry_classes_free(missing, NULL) != MC_REGISTRY_OK)
        ok = false;
    return ok;
}

int main(int argc, char **argv) {
    if (setvbuf(stdout, NULL, _IONBF, 0) != 0)
        return 1;
    int result = 1;
    fixture *f = NULL;
    char root[8192];
    mc_entity *player = NULL;
    mc_player_session_classes *sessions = NULL;
    mc_player_session *session = NULL;
    const uint16_t title[] = {'I', 'n', 'v', 'e', 'n', 't', 'o', 'r', 'y'};
    if (argc != 3 || strcmp(argv[1], "inventory-tick") != 0 ||
        !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 1;
    f = calloc(1, sizeof(*f));
    CHECK(f != NULL && mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_materials_create(&f->materials) &&
          mc_java_random_profile_create(8u * 1024u * 1024u, &f->profile) ==
              MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
          MC_WORLD_INFO_OK);
    resources *r = &f->sources[0];
    r->streams[0] = (stream){.bytes = language, .length = sizeof(language) - 1};
    r->streams[1] = r->streams[2] = (stream){.bytes = language};
    r->api = (mc_registry_resources){r, open_resource, close_resource, ready_resource};
    const mc_registry_options options = {MC_REGISTRY_CLIENT, f->profile, f->materials, 0, &r->api};
    CHECK(mc_registry_classes_allocate(&options, &f->registry[0], NULL) == MC_REGISTRY_OK &&
          mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile, &trig, &f->classes[0],
                                          NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_classes_bind_registry(f->classes[0], f->registry[0], NULL) ==
              MC_REGISTRY_OK &&
          mc_entity_classes_create(MC_ENTITY_CLIENT, f->profile, NULL, &f->entity_classes[0]) ==
              MC_ENTITY_OK);
    CHECK(mc_world_runtime_classes_bind_entities(f->classes[0], f->entity_classes[0]) ==
              MC_ENTITY_OK &&
          mc_registry_classes_initialize_blocks(f->registry[0], NULL) == MC_REGISTRY_OK);
    CHECK(boxes_open_store(f, 0, root) && boxes_create_world(f, 0));
    CHECK(mc_player_session_classes_create(f->entity_classes[0], f->registry[0], &sessions, NULL) ==
              MC_PLAYER_OK &&
          mc_player_session_create(sessions, NULL, NULL, &session, NULL) == MC_PLAYER_OK);
    CHECK(mc_local_player_allocate(f->entity_classes[0], &player, NULL) == MC_ENTITY_OK &&
          mc_local_player_construct(player, f->worlds[0], NULL, session, 0, NULL, NULL) ==
              MC_PLAYER_OK);
    CHECK(inventory_cases(f, player) && item_cases(f) && map_cases(f, player) &&
          construction_prefix_controls(f) && lifetime_controls(f, player));
    result = 0;
cleanup:
    mc_entity_release(player);
    mc_player_session_retire(&session);
    if (mc_player_session_classes_free(&sessions, NULL) != MC_PLAYER_OK || !cleanup_fixture(f))
        return 1;
    if (result == 0 && printf("Inventory tick: %u checks\n", checks) < 0)
        return 1;
    return result;
}
