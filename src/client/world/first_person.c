#include "mc/client/world/first_person.h"
#include "../../entity/player/player_internal.h"
#include "../application_access.h"
#include "../application_state.h"
#include "mc/block/registry/runtime.h"
#include "mc/core/java/java.h"
#include "mc/world/runtime/construction.h"
#include <emmintrin.h>
#include <float.h>
#include <math.h>
#include <string.h>

#if !defined(_M_X64) && !defined(_M_AMD64) && !defined(__x86_64__)
#error "First-person camera requires x86-64/SSE2"
#endif
#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) ||       \
    (defined(_M_FP_FAST) && _M_FP_FAST)
#error "First-person camera requires strict floating transformations"
#endif

_Static_assert(sizeof(float) == 4 && sizeof(double) == 8 && FLT_RADIX == 2 && FLT_MANT_DIG == 24 &&
                   DBL_MANT_DIG == 53 && FLT_EVAL_METHOD == 0,
               "Camera binary32/binary64 evaluation profile");

void mc_client_camera_error_init(mc_client_camera_error *e) {
    if (e != NULL)
        memset(e, 0, sizeof(*e));
}

static mc_client_camera_status fail(mc_client_camera_error *e, mc_client_camera_status status,
                                    const char *operation, const char *method, unsigned int pc) {
    e->status = status;
    e->operation = operation;
    e->method = method;
    e->pc = pc;
    return status;
}

/* Explicit stores prevent reassociation, contraction and excess intermediates. */
static float add(float a, float b) {
    volatile float v = a + b;
    return v;
}

static float sub(float a, float b) {
    volatile float v = a - b;
    return v;
}

static float mul(float a, float b) {
    volatile float v = a * b;
    return v;
}

static float divide(float a, float b) {
    volatile float v = a / b;
    return v;
}

static float lerp(float a, float b, float t) {
    return add(a, mul(sub(b, a), t));
}

static double position(double a, double b, float t) {
    volatile double delta = b - a;
    volatile double scaled = delta * (double)t;
    volatile double value = a + scaled;
    return value;
}

static void identity(float m[16]) {
    memset(m, 0, 16u * sizeof(*m));
    m[0] = m[5] = m[10] = m[15] = 1;
}

static void multiply(float m[16], const float right[16]) {
    float result[16];
    for (size_t col = 0; col < 4; ++col)
        for (size_t row = 0; row < 4; ++row) {
            float v = mul(m[row], right[col * 4u]);
            for (size_t k = 1; k < 4; ++k)
                v = add(v, mul(m[k * 4u + row], right[col * 4u + k]));
            result[col * 4u + row] = v;
        }
    memcpy(m, result, sizeof(result));
}

static void record(mc_client_camera_frame *f, bool projection, const char *method, unsigned pc,
                   mc_client_camera_operation_kind kind, float a, float b, float c, float d) {
    mc_client_camera_operation *op = &f->operations[f->operation_count++];
    *op = (mc_client_camera_operation){kind, projection, method, pc, {a, b, c, d}};
    float *m = projection ? f->projection : f->modelview;
    if (kind == MC_CLIENT_CAMERA_TRANSLATE) {
        for (size_t row = 0; row < 4; ++row) {
            float v = mul(m[row], a);
            v = add(v, mul(m[4u + row], b));
            v = add(v, mul(m[8u + row], c));
            m[12u + row] = add(v, m[12u + row]);
        }
    } else if (kind == MC_CLIENT_CAMERA_ROTATE) {
        if (a == 0)
            return;
        /* This selected closure supplies one unit coordinate axis only. */
        const double radians = (double)a * 0.017453292519943295769;
        const float cosine = (float)cos(radians), sine = (float)sin(radians);
        float rotation[16];
        identity(rotation);
        if (b != 0) {
            rotation[5] = rotation[10] = cosine;
            rotation[6] = mul(sine, b);
            rotation[9] = -rotation[6];
        } else if (c != 0) {
            rotation[0] = rotation[10] = cosine;
            rotation[8] = mul(sine, c);
            rotation[2] = -rotation[8];
        } else {
            rotation[0] = rotation[5] = cosine;
            rotation[1] = mul(sine, d);
            rotation[4] = -rotation[1];
        }
        multiply(m, rotation);
    } else {
        /* Actual LWJGL Project.gluPerspective float operation order. */
        const float radians = divide(mul(divide(a, 2), 3.1415927f), 180);
        const float sine = (float)sin((double)radians), delta = sub(d, c);
        if (delta == 0 || sine == 0 || b == 0)
            return; /* Actual GLU early return; no replacement projection. */
        const float cotangent = divide((float)cos((double)radians), sine);
        float perspective[16] = {0};
        perspective[0] = divide(cotangent, b);
        perspective[5] = cotangent;
        perspective[10] = -divide(add(d, c), delta);
        perspective[11] = -1;
        perspective[14] = divide(mul(mul(-2, c), d), delta);
        multiply(m, perspective);
    }
}

