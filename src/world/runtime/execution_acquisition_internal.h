#ifndef MC_WORLD_RUNTIME_EXECUTION_ACQUISITION_INTERNAL_H
#define MC_WORLD_RUNTIME_EXECUTION_ACQUISITION_INTERNAL_H

#include "../../entity/tick/execution_internal.h"
#include "mc/world/runtime/acquisition_error.h"

/* Prospective PRIVATE native seams only. No get/acquire, initializer, provider,
 * callback or source argument evaluation is implemented here. The kernel must
 * call its SAME actual body exactly once between before/after. The real source
 * result/cause stays independent from this helper's native ownership status.
 * Actual kernel/activation/TileEntity joins are OPEN, not successful stubs. */
static inline mc_world_storage_status
mc_world_execution_acquisition_before(mc_entity_tick_execution *execution,
                                      mc_world_runtime *evaluated_world,
                                      mc_entity_tick_execution_loader_attempt *attempt) {
    if (execution == NULL || attempt == NULL)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    mc_entity_tick_execution_world_token current;
    mc_world_runtime *owned = NULL;
    if (!mc_entity_tick_execution_current_world(execution, &current) ||
        !mc_entity_tick_execution_read_world(execution, &current, &owned) ||
        owned != evaluated_world) {
        execution->admission = (mc_entity_tick_execution_error){
            MC_WORLD_STORAGE_UNSUPPORTED_STATE,
            "Acquisition exact current evaluated World ownership", execution->site};
        return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    }
    return mc_entity_tick_execution_loader_begin(execution, &current, attempt);
}

/* Call at the selected kernel boundary BEFORE restoring its scoped acquisition
 * frame or projecting it into mc_live_tile_entity_error. OK's failure-only
 * frame is never inspected. On failure require the full CURRENT returned frame
 * and status to agree; never query a loader's ambient last_error to infer it.
 *
 * Pass selected_child only for the actual reached failed child arm. In that
 * case finish THIS reservation with explicit NONE and forward the exact child
 * token, even if the full parent frame also forwards that child's raw pointer.
 * Do not recopy B's loader under A's World or infer a producer from that pointer.
 * Without a selected child, source_chunk must be this kernel's actual DIRECT
 * loader result, selected while its evaluated World owner is held.
 * Ending an outer reservation never changes the inner token/owner. Choosing
 * the actual reached child arm is a caller join, not an owner-wide search. */
static inline mc_world_storage_status mc_world_execution_acquisition_after(
    mc_entity_tick_execution *execution, const mc_entity_tick_execution_loader_attempt *attempt,
    mc_chunk_acquire_status actual_status, const mc_world_runtime_acquisition_error *actual_frame,
    const mc_entity_tick_execution_loader_selection *selected_child,
    mc_entity_tick_execution_loader_selection *selection) {
    if (execution == NULL || selection == NULL)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    if (actual_status != MC_CHUNK_ACQUIRE_OK &&
        (actual_frame == NULL || actual_frame->status != actual_status)) {
        execution->admission = (mc_entity_tick_execution_error){
            MC_WORLD_STORAGE_UNSUPPORTED_STATE,
            "Acquisition current status/frame selection mismatch", execution->site};
        return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    }
    mc_entity_tick_execution_loader_selection child = {0}, direct;
    if (selected_child != NULL) {
        mc_entity_tick_execution_loader_view view;
        child = *selected_child; /* VALUE only; supports an aliased forwarding out. */
        if (actual_status == MC_CHUNK_ACQUIRE_OK ||
            !mc_entity_tick_execution_loader_read(execution, &child, &view) ||
            view.status == MC_CHUNK_ACQUIRE_OK) {
            execution->admission = (mc_entity_tick_execution_error){
                MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                "Acquisition explicit failed child selection ownership", execution->site};
            return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
        }
    }
    const mc_world_storage_status status = mc_entity_tick_execution_loader_finish(
        execution, attempt, actual_status,
        actual_status == MC_CHUNK_ACQUIRE_OK || selected_child != NULL ? NULL
                                                                       : actual_frame->source_chunk,
        &direct);
    if (status == MC_WORLD_STORAGE_OK)
        *selection = selected_child == NULL ? direct : child;
    return status;
}

#endif
