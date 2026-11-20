#include "precipitation_internal.h"

#include "../../../entity/entity_internal.h"
#include "../../application_state.h"
#include "../../display/fog_surface.h"
#include "../../display/source_surface.h"
#include "../../display/world_frame_surface.h"
#include "../../render/interpolation_internal.h"
#include "../../render_engine/nw.h"
#include "../../render_engine/texture_lookup.h"
#include "internal.h"
#include "mc/core/java/java.h"
#include "mc/core/math/math.h"
#include "mc/entity/entity.h"
#include "mc/world/runtime/environment.h"

#include <math.h>
#include <stdlib.h>

struct mc_client_precipitation_call {
    mc_client_precipitation_state state;
    mc_client_precipitation_error failure;
    mc_client_entity_renderer_error owner_error;
    mc_math_classes *math;
    mc_client_tessellator *nw;
    mc_world_chunk_manager *manager;
    mc_client_render_engine *engines[2];
    mc_client_texture_lookup_error texture[2], texture_cleanup;
    bool owner_held, camera_held, world_held, math_held, graphics_held, entered;
    bool lookup_owned[2];
};

static bool fail(mc_client_precipitation_call *c, mc_client_precipitation_status status,
                 const char *operation, unsigned int pc, bool native) {
    c->failure.status = status;
    c->failure.operation = operation;
    c->failure.source_pc = c->state.source_pc = pc;
    c->failure.native_policy = native;
    return false;
}

static bool fp(mc_client_precipitation_call *c, unsigned int pc) {
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u ||
           fail(c, MC_CLIENT_PRECIPITATION_UNSUPPORTED_FP, "precipitation strict floating controls",
                pc, true);
}

static bool graphics(mc_client_precipitation_call *c, bool result, unsigned int pc) {
    return result || fail(c, MC_CLIENT_PRECIPITATION_CHILD_FAILED, c->failure.graphics.operation,
                          pc, c->failure.graphics.native_policy);
}

static bool display(mc_client_precipitation_call *c, bool result, unsigned int pc) {
    return result ||
           fail(c, MC_CLIENT_PRECIPITATION_CHILD_FAILED, c->failure.display.operation, pc, true);
}

static bool math_use(mc_client_precipitation_call *c, unsigned int pc) {
    if (!c->math_held) {
        if (mc_math_classes_retain(c->math, &c->failure.math) != MC_MATH_CLASS_OK)
            return fail(c, MC_CLIENT_PRECIPITATION_CHILD_FAILED, "precipitation Math occurrence",
                        pc, true);
        c->math_held = true;
    }
    if (mc_math_classes_ensure(c->math, &c->failure.math) != MC_MATH_CLASS_OK)
        return fail(c, MC_CLIENT_PRECIPITATION_CHILD_FAILED, c->failure.math.operation, pc, true);
    return fp(c, pc);
}

static client_application *app(mc_client_precipitation_call *c, unsigned int pc) {
    client_application *value = c->state.owner->fields.j;
    if (value == NULL)
        fail(c, MC_CLIENT_PRECIPITATION_SOURCE_NULL, "precipitation App receiver", pc, false);
    return value;
}

static bool fancy(mc_client_precipitation_call *c, unsigned int app_pc, unsigned int pc,
                  bool *out) {
    client_application *a = app(c, app_pc);
    if (a == NULL)
        return false;
    mc_game_settings_state settings;
    if (!mc_game_settings_read_state(a->settings, &settings))
        return fail(c,
                    a->settings == NULL ? MC_CLIENT_PRECIPITATION_SOURCE_NULL
                                        : MC_CLIENT_PRECIPITATION_CHILD_FAILED,
                    "precipitation Settings receiver", pc, a->settings != NULL);
    *out = settings.values.fancy_graphics;
    return true;
}

