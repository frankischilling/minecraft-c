#ifndef MC_ENTITY_ENTITY_INTERNAL_H
#define MC_ENTITY_ENTITY_INTERNAL_H

#include "mc/entity/entity.h"

/* Actual production layouts, shared only with native invariant tests. */
typedef struct mc_entity_bounds_pool {
    mc_entity_bounds **elements;
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

struct mc_entity_watch_record {
    int32_t index, type;
    int8_t byte_value;
    bool dirty;
};

typedef struct mc_entity_watch_node {
    int32_t key;
    mc_entity_watch_record *record;
    struct mc_entity_watch_node *next;
} mc_entity_watch_node;

typedef struct mc_entity_watch_map {
    mc_entity_watch_node *head;
    size_t size;
} mc_entity_watch_map;

struct mc_entity_watcher {
    mc_entity_watch_map *map;
    bool dirty;
};

struct mc_entity_classes {
    mc_entity_source source;
    mc_java_random_profile *profile;
    mc_entity_limits limits;
    size_t allocated_bytes, live_entities;
    mc_entity_class_phase entity_phase, bounds_phase, watcher_phase;
    int32_t next_id, bounds_cursor;
    mc_entity_bounds_pool *bounds_pool;
    mc_entity_type_map *types;
};

struct mc_entity {
    mc_entity_classes *classes;
    size_t holds, direct_bytes;
    mc_entity_phase phase;
    uint16_t base_bytecode, item_bytecode;
    mc_entity_base_fields base;
    mc_item_entity_fields item;
    mc_entity_bounds *bounds;
    mc_java_random *random;
    mc_entity_watcher *watcher;
    mc_item_stack *stack;
};

/* The production constructor calls these after each real shared Math draw.
 * Their internal seam also permits replay of original observed field formulas;
 * it injects neither constructor entropy nor a Math stream into production. */
float mc_entity_item_phase_from_draw(double draw);
float mc_entity_item_yaw_from_draw(double draw);
double mc_entity_item_velocity_from_draw(double draw);

#endif
