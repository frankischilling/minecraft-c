#ifndef MC_CLIENT_VIEW_SLOT_H
#define MC_CLIENT_VIEW_SLOT_H

#include "application_state.h"

/* The actual App.i has one native occurrence, independent of App.h and World
 * membership. All source stores share this lifetime cell. Calls require the
 * complete App/Entity graph live and exclusive. Replaced unmanaged fields
 * refuse cleanup/store rather than releasing a caller's occurrence. */
static inline mc_entity_status mc_client_view_slot_store(client_application *app, mc_entity *view) {
    if (app == NULL)
        return MC_ENTITY_INVALID_ARGUMENT;
    if (app->i != app->view_occurrence)
        return MC_ENTITY_BUSY;
    if (view != NULL) {
        const mc_entity_status status = mc_entity_retain(view);
        if (status != MC_ENTITY_OK)
            return status;
    }
    mc_entity *old = app->view_occurrence;
    app->i = view;
    app->view_occurrence = view;
    mc_entity_release(old);
    return MC_ENTITY_OK;
}

#endif