static bool texture(mc_client_precipitation_call *c, unsigned int pass) {
    const unsigned int app_pc = pass == 0 ? 107u : 1002u;
    const unsigned int invoke_pc = pass == 0 ? 115u : 1010u;
    client_application *a = app(c, app_pc);
    if (a == NULL)
        return false;
    mc_client_render_engine *engine = a->render_engine;
    if (engine == NULL)
        return fail(c, MC_CLIENT_PRECIPITATION_SOURCE_NULL, "precipitation texture Engine receiver",
                    invoke_pc, false);
    if (mc_client_render_engine_retain(engine) != MC_CLIENT_RENDER_ENGINE_OK)
        return fail(c, MC_CLIENT_PRECIPITATION_CHILD_FAILED, "precipitation Engine occurrence",
                    invoke_pc, true);
    c->engines[pass] = engine;
    static const uint16_t snow[] = {'/', 'e', 'n', 'v', 'i', 'r', 'o', 'n', 'm', 'e', 'n',
                                    't', '/', 's', 'n', 'o', 'w', '.', 'p', 'n', 'g'};
    static const uint16_t rain[] = {'/', 'e', 'n', 'v', 'i', 'r', 'o', 'n', 'm', 'e', 'n',
                                    't', '/', 'r', 'a', 'i', 'n', '.', 'p', 'n', 'g'};
    const mc_texture_pack_text key = {pass == 0 ? snow : rain, sizeof snow / sizeof(*snow), false};
    const bool pending = mc_client_texture_lookup_pending(engine);
    mc_client_texture id;
    const mc_client_texture_lookup_status status = mc_client_render_engine_texture(
        engine, c->state.owner->services.display, NULL, key, &id, &c->texture[pass]);
    c->lookup_owned[pass] = !pending && mc_client_texture_lookup_pending(engine);
    if (status != MC_CLIENT_LOOKUP_OK) {
        c->failure.texture = &c->texture[pass];
        return fail(c, MC_CLIENT_PRECIPITATION_CHILD_FAILED, c->texture[pass].operation, invoke_pc,
                    c->texture[pass].native_policy);
    }
    if (!display(c,
                 mc_client_display_source_bind_id(c->state.owner->services.display, id,
                                                  &c->failure.display),
                 pass == 0 ? 118u : 1013u))
        return false;
    if (pass == 0)
        c->state.snow_texture_bound = true;
    else
        c->state.rain_texture_bound = true;
    return true;
}

static int32_t column_seed(int32_t x, int32_t z) {
    int32_t seed = mc_java_i32_mul(mc_java_i32_mul(x, x), 3121);
    seed = mc_java_i32_add(seed, mc_java_i32_mul(x, 45238971));
    seed = mc_java_i32_add(seed, mc_java_i32_mul(mc_java_i32_mul(z, z), 418711));
    return mc_java_i32_add(seed, mc_java_i32_mul(z, 13761));
}

