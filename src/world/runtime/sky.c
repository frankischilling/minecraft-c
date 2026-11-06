#include "mc/world/runtime/sky.h"

#include "../../entity/player/tick_math_internal.h"
#include "colors_internal.h"

#include <string.h>

mc_world_storage_status mc_world_runtime_sky_angle(mc_world_runtime *world, float partial,
                                                   float *out, mc_world_color_error *error) {
    color_frame frame;
    if (!mc_world_color_begin(&frame, world, NULL, out, "b", "(F)F"))
        return mc_world_color_finish(&frame, error);
    float value;
    if (mc_world_color_angle(&frame, partial, &value, 0)) {
        *out = value;
        frame.error.site.bytecode = 15;
    } else {
        frame.error.site.bytecode =
            frame.error.child.class_name != NULL && strcmp(frame.error.child.class_name, "fd") == 0
                ? frame.error.child.bytecode
                : 12;
    }
    return mc_world_color_finish(&frame, error);
}

mc_world_storage_status mc_world_runtime_sky_rain_strength(mc_world_runtime *world, float partial,
                                                           float *out,
                                                           mc_world_color_error *error) {
    color_frame frame;
    if (!mc_world_color_begin(&frame, world, NULL, out, "g", "(F)F"))
        return mc_world_color_finish(&frame, error);
    float value;
    if (mc_world_color_weather(&frame, partial, false, &value, 0)) {
        *out = value;
        frame.error.site.bytecode = 16;
    }
    return mc_world_color_finish(&frame, error);
}

mc_world_storage_status mc_world_runtime_star_brightness(mc_world_runtime *world, float partial,
                                                         mc_math_classes *math, float *out,
                                                         mc_world_color_error *error) {
    color_frame frame;
    if (!mc_world_color_begin(&frame, world, math, out, "e", "(F)F"))
        return mc_world_color_finish(&frame, error);
    float angle;
    if (!mc_world_color_angle(&frame, partial, &angle, 2) || !mc_world_color_fp(&frame, 10))
        goto done;
    const float argument = at_fmul(at_fmul(angle, 3.1415927f), 2.0f);
    if (!mc_world_color_math_ready(&frame, 13))
        goto done;
    float value = at_fsub(
        1.0f, at_fadd(at_fmul(mc_math_cos(mc_math_classes_table(math), argument), 2.0f), 0.75f));
    if (value < 0.0f)
        value = 0.0f;
    if (value > 1.0f)
        value = 1.0f;
    *out = at_fmul(at_fmul(value, value), 0.5f);
    frame.error.site.bytecode = 45;
done:
    return mc_world_color_finish(&frame, error);
}
