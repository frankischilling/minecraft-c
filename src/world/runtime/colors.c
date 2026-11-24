#include "mc/world/runtime/colors.h"

#include "../../entity/player/tick_math_internal.h"
#include "colors_internal.h"
#include "construction_internal.h"
#include "runtime_internal.h"

#include <string.h>

bool mc_world_color_fail(color_frame *frame, const char *operation, unsigned int pc, bool native) {
    frame->error.status = MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    frame->error.operation = operation;
    frame->error.native_policy = native;
    frame->error.site.bytecode = pc;
    frame->error.cause = (mc_world_storage_error){frame->error.status, operation, 0};
    return false;
}

bool mc_world_color_begin(color_frame *frame, mc_world_runtime *world, mc_math_classes *math,
                          const void *out, const char *method, const char *descriptor) {
    memset(frame, 0, sizeof(*frame));
    frame->world = world;
    frame->math = math;
    frame->error.site = (mc_world_color_site){"fd", method, descriptor, 0};
    frame->error.math.source_pc = -1;
    if (out == NULL) {
        mc_world_color_fail(frame, "World color output admission", 0, true);
        frame->error.status = frame->error.cause.status = MC_WORLD_STORAGE_INVALID_ARGUMENT;
        return false;
    }
    if (world == NULL)
        return mc_world_color_fail(frame, "actual World color null receiver", 0, false);
    if (world->construction.mode != MC_WORLD_RUNTIME_GAME ||
        world->construction.phase == MC_WORLD_RUNTIME_RETIRED ||
        world->classes->side != MC_WORLD_INFO_CLIENT)
        return mc_world_color_fail(frame, "World color live client GAME admission", 0, true);
    const mc_world_storage_status entered = mc_world_runtime_call_enter(world, &frame->error.cause);
    if (entered != MC_WORLD_STORAGE_OK) {
        frame->error.status = entered;
        frame->error.operation = frame->error.cause.operation;
        frame->error.native_policy = true;
        return false;
    }
    frame->entered = true;
    return true;
}

mc_world_storage_status mc_world_color_finish(color_frame *frame, mc_world_color_error *error) {
    if (frame->camera_held)
        mc_entity_release(frame->camera);
    if (frame->math_held) {
        /* No source callback remains entered here. ensure either completed
         * or latched failure, so this ordinary occurrence can always drop. */
        (void)mc_math_classes_release(&frame->math, NULL);
    }
    if (frame->entered) {
        if (frame->error.status != MC_WORLD_STORAGE_OK)
            frame->world->last_error = frame->error.cause;
        mc_world_runtime_call_leave(frame->world);
    }
    if (error != NULL)
        *error = frame->error;
    return frame->error.status;
}

bool mc_world_color_fp(color_frame *frame, unsigned int pc) {
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u ||
           mc_world_color_fail(frame, "World color strict floating environment", pc, true);
}

bool mc_world_color_math_ready(color_frame *frame, unsigned int pc) {
    const mc_math_trig *expected = mc_world_runtime_class_trig(frame->world);
    if (frame->math == NULL || (expected != NULL && mc_math_classes_table(frame->math) != expected))
        return mc_world_color_fail(frame, "World color SAME MathHelper namespace", pc, true);
    if (!frame->math_held) {
        mc_math_class_state state;
        if (mc_math_classes_state(frame->math, &state, &frame->error.math) != MC_MATH_CLASS_OK ||
            state.phase == MC_MATH_CLASS_CONSTRUCTING || state.phase == MC_MATH_CLASS_RETIRING)
            return mc_world_color_fail(frame, "World color MathHelper entered admission", pc, true);
        if (mc_math_classes_retain(frame->math, &frame->error.math) != MC_MATH_CLASS_OK)
            return mc_world_color_fail(frame, frame->error.math.operation, pc, true);
        frame->math_held = true;
    }
    if (mc_math_classes_ensure(frame->math, &frame->error.math) != MC_MATH_CLASS_OK) {
        frame->error.child = (mc_world_color_site){
            "in", "<clinit>", "()V",
            frame->error.math.source_pc < 0 ? 0u : (unsigned int)frame->error.math.source_pc};
        return mc_world_color_fail(frame, frame->error.math.operation, pc, true);
    }
    return true;
}

