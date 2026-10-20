#ifndef MC_ENTITY_ENTITY_INTERNAL_H
#define MC_ENTITY_ENTITY_INTERNAL_H

#include "classes/classes_internal.h"
#include "mc/entity/entity.h"

/* Actual production layouts, shared only with native invariant tests. */
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

/* Actual post-constructor ItemEntity.c store by Block spawn. The caller owns
 * the concrete Entity; this does not add a source phase gate or constructor PC. */
mc_entity_status mc_entity_item_pickup_delay_set(mc_entity *entity, int32_t delay);

/* Borrow the actual source fields while holding this Entity. Reads and stores
 * belong to the caller's concrete source operation; no constructor/READY gate,
 * World rebinding, snapshot copy or callback is inserted here. */
mc_entity_base_fields *mc_entity_runtime_fields(mc_entity *entity);

/* Actual shared source position body. Entity and its existing bounds must be
 * live/non-null; no READY/FP admission, World access or allocation occurs.
 * Stores current coordinates and mutates those same bounds in original order.
 * Caller owns reached native/source guards and leaves previous fields alone. */
void mc_entity_source_position(mc_entity *entity, double x, double y, double z);

#endif
