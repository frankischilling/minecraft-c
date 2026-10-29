#include "effect_internal.h"

bool mc_texture_portal_construct(mc_texture_effect_owner *o, mc_texture_effect_error *e) {
    o->counter = 0;
    o->last_store_pc = 12;
    o->pending_frames = mc_texture_effect_array(o, 32 * sizeof(*o->pending_frames), 21, e);
    if (o->pending_frames == NULL)
        return false;
    for (size_t n = 0; n < 32; ++n) {
        o->pending_frames[n] = mc_texture_effect_array(o, 1024, 21, e);
        if (o->pending_frames[n] == NULL)
            return false;
        ++o->pending_frame_count;
    }
    o->frames = o->pending_frames;
    o->frame_count = 32;
    o->pending_frames = NULL;
    o->pending_frame_count = 0;
    o->last_store_pc = 25;
    o->random = calloc(1, sizeof(*o->random));
    if (o->random == NULL) {
        mc_texture_effect_fail(e, MC_TEXTURE_EFFECT_ALLOCATION_FAILED, "hs actual new Random", "hs",
                               "<init>", 28);
        return false;
    }
    mc_java_random_seed(o->random, INT64_C(100)); /* Actual Random(long)35, no ordinary entropy. */
    o->random_constructed = true;
    if (!mc_texture_effect_fp(e, "hs", 82))
        return false;
    for (int32_t frame = 0; frame < 32; ++frame)
        for (int32_t x = 0; x < 16; ++x)
            for (int32_t y = 0; y < 16; ++y) {
                float value = 0.0f;
                for (int32_t side = 0; side < 2; ++side) {
                    const float offset = (float)(side * 8);
                    float dx = ef_mul(ef_div(ef_sub((float)x, offset), 16.0f), 2.0f);
                    float dy = ef_mul(ef_div(ef_sub((float)y, offset), 16.0f), 2.0f);
                    if (dx < -1.0f)
                        dx = ef_add(dx, 2.0f);
                    if (dx >= 1.0f)
                        dx = ef_sub(dx, 2.0f);
                    if (dy < -1.0f)
                        dy = ef_add(dy, 2.0f);
                    if (dy >= 1.0f)
                        dy = ef_sub(dy, 2.0f);
                    const float distance = ef_add(ef_mul(dx, dx), ef_mul(dy, dy));
                    const float polar = at_narrow(mc_player_tick_atan2(at_widen(dy), at_widen(dx)));
                    float spin = ef_mul(ef_mul(ef_div((float)frame, 32.0f), 0x1.921fb6p+1f), 2.0f);
                    spin = ef_sub(spin, ef_mul(distance, 10.0f));
                    spin = ef_add(spin, (float)(side * 2));
                    spin = ef_mul(spin, (float)(side * 2 - 1));
                    const float angle = ef_add(polar, spin);
                    if (o->services.trig == NULL) {
                        mc_texture_effect_fail(e, MC_TEXTURE_EFFECT_UNIMPLEMENTED,
                                               "hs actual in lookup prerequisite", "hs", "<init>",
                                               230);
                        return false;
                    }
                    float wave = ef_div(ef_add(mc_math_sin(o->services.trig, angle), 1.0f), 2.0f);
                    wave = ef_div(wave, ef_add(distance, 1.0f));
                    value = ef_add(value, ef_mul(wave, 0.5f));
                }
                value = ef_add(value, ef_mul(mc_java_random_next_float(o->random), 0x1.99999ap-4f));
                const int32_t red = mc_java_float_to_i32(ef_add(ef_mul(value, 100.0f), 155.0f));
                const int32_t green =
                    mc_java_float_to_i32(ef_add(ef_mul(ef_mul(value, value), 200.0f), 55.0f));
                float blue = ef_mul(value, value);
                blue = ef_mul(blue, value);
                blue = ef_mul(blue, value);
                blue = ef_mul(blue, 255.0f);
                const int32_t b = mc_java_float_to_i32(blue);
                const int32_t alpha = mc_java_float_to_i32(ef_add(ef_mul(value, 100.0f), 155.0f));
                const size_t index = (size_t)(y * 16 + x) * 4;
                uint8_t *target = o->frames[(size_t)frame];
                target[index] = (uint8_t)(uint32_t)green;
                o->last_store_pc = 353;
                target[index + 1] = (uint8_t)(uint32_t)b;
                o->last_store_pc = 369;
                target[index + 2] = (uint8_t)(uint32_t)red;
                o->last_store_pc = 385;
                target[index + 3] = (uint8_t)(uint32_t)alpha;
                o->last_store_pc = 401;
            }
    return true;
}

bool mc_texture_portal_tick(mc_texture_effect_owner *o, mc_texture_effect_error *e) {
    o->counter = mc_java_i32_add(o->counter, 1);
    o->last_store_pc = 7;
    const size_t frame = (uint32_t)o->counter & 31u;
    if (o->frames == NULL || frame >= o->frame_count || o->frames[frame] == NULL) {
        mc_texture_effect_fail(e,
                               o->frames == NULL ||
                                       (frame < o->frame_count && o->frames[frame] == NULL)
                                   ? MC_TEXTURE_EFFECT_NULL_REFERENCE
                                   : MC_TEXTURE_EFFECT_INDEX_OUT_OF_RANGE,
                               "hs actual h frame aaload", "hs", "a", 21);
        return false;
    }
    const uint8_t *bytes = o->frames[frame];
    static const int32_t pcs[] = {163, 176, 189, 202};
    for (size_t pixel = 0; pixel < 256; ++pixel)
        if (!mc_texture_effect_pixel(o, pixel, bytes[pixel * 4], bytes[pixel * 4 + 1],
                                     bytes[pixel * 4 + 2], bytes[pixel * 4 + 3], pcs, e))
            return false;
    return true;
}