/* fd.b(F)F captures t before reading Info time; a null t does not skip x/f. */
bool mc_world_color_angle(color_frame *frame, float partial, float *out, unsigned int pc) {
    mc_world_provider *dimension = frame->world->fields.t;
    int64_t time;
    frame->error.info = mc_world_info_time_get(frame->world->fields.x, &time);
    if (frame->error.info != MC_WORLD_INFO_OK) {
        frame->error.child = (mc_world_color_site){"fd", "b", "(F)F", 8};
        return mc_world_color_fail(frame, "actual World celestial Info time receiver", pc,
                                   frame->world->fields.x != NULL);
    }
    if (dimension == NULL) {
        frame->error.child = (mc_world_color_site){"fd", "b", "(F)F", 12};
        return mc_world_color_fail(frame, "actual World celestial null Dimension receiver", pc,
                                   false);
    }
    if (mc_world_provider_celestial_angle(dimension, time, partial, out, &frame->error.provider) !=
        MC_WORLD_PROVIDER_OK) {
        frame->error.child =
            (mc_world_color_site){"xa", "a", "(JF)F", frame->error.provider.bytecode};
        return mc_world_color_fail(frame, frame->error.provider.operation, pc, true);
    }
    return true;
}

static bool daylight(color_frame *frame, float angle_value, float *out, unsigned int pc) {
    if (!mc_world_color_fp(frame, pc) || !mc_world_color_math_ready(frame, pc))
        return false;
    const float argument = at_fmul(at_fmul(angle_value, 3.1415927f), 2.0f);
    float value =
        at_fadd(at_fmul(mc_math_cos(mc_math_classes_table(frame->math), argument), 2.0f), 0.5f);
    if (value < 0.0f)
        value = 0.0f;
    if (value > 1.0f)
        value = 1.0f;
    *out = value;
    return true;
}

static float channel(uint32_t value) {
    const float converted =
        _mm_cvtss_f32(_mm_cvtsi32_ss(_mm_setzero_ps(), (int32_t)(value & 255u)));
    return at_fdiv(converted, 255.0f);
}

static void desaturate(float rgb[3], float strength, float gray_factor, float attenuation) {
    const float gray = at_fmul(
        at_fadd(at_fadd(at_fmul(rgb[0], 0.3f), at_fmul(rgb[1], 0.59f)), at_fmul(rgb[2], 0.11f)),
        gray_factor);
    const float factor = at_fsub(1.0f, at_fmul(strength, attenuation));
    for (size_t index = 0; index < 3; ++index)
        rgb[index] = at_fadd(at_fmul(rgb[index], factor), at_fmul(gray, at_fsub(1.0f, factor)));
}

bool mc_world_color_weather(color_frame *frame, float partial, bool thunder, float *out,
                            unsigned int pc) {
    const mc_world_storage_status status =
        thunder ? mc_world_runtime_thunder_strength(frame->world, partial, out, &frame->error.cause)
                : mc_world_runtime_rain_strength(frame->world, partial, out, &frame->error.cause);
    if (status != MC_WORLD_STORAGE_OK) {
        frame->error.status = status;
        frame->error.operation = frame->error.cause.operation;
        frame->error.native_policy = true;
        frame->error.site.bytecode = pc;
        frame->error.child = (mc_world_color_site){"fd", thunder ? "f" : "g", "(F)F", 0};
        return false;
    }
    return true;
}

static bool vector(color_frame *frame, const float rgb[3], mc_entity_vector **out,
                   unsigned int pc) {
    mc_entity_classes *classes = mc_world_runtime_entities(frame->world);
    if (classes == NULL)
        return mc_world_color_fail(frame, "World color canonical Vec3 namespace", pc, true);
    frame->error.entity = mc_entity_vector_pool_get(classes, at_widen(rgb[0]), at_widen(rgb[1]),
                                                    at_widen(rgb[2]), out, &frame->error.vector);
    if (frame->error.entity != MC_ENTITY_OK) {
        frame->error.child =
            (mc_world_color_site){"bt", "b", "(DDD)Lbt;", frame->error.vector.bytecode};
        return mc_world_color_fail(frame, frame->error.vector.operation, pc, true);
    }
    return true;
}

static bool manager(color_frame *frame, unsigned int getter_pc, unsigned int invoke_pc,
                    mc_world_chunk_manager **out) {
    mc_world_provider_fields dimension;
    if (frame->world->fields.t == NULL) {
        frame->error.child = (mc_world_color_site){"fd", "a", "()Lxv;", 4};
        return mc_world_color_fail(frame, "actual World color biome-manager Dimension receiver",
                                   getter_pc, false);
    }
    if (mc_world_provider_fields_get(frame->world->fields.t, &dimension) != MC_WORLD_PROVIDER_OK)
        return mc_world_color_fail(frame, "World color Dimension field access", getter_pc, true);
    if (dimension.b == NULL)
        return mc_world_color_fail(frame, "actual World color null biome-manager invocation",
                                   invoke_pc, false);
    *out = dimension.b;
    return true;
}

