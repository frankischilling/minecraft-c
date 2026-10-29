#include "mc/client/world/render_global_update.h"

#include "../../entity/player/tick_math_internal.h"
#include "../render_engine/internal.h"
#include "render_global_update_sort.h"
#include "source_chunk_compile_internal.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

struct mc_client_render_global_update_call {
    mc_client_render_global_update_state state;
    mc_client_render_global_error failure, cleanup;
    mc_client_source_dk **near_chunks, **order;
    size_t near_capacity, order_capacity;
    mc_source_dk_compile *compilation;
    mc_client_chunk_compile_state compile_state;
    bool camera_held;
};

static bool fail(mc_client_render_global_update_call *c, mc_client_render_global_status status,
                 const char *operation, unsigned int pc, bool native) {
    c->state.source_pc = pc;
    mc_render_global_source_fail(&c->failure, status, operation, "n", "a", "(Lls;Z)Z", pc, native);
    return false;
}

static mc_client_render_list *list(mc_client_render_global_update_call *c, unsigned int pc) {
    mc_client_render_list *m = c->state.owner->fields.m;
    if (m == NULL) {
        fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE, "actual N dirty List receiver", pc, false);
        return NULL;
    }
    if (m->count > m->capacity || m->capacity > INT32_MAX ||
        (m->capacity != 0 && m->entries == NULL)) {
        fail(c, MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE, "N dirty List native extent", pc, true);
        return NULL;
    }
    return m;
}

static bool owned(mc_client_render_global_update_call *c, mc_client_source_dk *dk,
                  unsigned int pc) {
    if (dk == NULL)
        return true; /* CHECKCAST and List.get permit actual null elements. */
    for (const mc_client_source_dk *p = c->state.owner->chunk_owners; p != NULL; p = p->next)
        if (p == dk)
            return true;
    return fail(c, MC_CLIENT_RENDER_GLOBAL_INVALID_ARGUMENT, "N DK native occurrence provenance",
                pc, true);
}

static bool get(mc_client_render_global_update_call *c, int32_t index, unsigned int pc,
                mc_client_source_dk **out) {
    mc_client_render_list *m = list(c, pc);
    if (m == NULL)
        return false;
    if (index < 0 || (size_t)index >= m->count)
        return fail(c, MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE, "actual N List.get index", pc, false);
    mc_client_source_dk *dk = m->entries[index];
    if (!owned(c, dk, pc))
        return false;
    *out = dk;
    return true;
}

static bool set(mc_client_render_global_update_call *c, int32_t index, mc_client_source_dk *dk,
                unsigned int pc) {
    mc_client_render_list *m = list(c, pc);
    if (m == NULL)
        return false;
    if (index < 0 || (size_t)index >= m->count)
        return fail(c, MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE, "actual N List.set index", pc, false);
    m->entries[index] = dk;
    return true;
}

static bool distance(mc_client_render_global_update_call *c, mc_client_source_dk *dk, float *out,
                     unsigned int parent, unsigned int md_pc) {
    if (dk == NULL)
        return fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE, "actual DK distance receiver",
                    parent, false);
    if (c->state.camera == NULL) {
        fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE, "actual DK distance camera", parent, false);
        c->failure.child =
            md_pc == 0 ? (mc_client_graphics_site){"dk", "a", "(Lsn;)F", 1}
                       : (mc_client_graphics_site){"md", "a", "(Ldk;Ldk;)I", (int32_t)md_pc};
        return false;
    }
    mc_entity_snapshot camera;
    if (mc_entity_get_snapshot(c->state.camera, &camera) != MC_ENTITY_OK)
        return fail(c, MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE, "DK canonical Entity fields", parent,
                    true);
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
        return fail(c, MC_CLIENT_RENDER_GLOBAL_UNSUPPORTED_FP, "DK distance strict FP environment",
                    parent, true);
    const float x = at_narrow(at_sub(camera.base.x, (double)dk->fields.q));
    const float y = at_narrow(at_sub(camera.base.y, (double)dk->fields.r));
    const float z = at_narrow(at_sub(camera.base.z, (double)dk->fields.s));
    *out = at_fadd(at_fadd(at_fmul(x, x), at_fmul(y, y)), at_fmul(z, z));
    return true;
}

