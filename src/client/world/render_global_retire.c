#include "render_global_internal.h"

mc_client_render_global_status mc_client_render_global_retire(mc_client_render_global **slot,
                                                              mc_client_render_global_error *out) {
    mc_client_render_global_error error = {0};
    if (slot == NULL)
        mc_render_global_fail(&error, MC_CLIENT_RENDER_GLOBAL_INVALID_ARGUMENT,
                              "strict RenderGlobal native owner output", NULL, 0, true);
    else if (*slot != NULL && ((*slot)->active || (*slot)->holds != 1 ||
                               (*slot)->sky_call != NULL || (*slot)->cull_call != NULL ||
                               (*slot)->update_call != NULL || (*slot)->entities_call != NULL))
        mc_render_global_fail(&error, MC_CLIENT_RENDER_GLOBAL_BUSY,
                              "strict RenderGlobal active/external occurrences", NULL, 0, true);
    else
        return mc_client_render_global_release(slot, out);
    if (out != NULL)
        *out = error;
    return error.status;
}
