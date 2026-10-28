/* Actual mk matrix/clear surface on the same Display source state. */
bool mc_client_display_loading_projection(mc_client_display *display, double width, double height,
                                          mc_client_loading_projection_stage *stage,
                                          mc_client_display_error *error) {
    if (stage == NULL)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "LoadingScreen projection admission", "null stage output", 0);
    if (!source_fp_admit("LoadingScreen native extent admission FP", error)) return false;
    if (!isfinite(width) || !isfinite(height) || width <= 0 || height <= 0 ||
        fabs(2.0 / width) > FLT_MAX || fabs(-2.0 / height) > FLT_MAX)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "LoadingScreen projection admission", "invalid exact extent/output", 0);
    if (!source_enter(display, "LoadingScreen source projection", error)) return false;
    display->source.loading_ready = false;
    *stage = MC_CLIENT_LOADING_PROJECTION_DEPTH_CLEAR;
    display->gl.Clear(GL_DEPTH_BUFFER_BIT);
    if (!gl_check(display, "LoadingScreen source depth clear", error))
        return source_leave(display, false, error);
    *stage = MC_CLIENT_LOADING_PROJECTION_ORTHO;
    display->source.matrix_mode = GL_PROJECTION; /* Actual mk102. */
    const float identity[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    memcpy(display->source.projection, identity, sizeof(identity)); /* Actual mk105. */
    if (!source_fp_admit("LoadingScreen ortho FP after depth clear", error))
        return source_leave(display, false, error);
    const float projection[16] = {(float)(2.0 / width), 0, 0, 0, 0, (float)(-2.0 / height), 0, 0,
                                  0, 0, (float)(-2.0 / 200.0), 0, -1, 1, -2, 1};
    memcpy(display->source.projection, projection, sizeof(projection));
    *stage = MC_CLIENT_LOADING_PROJECTION_MODELVIEW;
    display->source.matrix_mode = GL_MODELVIEW; /* Actual mk132. */
    display->source.modelview = (mc_client_modelview){{1, 0, 0, 0, 0, 1, 0, 0,
                                                      0, 0, 1, 0, 0, 0, -200, 1}};
    display->source.loading_ready = true;
    display->exact_width = width;
    display->exact_height = height;
    display->frame_started = true;
    return source_leave(display, true, error);
}

bool mc_client_display_loading_clear_color_depth(mc_client_display *display,
                                                 mc_client_display_error *error) {
    if (display == NULL || !display->source.loading_ready)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "LoadingScreen second clear admission", "source matrices not complete", 0);
    if (!source_enter(display, "LoadingScreen second source clear", error)) return false;
    display->gl.Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    return source_leave(display, gl_check(display, "LoadingScreen second source clear", error), error);
}
