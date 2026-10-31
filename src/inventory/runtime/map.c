#include "../../item/stack/source_internal.h"
#include "../../world/runtime/construction_internal.h"
#include "mc/core/java/java.h"
#include "mc/world/runtime/saved_data.h"
#include "mc/world/saved_data/storage.h"
#include "tick_internal.h"

static bool name_result(mc_inventory_tick_error *e, mc_world_info_status status) {
    e->info = status;
    return status == MC_WORLD_INFO_OK ||
           mc_inventory_tick_fail(e,
                                  status == MC_WORLD_INFO_LIMIT ? MC_INVENTORY_TICK_LIMIT
                                  : status == MC_WORLD_INFO_ALLOCATION_FAILED
                                      ? MC_INVENTORY_TICK_ALLOCATION_FAILED
                                      : MC_INVENTORY_TICK_WORLD_FAILED,
                                  "Map actual String identity allocation");
}

static bool key(mc_inventory_tick_scope *s, mc_item_stack *stack, mc_world_info_name **out,
                unsigned int getter, unsigned int text, mc_inventory_tick_error *e) {
    e->map_lookup_bytecode = getter;
    if (stack == NULL)
        return mc_inventory_tick_fail(e, MC_INVENTORY_TICK_NULL_REFERENCE,
                                      "Map actual Stack.i receiver");
    mc_item_stack_fields fields;
    if (!mc_inventory_tick_stack_result(e, mc_item_stack_fields_get(stack, &fields),
                                        "Map actual Stack.i receiver"))
        return false;
    uint16_t units[15] = {'m', 'a', 'p', '_'};
    size_t count = 4;
    uint32_t value =
        fields.damage < 0 ? (uint32_t)(-(int64_t)fields.damage) : (uint32_t)fields.damage;
    if (fields.damage < 0)
        units[count++] = '-';
    uint16_t reverse[10];
    size_t length = 0;
    do {
        reverse[length++] = (uint16_t)('0' + value % 10u);
        value /= 10u;
    } while (value != 0);
    while (length != 0)
        units[count++] = reverse[--length];
    e->map_lookup_bytecode = text;
    return name_result(e, mc_world_info_name_copy((mc_nbt_string){units, count},
                                                  s->limits.max_name_allocation_bytes, out));
}

static void prefix(mc_saved_data_value *value, mc_inventory_tick_error *e) {
    mc_saved_data_map_fields fields;
    if (mc_saved_data_map_fields_get(value, &fields) == MC_WORLD_STORAGE_OK) {
        e->map.dirty = fields.dirty;
        e->map.x_center = fields.x_center;
        e->map.z_center = fields.z_center;
        e->map.dimension = fields.dimension;
        e->map.scale = fields.scale;
    }
}

static bool map_world(mc_inventory_tick_scope *s, mc_world_runtime *world,
                      mc_inventory_tick_error *e) {
    if (world == NULL)
        return mc_inventory_tick_fail(e, MC_INVENTORY_TICK_NULL_REFERENCE,
                                      "Map actual World receiver");
    if (world->construction.mode != MC_WORLD_RUNTIME_GAME ||
        mc_world_runtime_registry(world) != s->classes)
        return mc_inventory_tick_fail(e, MC_INVENTORY_TICK_NAMESPACE,
                                      "Map actual World/code Registry namespace");
    return true;
}

static bool info_get(mc_world_runtime *world, int32_t *out, bool x, mc_inventory_tick_error *e) {
    mc_world_info *info = mc_world_runtime_info(world);
    mc_world_info_fields fields;
    e->info = mc_world_info_fields_get(info, &fields);
    if (e->info != MC_WORLD_INFO_OK)
        return mc_inventory_tick_fail(
            e, info == NULL ? MC_INVENTORY_TICK_NULL_REFERENCE : MC_INVENTORY_TICK_WORLD_FAILED,
            "Map actual World.x/Info spawn getter");
    *out = x ? fields.value.spawn_x : fields.value.spawn_z;
    return true;
}

