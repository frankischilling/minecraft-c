#include "mc/entity/render_distance.h"

#include "entity_internal.h"

#include <emmintrin.h>

#if !defined(_M_X64) && !defined(_M_AMD64) && !defined(__x86_64__)
#error "Source Entity render distance requires x86-64/SSE2"
#endif
#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) ||       \
    (defined(_M_FP_FAST) && _M_FP_FAST)
#error "Source Entity render distance requires strict floating transformations"
#endif

static mc_entity_render_distance_error empty_error(void) {
    return (mc_entity_render_distance_error){
        .pc = -1, .vector_pc = -1, .range_pc = -1, .bounds_pc = -1};
}

static mc_entity_render_distance_status fail(mc_entity_render_distance_error *error,
                                             mc_entity_render_distance_status status,
                                             const char *operation, const char *source,
                                             const char *descriptor, int32_t pc, bool native) {
    error->status = status;
    error->operation = operation;
    error->source_class = source;
    error->source_method = source == NULL ? NULL : "a";
    error->source_descriptor = descriptor;
    error->pc = pc;
    error->native_policy = native;
    return status;
}

static bool range_override(mc_entity_class_kind kind, bool *override) {
    switch (kind) {
        case MC_ENTITY_CLASS_KIND_SNOWBALL:
        case MC_ENTITY_CLASS_KIND_VV:
        case MC_ENTITY_CLASS_KIND_CF:
        case MC_ENTITY_CLASS_KIND_LX:
            *override = true;
            return true;
        case MC_ENTITY_CLASS_KIND_ARROW:
        case MC_ENTITY_CLASS_KIND_ITEM:
        case MC_ENTITY_CLASS_KIND_PAINTING:
        case MC_ENTITY_CLASS_KIND_LIVING_ENTITY:
        case MC_ENTITY_CLASS_KIND_MONSTER:
        case MC_ENTITY_CLASS_KIND_CREEPER:
        case MC_ENTITY_CLASS_KIND_SKELETON:
        case MC_ENTITY_CLASS_KIND_SPIDER:
        case MC_ENTITY_CLASS_KIND_GIANT:
        case MC_ENTITY_CLASS_KIND_ZOMBIE:
        case MC_ENTITY_CLASS_KIND_SLIME:
        case MC_ENTITY_CLASS_KIND_GHAST:
        case MC_ENTITY_CLASS_KIND_PIG_ZOMBIE:
        case MC_ENTITY_CLASS_KIND_PIG:
        case MC_ENTITY_CLASS_KIND_SHEEP:
        case MC_ENTITY_CLASS_KIND_COW:
        case MC_ENTITY_CLASS_KIND_CHICKEN:
        case MC_ENTITY_CLASS_KIND_SQUID:
        case MC_ENTITY_CLASS_KIND_WOLF:
        case MC_ENTITY_CLASS_KIND_PRIMED_TNT:
        case MC_ENTITY_CLASS_KIND_FALLING_SAND:
        case MC_ENTITY_CLASS_KIND_MINECART:
        case MC_ENTITY_CLASS_KIND_BOAT:
        case MC_ENTITY_CLASS_KIND_ENTITY:
        case MC_ENTITY_CLASS_KIND_PLAYER:
        case MC_ENTITY_CLASS_KIND_LOCAL_PLAYER:
        case MC_ENTITY_CLASS_KIND_PARTICLE:
        case MC_ENTITY_CLASS_KIND_PICKUP_PARTICLE:
            *override = false;
            return true;
        default:
            return false;
    }
}

