#ifndef MC_ITEM_STACK_STACK_H
#define MC_ITEM_STACK_STACK_H

#include "mc/storage/nbt/nbt.h"

/* Item fields and storage provenance have one stable owner. Registry, world,
 * and item gameplay behavior belong to their respective modules. */
typedef struct mc_item_stack mc_item_stack;

typedef struct mc_item_stack_fields {
    int32_t count, animation, id, damage;
} mc_item_stack_fields;

typedef enum mc_item_stack_phase {
    MC_ITEM_STACK_RAW = 0,
    MC_ITEM_STACK_READY,
    MC_ITEM_STACK_FAILED_CONSTRUCTION
} mc_item_stack_phase;

typedef enum mc_item_stack_status {
    MC_ITEM_STACK_OK = 0,
    MC_ITEM_STACK_INVALID_ARGUMENT,
    MC_ITEM_STACK_INVALID_PHASE,
    MC_ITEM_STACK_INVALID_FIELD,
    MC_ITEM_STACK_ALLOCATION_FAILED,
    MC_ITEM_STACK_LIMIT,
    MC_ITEM_STACK_NBT_ERROR
} mc_item_stack_status;

typedef struct mc_item_stack_error {
    mc_item_stack_status status;
    const char *field; /* Static ASCII key or NULL. */
    mc_nbt_error nbt;
} mc_item_stack_error;

const char *mc_item_stack_status_string(mc_item_stack_status status);

/* Allocate before the original constructor's argument evaluation: one native
 * hold, raw phase, all four int32 fields zero. Failure leaves out unchanged.
 * Item/block adapters dereference actual bf/bn before construct, preserving
 * their reached null failure; no registry/range/max-count check belongs here. */
mc_item_stack_status mc_item_stack_allocate(mc_item_stack **out);
mc_item_stack_status mc_item_stack_construct(mc_item_stack *raw, int32_t id, int32_t count,
                                             int32_t damage);

/* Construction is valid only in RAW. NBT construction writes initial count0,
 * then id/count/damage in order. Any NBT failure permanently marks the owner
 * FAILED_CONSTRUCTION; it cannot be retried as a fresh transaction. Successful
 * fields remain visible through snapshots; release is always supported. */
mc_item_stack_status mc_item_stack_construct_nbt(mc_item_stack *raw, const mc_nbt_tag *compound,
                                                 mc_item_stack_error *error);

/* READY only. Absent fields use zero; present fields require Short id, Byte
 * Count, Short Damage. Each successful assignment precedes the next lookup.
 * Failure retains completed assignments, unchanged remaining fields, animation,
 * phase and storage provenance. Null input fails at the first field, id.
 * Input is an immutable valid NBT tree; no source memory is retained here. */
mc_item_stack_status mc_item_stack_read_nbt(mc_item_stack *stack, const mc_nbt_tag *compound,
                                            mc_item_stack_error *error);

/* Holds are native resources, independent of world/container registration.
 * Retain accepts raw/failed owners and fails unchanged on hold overflow.
 * release(NULL) is safe. All calls require exclusive access; no atomic/thread
 * synchronization is implicit. Results/errors are mutually disjoint and cannot
 * overlap owner, owned provenance, input storage or limits. Input trees may
 * borrow immutable provenance.
 * Snapshots and phase reads accept every phase. */
mc_item_stack_status mc_item_stack_retain(mc_item_stack *stack);
void mc_item_stack_release(mc_item_stack *stack);
mc_item_stack_status mc_item_stack_fields_get(const mc_item_stack *stack,
                                              mc_item_stack_fields *out);
mc_item_stack_status mc_item_stack_phase_get(const mc_item_stack *stack, mc_item_stack_phase *out);
mc_item_stack_status mc_item_stack_count_set(mc_item_stack *stack, int32_t count);
mc_item_stack_status mc_item_stack_damage_set(mc_item_stack *stack, int32_t damage);

/* Exact requested owner allocation before allocation, and checked current
 * owner+source-tree bytes. The latter accepts every live phase, counts one
 * identity once regardless of holds, requires out, and leaves it unchanged on
 * failure. Other owners/documents/allocator overhead are not included. */
size_t mc_item_stack_object_bytes(void);
mc_item_stack_status mc_item_stack_native_bytes(const mc_item_stack *stack, size_t *out);

/* READY source required. Clone allocates a distinct owner with the original
 * id/count/damage, animation0, and no source provenance. Nullable clone alone
 * succeeds with NULL for a NULL input. Split first Java-wrap subtracts amount
 * from source count, THEN allocates its new amount-count owner. Allocation
 * failure retains that decrement and leaves out unchanged. No amount clamp.
 * Source provenance remains with the source identity across both operations. */
