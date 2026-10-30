#include "source_terrain_internal.h"

#include "../../entity/player/tick_math_internal.h"
#include "../application_state.h"
#include "../render_engine/internal.h"
#include "render_batch_internal.h"

#include <limits.h>

static mc_client_render_list *selected(mc_client_render_global_submit_call *c, unsigned int pc) {
    mc_client_render_list *list = c->state.owner->fields.S;
    if (list == NULL) {
        mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE, "actual N selected List", pc,
                        false);
        return NULL;
    }
    if (list->count > list->capacity || list->capacity > INT32_MAX ||
        (list->capacity != 0 && list->entries == NULL)) {
        mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE, "N selected List native extent",
                        pc, true);
        return NULL;
    }
    return list;
}

static bool pass_empty(mc_client_render_global_submit_call *c, mc_client_source_dk *dk,
                       unsigned int field_pc, unsigned int index_pc, bool *out) {
    if (!mc_terrain_dk_receiver(c, dk, field_pc))
        return false;
    const bool *p = dk->fields.p;
    if (p == NULL)
        return mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE, "actual DK pass array",
                               index_pc, false);
    if (c->state.pass < 0 || c->state.pass >= 2)
        return mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_SOURCE_INDEX,
                               "actual DK pass array index", index_pc, false);
    *out = p[c->state.pass];
    return true;
}

static bool batch(mc_client_render_global_submit_call *c, int32_t index, unsigned int pc,
                  mc_client_render_batch **out) {
    mc_client_render_global *n = c->state.owner;
    if (n->fields.T == NULL)
        return mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE, "actual N TG array", pc,
                               false);
    if (n->t_length > INT32_MAX)
        return mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_LIMIT, "N TG native extent", pc, true);
    if (index < 0 || (size_t)index >= n->t_length)
        return mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_SOURCE_INDEX, "actual N TG array index",
                               pc, false);
    *out = n->fields.T[index];
    return true;
}

static bool batch_length(mc_client_render_global_submit_call *c, unsigned int pc, int32_t *out) {
    mc_client_render_global *n = c->state.owner;
    if (n->fields.T == NULL)
        return mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                               "actual N TG array length", pc, false);
    if (n->t_length > INT32_MAX)
        return mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_LIMIT, "N TG native extent", pc, true);
    *out = (int32_t)n->t_length;
    return true;
}

static void increment(mc_client_render_global_submit_call *c, int32_t *field, unsigned int pc) {
    *field = mc_java_i32_add(*field, 1);
    mc_terrain_store(c, pc);
}

static bool statistics(mc_client_render_global_submit_call *c, int32_t index) {
    mc_client_render_global *n = c->state.owner;
    mc_client_source_dk *dk;
    bool empty;
    increment(c, &n->fields.M, 32);
    if (!mc_terrain_array(c, true, index, 41, &dk) || !pass_empty(c, dk, 42, 46, &empty))
        return false;
    if (empty) {
        increment(c, &n->fields.Q, 57);
        return true;
    }
    if (!mc_terrain_array(c, true, index, 69, &dk) || !mc_terrain_dk_receiver(c, dk, 70))
        return false;
    if (!dk->fields.o) {
        increment(c, &n->fields.N, 83);
        return true;
    }
    if (n->fields.w) {
        if (!mc_terrain_array(c, true, index, 102, &dk) || !mc_terrain_dk_receiver(c, dk, 103))
            return false;
        if (!dk->fields.x) {
            increment(c, &n->fields.O, 116);
            return true;
        }
    }
    increment(c, &n->fields.P, 129);
    return true;
}

