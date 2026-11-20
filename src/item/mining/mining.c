#include "mc/item/mining/mining.h"
#include "mc/block/registry/runtime.h"

#include "../../block/registry/registry_internal.h"
#include "../../inventory/internal.h"
#include "../stack/source_internal.h"

typedef enum mining_body {
    BODY_NONE,
    BODY_GM,
    BODY_DQ,
    BODY_WC,
    BODY_AU,
    BODY_QD,
    BODY_BL
} mining_body;

/* Complete client declaration inventory, including DQ and CP ancestors. */
static const mining_body bodies[MC_ITEM_KIND_COUNT] = {[MC_ITEM_BASE] = BODY_GM,
                                                       [MC_ITEM_SHOVEL] = BODY_WC,
                                                       [MC_ITEM_PICKAXE] = BODY_AU,
                                                       [MC_ITEM_AXE] = BODY_DQ,
                                                       [MC_ITEM_FLINT_AND_STEEL] = BODY_GM,
                                                       [MC_ITEM_FOOD] = BODY_GM,
                                                       [MC_ITEM_BOW] = BODY_GM,
                                                       [MC_ITEM_COAL] = BODY_GM,
                                                       [MC_ITEM_SWORD] = BODY_QD,
                                                       [MC_ITEM_HOE] = BODY_GM,
                                                       [MC_ITEM_SEEDS] = BODY_GM,
                                                       [MC_ITEM_ARMOR] = BODY_GM,
                                                       [MC_ITEM_PAINTING] = BODY_GM,
                                                       [MC_ITEM_SIGN] = BODY_GM,
                                                       [MC_ITEM_DOOR] = BODY_GM,
                                                       [MC_ITEM_BUCKET] = BODY_GM,
                                                       [MC_ITEM_MINECART] = BODY_GM,
                                                       [MC_ITEM_SADDLE] = BODY_GM,
                                                       [MC_ITEM_REDSTONE] = BODY_GM,
                                                       [MC_ITEM_SNOWBALL] = BODY_GM,
                                                       [MC_ITEM_BOAT] = BODY_GM,
                                                       [MC_ITEM_BLOCK_PLACER] = BODY_GM,
                                                       [MC_ITEM_EGG] = BODY_GM,
                                                       [MC_ITEM_FISHING_ROD] = BODY_GM,
                                                       [MC_ITEM_DYE] = BODY_GM,
                                                       [MC_ITEM_BED] = BODY_GM,
                                                       [MC_ITEM_SHEARS] = BODY_BL,
                                                       [MC_ITEM_RECORD] = BODY_GM,
                                                       [MC_ITEM_SOUP] = BODY_GM,
                                                       [MC_ITEM_COOKIE] = BODY_GM,
                                                       [MC_ITEM_MAP] = BODY_GM,
                                                       [MC_ITEM_BLOCK] = BODY_GM,
                                                       [MC_ITEM_WOOL_BLOCK] = BODY_GM,
                                                       [MC_ITEM_LOG_BLOCK] = BODY_GM,
                                                       [MC_ITEM_SLAB_BLOCK] = BODY_GM,
                                                       [MC_ITEM_SAPLING_BLOCK] = BODY_GM,
                                                       [MC_ITEM_LEAVES_BLOCK] = BODY_GM,
                                                       [MC_ITEM_PISTON_BLOCK] = BODY_GM};

static bool fail(mc_inventory_error *e, mc_inventory_status status, const char *operation) {
    e->status = status;
    e->operation = operation;
    return false;
}

static bool registry_result(mc_inventory_error *e, mc_registry_status status) {
    e->registry_status = status;
    if (status == MC_REGISTRY_OK)
        return true;
    return fail(e,
                status == MC_REGISTRY_BUSY                 ? MC_INVENTORY_BUSY
                : status == MC_REGISTRY_LIMIT              ? MC_INVENTORY_LIMIT
                : status == MC_REGISTRY_ALLOCATION_FAILED  ? MC_INVENTORY_ALLOCATION_FAILED
                : status == MC_REGISTRY_NULL_REFERENCE     ? MC_INVENTORY_NULL_REFERENCE
                : status == MC_REGISTRY_INDEX_OUT_OF_RANGE ? MC_INVENTORY_INDEX_OUT_OF_BOUNDS
                                                           : MC_INVENTORY_REGISTRY_FAILED,
                e->registry.message);
}

static bool item_admit(mc_registry_classes *classes, const mc_item *item, const void *out,
                       mc_inventory_error *e) {
    if (out == NULL)
        return fail(e, MC_INVENTORY_INVALID_ARGUMENT, "Item mining native output");
    if (item == NULL)
        return fail(e, MC_INVENTORY_NULL_REFERENCE, "Item mining null invocation receiver");
    if (classes == NULL || classes->options.side != MC_REGISTRY_CLIENT ||
        item->classes != classes || (unsigned int)item->kind >= MC_ITEM_KIND_COUNT ||
        bodies[item->kind] == BODY_NONE)
        return fail(e, MC_INVENTORY_INVALID_ARGUMENT, "Item mining actual client Class namespace");
    if (classes->entered != 0)
        return fail(e, MC_INVENTORY_BUSY, "Item mining entered Registry");
    return true;
}

