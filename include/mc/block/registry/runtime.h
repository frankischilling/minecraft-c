#ifndef MC_BLOCK_REGISTRY_RUNTIME_H
#define MC_BLOCK_REGISTRY_RUNTIME_H

#include "mc/block/registry/registry.h"

/* Counted native borrows keep the common class owner alive for its runtime
 * users. Retain requires the exact configured side and same Random profile
 * identity. Neither operation initializes a class, reads resources, allocates
 * or consumes entropy. Release drops one existing borrow without destroying
 * the owner; zero release is INVALID_ARGUMENT and overflow retain is LIMIT.
 * Both operations reject entered initialization with BUSY. Destruction stays
 * BUSY until every successful retain has a matching release. The borrowed
 * Material/profile/resource dependencies retain their existing lifetimes. */
mc_registry_status mc_registry_classes_retain(mc_registry_classes *classes,
                                              mc_registry_side expected_side,
                                              mc_java_random_profile *expected_profile,
                                              mc_registry_error *error);
mc_registry_status mc_registry_classes_release(mc_registry_classes *classes,
                                               mc_registry_error *error);

/* Ordinary runtime reads perform real Block active use before selecting the
 * static cell or loading the m/o array. They can initialize the actual class,
 * retain constructor/resource/profile effects, or propagate its cached first
 * failure. They never return diagnostic failed-prefix storage as a successful
 * active use. Public reentry is BUSY; admitted private recursion is unchanged.
 * Reached initialization has registry.h's FP and dependency requirements.
 *
 * Static names are case-sensitive uu/na fields u..bl; a selected NULL cell is
 * a successful result. Unknown names are rejected after Block active use.
 * Array reads check the actual array for NULL before signed indices 0..255.
 * block_get may likewise succeed with a NULL element; opaque_get reads o[id]
 * directly rather than invoking a Block receiver predicate. Fresh post-active-
 * use array failures have no initializer or constructor class/PC attribution;
 * an initialization failure retains its original nested attribution. */
mc_registry_status mc_registry_block_static_get(mc_registry_classes *classes, const char *name,
                                                const mc_block **out, mc_registry_error *error);
mc_registry_status mc_registry_block_get(mc_registry_classes *classes, int32_t id,
                                         const mc_block **out, mc_registry_error *error);
mc_registry_status mc_registry_block_opaque_get(mc_registry_classes *classes, int32_t id, bool *out,
                                                mc_registry_error *error);

/* Returns the actual prebuilt ln/hj material identity for a declared selector.
 * This operation does not initialize Block, including when Block is COLD or
 * FAILED. Public reentry is BUSY. It does not copy, reset or retain a separate
 * Material owner. */
mc_registry_status mc_registry_material_get(mc_registry_classes *classes, mc_material_id id,
                                            mc_material **out, mc_registry_error *error);

/* All operations require exclusive access and live owner dependencies. Output
 * slots and error storage must be disjoint from each other and from all owner
 * and dependency storage. Ordinary outputs are unchanged on failure; error
 * receives the diagnostic when supplied. Returned objects borrow the common
 * owner/dependency lifetime and remain usable only while those owners live.
 * A counted borrow prevents class-owner destruction, not dependency teardown. */

#endif
