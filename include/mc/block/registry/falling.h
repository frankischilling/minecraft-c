#ifndef MC_BLOCK_REGISTRY_FALLING_H
#define MC_BLOCK_REGISTRY_FALLING_H

#include "mc/block/registry/registry.h"

/* Actual shared declaring-class gk/ei.a, already owned by Registry gk_a.
 * Each get/set performs real declaring-class active use before access, including
 * its actual Block superclass and reached resource/profile effects. No new
 * cell, World flag, constructor, callback or reset is substituted. A successful
 * warm access does not reset an earlier value or reinitialize either class.
 *
 * Public Registry entry retains its existing BUSY policy; admitted private
 * clinit recursion is handled by the existing actual class initializer.
 * Initialization failure keeps its complete first cause and reached effects.
 * Cached declaring-class failure returns FAILED_INITIALIZATION with that
 * original diagnostic record, whose status may name the original cause.
 * Native argument/BUSY failures and successful calls receive fresh diagnostics
 * with no initializer/constructor attribution. Ordinary get out is unchanged
 * on failure. error may be NULL and success never adds a runtime borrow.
 *
 * Keep the same owner and its Material/profile/resource dependencies live and
 * exclusive. Output/error storage is disjoint from each other and from all
 * owner/dependency storage. Neither accessor restores a previous flag value;
 * the source caller owns its actual reached get/put order and failure prefix.
 * These accessors do not implement falling hooks, ticks or Entity bodies. */
mc_registry_status mc_registry_falling_flag_get(mc_registry_classes *classes, bool *out,
                                                mc_registry_error *error);
mc_registry_status mc_registry_falling_flag_set(mc_registry_classes *classes, bool value,
                                                mc_registry_error *error);

#endif
