#ifndef MC_PLATFORM_TIME_DATE_FORMAT_H
#define MC_PLATFORM_TIME_DATE_FORMAT_H

#include "../text/locale.h"
#include "calendar.h"

#include <stddef.h>

typedef struct mc_platform_date_format mc_platform_date_format;

typedef enum mc_platform_date_format_status {
    MC_PLATFORM_DATE_FORMAT_OK,
    MC_PLATFORM_DATE_FORMAT_ARGUMENT,
    MC_PLATFORM_DATE_FORMAT_ALLOCATION,
    MC_PLATFORM_DATE_FORMAT_LOCALE,
    MC_PLATFORM_DATE_FORMAT_TIMEZONE,
    MC_PLATFORM_DATE_FORMAT_ENVIRONMENT,
    MC_PLATFORM_DATE_FORMAT_CLOCK,
    MC_PLATFORM_DATE_FORMAT_RANGE
} mc_platform_date_format_status;

typedef struct mc_platform_date_format_error {
    mc_platform_date_format_status status;
    const char *operation;
    mc_text_locale_status locale;
    mc_platform_calendar_error calendar;
} mc_platform_date_format_error;

typedef struct mc_platform_date_format_text {
    uint16_t units[64]; /* Owned UTF16, with a trailing zero outside length. */
    size_t length;
} mc_platform_date_format_text;

typedef struct mc_platform_date_format_state {
    mc_text_number_locale locale;
    mc_platform_calendar_zone zone;
    int64_t initial_millis, last_millis;
    bool formatted;
} mc_platform_date_format_state;

/* Native owner for selected Temurin17 SimpleDateFormat() SHORT/SHORT. Capture
 * the actual FORMAT locale and default timezone during construction, including
 * an empty-save screen. English/en_US use the observed M/d/yy, h:mm a pattern.
 * The admitted timezone profiles are UTC and US Eastern, UTC years1967..2099.
 * Other locales/zones/ranges stop explicitly. No JVM, JDK file, strftime,
 * process locale mutation, guessed UTC fallback or caller-supplied date profile.
 *
 * Calls require exclusive use. Calendar's stable environment/timezone contract
 * applies: Linux detects changed TZ and refuses it. Locale changes after create
 * do not replace this owner's captured pattern. Native allocation timing and
 * constructor clock multiplicity do not claim JVM allocation/clock parity.
 * Output/error storage is live and disjoint from owner and each other. */
mc_platform_date_format_status
mc_platform_date_format_default_create(mc_platform_date_format **slot,
                                       mc_platform_date_format_error *error);

/* Format the supplied Date(long) value. No wall-clock sample or allocation.
 * The UTF16 result is caller-owned; copying it into an immutable String is the
 * enclosing source caller's operation. Failure preserves output and last date.
 * Successful formatting installs the supplied millis in the owned Calendar. */
mc_platform_date_format_status mc_platform_date_format_format(mc_platform_date_format *,
                                                              int64_t millis,
                                                              mc_platform_date_format_text *,
                                                              mc_platform_date_format_error *);
bool mc_platform_date_format_inspect(const mc_platform_date_format *,
                                     mc_platform_date_format_state *);
mc_platform_date_format_status mc_platform_date_format_retire(mc_platform_date_format **slot,
                                                              mc_platform_date_format_error *);

#endif
