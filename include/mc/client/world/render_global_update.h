#ifndef MC_CLIENT_WORLD_RENDER_GLOBAL_UPDATE_H
#define MC_CLIENT_WORLD_RENDER_GLOBAL_UPDATE_H

#include "mc/client/world/source_chunk_renderer.h"
#include "mc/entity/entity.h"

typedef struct mc_client_render_global_update_call mc_client_render_global_update_call;
typedef struct mc_client_chunk_compile_state mc_client_chunk_compile_state;

typedef struct mc_client_render_global_update_state {
    mc_client_render_global *owner;
    mc_entity *camera;
    bool active, visible_only, returned, result;
    unsigned int source_pc;
    int32_t initial_count, near_count, far_count, index;
    size_t near_length, compile_attempts, completed_compiles, remaining_count;
    mc_client_source_dk *const *near_chunks;
    mc_client_source_dk *far_chunks[2];
    mc_client_source_dk *const *compile_order;
    const mc_client_chunk_compile_state *compilation;
    const mc_client_render_global_error *failure, *cleanup;
} mc_client_render_global_update_state;

/* Actual n.a(Lls;Z)Z. The same dirty List is mutated before compilation and
 * compacted only after the reached children return. A false source result is
 * distinct from a failed call. No failed source call can be resumed/replayed.
 * App, its native class namespaces and the Display stay live and exclusive
 * through retirement. The exact nullable camera is independently retained.
 * Out starts NULL; reached failures publish an inspectable terminal owner.
 * Borrowed List/DK/compile views expire on mutation or native retirement. */
mc_client_render_global_status
mc_client_render_global_update_begin(mc_client_render_global *, mc_entity *nullable_camera,
                                     bool visible_only, mc_client_render_global_update_call **,
                                     mc_client_render_global_error *);
bool mc_client_render_global_update_inspect(const mc_client_render_global_update_call *,
                                            mc_client_render_global_update_state *);
mc_client_render_global_status
mc_client_render_global_update_retire(mc_client_render_global_update_call **,
                                      mc_client_render_global_error *);

#endif
