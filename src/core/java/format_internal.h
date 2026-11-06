#ifndef MC_CORE_JAVA_FORMAT_INTERNAL_H
#define MC_CORE_JAVA_FORMAT_INTERNAL_H

#include "mc/core/java/format.h"

/* English full uppercase on UTF-16, including supplementary characters,
 * one-to-many mappings and retained lone surrogates. The result may borrow
 * unchanged input. Native allocations use the caller's retained arena. */
mc_java_format_status mc_java_format_upper(const mc_java_format_options *options,
                                           const mc_java_format_text *input,
                                           mc_java_format_text *out);

#endif
