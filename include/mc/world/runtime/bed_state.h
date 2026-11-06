#ifndef MC_WORLD_RUNTIME_BED_STATE_H
#define MC_WORLD_RUNTIME_BED_STATE_H

#include "mc/world/runtime/placement.h"

/* Actual client static VE.a(World,III,Z)V. The invoking Registry supplies the
 * declaring VE class; its active use precedes metadata4 on the nullable World.
 * The fresh metadata changes only mask4, then the notifying World.d36 body runs.
 * No Bed singleton/receiver or remote flag is read by this method.
 *
 * error is initially zero and noncopyable. A reached call retains its exact
 * Registry/World/query/mutation/callback graph until the existing checked
 * mc_world_placement_error_retire succeeds, including after source failure.
 * Retirement consumes native occurrences only and never retries the method.
 * site is the reached VE helper frame (0,4,36,39); caller frames belong to the
 * enclosing source invocation. Failure preserves every earlier source effect. */
mc_world_storage_status mc_world_bed_set_occupied(mc_registry_classes *code_registry,
                                                  mc_world_runtime *world, int32_t x, int32_t y,
                                                  int32_t z, bool occupied,
                                                  mc_world_placement_error *error);

#endif
