#ifndef MC_WORLD_PATH_FINDER_INTERNAL_H
#define MC_WORLD_PATH_FINDER_INTERNAL_H

#include "mc/world/path/path.h"
#include "mc/world/runtime/chunk_cache.h"

typedef struct mc_path_finder mc_path_finder;
typedef struct mc_path_map_entry mc_path_map_entry;

typedef struct mc_path_budget {
    size_t max_bytes, max_allocations;
    size_t bytes, peak_bytes, allocations;
} mc_path_budget;

typedef struct mc_path_finder_limits {
    size_t max_points, max_expanded_points, max_cell_queries;
    size_t max_predecessor_steps, max_work;
} mc_path_finder_limits;

typedef struct mc_path_finder_error {
    mc_path_error path;
    mc_registry_error registry;
    mc_client_chunk_cache_error cache;
} mc_path_finder_error;

typedef struct mc_path_query {
    bool metadata, completed;
    int32_t x, y, z, value;
    mc_path_site site;
    mc_client_chunk_cache_status status;
} mc_path_query;

typedef struct mc_path_finder_state {
    bool constructed, active, returned, failed;
    unsigned int constructor_pc;
    mc_client_chunk_cache *a;
    const void *heap_identity, *map_identity, *pending_heap_identity, *pending_map_identity;
    mc_path_point *const *heap;
    size_t heap_capacity;
    int32_t heap_count;
    const mc_path_map_entry *const *map;
    size_t map_capacity;
    int32_t map_count, map_threshold, map_modifications;
    const mc_path_map_entry *const *pending_map_array;
    size_t pending_map_capacity;
    const mc_path_map_entry *map_transfer_entry;
    mc_path_point *const *neighbors;
    mc_path_point *start, *goal, *extent, *closest, *current, *pending_point;
    mc_path_point *intern_point, *heap_work_point;
    int32_t heap_work_index;
    mc_path_point *const *pending_array;
    size_t pending_array_length, points, expanded_points, cell_queries, predecessor_steps, work;
    const mc_path_query *queries;
    size_t query_count;
    mc_path *result, *pending_path;
    mc_path_site site, heap_site, map_site, point_site;
    mc_path_site preparation_site, search_site, neighbors_site, candidate_site;
    mc_path_site clearance_site, intern_site, reconstruction_site;
    const mc_path_finder_error *error;
} mc_path_finder_state;

typedef struct mc_path_map_entry_state {
    int32_t hash, key;
    const mc_path_point *value;
    const mc_path_map_entry *next;
} mc_path_map_entry_state;

/* The FD owner holds these concrete dependencies throughout this entered FW
 * invocation. No receiver adapter or callback is accepted. Failed construction
 * publishes its actual partial owner when native work allocation succeeded.
 * budget storage outlives the finder and starts with FD/EW charges included. */
mc_path_status mc_path_finder_create(mc_entity_classes *, mc_registry_classes *, mc_math_classes *,
                                     mc_client_chunk_cache *, const mc_path_finder_limits *,
                                     mc_path_budget *, mc_path_finder **, mc_path_error *);
mc_path_status mc_path_finder_to_entity(mc_path_finder *, mc_entity *, mc_entity *, float);
mc_path_status mc_path_finder_to_coordinates(mc_path_finder *, mc_entity *, int32_t, int32_t,
                                             int32_t, float);
bool mc_path_finder_inspect(const mc_path_finder *, mc_path_finder_state *);
bool mc_path_finder_point_inspect(const mc_path_finder *, const mc_path_point *,
                                  mc_path_point_state *);
bool mc_path_map_entry_inspect(const mc_path_map_entry *, mc_path_map_entry_state *);
void mc_path_finder_destroy(mc_path_finder *);

#endif
