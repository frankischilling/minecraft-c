#include "path_internal.h"
#include <stddef.h>
#include <string.h>

_Static_assert(SIZE_MAX / sizeof(mc_path_point *) >= INT32_MAX,
               "Path result arrays require the qualified 64-bit size_t");

static bool descriptor_budget(mc_path_finder *w, size_t bytes) {
    if (w->budget->bytes > w->budget->max_bytes ||
        bytes > w->budget->max_bytes - w->budget->bytes ||
        w->budget->allocations >= w->budget->max_allocations)
        return pf_fail(w, MC_PATH_LIMIT, "Path descriptor request ceiling");
    return true;
}

static void charged(mc_path_budget *b, size_t bytes) {
    b->bytes += bytes;
    if (b->bytes > b->peak_bytes)
        b->peak_bytes = b->bytes;
    ++b->allocations;
}

mc_path_status mc_path_finder_create(mc_entity_classes *classes, mc_registry_classes *registry,
                                     mc_math_classes *math, mc_client_chunk_cache *cache,
                                     const mc_path_finder_limits *limits, mc_path_budget *budget,
                                     mc_path_finder **out, mc_path_error *error) {
    mc_path_error local;
    if (error == NULL)
        error = &local;
    mc_path_error_init(error);
    if (out == NULL || *out != NULL || classes == NULL || registry == NULL || budget == NULL ||
        classes->source != MC_ENTITY_CLIENT || registry->options.side != MC_REGISTRY_CLIENT ||
        registry->options.random_profile != classes->profile || registry->entities != classes) {
        error->status = MC_PATH_ARGUMENT;
        error->native_policy = true;
        error->operation = "Path defining client namespaces";
        return error->status;
    }
    if (budget->bytes > budget->max_bytes ||
        sizeof(mc_path_finder) > budget->max_bytes - budget->bytes ||
        budget->allocations >= budget->max_allocations) {
        error->status = MC_PATH_LIMIT;
        error->native_policy = true;
        error->operation = "Path finder native request ceiling";
        return error->status;
    }
    mc_path_finder *w = mc_entity_class_memory_allocate(classes, sizeof(*w), &error->entity,
                                                        "Path finder native owner", 0);
    if (w == NULL) {
        error->status =
            error->entity.status == MC_ENTITY_LIMIT ? MC_PATH_LIMIT : MC_PATH_ALLOCATION;
        error->native_policy = true;
        error->operation = "Path finder native owner";
        return error->status;
    }
    memset(w, 0, sizeof(*w));
    charged(budget, sizeof(*w));
    w->classes = classes;
    w->registry = registry;
    w->math = math;
    w->budget = budget;
    w->limits =
        limits == NULL ? (mc_path_finder_limits){65536, 65536, 4194304, 65536, 16777216} : *limits;
    mc_path_error_init(&w->error.path);
    w->state.error = &w->error;
    *out = w;
    pf_at(w, "<init>", "(Lxp;)V", 1);
    if (mc_entity_path_arena_open(classes, &w->error.path.entity) != MC_ENTITY_OK) {
        pf_fail(w, w->error.path.entity.status == MC_ENTITY_LIMIT ? MC_PATH_LIMIT : MC_PATH_STATE,
                "Path arena namespace occurrence");
        return w->error.path.status;
    }
    w->namespace_open = true;
    if (!descriptor_budget(w, sizeof(mc_path_arena)))
        return w->error.path.status;
    w->arena = mc_entity_class_memory_allocate(classes, sizeof(*w->arena), &w->error.path.entity,
                                               "Path native arena", 0);
    if (w->arena == NULL) {
        pf_fail(w,
                w->error.path.entity.status == MC_ENTITY_LIMIT ? MC_PATH_LIMIT : MC_PATH_ALLOCATION,
                "Path native arena allocation");
        return w->error.path.status;
    }
    memset(w->arena, 0, sizeof(*w->arena));
    charged(budget, sizeof(*w->arena));
    w->arena->classes = classes;
    w->arena->holds = 1;
    w->arena->bytes = sizeof(*w->arena);
    w->namespace_open = false;
    pf_at(w, "<init>", "(Lxp;)V", 5);
    w->pending_heap = pf_alloc(w, sizeof(*w->pending_heap), false);
    if (w->pending_heap == NULL)
        return w->error.path.status;
    pf_child(w, "lf", "<init>", "()V", 8);
    w->pending_heap->a = pf_alloc(w, 1024 * sizeof(*w->pending_heap->a), false);
    if (w->pending_heap->a == NULL)
        return w->error.path.status;
    w->pending_heap->capacity = 1024;
    w->pending_heap->b = 0;
    w->b = w->pending_heap;
    w->pending_heap = NULL;
    w->state.constructor_pc = 12;
    pf_at(w, "<init>", "(Lxp;)V", 16);
    w->pending_map = pf_alloc(w, sizeof(*w->pending_map), false);
    if (w->pending_map == NULL)
        return w->error.path.status;
    pf_child(w, "jx", "<init>", "()V", 7);
    w->pending_map->d = .75f;
    w->pending_map->c = 12;
    pf_child(w, "jx", "<init>", "()V", 19);
    w->pending_map->a = pf_alloc(w, 16 * sizeof(*w->pending_map->a), false);
    if (w->pending_map->a == NULL)
        return w->error.path.status;
    w->pending_map->capacity = 16;
    w->c = w->pending_map;
    w->pending_map = NULL;
    w->state.constructor_pc = 23;
    pf_at(w, "<init>", "(Lxp;)V", 29);
    w->d = pf_alloc(w, 32 * sizeof(*w->d), false);
    if (w->d == NULL)
        return w->error.path.status;
    w->state.constructor_pc = 32;
    w->a = cache;
    w->state.constructor_pc = 37;
    w->state.constructed = true;
    pf_at(w, "<init>", "(Lxp;)V", 40);
    return MC_PATH_OK;
}

