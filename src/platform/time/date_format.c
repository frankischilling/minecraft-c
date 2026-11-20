#include "date_format.h"

#include <stdlib.h>

struct mc_platform_date_format {
    mc_platform_calendar *calendar;
    mc_platform_date_format_state state;
};

static mc_platform_date_format_status format_result(mc_platform_date_format_error *out,
                                                    mc_platform_date_format_status status,
                                                    const char *operation) {
    if (out != NULL)
        *out = (mc_platform_date_format_error){.status = status, .operation = operation};
    return status;
}

static mc_platform_date_format_status calendar_result(mc_platform_date_format_error *out,
                                                      mc_platform_calendar_error cause) {
    mc_platform_date_format_status status;
    switch (cause.status) {
        case MC_PLATFORM_CALENDAR_OK:
            status = MC_PLATFORM_DATE_FORMAT_OK;
            break;
        case MC_PLATFORM_CALENDAR_ALLOCATION:
            status = MC_PLATFORM_DATE_FORMAT_ALLOCATION;
            break;
        case MC_PLATFORM_CALENDAR_LOCALE:
            status = MC_PLATFORM_DATE_FORMAT_LOCALE;
            break;
        case MC_PLATFORM_CALENDAR_TIMEZONE:
            status = MC_PLATFORM_DATE_FORMAT_TIMEZONE;
            break;
        case MC_PLATFORM_CALENDAR_ENVIRONMENT:
            status = MC_PLATFORM_DATE_FORMAT_ENVIRONMENT;
            break;
        case MC_PLATFORM_CALENDAR_CLOCK:
            status = MC_PLATFORM_DATE_FORMAT_CLOCK;
            break;
        case MC_PLATFORM_CALENDAR_RANGE:
            status = MC_PLATFORM_DATE_FORMAT_RANGE;
            break;
        default:
            status = MC_PLATFORM_DATE_FORMAT_ARGUMENT;
            break;
    }
    format_result(out, status, cause.operation);
    if (out != NULL)
        out->calendar = cause;
    return status;
}

mc_platform_date_format_status
mc_platform_date_format_default_create(mc_platform_date_format **slot,
                                       mc_platform_date_format_error *error) {
    if (slot == NULL || *slot != NULL)
        return format_result(error, MC_PLATFORM_DATE_FORMAT_ARGUMENT, "DateFormat output slot");
    mc_platform_date_format *owner = calloc(1, sizeof(*owner));
    if (owner == NULL)
        return format_result(error, MC_PLATFORM_DATE_FORMAT_ALLOCATION, "DateFormat native owner");
    mc_text_locale_status locale = mc_text_number_locale_query(&owner->state.locale);
    if (locale != MC_TEXT_LOCALE_OK || owner->state.locale == MC_TEXT_NUMBER_LOCALE_UNKNOWN) {
        free(owner);
        const mc_platform_date_format_status status = locale == MC_TEXT_LOCALE_ALLOCATION_FAILED
                                                          ? MC_PLATFORM_DATE_FORMAT_ALLOCATION
                                                          : MC_PLATFORM_DATE_FORMAT_LOCALE;
        format_result(error, status, "DateFormat actual FORMAT locale profile");
        if (error != NULL)
            error->locale = locale;
        return status;
    }
    mc_platform_calendar_error cause = {0};
    if (mc_platform_calendar_default_create(&owner->calendar, &cause) != MC_PLATFORM_CALENDAR_OK) {
        free(owner);
        return calendar_result(error, cause);
    }
    owner->state.zone = mc_platform_calendar_date_format_zone(owner->calendar);
    if (owner->state.zone == MC_PLATFORM_CALENDAR_ZONE_UNSUPPORTED) {
        mc_platform_calendar_retire(&owner->calendar, NULL);
        free(owner);
        format_result(error, MC_PLATFORM_DATE_FORMAT_TIMEZONE,
                      "DateFormat default timezone profile");
        if (error != NULL)
            error->calendar = cause;
        return MC_PLATFORM_DATE_FORMAT_TIMEZONE;
    }
    mc_platform_calendar_state calendar;
    (void)mc_platform_calendar_inspect(owner->calendar, &calendar);
    owner->state.initial_millis = calendar.initial_millis;
    owner->state.last_millis = calendar.millis;
    *slot = owner;
    format_result(error, MC_PLATFORM_DATE_FORMAT_OK, NULL);
    if (error != NULL)
        error->calendar = cause;
    return MC_PLATFORM_DATE_FORMAT_OK;
}

static void character(mc_platform_date_format_text *out, uint16_t unit) {
    out->units[out->length++] = unit;
}

static void number(mc_platform_date_format_text *out, int32_t value, bool padded) {
    if (padded || value >= 10)
        character(out, (uint16_t)('0' + value / 10));
    character(out, (uint16_t)('0' + value % 10));
}

mc_platform_date_format_status
mc_platform_date_format_format(mc_platform_date_format *owner, int64_t millis,
                               mc_platform_date_format_text *out,
                               mc_platform_date_format_error *error) {
    if (owner == NULL || out == NULL)
        return format_result(error, MC_PLATFORM_DATE_FORMAT_ARGUMENT,
                             "DateFormat format arguments");
    mc_platform_calendar_date_time date;
    mc_platform_calendar_error cause;
    if (mc_platform_calendar_date_time_fields(owner->calendar, millis, &date, &cause) !=
        MC_PLATFORM_CALENDAR_OK)
        return calendar_result(error, cause);
    mc_platform_date_format_text text = {0};
    number(&text, date.month, false);
    character(&text, '/');
    number(&text, date.day, false);
    character(&text, '/');
    number(&text, date.year % 100, true);
    character(&text, ',');
    character(&text, ' ');
    int32_t hour = date.hour % 12;
    if (hour == 0)
        hour = 12;
    number(&text, hour, false);
    character(&text, ':');
    number(&text, date.minute, true);
    character(&text, ' ');
    character(&text, date.hour >= 12 ? 'P' : 'A');
    character(&text, 'M');
    *out = text;
    owner->state.last_millis = millis;
    owner->state.formatted = true;
    return format_result(error, MC_PLATFORM_DATE_FORMAT_OK, NULL);
}

bool mc_platform_date_format_inspect(const mc_platform_date_format *owner,
                                     mc_platform_date_format_state *out) {
    if (owner == NULL || out == NULL)
        return false;
    *out = owner->state;
    return true;
}

mc_platform_date_format_status
mc_platform_date_format_retire(mc_platform_date_format **slot,
                               mc_platform_date_format_error *error) {
    if (slot == NULL)
        return format_result(error, MC_PLATFORM_DATE_FORMAT_ARGUMENT, "DateFormat retirement slot");
    if (*slot != NULL) {
        mc_platform_calendar_retire(&(*slot)->calendar, NULL);
        free(*slot);
        *slot = NULL;
    }
    return format_result(error, MC_PLATFORM_DATE_FORMAT_OK, NULL);
}
