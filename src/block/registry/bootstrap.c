#include "registry_internal.h"

/* The actual uu/na tail follows the 96 constructor chains. Captured operands
 * survive nested active use; later getstatic instructions read the live cells
 * again. No final Item table is synthesized when recursion sees partial state. */
#define AT(pc) mc_registry_initializer_stage(classes, (pc))
#define TRY(expression)                                                                            \
    do {                                                                                           \
        mc_registry_status status_ = (expression);                                                 \
        if (status_ != MC_REGISTRY_OK)                                                             \
            return status_;                                                                        \
    } while (0)

static mc_registry_status block_id(mc_registry_classes *classes, mc_block *block, int32_t *out)
{
    if (block == NULL)
        return mc_registry_fail(classes, MC_REGISTRY_NULL_REFERENCE, "bootstrap Block ID", NULL);
    *out = block->fields.bn;
    return MC_REGISTRY_OK;
}

static mc_registry_status block_slot(mc_registry_classes *classes, int32_t index, mc_block **out)
{
    mc_block **array = classes->blocks.m;
    if (array == NULL)
        return mc_registry_fail(classes, MC_REGISTRY_NULL_REFERENCE, "bootstrap Block array", NULL);
    if (index < 0 || index >= (int32_t)MC_REGISTRY_BLOCK_SLOTS)
        return mc_registry_fail(classes, MC_REGISTRY_INDEX_OUT_OF_RANGE, "bootstrap Block index",
                                NULL);
    *out = array[(size_t)index];
    return MC_REGISTRY_OK;
}

static mc_registry_status explicit_item(mc_registry_classes *classes, int32_t pc, const char *field,
                                        mc_item_kind kind, const char *name)
{
    AT(pc);
    TRY(mc_registry_items_active_use(classes));
    mc_registry_item_array *destination = classes->items.c;
    mc_block *block = NULL;
    AT(pc + 3);
    TRY(mc_registry_block_static_load(classes, field, &block));
    int32_t index = 0;
    AT(pc + 6);
    TRY(block_id(classes, block, &index));
    AT(pc + 9);
    mc_item *item = NULL;
    TRY(mc_registry_item_allocate(classes, kind, &item));
    AT(pc + 13);
    TRY(mc_registry_block_static_load(classes, field, &block));
    int32_t id = 0;
    AT(pc + 16);
    TRY(block_id(classes, block, &id));
    id = mc_java_i32_sub(id, 256);
    AT(pc + 23);
    const mc_item_arguments arguments = {.id = id};
    TRY(mc_registry_item_construct(classes, item, &arguments));
    if (name != NULL) {
        AT(pc + 26);
        const mc_registry_string *literal = NULL;
        TRY(mc_registry_literal_ascii(classes, name, &literal));
        /* The source LDC identity is retained in the owner's literal pool.
         * The actual setter constructs a distinct item.+suffix String. */
        (void)literal;
        AT(pc + 28);
        TRY(mc_registry_item_name(classes, item, name));
        AT(pc + 31);
    } else
        AT(pc + 26);
    TRY(mc_registry_item_array_store(classes, destination, index, item));
    classes->current_item = NULL;
    return MC_REGISTRY_OK;
}

/* pt/yq.a(III)V writes a[index] at6 before even reading b at8. Both actual
 * constructor arrays have 256 cells; a later failure retains the first store. */
static mc_registry_status fire_values(mc_registry_classes *classes, mc_block *fire, int32_t index,
                                      int32_t encouragement, int32_t flammability)
{
    int32_t *first = fire->subtype.yq.a;
    if (first == NULL)
        return mc_registry_fail(classes, MC_REGISTRY_NULL_REFERENCE, "fire a(III) array a at6",
                                NULL);
    if (index < 0 || index >= (int32_t)MC_REGISTRY_BLOCK_SLOTS)
        return mc_registry_fail(classes, MC_REGISTRY_INDEX_OUT_OF_RANGE, "fire a(III) index at6",
                                NULL);
    first[(size_t)index] = encouragement;
    int32_t *second = fire->subtype.yq.b;
    if (second == NULL)
        return mc_registry_fail(classes, MC_REGISTRY_NULL_REFERENCE, "fire a(III) array b at13",
                                NULL);
    second[(size_t)index] = flammability;
    return MC_REGISTRY_OK;
}

