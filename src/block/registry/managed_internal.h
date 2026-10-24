#ifndef MC_BLOCK_REGISTRY_MANAGED_INTERNAL_H
#define MC_BLOCK_REGISTRY_MANAGED_INTERNAL_H

#include "mc/block/registry/registry.h"

#include <stdbool.h>
#include <stddef.h>

/* Internal reclaimable storage for an exclusively used, live Registry. No
 * class use, borrow, callback or last_error mutation occurs. Native arguments,
 * outputs and owner storage must be disjoint. Payloads are zeroed and aligned
 * for fundamental scalar types. Each allocation charges its header and payload
 * against the joint permanent/current-managed Registry budget. out is required
 * and unchanged on failure; zero bytes is an invalid native request. */
mc_registry_status mc_registry_managed_allocate(mc_registry_classes *classes, size_t bytes,
                                                void **out);

/* NULL storage is accepted. Otherwise storage must be the live, unchanged
 * result of managed_allocate for this exact owner, released once. Unlink and
 * reclaim its entire charge; historical peak and last_error stay unchanged.
 * No source callback or other object's lifetime operation is performed. */
void mc_registry_managed_release(mc_registry_classes *classes, void *storage);

/* Joint admission for complete native charges, including allocation metadata
 * where applicable. fits rejects NULL or inconsistent/over-budget accounting.
 * remaining returns zero in those cases. Neither function allocates, records
 * a reservation, changes the peak or mutates diagnostics. */
bool mc_registry_managed_fits(const mc_registry_classes *classes, size_t charged_bytes);
size_t mc_registry_managed_remaining(const mc_registry_classes *classes);

/* Record the joint current total after a successful permanent or managed
 * acquisition. Callers admit its complete charge before acquisition. No
 * speculative/failed reservation contributes to this historical maximum. */
void mc_registry_managed_peak_update(mc_registry_classes *classes);

/* Final, quiescent Registry retirement only, after all managed children have
 * retired their logical owners and no snapshot remains. Reclaim every ledger
 * entry, including unpublished native storage, without source callbacks.
 * NULL is accepted; repeated retirement before arena free is harmless. */
void mc_registry_managed_retire(mc_registry_classes *classes);

#endif
