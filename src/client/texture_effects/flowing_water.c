#include "effect_internal.h"

bool mc_texture_flowing_water_tick(mc_texture_effect_owner *o, mc_texture_effect_error *e) {
    o->counter = mc_java_i32_add(o->counter, 1);
    o->last_store_pc = 7;
    for (int32_t x = 0; x < 16; ++x)
        for (int32_t y = 0; y < 16; ++y) {
            float sum = 0.0f;
            for (int32_t neighbor = y - 2; neighbor <= y; ++neighbor) {
                float value;
                const size_t at = (uint32_t)x + ((uint32_t)neighbor & 15u) * 16u;
                if (!mc_texture_effect_load(o, o->g, o->g_length, at, 65, &value, e) ||
                    !mc_texture_effect_fp(e, "oh", 66))
                    return false;
                sum = ef_add(sum, value);
            }
            const size_t at = (size_t)(x + y * 16);
            const float divided = ef_div(sum, 0x1.99999ap+1f);
            float impulse;
            if (!mc_texture_effect_load(o, o->i, o->i_length, at, 98, &impulse, e))
                return false;
            if (!mc_texture_effect_store(o, o->h, o->h_length, at,
                                         ef_add(divided, ef_mul(impulse, 0x1.99999ap-1f)), 103, e))
                return false;
        }
    for (int32_t x = 0; x < 16; ++x)
        for (int32_t y = 0; y < 16; ++y) {
            const size_t at = (size_t)(x + y * 16);
            float value, impulse;
            if (!mc_texture_effect_load(o, o->i, o->i_length, at, 143, &value, e) ||
                !mc_texture_effect_load(o, o->j, o->j_length, at, 154, &impulse, e) ||
                !mc_texture_effect_store(o, o->i, o->i_length, at,
                                         ef_add(value, ef_mul(impulse, 0x1.99999ap-5f)), 159, e) ||
                !mc_texture_effect_load(o, o->i, o->i_length, at, 170, &value, e))
                return false;
            if (value < 0.0f && !mc_texture_effect_store(o, o->i, o->i_length, at, 0.0f, 187, e))
                return false;
            if (!mc_texture_effect_load(o, o->j, o->j_length, at, 199, &impulse, e) ||
                !mc_texture_effect_store(o, o->j, o->j_length, at, ef_sub(impulse, 0x1.333334p-2f),
                                         203, e))
                return false;
            double draw;
            if (!mc_texture_effect_random(o, 204, &draw, e))
                return false;
            if (draw < 0.2 && !mc_texture_effect_store(o, o->j, o->j_length, at, 0.5f, 226, e))
                return false;
        }
    float *old = o->h;
    const size_t old_length = o->h_length;
    o->h = o->g;
    o->h_length = o->g_length;
    o->last_store_pc = 249;
    o->g = old;
    o->g_length = old_length;
    o->last_store_pc = 254;
    static const int32_t pcs[] = {429, 442, 455, 468};
    for (int32_t pixel = 0; pixel < 256; ++pixel) {
        const size_t at = (uint32_t)mc_java_i32_sub(pixel, mc_java_i32_mul(o->counter, 16)) & 255u;
        float value;
        if (!mc_texture_effect_load(o, o->g, o->g_length, at, 283, &value, e))
            return false;
        if (value > 1.0f)
            value = 1.0f;
        if (value < 0.0f)
            value = 0.0f;
        const float squared = ef_mul(value, value);
        const int32_t red = mc_java_float_to_i32(ef_add(32.0f, ef_mul(squared, 32.0f)));
        const int32_t green = mc_java_float_to_i32(ef_add(50.0f, ef_mul(squared, 64.0f)));
        const int32_t alpha = mc_java_float_to_i32(ef_add(146.0f, ef_mul(squared, 50.0f)));
        if (!mc_texture_effect_pixel(o, (size_t)pixel, red, green, 255, alpha, pcs, e))
            return false;
    }
    return true;
}
