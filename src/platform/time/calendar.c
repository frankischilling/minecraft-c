#if defined(__linux__)
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#elif _POSIX_C_SOURCE < 200809L
#error "Calendar requires POSIX.1-2008 declarations"
#endif
#endif

#include "calendar.h"

#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(__linux__)
#include <time.h>
_Static_assert(sizeof(time_t) == sizeof(int64_t), "Calendar requires signed 64-bit time_t");
_Static_assert((time_t)-1 < (time_t)0, "Calendar requires signed time_t");
#else
#error "Calendar supports only the selected Windows/Linux profile"
#endif
#if !defined(_M_X64) && !defined(_M_AMD64) && !defined(__x86_64__)
#error "Calendar supports only the selected x86-64 profile"
#endif

struct mc_platform_calendar {
    mc_platform_calendar_state state;
#ifdef _WIN32
    DYNAMIC_TIME_ZONE_INFORMATION zone;
#else
    char *tz;
    bool tz_present;
#endif
};

static mc_platform_calendar_status result(mc_platform_calendar_error *out,
                                          mc_platform_calendar_status status, uint32_t system_error,
                                          const char *operation) {
    if (out != NULL)
        *out = (mc_platform_calendar_error){
            .status = status, .system_error = system_error, .operation = operation};
    return status;
}

#ifndef _WIN32
static bool admitted_locale(const char *value) {
    if (value == NULL || value[0] == '\0')
        return true;
    if (strcmp(value, "C") == 0 || strcmp(value, "POSIX") == 0)
        return true;
    if (value[0] == 'C' && value[1] == '.')
        return strchr(value, '@') == NULL;
    return value[0] == 'e' && value[1] == 'n' &&
           (value[2] == '\0' || value[2] == '_' || value[2] == '.') && strchr(value, '@') == NULL;
}
#endif

static mc_platform_calendar_status select_defaults(mc_platform_calendar *calendar,
                                                   mc_platform_calendar_error *error) {
#ifdef _WIN32
    WCHAR locale[LOCALE_NAME_MAX_LENGTH];
    if (GetUserDefaultLocaleName(locale, LOCALE_NAME_MAX_LENGTH) == 0)
        return result(error, MC_PLATFORM_CALENDAR_LOCALE, GetLastError(), "default locale query");
    if (locale[0] != L'e' || locale[1] != L'n' || (locale[2] != L'-' && locale[2] != L'\0'))
        return result(error, MC_PLATFORM_CALENDAR_LOCALE, 0, "default Gregorian locale scope");
    /* This profile selects the native system zone. A process TZ override needs
     * its own evidenced mapping rather than an inferred Windows/JDK mapping. */
    const char *tz = getenv("TZ");
    if (tz != NULL && tz[0] != '\0')
        return result(error, MC_PLATFORM_CALENDAR_TIMEZONE, 0, "Windows TZ override unsupported");
    if (GetDynamicTimeZoneInformation(&calendar->zone) == TIME_ZONE_ID_INVALID)
        return result(error, MC_PLATFORM_CALENDAR_TIMEZONE, GetLastError(),
                      "default dynamic timezone");
#else
    /* Restrict every supplied locale category instead of guessing which
     * platform default-locale precedence a different JDK build uses. */
    static const char *const names[] = {"LC_ALL", "LC_TIME", "LC_CTYPE", "LC_MESSAGES", "LANG"};
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
        if (!admitted_locale(getenv(names[i])))
            return result(error, MC_PLATFORM_CALENDAR_LOCALE, 0, "default Gregorian locale scope");
    /* tzset selects the genuine process/system timezone; it does not sample
     * time. No concurrent process environment or timezone mutation is admitted. */
    tzset();
    const char *tz = getenv("TZ");
    calendar->tz_present = tz != NULL;
    if (tz != NULL) {
        const size_t length = strlen(tz);
        if (length >= 4096u)
            return result(error, MC_PLATFORM_CALENDAR_TIMEZONE, 0, "default TZ native bound");
        calendar->tz = malloc(length + 1u);
        if (calendar->tz == NULL)
            return result(error, MC_PLATFORM_CALENDAR_ALLOCATION, 0, "default TZ capture");
        memcpy(calendar->tz, tz, length + 1u);
    }
#endif
    return result(error, MC_PLATFORM_CALENDAR_OK, 0, NULL);
}

