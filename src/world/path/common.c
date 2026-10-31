#include "path_internal.h"
#include <stddef.h>
#include <string.h>

void pf_at(mc_path_finder *w, const char *m, const char *descriptor, int32_t pc) {
    w->state.site = w->error.path.site = (mc_path_site){"fw", m, descriptor, pc};
    w->error.path.child_site = (mc_path_site){.pc = -1};
    mc_path_site *frame = strcmp(descriptor, "(Lsn;DDDF)Ldh;") == 0 ? &w->state.preparation_site
                          : strcmp(descriptor, "(Lsn;Ld;Ld;Ld;F)Ldh;") == 0 ? &w->state.search_site
                          : strcmp(descriptor, "(Lsn;Ld;Ld;Ld;F)I") == 0 ? &w->state.neighbors_site
                          : strcmp(descriptor, "(Lsn;IIILd;I)Ld;") == 0  ? &w->state.candidate_site
                          : strcmp(descriptor, "(Lsn;IIILd;)I") == 0     ? &w->state.clearance_site
                          : strcmp(descriptor, "(III)Ld;") == 0          ? &w->state.intern_site
                          : strcmp(descriptor, "(Ld;Ld;)Ldh;") == 0 ? &w->state.reconstruction_site
                                                                    : NULL;
    if (frame != NULL)
        *frame = w->state.site;
}

void pf_child(mc_path_finder *w, const char *c, const char *m, const char *descriptor, int32_t pc) {
    w->error.path.child_site = (mc_path_site){c, m, descriptor, pc};
    if (strcmp(c, "lf") == 0)
        w->state.heap_site = w->error.path.child_site;
    else if (strcmp(c, "jx") == 0 || strcmp(c, "wm") == 0)
        w->state.map_site = w->error.path.child_site;
    else if (strcmp(c, "d") == 0 || strcmp(c, "dh") == 0)
        w->state.point_site = w->error.path.child_site;
}

bool pf_fail(mc_path_finder *w, mc_path_status status, const char *operation) {
    if (!w->state.failed) {
        w->error.path.status = status;
        w->error.path.operation = operation;
        w->error.path.native_policy = !w->error.path.source_exception;
        w->state.failed = true;
    }
    return false;
}

bool pf_source(mc_path_finder *w, const char *exception, const char *operation) {
    if (!w->state.failed) {
        w->error.path.source_exception = true;
        w->error.path.exception_class = exception;
    }
    return pf_fail(w, MC_PATH_SOURCE_EXCEPTION, operation);
}

bool pf_fp(mc_path_finder *w) {
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u ||
           pf_fail(w, MC_PATH_STATE, "Path floating controls");
}

bool pf_math(mc_path_finder *w) {
    if (w->math == NULL)
        return pf_fail(w, MC_PATH_STATE, "Path actual Math namespace");
    const mc_math_class_status status = mc_math_classes_ensure(w->math, &w->error.path.math);
    return status == MC_MATH_CLASS_OK ||
           pf_fail(w, status == MC_MATH_CLASS_LIMIT ? MC_PATH_LIMIT : MC_PATH_CHILD_FAILED,
                   "Path Math active use");
}

bool pf_work(mc_path_finder *w) {
    if (w->state.work == w->limits.max_work)
        return pf_fail(w, MC_PATH_LIMIT, "Path pending iteration ceiling");
    ++w->state.work;
    return true;
}

void *pf_alloc(mc_path_finder *w, size_t bytes, bool arena) {
    mc_path_budget *b = w->budget;
    if (bytes > SIZE_MAX - sizeof(mc_path_mem)) {
        pf_fail(w, MC_PATH_LIMIT, "Path allocation size");
        return NULL;
    }
    const size_t total = sizeof(mc_path_mem) + bytes;
    if (b->bytes > b->max_bytes || total > b->max_bytes - b->bytes ||
        b->allocations >= b->max_allocations) {
        pf_fail(w, MC_PATH_LIMIT, "Path request allocation ceiling");
        return NULL;
    }
    const int32_t pc =
        w->error.path.child_site.pc >= 0 ? w->error.path.child_site.pc : w->state.site.pc;
    mc_path_mem *p = mc_entity_class_memory_allocate(w->classes, total, &w->error.path.entity,
                                                     "Path native storage", (uint16_t)pc);
    if (p == NULL) {
        pf_fail(w,
                w->error.path.entity.status == MC_ENTITY_LIMIT ? MC_PATH_LIMIT : MC_PATH_ALLOCATION,
                "Path namespace allocation");
        return NULL;
    }
    memset(p, 0, total);
    p->bytes = total;
    if (arena) {
        p->next = w->arena->allocations;
        w->arena->allocations = p;
        w->arena->bytes += total;
    } else {
        p->next = w->allocations;
        w->allocations = p;
    }
    b->bytes += total;
    if (b->bytes > b->peak_bytes)
        b->peak_bytes = b->bytes;
    ++b->allocations;
    return p->data;
}

