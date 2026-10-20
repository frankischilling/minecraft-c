#ifndef MC_ENTITY_ENTITY_H
#define MC_ENTITY_ENTITY_H

#include "mc/core/java/random_profile.h"
#include "mc/entity/classes/classes.h"
#include "mc/entity/errors.h"
#include "mc/item/stack/stack.h"
#include "mc/world/runtime/runtime.h"

typedef struct mc_entity mc_entity;
typedef struct mc_entity_watcher mc_entity_watcher;
typedef struct mc_entity_watch_record mc_entity_watch_record;

typedef enum mc_entity_phase {
    MC_ENTITY_RAW = 0,
    MC_ENTITY_CONSTRUCTING,
    MC_ENTITY_READY,
    MC_ENTITY_FAILED_CONSTRUCTION
} mc_entity_phase;

/* Field selectors use client sn labels where no semantic name is established.
 * The server mapping and five client-only fields are documented separately.
 * Reference fields stay null unless assigned by construction. Semantic NBT
 * read updates only its documented scalar fields and concrete Item Stack.
 * No generic String, relationship or watcher Object API is
 * implied. Nested base/item/falling records preserve shadowed names and types. */
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

typedef struct mc_falling_entity_fields {
    int32_t a, b;
} mc_falling_entity_fields;

typedef struct mc_entity_bounds {
    double min_x, min_y, min_z, max_x, max_y, max_z;
} mc_entity_bounds;

typedef struct mc_entity_snapshot {
    mc_entity_source source;
    const mc_entity_class_literal *class_literal; /* Immutable same-owner identity. */
    mc_entity_phase phase;
    uint16_t base_bytecode, item_bytecode, falling_bytecode; /* Last completed store/call/return. */
    mc_entity_base_fields base;
    mc_item_entity_fields item;
    mc_falling_entity_fields falling;
    const mc_entity_bounds *bounds;
    const mc_java_random *random;
    const mc_entity_watcher *watcher;
    mc_item_stack *stack; /* Borrowed same identity, possibly null. */
} mc_entity_snapshot;

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

/* Allocate RAW before evaluating constructor arguments. First active use runs
 * the actual Entity counter initializer; allocation does not consume an ID,
 * create the later children or draw entropy. One native hold is published.
 * Only RAW can construct. Native argument/binding rejection preserves RAW;
 * reached failure permanently retains FAILED_CONSTRUCTION and its field/class/
 * ID/entropy prefix. No rollback or retry occurs. AABB and watcher class failure
 * latch separately, while ordinary instance failure does not poison classes.
 * Constructor FP arithmetic requires the java.h strict environment.
 *
 * The allocator fixes the actual canonical class literal from this classes
 * owner; construction cannot change it. Each constructor requires its matching
 * concrete kind. Base equality continues to compare loaded base ownership/IDs.
 * Only the subtype record selected by class_literal represents source fields;
 * the other subtype record remains zero native storage.
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

/* Actual ju/gl constructors, with the same RAW/failed-prefix/ownership and
 * strict FP environment contract as the Item constructors above. They call the
 * base constructor once and never run Item initialization, retain a Stack or
 * draw Math entropy. The world-only overload stores falling.b=0, then returns
 * with the base size/bounds unchanged. The full overload stores falling.a=id,
 * enables prevent-spawn collision, sets size to 0.98f, derives offset from the
 * actual height/2, positions the same bounds, zeroes velocities, then stores
 * the supplied coordinates into the previous-position fields. No insertion,
 * falling tick, collision handling or subtype NBT behavior is implied. */
mc_entity_status mc_falling_entity_allocate(mc_entity_classes *classes, mc_entity **out,
                                            mc_entity_error *error);
mc_entity_status mc_falling_entity_construct_world(mc_entity *raw, mc_world_runtime *world,
                                                   mc_entity_error *error);
mc_entity_status mc_falling_entity_construct(mc_entity *raw, mc_world_runtime *world, double x,
                                             double y, double z, int32_t id,
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
 * a distinct loaded Entity classes owner never compares equal. Concrete
 * subtype identity adds no equality gate. IDs wrap as Java int. */
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
