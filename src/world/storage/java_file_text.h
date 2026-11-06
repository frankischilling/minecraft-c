#ifndef MC_WORLD_STORAGE_JAVA_FILE_TEXT_H
#define MC_WORLD_STORAGE_JAVA_FILE_TEXT_H
#include "mc/world/storage/java_file.h"
/* Shared strict native UTF8->existing immutable UTF16 String conversion.
 * Used by File enumeration and actual default-property native providers. */
mc_java_file_status mc_file_decode_utf8(const char *input, size_t maximum_bytes,
                                        mc_world_info_name **out, mc_java_file_error *error);
#endif