mc_world_storage_status mc_world_runtime_sky_color(mc_world_runtime *world, mc_entity *camera,
                                                   float partial, mc_math_classes *math,
                                                   mc_entity_vector **out,
                                                   mc_world_color_error *error) {
    color_frame frame;
    if (!mc_world_color_begin(&frame, world, math, out, "a", "(Lsn;F)Lbt;"))
        return mc_world_color_finish(&frame, error);
    float celestial, brightness, rgb[3], strength;
    if (!mc_world_color_angle(&frame, partial, &celestial, 2) ||
        !daylight(&frame, celestial, &brightness, 12))
        goto done;
    if (camera == NULL) {
        mc_world_color_fail(&frame, "actual World sky null Entity X receiver", 43, false);
        goto done;
    }
    frame.error.entity = mc_entity_retain(camera);
    if (frame.error.entity != MC_ENTITY_OK) {
        mc_world_color_fail(&frame, "World sky camera native occurrence", 43, true);
        goto done;
    }
    frame.camera = camera;
    frame.camera_held = true;
    mc_entity_snapshot entity;
    if (mc_entity_get_snapshot(camera, &entity) != MC_ENTITY_OK) {
        mc_world_color_fail(&frame, "World sky actual Entity X field", 43, true);
        goto done;
    }
    const int32_t x = mc_math_floor_double(entity.base.x);
    if (mc_entity_get_snapshot(camera, &entity) != MC_ENTITY_OK) {
        mc_world_color_fail(&frame, "World sky actual Entity Z field", 52, true);
        goto done;
    }
    const int32_t z = mc_math_floor_double(entity.base.z);
    mc_world_chunk_manager *biome_manager;
    if (!manager(&frame, 61, 68, &biome_manager))
        goto done;
    double raw_temperature;
    if (mc_world_chunk_manager_temperature_raw(biome_manager, x, z, &raw_temperature,
                                               &frame.error.biome) != MC_WORLD_PROVIDER_OK) {
        frame.error.child = (mc_world_color_site){"xv", "b", "(II)D", frame.error.biome.bytecode};
        mc_world_color_fail(&frame, frame.error.biome.operation, 68, true);
        goto done;
    }
    const float temperature = at_narrow(raw_temperature);
    if (!manager(&frame, 75, 82, &biome_manager))
        goto done;
    const mc_biome *biome;
    if (mc_world_chunk_manager_point(biome_manager, x, z, &biome, &frame.error.biome) !=
        MC_WORLD_PROVIDER_OK) {
        frame.error.child =
            (mc_world_color_site){"xv", "a", "(II)Lkd;", frame.error.biome.bytecode};
        mc_world_color_fail(&frame, frame.error.biome.operation, 82, true);
        goto done;
    }
    int32_t color;
    if (mc_world_biome_sky_color(biome, temperature, &color, &frame.error.sky) != MC_REGISTRY_OK) {
        frame.error.child = (mc_world_color_site){"kd", "a", "(F)I", frame.error.sky.bytecode};
        mc_world_color_fail(&frame, frame.error.sky.operation, 87, frame.error.sky.native_policy);
        goto done;
    }
    rgb[0] = at_fmul(channel((uint32_t)color >> 16u), brightness);
    rgb[1] = at_fmul(channel((uint32_t)color >> 8u), brightness);
    rgb[2] = at_fmul(channel((uint32_t)color), brightness);
    if (!mc_world_color_weather(&frame, partial, false, &strength, 157))
        goto done;
    if (strength > 0.0f)
        desaturate(rgb, strength, 0.6f, 0.75f);
    if (!mc_world_color_weather(&frame, partial, true, &strength, 247))
        goto done;
    if (strength > 0.0f)
        desaturate(rgb, strength, 0.2f, 0.75f);
    if (world->fields.n > 0) {
        const float count = _mm_cvtss_f32(_mm_cvtsi32_ss(_mm_setzero_ps(), world->fields.n));
        float flash = at_fsub(count, partial);
        if (flash > 1.0f)
            flash = 1.0f;
        flash = at_fmul(flash, 0.45f);
        for (size_t index = 0; index < 3; ++index)
            rgb[index] = at_fadd(at_fmul(rgb[index], at_fsub(1.0f, flash)),
                                 at_fmul(index == 2 ? 1.0f : 0.8f, flash));
    }
    if (vector(&frame, rgb, out, 421))
        frame.error.site.bytecode = 424;
done:
    return mc_world_color_finish(&frame, error);
}

