#include "timer_internal.h"
#include <stdlib.h>

static mc_timer_construction_status report(mc_timer_construction_error value,
                                           mc_timer_construction_error *out) {
    if (out != NULL)
        *out = value;
    return value.status;
}

static mc_timer_construction_error native_error(mc_timer_construction_status status) {
    return (mc_timer_construction_error){.status = status, .py_pc = -1};
}

mc_timer_construction_status mc_timer_allocate_raw(size_t limit, mc_timer **out,
                                                   mc_timer_construction_error *error) {
    if (out == NULL)
        return report(native_error(MC_TIMER_CONSTRUCTION_INVALID_ARGUMENT), error);
    if (limit != 0 && limit < sizeof(mc_timer)) {
        mc_timer_construction_error value = native_error(MC_TIMER_CONSTRUCTION_ALLOCATION_LIMIT);
        value.allocation_bytes = sizeof(mc_timer);
        return report(value, error);
    }
    mc_timer *timer = calloc(1, sizeof(*timer));
    if (timer == NULL) {
        mc_timer_construction_error value = native_error(MC_TIMER_CONSTRUCTION_ALLOCATION_FAILED);
        value.allocation_bytes = sizeof(mc_timer);
        return report(value, error);
    }
    timer->native.last_store_pc = -1;
    timer->native.failure.py_pc = -1;
    *out = timer;
    return report(native_error(MC_TIMER_CONSTRUCTION_OK), error);
}

static void store(mc_timer *timer, int32_t pc) {
    timer->native.stores[timer->native.store_count++] = pc;
    timer->native.last_store_pc = pc;
}

static mc_timer_construction_status failed(mc_timer *timer, mc_timer_construction_error cause,
                                           mc_timer_construction_error *error) {
    timer->native.failure = cause;
    timer->native.phase = MC_TIMER_NATIVE_FAILED;
    return report(cause, error);
}

/* A concrete primitive-clock seam, not a callback replacing a game child. */
typedef struct constructor_clock {
    mc_timer_constructor_origin origin;
    mc_platform_nanotime *system;
    const mc_timer_observed_constructor_clocks *observed;
} constructor_clock;

static mc_platform_millis_status wall(const constructor_clock *clock, int64_t *out) {
    if (clock->origin == MC_TIMER_ORIGIN_SYSTEM)
        return mc_platform_current_time_millis(out);
    if (clock->observed == NULL)
        return MC_PLATFORM_MILLIS_INVALID_ARGUMENT;
    if (clock->observed->wall_status != MC_PLATFORM_MILLIS_OK)
        return clock->observed->wall_status;
    *out = clock->observed->wall_millis;
    return MC_PLATFORM_MILLIS_OK;
}

static mc_platform_nanotime_status nano(mc_timer *timer, const constructor_clock *clock,
                                        mc_timer_clock_stage *stage, int64_t *out) {
    *stage = MC_TIMER_CLOCK_NANO_SAMPLE;
    if (clock->origin == MC_TIMER_ORIGIN_OBSERVED) {
        if (clock->observed == NULL)
            return MC_PLATFORM_NANOTIME_INVALID_ARGUMENT;
        if (clock->observed->nano_status != MC_PLATFORM_NANOTIME_OK)
            return clock->observed->nano_status;
        *out = clock->observed->nano_time;
        return MC_PLATFORM_NANOTIME_OK;
    }
    if (clock->system == NULL || !clock->system->initialized) {
        *stage = MC_TIMER_CLOCK_NANO_INIT;
        ++timer->native.nano_init_calls;
        const mc_platform_nanotime_status status = mc_platform_nanotime_init(clock->system);
        if (status != MC_PLATFORM_NANOTIME_OK)
            return status;
        timer->native.nano_initialized_by_call = true;
    }
    *stage = MC_TIMER_CLOCK_NANO_SAMPLE;
    return mc_platform_nanotime_sample(clock->system, out);
}

