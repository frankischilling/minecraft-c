#ifndef MC_ENTITY_CLASSES_INTERNAL_H
#define MC_ENTITY_CLASSES_INTERNAL_H

#include "../registry/registry_internal.h"
#include "mc/entity/classes/classes.h"
#include "mc/entity/hit_result.h"
#include "mc/entity/particles/origin.h"

struct mc_minecart_shape_array;
struct mc_entity_vector;
struct mc_entity_vector_object;
struct mc_entity_bounds;
struct mc_entity_bounds_object;
struct mc_entity_hit_kind_state;

/* Actual class-owned production layouts, shared with the concrete constructors
 * and native invariant tests. No World/simulation operation belongs here. */
typedef struct mc_entity_bounds_pool {
    struct mc_entity_bounds **elements;
    size_t length, capacity;
} mc_entity_bounds_pool;

typedef struct mc_entity_vector_pool {
    struct mc_entity_vector **elements;
    size_t length, capacity;
} mc_entity_vector_pool;

typedef struct mc_entity_type_entry {
    int32_t kind, value;
    struct mc_entity_type_entry *next;
} mc_entity_type_entry;

typedef struct mc_entity_type_map {
    mc_entity_type_entry *head, *tail;
    size_t size;
} mc_entity_type_map;

struct mc_entity_class_literal {
    const mc_entity_classes *owner;
    mc_entity_source source;
    mc_entity_class_kind kind;
    const char *client_name, *server_name;
};

struct mc_entity_classes {
    mc_entity_source source;
    mc_java_random_profile *profile;
    mc_entity_limits limits;
    size_t allocated_bytes, live_entities, borrowers;
    mc_entity_class_phase entity_phase, bounds_phase, watcher_phase;
    mc_particle_origin particle_origin; /* Actual XW.l/m/n; class aggregate defaults to +0. */
    int32_t next_id, bounds_cursor;
    mc_entity_bounds_pool *bounds_pool;
    struct mc_entity_bounds_object *bounds_objects;
    size_t bounds_holds;
    mc_entity_class_phase vector_phase;
    int32_t vector_cursor;
    mc_entity_vector_pool *vector_pool;
    struct mc_entity_vector_object *vector_objects;
    size_t vector_holds;
    size_t live_hit_results, live_hit_calls;
    mc_entity_class_phase hit_kind_phase;
    struct mc_entity_hit_kind_state *hit_kinds;
    mc_entity_hit_kind_error hit_kind_failure;
    mc_entity_type_map *types;
    mc_entity_class_literal literals[MC_ENTITY_CLASS_KIND_COUNT];
    mc_entity_class_phase registry_phase;
    mc_entity_registry_error registry_failure;
    mc_entity_registry_map *registry_maps[4];
    mc_entity_class_phase minecart_phase;
    uint16_t minecart_bytecode;
    mc_entity_error minecart_failure;
    struct mc_minecart_shape_array *minecart_j;
};

/* Read-only admission for classes_release: NULL is INVALID_ARGUMENT and zero
 * borrowers is STATE_ERROR. No class phase, live-entity or entered gate is
 * added, and no owner state, allocation or callback is touched. Success
 * reserves nothing; keep this owner exclusive and unchanged through release. */
mc_entity_status mc_entity_classes_runtime_release_admit(const mc_entity_classes *classes);

#endif