bool mc_inventory_tick_map_lookup(mc_inventory_tick_scope *s, mc_item_stack *stack,
                                  mc_world_runtime *world, mc_saved_data_value **out,
                                  mc_inventory_tick_error *e) {
    bool ok = false;
    mc_world_info_name *first = NULL, *lookup = NULL, *name = NULL;
    mc_saved_data_value *value = NULL;
    if (!key(s, stack, &first, 13, 19, e))
        goto cleanup;
    /* Capture the same World argument before constructing the second key. */
    mc_world_runtime *captured_world = world;
    if (!key(s, stack, &lookup, 39, 45, e))
        goto cleanup;
    e->map_lookup_bytecode = 48;
    if (!map_world(s, captured_world, e))
        goto cleanup;
    if (!mc_inventory_tick_world_result(
            e,
            mc_world_runtime_saved_data_get(captured_world,
                                            mc_saved_data_map_class(MC_WORLD_INFO_CLIENT), lookup,
                                            &value, &e->world),
            "Map actual World SavedData get"))
        goto cleanup;
    e->map_lookup_bytecode = 51;
    if (value != NULL &&
        mc_saved_data_value_class(value) != mc_saved_data_map_class(MC_WORLD_INFO_CLIENT)) {
        mc_inventory_tick_fail(e, MC_INVENTORY_TICK_CLASS_CAST, "Map actual iu checkcast");
        goto cleanup;
    }
    if (value == NULL) {
        static const uint16_t map_units[] = {'m', 'a', 'p'};
        const mc_nbt_string map_key = {map_units, 3};
        int32_t next;
        e->map_lookup_bytecode = 65;
        if (!map_world(s, world, e) ||
            !mc_inventory_tick_world_result(
                e, mc_world_runtime_saved_data_next_id(world, &map_key, &next, &e->world),
                "Map actual World next map ID"))
            goto cleanup;
        e->map_lookup_bytecode = 68;
        if (!mc_inventory_tick_stack_result(e, mc_item_stack_source_damage_set(stack, next),
                                            "Map actual Stack.b damage store"))
            goto cleanup;
        if (!key(s, stack, &name, 84, 90, e))
            goto cleanup;
        e->map_lookup_bytecode = 99;
        const mc_world_storage_status created =
            mc_saved_data_map_create(mc_saved_data_map_class(MC_WORLD_INFO_CLIENT), name,
                                     s->limits.max_map_allocation_bytes, &value, &e->saved_data);
        if (!mc_inventory_tick_world_result(e, created, "Map actual MapData constructor"))
            goto cleanup;
        e->map.created = true;
        prefix(value, e);
        const mc_saved_data_map_cells *cells = mc_saved_data_map_live(value);
        int32_t center;
        e->map_lookup_bytecode = 110;
        if (!info_get(world, &center, true, e))
            goto cleanup;
        *cells->x_center = center;
        prefix(value, e);
        e->map_lookup_bytecode = 122;
        if (!info_get(world, &center, false, e))
            goto cleanup;
        *cells->z_center = center;
        prefix(value, e);
        *cells->scale = 3;
        prefix(value, e);
        e->map_lookup_bytecode = 140;
        mc_world_provider *dimension = world->fields.t;
        mc_world_provider_fields fields;
        e->provider = mc_world_provider_fields_get(dimension, &fields);
        if (e->provider != MC_WORLD_PROVIDER_OK) {
            mc_inventory_tick_fail(e,
                                   dimension == NULL ? MC_INVENTORY_TICK_NULL_REFERENCE
                                                     : MC_INVENTORY_TICK_WORLD_FAILED,
                                   "Map actual World.t.g receiver");
            goto cleanup;
        }
        *cells->dimension = mc_java_i8_narrow(fields.g);
        prefix(value, e);
        e->map_lookup_bytecode = 149;
        if (!mc_inventory_tick_world_result(e, mc_saved_data_value_mark_dirty(value),
                                            "Map actual MapData mark dirty"))
            goto cleanup;
        prefix(value, e);
        e->map_lookup_bytecode = 156;
        if (!mc_inventory_tick_world_result(
                e, mc_world_runtime_saved_data_put(world, name, value, &e->world),
                "Map actual World SavedData put"))
            goto cleanup;
    }
    e->map.published = true;
    prefix(value, e);
    *out = value;
    value = NULL;
    ok = true;
cleanup:
    mc_saved_data_value_release(value);
    mc_world_info_name_release(name);
    mc_world_info_name_release(lookup);
    mc_world_info_name_release(first);
    return ok;
}

bool mc_inventory_tick_map_update(mc_inventory_tick_scope *s, mc_item_stack *stack,
                                  mc_world_runtime *world, mc_entity *entity, bool selected,
                                  mc_inventory_tick_error *e) {
    e->item_bytecode = 1;
    if (!map_world(s, world, e))
        return false;
    if (world->fields.B)
        return true;
    mc_saved_data_value *value = NULL;
    e->item_bytecode = 11;
    if (!mc_inventory_tick_map_lookup(s, stack, world, &value, e))
        return false;
    e->pending_map = value;
    e->item_bytecode = 17;
    bool player = false;
    if (entity != NULL) {
        if (entity->classes->source != MC_ENTITY_CLIENT ||
            s->classes->entities != entity->classes ||
            mc_world_runtime_entities(world) != entity->classes) {
            mc_saved_data_value_release(value);
            return mc_inventory_tick_fail(e, MC_INVENTORY_TICK_NAMESPACE,
                                          "Map actual Entity/Player class namespace");
        }
        if (!mc_inventory_tick_is_player(entity, &player, e)) {
            mc_saved_data_value_release(value);
            return false;
        }
    }
    if (player) {
        e->item_bytecode = 34;
        e->tracker_bytecode = 0;
        mc_saved_data_value_release(value);
        return mc_inventory_tick_fail(e, MC_INVENTORY_TICK_UNIMPLEMENTED_DEPENDENCY,
                                      "Actual iu.a(Player,Stack) MapData tracker body pending");
    }
    if (selected) {
        e->item_bytecode = 47;
        e->terrain_bytecode = 0;
        mc_saved_data_value_release(value);
        return mc_inventory_tick_fail(
            e, MC_INVENTORY_TICK_UNIMPLEMENTED_DEPENDENCY,
            "Actual wr.a(World,Entity,MapData) selected terrain body pending");
    }
    mc_saved_data_value_release(value);
    return true;
}
