#ifndef MC_ENTITY_ARROW_CONSTRUCTION_H
#define MC_ENTITY_ARROW_CONSTRUCTION_H

#include "mc/core/math/classes.h"
#include "mc/entity/entity.h"

typedef struct mc_arrow_fields {
    bool a;
    int32_t b;
    mc_entity *c;
    int32_t d, e, f, g, h;
    bool i;
    int32_t j, k;
} mc_arrow_fields;

typedef struct mc_arrow_site {
    const char *class_name, *method, *descriptor;
    int32_t pc;
} mc_arrow_site;

typedef struct mc_arrow_position_error {
    mc_entity_status status;
    const char *operation;
    unsigned int bytecode, position_bytecode;
    bool source_null, native_policy;
} mc_arrow_position_error;

/* Value observations only. No diagnostic owns an Arrow, shooter, World or
 * deferred cleanup. Source fields and all reached constructor prefixes belong
 * to the actual Entity even with NULL error output. On success site names the
 * actual return; heading observations describe only this invocation. */
typedef struct mc_arrow_error {
    mc_entity_status status;
    const char *operation, *exception_class;
    bool native_policy, source_exception;
    mc_arrow_site site, child, heading_site;
    mc_entity_error entity;
    mc_math_class_error math;
    mc_arrow_position_error pose, position;
    bool heading_entered, heading_returned;
    unsigned int gaussian_calls;
    /* Copied from each already-read actual Gaussian receiver and return.
     * Absent before the first reached draw (including null Random43). These
     * observations neither draw entropy nor seed/replace any source stream. */
    bool random_captured;
    mc_java_random random_before, random_after;
    double gaussian_values[3];
} mc_arrow_error;

typedef struct mc_arrow_state {
    mc_entity_phase phase;
    mc_arrow_fields fields;
    unsigned int constructor_pc, heading_pc;
    bool constructor_active, heading_active;
    const mc_arrow_error *failure;
} mc_arrow_state;

/* Genuine client SL extends SN, not a Living or Mob layout. Allocate RAW
 * before evaluating constructor arguments. Base Class active use precedes the
 * actual accounted header allocation; no ID, entropy or shooter hold is used. */
mc_entity_status mc_arrow_allocate(mc_entity_classes *, mc_entity **, mc_entity_error *);

/* One-shot RAW -> CONSTRUCTING -> READY/FAILED. Preserve every reached ID,
 * field, bounds, watcher, entropy and World/shooter occurrence on failure.
 * NULL World is supported; a nonnull World uses the same Random profile.
 * World-only changes size without rebuilding base bounds. Coordinate adds the
 * actual position child. Neither enters Math or heading. Living stores the
 * nullable shooter at52, flag60 and size68 before its first null read73; its
 * first Math active use is116, after the actual SN.c pose/history stores.
 * No publication, tick, NBT or rendering is supplied by these constructors. */
mc_entity_status mc_arrow_construct_world(mc_entity *, mc_world_runtime *, mc_arrow_error *);
mc_entity_status mc_arrow_construct_position(mc_entity *, mc_world_runtime *, double x, double y,
                                             double z, mc_arrow_error *);
mc_entity_status mc_arrow_construct_living(mc_entity *, mc_world_runtime *,
                                           mc_entity *nullable_living,
                                           mc_math_classes *nullable_math, mc_arrow_error *);

/* Actual SL.a(DDDFF)V on an independently held READY Arrow. Native active-call
 * admission precedes source work; the constructor uses the same entered body.
 * Float sqrt normalization, all three own-Random Gaussian calls, velocity and
 * angle stores run even for zero/NaN/infinite inputs. No reseed or finite clamp.
 * A failed call preserves completed fields and Random/cache, never replays it.
 * Caller keeps owners/arguments/Math live and exclusive through the whole call;
 * outputs, errors, inputs and owners are mutually disjoint. */
mc_entity_status mc_arrow_heading(mc_entity *, mc_math_classes *, double x, double y, double z,
                                  float speed, float spread, mc_arrow_error *);
bool mc_arrow_inspect(const mc_entity *, mc_arrow_state *);
/* Snapshots borrow source identities and failure from the same live Arrow;
 * copied values acquire no hold and expire on corresponding mutation/release. */

#endif
