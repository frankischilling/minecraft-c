#include "client/timer/timer_internal.h"
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <xmmintrin.h>

/* Authored source-prefix contracts. No captured answers, substituted Timer,
 * callback game child or input into an already constructed source receiver. */
static unsigned checks;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            fprintf(stderr, "Timer constructor check%u line%d: %s\n", checks, __LINE__, #x);       \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

static uint32_t fb(float value) {
    uint32_t result;
    _Static_assert(sizeof(result) == sizeof(value), "binary32 fields");
    memcpy(&result, &value, sizeof(result));
    return result;
}

static uint64_t db(double value) {
    uint64_t result;
    _Static_assert(sizeof(result) == sizeof(value), "binary64 fields");
    memcpy(&result, &value, sizeof(result));
    return result;
}

static bool same_float(float a, float b) {
    return (isnan(a) && isnan(b)) || fb(a) == fb(b);
}

static bool defaults(const mc_timer *timer, bool started) {
    return &timer->partial_ticks != &timer->remainder && fb(timer->partial_ticks) == 0 &&
           fb(timer->remainder) == 0 && timer->ticks == 0 && db(timer->last_high_seconds) == 0 &&
           timer->accumulated_wall_millis == 0 &&
           fb(timer->speed) == (started ? UINT32_C(0x3f800000) : 0) &&
           db(timer->adjustment) == (started ? UINT64_C(0x3ff0000000000000) : 0);
}

static bool prefix(const mc_timer_constructor_state *state, size_t stores, size_t clocks) {
    static const int32_t actual_stores[] = {6, 11, 16, 21, 28, 39};
    static const int32_t actual_clocks[] = {25, 32};
    if (state->store_count != stores || state->clock_call_count != clocks ||
        state->last_store_pc != (stores == 0 ? -1 : actual_stores[stores - 1]))
        return false;
    for (size_t i = 0; i < stores; ++i)
        if (state->stores[i] != actual_stores[i])
            return false;
    for (size_t i = 0; i < clocks; ++i)
        if (state->clock_calls[i] != actual_clocks[i])
            return false;
    return true;
}

static int raw_and_retirement(void) {
    int result = 1;
    mc_timer *timer = NULL;
    mc_timer_construction_error error;
    mc_timer_constructor_state state;
    CHECK(mc_timer_allocate_raw(0, NULL, &error) == MC_TIMER_CONSTRUCTION_INVALID_ARGUMENT);
    CHECK(error.py_pc == -1);
    CHECK(mc_timer_allocate_raw(0, &timer, &error) == MC_TIMER_CONSTRUCTION_OK);
    CHECK(defaults(timer, false) && fb(timer->ticks_per_second) == 0 &&
          timer->last_wall_millis == 0 && timer->last_high_millis == 0);
    CHECK(mc_timer_constructor_inspect(timer, &state) && state.phase == MC_TIMER_NATIVE_RAW &&
          state.origin == MC_TIMER_ORIGIN_RAW && prefix(&state, 0, 0));
    CHECK(fb(state.fields.c) == fb(timer->partial_ticks) &&
          fb(state.fields.e) == fb(timer->remainder));
    mc_timer *before_owner = timer;
    CHECK(mc_timer_allocate_raw(1, &timer, &error) == MC_TIMER_CONSTRUCTION_ALLOCATION_LIMIT &&
          timer == before_owner && error.allocation_bytes == sizeof(*timer));
    unsigned char before[sizeof(*timer)];
    memcpy(before, timer, sizeof(before));
    mc_timer_view view;
    memset(&view, 0xa5, sizeof(view));
    unsigned char saved_view[sizeof(view)];
    memcpy(saved_view, &view, sizeof(view));
    CHECK(!mc_timer_view_get(timer, &view) && memcmp(&view, saved_view, sizeof(view)) == 0);
    CHECK(!mc_timer_set_speed(timer, 2.0f) &&
          !mc_timer_update(timer, (mc_timer_clock_sample){INT64_MIN, INT64_MAX}));
    CHECK(memcmp(before, timer, sizeof(before)) == 0);
    CHECK(!mc_timer_constructor_inspect(NULL, &state) &&
          !mc_timer_constructor_inspect(timer, NULL));
    CHECK(mc_timer_retire(&timer, &error) == MC_TIMER_CONSTRUCTION_OK && timer == NULL);
    CHECK(mc_timer_retire(&timer, NULL) == MC_TIMER_CONSTRUCTION_OK);
    CHECK(mc_timer_retire(NULL, &error) == MC_TIMER_CONSTRUCTION_INVALID_ARGUMENT);
    result = 0;
cleanup:
    mc_timer_free(timer);
    return result;
}

static int failed_prefixes(void) {
    int result = 1;
    mc_timer *timer = NULL;
    mc_timer_construction_error error;
    mc_timer_constructor_state state;
    const mc_platform_millis_status wall_failures[] = {MC_PLATFORM_MILLIS_INVALID_ARGUMENT,
                                                       MC_PLATFORM_MILLIS_PLATFORM_FAILURE,
                                                       MC_PLATFORM_MILLIS_RANGE};
    const mc_platform_nanotime_status nano_failures[] = {
        MC_PLATFORM_NANOTIME_INVALID_ARGUMENT, MC_PLATFORM_NANOTIME_PLATFORM_FAILURE,
        MC_PLATFORM_NANOTIME_RANGE, MC_PLATFORM_NANOTIME_UNSUPPORTED_FP};
    for (size_t i = 0; i < sizeof(wall_failures) / sizeof(wall_failures[0]); ++i) {
        const mc_timer_observed_constructor_clocks observed = {.wall_status = wall_failures[i],
                                                               .wall_millis = INT64_MIN,
                                                               .nano_status =
                                                                   MC_PLATFORM_NANOTIME_RANGE,
                                                               .nano_time = INT64_MAX};
        CHECK(mc_timer_allocate_raw(0, &timer, NULL) == MC_TIMER_CONSTRUCTION_OK);
        CHECK(mc_timer_construct_observed(timer, 7.25f, &observed, &error) ==
              MC_TIMER_CONSTRUCTION_WALL_FAILED);
        CHECK(mc_timer_constructor_inspect(timer, &state) &&
              state.phase == MC_TIMER_NATIVE_FAILED && state.origin == MC_TIMER_ORIGIN_OBSERVED &&
              prefix(&state, 4, 1));
        CHECK(error.py_pc == 25 && error.stage == MC_TIMER_CLOCK_WALL &&
              error.wall == wall_failures[i] && error.nano == MC_PLATFORM_NANOTIME_OK);
        CHECK(defaults(timer, true) && fb(timer->ticks_per_second) == fb(7.25f) &&
              timer->last_wall_millis == 0 && timer->last_high_millis == 0);
        CHECK(fb(state.fields.c) == fb(timer->partial_ticks) &&
              fb(state.fields.e) == fb(timer->remainder));
        unsigned char before[sizeof(*timer)];
        memcpy(before, timer, sizeof(before));
        CHECK(mc_timer_construct_observed(timer, 20.0f, NULL, &error) ==
              MC_TIMER_CONSTRUCTION_WRONG_PHASE);
        CHECK(!mc_timer_update(timer, (mc_timer_clock_sample){0, 0}) &&
              !mc_timer_set_speed(timer, 1.0f));
        mc_timer_view view;
        CHECK(!mc_timer_view_get(timer, &view) && memcmp(before, timer, sizeof(before)) == 0);
        CHECK(mc_timer_retire(&timer, NULL) == MC_TIMER_CONSTRUCTION_OK && timer == NULL);
    }
    for (size_t i = 0; i < sizeof(nano_failures) / sizeof(nano_failures[0]); ++i) {
        const mc_timer_observed_constructor_clocks observed = {.wall_status = MC_PLATFORM_MILLIS_OK,
                                                               .wall_millis = INT64_MIN + 7,
                                                               .nano_status = nano_failures[i],
                                                               .nano_time = INT64_MAX};
        CHECK(mc_timer_allocate_raw(0, &timer, NULL) == MC_TIMER_CONSTRUCTION_OK);
        CHECK(mc_timer_construct_observed(timer, 20.0f, &observed, &error) ==
              MC_TIMER_CONSTRUCTION_NANO_FAILED);
        CHECK(mc_timer_constructor_inspect(timer, &state) &&
              state.phase == MC_TIMER_NATIVE_FAILED && prefix(&state, 5, 2) &&
              state.nano_init_calls == 0);
        CHECK(error.py_pc == 32 && error.stage == MC_TIMER_CLOCK_NANO_SAMPLE &&
              error.nano == nano_failures[i] && error.wall == MC_PLATFORM_MILLIS_OK);
        CHECK(defaults(timer, true) && timer->last_wall_millis == INT64_MIN + 7 &&
              timer->last_high_millis == 0 && fb(state.fields.c) == fb(timer->partial_ticks) &&
              fb(state.fields.e) == fb(timer->remainder));
        CHECK(mc_timer_retire(&timer, NULL) == MC_TIMER_CONSTRUCTION_OK && timer == NULL);
    }
    CHECK(mc_timer_allocate_raw(0, &timer, NULL) == MC_TIMER_CONSTRUCTION_OK);
    CHECK(mc_timer_construct_observed(timer, -0.0f, NULL, &error) ==
              MC_TIMER_CONSTRUCTION_WALL_FAILED &&
          error.py_pc == 25 && fb(timer->ticks_per_second) == UINT32_C(0x80000000));
    CHECK(mc_timer_constructor_inspect(timer, &state) && prefix(&state, 4, 1));
    result = 0;
cleanup:
    mc_timer_free(timer);
    return result;
}

static int completed_and_legacy(void) {
    int result = 1;
    mc_timer *timer = NULL, *legacy = NULL;
    mc_timer_constructor_state state;
    const int64_t nanos[] = {INT64_MIN, -1999999, -999999, 0, 1999999, INT64_MAX};
    const float rates[] = {20.0f, 7.25f, 0.0f, -0.0f, -1.0f, INFINITY, -INFINITY, NAN};
    for (size_t r = 0; r < sizeof(rates) / sizeof(rates[0]); ++r)
        for (size_t n = 0; n < sizeof(nanos) / sizeof(nanos[0]); ++n) {
            const mc_timer_clock_sample sample = {INT64_MIN + 1, nanos[n]};
            const mc_timer_observed_constructor_clocks observed = {
                .wall_millis = sample.wall_millis, .nano_time = sample.nano_time};
            CHECK(mc_timer_allocate_raw(0, &timer, NULL) == MC_TIMER_CONSTRUCTION_OK);
            CHECK(mc_timer_construct_observed(timer, rates[r], &observed, NULL) ==
                  MC_TIMER_CONSTRUCTION_OK);
            CHECK(mc_timer_constructor_inspect(timer, &state) &&
                  state.phase == MC_TIMER_NATIVE_READY && prefix(&state, 6, 2) &&
                  state.nano_init_calls == 0);
            CHECK(defaults(timer, true) && same_float(timer->ticks_per_second, rates[r]) &&
                  timer->last_wall_millis == sample.wall_millis &&
                  timer->last_high_millis == sample.nano_time / INT64_C(1000000));
            CHECK(fb(state.fields.c) == fb(timer->partial_ticks) &&
                  fb(state.fields.e) == fb(timer->remainder));
            CHECK(mc_timer_create(rates[r], sample, &legacy));
            CHECK(mc_timer_constructor_inspect(legacy, &state) &&
                  state.phase == MC_TIMER_NATIVE_READY &&
                  state.origin == MC_TIMER_ORIGIN_LEGACY_SAMPLES && prefix(&state, 0, 0));
            const mc_timer_clock_sample steps[] = {{INT64_MIN + 26, 25000000},
                                                   {INT64_MIN + 1026, 512000000},
                                                   {INT64_MAX, -1999999},
                                                   {INT64_MIN, INT64_MIN}};
            for (size_t s = 0; s < sizeof(steps) / sizeof(steps[0]); ++s) {
                CHECK(mc_timer_update(timer, steps[s]) && mc_timer_update(legacy, steps[s]));
                mc_timer_view a, b;
                CHECK(mc_timer_view_get(timer, &a) && mc_timer_view_get(legacy, &b));
                CHECK(a.ticks == b.ticks && same_float(a.partial_ticks, b.partial_ticks) &&
                      same_float(a.ticks_per_second, b.ticks_per_second) &&
                      same_float(a.speed, b.speed));
                CHECK(same_float(timer->partial_ticks, timer->remainder) &&
                      same_float(a.partial_ticks, timer->partial_ticks));
            }
            CHECK(mc_timer_retire(&timer, NULL) == MC_TIMER_CONSTRUCTION_OK && timer == NULL);
            mc_timer_free(legacy);
            legacy = NULL;
        }
    result = 0;
cleanup:
    mc_timer_free(timer);
    mc_timer_free(legacy);
    return result;
}

static int c_before_first_update(void) {
    int result = 1;
    mc_timer *timer = NULL;
    const mc_timer_observed_constructor_clocks observed = {0};
    CHECK(mc_timer_allocate_raw(0, &timer, NULL) == MC_TIMER_CONSTRUCTION_OK);
    CHECK(mc_timer_construct_observed(timer, 20.0f, &observed, NULL) == MC_TIMER_CONSTRUCTION_OK);
    CHECK(&timer->partial_ticks != &timer->remainder && fb(timer->partial_ticks) == 0 &&
          fb(timer->remainder) == 0);
    mc_timer_view view;
    CHECK(mc_timer_view_get(timer, &view) && fb(view.partial_ticks) == 0);
    CHECK(mc_timer_update(timer, (mc_timer_clock_sample){25, 25000000}));
    CHECK(mc_timer_view_get(timer, &view) && fb(view.partial_ticks) == UINT32_C(0x3f000000) &&
          fb(timer->partial_ticks) == UINT32_C(0x3f000000) &&
          fb(timer->remainder) == UINT32_C(0x3f000000));
    result = 0;
cleanup:
    mc_timer_free(timer);
    return result;
}

static int system_and_fp_order(void) {
    int result = 1;
    mc_timer *timer = NULL;
    mc_platform_nanotime clock = {0};
    mc_timer_construction_error error;
    mc_timer_constructor_state state;
    const unsigned saved = _mm_getcsr();
    _mm_setcsr(0x9fc0u);
    CHECK(mc_timer_allocate_raw(0, &timer, NULL) == MC_TIMER_CONSTRUCTION_OK);
    CHECK(mc_timer_construct_system(timer, 20.0f, &clock, &error) ==
          MC_TIMER_CONSTRUCTION_NANO_FAILED);
    CHECK((_mm_getcsr() & 0xffc0u) == 0x9fc0u);
    _mm_setcsr(saved);
    CHECK(error.py_pc == 32 && error.stage == MC_TIMER_CLOCK_NANO_INIT &&
          error.nano == MC_PLATFORM_NANOTIME_UNSUPPORTED_FP && !clock.initialized);
    CHECK(mc_timer_constructor_inspect(timer, &state) && state.phase == MC_TIMER_NATIVE_FAILED &&
          prefix(&state, 5, 2) && state.nano_init_calls == 1 && !state.nano_initialized_by_call);
    CHECK(defaults(timer, true) && state.fields.g == timer->last_wall_millis &&
          timer->last_high_millis == 0 && fb(state.fields.c) == fb(timer->partial_ticks) &&
          fb(state.fields.e) == fb(timer->remainder));
    CHECK(mc_timer_retire(&timer, NULL) == MC_TIMER_CONSTRUCTION_OK && timer == NULL);
    clock.initialized = true;
    clock.frequency = 0;
    _mm_setcsr(0x9fc0u);
    CHECK(mc_timer_allocate_raw(0, &timer, NULL) == MC_TIMER_CONSTRUCTION_OK);
    CHECK(mc_timer_construct_system(timer, 20.0f, &clock, &error) ==
          MC_TIMER_CONSTRUCTION_NANO_FAILED);
    _mm_setcsr(saved);
    CHECK(error.py_pc == 32 && error.stage == MC_TIMER_CLOCK_NANO_SAMPLE &&
          error.nano == MC_PLATFORM_NANOTIME_RANGE);
    CHECK(mc_timer_constructor_inspect(timer, &state) && prefix(&state, 5, 2) &&
          state.nano_init_calls == 0);
    CHECK(mc_timer_retire(&timer, NULL) == MC_TIMER_CONSTRUCTION_OK && timer == NULL);
    CHECK(mc_timer_allocate_raw(0, &timer, NULL) == MC_TIMER_CONSTRUCTION_OK);
    _mm_setcsr(0x9fc0u);
    CHECK(mc_timer_construct_system(timer, 20.0f, NULL, &error) ==
          MC_TIMER_CONSTRUCTION_NANO_FAILED);
    _mm_setcsr(saved);
    CHECK(error.py_pc == 32 && error.nano == MC_PLATFORM_NANOTIME_INVALID_ARGUMENT);
    CHECK(mc_timer_retire(&timer, NULL) == MC_TIMER_CONSTRUCTION_OK && timer == NULL);
    /* Observed raw longs have no platform conversion/FP requirement. */
    const mc_timer_observed_constructor_clocks observed = {.wall_millis = -7,
                                                           .nano_time = -1999999};
    CHECK(mc_timer_allocate_raw(0, &timer, NULL) == MC_TIMER_CONSTRUCTION_OK);
    _mm_setcsr(0x9fc0u);
    CHECK(mc_timer_construct_observed(timer, 20.0f, &observed, &error) == MC_TIMER_CONSTRUCTION_OK);
    CHECK((_mm_getcsr() & 0xffc0u) == 0x9fc0u);
    _mm_setcsr(saved);
    CHECK(mc_timer_constructor_inspect(timer, &state) && prefix(&state, 6, 2) &&
          timer->last_wall_millis == -7 && timer->last_high_millis == -1 && defaults(timer, true));
    CHECK(mc_timer_retire(&timer, NULL) == MC_TIMER_CONSTRUCTION_OK && timer == NULL);
    memset(&clock, 0, sizeof(clock));
    CHECK(mc_timer_allocate_raw(0, &timer, NULL) == MC_TIMER_CONSTRUCTION_OK);
    int64_t before, after;
    CHECK(mc_platform_current_time_millis(&before) == MC_PLATFORM_MILLIS_OK);
    CHECK(mc_timer_construct_system(timer, 20.0f, &clock, &error) == MC_TIMER_CONSTRUCTION_OK);
    CHECK(mc_platform_current_time_millis(&after) == MC_PLATFORM_MILLIS_OK);
    CHECK(mc_timer_constructor_inspect(timer, &state) && state.phase == MC_TIMER_NATIVE_READY &&
          state.origin == MC_TIMER_ORIGIN_SYSTEM && prefix(&state, 6, 2) && clock.initialized &&
          state.nano_init_calls == 1 && state.nano_initialized_by_call);
    CHECK(before > after || (state.fields.g >= before && state.fields.g <= after));
    CHECK(defaults(timer, true));
    unsigned char saved_clock[sizeof(clock)];
    memcpy(saved_clock, &clock, sizeof(clock));
    CHECK(mc_timer_retire(&timer, NULL) == MC_TIMER_CONSTRUCTION_OK && timer == NULL &&
          memcmp(saved_clock, &clock, sizeof(clock)) == 0);
    CHECK(mc_timer_allocate_raw(0, &timer, NULL) == MC_TIMER_CONSTRUCTION_OK);
    CHECK(mc_timer_construct_system(timer, 20.0f, &clock, NULL) == MC_TIMER_CONSTRUCTION_OK);
    CHECK(mc_timer_constructor_inspect(timer, &state) && prefix(&state, 6, 2) &&
          state.nano_init_calls == 0 && !state.nano_initialized_by_call);
    CHECK(memcmp(saved_clock, &clock, sizeof(clock)) == 0);
    CHECK(mc_timer_retire(&timer, NULL) == MC_TIMER_CONSTRUCTION_OK && timer == NULL);
    CHECK(mc_timer_allocate_raw(0, &timer, NULL) == MC_TIMER_CONSTRUCTION_OK);
    _mm_setcsr(0x9fc0u);
    CHECK(mc_timer_construct_system(timer, 20.0f, &clock, &error) ==
          MC_TIMER_CONSTRUCTION_NANO_FAILED);
    _mm_setcsr(saved);
    CHECK(error.py_pc == 32 && error.stage == MC_TIMER_CLOCK_NANO_SAMPLE &&
          error.nano == MC_PLATFORM_NANOTIME_UNSUPPORTED_FP);
    CHECK(mc_timer_constructor_inspect(timer, &state) && prefix(&state, 5, 2) &&
          state.nano_init_calls == 0 && memcmp(saved_clock, &clock, sizeof(clock)) == 0);
    result = 0;
cleanup:
    _mm_setcsr(saved);
    mc_timer_free(timer);
    return result;
}

int main(void) {
    if (raw_and_retirement() || failed_prefixes() || completed_and_legacy() ||
        c_before_first_update() || system_and_fp_order())
        return 1;
    printf("Timer constructor: %u authored checks; actual ordered fields/clocks and "
           "independent c/e\n",
           checks);
    return 0;
}
