#ifndef MC_CLIENT_WORLD_SOURCE_CHUNK_COMPILE_INTERNAL_H
#define MC_CLIENT_WORLD_SOURCE_CHUNK_COMPILE_INTERNAL_H

#include "source_chunk_renderer_internal.h"

typedef struct mc_client_chunk_cache mc_client_chunk_cache;
typedef struct mc_client_chunk_cache_error mc_client_chunk_cache_error;
typedef struct mc_source_dk_compile mc_source_dk_compile;
typedef struct mc_client_chunk_compile_state mc_client_chunk_compile_state;
typedef struct mc_cv_context mc_cv_context;

struct mc_client_chunk_compile_state {
    mc_client_source_dk *chunk;
    const mc_client_chunk_cache *cache;
    const mc_client_chunk_cache_error *cache_failure;
    const mc_client_chunk_cache_error *cache_cleanup;
    mc_client_render_blocks renderer;
    const mc_cv_context *render_blocks;
    unsigned int source_pc, last_store_pc;
    int32_t x, y, z, block_id, pass;
    bool active, returned, list_open, matrix_pushed, tessellator_started;
    bool needs_later_pass, pass_rendered;
    bool tile_eligibility_returned, tile_eligible;
    size_t tile_eligibility_calls;
    size_t old_tile_set_count, new_tile_set_count, shared_tiles_added, shared_tiles_removed;
    const mc_client_render_global_error *failure;
};

/* Only the admitted N operation may enter these borrowed-guard children. */
bool mc_source_dk_compile_entered(mc_client_source_dk *, mc_source_dk_compile **,
                                  mc_client_render_global_error *);
/* Selected DK.a prefix at getstatic394/invoke399/List.add411, using the same
 * child as complete compilation. No GL, World/cache query or Block draw is
 * inserted. Supplied nullable Tile is the already evaluated source local19.
 * Native test/integration callers hold entered N and actual defining owners;
 * the retained output, including failures, must be retired normally. */
bool mc_source_dk_tile_eligibility_entered(mc_client_source_dk *, mc_tile_entity *,
                                           mc_source_dk_compile **,
                                           mc_client_render_global_error *);
/* Selected List/Set continuation control: old Set capture and B.clear, the same
 * eligibility child for already evaluated Tile locals, then actual547..626.
 * Native fixture supplies registered List origins and entered N. No World,
 * ChunkCache, Block geometry, or source HashSet iteration order is fabricated. */
bool mc_source_dk_tile_lists_entered(mc_client_source_dk *, mc_tile_entity *const *, size_t,
                                     mc_source_dk_compile **, mc_client_render_global_error *);
bool mc_source_dk_compile_inspect(const mc_source_dk_compile *,
                                  struct mc_client_chunk_compile_state *);
mc_client_render_global_status mc_source_dk_compile_retire(mc_source_dk_compile **,
                                                           mc_client_render_global_error *);
#endif