static bool choose(mc_client_render_global_submit_call *c, int32_t index, int32_t *count) {
    mc_client_render_global *n = c->state.owner;
    mc_client_source_dk *dk;
    bool empty;
    if (!mc_terrain_array(c, true, index, 138, &dk) || !pass_empty(c, dk, 139, 143, &empty))
        return false;
    if (empty)
        return true;
    if (!mc_terrain_array(c, true, index, 153, &dk) || !mc_terrain_dk_receiver(c, dk, 154))
        return false;
    if (!dk->fields.o)
        return true;
    if (n->fields.w) {
        if (!mc_terrain_array(c, true, index, 173, &dk) || !mc_terrain_dk_receiver(c, dk, 174))
            return false;
        if (!dk->fields.x)
            return true;
    }
    if (!mc_terrain_array(c, true, index, 186, &dk) || !mc_terrain_dk_receiver(c, dk, 188))
        return false;
    int32_t id;
    if (!mc_terrain_child(c, mc_source_dk_render_list(dk, c->state.pass, &id, &c->failure), 188))
        return false;
    if (id < 0)
        return true;
    mc_client_render_list *s = selected(c, 209);
    if (s == NULL || !mc_terrain_array(c, true, index, 208, &dk))
        return false;
    mc_terrain_pc(c, 209);
    if (!mc_terrain_child(
            c,
            mc_render_global_pointer_append(
                n, s, dk, (mc_client_graphics_site){"n", "a", "(IIID)I", 209}, &c->failure),
            209))
        return false;
    *count = mc_java_i32_add(*count, 1);
    ++c->state.selected_chunks;
    return true;
}

static bool camera(mc_client_render_global_submit_call *c, double *x, double *y, double *z) {
    client_application *app = c->state.owner->fields.t;
    if (app == NULL)
        return mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                               "actual N batching App.i receiver", 228, false);
    mc_entity *current = app->i;
    if (current != NULL && mc_entity_retain(current) != MC_ENTITY_OK)
        return mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_BUSY,
                               "N batching camera native occurrence", 235, true);
    if (c->batch_camera_held)
        mc_entity_release(c->state.batch_camera);
    c->state.batch_camera = current;
    c->batch_camera_held = current != NULL;
    if (current == NULL)
        return mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                               "actual N batching camera", 235, false);
    mc_entity_snapshot v;
    c->failure.entity.status = mc_entity_get_snapshot(current, &v);
    if (c->failure.entity.status != MC_ENTITY_OK)
        return mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE,
                               "N batching canonical camera fields", 235, true);
    if (!mc_terrain_fp(c, 248))
        return false;
    *x = at_add(v.base.bl, at_mul(at_sub(v.base.x, v.base.bl), c->state.partial));
    *y = at_add(v.base.bm, at_mul(at_sub(v.base.y, v.base.bm), c->state.partial));
    *z = at_add(v.base.bn, at_mul(at_sub(v.base.z, v.base.bn), c->state.partial));
    return true;
}

bool mc_terrain_replay(mc_client_render_global_submit_call *c) {
    mc_client_render_global *n = c->state.owner;
    if (!mc_terrain_push(c, "n", "a", "(ID)V"))
        return false;
    for (int32_t i = 0;; i = mc_java_i32_add(i, 1)) {
        int32_t length;
        if (!batch_length(c, 9, &length))
            return false;
        if (i >= length)
            break;
        mc_client_render_batch *b;
        if (!batch(c, i, 19, &b))
            return false;
        if (b == NULL)
            return mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                                   "actual TG draw receiver", 20, false);
        mc_terrain_pc(c, 20);
        if (!mc_terrain_child(c, mc_render_batch_draw(n, b, &c->failure), 20))
            return false;
        ++c->state.drawn_batches;
    }
    mc_terrain_pc(c, 29);
    mc_terrain_pop(c);
    return true;
}

static bool draw_all(mc_client_render_global_submit_call *c) {
    mc_terrain_pc(c, 484);
    return mc_terrain_replay(c);
}

