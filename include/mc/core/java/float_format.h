#ifndef MC_CORE_JAVA_FLOAT_FORMAT_H
#define MC_CORE_JAVA_FLOAT_FORMAT_H

#include <stddef.h>

typedef struct mc_java_float_string {
    char text[24];
    size_t length;
} mc_java_float_string;

typedef enum mc_java_float_format_status {
    MC_JAVA_FLOAT_FORMAT_OK = 0,
    MC_JAVA_FLOAT_FORMAT_INVALID_ARGUMENT,
    MC_JAVA_FLOAT_FORMAT_INTERNAL_LIMIT
} mc_java_float_format_status;

/* Float.toString/StringBuilder.append(float) conversion for the retained
 * Temurin 17.0.17 profile. This is a primitive, separate from GameSettings
 * mutation, file ordering and persistence. The result owns ASCII bytes and a
 * terminating NUL; length excludes that NUL. No destructor is needed.
 *
 * All IEEE binary32 values are admitted. Signed zero retains its sign, every
 * NaN prints NaN, and infinities retain their sign. Other values use the legacy
 * decimal grid/interval selection, including its reached primitive-width
 * wrapping decisions, then ordinary notation for decimal orders -3..6 and
 * uppercase E elsewhere. A fractional digit is retained. This profile can
 * differ from newer shortest-decimal algorithms and other JVM versions.
 *
 * Input bits are read with memcpy; no floating arithmetic is performed on
 * value. The bounded exponent estimator requires IEEE binary64 nearest/even
 * evaluation, disabled floating traps and no excess precision, reassociation
 * or fused operations. The function does not inspect or alter FP controls.
 * Requires eight-bit bytes and IEEE binary32/binary64 storage on the admitted
 * x86-64 platforms. NaN payload preservation and FP exception flags are not
 * part of the text contract.
 *
 * No heap, I/O, locale, callbacks, mutable global state or CRT float conversion.
 * Checked 512-bit integers, a fixed double-width product temporary and at most
 * ten decimal attempts bound the conversion. NULL out is native
 * INVALID_ARGUMENT. INTERNAL_LIMIT denotes a
 * checked internal invariant failure, not Java allocation-failure semantics.
 * All failures preserve out; a successful call replaces the entire result.
 */
mc_java_float_format_status mc_java_float_to_string(float value, mc_java_float_string *out);

#endif
