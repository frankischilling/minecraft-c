#include "orientation.h"

bool mc_texture_compass_tick(mc_texture_effect_owner *o, mc_texture_effect_error *e) {
    static const int32_t background_pc[] = {146, 159, 172, 184};
    for (size_t pixel = 0; pixel < 256; ++pixel) {
        if (o->icon == NULL) {
            mc_texture_effect_fail(e, MC_TEXTURE_EFFECT_NULL_REFERENCE, "av actual h iaload", "av",
                                   "a", 14);
            return false;
        }
        const uint32_t argb = o->icon[pixel];
        if (!mc_texture_effect_pixel(o, pixel, (int32_t)((argb >> 16) & 255u),
                                     (int32_t)((argb >> 8) & 255u), (int32_t)(argb & 255u),
                                     (int32_t)(argb >> 24), background_pc, e))
            return false;
    }
    double target = 0.0;
    client_application *app;
    if (!effect_app(o, 197, &app, e))
        return false;
    if (app->active_world != NULL) {
        if (!effect_app(o, 207, &app, e))
            return false;
        if (app->h != NULL) {
            if (!effect_app(o, 217, &app, e))
                return false;
            mc_world_runtime *world = app->active_world;
            if (world == NULL) {
                mc_texture_effect_fail(e, MC_TEXTURE_EFFECT_NULL_REFERENCE,
                                       "av actual fd.u receiver", "av", "a", 220);
                return false;
            }
            /* Bounded native retention: an earlier call's local br can be
             * retired before this next actual allocation. No source field is
             * cleared and no World/Player state is inferred or rolled back. */
            free(o->coordinate);
            o->coordinate = calloc(1, sizeof(*o->coordinate));
            if (o->coordinate == NULL) {
                mc_texture_effect_fail(e, MC_TEXTURE_EFFECT_ALLOCATION_FAILED, "actual fd.u new br",
                                       "fd", "u", 0);
                e->source_descriptor = "()Lbr;";
                e->enclosing_pc = 220;
                return false;
            }
            int32_t values[3];
            mc_world_spawn_error child = {0};
            if (!mc_world_source_spawn_values(world, values, &child)) {
                e->spawn = child;
                e->world.status = child.world;
                mc_texture_effect_fail(e, MC_TEXTURE_EFFECT_WORLD_FAILED,
                                       "actual fd.u Info argument", "fd", "u", (int32_t)child.pc);
                e->source_descriptor = "()Lbr;";
                e->enclosing_pc = 220;
                return false;
            }
            o->coordinate->x = values[0];
            o->coordinate->y = values[1];
            o->coordinate->z = values[2];
            if (!mc_texture_effect_fp(e, "av", 228))
                return false;
            if (!effect_app(o, 233, &app, e))
                return false;
            if (app->h == NULL) {
                mc_texture_effect_fail(e, MC_TEXTURE_EFFECT_NULL_REFERENCE,
                                       "av actual Player.aM receiver", "av", "a", 236);
                return false;
            }
            const double dx = ed_sub((double)o->coordinate->x, mc_entity_runtime_fields(app->h)->x);
            if (!effect_app(o, 251, &app, e))
                return false;
            if (app->h == NULL) {
                mc_texture_effect_fail(e, MC_TEXTURE_EFFECT_NULL_REFERENCE,
                                       "av actual Player.aO receiver", "av", "a", 254);
                return false;
            }
            const double dz = ed_sub((double)o->coordinate->z, mc_entity_runtime_fields(app->h)->z);
            if (!effect_app(o, 264, &app, e))
                return false;
            if (app->h == NULL) {
                mc_texture_effect_fail(e, MC_TEXTURE_EFFECT_NULL_REFERENCE,
                                       "av actual Player.aS receiver", "av", "a", 267);
                return false;
            }
            const float yaw = ef_sub(mc_entity_runtime_fields(app->h)->yaw, 90.0f);
            const double heading = ed_div(ed_mul(at_widen(yaw), 0x1.921fb54442d18p+1), 180.0);
            target = ed_sub(heading, mc_player_tick_atan2(dz, dx));
            if (!effect_app(o, 295, &app, e))
                return false;
            bool randomize;
            if (!effect_provider_flag(o, app->active_world, 298, 301, &randomize, e))
                return false;
            if (randomize) {
                double draw;
                if (!mc_texture_effect_random(o, 307, &draw, e))
                    return false;
                target = ed_mul(ed_mul(draw, 0x1.921fb6p+1), 2.0);
            }
        }
    }
    if (!effect_damp(o, target, 324, 338, 391, 403, 416, e))
        return false;
    const double sine = mc_math_native_double_sin(o->angle);
    const double cosine = mc_math_native_double_cos(o->angle);
    static const int32_t cross_pc[] = {598, 612, 626, 640};
    for (int32_t step = -4; step <= 4; ++step) {
        const int32_t x =
            mc_java_double_to_i32(ed_add(8.5, ed_mul(ed_mul(cosine, (double)step), 0.3)));
        const int32_t y = mc_java_double_to_i32(
            ed_sub(7.5, ed_mul(ed_mul(ed_mul(sine, (double)step), 0.3), 0.5)));
        const int32_t at = mc_java_i32_add(mc_java_i32_mul(y, 16), x);
        if (!mc_texture_effect_pixel(o, (size_t)(uint32_t)at, 100, 100, 100, 255, cross_pc, e))
            return false;
    }
    static const int32_t needle_pc[] = {840, 854, 868, 882};
    for (int32_t step = -8; step <= 16; ++step) {
        const int32_t x =
            mc_java_double_to_i32(ed_add(8.5, ed_mul(ed_mul(sine, (double)step), 0.3)));
        const int32_t y = mc_java_double_to_i32(
            ed_add(7.5, ed_mul(ed_mul(ed_mul(cosine, (double)step), 0.3), 0.5)));
        const int32_t at = mc_java_i32_add(mc_java_i32_mul(y, 16), x);
        if (!mc_texture_effect_pixel(o, (size_t)(uint32_t)at, step >= 0 ? 255 : 100,
                                     step >= 0 ? 20 : 100, step >= 0 ? 20 : 100, 255, needle_pc, e))
            return false;
    }
    return true;
}
