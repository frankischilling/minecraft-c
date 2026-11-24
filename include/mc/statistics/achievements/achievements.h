#ifndef MC_STATISTICS_ACHIEVEMENTS_ACHIEVEMENTS_H
#define MC_STATISTICS_ACHIEVEMENTS_ACHIEVEMENTS_H

#include "mc/item/stack/stack.h"
#include "mc/statistics/registry/registry.h"

/* Subtype fields remain distinct from inherited Stat fields. The client-only
 * gt callback has no reached startup assignment; later UI methods are separate. */
typedef struct mc_achievement_fields {
    int32_t a, b;
    const mc_statistic *c;
    const mc_item_stack *d;
    const mc_registry_string *l;
    bool special; /* client n / server m */
    const struct mc_achievement_description_callback *client_callback;
} mc_achievement_fields;

typedef struct mc_registry_achievement_view {
    mc_registry_phase phase;
    int32_t initializer_pc;
    int32_t a, b, c, d;
    const mc_statistic_list *e;
    mc_statistic *const *named; /* actual ep/cy f..u declaration order */
} mc_registry_achievement_view;

/* Execute the shared ep/cy owner on the Block/Item/Stats class lifetime.
 * Reached dependencies, retained first failures and same-flow recursion are
 * real; diagnostic inspection never initializes a class or changes its phase. */
mc_registry_status mc_registry_classes_initialize_achievements(mc_registry_classes *classes,
                                                               mc_registry_error *error);
mc_registry_status mc_registry_classes_inspect_achievements(const mc_registry_classes *classes,
                                                            mc_registry_achievement_view *out,
                                                            mc_registry_error *error);
const mc_achievement_fields *mc_statistic_achievement_fields_get(const mc_statistic *statistic);

#endif
