#include "renderer_fog_internal.h"

#include "../../../world/runtime/construction_internal.h"
#include "../../../world/runtime/runtime_internal.h"
#include "../../application_state.h"
#include "../../display/world_frame_surface.h"
#include "internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/core/java/java.h"
#include "mc/world/runtime/environment.h"

#include <emmintrin.h>
#include <math.h>
#include <stdlib.h>

#if !defined(_M_X64) && !defined(_M_AMD64) && !defined(__x86_64__)
#error "Renderer fog arithmetic requires x86-64/SSE2"
#endif
#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) ||       \
    (defined(_M_FP_FAST) && _M_FP_FAST)
#error "Renderer fog arithmetic requires strict floating transformations"
#endif

struct mc_client_renderer_fog_call {
    mc_client_renderer_fog_state state;
    mc_client_renderer_fog_error failure;
    mc_entity_classes *vectors;
    mc_entity_vector *sky, *fog, *cloud;
    bool world_held, camera_held, sky_camera_held;
};

static bool fail(mc_client_renderer_fog_call *call, mc_client_renderer_fog_status status,
                 const char *operation, unsigned int pc, bool native) {
    call->failure.status = status;
    call->failure.operation = operation;
    call->failure.site = (mc_client_graphics_site){"px", "h", "(F)V", (int32_t)pc};
    call->failure.native_policy = native;
    call->state.source_pc = pc;
    return false;
}

static bool fp(mc_client_renderer_fog_call *call, unsigned int pc) {
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u ||
           fail(call, MC_CLIENT_RENDERER_FOG_UNSUPPORTED_FP,
                "renderer fog native floating environment", pc, true);
}

static float add(float a, float b) {
    return _mm_cvtss_f32(_mm_add_ss(_mm_set_ss(a), _mm_set_ss(b)));
}

static float sub(float a, float b) {
    return _mm_cvtss_f32(_mm_sub_ss(_mm_set_ss(a), _mm_set_ss(b)));
}

static float mul(float a, float b) {
    return _mm_cvtss_f32(_mm_mul_ss(_mm_set_ss(a), _mm_set_ss(b)));
}

static float divide(float a, float b) {
    return _mm_cvtss_f32(_mm_div_ss(_mm_set_ss(a), _mm_set_ss(b)));
}

static float narrow(double value) {
    return _mm_cvtss_f32(_mm_cvtsd_ss(_mm_setzero_ps(), _mm_set_sd(value)));
}

static client_application *app_at(mc_client_renderer_fog_call *call, unsigned int pc) {
    client_application *app = call->state.owner->fields.j;
    if (app == NULL)
        (void)fail(call, MC_CLIENT_RENDERER_FOG_SOURCE_NULL, "actual fog App receiver", pc, false);
    return app;
}

static bool capture_camera(mc_client_renderer_fog_call *call, mc_entity *camera, bool *held,
                           unsigned int pc) {
    if (camera == NULL)
        return true;
    call->failure.entity = mc_entity_retain(camera);
    if (call->failure.entity != MC_ENTITY_OK)
        return fail(call, MC_CLIENT_RENDERER_FOG_NATIVE_FAILED, "fog Entity occurrence", pc, true);
    *held = true;
    return true;
}

static bool colors_result(mc_client_renderer_fog_call *call, mc_world_storage_status status,
                          unsigned int pc) {
    if (status == MC_WORLD_STORAGE_OK)
        return true;
    const mc_world_color_error *error = &call->failure.colors;
    /* The complete World cause remains by value in this heap-owned record. */
    call->failure.child =
        (mc_client_graphics_site){error->site.class_name, error->site.method,
                                  error->site.descriptor, (int32_t)error->site.bytecode};
    return fail(call,
                error->native_policy ? MC_CLIENT_RENDERER_FOG_CHILD_FAILED
                                     : MC_CLIENT_RENDERER_FOG_SOURCE_NULL,
                error->operation, pc, error->native_policy);
}

static bool weather(mc_client_renderer_fog_call *call, bool thunder) {
    float *out = thunder ? &call->state.thunder : &call->state.rain;
    const mc_world_storage_status status =
        thunder ? mc_world_runtime_thunder_strength(call->state.world, call->state.partial, out,
                                                    &call->failure.weather)
                : mc_world_runtime_rain_strength(call->state.world, call->state.partial, out,
                                                 &call->failure.weather);
    if (status == MC_WORLD_STORAGE_OK)
        return true;
    call->failure.child = (mc_client_graphics_site){"fd", thunder ? "f" : "g", "(F)F", 0};
    return fail(call, MC_CLIENT_RENDERER_FOG_CHILD_FAILED, call->failure.weather.operation,
                thunder ? 246u : 181u, true);
}

