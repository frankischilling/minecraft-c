#ifndef MC_CORE_JAVA_STRING_CASE_H
#define MC_CORE_JAVA_STRING_CASE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Counted UTF16 String.equalsIgnoreCase for the qualified JDK17 runtime.
 * The caller classifies an actual nullable Java receiver separately. Native
 * spans with positive length require readable nonnull storage; invalid spans
 * return false. No locale, allocation, normalization or full-uppercase expansion. */
bool mc_java_string_equals_ignore_case(const uint16_t *, size_t, const uint16_t *, size_t);

#endif
