#include "mc/client/timer/timer.h"

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

/* Authored clock samples and literal source-derived outcomes, with no OS/JVM
 * clock or expected-state injection. This is a native source contract. */
static unsigned int checks;
#define COUNT(values) (sizeof(values) / sizeof((values)[0]))
#define REQUIRE(condition)                                                                         \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

static uint32_t bits(float value) {
    uint32_t result;
    _Static_assert(sizeof(result) == sizeof(value), "binary32 Timer result");
    memcpy(&result, &value, sizeof(result));
    return result;
}

static bool outcome(const mc_timer *timer, int32_t ticks, uint32_t partial) {
    mc_timer_view view;
    return mc_timer_view_get(timer, &view) && view.ticks == ticks &&
           bits(view.partial_ticks) == partial;
}

typedef struct step {
    int64_t wall, nanos;
    int32_t ticks;
    uint32_t partial;
} step;

static int test_initial_and_fractions(void) {
    int result = 1;
    mc_timer *timer = NULL;
    REQUIRE(mc_timer_create(7.25f, (mc_timer_clock_sample){5000, INT64_C(123000000000)}, &timer));
    mc_timer_view view;
    REQUIRE(mc_timer_view_get(timer, &view) && view.ticks == 0 && bits(view.partial_ticks) == 0 &&
            view.speed == 1.0f && view.ticks_per_second == 7.25f);
    /* Constructor nanoTime was 123 seconds; f nevertheless starts at zero. */
    REQUIRE(mc_timer_update(timer, (mc_timer_clock_sample){5000, INT64_C(123000000000)}) &&
            outcome(timer, 7, UINT32_C(0x3e800000)));
    mc_timer_free(timer);
    timer = NULL;
    REQUIRE(mc_timer_create(20.0f, (mc_timer_clock_sample){0, 0}, &timer));
    static const step steps[] = {{25, 25000000, 0, 0x3f000000},
                                 {50, 50000000, 1, 0},
                                 {75, 75000000, 0, 0x3f000000},
                                 {125, 125000000, 1, 0x3f000000}};
    for (size_t i = 0; i < COUNT(steps); ++i)
        REQUIRE(mc_timer_update(timer, (mc_timer_clock_sample){steps[i].wall, steps[i].nanos}) &&
                outcome(timer, steps[i].ticks, steps[i].partial));
    result = 0;
cleanup:
    mc_timer_free(timer);
    return result;
}

static int test_resets_and_clamps(void) {
    int result = 1;
    mc_timer *timer = NULL;
    REQUIRE(mc_timer_create(20.75f, (mc_timer_clock_sample){0, 0}, &timer));
    /* Exactly 1000 is admitted; uncapped 20 is subtracted before max 10. */
    REQUIRE(mc_timer_update(timer, (mc_timer_clock_sample){1000, 1000000000}) &&
            outcome(timer, 10, UINT32_C(0x3f400000)));
    mc_timer_free(timer);
    timer = NULL;
    REQUIRE(mc_timer_create(20.75f, (mc_timer_clock_sample){0, 0}, &timer));
    REQUIRE(mc_timer_update(timer, (mc_timer_clock_sample){1001, 1000000000}) &&
            outcome(timer, 0, 0));
    REQUIRE(mc_timer_update(timer, (mc_timer_clock_sample){1126, 1125000000}) &&
            outcome(timer, 2, UINT32_C(0x3f180000)));
    mc_timer_free(timer);
    timer = NULL;
    REQUIRE(mc_timer_create(20.75f, (mc_timer_clock_sample){0, 0}, &timer));
    /* Monotonic two seconds with only 500 wall ms clamps the elapsed seconds.
     * The discarded second does not become a later tick backlog. */
    REQUIRE(mc_timer_update(timer, (mc_timer_clock_sample){500, 2000000000}) &&
            outcome(timer, 10, UINT32_C(0x3f400000)));
    REQUIRE(mc_timer_update(timer, (mc_timer_clock_sample){625, 2125000000}) &&
            outcome(timer, 3, UINT32_C(0x3eb00000)));
    mc_timer_free(timer);
    timer = NULL;
    REQUIRE(mc_timer_create(20.0f, (mc_timer_clock_sample){0, 0}, &timer));
    static const step backwards_high[] = {
        {125, 125000000, 2, 0x3f000000}, {250, 0, 0, 0x3f000000}, {375, 125000000, 3, 0}};
    for (size_t i = 0; i < COUNT(backwards_high); ++i)
        REQUIRE(mc_timer_update(timer, (mc_timer_clock_sample){backwards_high[i].wall,
                                                               backwards_high[i].nanos}) &&
                outcome(timer, backwards_high[i].ticks, backwards_high[i].partial));
    mc_timer_free(timer);
    timer = NULL;
    REQUIRE(mc_timer_create(20.0f, (mc_timer_clock_sample){0, 0}, &timer));
    static const step backwards_wall[] = {{125, 125000000, 2, 0x3f000000},
                                          {124, 250000000, 0, 0x3f000000},
                                          {249, 375000000, 3, 0},
                                          {2250, 2000000000, 0, 0},
                                          {2375, 2125000000, 2, 0x3f000000}};
    for (size_t i = 0; i < COUNT(backwards_wall); ++i)
        REQUIRE(mc_timer_update(timer, (mc_timer_clock_sample){backwards_wall[i].wall,
                                                               backwards_wall[i].nanos}) &&
                outcome(timer, backwards_wall[i].ticks, backwards_wall[i].partial));
    result = 0;
cleanup:
    mc_timer_free(timer);
    return result;
}

