#ifndef MC_ITEM_USE_METADATA_INTERNAL_H
#define MC_ITEM_USE_METADATA_INTERNAL_H

#include "../registry/registry_internal.h"

/* Entered GM.b(I)I and its six overrides. The caller owns the actual Item
 * receiver and preceding argument evaluation; these bodies read no fields. */
static inline bool mc_item_placement_metadata(mc_item_kind kind, int32_t damage, int32_t *out) {
    switch (kind) {
        case MC_ITEM_WOOL_BLOCK:
        case MC_ITEM_SLAB_BLOCK:
        case MC_ITEM_LOG_BLOCK:
        case MC_ITEM_SAPLING_BLOCK:
            *out = damage; /* BI/EN/GO/MR: iload_1, ireturn1. */
            return true;
        case MC_ITEM_PISTON_BLOCK:
            *out = 7; /* QR: bipush7, ireturn2. */
            return true;
        case MC_ITEM_LEAVES_BLOCK:
            *out = damage | INT32_C(8); /* UY: ior3, ireturn4. */
            return true;
        case MC_ITEM_BASE:
        case MC_ITEM_SHOVEL:
        case MC_ITEM_PICKAXE:
        case MC_ITEM_AXE:
        case MC_ITEM_FLINT_AND_STEEL:
        case MC_ITEM_FOOD:
        case MC_ITEM_BOW:
        case MC_ITEM_COAL:
        case MC_ITEM_SWORD:
        case MC_ITEM_HOE:
        case MC_ITEM_SEEDS:
        case MC_ITEM_ARMOR:
        case MC_ITEM_PAINTING:
        case MC_ITEM_SIGN:
        case MC_ITEM_DOOR:
        case MC_ITEM_BUCKET:
        case MC_ITEM_MINECART:
        case MC_ITEM_SADDLE:
        case MC_ITEM_REDSTONE:
        case MC_ITEM_SNOWBALL:
        case MC_ITEM_BOAT:
        case MC_ITEM_BLOCK_PLACER:
        case MC_ITEM_EGG:
        case MC_ITEM_FISHING_ROD:
        case MC_ITEM_DYE:
        case MC_ITEM_BED:
        case MC_ITEM_SHEARS:
        case MC_ITEM_RECORD:
        case MC_ITEM_SOUP:
        case MC_ITEM_COOKIE:
        case MC_ITEM_MAP:
        case MC_ITEM_BLOCK:
            *out = 0; /* Independently resolved inherited GM body, ireturn1. */
            return true;
        default:
            return false;
    }
}

#endif
