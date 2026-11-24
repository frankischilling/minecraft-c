#include "tick_internal.h"

static bool solid(mc_item_tick_scope *s, int32_t x, int32_t y, int32_t z, bool *out,
                  unsigned int pc, mc_item_entity_tick_error *e) {
    e->pushout_bytecode = pc;
    mc_world_runtime *world = s->entity->base.world;
    if (!mc_item_tick_world(s, world, e))
        return false;
    mc_item_tick_child(e, MC_ITEM_TICK_CHILD_SOLID);
    return mc_item_tick_world_result(
        e, mc_world_runtime_solid_support_get(world, x, y, z, out, &e->child_error.solid),
        "Item inherited push-out actual World.h");
}

bool mc_item_tick_pushout(mc_item_tick_scope *s, double x, double y, double z,
                          mc_item_entity_tick_error *e) {
    e->pushout_bytecode = 1;
    if (!mc_item_tick_fp(e))
        return false;
    const int32_t ix = mc_math_floor_double(x), iy = mc_math_floor_double(y),
                  iz = mc_math_floor_double(z);
    const double dx = mc_travel_dsub(x, (double)ix), dy = mc_travel_dsub(y, (double)iy),
                 dz = mc_travel_dsub(z, (double)iz);
    bool value;
    if (!solid(s, ix, iy, iz, &value, 51, e))
        return false;
    if (!value)
        return true; /* Actual false return442/443, discarded by hl196. */
    const int32_t nx[] = {mc_java_i32_sub(ix, 1), mc_java_i32_add(ix, 1), ix, ix, ix, ix};
    const int32_t ny[] = {iy, iy, mc_java_i32_sub(iy, 1), mc_java_i32_add(iy, 1), iy, iy};
    const int32_t nz[] = {iz, iz, iz, iz, mc_java_i32_sub(iz, 1), mc_java_i32_add(iz, 1)};
    static const unsigned int calls[] = {69, 94, 119, 144, 169, 194};
    bool open[6];
    for (size_t i = 0; i < 6; ++i) {
        if (!solid(s, nx[i], ny[i], nz[i], &value, calls[i], e))
            return false;
        open[i] = !value;
    }
    int direction = -1;
    double nearest = 9999.0;
    const double offsets[] = {dx, dy, dz};
    for (size_t i = 0; i < 6; ++i) {
        if (open[i]) {
            e->pushout_bytecode = (unsigned int)(224 + 22 * i);
            if (!mc_item_tick_fp(e))
                return false;
            const double distance =
                (i & 1u) == 0 ? offsets[i / 2] : mc_travel_dsub(1.0, offsets[i / 2]);
            if (distance < nearest) {
                nearest = distance;
                direction = (int)i;
            }
        }
    }
    (void)nearest;
    e->pushout_bytecode = 351;
    if (s->entity->random == NULL)
        return mc_item_tick_fail(e, MC_ENTITY_STATE_ERROR, "Item push-out null Random receiver");
    /* The original draw occurs even when all six neighbors are solid. */
    const float draw = mc_java_random_next_float(s->entity->random);
    if (!mc_item_tick_fp(e))
        return false;
    const float speed = mc_travel_fadd(mc_travel_fmul(draw, .2f), .1f);
    if (direction == 0)
        s->entity->base.velocity_x = mc_item_tick_widen(mc_item_tick_negate(speed));
    if (direction == 1)
        s->entity->base.velocity_x = mc_item_tick_widen(speed);
    if (direction == 2)
        s->entity->base.velocity_y = mc_item_tick_widen(mc_item_tick_negate(speed));
    if (direction == 3)
        s->entity->base.velocity_y = mc_item_tick_widen(speed);
    if (direction == 4)
        s->entity->base.velocity_z = mc_item_tick_widen(mc_item_tick_negate(speed));
    if (direction == 5)
        s->entity->base.velocity_z = mc_item_tick_widen(speed);
    return true;
}
