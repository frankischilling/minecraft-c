#include "source_container_screen_internal.h"
#include "source_screen_transition_receipts_internal.h"

bool mc_screen_transition_initialization_capture(mc_screen_transition_receipts *r,
                                                 mc_client_screen_error *error,
                                                 const mc_client_screen_init_error *owner) {
    if (r == NULL || error == NULL || owner == NULL ||
        (error->initialization != owner &&
         !(error->initialization == &r->initialization && r->initialization_source == owner)))
        return false;
    r->initialization = *owner;
    r->initialization_source = owner;
    if (r->inventory_source != NULL && r->initialization.screen.inventory == r->inventory_source)
        r->initialization.screen.inventory = &r->inventory;
    if (r->sign_source != NULL && r->initialization.screen.sign == r->sign_source)
        r->initialization.screen.sign = &r->sign;
    error->initialization = &r->initialization;
    return true;
}

void mc_screen_transition_initialization_after(mc_screen_transition_receipts *r,
                                               const mc_client_screen_init_error *owner) {
    if (owner != NULL) {
        r->initialization = *owner;
        if (r->inventory_source != NULL &&
            r->initialization.screen.inventory == r->inventory_source)
            r->initialization.screen.inventory = &r->inventory;
        if (r->sign_source != NULL && r->initialization.screen.sign == r->sign_source)
            r->initialization.screen.sign = &r->sign;
    } else if (r->initialization.screen.initialization == r->initialization_source)
        r->initialization.screen.initialization = NULL;
    r->initialization_source = owner;
}

bool mc_screen_transition_sign_capture(mc_screen_transition_receipts *r,
                                       mc_client_screen_error *error,
                                       const mc_client_sign_screen_error *owner) {
    if (r == NULL || error == NULL || owner == NULL)
        return false;
    const bool direct =
        error->sign == owner || (error->sign == &r->sign && r->sign_source == owner);
    const bool nested =
        error->initialization != NULL &&
        (error->initialization->screen.sign == owner ||
         (error->initialization->screen.sign == &r->sign && r->sign_source == owner));
    if (!direct && !nested)
        return false;
    if (nested && error->initialization != &r->initialization)
        (void)mc_screen_transition_initialization_capture(r, error, error->initialization);
    r->sign = *owner;
    r->sign_source = owner;
    if (direct)
        error->sign = &r->sign;
    if (nested)
        r->initialization.screen.sign = &r->sign;
    return true;
}

void mc_screen_transition_sign_after(mc_screen_transition_receipts *r,
                                     const mc_client_sign_screen_error *owner) {
    if (owner != NULL)
        r->sign = *owner; /* Checked cleanup may already have consumed native holds. */
    else {
        /* These are borrowed views of actual dirty/acquisition producers, not
         * ownership acquired by this scalar receipt. The consumed YC has
         * ended its captures; keep its source/cause fields and null witnesses. */
        r->sign.dirty.world_access_failure.receiver = NULL;
        r->sign.dirty.world_access_failure.render_global = NULL;
        r->sign.dirty.world_access_failure.receipt = NULL;
        r->sign.dirty.publication.world_access_failure.receiver = NULL;
        r->sign.dirty.publication.world_access_failure.render_global = NULL;
        r->sign.dirty.publication.world_access_failure.receipt = NULL;
        r->sign.dirty.batch.publication.world_access_failure.receiver = NULL;
        r->sign.dirty.batch.publication.world_access_failure.render_global = NULL;
        r->sign.dirty.batch.publication.world_access_failure.receipt = NULL;
    }
    r->sign_source = owner;
}

bool mc_screen_transition_inventory_capture(mc_screen_transition_receipts *r,
                                            mc_client_screen_error *error,
                                            const mc_client_inventory_screen_error *owner) {
    if (r == NULL || error == NULL || owner == NULL)
        return false;
    const bool direct = error->inventory == owner ||
                        (error->inventory == &r->inventory && r->inventory_source == owner);
    const bool nested = error->initialization != NULL &&
                        (error->initialization->screen.inventory == owner ||
                         (error->initialization->screen.inventory == &r->inventory &&
                          r->inventory_source == owner));
    if (!direct && !nested)
        return false;
    if (nested && error->initialization != &r->initialization)
        (void)mc_screen_transition_initialization_capture(r, error, error->initialization);
    r->inventory = *owner;
    r->inventory_source = owner;
    if (direct)
        error->inventory = &r->inventory;
    if (nested)
        r->initialization.screen.inventory = &r->inventory;
    return true;
}

static void expire_container_receipt(mc_player_container_error *error) {
    error->drop.publication = NULL;
    error->drop.statistic = NULL;
}

void mc_screen_transition_inventory_after(mc_screen_transition_receipts *r,
                                          const mc_client_inventory_screen_error *owner) {
    if (owner != NULL)
        r->inventory = *owner; /* Checked BUSY may already have consumed earlier children. */
    else {
        /* These four fields contain only this Screen's owned adapter views.
         * Successful Screen retirement proves every such child has ended. */
        expire_container_receipt(&r->inventory.container);
        expire_container_receipt(&r->inventory.cleanup_container);
        expire_container_receipt(&r->inventory.inherited_close);
        expire_container_receipt(&r->inventory.direct_close);
        mc_screen_container_expire_chest_views(&r->inventory);
    }
    r->inventory_source = owner;
}