static bool block_static(mc_registry_classes *classes, const char *name, const mc_block **out,
                         mc_inventory_error *e) {
    return registry_result(e, mc_registry_block_static_get(classes, name, out, &e->registry));
}

static bool same_id(mc_registry_classes *classes, const mc_block *block, const char *name,
                    bool *out, int32_t *pc, mc_inventory_error *e) {
    /* QD/BL read argument.bn before the getstatic, then its singleton.bn. */
    if (block == NULL)
        return fail(e, MC_INVENTORY_NULL_REFERENCE, "Item mining null Block field receiver");
    const int32_t id = block->fields.bn;
    const mc_block *other = NULL;
    *pc += 3;
    if (!block_static(classes, name, &other, e))
        return false;
    *pc += 3;
    if (other == NULL)
        return fail(e, MC_INVENTORY_NULL_REFERENCE, "Item mining null static Block field receiver");
    *out = id == other->fields.bn;
    return true;
}

mc_inventory_status mc_item_mining_speed(mc_registry_classes *classes, const mc_item *item,
                                         const mc_item_stack *stack, const mc_block *block,
                                         float *out, mc_inventory_error *error) {
    (void)stack; /* All actual speed bodies ignore the Stack argument. */
    mc_inventory_error e = {0};
    if (!item_admit(classes, item, out, &e))
        return inv_finish(error, &e);
    const mining_body body = bodies[item->kind];
    const char *owner = body == BODY_QD ? "qd" : body == BODY_BL ? "bl" : "dq";
    int32_t pc = 0;
    float value = 1.0f;
    if (body == BODY_DQ || body == BODY_WC || body == BODY_AU) {
        for (size_t i = 0;; ++i) {
            const mc_registry_block_array *array = item->dq.bk;
            pc = 7;
            if (array == NULL) {
                (void)fail(&e, MC_INVENTORY_NULL_REFERENCE, "DQ null effective Block array length");
                break;
            }
            if (i >= array->length)
                break;
            if (array->elements == NULL) {
                (void)fail(&e, MC_INVENTORY_INVALID_ARGUMENT, "DQ native effective Block storage");
                break;
            }
            if (array->elements[i] == block) {
                value = item->dq.bl;
                break;
            }
        }
    } else if (body == BODY_QD || body == BODY_BL) {
        bool match;
        pc = 1;
        if (same_id(classes, block, "X", &match, &pc, &e)) {
            if (match)
                value = 15.0f;
            else if (body == BODY_QD)
                value = 1.5f;
            else {
                pc = 14;
                if (same_id(classes, block, "L", &match, &pc, &e)) {
                    if (match)
                        value = 15.0f;
                    else {
                        pc = 30;
                        if (same_id(classes, block, "ac", &match, &pc, &e) && match)
                            value = 5.0f;
                    }
                }
            }
        }
    }
    if (e.status == MC_INVENTORY_OK)
        *out = value;
    else
        inv_frame(&e, MC_REGISTRY_CLIENT, owner, owner, "a", "(Liz;Luu;)F", pc);
    return inv_finish(error, &e);
}

static bool identity(mc_registry_classes *classes, const mc_block *block, const char *name,
                     bool *out, mc_inventory_error *e) {
    const mc_block *other = NULL;
    if (!block_static(classes, name, &other, e))
        return false;
    *out = block == other;
    return true;
}

