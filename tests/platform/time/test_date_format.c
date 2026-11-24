#if defined(__linux__) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "date_format.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t samples;

static mc_platform_millis_status record_sample(int64_t *out) {
    ++samples;
    return mc_platform_current_time_millis(out);
}

/* Count genuine native samples. No timestamp/profile is injected. This also
 * proves formatting does not call the otherwise live wall-clock dependency. */
#define mc_platform_current_time_millis record_sample
#include "../../../src/platform/time/calendar.c"
#undef mc_platform_current_time_millis

static unsigned int checks;
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "DateFormat check%u line%d: %s\n", checks, __LINE__, #condition);      \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static bool decode(const char *input, mc_platform_date_format_text *out) {
    mc_platform_date_format_text text = {0};
    for (;;) {
        if (text.length >= sizeof(text.units) / sizeof(text.units[0]) - 1u)
            return false;
        uint16_t value = 0;
        for (size_t i = 0; i < 4; ++i) {
            unsigned char c = (unsigned char)*input++;
            unsigned int digit;
            if (c >= '0' && c <= '9')
                digit = c - '0';
            else if (c >= 'a' && c <= 'f')
                digit = c - 'a' + 10u;
            else
                return false;
            value = (uint16_t)((unsigned int)value * 16u + digit);
        }
        text.units[text.length++] = value;
        if (*input == '\0' || *input == '\n' || *input == '\r')
            break;
        if (*input++ != ',')
            return false;
    }
    *out = text;
    return true;
}

static bool equal_text(mc_platform_date_format_text a, mc_platform_date_format_text b) {
    return a.length == b.length && a.units[a.length] == 0 && b.units[b.length] == 0 &&
           memcmp(a.units, b.units, a.length * sizeof(a.units[0])) == 0;
}

static bool unchanged(mc_platform_date_format_state a, mc_platform_date_format_state b) {
    return a.locale == b.locale && a.zone == b.zone && a.initial_millis == b.initial_millis &&
           a.last_millis == b.last_millis && a.formatted == b.formatted;
}

static int observe(int argc, char **argv) {
    mc_platform_date_format *format = NULL;
    mc_platform_date_format_error error;
    CHECK(mc_platform_date_format_default_create(&format, &error) == MC_PLATFORM_DATE_FORMAT_OK);
    mc_platform_date_format_state state;
    CHECK(mc_platform_date_format_inspect(format, &state));
    printf("locale=%s\ntimezone=%s\npattern=004d,002f,0064,002f,0079,0079,002c,0020,0068,003a,006d,"
           "006d,0020,0061\n",
           state.locale == MC_TEXT_NUMBER_LOCALE_EN_US ? "en-US" : "en",
           state.zone == MC_PLATFORM_CALENDAR_ZONE_UTC ? "UTC" : "America/New_York");
    for (int i = 2; i < argc; ++i) {
        char *end;
        errno = 0;
        const int64_t millis = strtoll(argv[i], &end, 10);
        CHECK(errno == 0 && end != argv[i] && *end == '\0');
        mc_platform_date_format_text text;
        CHECK(mc_platform_date_format_format(format, millis, &text, &error) ==
              MC_PLATFORM_DATE_FORMAT_OK);
        printf("%" PRId64 "=", millis);
        for (size_t j = 0; j < text.length; ++j)
            printf(j == 0 ? "%04x" : ",%04x", (unsigned int)text.units[j]);
        putchar('\n');
    }
    CHECK(samples == 1);
    CHECK(mc_platform_date_format_retire(&format, &error) == MC_PLATFORM_DATE_FORMAT_OK);
    return 0;
}

