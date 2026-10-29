#ifndef MC_CLIENT_WORLD_CHUNK_CACHE_H
#define MC_CLIENT_WORLD_CHUNK_CACHE_H

#include "mc/world/chunk/object.h"
#include "mc/world/runtime/acquisition_error.h"
#include "mc/world/runtime/tile_entities/errors.h"

typedef struct mc_client_chunk_cache mc_client_chunk_cache;
typedef struct mc_tile_entity mc_tile_entity;
typedef struct mc_world_chunk_manager mc_world_chunk_manager;

typedef enum mc_client_chunk_cache_status {
    MC_CLIENT_CHUNK_CACHE_OK,
    MC_CLIENT_CHUNK_CACHE_INVALID_ARGUMENT,
    MC_CLIENT_CHUNK_CACHE_BUSY,
    MC_CLIENT_CHUNK_CACHE_LIMIT,
    MC_CLIENT_CHUNK_CACHE_ALLOCATION_FAILED,
    MC_CLIENT_CHUNK_CACHE_SOURCE_NULL,
    MC_CLIENT_CHUNK_CACHE_SOURCE_INDEX,
    MC_CLIENT_CHUNK_CACHE_SOURCE_NEGATIVE_ARRAY,
    MC_CLIENT_CHUNK_CACHE_CHILD_FAILED
} mc_client_chunk_cache_status;

typedef struct mc_client_chunk_cache_frame {
    const char *method, *descriptor;
    unsigned int bytecode;
} mc_client_chunk_cache_frame;

typedef struct mc_client_chunk_cache_error {
    mc_client_chunk_cache_status status;
    const char *operation, *method, *descriptor;
    unsigned int bytecode, child_bytecode;
    bool native_policy;
    /* Actual inner ew frames, outermost first. Direct Chunk/World/Registry
     * causes remain in their typed records/child_bytecode below. */
    mc_client_chunk_cache_frame nested[6];
    size_t nested_count;
    mc_world_storage_error storage;
    mc_world_runtime_acquisition_error acquisition;
    mc_registry_error registry;
    mc_live_tile_entity_error tile_entity;
} mc_client_chunk_cache_error;

typedef struct mc_client_chunk_cache_state {
    mc_world_runtime *d;
    int32_t a, b, rows, columns;
    mc_chunk_state *const *const *c;
    size_t acquired, leased;
    unsigned int last_store_pc, source_pc;
    bool active, returned, retiring;
    const mc_client_chunk_cache_error *failure, *cleanup;
} mc_client_chunk_cache_state;

/* Actual ew(World,IIIIII): Y endpoints are unused; every inclusive X-major,
 * Z-minor World.c(II) result is stored in the SAME rectangular c. Source d/a/b
 * stores and earlier acquisitions survive failure. A published native owner
 * remains inspectable/retirable on success or reached failure. NULL World is
 * a source operand, reached only at112 when a nonempty acquisition is needed.
 * Native rectangular allocation is bounded to 1048576 cells, distinct from
 * Java OOM timing. No coordinate normalization, generation substitute or
 * accessor reacquisition is performed. out must initially be NULL.
 *
 * Each acquired nonnull exact Store identity has an independent lease. World,
 * Registry, Store and their dependencies stay live/exclusive through checked
 * retirement. All outputs/errors must be live and disjoint from this graph.
 * Native cleanup releases only occurrences; no source rollback/retry occurs.
 * c and its cell views expire when retirement starts, including refusal. */
mc_client_chunk_cache_status mc_client_chunk_cache_create(mc_world_runtime *, int32_t x0,
                                                          int32_t y0, int32_t z0, int32_t x1,
                                                          int32_t y1, int32_t z1,
                                                          mc_client_chunk_cache **out,
                                                          mc_client_chunk_cache_error *);
bool mc_client_chunk_cache_inspect(const mc_client_chunk_cache *, mc_client_chunk_cache_state *);
mc_client_chunk_cache_status mc_client_chunk_cache_retire(mc_client_chunk_cache **,
                                                          mc_client_chunk_cache_error *);

/* Actual xp methods on this cache. Ordinary outputs remain unchanged on
 * failure. Per-call errors are values; constructor diagnostics stay immutable.
 * Accessors admit a failed constructor's real prefix. Only block has its
 * explicit out-of-cache/null-cell zero guards. Metadata has the Y gate but
 * real array/null failures; TileEntity and light have their own source rules.
 * TileEntity success returns one held occurrence (or NULL); release it with
 * mc_tile_entity_release. Material/manager are borrowed canonical identities. */
mc_client_chunk_cache_status mc_client_chunk_cache_block(mc_client_chunk_cache *, int32_t, int32_t,
                                                         int32_t, int32_t *,
                                                         mc_client_chunk_cache_error *);
mc_client_chunk_cache_status mc_client_chunk_cache_metadata(mc_client_chunk_cache *, int32_t,
                                                            int32_t, int32_t, int32_t *,
                                                            mc_client_chunk_cache_error *);
mc_client_chunk_cache_status mc_client_chunk_cache_tile_entity(mc_client_chunk_cache *, int32_t,
                                                               int32_t, int32_t, mc_tile_entity **,
                                                               mc_client_chunk_cache_error *);
mc_client_chunk_cache_status mc_client_chunk_cache_light(mc_client_chunk_cache *, int32_t, int32_t,
                                                         int32_t, bool neighbors, int32_t *,
                                                         mc_client_chunk_cache_error *);
mc_client_chunk_cache_status mc_client_chunk_cache_brightness(mc_client_chunk_cache *, int32_t,
                                                              int32_t, int32_t, float *,
                                                              mc_client_chunk_cache_error *);
mc_client_chunk_cache_status mc_client_chunk_cache_min_brightness(mc_client_chunk_cache *, int32_t,
                                                                  int32_t, int32_t, int32_t minimum,
                                                                  float *,
                                                                  mc_client_chunk_cache_error *);
mc_client_chunk_cache_status mc_client_chunk_cache_material(mc_client_chunk_cache *, int32_t,
                                                            int32_t, int32_t, mc_material **,
                                                            mc_client_chunk_cache_error *);
mc_client_chunk_cache_status mc_client_chunk_cache_opaque(mc_client_chunk_cache *, int32_t, int32_t,
                                                          int32_t, bool *,
                                                          mc_client_chunk_cache_error *);
mc_client_chunk_cache_status mc_client_chunk_cache_normal_cube(mc_client_chunk_cache *, int32_t,
                                                               int32_t, int32_t, bool *,
                                                               mc_client_chunk_cache_error *);
mc_client_chunk_cache_status mc_client_chunk_cache_manager(mc_client_chunk_cache *,
                                                           mc_world_chunk_manager **,
                                                           mc_client_chunk_cache_error *);

#endif
