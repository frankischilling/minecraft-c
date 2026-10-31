#ifndef MC_ENTITY_CLASSES_CLASSES_H
#define MC_ENTITY_CLASSES_CLASSES_H

#include "mc/entity/errors.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct mc_entity_classes mc_entity_classes;
typedef struct mc_entity_class_literal mc_entity_class_literal;

typedef enum mc_entity_source { MC_ENTITY_CLIENT = 0, MC_ENTITY_SERVER } mc_entity_source;

typedef enum mc_entity_class_phase {
    MC_ENTITY_CLASS_COLD = 0,
    MC_ENTITY_CLASS_INITIALIZING,
    MC_ENTITY_CLASS_READY,
    MC_ENTITY_CLASS_FAILED
} mc_entity_class_phase;

typedef struct mc_entity_limits {
    size_t max_allocation_bytes, max_live_entities;
} mc_entity_limits;

#define MC_ENTITY_DEFAULT_ALLOCATION_BYTES (64u * 1024u * 1024u)

typedef struct mc_entity_class_snapshot {
    mc_entity_source source;
    mc_entity_class_phase entity_class, bounds_class, watcher_class;
    int32_t next_id, bounds_pool_cursor;
    const void *bounds_pool_identity, *watcher_types_identity;
    size_t watcher_type_count, live_entities, allocation_bytes;
    size_t live_path_arenas;
} mc_entity_class_snapshot;

/* The first 24 native literal selectors retain jc/ga registration order;
 * query-only Entity and Player follow Boat, then client-only LocalPlayer.
 * These are not runtime instance
 * IDs, serialized numerical IDs, constructor availability or class phases. */
typedef enum mc_entity_class_kind {
    MC_ENTITY_CLASS_KIND_ARROW,
    MC_ENTITY_CLASS_KIND_SNOWBALL,
    MC_ENTITY_CLASS_KIND_ITEM,
    MC_ENTITY_CLASS_KIND_PAINTING,
    MC_ENTITY_CLASS_KIND_LIVING_ENTITY,
    MC_ENTITY_CLASS_KIND_MONSTER,
    MC_ENTITY_CLASS_KIND_CREEPER,
    MC_ENTITY_CLASS_KIND_SKELETON,
    MC_ENTITY_CLASS_KIND_SPIDER,
    MC_ENTITY_CLASS_KIND_GIANT,
    MC_ENTITY_CLASS_KIND_ZOMBIE,
    MC_ENTITY_CLASS_KIND_SLIME,
    MC_ENTITY_CLASS_KIND_GHAST,
    MC_ENTITY_CLASS_KIND_PIG_ZOMBIE,
    MC_ENTITY_CLASS_KIND_PIG,
    MC_ENTITY_CLASS_KIND_SHEEP,
    MC_ENTITY_CLASS_KIND_COW,
    MC_ENTITY_CLASS_KIND_CHICKEN,
    MC_ENTITY_CLASS_KIND_SQUID,
    MC_ENTITY_CLASS_KIND_WOLF,
    MC_ENTITY_CLASS_KIND_PRIMED_TNT,
    MC_ENTITY_CLASS_KIND_FALLING_SAND,
    MC_ENTITY_CLASS_KIND_MINECART,
    MC_ENTITY_CLASS_KIND_BOAT,
    MC_ENTITY_CLASS_KIND_ENTITY,
    MC_ENTITY_CLASS_KIND_PLAYER,
    MC_ENTITY_CLASS_KIND_LOCAL_PLAYER,    /* Client dc; existing selectors stay stable. */
    MC_ENTITY_CLASS_KIND_PARTICLE,        /* Client xw, query-only ancestor. */
    MC_ENTITY_CLASS_KIND_PICKUP_PARTICLE, /* Client em. */
    MC_ENTITY_CLASS_KIND_VV,              /* Actual client vv, query-only. */
    MC_ENTITY_CLASS_KIND_CF,              /* Actual client cf, query-only. */
    MC_ENTITY_CLASS_KIND_LX,              /* Actual client lx, query-only. */
    MC_ENTITY_CLASS_KIND_C,               /* Actual client Lightning c; yu is omitted ancestor. */
    MC_ENTITY_CLASS_KIND_COUNT
} mc_entity_class_kind;

