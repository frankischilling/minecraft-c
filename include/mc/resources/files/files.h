#ifndef MC_RESOURCES_FILES_FILES_H
#define MC_RESOURCES_FILES_FILES_H

#include "mc/resources/translation/translation.h"

typedef struct mc_resource_files mc_resource_files;

/* Read resources from a user's imported asset directory, without a JAR/JVM.
 * The UTF-8 root is copied; creation performs no filesystem access. Resource
 * names must be absolute ASCII class-loader names with ordinary path segments.
 * Empty, dot, parent, backslash and drive segments are rejected. The imported
 * directory is trusted local input; this is not a symlink sandbox.
 * Calls require exclusive access. Native failures leave outputs unchanged. */
mc_registry_status mc_resource_files_create(const char *root, mc_resource_files **out);
const mc_registry_resources *mc_resource_files_source(mc_resource_files *files);

/* Open streams remain owned by this source, including after EOF or failure.
 * Retire all borrowers before freeing it. Every stream is closed once here;
 * all storage is released even if a close reports an I/O error. */
mc_registry_status mc_resource_files_free(mc_resource_files *files);

#endif
