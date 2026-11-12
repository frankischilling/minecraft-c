#include "path_internal.h"
#include <string.h>

void mc_path_error_init(mc_path_error *e) {
    if (e == NULL)
        return;
    memset(e, 0, sizeof(*e));
    e->site.pc = e->child_site.pc = -1;
}

static mc_path_status path_fail(mc_path_error *e, mc_path_status status, const char *operation,
                                const char *method, const char *descriptor, int32_t pc,
                                const char *exception) {
    if (e != NULL) {
        e->status = status;
        e->operation = operation;
        e->site = (mc_path_site){"dh", method, descriptor, pc};
        e->source_exception = exception != NULL;
        e->native_policy = exception == NULL;
        e->exception_class = exception;
    }
    return status;
}

mc_path_status mc_path_retain(mc_path *p, mc_path_error *e) {
    mc_path_error_init(e);
    if (p == NULL || p->holds == 0)
        return path_fail(e, MC_PATH_ARGUMENT, "Path native occurrence", NULL, NULL, -1, NULL);
    if (p->holds == SIZE_MAX)
        return path_fail(e, MC_PATH_LIMIT, "Path occurrence ceiling", NULL, NULL, -1, NULL);
    ++p->holds;
    return MC_PATH_OK;
}

void mc_path_release(mc_path *p) {
    if (p != NULL && --p->holds == 0)
        pf_arena_drop(p->arena);
}

bool mc_path_inspect(const mc_path *p, mc_path_state *out) {
    if (p == NULL || out == NULL || p->holds == 0)
        return false;
    *out = (mc_path_state){.a = p->a,
                           .c = p->c,
                           .b = p->b,
                           .array_length = p->array_length,
                           .holds = p->holds,
                           .arena_holds = p->arena->holds,
                           .arena_points = p->arena->points,
                           .arena_bytes = p->arena->bytes,
                           .classes = p->arena->classes,
                           .arena_identity = p->arena,
                           .constructed = p->constructed,
                           .constructor_pc = p->constructor_pc};
    return true;
}

bool pf_point_snapshot(const mc_path_arena *arena, const mc_path_point *point,
                       mc_path_point_state *out) {
    if (arena == NULL || point == NULL || out == NULL)
        return false;
    for (const mc_path_point *p = arena->point_list; p != NULL; p = p->allocation_next)
        if (p == point) {
            *out = (mc_path_point_state){p->a, p->b, p->c, p->j, p->d,           p->e,
                                         p->f, p->g, p->h, p->i, p->constructed, p->constructor_pc};
            return true;
        }
    return false;
}

bool mc_path_point_inspect(const mc_path *p, const mc_path_point *point, mc_path_point_state *out) {
    return p != NULL && p->holds != 0 && pf_point_snapshot(p->arena, point, out);
}

mc_path_status mc_path_advance(mc_path *p, mc_path_error *e) {
    mc_path_error_init(e);
    if (p == NULL)
        return path_fail(e, MC_PATH_SOURCE_EXCEPTION, "Path advance null receiver", "a", "()V", 2,
                         "java/lang/NullPointerException");
    p->c = mc_java_i32_add(p->c, 1);
    if (e != NULL)
        e->site = (mc_path_site){"dh", "a", "()V", 10};
    return MC_PATH_OK;
}

mc_path_status mc_path_finished(mc_path *p, bool *out, mc_path_error *e) {
    mc_path_error_init(e);
    if (out == NULL)
        return path_fail(e, MC_PATH_ARGUMENT, "Path finished output", NULL, NULL, -1, NULL);
    if (p == NULL || p->b == NULL)
        return path_fail(e, MC_PATH_SOURCE_EXCEPTION, "Path finished null receiver/array", "b",
                         "()Z", p == NULL ? 1 : 8, "java/lang/NullPointerException");
    *out = p->c >= 0 && (size_t)(uint32_t)p->c >= p->array_length;
    if (e != NULL)
        e->site = (mc_path_site){"dh", "b", "()Z", 17};
    return MC_PATH_OK;
}

static mc_path_status element(mc_path *p, int32_t index, unsigned int pc, const char *method,
                              const char *descriptor, mc_path_point **out, mc_path_error *e) {
    if (p->b == NULL)
        return path_fail(e, MC_PATH_SOURCE_EXCEPTION, "Path null Point array", method, descriptor,
                         (int32_t)pc, "java/lang/NullPointerException");
    if (index < 0 || (size_t)(uint32_t)index >= p->array_length)
        return path_fail(e, MC_PATH_SOURCE_EXCEPTION, "Path Point array index", method, descriptor,
                         (int32_t)pc, "java/lang/ArrayIndexOutOfBoundsException");
    *out = p->b[index];
    return MC_PATH_OK;
}

