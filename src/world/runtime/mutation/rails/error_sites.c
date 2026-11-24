#include "error_sites_internal.h"

#include <stddef.h>
#include <string.h>

#define DETECTOR_CURRENT                                                                           \
    (MC_WORLD_RAIL_SITE_DETECTOR_CONTACT | MC_WORLD_RAIL_SITE_DETECTOR_TICK |                      \
     MC_WORLD_RAIL_SITE_DETECTOR_UPDATE)
#define LIFECYCLE_CURRENT                                                                          \
    (MC_WORLD_RAIL_SITE_ADDED | MC_WORLD_RAIL_SITE_SHAPE | MC_WORLD_RAIL_SITE_NEIGHBOR)
#define SEARCH_CURRENT (MC_WORLD_RAIL_SITE_SEARCH | MC_WORLD_RAIL_SITE_VERIFY)
#define CURRENT_SITES                                                                              \
    (DETECTOR_CURRENT | LIFECYCLE_CURRENT | MC_WORLD_RAIL_SITE_CONNECTION |                        \
     MC_WORLD_RAIL_SITE_PREDICATE | MC_WORLD_RAIL_SITE_ACCESSOR | SEARCH_CURRENT)
#define CAUSE_SITES                                                                                \
    (MC_WORLD_RAIL_SITE_DETECTOR_CAUSE | MC_WORLD_RAIL_SITE_LIFECYCLE_CAUSE |                      \
     MC_WORLD_RAIL_SITE_CONNECTION_CAUSE | MC_WORLD_RAIL_SITE_PREDICATE_CAUSE |                    \
     MC_WORLD_RAIL_SITE_ACCESSOR_CAUSE | MC_WORLD_RAIL_SITE_SEARCH_CAUSE)

static bool one_or_zero(uint32_t value) {
    return value == 0 || (value & (value - UINT32_C(1))) == 0;
}

static bool channel_plan(uint32_t lower_selected, uint32_t currents, uint32_t cause,
                         mc_world_rail_merge_plan plan) {
    const uint32_t lower_origin = plan.lower_origin & currents;
    const uint32_t outer_origin = plan.outer_origin & currents;
    if (!one_or_zero(lower_origin) || !one_or_zero(outer_origin))
        return false;
    if ((lower_selected & cause) == 0 && (lower_selected & currents) != 0 && lower_origin == 0)
        return false;
    if ((plan.restore_current & currents) != 0 && outer_origin == 0)
        return false;
    return true;
}

static bool valid_plan(const mc_world_mutation_error *lower, const mc_world_mutation_error *outer,
                       mc_world_rail_merge_plan plan) {
    const uint32_t all_sites = CURRENT_SITES | CAUSE_SITES;
    if (((lower->rail_sites_selected | outer->rail_sites_selected) & ~all_sites) != 0 ||
        ((plan.restore_current | plan.lower_origin | plan.outer_origin) &
         ~(uint32_t)CURRENT_SITES) != 0 ||
        (plan.restore_current & ~outer->rail_sites_selected) != 0 ||
        (plan.lower_origin & ~lower->rail_sites_selected) != 0 ||
        (plan.outer_origin & ~plan.restore_current) != 0)
        return false;
    return channel_plan(lower->rail_sites_selected, DETECTOR_CURRENT,
                        MC_WORLD_RAIL_SITE_DETECTOR_CAUSE, plan) &&
           channel_plan(lower->rail_sites_selected, LIFECYCLE_CURRENT,
                        MC_WORLD_RAIL_SITE_LIFECYCLE_CAUSE, plan) &&
           channel_plan(lower->rail_sites_selected, MC_WORLD_RAIL_SITE_CONNECTION,
                        MC_WORLD_RAIL_SITE_CONNECTION_CAUSE, plan) &&
           channel_plan(lower->rail_sites_selected, MC_WORLD_RAIL_SITE_PREDICATE,
                        MC_WORLD_RAIL_SITE_PREDICATE_CAUSE, plan) &&
           channel_plan(lower->rail_sites_selected, MC_WORLD_RAIL_SITE_ACCESSOR,
                        MC_WORLD_RAIL_SITE_ACCESSOR_CAUSE, plan) &&
           channel_plan(lower->rail_sites_selected, SEARCH_CURRENT, MC_WORLD_RAIL_SITE_SEARCH_CAUSE,
                        plan);
}

/* Every choice comes from explicit selected bits, never PC0, entered=false,
 * names, sentinel status or a scan of a lower error's apparent values. Causes
 * are chosen BEFORE outer currents overwrite matching lower current slots. */