static mc_timer_construction_status construct(mc_timer *timer, float rate,
                                              const constructor_clock *clock,
                                              mc_timer_construction_error *error) {
    if (timer == NULL)
        return report(native_error(MC_TIMER_CONSTRUCTION_INVALID_ARGUMENT), error);
    if (timer->native.phase != MC_TIMER_NATIVE_RAW)
        return report(native_error(MC_TIMER_CONSTRUCTION_WRONG_PHASE), error);
    timer->native.phase = MC_TIMER_NATIVE_CONSTRUCTING;
    timer->native.origin = clock->origin;
    timer->speed = 1.0f;
    store(timer, 6);
    timer->remainder = 0.0f;
    store(timer, 11);
    timer->adjustment = 1.0;
    store(timer, 16);
    timer->ticks_per_second = rate;
    store(timer, 21);
    int64_t value;
    timer->native.clock_calls[timer->native.clock_call_count++] = 25;
    const mc_platform_millis_status wall_status = wall(clock, &value);
    if (wall_status != MC_PLATFORM_MILLIS_OK)
        return failed(timer,
                      (mc_timer_construction_error){.status = MC_TIMER_CONSTRUCTION_WALL_FAILED,
                                                    .py_pc = 25,
                                                    .stage = MC_TIMER_CLOCK_WALL,
                                                    .wall = wall_status},
                      error);
    timer->last_wall_millis = value;
    store(timer, 28);
    timer->native.clock_calls[timer->native.clock_call_count++] = 32;
    mc_timer_clock_stage stage;
    const mc_platform_nanotime_status nano_status = nano(timer, clock, &stage, &value);
    if (nano_status != MC_PLATFORM_NANOTIME_OK)
        return failed(timer,
                      (mc_timer_construction_error){.status = MC_TIMER_CONSTRUCTION_NANO_FAILED,
                                                    .py_pc = 32,
                                                    .stage = stage,
                                                    .nano = nano_status},
                      error);
    timer->last_high_millis = value / INT64_C(1000000);
    store(timer, 39);
    timer->native.phase = MC_TIMER_NATIVE_READY;
    return report(native_error(MC_TIMER_CONSTRUCTION_OK), error);
}

mc_timer_construction_status mc_timer_construct_system(mc_timer *timer, float rate,
                                                       mc_platform_nanotime *clock,
                                                       mc_timer_construction_error *error) {
    const constructor_clock source = {.origin = MC_TIMER_ORIGIN_SYSTEM, .system = clock};
    return construct(timer, rate, &source, error);
}

mc_timer_construction_status
mc_timer_construct_observed(mc_timer *timer, float rate,
                            const mc_timer_observed_constructor_clocks *observed,
                            mc_timer_construction_error *error) {
    const constructor_clock source = {.origin = MC_TIMER_ORIGIN_OBSERVED, .observed = observed};
    return construct(timer, rate, &source, error);
}

bool mc_timer_constructor_inspect(const mc_timer *timer, mc_timer_constructor_state *out) {
    if (timer == NULL || out == NULL)
        return false;
    *out = (mc_timer_constructor_state){.phase = timer->native.phase,
                                        .origin = timer->native.origin,
                                        .fields = {.a = timer->ticks_per_second,
                                                   .b = timer->ticks,
                                                   .c = timer->partial_ticks,
                                                   .d = timer->speed,
                                                   .e = timer->remainder,
                                                   .f = timer->last_high_seconds,
                                                   .g = timer->last_wall_millis,
                                                   .h = timer->last_high_millis,
                                                   .i = timer->accumulated_wall_millis,
                                                   .j = timer->adjustment},
                                        .last_store_pc = timer->native.last_store_pc,
                                        .store_count = timer->native.store_count,
                                        .clock_call_count = timer->native.clock_call_count,
                                        .nano_init_calls = timer->native.nano_init_calls,
                                        .nano_initialized_by_call =
                                            timer->native.nano_initialized_by_call,
                                        .failure = timer->native.failure};
    for (size_t i = 0; i < timer->native.store_count; ++i)
        out->stores[i] = timer->native.stores[i];
    for (size_t i = 0; i < timer->native.clock_call_count; ++i)
        out->clock_calls[i] = timer->native.clock_calls[i];
    return true;
}

mc_timer_construction_status mc_timer_retire(mc_timer **slot, mc_timer_construction_error *error) {
    if (slot == NULL)
        return report(native_error(MC_TIMER_CONSTRUCTION_INVALID_ARGUMENT), error);
    if (*slot == NULL)
        return report(native_error(MC_TIMER_CONSTRUCTION_OK), error);
    if ((*slot)->native.phase == MC_TIMER_NATIVE_CONSTRUCTING)
        return report(native_error(MC_TIMER_CONSTRUCTION_BUSY), error);
    (*slot)->native.phase = MC_TIMER_NATIVE_RETIRING;
    free(*slot);
    *slot = NULL;
    return report(native_error(MC_TIMER_CONSTRUCTION_OK), error);
}