static bool column(mc_client_precipitation_call *c, unsigned int pass) {
    mc_client_precipitation_state *s = &c->state;
    int32_t top;
    if (s->world == NULL)
        return fail(c, MC_CLIENT_PRECIPITATION_SOURCE_NULL, "precipitation column World receiver",
                    pass == 0 ? 301u : 1097u, false);
    if (mc_world_runtime_top_solid_liquid(s->world, s->x, s->z, &top, &c->failure.query) !=
        MC_WORLD_STORAGE_OK)
        return fail(c, MC_CLIENT_PRECIPITATION_CHILD_FAILED, c->failure.query.operation,
                    pass == 0 ? 301u : 1097u, true);
    if (pass == 0 && top < 0)
        top = 0;
    s->brightness_y = pass == 0 ? (top < s->eye_y ? s->eye_y : top) : 128;
    s->lower = mc_java_i32_sub(s->camera_y, s->radius);
    s->upper = mc_java_i32_add(s->camera_y, s->radius);
    if (s->lower < top)
        s->lower = top;
    if (s->upper < top)
        s->upper = top;
    if (s->lower == s->upper)
        return true;
    mc_java_random *random = s->owner->fields.J;
    const int32_t seed = column_seed(s->x, s->z);
    if (random == NULL)
        return fail(c, MC_CLIENT_PRECIPITATION_SOURCE_NULL, "precipitation Random.setSeed receiver",
                    pass == 0 ? 411u : 1184u, false);
    mc_java_random_reseed(random, (int64_t)seed);
    if (!fp(c, pass == 0 ? 418u : 1226u))
        return false;
    float scroll, u_shift = 0.0f, v_shift = 0.0f;
    if (pass == 0) {
        const float tick = at_fadd((float)s->owner->fields.l, s->partial);
        scroll = at_fdiv(at_fadd((float)(s->owner->fields.l & 511), s->partial), 512.0f);
        const float first = mc_java_random_next_float(s->owner->fields.J);
        const float scaled = at_fmul(tick, 0.01f);
        const float gaussian = (float)mc_java_random_next_gaussian(s->owner->fields.J);
        u_shift = at_fadd(first, at_fmul(scaled, gaussian));
        const float second = mc_java_random_next_float(s->owner->fields.J);
        const float gaussian2 = (float)mc_java_random_next_gaussian(s->owner->fields.J);
        v_shift = at_fadd(second, at_fmul(at_fmul(tick, gaussian2), 0.001f));
    } else {
        /* Keep each source integer addition before the mask and narrowing. */
        const int32_t tick = mc_java_i32_add(s->owner->fields.l, seed);
        const float phase = at_fdiv(at_fadd((float)(tick & 31), s->partial), 32.0f);
        scroll = at_fmul(phase, at_fadd(3.0f, mc_java_random_next_float(s->owner->fields.J)));
    }
    const double dx = at_sub((double)at_fadd((float)s->x, 0.5f), s->camera->base.x);
    const double dz = at_sub((double)at_fadd((float)s->z, 0.5f), s->camera->base.z);
    const double distance = at_add(at_mul(dx, dx), at_mul(dz, dz));
    if (!math_use(c, pass == 0 ? 526u : 1284u))
        return false;
    const float radial = at_fdiv((float)sqrt(distance), (float)s->radius);
    if (!graphics(c, mc_client_nw_begin_quads(c->nw, &c->failure.graphics),
                  pass == 0 ? 537u : 1295u))
        return false;
    float brightness;
    if (mc_world_runtime_brightness(s->world, s->x, s->brightness_y, s->z, &brightness,
                                    &c->failure.brightness) != MC_WORLD_STORAGE_OK)
        return fail(c, MC_CLIENT_PRECIPITATION_CHILD_FAILED, c->failure.brightness.operation,
                    pass == 0 ? 548u : 1307u, c->failure.brightness.native_policy);
    if (!fp(c, pass == 0 ? 564u : 1312u))
        return false;
    if (pass != 0)
        brightness = at_fadd(at_fmul(brightness, 0.85f), 0.15f);
    const float alpha = at_fmul(
        at_fadd(at_fmul(at_fsub(1.0f, at_fmul(radial, radial)), pass == 0 ? 0.3f : 0.5f), 0.5f),
        s->strength);
    mc_client_display *d = s->owner->services.display;
    if (!display(c,
                 mc_client_display_source_color(d, brightness, brightness, brightness, alpha,
                                                &c->failure.display),
                 pass == 0 ? 574u : 1339u) ||
        !graphics(c,
                  mc_client_nw_translation(c->nw, at_mul(-s->render_x, 1.0),
                                           at_mul(-s->render_y, 1.0), at_mul(-s->render_z, 1.0),
                                           &c->failure.graphics),
                  pass == 0 ? 594u : 1359u))
        return false;
    static const unsigned int snow_pcs[] = {641, 688, 735, 782, 829, 876, 923, 970};
    static const unsigned int rain_pcs[] = {1400, 1441, 1482, 1523, 1564, 1605, 1646, 1687};
    for (unsigned int vertex = 0; vertex < 8; ++vertex) {
        const bool high = vertex == 2 || vertex == 3 || vertex == 6 || vertex == 7;
        const bool one = vertex == 1 || vertex == 2 || vertex == 5 || vertex == 6;
        const int32_t y = high ? s->upper : s->lower;
        const double x =
            vertex < 4 ? (double)mc_java_i32_add(s->x, one ? 1 : 0) : at_add((double)s->x, 0.5);
        const double z =
            vertex < 4 ? at_add((double)s->z, 0.5) : (double)mc_java_i32_add(s->z, one ? 1 : 0);
        const float u = at_fadd(at_fmul(one ? 1.0f : 0.0f, 1.0f), u_shift);
        float v = at_fadd(at_fdiv(at_fmul((float)y, 1.0f), 4.0f), at_fmul(scroll, 1.0f));
        if (pass == 0)
            v = at_fadd(v, v_shift);
        if (!graphics(c,
                      mc_client_nw_vertex_uv(c->nw, x, (double)y, z, (double)u, (double)v,
                                             &c->failure.graphics),
                      pass == 0 ? snow_pcs[vertex] : rain_pcs[vertex]))
            return false;
    }
    if (!graphics(c, mc_client_nw_translation(c->nw, 0.0, 0.0, 0.0, &c->failure.graphics),
                  pass == 0 ? 978u : 1695u) ||
        !graphics(c, mc_client_nw_draw(c->nw, d, &c->failure.graphics), pass == 0 ? 983u : 1700u))
        return false;
    if (pass == 0)
        ++s->snow_draws;
    else
        ++s->rain_draws;
    return true;
}

