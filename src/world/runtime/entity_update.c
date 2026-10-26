#include "entity_tick_internal.h"

static void at(wt_scope *s, unsigned int pc) {
    wt_at(s, "a(Lsn;Z)V", pc);
    s->error.update_bytecode = pc;
}

static bool body(wt_scope *s, mc_entity *p, bool flag) {
    at(s, 1);
    if (p == NULL)
        return wt_fail(s, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                       "World update null Entity X receiver");
    const double x = p->base.x;
    at(s, 4);
    if (!wt_fp(s, 4))
        return false;
    const int32_t block_x = mc_math_floor_double(x);
    at(s, 9);
    const double z = p->base.z;
    const int32_t block_z = mc_math_floor_double(z);
    if (flag) {
        at(s, 48);
        bool present;
        mc_world_presence_error presence = {0};
        const mc_world_storage_status status = mc_world_runtime_region_present_held(
            s->world, mc_java_i32_sub(block_x, 32), 0, mc_java_i32_sub(block_z, 32),
            mc_java_i32_add(block_x, 32), 128, mc_java_i32_add(block_z, 32), &present, &presence);
        if (status != MC_WORLD_STORAGE_OK) {
            s->error.presence_bytecode = presence.bytecode;
            s->error.provider_bytecode = presence.provider_bytecode;
            return wt_cause(s, presence.cause);
        }
        if (!present) {
            at(s, 54);
            return true;
        }
    }
    p->base.bl = p->base.x;
    p->base.bm = p->base.y;
    p->base.bn = p->base.z;
    p->base.aU = p->base.yaw;
    p->base.aV = p->base.aT;
    if (flag && p->base.bF) {
        if (p->base.aH != NULL) {
            at(s, 114);
            if (!wt_riding(s, p, 114))
                return false;
        } else {
            at(s, 121);
            if (!wt_tick(s, p, 121))
                return false;
        }
    }
    s->error.virtual_class = s->error.virtual_method = s->error.riding_class = NULL;
    s->error.riding_bytecode = s->error.mount_bytecode = 0;
    s->error.mount_class = s->error.mount_method = NULL;
    /* The repair reads LIVE backup fields after the child, and deliberately
     * leaves bounds, velocity and a still-nonfinite backup alone. */
    at(s, 124);
    if (wt_nonfinite(p->base.x))
        p->base.x = p->base.bl;
    if (wt_nonfinite(p->base.y))
        p->base.y = p->base.bm;
    if (wt_nonfinite(p->base.z))
        p->base.z = p->base.bn;
    if (wt_nonfinite_float(p->base.aT))
        p->base.aT = p->base.aV;
    if (wt_nonfinite_float(p->base.yaw))
        p->base.yaw = p->base.aU;
    at(s, 275);
    if (!wt_fp(s, 275))
        return false;
    const int32_t cx = mc_math_floor_double(wt_div(p->base.x, 16.0));
    const int32_t cy = mc_math_floor_double(wt_div(p->base.y, 16.0));
    const int32_t cz = mc_math_floor_double(wt_div(p->base.z, 16.0));
    if (!p->base.bF || p->base.bG != cx || p->base.bH != cy || p->base.bI != cz) {
        if (p->base.bF) {
            at(s, 357);
            bool present;
            if (!wt_present(s, p->base.bG, p->base.bI, 357, &present))
                return false;
            if (present) {
                at(s, 372);
                mc_chunk_state *state;
                if (!wt_get(s, p->base.bG, p->base.bI, 372, &state))
                    return false;
                at(s, 377);
                const int32_t old_slice = p->base.bH;
                at(s, 380);
                if (!wt_chunk_remove(s, state, p, false, old_slice, 380))
                    return false;
            }
        }
        at(s, 388);
        bool present;
        if (!wt_present(s, cx, cz, 388, &present))
            return false;
        if (present) {
            p->base.bF = true;
            at(s, 404);
            mc_chunk_state *state;
            if (!wt_get(s, cx, cz, 404, &state))
                return false;
            at(s, 408);
            if (!wt_chunk_add(s, state, p, 408))
                return false;
        } else
            p->base.bF = false;
    }
    at(s, 419);
    return true;
}

static bool capacity(wt_scope *s) {
    if (s->parent_count < s->parent_capacity)
        return true;
    size_t next = s->parent_capacity == 0 ? 8 : s->parent_capacity * 2;
    if (next < s->parent_capacity || next > SIZE_MAX / sizeof(*s->parents))
        return wt_fail(s, MC_WORLD_STORAGE_LIMIT, "World passenger hold ledger extent");
    mc_entity **replacement = wt_allocate(s, next * sizeof(*replacement), 479);
    if (replacement == NULL)
        return false;
    if (s->parent_count != 0)
        memcpy(replacement, s->parents, s->parent_count * sizeof(*replacement));
    wt_free(s, s->parents, s->parent_capacity * sizeof(*s->parents));
    s->parents = replacement;
    s->parent_capacity = next;
    return true;
}

bool wt_update(wt_scope *s, mc_entity *entity, bool flag, bool wrapper) {
    mc_entity *current = entity;
    if (!wt_hold(s, current, 0))
        return false;
    const size_t start = s->parent_count;
    bool ok = false;
    for (;;) {
        if (wrapper)
            s->error.wrapper_bytecode = 3;
        /* Actual server dp overrides virtual a before the base presence or
         * backup code. It is an explicit reached dependency, not base reuse. */
        if (s->world->construction.entry == MC_WORLD_RUNTIME_SERVER_DEDICATED) {
            s->error.dedicated_bytecode = 4;
            wt_fail(s, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                    "Dedicated dp.a actual server.m/rider override dependency");
            break;
        }
        if (!body(s, current, flag))
            break;
        /* A presence-false return54 has no passenger continuation. */
        if (s->error.update_bytecode == 54) {
            ok = true;
            break;
        }
        if (!flag || !current->base.bF || current->base.aG == NULL) {
            ok = true;
            break;
        }
        at(s, 441);
        mc_entity *passenger;
        if (!wt_relationship(s, current->base.aG, current, 441, &passenger))
            break;
        const bool stale = passenger->base.be || passenger->base.aH != current;
        if (stale) {
            at(s, 463);
            current->base.aG->base.aH = NULL;
            at(s, 468);
            current->base.aG = NULL;
            mc_entity_release(passenger);
            ok = true;
            break;
        }
        at(s, 479);
        if (s->error.passenger_calls == s->error.max_passenger_calls) {
            mc_entity_release(passenger);
            wt_fail(s, MC_WORLD_STORAGE_LIMIT, "World pending passenger call ceiling");
            break;
        }
        if (!capacity(s)) {
            mc_entity_release(passenger);
            break;
        }
        /* Keep every source caller's strong local alive through the whole
         * tail chain. Duplicate/cyclic occurrences own independent holds. */
        s->parents[s->parent_count++] = current;
        current = passenger;
        ++s->error.passenger_calls;
        flag = true;
        wrapper = true;
    }
    mc_entity_release(current);
    while (s->parent_count > start) {
        mc_entity *p = s->parents[--s->parent_count];
        s->parents[s->parent_count] = NULL;
        mc_entity_release(p);
    }
    if (ok) {
        s->error.update_bytecode = 0;
        s->error.wrapper_bytecode = 0;
    }
    return ok;
}