void pf_free(mc_path_finder *w, void *data) {
    if (data == NULL)
        return;
    mc_path_mem *p = (mc_path_mem *)((unsigned char *)data - offsetof(mc_path_mem, data));
    mc_path_mem **at = &w->allocations;
    while (*at != p)
        at = &(*at)->next;
    *at = p->next;
    w->budget->bytes -= p->bytes;
    mc_entity_class_memory_retire(w->classes, p, p->bytes);
}

void pf_arena_drop(mc_path_arena *arena) {
    if (arena == NULL || --arena->holds != 0)
        return;
    mc_entity_classes *classes = arena->classes;
    mc_path_mem *p = arena->allocations;
    while (p != NULL) {
        mc_path_mem *next = p->next;
        mc_entity_class_memory_retire(classes, p, p->bytes);
        p = next;
    }
    mc_entity_class_memory_retire(classes, arena, sizeof(*arena));
    mc_entity_path_arena_close(classes);
}

static const char *registry_source(mc_registry_status status) {
    switch (status) {
        case MC_REGISTRY_NULL_REFERENCE:
            return "java/lang/NullPointerException";
        case MC_REGISTRY_INDEX_OUT_OF_RANGE:
            return "java/lang/ArrayIndexOutOfBoundsException";
        case MC_REGISTRY_ILLEGAL_ARGUMENT:
        case MC_REGISTRY_DUPLICATE_BLOCK:
        case MC_REGISTRY_DUPLICATE_STATISTIC:
            return "java/lang/IllegalArgumentException";
        default:
            return NULL;
    }
}

static bool same_cause(const mc_registry_error *a, const mc_registry_error *b, bool cached) {
    return (cached ? a->status == MC_REGISTRY_FAILED_INITIALIZATION : a->status == b->status) &&
           a->message == b->message && a->initializer_pc == b->initializer_pc &&
           a->block_id == b->block_id && a->constructor_class == b->constructor_class &&
           a->constructor_pc == b->constructor_pc && a->profile_status == b->profile_status &&
           a->active_initializer_class == b->active_initializer_class &&
           a->active_initializer_pc == b->active_initializer_pc &&
           a->item_argument == b->item_argument &&
           a->format_error.status == b->format_error.status &&
           a->format_error.offset == b->format_error.offset &&
           a->format_error.length == b->format_error.length &&
           a->format_error.value == b->format_error.value &&
           a->format_error.flags == b->format_error.flags &&
           a->format_error.conversion == b->format_error.conversion &&
           a->format_error.date_time == b->format_error.date_time;
}

