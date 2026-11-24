#ifndef MC_WORLD_RUNTIME_SAVED_LIGHT_INTERNAL_H
#define MC_WORLD_RUNTIME_SAVED_LIGHT_INTERNAL_H

#include "mc/world/runtime/queries.h"

/* Same Source body and admission as the public saved-light query. error must
 * be non-NULL and already contain the canonical successful query receipt.
 * Success leaves it canonical without copying the whole diagnostic. Failure
 * retains the complete reached receipt; the caller must abort, not reuse it.
 * Propagation initializes its own query child once and exits on first failure;
 * synchronous reentrant operations own independent frames. */
mc_world_storage_status mc_world_runtime_saved_light_canonical(mc_world_runtime *world,
                                                               const mc_world_light_type *type,
                                                               int32_t x, int32_t y, int32_t z,
                                                               int32_t *out,
                                                               mc_world_query_error *error);

#endif