static int test_truncation_and_long_wrap(void) {
    int result = 1;
    mc_timer *timer = NULL;
    REQUIRE(mc_timer_create(1000.0f, (mc_timer_clock_sample){0, -1999999}, &timer));
    REQUIRE(mc_timer_update(timer, (mc_timer_clock_sample){0, -999999}) && outcome(timer, 0, 0));
    REQUIRE(mc_timer_update(timer, (mc_timer_clock_sample){1, 1999999}) && outcome(timer, 1, 0));
    REQUIRE(mc_timer_update(timer, (mc_timer_clock_sample){2, 2000000}) && outcome(timer, 1, 0));
    mc_timer_free(timer);
    timer = NULL;
    REQUIRE(mc_timer_create(20.0f, (mc_timer_clock_sample){INT64_MAX - 100, 0}, &timer));
    REQUIRE(mc_timer_update(timer, (mc_timer_clock_sample){INT64_MIN + 24, 125000000}) &&
            outcome(timer, 2, UINT32_C(0x3f000000)));
    mc_timer_free(timer);
    timer = NULL;
    REQUIRE(mc_timer_create(20.0f, (mc_timer_clock_sample){INT64_MIN + 100, 0}, &timer));
    REQUIRE(mc_timer_update(timer, (mc_timer_clock_sample){INT64_MAX - 24, 125000000}) &&
            outcome(timer, 0, 0));
    REQUIRE(mc_timer_update(timer, (mc_timer_clock_sample){INT64_MIN + 100, 250000000}) &&
            outcome(timer, 2, UINT32_C(0x3f000000)));
    mc_timer_free(timer);
    timer = NULL;
    REQUIRE(mc_timer_create(10.5f, (mc_timer_clock_sample){0, INT64_MIN}, &timer));
    REQUIRE(mc_timer_update(timer, (mc_timer_clock_sample){0, INT64_MIN}) && outcome(timer, 0, 0));
    REQUIRE(mc_timer_update(timer, (mc_timer_clock_sample){0, INT64_MAX}) &&
            outcome(timer, 10, UINT32_C(0x3f000000)));
    result = 0;
cleanup:
    mc_timer_free(timer);
    return result;
}