#define TRANSLATE(f, p, m, pc, x, y, z) record(f, p, m, pc, MC_CLIENT_CAMERA_TRANSLATE, x, y, z, 0)
#define ROTATE(f, m, pc, angle, x, y, z)                                                           \
    record(f, false, m, pc, MC_CLIENT_CAMERA_ROTATE, angle, x, y, z)

typedef struct camera_inputs {
    const client_application *application;
    const mc_game_settings *settings;
    mc_world_runtime *world;
    int32_t width, height;
    bool live_application;
} camera_inputs;

static bool fp(mc_client_camera_error *e, const char *method, unsigned int pc) {
    if ((_mm_getcsr() & 0xffc0u) == 0x1f80u)
        return true;
    fail(e, MC_CLIENT_CAMERA_UNSUPPORTED_FP, "Camera FP admission", method, pc);
    return false;
}

static bool settings_at(const camera_inputs *in, const char *method, unsigned int app_pc,
                        unsigned int field_pc, mc_game_settings_state *out,
                        mc_client_camera_error *e) {
    if (in->live_application && in->application == NULL) {
        fail(e, MC_CLIENT_CAMERA_NULL_REFERENCE, "Camera Minecraft receiver", method, app_pc);
        return false;
    }
    const mc_game_settings *settings =
        in->live_application ? mc_client_application_live_settings(in->application) : in->settings;
    if (!mc_game_settings_read_state(settings, out)) {
        fail(e, MC_CLIENT_CAMERA_NULL_REFERENCE, "Camera settings receiver", method, field_pc);
        return false;
    }
    return true;
}

static bool world_at(const camera_inputs *in, mc_entity *entity, mc_registry_classes *registry,
                     unsigned int field_pc, mc_world_runtime **out, mc_client_camera_error *e) {
    if (in->live_application && in->application == NULL) {
        fail(e, MC_CLIENT_CAMERA_NULL_REFERENCE, "Camera Minecraft receiver", "g", field_pc);
        return false;
    }
    mc_world_runtime *world =
        in->live_application ? mc_client_application_live_world(in->application) : in->world;
    if (world != NULL) {
        if (mc_world_runtime_registry(world) != registry ||
            mc_world_runtime_random_profile(world) != entity->classes->profile) {
            fail(e, MC_CLIENT_CAMERA_INVALID_ARGUMENT, "Camera World receiver namespace", "g",
                 field_pc);
            return false;
        }
        if (!mc_world_runtime_binding_retain(world)) {
            const mc_world_storage_error *cause = mc_world_runtime_last_error(world);
            if (cause != NULL)
                e->world = *cause;
            fail(e, MC_CLIENT_CAMERA_OWNER_FAILED, "Camera World receiver hold", "g", field_pc);
            return false;
        }
    }
    *out = world; /* Nullable source reference, checked only at its invoke. */
    return true;
}

