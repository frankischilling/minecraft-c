#include "internal.h"

static int run_worker(void *context) {
    stats_worker *worker = context;
    mc_client_stats_writer *writer = worker->owner;
    mc_client_stats_error error;
    stats_error_clear(&error);
    mc_client_stats_report report = {0};
    mc_client_stats_status status = MC_CLIENT_STATS_OK;
    const bool typed = writer->memory->source != NULL;
    stats_source_error_scope source_diagnostic;
    if (typed)
        stats_source_scope_open(&source_diagnostic, &error);
    if (worker->write)
        status = stats_write_file_set(writer, worker->map, 0, &report, &error);
    else {
        (void)mtx_lock(&writer->mutex);
        const bool had_pending = writer->retained_syncher->pending_main != NULL;
        (void)mtx_unlock(&writer->mutex);
        if (had_pending) {
            /* Two genuine volatile loads. Retain the second loaded identity,
             * including a null reload after source service cleared the field. */
            (void)mtx_lock(&writer->mutex);
            mc_client_stats_map *map = writer->retained_syncher->pending_main;
            if (map != NULL) {
                status = mc_client_stats_map_retain(map, &error);
                if (status != MC_CLIENT_STATS_OK)
                    map = NULL;
            }
            (void)mtx_unlock(&writer->mutex);
            if (status == MC_CLIENT_STATS_OK)
                status = stats_write_file_set(writer, map, 1, &report, &error);
            if (typed && status != MC_CLIENT_STATS_OK)
                stats_source_frame(&error, "vk", "run", "()V", 42);
            mc_client_stats_map_retire(&map);
        } else {
            const bool exists = stats_owner_file_exists(writer, 1, &error);
            if (typed && error.status != MC_CLIENT_STATS_OK) {
                error.class_name = "vk";
                error.method = "run";
                error.descriptor = "()V";
                error.bytecode = 55;
            }
            if (exists) {
                mc_client_stats_map *map = NULL;
                status = stats_read_file_set(writer, 1, &map, &report, &error);
                if (status == MC_CLIENT_STATS_OK) {
                    (void)mtx_lock(&writer->mutex);
                    mc_client_stats_map *displaced = writer->retained_syncher->pending_main;
                    writer->retained_syncher->pending_main = map;
                    (void)mtx_unlock(&writer->mutex);
                    mc_client_stats_map_retire(&displaced);
                } else if (typed)
                    stats_source_frame(&error, "vk", "run", "()V", 90);
            }
        }
        status = error.status;
    }
    if (status == MC_CLIENT_STATS_SOURCE_EXCEPTION) {
        if (typed)
            (void)stats_source_worker_caught(writer, &report, &error);
        else
            (void)stats_catch(writer, &report, &error);
    }
    (void)mtx_lock(&writer->mutex);
    /* A stopped native dependency is not a completed Java finally. Native
     * completion/join remains separate, and the reached source busy prefix stays. */
    if (!typed || error.status == MC_CLIENT_STATS_OK)
        writer->retained_syncher->busy = false;
    if (typed && error.status != MC_CLIENT_STATS_OK)
        stats_source_publish_failure(writer, &error);
    worker->error = error;
    worker->report = report;
    worker->completed = true;
    ++writer->workers_completed;
    writer->last_worker_error = error;
    writer->last_worker_report = report;
    (void)mtx_unlock(&writer->mutex);
    if (typed)
        stats_source_scope_close(&source_diagnostic);
    return 0;
}

/* Called with the writer's recursive mutex held once by its entered API.
 * Completed contexts stay retained until their real handles are joined. */
mc_client_stats_status stats_join_workers(mc_client_stats_writer *writer, bool completed_only,
                                          mc_client_stats_error *error) {
    mc_client_stats_status status = MC_CLIENT_STATS_OK;
    stats_worker **link = &writer->workers;
    while (*link != NULL) {
        stats_worker *worker = *link;
        if (!worker->started || (completed_only && !worker->completed)) {
            link = &worker->next;
            continue;
        }
        (void)mtx_unlock(&writer->mutex);
        int result = 0;
        const int joined = thrd_join(worker->thread, &result);
        (void)mtx_lock(&writer->mutex);
        if (joined != thrd_success)
            return stats_fail(error, MC_CLIENT_STATS_THREAD_FAILED, "Stats native worker join",
                              NULL, NULL, NULL, 0);
        worker->joined = true;
        ++writer->workers_joined;
        if (status == MC_CLIENT_STATS_OK && worker->error.status != MC_CLIENT_STATS_OK) {
            *error = worker->error;
            status = error->status;
        }
        *link = worker->next;
        mc_client_stats_map_retire(&worker->map);
        stats_free(worker);
    }
    return status;
}

