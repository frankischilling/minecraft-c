#include "active_internal.h"

static bool solid(mc_active_scope *s, int32_t x, int32_t y, int32_t z, bool *out,
                  mc_player_tick_error *e, unsigned int call) {
    e->pushout_bytecode = call;
    e->solid_bytecode = 7;
    mc_world_runtime *world = s->player->base.world;
    if (world == NULL)
        return mc_active_fail(e, MC_ENTITY_STATE_ERROR, "Push-out private d null World receiver");
    return mc_active_world_result(
        e, mc_world_runtime_solid_support_get(world, x, y, z, out, &e->solid),
        "Push-out actual World.h child");
}

bool mc_active_pushout(mc_active_scope *s, double x, double y, double z, mc_player_tick_error *e) {
    e->source_class = "dc";
    e->source_method = "c(DDD)Z";
    e->pushout_bytecode = 1;
    if (!mc_active_fp(e))
        return false;
    const int32_t ix = mc_math_floor_double(x), iy = mc_math_floor_double(y),
                  iz = mc_math_floor_double(z);
    const double dx = at_sub(x, (double)ix), dz = at_sub(z, (double)iz);
    bool value;
    if (!solid(s, ix, iy, iz, &value, e, 41))
        return false;
    if (!value) {
        if (!solid(s, ix, mc_java_i32_add(iy, 1), iz, &value, e, 56))
            return false;
        if (!value)
            return true;
    } /* Source returns false; caller discards it. */
    bool open[4];
    const int32_t nx[] = {mc_java_i32_sub(ix, 1), mc_java_i32_add(ix, 1), ix, ix};
    const int32_t nz[] = {iz, iz, mc_java_i32_sub(iz, 1), mc_java_i32_add(iz, 1)};
    static const unsigned int first[] = {71, 110, 149, 188}, second[] = {88, 127, 166, 205};
    for (size_t i = 0; i < 4; ++i) {
        if (!solid(s, nx[i], iy, nz[i], &value, e, first[i]))
            return false;
        open[i] = false;
        if (!value) {
            if (!solid(s, nx[i], mc_java_i32_add(iy, 1), nz[i], &value, e, second[i]))
                return false;
            open[i] = !value;
        }
    }
    double nearest = 9999.0;
    int direction = -1;
    if (open[0]) {
        e->pushout_bytecode = 235;
        if (!mc_active_fp(e))
            return false;
        if (dx < nearest) {
            nearest = dx;
            direction = 0;
        }
    }
    if (open[1]) {
        e->pushout_bytecode = 254;
        if (!mc_active_fp(e))
            return false;
        if (at_sub(1.0, dx) < nearest) {
            nearest = at_sub(1.0, dx);
            direction = 1;
        }
    }
    if (open[2]) {
        e->pushout_bytecode = 279;
        if (!mc_active_fp(e))
            return false;
        if (dz < nearest) {
            nearest = dz;
            direction = 4;
        }
    }
    if (open[3]) {
        e->pushout_bytecode = 298;
        if (!mc_active_fp(e))
            return false;
        if (at_sub(1.0, dz) < nearest) {
            nearest = at_sub(1.0, dz);
            direction = 5;
        }
    }
    (void)nearest;
    if (direction == 0)
        s->player->base.velocity_x = at_widen(-.1f);
    if (direction == 1)
        s->player->base.velocity_x = at_widen(.1f);
    if (direction == 4)
        s->player->base.velocity_z = at_widen(-.1f);
    if (direction == 5)
        s->player->base.velocity_z = at_widen(.1f);
    return true;
}
