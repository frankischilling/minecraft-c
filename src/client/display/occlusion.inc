/* Included by the actual Display TU after source_surface.inc. Optional ARB
 * pointers stay on the one context owner, outside mandatory load_gl entries. */
bool mc_client_display_source_arb_occlusion_query(mc_client_display *display, bool *out,
                                                  mc_client_display_error *error) {
    if (out == NULL || display == NULL)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "current ARB occlusion capability",
                    "null owner/output", 0);
    /* GLContext.getCapabilities does not switch to a different context. */
    if (display->context == NULL || display->window == NULL ||
        SDL_GL_GetCurrentContext() != display->context ||
        SDL_GL_GetCurrentWindow() != display->window)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "current ARB occlusion capability",
                    "this Display context is not current", 0);
    if (!source_enter(display, "current ARB occlusion capability", error))
        return false;
    if (!display->functions_ready || display->native_context_generation == 0)
        return source_leave(display,
                            fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL,
                                 "current ARB occlusion capability", "incomplete GL context", 0),
                            error);
    if (!display->occlusion.initialized ||
        display->occlusion.generation != display->native_context_generation) {
        display_occlusion_capability candidate = {0};
        candidate.generation = display->native_context_generation;
        const PFNGLGETSTRINGIPROC get_string_i =
            (PFNGLGETSTRINGIPROC)SDL_GL_GetProcAddress("glGetStringi");
        if (get_string_i == NULL || display->gl.GetIntegerv == NULL)
            return source_leave(display,
                                fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL,
                                     "enumerate actual context extensions",
                                     "GL3 extension introspection function unavailable", 0),
                                error);
        GLint count = 0;
        display->gl.GetIntegerv(GL_NUM_EXTENSIONS, &count);
        if (count < 0)
            return source_leave(display,
                                fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL,
                                     "enumerate actual context extensions",
                                     "negative native extension count", 0),
                                error);
        for (GLint index = 0; index < count; ++index) {
            const GLubyte *token = get_string_i(GL_EXTENSIONS, (GLuint)index);
            if (token == NULL)
                return source_leave(display,
                                    fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL,
                                         "enumerate actual context extensions",
                                         "null native extension token", 0),
                                    error);
            if (strcmp((const char *)token, "GL_ARB_occlusion_query") == 0)
                candidate.advertised = true;
        }
        if (candidate.advertised) {
            /* Actual ContextCapabilities helper resolves all eight, combining
             * with non-short-circuit AND. No core-name fallback is invented. */
            candidate.GenQueries = (PFNGLGENQUERIESARBPROC)SDL_GL_GetProcAddress("glGenQueriesARB");
            candidate.DeleteQueries =
                (PFNGLDELETEQUERIESARBPROC)SDL_GL_GetProcAddress("glDeleteQueriesARB");
            candidate.IsQuery = (PFNGLISQUERYARBPROC)SDL_GL_GetProcAddress("glIsQueryARB");
            candidate.BeginQuery = (PFNGLBEGINQUERYARBPROC)SDL_GL_GetProcAddress("glBeginQueryARB");
            candidate.EndQuery = (PFNGLENDQUERYARBPROC)SDL_GL_GetProcAddress("glEndQueryARB");
            candidate.GetQueryiv = (PFNGLGETQUERYIVARBPROC)SDL_GL_GetProcAddress("glGetQueryivARB");
            candidate.GetQueryObjectiv =
                (PFNGLGETQUERYOBJECTIVARBPROC)SDL_GL_GetProcAddress("glGetQueryObjectivARB");
            candidate.GetQueryObjectuiv =
                (PFNGLGETQUERYOBJECTUIVARBPROC)SDL_GL_GetProcAddress("glGetQueryObjectuivARB");
            candidate.supported = candidate.GenQueries != NULL && candidate.DeleteQueries != NULL &&
                                  candidate.IsQuery != NULL && candidate.BeginQuery != NULL &&
                                  candidate.EndQuery != NULL && candidate.GetQueryiv != NULL &&
                                  candidate.GetQueryObjectiv != NULL &&
                                  candidate.GetQueryObjectuiv != NULL;
        }
        candidate.initialized = true;
        display->occlusion = candidate;
    }
    *out = display->occlusion.supported;
    return source_leave(display, true, error);
}
