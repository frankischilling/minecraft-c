#include "../mob_tick_diagnostics_adapter.h"

/* Functional profiles share the exact compiled algorithm adapters. They do
 * not time work or need the CPU workload's passive allocation-path counters. */
void mc_benchmark_mob_tick_observe_dispatch(bool reserved) {
    (void)reserved;
}
