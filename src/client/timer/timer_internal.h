#ifndef MC_CLIENT_TIMER_INTERNAL_H
#define MC_CLIENT_TIMER_INTERNAL_H
#include "construction.h"

typedef struct mc_timer_native_state {
    mc_timer_native_phase phase;
    mc_timer_constructor_origin origin;
    int32_t last_store_pc;
    size_t store_count, clock_call_count, nano_init_calls;
    int32_t stores[6], clock_calls[2];
    bool nano_initialized_by_call;
    mc_timer_construction_error failure;
} mc_timer_native_state;

/* The one real Timer representation. Existing source fields keep their order;
 * source c and native-only constructor/lifetime metadata are appended. */
struct mc_timer {
    float ticks_per_second, speed, remainder;
    int32_t ticks;
    double last_high_seconds, adjustment;
    int64_t last_wall_millis, last_high_millis, accumulated_wall_millis;
    float partial_ticks; /* actual c, distinct from remainder/e */
    mc_timer_native_state native;
};
#endif
