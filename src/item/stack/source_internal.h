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

/* Actual iz.k(): allocate at0 before reading id5/count9/damage13, then
 * numeric construct16. Source is live/exclusive in every phase, with an
 * independent hold across the allocator. Neither phase nor provenance is
 * changed; the copy is a fresh READY owner with animation0/no provenance.
 * Public clone retains its separate READY admission. */
mc_item_stack_status mc_item_stack_source_clone(const mc_item_stack *source,
                                                mc_item_stack_source_allocator allocate,
                                                mc_item_stack **out,
                                                mc_item_stack_source_site *site);

/* Live nonnull exclusive owners in all phases; phase/provenance unchanged.
 * These are private source field/kernel operations, not wider public READY
 * APIs. split reads original id15/damage20 only AFTER the actual allocation,
 * constructs23, and preserves out on failure and the earlier count store. */
mc_item_stack_status mc_item_stack_source_count_get(const mc_item_stack *stack, int32_t *out);
mc_item_stack_status mc_item_stack_source_count_set(mc_item_stack *stack, int32_t count);
/* Phase-transparent source iz.b(I) field store; provenance/phase unchanged. */
mc_item_stack_status mc_item_stack_source_damage_set(mc_item_stack *stack, int32_t damage);
/* Exact live animation field store and iz.a inventory-update prefix. Neither
 * inserts a constructor/READY gate or touches id/count/damage/provenance.
 * The prefix rereads positive b and stores b-1 at14 before Item active use.
 * stored is required, disjoint and published only after a valid owner call. */
mc_item_stack_status mc_item_stack_source_animation_set(mc_item_stack *stack, int32_t animation);
mc_item_stack_status mc_item_stack_source_inventory_animation_tick(mc_item_stack *stack,
                                                                   bool *stored);
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