mc_path_status mc_path_last(mc_path *p, const mc_path_point **out, mc_path_error *e) {
    mc_path_error local;
    if (e == NULL)
        e = &local;
    mc_path_error_init(e);
    if (out == NULL)
        return path_fail(e, MC_PATH_ARGUMENT, "Path last output", NULL, NULL, -1, NULL);
    if (p == NULL)
        return path_fail(e, MC_PATH_SOURCE_EXCEPTION, "Path last null receiver", "c", "()Ld;", 1,
                         "java/lang/NullPointerException");
    mc_path_point *point = NULL;
    if (p->a > 0 && element(p, mc_java_i32_sub(p->a, 1), 17, "c", "()Ld;", &point, e) != MC_PATH_OK)
        return e->status;
    *out = point;
    e->site = (mc_path_site){"dh", "c", "()Ld;", p->a > 0 ? 18 : 20};
    return MC_PATH_OK;
}

mc_path_status mc_path_waypoint(mc_path *p, mc_entity *entity, mc_entity_vector **out,
                                mc_path_error *e) {
    mc_path_error local;
    if (e == NULL)
        e = &local;
    mc_path_error_init(e);
    if (out == NULL)
        return path_fail(e, MC_PATH_ARGUMENT, "Path waypoint output", NULL, NULL, -1, NULL);
    if (p == NULL)
        return path_fail(e, MC_PATH_SOURCE_EXCEPTION, "Path waypoint null receiver", "a",
                         "(Lsn;)Lbt;", 1, "java/lang/NullPointerException");
    mc_path_point *point;
    if (element(p, p->c, 8, "a", "(Lsn;)Lbt;", &point, e) != MC_PATH_OK)
        return e->status;
    if (point == NULL || entity == NULL)
        return path_fail(e, MC_PATH_SOURCE_EXCEPTION, "Path waypoint null Point/Entity", "a",
                         "(Lsn;)Lbt;", point == NULL ? 9 : 14, "java/lang/NullPointerException");
    if (entity->classes != p->arena->classes || entity->class_literal == NULL ||
        !mc_entity_is_kind(entity, entity->class_literal->kind))
        return path_fail(e, MC_PATH_STATE, "Path waypoint canonical Entity namespace", "a",
                         "(Lsn;)Lbt;", 14, NULL);
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
        return path_fail(e, MC_PATH_STATE, "Path waypoint floating controls", "a", "(Lsn;)Lbt;", 18,
                         NULL);
    const double x =
        at_add((double)point->a,
               at_mul((double)mc_java_float_to_i32(at_fadd(entity->base.width, 1.0f)), .5));
    if (element(p, p->c, 35, "a", "(Lsn;)Lbt;", &point, e) != MC_PATH_OK)
        return e->status;
    if (point == NULL)
        return path_fail(e, MC_PATH_SOURCE_EXCEPTION, "Path waypoint null Y Point", "a",
                         "(Lsn;)Lbt;", 36, "java/lang/NullPointerException");
    const double y = (double)point->b;
    if (element(p, p->c, 50, "a", "(Lsn;)Lbt;", &point, e) != MC_PATH_OK)
        return e->status;
    if (point == NULL)
        return path_fail(e, MC_PATH_SOURCE_EXCEPTION, "Path waypoint null Z Point", "a",
                         "(Lsn;)Lbt;", 51, "java/lang/NullPointerException");
    const double z =
        at_add((double)point->c,
               at_mul((double)mc_java_float_to_i32(at_fadd(entity->base.width, 1.0f)), .5));
    e->site = (mc_path_site){"dh", "a", "(Lsn;)Lbt;", 75};
    if (mc_entity_vector_pool_get(p->arena->classes, x, y, z, out, &e->vector) != MC_ENTITY_OK)
        return path_fail(e, MC_PATH_CHILD_FAILED, "Path actual pooled waypoint", "a", "(Lsn;)Lbt;",
                         75, NULL);
    e->site = (mc_path_site){"dh", "a", "(Lsn;)Lbt;", 78};
    return MC_PATH_OK;
}