static bool compare(mc_client_render_global_update_call *c, mc_client_source_dk *a,
                    mc_client_source_dk *b, unsigned int pc, int *out) {
    if (a == NULL || b == NULL) {
        fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE, "actual MD DK flag receiver", pc, false);
        c->failure.child = (mc_client_graphics_site){"md", "a", "(Ldk;Ldk;)I", a == NULL ? 1 : 6};
        return false;
    }
    if (a->fields.o && !b->fields.o) {
        *out = 1;
        return true;
    }
    if (b->fields.o && !a->fields.o) {
        *out = -1;
        return true;
    }
    float left, right;
    if (!distance(c, a, &left, pc, 38) || !distance(c, b, &right, pc, 49))
        return false;
    /* dcmpg/dcmpl make both NaN comparisons fall through to the ordinal. */
    if (left < right)
        *out = 1;
    else if (left > right)
        *out = -1;
    else
        *out = a->fields.w < b->fields.w ? 1 : -1;
    return true;
}

static bool reserve(mc_client_render_global_update_call *c, mc_client_source_dk ***slots,
                    size_t *capacity, size_t wanted, unsigned int pc) {
    if (wanted <= *capacity)
        return true;
    size_t next = *capacity == 0 ? 8 : *capacity * 2;
    if (next < wanted)
        next = wanted;
    /* Per-call transient storage is bounded separately from N's permanent arena. */
    if (next > INT32_MAX || next > (8u * 1024u * 1024u) / sizeof(**slots))
        return fail(c, MC_CLIENT_RENDER_GLOBAL_LIMIT, "N update transient List storage", pc, true);
    void *memory = realloc(*slots, next * sizeof(**slots));
    if (memory == NULL)
        return fail(c, MC_CLIENT_RENDER_GLOBAL_ALLOCATION_FAILED,
                    "N update transient List allocation", pc, true);
    *slots = memory;
    *capacity = next;
    return true;
}

static bool sort_compare(void *context, mc_client_source_dk *a, mc_client_source_dk *b, int *out) {
    return compare(context, a, b, 411, out);
}

static bool sort_near(mc_client_render_global_update_call *c) {
    if (c->state.near_length < 2)
        return true;
    if (!mc_render_global_update_sort(c->near_chunks, c->state.near_length, c, sort_compare,
                                      &c->failure)) {
        c->state.source_pc = 411;
        return false;
    }
    return true;
}

static bool compile(mc_client_render_global_update_call *c, mc_client_source_dk *dk,
                    unsigned int pc) {
    if (dk == NULL)
        return fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE, "actual DK compile receiver", pc,
                    false);
    if (mc_source_dk_compile_retire(&c->compilation, &c->cleanup) != MC_CLIENT_RENDER_GLOBAL_OK) {
        if (c->compilation != NULL)
            (void)mc_source_dk_compile_inspect(c->compilation, &c->compile_state);
        return fail(c, MC_CLIENT_RENDER_GLOBAL_CLEANUP_FAILED,
                    "completed DK compilation native retirement", pc, true);
    }
    c->state.compilation = NULL;
    if (!reserve(c, &c->order, &c->order_capacity, c->state.compile_attempts + 1, pc))
        return false;
    c->state.compile_order = c->order;
    c->order[c->state.compile_attempts++] = dk;
    mc_client_render_global_error child = {0};
    const bool ok = mc_source_dk_compile_entered(dk, &c->compilation, &child);
    if (c->compilation != NULL) {
        (void)mc_source_dk_compile_inspect(c->compilation, &c->compile_state);
        c->state.compilation = &c->compile_state;
    }
    if (!ok) {
        c->failure = child;
        c->failure.child =
            (mc_client_graphics_site){child.source_class, child.source_method,
                                      child.source_descriptor, (int32_t)child.source_pc};
        return fail(c, child.status, child.operation, pc, child.native_policy);
    }
    ++c->state.completed_compiles;
    dk->fields.u = false; /* Only after the SAME dk.a returned. */
    dk->last_store_pc = pc == 442 ? 448 : 528;
    return true;
}

