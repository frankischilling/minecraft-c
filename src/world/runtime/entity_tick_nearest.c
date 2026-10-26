#include "entity_tick_internal.h"

bool wt_nearest(wt_scope *s, mc_world_runtime *world, double x, double y, double z, double radius,
                mc_entity **out) {
    s->error.nearest_bytecode = 0;
    if (world == NULL)
        return wt_fail(s, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                       "Spawner nearest null World receiver");
    mc_world_storage_error cause = {0};
    if (mc_world_runtime_call_enter(world, &cause) != MC_WORLD_STORAGE_OK)
        return wt_cause(s, cause);
    mc_world_runtime *outer = s->world;
    s->world = world;
    double distance = -1.0;
    mc_entity *best = NULL;
    bool ok = false;
    for (int32_t i = 0;; i = mc_java_i32_add(i, 1)) {
        size_t size;
        s->error.nearest_bytecode = 17;
        if (!wt_list_size(s, world->fields.d, s->error.bytecode, &size))
            break;
        if ((size_t)i >= size) {
            ok = true;
            break;
        }
        s->error.nearest_bytecode = 31;
        if (!wt_work(s, s->error.bytecode))
            break;
        mc_entity *p = NULL;
        if (!wt_list_get(s, world->fields.d, i, s->error.bytecode, s->error.bytecode, &p))
            break;
        s->error.nearest_bytecode = 36;
        /* dc is the supplied actual concrete descendant of gs. No substitute
         * Player or cast from a same-shaped non-Player is admitted. */
        if (p != NULL && !mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER)) {
            mc_entity_release(p);
            wt_fail(s, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Nearest actual gs class cast");
            break;
        }
        s->error.nearest_bytecode = 47;
        if (p == NULL) {
            wt_fail(s, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Nearest null gs distance receiver");
            break;
        }
        if (!wt_fp(s, s->error.bytecode)) {
            mc_entity_release(p);
            break;
        }
        /* sn.g(DDD)D: x*x + y*y, then z*z, with each source operation. */
        const double dx = wt_sub(p->base.x, x), dy = wt_sub(p->base.y, y);
        const double dz = wt_sub(p->base.z, z);
        const double d = wt_add(wt_add(wt_mul(dx, dx), wt_mul(dy, dy)), wt_mul(dz, dz));
        if ((radius < 0.0 || d < wt_mul(radius, radius)) && (distance == -1.0 || d < distance)) {
            mc_entity_release(best);
            best = p;
            distance = d;
        } else
            mc_entity_release(p);
    }
    s->world = outer;
    mc_world_runtime_call_leave(world);
    if (!ok) {
        mc_entity_release(best);
        return false;
    }
    *out = best;
    s->error.nearest_bytecode = 0;
    return true;
}