static bool immersion(mc_client_renderer_fog_call *call, bool lava, bool *inside) {
    const unsigned int material_pc = lava ? 379u : 348u, invoke_pc = lava ? 382u : 351u;
    mc_material *material = NULL;
    if (mc_registry_material_get(mc_world_runtime_registry(call->state.world),
                                 lava ? MC_MATERIAL_h : MC_MATERIAL_g, &material,
                                 &call->failure.registry) != MC_REGISTRY_OK)
        return fail(call, MC_CLIENT_RENDERER_FOG_CHILD_FAILED, "actual fog Material active use",
                    material_pc, true);
    if (call->state.camera == NULL)
        return fail(call, MC_CLIENT_RENDERER_FOG_SOURCE_NULL,
                    "actual fog camera immersion receiver", invoke_pc, false);
    const mc_entity_status status =
        mc_entity_inside_material(call->state.camera, material, inside, &call->failure.material);
    if (status != MC_ENTITY_OK) {
        call->failure.child = (mc_client_graphics_site){"sn", "a", "(Lln;)Z",
                                                        (int32_t)call->failure.material.bytecode};
        const bool source_null =
            status == MC_ENTITY_WORLD_FAILED &&
            call->failure.material.world.status == MC_WORLD_STORAGE_INVALID_ARGUMENT;
        return fail(call,
                    source_null ? MC_CLIENT_RENDERER_FOG_SOURCE_NULL
                                : MC_CLIENT_RENDERER_FOG_CHILD_FAILED,
                    call->failure.material.operation, invoke_pc, !source_null);
    }
    return true;
}

static void store(mc_client_renderer_fog_call *call, float *field, float value, unsigned int pc) {
    *field = value;
    call->state.last_store_pc = pc;
    call->state.owner->last_store = (int32_t)pc;
}

