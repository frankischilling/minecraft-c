/* Runs inside an already admitted source call; it never enters or leaves it. */
static bool source_material_viewport(mc_client_display *display, int32_t width, int32_t height,
                                     mc_client_display_error *error) {
    if (width >= 0 && height >= 0 && source_material_is_staged(&display->source) &&
        (display->source.attribute_staging.flags & SOURCE_STAGE_STORED) != 0) {
        /* Mesa validates first, clamps, then tests all indexed viewports.
         * Index0 alone can match while untouched later indices differ. */
        GLint limits[2] = {0}, count = 0;
        display->gl.GetIntegerv(GL_MAX_VIEWPORT_DIMS, limits);
        display->gl.GetIntegerv(GL_MAX_VIEWPORTS, &count);
        /* The captured llvmpipe limits are small exact integers. Do not add
         * rounded integer-to-float work under an unchecked caller FP mode. */
        if (limits[0] <= 0 || limits[1] <= 0 || limits[0] > 16777216 || limits[1] > 16777216 ||
            count <= 0)
            return fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source MC viewport",
                        "unimplemented Mesa viewport limit precision", 0);
        PFNGLGETFLOATI_VPROC get_indexed = NULL;
        SDL_FunctionPointer query = SDL_GL_GetProcAddress("glGetFloati_v");
        _Static_assert(sizeof(get_indexed) == sizeof(query),
                       "GL indexed viewport query representation");
        if (query == NULL)
            return fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source MC viewport",
                        "actual indexed core viewport query unavailable", 0);
        memcpy(&get_indexed, &query, sizeof(get_indexed));
        const GLfloat desired[4] = {0, 0, (GLfloat)(width > limits[0] ? limits[0] : width),
                                    (GLfloat)(height > limits[1] ? limits[1] : height)};
        for (GLint index = 0; index < count; ++index) {
            GLfloat actual[4] = {0};
            get_indexed(GL_VIEWPORT, (GLuint)index, actual);
            if (!source_material_float_equal(actual[0], desired[0]) ||
                !source_material_float_equal(actual[1], desired[1]) ||
                !source_material_float_equal(actual[2], desired[2]) ||
                !source_material_float_equal(actual[3], desired[3])) {
                source_material_vertices(&display->source);
                break;
            }
        }
    }
    return true;
}

bool mc_client_display_source_viewport(mc_client_display *display, int32_t width, int32_t height,
                                       mc_client_display_error *error) {
    if (!source_enter(display, "source MC viewport", error))
        return false;
    if (display->source.compiling != NULL)
        return source_leave(display,
                            fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source MC viewport",
                                 "compiled viewport operation is not implemented", 0),
                            error);
    if (!source_material_viewport(display, width, height, error))
        return source_leave(display, false, error);
    /* Negative source dimensions reach the real driver and its GL error flag.
     * There is no eager size clamp or manufactured Java/native failure. */
    display->gl.Viewport(0, 0, width, height);
    return source_leave(display, true, error);
}
