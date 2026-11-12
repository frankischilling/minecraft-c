#include "mc/server/runtime/construction.h"

#include <stdlib.h>
#include <threads.h>

struct mc_server_reference_list {
    const void **items;
    size_t count, capacity;
};

struct mc_server_synchronized_list {
    mc_server_reference_list *backing;
    mtx_t mutex;
};

struct mc_server_tracker_array {
    mc_server_tracker *items[2];
};

typedef struct mc_server_lifetime {
    mtx_t mutex;
    cnd_t condition;
    thrd_t thread;
    bool mutex_ready, condition_ready, started, stopping;
} mc_server_lifetime;

struct mc_server_runtime {
    mc_server_runtime_fields fields;
    mc_server_construction construction;
    mc_server_reference_list *s_backing;
    mc_server_lifetime *lifetime;
};

static mc_server_status result(mc_server_error *error, mc_server_status status,
                               const char *operation, unsigned int bytecode, int thread_status) {
    if (error != NULL)
        *error = (mc_server_error){status, operation, bytecode, thread_status};
    return status;
}

static void *request(mc_server_runtime *server, size_t bytes, unsigned int bytecode,
                     const char *operation, mc_server_error *error) {
    mc_server_construction *state = &server->construction;
    state->failed_call_pc = bytecode;
    if (bytes > state->max_direct_allocation_bytes - state->direct_allocation_bytes) {
        (void)result(error, MC_SERVER_LIMIT, operation, bytecode, 0);
        return NULL;
    }
    void *memory = calloc(1, bytes);
    if (memory == NULL) {
        (void)result(error, MC_SERVER_ALLOCATION_FAILED, operation, bytecode, 0);
        return NULL;
    }
    state->direct_allocation_bytes += bytes;
    return memory;
}

static int lifetime_wait(void *argument) {
    mc_server_lifetime *lifetime = argument;
    if (mtx_lock(&lifetime->mutex) != thrd_success)
        return thrd_error;
    while (!lifetime->stopping) {
        if (cnd_wait(&lifetime->condition, &lifetime->mutex) != thrd_success) {
            (void)mtx_unlock(&lifetime->mutex);
            return thrd_error;
        }
    }
    return mtx_unlock(&lifetime->mutex);
}

mc_server_status mc_server_runtime_allocate(size_t max_direct_bytes, mc_server_runtime **out,
                                            mc_server_error *error) {
    if (out == NULL)
        return result(error, MC_SERVER_INVALID_ARGUMENT, "Server allocation output", 0, 0);
    if (max_direct_bytes < sizeof(mc_server_runtime))
        return result(error, MC_SERVER_LIMIT, "Server raw owner limit", 0, 0);
    mc_server_runtime *server = calloc(1, sizeof(*server));
    if (server == NULL)
        return result(error, MC_SERVER_ALLOCATION_FAILED, "Server raw owner", 0, 0);
    server->construction = (mc_server_construction){.phase = MC_SERVER_RAW,
                                                    .direct_allocation_bytes = sizeof(*server),
                                                    .max_direct_allocation_bytes = max_direct_bytes,
                                                    .references = 1};
    *out = server;
    return result(error, MC_SERVER_OK, NULL, 0, 0);
}

