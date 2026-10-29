#ifndef MC_PLATFORM_TIME_CALENDAR_H
#define MC_PLATFORM_TIME_CALENDAR_H

#include "millis.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct mc_platform_calendar mc_platform_calendar;

typedef struct mc_platform_date {
    int64_t millis;
    bool initialized;
} mc_platform_date;

typedef enum mc_platform_calendar_status {
    MC_PLATFORM_CALENDAR_OK = 0,
    MC_PLATFORM_CALENDAR_ARGUMENT,
    MC_PLATFORM_CALENDAR_ALLOCATION,
    MC_PLATFORM_CALENDAR_LOCALE,
    MC_PLATFORM_CALENDAR_TIMEZONE,
    MC_PLATFORM_CALENDAR_ENVIRONMENT,
    MC_PLATFORM_CALENDAR_CLOCK,
    MC_PLATFORM_CALENDAR_RANGE
} mc_platform_calendar_status;

typedef struct mc_platform_calendar_error {
    mc_platform_calendar_status status;
    mc_platform_millis_status clock;
    int64_t sampled_millis;
    bool sampled;
    uint32_t system_error;
    const char *operation;
} mc_platform_calendar_error;

typedef struct mc_platform_calendar_state {
    int64_t initial_millis, millis;
    int32_t month, day_of_month;
    bool date_installed;
} mc_platform_calendar_state;

/* Private native profile for the reached default Calendar/Date calls. Each
 * Calendar owns its selected timezone and cached Gregorian fields. Date is a
 * distinct caller-owned value whose address represents that source occurrence.
 * No JVM, injected date, clock fallback or application callback is used.
 *
 * The initial locale scope is Windows English and Linux C/POSIX/English.
 * Unsupported locale/calendar, timezone and date ranges are explicit native
 * refusals, not source exceptions. The native Gregorian range is 1601..9999;
 * this does not implement the Julian/Gregorian cutover or other calendars.
 * Native timezone rules are not claimed equal to every JDK tzdata release.
 *
 * All calls are exclusive. Locale/TZ environment and system timezone files or
 * registry rules must stay stable throughout the Calendar lifetime. Linux
 * uses the selected libc timezone under that contract; a changed TZ string is
 * additionally rejected by set_time. Windows retains the captured dynamic zone.
 * Caller slots, values, errors and views must be valid and disjoint. */

/* Allocate native call storage, admit the default locale, select the actual
 * default timezone, then take exactly one real wall-clock sample and compute its
 * fields. *out must be NULL. Failure leaves *out unchanged and records whether
 * the sample completed. Native allocation timing does not claim JVM allocation
 * parity. This is the getInstance call, not a Date constructor. */
mc_platform_calendar_status mc_platform_calendar_default_create(mc_platform_calendar **out,
                                                                mc_platform_calendar_error *error);

/* Date's default constructor takes its own fresh wall-clock sample. Success
 * initializes all semantic output fields; failure preserves the entire value. */
mc_platform_calendar_status mc_platform_date_now(mc_platform_date *out,
                                                 mc_platform_calendar_error *error);

/* Uses the supplied Date's stored millis with this Calendar's selected zone.
 * It takes no clock sample. Failure preserves all prior Calendar fields. */
mc_platform_calendar_status mc_platform_calendar_set_time(mc_platform_calendar *calendar,
                                                          const mc_platform_date *date,
                                                          mc_platform_calendar_error *error);

/* Reached fields only: 2=zero-based MONTH, 5=DAY_OF_MONTH. No clock, timezone,
 * environment query or allocation occurs. Unsupported fields preserve out. */
mc_platform_calendar_status mc_platform_calendar_get(const mc_platform_calendar *calendar,
                                                     int32_t field, int32_t *out,
                                                     mc_platform_calendar_error *error);
bool mc_platform_calendar_inspect(const mc_platform_calendar *calendar,
                                  mc_platform_calendar_state *out);

/* Native cleanup only. NULL *slot succeeds. No source close/finalizer exists. */
mc_platform_calendar_status mc_platform_calendar_retire(mc_platform_calendar **slot,
                                                        mc_platform_calendar_error *error);

#endif
