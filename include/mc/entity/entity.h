#ifndef MC_ENTITY_ENTITY_H
#define MC_ENTITY_ENTITY_H

#include "mc/core/java/random_profile.h"
#include "mc/item/stack/stack.h"
#include "mc/world/runtime/runtime.h"

typedef struct mc_entity_classes mc_entity_classes;
typedef struct mc_entity mc_entity;
typedef struct mc_entity_watcher mc_entity_watcher;
typedef struct mc_entity_watch_record mc_entity_watch_record;

typedef enum mc_entity_source { MC_ENTITY_CLIENT = 0, MC_ENTITY_SERVER } mc_entity_source;
typedef enum mc_entity_phase {
    MC_ENTITY_RAW = 0,
    MC_ENTITY_CONSTRUCTING,
    MC_ENTITY_READY,
    MC_ENTITY_FAILED_CONSTRUCTION
} mc_entity_phase;
typedef enum mc_entity_class_phase {
    MC_ENTITY_CLASS_COLD = 0,
    MC_ENTITY_CLASS_INITIALIZING,
    MC_ENTITY_CLASS_READY,
    MC_ENTITY_CLASS_FAILED
} mc_entity_class_phase;
typedef enum mc_entity_status {
    MC_ENTITY_OK = 0,
    MC_ENTITY_INVALID_ARGUMENT,
    MC_ENTITY_INVALID_PHASE,
    MC_ENTITY_LIMIT,
    MC_ENTITY_ALLOCATION_FAILED,
    MC_ENTITY_PROFILE_FAILED,
    MC_ENTITY_WORLD_FAILED,
    MC_ENTITY_ITEM_FAILED,
    MC_ENTITY_CLASS_FAILED_STATUS,
    MC_ENTITY_BUSY,
    MC_ENTITY_STATE_ERROR,
    MC_ENTITY_UNSUPPORTED_FP
} mc_entity_status;

typedef struct mc_entity_error {
    mc_entity_status status;
    const char *operation; /* Static diagnostic naming the reached constructor. */
    uint16_t bytecode;
    mc_java_random_profile_status profile;
    mc_item_stack_status item;
    mc_world_storage_status world;
} mc_entity_error;

typedef struct mc_entity_limits {
    size_t max_allocation_bytes, max_live_entities;
} mc_entity_limits;
#define MC_ENTITY_DEFAULT_ALLOCATION_BYTES (64u * 1024u * 1024u)

/* Field selectors use client sn labels where no semantic name is established.
 * The server mapping and five client-only fields are documented separately.
 * Reference fields in this constructor-only record stay null unless assigned
 * by construction. No generic String, relationship or watcher Object API is
 * implied. Nested base/item records preserve shadowed names and their types. */
typedef struct mc_entity_base_fields {
    int32_t id;
    double aE;
    bool aF;
    mc_entity *aG, *aH;
    mc_world_runtime *world;
    double aJ, aK, aL;
    double x, y, z;
    double velocity_x, velocity_y, velocity_z;
    float yaw, aT, aU, aV;
    bool aX, aY, aZ, ba, bb, bc, bd, be;
    float offset, width, height, bi, bj, bk;
    int32_t private_b;
    double bl, bm, bn;
    float y_size, bp;
    bool bq;
    float br;
    int32_t bt, bu, bv, bw;
    bool bx;
    int32_t by, bz;
    bool private_c, bC;
    float bE;
    double private_d, private_e;
    bool bF;
    int32_t bG, bH, bI;
    bool bM;
    struct {
        const void *bA, *bB;
        int32_t bJ, bK, bL;
    } client_only;
} mc_entity_base_fields;

typedef struct mc_item_entity_fields {
    int32_t b, e, c;
    float d;
    int32_t f;
} mc_item_entity_fields;

typedef struct mc_entity_bounds {
    double min_x, min_y, min_z, max_x, max_y, max_z;
} mc_entity_bounds;

typedef struct mc_entity_snapshot {
    mc_entity_source source;
    mc_entity_phase phase;
    uint16_t base_bytecode, item_bytecode; /* Last completed store/call. */
    mc_entity_base_fields base;
    mc_item_entity_fields item;
    const mc_entity_bounds *bounds;
    const mc_java_random *random;
    const mc_entity_watcher *watcher;
    mc_item_stack *stack; /* Borrowed same identity, possibly null. */
} mc_entity_snapshot;

