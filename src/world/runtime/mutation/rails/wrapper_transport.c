#include "wrapper_transport_internal.h"

#include "../mutation_internal.h"
#include "error_sites_internal.h"

#include <stddef.h>
#include <string.h>

#define DETECTOR_CURRENT                                                                           \
    (MC_WORLD_RAIL_SITE_DETECTOR_CONTACT | MC_WORLD_RAIL_SITE_DETECTOR_TICK |                      \
     MC_WORLD_RAIL_SITE_DETECTOR_UPDATE)
#define LIFECYCLE_CURRENT                                                                          \
    (MC_WORLD_RAIL_SITE_ADDED | MC_WORLD_RAIL_SITE_SHAPE | MC_WORLD_RAIL_SITE_NEIGHBOR)
#define CONNECTION_CURRENT                                                                         \
    (MC_WORLD_RAIL_SITE_CONNECTION | MC_WORLD_RAIL_SITE_PREDICATE | MC_WORLD_RAIL_SITE_ACCESSOR)
#define CONNECTION_CAUSES                                                                          \
    (MC_WORLD_RAIL_SITE_CONNECTION_CAUSE | MC_WORLD_RAIL_SITE_PREDICATE_CAUSE |                    \
     MC_WORLD_RAIL_SITE_ACCESSOR_CAUSE)
#define SEARCH_CURRENT (MC_WORLD_RAIL_SITE_SEARCH | MC_WORLD_RAIL_SITE_VERIFY)

static void transport_misuse(mc_world_mutation_error *error) {
    /* The source operation already failed. Retain its exact actual .cause and
     * every typed child; never reinterpret malformed diagnostic state as a
     * successful operation or a fabricated source Throwable. */
    error->status = MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    error->operation = "Rail wrapper diagnostic transport plan misuse";
}

static void publish(mc_world_mutation_error *destination, const mc_world_mutation_error *lower,
                    const mc_world_mutation_error *outer, uint32_t selected, uint32_t origin,
                    uint32_t currents, uint32_t causes) {
    if ((selected & ~(currents | causes)) != 0 || (origin & ~currents) != 0) {
        transport_misuse(destination);
        return;
    }
    const mc_world_rail_merge_plan plan = {
        .restore_current = selected & currents,
        /* Every independently returned public lower rail failure has already
         * finalized its causes. Unfinalized lower currents are a plan failure,
         * not an invitation to infer the innermost current from values. */
        .lower_origin = 0,
        .outer_origin = origin,
        .outer_hook = {0},
        .lower_current_hook_selected = lower->hook_class != NULL,
        .lower_cause_hook_selected = lower->cause_hook_class != NULL};
    if (!mc_world_rail_error_merge(destination, lower, outer, plan))
        transport_misuse(destination);
}

void mc_world_detector_error_publish(mc_world_detector_error *error) {
    mc_world_mutation_error lower;
    memcpy(&lower, &error->mutation, sizeof(lower));
    mc_world_mutation_error outer = mc_world_mutation_empty_error();
    outer.rail_sites_selected = error->selected & DETECTOR_CURRENT;
    outer.rail_detector_contact = error->contact;
    outer.rail_detector_tick = error->tick;
    outer.rail_detector_update = error->update;
    if ((error->selected & MC_WORLD_RAIL_SITE_DETECTOR_CAUSE) != 0 &&
        (lower.rail_sites_selected & (DETECTOR_CURRENT | MC_WORLD_RAIL_SITE_DETECTOR_CAUSE)) == 0) {
        lower.rail_detector_cause = error->cause;
        lower.rail_sites_selected |= MC_WORLD_RAIL_SITE_DETECTOR_CAUSE;
    }
    publish(&error->mutation, &lower, &outer, error->selected, error->origin, DETECTOR_CURRENT,
            MC_WORLD_RAIL_SITE_DETECTOR_CAUSE);
}