static bool body(mc_client_renderer_fog_call *call, mc_math_classes *math) {
    mc_client_source_px *owner = call->state.owner;
    mc_client_entity_renderer_fields *f = &owner->fields;
    client_application *app = app_at(call, 4);
    if (app == NULL)
        return false;
    call->state.world = app->active_world;
    call->state.world_captured = true;
    if (call->state.world != NULL) {
        if (!mc_world_runtime_binding_retain(call->state.world)) {
            const mc_world_storage_error *error = mc_world_runtime_last_error(call->state.world);
            if (error != NULL)
                call->failure.weather = *error;
            return fail(call, MC_CLIENT_RENDERER_FOG_NATIVE_FAILED, "fog World occurrence", 7,
                        true);
        }
        call->world_held = true;
        call->vectors = mc_world_runtime_entities(call->state.world);
    }
    app = app_at(call, 12);
    if (app == NULL)
        return false;
    call->state.camera = app->i;
    call->state.camera_captured = true;
    if (!capture_camera(call, call->state.camera, &call->camera_held, 15))
        return false;
    app = app_at(call, 22);
    if (app == NULL)
        return false;
    mc_game_settings_state settings;
    if (!mc_game_settings_read_state(app->settings, &settings))
        return fail(call,
                    app->settings == NULL ? MC_CLIENT_RENDERER_FOG_SOURCE_NULL
                                          : MC_CLIENT_RENDERER_FOG_NATIVE_FAILED,
                    "actual fog render-distance Settings receiver", 25, app->settings != NULL);
    call->state.render_distance = settings.values.render_distance;
    call->state.distance_read = true;
    if (!fp(call, 29))
        return false;
    const int32_t denominator = mc_java_i32_sub(4, settings.values.render_distance);
    const float converted = _mm_cvtss_f32(_mm_cvtsi32_ss(_mm_setzero_ps(), denominator));
    const double powered = pow((double)divide(1.0f, converted), 0.25);
    call->state.distance_blend = sub(1.0f, narrow(powered));
    call->state.blend_computed = true;
    app = app_at(call, 52);
    if (app == NULL)
        return false;
    call->state.sky_camera = app->i; /* Independent argument read after Math.pow. */
    if (!capture_camera(call, call->state.sky_camera, &call->sky_camera_held, 52))
        return false;
    if (call->state.world == NULL)
        return fail(call, MC_CLIENT_RENDERER_FOG_SOURCE_NULL, "actual fog sky-color World receiver",
                    56, false);
    if (!colors_result(call,
                       mc_world_runtime_sky_color(call->state.world, call->state.sky_camera,
                                                  call->state.partial, math, &call->sky,
                                                  &call->failure.colors),
                       56))
        return false;
    call->state.sky = call->sky;
    call->state.sky_returned = true;
    if (call->sky == NULL)
        return fail(call, MC_CLIENT_RENDERER_FOG_SOURCE_NULL, "actual fog sky Vec receiver", 63,
                    false);
    if (!fp(call, 66))
        return false;
    call->state.sky_red = narrow(call->sky->x);
    call->state.sky_green = narrow(call->sky->y);
    call->state.sky_blue = narrow(call->sky->z);
    if (!colors_result(call,
                       mc_world_runtime_fog_color(call->state.world, call->state.partial, math,
                                                  &call->fog, &call->failure.colors),
                       87))
        return false;
    call->state.fog = call->fog;
    call->state.fog_returned = true;
    if (call->fog == NULL)
        return fail(call, MC_CLIENT_RENDERER_FOG_SOURCE_NULL, "actual fog Vec receiver", 95, false);
    if (!fp(call, 98))
        return false;
    store(call, &f->g, narrow(call->fog->x), 99);
    store(call, &f->h, narrow(call->fog->y), 109);
    store(call, &f->i, narrow(call->fog->z), 119);
    store(call, &f->g, add(f->g, mul(sub(call->state.sky_red, f->g), call->state.distance_blend)),
          138);
    store(call, &f->h, add(f->h, mul(sub(call->state.sky_green, f->h), call->state.distance_blend)),
          157);
    store(call, &f->i, add(f->i, mul(sub(call->state.sky_blue, f->i), call->state.distance_blend)),
          176);
    if (!weather(call, false))
        return false;
    call->state.rain_returned = true;
    if (!fp(call, 189))
        return false;
    if (call->state.rain > 0.0f) {
        const float rg = sub(1.0f, mul(call->state.rain, 0.5f));
        const float blue = sub(1.0f, mul(call->state.rain, 0.4f));
        store(call, &f->g, mul(f->g, rg), 219);
        store(call, &f->h, mul(f->h, rg), 230);
        store(call, &f->i, mul(f->i, blue), 241);
    }
    if (!weather(call, true))
        return false;
    call->state.thunder_returned = true;
    if (!fp(call, 254))
        return false;
    if (call->state.thunder > 0.0f) {
        const float attenuation = sub(1.0f, mul(call->state.thunder, 0.5f));
        store(call, &f->g, mul(f->g, attenuation), 275);
        store(call, &f->h, mul(f->h, attenuation), 286);
        store(call, &f->i, mul(f->i, attenuation), 297);
    }
    if (f->D) {
        if (!colors_result(call,
                           mc_world_runtime_cloud_color(call->state.world, call->state.partial,
                                                        math, &call->cloud, &call->failure.colors),
                           309))
            return false;
        call->state.cloud = call->cloud;
        call->state.cloud_returned = true;
        if (call->cloud == NULL)
            return fail(call, MC_CLIENT_RENDERER_FOG_SOURCE_NULL, "actual fog cloud Vec receiver",
                        317, false);
        if (!fp(call, 320))
            return false;
        store(call, &f->g, narrow(call->cloud->x), 321);
        store(call, &f->h, narrow(call->cloud->y), 331);
        store(call, &f->i, narrow(call->cloud->z), 341);
    } else {
        if (!immersion(call, false, &call->state.in_water))
            return false;
        call->state.water_tested = true;
        if (call->state.in_water) {
            store(call, &f->g, 0.02f, 360);
            store(call, &f->h, 0.02f, 366);
            store(call, &f->i, 0.2f, 372);
        } else {
            if (!immersion(call, true, &call->state.in_lava))
                return false;
            call->state.lava_tested = true;
            if (call->state.in_lava) {
                store(call, &f->g, 0.6f, 391);
                store(call, &f->h, 0.1f, 397);
                store(call, &f->i, 0.0f, 402);
            }
        }
    }
    if (!fp(call, 417))
        return false;
    call->state.brightness = add(f->L, mul(sub(f->M, f->L), call->state.partial));
    store(call, &f->g, mul(f->g, call->state.brightness), 431);
    store(call, &f->h, mul(f->h, call->state.brightness), 442);
    store(call, &f->i, mul(f->i, call->state.brightness), 453);
    call->state.brightness_applied = true;
    app = app_at(call, 460);
    if (app == NULL)
        return false;
    if (!mc_game_settings_read_state(app->settings, &settings))
        return fail(call,
                    app->settings == NULL ? MC_CLIENT_RENDERER_FOG_SOURCE_NULL
                                          : MC_CLIENT_RENDERER_FOG_NATIVE_FAILED,
                    "actual fog anaglyph Settings receiver", 463, app->settings != NULL);
    call->state.anaglyph_read = true;
    call->state.anaglyph = settings.values.anaglyph;
    if (call->state.anaglyph) {
        if (!fp(call, 475))
            return false;
        const float red =
            divide(add(add(mul(f->g, 30.0f), mul(f->h, 59.0f)), mul(f->i, 11.0f)), 100.0f);
        const float green = divide(add(mul(f->g, 30.0f), mul(f->h, 70.0f)), 100.0f);
        const float blue = divide(add(mul(f->g, 30.0f), mul(f->i, 70.0f)), 100.0f);
        store(call, &f->g, red, 540);
        store(call, &f->h, green, 546);
        store(call, &f->i, blue, 552);
    }
    if (!mc_client_display_source_clear_color(owner->services.display, f->g, f->h, f->i, 0.0f,
                                              &call->failure.display))
        return fail(call, MC_CLIENT_RENDERER_FOG_NATIVE_FAILED, call->failure.display.operation,
                    568, true);
    call->state.clear_color_set = call->state.returned = true;
    call->state.source_pc = 571;
    return true;
}

