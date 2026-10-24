#include "mc/client/timer/timer.h"

#include "mc/core/java/java.h"

#include <float.h>
#include <math.h>
#include <stdlib.h>

_Static_assert(FLT_RADIX == 2 && FLT_MANT_DIG == 24 && FLT_MAX_EXP == 128,
               "Timer requires IEEE binary32");
_Static_assert(DBL_MANT_DIG == 53 && DBL_MAX_EXP == 1024, "Timer requires IEEE binary64");

struct mc_timer {
    float ticks_per_second, speed, remainder;
    int32_t ticks;
    double last_high_seconds, adjustment;
    int64_t last_wall_millis, last_high_millis, accumulated_wall_millis;
};

/* Java d2f overflows to infinity, including the half-ulp tie above FLT_MAX.
 * Avoid C17 casts outside the finite float range. Ordinary in-range casts use
 * the admitted nearest-even environment; NaN payloads are outside the contract. */
static float narrowed(double value) {
    if (isnan(value))
        return NAN;
    if (value >= 0x1.ffffffp127)
        return INFINITY;
    if (value <= -0x1.ffffffp127)
        return -INFINITY;
    if (value > (double)FLT_MAX)
        return FLT_MAX;
    if (value < -(double)FLT_MAX)
        return -FLT_MAX;
    return (float)value;
}

bool mc_timer_create(float ticks_per_second, mc_timer_clock_sample clocks, mc_timer **out) {
    if (out == NULL)
        return false;
    mc_timer *timer = malloc(sizeof(*timer));
    if (timer == NULL)
        return false;
    *timer = (mc_timer){.ticks_per_second = ticks_per_second,
                        .speed = 1.0f,
                        .adjustment = 1.0,
                        .last_wall_millis = clocks.wall_millis,
                        .last_high_millis = clocks.nano_time / INT64_C(1000000)};
    *out = timer;
    return true;
}

void mc_timer_free(mc_timer *timer) {
    free(timer);
}

bool mc_timer_update(mc_timer *timer, mc_timer_clock_sample clocks) {
    if (timer == NULL)
        return false;
    const int64_t wall_delta = mc_java_i64_sub(clocks.wall_millis, timer->last_wall_millis);
    const int64_t high_millis = clocks.nano_time / INT64_C(1000000);
    volatile double high_seconds = (double)high_millis / 1000.0;
    if (wall_delta > INT64_C(1000) || wall_delta < 0) {
        timer->last_high_seconds = high_seconds;
    } else {
        timer->accumulated_wall_millis =
            mc_java_i64_add(timer->accumulated_wall_millis, wall_delta);
        if (timer->accumulated_wall_millis > INT64_C(1000)) {
            const int64_t high_interval = mc_java_i64_sub(high_millis, timer->last_high_millis);
            /* The numerator is positive here. Java floating division by +0
             * yields +infinity; avoid C17's zero-divisor expression. */
            volatile double ratio =
                high_interval == 0 ? INFINITY
                                   : (double)timer->accumulated_wall_millis / (double)high_interval;
            volatile double difference = ratio - timer->adjustment;
            volatile double weighted = difference * (double)0.2f;
            volatile double adjusted = timer->adjustment + weighted;
            timer->adjustment = adjusted;
            timer->last_high_millis = high_millis;
            timer->accumulated_wall_millis = 0;
        }
        if (timer->accumulated_wall_millis < 0)
            timer->last_high_millis = high_millis;
    }
    timer->last_wall_millis = clocks.wall_millis;
    volatile double high_delta = high_seconds - timer->last_high_seconds;
    volatile double elapsed = high_delta * timer->adjustment;
    timer->last_high_seconds = high_seconds;
    if (elapsed < 0.0)
        elapsed = 0.0;
    if (elapsed > 1.0)
        elapsed = 1.0;
    volatile double scaled = elapsed * (double)timer->speed;
    scaled = scaled * (double)timer->ticks_per_second;
    volatile double accumulated = (double)timer->remainder + scaled;
    timer->remainder = narrowed(accumulated);
    const int32_t ticks = mc_java_float_to_i32(timer->remainder);
    volatile float whole = (float)ticks;
    volatile float remainder = timer->remainder - whole;
    timer->remainder = remainder;
    timer->ticks = ticks > 10 ? 10 : ticks;
    return true;
}

bool mc_timer_set_speed(mc_timer *timer, float speed) {
    if (timer == NULL)
        return false;
    timer->speed = speed;
    return true;
}

bool mc_timer_view_get(const mc_timer *timer, mc_timer_view *out) {
    if (timer == NULL || out == NULL)
        return false;
    *out = (mc_timer_view){.ticks = timer->ticks,
                           .partial_ticks = timer->remainder,
                           .ticks_per_second = timer->ticks_per_second,
                           .speed = timer->speed};
    return true;
}
