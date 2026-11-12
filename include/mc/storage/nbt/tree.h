#ifndef MC_STORAGE_NBT_TREE_H
#define MC_STORAGE_NBT_TREE_H

#include "mc/storage/nbt/nbt.h"

/* Independent immutable provenance owner. This is a native storage operation,
 * not an original gameplay constructor or an mc_nbt_document implementation. */
typedef struct mc_nbt_tree mc_nbt_tree;

/* Validate with mc_nbt_measure, then copy a compound into one exact allocation
 * containing this owner and its aligned tags, UTF-16 spans, arrays and child
 * pointers. NULL limits selects NBT defaults. All normal measure limits apply;
 * max_input_bytes is unused because no encoded input is parsed.
 *
 * Preserve compound/root names, physical order, types, unknown children, raw
 * float/double bits and list metadata, including an empty list's element type.
 * List-child names are ignored by the codec and become {NULL,0}; their pointer
 * and length values are not interpreted. No normal-writer reencoding occurs.
 *
 * The caller supplies a readable immutable acyclic C tree and readable spans
 * for the fields that the NBT codec actually uses. Exclusive access is required
 * through validation and both copy passes. out is required; error is optional.
 * out/error storage must be mutually disjoint and must not overlap limits,
 * input tags, input buffers or any owner supplying those borrowed views.
 *
 * max_allocation_bytes bounds the peak requested heap bytes of this operation:
 * either codec validation scratch, or the new exact owner+tree allocation.
 * Those phases do not overlap. Caller-owned input/old owners, other documents,
 * allocator overhead and the bounded C recursion stack are excluded. A caller
 * combining this operation with its own live allocations must deduct them.
 *
 * No source memory is retained. Input may borrow an existing tree owner, which
 * the caller must keep live until this call returns and free only after a
 * successful replacement if desired. This function never frees an old owner.
 * On every failure *out is unchanged. Core validation errors retain their
 * status/offset; wrapper/planning/allocation errors and success use offset0.
 * A valid noncompound root returns ROOT_TYPE after normal validation.
 * Native allocation failure returns ALLOCATION_FAILED; size/budget overflow
 * returns LIMIT; malformed copied structure returns INVALID_TREE/INVALID_TYPE.
 * Copying a tree does not apply a world or item schema. */
mc_nbt_status mc_nbt_tree_copy_compound(const mc_nbt_tag *compound, const mc_nbt_limits *limits,
                                        mc_nbt_tree **out, mc_nbt_error *error);

/* Immutable borrowed root, valid until this owner is freed. NULL returns NULL. */
const mc_nbt_tag *mc_nbt_tree_root(const mc_nbt_tree *tree);

/* Entire exact requested allocation, including owner/alignment/tree payload.
 * NULL returns zero. This does not include prior codec scratch allocations. */
size_t mc_nbt_tree_allocation_bytes(const mc_nbt_tree *tree);

/* One owner, one free; no implicit retain or synchronization. NULL is safe. */
void mc_nbt_tree_free(mc_nbt_tree *tree);

#endif
