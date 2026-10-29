#ifndef MC_CLIENT_WORLD_SOURCE_CHUNK_COMPILE_INTERNAL_H
#define MC_CLIENT_WORLD_SOURCE_CHUNK_COMPILE_INTERNAL_H

#include "source_chunk_renderer_internal.h"

typedef struct mc_client_chunk_cache mc_client_chunk_cache;
typedef struct mc_client_chunk_cache_error mc_client_chunk_cache_error;
typedef struct mc_source_dk_compile mc_source_dk_compile;
typedef struct mc_client_chunk_compile_state mc_client_chunk_compile_state;

struct mc_client_chunk_compile_state {
    mc_client_source_dk *chunk;
    const mc_client_chunk_cache *cache;
    const mc_client_chunk_cache_error *cache_failure;
    const mc_client_chunk_cache_error *cache_cleanup;
    mc_client_render_blocks renderer;
    unsigned int source_pc, last_store_pc;
    int32_t x, y, z, block_id, pass;
    bool active, returned, list_open, matrix_pushed, tessellator_started;
    bool needs_later_pass, pass_rendered;
    const mc_client_render_global_error *failure;
};

/* Only the admitted N operation may enter these borrowed-guard children. */
bool mc_source_dk_compile_entered(mc_client_source_dk *, mc_source_dk_compile **,
                                  mc_client_render_global_error *);
bool mc_source_dk_compile_inspect(const mc_source_dk_compile *,
                                  struct mc_client_chunk_compile_state *);
mc_client_render_global_status mc_source_dk_compile_retire(mc_source_dk_compile **,
                                                           mc_client_render_global_error *);
#endif
