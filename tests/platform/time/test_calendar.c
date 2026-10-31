#if defined(__linux__)
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#elif _POSIX_C_SOURCE < 200809L
#error "Calendar tests require POSIX.1-2008 declarations"
#endif
#endif

#include "calendar.h"

#include <stddef.h>
#include <stdio.h>

static size_t samples;
static int64_t observed[4];

/* Forward every selected sample to the real platform clock. This records
 * source-call multiplicity without selecting or injecting a timestamp. */
static mc_platform_millis_status record_real_sample(int64_t *out) {
    const mc_platform_millis_status status = mc_platform_current_time_millis(out);
    if (status == MC_PLATFORM_MILLIS_OK && samples < sizeof(observed) / sizeof(observed[0]))
        observed[samples] = *out;
    ++samples;
    return status;
}

#define mc_platform_current_time_millis record_real_sample
#include "../../../src/platform/time/calendar.c"
#undef mc_platform_current_time_millis

static unsigned int checks;
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "Calendar check%u line%d: %s\n", checks, __LINE__, #condition);        \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static bool same_state(mc_platform_calendar_state left, mc_platform_calendar_state right) {
    return left.initial_millis == right.initial_millis && left.millis == right.millis &&
           left.month == right.month && left.day_of_month == right.day_of_month &&
           left.date_installed == right.date_installed;
}

int main(void) {
    mc_platform_calendar_error error = {0};
    mc_platform_calendar *calendar = NULL;
    mc_platform_date date = {.millis = 12345, .initialized = false};
    CHECK(mc_platform_calendar_default_create(NULL, &error) == MC_PLATFORM_CALENDAR_ARGUMENT);
    CHECK(mc_platform_date_now(NULL, &error) == MC_PLATFORM_CALENDAR_ARGUMENT && samples == 0);
    CHECK(mc_platform_calendar_default_create(&calendar, &error) == MC_PLATFORM_CALENDAR_OK);
    CHECK(calendar != NULL && samples == 1 && error.sampled && error.sampled_millis == observed[0]);
    mc_platform_calendar_state first, installed, after;
    CHECK(mc_platform_calendar_inspect(calendar, &first));
    CHECK(first.initial_millis == observed[0] && first.millis == observed[0] &&
          !first.date_installed && first.month >= 0 && first.month <= 11 &&
          first.day_of_month >= 1 && first.day_of_month <= 31);
    CHECK(mc_platform_calendar_default_create(&calendar, &error) == MC_PLATFORM_CALENDAR_ARGUMENT &&
          samples == 1);
    CHECK(mc_platform_calendar_set_time(calendar, &date, &error) == MC_PLATFORM_CALENDAR_ARGUMENT);
    CHECK(mc_platform_calendar_inspect(calendar, &after) && same_state(first, after));

    CHECK(mc_platform_date_now(&date, &error) == MC_PLATFORM_CALENDAR_OK);
    CHECK(samples == 2 && date.initialized && date.millis == observed[1] && error.sampled &&
          error.sampled_millis == observed[1]);
    /* No monotonicity assertion: the real wall clock is allowed to move. */
    CHECK(mc_platform_calendar_set_time(calendar, &date, &error) == MC_PLATFORM_CALENDAR_OK);
    CHECK(mc_platform_calendar_inspect(calendar, &installed));
    CHECK(installed.initial_millis == observed[0] && installed.millis == observed[1] &&
          installed.date_installed && !error.sampled && samples == 2);
    for (unsigned int i = 0; i < 4; ++i) {
        int32_t month = -1, day = -1;
        CHECK(mc_platform_calendar_get(calendar, 2, &month, &error) == MC_PLATFORM_CALENDAR_OK &&
              month == installed.month);
        CHECK(mc_platform_calendar_get(calendar, 5, &day, &error) == MC_PLATFORM_CALENDAR_OK &&
              day == installed.day_of_month && samples == 2);
    }
    int32_t untouched = 12345;
    CHECK(mc_platform_calendar_get(calendar, 1, &untouched, &error) ==
              MC_PLATFORM_CALENDAR_ARGUMENT &&
          untouched == 12345 && samples == 2);
    const mc_platform_date outside = {.millis = INT64_MAX, .initialized = true};
    CHECK(mc_platform_calendar_set_time(calendar, &outside, &error) != MC_PLATFORM_CALENDAR_OK);
    CHECK(mc_platform_calendar_inspect(calendar, &after) && same_state(installed, after) &&
          samples == 2);
#ifndef _WIN32
    CHECK(admitted_locale("C") && admitted_locale("C.UTF-8") && admitted_locale("POSIX") &&
          admitted_locale("en_US.UTF-8"));
    CHECK(!admitted_locale("th_TH.UTF-8") && !admitted_locale("en_US@calendar=buddhist"));
#endif
    CHECK(mc_platform_calendar_retire(&calendar, &error) == MC_PLATFORM_CALENDAR_OK &&
          calendar == NULL && samples == 2);
    CHECK(mc_platform_calendar_retire(&calendar, &error) == MC_PLATFORM_CALENDAR_OK &&
          samples == 2);
    CHECK(mc_platform_calendar_retire(NULL, &error) == MC_PLATFORM_CALENDAR_ARGUMENT);
    printf("Calendar: %u checks; two real wall-clock samples\n", checks);
    return 0;
}
