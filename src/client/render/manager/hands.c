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
        /* Gate EVERY actual external RA occurrence before any child cleanup.
         * Per-App callers never enter this explicit outer terminal API. */
        if (m->hand != NULL && !mc_client_held_item_is_final_inactive(m->hand))
            goto busy;
        for (manager_displaced_hand *p = m->displaced; p != NULL; p = p->next)
            if (!mc_client_held_item_is_final_inactive(p->owner))
                goto busy;
        if (mc_client_held_item_retire(&m->hand, &e.hand) != MC_CLIENT_ENTITY_RENDERER_OK)
            goto retry;
        while (m->displaced != NULL) {
            manager_displaced_hand *p = m->displaced;
            if (mc_client_held_item_retire(&p->owner, &e.hand) != MC_CLIENT_ENTITY_RENDERER_OK)
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