bool mc_terrain_subset(mc_client_render_global_submit_call *c, int32_t start, int32_t end,
                       unsigned int caller_pc, int32_t *result) {
    mc_client_render_global *n = c->state.owner;
    mc_terrain_pc(c, caller_pc);
    if (!mc_terrain_push(c, "n", "a", "(IIID)I"))
        return false;
    ++c->state.subsets;
    mc_client_render_list *s = selected(c, 4);
    if (s == NULL)
        return false;
    for (size_t i = 0; i < s->count; ++i)
        s->entries[i] = NULL;
    s->count = 0;
    int32_t count = 0;
    for (int32_t i = start; i < end; i = mc_java_i32_add(i, 1)) {
        c->state.index = i;
        if ((c->state.pass == 0 && !statistics(c, i)) || !choose(c, i, &count))
            return false;
    }
    double x, y, z;
    if (!camera(c, &x, &y, &z))
        return false;
    for (int32_t i = 0;; i = mc_java_i32_add(i, 1)) {
        int32_t length;
        if (!batch_length(c, 311, &length))
            return false;
        if (i >= length)
            break;
        mc_client_render_batch *b;
        if (!batch(c, i, 321, &b))
            return false;
        if (b == NULL)
            return mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                                   "actual TG reset receiver", 322, false);
        mc_terrain_pc(c, 322);
        if (!mc_terrain_child(c, mc_render_batch_reset(n, b, &c->failure), 322))
            return false;
    }
    int32_t used = 0;
    for (int32_t i = 0;; i = mc_java_i32_add(i, 1)) {
        s = selected(c, 340);
        if (s == NULL)
            return false;
        if ((size_t)i >= s->count)
            break;
        s = selected(c, 354);
        if (s == NULL)
            return false;
        if (i < 0 || (size_t)i >= s->count)
            return mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_SOURCE_INDEX,
                                   "actual N selected List.get index", 354, false);
        mc_client_source_dk *dk = s->entries[i];
        int32_t chosen = -1;
        for (int32_t j = 0; j < used; j = mc_java_i32_add(j, 1)) {
            mc_client_render_batch *b;
            if (!batch(c, j, 383, &b) || !mc_terrain_dk_receiver(c, dk, 386))
                return false;
            if (b == NULL)
                return mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                                       "actual TG match receiver", 399, false);
            bool matches;
            mc_terrain_pc(c, 399);
            if (!mc_terrain_child(c,
                                  mc_render_batch_matches(n, b, dk->fields.i, dk->fields.j,
                                                          dk->fields.k, &matches, &c->failure),
                                  399))
                return false;
            if (matches)
                chosen = j; /* The original search deliberately has no break. */
        }
        if (chosen < 0) {
            chosen = used;
            used = mc_java_i32_add(used, 1);
            mc_client_render_batch *b;
            if (!batch(c, chosen, 433, &b) || !mc_terrain_dk_receiver(c, dk, 436))
                return false;
            if (b == NULL)
                return mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                                       "actual TG configure receiver", 455, false);
            mc_terrain_pc(c, 455);
            if (!mc_terrain_child(c,
                                  mc_render_batch_configure(n, b, dk->fields.i, dk->fields.j,
                                                            dk->fields.k, x, y, z, &c->failure),
                                  455))
                return false;
            ++c->state.configured_batches;
        }
        mc_client_render_batch *b;
        int32_t id;
        if (!batch(c, chosen, 464, &b) || !mc_terrain_dk_receiver(c, dk, 468))
            return false;
        mc_terrain_pc(c, 468);
        if (!mc_terrain_child(c, mc_source_dk_render_list(dk, c->state.pass, &id, &c->failure),
                              468))
            return false;
        if (b == NULL)
            return mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                                   "actual TG append receiver", 471, false);
        mc_terrain_pc(c, 471);
        if (!mc_terrain_child(c, mc_render_batch_append(n, b, id, &c->failure), 471))
            return false;
        ++c->state.appended_lists;
    }
    if (!draw_all(c))
        return false;
    mc_terrain_pc(c, 489);
    mc_terrain_pop(c);
    *result = count;
    return true;
}
