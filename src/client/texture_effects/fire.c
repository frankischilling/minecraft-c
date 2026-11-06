#include "effect_internal.h"

bool mc_texture_fire_tick(mc_texture_effect_owner *o, mc_texture_effect_error *e) {
    for (int32_t x = 0; x < 16; ++x)
        for (int32_t y = 0; y < 20; ++y) {
            int32_t weight = 18;
            float value;
            if (!mc_texture_effect_load(o, o->g, o->g_length, (size_t)(x + ((y + 1) % 20) * 16), 34,
                                        &value, e) ||
                !mc_texture_effect_fp(e, "sd", 36))
                return false;
            float sum = ef_mul(value, (float)weight);
            for (int32_t nx = x - 1; nx <= x + 1; ++nx)
                for (int32_t ny = y; ny <= y + 1; ++ny) {
                    if (nx >= 0 && ny >= 0 && nx < 16 && ny < 20) {
                        if (!mc_texture_effect_load(o, o->g, o->g_length, (size_t)(nx + ny * 16),
                                                    110, &value, e))
                            return false;
                        sum = ef_add(sum, value);
                    }
                    ++weight; /* Source increments even for a rejected neighbor. */
                }
            const size_t at = (size_t)(x + y * 16);
            if (!mc_texture_effect_store(o, o->h, o->h_length, at,
                                         ef_div(sum, ef_mul((float)weight, 0x1.0f5c28p+0f)), 147,
                                         e))
                return false;
            if (y >= 19) {
                double a, b, c, d;
                if (!mc_texture_effect_random(o, 164, &a, e) ||
                    !mc_texture_effect_random(o, 167, &b, e))
                    return false;
                double product = ed_mul(a, b);
                if (!mc_texture_effect_random(o, 171, &c, e))
                    return false;
                product = ed_mul(ed_mul(product, c), 4.0);
                if (!mc_texture_effect_random(o, 179, &d, e))
                    return false;
                const double next =
                    ed_add(ed_add(product, ed_mul(d, 0x1.99999ap-4)), 0x1.99999ap-3);
                if (!mc_texture_effect_store(o, o->h, o->h_length, at, at_narrow(next), 192, e))
                    return false;
            }
        }
    float *old = o->h;
    const size_t old_length = o->h_length;
    o->h = o->g;
    o->h_length = o->g_length;
    o->last_store_pc = 215;
    o->g = old;
    o->g_length = old_length;
    o->last_store_pc = 220;
    static const int32_t pcs[] = {428, 441, 454, 467};
    for (size_t pixel = 0; pixel < 256; ++pixel) {
        float value;
        if (!mc_texture_effect_load(o, o->g, o->g_length, pixel, 237, &value, e))
            return false;
        value = ef_mul(value, 0x1.ccccccp+0f);
        if (value > 1.0f)
            value = 1.0f;
        if (value < 0.0f)
            value = 0.0f;
        const int32_t red = mc_java_float_to_i32(ef_add(ef_mul(value, 155.0f), 100.0f));
        const int32_t green = mc_java_float_to_i32(ef_mul(ef_mul(value, value), 255.0f));
        float blue = value;
        for (unsigned n = 0; n < 9; ++n)
            blue = ef_mul(blue, value);
        const int32_t b = mc_java_float_to_i32(ef_mul(blue, 255.0f));
        const int32_t alpha = value < 0.5f ? 0 : 255;
        volatile float discarded = ef_mul(ef_sub(value, 0.5f), 2.0f);
        (void)discarded;
        if (!mc_texture_effect_pixel(o, pixel, red, green, b, alpha, pcs, e))
            return false;
    }
    return true;
}
