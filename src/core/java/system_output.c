#include "print_stream_internal.h"

#include <stdint.h>
#include <stdlib.h>

static bool system_lock(mc_java_system_output *s, mc_java_print_error *error) {
    if (s == NULL) {
        mc_ps_fail(error, MC_JAVA_PRINT_INVALID_ARGUMENT, "null native System namespace");
        return false;
    }
    if (mtx_trylock(&s->monitor) != thrd_success) {
        mc_ps_fail(error, MC_JAVA_PRINT_BUSY, "System output namespace monitor unavailable");
        return false;
    }
    return true;
}

static bool system_enter(mc_java_system_output *s, mc_java_print_error *error) {
    if (!system_lock(s, error))
        return false;
    if (s->active || s->phase != MC_JAVA_PRINT_READY) {
        mc_ps_fail(error, s->active ? MC_JAVA_PRINT_BUSY : MC_JAVA_PRINT_INVALID_PHASE,
                   "System output field admission");
        mtx_unlock(&s->monitor);
        return false;
    }
    s->active = true;
    return true;
}

static mc_java_print_status system_leave(mc_java_system_output *s, mc_java_print_error *error) {
    s->active = false;
    mtx_unlock(&s->monitor);
    return error->status;
}

mc_java_print_status mc_java_system_output_allocate(mc_java_system_output **out,
                                                    mc_java_print_error *error) {
    if (error == NULL)
        return MC_JAVA_PRINT_INVALID_ARGUMENT;
    *error = (mc_java_print_error){0};
    if (out == NULL)
        return mc_ps_fail(error, MC_JAVA_PRINT_INVALID_ARGUMENT, "System namespace slot");
    mc_java_system_output *s = calloc(1, sizeof(*s));
    if (s == NULL)
        return mc_ps_fail(error, MC_JAVA_PRINT_ALLOCATION_FAILED, "System output namespace");
    if (mtx_init(&s->monitor, mtx_recursive) != thrd_success) {
        free(s);
        return mc_ps_fail(error, MC_JAVA_PRINT_NATIVE_FAILURE, "System output recursive monitor");
    }
    s->holds = 1;
    *out = s;
    return MC_JAVA_PRINT_OK;
}

mc_java_print_status mc_java_system_output_construct(mc_java_system_output *s,
                                                     mc_java_print_error *error) {
    if (error == NULL)
        return MC_JAVA_PRINT_INVALID_ARGUMENT;
    *error = (mc_java_print_error){0};
    if (!system_lock(s, error))
        return error->status;
    if (s->active || s->phase != MC_JAVA_PRINT_RAW) {
        mc_ps_fail(error, s->active ? MC_JAVA_PRINT_BUSY : MC_JAVA_PRINT_INVALID_PHASE,
                   "System output bootstrap phase");
        mtx_unlock(&s->monitor);
        return error->status;
    }
    s->active = true;
    s->phase = MC_JAVA_PRINT_CONSTRUCTING;
    if (mc_java_print_stream_allocate(&s->pending, error) != MC_JAVA_PRINT_OK ||
        mc_java_print_stream_construct_stdout(s->pending, error) != MC_JAVA_PRINT_OK) {
        s->failure = *error;
        s->phase = MC_JAVA_PRINT_FAILED;
        return system_leave(s, error);
    }
    s->out = s->pending;
    s->pending = NULL;
    s->phase = MC_JAVA_PRINT_READY;
    return system_leave(s, error);
}

mc_java_print_status mc_java_system_output_retain(mc_java_system_output *s,
                                                  mc_java_print_error *error) {
    if (error == NULL)
        return MC_JAVA_PRINT_INVALID_ARGUMENT;
    *error = (mc_java_print_error){0};
    if (!system_lock(s, error))
        return error->status;
    if (s->phase == MC_JAVA_PRINT_RETIRING || s->holds == SIZE_MAX)
        mc_ps_fail(error, s->holds == SIZE_MAX ? MC_JAVA_PRINT_LIMIT : MC_JAVA_PRINT_INVALID_PHASE,
                   "System output retain admission");
    else
        ++s->holds;
    mtx_unlock(&s->monitor);
    return error->status;
}