mc_entity_render_distance_status mc_entity_render_distance(mc_entity *entity,
                                                           const mc_entity_vector *eye, bool *out,
                                                           mc_entity_render_distance_error *error) {
    mc_entity_render_distance_error frame = empty_error();
    if (out == NULL) {
        fail(&frame, MC_ENTITY_RENDER_DISTANCE_INVALID_ARGUMENT, "render distance output", NULL,
             NULL, -1, true);
    } else if (entity == NULL) {
        frame.vector_pc = 1;
        fail(&frame, MC_ENTITY_RENDER_DISTANCE_SOURCE_NULL, "actual null SN receiver", "sn",
             "(Lbt;)Z", 1, false);
    } else {
        mc_entity_class_literal_view type;
        frame.entity = mc_entity_class_literal_inspect(entity->class_literal, &type);
        if (frame.entity != MC_ENTITY_OK || type.owner != entity->classes ||
            !mc_entity_is_kind(entity, type.kind)) {
            fail(&frame, MC_ENTITY_RENDER_DISTANCE_ENTITY_FAILED,
                 "render distance canonical Entity class", NULL, NULL, -1, true);
        } else if (type.source != MC_ENTITY_CLIENT) {
            fail(&frame, MC_ENTITY_RENDER_DISTANCE_UNIMPLEMENTED_DEPENDENCY,
                 "render distance requires actual client source class", NULL, NULL, -1, true);
        } else if (type.kind == MC_ENTITY_CLASS_KIND_C) {
            frame.vector_pc = 1;
            fail(&frame, MC_ENTITY_RENDER_DISTANCE_UNIMPLEMENTED_DEPENDENCY,
                 "actual c.b predicate requires its concrete native subobject", "c", "(Lbt;)Z", 1,
                 true);
        } else {
            bool override;
            if (!range_override(type.kind, &override)) {
                fail(&frame, MC_ENTITY_RENDER_DISTANCE_UNIMPLEMENTED_DEPENDENCY,
                     "actual Entity render-distance override", type.client_name, "(Lbt;)Z", 0,
                     true);
            } else {
                const double x = entity->base.x; /* Actual SN field read1 precedes BT read5. */
                if (eye == NULL) {
                    frame.vector_pc = 5;
                    fail(&frame, MC_ENTITY_RENDER_DISTANCE_SOURCE_NULL, "actual null BT receiver",
                         "sn", "(Lbt;)Z", 5, false);
                } else {
                    const double eye_x = eye->x;
                    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u) {
                        frame.vector_pc = 8;
                        fail(&frame, MC_ENTITY_RENDER_DISTANCE_UNSUPPORTED_FP,
                             "SN distance strict floating controls", "sn", "(Lbt;)Z", 8, true);
                    } else {
                        volatile double dx = x - eye_x;               /*8*/
                        volatile double dy = entity->base.y - eye->y; /*18*/
                        volatile double dz = entity->base.z - eye->z; /*29*/
                        volatile double xx = dx * dx, yy = dy * dy;   /*34,39*/
                        volatile double xy = xx + yy, zz = dz * dz;   /*40,45*/
                        volatile double distance = xy + zz;           /*46*/
                        frame.vector_pc = 52; /* Actual virtual a(D) child. */
                        frame.range_pc = 4;
                        if (entity->bounds == NULL) {
                            fail(&frame, MC_ENTITY_RENDER_DISTANCE_SOURCE_NULL,
                                 "actual null EQ receiver", override ? type.client_name : "sn",
                                 "(D)Z", 4, false);
                        } else {
                            const mc_entity_bounds *bounds = entity->bounds;
                            volatile double width = bounds->max_x - bounds->min_x;  /*EQ8*/
                            volatile double height = bounds->max_y - bounds->min_y; /*EQ18*/
                            volatile double depth = bounds->max_z - bounds->min_z;  /*EQ28*/
                            volatile double wh = width + height;                    /*EQ33*/
                            volatile double whd = wh + depth;                       /*EQ36*/
                            volatile double average = whd / 3.0;                    /*EQ40*/
                            volatile double range;
                            if (override) {
                                volatile double four = average * 4.0; /*override10*/
                                range = four * 64.0;                  /*override16*/
                            } else {
                                volatile double scale = 64.0 * entity->base.aE; /*SN16*/
                                range = average * scale;                        /*SN17*/
                            }
                            volatile double square = range * range; /*SN22/override21*/
                            const bool visible =
                                distance < square; /* dcmpg/ifge: unordered false. */
                            *out = visible;
                            frame = empty_error();
                        }
                    }
                }
            }
        }
    }
    if (error != NULL)
        *error = frame;
    return frame.status;
}
