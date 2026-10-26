#include "../../block/registry/runtime_internal.h"
#include "../../item/stack/source_internal.h"
#include "../internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/item/registry/runtime.h"
#include "mc/world/saved_data/storage.h"
#include "tick_internal.h"

mc_inventory_tick_limits mc_inventory_tick_default_limits(void) {
    const size_t bytes = mc_world_saved_data_default_limits().max_allocation_bytes;
    return (mc_inventory_tick_limits){bytes, bytes};
}

mc_inventory_tick_error mc_inventory_tick_empty_error(void) {
    mc_inventory_tick_error e = {0};
    e.slot = -1;
    e.registry = (mc_registry_error){.initializer_pc = -1,
                                     .block_id = -1,
                                     .constructor_pc = -1,
                                     .active_initializer_pc = -1,
                                     .item_argument = -1};
    return e;
}

bool mc_inventory_tick_fail(mc_inventory_tick_error *e, mc_inventory_tick_status status,
                            const char *operation) {
    e->status = status;
    e->operation = operation;
    return false;
}

bool mc_inventory_tick_stack_result(mc_inventory_tick_error *e, mc_item_stack_status status,
                                    const char *operation) {
    e->stack = status;
    return status == MC_ITEM_STACK_OK ||
           mc_inventory_tick_fail(e,
                                  status == MC_ITEM_STACK_LIMIT ? MC_INVENTORY_TICK_LIMIT
                                  : status == MC_ITEM_STACK_ALLOCATION_FAILED
                                      ? MC_INVENTORY_TICK_ALLOCATION_FAILED
                                      : MC_INVENTORY_TICK_STACK_FAILED,
                                  operation);
}

bool mc_inventory_tick_world_result(mc_inventory_tick_error *e, mc_world_storage_status status,
                                    const char *operation) {
    return status == MC_WORLD_STORAGE_OK ||
           mc_inventory_tick_fail(
               e,
               status == MC_WORLD_STORAGE_LIMIT               ? MC_INVENTORY_TICK_LIMIT
               : status == MC_WORLD_STORAGE_ALLOCATION_FAILED ? MC_INVENTORY_TICK_ALLOCATION_FAILED
               : status == MC_WORLD_STORAGE_BUSY              ? MC_INVENTORY_TICK_BUSY
                                                              : MC_INVENTORY_TICK_WORLD_FAILED,
               operation);
}

static bool registry_result(mc_inventory_tick_error *e, mc_registry_status status,
                            const char *operation) {
    return status == MC_REGISTRY_OK ||
           mc_inventory_tick_fail(e,
                                  status == MC_REGISTRY_LIMIT ? MC_INVENTORY_TICK_LIMIT
                                  : status == MC_REGISTRY_ALLOCATION_FAILED
                                      ? MC_INVENTORY_TICK_ALLOCATION_FAILED
                                  : status == MC_REGISTRY_BUSY ? MC_INVENTORY_TICK_BUSY
                                                               : MC_INVENTORY_TICK_REGISTRY_FAILED,
                                  operation);
}

static bool enter(mc_inventory_tick_scope *s, mc_registry_classes *classes,
                  const mc_inventory_tick_limits *limits, mc_inventory_tick_error *e) {
    s->classes = classes;
    s->limits = limits == NULL ? mc_inventory_tick_default_limits() : *limits;
    if (classes == NULL)
        return true;
    const mc_registry_status status = mc_registry_classes_retain(
        classes, MC_REGISTRY_CLIENT, classes->options.random_profile, &e->registry);
    if (!registry_result(e, status, "Inventory tick actual code Registry borrow"))
        return false;
    s->registry_held = true;
    return true;
}

static mc_inventory_tick_status finish(mc_inventory_tick_scope *s, mc_inventory_tick_error *e,
                                       mc_inventory_tick_error *out) {
    if (s->registry_held && !mc_registry_runtime_borrow_release(s->classes))
        mc_inventory_tick_fail(e, MC_INVENTORY_TICK_OWNER,
                               "Inventory tick Registry borrow retirement");
    if (e->status == MC_INVENTORY_TICK_OK)
        *e = mc_inventory_tick_empty_error();
    if (out != NULL)
        *out = *e;
    return e->status;
}

