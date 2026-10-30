/* Concrete native query occurrence, included in the canonical Display TU. */
struct mc_client_display_query_names {
    mc_client_display *display;
    mc_client_display_context_lease *context;
    uint64_t generation;
    GLuint *names;
    GLsizei count;
    size_t query_holds;
    bool issued, deleted;
};

struct mc_client_display_active_query {
    mc_client_display_query_names *names;
    uint64_t generation;
    GLuint id;
    bool begin_issued, source_ended, native_aborted;
};

_Static_assert(sizeof(GLuint) == sizeof(int32_t), "actual LWJGL IntBuffer query-name width");

bool mc_client_display_source_gen_queries(mc_client_display *display, void *storage, size_t count,
                                          mc_client_display_query_names **out,
                                          mc_client_display_error *error) {
    if (display == NULL || out == NULL || *out != NULL || (storage == NULL && count != 0) ||
        count > INT32_MAX)
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
    if (display->source.active_calls != 0 || names->query_holds != 0)
        return fail(error, MC_CLIENT_DISPLAY_BUSY, "retire native ARB queries",
                    "Display source call or query occurrence active", 0);
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
                                           .query_holds = names->query_holds,
                                           .generation = names->generation,
                                           .issued = names->issued,
                                           .deleted = names->deleted};
    return true;
}

static bool query_current(mc_client_display *display, const char *operation,
                          mc_client_display_error *error) {
    if (display == NULL || display->window == NULL || display->context == NULL ||
        SDL_GL_GetCurrentWindow() != display->window ||
        SDL_GL_GetCurrentContext() != display->context)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, operation,
                    "the owning Display context is not current", 0);
    return true;
}

static bool query_owner(mc_client_display *display, const mc_client_display_query_names *names,
                        GLuint id, const char *operation, mc_client_display_error *error) {
    if (display == NULL || names == NULL || names->display != display || !names->issued ||
        names->deleted)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, operation,
                    "live generated names from this Display required", 0);
    if (names->generation != display->native_context_generation ||
        !display->occlusion.initialized || !display->occlusion.supported ||
        display->occlusion.generation != names->generation)
        return fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, operation,
                    "original context/query generation unavailable", 0);
    for (GLsizei i = 0; i < names->count; ++i)
        if (names->names[i] == id)
            return true;
    return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, operation,
                "query ID is outside its native generated-name owner", 0);
}

bool mc_client_display_source_begin_query(mc_client_display *display,
                                          mc_client_display_query_names *names, int32_t id,
                                          mc_client_display_active_query **out,
                                          mc_client_display_error *error) {
    const char *operation = "source ARB glBeginQuery";
    if (out == NULL || *out != NULL)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, operation,
                    "initially empty active-query output required", 0);
    if (!query_current(display, operation, error) ||
        !query_owner(display, names, (GLuint)(uint32_t)id, operation, error))
        return false;
    if (display->active_query != NULL || display->source.active_calls != 0)
        return fail(error, MC_CLIENT_DISPLAY_BUSY, operation,
                    "source query or Display operation already active", 0);
    if (display->occlusion.BeginQuery == NULL || display->occlusion.EndQuery == NULL)
        return fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, operation,
                    "same-context cached ARB begin/end addresses required", 0);
    if (names->query_holds == SIZE_MAX)
        return fail(error, MC_CLIENT_DISPLAY_LIMIT, operation, "native query hold limit", 0);
    mc_client_display_active_query *query = calloc(1, sizeof(*query));
    if (query == NULL)
        return fail(error, MC_CLIENT_DISPLAY_ALLOCATION_FAILED, operation,
                    "native active-query allocation failed", 0);
    query->names = names;
    query->generation = names->generation;
    query->id = (GLuint)(uint32_t)id;
    ++names->query_holds;
    display->active_query = query;
    *out = query; /* Retain cleanup rights before admission and the void effect. */
    if (!source_enter(display, operation, error))
        return false;
    query->begin_issued = true; /* Do not infer GL error rollback. */
    display->occlusion.BeginQuery((GLenum)35092, query->id);
    return source_leave(display, true, error);
}

