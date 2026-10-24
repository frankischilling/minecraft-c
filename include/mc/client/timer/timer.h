#ifndef MC_CLIENT_TIMER_TIMER_H
#define MC_CLIENT_TIMER_TIMER_H

#include <stdbool.h>
#include <stdint.h>

typedef struct mc_timer mc_timer;

/* Values from one source-order pair: currentTimeMillis first, nanoTime second.
 * nano_time is the raw signed nanosecond sample, not a converted duration.
 * The caller owns sampling/order; this module never queries an OS clock. */
typedef struct mc_timer_clock_sample {
    int64_t wall_millis;
    int64_t nano_time;
} mc_timer_clock_sample;

typedef struct mc_timer_view {
    int32_t ticks;
    float partial_ticks;
    float ticks_per_second;
    float speed;
} mc_timer_view;

/* py(float): retain the supplied clocks, speed 1, zero ticks/remainder and
 * adjustment 1. The last high-resolution seconds field deliberately starts at
 * zero rather than the constructor sample. Any binary32 tick rate is admitted,
 * including negative, zero, infinities and NaNs. No clock, SDL or World call.
 * out is required and unchanged on failure (invalid output or allocation).
 * Release a previous owner before replacing it on success. */
bool mc_timer_create(float ticks_per_second, mc_timer_clock_sample clocks, mc_timer **out);
void mc_timer_free(mc_timer *timer); /* NULL is harmless. */

/* Execute one complete py.a() using the next supplied sample pair. Raw
 * nanotime divides by 1000000 with Java long truncation toward zero; signed
 * long differences/additions wrap. Wall jumps >1000 or <0 reset only the last
 * high-resolution seconds. Drift correction retains the widened 0.2f factor.
 * Elapsed seconds are clamped to [0,1]; tick rate/speed/remainder are not clamped.
 * The float remainder subtracts the uncapped Java f2i result before ticks are
 * capped above at 10. Negative ticks and nonfinite partials are real results.
 * NULL fails without mutation; otherwise no allocation or native failure.
 * Calls on one owner require exclusive access. */
bool mc_timer_update(mc_timer *timer, mc_timer_clock_sample clocks);

/* Source field d assignment: every binary32 value is admitted. Existing ticks,
 * remainder and clock synchronization state are preserved. NULL fails. */
bool mc_timer_set_speed(mc_timer *timer, float speed);

/* Read without advancing clocks/ticks. NULL arguments fail and preserve out.
 * out must be disjoint from owner storage. Pause handling belongs to the caller:
 * preserve its render partial outside Timer while Timer continues updating. */
bool mc_timer_view_get(const mc_timer *timer, mc_timer_view *out);

/* Floating contract: IEEE binary32/binary64, round-to-nearest ties-to-even,
 * gradual underflow, masked traps, no excess precision, reassociation or fused
 * operations. The module does not change the caller's floating environment.
 * NaN classification is retained; payload/sign and fenv flags are not promised.
 * Native allocation policy and authored tests are not original execution or
 * OS-clock/default-timing qualification. */

#endif