mc_world_storage_status mc_world_runtime_cloud_color(mc_world_runtime *world, float partial,
                                                     mc_math_classes *math, mc_entity_vector **out,
                                                     mc_world_color_error *error) {
    color_frame frame;
    if (!mc_world_color_begin(&frame, world, math, out, "c", "(F)Lbt;"))
        return mc_world_color_finish(&frame, error);
    float celestial, brightness, rgb[3], strength;
    if (!mc_world_color_angle(&frame, partial, &celestial, 2) ||
        !daylight(&frame, celestial, &brightness, 12))
        goto done;
    rgb[0] = channel((uint32_t)((uint64_t)world->fields.H >> 16u));
    rgb[1] = channel((uint32_t)((uint64_t)world->fields.H >> 8u));
    rgb[2] = channel((uint32_t)(uint64_t)world->fields.H);
    if (!mc_world_color_weather(&frame, partial, false, &strength, 87))
        goto done;
    if (strength > 0.0f)
        desaturate(rgb, strength, 0.6f, 0.95f);
    rgb[0] = at_fmul(rgb[0], at_fadd(at_fmul(brightness, 0.9f), 0.1f));
    rgb[1] = at_fmul(rgb[1], at_fadd(at_fmul(brightness, 0.9f), 0.1f));
    rgb[2] = at_fmul(rgb[2], at_fadd(at_fmul(brightness, 0.85f), 0.15f));
    if (!mc_world_color_weather(&frame, partial, true, &strength, 213))
        goto done;
    if (strength > 0.0f)
        desaturate(rgb, strength, 0.2f, 0.95f);
    if (vector(&frame, rgb, out, 310))
        frame.error.site.bytecode = 313;
done:
    return mc_world_color_finish(&frame, error);
}

mc_world_storage_status mc_world_runtime_fog_color(mc_world_runtime *world, float partial,
                                                   mc_math_classes *math, mc_entity_vector **out,
                                                   mc_world_color_error *error) {
    color_frame frame;
    if (!mc_world_color_begin(&frame, world, math, out, "d", "(F)Lbt;"))
        return mc_world_color_finish(&frame, error);
    float celestial, brightness, rgb[3];
    if (!mc_world_color_angle(&frame, partial, &celestial, 2))
        goto done;
    mc_world_provider *dimension = world->fields.t;
    if (dimension == NULL) {
        mc_world_color_fail(&frame, "actual World fog null Dimension receiver", 12, false);
        goto done;
    }
    mc_world_provider_state state;
    if (mc_world_provider_state_get(dimension, &state) != MC_WORLD_PROVIDER_OK) {
        mc_world_color_fail(&frame, "World fog actual Dimension kind", 12, true);
        goto done;
    }
    const char *declaring = state.kind == MC_WORLD_PROVIDER_NETHER ? "wd"
                            : state.kind == MC_WORLD_PROVIDER_SKY  ? "ay"
                                                                   : "xa";
    if (state.kind == MC_WORLD_PROVIDER_NETHER) {
        rgb[0] = 0.2f;
        rgb[1] = rgb[2] = 0.03f;
    } else {
        if (!daylight(&frame, celestial, &brightness, 12)) {
            if (frame.error.child.class_name == NULL)
                frame.error.child = (mc_world_color_site){
                    declaring, "b", "(FF)Lbt;", state.kind == MC_WORLD_PROVIDER_SKY ? 9u : 6u};
            goto done;
        }
        rgb[0] = state.kind == MC_WORLD_PROVIDER_SKY ? channel(128) : 0.7529412f;
        rgb[1] = state.kind == MC_WORLD_PROVIDER_SKY ? channel(128) : 0.84705883f;
        rgb[2] = state.kind == MC_WORLD_PROVIDER_SKY ? channel(160) : 1.0f;
        rgb[0] = at_fmul(rgb[0], at_fadd(at_fmul(brightness, 0.94f), 0.06f));
        rgb[1] = at_fmul(rgb[1], at_fadd(at_fmul(brightness, 0.94f), 0.06f));
        rgb[2] = at_fmul(rgb[2], at_fadd(at_fmul(brightness, 0.91f), 0.09f));
    }
    if (!vector(&frame, rgb, out, 12)) {
        frame.error.child = (mc_world_color_site){declaring, "b", "(FF)Lbt;",
                                                  state.kind == MC_WORLD_PROVIDER_NETHER ? 9u
                                                  : state.kind == MC_WORLD_PROVIDER_SKY  ? 126u
                                                                                         : 87u};
        goto done;
    }
    frame.error.site.bytecode = 15;
done:
    return mc_world_color_finish(&frame, error);
}