int main(int argc, char **argv) {
    if (argc > 2 && strcmp(argv[1], "--observe") == 0)
        return observe(argc, argv);
    if (argc == 2 &&
        (strcmp(argv[1], "--reject-locale") == 0 || strcmp(argv[1], "--reject-zone") == 0)) {
        mc_platform_date_format *rejected = NULL;
        mc_platform_date_format_error cause;
        const mc_platform_date_format_status expected = strcmp(argv[1], "--reject-locale") == 0
                                                            ? MC_PLATFORM_DATE_FORMAT_LOCALE
                                                            : MC_PLATFORM_DATE_FORMAT_TIMEZONE;
        CHECK(mc_platform_date_format_default_create(&rejected, &cause) == expected &&
              rejected == NULL && cause.status == expected);
        CHECK(expected != MC_PLATFORM_DATE_FORMAT_LOCALE || samples == 0);
        CHECK(samples <= 1);
        CHECK(mc_platform_date_format_retire(&rejected, &cause) == MC_PLATFORM_DATE_FORMAT_OK);
        puts("DateFormat: unsupported constructor profile refused; no formatter published");
        return 0;
    }
    if (argc != 2)
        return 2;
    mc_platform_date_format *format = NULL;
    mc_platform_date_format_error error;
    CHECK(mc_platform_date_format_default_create(NULL, &error) ==
              MC_PLATFORM_DATE_FORMAT_ARGUMENT &&
          samples == 0);
    CHECK(mc_platform_date_format_default_create(&format, &error) == MC_PLATFORM_DATE_FORMAT_OK &&
          format != NULL && samples == 1 && error.calendar.sampled);
    mc_platform_date_format_state before, after;
    CHECK(mc_platform_date_format_inspect(format, &before) && !before.formatted &&
          before.initial_millis == before.last_millis &&
          before.initial_millis == error.calendar.sampled_millis);
    CHECK(mc_platform_date_format_default_create(&format, &error) ==
              MC_PLATFORM_DATE_FORMAT_ARGUMENT &&
          samples == 1);
    FILE *file = fopen(argv[1], "rb");
    CHECK(file != NULL);
    char line[512];
    size_t cases = 0;
    while (fgets(line, (int)sizeof(line), file) != NULL) {
        if (line[0] == '#')
            continue;
        char *end;
        errno = 0;
        const int64_t millis = strtoll(line, &end, 10);
        CHECK(errno == 0 && end != line && *end++ == '\t');
        char *utc = strchr(end, '\t');
        CHECK(utc != NULL);
        *utc++ = '\0';
        mc_platform_date_format_text expected, actual;
        CHECK(decode(before.zone == MC_PLATFORM_CALENDAR_ZONE_UTC ? utc : end, &expected));
        CHECK(mc_platform_date_format_format(format, millis, &actual, &error) ==
              MC_PLATFORM_DATE_FORMAT_OK);
        if (!equal_text(expected, actual)) {
            fprintf(stderr, "DateFormat vector %" PRId64 " mismatch\n", millis);
            fclose(file);
            return 1;
        }
        CHECK(mc_platform_date_format_inspect(format, &after) && after.formatted &&
              after.last_millis == millis && after.initial_millis == before.initial_millis);
        ++cases;
    }
    CHECK(ferror(file) == 0 && fclose(file) == 0 && cases >= 2900 && samples == 1);
    CHECK(mc_platform_date_format_inspect(format, &before));
    mc_platform_date_format_text sentinel, output;
    memset(&sentinel, 0xa5, sizeof(sentinel));
    output = sentinel;
    CHECK(mc_platform_date_format_format(NULL, 0, &output, &error) ==
              MC_PLATFORM_DATE_FORMAT_ARGUMENT &&
          memcmp(&sentinel, &output, sizeof(output)) == 0);
    CHECK(mc_platform_date_format_format(format, 0, NULL, &error) ==
          MC_PLATFORM_DATE_FORMAT_ARGUMENT);
    static const int64_t rejected[] = {INT64_MIN, INT64_MAX, -94694400001LL, 4102444800000LL};
    for (size_t i = 0; i < sizeof(rejected) / sizeof(rejected[0]); ++i) {
        CHECK(mc_platform_date_format_format(format, rejected[i], &output, &error) ==
                  MC_PLATFORM_DATE_FORMAT_RANGE &&
              memcmp(&sentinel, &output, sizeof(output)) == 0);
        CHECK(mc_platform_date_format_inspect(format, &after) && unchanged(before, after));
    }
#ifndef _WIN32
    const char *present = getenv("TZ");
    const bool tz_present = present != NULL;
    char saved[4096];
    CHECK(present == NULL || strlen(present) < sizeof(saved));
    if (present != NULL)
        strcpy(saved, present);
    CHECK(setenv("TZ", before.zone == MC_PLATFORM_CALENDAR_ZONE_UTC ? "America/New_York" : "UTC",
                 1) == 0);
    CHECK(mc_platform_date_format_format(format, 0, &output, &error) ==
              MC_PLATFORM_DATE_FORMAT_ENVIRONMENT &&
          memcmp(&sentinel, &output, sizeof(output)) == 0);
    CHECK(mc_platform_date_format_inspect(format, &after) && unchanged(before, after));
    CHECK((!tz_present ? unsetenv("TZ") : setenv("TZ", saved, 1)) == 0);
#endif
    CHECK(samples == 1);
    CHECK(mc_platform_date_format_retire(&format, &error) == MC_PLATFORM_DATE_FORMAT_OK &&
          format == NULL && samples == 1);
    CHECK(mc_platform_date_format_retire(&format, &error) == MC_PLATFORM_DATE_FORMAT_OK);
    CHECK(mc_platform_date_format_retire(NULL, &error) == MC_PLATFORM_DATE_FORMAT_ARGUMENT);
    CHECK(!mc_platform_date_format_inspect(NULL, &after));
    printf("DateFormat: %u checks; %zu JDK vectors; one real constructor clock, no format clocks\n",
           checks, cases);
    return 0;
}