bool pf_registry_fail(mc_path_finder *w, const char *operation) {
    const mc_registry_error *e = &w->error.registry;
    const mc_registry_classes *c = w->registry;
    const mc_registry_error *owner = NULL;
    const bool cached = e->status == MC_REGISTRY_FAILED_INITIALIZATION;
    if (c != NULL && c->blocks_phase == MC_REGISTRY_FAILED &&
        c->block_states[MC_BLOCK_BASE].phase == MC_REGISTRY_FAILED &&
        same_cause(&c->block_failure, &c->block_states[MC_BLOCK_BASE].failure, false) &&
        same_cause(e, &c->block_failure, cached))
        owner = &c->block_failure;
    if (c != NULL && owner == NULL && c->block_states[MC_BLOCK_DOOR].phase == MC_REGISTRY_FAILED &&
        same_cause(e, &c->block_states[MC_BLOCK_DOOR].failure, cached))
        owner = &c->block_states[MC_BLOCK_DOOR].failure;
    const mc_registry_error *item =
        c != NULL && c->item_states[MC_ITEM_BASE].phase == MC_REGISTRY_FAILED
            ? &c->item_states[MC_ITEM_BASE].failure
            : NULL;
    const char *exception =
        owner != NULL && registry_source(owner->status) != NULL
            ? cached ? "java/lang/NoClassDefFoundError" : "java/lang/ExceptionInInitializerError"
        : !cached && e->active_initializer_class == NULL ? registry_source(e->status)
                                                         : NULL;
    if (owner != NULL && owner->status == MC_REGISTRY_FAILED_INITIALIZATION && c != NULL) {
        if ((item != NULL && same_cause(owner, item, true) &&
             registry_source(item->status) != NULL) ||
            (owner != &c->block_failure && same_cause(owner, &c->block_failure, true) &&
             registry_source(c->block_failure.status) != NULL))
            exception = "java/lang/NoClassDefFoundError";
    }
    if (exception != NULL)
        return pf_source(w, exception, operation);
    return pf_fail(w, e->status == MC_REGISTRY_LIMIT ? MC_PATH_LIMIT : MC_PATH_CHILD_FAILED,
                   operation);
}

bool pf_cache_fail(mc_path_finder *w, const char *operation) {
    const mc_client_chunk_cache_status status = w->error.cache.status;
    if (!w->error.cache.native_policy) {
        if (status == MC_CLIENT_CHUNK_CACHE_SOURCE_NULL)
            return pf_source(w, "java/lang/NullPointerException", operation);
        if (status == MC_CLIENT_CHUNK_CACHE_SOURCE_INDEX)
            return pf_source(w, "java/lang/ArrayIndexOutOfBoundsException", operation);
        if (status == MC_CLIENT_CHUNK_CACHE_SOURCE_NEGATIVE_ARRAY)
            return pf_source(w, "java/lang/NegativeArraySizeException", operation);
    }
    if (w->error.cache.registry.status != MC_REGISTRY_OK) {
        w->error.registry = w->error.cache.registry;
        return pf_registry_fail(w, operation);
    }
    return pf_fail(w, status == MC_CLIENT_CHUNK_CACHE_LIMIT ? MC_PATH_LIMIT : MC_PATH_CHILD_FAILED,
                   operation);
}

bool pf_query(mc_path_finder *w, bool metadata, int32_t x, int32_t y, int32_t z, int32_t *out) {
    if (w->state.cell_queries == w->limits.max_cell_queries)
        return pf_fail(w, MC_PATH_LIMIT, "Path pending cell query ceiling");
    if (w->state.query_count == w->query_capacity) {
        const size_t capacity = w->query_capacity == 0 ? 64 : w->query_capacity * 2;
        if (capacity < w->query_capacity || capacity > SIZE_MAX / sizeof(mc_path_query))
            return pf_fail(w, MC_PATH_LIMIT, "Path query receipt extent");
        mc_path_query *next = pf_alloc(w, capacity * sizeof(*next), false);
        if (next == NULL)
            return false;
        if (w->queries != NULL)
            memcpy(next, w->queries, w->state.query_count * sizeof(*next));
        pf_free(w, w->queries);
        w->queries = next;
        w->query_capacity = capacity;
    }
    mc_path_query *q = &w->queries[w->state.query_count++];
    *q = (mc_path_query){.metadata = metadata, .x = x, .y = y, .z = z, .site = w->state.site};
    ++w->state.cell_queries;
    if (w->a == NULL)
        return pf_source(w, "java/lang/NullPointerException", "Path null XP receiver");
    int32_t value;
    q->status = metadata ? mc_client_chunk_cache_metadata(w->a, x, y, z, &value, &w->error.cache)
                         : mc_client_chunk_cache_block(w->a, x, y, z, &value, &w->error.cache);
    if (q->status != MC_CLIENT_CHUNK_CACHE_OK)
        return pf_cache_fail(w, "Path actual captured EW query");
    q->completed = true;
    q->value = value;
    *out = value;
    return true;
}
