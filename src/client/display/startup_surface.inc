/* Included once by Display, sharing its real GL table and source state. */
/* Defined in source_viewport.inc later in the same Display translation unit. */
static bool source_material_viewport(mc_client_display *, int32_t, int32_t,
                                     mc_client_display_error *);

bool mc_client_display_startup_projection(mc_client_display *display, double width, double height,
                                          mc_client_startup_projection_stage *stage,
                                          mc_client_display_error *error) {
    if (stage == NULL)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "startup projection admission",
                    "null stage output", 0);
    *stage = MC_CLIENT_STARTUP_PROJECTION_CLEAR;
    if (!source_fp_admit("startup projection native extent FP", error))
        return false;
    if (!isfinite(width) || !isfinite(height) || width <= 0 || height <= 0 ||
        fabs(2.0 / width) > FLT_MAX || fabs(-2.0 / height) > FLT_MAX)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "startup projection admission",
                    "invalid exact extent", 0);
    if (!source_enter(display, "startup source projection", error))
        return false;
    if (display->source.compiling != NULL)
        return source_leave(display,
                            fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "startup projection",
                                 "compiled matrix path", 0),
                            error);
    display->source.loading_ready = false;
    source_material_vertices(&display->source); /* Mesa clear, before mask validation. */
    display->gl.Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (!gl_check(display, "startup inherited color/depth clear", error))
        return source_leave(display, false, error);
    display->frame_started = true;
    *stage = MC_CLIENT_STARTUP_PROJECTION_ORTHO;
    display->source.matrix_mode = GL_PROJECTION; /* Actual MatrixMode29. */
    const float identity[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    source_material_vertices(&display->source); /* Actual identity32, even unchanged. */
    source_matrix_changed(&display->source);
    memcpy(display->source.projection, identity, sizeof(identity)); /* Actual identity32. */
    if (!source_fp_admit("startup ortho FP after clear", error))
        return source_leave(display, false, error);
    source_material_vertices(&display->source); /* Admitted nondegenerate glOrtho. */
    const float projection[16] = {(float)(2.0 / width),
                                  0,
                                  0,
                                  0,
                                  0,
                                  (float)(-2.0 / height),
                                  0,
                                  0,
                                  0,
                                  0,
                                  (float)(-2.0 / 2000.0),
                                  0,
                                  -1,
                                  1,
                                  -2,
                                  1};
    memcpy(display->source.projection, projection, sizeof(projection));
    *stage = MC_CLIENT_STARTUP_PROJECTION_MODELVIEW;
    display->source.matrix_mode = GL_MODELVIEW; /* Actual MatrixMode57. */
    /* LoadIdentity and its following Translate both flush; nothing stages
     * fixed attributes between these represented matrix operations. */
    source_material_vertices(&display->source);
    source_matrix_changed(&display->source);
    display->source.modelview =
        (mc_client_modelview){{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, -2000, 1}};
    display->source.loading_ready = true;
    display->exact_width = width;
    display->exact_height = height;
    return source_leave(display, true, error);
}

bool mc_client_display_startup_viewport(mc_client_display *display, int32_t width, int32_t height,
                                        mc_client_startup_projection_stage *stage,
                                        mc_client_display_error *error) {
    if (stage == NULL)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "startup viewport admission",
                    "null stage output", 0);
    *stage = MC_CLIENT_STARTUP_PROJECTION_VIEWPORT;
    if (width < 0 || height < 0)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "startup viewport admission",
                    "negative native viewport", 0);
    if (!source_enter(display, "startup source viewport", error))
        return false;
    if (display->source.compiling != NULL)
        return source_leave(display,
                            fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "startup viewport",
                                 "compiled viewport path", 0),
                            error);
    if (!source_material_viewport(display, width, height, error))
        return source_leave(display, false, error);
    display->gl.Viewport(0, 0, width, height);
    if (!gl_check(display, "startup actual App viewport", error))
        return source_leave(display, false, error);
    *stage = MC_CLIENT_STARTUP_PROJECTION_CLEAR_COLOR;
    display->gl.ClearColor(0, 0, 0, 0);
    return source_leave(display, gl_check(display, "startup next clear color", error), error);
}

bool mc_client_display_source_disable_unlit_feature(mc_client_display *display,
                                                    mc_client_source_unlit_feature feature,
                                                    mc_client_display_error *error) {
    if (feature != MC_CLIENT_SOURCE_LIGHTING && feature != MC_CLIENT_SOURCE_FOG)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "source unlit state",
                    "invalid source feature", 0);
    if (!source_enter(display, "source disable lighting/fog", error))
        return false;
    if (display->source.compiling != NULL)
        return source_leave(display,
                            fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source unlit state",
                                 "compiled lighting/fog path", 0),
                            error);
    if (feature == MC_CLIENT_SOURCE_LIGHTING) {
        if (display->source.lighting_enabled)
            source_material_vertices(&display->source);
        display->source.lighting_enabled = false;
    } else {
        if (display->source.fog_enabled)
            source_material_vertices(&display->source);
        display->source.fog_enabled = false;
    }
    return source_leave(display, true, error);
}

bool mc_client_display_startup_alpha(mc_client_display *display, mc_client_display_error *error) {
    if (!source_enter(display, "startup source alpha state", error))
        return false;
    if (display->source.compiling != NULL)
        return source_leave(display,
                            fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "startup alpha state",
                                 "compiled alpha path", 0),
                            error);
    if (display->source.lighting_enabled)
        source_material_vertices(&display->source);
    display->source.lighting_enabled = false;
    if (display->source.fog_enabled)
        source_material_vertices(&display->source);
    display->source.fog_enabled = false;
    if (!display->source.alpha_test)
        source_material_vertices(&display->source);
    display->source.alpha_test = true;
    return source_leave(display, true, error);
}
