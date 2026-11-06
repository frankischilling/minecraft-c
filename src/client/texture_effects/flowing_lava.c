#include "effect_internal.h"

bool mc_texture_flowing_lava_tick(mc_texture_effect_owner *o, mc_texture_effect_error *e) {
    o->counter = mc_java_i32_add(o->counter, 1);
    o->last_store_pc = 7;
    if (!mc_texture_effect_fp(e, "if", 29))
        return false;
    for (int32_t x = 0; x < 16; ++x)
        for (int32_t y = 0; y < 16; ++y) {
            if (o->services.trig == NULL) {
                mc_texture_effect_fail(e, MC_TEXTURE_EFFECT_UNIMPLEMENTED,
                                       "if actual in lookup prerequisite", "if", "a", 38);
                return false;
            }
            float angle = ef_div(ef_mul(ef_mul((float)y, 0x1.921fb6p+1f), 2.0f), 16.0f);
            const int32_t dx =
                mc_java_float_to_i32(ef_mul(mc_math_sin(o->services.trig, angle), 0x1.333334p+0f));
            angle = ef_div(ef_mul(ef_mul((float)x, 0x1.921fb6p+1f), 2.0f), 16.0f);
            const int32_t dy =
                mc_java_float_to_i32(ef_mul(mc_math_sin(o->services.trig, angle), 0x1.333334p+0f));
            float sum = 0.0f;
            for (int32_t nx = x - 1; nx <= x + 1; ++nx)
                for (int32_t ny = y - 1; ny <= y + 1; ++ny) {
                    const size_t at = ((uint32_t)mc_java_i32_add(nx, dx) & 15u) +
                                      ((uint32_t)mc_java_i32_add(ny, dy) & 15u) * 16u;
                    float value;
                    if (!mc_texture_effect_load(o, o->g, o->g_length, at, 125, &value, e))
                        return false;
                    sum = ef_add(sum, value);
                }
            const size_t at = (size_t)(x + y * 16);
            const float divided = ef_div(sum, 10.0f);
            const size_t indices[] = {at, ((uint32_t)(x + 1) & 15u) + (uint32_t)y * 16u,
                                      ((uint32_t)(x + 1) & 15u) + ((uint32_t)(y + 1) & 15u) * 16u,
                                      (uint32_t)x + ((uint32_t)(y + 1) & 15u) * 16u};
            static const int32_t load_pcs[] = {174, 195, 217, 239};
            float neighbors = 0.0f;
            for (size_t n = 0; n < 4; ++n) {
                float value;
                if (!mc_texture_effect_load(o, o->i, o->i_length, indices[n], load_pcs[n], &value,
                                            e))
                    return false;
                neighbors = n == 0 ? value : ef_add(neighbors, value);
            }
            if (!mc_texture_effect_store(
                    o, o->h, o->h_length, at,
                    ef_add(divided, ef_mul(ef_div(neighbors, 4.0f), 0x1.99999ap-1f)), 248, e))
                return false;
            float value, impulse;
            if (!mc_texture_effect_load(o, o->i, o->i_length, at, 260, &value, e) ||
                !mc_texture_effect_load(o, o->j, o->j_length, at, 271, &impulse, e) ||
                !mc_texture_effect_store(o, o->i, o->i_length, at,
                                         ef_add(value, ef_mul(impulse, 0x1.47ae14p-7f)), 276, e) ||
                !mc_texture_effect_load(o, o->i, o->i_length, at, 287, &value, e))
                return false;
            if (value < 0.0f && !mc_texture_effect_store(o, o->i, o->i_length, at, 0.0f, 304, e))
                return false;
            if (!mc_texture_effect_load(o, o->j, o->j_length, at, 316, &impulse, e) ||
                !mc_texture_effect_store(o, o->j, o->j_length, at, ef_sub(impulse, 0x1.eb851ep-5f),
                                         320, e))
                return false;
            double draw;
            if (!mc_texture_effect_random(o, 321, &draw, e))
                return false;
            if (draw < 0.005 && !mc_texture_effect_store(o, o->j, o->j_length, at, 1.5f, 343, e))
                return false;
        }
    float *old = o->h;
    const size_t old_length = o->h_length;
    o->h = o->g;
    o->h_length = o->g_length;
    o->last_store_pc = 366;
    o->g = old;
    o->g_length = old_length;
    o->last_store_pc = 371;
    static const int32_t pcs[] = {549, 562, 575, 586};
    for (int32_t pixel = 0; pixel < 256; ++pixel) {
        const size_t at =
            (uint32_t)mc_java_i32_sub(pixel, mc_java_i32_mul(o->counter / 3, 16)) & 255u;
        float value;
        if (!mc_texture_effect_load(o, o->g, o->g_length, at, 402, &value, e))
            return false;
        value = ef_mul(value, 2.0f);
        if (value > 1.0f)
            value = 1.0f;
        if (value < 0.0f)
            value = 0.0f;
        const int32_t red = mc_java_float_to_i32(ef_add(ef_mul(value, 100.0f), 155.0f));
        const int32_t green = mc_java_float_to_i32(ef_mul(ef_mul(value, value), 255.0f));
        float blue = ef_mul(value, value);
        blue = ef_mul(blue, value);
        blue = ef_mul(blue, value);
        if (!mc_texture_effect_pixel(o, (size_t)pixel, red, green,
                                     mc_java_float_to_i32(ef_mul(blue, 128.0f)), 255, pcs, e))
            return false;
    }
    return true;
}