static int test_drift(void) {
    int result = 1;
    mc_timer *timer = NULL;
    REQUIRE(mc_timer_create(125.0f, (mc_timer_clock_sample){0, 0}, &timer));
    REQUIRE(mc_timer_update(timer, (mc_timer_clock_sample){512, 256000000}) &&
            outcome(timer, 10, 0));
    /* Accumulated 1024/high 512 gives ratio 2. Widened 0.2f yields adjustment
     * 1.2000000029802322; the next float tick total is 38.40000152587890625. */
    REQUIRE(mc_timer_update(timer, (mc_timer_clock_sample){1024, 512000000}) &&
            outcome(timer, 10, UINT32_C(0x3ecccd00)));
    REQUIRE(mc_timer_update(timer, (mc_timer_clock_sample){1536, 768000000}) &&
            outcome(timer, 10, UINT32_C(0x3f4ccd00)));
    mc_timer_free(timer);
    timer = NULL;
    /* Constructor nanoTime truncates to -1ms, not floor(-1.999999)=-2. */
    REQUIRE(mc_timer_create(125.0f, (mc_timer_clock_sample){0, -1999999}, &timer));
    REQUIRE(mc_timer_update(timer, (mc_timer_clock_sample){512, 255000000}) &&
            outcome(timer, 10, UINT32_C(0x3f600000)));
    REQUIRE(mc_timer_update(timer, (mc_timer_clock_sample){1024, 511000000}) &&
            outcome(timer, 10, UINT32_C(0x3e8ccd00)));
    mc_timer_free(timer);
    timer = NULL;
    /* A negative synchronization ratio may make adjustment negative. Clamp
     * the product, not the raw high-resolution delta before multiplication. */
    REQUIRE(mc_timer_create(1000.0f, (mc_timer_clock_sample){0, 1000000}, &timer));
    REQUIRE(mc_timer_update(timer, (mc_timer_clock_sample){512, 2000000}) && outcome(timer, 2, 0));
    REQUIRE(mc_timer_update(timer, (mc_timer_clock_sample){1024, 0}) && outcome(timer, 10, 0));
    mc_timer_free(timer);
    timer = NULL;
    REQUIRE(mc_timer_create(1.0f, (mc_timer_clock_sample){0, 0}, &timer));
    REQUIRE(mc_timer_update(timer, (mc_timer_clock_sample){512, 0}) && outcome(timer, 0, 0));
    REQUIRE(mc_timer_update(timer, (mc_timer_clock_sample){1024, 0}));
    mc_timer_view view;
    REQUIRE(mc_timer_view_get(timer, &view) && view.ticks == 0 && isnan(view.partial_ticks));
    REQUIRE(mc_timer_update(timer, (mc_timer_clock_sample){1536, 1000000}));
    REQUIRE(mc_timer_view_get(timer, &view) && view.ticks == 0 && isnan(view.partial_ticks));
    result = 0;
cleanup:
    mc_timer_free(timer);
    return result;
}

