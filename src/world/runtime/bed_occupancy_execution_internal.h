#ifndef MC_WORLD_RUNTIME_BED_OCCUPANCY_EXECUTION_INTERNAL_H
#define MC_WORLD_RUNTIME_BED_OCCUPANCY_EXECUTION_INTERNAL_H

#include "blocks_execution_internal.h"
#include "mc/world/runtime/bed_state.h"
#include "mutation/world_execution_internal.h"

typedef struct mc_world_bed_occupancy_execution {
    const mc_world_mutation_execution_context *context;
    mc_world_block_execution *scalar;
    mc_world_mutation_execution_call **mutation;
} mc_world_bed_occupancy_execution;

/* The caller owns the nullable static argument and the canonical result. VE
 * still activates before the actual metadata receiver is checked. A refused
 * continuation can leave raw status OK and the reached mutation retained. */
bool mc_world_bed_set_occupied_in_execution(mc_registry_classes *, mc_world_runtime *, int32_t,
                                            int32_t, int32_t, bool, mc_world_placement_error *,
                                            mc_world_bed_occupancy_execution *,
                                            mc_world_storage_status *);

#endif