typedef struct mc_entity_class_literal_view {
    const mc_entity_classes *owner;
    mc_entity_source source;
    mc_entity_class_kind kind;
    /* Diagnostic obfuscated names only, never source String identities. */
    const char *client_name, *server_name;
} mc_entity_class_literal_view;

/* One game-class owner spans worlds/subtypes and borrows the same existing
 * application/library Random profile as those worlds and registries. It never
 * creates/resets/frees that profile. Source distinguishes actual game-class
 * identities; separate owners also denote separate loaded classes.
 *
 * limits==NULL selects 64MiB and SIZE_MAX live entities. The ledger charges all
 * direct class/entity/bounds/Random/watcher/map/record requests before malloc at
 * their reached point. External stack/world/profile owners, allocator overhead
 * and C stack are excluded. Failure preserves out. All access is exclusive;
 * the profile must outlive every dependent class/entity and concurrent user.
 * create alone performs no game-class initialization or entropy draw. The 33
 * passive canonical literal cells are charged in this actual native header. */
mc_entity_status mc_entity_classes_create(mc_entity_source source, mc_java_random_profile *profile,
                                          const mc_entity_limits *limits, mc_entity_classes **out);
mc_entity_status mc_entity_classes_get_snapshot(const mc_entity_classes *classes,
                                                mc_entity_class_snapshot *out);
/* Native application bindings retain this exact class owner without active use.
 * Side/profile must match. Overflow or mismatch leaves the owner unchanged.
 * release balances one binding; it does not free or reset class state. */
mc_entity_status mc_entity_classes_retain(mc_entity_classes *classes, mc_entity_source source,
                                          mc_java_random_profile *profile);
mc_entity_status mc_entity_classes_release(mc_entity_classes *classes);
mc_entity_status mc_entity_classes_free(mc_entity_classes *classes);

/* Actual immutable Class identity, borrowed from this exact live owner. These
 * calls do not initialize Entity/EntityList/target classes, allocate an Entity,
 * change an ID/counter, use a World, or consume/reset Random/profile state.
 * Identity equality is pointer equality, never name/profile/kind equality
 * between different owners. The diagnostic view has no runtime String owner.
 * Hold classes (or its counted application binding) throughout every borrowed
 * literal/view use. Free is BUSY while borrowers, live Entities or native
 * pooled or standalone bounds holds, or Path arenas remain.
 * Failure preserves out; arguments/outputs and owners must be disjoint. */
mc_entity_status mc_entity_class_literal_get(const mc_entity_classes *classes,
                                             mc_entity_class_kind kind,
                                             const mc_entity_class_literal **out);
mc_entity_status mc_entity_class_literal_inspect(const mc_entity_class_literal *literal,
                                                 mc_entity_class_literal_view *out);

/* Passive Class.isAssignableFrom for the admitted Entity Class namespace.
 * Both arguments must be canonical literals borrowed from live owners. The
 * candidate matches itself and its admitted ancestors in that same owner;
 * a valid literal from a different owner is an ordinary false result. Omitted
 * intermediate superclasses are represented by the fixed admitted parent graph.
 * This does not initialize classes, allocate, consume Random or inspect an
 * Entity/World. NULL or noncanonical arguments are INVALID_ARGUMENT and
 * preserve out. Hold both owners throughout the call; out must be disjoint. */
mc_entity_status
mc_entity_class_literal_is_assignable_from(const mc_entity_class_literal *target,
                                           const mc_entity_class_literal *candidate, bool *out);

#endif
