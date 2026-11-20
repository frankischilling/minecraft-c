#ifndef MC_BLOCK_REGISTRY_HORIZONTAL_DIRECTIONS_INTERNAL_H
#define MC_BLOCK_REGISTRY_HORIZONTAL_DIRECTIONS_INTERNAL_H

#include "mc/block/registry/registry.h"

/* Reached jj/ge.b active use initializes the whole independent jj/ge class,
 * including a and c, in source order. It does not initialize wj/od, ve/nj,
 * Block, Entity or a Random owner. A nested exclusive Registry initializer
 * flow is admitted and restored; an erroneous class retains its first cause.
 * Capture succeeds for a NULL current b and inspects no array element.
 * Views are borrowed: keep the supplying owner live/exclusive through use. */
typedef struct mc_registry_horizontal_view {
    mc_registry_classes *classes;
    const int32_t *identity;
    size_t length;
} mc_registry_horizontal_view;

mc_registry_status mc_registry_horizontal_b_get(mc_registry_classes *classes,
                                                mc_registry_horizontal_view *out,
                                                mc_registry_error *error);

/* Reached iaload uses the captured reference, never current b or class phase.
 * NULL precedes signed bounds. A nonnull reference must be one of that owner's
 * actual retained four-int a/b allocations; unknown/foreign projections are
 * reported without adoption or dereference. Array cells remain mutable. No
 * initialization, allocation, callback, Random operation or hold occurs here.
 * Failures preserve out and do not overwrite a cached initializer failure.
 * Native argument/extent admission leaves owner last_error unchanged. */
mc_registry_status mc_registry_horizontal_at(const mc_registry_horizontal_view *array,
                                             int32_t index, int32_t *out, mc_registry_error *error);

#endif
