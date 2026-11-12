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
#include <unistd.h>
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
    mc_platform_calendar_zone date_format_zone;
#ifdef _WIN32
    DYNAMIC_TIME_ZONE_INFORMATION zone;
#else
    char *tz;
    bool tz_present;
#endif
};

#ifndef _WIN32
static mc_platform_calendar_zone date_format_zone(const char *supplied) {
    char selected[4096];
    const char *name = supplied;
    if (name == NULL) {
        const ssize_t size = readlink("/etc/localtime", selected, sizeof(selected) - 1u);
        if (size < 0 || (size_t)size >= sizeof(selected) - 1u)
            return MC_PLATFORM_CALENDAR_ZONE_UNSUPPORTED;
        selected[size] = '\0';
        name = selected;
    }
    if (name[0] == ':')
        ++name;
    static const char *const prefixes[] = {"/usr/share/zoneinfo/", "/usr/lib/zoneinfo/",
                                           "/usr/share/lib/zoneinfo/"};
    for (size_t i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); ++i) {
        const size_t length = strlen(prefixes[i]);
        if (strncmp(name, prefixes[i], length) == 0) {
            name += length;
            break;
        }
    }
    if (strcmp(name, "America/New_York") == 0 || strcmp(name, "US/Eastern") == 0)
        return MC_PLATFORM_CALENDAR_ZONE_US_EASTERN;
    if (strcmp(name, "UTC") == 0 || strcmp(name, "Etc/UTC") == 0 || strcmp(name, "Etc/GMT") == 0 ||
        strcmp(name, "GMT") == 0 || strcmp(name, "UTC0") == 0 || strcmp(name, "GMT0") == 0)
        return MC_PLATFORM_CALENDAR_ZONE_UTC;
    return MC_PLATFORM_CALENDAR_ZONE_UNSUPPORTED;
}
#endif

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
    if (wcscmp(calendar->zone.TimeZoneKeyName, L"UTC") == 0 && calendar->zone.Bias == 0 &&
        calendar->zone.StandardBias == 0 && calendar->zone.DaylightBias == 0)
        calendar->date_format_zone = MC_PLATFORM_CALENDAR_ZONE_UTC;
    else if (wcscmp(calendar->zone.TimeZoneKeyName, L"Eastern Standard Time") == 0 &&
             calendar->zone.Bias == 300 && calendar->zone.StandardBias == 0 &&
             calendar->zone.DaylightBias == -60 && !calendar->zone.DynamicDaylightTimeDisabled)
        calendar->date_format_zone = MC_PLATFORM_CALENDAR_ZONE_US_EASTERN;
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
    calendar->date_format_zone = date_format_zone(tz);
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

mc_platform_calendar_zone mc_platform_calendar_date_format_zone(const mc_platform_calendar *c) {
    return c == NULL ? MC_PLATFORM_CALENDAR_ZONE_UNSUPPORTED : c->date_format_zone;
}