mc_inventory_status mc_item_mining_harvest(mc_registry_classes *classes, const mc_item *item,
                                           const mc_block *block, bool *out,
                                           mc_inventory_error *error) {
    mc_inventory_error e = {0};
    if (!item_admit(classes, item, out, &e))
        return inv_finish(error, &e);
    const mining_body body = bodies[item->kind];
    const char *owner = body == BODY_AU   ? "au"
                        : body == BODY_WC ? "wc"
                        : body == BODY_QD ? "qd"
                                          : "bl";
    int32_t pc = 0;
    bool value = false;
    if (body == BODY_QD || body == BODY_BL) {
        pc = 1;
        (void)same_id(classes, block, "X", &value, &pc, &e);
    } else if (body == BODY_WC) {
        pc = 1;
        if (identity(classes, block, "aT", &value, &e) && !value) {
            pc = 10;
            (void)identity(classes, block, "aV", &value, &e);
        }
    } else if (body == BODY_AU) {
        static const struct {
            const char *name;
            int32_t load_pc, tool_pc, level;
            bool exact;
        } checks[] = {
            {"aq", 1, 11, 3, true},     {"ay", 25, 42, 2, false},  {"ax", 32, 42, 2, false},
            {"ai", 56, 73, 2, false},   {"H", 63, 73, 2, false},   {"aj", 87, 104, 1, false},
            {"I", 94, 104, 1, false},   {"P", 118, 135, 1, false}, {"O", 125, 135, 1, false},
            {"aO", 149, 166, 2, false}, {"aP", 156, 166, 2, false}};

        bool matched = false;
        for (size_t i = 0; i < sizeof(checks) / sizeof(*checks); ++i) {
            pc = checks[i].load_pc;
            if (!identity(classes, block, checks[i].name, &matched, &e) || matched) {
                if (matched) {
                    pc = checks[i].tool_pc;
                    if (item->dq.a == NULL)
                        (void)fail(&e, MC_INVENTORY_NULL_REFERENCE,
                                   "AU null ToolMaterial receiver");
                    else
                        value = checks[i].exact ? item->dq.a->f == checks[i].level
                                                : item->dq.a->f >= checks[i].level;
                }
                break;
            }
        }
        if (!matched && e.status == MC_INVENTORY_OK) {
            pc = 180;
            if (block == NULL)
                (void)fail(&e, MC_INVENTORY_NULL_REFERENCE,
                           "AU null Block Material field receiver");
            else {
                const mc_material *actual = block->fields.bA;
                mc_material *material = NULL;
                pc = 183;
                if (registry_result(&e, mc_registry_material_get(classes, MC_MATERIAL_e, &material,
                                                                 &e.registry))) {
                    value = actual == material;
                    if (!value) {
                        actual = block->fields.bA;
                        pc = 195;
                        if (registry_result(&e, mc_registry_material_get(classes, MC_MATERIAL_f,
                                                                         &material, &e.registry)))
                            value = actual == material;
                    }
                }
            }
        }
    }
    if (e.status == MC_INVENTORY_OK)
        *out = value;
    else
        inv_frame(&e, MC_REGISTRY_CLIENT, owner, owner, "a", "(Luu;)Z", pc);
    return inv_finish(error, &e);
}

static bool resolve(mc_registry_classes *classes, const mc_item_stack *stack, bool harvest,
                    const mc_item **out, mc_inventory_error *e) {
    if (stack == NULL)
        return fail(e, MC_INVENTORY_NULL_REFERENCE, "IZ mining null Stack invocation receiver");
    const mc_registry_item_array *array = NULL;
    int32_t pc = 0, id;
    if (registry_result(e, mc_registry_item_c_get(classes, &array, &e->registry))) {
        pc = 4;
        e->stack_status = mc_item_stack_source_id_get(stack, &id);
        if (e->stack_status != MC_ITEM_STACK_OK)
            (void)fail(e, MC_INVENTORY_STACK_FAILED, "IZ mining actual live Stack.id");
        else {
            pc = 7;
            (void)registry_result(e, mc_registry_item_c_at(classes, array, id, out, &e->registry));
        }
    }
    if (e->status != MC_INVENTORY_OK) {
        inv_frame(e, MC_REGISTRY_CLIENT, "iz", "fy", harvest ? "b" : "a",
                  harvest ? "(Luu;)Z" : "()Lgm;", pc);
        if (!harvest)
            inv_frame(e, MC_REGISTRY_CLIENT, "iz", "fy", "a", "(Luu;)F", 1);
    }
    return e->status == MC_INVENTORY_OK;
}

mc_inventory_status mc_item_stack_mining_speed(mc_registry_classes *classes,
                                               const mc_item_stack *stack, const mc_block *block,
                                               float *out, mc_inventory_error *error) {
    mc_inventory_error e = {0};
    const mc_item *item = NULL;
    if (out == NULL)
        (void)fail(&e, MC_INVENTORY_INVALID_ARGUMENT, "IZ mining speed native output");
    else if (resolve(classes, stack, false, &item, &e) &&
             mc_item_mining_speed(classes, item, stack, block, out, &e) != MC_INVENTORY_OK)
        inv_frame(&e, MC_REGISTRY_CLIENT, "iz", "fy", "a", "(Luu;)F", 6);
    return inv_finish(error, &e);
}

mc_inventory_status mc_item_stack_mining_harvest(mc_registry_classes *classes,
                                                 const mc_item_stack *stack, const mc_block *block,
                                                 bool *out, mc_inventory_error *error) {
    mc_inventory_error e = {0};
    const mc_item *item = NULL;
    if (out == NULL)
        (void)fail(&e, MC_INVENTORY_INVALID_ARGUMENT, "IZ mining harvest native output");
    else if (resolve(classes, stack, true, &item, &e) &&
             mc_item_mining_harvest(classes, item, block, out, &e) != MC_INVENTORY_OK)
        inv_frame(&e, MC_REGISTRY_CLIENT, "iz", "fy", "b", "(Luu;)Z", 9);
    return inv_finish(error, &e);
}