bool mc_path_finder_inspect(const mc_path_finder *w, mc_path_finder_state *out) {
    if (w == NULL || out == NULL)
        return false;
    *out = w->state;
    out->a = w->a;
    out->heap_identity = w->b;
    out->map_identity = w->c;
    out->pending_heap_identity = w->pending_heap;
    out->pending_map_identity = w->pending_map;
    out->heap = w->b == NULL ? NULL : w->b->a;
    out->heap_capacity = w->b == NULL ? 0 : w->b->capacity;
    out->heap_count = w->b == NULL ? 0 : w->b->b;
    out->map = w->c == NULL ? NULL : (const mc_path_map_entry *const *)w->c->a;
    out->map_capacity = w->c == NULL ? 0 : w->c->capacity;
    out->map_count = w->c == NULL ? 0 : w->c->b;
    out->map_threshold = w->c == NULL ? 0 : w->c->c;
    out->map_modifications = w->c == NULL ? 0 : w->c->e;
    out->neighbors = w->d;
    out->points = w->arena == NULL ? 0 : w->arena->points;
    out->queries = w->queries;
    return true;
}

bool mc_path_finder_point_inspect(const mc_path_finder *w, const mc_path_point *p,
                                  mc_path_point_state *out) {
    return w != NULL && pf_point_snapshot(w->arena, p, out);
}

void mc_path_finder_destroy(mc_path_finder *w) {
    if (w == NULL)
        return;
    mc_entity_classes *classes = w->classes;
    if (w->state.pending_path != NULL)
        mc_path_release(w->state.pending_path);
    if (w->state.result != NULL)
        mc_path_release(w->state.result);
    if (w->arena != NULL) {
        w->budget->bytes -= w->arena->bytes;
        pf_arena_drop(w->arena);
    }
    while (w->allocations != NULL)
        pf_free(w, w->allocations->data);
    if (w->namespace_open)
        mc_entity_path_arena_close(classes);
    w->budget->bytes -= sizeof(*w);
    mc_entity_class_memory_retire(classes, w, sizeof(*w));
}