static void deepest(mc_world_mutation_error *out, const mc_world_mutation_error *lower,
                    const mc_world_mutation_error *outer, mc_world_rail_merge_plan plan) {
    if ((lower->rail_sites_selected & MC_WORLD_RAIL_SITE_DETECTOR_CAUSE) == 0) {
        const mc_world_mutation_error *source =
            (plan.lower_origin & DETECTOR_CURRENT) != 0 ? lower : outer;
        const uint32_t origin = (plan.lower_origin & DETECTOR_CURRENT) != 0
                                    ? plan.lower_origin & DETECTOR_CURRENT
                                    : plan.outer_origin & DETECTOR_CURRENT;
        if (origin != 0) {
            if (origin == MC_WORLD_RAIL_SITE_DETECTOR_CONTACT)
                out->rail_detector_cause = source->rail_detector_contact;
            else if (origin == MC_WORLD_RAIL_SITE_DETECTOR_TICK)
                out->rail_detector_cause = source->rail_detector_tick;
            else
                out->rail_detector_cause = source->rail_detector_update;
            out->rail_sites_selected |= MC_WORLD_RAIL_SITE_DETECTOR_CAUSE;
        }
    }
    if ((lower->rail_sites_selected & MC_WORLD_RAIL_SITE_LIFECYCLE_CAUSE) == 0) {
        const mc_world_mutation_error *source =
            (plan.lower_origin & LIFECYCLE_CURRENT) != 0 ? lower : outer;
        const uint32_t origin = (plan.lower_origin & LIFECYCLE_CURRENT) != 0
                                    ? plan.lower_origin & LIFECYCLE_CURRENT
                                    : plan.outer_origin & LIFECYCLE_CURRENT;
        if (origin != 0) {
            if (origin == MC_WORLD_RAIL_SITE_ADDED)
                out->rail_lifecycle_cause = source->rail_added;
            else if (origin == MC_WORLD_RAIL_SITE_SHAPE)
                out->rail_lifecycle_cause = source->rail_shape;
            else
                out->rail_lifecycle_cause = source->rail_neighbor;
            out->rail_sites_selected |= MC_WORLD_RAIL_SITE_LIFECYCLE_CAUSE;
        }
    }
    if ((lower->rail_sites_selected & MC_WORLD_RAIL_SITE_CONNECTION_CAUSE) == 0) {
        if ((plan.lower_origin & MC_WORLD_RAIL_SITE_CONNECTION) != 0) {
            out->rail_connection_cause = lower->rail_connection;
            out->rail_sites_selected |= MC_WORLD_RAIL_SITE_CONNECTION_CAUSE;
        } else if ((plan.outer_origin & MC_WORLD_RAIL_SITE_CONNECTION) != 0) {
            out->rail_connection_cause = outer->rail_connection;
            out->rail_sites_selected |= MC_WORLD_RAIL_SITE_CONNECTION_CAUSE;
        }
    }
    if ((lower->rail_sites_selected & MC_WORLD_RAIL_SITE_PREDICATE_CAUSE) == 0) {
        if ((plan.lower_origin & MC_WORLD_RAIL_SITE_PREDICATE) != 0) {
            out->rail_predicate_cause = lower->rail_predicate;
            out->rail_sites_selected |= MC_WORLD_RAIL_SITE_PREDICATE_CAUSE;
        } else if ((plan.outer_origin & MC_WORLD_RAIL_SITE_PREDICATE) != 0) {
            out->rail_predicate_cause = outer->rail_predicate;
            out->rail_sites_selected |= MC_WORLD_RAIL_SITE_PREDICATE_CAUSE;
        }
    }
    if ((lower->rail_sites_selected & MC_WORLD_RAIL_SITE_ACCESSOR_CAUSE) == 0) {
        if ((plan.lower_origin & MC_WORLD_RAIL_SITE_ACCESSOR) != 0) {
            out->rail_accessor_cause = lower->rail_accessor;
            out->rail_sites_selected |= MC_WORLD_RAIL_SITE_ACCESSOR_CAUSE;
        } else if ((plan.outer_origin & MC_WORLD_RAIL_SITE_ACCESSOR) != 0) {
            out->rail_accessor_cause = outer->rail_accessor;
            out->rail_sites_selected |= MC_WORLD_RAIL_SITE_ACCESSOR_CAUSE;
        }
    }
    if ((lower->rail_sites_selected & MC_WORLD_RAIL_SITE_SEARCH_CAUSE) == 0) {
        const mc_world_mutation_error *source =
            (plan.lower_origin & SEARCH_CURRENT) != 0 ? lower : outer;
        const uint32_t origin = (plan.lower_origin & SEARCH_CURRENT) != 0
                                    ? plan.lower_origin & SEARCH_CURRENT
                                    : plan.outer_origin & SEARCH_CURRENT;
        if (origin != 0) {
            out->rail_search_cause =
                origin == MC_WORLD_RAIL_SITE_SEARCH ? source->rail_search : source->rail_verify;
            out->rail_sites_selected |= MC_WORLD_RAIL_SITE_SEARCH_CAUSE;
        }
    }
}

