#ifndef MC_WORLD_RUNTIME_SAVING_PROGRESS_INTERNAL_H
#define MC_WORLD_RUNTIME_SAVING_PROGRESS_INTERNAL_H

#include "mc/world/runtime/saving/saving.h"

/* Trusted synchronous Source UI progress only, on a genuine GAME World.
 * No mutable save/cache context is delivered. Owner and result remain live;
 * the result may be a subobject of the callback owner. Runtime/result remain
 * disjoint and callback-owned data stays valid through return.
 * NULL callback disables progress for this call,
 * including configured options progress; runtime options are never rebound.
 * Actual public raw borrows inside a callback still discard the owned index.
 * Failure does not replay work; the ordinary full lifecycle owns all holds,
 * callback order, validation and per-attempt errors. Not a COMPOSITION hook. */
typedef mc_chunk_callback_result (*mc_world_runtime_save_stage_callback)(void *,
                                                                         mc_world_save_stage);

mc_chunk_lifecycle_status
mc_world_runtime_save_stages_detailed(mc_world_runtime *, bool force, void *owner,
                                      mc_world_runtime_save_stage_callback,
                                      mc_world_runtime_lifecycle_error *);

#endif