mc_platform_calendar_status
mc_platform_calendar_date_time_fields(mc_platform_calendar *c, int64_t millis,
                                      mc_platform_calendar_date_time *out,
                                      mc_platform_calendar_error *error) {
    if (c == NULL || out == NULL)
        return result(error, MC_PLATFORM_CALENDAR_ARGUMENT, 0, "DateFormat Calendar fields output");
    if (c->date_format_zone == MC_PLATFORM_CALENDAR_ZONE_UNSUPPORTED)
        return result(error, MC_PLATFORM_CALENDAR_TIMEZONE, 0, "DateFormat captured zone profile");
    mc_platform_calendar_date_time value;
#ifdef _WIN32
    const int64_t epoch = INT64_C(116444736000000000);
    if (millis < -INT64_C(11644473600000) || millis > (INT64_MAX - epoch) / INT64_C(10000))
        return result(error, MC_PLATFORM_CALENDAR_RANGE, 0, "DateFormat FILETIME range");
    const uint64_t ticks = (uint64_t)(millis * INT64_C(10000) + epoch);
    const FILETIME file = {(DWORD)ticks, (DWORD)(ticks >> 32u)};
    SYSTEMTIME utc, local;
    if (!FileTimeToSystemTime(&file, &utc))
        return result(error, MC_PLATFORM_CALENDAR_RANGE, GetLastError(), "DateFormat UTC fields");
    if (utc.wYear < 1967 || utc.wYear > 2099)
        return result(error, MC_PLATFORM_CALENDAR_RANGE, 0, "DateFormat UTC year profile");
    if (c->date_format_zone == MC_PLATFORM_CALENDAR_ZONE_US_EASTERN) {
        /* Selected JDK17 America/New_York transition observations,1967..2099.
         * Windows otherwise extrapolates its first stored dynamic rule into
         * earlier years, changing pre2007 DST. The actual native captured zone
         * identity and biases are admitted before this selected rule closure. */
        TIME_ZONE_INFORMATION zone = {0};
        zone.Bias = c->zone.Bias;
        zone.StandardBias = c->zone.StandardBias;
        zone.DaylightBias = c->zone.DaylightBias;
        zone.DaylightDate.wDayOfWeek = 0;
        zone.DaylightDate.wHour = 2;
        zone.StandardDate.wDayOfWeek = 0;
        zone.StandardDate.wHour = 2;
        if (utc.wYear >= 2007) {
            zone.DaylightDate.wMonth = 3;
            zone.DaylightDate.wDay = 2;
            zone.StandardDate.wMonth = 11;
            zone.StandardDate.wDay = 1;
        } else {
            zone.DaylightDate.wMonth = 4;
            zone.DaylightDate.wDay = utc.wYear >= 1987 ? 1 : 5;
            zone.StandardDate.wMonth = 10;
            zone.StandardDate.wDay = 5;
            if (utc.wYear == 1974) {
                zone.DaylightDate.wMonth = 1;
                zone.DaylightDate.wDay = 1;
            } else if (utc.wYear == 1975) {
                zone.DaylightDate.wMonth = 2;
                zone.DaylightDate.wDay = 4;
            }
        }
        if (!SystemTimeToTzSpecificLocalTime(&zone, &utc, &local))
            return result(error, MC_PLATFORM_CALENDAR_TIMEZONE, GetLastError(),
                          "DateFormat selected Eastern conversion");
    } else
        local = utc;
    value = (mc_platform_calendar_date_time){(int32_t)local.wYear,   (int32_t)local.wMonth,
                                             (int32_t)local.wDay,    (int32_t)local.wHour,
                                             (int32_t)local.wMinute, (int32_t)local.wSecond};
#else
    const char *tz = getenv("TZ");
    if ((tz != NULL) != c->tz_present || (tz != NULL && strcmp(tz, c->tz) != 0))
        return result(error, MC_PLATFORM_CALENDAR_ENVIRONMENT, 0, "DateFormat captured TZ changed");
    int64_t seconds = millis / INT64_C(1000);
    if (millis % INT64_C(1000) < 0)
        --seconds;
    const time_t instant = (time_t)seconds;
    struct tm utc, local;
    errno = 0;
    if (gmtime_r(&instant, &utc) == NULL)
        return result(error, MC_PLATFORM_CALENDAR_RANGE, (uint32_t)errno, "DateFormat UTC fields");
    if (utc.tm_year < 67 || utc.tm_year > 199)
        return result(error, MC_PLATFORM_CALENDAR_RANGE, 0, "DateFormat UTC year profile");
    if (localtime_r(&instant, &local) == NULL)
        return result(error, MC_PLATFORM_CALENDAR_TIMEZONE, (uint32_t)errno,
                      "DateFormat captured-zone conversion");
    value = (mc_platform_calendar_date_time){local.tm_year + 1900, local.tm_mon + 1, local.tm_mday,
                                             local.tm_hour,        local.tm_min,     local.tm_sec};
#endif
    *out = value;
    c->state.millis = millis;
    c->state.month = value.month - 1;
    c->state.day_of_month = value.day;
    c->state.date_installed = true;
    return result(error, MC_PLATFORM_CALENDAR_OK, 0, NULL);
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