static bool run(mc_entity *entity, const camera_inputs *in, mc_registry_classes *registry,
                const mc_math_trig *trig, float t, int32_t eye, mc_client_camera_frame *f,
                mc_client_camera_error *e) {
    mc_game_settings_state options;
    if (!settings_at(in, "a", 8, 11, &options, e) || !fp(e, "a", 15))
        return false;
    f->far_distance = (float)mc_java_i32_shr(256, options.values.render_distance);
    if (!settings_at(in, "a", 35, 38, &options, e))
        return false;
    if (options.values.anaglyph) {
        const int32_t signed_eye = mc_java_i32_sub(mc_java_i32_mul(eye, 2), 1);
        TRANSLATE(f, true, "a", 55, mul((float)mc_java_i32_neg(signed_eye), .07f), 0, 0);
    }
    mc_material *water = NULL;
    if (mc_registry_material_get(registry, MC_MATERIAL_g, &water, &e->registry) != MC_REGISTRY_OK) {
        fail(e, MC_CLIENT_CAMERA_REGISTRY_FAILED, "Camera water Material", "d", 12);
        return false;
    }
    if (entity == NULL) {
        fail(e, MC_CLIENT_CAMERA_NULL_REFERENCE, "Camera view Entity receiver", "d", 15);
        return false;
    }
    bool submerged;
    e->entity = mc_entity_inside_material(entity, water, &submerged, &e->material);
    if (e->entity != MC_ENTITY_OK) {
        fail(e,
             e->entity == MC_ENTITY_UNSUPPORTED_FP ? MC_CLIENT_CAMERA_UNSUPPORTED_FP
                                                   : MC_CLIENT_CAMERA_MATERIAL_FAILED,
             "Camera eye immersion", "d", 15);
        return false;
    }
    const mc_entity_base_fields *base = &entity->base;
    const mc_local_player_snapshot *p = &entity->local_player->fields;
    const mc_living_entity_fields *living = &p->living;
    float fov = submerged ? 60.0f : 70.0f;
    if (living->Y <= 0) {
        if (!fp(e, "d", 35))
            return false;
        const float death = add((float)living->ad, t);
        fov = divide(fov, add(mul(sub(1, divide(500, add(death, 500))), 2), 1));
    }
    if (!fp(e, "d", 62))
        return false;
    f->vertical_fov = add(add(fov, 0), mul(sub(0, 0), t));
    /* Real App d/e are loaded after acquiring d(F), not at API entry. */
    f->width = in->live_application ? in->application->display_width : in->width;
    f->height = in->live_application ? in->application->display_height : in->height;
    record(f, true, "a", 160, MC_CLIENT_CAMERA_PERSPECTIVE, f->vertical_fov,
           divide((float)f->width, (float)f->height), .05f, mul(f->far_distance, 2));
    if (!settings_at(in, "a", 176, 179, &options, e))
        return false;
    if (options.values.anaglyph) {
        const int32_t signed_eye = mc_java_i32_sub(mc_java_i32_mul(eye, 2), 1);
        TRANSLATE(f, false, "a", 196, mul((float)signed_eye, .1f), 0, 0);
    }
    float hurt = sub((float)living->aa, t);
    if (living->Y <= 0)
        ROTATE(f, "e", 46, sub(40, divide(8000, add(add((float)living->ad, t), 200))), 0, 0, 1);
    if (!(hurt < 0)) {
        hurt = divide(hurt, (float)living->ab);
        hurt = mc_math_sin(trig, mul(mul(mul(mul(hurt, hurt), hurt), hurt), 3.1415927f));
        ROTATE(f, "e", 90, -living->ac, 0, 1, 0);
        ROTATE(f, "e", 101, mul(-hurt, 14), 0, 0, 1);
        ROTATE(f, "e", 109, living->ac, 0, 1, 0);
    }
    if (!settings_at(in, "a", 208, 211, &options, e))
        return false;
    if (options.values.view_bobbing) {
        const float phase = -add(base->bj, mul(sub(base->bj, base->bi), t));
        const float amplitude = lerp(p->player.h, p->player.i, t);
        const float sway = lerp(living->af, living->ag, t);
        const float angle = mul(phase, 3.1415927f);
        TRANSLATE(f, false, "f", 112, mul(mul(mc_math_sin(trig, angle), amplitude), .5f),
                  -fabsf(mul(mc_math_cos(trig, angle), amplitude)), 0);
        ROTATE(f, "f", 132, mul(mul(mc_math_sin(trig, angle), amplitude), 3), 0, 0, 1);
        ROTATE(f, "f", 158, mul(fabsf(mul(mc_math_cos(trig, sub(angle, .2f)), amplitude)), 5), 1, 0,
               0);
        ROTATE(f, "f", 166, sway, 1, 0, 0);
    }
    f->portal = lerp(p->player.C, p->player.B, t);
    if (f->portal > 0) {
        const float scale = sub(divide(5, add(mul(f->portal, f->portal), 5)), mul(f->portal, .04f));
        f->portal_scale = mul(scale, scale);
        fail(e, MC_CLIENT_CAMERA_UNIMPLEMENTED_DEPENDENCY, "Camera real px render-tick counter",
             "a", 292);
        return false;
    }
    float offset = sub(base->offset, 1.62f);
    f->render_x = position(base->aJ, base->x, t);
    f->render_y = position(base->aK, base->y, t);
    f->render_z = position(base->aL, base->z, t);
    ROTATE(f, "g", 95, lerp(0, 0, t), 0, 0, 1);
    bool sleeping;
    if (mc_player_is_sleeping(entity, &sleeping, &e->player) != MC_PLAYER_OK) {
        fail(e, MC_CLIENT_CAMERA_OWNER_FAILED, "Camera sleeping query", "g", 99);
        return false;
    }
    if (sleeping) {
        volatile double shifted = (double)offset + 1.0;
        offset = (float)shifted;
        TRANSLATE(f, false, "g", 115, 0, .3f, 0);
        if (!settings_at(in, "g", 122, 125, &options, e))
            return false;
        if (!options.values.debug_camera) {
            mc_world_runtime *world = NULL;
            if (!world_at(in, entity, registry, 135, &world, e))
                return false;
            const int32_t x = mc_math_floor_double(base->x), y = mc_math_floor_double(base->y),
                          z = mc_math_floor_double(base->z);
            if (world == NULL) {
                fail(e, MC_CLIENT_CAMERA_NULL_REFERENCE, "Camera sleeping World receiver", "g",
                     159);
                return false;
            }
            int32_t id;
            const mc_world_storage_status status =
                mc_world_runtime_block_get(world, x, y, z, &id, &e->world);
            mc_world_runtime_binding_release(world);
            if (status != MC_WORLD_STORAGE_OK) {
                fail(e, MC_CLIENT_CAMERA_WORLD_FAILED, "Camera sleeping Block", "g", 159);
                return false;
            }
            const mc_block *bed;
            if (mc_registry_block_static_get(registry, "T", &bed, &e->registry) != MC_REGISTRY_OK) {
                fail(e, MC_CLIENT_CAMERA_REGISTRY_FAILED, "Camera bed Block active use", "g", 166);
                return false;
            }
            const mc_block_fields *fields = mc_block_fields_get(bed);
            if (fields == NULL) {
                fail(e, MC_CLIENT_CAMERA_NULL_REFERENCE, "Camera bed Block receiver", "g", 169);
                return false;
            }
            if (id == fields->bn) {
                if (!world_at(in, entity, registry, 179, &world, e))
                    return false;
                if (!fp(e, "g", 185)) {
                    mc_world_runtime_binding_release(world);
                    return false;
                }
                const int32_t mx = mc_math_floor_double(base->x),
                              my = mc_math_floor_double(base->y),
                              mz = mc_math_floor_double(base->z);
                if (world == NULL) {
                    fail(e, MC_CLIENT_CAMERA_NULL_REFERENCE, "Camera bed metadata World receiver",
                         "g", 203);
                    return false;
                }
                int32_t metadata;
                const mc_world_storage_status meta_status =
                    mc_world_runtime_metadata_get(world, mx, my, mz, &metadata, &e->world);
                mc_world_runtime_binding_release(world);
                if (meta_status != MC_WORLD_STORAGE_OK) {
                    fail(e, MC_CLIENT_CAMERA_WORLD_FAILED, "Camera bed metadata", "g", 203);
                    return false;
                }
                if (!fp(e, "g", 219))
                    return false;
                ROTATE(f, "g", 223, (float)mc_java_i32_mul(metadata & 3, 90), 0, 1, 0);
            }
            if (!fp(e, "g", 238))
                return false;
            ROTATE(f, "g", 249, add(lerp(base->aU, base->yaw, t), 180), 0, -1, 0);
            ROTATE(f, "g", 272, lerp(base->aV, base->aT, t), -1, 0, 0);
        }
    } else {
        if (!settings_at(in, "g", 282, 285, &options, e))
            return false;
        if (options.values.third_person_view) {
            fail(e, MC_CLIENT_CAMERA_UNIMPLEMENTED_DEPENDENCY, "Camera real px third-person owner",
                 "g", 292);
            return false;
        }
        TRANSLATE(f, false, "g", 721, 0, 0, -.1f);
    }
    if (!settings_at(in, "g", 728, 731, &options, e))
        return false;
    if (!options.values.debug_camera) {
        ROTATE(f, "g", 756, lerp(base->aV, base->aT, t), 1, 0, 0);
        ROTATE(f, "g", 781, add(lerp(base->aU, base->yaw, t), 180), 0, 1, 0);
    }
    TRANSLATE(f, false, "g", 787, 0, offset, 0);
    /* Fresh final g790..848 position reads, including callback mutations. */
    f->render_x = position(base->aJ, base->x, t);
    f->render_y = position(base->aK, base->y, t);
    f->render_z = position(base->aL, base->z, t);
    f->fog_x = f->render_x;
    volatile double fog_y = f->render_y - (double)offset;
    f->fog_y = fog_y;
    f->fog_z = f->render_z;
    f->cloud_fog = false;
    f->complete = true;
    return true;
}