bool mc_inventory_tick_is_player(mc_entity *entity, bool *out, mc_inventory_tick_error *e) {
    const mc_entity_class_literal *target = NULL;
    bool answer = false;
    e->entity = mc_entity_class_literal_get(entity->classes, MC_ENTITY_CLASS_KIND_PLAYER, &target);
    if (e->entity == MC_ENTITY_OK)
        e->entity =
            mc_entity_class_literal_is_assignable_from(target, entity->class_literal, &answer);
    if (e->entity != MC_ENTITY_OK)
        return mc_inventory_tick_fail(e, MC_INVENTORY_TICK_OWNER,
                                      "Actual Player Class assignability");
    *out = answer;
    return true;
}

static bool entity_hold(mc_entity *entity, mc_inventory_tick_error *e) {
    if (entity == NULL)
        return true;
    e->entity = mc_entity_retain(entity);
    return e->entity == MC_ENTITY_OK ||
           mc_inventory_tick_fail(
               e, e->entity == MC_ENTITY_LIMIT ? MC_INVENTORY_TICK_LIMIT : MC_INVENTORY_TICK_OWNER,
               "Inventory update Entity lifetime");
}

static bool world_hold(mc_world_runtime *world, mc_inventory_tick_error *e) {
    return world == NULL ||
           mc_inventory_tick_world_result(e, mc_world_runtime_call_enter(world, &e->world),
                                          "Inventory update World lifetime");
}

/* An explicit table, rather than 'every kind except Map', prevents a future
 * unreviewed native subtype from silently acquiring the base no-op. All40
 * actual supplied-JAR descendants are resolved in the public metadata. */
static bool base_update(mc_item_kind kind) {
    static const bool proven[MC_ITEM_KIND_COUNT] = {[MC_ITEM_BASE] = true,
                                                    [MC_ITEM_SHOVEL] = true,
                                                    [MC_ITEM_PICKAXE] = true,
                                                    [MC_ITEM_AXE] = true,
                                                    [MC_ITEM_FLINT_AND_STEEL] = true,
                                                    [MC_ITEM_FOOD] = true,
                                                    [MC_ITEM_BOW] = true,
                                                    [MC_ITEM_COAL] = true,
                                                    [MC_ITEM_SWORD] = true,
                                                    [MC_ITEM_HOE] = true,
                                                    [MC_ITEM_SEEDS] = true,
                                                    [MC_ITEM_ARMOR] = true,
                                                    [MC_ITEM_PAINTING] = true,
                                                    [MC_ITEM_SIGN] = true,
                                                    [MC_ITEM_DOOR] = true,
                                                    [MC_ITEM_BUCKET] = true,
                                                    [MC_ITEM_MINECART] = true,
                                                    [MC_ITEM_SADDLE] = true,
                                                    [MC_ITEM_REDSTONE] = true,
                                                    [MC_ITEM_SNOWBALL] = true,
                                                    [MC_ITEM_BOAT] = true,
                                                    [MC_ITEM_BLOCK_PLACER] = true,
                                                    [MC_ITEM_EGG] = true,
                                                    [MC_ITEM_FISHING_ROD] = true,
                                                    [MC_ITEM_DYE] = true,
                                                    [MC_ITEM_BED] = true,
                                                    [MC_ITEM_SHEARS] = true,
                                                    [MC_ITEM_RECORD] = true,
                                                    [MC_ITEM_SOUP] = true,
                                                    [MC_ITEM_COOKIE] = true,
                                                    [MC_ITEM_BLOCK] = true,
                                                    [MC_ITEM_WOOL_BLOCK] = true,
                                                    [MC_ITEM_LOG_BLOCK] = true,
                                                    [MC_ITEM_SLAB_BLOCK] = true,
                                                    [MC_ITEM_SAPLING_BLOCK] = true,
                                                    [MC_ITEM_LEAVES_BLOCK] = true,
                                                    [MC_ITEM_PISTON_BLOCK] = true};
    return (unsigned int)kind < (unsigned int)MC_ITEM_KIND_COUNT && proven[kind];
}

static bool item_admit(mc_inventory_tick_scope *s, const mc_item *item,
                       mc_inventory_tick_error *e) {
    e->item = item;
    if (item == NULL)
        return mc_inventory_tick_fail(e, MC_INVENTORY_TICK_NULL_REFERENCE,
                                      "Actual Item update receiver");
    if (item->classes != s->classes)
        return mc_inventory_tick_fail(e, MC_INVENTORY_TICK_NAMESPACE,
                                      "Item update actual Registry namespace");
    if (item->phase != MC_ITEM_CONSTRUCTED)
        return mc_inventory_tick_fail(e, MC_INVENTORY_TICK_OWNER,
                                      "Item update constructed receiver");
    return true;
}

