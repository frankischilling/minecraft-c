#ifndef MC_CORE_JAVA_FORMAT_H
#define MC_CORE_JAVA_FORMAT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct mc_java_format_text {
    const uint16_t *units;
    size_t length;
} mc_java_format_text;

typedef struct mc_java_format_allocator {
    void *context;
    void *(*allocate)(void *context, size_t bytes);
} mc_java_format_allocator;

typedef enum mc_java_format_locale {
    MC_JAVA_FORMAT_LOCALE_UNSUPPORTED,
    MC_JAVA_FORMAT_LOCALE_ENGLISH
} mc_java_format_locale;

typedef enum mc_java_format_status {
    MC_JAVA_FORMAT_OK,
    MC_JAVA_FORMAT_INVALID_ARGUMENT,
    MC_JAVA_FORMAT_ALLOCATION_FAILED,
    MC_JAVA_FORMAT_LIMIT,
    MC_JAVA_FORMAT_NULL_FORMAT,
    MC_JAVA_FORMAT_UNKNOWN_CONVERSION,
    MC_JAVA_FORMAT_DUPLICATE_FLAGS,
    MC_JAVA_FORMAT_ILLEGAL_FLAGS,
    MC_JAVA_FORMAT_FLAGS_CONVERSION_MISMATCH,
    MC_JAVA_FORMAT_MISSING_WIDTH,
    MC_JAVA_FORMAT_ILLEGAL_WIDTH,
    MC_JAVA_FORMAT_ILLEGAL_PRECISION,
    MC_JAVA_FORMAT_ILLEGAL_ARGUMENT_INDEX,
    MC_JAVA_FORMAT_MISSING_ARGUMENT,
    MC_JAVA_FORMAT_ILLEGAL_CONVERSION,
    MC_JAVA_FORMAT_UNSUPPORTED_LOCALE
} mc_java_format_status;

typedef struct mc_java_format_error {
    mc_java_format_status status;
    size_t offset, length;
    /* Native diagnostic operands, not a fabricated Java Throwable. */
    int32_t value;
    uint32_t flags;
    uint16_t conversion;
    bool date_time;
} mc_java_format_error;

typedef struct mc_java_format_options {
    mc_java_format_allocator allocator;
    mc_java_format_locale locale;
    /* Actual platform line separator used by the reached %n conversion. */
    mc_java_format_text line_separator;
    /* Zero selects INT32_MAX UTF-16 units. Native allocation limits remain
     * distinct from Java OutOfMemoryError allocation timing. */
    size_t max_output_units;
} mc_java_format_options;

/* String.format's reached String/null argument profile. NULL pattern is a
 * Java null format reference. A NULL argument-array pointer is Java's null
 * varargs array (all referenced arguments are null); argument_count must be
 * zero in that case. A nonnull array of length zero has no arguments.
 * Individual NULL entries are null Strings, distinct from empty text views.
 *
 * Parse/validation order, indices, flags, width/precision, UTF-16 truncation,
 * String hashes, English uppercase, null conversions and error categories
 * belong to this operation. No arbitrary Object/Formattable, numeric/date
 * value argument, Java Throwable identity or reflection contract is implied.
 * Input text may borrow earlier retained allocations. Each new allocation must
 * be disjoint from live inputs and output slots; callbacks must not mutate them.
 * Allocator storage outlives the result; no individual allocation is freed or
 * rolled back. On failure result remains unchanged and error describes the
 * reached failure. Java text errors and native resource failures are distinct.
 */
mc_java_format_status mc_java_format_strings(const mc_java_format_options *options,
                                             const mc_java_format_text *pattern,
                                             const mc_java_format_text *const *arguments,
                                             size_t argument_count, mc_java_format_text *result,
                                             mc_java_format_error *error);

#endif