static int test_speed_and_nonfinite(void) {
    int result = 1;
    mc_timer *timer = NULL;
    REQUIRE(mc_timer_create(20.75f, (mc_timer_clock_sample){0, 0}, &timer));
    REQUIRE(mc_timer_update(timer, (mc_timer_clock_sample){125, 125000000}) &&
            outcome(timer, 2, UINT32_C(0x3f180000)));
    REQUIRE(mc_timer_set_speed(timer, 0.0f) && outcome(timer, 2, UINT32_C(0x3f180000)));
    REQUIRE(mc_timer_update(timer, (mc_timer_clock_sample){250, 250000000}) &&
            outcome(timer, 0, UINT32_C(0x3f180000)));
    REQUIRE(mc_timer_set_speed(timer, 2.0f));
    REQUIRE(mc_timer_update(timer, (mc_timer_clock_sample){375, 375000000}) &&
            outcome(timer, 5, UINT32_C(0x3f480000)));
    REQUIRE(mc_timer_set_speed(timer, -1.0f));
    REQUIRE(mc_timer_update(timer, (mc_timer_clock_sample){500, 500000000}) &&
            outcome(timer, -1, UINT32_C(0xbf500000)));
    mc_timer_view view;
    REQUIRE(mc_timer_view_get(timer, &view) && view.speed == -1.0f &&
            view.ticks_per_second == 20.75f);
    mc_timer_free(timer);
    timer = NULL;
    REQUIRE(mc_timer_create(0x1p31f, (mc_timer_clock_sample){0, 0}, &timer));
    REQUIRE(mc_timer_update(timer, (mc_timer_clock_sample){0, 1000000000}) &&
            outcome(timer, 10, 0));
    mc_timer_free(timer);
    timer = NULL;
    REQUIRE(mc_timer_create(-0x1p31f, (mc_timer_clock_sample){0, 0}, &timer));
    REQUIRE(mc_timer_update(timer, (mc_timer_clock_sample){0, 1000000000}) &&
            outcome(timer, INT32_MIN, 0));
    mc_timer_free(timer);
    timer = NULL;
    REQUIRE(mc_timer_create(FLT_MAX, (mc_timer_clock_sample){0, 0}, &timer));
    REQUIRE(mc_timer_update(timer, (mc_timer_clock_sample){0, 1000000000}) &&
            outcome(timer, 10, UINT32_C(0x7f7fffff)));
    mc_timer_free(timer);
    timer = NULL;
    REQUIRE(mc_timer_create(FLT_MAX, (mc_timer_clock_sample){0, 0}, &timer) &&
            mc_timer_set_speed(timer, 2.0f));
    REQUIRE(mc_timer_update(timer, (mc_timer_clock_sample){0, 1000000000}));
    REQUIRE(mc_timer_view_get(timer, &view) && view.ticks == 10 && isinf(view.partial_ticks) &&
            !signbit(view.partial_ticks));
    mc_timer_free(timer);
    timer = NULL;
    REQUIRE(mc_timer_create(FLT_MAX, (mc_timer_clock_sample){0, 0}, &timer) &&
            mc_timer_set_speed(timer, -2.0f));
    REQUIRE(mc_timer_update(timer, (mc_timer_clock_sample){0, 1000000000}));
    REQUIRE(mc_timer_view_get(timer, &view) && view.ticks == INT32_MIN &&
            isinf(view.partial_ticks) && signbit(view.partial_ticks));
    mc_timer_free(timer);
    timer = NULL;
    REQUIRE(mc_timer_create(20.0f, (mc_timer_clock_sample){0, 0}, &timer) &&
            mc_timer_set_speed(timer, INFINITY));
    REQUIRE(mc_timer_update(timer, (mc_timer_clock_sample){0, 0}));
    REQUIRE(mc_timer_view_get(timer, &view) && view.ticks == 0 && isnan(view.partial_ticks));
    mc_timer_free(timer);
    timer = NULL;
    REQUIRE(mc_timer_create(NAN, (mc_timer_clock_sample){0, 0}, &timer));
    REQUIRE(mc_timer_update(timer, (mc_timer_clock_sample){0, 1000000000}));
    REQUIRE(mc_timer_view_get(timer, &view) && view.ticks == 0 && isnan(view.partial_ticks) &&
            isnan(view.ticks_per_second));
    mc_timer_free(timer);
    timer = NULL;
    REQUIRE(mc_timer_create(20.0f, (mc_timer_clock_sample){0, 0}, &timer) &&
            mc_timer_set_speed(timer, NAN));
    REQUIRE(mc_timer_update(timer, (mc_timer_clock_sample){0, 1000000000}));
    REQUIRE(mc_timer_view_get(timer, &view) && view.ticks == 0 && isnan(view.partial_ticks) &&
            isnan(view.speed));
    mc_timer_free(timer);
    timer = NULL;
    REQUIRE(mc_timer_create(0.0f, (mc_timer_clock_sample){0, 0}, &timer));
    REQUIRE(mc_timer_update(timer, (mc_timer_clock_sample){0, 1000000000}) && outcome(timer, 0, 0));
    REQUIRE(mc_timer_set_speed(timer, INFINITY));
    REQUIRE(mc_timer_update(timer, (mc_timer_clock_sample){0, 2000000000}));
    REQUIRE(mc_timer_view_get(timer, &view) && view.ticks == 0 && isnan(view.partial_ticks));
    result = 0;
cleanup:
    mc_timer_free(timer);
    return result;
}

static int test_invalid(void) {
    int result = 1;
    mc_timer *timer = NULL;
    REQUIRE(!mc_timer_create(20.0f, (mc_timer_clock_sample){0, 0}, NULL));
    REQUIRE(mc_timer_create(20.0f, (mc_timer_clock_sample){0, 0}, &timer));
    mc_timer_view view = {.ticks = 123, .partial_ticks = 0.25f, .ticks_per_second = 7, .speed = -1};
    unsigned char before[sizeof(view)];
    memcpy(before, &view, sizeof(view));
    REQUIRE(!mc_timer_view_get(NULL, &view) && memcmp(before, &view, sizeof(view)) == 0);
    REQUIRE(!mc_timer_view_get(timer, NULL));
    REQUIRE(!mc_timer_update(NULL, (mc_timer_clock_sample){INT64_MAX, INT64_MIN}));
    REQUIRE(!mc_timer_set_speed(NULL, NAN));
    REQUIRE(outcome(timer, 0, 0));
    result = 0;
cleanup:
    mc_timer_free(timer);
    mc_timer_free(NULL);
    return result;
}

int main(void) {
    if (test_initial_and_fractions() || test_resets_and_clamps() ||
        test_truncation_and_long_wrap() || test_drift() || test_speed_and_nonfinite() ||
        test_invalid())
        return 1;
    printf("PASS: %u Timer native source-contract checks\n", checks);
    return 0;
}
