#ifndef MC_CORE_JAVA_INTEGER_PARSE_H
#define MC_CORE_JAVA_INTEGER_PARSE_H

#include <stddef.h>
#include <stdint.h>

#define MC_JAVA_INTEGER_PARSE_MAX_UNITS (8u * 1024u * 1024u)

typedef struct mc_java_integer_parse_text {
    const uint16_t *units;
    size_t length;
} mc_java_integer_parse_text;

typedef enum mc_java_integer_parse_status {
    MC_JAVA_INTEGER_PARSE_OK = 0,
    MC_JAVA_INTEGER_PARSE_INVALID_ARGUMENT,
    MC_JAVA_INTEGER_PARSE_LIMIT,
    MC_JAVA_INTEGER_PARSE_NUMBER_FORMAT
} mc_java_integer_parse_status;

/* Counted UTF-16 Integer.parseInt(String), radix10, selected JDK17 character
 * profile. Accept an optional ASCII +/- followed by decimal digits, including
 * the profile's 370 BMP decimal units. No whitespace trim, radix prefix,
 * separators, code-point composition or NUL termination. Mixed digit scripts
 * and arbitrarily many leading zeroes within the native budget are accepted.
 *
 * NULL text, an empty String, invalid digits and signed32 overflow return
 * NUMBER_FORMAT, matching the Java exception category. NULL out is a native
 * INVALID_ARGUMENT before text admission. A nonempty view needs readable units;
 * an over-budget length returns native LIMIT before traversal. No heap, CRT
 * parsing, locale, floating arithmetic or mutable global state. Every failure
 * preserves out; input/view/out storage must be disjoint and live for the call.
 * Other Java/Unicode profiles and exception-message text are not supplied. */
mc_java_integer_parse_status mc_java_integer_parse(const mc_java_integer_parse_text *text,
                                                   int32_t *out);

#endif
