#ifndef MC_CORE_JAVA_FLOAT_PARSE_H
#define MC_CORE_JAVA_FLOAT_PARSE_H

#include <stddef.h>
#include <stdint.h>

#define MC_JAVA_FLOAT_PARSE_MAX_TEXT_BYTES (16u * 1024u * 1024u)
#define MC_JAVA_FLOAT_PARSE_MAX_UNITS (MC_JAVA_FLOAT_PARSE_MAX_TEXT_BYTES / 2u)

typedef struct mc_java_float_parse_text {
    const uint16_t *units;
    size_t length;
} mc_java_float_parse_text;

typedef enum mc_java_float_parse_status {
    MC_JAVA_FLOAT_PARSE_OK = 0,
    MC_JAVA_FLOAT_PARSE_INVALID_ARGUMENT,
    MC_JAVA_FLOAT_PARSE_LIMIT,
    MC_JAVA_FLOAT_PARSE_NULL_TEXT,
    MC_JAVA_FLOAT_PARSE_NUMBER_FORMAT
} mc_java_float_parse_status;

/* Counted UTF-16 Float.parseFloat primitive, separate from GameSettings' raw
 * true/false wrapper. A NULL text is Java null text; a nonnull empty view,
 * including {NULL,0}, is an empty String and returns NUMBER_FORMAT. Nonempty
 * views require readable units. NULL out is a native INVALID_ARGUMENT before
 * text admission; an over-budget length returns native LIMIT before traversal.
 *
 * Trim only edge units <=U+0020. Accept the whole Java decimal/hexadecimal
 * grammar, optional signs and f/F/d/D numeric suffix, or exact signed NaN and
 * Infinity without suffix. Hex requires a p/P decimal exponent. No locale,
 * underscores, Unicode digits, interior NUL terminators or partial parse.
 * Grammar is validated before numerical overflow/underflow classification.
 *
 * Construct binary32 bits using bounded integer comparisons, rounding nearest
 * with ties to even. Preserve underflow's sign; canonical positive quiet NaN
 * is 0x7fc00000. No CRT parsing, binary64 intermediate, floating arithmetic,
 * heap, callbacks, mutable global state or floating-environment dependency.
 * Requires eight-bit bytes and IEEE binary32 storage. A 192-digit prefix plus
 * sticky tail is exact for this range; checked 64-limb workspaces bound all
 * arithmetic. LIMIT is a native policy, not Java OutOfMemoryError timing.
 *
 * All failures preserve out. Input/view/out storage must be disjoint and live
 * through the call. Statuses describe outcomes, not Throwable objects or a
 * qualified original runtime. Successful parsing does not supply Float.toString.
 */
mc_java_float_parse_status mc_java_float_parse(const mc_java_float_parse_text *text, float *out);

#endif