mc_java_print_status mc_java_system_output_get(mc_java_system_output *s, mc_java_print_stream **out,
                                               mc_java_print_error *error) {
    if (error == NULL)
        return MC_JAVA_PRINT_INVALID_ARGUMENT;
    *error = (mc_java_print_error){0};
    if (out == NULL)
        return mc_ps_fail(error, MC_JAVA_PRINT_INVALID_ARGUMENT, "System.out read slot");
    if (!system_enter(s, error))
        return error->status;
    if (s->out != NULL && mc_java_print_stream_retain(s->out, error) != MC_JAVA_PRINT_OK)
        return system_leave(s, error);
    *out = s->out;
    return system_leave(s, error);
}

mc_java_print_status mc_java_system_output_bind(mc_java_system_output *s,
                                                mc_java_print_stream *value,
                                                mc_java_print_error *error) {
    if (error == NULL)
        return MC_JAVA_PRINT_INVALID_ARGUMENT;
    *error = (mc_java_print_error){0};
    if (!system_enter(s, error))
        return error->status;
    if (s->pending_old != NULL &&
        mc_java_print_stream_release(&s->pending_old, error) != MC_JAVA_PRINT_OK)
        return system_leave(s, error);
    if (value != NULL) {
        mc_java_print_state state;
        if (!mc_java_print_stream_inspect(value, &state) || state.phase != MC_JAVA_PRINT_READY) {
            mc_ps_fail(error, MC_JAVA_PRINT_INVALID_PHASE,
                       "System.out requires constructed stream");
            return system_leave(s, error);
        }
        if (mc_java_print_stream_retain(value, error) != MC_JAVA_PRINT_OK)
            return system_leave(s, error);
    }
    s->pending_old = s->out;
    s->out = value; /* Source field publication remains even if old native detach fails. */
    if (mc_java_print_stream_release(&s->pending_old, error) != MC_JAVA_PRINT_OK)
        s->failure = *error;
    return system_leave(s, error);
}

bool mc_java_system_output_inspect(mc_java_system_output *s, mc_java_system_output_state *out) {
    if (s == NULL || out == NULL || mtx_trylock(&s->monitor) != thrd_success)
        return false;
    *out = (mc_java_system_output_state){s->phase,   s->holds,       s->out,
                                         s->pending, s->pending_old, s->failure};
    mtx_unlock(&s->monitor);
    return true;
}

mc_java_print_status mc_java_system_output_release(mc_java_system_output **slot,
                                                   mc_java_print_error *error) {
    if (error == NULL)
        return MC_JAVA_PRINT_INVALID_ARGUMENT;
    *error = (mc_java_print_error){0};
    if (slot == NULL)
        return mc_ps_fail(error, MC_JAVA_PRINT_INVALID_ARGUMENT, "System output release slot");
    mc_java_system_output *s = *slot;
    if (s == NULL)
        return MC_JAVA_PRINT_OK;
    if (!system_lock(s, error))
        return error->status;
    if (s->holds > 1) {
        --s->holds;
        *slot = NULL;
        mtx_unlock(&s->monitor);
        return MC_JAVA_PRINT_OK;
    }
    if (s->active) {
        mtx_unlock(&s->monitor);
        return mc_ps_fail(error, MC_JAVA_PRINT_BUSY, "active final System output occurrence");
    }
    s->active = true;
    s->phase = MC_JAVA_PRINT_RETIRING;
    mc_java_print_error first = {0};
    mc_java_print_stream **children[] = {&s->out, &s->pending, &s->pending_old};
    for (size_t i = 0; i < sizeof(children) / sizeof(children[0]); ++i) {
        if (mc_java_print_stream_release(children[i], error) != MC_JAVA_PRINT_OK) {
            if (*children[i] != NULL) {
                s->failure = *error;
                return system_leave(s, error);
            }
            if (first.status == MC_JAVA_PRINT_OK)
                first = *error;
        }
    }
    *error = first;
    mtx_unlock(&s->monitor);
    mtx_destroy(&s->monitor);
    free(s);
    *slot = NULL;
    return error->status;
}