static bool body(mc_client_render_global_update_call *c) {
    /* The source local at0 is always false. Its old whole-queue path is dead. */
    mc_client_render_list *m = list(c, 199);
    if (m == NULL)
        return false;
    c->state.initial_count = (int32_t)m->count;
    for (c->state.index = 0; c->state.index < c->state.initial_count; ++c->state.index) {
        mc_client_source_dk *dk;
        if (!get(c, c->state.index, 225, &dk))
            return false;
        if (!c->state.visible_only) {
            float d;
            if (!distance(c, dk, &d, 242, 0))
                return false;
            if (d > 256.0f) { /* fcmpl makes NaN enter the near path. */
                int slot = 0;
                for (; slot < 2; ++slot) {
                    if (c->state.far_chunks[slot] != NULL) {
                        int cmp;
                        if (!compare(c, c->state.far_chunks[slot], dk, 278, &cmp))
                            return false;
                        if (cmp > 0)
                            break;
                    }
                }
                --slot; /* Preserve the original unusual decrement/gate. */
                if (slot > 0) {
                    int move = slot;
                    while (--move != 0)
                        c->state.far_chunks[move - 1] = c->state.far_chunks[move];
                    c->state.far_chunks[slot] = dk;
                }
                continue;
            }
        } else {
            if (dk == NULL)
                return fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                            "actual DK visible-only flag receiver", 340, false);
            if (!dk->fields.o)
                continue;
        }
        if (!reserve(c, &c->near_chunks, &c->near_capacity, c->state.near_length + 1, 370))
            return false;
        c->state.near_count = mc_java_i32_add(c->state.near_count, 1);
        c->near_chunks[c->state.near_length++] = dk;
        c->state.near_chunks = c->near_chunks;
        if (!set(c, c->state.index, NULL, 381))
            return false;
    }
    if (!sort_near(c))
        return false;
    for (size_t i = c->state.near_length; i != 0; --i)
        if (!compile(c, c->near_chunks[i - 1], 442))
            return false;
    for (int i = 1; i >= 0; --i) {
        mc_client_source_dk *dk = c->state.far_chunks[i];
        if (dk == NULL)
            continue;
        if (!dk->fields.o && i != 1) {
            c->state.far_chunks[i] = c->state.far_chunks[0] = NULL;
            break;
        }
        if (!compile(c, dk, 519))
            return false;
        c->state.far_count = mc_java_i32_add(c->state.far_count, 1);
    }
    m = list(c, 550);
    if (m == NULL)
        return false;
    const int32_t count = (int32_t)m->count;
    int32_t kept = 0;
    for (int32_t i = 0; i != count; ++i) {
        mc_client_source_dk *dk;
        if (!get(c, i, 570, &dk))
            return false;
        if (dk != NULL && dk != c->state.far_chunks[0] && dk != c->state.far_chunks[1]) {
            if (kept != i && !set(c, kept, dk, 642))
                return false;
            ++kept;
        }
    }
    for (int32_t i = count; i > kept;) {
        --i;
        m = list(c, 673);
        if (m == NULL)
            return false;
        if ((size_t)i >= m->count)
            return fail(c, MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE, "actual N List.remove index", 673,
                        false);
        memmove(m->entries + i, m->entries + i + 1,
                (m->count - (size_t)i - 1) * sizeof(*m->entries));
        m->entries[--m->count] = NULL; /* ArrayList drops its trailing reference. */
    }
    c->state.remaining_count = m->count;
    c->state.result =
        c->state.initial_count == mc_java_i32_add(c->state.near_count, c->state.far_count);
    c->state.source_pc = 697;
    c->state.returned = true;
    return true;
}

