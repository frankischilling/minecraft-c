#ifndef MC_CORE_JAVA_STRING_LOWER_HEX_H
#define MC_CORE_JAVA_STRING_LOWER_HEX_H

#include <stddef.h>
#include <stdint.h>

typedef enum mc_java_string_lower_hex_status {
    MC_JAVA_STRING_LOWER_HEX_OK,
    MC_JAVA_STRING_LOWER_HEX_INVALID_ARGUMENT,
    MC_JAVA_STRING_LOWER_HEX_UNSUPPORTED
} mc_java_string_lower_hex_status;

/* Borrowed proof of the qualified JDK17 whole-String lowercase hex domain.
 * Initialize only through prepare; keep the span unchanged while using it. */
typedef struct mc_java_string_lower_hex_view {
    const uint16_t *units;
    size_t length;
} mc_java_string_lower_hex_view;

/* Scan the whole counted UTF16 String once. Accept only inputs whose possible
 * default-Locale lowercase paths preserve all UTF16 positions and ASCII hex
 * projection. Potential insertion/removal is unsupported, including changes
 * after a selected format site. No allocation, host Locale or general lowercase.
 * Positive length requires readable nonnull storage. Length must fit Java int.
 * Empty spans are valid. Invalid/unsupported inputs leave out unchanged. */
mc_java_string_lower_hex_status
mc_java_string_lower_hex_prepare(const uint16_t *units, size_t length,
                                 mc_java_string_lower_hex_view *out);

/* Project lower(String).charAt(index) into "0123456789abcdef", with fallback15.
 * Requires an unchanged successfully prepared view and index < view->length.
 * The whole-String domain check is essential: this is not general char lower. */
uint8_t mc_java_string_lower_hex_project(const mc_java_string_lower_hex_view *view, size_t index);

#endif
