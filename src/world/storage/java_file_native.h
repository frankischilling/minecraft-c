#ifndef MC_WORLD_STORAGE_JAVA_FILE_NATIVE_H
#define MC_WORLD_STORAGE_JAVA_FILE_NATIVE_H
#include "../../platform/files/directory.h"
#include "mc/world/storage/java_file.h"

/* Private transport only, never a source File/String identity. Requires a
 * live READY/exclusive File under the unchanged f native environment/CWD
 * lifetime domain. Uses SAME mc_file_native encoding/bounds/provider kernel,
 * performs no File attribute/open operation, and creates no File/normalization.
 * Success returns an independent malloc-owned NUL-terminated native name;
 * caller free()s it. Failure preserves out. No hidden File hold is acquired:
 * caller retains the SAME File above native open/ZIP/stream lifetime. */
mc_java_file_status mc_java_file_native_path_copy(mc_java_file *file, mc_file_native_char **out,
                                                  mc_java_file_error *error);
#endif
