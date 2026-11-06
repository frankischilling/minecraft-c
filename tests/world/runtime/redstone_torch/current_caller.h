#ifndef DB_CURRENT_CALLER_H
#define DB_CURRENT_CALLER_H

#include "block/registry/redstone_set_internal.h"
#include "block/registry/redstone_torch_list_internal.h"
#include "mc/entity/entity.h"
#include "mc/resources/files/files.h"
#include "mc/world/chunk/nibbles.h"
#include "mc/world/runtime/lighting.h"
#include "mc/world/runtime/scheduling.h"
#include "mc/world/runtime/world_access.h"
#include <stdio.h>

enum {
    DB_WORLDS = 6,
    DB_CHUNKS = 4,
    DB_PLANES = 5,
    DB_NODES = 256,
    DB_EVENTS = 4096,
    DB_BUCKETS = 1024,
    DB_ARRAY_BYTES = MC_CHUNK_BLOCKS + 3 * MC_CHUNK_NIBBLE_BYTES + MC_CHUNK_HEIGHTMAP_BYTES
};

enum {
    DB_WORLD,
    DB_ENTRY,
    DB_RECEIVER,
    DB_X,
    DB_Y,
    DB_Z,
    DB_METADATA,
    DB_CENTER_ID,
    DB_LAYOUT,
    DB_REMOTE,
    DB_NULL_WORLD,
    DB_RANDOM_MODE,
    DB_RECORD,
    DB_NEIGHBOR_ID,
    DB_OBSERVER_MODE,
    DB_OPERANDS
};

typedef struct db_profile {
    char name[128];
    int32_t values[DB_OPERANDS];
} db_profile;
typedef struct db_world db_world;
typedef struct db_fixture db_fixture;

typedef struct db_math_state {
    bool observed;
    uint64_t seed, gaussian;
    bool have;
} db_math_state;

typedef enum db_event_kind { DB_CELL, DB_RANGE, DB_SOUND, DB_PARTICLE, DB_ITEM } db_event_kind;

typedef struct db_event {
    db_event_kind kind;
    size_t world_role;
    int32_t integers[6];
    bool cell_captured;
    double position[3], motion[3];
    float volume, pitch;
    /* Names are actual borrowed Registry literals; fixture Registry stays live. */
    const mc_registry_string *name;
    mc_entity *entity;
    mc_entity_snapshot item;
    mc_item_stack_fields stack;
    mc_java_random world_random, independent_random, entity_random;
    uint64_t uniquifier;
    db_math_state math;
} db_event;

struct db_world {
    db_fixture *fixture;
    size_t role;
    mc_world_regions *regions;
    mc_world_store *store;
    mc_world_provider *dimension;
    mc_world_runtime *world;
    mc_chunk_state *chunks[DB_CHUNKS];
    mc_world_access *access;
    int64_t saved_time;
    uint64_t constructor_uniquifier_before, constructor_uniquifier_after;
    db_math_state constructor_math_before, constructor_math_after;
    size_t terminal_chunk_count;
};

struct db_fixture {
    mc_resource_files *files;
    mc_materials *materials;
    mc_java_random_profile *profile;
    mc_entity_classes *entities;
    mc_registry_classes *registry;
    mc_world_runtime_classes *world_classes;
    mc_world_info_name *name;
    mc_math_trig trig;
    /* This is the one declared ordinary Random(31) argument, never World.r. */
    mc_java_random independent;
    size_t world_count, active_role, event_count;
    db_world worlds[DB_WORLDS];
    db_event events[DB_EVENTS];
    int32_t observer_mode;
    bool observer_refused, observer_failed, item_seen, opened_complete;
    const mc_block *receivers[2], *redstone;
    const void *history_owner;
};

typedef struct db_scheduled_inventory {
    const void *owner;
    const mc_scheduled_tick *records[DB_NODES];
    mc_scheduled_tick_fields fields[DB_NODES];
    size_t count;
} db_scheduled_inventory;

typedef struct db_world_snapshot {
    mc_world_runtime *identity;
    int64_t time;
    bool a, remote, o, s;
    int32_t M;
    const mc_java_random *random_owner;
    mc_java_random random;
    const uint8_t *array_owners[DB_CHUNKS][DB_PLANES];
    uint8_t *array_bytes;
    const void *light_owner;
    size_t light_capacity, light_count;
    const mc_world_light_region *light_records[DB_NODES];
    int32_t light_regions[DB_NODES][7];
    db_scheduled_inventory ordered, membership;
    const void *entity_list_owner;
    size_t entity_count, entity_capacity;
    const mc_entity *entities[DB_NODES];
    mc_entity_snapshot entity_fields[DB_NODES];
    mc_item_stack_fields entity_stacks[DB_NODES];
} db_world_snapshot;

typedef struct db_snapshot {
    size_t world_count;
    db_world_snapshot worlds[DB_WORLDS];
    mc_registry_redstone_torch_snapshot *history;
    mc_registry_redstone_torch_list_view history_view;
    mc_registry_redstone_torch_record_view history_slots[DB_NODES];
    const void *history_owner, *position_set_owner;
    mc_registry_redstone_set_view position_set;
    mc_registry_redstone_node_view position_nodes[DB_NODES];
    bool position_output;
    mc_world_schedule_class_state sequence;
    mc_java_random independent;
    uint64_t uniquifier;
    db_math_state math;
} db_snapshot;

typedef struct db_outcome {
    bool entered, returned, value, observer_refused;
    /* Native callback refusal is not a Java ISE claim. */
    bool source_null, native_failure;
    mc_world_mutation_error *error;
} db_outcome;

/* Shared genuine class owners; one or six genuine saved Worlds. Does not copy
 * templates: runner must provide fresh, byte-checked mutable directories. */
bool db_fixture_open(db_fixture *, const char *resources, const char *world_root, bool aging);
bool db_fixture_close(db_fixture *);
/* Strict name+15-operand reader; no roster synthesis or reordering. */
bool db_profile_read(FILE *, db_profile *, bool *eof);
/* Only the declared raw b/e arrays and World.B are authored. Other fields,
 * arrays, List/Set/queues, phase/time and Random owners remain untouched. */
bool db_prepare(db_fixture *, const db_profile *);
/* out starts zero and owns no previous snapshot. A partial failure must also
 * be retired. All passive arrays/records own copies or explicit native holds until retire.
 * These are CURRENT physical observations, without invented JDK modCounts. */
bool db_snapshot_take(db_fixture *, db_snapshot *);
void db_snapshot_retire(db_snapshot *);
/* Existing virtual block dispatch for0..3; exact private DB bodies for4/5.
 * Uses real World call and Registry holds; no successful backend substitute. */
bool db_invoke(db_fixture *, const db_profile *, db_outcome *);
void db_outcome_retire(db_outcome *);

#endif