static void restore_current(mc_world_mutation_error *out, const mc_world_mutation_error *outer,
                            uint32_t selected) {
    if ((selected & MC_WORLD_RAIL_SITE_DETECTOR_CONTACT) != 0)
        out->rail_detector_contact = outer->rail_detector_contact;
    if ((selected & MC_WORLD_RAIL_SITE_DETECTOR_TICK) != 0)
        out->rail_detector_tick = outer->rail_detector_tick;
    if ((selected & MC_WORLD_RAIL_SITE_DETECTOR_UPDATE) != 0)
        out->rail_detector_update = outer->rail_detector_update;
    if ((selected & MC_WORLD_RAIL_SITE_ADDED) != 0)
        out->rail_added = outer->rail_added;
    if ((selected & MC_WORLD_RAIL_SITE_SHAPE) != 0)
        out->rail_shape = outer->rail_shape;
    if ((selected & MC_WORLD_RAIL_SITE_NEIGHBOR) != 0)
        out->rail_neighbor = outer->rail_neighbor;
    if ((selected & MC_WORLD_RAIL_SITE_CONNECTION) != 0)
        out->rail_connection = outer->rail_connection;
    if ((selected & MC_WORLD_RAIL_SITE_PREDICATE) != 0)
        out->rail_predicate = outer->rail_predicate;
    if ((selected & MC_WORLD_RAIL_SITE_ACCESSOR) != 0)
        out->rail_accessor = outer->rail_accessor;
    if ((selected & MC_WORLD_RAIL_SITE_SEARCH) != 0)
        out->rail_search = outer->rail_search;
    if ((selected & MC_WORLD_RAIL_SITE_VERIFY) != 0)
        out->rail_verify = outer->rail_verify;
    out->rail_sites_selected |= selected;
}

bool mc_world_rail_error_merge(mc_world_mutation_error *destination,
                               const mc_world_mutation_error *lower,
                               const mc_world_mutation_error *outer,
                               mc_world_rail_merge_plan plan) {
    if (destination == NULL || lower == NULL || outer == NULL || !valid_plan(lower, outer, plan))
        return false;
    /* Capture before publication: all three pointers may alias. No caller
     * restoration can erase the lower source record before deepest selection. */
    mc_world_mutation_error captured_lower, captured_outer;
    memcpy(&captured_lower, lower, sizeof(captured_lower));
    memcpy(&captured_outer, outer, sizeof(captured_outer));
    memcpy(destination, &captured_lower, sizeof(*destination));
    deepest(destination, &captured_lower, &captured_outer, plan);
    restore_current(destination, &captured_outer, plan.restore_current);

    /* Generic concrete hooks retain their existing ABI. The caller explicitly
     * supplies actual returned-child selection; no rail frame is reconstructed
     * by recognizing a generic hook's class/method string or a nonzero PC. */
    if (!plan.lower_cause_hook_selected) {
        if (plan.lower_current_hook_selected) {
            destination->cause_hook_class = captured_lower.hook_class;
            destination->cause_hook_method = captured_lower.hook_method;
            destination->cause_hook_bytecode = captured_lower.hook_bytecode;
        } else if (plan.outer_hook.selected) {
            destination->cause_hook_class = plan.outer_hook.declaring_class;
            destination->cause_hook_method = plan.outer_hook.method;
            destination->cause_hook_bytecode = plan.outer_hook.bytecode;
        }
    }
    if (plan.outer_hook.selected) {
        destination->hook_class = plan.outer_hook.declaring_class;
        destination->hook_method = plan.outer_hook.method;
        destination->hook_bytecode = plan.outer_hook.bytecode;
    }
    return true;
}

#undef DETECTOR_CURRENT
#undef LIFECYCLE_CURRENT
#undef SEARCH_CURRENT
#undef CURRENT_SITES
#undef CAUSE_SITES