static bool traverse(mc_client_precipitation_call *c, unsigned int pass) {
    mc_client_precipitation_state *s = &c->state;
    size_t index = 0;
    const int32_t end_x = mc_java_i32_add(s->camera_x, s->radius);
    const int32_t end_z = mc_java_i32_add(s->camera_z, s->radius);
    for (s->x = mc_java_i32_sub(s->camera_x, s->radius); s->x <= end_x;
         s->x = mc_java_i32_add(s->x, 1)) {
        for (s->z = mc_java_i32_sub(s->camera_z, s->radius); s->z <= end_z;
             s->z = mc_java_i32_add(s->z, 1)) {
            const size_t cell = index++;
            if (s->biomes == NULL || cell >= mc_biome_reference_array_length(s->biomes))
                return fail(c, MC_CLIENT_PRECIPITATION_CHILD_FAILED,
                            "precipitation biome array access", pass == 0 ? 281u : 1077u, true);
            const mc_biome *biome = mc_biome_reference_array_data(s->biomes)[cell];
            const mc_biome_fields *fields = mc_biome_fields_get(biome);
            if (fields == NULL)
                return fail(c, MC_CLIENT_PRECIPITATION_SOURCE_NULL,
                            "precipitation Biome predicate receiver", pass == 0 ? 286u : 1082u,
                            false);
            if (pass == 0)
                ++s->snow_cells;
            else
                ++s->rain_cells;
            if ((pass == 0 ? fields->v : !fields->v && fields->w) && !column(c, pass))
                return false;
            /* The source counter wraps. Refuse an unbounded native traversal
             * only after completing the reached column at INT32_MAX. */
            if (s->z == INT32_MAX)
                return fail(c, MC_CLIENT_PRECIPITATION_CHILD_FAILED,
                            "precipitation wrapped Z traversal", pass == 0 ? 986u : 1703u, true);
        }
        if (s->x == INT32_MAX)
            return fail(c, MC_CLIENT_PRECIPITATION_CHILD_FAILED,
                        "precipitation wrapped X traversal", pass == 0 ? 992u : 1709u, true);
    }
    return true;
}

