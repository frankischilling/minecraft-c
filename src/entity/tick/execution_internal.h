#ifndef MC_ENTITY_TICK_EXECUTION_INTERNAL_H
#define MC_ENTITY_TICK_EXECUTION_INTERNAL_H

#include "../../core/math/execution_internal.h"
#include "../../world/runtime/world_access_lease_internal.h"
#include "mc/entity/tick/environment.h"
#include "mc/world/cache/acquire.h"
#include "mc/world/storage/source_chunk_loader.h"

#include <stddef.h>
#include <stdint.h>

#define MC_ENTITY_TICK_EXECUTION_RECEIPTS 8u
#define MC_ENTITY_TICK_EXECUTION_LOADERS 2u
#define MC_ENTITY_TICK_EXECUTION_WORLDS MC_ENTITY_TICK_EXECUTION_RECEIPTS

typedef struct mc_entity_tick_execution_site {
    const char *class_name, *method, *descriptor;
    int pc;
} mc_entity_tick_execution_site;

typedef struct mc_entity_tick_execution_error {
    mc_world_storage_status status;
    const char *operation;
    mc_entity_tick_execution_site site;
} mc_entity_tick_execution_error;

typedef enum mc_entity_tick_execution_phase {
    MC_ENTITY_TICK_EXECUTION_OPEN,
    MC_ENTITY_TICK_EXECUTION_FAILED,
    MC_ENTITY_TICK_EXECUTION_RETIRING,
    MC_ENTITY_TICK_EXECUTION_RETIRED
} mc_entity_tick_execution_phase;

struct mc_entity_tick_execution;

/* Ordinary VALUE tokens may be copied. They carry no new occurrence and are
 * valid only under the same stable owner and exact non-reused slot generation.
 * No token contains an unowned World or loader pointer. */
typedef struct mc_entity_tick_execution_world_token {
    const struct mc_entity_tick_execution *owner;
    size_t slot;
    uint64_t generation;
} mc_entity_tick_execution_world_token;

typedef struct mc_entity_tick_execution_loader_attempt {
    mc_entity_tick_execution_world_token world;
    size_t slot;
    uint64_t generation;
} mc_entity_tick_execution_loader_attempt;

typedef struct mc_entity_tick_execution_loader_selection {
    mc_entity_tick_execution_world_token world;
    uint64_t generation;
    mc_chunk_acquire_status status;
} mc_entity_tick_execution_loader_selection;

typedef struct mc_entity_tick_execution_loader_view {
    mc_chunk_acquire_status status;
    /* NULL is an explicit current no-loader outcome, never an old selection. */
    const mc_world_source_chunk_error *failure;
} mc_entity_tick_execution_loader_view;

typedef enum mc_entity_tick_execution_world_slot_phase {
    MC_ENTITY_TICK_EXECUTION_WORLD_FREE,
    MC_ENTITY_TICK_EXECUTION_WORLD_OPEN,
    MC_ENTITY_TICK_EXECUTION_WORLD_RETAINED
} mc_entity_tick_execution_world_slot_phase;

typedef struct mc_entity_tick_execution_world_slot {
    mc_entity_tick_execution_world_slot_phase phase;
    uint64_t generation, selection_generation;
    size_t parent, loader_slot;
    mc_world_runtime *world;
    mc_entity_tick_execution_site site;
    bool held, loader_open, selection_ready;
    mc_chunk_acquire_status selection_status;
} mc_entity_tick_execution_world_slot;

typedef enum mc_entity_tick_execution_loader_slot_phase {
    MC_ENTITY_TICK_EXECUTION_LOADER_FREE,
    MC_ENTITY_TICK_EXECUTION_LOADER_RESERVED,
    MC_ENTITY_TICK_EXECUTION_LOADER_COPIED
} mc_entity_tick_execution_loader_slot_phase;

typedef struct mc_entity_tick_execution_loader_slot {
    mc_entity_tick_execution_loader_slot_phase phase;
    mc_entity_tick_execution_world_token world;
    uint64_t generation;
} mc_entity_tick_execution_loader_slot;

