#include "path_internal.h"

#include "../../block/registry/registry_internal.h"
#include "../../block/registry/runtime_internal.h"
#include "../../entity/entity_internal.h"
#include "construction_internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/core/java/java.h"
#include "runtime_internal.h"

#include <emmintrin.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_M_X64) && !defined(__x86_64__)
#error "World paths require the qualified x86-64 strict FP profile"
#endif
#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) ||       \
    (defined(_M_FP_FAST) && _M_FP_FAST)
#error "World paths require strict floating transformations"
#endif

mc_world_path_limits mc_world_path_default_limits(void) {
    return (mc_world_path_limits){1048576u, 64u * 1048576u, 1048576u, 65536u,
                                  65536u,   4u * 1048576u,  65536u,   4u * 1048576u};
}

static mc_path_site fd_site(const mc_world_path_call *c, int32_t pc) {
    return (mc_path_site){"fd", "a", c->state.to_entity ? "(Lsn;Lsn;F)Ldh;" : "(Lsn;IIIF)Ldh;", pc};
}

static void at(mc_world_path_call *c, unsigned int pc) {
    c->state.source_pc = pc;
}

static mc_world_path_status fail(mc_world_path_call *c, mc_world_path_status status,
                                 const char *operation, mc_path_site site, bool native) {
    c->failure.status = status;
    c->failure.operation = operation;
    c->failure.site = site;
    c->failure.native_policy = native;
    c->failure.caller = fd_site(c, (int32_t)c->state.source_pc);
    return status;
}

static mc_world_path_status source_exception(mc_world_path_call *c, const char *name,
                                             const char *operation, mc_path_site site) {
    c->failure.source_exception = true;
    c->failure.exception_class = name;
    return fail(c, MC_WORLD_PATH_SOURCE_EXCEPTION, operation, site, false);
}

static mc_world_path_status path_status(mc_path_status status) {
    switch (status) {
        case MC_PATH_OK:
            return MC_WORLD_PATH_OK;
        case MC_PATH_ARGUMENT:
            return MC_WORLD_PATH_ARGUMENT;
        case MC_PATH_STATE:
            return MC_WORLD_PATH_STATE;
        case MC_PATH_BUSY:
            return MC_WORLD_PATH_BUSY;
        case MC_PATH_LIMIT:
            return MC_WORLD_PATH_LIMIT;
        case MC_PATH_ALLOCATION:
            return MC_WORLD_PATH_ALLOCATION;
        case MC_PATH_SOURCE_EXCEPTION:
            return MC_WORLD_PATH_SOURCE_EXCEPTION;
        case MC_PATH_UNIMPLEMENTED:
            return MC_WORLD_PATH_UNIMPLEMENTED;
        default:
            return MC_WORLD_PATH_CHILD_FAILED;
    }
}

static mc_world_path_status entity_status(mc_entity_status status) {
    return status == MC_ENTITY_LIMIT               ? MC_WORLD_PATH_LIMIT
           : status == MC_ENTITY_ALLOCATION_FAILED ? MC_WORLD_PATH_ALLOCATION
           : status == MC_ENTITY_BUSY              ? MC_WORLD_PATH_BUSY
                                                   : MC_WORLD_PATH_STATE;
}

static bool fp(mc_world_path_call *c, unsigned int pc) {
    if ((_mm_getcsr() & 0xffc0u) == 0x1f80u)
        return true;
    (void)fail(c, MC_WORLD_PATH_STATE, "World path strict floating prerequisite",
               fd_site(c, (int32_t)pc), true);
    return false;
}

static bool floor_coordinate(mc_world_path_call *c, double value, unsigned int pc, int32_t *out) {
    at(c, pc);
    if (c->math == NULL) {
        (void)fail(c, MC_WORLD_PATH_STATE, "World path defining Math namespace",
                   fd_site(c, (int32_t)pc), true);
        return false;
    }
    const mc_math_class_status status = mc_math_classes_ensure(c->math, &c->math_error);
    if (status != MC_MATH_CLASS_OK) {
        c->failure.math = &c->math_error;
        (void)fail(c,
                   status == MC_MATH_CLASS_LIMIT               ? MC_WORLD_PATH_LIMIT
                   : status == MC_MATH_CLASS_ALLOCATION_FAILED ? MC_WORLD_PATH_ALLOCATION
                   : status == MC_MATH_CLASS_BUSY              ? MC_WORLD_PATH_BUSY
                                                               : MC_WORLD_PATH_STATE,
                   "World path source Math active use", fd_site(c, (int32_t)pc), true);
        return false;
    }
    if (!fp(c, pc))
        return false;
    *out = mc_math_floor_double(value);
    return true;
}