bool pf_path_construct(mc_path_finder *w, mc_path_point **array, size_t length) {
    pf_at(w, "a", "(Ld;Ld;)Ldh;", 71);
    mc_path *p = pf_alloc(w, sizeof(*p), true);
    if (p == NULL)
        return false;
    p->arena = w->arena;
    p->holds = 1;
    if (w->arena->holds == SIZE_MAX)
        return pf_fail(w, MC_PATH_LIMIT, "Path arena occurrence ceiling");
    ++w->arena->holds;
    w->state.pending_path = p;
    pf_child(w, "dh", "<init>", "([Ld;)V", 6);
    p->b = array;
    p->array_length = length;
    p->constructor_pc = 6;
    pf_child(w, "dh", "<init>", "([Ld;)V", 11);
    if (array == NULL)
        return pf_source(w, "java/lang/NullPointerException", "DH constructor null array");
    if (length > (size_t)INT32_MAX)
        return pf_fail(w, MC_PATH_LIMIT, "Path source array extent");
    p->a = (int32_t)length;
    p->constructor_pc = 12;
    p->constructed = true;
    w->state.result = p;
    w->state.pending_path = NULL;
    pf_child(w, "dh", "<init>", "([Ld;)V", 15);
    return true;
}

bool pf_reconstruct(mc_path_finder *w, mc_path_point *end) {
    int32_t length = 1;
    mc_path_point *p = end;
    while (p->h != NULL) {
        pf_at(w, "a", "(Ld;Ld;)Ldh;", 7);
        if (w->state.predecessor_steps == w->limits.max_predecessor_steps || !pf_work(w))
            return pf_fail(w, MC_PATH_LIMIT, "Path predecessor count ceiling");
        ++w->state.predecessor_steps;
        length = mc_java_i32_add(length, 1);
        p = p->h;
    }
    pf_at(w, "a", "(Ld;Ld;)Ldh;", 27);
    if (length < 0)
        return pf_source(w, "java/lang/NegativeArraySizeException",
                         "Path reconstruction negative array");
    mc_path_point **array = pf_alloc(w, (size_t)(uint32_t)length * sizeof(*array), true);
    if (array == NULL)
        return false;
    w->state.pending_array = array;
    w->state.pending_array_length = (size_t)(uint32_t)length;
    p = end;
    int32_t index = mc_java_i32_sub(length, 1);
    array[index] = p;
    while (p->h != NULL) {
        pf_at(w, "a", "(Ld;Ld;)Ldh;", 46);
        if (w->state.predecessor_steps == w->limits.max_predecessor_steps || !pf_work(w))
            return pf_fail(w, MC_PATH_LIMIT, "Path predecessor fill ceiling");
        ++w->state.predecessor_steps;
        p = p->h;
        index = mc_java_i32_sub(index, 1);
        if (index < 0)
            return pf_source(w, "java/lang/ArrayIndexOutOfBoundsException",
                             "Path reconstruction fill index");
        array[index] = p;
    }
    if (!pf_path_construct(w, array, (size_t)(uint32_t)length))
        return false;
    w->state.pending_array = NULL;
    w->state.pending_array_length = 0;
    return true;
}

