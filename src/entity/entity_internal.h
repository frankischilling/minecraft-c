#ifndef MC_ENTITY_ENTITY_INTERNAL_H
#define MC_ENTITY_ENTITY_INTERNAL_H

#include "classes/classes_internal.h"
#include "mc/entity/entity.h"
#include "mc/storage/nbt/tree.h"

/* Actual production layouts, shared only with native invariant tests. */
struct mc_entity_watch_record {
    int32_t index, type;
    mc_entity_watch_payload_kind payload_kind;
    int8_t byte_value;
    int32_t integer_value;
    const struct mc_player_string *string_value;
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

struct mc_minecart_entity_state;
struct mc_local_player_state;
struct mc_particle_state;
struct mc_mob_entity_state;
struct mc_lightning_entity_state;

struct mc_entity {
    mc_entity_classes *classes;
    const mc_entity_class_literal *class_literal;
    size_t holds, direct_bytes;
    mc_entity_phase phase;
    uint16_t base_bytecode, item_bytecode, falling_bytecode;
    mc_entity_base_fields base;
    mc_item_entity_fields item;
    mc_falling_entity_fields falling;
    mc_entity_bounds *bounds;
    mc_java_random *random;
    mc_entity_watcher *watcher;
    mc_item_stack *stack;
    /* Independent native storage provenance, not a source field. This tree's
     * allocation is outside the direct constructor ledger; the pointer cell
     * remains part of the actual Entity header. Its lifetime follows Entity. */
    mc_nbt_tree *storage_source;
    /* Native occurrences backing source aH/aG. Authored borrowed relationships
     * may have NULL holds and require an independently live caller scope. */
    struct mc_entity *mount_hold, *passenger_hold;
    /* A real embedded subobject of the Minecart native allocation, else NULL. */
    struct mc_minecart_entity_state *minecart;
    /* Actual embedded subobject of a dc allocation, never an invented Player. */
    struct mc_local_player_state *local_player;
    /* Actual XW subobject of a complete concrete particle allocation, else NULL. */
    struct mc_particle_state *particle;
    /* Actual embedded LS/II/GZ/BG/AR/leaf storage of a concrete client Mob. */
    struct mc_mob_entity_state *mob;
    /* Actual C/YU subobject of a genuine Lightning allocation, else NULL. */
    struct mc_lightning_entity_state *lightning;
};

/* Passive actual subtype admission, valid in every phase. The immutable
 * literal must be this Entity classes owner's canonical literal for kind. */
bool mc_entity_is_kind(const mc_entity *entity, mc_entity_class_kind kind);
/* Complete concrete XW storage, excluding the query-only XW ancestor. These
 * passive predicates deliberately admit RAW/FAILED storage for retirement;
 * callers keep their own reached READY/active-call checks. */
bool mc_entity_is_particle_kind(mc_entity_class_kind);
bool mc_entity_has_particle(const mc_entity *);
/* EM has its separate actor-reference tick and remains outside movement. */
bool mc_entity_has_moving_particle(const mc_entity *);

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

mc_entity_status mc_entity_mount_vehicle_store(mc_entity *, mc_entity *);
mc_entity_status mc_entity_mount_passenger_store(mc_entity *, mc_entity *);
void mc_entity_mount_references_dispose(mc_entity *);

/* Trusted World joint-commit pin only, after fresh canonical graph validation
 * and a checked holds < SIZE_MAX for every reached Entity. This consumes no
 * Source admission right and can pin a natively sealed Player. The same graph
 * stays exclusive until all temporary occurrences are released. */
void mc_entity_terminal_pin(mc_entity *);

#endif
