#ifndef MC_ENTITY_CLASSES_INTERNAL_H
#define MC_ENTITY_CLASSES_INTERNAL_H

#include "../registry/registry_internal.h"
#include "mc/entity/classes/classes.h"

struct mc_entity_bounds;

/* Actual class-owned production layouts, shared with the concrete constructors
 * and native invariant tests. No World/simulation operation belongs here. */
typedef struct mc_entity_bounds_pool {
    struct mc_entity_bounds **elements;
    size_t length, capacity;
} mc_entity_bounds_pool;

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
    int32_t next_id, bounds_cursor;
    mc_entity_bounds_pool *bounds_pool;
    mc_entity_type_map *types;
    mc_entity_class_literal literals[MC_ENTITY_CLASS_KIND_COUNT];
    mc_entity_class_phase registry_phase;
    mc_entity_registry_error registry_failure;
    mc_entity_registry_map *registry_maps[4];
};

/* Read-only admission for classes_release: NULL is INVALID_ARGUMENT and zero
 * borrowers is STATE_ERROR. No class phase, live-entity or entered gate is
 * added, and no owner state, allocation or callback is touched. Success
 * reserves nothing; keep this owner exclusive and unchanged through release. */
mc_entity_status mc_entity_classes_runtime_release_admit(const mc_entity_classes *classes);

#endif
