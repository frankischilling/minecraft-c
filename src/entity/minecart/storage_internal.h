#ifndef MC_ENTITY_MINECART_STORAGE_INTERNAL_H
#define MC_ENTITY_MINECART_STORAGE_INTERNAL_H

#include "mc/entity/minecart/minecart.h"
#include "mc/entity/nbt/storage.h"

/* Native-only immutable provenance. The current root and an h sidecar each
 * own one reference; record pointers never own individual trees. Definitions
 * are shared with native invariant tests, not copied as lookalike layouts. */
typedef struct mc_minecart_storage_generation {
    size_t references;
    mc_nbt_tree *tree;
} mc_minecart_storage_generation;

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4200)
#endif
typedef struct mc_minecart_storage_sidecar {
    mc_minecart_storage_generation *generation;
    size_t length;
    const mc_nbt_tag *records[];
} mc_minecart_storage_sidecar;
#ifdef _MSC_VER
#pragma warning(pop)
#endif

/* Concrete canonical Minecart, live/exclusive dependencies and a required
 * fresh enclosing error frame. No source call, phase initialization or World
 * entry. Byte inspection accepts every live Stack phase and deduplicates
 * physical allocations, not holds. Outputs remain unchanged on failure. */
mc_entity_nbt_status mc_minecart_storage_native_bytes(const mc_entity *cart, size_t *out,
                                                      mc_entity_nbt_error *frame);
mc_entity_nbt_status mc_minecart_storage_preserve_source(mc_entity *cart,
                                                         const mc_nbt_tag *compound,
                                                         const mc_nbt_limits *limits,
                                                         mc_entity_nbt_error *frame);

/* Actual successful semantic Boolean result, already held and charged by the
 * generic wrapper. remaining excludes the cart graph and semantic tree.
 * No Stack/source field is reread to model the generated result; only actual
 * generated fields and current occurrence provenance are composed. */
mc_entity_nbt_status mc_minecart_storage_overlay_after_semantic(const mc_entity *cart,
                                                                const mc_nbt_tree *semantic,
                                                                const mc_nbt_limits *remaining,
                                                                mc_nbt_tree **out,
                                                                mc_entity_nbt_error *frame);

/* Nonfallible lifetime hooks. Source store/take calls clear only AFTER its
 * actual identity store; retain refusal never clears metadata. Detach keeps
 * these owners until all borrowing input use finishes, then retires them.
 * Root retirement clears the borrowed generic mirror before final free. */
const mc_nbt_tag *mc_minecart_storage_occurrence_source(const mc_minecart_stack_array *array,
                                                        size_t index);
void mc_minecart_storage_occurrence_clear(mc_minecart_stack_array *array, size_t index);
void mc_minecart_storage_array_sources_retire(mc_minecart_stack_array *array);
void mc_minecart_storage_root_retire(mc_entity *cart);

#endif
