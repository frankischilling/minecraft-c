#ifndef MC_ITEM_STACK_SOURCE_INTERNAL_H
#define MC_ITEM_STACK_SOURCE_INTERNAL_H

#include "mc/item/stack/stack.h"

#include <stdbool.h>

/* Side-neutral explicit source sites: Stack has no game-loader/profile owner.
 * The concrete caller maps selected method kinds to its iz/fy class context. */
typedef struct mc_item_stack_source_site {
    bool selected, entered;
    uint16_t bytecode;
} mc_item_stack_source_site;

typedef enum mc_item_stack_source_constructor_kind {
    MC_ITEM_STACK_SOURCE_CONSTRUCTOR_NONE = 0,
    MC_ITEM_STACK_SOURCE_CONSTRUCTOR_NUMERIC,
    MC_ITEM_STACK_SOURCE_CONSTRUCTOR_NBT
} mc_item_stack_source_constructor_kind;

typedef struct mc_item_stack_source_context {
    mc_item_stack_source_constructor_kind constructor_kind;
    mc_item_stack_source_site split, constructor, read;
} mc_item_stack_source_context;

typedef struct mc_item_stack_source_error {
    mc_item_stack_source_context context;
    mc_item_stack_error cause;
} mc_item_stack_source_error;

/* Same concrete allocation contract as mc_item_stack_allocate. This is an
 * explicit per-operation dependency, never a global hook. Production passes
 * the real allocator. A native refusal fixture may return a failure without
 * publishing an owner; successful calls must return a real independent RAW
 * Stack hold. Called only at reached new10 after the source count store7. */
typedef mc_item_stack_status (*mc_item_stack_source_allocator)(mc_item_stack **out);

/* Live nonnull exclusive owners in all phases; phase/provenance unchanged.
 * These are private source field/kernel operations, not wider public READY
 * APIs. split reads original id15/damage20 only AFTER the actual allocation,
 * constructs23, and preserves out on failure and the earlier count store. */
mc_item_stack_status mc_item_stack_source_count_get(const mc_item_stack *stack, int32_t *out);
mc_item_stack_status mc_item_stack_source_count_set(mc_item_stack *stack, int32_t count);
mc_item_stack_status mc_item_stack_source_split(mc_item_stack *stack, int32_t amount,
                                                mc_item_stack_source_allocator allocate,
                                                mc_item_stack **out,
                                                mc_item_stack_source_error *error);

/* RAW-only NBT constructor kernel behind the unchanged public constructor.
 * Exact count0/read order, full Stack cause and explicit constructor11/read
 * getters4/14/24. No field-string inference or Entity/side dependency. */
mc_item_stack_status mc_item_stack_source_construct_nbt(mc_item_stack *raw,
                                                        const mc_nbt_tag *compound,
                                                        mc_item_stack_source_error *error);

#endif