/* Frame-resident, fresh, stable, exclusive and disjoint. Never move/copy this
 * owner or its intrusive leases. Math is the SAME borrowed context and is never
 * retired here. Each evaluated nullable World gets its independent native
 * binding BEFORE subsequent source arguments. NULL is a source value, not an
 * eager invocation failure. Slot i and lease i never move or compact.
 *
 * world/world_held/site mirror the CURRENT open scope. With no open scope they
 * preserve the last retained outer World for existing owner2 component reads.
 * Those fields alone are not the ownership ledger. world_open/current_lease
 * retain the old sound/particle child contract. lease_count counts used slots;
 * it is NOT an array prefix. Failed closed inner scopes remain in world_slots.
 *
 * Exactly the old two full loader copies are precharged. New metadata/tokens
 * are small private bookkeeping; no public/Entity/Mob/TileEntity error grows.
 * Source diagnostics stay in the enclosing child. first/capture/admission/
 * cleanup are neutral values, NEVER current source-loader selectors. */
typedef struct mc_entity_tick_execution {
    const struct mc_entity_tick_execution *identity;
    mc_math_execution *math;
    mc_entity_tick_execution_phase phase;
    mc_world_runtime *world;
    bool world_held, world_open, child_failed, capture_pending;
    mc_entity_tick_execution_site site;
    mc_entity_tick_execution_error first, capture, admission, cleanup;
    mc_world_source_chunk_error loaders[MC_ENTITY_TICK_EXECUTION_LOADERS];
    /* Comparison identities only; never dereferenced during retirement. */
    const mc_world_source_chunk_error *loader_originals[MC_ENTITY_TICK_EXECUTION_LOADERS];
    size_t loader_count;
    mc_world_access_failure receipts[MC_ENTITY_TICK_EXECUTION_RECEIPTS];
    size_t receipt_count;
    mc_world_access_failure_lease leases[MC_ENTITY_TICK_EXECUTION_RECEIPTS];
    size_t lease_count, current_lease;
    mc_entity_tick_execution_world_slot world_slots[MC_ENTITY_TICK_EXECUTION_WORLDS];
    bool lease_used[MC_ENTITY_TICK_EXECUTION_RECEIPTS];
    size_t world_count;
    mc_entity_tick_execution_loader_slot loader_slots[MC_ENTITY_TICK_EXECUTION_LOADERS];
} mc_entity_tick_execution;

void mc_entity_tick_execution_init(mc_entity_tick_execution *, mc_math_execution *);

/* Unchanged top-level source-parent API: begin requires no open scope, end
 * closes the current scope. Existing already-open sound/particle wrappers and
 * mc_world_execution_child_lease need no signature/body migration. */
mc_world_storage_status mc_entity_tick_execution_begin_world(mc_entity_tick_execution *,
                                                             mc_world_runtime *,
                                                             mc_entity_tick_execution_site);
mc_world_access_failure_lease *mc_entity_tick_execution_world_lease(mc_entity_tick_execution *);
mc_world_storage_status mc_entity_tick_execution_end_world(mc_entity_tick_execution *, bool);

/* New nested evaluated-World seam. Push at the ACTUAL receiver load, before
 * Map.values/slice/pooled arguments, not just at invocation. A and B may differ,
 * alias or be NULL. Admission has no source/getter/Math/callback work. All
 * capacity/generation checks precede the binding retain and source invocation.
 * Pop requires the exact current token and checked unused-node preflight;
 * refusal consumes nothing. Fully returning transients release immediately.
 * A failed child closes into retained storage before its local right vanishes.
 * Restoring A after B closes does not change B's fixed node or slot.
 * No output token is published on refusal. */
mc_world_storage_status mc_entity_tick_execution_push_world(mc_entity_tick_execution *,
                                                            mc_world_runtime *,
                                                            mc_entity_tick_execution_site,
                                                            mc_entity_tick_execution_world_token *);