typedef struct mc_entity_class_snapshot {
    mc_entity_source source;
    mc_entity_class_phase entity_class, bounds_class, watcher_class;
    int32_t next_id, bounds_pool_cursor;
    const void *bounds_pool_identity, *watcher_types_identity;
    size_t watcher_type_count, live_entities, allocation_bytes;
} mc_entity_class_snapshot;

/* Only the reached index0/Byte0 construction record is exposed here. The record
 * has stable identity; the Byte is a typed value, not a per-record Java box.
 * This does not implement generic watcher mutation, boxing or serialization. */
typedef struct mc_entity_watcher_snapshot {
    bool dirty;
    size_t size;
    const mc_entity_watch_record *record;
    int32_t index, type;
    int8_t byte_value;
    bool record_dirty;
} mc_entity_watcher_snapshot;

const char *mc_entity_status_string(mc_entity_status status);

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
 * create alone performs no game-class initialization or entropy draw. */
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

/* Allocate RAW before evaluating constructor arguments. First active use runs
 * the actual Entity counter initializer; allocation does not consume an ID,
 * create the later children or draw entropy. One native hold is published.
 * Only RAW can construct. Native argument/binding rejection preserves RAW;
 * reached failure permanently retains FAILED_CONSTRUCTION and its field/class/
 * ID/entropy prefix. No rollback or retry occurs. AABB and watcher class failure
 * latch separately, while ordinary instance failure does not poison classes.
 *
 * NULL world/stack are accepted. A nonnull world must borrow this same profile;
 * caller keeps it live until its reached counted binding at base bytecode205.
 * Stack storage is retained at item bytecode61 without a READY/field check.
 * The caller keeps the supplied stack live until return. No constructor world
 * callback/insertion, registry lookup, collision or Gaussian draw occurs. */
mc_entity_status mc_item_entity_allocate(mc_entity_classes *classes, mc_entity **out,
                                         mc_entity_error *error);
mc_entity_status mc_item_entity_construct(mc_entity *raw, mc_world_runtime *world, double x,
                                          double y, double z, mc_item_stack *stack,
                                          mc_entity_error *error);
/* The original world-only overload shares the first Math/size/offset prefix,
 * then returns. It leaves the earlier base bounds intact, stack null, yaw and
 * velocities zero; changing size does not itself recompute the bounds. */
mc_entity_status mc_item_entity_construct_world(mc_entity *raw, mc_world_runtime *world,
                                                mc_entity_error *error);
mc_entity_status mc_entity_retain(mc_entity *entity);
void mc_entity_release(mc_entity *entity);
mc_entity_status mc_entity_get_snapshot(const mc_entity *entity, mc_entity_snapshot *out);
mc_entity_status mc_entity_watcher_get_snapshot(const mc_entity_watcher *watcher,
                                                mc_entity_watcher_snapshot *out);
mc_entity_status mc_item_entity_stack_get(mc_entity *entity, mc_item_stack **out);
size_t mc_entity_native_bytes(const mc_entity *entity);

/* READY-only mutations. Each velocity call writes one component so the caller
 * can preserve chest Gaussian/store order. Position updates the same bounds;
 * previous positions/angles remain untouched. No dirty/world/RNG callbacks or
 * coordinate/finite-value clamp is added. FP arithmetic needs the java.h strict
 * environment. Axis is 0,1,2. Equality/hash require READY; right may be NULL and
 * a distinct loaded game class never compares equal. IDs wrap as Java int. */
mc_entity_status mc_entity_set_velocity(mc_entity *entity, unsigned int axis, double value);
mc_entity_status mc_entity_set_position(mc_entity *entity, double x, double y, double z);
mc_entity_status mc_entity_set_size(mc_entity *entity, float width, float height);
bool mc_entity_equals(const mc_entity *left, const mc_entity *right);
int32_t mc_entity_hash_code(const mc_entity *entity);

/* Snapshots/native-byte queries and holds support all entity phases. Every
 * borrowed view requires a live entity/class hold and exclusive access; a view
 * expires on the corresponding mutation/final release. Class free is BUSY while
 * any RAW/READY/FAILED entity remains. release(NULL)/classes_free(NULL) are safe.
 * Results/errors/limits are mutually disjoint from each other and all owners,
 * dependencies and input views. Failure preserves outputs except explicit
 * diagnostics and retained constructor prefixes. No stale-pointer detection,
 * asynchronous cancellation or general Java class-init engine is provided. */

#endif