void mc_rail_lifecycle_error_publish(mc_rail_lifecycle_error *error) {
    mc_world_mutation_error lower;
    memcpy(&lower, &error->mutation, sizeof(lower));
    mc_world_mutation_error outer = mc_world_mutation_empty_error();
    outer.rail_sites_selected = error->selected & LIFECYCLE_CURRENT;
    outer.rail_added = error->added;
    outer.rail_shape = error->shape;
    outer.rail_neighbor = error->neighbor;
    if ((error->selected & MC_WORLD_RAIL_SITE_LIFECYCLE_CAUSE) != 0 &&
        (lower.rail_sites_selected & (LIFECYCLE_CURRENT | MC_WORLD_RAIL_SITE_LIFECYCLE_CAUSE)) ==
            0) {
        lower.rail_lifecycle_cause = error->cause;
        lower.rail_sites_selected |= MC_WORLD_RAIL_SITE_LIFECYCLE_CAUSE;
    }
    publish(&error->mutation, &lower, &outer, error->selected, error->origin, LIFECYCLE_CURRENT,
            MC_WORLD_RAIL_SITE_LIFECYCLE_CAUSE);
}

void mc_rail_connections_error_publish(mc_rail_connections_error *error) {
    mc_world_mutation_error lower;
    memcpy(&lower, &error->mutation, sizeof(lower));
    mc_world_mutation_error outer = mc_world_mutation_empty_error();
    outer.rail_sites_selected = error->selected & CONNECTION_CURRENT;
    outer.rail_connection = error->connection;
    outer.rail_predicate = error->predicate;
    outer.rail_accessor = error->accessor;
    if ((error->selected & MC_WORLD_RAIL_SITE_CONNECTION_CAUSE) != 0 &&
        (lower.rail_sites_selected &
         (MC_WORLD_RAIL_SITE_CONNECTION | MC_WORLD_RAIL_SITE_CONNECTION_CAUSE)) == 0) {
        lower.rail_connection_cause = error->cause_connection;
        lower.rail_sites_selected |= MC_WORLD_RAIL_SITE_CONNECTION_CAUSE;
    }
    if ((error->selected & MC_WORLD_RAIL_SITE_PREDICATE_CAUSE) != 0 &&
        (lower.rail_sites_selected &
         (MC_WORLD_RAIL_SITE_PREDICATE | MC_WORLD_RAIL_SITE_PREDICATE_CAUSE)) == 0) {
        /* No later source call runs after this recorded predicate failure;
         * its private current is the exact selected failure origin. */
        lower.rail_predicate_cause = error->predicate;
        lower.rail_sites_selected |= MC_WORLD_RAIL_SITE_PREDICATE_CAUSE;
    }
    if ((error->selected & MC_WORLD_RAIL_SITE_ACCESSOR_CAUSE) != 0 &&
        (lower.rail_sites_selected &
         (MC_WORLD_RAIL_SITE_ACCESSOR | MC_WORLD_RAIL_SITE_ACCESSOR_CAUSE)) == 0) {
        lower.rail_accessor_cause = error->accessor;
        lower.rail_sites_selected |= MC_WORLD_RAIL_SITE_ACCESSOR_CAUSE;
    }
    publish(&error->mutation, &lower, &outer, error->selected, error->origin, CONNECTION_CURRENT,
            CONNECTION_CAUSES);
}

void mc_rail_power_search_error_publish(mc_rail_power_search_error *error) {
    mc_world_mutation_error lower;
    memcpy(&lower, &error->mutation, sizeof(lower));
    mc_world_mutation_error outer = mc_world_mutation_empty_error();
    outer.rail_sites_selected = error->selected & SEARCH_CURRENT;
    outer.rail_search = error->search;
    outer.rail_verify = error->verify;
    if ((error->selected & MC_WORLD_RAIL_SITE_SEARCH_CAUSE) != 0 &&
        (lower.rail_sites_selected & (SEARCH_CURRENT | MC_WORLD_RAIL_SITE_SEARCH_CAUSE)) == 0) {
        lower.rail_search_cause = error->cause;
        lower.rail_sites_selected |= MC_WORLD_RAIL_SITE_SEARCH_CAUSE;
    }
    publish(&error->mutation, &lower, &outer, error->selected, error->origin, SEARCH_CURRENT,
            MC_WORLD_RAIL_SITE_SEARCH_CAUSE);
}

#undef DETECTOR_CURRENT
#undef LIFECYCLE_CURRENT
#undef CONNECTION_CURRENT
#undef CONNECTION_CAUSES
#undef SEARCH_CURRENT
