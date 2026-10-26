#include "internal.h"

static int run_worker(void *context) {
    stats_worker *worker = context;
    mc_client_stats_writer *writer = worker->owner;
    mc_client_stats_error error;
    stats_error_clear(&error);
    mc_client_stats_report report = {0};
    mc_client_stats_status status = MC_CLIENT_STATS_OK;
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
            mc_client_stats_map_retire(&map);
        } else if (stats_file_exists(writer->retained_syncher->files[1], &error)) {
            mc_client_stats_map *map = NULL;
            status = stats_read_file_set(writer, 1, &map, &report, &error);
            if (status == MC_CLIENT_STATS_OK) {
                (void)mtx_lock(&writer->mutex);
                mc_client_stats_map *displaced = writer->retained_syncher->pending_main;
                writer->retained_syncher->pending_main = map;
                (void)mtx_unlock(&writer->mutex);
                mc_client_stats_map_retire(&displaced);
            }
        }
        status = error.status;
    }
    if (status == MC_CLIENT_STATS_SOURCE_EXCEPTION)
        (void)stats_catch(writer, &report, &error);
    (void)mtx_lock(&writer->mutex);
    writer->retained_syncher->busy = false; /* Both actual worker finally paths. */
    worker->error = error;
    worker->report = report;
    worker->completed = true;
    ++writer->workers_completed;
    writer->last_worker_error = error;
    writer->last_worker_report = report;
    (void)mtx_unlock(&writer->mutex);
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

mc_client_stats_status mc_client_stats_writer_retire(mc_client_stats_writer **owner,
                                                     mc_client_stats_error *out_error) {
    mc_client_stats_error error;
    stats_error_clear(&error);
    if (owner == NULL)
        stats_fail(&error, MC_CLIENT_STATS_INVALID_ARGUMENT, "Stats native retirement output", NULL,
                   NULL, NULL, 0);
    else if (*owner != NULL) {
        mc_client_stats_writer *writer = *owner;
        if (stats_enter(writer, &error)) {
            writer->retiring = true;
            (void)stats_join_workers(writer, false, &error);
            if (writer->workers != NULL) {
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
    return error.status;
}
