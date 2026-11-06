#include "internal.h"

mc_client_manager_status mc_client_render_manager_classes_existing(
    const mc_client_render_classes *models, const mc_registry_classes *registry,
    const mc_java_random_profile *profile, mc_client_render_manager_classes **out,
    mc_client_manager_error *error) {
    mc_client_manager_error e;
    manager_error_init(&e);
    if (models == NULL || registry == NULL || profile == NULL || out == NULL ||
        models->profile != profile) {
        manager_fail(&e, MC_CLIENT_MANAGER_INVALID_ARGUMENT,
                     "TH passive existing association provenance", NULL, NULL, NULL, -1, true);
    } else if (models->th == NULL) {
        *out = NULL;
    } else {
        mc_client_render_manager_classes *c = models->th;
        if (models->th_retired || c->models != models || c->registry != registry ||
            c->profile != profile) {
            manager_fail(&e, MC_CLIENT_MANAGER_INVALID_ARGUMENT,
                         "TH passive existing owner association", NULL, NULL, NULL, -1, true);
        } else if (c->active || c->phase == MC_CLIENT_MANAGER_CONSTRUCTING ||
                   (c->candidate != NULL && c->candidate->active) ||
                   (c->a != NULL && c->a->active)) {
            manager_fail(&e, MC_CLIENT_MANAGER_BUSY, "TH passive existing owner active", NULL, NULL,
                         NULL, -1, true);
        } else {
            *out = c;
        }
    }
    if (error != NULL)
        *error = e;
    return e.status;
}
