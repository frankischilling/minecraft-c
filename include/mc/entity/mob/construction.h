#ifndef MC_ENTITY_MOB_CONSTRUCTION_H
#define MC_ENTITY_MOB_CONSTRUCTION_H
#include "mc/entity/player/player.h"
#include "mc/item/registry/runtime.h"

typedef struct mc_path mc_path;

typedef enum mc_mob_status {
    MC_MOB_OK,
    MC_MOB_ARGUMENT,
    MC_MOB_PHASE,
    MC_MOB_BUSY,
    MC_MOB_LIMIT,
    MC_MOB_ALLOCATION,
    MC_MOB_CLASS_ERRONEOUS,
    MC_MOB_ENTITY_FAILED,
    MC_MOB_REGISTRY_FAILED,
    MC_MOB_STACK_FAILED,
    MC_MOB_UNSUPPORTED_FP,
    MC_MOB_SOURCE_NULL
} mc_mob_status;

typedef struct mc_mob_site {
    const char *class_name, *method, *descriptor;
    int32_t pc;
} mc_mob_site;

typedef struct mc_mob_class_cause {
    mc_mob_status status;
    const char *operation;
    mc_mob_site site, child;
    bool native_policy, source_exception;
    const char *exception_class;
    mc_entity_error entity;
    mc_registry_error registry;
    mc_item_stack_status stack;
} mc_mob_class_cause;

typedef struct mc_mob_error {
    mc_mob_status status;
    const char *operation;
    mc_mob_site site, child;
    bool native_policy, source_exception;
    const char *exception_class;
    mc_entity_error entity;
    mc_registry_error registry;
    mc_item_stack_status stack;
    mc_mob_class_cause first_class_failure;
} mc_mob_error;

typedef struct mc_mob_chicken_fields {
    bool a;
    float b, c, f, g, h;
    int32_t i;
} mc_mob_chicken_fields;

typedef struct mc_mob_wolf_fields {
    bool a;
    float b, c;
    bool f, g;
    float h, i;
} mc_mob_wolf_fields;

typedef struct mc_mob_squid_fields {
    float a, b, c, f, g, h, i, j, k, l, m, n, o, p;
} mc_mob_squid_fields;

/* Actual DL.a outer array and its independently allocated float[3] rows.
 * Inspection borrows these identities; callers must not modify them. */
typedef struct mc_mob_sheep_color_row {
    size_t length;
    float values[3];
} mc_mob_sheep_color_row;

typedef struct mc_mob_sheep_colors {
    size_t length;
    mc_mob_sheep_color_row *rows[16];
} mc_mob_sheep_colors;

typedef struct mc_mob_state {
    mc_entity_class_kind kind;
    mc_entity_phase phase;
    mc_living_entity_fields living;
    /* Actual ii fields; no path or target is manufactured by construction. */
    const mc_path *creature_path;
    mc_entity *creature_target;
    bool creature_attacking;
    int32_t monster_damage, creeper_a, creeper_b, slime_jump_delay;
    float slime_a, slime_b;
    mc_mob_chicken_fields chicken;
    mc_mob_wolf_fields wolf;
    mc_mob_squid_fields squid;
    unsigned int living_pc, creature_pc, monster_pc, constructor_pc, watcher_hook_pc;
    unsigned int animal_pc, water_creature_pc;
    bool watcher_16_present, watcher_17_present;
    int8_t watcher_16, watcher_17;
    bool watcher_16_dirty, watcher_17_dirty, watcher_dirty;
    bool watcher_18_present, watcher_18_dirty;
    int32_t watcher_16_type, watcher_17_type, watcher_18_type;
    const mc_player_string *watcher_17_string;
    int32_t watcher_18_integer;
    const mc_mob_error *failure;
} mc_mob_state;

typedef struct mc_mob_class_state {
    mc_entity_class_phase skeleton_phase;
    unsigned int skeleton_pc;
    const mc_item_stack *skeleton_a, *skeleton_candidate;
    size_t allocation_bytes, skeleton_stack_bytes;
    const mc_mob_error *failure;
    mc_entity_class_phase sheep_phase;
    unsigned int sheep_pc;
    const mc_mob_sheep_colors *sheep_a, *sheep_candidate;
    const mc_mob_sheep_color_row *sheep_row_candidate;
    size_t sheep_rows_published, sheep_array_bytes;
    const mc_mob_error *sheep_failure;
} mc_mob_class_state;

/* Genuine client CN/UZ/FR/GB/UW/WH/DL/BX/WW/GI/XT constructors. The literal must be the SAME
 * Entity namespace's canonical concrete Class. FR initializes its actual static
 * Bow Stack before Entity allocation; Registry is required only on that first
 * active use and is borrowed throughout the call, never retained as a cycle.
 * Same Entity limits account for concrete headers, inherited allocations and
 * retained Skeleton and Sheep initializer storage. Outputs are unchanged on failure. */
mc_mob_status mc_mob_allocate(mc_entity_classes *, mc_registry_classes *nullable_registry,
                              const mc_entity_class_literal *, mc_entity **, mc_mob_error *);
/* A one-shot RAW -> CONSTRUCTING -> READY/FAILED source operation. Nullable
 * World is source-supported. Nonnull World uses the exact Entity/profile owner.
 * Every failed prefix remains owned and inspectable; construction is not replayed.
 * The actual SN virtual watcher hook runs before LS initialization. */
mc_mob_status mc_mob_construct(mc_entity *, mc_world_runtime *nullable_world, mc_mob_error *);
bool mc_mob_inspect(const mc_entity *, mc_mob_state *);
bool mc_mob_classes_inspect(const mc_entity_classes *, mc_mob_class_state *);
/* Actual UW.e(int): typed Byte16 assignment, resize, health and explicit
 * position in source order. The caller owns a constructed genuine Slime.
 * This operation draws no entropy and leaves previous pose fields alone. */
mc_mob_status mc_mob_slime_set_size(mc_entity *, int32_t size, mc_mob_error *);
/* Actual DL.e_(int): preserve Byte16's high bits and replace the low color
 * nibble, with UD.b equality/dirty effects. Errors belong to this operation. */
mc_mob_status mc_mob_sheep_set_color(mc_entity *, int32_t color, mc_mob_error *);
/* Ordinary mc_entity_retain/release also apply to every Mob phase. This strict
 * terminal operation consumes only a final inactive occurrence. BUSY preserves
 * the slot. Static Skeleton and Sheep storage belongs to Entity classes until
 * their free. */
mc_mob_status mc_mob_retire(mc_entity **, mc_mob_error *);
#endif
