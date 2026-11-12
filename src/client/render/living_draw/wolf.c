#include "internal.h"

bool ld_wolf_age(ld_frame *f, float partial, float *out) {
    (void)partial;
    mc_client_living_draw_error *e = f->error;
    const mc_client_graphics_site parent = e->parent;
    e->parent = ld_site("gv", "a", "(Lls;DDDFF)V", 147);
    ld_at(f, "we", "c", "(Lls;F)F", 2);
    if (!ld_cast_actor(f, MC_ENTITY_CLASS_KIND_WOLF, "Wolf age actual GI checkcast"))
        return false;
    ld_at(f, "we", "a", "(Lgi;F)F", 1);
    if (f->actor == NULL)
        return ld_null(f, "Wolf age null tail invoke receiver");
    if (!ld_entity(f, mc_entity_render_wolf_tail(f->actor, out, &e->detail.entity)))
        return false;
    ld_at(f, "we", "a", "(Lgi;F)F", 4);
    e->parent = parent;
    return true;
}

bool ld_wolf_prescale(ld_frame *f, float partial) {
    (void)partial;
    ld_at(f, "we", "a", "(Lls;F)V", 2);
    if (!ld_cast_actor(f, MC_ENTITY_CLASS_KIND_WOLF, "Wolf pre-scale actual GI checkcast"))
        return false;
    /* Actual WE.b(GI,F) return0 does not dereference a nullable GI. */
    ld_at(f, "we", "b", "(Lgi;F)V", 0);
    return true;
}
