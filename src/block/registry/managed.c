#include "managed_internal.h"
#include "registry_internal.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

union mc_registry_managed_allocation {
    struct {
        mc_registry_managed_allocation *previous, *next;
        mc_registry_classes *owner;
        size_t bytes;
    } metadata;

    /* Fundamental scalar alignment, including Windows CRTs that omit
     * max_align_t in C mode. The header extent preserves this alignment. */
    long double floating_alignment;
    long long integer_alignment;
    void *pointer_alignment;
};

bool mc_registry_managed_fits(const mc_registry_classes *classes, size_t charged_bytes) {
    if (classes == NULL || classes->allocation_bytes > classes->options.max_allocation_bytes)
        return false;
    const size_t remaining = classes->options.max_allocation_bytes - classes->allocation_bytes;
    return classes->managed_current_bytes <= remaining &&
           charged_bytes <= remaining - classes->managed_current_bytes;
}

size_t mc_registry_managed_remaining(const mc_registry_classes *classes) {
    if (!mc_registry_managed_fits(classes, 0))
        return 0;
    return classes->options.max_allocation_bytes - classes->allocation_bytes -
           classes->managed_current_bytes;
}

void mc_registry_managed_peak_update(mc_registry_classes *classes) {
    if (classes == NULL || classes->managed_current_bytes > SIZE_MAX - classes->allocation_bytes)
        return;
    const size_t total = classes->allocation_bytes + classes->managed_current_bytes;
    if (total > classes->total_peak_bytes)
        classes->total_peak_bytes = total;
}

mc_registry_status mc_registry_managed_allocate(mc_registry_classes *classes, size_t bytes,
                                                void **out) {
    if (classes == NULL || out == NULL || bytes == 0)
        return MC_REGISTRY_INVALID_ARGUMENT;
    if (bytes > SIZE_MAX - sizeof(mc_registry_managed_allocation))
        return MC_REGISTRY_LIMIT;
    const size_t charge = sizeof(mc_registry_managed_allocation) + bytes;
    if (!mc_registry_managed_fits(classes, charge))
        return MC_REGISTRY_LIMIT;
    mc_registry_managed_allocation *allocation = calloc(1, charge);
    if (allocation == NULL)
        return MC_REGISTRY_ALLOCATION_FAILED;
    allocation->metadata.owner = classes;
    allocation->metadata.bytes = charge;
    allocation->metadata.next = classes->managed_allocations;
    if (classes->managed_allocations != NULL)
        classes->managed_allocations->metadata.previous = allocation;
    classes->managed_allocations = allocation;
    classes->managed_current_bytes += charge;
    mc_registry_managed_peak_update(classes);
    *out = allocation + 1;
    return MC_REGISTRY_OK;
}

void mc_registry_managed_release(mc_registry_classes *classes, void *storage) {
    if (storage == NULL)
        return;
    mc_registry_managed_allocation *allocation = (mc_registry_managed_allocation *)storage - 1;
    assert(classes != NULL && allocation->metadata.owner == classes);
    assert(allocation->metadata.bytes <= classes->managed_current_bytes);
    if (allocation->metadata.previous != NULL)
        allocation->metadata.previous->metadata.next = allocation->metadata.next;
    else {
        assert(classes->managed_allocations == allocation);
        classes->managed_allocations = allocation->metadata.next;
    }
    if (allocation->metadata.next != NULL)
        allocation->metadata.next->metadata.previous = allocation->metadata.previous;
    classes->managed_current_bytes -= allocation->metadata.bytes;
    free(allocation);
}

void mc_registry_managed_retire(mc_registry_classes *classes) {
    if (classes == NULL)
        return;
    while (classes->managed_allocations != NULL)
        mc_registry_managed_release(classes, classes->managed_allocations + 1);
    assert(classes->managed_current_bytes == 0);
}
