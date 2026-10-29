#include "cx_internal.h"

mc_client_cx_status mc_client_cx_supported(mc_client_source_cx *o, mc_client_display *display,
                                           bool *out, mc_client_cx_error *error) {
    mc_client_cx_error e = {0};
    if (out == NULL)
        cx_fail(&e, MC_CLIENT_CX_INVALID_ARGUMENT, "cx capability output", NULL, 0);
    else if (cx_instance_lock(o, &e)) {
        if (o->phase != MC_CLIENT_CX_READY)
            cx_fail(&e, MC_CLIENT_CX_INVALID_PHASE, "cx.a requires constructed receiver", NULL, 0);
        else {
            bool value = false;
            const bool enabled = o->classes->enabled; /* Actual getstatic0/ifeq3. */
            if (enabled &&
                !mc_client_display_source_arb_occlusion_query(display, &value, &e.display))
                cx_fail(&e, MC_CLIENT_CX_DISPLAY_FAILED, "cx GLContext.getCapabilities", "a", 6);
            else {
                e.capability_read = enabled; /* Actual field9, if reached. */
                *out = value;
                e.source_class = "cx";
                e.source_method = "a";
                e.source_descriptor = "()Z";
                e.source_pc = 20;
                e.source_returned = true;
            }
        }
        (void)mtx_unlock(&o->classes->mutex);
    }
    return cx_publish(&e, error);
}
