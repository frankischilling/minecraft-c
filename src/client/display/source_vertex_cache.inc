enum {
    SOURCE_CACHE_SLOTS = 4096,
    SOURCE_CACHE_BYTES = 32 * 1024 * 1024,
    SOURCE_CACHE_MIN_VERTICES = 96
};

typedef struct source_cached_vertices {
    /* Recorded draw bytes identify one immutable, committed command. */
    const uint8_t *data;
    uint64_t generation, last_used;
    mc_client_source_attribute_profile profile;
    float color[4], normal[3];
    size_t bytes;
    GLuint vao, vbo;
    bool layout_ready;
    mc_client_source_gpu_vertex last_vertex;
} source_cached_vertices;

struct source_vertex_cache {
    source_cached_vertices slots[SOURCE_CACHE_SLOTS];
    size_t bytes;
    uint64_t sequence;
};

static size_t source_vertex_cache_index(const uint8_t *data) {
    uint64_t hash = (uintptr_t)data;
    hash ^= hash >> 33;
    hash *= UINT64_C(0xff51afd7ed558ccd);
    hash ^= hash >> 33;
    return (size_t)(hash & (SOURCE_CACHE_SLOTS - 1));
}

static source_cached_vertices *source_vertex_cache_slot(mc_client_display *display,
                                                        const uint8_t *data) {
    if (display->source_cache == NULL)
        display->source_cache = calloc(1, sizeof(*display->source_cache));
    if (display->source_cache == NULL)
        return NULL;
    return &display->source_cache->slots[source_vertex_cache_index(data)];
}

static bool source_vertex_cache_matches(const source_cached_vertices *entry, const uint8_t *data,
                                        uint64_t generation,
                                        mc_client_source_attribute_profile profile,
                                        const float *color, const float *normal) {
    return entry->data == data && entry->generation == generation && entry->profile == profile &&
           (color == NULL || memcmp(entry->color, color, sizeof(entry->color)) == 0) &&
           (normal == NULL || memcmp(entry->normal, normal, sizeof(entry->normal)) == 0);
}

static void source_vertex_cache_bind(mc_client_display *display,
                                     const source_cached_vertices *entry) {
    display->gl.BindVertexArray(entry->vao);
    display->gl.BindBuffer(GL_ARRAY_BUFFER, entry->vbo);
}

static void source_vertex_cache_invalidate(mc_client_display *display, const uint8_t *data) {
    if (display->source_cache == NULL)
        return;
    source_cached_vertices *entry = &display->source_cache->slots[source_vertex_cache_index(data)];
    if (entry->data == data) {
        entry->data = NULL;
        entry->last_used = 0;
    }
}

static bool source_vertex_cache_make_room(mc_client_display *display,
                                          const source_cached_vertices *entry, size_t bytes,
                                          mc_client_display_error *error) {
    source_vertex_cache *cache = display->source_cache;
    while (bytes > SOURCE_CACHE_BYTES - (cache->bytes - entry->bytes)) {
        /* The incoming batch fits by itself, so other slots hold the excess. */
        source_cached_vertices *oldest = NULL;
        for (size_t i = 0; i < SOURCE_CACHE_SLOTS; ++i) {
            source_cached_vertices *candidate = &cache->slots[i];
            if (candidate != entry && candidate->bytes != 0 &&
                (oldest == NULL || candidate->last_used < oldest->last_used))
                oldest = candidate;
        }
        oldest->data = NULL;
        display->gl.DeleteBuffers(1, &oldest->vbo);
        display->gl.DeleteVertexArrays(1, &oldest->vao);
        if (!gl_check(display, "evict source vertex cache storage", error))
            return false;
        cache->bytes -= oldest->bytes;
        *oldest = (source_cached_vertices){0};
    }
    return true;
}

static bool source_vertex_cache_upload(mc_client_display *display, source_cached_vertices *entry,
                                       const uint8_t *data, size_t bytes,
                                       mc_client_source_attribute_profile profile,
                                       const float *color, const float *normal,
                                       const mc_client_source_gpu_vertex *vertices, size_t count,
                                       mc_client_display_error *error) {
    entry->data = NULL;
    if (!source_vertex_cache_make_room(display, entry, bytes, error))
        return false;
    if (!entry->layout_ready) {
        if (entry->vao == 0)
            display->gl.GenVertexArrays(1, &entry->vao);
        if (entry->vbo == 0)
            display->gl.GenBuffers(1, &entry->vbo);
        if (!gl_check(display, "source vertex cache storage", error))
            return false;
        if (entry->vao == 0 || entry->vbo == 0)
            return fail(error, MC_CLIENT_DISPLAY_GL_ERROR, "source vertex cache storage",
                        "zero name", 0);
        source_vertex_cache_bind(display, entry);
        source_gpu_vertex_layout(display);
        if (!gl_check(display, "source vertex cache layout", error))
            return false;
        entry->layout_ready = true;
    } else {
        source_vertex_cache_bind(display, entry);
    }
    if (entry->bytes == bytes) {
        display->gl.BufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)bytes, vertices);
        if (!gl_check(display, "source VBO upload", error))
            return false;
    } else {
        display->gl.BufferData(GL_ARRAY_BUFFER, (GLsizeiptr)bytes, vertices, GL_STATIC_DRAW);
        if (!gl_check(display, "source VBO allocation/upload", error)) {
            display->source.vbo_capacity_uncertain = true;
            return false;
        }
        display->source_cache->bytes = display->source_cache->bytes - entry->bytes + bytes;
        entry->bytes = bytes;
    }
    entry->data = data;
    entry->generation = display->native_context_generation;
    entry->profile = profile;
    entry->last_vertex = vertices[count - 1];
    if (color != NULL)
        memcpy(entry->color, color, sizeof(entry->color));
    if (normal != NULL)
        memcpy(entry->normal, normal, sizeof(entry->normal));
    return true;
}

static void source_vertex_cache_retire(mc_client_display *display, bool *ok,
                                       mc_client_display_error *first) {
    if (display->source_cache == NULL)
        return;
    for (size_t i = 0; i < SOURCE_CACHE_SLOTS && !display->context_lost; ++i) {
        const source_cached_vertices *entry = &display->source_cache->slots[i];
        if (entry->vbo != 0) {
            display->gl.DeleteBuffers(1, &entry->vbo);
            cleanup_check(display, "delete source vertex cache VBO", ok, first);
        }
        if (!display->context_lost && entry->vao != 0) {
            display->gl.DeleteVertexArrays(1, &entry->vao);
            cleanup_check(display, "delete source vertex cache VAO", ok, first);
        }
    }
}