static mc_platform_calendar_status fields(const mc_platform_calendar *calendar, int64_t millis,
                                          int32_t *month, int32_t *day,
                                          mc_platform_calendar_error *error) {
#ifdef _WIN32
    const int64_t epoch = INT64_C(116444736000000000);
    if (millis < -INT64_C(11644473600000) || millis > (INT64_MAX - epoch) / INT64_C(10000))
        return result(error, MC_PLATFORM_CALENDAR_RANGE, 0, "Calendar FILETIME range");
    const uint64_t ticks = (uint64_t)(millis * INT64_C(10000) + epoch);
    const FILETIME file = {(DWORD)ticks, (DWORD)(ticks >> 32u)};
    SYSTEMTIME utc, local;
    if (!FileTimeToSystemTime(&file, &utc) ||
        !SystemTimeToTzSpecificLocalTimeEx(&calendar->zone, &utc, &local))
        return result(error, MC_PLATFORM_CALENDAR_TIMEZONE, GetLastError(),
                      "Calendar selected-zone conversion");
    if (local.wYear < 1601 || local.wYear > 9999 || local.wMonth < 1 || local.wMonth > 12 ||
        local.wDay < 1 || local.wDay > 31)
        return result(error, MC_PLATFORM_CALENDAR_RANGE, 0, "Calendar Gregorian range");
    *month = (int32_t)local.wMonth - 1;
    *day = (int32_t)local.wDay;
#else
    const char *tz = getenv("TZ");
    if ((tz != NULL) != calendar->tz_present || (tz != NULL && strcmp(tz, calendar->tz) != 0))
        return result(error, MC_PLATFORM_CALENDAR_ENVIRONMENT, 0, "Calendar TZ changed");
    /* Floor before conversion, including negative milliseconds. */
    int64_t seconds = millis / INT64_C(1000);
    if (millis % INT64_C(1000) < 0)
        --seconds;
    const time_t instant = (time_t)seconds;
    struct tm local;
    errno = 0;
    if (localtime_r(&instant, &local) == NULL)
        return result(error, MC_PLATFORM_CALENDAR_TIMEZONE, (uint32_t)errno,
                      "Calendar selected-zone conversion");
    if (local.tm_year < -299 || local.tm_year > 8099 || local.tm_mon < 0 || local.tm_mon > 11 ||
        local.tm_mday < 1 || local.tm_mday > 31)
        return result(error, MC_PLATFORM_CALENDAR_RANGE, 0, "Calendar Gregorian range");
    *month = (int32_t)local.tm_mon;
    *day = (int32_t)local.tm_mday;
#endif
    return result(error, MC_PLATFORM_CALENDAR_OK, 0, NULL);
}

static void dispose(mc_platform_calendar *calendar) {
#ifndef _WIN32
    free(calendar->tz);
#endif
    free(calendar);
}

