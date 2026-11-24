#include "internal.h"
#include <stdlib.h>

mc_client_manager_status mc_client_render_manager_publish_hand(mc_client_render_manager_classes *c,
                                                               mc_client_render_manager *captured,
                                                               mc_client_held_item **candidate,
                                                               mc_client_manager_error *out) {
    mc_client_manager_error e;
    manager_error_init(&e);
    manager_displaced_hand *pending = NULL;
    if (c == NULL || candidate == NULL || *candidate == NULL)
        manager_fail(&e, MC_CLIENT_MANAGER_INVALID_ARGUMENT, "actual TH.f candidate/namespace",
                     NULL, NULL, NULL, -1, true);
    else if (captured == NULL)
        manager_fail(&e, MC_CLIENT_MANAGER_NULL_REFERENCE,
                     "actual MC captured TH receiver at putfield f",
                     "net/minecraft/client/Minecraft", "a", "()V", 321, false);
    else if (c->active || captured->active)
        manager_fail(&e, MC_CLIENT_MANAGER_BUSY, "actual TH.f active owner", NULL, NULL, NULL, -1,
                     true);
    else if (c->phase != MC_CLIENT_MANAGER_READY || captured != c->a || c->consumers == 0)
        manager_fail(&e, MC_CLIENT_MANAGER_INVALID_PHASE,
                     "actual captured TH namespace/source admission", NULL, NULL, NULL, -1, true);
    else {
        mc_client_held_item_state state;
        if (!mc_client_held_item_inspect(*candidate, &state) ||
            state.ra_phase != MC_CLIENT_ENTITY_RENDERER_READY) {
            manager_fail(&e, MC_CLIENT_MANAGER_INVALID_PHASE, "actual TH.f only completed RA", NULL,
                         NULL, NULL, -1, true);
            goto done;
        }
        mc_client_held_item *old = captured->hand;
        if (old != NULL && old != *candidate) {
            if (captured->displaced_count >= c->options.maximum_displaced_hands) {
                manager_fail(&e, MC_CLIENT_MANAGER_LIMIT,
                             "native displaced actual RA occurrence bound", NULL, NULL, NULL, -1,
                             true);
                goto done;
            }
            pending = manager_allocate_native(c, sizeof(*pending), "net/minecraft/client/Minecraft",
                                              321, &e);
            if (pending == NULL)
                goto done;
        }
        if (mc_client_held_item_retain(*candidate, &e.hand) != MC_CLIENT_ENTITY_RENDERER_OK) {
            manager_fail(&e, MC_CLIENT_MANAGER_HAND_FAILED, "actual RA publication occurrence hold",
                         NULL, NULL, NULL, -1, true);
            if (pending != NULL) {
                c->bytes -= sizeof(*pending);
                free(pending);
            }
            goto done;
        }
        captured->fields.f =
            mc_client_held_item_identity(*candidate); /* Actual MC putfield TH.f321. */
        captured->hand = *candidate;
        if (pending != NULL) {
            pending->owner = old;
            pending->next = captured->displaced;
            captured->displaced = pending;
            ++captured->displaced_count;
        } else if (old != NULL) {
            /* Same identity: gained new occurrence before store, then balance
             * ONLY the old occurrence. A final teardown cannot occur here. */
            (void)mc_client_held_item_release(&old, &e.hand);
        }
        /* This is guaranteed nonfinal after the retained publication above.
         * No child/resource cleanup is part of the source field store. */
        (void)mc_client_held_item_release(candidate, &e.hand);
    }
done:
    if (out != NULL)
        *out = e;
    return e.status;
}

/* Every traversal is bounded by the actual native list's declared extent.
 * Reject a short, long, cyclic or empty-owner list before touching any RA. */
static bool hands_extent(const mc_client_render_manager_classes *c,
                         const mc_client_render_manager *m) {
    if (m->displaced_count > c->options.maximum_displaced_hands ||
        (m->hand != NULL && m->displaced_count == SIZE_MAX))
        return false;
    const manager_displaced_hand *p = m->displaced;
    for (size_t i = 0; i < m->displaced_count; ++i) {
        if (p == NULL || p->owner == NULL)
            return false;
        p = p->next;
    }
    return p == NULL;
}

