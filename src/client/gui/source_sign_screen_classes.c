/* CPU-only final captured-String cleanup and passive class observation. This
 * object introduces no direct FP resource, Font, Display, SDK or World method
 * linkage. Its optional native release hook was installed by actual FP use. */
#include "source_screen_internal.h"
#include "source_sign_screen.h"

bool mc_screen_sign_class_cleanup_locked(mc_client_screen_classes *c, mc_client_screen_error *e) {
    c->sign_admission_ended = true;
    if (c->sign_mapping_cleanup != NULL && !c->sign_mapping_cleanup(c, e))
        return false;
    mc_player_string_release(c->sign_accepted);
    c->sign_accepted = NULL;
    return true;
}

mc_client_screen_status mc_client_screen_sign_class_inspect(mc_client_screen_classes *c,
                                                            mc_client_sign_class_state *out,
                                                            mc_client_screen_error *error) {
    mc_client_screen_error e = {0};
    if (out == NULL)
        mc_screen_fail(&e, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "YC native class inspection output",
                       NULL, NULL, NULL, 0);
    else if (mc_screen_lock(c, &e)) {
        *out = (mc_client_sign_class_state){c->sign_phase,
                                            c->sign_mapping,
                                            c->sign_accepted,
                                            c->sign_pc,
                                            c->sign_store_pc,
                                            &c->sign_failure,
                                            &c->sign_mapping_failure,
                                            c->sign_mapping != NULL,
                                            c->sign_admission_ended};
        mtx_unlock(&c->mutex);
    }
    return mc_screen_publish(&e, error);
}
