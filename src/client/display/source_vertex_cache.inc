#if defined(_MSC_VER)
#include <intrin.h>
#endif

enum {
    SOURCE_CACHE_SLOTS = 4096,
    SOURCE_CACHE_WAYS = 4,
    SOURCE_CACHE_GROUP_SIZE = 64,
    SOURCE_CACHE_GROUPS = SOURCE_CACHE_SLOTS / SOURCE_CACHE_GROUP_SIZE,
    SOURCE_CACHE_BYTES = 64 * 1024 * 1024,
    SOURCE_CACHE_MIN_VERTICES = 6,
    SOURCE_CACHE_REGULAR_VERTICES = 18,
    SOURCE_CACHE_LARGE_VERTICES = 96
};

_Static_assert(SOURCE_CACHE_SLOTS % SOURCE_CACHE_GROUP_SIZE == 0 && SOURCE_CACHE_GROUPS <= 64,
               "one dirty bit per complete vertex cache group");
_Static_assert(SOURCE_CACHE_GROUP_SIZE % SOURCE_CACHE_WAYS == 0,
               "each vertex cache set is contained in one eviction group");
_Static_assert(SOURCE_CACHE_GROUP_SIZE <= 64, "one occupancy bit per slot in a group");
_Static_assert((SOURCE_CACHE_GROUPS & (SOURCE_CACHE_GROUPS - 1)) == 0,
               "the eviction tree has a power-of-two leaf count");

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
    source_cached_vertices *eviction_tree[2 * SOURCE_CACHE_GROUPS];
    uint64_t occupied[SOURCE_CACHE_GROUPS];
    uint8_t occupied_count[SOURCE_CACHE_GROUPS];
    size_t bytes, tiny_bytes;
    uint64_t sequence, dirty_groups;
};

static size_t source_vertex_cache_index(const uint8_t *data) {
    uint64_t hash = (uintptr_t)data;
    hash ^= hash >> 33;
    hash *= UINT64_C(0xff51afd7ed558ccd);
    hash ^= hash >> 33;
    return (size_t)(hash & (SOURCE_CACHE_SLOTS / SOURCE_CACHE_WAYS - 1)) * SOURCE_CACHE_WAYS;
}

static size_t source_vertex_cache_tiny_charge(size_t bytes) {
    return bytes < SOURCE_CACHE_REGULAR_VERTICES * sizeof(mc_client_source_gpu_vertex) ? bytes : 0;
}

static source_cached_vertices *source_vertex_cache_resident(source_vertex_cache *cache,
                                                            const uint8_t *data, uint32_t hint) {
    if (cache == NULL || hint == 0)
        return NULL;
    source_cached_vertices *entry = &cache->slots[hint - 1u];
    return entry->data == data ? entry : NULL;
}

