#ifndef MC_BLOCK_REGISTRY_BED_RENDER_TABLES_INTERNAL_H
#define MC_BLOCK_REGISTRY_BED_RENDER_TABLES_INTERNAL_H

#include "mc/block/registry/registry.h"

typedef struct mc_registry_bed_render_array {
    mc_registry_classes *classes;
    const int32_t *identity;
} mc_registry_bed_render_array;

typedef struct mc_registry_bed_render_matrix {
    mc_registry_classes *classes;
    int32_t *const *identity;
} mc_registry_bed_render_matrix;

/* Actual JJ getstatic captures perform first active use. Element operations
 * use captured identities without reloading current fields/rows. NULL precedes
 * index bounds; unknown native backing is refused without dereference. Array
 * allocations stay in the Registry's genuine permanent ownership ledger. */
mc_registry_status mc_registry_bed_render_array_get(mc_registry_classes *, bool second,
                                                    mc_registry_bed_render_array *,
                                                    mc_registry_error *);
mc_registry_status mc_registry_bed_render_matrix_get(mc_registry_classes *,
                                                     mc_registry_bed_render_matrix *,
                                                     mc_registry_error *);
mc_registry_status mc_registry_bed_render_array_at(const mc_registry_bed_render_array *, int32_t,
                                                   int32_t *, mc_registry_error *);
mc_registry_status mc_registry_bed_render_matrix_at(const mc_registry_bed_render_matrix *, int32_t,
                                                    mc_registry_bed_render_array *,
                                                    mc_registry_error *);

#endif