static bool item_update(mc_inventory_tick_scope *s, const mc_item *item, mc_item_stack *stack,
                        mc_world_runtime *world, mc_entity *entity, bool selected,
                        mc_inventory_tick_error *e) {
    if (!item_admit(s, item, e))
        return false;
    if (base_update(item->kind))
        return true; /* Genuine gm.a Code1 return0. */
    if (item->kind == MC_ITEM_MAP)
        return mc_inventory_tick_map_update(s, stack, world, entity, selected, e);
    return mc_inventory_tick_fail(e, MC_INVENTORY_TICK_UNIMPLEMENTED_DEPENDENCY,
                                  "Item inventory-update concrete virtual not proven");
}

static bool stack_update(mc_inventory_tick_scope *s, mc_item_stack *stack, mc_world_runtime *world,
                         mc_entity *entity, bool selected, mc_inventory_tick_error *e) {
    e->stack_bytecode = 1;
    if (stack == NULL)
        return mc_inventory_tick_fail(e, MC_INVENTORY_TICK_NULL_REFERENCE,
                                      "Actual Stack inventory update receiver");
    bool stored = false;
    if (!mc_inventory_tick_stack_result(
            e, mc_item_stack_source_inventory_animation_tick(stack, &stored),
            "Actual Stack inventory animation prefix"))
        return false;
    e->animation_stored = e->animation_stored || stored;
    e->stack_bytecode = 17;
    if (s->classes == NULL)
        return mc_inventory_tick_fail(e, MC_INVENTORY_TICK_NAMESPACE,
                                      "Stack Item.c code namespace");
    const mc_registry_item_array *array;
    if (!registry_result(e, mc_registry_item_c_get(s->classes, &array, &e->registry),
                         "Actual Stack Item.c active use"))
        return false;
    mc_item_stack_fields fields;
    e->stack_bytecode = 21;
    if (!mc_inventory_tick_stack_result(e, mc_item_stack_fields_get(stack, &fields),
                                        "Actual Stack current id read"))
        return false;
    const mc_item *item;
    e->stack_bytecode = 24;
    if (!registry_result(e,
                         mc_registry_item_c_at(s->classes, array, fields.id, &item, &e->registry),
                         "Actual Stack captured Item.c aaload"))
        return false;
    e->stack_bytecode = 31;
    return item_update(s, item, stack, world, entity, selected, e);
}

static bool call(mc_inventory_tick_scope *s, const mc_item *item, mc_item_stack *stack,
                 mc_world_runtime *world, mc_entity *entity, bool selected, bool stack_body,
                 mc_inventory_tick_error *e) {
    bool held_stack = false, held_entity = false, held_world = false, ok = false;
    if (stack != NULL) {
        if (!mc_inventory_tick_stack_result(e, mc_item_stack_retain(stack),
                                            "Inventory update Stack lifetime"))
            goto cleanup;
        held_stack = true;
    }
    if (!entity_hold(entity, e))
        goto cleanup;
    held_entity = entity != NULL;
    if (!world_hold(world, e))
        goto cleanup;
    held_world = world != NULL;
    ok = stack_body ? stack_update(s, stack, world, entity, selected, e)
                    : item_update(s, item, stack, world, entity, selected, e);
cleanup:
    if (held_world)
        mc_world_runtime_call_leave(world);
    if (held_entity)
        mc_entity_release(entity);
    if (held_stack)
        mc_item_stack_release(stack);
    return ok;
}

