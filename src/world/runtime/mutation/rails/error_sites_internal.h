#ifndef MC_WORLD_RUNTIME_RAIL_ERROR_SITES_INTERNAL_H
#define MC_WORLD_RUNTIME_RAIL_ERROR_SITES_INTERNAL_H

#include "mc/world/runtime/mutations.h"

/* An explicit actual caller hook, not a decoded rail site or a new legacy
 * generic-hook ABI. selected is supplied by the reached caller, independently
 * of PC/string/entered values. Strings remain borrowed actual method names. */
typedef struct mc_world_rail_caller_hook {
    bool selected;
    const char *declaring_class, *method;
    unsigned int bytecode;
} mc_world_rail_caller_hook;

typedef struct mc_world_rail_merge_plan {
    /* Exact outer current slots to restore; causes are forbidden here. Every
     * restored bit must be explicitly selected in the saved outer error. */
    uint32_t restore_current;
    /* One explicitly chosen innermost current per relevant channel (detector,
     * lifecycle, connection, predicate, accessor, search). A selected lower
     * cause wins first. Otherwise lower_origin is required if that lower
     * channel contains selected currents. outer_origin is the caller fallback;
     * it must choose one of the explicitly restored outer current slots. */
    uint32_t lower_origin, outer_origin;
    mc_world_rail_caller_hook outer_hook;
    /* Actual returned lower generic-hook context. These booleans are supplied
     * by its caller/dispatcher; this helper does not infer body entry, generic
     * hook presence or any independent rail site from names or PC values. */
    bool lower_current_hook_selected, lower_cause_hook_selected;
} mc_world_rail_merge_plan;

/* Pure failed-error transport. Copies the entire actual failed lower error,
 * preserving every typed power/access/support/nearby/schedule/etc child; only
 * explicit rail current/cause selection and the supplied concrete caller hook
 * are overlaid. No World/Registry/source field, allocation, counter or errno
 * changes. Full lower and outer records may alias each other and destination.
 * The plan is passed by value and must describe actual reached source frames.
 *
 * Fresh entry/child error initialization remains the caller's responsibility.
 * On leaf failure use destination=lower=outer, restore_current=outer_origin=0,
 * and lower_origin naming the actual failing currents. On nested failure pass
 * a saved outer frame and a separately initialized actual failed lower record.
 * Selected lower causes/currents win even when entered=false. No stack/depth
 * or gameplay work limit follows from this finite diagnostic representation.
 *
 * Returns false for native plan/pointer misuse, preserving destination. Every
 * nonempty restored channel needs exactly one outer_origin; every selected
 * lower-current channel without a lower cause needs exactly one lower_origin.
 * Unknown bits, cause bits in current masks, unselected sources and ambiguous
 * origins are refused. Correct operation performs no failure reinterpretation
 * and never discovers typed child data from sentinel values or ambient state. */
bool mc_world_rail_error_merge(mc_world_mutation_error *destination,
                               const mc_world_mutation_error *lower,
                               const mc_world_mutation_error *outer, mc_world_rail_merge_plan plan);

#endif
