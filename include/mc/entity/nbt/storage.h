#ifndef MC_ENTITY_NBT_STORAGE_H
#define MC_ENTITY_NBT_STORAGE_H

#include "mc/entity/nbt/factory.h"

/* EXTRA native storage operation after an actual successful ItemEntity NBT
 * read, not a semantic reader or constructor. The caller identifies that same
 * input Compound and its actual current, successfully read READY Stack. Copy
 * the complete Entity Compound and attach its Item Compound to that same Stack
 * through the real Stack provenance owner. Missing Item uses an empty Compound.
 * No source field, ID, Random, Class phase/accounting, World binding or hold is
 * changed. The Entity's independent source is freed only on replacement or its
 * final hold. Both old attached sources stay unchanged on every failure.
 *
 * NULL limits selects NBT defaults. max_allocation_bytes bounds the entire
 * operation's simultaneous requested heap bytes: this Entity's direct bytes,
 * its old source tree, its current Stack plus that Stack's old source tree,
 * and each new tree/validation scratch while it overlaps those live owners.
 * Existing tree-copy validation and exact allocation use the residual ceiling;
 * neither encoded length nor a private document layout estimates tree size.
 * The Entity and Stack copies are separate allocations, even for equal input.
 * Borrowed input, Class/World/profile/other owners, allocator overhead and the
 * bounded C stack are excluded. These native budgets do not change the direct
 * EntityClass constructor ledger or claim a whole-World/process heap bound.
 *
 * Input may borrow either old source and is kept live until all copies finish.
 * Entity, current Stack, input and dependencies remain live/exclusive through
 * return. Inputs/limits/error are disjoint from mutable owner storage. A shared
 * current Stack deliberately receives the new source under that SAME identity.
 * Unknown names/order/types/raw floating bits survive the independent copies;
 * the tree contract canonicalizes ignored list-child names to empty.
 * Native provenance errors have no invented original source bytecode. */
mc_entity_nbt_status mc_entity_preserve_source(mc_entity *entity, const mc_nbt_tag *compound,
                                               const mc_nbt_limits *limits,
                                               mc_entity_nbt_error *error);

/* Immutable borrowed Entity source, valid until successful preservation or
 * final release. NULL Entity returns NULL. No Class/phase active use occurs. */
const mc_nbt_tag *mc_entity_storage_source(const mc_entity *entity);

/* Checked direct Entity bytes + its actual source allocation + actual current
 * Stack/native source bytes, once per identity in this one-Entity graph. Holds
 * do not multiply allocations. NULL Stack contributes zero; every live Entity
 * and Stack phase is accepted. out is required and unchanged on failure.
 * This adds an inspector; mc_entity_native_bytes and its direct ledger retain
 * their existing meaning. Other Entities/containers may share the same Stack;
 * callers combining graphs must deduplicate that real allocation themselves. */
mc_entity_nbt_status mc_entity_storage_native_bytes(const mc_entity *entity, size_t *out,
                                                    mc_entity_nbt_error *error);

/* Actual semantic Boolean save FIRST, then EXTRA native storage overlay. The
 * same source class-name/dead decision and late Stack writer remain in force.
 * No READY-only Stack overlay/phase gate is called: supported RAW/failed Stack
 * fields accepted by the semantic writer remain accepted here. A true result
 * overlays the generated fields over the Entity source, and the generated
 * current Item over only the CURRENT Stack source. Replacing a Stack without
 * provenance therefore cannot resurrect the Entity's old Item unknown fields.
 * Retained positions/root name and unknown fields survive; missing generated
 * names append in generated order. This preservation policy is stronger than
 * original fresh-compound saves, not original byte/order equivalence.
 *
 * The budget includes storage_native_bytes plus the semantic writer's actual
 * intermediate peak, then its returned tree, overlay pointer scratch, and the
 * final independent tree/validation scratch while all remain live. Other held
 * caller trees/owners remain excluded as above and must be deducted by an outer
 * composite caller. Pure checked byte measurement selects the residual before
 * the semantic call; no provenance validation/allocation precedes that call.
 * A normal false needs no storage assembly and changes only serialized, even
 * when the retained live graph exceeds the supplied operation ceiling. Actual
 * name/namespace effects still occur; a true build has no remaining allocation
 * budget in that case and fails. Native overlay failures have fresh zero PCs;
 * semantic failure returns its whole existing child unchanged.
 *
 * out/serialized are required, unchanged on every failure; normal false writes
 * only serialized=false. Success true transfers one independent tree, freed by
 * mc_nbt_tree_free. error is optional and fresh on every call. All outputs,
 * limits and owners are mutually disjoint; caller keeps Entity, current Stack
 * and their sources live/exclusive until return. No source mutation, provenance
 * attachment, Chunk/World publication, disk write, journal or cleanup occurs. */
mc_entity_nbt_status mc_entity_save_storage_nbt(mc_entity *entity, const mc_nbt_limits *limits,
                                                bool *serialized, mc_nbt_tree **out,
                                                mc_entity_nbt_error *error);

#endif
