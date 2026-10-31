#ifndef MC_WORLD_PATH_INTERNAL_H
#define MC_WORLD_PATH_INTERNAL_H

#include "../../block/registry/registry_internal.h"
#include "../../block/registry/runtime_internal.h"
#include "../../entity/classes/memory_internal.h"
#include "../../entity/classes/path_namespace_internal.h"
#include "../../entity/entity_internal.h"
#include "../../entity/player/tick_math_internal.h"
#include "finder_internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/core/java/java.h"

typedef struct mc_path_mem {
    struct mc_path_mem *next;
    size_t bytes;

    /* Fundamental scalar alignment, including Windows CRT C mode. The
     * explicit extent preserves malloc's qualified x64 payload alignment. */
    union {
        long double floating_alignment;
        long long integer_alignment;
        void *pointer_alignment;
        uint64_t extent_alignment[2];
    } alignment;

    unsigned char data[];
} mc_path_mem;

_Static_assert(offsetof(mc_path_mem, data) == sizeof(mc_path_mem), "Path header extent");
_Static_assert(offsetof(mc_path_mem, data) % 16u == 0, "preserve x64 Path payload alignment");
_Static_assert(_Alignof(mc_path_mem) >= _Alignof(double), "Path double alignment");
_Static_assert(_Alignof(mc_path_mem) >= _Alignof(void *), "Path pointer alignment");

typedef struct mc_path_arena {
    mc_entity_classes *classes;
    mc_path_mem *allocations;
    size_t holds, bytes, points;
    struct mc_path_point *point_list;
} mc_path_arena;

struct mc_path_point {
    int32_t a, b, c, j, d;
    float e, f, g;
    struct mc_path_point *h;
    bool i, constructed;
    unsigned int constructor_pc;
    struct mc_path_point *allocation_next;
};

struct mc_path {
    mc_path_point **b;
    int32_t a, c;
    size_t array_length, holds;
    mc_path_arena *arena;
    bool constructed;
    unsigned int constructor_pc;
};

typedef struct mc_path_heap {
    mc_path_point **a;
    size_t capacity;
    int32_t b;
} mc_path_heap;

struct mc_path_map_entry {
    int32_t a, d;
    mc_path_point *b;
    struct mc_path_map_entry *c;
};

typedef struct mc_path_map {
    mc_path_map_entry **a;
    size_t capacity;
    int32_t b, c, e;
    float d;
} mc_path_map;

struct mc_path_finder {
    mc_entity_classes *classes;
    mc_registry_classes *registry;
    mc_math_classes *math;
    mc_client_chunk_cache *a;
    mc_path_heap *b;
    mc_path_map *c;
    mc_path_heap *pending_heap;
    mc_path_map *pending_map;
    mc_path_point **d;
    mc_path_arena *arena;
    mc_path_mem *allocations;
    mc_path_budget *budget;
    mc_path_finder_limits limits;
    mc_path_finder_state state;
    mc_path_finder_error error;
    mc_path_query *queries;
    size_t query_capacity;
    bool namespace_open;
};

void pf_at(mc_path_finder *, const char *, const char *, int32_t);
void pf_child(mc_path_finder *, const char *, const char *, const char *, int32_t);
bool pf_fail(mc_path_finder *, mc_path_status, const char *);
bool pf_source(mc_path_finder *, const char *, const char *);
bool pf_math(mc_path_finder *);
bool pf_fp(mc_path_finder *);
bool pf_work(mc_path_finder *);
void *pf_alloc(mc_path_finder *, size_t, bool);
void pf_free(mc_path_finder *, void *);
void pf_arena_drop(mc_path_arena *);
bool pf_registry_fail(mc_path_finder *, const char *);
bool pf_cache_fail(mc_path_finder *, const char *);
bool pf_query(mc_path_finder *, bool, int32_t, int32_t, int32_t, int32_t *);
mc_path_point *pf_point_new(mc_path_finder *);
int32_t pf_point_key(int32_t, int32_t, int32_t);
bool pf_point_equals(const mc_path_point *, const mc_path_point *);
void pf_point_construct(mc_path_finder *, mc_path_point *, int32_t, int32_t, int32_t);
mc_path_point *pf_intern(mc_path_finder *, int32_t, int32_t, int32_t);
bool pf_distance(mc_path_finder *, const mc_path_point *, const mc_path_point *, float *);
bool pf_heap_add(mc_path_finder *, mc_path_point *);
bool pf_heap_pop(mc_path_finder *, mc_path_point **);
bool pf_heap_change(mc_path_finder *, mc_path_point *, float);
void pf_heap_clear(mc_path_finder *);
mc_path_point *pf_map_get(mc_path_finder *, int32_t);
bool pf_map_put(mc_path_finder *, int32_t, mc_path_point *);
bool pf_map_clear(mc_path_finder *);
bool pf_path_construct(mc_path_finder *, mc_path_point **, size_t);
bool pf_clearance(mc_path_finder *, int32_t, int32_t, int32_t, mc_path_point *, int32_t *);
bool pf_neighbors(mc_path_finder *, mc_path_point *, mc_path_point *, mc_path_point *, float,
                  size_t *);
bool pf_candidate(mc_path_finder *, int32_t, int32_t, int32_t, mc_path_point *, int32_t,
                  mc_path_point **);
bool pf_reconstruct(mc_path_finder *, mc_path_point *);
bool pf_point_snapshot(const mc_path_arena *, const mc_path_point *, mc_path_point_state *);

#endif