static source_cached_vertices *source_vertex_cache_slot(mc_client_display *display,
                                                        const uint8_t *data, size_t bytes,
                                                        uint32_t hint) {
    if (display->source_cache == NULL)
        display->source_cache = calloc(1, sizeof(*display->source_cache));
    if (display->source_cache == NULL)
        return NULL;
    source_vertex_cache *cache = display->source_cache;
    source_cached_vertices *resident = source_vertex_cache_resident(cache, data, hint);
    if (resident != NULL)
        return resident;
    source_cached_vertices *set = &cache->slots[source_vertex_cache_index(data)];

    const size_t regular = SOURCE_CACHE_REGULAR_VERTICES * sizeof(mc_client_source_gpu_vertex);
    const size_t large = SOURCE_CACHE_LARGE_VERTICES * sizeof(mc_client_source_gpu_vertex);
    const bool tiny = bytes < regular;
    const bool small = bytes < large;
    const size_t protected_bytes = tiny ? regular : large;
    const size_t free_bytes = SOURCE_CACHE_BYTES - cache->bytes;
    source_cached_vertices *replacement = NULL;
    source_cached_vertices *tiny_replacement = NULL;
    /* Tiny batches use spare budget or replace a tiny slot. A regular batch
     * may also reclaim tiny storage when spare bytes run out. */
    for (size_t i = 0; i < SOURCE_CACHE_WAYS; ++i) {
        source_cached_vertices *entry = &set[i];
        if (small) {
            const size_t reclaimable =
                tiny ? 0 : cache->tiny_bytes - source_vertex_cache_tiny_charge(entry->bytes);
            if ((entry->data != NULL && entry->bytes >= protected_bytes) ||
                bytes > free_bytes + entry->bytes + reclaimable)
                continue;
        }
        if (!tiny && entry->data != NULL && entry->bytes < regular) {
            if (tiny_replacement == NULL || entry->last_used < tiny_replacement->last_used)
                tiny_replacement = entry;
            continue;
        }
        if (replacement == NULL || entry->last_used < replacement->last_used)
            replacement = entry;
    }
    if (tiny_replacement != NULL && (replacement == NULL || replacement->data != NULL))
        return tiny_replacement;
    return replacement;
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

static void source_vertex_cache_changed(source_vertex_cache *cache,
                                        const source_cached_vertices *entry) {
    const size_t index = (size_t)(entry - cache->slots);
    cache->dirty_groups |= UINT64_C(1) << (index / SOURCE_CACHE_GROUP_SIZE);
}

static void source_vertex_cache_set_bytes(source_vertex_cache *cache, source_cached_vertices *entry,
                                          size_t bytes) {
    const size_t before = entry->bytes;
    const size_t previous_tiny = source_vertex_cache_tiny_charge(before);
    const size_t next_tiny = source_vertex_cache_tiny_charge(bytes);
    cache->bytes = cache->bytes - before + bytes;
    cache->tiny_bytes = cache->tiny_bytes - previous_tiny + next_tiny;
    entry->bytes = bytes;

    const size_t index = (size_t)(entry - cache->slots);
    const size_t group = index / SOURCE_CACHE_GROUP_SIZE;
    const bool occupancy_changed = (before == 0) != (bytes == 0);
    if (occupancy_changed) {
        cache->occupied[group] ^= UINT64_C(1) << (index % SOURCE_CACHE_GROUP_SIZE);
        if (bytes != 0)
            ++cache->occupied_count[group];
        else
            --cache->occupied_count[group];
    }
    if (occupancy_changed || (previous_tiny != 0) != (next_tiny != 0))
        cache->dirty_groups |= UINT64_C(1) << group;
}

static void source_vertex_cache_touch(source_vertex_cache *cache, source_cached_vertices *entry) {
    entry->last_used = ++cache->sequence;
    source_vertex_cache_changed(cache, entry);
}

static void source_vertex_cache_invalidate(mc_client_display *display, const uint8_t *data,
                                           uint32_t hint) {
    source_cached_vertices *entry = source_vertex_cache_resident(display->source_cache, data, hint);
    if (entry != NULL) {
        entry->data = NULL;
        entry->last_used = 0;
        source_vertex_cache_changed(display->source_cache, entry);
    }
}

static bool source_vertex_cache_eviction_before(const source_cached_vertices *a,
                                                const source_cached_vertices *b) {
    if (b == NULL)
        return true;
    const bool a_tiny = source_vertex_cache_tiny_charge(a->bytes) != 0;
    const bool b_tiny = source_vertex_cache_tiny_charge(b->bytes) != 0;
    return a_tiny == b_tiny ? a->last_used < b->last_used : a_tiny;
}

static size_t source_vertex_cache_first_bit(uint64_t bits) {
#if defined(_MSC_VER)
    unsigned long index;
    _BitScanForward64(&index, bits);
    return (size_t)index;
#else
    return (size_t)__builtin_ctzll(bits);
#endif
}

static source_cached_vertices *
source_vertex_cache_group_oldest(source_vertex_cache *cache, size_t group,
                                 const source_cached_vertices *excluded) {
    const size_t start = group * SOURCE_CACHE_GROUP_SIZE;
    source_cached_vertices *oldest = NULL;
    /* Contiguous reads are cheaper when most of a group is occupied. */
    if (cache->occupied_count[group] > SOURCE_CACHE_GROUP_SIZE / 2) {
        for (size_t i = start; i < start + SOURCE_CACHE_GROUP_SIZE; ++i) {
            source_cached_vertices *candidate = &cache->slots[i];
            if (candidate != excluded && candidate->bytes != 0 &&
                source_vertex_cache_eviction_before(candidate, oldest))
                oldest = candidate;
        }
        return oldest;
    }

    uint64_t occupied = cache->occupied[group];
    if (excluded != NULL) {
        const size_t offset = (size_t)(excluded - (cache->slots + start));
        occupied &= ~(UINT64_C(1) << offset);
    }
    while (occupied != 0) {
        const size_t offset = source_vertex_cache_first_bit(occupied);
        occupied &= occupied - 1;
        source_cached_vertices *candidate = &cache->slots[start + offset];
        if (source_vertex_cache_eviction_before(candidate, oldest))
            oldest = candidate;
    }
    return oldest;
}

static source_cached_vertices *source_vertex_cache_min(source_cached_vertices *left,
                                                       source_cached_vertices *right) {
    /* All slots from the left subtree precede those from the right. */
    return right != NULL && source_vertex_cache_eviction_before(right, left) ? right : left;
}

static void source_vertex_cache_refresh(source_vertex_cache *cache) {
    uint64_t dirty = cache->dirty_groups;
    uint64_t parents = 0;
    while (dirty != 0) {
        const size_t group = source_vertex_cache_first_bit(dirty);
        dirty &= dirty - 1;
        cache->eviction_tree[SOURCE_CACHE_GROUPS + group] =
            source_vertex_cache_group_oldest(cache, group, NULL);
        parents |= UINT64_C(1) << (group / 2);
    }
    /* Each dirty leaf contributes its parent once at each level. */
    for (size_t width = SOURCE_CACHE_GROUPS / 2; width != 0 && parents != 0; width /= 2) {
        dirty = parents;
        parents = 0;
        while (dirty != 0) {
            const size_t index = source_vertex_cache_first_bit(dirty);
            dirty &= dirty - 1;
            const size_t node = width + index;
            cache->eviction_tree[node] = source_vertex_cache_min(
                cache->eviction_tree[2 * node], cache->eviction_tree[2 * node + 1]);
            parents |= UINT64_C(1) << (index / 2);
        }
    }
    cache->dirty_groups = 0;
}

static source_cached_vertices *source_vertex_cache_oldest(source_vertex_cache *cache,
                                                          const source_cached_vertices *excluded) {
    source_vertex_cache_refresh(cache);
    source_cached_vertices *oldest = cache->eviction_tree[1];
    if (oldest == NULL || oldest != excluded)
        return oldest;

    /* Scan the excluded slot's group again, then combine its sibling
     * subtrees with lower indices breaking ties. The shared index stays intact. */
    const size_t group = (size_t)(excluded - cache->slots) / SOURCE_CACHE_GROUP_SIZE;
    oldest = source_vertex_cache_group_oldest(cache, group, excluded);
    for (size_t node = SOURCE_CACHE_GROUPS + group; node > 1; node /= 2) {
        source_cached_vertices *sibling = cache->eviction_tree[node ^ 1u];
        oldest = (node & 1u) != 0 ? source_vertex_cache_min(sibling, oldest)
                                  : source_vertex_cache_min(oldest, sibling);
    }
    return oldest;
}

static bool source_vertex_cache_make_room(mc_client_display *display,
                                          const source_cached_vertices *entry, size_t bytes,
                                          mc_client_display_error *error) {
    source_vertex_cache *cache = display->source_cache;
    while (bytes > SOURCE_CACHE_BYTES - (cache->bytes - entry->bytes)) {
        /* The incoming batch fits by itself, so other slots hold the excess. */
        source_cached_vertices *oldest = source_vertex_cache_oldest(cache, entry);
        oldest->data = NULL;
        display->gl.DeleteBuffers(1, &oldest->vbo);
        display->gl.DeleteVertexArrays(1, &oldest->vao);
        if (!gl_check(display, "evict source vertex cache storage", error))
            return false;
        source_vertex_cache_set_bytes(cache, oldest, 0);
        *oldest = (source_cached_vertices){0};
    }
    return true;
}

static bool source_vertex_cache_upload(mc_client_display *display, source_cached_vertices *entry,
                                       mc_client_source_draw_command *draw, size_t bytes,
                                       mc_client_source_attribute_profile profile,
                                       const float *color, const float *normal,
                                       const mc_client_source_gpu_vertex *vertices,
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
        source_vertex_cache_set_bytes(display->source_cache, entry, bytes);
    }
    entry->data = draw->bytes;
    entry->generation = display->native_context_generation;
    entry->profile = profile;
    entry->last_vertex = vertices[draw->count - 1];
    if (color != NULL)
        memcpy(entry->color, color, sizeof(entry->color));
    if (normal != NULL)
        memcpy(entry->normal, normal, sizeof(entry->normal));
    draw->cache_slot_hint = (uint32_t)(entry - display->source_cache->slots) + 1u;
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