mc_platform_calendar_status mc_platform_calendar_default_create(mc_platform_calendar **out,
                                                                mc_platform_calendar_error *error) {
    if (out == NULL || *out != NULL)
        return result(error, MC_PLATFORM_CALENDAR_ARGUMENT, 0, "Calendar output slot");
    mc_platform_calendar *calendar = calloc(1, sizeof(*calendar));
    if (calendar == NULL)
        return result(error, MC_PLATFORM_CALENDAR_ALLOCATION, 0, "Calendar native allocation");
    mc_platform_calendar_error cause = {0};
    mc_platform_calendar_status status = select_defaults(calendar, &cause);
    if (status == MC_PLATFORM_CALENDAR_OK) {
        const mc_platform_millis_status clock =
            mc_platform_current_time_millis(&calendar->state.initial_millis);
        if (clock != MC_PLATFORM_MILLIS_OK) {
            result(&cause, MC_PLATFORM_CALENDAR_CLOCK, 0, "Calendar initial wall clock");
            cause.clock = clock;
            status = cause.status;
        } else {
            calendar->state.millis = calendar->state.initial_millis;
            status = fields(calendar, calendar->state.millis, &calendar->state.month,
                            &calendar->state.day_of_month, &cause);
            cause.sampled = true;
            cause.sampled_millis = calendar->state.initial_millis;
        }
    }
    if (status == MC_PLATFORM_CALENDAR_OK)
        *out = calendar;
    else
        dispose(calendar);
    if (error != NULL)
        *error = cause;
    return status;
}

mc_platform_calendar_status mc_platform_date_now(mc_platform_date *out,
                                                 mc_platform_calendar_error *error) {
    if (out == NULL)
        return result(error, MC_PLATFORM_CALENDAR_ARGUMENT, 0, "Date output");
    int64_t millis;
    const mc_platform_millis_status clock = mc_platform_current_time_millis(&millis);
    if (clock != MC_PLATFORM_MILLIS_OK) {
        result(error, MC_PLATFORM_CALENDAR_CLOCK, 0, "Date wall clock");
        if (error != NULL)
            error->clock = clock;
        return MC_PLATFORM_CALENDAR_CLOCK;
    }
    *out = (mc_platform_date){.millis = millis, .initialized = true};
    result(error, MC_PLATFORM_CALENDAR_OK, 0, NULL);
    if (error != NULL) {
        error->sampled = true;
        error->sampled_millis = millis;
    }
    return MC_PLATFORM_CALENDAR_OK;
}

mc_platform_calendar_status mc_platform_calendar_set_time(mc_platform_calendar *calendar,
                                                          const mc_platform_date *date,
                                                          mc_platform_calendar_error *error) {
    if (calendar == NULL || date == NULL || !date->initialized)
        return result(error, MC_PLATFORM_CALENDAR_ARGUMENT, 0, "Calendar Date argument");
    int32_t month, day;
    const mc_platform_calendar_status status = fields(calendar, date->millis, &month, &day, error);
    if (status != MC_PLATFORM_CALENDAR_OK)
        return status;
    calendar->state.millis = date->millis;
    calendar->state.month = month;
    calendar->state.day_of_month = day;
    calendar->state.date_installed = true;
    return MC_PLATFORM_CALENDAR_OK;
}

mc_platform_calendar_status mc_platform_calendar_get(const mc_platform_calendar *calendar,
                                                     int32_t field, int32_t *out,
                                                     mc_platform_calendar_error *error) {
    if (calendar == NULL || out == NULL || (field != 2 && field != 5))
        return result(error, MC_PLATFORM_CALENDAR_ARGUMENT, 0, "Calendar reached field");
    *out = field == 2 ? calendar->state.month : calendar->state.day_of_month;
    return result(error, MC_PLATFORM_CALENDAR_OK, 0, NULL);
}

bool mc_platform_calendar_inspect(const mc_platform_calendar *calendar,
                                  mc_platform_calendar_state *out) {
    if (calendar == NULL || out == NULL)
        return false;
    *out = calendar->state;
    return true;
}

mc_platform_calendar_status mc_platform_calendar_retire(mc_platform_calendar **slot,
                                                        mc_platform_calendar_error *error) {
    if (slot == NULL)
        return result(error, MC_PLATFORM_CALENDAR_ARGUMENT, 0, "Calendar retirement slot");
    if (*slot != NULL) {
        dispose(*slot);
        *slot = NULL;
    }
    return result(error, MC_PLATFORM_CALENDAR_OK, 0, NULL);
}
