#include "../../block/registry/registry_internal.h"

#include "mc/core/java/java.h"

/* RAW allocation and declaring-class initialization precede argument
 * evaluation in the caller. These bodies retain every reached base/static
 * publication when a later constructor operation fails. */
#define STAGE(c, s, pc) mc_registry_item_stage(item, (c), (s), (pc))
#define TRY(expression)                                                                            \
    do {                                                                                           \
        mc_registry_status status = (expression);                                                  \
        if (status != MC_REGISTRY_OK)                                                              \
            return status;                                                                         \
    } while (0)

static mc_registry_status base_prefix(mc_registry_classes *classes, mc_item *item, int32_t id,
                                      const char *client, const char *server) {
    STAGE(client, server, 0);
    STAGE(client, server, 2);
    TRY(mc_registry_item_base(classes, item, id));
    STAGE(client, server, 5);
    return MC_REGISTRY_OK;
}

static mc_registry_status dq_construct(mc_registry_classes *classes, mc_item *item, int32_t id,
                                       int32_t attack, const mc_item_tool_material *tool,
                                       mc_registry_block_array *effective) {
    TRY(base_prefix(classes, item, id, "dq", "cj"));
    STAGE("dq", "cj", 8);
    item->dq.bl = 4.0f;
    STAGE("dq", "cj", 13);
    item->dq.a = tool;
    STAGE("dq", "cj", 19);
    item->dq.bk = effective;
    STAGE("dq", "cj", 24);
    item->fields.bg = 1;
    STAGE("dq", "cj", 29);
    if (tool == NULL)
        return mc_registry_fail(classes, MC_REGISTRY_NULL_REFERENCE, "tool material durability",
                                NULL);
    int32_t maximum = tool->g;
    STAGE("dq", "cj", 32);
    mc_registry_item_durability(item, maximum);
    STAGE("dq", "cj", 38);
    float efficiency = tool->h;
    STAGE("dq", "cj", 41);
    item->dq.bl = efficiency;
    STAGE("dq", "cj", 47);
    int32_t damage = tool->i;
    damage = mc_java_i32_add(attack, damage);
    STAGE("dq", "cj", 51);
    item->dq.bm = damage;
    STAGE("dq", "cj", 54);
    return MC_REGISTRY_OK;
}

static mc_registry_status yw_construct(mc_registry_classes *classes, mc_item *item, int32_t id,
                                       int32_t amount, bool flag) {
    TRY(base_prefix(classes, item, id, "yw", "px"));
    STAGE("yw", "px", 7);
    item->yw.a = amount;
    STAGE("yw", "px", 12);
    item->yw.bk = flag;
    STAGE("yw", "px", 17);
    item->fields.bg = 1;
    STAGE("yw", "px", 20);
    return MC_REGISTRY_OK;
}

mc_registry_status mc_registry_item_ck(mc_registry_classes *classes, mc_item *item, int32_t id) {
    TRY(base_prefix(classes, item, id, "ck", "bk"));
    int32_t block_id = mc_java_i32_add(id, 256);
    STAGE("ck", "bk", 11);
    item->ck_a = block_id;
    STAGE("ck", "bk", 15);
    TRY(mc_registry_blocks_active_use(classes));
    mc_block **array = classes->blocks.m;
    STAGE("ck", "bk", 23);
    if (array == NULL)
        return mc_registry_fail(classes, MC_REGISTRY_NULL_REFERENCE, "BlockItem Block array", NULL);
    if (block_id < 0 || block_id >= (int32_t)MC_REGISTRY_BLOCK_SLOTS)
        return mc_registry_fail(classes, MC_REGISTRY_INDEX_OUT_OF_RANGE, "BlockItem Block index",
                                NULL);
    mc_block *block = array[(size_t)block_id];
    STAGE("ck", "bk", 25);
    int32_t texture = 0;
    mc_registry_status status = mc_registry_block_texture_side(classes, block, 2, &texture);
    if (status != MC_REGISTRY_OK)
        return mc_registry_fail(classes, status, "BlockItem side texture", NULL);
    STAGE("ck", "bk", 28);
    mc_registry_item_icon_direct(item, texture);
    STAGE("ck", "bk", 32);
    return MC_REGISTRY_OK;
}

static mc_registry_status block_item(mc_registry_classes *classes, mc_item *item, int32_t id,
                                     const char *client, const char *server, bool subtypes) {
    STAGE(client, server, 0);
    STAGE(client, server, 2);
    TRY(mc_registry_item_ck(classes, item, id));
    STAGE(client, server, 5);
    if (subtypes) {
        STAGE(client, server, 7);
        mc_registry_item_durability(item, 0);
        STAGE(client, server, 13);
        mc_registry_item_subtypes(item, true);
        STAGE(client, server, 17);
    }
    return MC_REGISTRY_OK;
}