mc_client_render_global_status
mc_client_render_global_update_begin(mc_client_render_global *n, mc_entity *camera,
                                     bool visible_only, mc_client_render_global_update_call **out,
                                     mc_client_render_global_error *error) {
    mc_client_render_global_error local = {0};
    if (error == NULL)
        error = &local;
    *error = (mc_client_render_global_error){0};
    if (n == NULL || out == NULL || *out != NULL)
        return mc_render_global_source_fail(error, MC_CLIENT_RENDER_GLOBAL_INVALID_ARGUMENT,
                                            "N update native owner/output", NULL, NULL, NULL, 0,
                                            true);
    mc_client_render_global_world_state attachment;
    const bool blocked = n->world_call != NULL &&
                         (!mc_client_render_global_world_inspect(n->world_call, &attachment) ||
                          attachment.active || !attachment.source_returned);
    if (n->active || n->world_retiring || blocked || n->sky_call != NULL || n->cull_call != NULL ||
        n->update_call != NULL || n->phase == MC_CLIENT_RENDER_GLOBAL_CONSTRUCTING ||
        n->phase == MC_CLIENT_RENDER_GLOBAL_RETIRING)
        return mc_render_global_source_fail(error, MC_CLIENT_RENDER_GLOBAL_BUSY,
                                            "N update entered/pending/retiring owner", NULL, NULL,
                                            NULL, 0, true);
    mc_client_render_global_update_call *c = calloc(1, sizeof(*c));
    if (c == NULL)
        return mc_render_global_source_fail(error, MC_CLIENT_RENDER_GLOBAL_ALLOCATION_FAILED,
                                            "N update native call allocation", NULL, NULL, NULL, 0,
                                            true);
    if (camera != NULL && mc_entity_retain(camera) != MC_ENTITY_OK) {
        free(c);
        return mc_render_global_source_fail(error, MC_CLIENT_RENDER_GLOBAL_BUSY,
                                            "N update canonical camera occurrence", NULL, NULL,
                                            NULL, 0, true);
    }
    c->camera_held = camera != NULL;
    if (mc_client_render_global_retain(n, error) != MC_CLIENT_RENDER_GLOBAL_OK) {
        if (c->camera_held)
            mc_entity_release(camera);
        free(c);
        return error->status;
    }
    c->state.owner = n;
    c->state.camera = camera;
    c->state.visible_only = visible_only;
    c->state.failure = &c->failure;
    c->state.cleanup = &c->cleanup;
    n->update_call = c;
    *out = c;
    c->state.active = n->active = true;
    (void)body(c);
    c->state.active = n->active = false;
    *error = c->failure;
    return c->failure.status;
}

bool mc_client_render_global_update_inspect(const mc_client_render_global_update_call *c,
                                            mc_client_render_global_update_state *out) {
    if (c == NULL || out == NULL || c->state.active)
        return false;
    *out = c->state;
    if (c->state.owner->fields.m != NULL)
        out->remaining_count = c->state.owner->fields.m->count;
    return true;
}

mc_client_render_global_status
mc_client_render_global_update_retire(mc_client_render_global_update_call **slot,
                                      mc_client_render_global_error *error) {
    mc_client_render_global_error local = {0};
    if (error == NULL)
        error = &local;
    *error = (mc_client_render_global_error){0};
    if (slot == NULL)
        return mc_render_global_source_fail(error, MC_CLIENT_RENDER_GLOBAL_INVALID_ARGUMENT,
                                            "N update native retirement slot", NULL, NULL, NULL, 0,
                                            true);
    mc_client_render_global_update_call *c = *slot;
    if (c == NULL)
        return MC_CLIENT_RENDER_GLOBAL_OK;
    if (c->state.active || c->state.owner->active || c->state.owner->update_call != c)
        return mc_render_global_source_fail(error, MC_CLIENT_RENDER_GLOBAL_BUSY,
                                            "N update active/changed retirement", NULL, NULL, NULL,
                                            0, true);
    if (mc_source_dk_compile_retire(&c->compilation, &c->cleanup) != MC_CLIENT_RENDER_GLOBAL_OK) {
        if (c->compilation != NULL)
            (void)mc_source_dk_compile_inspect(c->compilation, &c->compile_state);
        *error = c->cleanup;
        return error->status;
    }
    c->state.compilation = NULL;
    mc_client_render_global *owner = c->state.owner;
    owner->update_call = NULL;
    if (mc_client_render_global_release(&c->state.owner, &c->cleanup) !=
        MC_CLIENT_RENDER_GLOBAL_OK) {
        owner->update_call = c;
        *error = c->cleanup;
        return error->status;
    }
    if (c->camera_held)
        mc_entity_release(c->state.camera);
    free(c->near_chunks);
    free(c->order);
    free(c);
    *slot = NULL;
    return MC_CLIENT_RENDER_GLOBAL_OK;
}
