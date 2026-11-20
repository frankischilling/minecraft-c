#include "../application_state.h"
#include "loading_internal.h"

static mc_client_loading_status project(client_application *app,
                                        const mc_scaled_resolution *resolution, bool progress,
                                        mc_client_loading_error *error) {
    mc_client_loading_projection_stage stage = MC_CLIENT_LOADING_PROJECTION_DEPTH_CLEAR;
    if (mc_client_display_loading_projection(app->display, resolution->width_exact,
                                             resolution->height_exact, &stage, &error->display))
        return MC_CLIENT_LOADING_OK;
    const int32_t source_pc =
        stage == MC_CLIENT_LOADING_PROJECTION_DEPTH_CLEAR ? (progress ? 96 : 63)
        : stage == MC_CLIENT_LOADING_PROJECTION_ORTHO     ? (progress ? 126 : 91)
                                                          : (progress ? 142 : 107);
    return mc_client_loading_fail(error,
                                  error->display.status == MC_CLIENT_DISPLAY_UNSUPPORTED_FP
                                      ? MC_CLIENT_LOADING_UNSUPPORTED_FP
                                      : MC_CLIENT_LOADING_DISPLAY_FAILED,
                                  "LoadingScreen source projection", progress ? "a" : "c",
                                  progress ? "(I)V" : "(Ljava/lang/String;)V", source_pc);
}

mc_client_loading_status mc_client_loading_projection(client_application *app,
                                                      const mc_scaled_resolution *resolution,
                                                      mc_client_loading_error *error) {
    return project(app, resolution, false, error);
}

mc_client_loading_status
mc_client_loading_progress_projection(client_application *app,
                                      const mc_scaled_resolution *resolution,
                                      mc_client_loading_error *error) {
    return project(app, resolution, true, error);
}
