#ifndef MC_WORLD_RUNTIME_WORLD_ACCESS_ERROR_H
#define MC_WORLD_RUNTIME_WORLD_ACCESS_ERROR_H

#include "mc/world/storage/status.h"

#include <stdbool.h>

typedef struct mc_world_access mc_world_access;
typedef struct mc_client_render_global_error mc_client_render_global_error;
typedef struct mc_world_access_receipt mc_world_access_receipt;

/* An explicit receipt from this particular interface invocation. The concrete
 * receiver owns the immutable full failure through its checked retirement;
 * callers keep that receiver/graph live while inspecting this borrowed view.
 * NULL fields describe an ordinary native-adapter failure. No ambient owner
 * diagnostic is queried to reconstruct this record. */
typedef struct mc_world_access_failure {
    const mc_world_access *receiver;
    const mc_client_render_global_error *render_global;
    mc_world_access_receipt *receipt;
} mc_world_access_failure;

/* Capture the same invocation receipt before its enclosing source graph may
 * retire. The owned destination must be empty and disjoint. Release consumes
 * only that captured occurrence. A concrete receiver's checked retirement is
 * BUSY until all captures are released; the original borrowed receipt expires
 * only when that receiver's receipt owner is successfully retired. */
mc_world_storage_status mc_world_access_failure_retain(const mc_world_access_failure *,
                                                       mc_world_access_failure *);
void mc_world_access_failure_release(mc_world_access_failure *);
/* Exact producer classification for this same invocation, usable by source
 * catch(Exception) callers without linking a concrete client implementation.
 * False preserves optional out; native limits/frontiers are not Throwables. */
bool mc_world_access_failure_source_exception(const mc_world_access_failure *,
                                              const char **exception_class);

#endif