static bool search(mc_path_finder *w, mc_path_point *start, mc_path_point *goal,
                   mc_path_point *extent, float range) {
    pf_at(w, "a", "(Lsn;Ld;Ld;Ld;F)Ldh;", 2);
    start->e = 0.0f;
    float distance;
    pf_at(w, "a", "(Lsn;Ld;Ld;Ld;F)Ldh;", 8);
    if (!pf_distance(w, start, goal, &distance))
        return false;
    start->f = distance;
    start->g = start->f;
    pf_at(w, "a", "(Lsn;Ld;Ld;Ld;F)Ldh;", 26);
    pf_heap_clear(w);
    pf_at(w, "a", "(Lsn;Ld;Ld;Ld;F)Ldh;", 34);
    if (!pf_heap_add(w, start))
        return false;
    mc_path_point *closest = start;
    w->state.closest = closest;
    while (w->b->b != 0) {
        pf_at(w, "a", "(Lsn;Ld;Ld;Ld;F)Ldh;", 55);
        if (w->state.expanded_points == w->limits.max_expanded_points || !pf_work(w))
            return pf_fail(w, MC_PATH_LIMIT, "Path pending expansion ceiling");
        mc_path_point *current;
        if (!pf_heap_pop(w, &current))
            return false;
        w->state.current = current;
        ++w->state.expanded_points;
        if (pf_point_equals(current, goal)) {
            pf_at(w, "a", "(Lsn;Ld;Ld;Ld;F)Ldh;", 72);
            return pf_reconstruct(w, goal);
        }
        float current_distance, closest_distance;
        pf_at(w, "a", "(Lsn;Ld;Ld;Ld;F)Ldh;", 79);
        if (!pf_distance(w, current, goal, &current_distance))
            return false;
        pf_at(w, "a", "(Lsn;Ld;Ld;Ld;F)Ldh;", 85);
        if (!pf_distance(w, closest, goal, &closest_distance))
            return false;
        if (current_distance < closest_distance)
            w->state.closest = closest = current;
        current->i = true;
        size_t count;
        pf_at(w, "a", "(Lsn;Ld;Ld;Ld;F)Ldh;", 111);
        if (!pf_neighbors(w, current, extent, goal, range, &count))
            return false;
        for (size_t i = 0; i < count; ++i) {
            mc_path_point *next = w->d[i];
            const float prior_cost = current->e;
            pf_at(w, "a", "(Lsn;Ld;Ld;Ld;F)Ldh;", 144);
            if (!pf_distance(w, current, next, &distance))
                return false;
            const float cost = at_fadd(prior_cost, distance);
            if (next->d < 0 || cost < next->e) {
                next->h = current;
                next->e = cost;
                pf_at(w, "a", "(Lsn;Ld;Ld;Ld;F)Ldh;", 188);
                if (!pf_distance(w, next, goal, &distance))
                    return false;
                next->f = distance;
                const float priority = at_fadd(next->e, next->f);
                if (next->d >= 0) {
                    pf_at(w, "a", "(Lsn;Ld;Ld;Ld;F)Ldh;", 219);
                    if (!pf_heap_change(w, next, priority))
                        return false;
                } else {
                    next->g = priority;
                    pf_at(w, "a", "(Lsn;Ld;Ld;Ld;F)Ldh;", 247);
                    if (!pf_heap_add(w, next))
                        return false;
                }
            }
        }
    }
    if (closest == start) {
        pf_at(w, "a", "(Lsn;Ld;Ld;Ld;F)Ldh;", 266);
        return true;
    }
    pf_at(w, "a", "(Lsn;Ld;Ld;Ld;F)Ldh;", 272);
    return pf_reconstruct(w, closest);
}

static bool floor_at(mc_path_finder *w, double value, int32_t pc, int32_t *out) {
    pf_at(w, "a", "(Lsn;DDDF)Ldh;", pc);
    if (!pf_math(w) || !pf_fp(w))
        return false;
    *out = mc_math_floor_double(value);
    return true;
}

static bool prepare(mc_path_finder *w, mc_entity *source, double x, double y, double z,
                    float range) {
    pf_at(w, "a", "(Lsn;DDDF)Ldh;", 4);
    pf_heap_clear(w);
    pf_at(w, "a", "(Lsn;DDDF)Ldh;", 11);
    if (!pf_map_clear(w))
        return false;
    pf_at(w, "a", "(Lsn;DDDF)Ldh;", 16);
    if (source == NULL)
        return pf_source(w, "java/lang/NullPointerException", "Path preparation null Entity");
    if (source->classes != w->classes || source->class_literal == NULL ||
        !mc_entity_is_kind(source, source->class_literal->kind))
        return pf_fail(w, MC_PATH_STATE, "Path preparation canonical Entity");
    pf_at(w, "a", "(Lsn;DDDF)Ldh;", 19);
    if (source->bounds == NULL)
        return pf_source(w, "java/lang/NullPointerException", "Path preparation null bounds");
    int32_t xx, yy, zz;
    if (!floor_at(w, source->bounds->min_x, 22, &xx))
        return false;
    pf_at(w, "a", "(Lsn;DDDF)Ldh;", 29);
    if (source->bounds == NULL)
        return pf_source(w, "java/lang/NullPointerException", "Path preparation null Y bounds");
    if (!floor_at(w, source->bounds->min_y, 32, &yy))
        return false;
    pf_at(w, "a", "(Lsn;DDDF)Ldh;", 39);
    if (source->bounds == NULL)
        return pf_source(w, "java/lang/NullPointerException", "Path preparation null Z bounds");
    if (!floor_at(w, source->bounds->min_z, 42, &zz))
        return false;
    mc_path_point *start = pf_intern(w, xx, yy, zz);
    if (start == NULL)
        return false;
    w->state.start = start;
    if (!floor_at(w, at_sub(x, at_widen(at_fdiv(source->base.width, 2.0f))), 60, &xx) ||
        !floor_at(w, y, 65, &yy) ||
        !floor_at(w, at_sub(z, at_widen(at_fdiv(source->base.width, 2.0f))), 78, &zz))
        return false;
    mc_path_point *goal = pf_intern(w, xx, yy, zz);
    if (goal == NULL)
        return false;
    w->state.goal = goal;
    pf_at(w, "a", "(Lsn;DDDF)Ldh;", 86);
    mc_path_point *extent = pf_point_new(w);
    if (extent == NULL)
        return false;
    static const int32_t pcs[] = {96, 105, 114};
    int32_t dimensions[3];
    for (size_t i = 0; i < 3; ++i) {
        const float value = at_fadd(i == 1 ? source->base.height : source->base.width, 1.0f);
        pf_at(w, "a", "(Lsn;DDDF)Ldh;", pcs[i]);
        if (!pf_math(w))
            return false;
        const int32_t converted = mc_java_float_to_i32(value);
        dimensions[i] = value < (float)converted ? mc_java_i32_sub(converted, 1) : converted;
    }
    pf_at(w, "a", "(Lsn;DDDF)Ldh;", 117);
    pf_point_construct(w, extent, dimensions[0], dimensions[1], dimensions[2]);
    w->state.extent = extent;
    pf_at(w, "a", "(Lsn;DDDF)Ldh;", 132);
    if (!search(w, start, goal, extent, range))
        return false;
    pf_at(w, "a", "(Lsn;DDDF)Ldh;", 139);
    return true;
}