static mc_registry_status armor(mc_registry_classes *classes, mc_item *item,
                                const mc_item_arguments *a) {
    TRY(base_prefix(classes, item, a->id, "wa", "nx"));
    STAGE("wa", "nx", 7);
    item->subtype.wa.a = a->integers[0];
    STAGE("wa", "nx", 13);
    item->subtype.wa.bk = a->integers[2];
    STAGE("wa", "nx", 18);
    item->subtype.wa.bm = a->integers[1];
    STAGE("wa", "nx", 22);
    int32_t *protection = classes->items.wa_bn;
    int32_t slot = a->integers[2];
    STAGE("wa", "nx", 27);
    if (protection == NULL)
        return mc_registry_fail(classes, MC_REGISTRY_NULL_REFERENCE, "armor protection array",
                                NULL);
    if (slot < 0 || slot >= 4)
        return mc_registry_fail(classes, MC_REGISTRY_INDEX_OUT_OF_RANGE, "armor protection index",
                                NULL);
    int32_t value = protection[(size_t)slot];
    STAGE("wa", "nx", 28);
    item->subtype.wa.bl = value;
    STAGE("wa", "nx", 32);
    int32_t *durability = classes->items.wa_bo;
    STAGE("wa", "nx", 37);
    if (durability == NULL)
        return mc_registry_fail(classes, MC_REGISTRY_NULL_REFERENCE, "armor durability array",
                                NULL);
    value = durability[(size_t)slot];
    value = mc_java_i32_mul(value, 3);
    value = mc_java_i32_shl(value, a->integers[0]);
    STAGE("wa", "nx", 42);
    mc_registry_item_durability(item, value);
    STAGE("wa", "nx", 48);
    item->fields.bg = 1;
    STAGE("wa", "nx", 51);
    return MC_REGISTRY_OK;
}