mc_server_status mc_server_runtime_construct(mc_server_runtime *server, mc_server_error *error) {
    mc_server_error ignored;
    if (error == NULL)
        error = &ignored;
    if (server == NULL)
        return result(error, MC_SERVER_INVALID_ARGUMENT, "Server constructor receiver", 0, 0);
    mc_server_construction *state = &server->construction;
    if (state->phase != MC_SERVER_RAW)
        return result(error, MC_SERVER_UNSUPPORTED_STATE, "Server constructor phase", 0, 0);
    state->phase = MC_SERVER_CONSTRUCTING;
    mc_server_runtime_fields *f = &server->fields;
    f->q = true;
    state->last_store_pc = 6;
    f->g = false;
    state->last_store_pc = 11;
    f->h = 0;
    state->last_store_pc = 16;
    f->r = request(server, sizeof(*f->r), 24, "Server r List", error);
    if (f->r == NULL)
        goto failed;
    state->last_store_pc = 27;
    server->s_backing = request(server, sizeof(*server->s_backing), 35, "Server s backing", error);
    if (server->s_backing == NULL)
        goto failed;
    mc_server_synchronized_list *wrapper =
        request(server, sizeof(*wrapper), 38, "Server synchronized List", error);
    if (wrapper == NULL)
        goto failed;
    const int mutex_status = mtx_init(&wrapper->mutex, mtx_plain);
    if (mutex_status != thrd_success) {
        free(wrapper);
        state->direct_allocation_bytes -= sizeof(*wrapper);
        (void)result(error, MC_SERVER_THREAD_FAILED, "Server List mutex", 38, mutex_status);
        goto failed;
    }
    wrapper->backing = server->s_backing;
    f->s = wrapper;
    state->last_store_pc = 41;
    f->k = request(server, sizeof(*f->k), 46, "Server tracker array", error);
    if (f->k == NULL)
        goto failed;
    state->last_store_pc = 49;
    server->lifetime = request(server, sizeof(*server->lifetime), 57, "Server lifetime", error);
    if (server->lifetime == NULL)
        goto failed;
    mc_server_lifetime *lifetime = server->lifetime;
    int thread_status = mtx_init(&lifetime->mutex, mtx_plain);
    if (thread_status != thrd_success) {
        (void)result(error, MC_SERVER_THREAD_FAILED, "Server lifetime mutex", 57, thread_status);
        goto failed;
    }
    lifetime->mutex_ready = true;
    thread_status = cnd_init(&lifetime->condition);
    if (thread_status != thrd_success) {
        (void)result(error, MC_SERVER_THREAD_FAILED, "Server lifetime condition", 57,
                     thread_status);
        goto failed;
    }
    lifetime->condition_ready = true;
    thread_status = thrd_create(&lifetime->thread, lifetime_wait, lifetime);
    if (thread_status != thrd_success) {
        (void)result(error, MC_SERVER_THREAD_FAILED, "Server lifetime thread", 57, thread_status);
        goto failed;
    }
    lifetime->started = true;
    state->lifetime_started = true;
    state->failed_call_pc = 0;
    state->phase = MC_SERVER_READY;
    return result(error, MC_SERVER_OK, NULL, 0, 0);
failed:
    state->phase = MC_SERVER_FAILED_CONSTRUCTION;
    return error->status;
}

mc_server_status mc_server_runtime_inspect(const mc_server_runtime *server,
                                           mc_server_runtime_fields *fields,
                                           mc_server_construction *construction) {
    if (server == NULL || (fields == NULL && construction == NULL))
        return MC_SERVER_INVALID_ARGUMENT;
    if (fields != NULL)
        *fields = server->fields;
    if (construction != NULL)
        *construction = server->construction;
    return MC_SERVER_OK;
}

mc_server_status mc_server_runtime_retain(mc_server_runtime *server) {
    if (server == NULL)
        return MC_SERVER_INVALID_ARGUMENT;
    if (server->construction.references == SIZE_MAX)
        return MC_SERVER_LIMIT;
    ++server->construction.references;
    return MC_SERVER_OK;
}

mc_server_status mc_server_runtime_release(mc_server_runtime *server) {
    if (server == NULL)
        return MC_SERVER_OK;
    if (server->construction.references > 1) {
        --server->construction.references;
        return MC_SERVER_OK;
    }
    mc_server_lifetime *lifetime = server->lifetime;
    if (lifetime != NULL && lifetime->started) {
        if (mtx_lock(&lifetime->mutex) != thrd_success)
            return MC_SERVER_THREAD_FAILED;
        lifetime->stopping = true;
        const int signaled = cnd_signal(&lifetime->condition);
        const int unlocked = mtx_unlock(&lifetime->mutex);
        if (signaled != thrd_success || unlocked != thrd_success)
            return MC_SERVER_THREAD_FAILED;
        int outcome;
        if (thrd_join(lifetime->thread, &outcome) != thrd_success)
            return MC_SERVER_THREAD_FAILED;
        lifetime->started = false;
    }
    if (lifetime != NULL) {
        if (lifetime->condition_ready)
            cnd_destroy(&lifetime->condition);
        if (lifetime->mutex_ready)
            mtx_destroy(&lifetime->mutex);
        free(lifetime);
    }
    if (server->fields.s != NULL) {
        mtx_destroy(&server->fields.s->mutex);
        free(server->fields.s);
    }
    free(server->s_backing);
    free(server->fields.r);
    free(server->fields.k);
    free(server);
    return MC_SERVER_OK;
}

mc_server_status mc_server_reference_list_view(const mc_server_reference_list *list,
                                               const void *const **items, size_t *count) {
    if (list == NULL || items == NULL || count == NULL)
        return MC_SERVER_INVALID_ARGUMENT;
    *items = list->items;
    *count = list->count;
    return MC_SERVER_OK;
}

mc_server_status mc_server_synchronized_list_view(const mc_server_synchronized_list *list,
                                                  const mc_server_reference_list **backing) {
    if (list == NULL || backing == NULL)
        return MC_SERVER_INVALID_ARGUMENT;
    *backing = list->backing;
    return MC_SERVER_OK;
}

mc_server_status mc_server_tracker_array_view(const mc_server_tracker_array *array,
                                              mc_server_tracker *const **items, size_t *count) {
    if (array == NULL || items == NULL || count == NULL)
        return MC_SERVER_INVALID_ARGUMENT;
    *items = array->items;
    *count = 2;
    return MC_SERVER_OK;
}
