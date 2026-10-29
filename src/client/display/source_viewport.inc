bool mc_client_display_source_viewport(mc_client_display *display, int32_t width, int32_t height,
                                       mc_client_display_error *error) {
    if (!source_enter(display, "source MC viewport", error))
        return false;
    if (display->source.compiling != NULL)
        return source_leave(display,
                            fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source MC viewport",
                                 "compiled viewport operation is not implemented", 0),
                            error);
    /* Negative source dimensions reach the real driver and its GL error flag.
     * There is no eager size clamp or manufactured Java/native failure. */
    display->gl.Viewport(0, 0, width, height);
    return source_leave(display, true, error);
}
