#ifndef MC_WORLD_RUNTIME_PATH_H
#define MC_WORLD_RUNTIME_PATH_H

#include "mc/world/path/path.h"
#include "mc/world/runtime/chunk_cache.h"

typedef struct mc_world_path_call mc_world_path_call;
typedef struct mc_path_finder mc_path_finder;
typedef struct mc_path_finder_error mc_path_finder_error;
typedef struct mc_world_source_chunk_error mc_world_source_chunk_error;

typedef enum mc_world_path_status {
    MC_WORLD_PATH_OK,
    MC_WORLD_PATH_ARGUMENT,
    MC_WORLD_PATH_STATE,
    MC_WORLD_PATH_BUSY,
    MC_WORLD_PATH_LIMIT,
    MC_WORLD_PATH_ALLOCATION,
    MC_WORLD_PATH_SOURCE_EXCEPTION,
    MC_WORLD_PATH_CHILD_FAILED,
    MC_WORLD_PATH_UNIMPLEMENTED,
    MC_WORLD_PATH_CLEANUP_FAILED
} mc_world_path_status;

typedef enum mc_world_path_phase {
    MC_WORLD_PATH_ACTIVE,
    MC_WORLD_PATH_RETURNED,
    MC_WORLD_PATH_FAILED,
    MC_WORLD_PATH_RETIRING
} mc_world_path_phase;

typedef struct mc_world_path_limits {
    /* Request-owned FD/EW/FW/D/DH storage, including old/new overlap.
     * World acquisition and defining Math keep their own owner policies.
     * Allocation count is cumulative; bytes count live request storage. */
    size_t max_cache_cells, max_native_bytes, max_allocations;
    size_t max_points, max_expanded_points, max_cell_queries;
    size_t max_predecessor_steps, max_work;
} mc_world_path_limits;

typedef struct mc_world_path_error {
    mc_world_path_status status;
    const char *operation, *exception_class;
    bool native_policy, source_exception;
    mc_path_site site, caller, child;
    const mc_world_storage_error *world;
    const mc_entity_error *entity;
    const mc_math_class_error *math;
    const mc_registry_error *registry;
    const mc_client_chunk_cache_error *cache;
    const mc_path_finder_error *finder;
    const mc_world_source_chunk_error *source_chunk;
} mc_world_path_error;

typedef struct mc_world_path_call_state {
    mc_world_path_phase phase;
    bool active, retiring, source_returned, to_entity;
    bool center_stored, endpoints_stored, cache_returned, finder_constructed;
    unsigned int source_pc;
    int32_t center[3], extent, endpoints[6], coordinates[3];
    float range;
    mc_world_runtime *world;
    mc_entity *source, *target;
    mc_client_chunk_cache *cache;
    const mc_path_finder *finder;
    mc_path *result;
    size_t native_bytes, native_peak_bytes, native_allocations;
    const mc_world_path_error *failure, *cleanup;
} mc_world_path_call_state;

mc_world_path_limits mc_world_path_default_limits(void);
/* Actual client FD target/coordinate overloads. A synchronous one-shot call
 * publishes its retained owner after native allocation, including on failure.
 * NULL Entity operands survive until their actual source read. Native World
 * NULL is admission failure; an entered caller owns its invocation NPE.
 * The same World, Entity operands, defining Registry and Math remain held
 * through checked retirement. No phase repair, query substitute or pool reset.
 * A returned NULL Path is distinguished by source_returned; failure is never
 * an ordinary null result. Inspect borrows result/errors; retain a result
 * before storing it or retiring the call. All access is exclusive and output
 * storage is initially NULL, live and disjoint from the complete owner graph.
 * Error views borrow this call. Nested receipts borrow their actual producer;
 * keep that producer live through diagnosis after copying a view. */
mc_world_path_status
mc_world_runtime_path_to_entity_begin(mc_world_runtime *, mc_entity *nullable_source,
                                      mc_entity *nullable_target, float range, mc_math_classes *,
                                      const mc_world_path_limits *, mc_world_path_call **);
mc_world_path_status mc_world_runtime_path_to_coordinates_begin(
    mc_world_runtime *, mc_entity *nullable_source, int32_t x, int32_t y, int32_t z, float range,
    mc_math_classes *, const mc_world_path_limits *, mc_world_path_call **);
bool mc_world_path_call_inspect(const mc_world_path_call *, mc_world_path_call_state *);
/* Native-only, retryable cleanup. Active callback reentry is BUSY before any
 * mutation. Partial success consumes only completed occurrences and invalidates
 * prior child borrows; inspect again after refusal. Source work is never replayed.
 * A separately held DH retains its actual Point arena without World/cache edges. */
mc_world_path_status mc_world_path_call_retire(mc_world_path_call **);

#endif