static void body(mc_client_precipitation_call *c) {
    mc_client_precipitation_state *s = &c->state;
    if (s->owner == NULL) {
        fail(c, MC_CLIENT_PRECIPITATION_SOURCE_NULL, "precipitation PX receiver", 1, false);
        return;
    }
    client_application *a = app(c, 4);
    if (a == NULL)
        return;
    if (a->active_world == NULL) {
        fail(c, MC_CLIENT_PRECIPITATION_SOURCE_NULL, "precipitation strength World receiver", 8,
             false);
        return;
    }
    if (mc_world_runtime_rain_strength(a->active_world, s->partial, &s->strength,
                                       &c->failure.world) != MC_WORLD_STORAGE_OK) {
        fail(c, MC_CLIENT_PRECIPITATION_CHILD_FAILED, c->failure.world.operation, 8, true);
        return;
    }
    s->strength_read = true;
    if (!fp(c, 14))
        return;
    /* fcmpg takes the draw branch for NaN as well as positive strength. */
    if (s->strength <= 0.0f) {
        s->returned = true;
        s->source_pc = 18;
        return;
    }
    a = app(c, 23);
    if (a == NULL)
        return;
    s->camera = a->i;
    if (s->camera != NULL) {
        c->failure.entity = mc_entity_retain(s->camera);
        if (c->failure.entity != MC_ENTITY_OK) {
            fail(c, MC_CLIENT_PRECIPITATION_CHILD_FAILED, "precipitation camera occurrence", 26,
                 true);
            return;
        }
        c->camera_held = true;
    }
    s->camera_captured = true;
    a = app(c, 31);
    if (a == NULL)
        return;
    s->world = a->active_world;
    if (s->world != NULL) {
        if (!mc_world_runtime_binding_retain(s->world)) {
            fail(c, MC_CLIENT_PRECIPITATION_CHILD_FAILED, "precipitation World occurrence", 34,
                 true);
            return;
        }
        c->world_held = true;
    }
    s->world_captured = true;
    if (s->camera == NULL) {
        fail(c, MC_CLIENT_PRECIPITATION_SOURCE_NULL, "precipitation camera X receiver", 37, false);
        return;
    }
    if (!math_use(c, 40))
        return;
    s->camera_x = mc_math_floor_double(s->camera->base.x);
    if (!math_use(c, 49))
        return;
    s->camera_y = mc_math_floor_double(s->camera->base.y);
    if (!math_use(c, 58))
        return;
    s->camera_z = mc_math_floor_double(s->camera->base.z);
    if (mc_client_render_engine_classes_retain(s->owner->services.graphics) !=
        MC_CLIENT_RENDER_ENGINE_OK) {
        fail(c, MC_CLIENT_PRECIPITATION_CHILD_FAILED, "precipitation graphics occurrence", 63,
             true);
        return;
    }
    c->graphics_held = true;
    if (!graphics(c,
                  mc_client_nw_active_use(s->owner->services.graphics,
                                          (mc_client_graphics_site){"px", "c", "(F)V", 63}, &c->nw,
                                          &c->failure.graphics),
                  63))
        return;
    mc_client_display *d = s->owner->services.display;
    if (!display(c, mc_client_display_source_cull_test(d, false, &c->failure.display), 71) ||
        !display(c, mc_client_display_source_normal(d, 0.0f, 1.0f, 0.0f, &c->failure.display),
                 77) ||
        !display(c, mc_client_display_source_blend_enable(d, true, &c->failure.display), 83) ||
        !display(c, mc_client_display_source_blend_function(d, 770, 771, &c->failure.display),
                 92) ||
        !display(c, mc_client_display_source_alpha_function(d, 516, 0.01f, &c->failure.display),
                 100) ||
        !texture(c, 0))
        return;
    mc_client_interpolation_error arithmetic;
    if (!mc_client_interpolate_double(s->camera->base.bl, s->camera->base.x, s->partial, 133, 135,
                                      136, 137, &s->render_x, &arithmetic) ||
        !mc_client_interpolate_double(s->camera->base.bm, s->camera->base.y, s->partial, 152, 154,
                                      155, 156, &s->render_y, &arithmetic) ||
        !mc_client_interpolate_double(s->camera->base.bn, s->camera->base.z, s->partial, 171, 173,
                                      174, 175, &s->render_z, &arithmetic)) {
        fail(c, MC_CLIENT_PRECIPITATION_UNSUPPORTED_FP, "precipitation interpolation profile",
             (unsigned int)arithmetic.pc, true);
        return;
    }
    if (!math_use(c, 180))
        return;
    s->eye_y = mc_math_floor_double(s->render_y);
    bool setting;
    if (!fancy(c, 192, 195, &setting))
        return;
    s->radius = setting ? 10 : 5;
    mc_world_runtime_fields world = {0};
    mc_world_provider_fields dimension;
    if (mc_world_runtime_fields_get(s->world, &world) != MC_WORLD_STORAGE_OK ||
        mc_world_provider_fields_get(world.t, &dimension) != MC_WORLD_PROVIDER_OK) {
        fail(c,
             s->world == NULL || world.t == NULL ? MC_CLIENT_PRECIPITATION_SOURCE_NULL
                                                 : MC_CLIENT_PRECIPITATION_CHILD_FAILED,
             "precipitation World biome-manager receiver", 207,
             s->world != NULL && world.t != NULL);
        return;
    }
    c->manager = dimension.b;
    if (c->manager == NULL) {
        fail(c, MC_CLIENT_PRECIPITATION_SOURCE_NULL, "precipitation biome-manager receiver", 232,
             false);
        return;
    }
    if (mc_world_chunk_manager_retain(c->manager) != MC_WORLD_PROVIDER_OK) {
        c->manager = NULL;
        fail(c, MC_CLIENT_PRECIPITATION_CHILD_FAILED, "precipitation biome-manager occurrence", 232,
             true);
        return;
    }
    mc_biome_reference_array *biomes = NULL;
    const size_t size = (size_t)(s->radius * 2 + 1);
    if (mc_world_chunk_manager_cached(c->manager, mc_java_i32_sub(s->camera_x, s->radius),
                                      mc_java_i32_sub(s->camera_z, s->radius), size, size, &biomes,
                                      &c->failure.biome) != MC_WORLD_PROVIDER_OK) {
        fail(c, MC_CLIENT_PRECIPITATION_CHILD_FAILED, c->failure.biome.operation, 232, true);
        return;
    }
    s->biomes = biomes;
    s->biomes_returned = true;
    if (!traverse(c, 0) || !texture(c, 1) || !fancy(c, 1020, 1023, &setting))
        return;
    if (setting)
        s->radius = 10; /* The second Settings read never reduces the radius. */
    if (!traverse(c, 1) ||
        !display(c, mc_client_display_source_cull_test(d, true, &c->failure.display), 1718) ||
        !display(c, mc_client_display_source_blend_enable(d, false, &c->failure.display), 1724) ||
        !display(c, mc_client_display_source_alpha_function(d, 516, 0.1f, &c->failure.display),
                 1732))
        return;
    s->state_restored = s->returned = true;
    s->source_pc = 1735;
}

