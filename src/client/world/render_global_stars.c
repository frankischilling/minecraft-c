#include "../../entity/player/tick_math_internal.h"
#include "mc/core/math/strict.h"
#include "mc/core/math/trig_double.h"
#include "render_global_internal.h"

bool mc_render_global_stars(mc_client_render_global *o) {
    o->failure.source_method = "g";
    o->failure.source_descriptor = "()V";
    o->star_pc = 0;
    o->star_random = mc_client_render_arena_allocate(&o->arena, sizeof(*o->star_random),
                                                     (mc_client_graphics_site){"n", "g", "()V", 0},
                                                     &o->failure.graphics);
    if (o->star_random == NULL)
        return mc_render_global_graphics(o, 0);
    mc_java_random_seed(o->star_random, INT64_C(10842)); /* Actual explicit Random ctor7. */
    o->star_pc = 7;
    if (!mc_client_nw_active_use(o->classes, (mc_client_graphics_site){"n", "g", "()V", 11},
                                 &o->tessellator, &o->failure.graphics))
        return mc_render_global_graphics(o, 11);
    o->star_pc = 16;
    if (!mc_client_nw_begin_quads(o->tessellator, &o->failure.graphics))
        return mc_render_global_graphics(o, 16);
    for (int32_t attempt = 0; attempt < 1500; ++attempt) {
        o->star_pc = 29;
        if (!mc_render_global_fp(o, "g", 29))
            return false;
        /* Source nextFloat, binary32 multiply/subtract, then f2d. The size
         * expression likewise completes in binary32 before its widening. */
        double x =
            at_widen(at_fsub(at_fmul(mc_java_random_next_float(o->star_random), 2.0f), 1.0f));
        double y =
            at_widen(at_fsub(at_fmul(mc_java_random_next_float(o->star_random), 2.0f), 1.0f));
        double z =
            at_widen(at_fsub(at_fmul(mc_java_random_next_float(o->star_random), 2.0f), 1.0f));
        const double size =
            at_widen(at_fadd(0.25f, at_fmul(mc_java_random_next_float(o->star_random), 0.25f)));
        ++o->star_attempts;
        const double distance = at_add(at_add(at_mul(x, x), at_mul(y, y)), at_mul(z, z));
        if (!(distance < 1.0 && distance > 0x1.47ae147ae147bp-7))
            continue;
        ++o->accepted_stars;
        o->star_pc = 112;
        const double inverse = at_div(1.0, mc_math_strict_sqrt(distance));
        x = at_mul(x, inverse);
        y = at_mul(y, inverse);
        z = at_mul(z, inverse);
        const double center_x = at_mul(x, 100.0);
        const double center_y = at_mul(y, 100.0);
        const double center_z = at_mul(z, 100.0);
        o->star_pc = 167;
        const double yaw = mc_player_tick_atan2(x, z);
        o->star_pc = 174;
        const double sin_yaw = mc_math_native_double_sin(yaw);
        if (!mc_render_global_fp(o, "g", 181))
            return false;
        const double cos_yaw = mc_math_native_double_cos(yaw);
        if (!mc_render_global_fp(o, "g", 190))
            return false;
        const double pitch =
            mc_player_tick_atan2(mc_math_strict_sqrt(at_add(at_mul(x, x), at_mul(z, z))), y);
        o->star_pc = 209;
        const double sin_pitch = mc_math_native_double_sin(pitch);
        if (!mc_render_global_fp(o, "g", 216))
            return false;
        const double cos_pitch = mc_math_native_double_cos(pitch);
        if (!mc_render_global_fp(o, "g", 222))
            return false;
        const double roll =
            at_mul(at_mul(mc_java_random_next_double(o->star_random), 0x1.921fb54442d18p+1), 2.0);
        o->star_pc = 237;
        const double sin_roll = mc_math_native_double_sin(roll);
        if (!mc_render_global_fp(o, "g", 244))
            return false;
        const double cos_roll = mc_math_native_double_cos(roll);
        if (!mc_render_global_fp(o, "g", 270))
            return false;
        for (int32_t corner = 0; corner < 4; ++corner) {
            const double source_zero = 0.0;
            const double side_y = at_mul((double)((corner & 2) - 1), size);
            const double side_z = at_mul((double)(((corner + 1) & 2) - 1), size);
            const double prior_x = source_zero;
            const double rolled_y = at_sub(at_mul(side_y, cos_roll), at_mul(side_z, sin_roll));
            const double rolled_z = at_add(at_mul(side_z, cos_roll), at_mul(side_y, sin_roll));
            const double prior_z = rolled_z;
            const double pitched_y =
                at_add(at_mul(rolled_y, sin_pitch), at_mul(prior_x, cos_pitch));
            const double pitched_x =
                at_sub(at_mul(prior_x, sin_pitch), at_mul(rolled_y, cos_pitch));
            const double final_x = at_sub(at_mul(pitched_x, sin_yaw), at_mul(prior_z, cos_yaw));
            const double final_y = pitched_y;
            const double final_z = at_add(at_mul(prior_z, sin_yaw), at_mul(pitched_x, cos_yaw));
            o->star_pc = 393;
            if (!mc_client_nw_vertex(o->tessellator, at_add(center_x, final_x),
                                     at_add(center_y, final_y), at_add(center_z, final_z),
                                     &o->failure.graphics))
                return mc_render_global_graphics(o, 393);
            ++o->star_vertices;
        }
    }
    o->star_pc = 409;
    if (!mc_client_nw_draw(o->tessellator, o->display, &o->failure.graphics))
        return mc_render_global_graphics(o, 409);
    o->star_pc = 412;
    return true;
}