static mc_registry_status fire_entry(mc_registry_classes *classes, mc_block *fire,
                                     const char *field, int32_t encouragement, int32_t flammability)
{
    mc_block *block = NULL;
    TRY(mc_registry_block_static_load(classes, field, &block));
    int32_t id = 0;
    TRY(block_id(classes, block, &id));
    return fire_values(classes, fire, id, encouragement, flammability);
}

static mc_registry_status block_post_item(mc_registry_classes *classes, mc_block *block)
{
    if (block == NULL)
        return mc_registry_fail(classes, MC_REGISTRY_NULL_REFERENCE, "Block post-Item receiver",
                                NULL);
    if (block->classes != classes || (unsigned int)block->kind >= MC_BLOCK_KIND_COUNT)
        return mc_registry_fail(classes, MC_REGISTRY_INVALID_ARGUMENT, "Block post-Item kind",
                                NULL);
    /* All other registered kinds select the actual uu.k/na.h ()V return0.
     * Only fire overrides that selected method in this closed class set. */
    if (block->kind != MC_BLOCK_YQ_PT)
        return MC_REGISTRY_OK;
    TRY(fire_entry(classes, block, "y", 5, 20));
    TRY(fire_entry(classes, block, "ba", 5, 20));
    TRY(fire_entry(classes, block, "au", 5, 20));
    TRY(fire_entry(classes, block, "K", 5, 5));
    TRY(fire_entry(classes, block, "L", 30, 60));
    TRY(fire_entry(classes, block, "ao", 30, 20));
    TRY(fire_entry(classes, block, "an", 15, 100));
    TRY(fire_entry(classes, block, "Y", 60, 100));
    TRY(fire_entry(classes, block, "ac", 30, 60));
    return MC_REGISTRY_OK;
}

mc_registry_status mc_registry_finish_blocks(mc_registry_classes *classes)
{
    TRY(explicit_item(classes, 3398, "ac", MC_ITEM_BI_AP, "cloth"));
    TRY(explicit_item(classes, 3430, "K", MC_ITEM_GO_EK, "log"));
    TRY(explicit_item(classes, 3462, "al", MC_ITEM_EN_CW, "stoneSlab"));
    TRY(explicit_item(classes, 3494, "z", MC_ITEM_MR_IA, "sapling"));
    TRY(explicit_item(classes, 3526, "L", MC_ITEM_UY_ND, "leaves"));
    TRY(explicit_item(classes, 3558, "aa", MC_ITEM_QR_KC, NULL));
    TRY(explicit_item(classes, 3585, "W", MC_ITEM_QR_KC, NULL));
    for (int32_t index = 0; index < (int32_t)MC_REGISTRY_BLOCK_SLOTS; ++index) {
        AT(3621);
        TRY(mc_registry_blocks_active_use(classes));
        AT(3625);
        mc_block *block = NULL;
        TRY(block_slot(classes, index, &block));
        if (block == NULL)
            continue;
        AT(3629);
        TRY(mc_registry_items_active_use(classes));
        mc_registry_item_array *observed = classes->items.c;
        mc_item *existing = NULL;
        AT(3633);
        TRY(mc_registry_item_array_load(classes, observed, index, &existing));
        if (existing != NULL)
            continue;
        AT(3637);
        TRY(mc_registry_items_active_use(classes));
        mc_registry_item_array *destination = classes->items.c;
        AT(3641);
        mc_item *item = NULL;
        TRY(mc_registry_item_allocate(classes, MC_ITEM_CK_BK, &item));
        const mc_item_arguments arguments = {.id = mc_java_i32_sub(index, 256)};
        AT(3650);
        TRY(mc_registry_item_construct(classes, item, &arguments));
        AT(3653);
        TRY(mc_registry_item_array_store(classes, destination, index, item));
        classes->current_item = NULL;
        AT(3654);
        TRY(mc_registry_blocks_active_use(classes));
        AT(3658);
        TRY(block_slot(classes, index, &block));
        AT(3659);
        TRY(block_post_item(classes, block));
    }
    AT(3668);
    TRY(mc_registry_blocks_active_use(classes));
    bool *array = classes->blocks.r;
    AT(3673);
    if (array == NULL)
        return mc_registry_fail(classes, MC_REGISTRY_NULL_REFERENCE, "Block r array", NULL);
    array[0] = true;
    AT(3674);
    return mc_registry_fail(classes, MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY,
                            "Block initializer requires actual statistics/resource bootstrap",
                            NULL);
}

#undef TRY
#undef AT