mc_world_storage_status
mc_entity_tick_execution_pop_world(mc_entity_tick_execution *,
                                   const mc_entity_tick_execution_world_token *, bool child_failed);
bool mc_entity_tick_execution_current_world(const mc_entity_tick_execution *,
                                            mc_entity_tick_execution_world_token *);
bool mc_entity_tick_execution_read_world(const mc_entity_tick_execution *,
                                         const mc_entity_tick_execution_world_token *,
                                         mc_world_runtime **);

/* Kernel-only acquisition seam, not yet joined. Reserve BEFORE the ONE actual
 * get/acquire call; two overlapping attempts are admitted by the existing two
 * full-copy slots, a third refuses before source work. Finish uses only that
 * invocation's actual status/source_chunk while its evaluated World is held,
 * BEFORE TileEntity projection or acquisition-frame restore. On OK it ignores
 * the failure argument (the failure-only callee may leave it unwritten).
 * On non-OK NULL is an explicit no-loader outcome; it cannot select an older
 * loader. A nonnull current borrow is copied into its already reserved slot.
 * No allocation, getter, ambient last_error query or post-failure retain runs.
 *
 * Pass the returned selection VALUE explicitly through the selected child arm
 * and lossy projections. An outer attempt can finish with its own no-loader
 * outcome without changing an inner retained token. Only the caller knows
 * which actual child failed: never scan old tokens/union arms or first/capture
 * to choose one. Future kernel signatures must thread this explicit out value.
 * The reserved-slot/World storage and outputs must be mutually disjoint. */
mc_world_storage_status
mc_entity_tick_execution_loader_begin(mc_entity_tick_execution *,
                                      const mc_entity_tick_execution_world_token *,
                                      mc_entity_tick_execution_loader_attempt *);
mc_world_storage_status
mc_entity_tick_execution_loader_finish(mc_entity_tick_execution *,
                                       const mc_entity_tick_execution_loader_attempt *,
                                       mc_chunk_acquire_status actual_status,
                                       const mc_world_source_chunk_error *actual_current_failure,
                                       mc_entity_tick_execution_loader_selection *);
bool mc_entity_tick_execution_loader_read(const mc_entity_tick_execution *,
                                          const mc_entity_tick_execution_loader_selection *,
                                          mc_entity_tick_execution_loader_view *);

/* Compatibility copy for existing owner-only users. It still copies a current
 * supplied live diagnostic without allocating; it does not create an attempt
 * selection and MUST NOT be used to reconstruct a loader after projection.
 * New kernel joins must use reserve/finish/read above. Receipts are protected
 * VALUE tuples under an actual HELD lease, never additional receipt.holds. */
mc_world_storage_status
mc_entity_tick_execution_copy_loader(mc_entity_tick_execution *,
                                     const mc_world_source_chunk_error **selected_slot);
mc_world_storage_status mc_entity_tick_execution_capture_receipt(mc_entity_tick_execution *,
                                                                 const mc_world_access_failure *);

/* Caller retires its new pending effect/query owners and clears all outer
 * borrowed typed views first. These helpers run no callbacks/source work.
 * Checked receipt release clears selected VALUEs before consuming exact fixed
 * nodes; partial cleanup records each consumed slot immediately. Worlds and
 * inline loader copies remain until final retirement. No open scope/attempt
 * may be silently abandoned. Structural refusal keeps remaining actual rights.
 * Current loader tokens expire when their scope succeeds/reuses or retires. */
mc_world_storage_status mc_entity_tick_execution_release_receipts(mc_entity_tick_execution *);
mc_world_storage_status mc_entity_tick_execution_retire(mc_entity_tick_execution *);

/* Still OPEN same-body composition; this package does not implement the base,
 * acquisition/activation/projection, damage, ray, Block or nearby joins. */
bool mc_tick_arrow_base_body_in_execution(mc_entity *, mc_registry_classes *,
                                          mc_entity_tick_execution *, const mc_entity_tick_limits *,
                                          mc_entity_tick_error *);

#endif