static mc_client_camera_status enter(mc_entity *entity, const camera_inputs *in,
                                     mc_registry_classes *registry, const mc_math_trig *trig,
                                     float t, int32_t eye, mc_client_camera_frame *frame,
                                     mc_client_camera_error *error) {
    mc_client_camera_error e;
    mc_client_camera_error_init(&e);
    bool held = false, registry_held = false;
    if (registry == NULL || trig == NULL || frame == NULL) {
        fail(&e, MC_CLIENT_CAMERA_INVALID_ARGUMENT, "Camera native arguments", NULL, 0);
        goto done;
    }
    if (entity != NULL) {
        if (!mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER) ||
            entity->phase != MC_ENTITY_READY) {
            fail(&e, MC_CLIENT_CAMERA_INVALID_ARGUMENT, "Camera genuine READY LocalPlayer", NULL,
                 0);
            goto done;
        }
        e.entity = mc_entity_retain(entity);
        if (e.entity != MC_ENTITY_OK) {
            fail(&e, MC_CLIENT_CAMERA_OWNER_FAILED, "Camera Player native hold", NULL, 0);
            goto done;
        }
        held = true;
        if (mc_registry_classes_retain(registry, MC_REGISTRY_CLIENT, entity->classes->profile,
                                       &e.registry) != MC_REGISTRY_OK) {
            fail(&e, MC_CLIENT_CAMERA_OWNER_FAILED, "Camera code namespace hold", NULL, 0);
            goto done;
        }
        registry_held = true;
        if (entity->base.world != NULL &&
            mc_world_runtime_registry(entity->base.world) != registry) {
            fail(&e, MC_CLIENT_CAMERA_INVALID_ARGUMENT, "Camera actual World code Registry", NULL,
                 0);
            goto done;
        }
    }
    memset(frame, 0, sizeof(*frame));
    identity(frame->projection);
    identity(frame->modelview);
    (void)run(entity, in, registry, trig, t, eye, frame, &e);
done:
    if (registry_held)
        (void)mc_registry_classes_release(registry, NULL);
    if (held)
        mc_entity_release(entity);
    if (error != NULL)
        *error = e;
    return e.status;
}

mc_client_camera_status mc_client_first_person_camera(
    mc_entity *entity, const mc_game_settings *settings, mc_world_runtime *application_world,
    mc_registry_classes *registry, const mc_math_trig *trig, int32_t width, int32_t height, float t,
    int32_t eye, mc_client_camera_frame *frame, mc_client_camera_error *error) {
    const camera_inputs in = {
        .settings = settings, .world = application_world, .width = width, .height = height};
    return enter(entity, &in, registry, trig, t, eye, frame, error);
}

mc_client_camera_status mc_client_first_person_camera_for_application(
    const client_application *application, mc_entity *selected_local_player,
    mc_registry_classes *code_registry, const mc_math_trig *trig, float partial_tick,
    int32_t eye_pass, mc_client_camera_frame *frame, mc_client_camera_error *error) {
    const camera_inputs in = {.application = application, .live_application = true};
    return enter(selected_local_player, &in, code_registry, trig, partial_tick, eye_pass, frame,
                 error);
}