static mc_client_precipitation_status admission(mc_client_precipitation_error *error,
                                                mc_client_precipitation_status status,
                                                const char *operation) {
    if (error != NULL)
        *error = (mc_client_precipitation_error){
            .status = status, .operation = operation, .native_policy = true};
    return status;
}

static mc_client_precipitation_status begin(mc_client_source_px *owner, mc_math_classes *math,
                                            float partial, mc_client_precipitation_call **out,
                                            mc_client_precipitation_error *error, bool entered) {
    if (out == NULL || *out != NULL || (entered && owner == NULL))
        return admission(error, MC_CLIENT_PRECIPITATION_ARGUMENT, "precipitation output slot");
    if (owner != NULL &&
        (owner->active != entered || owner->phase == MC_CLIENT_ENTITY_RENDERER_CONSTRUCTING ||
         owner->phase == MC_CLIENT_ENTITY_RENDERER_RETIRING))
        return admission(error, MC_CLIENT_PRECIPITATION_BUSY, "precipitation PX lifetime guard");
    mc_client_precipitation_call *c = calloc(1, sizeof(*c));
    if (c == NULL)
        return admission(error, MC_CLIENT_PRECIPITATION_ALLOCATION,
                         "precipitation call allocation");
    c->state.owner = owner;
    c->state.partial = partial;
    c->state.failure = &c->failure;
    c->math = math;
    c->entered = entered;
    if (!entered && owner != NULL) {
        if (mc_client_entity_renderer_retain(owner, &c->owner_error) !=
            MC_CLIENT_ENTITY_RENDERER_OK) {
            free(c);
            return admission(error, MC_CLIENT_PRECIPITATION_BUSY, "precipitation PX occurrence");
        }
        c->owner_held = true;
        owner->active = true;
    }
    *out = c;
    c->state.active = true;
    body(c);
    c->state.active = false;
    if (!entered && owner != NULL)
        owner->active = false;
    if (error != NULL)
        *error = c->failure;
    return c->failure.status;
}

