#include "entity_tick_internal.h"

static void at(wt_scope *s, unsigned int pc) {
    wt_at(s, "g()V", pc);
    s->error.loop_bytecode = pc;
}

static bool old_chunk(wt_scope *s, mc_entity *p, unsigned int read_pc, unsigned int present_pc,
                      unsigned int get_pc, unsigned int remove_pc) {
    at(s, read_pc);
    if (p == NULL)
        return wt_fail(s, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                       "World null deferred/dead Entity receiver");
    const int32_t x = p->base.bG, z = p->base.bI;
    if (!p->base.bF)
        return true;
    bool present;
    at(s, present_pc);
    if (!wt_present(s, x, z, present_pc, &present))
        return false;
    if (!present)
        return true;
    mc_chunk_state *state;
    at(s, get_pc);
    if (!wt_get(s, x, z, get_pc, &state))
        return false;
    at(s, remove_pc);
    return wt_chunk_remove(s, state, p, true, 0, remove_pc);
}

bool wt_loops(wt_scope *s) {
    int32_t i = 0;
    size_t size;
    mc_entity *p = NULL;
    for (;;) {
        s->error.weather_index = i;
        at(s, 7);
        if (!wt_list_size(s, s->world->fields.e, 7, &size))
            return false;
        if ((size_t)i >= size)
            break;
        if (!wt_work(s, 20) || !wt_list_get(s, s->world->fields.e, i, 20, 25, &p))
            return false;
        at(s, 30);
        bool ok = wt_tick(s, p, 30);
        if (ok) {
            at(s, 34);
            if (p->base.be) {
                /* Capture the live receiver and argument before Java i--. */
                mc_world_reference_list *list = s->world->fields.e;
                const int32_t index = i;
                i = mc_java_i32_sub(i, 1);
                s->error.weather_index = i;
                at(s, 48);
                ok = wt_list_remove(s, list, index, 48);
            }
        }
        mc_entity_release(p);
        p = NULL;
        if (!ok)
            return false;
        i = mc_java_i32_add(i, 1);
    }
    at(s, 68);
    if (!wt_list_remove_all(s, s->world->fields.b, s->world->fields.D, 68))
        return false;
    i = 0;
    for (;;) {
        s->error.deferred_index = i;
        at(s, 81);
        if (!wt_list_size(s, s->world->fields.D, 81, &size))
            return false;
        if ((size_t)i >= size)
            break;
        if (!wt_work(s, 94) || !wt_list_get(s, s->world->fields.D, i, 94, 99, &p))
            return false;
        const bool ok = old_chunk(s, p, 104, 125, 135, 139);
        mc_entity_release(p);
        p = NULL;
        if (!ok)
            return false;
        i = mc_java_i32_add(i, 1);
    }
    i = 0;
    for (;;) {
        s->error.deferred_index = i;
        at(s, 155);
        if (!wt_list_size(s, s->world->fields.D, 155, &size))
            return false;
        if ((size_t)i >= size)
            break;
        if (!wt_work(s, 169) || !wt_list_get(s, s->world->fields.D, i, 169, 174, &p))
            return false;
        at(s, 177);
        const bool ok = wt_observe_removed(s, p, 177);
        mc_entity_release(p);
        p = NULL;
        if (!ok)
            return false;
        i = mc_java_i32_add(i, 1);
    }
    at(s, 190);
    if (!wt_list_clear(s, s->world->fields.D, 190))
        return false;
    i = 0;
    for (;;) {
        s->error.loaded_index = i;
        at(s, 202);
        if (!wt_list_size(s, s->world->fields.b, 202, &size))
            return false;
        if ((size_t)i >= size)
            break;
        if (!wt_work(s, 215) || !wt_list_get(s, s->world->fields.b, i, 215, 220, &p))
            return false;
        at(s, 225);
        bool ok = true, skip = false;
        if (p == NULL)
            ok = wt_fail(s, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                         "World loaded null Entity mount receiver");
        else if (p->base.aH != NULL) {
            mc_entity *mount = NULL;
            if (!wt_relationship(s, p->base.aH, p, 235, &mount))
                ok = false;
            else {
                skip = !mount->base.be && mount->base.aG == p;
                if (!skip) {
                    at(s, 257);
                    mount->base.aG = NULL;
                    p->base.aH = NULL;
                }
                mc_entity_release(mount);
            }
        }
        if (ok && !skip) {
            if (!p->base.be) {
                at(s, 274);
                ok = wt_update(s, p, true, true);
            }
            if (ok && p->base.be) {
                ok = old_chunk(s, p, 288, 306, 316, 320);
                if (ok) {
                    mc_world_reference_list *list = s->world->fields.b;
                    const int32_t index = i;
                    i = mc_java_i32_sub(i, 1);
                    s->error.loaded_index = i;
                    at(s, 331);
                    ok = wt_list_remove(s, list, index, 331);
                    if (ok) {
                        at(s, 339);
                        ok = wt_observe_removed(s, p, 339);
                    }
                }
            }
        }
        mc_entity_release(p);
        p = NULL;
        if (!ok)
            return false;
        i = mc_java_i32_add(i, 1);
    }
    return wt_tile_active(s);
}
