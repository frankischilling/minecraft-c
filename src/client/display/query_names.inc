/* Concrete native query occurrence, included in the canonical Display TU. */
struct mc_client_display_query_names {
    mc_client_display *display;
    mc_client_display_context_lease *context;
    uint64_t generation;
    GLuint *names;
    GLsizei count;
    bool issued, deleted;
};

_Static_assert(sizeof(GLuint) == sizeof(int32_t), "actual LWJGL IntBuffer query-name width");

bool mc_client_display_source_gen_queries(mc_client_display *display, void *storage, size_t count,
                                          mc_client_display_query_names **out,
                                          mc_client_display_error *error) {
    if (display == NULL || out == NULL || (storage == NULL && count != 0) || count > INT32_MAX)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "actual ARB query span",
                    "owner/output/storage/count", 0);
    if (display->window == NULL || display->context == NULL ||
        SDL_GL_GetCurrentWindow() != display->window ||
        SDL_GL_GetCurrentContext() != display->context)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "actual ARB query context",
                    "the owning context is not current", 0);
    if (!display->occlusion.initialized || !display->occlusion.supported ||
        display->occlusion.generation != display->native_context_generation ||
        display->occlusion.GenQueries == NULL || display->occlusion.DeleteQueries == NULL)
        return fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "actual ARB query capability",
                    "same-context cached source capability required", 0);
    mc_client_display_query_names *names = calloc(1, sizeof(*names));
    if (names == NULL)
        return fail(error, MC_CLIENT_DISPLAY_ALLOCATION_FAILED, "native ARB query owner",
                    "allocation failed", 0);
    if (!mc_client_display_context_acquire(display, &names->context, error)) {
        free(names);
        return false;
    }
    names->display = display;
    names->generation = display->native_context_generation;
    names->names = (GLuint *)storage;
    names->count = (GLsizei)count;
    *out = names; /* Retain rights before every later failure and the void effect. */
    if (!source_enter(display, "source ARB glGenQueries", error))
        return false;
    names->issued = true; /* Issued effects are never guessed to have rolled back. */
    display->occlusion.GenQueries(names->count, names->names);
    return source_leave(display, true, error);
}

bool mc_client_display_query_names_retire(mc_client_display_query_names **slot,
                                          mc_client_display_error *error) {
    if (slot == NULL)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "retire native ARB queries",
                    "null slot", 0);
    mc_client_display_query_names *names = *slot;
    if (names == NULL)
        return success(error);
    mc_client_display *display = names->display;
    if (display->source.active_calls != 0)
        return fail(error, MC_CLIENT_DISPLAY_BUSY, "retire native ARB queries",
                    "Display source call active", 0);
    if (names->issued && !names->deleted) {
        if (!current(display, "retire native ARB queries", error))
            return false;
        if (names->generation != display->native_context_generation ||
            display->occlusion.generation != names->generation ||
            display->occlusion.DeleteQueries == NULL)
            return fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "retire native ARB queries",
                        "original context/query generation unavailable", 0);
        ++display->source.active_calls;
        names->deleted = true; /* Never issue a second deletion after this boundary. */
        display->occlusion.DeleteQueries(names->count, names->names);
        --display->source.active_calls;
    }
    if (!mc_client_display_context_release(&names->context, error))
        return false;
    free(names);
    *slot = NULL;
    return success(error);
}

bool mc_client_display_query_names_inspect(const mc_client_display_query_names *names,
                                           mc_client_display_query_state *out) {
    if (names == NULL || out == NULL)
        return false;
    *out = (mc_client_display_query_state){.count = (size_t)names->count,
                                           .generation = names->generation,
                                           .issued = names->issued,
                                           .deleted = names->deleted};
    return true;
}

bool mc_client_display_lists_abort(mc_client_display *display, int32_t id,
                                   mc_client_display_error *error) {
    if (display == NULL)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "abort native list definition",
                    "null Display", 0);
    if (display->source.active_calls != 0)
        return fail(error, MC_CLIENT_DISPLAY_BUSY, "abort native list definition",
                    "Display source call active", 0);
    mc_client_source_list *actual = source_find_list(display, id, NULL);
    if (actual == NULL ||
        (display->source.compiling != NULL && display->source.compiling != actual))
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "abort native list definition",
                    "unregistered or foreign pending definition", 0);
    if (display->source.compiling == actual) {
        display->source.compiling = NULL;
        display->source.compiling_range = NULL;
        /* Preserve building=true and every appended command: this partial
         * definition never becomes a successful source glEndList. */
    }
    return success(error);
}

bool mc_client_display_list_inspect(mc_client_display *display, int32_t id,
                                    mc_client_source_list_state *out) {
    if (display == NULL || out == NULL)
        return false;
    const mc_client_source_list *list = source_find_list(display, id, NULL);
    if (list == NULL)
        return false;
    mc_client_source_list_state state = {.defined = list->defined, .building = list->building};
    if (list->pending != NULL) {
        state.pending_commands = list->pending->commands;
        state.pending_bytes = sizeof(*list->pending) + list->pending->bytes;
    }
    for (const mc_client_source_command *c = list->first; c != NULL; c = c->next) {
        ++state.commands;
        if (c->kind == SOURCE_COMMAND_DRAW) {
            ++state.draws;
            state.vertices += (size_t)c->value.draw.count;
        }
    }
    *out = state;
    return true;
}

static bool source_inspect_batch(const mc_client_source_command *first, size_t index,
                                  mc_client_source_batch_view *out) {
    for (const mc_client_source_command *c = first; c != NULL; c = c->next) {
        if (c->kind != SOURCE_COMMAND_DRAW)
            continue;
        if (index-- == 0) {
            *out = (mc_client_source_batch_view){.bytes = c->value.draw.bytes,
                                                 .byte_count = c->value.draw.size,
                                                 .vertices = c->value.draw.count,
                                                 .mode = c->value.draw.mode,
                                                 .arrays = c->value.draw.arrays};
            return true;
        }
    }
    return false;
}

bool mc_client_display_list_batch(mc_client_display *display, int32_t id, size_t index,
                                  mc_client_source_batch_view *out) {
    if (display == NULL || out == NULL)
        return false;
    const mc_client_source_list *list = source_find_list(display, id, NULL);
    return list != NULL && source_inspect_batch(list->first, index, out);
}

bool mc_client_display_list_pending_batch(mc_client_display *display, int32_t id, size_t index,
                                          mc_client_source_batch_view *out) {
    if (display == NULL || out == NULL)
        return false;
    const mc_client_source_list *list = source_find_list(display, id, NULL);
    return list != NULL && list->pending != NULL &&
           source_inspect_batch(list->pending->first, index, out);
}
