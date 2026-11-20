#ifndef MC_CORE_JAVA_STRING_CASE_INTERNAL_H
#define MC_CORE_JAVA_STRING_CASE_INTERNAL_H

#include <stdint.h>

/* JDK17 simple case comparison key. Invalid scalar values stay unchanged.
 * This private seam supports exact exhaustive native/reference validation. */
uint32_t mc_java_string_case_fold(uint32_t);

#endif