static mc_client_renderer_fog_status result(mc_client_renderer_fog_error *e,
                                            mc_client_renderer_fog_status status,
                                            const char *operation) {
    if (e != NULL)
        *e = (mc_client_renderer_fog_error){.status = status,
                                            .operation = operation,
                                            .native_policy = status != MC_CLIENT_RENDERER_FOG_OK};
    return status;
}

mc_client_renderer_fog_status entity_renderer_fog_entered(mc_client_source_px *owner,
                                                          mc_math_classes *math, float partial,
                                                          mc_client_renderer_fog_call **out,
                                                          mc_client_renderer_fog_error *error) {
    if (owner == NULL || out == NULL || *out != NULL)
        return result(error, MC_CLIENT_RENDERER_FOG_ARGUMENT, "fog native owner/output");
    if (!owner->active || owner->phase != MC_CLIENT_ENTITY_RENDERER_READY)
        return result(error, MC_CLIENT_RENDERER_FOG_BUSY, "fog borrowed parent guard");
    mc_client_renderer_fog_call *call = calloc(1, sizeof(*call));
    if (call == NULL)
        return result(error, MC_CLIENT_RENDERER_FOG_ALLOCATION, "fog retained call allocation");
    call->state.owner = owner;
    call->state.partial = partial;
    call->state.failure = &call->failure;
    *out = call;
    call->state.active = true;
    (void)body(call, math);
    call->state.active = false;
    if (error != NULL)
        *error = call->failure;
    return call->failure.status;
}

bool mc_client_entity_renderer_fog_inspect(const mc_client_renderer_fog_call *call,
                                           mc_client_renderer_fog_state *out) {
    if (call == NULL || out == NULL || call->state.active)
        return false;
    *out = call->state;
    return true;
}

mc_client_renderer_fog_status
mc_client_entity_renderer_fog_retire(mc_client_renderer_fog_call **slot,
                                     mc_client_renderer_fog_error *error) {
    if (slot == NULL)
        return result(error, MC_CLIENT_RENDERER_FOG_ARGUMENT, "fog retirement slot");
    mc_client_renderer_fog_call *call = *slot;
    if (call == NULL)
        return result(error, MC_CLIENT_RENDERER_FOG_OK, NULL);
    if (call->state.active)
        return result(error, MC_CLIENT_RENDERER_FOG_BUSY, "fog active retirement");
    mc_entity_vector **vectors[] = {&call->cloud, &call->fog, &call->sky};
    for (size_t i = 0; i < sizeof(vectors) / sizeof(vectors[0]); ++i) {
        if (*vectors[i] != NULL) {
            const mc_entity_status status = mc_entity_vector_release(call->vectors, *vectors[i]);
            if (status != MC_ENTITY_OK)
                return result(error, MC_CLIENT_RENDERER_FOG_BUSY, "fog Vec occurrence release");
            *vectors[i] = NULL;
            if (i == 0)
                call->state.cloud = NULL;
            else if (i == 1)
                call->state.fog = NULL;
            else
                call->state.sky = NULL;
        }
    }
    if (call->sky_camera_held) {
        mc_entity_release(call->state.sky_camera);
        call->sky_camera_held = false;
    }
    if (call->camera_held) {
        mc_entity_release(call->state.camera);
        call->camera_held = false;
    }
    if (call->world_held) {
        mc_world_runtime_binding_release(call->state.world);
        call->world_held = false;
    }
    free(call);
    *slot = NULL;
    return result(error, MC_CLIENT_RENDERER_FOG_OK, NULL);
}
