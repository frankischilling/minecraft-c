#ifndef MC_BENCHMARK_MOB_TICK_DIAGNOSTICS_ADAPTER_H
#define MC_BENCHMARK_MOB_TICK_DIAGNOSTICS_ADAPTER_H

#include <stdbool.h>

/* Each answer comes from the actual complete adapter object linked before the
 * common archive. The workload itself always invokes the real public FD.g. */
bool mc_benchmark_mob_tick_legacy_tail(void);
bool mc_benchmark_mob_tick_legacy_dispatch(void);
/* Common passive benchmark-only observation, inserted identically after the
 * actual Mob return in each dispatch adapter. No owner/state is inspected. */
void mc_benchmark_mob_tick_observe_dispatch(bool reserved);

#endif
