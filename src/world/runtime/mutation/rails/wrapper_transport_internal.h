#ifndef MC_WORLD_RUNTIME_RAIL_WRAPPER_TRANSPORT_INTERNAL_H
#define MC_WORLD_RUNTIME_RAIL_WRAPPER_TRANSPORT_INTERNAL_H

#include "detector_internal.h"
#include "rail_lifecycle_internal.h"
#include "rail_power_search_internal.h"

/* Failed API publication only. Each wrapper keeps actual current sites and
 * first private failure causes with explicit selected/origin bits. These pure
 * projections preserve complete lower mutation children; existing public lower
 * causes win, including selected-but-unentered frames. No source operation,
 * owner lifetime, World/Registry state or gameplay ordering changes here.
 *
 * The actual nullable generic hook/cause fields use the existing legacy ABI's
 * presence convention. No class-name recognition or PC/entered test creates a
 * typed rail frame. Native merge-plan misuse stays a failure: top status and
 * operation identify transport misuse, while actual source cause/all children
 * remain retained. Each successful original API still returns its fresh error.
 *
 * Requires a live private wrapper, disjoint from actual source owners, the
 * public rail site types and the pure error-site merge module. No private
 * wrapper is embedded in the public mutation error. */
void mc_world_detector_error_publish(mc_world_detector_error *error);
void mc_rail_lifecycle_error_publish(mc_rail_lifecycle_error *error);
void mc_rail_connections_error_publish(mc_rail_connections_error *error);
void mc_rail_power_search_error_publish(mc_rail_power_search_error *error);

#endif
