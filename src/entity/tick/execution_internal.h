#ifndef MC_ENTITY_TICK_EXECUTION_INTERNAL_H
#define MC_ENTITY_TICK_EXECUTION_INTERNAL_H

#include "../../core/math/execution_internal.h"
#include "../../world/runtime/world_access_lease_internal.h"
#include "mc/entity/tick/environment.h"
#include "mc/world/storage/source_chunk_loader.h"

#include <stddef.h>
#include <stdint.h>

#define MC_ENTITY_TICK_EXECUTION_RECEIPTS 8u
#define MC_ENTITY_TICK_EXECUTION_LOADERS 2u

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

/* Prospective frame-resident owner. Fresh, stable, exclusive and disjoint
 * storage only: neither this frame nor its intrusive lease nodes may be copied
 * or moved. math borrows the SAME enclosing context and is never retired here.
 * One actual evaluated World is owned independently from mutable Entity.world.
 * NULL preserves the source receiver; it is not a native preflight failure.
 * Source/child errors remain in the enclosing call. first records only the
 * first neutral owner/capture failure; admission and cleanup cannot replace it.
 * The selected-view/lease and inline loader arrays are precharged here.
 * World/effect dispatch must still thread the protected handoff below every
 * selected diagnostic. Unprotected input is an OPEN integration failure. */
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
    /* Protected VALUE views, not additional receipt.holds occurrences.
     * Each tuple is borrowed under an exact owned HELD lease below. */
    mc_world_access_failure receipts[MC_ENTITY_TICK_EXECUTION_RECEIPTS];
    size_t receipt_count;
    mc_world_access_failure_lease leases[MC_ENTITY_TICK_EXECUTION_RECEIPTS];
    size_t lease_count, current_lease;
} mc_entity_tick_execution;

void mc_entity_tick_execution_init(mc_entity_tick_execution *, mc_math_execution *);
/* Immediately after the source receiver load, before intervening arguments,
 * initializers, draws or callbacks. Admission refusal owns no new occurrence. */
mc_world_storage_status mc_entity_tick_execution_begin_world(mc_entity_tick_execution *,
                                                             mc_world_runtime *,
                                                             mc_entity_tick_execution_site);
/* Pass this exact precharged node through the actual same-body World dispatch.
 * A NULL result is native admission failure, NEVER a request for legacy mode.
 * Source success leaves it IDLE; actual concrete callback failure transfers
 * the current receiver occurrence before the dispatcher's temporary release. */
mc_world_access_failure_lease *mc_entity_tick_execution_world_lease(mc_entity_tick_execution *);
/* Called while the actual source producer is live/exclusive and the evaluated
 * World scope is still open. Rewire a selected loader borrow to inline storage.
 * Concrete receipts MUST match an inspected HELD lease. Copy the protected
 * VALUE only: no extra receipt retain, counter change or capture allocation.
 * capture_pending means an unprotected/uncopied diagnostic.
 * Such input stores no raw pending view and remains fail-closed; callers must
 * suppress outer typed borrows and cannot claim complete owner integration. */
mc_world_storage_status
mc_entity_tick_execution_copy_loader(mc_entity_tick_execution *,
                                     const mc_world_source_chunk_error **selected_slot);
mc_world_storage_status mc_entity_tick_execution_capture_receipt(mc_entity_tick_execution *,
                                                                 const mc_world_access_failure *);
/* Success releases its transient World promptly. Failure closes the source
 * scope, forbids replay and keeps actual owned slots for native retirement.
 * world_open is independent of NULL/world_held, including source-NULL failure. */
mc_world_storage_status mc_entity_tick_execution_end_world(mc_entity_tick_execution *, bool);
/* No callback/source work. The caller first retires pending effect work while
 * these rights remain held, then clears every borrowed error/view that expires
 * with them. This stage validates/clears protected selected views BEFORE
 * releasing exact lease nodes. It never failure_releases an uncaptured VALUE.
 * World/inline loaders stay for remaining cleanup; checked refusal retains
 * unconsumed nodes. No borrowed pointer is retried to acquire a right. */
mc_world_storage_status mc_entity_tick_execution_release_receipts(mc_entity_tick_execution *);
mc_world_storage_status mc_entity_tick_execution_retire(mc_entity_tick_execution *);

/* Required SAME-body composition; not implemented by this owner-only package.
 * Actual World/receipt hooks below inherited base helpers and execution->math
 * must preserve precise FD4/SN.U138/191 active uses and receiver loads. */
bool mc_tick_arrow_base_body_in_execution(mc_entity *, mc_registry_classes *,
                                          mc_entity_tick_execution *, const mc_entity_tick_limits *,
                                          mc_entity_tick_error *);

#endif