mc_client_precipitation_status
mc_client_entity_renderer_precipitation_begin(mc_client_source_px *owner, mc_math_classes *math,
                                              float partial, mc_client_precipitation_call **out,
                                              mc_client_precipitation_error *error) {
    return begin(owner, math, partial, out, error, false);
}

mc_client_precipitation_status
entity_renderer_precipitation_entered(mc_client_source_px *owner, mc_math_classes *math,
                                      float partial, mc_client_precipitation_call **out,
                                      mc_client_precipitation_error *error) {
    return begin(owner, math, partial, out, error, true);
}

bool mc_client_entity_renderer_precipitation_inspect(const mc_client_precipitation_call *c,
                                                     mc_client_precipitation_state *out) {
    if (c == NULL || out == NULL || c->state.active)
        return false;
    *out = c->state;
    return true;
}

mc_client_precipitation_status
mc_client_entity_renderer_precipitation_retire(mc_client_precipitation_call **slot,
                                               mc_client_precipitation_error *error) {
    if (slot == NULL)
        return admission(error, MC_CLIENT_PRECIPITATION_ARGUMENT, "precipitation retirement slot");
    mc_client_precipitation_call *c = *slot;
    if (c == NULL)
        return admission(error, MC_CLIENT_PRECIPITATION_OK, NULL);
    if (c->state.active || (!c->entered && c->state.owner != NULL && c->state.owner->active))
        return admission(error, MC_CLIENT_PRECIPITATION_BUSY, "precipitation active retirement");
    for (unsigned int i = 0; i < 2; ++i) {
        if (c->engines[i] == NULL)
            continue;
        if (c->lookup_owned[i] && mc_client_render_engine_retire_lookup(
                                      c->engines[i], &c->texture_cleanup) != MC_CLIENT_LOOKUP_OK)
            return admission(error, MC_CLIENT_PRECIPITATION_BUSY,
                             "precipitation lookup retirement");
        c->lookup_owned[i] = false;
        if (mc_client_render_engine_release(c->engines[i]) != MC_CLIENT_RENDER_ENGINE_OK)
            return admission(error, MC_CLIENT_PRECIPITATION_BUSY, "precipitation Engine release");
        c->engines[i] = NULL;
    }
    if (c->manager != NULL) {
        mc_world_chunk_manager_release(c->manager);
        c->manager = NULL;
        c->state.biomes = NULL;
    }
    if (c->graphics_held) {
        if (mc_client_render_engine_classes_release(c->state.owner->services.graphics) !=
            MC_CLIENT_RENDER_ENGINE_OK)
            return admission(error, MC_CLIENT_PRECIPITATION_BUSY, "precipitation graphics release");
        c->graphics_held = false;
    }
    if (c->math_held && mc_math_classes_release(&c->math, &c->failure.math) != MC_MATH_CLASS_OK)
        return admission(error, MC_CLIENT_PRECIPITATION_BUSY, "precipitation Math release");
    c->math_held = false;
    if (c->camera_held) {
        mc_entity_release(c->state.camera);
        c->camera_held = false;
        c->state.camera = NULL;
    }
    if (c->world_held) {
        mc_world_runtime_binding_release(c->state.world);
        c->world_held = false;
        c->state.world = NULL;
    }
    if (c->owner_held) {
        mc_client_source_px *owner = c->state.owner;
        if (mc_client_entity_renderer_release(&owner, &c->owner_error) !=
            MC_CLIENT_ENTITY_RENDERER_OK)
            return admission(error, MC_CLIENT_PRECIPITATION_BUSY, "precipitation PX release");
        c->owner_held = false;
    }
    free(c);
    *slot = NULL;
    return admission(error, MC_CLIENT_PRECIPITATION_OK, NULL);
}