static bool same_registry(const mc_registry_error *a, const mc_registry_error *b, bool cached) {
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

static const char *registry_exception(mc_registry_status status) {
    if (status == MC_REGISTRY_NULL_REFERENCE)
        return "java/lang/NullPointerException";
    if (status == MC_REGISTRY_INDEX_OUT_OF_RANGE)
        return "java/lang/ArrayIndexOutOfBoundsException";
    if (status == MC_REGISTRY_ILLEGAL_ARGUMENT || status == MC_REGISTRY_DUPLICATE_BLOCK ||
        status == MC_REGISTRY_DUPLICATE_STATISTIC)
        return "java/lang/IllegalArgumentException";
    return NULL;
}

static bool nested_registry_exception(const mc_registry_classes *classes,
                                      const mc_registry_error *first) {
    if (first->status != MC_REGISTRY_FAILED_INITIALIZATION)
        return false;
    for (size_t i = 0; i < (size_t)MC_BLOCK_KIND_COUNT; ++i) {
        const mc_registry_class_state *state = &classes->block_states[i];
        if (state->phase == MC_REGISTRY_FAILED &&
            registry_exception(state->failure.status) != NULL &&
            same_registry(first, &state->failure, true))
            return true;
    }
    for (size_t i = 0; i < (size_t)MC_ITEM_KIND_COUNT; ++i) {
        const mc_registry_class_state *state = &classes->item_states[i];
        if (state->phase == MC_REGISTRY_FAILED &&
            registry_exception(state->failure.status) != NULL &&
            same_registry(first, &state->failure, true))
            return true;
    }
    return false;
}

static void classify_registry(mc_world_path_call *c, const mc_registry_error *error) {
    if (error->status == MC_REGISTRY_OK)
        return;
    const bool cached = error->status == MC_REGISTRY_FAILED_INITIALIZATION;
    const mc_registry_error *first = NULL;
    for (size_t i = 0; i < (size_t)MC_BLOCK_KIND_COUNT; ++i) {
        const mc_registry_class_state *state = &c->registry->block_states[i];
        if (state->phase == MC_REGISTRY_FAILED && same_registry(error, &state->failure, cached)) {
            first = &state->failure;
            break;
        }
    }
    if (first == NULL)
        for (size_t i = 0; i < (size_t)MC_ITEM_KIND_COUNT; ++i) {
            const mc_registry_class_state *state = &c->registry->item_states[i];
            if (state->phase == MC_REGISTRY_FAILED &&
                same_registry(error, &state->failure, cached)) {
                first = &state->failure;
                break;
            }
        }
    const char *exception =
        first != NULL && registry_exception(first->status) != NULL
            ? cached ? "java/lang/NoClassDefFoundError" : "java/lang/ExceptionInInitializerError"
        : !cached && error->active_initializer_class == NULL ? registry_exception(error->status)
                                                             : NULL;
    if (exception == NULL && first != NULL && nested_registry_exception(c->registry, first))
        exception = "java/lang/NoClassDefFoundError";
    if (exception != NULL) {
        c->failure.source_exception = true;
        c->failure.native_policy = false;
        c->failure.exception_class = exception;
        c->failure.status = MC_WORLD_PATH_SOURCE_EXCEPTION;
    }
}

static bool capture_receipt(mc_world_path_call *c, const mc_world_access_failure *failure) {
    if (failure->receipt == NULL)
        return true;
    for (size_t i = 0; i < c->receipt_count; ++i)
        if (c->receipts[i].receipt == failure->receipt)
            return true;
    if (c->receipt_count == sizeof(c->receipts) / sizeof(*c->receipts) ||
        mc_world_access_failure_retain(failure, &c->receipts[c->receipt_count]) !=
            MC_WORLD_STORAGE_OK)
        return false;
    ++c->receipt_count;
    const char *exception = NULL;
    if (mc_world_access_failure_source_exception(failure, &exception)) {
        c->failure.source_exception = true;
        c->failure.native_policy = false;
        c->failure.exception_class = exception;
        c->failure.status = MC_WORLD_PATH_SOURCE_EXCEPTION;
    }
    return true;
}

static bool capture_cache(mc_world_path_call *c, mc_client_chunk_cache_error *error) {
    if (error->acquisition.source_chunk != NULL) {
        c->source_chunk = *error->acquisition.source_chunk;
        error->acquisition.source_chunk = &c->source_chunk;
        c->failure.source_chunk = &c->source_chunk;
        if (c->source_chunk.source.source_exception) {
            c->failure.source_exception = true;
            c->failure.native_policy = false;
            c->failure.exception_class = c->source_chunk.source.exception_class;
            c->failure.status = MC_WORLD_PATH_SOURCE_EXCEPTION;
        }
    }
    const mc_live_tile_entity_error *t = &error->acquisition.tile_entity;
    if (!capture_receipt(c, &error->acquisition.batch.publication.world_access_failure) ||
        !capture_receipt(c, &t->world_access_failure) ||
        !capture_receipt(c, &t->publication.world_access_failure) ||
        !capture_receipt(c, &t->batch.publication.world_access_failure) ||
        !capture_receipt(c, &error->tile_entity.world_access_failure) ||
        !capture_receipt(c, &error->tile_entity.publication.world_access_failure) ||
        !capture_receipt(c, &error->tile_entity.batch.publication.world_access_failure)) {
        c->failure.source_exception = false;
        c->failure.exception_class = NULL;
        (void)fail(c, MC_WORLD_PATH_BUSY, "World path invocation receipt hold",
                   fd_site(c, (int32_t)c->state.source_pc), true);
        return false;
    }
    classify_registry(c, &error->registry);
    classify_registry(c, &error->acquisition.registry);
    classify_registry(c, &t->registry);
    return true;
}

static void cache_failed(mc_world_path_call *c, mc_client_chunk_cache_status status) {
    const char *exception =
        status == MC_CLIENT_CHUNK_CACHE_SOURCE_NULL    ? "java/lang/NullPointerException"
        : status == MC_CLIENT_CHUNK_CACHE_SOURCE_INDEX ? "java/lang/ArrayIndexOutOfBoundsException"
        : status == MC_CLIENT_CHUNK_CACHE_SOURCE_NEGATIVE_ARRAY
            ? "java/lang/NegativeArraySizeException"
            : NULL;
    const mc_path_site site = {"ew", c->cache_error.method, c->cache_error.descriptor,
                               (int32_t)c->cache_error.bytecode};
    if (exception != NULL)
        (void)source_exception(c, exception, c->cache_error.operation, site);
    else
        (void)fail(c,
                   status == MC_CLIENT_CHUNK_CACHE_LIMIT               ? MC_WORLD_PATH_LIMIT
                   : status == MC_CLIENT_CHUNK_CACHE_ALLOCATION_FAILED ? MC_WORLD_PATH_ALLOCATION
                   : status == MC_CLIENT_CHUNK_CACHE_BUSY              ? MC_WORLD_PATH_BUSY
                                                                       : MC_WORLD_PATH_CHILD_FAILED,
                   c->cache_error.operation, site, c->cache_error.native_policy);
    c->failure.cache = &c->cache_error;
    (void)capture_cache(c, &c->cache_error);
}

static void finder_failed(mc_world_path_call *c, mc_path_status status) {
    mc_path_finder_state state = {0};
    if (c->finder != NULL && mc_path_finder_inspect(c->finder, &state) && state.error != NULL) {
        c->finder_error = *state.error;
        c->path_error = c->finder_error.path;
        c->failure.finder = &c->finder_error;
    }
    (void)fail(c, path_status(status), c->path_error.operation, c->path_error.site,
               c->path_error.native_policy);
    c->failure.source_exception = c->path_error.source_exception;
    c->failure.exception_class = c->path_error.exception_class;
    c->failure.child = c->path_error.child_site;
    if (c->failure.finder != NULL) {
        (void)capture_cache(c, &c->finder_error.cache);
        classify_registry(c, &c->finder_error.registry);
    }
}

static bool execute(mc_world_path_call *c) {
    mc_entity *source = c->state.source;
    at(c, 1);
    if (source == NULL) {
        (void)source_exception(c, "java/lang/NullPointerException",
                               "World path null source coordinate receiver", fd_site(c, 1));
        return false;
    }
    const double x = source->base.x;
    if (!floor_coordinate(c, x, 4, &c->state.center[0]))
        return false;
    const double y = source->base.y;
    if (!floor_coordinate(c, y, 13, &c->state.center[1]))
        return false;
    const double z = source->base.z;
    if (!floor_coordinate(c, z, 22, &c->state.center[2]))
        return false;
    c->state.center_stored = true;
    at(c, c->state.to_entity ? 30u : 31u);
    if (!fp(c, c->state.source_pc))
        return false;
    const float extent = _mm_cvtss_f32(
        _mm_add_ss(_mm_set_ss(c->state.range), _mm_set_ss(c->state.to_entity ? 16.0f : 8.0f)));
    c->state.extent = mc_java_float_to_i32(extent);
    for (size_t i = 0; i < 3; ++i)
        c->state.endpoints[i] = mc_java_i32_sub(c->state.center[i], c->state.extent);
    for (size_t i = 0; i < 3; ++i)
        c->state.endpoints[i + 3] = mc_java_i32_add(c->state.center[i], c->state.extent);
    c->state.endpoints_stored = true;
    at(c, c->state.to_entity ? 93u : 94u);
    const mc_client_chunk_cache_limits cache_limits = {
        c->limits.max_cache_cells, c->budget.max_bytes - c->budget.bytes,
        c->budget.max_allocations - c->budget.allocations};
    const mc_client_chunk_cache_status cache_status = mc_client_chunk_cache_create_with_limits(
        c->state.world, c->state.endpoints[0], c->state.endpoints[1], c->state.endpoints[2],
        c->state.endpoints[3], c->state.endpoints[4], c->state.endpoints[5], &cache_limits,
        &c->cache, &c->cache_error);
    c->state.cache = c->cache;
    mc_client_chunk_cache_state cache_state = {0};
    if (mc_client_chunk_cache_inspect(c->cache, &cache_state)) {
        c->cache_bytes = cache_state.native_bytes;
        c->budget.bytes += cache_state.native_bytes;
        c->budget.allocations += cache_state.native_allocations;
        if (c->budget.bytes > c->budget.peak_bytes)
            c->budget.peak_bytes = c->budget.bytes;
    }
    if (cache_status != MC_CLIENT_CHUNK_CACHE_OK) {
        cache_failed(c, cache_status);
        return false;
    }
    c->state.cache_returned = true;
    at(c, c->state.to_entity ? 104u : 105u);
    const mc_path_finder_limits finder_limits = {
        c->limits.max_points, c->limits.max_expanded_points, c->limits.max_cell_queries,
        c->limits.max_predecessor_steps, c->limits.max_work};
    mc_path_status status =
        mc_path_finder_create(c->entities, c->registry, c->math, c->cache, &finder_limits,
                              &c->budget, &c->finder, &c->path_error);
    c->state.finder = c->finder;
    if (status != MC_PATH_OK) {
        finder_failed(c, status);
        return false;
    }
    c->state.finder_constructed = true;
    at(c, c->state.to_entity ? 110u : 115u);
    status = c->state.to_entity
                 ? mc_path_finder_to_entity(c->finder, source, c->state.target, c->state.range)
                 : mc_path_finder_to_coordinates(c->finder, source, c->state.coordinates[0],
                                                 c->state.coordinates[1], c->state.coordinates[2],
                                                 c->state.range);
    if (status != MC_PATH_OK) {
        finder_failed(c, status);
        return false;
    }
    mc_path_finder_state state = {0};
    if (!mc_path_finder_inspect(c->finder, &state) || !state.returned) {
        (void)fail(c, MC_WORLD_PATH_STATE, "World path finder did not return source body",
                   fd_site(c, (int32_t)c->state.source_pc), true);
        return false;
    }
    c->state.result = state.result;
    at(c, c->state.to_entity ? 113u : 118u);
    c->state.source_returned = true;
    return true;
}

static bool hold_entity(mc_world_path_call *c, mc_entity *p, bool *held) {
    if (p == NULL)
        return true;
    if (p->classes != c->entities || p->class_literal == NULL ||
        !mc_entity_is_kind(p, p->class_literal->kind)) {
        (void)fail(c, MC_WORLD_PATH_STATE, "World path actual Entity class namespace",
                   fd_site(c, 0), true);
        return false;
    }
    const mc_entity_status status = mc_entity_retain(p);
    if (status != MC_ENTITY_OK) {
        c->entity_error =
            (mc_entity_error){.status = status, .operation = "World path Entity operand hold"};
        c->failure.entity = &c->entity_error;
        (void)fail(c, entity_status(status), c->entity_error.operation, fd_site(c, 0), true);
        return false;
    }
    *held = true;
    return true;
}

static mc_world_path_status begin(mc_world_runtime *world, mc_entity *source, mc_entity *target,
                                  bool to_entity, int32_t x, int32_t y, int32_t z, float range,
                                  mc_math_classes *math, const mc_world_path_limits *limits,
                                  mc_world_path_call **out) {
    if (world == NULL || out == NULL || *out != NULL)
        return MC_WORLD_PATH_ARGUMENT;
    if (world->construction.mode != MC_WORLD_RUNTIME_GAME || world->classes == NULL ||
        world->classes->side != MC_WORLD_INFO_CLIENT || world->classes->entities == NULL ||
        world->classes->registry == NULL || world->classes->entities->source != MC_ENTITY_CLIENT)
        return MC_WORLD_PATH_STATE;
    const mc_world_path_limits admitted = limits == NULL ? mc_world_path_default_limits() : *limits;
    if (admitted.max_native_bytes < sizeof(mc_world_path_call) || admitted.max_allocations == 0)
        return MC_WORLD_PATH_LIMIT;
    mc_world_path_call *c = calloc(1, sizeof(*c));
    if (c == NULL)
        return MC_WORLD_PATH_ALLOCATION;
    c->limits = admitted;
    c->budget = (mc_path_budget){admitted.max_native_bytes, admitted.max_allocations, sizeof(*c),
                                 sizeof(*c), 1};
    c->state.phase = MC_WORLD_PATH_ACTIVE;
    c->state.active = true;
    c->state.to_entity = to_entity;
    c->state.world = world;
    c->state.source = source;
    c->state.target = target;
    c->state.coordinates[0] = x;
    c->state.coordinates[1] = y;
    c->state.coordinates[2] = z;
    c->state.range = range;
    c->state.failure = &c->failure;
    c->state.cleanup = &c->cleanup;
    c->entities = world->classes->entities;
    c->registry = world->classes->registry;
    c->math = math;
    c->failure.site.pc = c->failure.caller.pc = c->failure.child.pc = -1;
    *out = c;
    if (!mc_world_runtime_binding_retain(world)) {
        c->world_error = *mc_world_runtime_last_error(world);
        c->failure.world = &c->world_error;
        (void)fail(c,
                   c->world_error.status == MC_WORLD_STORAGE_LIMIT ? MC_WORLD_PATH_LIMIT
                                                                   : MC_WORLD_PATH_STATE,
                   c->world_error.operation, fd_site(c, 0), true);
        goto done;
    }
    c->world_held = true;
    if (!hold_entity(c, source, &c->source_held) || !hold_entity(c, target, &c->target_held))
        goto done;
    const mc_registry_status registry_status = mc_registry_classes_retain(
        c->registry, MC_REGISTRY_CLIENT, c->entities->profile, &c->registry_error);
    if (registry_status != MC_REGISTRY_OK) {
        c->failure.registry = &c->registry_error;
        (void)fail(c,
                   registry_status == MC_REGISTRY_LIMIT  ? MC_WORLD_PATH_LIMIT
                   : registry_status == MC_REGISTRY_BUSY ? MC_WORLD_PATH_BUSY
                                                         : MC_WORLD_PATH_STATE,
                   "World path defining Registry hold", fd_site(c, 0), true);
        goto done;
    }
    c->registry_held = true;
    if (c->registry->entities != c->entities) {
        (void)fail(c, MC_WORLD_PATH_STATE, "World path defining Registry Entity namespace",
                   fd_site(c, 0), true);
        goto done;
    }
    if (math != NULL) {
        const mc_math_class_status status = mc_math_classes_retain(math, &c->math_error);
        if (status != MC_MATH_CLASS_OK) {
            c->failure.math = &c->math_error;
            (void)fail(c,
                       status == MC_MATH_CLASS_LIMIT  ? MC_WORLD_PATH_LIMIT
                       : status == MC_MATH_CLASS_BUSY ? MC_WORLD_PATH_BUSY
                                                      : MC_WORLD_PATH_STATE,
                       "World path defining Math hold", fd_site(c, 0), true);
            goto done;
        }
        c->math_held = true;
    }
    if (mc_world_runtime_call_enter(world, &c->world_error) != MC_WORLD_STORAGE_OK) {
        c->failure.world = &c->world_error;
        (void)fail(c,
                   c->world_error.status == MC_WORLD_STORAGE_LIMIT ? MC_WORLD_PATH_LIMIT
                                                                   : MC_WORLD_PATH_STATE,
                   c->world_error.operation, fd_site(c, 0), true);
        goto done;
    }
    (void)execute(c);
    mc_world_runtime_call_leave(world);
done:
    c->state.active = false;
    c->state.phase = c->state.source_returned ? MC_WORLD_PATH_RETURNED : MC_WORLD_PATH_FAILED;
    return c->failure.status;
}

mc_world_path_status mc_world_runtime_path_to_entity_begin(mc_world_runtime *world,
                                                           mc_entity *source, mc_entity *target,
                                                           float range, mc_math_classes *math,
                                                           const mc_world_path_limits *limits,
                                                           mc_world_path_call **out) {
    return begin(world, source, target, true, 0, 0, 0, range, math, limits, out);
}

mc_world_path_status mc_world_runtime_path_to_coordinates_begin(
    mc_world_runtime *world, mc_entity *source, int32_t x, int32_t y, int32_t z, float range,
    mc_math_classes *math, const mc_world_path_limits *limits, mc_world_path_call **out) {
    return begin(world, source, NULL, false, x, y, z, range, math, limits, out);
}

bool mc_world_path_call_inspect(const mc_world_path_call *c, mc_world_path_call_state *out) {
    if (c == NULL || out == NULL)
        return false;
    *out = c->state;
    out->native_bytes = c->budget.bytes;
    out->native_peak_bytes = c->budget.peak_bytes;
    out->native_allocations = c->budget.allocations;
    return true;
}

static mc_world_path_status cleanup_failed(mc_world_path_call *c, const char *operation) {
    c->cleanup.status = MC_WORLD_PATH_CLEANUP_FAILED;
    c->cleanup.operation = operation;
    c->cleanup.native_policy = true;
    return c->cleanup.status;
}

mc_world_path_status mc_world_path_call_retire(mc_world_path_call **slot) {
    if (slot == NULL)
        return MC_WORLD_PATH_ARGUMENT;
    mc_world_path_call *c = *slot;
    if (c == NULL)
        return MC_WORLD_PATH_OK;
    if (c->state.active)
        return MC_WORLD_PATH_BUSY;
    c->state.retiring = true;
    c->state.phase = MC_WORLD_PATH_RETIRING;
    memset(&c->cleanup, 0, sizeof(c->cleanup));
    c->cleanup.site.pc = c->cleanup.caller.pc = c->cleanup.child.pc = -1;
    if (c->finder != NULL) {
        mc_path_finder_destroy(c->finder);
        c->finder = NULL;
        c->state.finder = NULL;
        c->state.result = NULL;
    }
    if (c->cache != NULL) {
        if (mc_client_chunk_cache_retire(&c->cache, &c->cache_cleanup) !=
            MC_CLIENT_CHUNK_CACHE_OK) {
            c->cleanup.cache = &c->cache_cleanup;
            return cleanup_failed(c, "World path owned EW retirement");
        }
        c->budget.bytes -= c->cache_bytes;
        c->cache_bytes = 0;
        c->state.cache = NULL;
    }
    if (c->math_held) {
        mc_math_classes *occurrence = c->math;
        if (mc_math_classes_release(&occurrence, &c->math_cleanup) != MC_MATH_CLASS_OK) {
            c->cleanup.math = &c->math_cleanup;
            return cleanup_failed(c, "World path Math occurrence retirement");
        }
        c->math_held = false;
    }
    if (c->registry_held) {
        if (!mc_registry_runtime_borrow_release(c->registry)) {
            c->registry_cleanup =
                (mc_registry_error){.status = MC_REGISTRY_INVALID_ARGUMENT,
                                    .message = "World path Registry occurrence count",
                                    .initializer_pc = -1,
                                    .block_id = -1,
                                    .constructor_pc = -1,
                                    .active_initializer_pc = -1,
                                    .item_argument = -1};
            c->cleanup.registry = &c->registry_cleanup;
            return cleanup_failed(c, "World path Registry occurrence retirement");
        }
        c->registry_held = false;
    }
    if (c->target_held) {
        mc_entity_release(c->state.target);
        c->target_held = false;
        c->state.target = NULL;
    }
    if (c->source_held) {
        mc_entity_release(c->state.source);
        c->source_held = false;
        c->state.source = NULL;
    }
    for (size_t i = 0; i < c->receipt_count; ++i)
        mc_world_access_failure_release(&c->receipts[i]);
    c->receipt_count = 0;
    if (c->world_held)
        mc_world_runtime_binding_release(c->state.world);
    free(c);
    *slot = NULL;
    return MC_WORLD_PATH_OK;
}