bool mc_client_display_source_end_query(mc_client_display_active_query *query,
                                        mc_client_display_error *error) {
    const char *operation = "source ARB glEndQuery";
    if (query == NULL || !query->begin_issued || query->source_ended || query->native_aborted)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, operation,
                    "same issued and unended query occurrence required", 0);
    mc_client_display *display = query->names->display;
    if (!query_current(display, operation, error) ||
        !query_owner(display, query->names, query->id, operation, error))
        return false;
    if (display->active_query != query)
        return fail(error, MC_CLIENT_DISPLAY_BUSY, operation,
                    "active query belongs to another native occurrence", 0);
    if (display->occlusion.EndQuery == NULL)
        return fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, operation,
                    "original ARB end address unavailable", 0);
    if (!source_enter(display, operation, error))
        return false;
    display->occlusion.EndQuery((GLenum)35092);
    query->source_ended = true;
    display->active_query = NULL;
    return source_leave(display, true, error);
}

bool mc_client_display_source_query_result(mc_client_display *display,
                                           mc_client_display_query_names *names, int32_t id,
                                           uint32_t pname, void *storage, size_t remaining,
                                           mc_client_display_error *error) {
    const char *operation = "source ARB glGetQueryObjectu";
    if (!query_current(display, operation, error) ||
        !query_owner(display, names, (GLuint)(uint32_t)id, operation, error))
        return false;
    if (display->occlusion.GetQueryObjectuiv == NULL)
        return fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, operation,
                    "original unsigned ARB result address unavailable", 0);
    if ((pname != 34919u && pname != 34918u) || storage == NULL ||
        (uintptr_t)storage % _Alignof(GLuint) != 0)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, operation,
                    "selected pname and writable remaining IntBuffer word required", 0);
    if (remaining < 1)
        return fail(error, MC_CLIENT_DISPLAY_BUFFER_TOO_SMALL, operation,
                    "IntBuffer requires at least one remaining word", 0);
    if (!source_enter(display, operation, error))
        return false;
    display->occlusion.GetQueryObjectuiv((GLuint)(uint32_t)id, (GLenum)pname, (GLuint *)storage);
    return source_leave(display, true, error);
}

bool mc_client_display_active_query_inspect(const mc_client_display_active_query *query,
                                            mc_client_display_active_query_state *out) {
    if (query == NULL || out == NULL || query->names->display->source.active_calls != 0)
        return false;
    uint32_t bits = (uint32_t)query->id;
    int32_t id;
    memcpy(&id, &bits, sizeof(id));
    *out = (mc_client_display_active_query_state){.id = id,
                                                  .generation = query->generation,
                                                  .begin_issued = query->begin_issued,
                                                  .source_ended = query->source_ended,
                                                  .native_aborted = query->native_aborted};
    return true;
}

bool mc_client_display_active_query_retire(mc_client_display_active_query **slot,
                                           mc_client_display_error *error) {
    const char *operation = "retire native active ARB query";
    if (slot == NULL)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, operation, "null slot", 0);
    mc_client_display_active_query *query = *slot;
    if (query == NULL)
        return success(error);
    mc_client_display *display = query->names->display;
    if (display->source.active_calls != 0)
        return fail(error, MC_CLIENT_DISPLAY_BUSY, operation, "Display source call active", 0);
    if (!query->source_ended && !query->native_aborted) {
        if (display->active_query != query)
            return fail(error, MC_CLIENT_DISPLAY_BUSY, operation, "active query occurrence changed",
                        0);
        if (query->begin_issued) {
            if (!current(display, operation, error))
                return false;
            if (query->generation != display->native_context_generation ||
                display->occlusion.generation != query->generation ||
                display->occlusion.EndQuery == NULL)
                return fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, operation,
                            "original context/ARB end address unavailable", 0);
            ++display->source.active_calls;
            query->native_aborted = true; /* Terminal native End, never source success. */
            display->occlusion.EndQuery((GLenum)35092);
            --display->source.active_calls;
        }
        display->active_query = NULL;
    }
    --query->names->query_holds;
    free(query);
    *slot = NULL;
    return success(error);
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

bool mc_client_display_source_draw_inspect(const mc_client_display *display,
                                           mc_client_source_draw_state *out) {
    if (display == NULL || out == NULL || !display->source.initialized ||
        display->source.active_calls != 0)
        return false;
    memcpy(out->color, display->source.color, sizeof(out->color));
    out->color_known = display->source.color_known;
    out->texture_enabled = display->source.texture_enabled;
    out->alpha_test = display->source.alpha_test;
    out->shade_flat = display->source.shade_flat;
    out->lighting_enabled = display->source.lighting_enabled;
    out->alpha_function_set = display->source.alpha_function_set;
    out->alpha_reference = display->source.alpha_reference;
    return true;
}