static mc_path_status enter(mc_path_finder *w) {
    if (w == NULL)
        return MC_PATH_ARGUMENT;
    if (w->state.active)
        return MC_PATH_BUSY;
    if (!w->state.constructed || w->state.failed || w->state.returned)
        return MC_PATH_STATE;
    w->state.active = true;
    return MC_PATH_OK;
}

static mc_path_status leave(mc_path_finder *w, bool ok) {
    w->state.active = false;
    if (ok) {
        w->state.returned = true;
        return MC_PATH_OK;
    }
    return w->error.path.status;
}

mc_path_status mc_path_finder_to_entity(mc_path_finder *w, mc_entity *source, mc_entity *target,
                                        float range) {
    const mc_path_status status = enter(w);
    if (status != MC_PATH_OK)
        return status;
    pf_at(w, "a", "(Lsn;Lsn;F)Ldh;", 3);
    if (target == NULL)
        return leave(w, pf_source(w, "java/lang/NullPointerException", "Path null target Entity"));
    if (target->classes != w->classes || target->class_literal == NULL ||
        !mc_entity_is_kind(target, target->class_literal->kind))
        return leave(w, pf_fail(w, MC_PATH_STATE, "Path target canonical Entity"));
    const double x = target->base.x;
    pf_at(w, "a", "(Lsn;Lsn;F)Ldh;", 10);
    if (target->bounds == NULL)
        return leave(w, pf_source(w, "java/lang/NullPointerException", "Path null target bounds"));
    const double y = target->bounds->min_y;
    const double z = target->base.z;
    const bool ok = prepare(w, source, x, y, z, range);
    if (ok)
        pf_at(w, "a", "(Lsn;Lsn;F)Ldh;", 21);
    return leave(w, ok);
}

mc_path_status mc_path_finder_to_coordinates(mc_path_finder *w, mc_entity *source, int32_t x,
                                             int32_t y, int32_t z, float range) {
    const mc_path_status status = enter(w);
    if (status != MC_PATH_OK)
        return status;
    pf_at(w, "a", "(Lsn;IIIF)Ldh;", 6);
    if (!pf_fp(w))
        return leave(w, false);
    const double xx = at_widen(at_fadd((float)x, .5f));
    const double yy = at_widen(at_fadd((float)y, .5f));
    const double zz = at_widen(at_fadd((float)z, .5f));
    const bool ok = prepare(w, source, xx, yy, zz, range);
    if (ok)
        pf_at(w, "a", "(Lsn;IIIF)Ldh;", 26);
    return leave(w, ok);
}
