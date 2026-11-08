#include "../runtime/acquisition_internal.h"
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

static mc_path_status world_status(mc_world_storage_status status) {
    return status == MC_WORLD_STORAGE_LIMIT               ? MC_PATH_LIMIT
           : status == MC_WORLD_STORAGE_BUSY              ? MC_PATH_BUSY
           : status == MC_WORLD_STORAGE_ALLOCATION_FAILED ? MC_PATH_ALLOCATION
                                                          : MC_PATH_CHILD_FAILED;
}

static bool capture_failed(mc_path_finder *w, mc_path_status status, const char *operation) {
    mc_path_world_access *access = w->world_access;
    access->cleanup.status = status;
    access->cleanup.operation = operation;
    access->cleanup.native_policy = true;
    access->cleanup.site = w->state.site;
    return false;
}

bool pf_capture_world_query(mc_path_finder *w) {
    mc_path_world_access *access = w->world_access;
    if (access == NULL)
        return true;
    mc_path_error_init(&access->cleanup);
    if (!access->query_failed)
        return true;
    if (access->acquisition.source_chunk != NULL && !access->source_copied) {
        access->source = *access->acquisition.source_chunk;
        access->source_copied = true;
        access->acquisition.source_chunk = &access->source;
        w->error.source_chunk = &access->source;
    }
    const mc_world_access_failure *receipts[] = {
        &access->acquisition.batch.publication.world_access_failure,
        &access->acquisition.tile_entity.world_access_failure,
        &access->acquisition.tile_entity.publication.world_access_failure,
        &access->acquisition.tile_entity.batch.publication.world_access_failure};
    for (size_t i = 0; i < sizeof(receipts) / sizeof(*receipts); ++i) {
        const mc_world_access_failure *receipt = receipts[i];
        if (receipt->receipt == NULL)
            continue;
        bool captured = false;
        for (size_t j = 0; j < access->receipt_count; ++j)
            if (access->receipts[j].receipt == receipt->receipt)
                captured = true;
        if (captured)
            continue;
        if (access->receipt_count == sizeof(access->receipts) / sizeof(*access->receipts))
            return capture_failed(w, MC_PATH_LIMIT, "Path direct World query receipt extent");
        const mc_world_storage_status status =
            mc_world_access_failure_retain(receipt, &access->receipts[access->receipt_count]);
        if (status != MC_WORLD_STORAGE_OK)
            return capture_failed(w, world_status(status), "Path direct World query receipt hold");
        ++access->receipt_count;
    }
    return true;
}

static bool world_query(mc_path_finder *w, mc_path_query *q, bool metadata, int32_t x, int32_t y,
                        int32_t z, int32_t *out) {
    mc_path_world_access *access = w->world_access;
    q->direct_world = true;
    if (access->world == NULL)
        return pf_source(w, "java/lang/NullPointerException", "Path null XP receiver");
    const mc_world_storage_status status =
        metadata
            ? mc_world_runtime_metadata_get_source_failure(access->world, x, y, z, out,
                                                           &access->error, &access->acquisition,
                                                           &access->metadata)
            : mc_world_runtime_block_get_failure(access->world, x, y, z, out, &access->error,
                                                 &access->acquisition);
    q->world_status = status;
    if (status == MC_WORLD_STORAGE_OK)
        return true;
    access->query_failed = true;
    w->error.world = &access->error;
    w->error.acquisition = &access->acquisition;
    w->error.source_chunk = access->acquisition.source_chunk;
    w->error.metadata = metadata ? &access->metadata : NULL;
    const char *exception =
        metadata && access->metadata.source_exception ? access->metadata.exception_class : NULL;
    const bool null_provider =
        access->error.operation != NULL &&
        strcmp(access->error.operation, "World block provider receiver") == 0;
    if (null_provider)
        exception = "java/lang/NullPointerException";
    if (access->acquisition.source_chunk != NULL &&
        access->acquisition.source_chunk->source.source_exception)
        exception = access->acquisition.source_chunk->source.exception_class;
    const mc_world_access_failure *receipts[] = {
        &access->acquisition.batch.publication.world_access_failure,
        &access->acquisition.tile_entity.world_access_failure,
        &access->acquisition.tile_entity.publication.world_access_failure,
        &access->acquisition.tile_entity.batch.publication.world_access_failure};
    for (size_t i = 0; exception == NULL && i < sizeof(receipts) / sizeof(*receipts); ++i)
        (void)mc_world_access_failure_source_exception(receipts[i], &exception);
    if (exception != NULL) {
        (void)pf_source(w, exception, access->error.operation);
        if (metadata && access->metadata.source_exception)
            w->error.path.child_site =
                access->metadata.nibble_pc == 0
                    ? (mc_path_site){"lm", "b", "(III)I", 7}
                    : (mc_path_site){"wi", "a", "(III)I", (int32_t)access->metadata.nibble_pc};
        else if (null_provider)
            w->error.path.child_site = (mc_path_site){"fd", "c", "(II)Llm;", 6};
    } else if (access->acquisition.registry.status != MC_REGISTRY_OK) {
        w->error.registry = access->acquisition.registry;
        (void)pf_registry_fail(w, access->error.operation);
    } else
        (void)pf_fail(w, world_status(status), access->error.operation);
    (void)pf_capture_world_query(w);
    return false;
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
    if (w->a == NULL && w->world_access == NULL)
        return pf_source(w, "java/lang/NullPointerException", "Path null XP receiver");
    int32_t value;
    if (w->world_access != NULL) {
        if (!world_query(w, q, metadata, x, y, z, &value))
            return false;
    } else {
        q->status = metadata
                        ? mc_client_chunk_cache_metadata(w->a, x, y, z, &value, &w->error.cache)
                        : mc_client_chunk_cache_block(w->a, x, y, z, &value, &w->error.cache);
        if (q->status != MC_CLIENT_CHUNK_CACHE_OK)
            return pf_cache_fail(w, "Path actual captured EW query");
    }
    q->completed = true;
    q->value = value;
    *out = value;
    return true;
}