mc_client_stats_status stats_start_worker(mc_client_stats_writer *writer, bool write,
                                          mc_client_stats_map *map, mc_client_stats_error *error) {
    stats_syncher *syncher = writer->retained_syncher;
    if (syncher->busy)
        return stats_exception(error, MC_CLIENT_STATS_ILLEGAL_STATE,
                               write ? "Can't save stats while StatsSyncher is busy!"
                                     : "Can't get stats from server while StatsSyncher is busy!",
                               "cj", "a", write ? "(Ljava/util/Map;)V" : "()V", 16);
    if (stats_join_workers(writer, true, error) != MC_CLIENT_STATS_OK)
        return error->status;
    syncher->delay = 100;
    syncher->busy = true; /* Retained on any launch failure. */
    if (writer->workers_started == SIZE_MAX)
        return stats_fail(error, MC_CLIENT_STATS_LIMIT, "Stats native worker count", NULL, NULL,
                          NULL, 0);
    stats_worker *worker = stats_alloc(writer->memory, sizeof(*worker), error);
    if (worker == NULL)
        return error->status;
    *worker = (stats_worker){.owner = writer, .write = write};
    if (write && map != NULL) {
        if (mc_client_stats_map_retain(map, error) != MC_CLIENT_STATS_OK) {
            stats_free(worker);
            return error->status;
        }
        worker->map = map;
    }
    const int created = thrd_create(&worker->thread, run_worker, worker);
    if (created != thrd_success) {
        mc_client_stats_map_retire(&worker->map);
        stats_free(worker);
        error->system_error = created;
        return stats_fail(error, MC_CLIENT_STATS_THREAD_FAILED, "Stats native worker launch", "cj",
                          "a", write ? "(Ljava/util/Map;)V" : "()V", write ? 37u : 36u);
    }
    worker->started = true;
    worker->next = writer->workers;
    writer->workers = worker;
    ++writer->workers_started;
    return MC_CLIENT_STATS_OK;
}

mc_client_stats_status mc_client_stats_writer_join(mc_client_stats_writer *writer,
                                                   mc_client_stats_error *out_error) {
    mc_client_stats_error error;
    stats_error_clear(&error);
    if (stats_enter(writer, &error)) {
        (void)stats_join_workers(writer, false, &error);
        stats_leave(writer);
    }
    if (out_error != NULL)
        *out_error = error;
    return error.status;
}

static mc_client_stats_status retire_source(mc_client_stats_writer *writer,
                                            mc_client_stats_error *original) {
    mc_client_stats_error cleanup;
    stats_error_clear(&cleanup);
    stats_source_error_scope source_diagnostic;
    stats_source_scope_open(&source_diagnostic, &cleanup);
    const mc_client_stats_status status = stats_source_retire(writer, &cleanup);
    stats_source_scope_close(&source_diagnostic);
    if (status != MC_CLIENT_STATS_OK)
        *original = cleanup;
    return status;
}

static mc_client_stats_status retire_writer(mc_client_stats_writer **owner,
                                            mc_client_stats_error *out_error,
                                            mc_client_stats_file_error *source_error) {
    mc_client_stats_error error;
    stats_error_clear(&error);
    mc_client_stats_file_error source_snapshot = {0};
    if (owner == NULL)
        stats_fail(&error, MC_CLIENT_STATS_INVALID_ARGUMENT, "Stats native retirement output", NULL,
                   NULL, NULL, 0);
    else if (*owner != NULL) {
        mc_client_stats_writer *writer = *owner;
        if (stats_enter_retirement(writer, &error)) {
            writer->retiring = true;
            (void)stats_join_workers(writer, false, &error);
            if (writer->memory->source != NULL && error.status != MC_CLIENT_STATS_OK) {
                const mc_client_stats_file_error *first = &writer->memory->source->failure;
                /* A distinct native join failure is not a File/Throwable cause. */
                if (first->base.status == error.status &&
                    first->base.operation == error.operation &&
                    first->base.class_name == error.class_name &&
                    first->base.bytecode == error.bytecode)
                    source_snapshot = *first;
            }
            if (writer->workers != NULL) {
                writer->retiring = false;
                stats_leave(writer);
            } else if (retire_source(writer, &error) != MC_CLIENT_STATS_OK) {
                if (writer->memory->source != NULL)
                    source_snapshot = writer->memory->source->cleanup_failure;
                writer->retiring = false;
                stats_leave(writer);
            } else {
                stats_syncher *syncher = writer->retained_syncher;
                if (syncher != NULL) {
                    mc_client_stats_map_retire(&syncher->pending_main);
                    mc_client_stats_map_retire(&syncher->pending_other);
                    mc_player_session_retire(&syncher->session);
                    stats_files_clear(syncher);
                    stats_free(syncher);
                }
                mc_client_stats_map_retire(&writer->total);
                mc_client_stats_map_retire(&writer->unsent);
                writer->phase = MC_CLIENT_STATS_RETIRED;
                stats_memory *memory = writer->memory;
                (void)mtx_unlock(&writer->mutex);
                mtx_destroy(&writer->mutex);
                stats_free(writer);
                stats_memory_release(memory);
                *owner = NULL;
            }
        }
    }
    if (out_error != NULL)
        *out_error = error;
    if (source_error != NULL) {
        source_snapshot.base = error;
        *source_error = source_snapshot;
    }
    return error.status;
}

mc_client_stats_status mc_client_stats_writer_retire(mc_client_stats_writer **owner,
                                                     mc_client_stats_error *out_error) {
    return retire_writer(owner, out_error, NULL);
}

mc_client_stats_status
mc_client_stats_writer_retire_file(mc_client_stats_writer **owner,
                                   mc_client_stats_file_error *source_error) {
    return retire_writer(owner, NULL, source_error);
}