static mc_registry_status constructor_body(mc_registry_classes *classes, mc_item *item,
                                           const mc_item_arguments *a) {
    mc_registry_block_array *effective;
    switch (item->kind) {
        case MC_ITEM_BASE:
            return mc_registry_item_base(classes, item, a->id);
        case MC_ITEM_SHOVEL:
            STAGE("wc", "ny", 0);
            STAGE("wc", "ny", 4);
            effective = classes->items.wc_bk;
            STAGE("wc", "ny", 7);
            TRY(dq_construct(classes, item, a->id, 1, a->tool, effective));
            STAGE("wc", "ny", 10);
            return MC_REGISTRY_OK;
        case MC_ITEM_PICKAXE:
            STAGE("au", "ah", 0);
            STAGE("au", "ah", 4);
            effective = classes->items.au_bk;
            STAGE("au", "ah", 7);
            TRY(dq_construct(classes, item, a->id, 2, a->tool, effective));
            STAGE("au", "ah", 10);
            return MC_REGISTRY_OK;
        case MC_ITEM_AXE:
            STAGE("ta", "ly", 0);
            STAGE("ta", "ly", 4);
            effective = classes->items.ta_bk;
            STAGE("ta", "ly", 7);
            TRY(dq_construct(classes, item, a->id, 3, a->tool, effective));
            STAGE("ta", "ly", 10);
            return MC_REGISTRY_OK;
        case MC_ITEM_FLINT_AND_STEEL:
            TRY(base_prefix(classes, item, a->id, "ye", "pl"));
            STAGE("ye", "pl", 7);
            item->fields.bg = 1;
            STAGE("ye", "pl", 13);
            mc_registry_item_durability(item, 64);
            STAGE("ye", "pl", 17);
            return MC_REGISTRY_OK;
        case MC_ITEM_FOOD:
            return yw_construct(classes, item, a->id, a->integers[0], a->flag);
        case MC_ITEM_BOW:
            TRY(base_prefix(classes, item, a->id, "qz", "kj"));
            STAGE("qz", "kj", 7);
            item->fields.bg = 1;
            STAGE("qz", "kj", 10);
            return MC_REGISTRY_OK;
        case MC_ITEM_COAL:
            TRY(base_prefix(classes, item, a->id, "rx", "lc"));
            STAGE("rx", "lc", 7);
            mc_registry_item_subtypes(item, true);
            STAGE("rx", "lc", 13);
            mc_registry_item_durability(item, 0);
            STAGE("rx", "lc", 17);
            return MC_REGISTRY_OK;
        case MC_ITEM_SWORD: {
            TRY(base_prefix(classes, item, a->id, "qd", "jw"));
            STAGE("qd", "jw", 7);
            item->fields.bg = 1;
            STAGE("qd", "jw", 12);
            if (a->tool == NULL)
                return mc_registry_fail(classes, MC_REGISTRY_NULL_REFERENCE,
                                        "weapon tool durability", NULL);
            int32_t maximum = a->tool->g;
            STAGE("qd", "jw", 15);
            mc_registry_item_durability(item, maximum);
            STAGE("qd", "jw", 22);
            int32_t damage = a->tool->i;
            damage = mc_java_i32_mul(damage, 2);
            damage = mc_java_i32_add(4, damage);
            STAGE("qd", "jw", 28);
            item->subtype.qd.a = damage;
            STAGE("qd", "jw", 31);
            return MC_REGISTRY_OK;
        }
        case MC_ITEM_HOE: {
            TRY(base_prefix(classes, item, a->id, "la", "gz"));
            STAGE("la", "gz", 7);
            item->fields.bg = 1;
            STAGE("la", "gz", 12);
            if (a->tool == NULL)
                return mc_registry_fail(classes, MC_REGISTRY_NULL_REFERENCE, "hoe tool durability",
                                        NULL);
            int32_t maximum = a->tool->g;
            STAGE("la", "gz", 15);
            mc_registry_item_durability(item, maximum);
            STAGE("la", "gz", 19);
            return MC_REGISTRY_OK;
        }
        case MC_ITEM_SEEDS:
            TRY(base_prefix(classes, item, a->id, "rk", "kr"));
            STAGE("rk", "kr", 7);
            item->subtype.rk.a = a->integers[0];
            STAGE("rk", "kr", 10);
            return MC_REGISTRY_OK;
        case MC_ITEM_ARMOR:
            return armor(classes, item, a);
        case MC_ITEM_PAINTING:
            return base_prefix(classes, item, a->id, "ym", "ps");
        case MC_ITEM_SIGN:
            TRY(base_prefix(classes, item, a->id, "vc", "ng"));
            STAGE("vc", "ng", 7);
            item->fields.bg = 1;
            STAGE("vc", "ng", 10);
            return MC_REGISTRY_OK;
        case MC_ITEM_DOOR:
            TRY(base_prefix(classes, item, a->id, "hu", "fg"));
            STAGE("hu", "fg", 7);
            item->subtype.hu.a = a->material;
            STAGE("hu", "fg", 12);
            item->fields.bg = 1;
            STAGE("hu", "fg", 15);
            return MC_REGISTRY_OK;
        case MC_ITEM_BUCKET:
            TRY(base_prefix(classes, item, a->id, "bc", "al"));
            STAGE("bc", "al", 7);
            item->fields.bg = 1;
            STAGE("bc", "al", 12);
            item->subtype.bc.a = a->integers[0];
            STAGE("bc", "al", 15);
            return MC_REGISTRY_OK;
        case MC_ITEM_MINECART:
            TRY(base_prefix(classes, item, a->id, "rl", "ks"));
            STAGE("rl", "ks", 7);
            item->fields.bg = 1;
            STAGE("rl", "ks", 12);
            item->subtype.rl.a = a->integers[0];
            STAGE("rl", "ks", 15);
            return MC_REGISTRY_OK;
        case MC_ITEM_SADDLE:
            TRY(base_prefix(classes, item, a->id, "sa", "lg"));
            STAGE("sa", "lg", 7);
            item->fields.bg = 1;
            STAGE("sa", "lg", 10);
            return MC_REGISTRY_OK;
        case MC_ITEM_REDSTONE:
            return base_prefix(classes, item, a->id, "ie", "fk");
        case MC_ITEM_SNOWBALL:
            TRY(base_prefix(classes, item, a->id, "dm", "cf"));
            STAGE("dm", "cf", 8);
            item->fields.bg = 16;
            STAGE("dm", "cf", 11);
            return MC_REGISTRY_OK;
        case MC_ITEM_BOAT:
            TRY(base_prefix(classes, item, a->id, "vd", "ni"));
            STAGE("vd", "ni", 7);
            item->fields.bg = 1;
            STAGE("vd", "ni", 10);
            return MC_REGISTRY_OK;
        case MC_ITEM_BLOCK_PLACER:
            TRY(base_prefix(classes, item, a->id, "lv", "hn"));
            STAGE("lv", "hn", 7);
            if (a->block == NULL)
                return mc_registry_fail(classes, MC_REGISTRY_NULL_REFERENCE, "seed item Block ID",
                                        NULL);
            {
                int32_t id = a->block->fields.bn;
                STAGE("lv", "hn", 10);
                item->subtype.lv.a = id;
            }
            STAGE("lv", "hn", 13);
            return MC_REGISTRY_OK;
        case MC_ITEM_EGG:
            TRY(base_prefix(classes, item, a->id, "mo", "hx"));
            STAGE("mo", "hx", 8);
            item->fields.bg = 16;
            STAGE("mo", "hx", 11);
            return MC_REGISTRY_OK;
        case MC_ITEM_FISHING_ROD:
            TRY(base_prefix(classes, item, a->id, "cs", "bv"));
            STAGE("cs", "bv", 8);
            mc_registry_item_durability(item, 64);
            STAGE("cs", "bv", 14);
            mc_registry_item_stack_limit(item, 1);
            STAGE("cs", "bv", 18);
            return MC_REGISTRY_OK;
        case MC_ITEM_DYE:
            TRY(base_prefix(classes, item, a->id, "km", "gu"));
            STAGE("km", "gu", 7);
            mc_registry_item_subtypes(item, true);
            STAGE("km", "gu", 13);
            mc_registry_item_durability(item, 0);
            STAGE("km", "gu", 17);
            return MC_REGISTRY_OK;
        case MC_ITEM_BED:
            return base_prefix(classes, item, a->id, "lp", "hk");
        case MC_ITEM_SHEARS:
            TRY(base_prefix(classes, item, a->id, "bl", "as"));
            STAGE("bl", "as", 7);
            mc_registry_item_stack_limit(item, 1);
            STAGE("bl", "as", 15);
            mc_registry_item_durability(item, 238);
            STAGE("bl", "as", 19);
            return MC_REGISTRY_OK;
        case MC_ITEM_RECORD:
            TRY(base_prefix(classes, item, a->id, "tr", "mj"));
            STAGE("tr", "mj", 7);
            item->subtype.tr.a = a->text;
            STAGE("tr", "mj", 12);
            item->fields.bg = 1;
            STAGE("tr", "mj", 15);
            return MC_REGISTRY_OK;
        case MC_ITEM_SOUP:
            STAGE("bz", "bf", 0);
            STAGE("bz", "bf", 4);
            TRY(yw_construct(classes, item, a->id, a->integers[0], false));
            STAGE("bz", "bf", 7);
            return MC_REGISTRY_OK;
        case MC_ITEM_COOKIE:
            STAGE("qk", "jy", 0);
            STAGE("qk", "jy", 4);
            TRY(yw_construct(classes, item, a->id, a->integers[0], a->flag));
            STAGE("qk", "jy", 10);
            item->fields.bg = a->integers[1];
            STAGE("qk", "jy", 13);
            return MC_REGISTRY_OK;
        case MC_ITEM_MAP:
            STAGE("wr", "ol", 0);
            STAGE("wr", "ol", 2);
            TRY(base_prefix(classes, item, a->id, "cp", "bo"));
            STAGE("wr", "ol", 7);
            mc_registry_item_stack_limit(item, 1);
            STAGE("wr", "ol", 11);
            return MC_REGISTRY_OK;
        case MC_ITEM_BLOCK:
            return mc_registry_item_ck(classes, item, a->id);
        case MC_ITEM_WOOL_BLOCK:
            return block_item(classes, item, a->id, "bi", "ap", true);
        case MC_ITEM_LOG_BLOCK:
            return block_item(classes, item, a->id, "go", "ek", true);
        case MC_ITEM_SLAB_BLOCK:
            return block_item(classes, item, a->id, "en", "cw", true);
        case MC_ITEM_SAPLING_BLOCK:
            return block_item(classes, item, a->id, "mr", "ia", true);
        case MC_ITEM_LEAVES_BLOCK:
            return block_item(classes, item, a->id, "uy", "nd", true);
        case MC_ITEM_PISTON_BLOCK:
            return block_item(classes, item, a->id, "qr", "kc", false);
        case MC_ITEM_KIND_COUNT:
            break;
    }
    return mc_registry_fail(classes, MC_REGISTRY_INVALID_ARGUMENT, "Item constructor kind", NULL);
}

mc_registry_status mc_registry_item_construct(mc_registry_classes *classes, mc_item *item,
                                              const mc_item_arguments *arguments) {
    if (classes == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    if (item == NULL || arguments == NULL || item->classes != classes ||
        (unsigned int)item->kind >= (unsigned int)MC_ITEM_KIND_COUNT)
        return mc_registry_fail(classes, MC_REGISTRY_INVALID_ARGUMENT, "Item constructor arguments",
                                NULL);
    if (item->phase != MC_ITEM_RAW)
        return mc_registry_fail(classes, MC_REGISTRY_FAILED_INITIALIZATION,
                                "Item constructor phase", NULL);
    item->requested_argument = arguments->id;
    classes->current_item = item;
    item->phase = MC_ITEM_CONSTRUCTING;
    mc_registry_status status = constructor_body(classes, item, arguments);
    item->phase = status == MC_REGISTRY_OK ? MC_ITEM_CONSTRUCTED : MC_ITEM_CONSTRUCTION_FAILED;
    return status;
}

#undef TRY
#undef STAGE
