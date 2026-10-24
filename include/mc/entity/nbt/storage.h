#ifndef MC_ENTITY_NBT_STORAGE_H
#define MC_ENTITY_NBT_STORAGE_H

#include "mc/entity/nbt/factory.h"

/* EXTRA native storage operation after an actual successful supported Entity
 * NBT read, not a semantic reader or constructor. Copy the same complete input
 * Entity Compound. Item additionally attaches its Item Compound to the actual
 * current successfully read READY Stack through that Stack's provenance owner;
 * missing Item uses an empty Compound. FallingSand owns an independent Entity
 * source only: no Item getter, Stack requirement or Stack attachment occurs.
 * Minecart has a separate occurrence policy below, with no Stack attachment.
 * No source field, ID, Random, Class phase/accounting, World binding or hold is
 * changed. The Entity's independent source is freed only on replacement or its
 * final hold. Both old attached sources stay unchanged on every failure.
 *
 * NULL limits selects NBT defaults. max_allocation_bytes bounds the entire
 * operation's simultaneous requested heap bytes: this Entity's direct bytes,
 * its old source tree, its current Stack plus that Stack's old source tree,
 * and each new tree/validation scratch while it overlaps those live owners.
 * FallingSand has no current Stack allocation and charges its actual Entity
 * and old/new Entity trees without a fictional Stack or Item source graph.
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
/* Minecart preservation is the same explicit post-successful-read native
 * operation. Intervening inventory edits are allowed: it does not enforce
 * historical Stack identity/ID/count equality. Copy the complete raw root for
 * inspection; for Type1 map each slot's LAST accepted in-range input record
 * to its CURRENT nonnull occurrence. Missing Slot is0, Byte is masked255,
 * and the Type1 source read range is0..26. Current h remains its actual27/36.
 * A Stack receiving a record must be READY; byte inspection and semantic save
 * still accept all live phases. Type0/2/other replace only the root and retain
 * the existing h occurrence metadata without an unrelated Stack-phase check.
 * All root/occurrence attachments replace atomically after complete prepare.
 * A single immutable root generation backs the per-occurrence borrowed tags;
 * root and h may retain different generations. No Stack.source is read as a
 * fallback or changed, including when two slots alias one Stack. Set (even
 * self-set), clear and whole take invalidate that occurrence's metadata;
 * partial split retains the original record, while the new Stack gets none.
 * Detached h retains its metadata through every borrowing semantic input use.
 * The complete raw root still contains ignored/overwritten records, but those
 * records are not replayed by storage save. No original unknown-field or
 * mutable-compound identity preservation is claimed by this native extension. */
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
 * callers combining graphs must deduplicate that real allocation themselves.
 * Minecart instead includes every distinct nonnull h Stack (and its own source)
 * once, its sidecar, and each distinct shared generation header/tree once.
 * The borrowed generic root mirror does not double-charge the same tree.
 * NULL h contributes no slots; actual h/header/composite requests already
 * belong to direct Entity bytes. Optional generation/sidecar/tree allocations
 * stay outside the EntityClass constructor ledger. This remains a one-Entity
 * operation scope, not an aggregate World/process allocation inspector. */
mc_entity_nbt_status mc_entity_storage_native_bytes(const mc_entity *entity, size_t *out,
                                                    mc_entity_nbt_error *error);

/* Actual semantic Boolean save FIRST, then EXTRA native storage overlay. The
 * same source class-name/dead decision and late Stack writer remain in force.
 * No READY-only Stack overlay/phase gate is called: supported RAW/failed Stack
 * fields accepted by the semantic writer remain accepted here. A true result
 * overlays the generated fields over the Entity source, and the generated
 * current Item over only the CURRENT Stack source. FallingSand overlays the
 * generated base+"Tile" carried-block-ID record directly onto its own Entity source, without a
 * Stack overlay; unknown fields remain native preservation, even an unknown
 * field whose name happens to be Item. Replacing an ItemEntity Stack without
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
 * Minecart also runs that semantic Boolean writer FIRST, retaining its actual
 * name/dead/false/failure behavior and actual h27/36 traversal in all phases.
 * A true result overlays each generated record with only that current slot's
 * retained winner metadata; no Stack.source or archived-root Items fallback
 * occurs. Emit only generated current nonnull records, never earlier duplicate
 * or skipped input records. Preserve unknown winner children and overwrite
 * Slot/id/Count/Damage. At root, Type is overwritten; inactive own-known names
 * Items/PushX/PushZ/Fuel are removed when the semantic result omits them.
 * Other unknown root names and retained positions survive. The full live
 * deduplicated graph, semantic intermediate peak/result, all pointer scratch
 * and final tree/validation peak are charged simultaneously where they overlap.
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
