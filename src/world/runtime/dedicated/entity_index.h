#ifndef MC_WORLD_RUNTIME_DEDICATED_ENTITY_INDEX_H
#define MC_WORLD_RUNTIME_DEDICATED_ENTITY_INDEX_H

#include "mc/entity/errors.h"
#include "mc/world/storage/status.h"

typedef struct mc_entity mc_entity;
typedef struct mc_world_entity_index mc_world_entity_index;
typedef struct mc_world_entity_index_node mc_world_entity_index_node;

/* Actual og fields: key, nullable Entity value, next and cached hash. Views are
 * borrowed and read-only, even where the C pointer type permits a store. */
struct mc_world_entity_index_node {
    int32_t a;
    mc_entity *b;
    mc_world_entity_index_node *c;
    int32_t d;
};

typedef struct mc_world_entity_index_limits {
    size_t max_nodes, max_retained_bytes, max_peak_bytes;
} mc_world_entity_index_limits;

typedef enum mc_world_entity_index_method {
    MC_WORLD_ENTITY_INDEX_NONE = 0,
    MC_WORLD_ENTITY_INDEX_CONSTRUCTOR,
    MC_WORLD_ENTITY_INDEX_PUT,
    MC_WORLD_ENTITY_INDEX_NODE,
    MC_WORLD_ENTITY_INDEX_RESIZE,
    MC_WORLD_ENTITY_INDEX_GET,
    MC_WORLD_ENTITY_INDEX_CONTAINS,
    MC_WORLD_ENTITY_INDEX_REMOVE,
    MC_WORLD_ENTITY_INDEX_CLEAR,
    MC_WORLD_ENTITY_INDEX_INSPECT
} mc_world_entity_index_method;

typedef struct mc_world_entity_index_error {
    mc_world_storage_error cause;
    mc_entity_status entity_status;
    mc_world_entity_index_method method;
    unsigned int bytecode;
} mc_world_entity_index_error;

/* a is the actual table backing, with source fields b=count, c=threshold,
 * d=load factor and e=wrapping modification counter. Nodes and table identity
 * are lossless; duplicates and nullable values are not collapsed. Keep the
 * owner live and exclusive. A mutation can unlink/free nodes or replace a. */
typedef struct mc_world_entity_index_view {
    const mc_world_entity_index *owner;
    mc_world_entity_index_node *const *a;
    size_t a_length;
    int32_t b, c;
    float d;
    int32_t e;
    size_t allocation_bytes;
} mc_world_entity_index_view;

/* Defaults: 65536 nodes, 8 MiB retained and 16 MiB peak native allocations.
 * Count owner/current table/nodes; peak also counts the replacement table
 * before transfer. Referenced Entity allocations and allocator overhead are
 * separate. max_nodes must fit INT32_MAX; zero budgets are valid. These are
 * native bounds, not JVM OOM timing. No allocator or gameplay callbacks run. */
mc_world_entity_index_limits mc_world_entity_index_default_limits(void);
mc_world_storage_status mc_world_entity_index_create(const mc_world_entity_index_limits *limits,
                                                     mc_world_entity_index **out,
                                                     mc_world_entity_index_error *error);

/* Actual go.a(int,Object): update EVERY matching node in chain order, then
 * increment e and ALWAYS prepend a new node. Each nonnull node owns one Entity
 * hold; replacing the same pointer keeps that hold. No Entity class, phase,
 * World or ID check is added. The caller keeps value live throughout the call.
 * A later retain/node/resize failure preserves completed replacements, e and
 * any published node/count. Resize tests the count BEFORE that insertion.
 * Native hold failure occurs at the reached value store/constructor bridge;
 * it does not claim an original reference-assignment/OOM failure boundary. */
mc_world_storage_status mc_world_entity_index_put(mc_world_entity_index *index, int32_t key,
                                                  mc_entity *value,
                                                  mc_world_entity_index_error *error);

/* Distinguish absent from present-null. get returns a borrowed actual value,
 * valid only while independently held or retained by an unchanged node. Both
 * outputs are required. contains tests node presence, never value nonnullness.
 * No query allocates, retains an Entity or changes source fields. */
mc_world_storage_status mc_world_entity_index_get(const mc_world_entity_index *index, int32_t key,
                                                  bool *present, mc_entity **out,
                                                  mc_world_entity_index_error *error);
mc_world_storage_status mc_world_entity_index_contains(const mc_world_entity_index *index,
                                                       int32_t key, bool *out,
                                                       mc_world_entity_index_error *error);

/* Remove the FIRST matching node. present is required; optional out receives
 * that node's transferred Entity hold (NULL for absent/present-null). When out
 * is NULL, release that hold here. An absent key changes neither b nor e. */
mc_world_storage_status mc_world_entity_index_remove(mc_world_entity_index *index, int32_t key,
                                                     bool *present, mc_entity **out,
                                                     mc_world_entity_index_error *error);
/* Actual clear increments e even when empty and retains table/threshold. */
mc_world_storage_status mc_world_entity_index_clear(mc_world_entity_index *index,
                                                    mc_world_entity_index_error *error);
mc_world_storage_status mc_world_entity_index_inspect(const mc_world_entity_index *index,
                                                      mc_world_entity_index_view *out,
                                                      mc_world_entity_index_error *error);

/* Internal retirement releases all holds/backings without invoking source
 * clear, observers, Entity death or World removal. NULL is harmless. */
void mc_world_entity_index_free(mc_world_entity_index *index);

/* All calls require exclusive access. Arguments/results/errors must be live,
 * mutually disjoint and disjoint from owner/backings. Failure preserves query
 * and create outputs. No stale-pointer detection or general Object map API. */

#endif