mc_item_stack_status mc_item_stack_clone(const mc_item_stack *stack, mc_item_stack **out);
mc_item_stack_status mc_item_stack_clone_nullable(const mc_item_stack *stack, mc_item_stack **out);
mc_item_stack_status mc_item_stack_split(mc_item_stack *stack, int32_t amount, mc_item_stack **out);

/* Explicit EXTRA storage ownership operation, after semantic construction.
 * Validate then deep-copy the complete compound through the concrete NBT tree
 * owner into one exactly sized native allocation, including its owner header
 * and alignment. Preserve compound/root names, order, types, unknown values and
 * raw floating bits. List-child names are ignored by the NBT codec and canonicalized
 * to {NULL,0}; their caller-supplied pointers/lengths are never inspected.
 * On failure old provenance and all fields remain unchanged. Source may borrow
 * this owner's old provenance; the old copy remains live until replacement.
 * source() is an immutable view valid until successful preserve_source/free.
 * A fresh/RAW/failed owner has no source. No NBT constructor silently calls it.
 * max_input_bytes is unused here because no encoded input is parsed. */
mc_item_stack_status mc_item_stack_preserve_source(mc_item_stack *stack, const mc_nbt_tag *compound,
                                                   const mc_nbt_limits *limits,
                                                   mc_item_stack_error *error);
const mc_nbt_tag *mc_item_stack_source(const mc_item_stack *stack);

/* Pure borrowed record assembly, READY only, with no allocation or document.
 * Extra fields are generic container-owned tags (Slot is not item state):
 * valid readable immutable NBT values with unique names disjoint from
 * id/Count/Damage, never End. Null extras is allowed only for count0.
 * pointer_count reports the exact final compound child-pointer count.
 * Overlay replaces known names at retained positions and appends absent item
 * fields then extras in argument order, using Java Short/Byte/Short narrowing.
 * Recursive validity is an input precondition and checked by the outer codec.
 *
 * The final out address MUST stay fixed: root borrows out->item_fields directly,
 * plus extras, child_scratch and the held stack's source. Keep all those spans
 * and an independent stack hold live/immutable until outer codec use ends.
 * Do not copy/move a prepared view without rebasing its embedded references.
 * out/error/count and scratch storage must be mutually disjoint and must not
 * overlap any input/owner/source spans. Result is unchanged on failure; scratch
 * may change. No gameplay mutation, source copy, or container callback occurs. */
typedef struct mc_item_stack_record_view {
    mc_nbt_tag root;
    mc_nbt_tag item_fields[3];
} mc_item_stack_record_view;

mc_item_stack_status mc_item_stack_record_pointer_count(const mc_item_stack *stack,
                                                        const mc_nbt_tag *extra_fields,
                                                        size_t extra_count, size_t *out,
                                                        mc_item_stack_error *error);
mc_item_stack_status
mc_item_stack_record_overlay(const mc_item_stack *stack, const mc_nbt_tag *extra_fields,
                             size_t extra_count, const mc_nbt_tag **child_scratch,
                             size_t scratch_count, mc_item_stack_record_view *out,
                             mc_item_stack_error *error);

/* READY only. Build an independent document with Short id, Byte Count, Short
 * Damage, using Java signed low-bit narrowing. Overlay known fields in their
 * retained positions; preserve unknown children/root name. Fresh owners emit
 * only known fields. This is stronger unknown-field preservation than original
 * fresh chest saves. Slot belongs to the container and must be replaced there.
 * No mutable supplied-compound partial-update equivalence is claimed here.
 *
 * NULL limits selects NBT defaults. max_allocation_bytes bounds this operation's
 * simultaneous requested heap bytes: this owner + its exact old NBT tree
 * allocation (including that tree's owner header and alignment) +
 * overlay pointers + encoded bytes + codec validator/new-document allocation.
 * preserve_source also charges old and new copies while both are live. Caller
 * input trees, other owners/documents, allocator overhead and C call stack are
 * excluded; this is not a global world/process budget. No private NBT document
 * layout is inspected. The result is independent and freed with
 * mc_nbt_document_free. out is required and unchanged on failure; source/fields
 * never change. Native source copies are not normal-writer canonicalized, but
 * this build uses the normal NBT codec and its documented canonicalization. */
mc_item_stack_status mc_item_stack_build(const mc_item_stack *stack, const mc_nbt_limits *limits,
                                         mc_nbt_document **out, mc_item_stack_error *error);

#endif
