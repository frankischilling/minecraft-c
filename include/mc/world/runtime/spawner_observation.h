#ifndef MC_WORLD_RUNTIME_SPAWNER_OBSERVATION_H
#define MC_WORLD_RUNTIME_SPAWNER_OBSERVATION_H

#include <stddef.h>

/* Copied observations of reached Source calls, with no Entity or work views.
 * Counts aggregate all Spawners visited by the enclosing World.g invocation. */
typedef struct mc_world_spawner_tick_observation {
    size_t attempts, factory_calls, queries, predicate_calls, accepted;
    size_t insertion_calls, insertion_true, insertion_false;
    size_t smoke_calls, flame_calls, explode_calls, delay_resets;
    size_t cap_returns, null_returns, completed;
} mc_world_spawner_tick_observation;

#endif
