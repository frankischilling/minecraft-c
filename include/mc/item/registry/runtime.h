#ifndef MC_ITEM_REGISTRY_RUNTIME_H
#define MC_ITEM_REGISTRY_RUNTIME_H

#include "mc/item/registry/registry.h"

/* Ordinary source-facing Item getstatic operations on the actual shared class
 * owner. They perform real gm/ej active use before selecting the cell, including
 * initialization/profile/resource effects and the cached first failure. They
 * never expose a failed diagnostic prefix as a successful active use.
 * Case-sensitive static selectors are the declared d..be Item fields; selected
 * NULL is a successful result. Unknown names fail after active use.
 * c_get captures the actual possibly-NULL c descriptor without copying it.
 * It does not create an ID table, retain a separate owner or pre-initialize a
 * dependency. Initialization has registry.h's live-dependency/FP requirements.
 */
mc_registry_status mc_registry_item_static_get(mc_registry_classes *classes, const char *name,
                                               const mc_item **out, mc_registry_error *error);
mc_registry_status mc_registry_item_c_get(mc_registry_classes *classes,
                                          const mc_registry_item_array **out,
                                          mc_registry_error *error);

/* Reached aaload of THAT captured array: no second active use, phase check,
 * comparison with current c, or c-cell reread. A NULL capture fails before
 * signed bounds; a valid element may itself be NULL and succeeds with NULL.
 * Caller supplies a real descriptor belonging to this SAME live class owner
 * and retains that owner across capture/intervening work/read. Its actual
 * descriptor/elements remain readable and exclusive until the read finishes.
 * A later current-c replacement does not invalidate the captured allocation.
 * No stale/foreign pointer detection or arbitrary descriptor adoption exists.
 * Native descriptor storage admission follows the signed source bounds check.
 */
mc_registry_status mc_registry_item_c_at(mc_registry_classes *classes,
                                         const mc_registry_item_array *captured_array,
                                         int32_t index, const mc_item **out,
                                         mc_registry_error *error);

/* Every public operation requires a quiescent exclusive Registry and rejects
 * entered initialization with BUSY, including c_at. Existing admitted private
 * bootstrap recursion continues through its own APIs. No new hold is acquired;
 * common counted lifetime borrows are mc_registry_classes_retain/release from
 * mc/block/registry/runtime.h. Destruction cannot precede release of the last
 * retained capture/user; Material/profile/resource lifetimes remain separate.
 *
 * out/error are mutually disjoint and disjoint from all owner/dependency/input
 * storage. Ordinary outputs stay unchanged on every failure. Fresh runtime
 * NULL/bounds/name/admission errors have initializer/constructor PCs -1 and no
 * class attribution; an actual active-use failure retains its whole original
 * cause. Success returns a fresh OK error and borrowed actual identities.
 * This facade does not complete arbitrary Dungeon loot or global bootstrap.
 */

#endif
