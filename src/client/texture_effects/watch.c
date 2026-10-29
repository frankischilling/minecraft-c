#include "orientation.h"

bool mc_texture_watch_tick(mc_texture_effect_owner *o, mc_texture_effect_error *e) {
    double target = 0.0;
    client_application *app;
    if (!effect_app(o, 6, &app, e))
        return false;
    if (app->active_world != NULL) {
        if (!effect_app(o, 16, &app, e))
            return false;
        if (app->h != NULL) {
            if (!effect_app(o, 26, &app, e))
                return false;
            if (app->active_world == NULL) {
                mc_texture_effect_fail(e, MC_TEXTURE_EFFECT_NULL_REFERENCE,
                                       "ku actual fd.b receiver", "ku", "a", 30);
                return false;
            }
            float celestial;
            if (mc_world_runtime_celestial_angle(app->active_world, 1.0f, &celestial, &e->world) !=
                MC_WORLD_STORAGE_OK) {
                mc_texture_effect_fail(e, MC_TEXTURE_EFFECT_WORLD_FAILED, "ku actual fd.b(1)", "ku",
                                       "a", 30);
                return false;
            }
            if (!mc_texture_effect_fp(e, "ku", 35))
                return false;
            target = at_widen(ef_mul(ef_mul(-celestial, 0x1.921fb6p+1f), 2.0f));
            if (!effect_app(o, 47, &app, e))
                return false;
            bool randomize;
            if (!effect_provider_flag(o, app->active_world, 50, 53, &randomize, e))
                return false;
            if (randomize) {
                double draw;
                if (!mc_texture_effect_random(o, 59, &draw, e))
                    return false;
                target = ed_mul(ed_mul(draw, 0x1.921fb6p+1), 2.0);
            }
        }
    }
    if (!effect_damp(o, target, 76, 90, 143, 155, 168, e))
        return false;
    const double sine = mc_math_native_double_sin(o->angle);
    const double cosine = mc_math_native_double_cos(o->angle);
    static const int32_t pcs[] = {547, 561, 575, 589};
    for (size_t pixel = 0; pixel < 256; ++pixel) {
        if (o->icon == NULL) {
            mc_texture_effect_fail(e, MC_TEXTURE_EFFECT_NULL_REFERENCE, "ku actual h iaload", "ku",
                                   "a", 206);
            return false;
        }
        const uint32_t argb = o->icon[pixel];
        int32_t alpha = (int32_t)(argb >> 24), red = (int32_t)((argb >> 16) & 255u),
                green = (int32_t)((argb >> 8) & 255u), blue = (int32_t)(argb & 255u);
        if (red == blue && green == 0 && blue > 0) {
            const double x = -ed_sub(ed_div((double)(pixel % 16), 15.0), 0.5);
            const double y = ed_sub(ed_div((double)(pixel / 16), 15.0), 0.5);
            const int32_t sx = mc_java_double_to_i32(
                ed_mul(ed_add(ed_add(ed_mul(x, cosine), ed_mul(y, sine)), 0.5), 16.0));
            const int32_t sy = mc_java_double_to_i32(
                ed_mul(ed_add(ed_sub(ed_mul(y, cosine), ed_mul(x, sine)), 0.5), 16.0));
            const size_t at = ((uint32_t)sx & 15u) + ((uint32_t)sy & 15u) * 16u;
            if (o->dial == NULL) {
                mc_texture_effect_fail(e, MC_TEXTURE_EFFECT_NULL_REFERENCE, "ku actual i iaload",
                                       "ku", "a", 383);
                return false;
            }
            const int32_t scale = red;
            const uint32_t dial = o->dial[at];
            alpha = (int32_t)(dial >> 24);
            red = (int32_t)((dial >> 16) & 255u) * scale / 255;
            green = (int32_t)((dial >> 8) & 255u) * scale / 255;
            blue = (int32_t)(dial & 255u) * scale / 255;
        }
        if (!mc_texture_effect_pixel(o, pixel, red, green, blue, alpha, pcs, e))
            return false;
    }
    return true;
}