/* Requires hands_extent. These are independent real ownership occurrences,
 * including a current owner that also appears in an older displaced slot. */
static size_t hand_occurrences(const mc_client_render_manager *m,
                               const mc_client_held_item *owner) {
    size_t count = m->hand == owner ? 1u : 0u;
    const manager_displaced_hand *p = m->displaced;
    for (size_t i = 0; i < m->displaced_count; ++i, p = p->next)
        if (p->owner == owner)
            ++count;
    return count;
}

static bool hands_inactive(const mc_client_render_manager *m) {
    if (m->hand != NULL &&
        !mc_client_held_item_is_inactive_occurrences(m->hand, hand_occurrences(m, m->hand)))
        return false;
    const manager_displaced_hand *p = m->displaced;
    for (size_t i = 0; i < m->displaced_count; ++i, p = p->next) {
        bool seen = p->owner == m->hand;
        const manager_displaced_hand *prior = m->displaced;
        for (size_t j = 0; !seen && j < i; ++j, prior = prior->next)
            seen = prior->owner == p->owner;
        if (!seen &&
            !mc_client_held_item_is_inactive_occurrences(p->owner, hand_occurrences(m, p->owner)))
            return false;
    }
    return true;
}

static mc_client_entity_renderer_status hand_drop(mc_client_render_manager *m,
                                                  mc_client_held_item **slot,
                                                  mc_client_entity_renderer_error *e) {
    if (*slot != NULL && hand_occurrences(m, *slot) > 1)
        return mc_client_held_item_release(slot, e);
    return mc_client_held_item_retire(slot, e);
}

mc_client_manager_status mc_client_render_manager_retire_hands(mc_client_render_manager_classes *c,
                                                               mc_client_manager_error *out) {
    mc_client_manager_error e;
    manager_error_init(&e);
    if (c == NULL)
        manager_fail(&e, MC_CLIENT_MANAGER_INVALID_ARGUMENT, "outer manager hand namespace", NULL,
                     NULL, NULL, -1, true);
    else if (manager_terminal_admit(c, &e)) {
        mc_client_render_manager *m = c->candidate;
        if (m == NULL)
            goto done;
        if (!hands_extent(c, m)) {
            manager_fail(&e, MC_CLIENT_MANAGER_INVALID_ARGUMENT,
                         "actual displaced RA native extent", NULL, NULL, NULL, -1, true);
            goto retained;
        }
        /* Count every remaining actual slot for each distinct RA before any
         * cleanup. An extra external occurrence or active RA refuses all drops.
         * A retry counts only slots not consumed by an earlier native attempt. */
        if (!hands_inactive(m))
            goto busy;
        if (hand_drop(m, &m->hand, &e.hand) != MC_CLIENT_ENTITY_RENDERER_OK)
            goto retry;
        while (m->displaced_count != 0) {
            manager_displaced_hand *p = m->displaced;
            if (hand_drop(m, &p->owner, &e.hand) != MC_CLIENT_ENTITY_RENDERER_OK)
                goto retry;
            m->displaced = p->next;
            --m->displaced_count;
            c->bytes -= sizeof(*p);
            free(p);
        }
        /* No source TH.f=NULL store. RETIRING hides expired source views. */
        goto done;
    busy:
        manager_fail(&e, MC_CLIENT_MANAGER_BUSY, "actual global RA external occurrence/operation",
                     NULL, NULL, NULL, -1, true);
        goto retained;
    retry:
        manager_fail(&e, MC_CLIENT_MANAGER_HAND_FAILED,
                     "actual global RA retryable native retirement", NULL, NULL, NULL, -1, true);
    retained:
        c->retirement = e;
    }
done:
    if (out != NULL)
        *out = e;
    return e.status;
}