mc_inventory_tick_status mc_inventory_player_tick(mc_inventory_player *inventory,
                                                  mc_registry_classes *classes,
                                                  const mc_inventory_tick_limits *limits,
                                                  mc_inventory_tick_error *out) {
    mc_inventory_tick_scope s = {0};
    mc_inventory_tick_error e = mc_inventory_tick_empty_error();
    bool active = false;
    if (inventory == NULL)
        mc_inventory_tick_fail(&e, MC_INVENTORY_TICK_INVALID_ARGUMENT,
                               "Inventory tick native owner");
    else if (inventory->side != MC_REGISTRY_CLIENT)
        mc_inventory_tick_fail(&e, MC_INVENTORY_TICK_NAMESPACE,
                               "Inventory tick actual client ix owner");
    else if (inventory->active)
        mc_inventory_tick_fail(&e, MC_INVENTORY_TICK_BUSY, "Inventory tick already active");
    else if (enter(&s, classes, limits, &e)) {
        inventory->active = true;
        active = true;
        for (int32_t slot = 0;; slot = mc_java_i32_add(slot, 1)) {
            e.slot = slot;
            e.inventory_bytecode = 7;
            if (inventory->main_slots == NULL) {
                mc_inventory_tick_fail(&e, MC_INVENTORY_TICK_NULL_REFERENCE,
                                       "Inventory main array length receiver");
                break;
            }
            if (slot >= 36)
                break; /* Constructor's SAME actual final-extent array. */
            e.inventory_bytecode = 16;
            if (inventory->main_slots[slot] != NULL) {
                e.inventory_bytecode = 25;
                mc_item_stack *stack = inventory->main_slots[slot];
                e.inventory_bytecode = 30;
                mc_entity *player = inventory->player;
                if (player == NULL) {
                    mc_inventory_tick_fail(&e, MC_INVENTORY_TICK_NULL_REFERENCE,
                                           "Inventory actual Player.aI receiver");
                    break;
                }
                bool actual_player = false;
                if (player->classes->source != MC_ENTITY_CLIENT ||
                    !mc_inventory_tick_is_player(player, &actual_player, &e) || !actual_player) {
                    mc_inventory_tick_fail(&e, MC_INVENTORY_TICK_OWNER,
                                           "Inventory actual Player class receiver");
                    break;
                }
                mc_world_runtime *world = player->base.world;
                mc_entity *entity = inventory->player;
                const bool selected = inventory->selected == slot;
                e.inventory_bytecode = 51;
                if (!call(&s, NULL, stack, world, entity, selected, true, &e))
                    break;
            }
            ++e.completed_slots;
        }
    }
    if (active)
        inventory->active = false;
    return finish(&s, &e, out);
}

mc_inventory_tick_status mc_item_stack_inventory_update(mc_item_stack *stack,
                                                        mc_registry_classes *classes,
                                                        mc_world_runtime *world, mc_entity *entity,
                                                        int32_t slot, bool selected,
                                                        const mc_inventory_tick_limits *limits,
                                                        mc_inventory_tick_error *out) {
    mc_inventory_tick_scope s = {0};
    mc_inventory_tick_error e = mc_inventory_tick_empty_error();
    e.slot = slot;
    if (enter(&s, classes, limits, &e))
        (void)call(&s, NULL, stack, world, entity, selected, true, &e);
    return finish(&s, &e, out);
}

mc_inventory_tick_status mc_item_inventory_update(const mc_item *item, mc_item_stack *stack,
                                                  mc_world_runtime *world, mc_entity *entity,
                                                  int32_t slot, bool selected,
                                                  const mc_inventory_tick_limits *limits,
                                                  mc_inventory_tick_error *out) {
    mc_inventory_tick_scope s = {0};
    mc_inventory_tick_error e = mc_inventory_tick_empty_error();
    e.slot = slot;
    if (enter(&s, item == NULL ? NULL : item->classes, limits, &e))
        (void)call(&s, item, stack, world, entity, selected, false, &e);
    return finish(&s, &e, out);
}

mc_inventory_tick_status mc_item_map_data_get(const mc_item *item, mc_item_stack *stack,
                                              mc_world_runtime *world,
                                              const mc_inventory_tick_limits *limits,
                                              mc_saved_data_value **out,
                                              mc_inventory_tick_error *error) {
    mc_inventory_tick_scope s = {0};
    mc_inventory_tick_error e = mc_inventory_tick_empty_error();
    mc_saved_data_value *value = NULL;
    bool held_stack = false, held_world = false, ok = false;
    if (out == NULL)
        mc_inventory_tick_fail(&e, MC_INVENTORY_TICK_INVALID_ARGUMENT, "Map lookup output");
    else if (enter(&s, item == NULL ? NULL : item->classes, limits, &e) &&
             item_admit(&s, item, &e)) {
        if (item->kind != MC_ITEM_MAP)
            mc_inventory_tick_fail(&e, MC_INVENTORY_TICK_OWNER, "Map lookup actual wr receiver");
        else {
            if (stack != NULL) {
                if (!mc_inventory_tick_stack_result(&e, mc_item_stack_retain(stack),
                                                    "Map lookup Stack lifetime"))
                    goto cleanup;
                held_stack = true;
            }
            if (!world_hold(world, &e))
                goto cleanup;
            held_world = world != NULL;
            ok = mc_inventory_tick_map_lookup(&s, stack, world, &value, &e);
        }
    }
cleanup:
    if (held_world)
        mc_world_runtime_call_leave(world);
    if (held_stack)
        mc_item_stack_release(stack);
    const mc_inventory_tick_status status = finish(&s, &e, error);
    if (status == MC_INVENTORY_TICK_OK && ok)
        *out = value;
    else
        mc_saved_data_value_release(value);
    return status;
}
