#include "internal.h"
#include <string.h>

static bool stats_admit(mc_client_stats_writer *writer, mc_client_stats_error *error,
                        bool retirement) {
    if (writer == NULL) {
        stats_fail(error, MC_CLIENT_STATS_INVALID_ARGUMENT, "Stats native null writer", NULL, NULL,
                   NULL, 0);
        return false;
    }
    (void)mtx_lock(&writer->mutex);
    if (writer->entered || writer->retiring || writer->diagnostic_depth != 0 ||
        (!retirement && writer->memory->source != NULL && writer->memory->source->retiring)) {
        (void)mtx_unlock(&writer->mutex);
        stats_fail(error, MC_CLIENT_STATS_BUSY, "Stats native entered writer", NULL, NULL, NULL, 0);
        return false;
    }
    writer->entered = true;
    return true; /* Recursive mutex stays held until leave; explicit joins/sleeps release it. */
}

bool stats_enter(mc_client_stats_writer *writer, mc_client_stats_error *error) {
    return stats_admit(writer, error, false);
}

bool stats_enter_retirement(mc_client_stats_writer *writer, mc_client_stats_error *error) {
    return stats_admit(writer, error, true);
}

void stats_leave(mc_client_stats_writer *writer) {
    writer->entered = false;
    (void)mtx_unlock(&writer->mutex);
}

mc_client_stats_status mc_client_stats_writer_allocate(mc_registry_classes *registry,
                                                       const mc_client_stats_options *input,
                                                       mc_client_stats_writer **out,
                                                       mc_client_stats_error *out_error) {
    mc_client_stats_error error;
    stats_error_clear(&error);
    if (registry == NULL || out == NULL)
        stats_fail(&error, MC_CLIENT_STATS_INVALID_ARGUMENT, "Stats allocate native arguments",
                   NULL, NULL, NULL, 0);
    else {
        mc_registry_error cause;
        const mc_registry_status rs = mc_registry_classes_retain(
            registry, MC_REGISTRY_CLIENT, registry->options.random_profile, &cause);
        if (rs != MC_REGISTRY_OK) {
            error.registry = rs;
            stats_fail(&error, MC_CLIENT_STATS_REGISTRY_FAILED, "Stats writer Registry hold", NULL,
                       NULL, NULL, 0);
        } else {
            const mc_client_stats_options options = stats_options(input);
            stats_memory *memory = stats_memory_create(options.limits.max_allocation_bytes);
            if (memory != NULL)
                memory->registry = registry;
            mc_client_stats_writer *writer =
                memory == NULL ? NULL : stats_alloc(memory, sizeof(*writer), &error);
            if (writer == NULL) {
                if (memory == NULL)
                    stats_fail(&error, MC_CLIENT_STATS_ALLOCATION_FAILED, "Stats memory owner",
                               NULL, NULL, NULL, 0);
                stats_memory_release(memory);
                if (memory == NULL)
                    (void)mc_registry_runtime_borrow_release(registry);
            } else {
                *writer = (mc_client_stats_writer){
                    .memory = memory, .registry = registry, .options = options};
                if (mtx_init(&writer->mutex, mtx_recursive) != thrd_success) {
                    stats_free(writer);
                    stats_memory_release(memory);
                    stats_fail(&error, MC_CLIENT_STATS_THREAD_FAILED, "Stats native writer mutex",
                               NULL, NULL, NULL, 0);
                } else
                    *out = writer;
            }
        }
    }
    if (out_error != NULL)
        *out_error = error;
    return error.status;
}

mc_client_stats_status stats_apply_map(mc_client_stats_writer *writer,
                                       const mc_client_stats_map *map, int operation,
                                       mc_client_stats_error *error) {
    if (map == NULL)
        return MC_CLIENT_STATS_OK;
    if (map->registry != writer->registry)
        return stats_fail(error, MC_CLIENT_STATS_INVALID_ARGUMENT, "Stats Map Registry identity",
                          NULL, NULL, NULL, 0);
    if (operation != 1)
        writer->dirty = true; /* xi.a(Map), xi.c(Map), before iteration. */
    mc_client_stats_status status = stats_map_lease(map, error);
    if (status != MC_CLIENT_STATS_OK)
        return status;
    for (const stats_entry *entry = map->head; entry != NULL; entry = entry->next) {
        if (operation == 1) {
            bool present;
            int32_t delta;
            status = mc_client_stats_map_get(writer->unsent, entry->key, &present, &delta, error);
            if (status == MC_CLIENT_STATS_OK) {
                const uint32_t sum = (uint32_t)entry->value + (uint32_t)delta;
                const int32_t value = sum <= INT32_MAX
                                          ? (int32_t)sum
                                          : INT32_MIN + (int32_t)(sum - UINT32_C(0x80000000));
                status = mc_client_stats_map_put(writer->total, entry->key, value, error);
            }
        } else {
            status = stats_map_add(writer->unsent, entry->key, entry->value, error);
            if (status == MC_CLIENT_STATS_OK && operation == 0)
                status = stats_map_add(writer->total, entry->key, entry->value, error);
        }
        if (status != MC_CLIENT_STATS_OK)
            break;
    }
    stats_map_unlease(map);
    return status;
}

static mc_client_stats_status construct(mc_client_stats_writer *writer, mc_player_session *session,
                                        const char *directory, mc_client_stats_classes *classes,
                                        mc_java_files *files, mc_java_file *parent,
                                        bool source_mode, mc_client_stats_report *out_report,
                                        mc_client_stats_error *out_error) {
    mc_client_stats_error error;
    stats_error_clear(&error);
    mc_client_stats_report report = {0};
    stats_source_error_scope source_diagnostic;
    if (source_mode)
        stats_source_scope_open(&source_diagnostic, &error);
    if (stats_enter(writer, &error)) {
        if (writer->phase != MC_CLIENT_STATS_RAW)
            stats_fail(&error, MC_CLIENT_STATS_INVALID_PHASE, "Stats constructor native phase",
                       NULL, NULL, NULL, 0);
        else if (session != NULL && !mc_player_session_registry_matches(session, writer->registry))
            stats_fail(&error, MC_CLIENT_STATS_INVALID_ARGUMENT,
                       "Stats constructor Session Registry identity", NULL, NULL, NULL, 0);
        else {
            writer->phase = MC_CLIENT_STATS_CONSTRUCTING;
            char *path = NULL;
            mc_player_session *temporary = NULL;
            if (source_mode &&
                stats_source_begin(writer, classes, files, parent, &error) != MC_CLIENT_STATS_OK)
                goto finish;
            if (session != NULL) {
                const mc_player_status ps = mc_player_session_retain(session);
                if (ps != MC_PLAYER_OK) {
                    error.session = ps;
                    stats_fail(&error, MC_CLIENT_STATS_SESSION_FAILED,
                               "Stats constructor native Session admission", NULL, NULL, NULL, 0);
                    goto finish;
                }
                temporary = session;
            }
            if (stats_map_make(writer->registry, writer->memory, &writer->options, &writer->total,
                               &error) != MC_CLIENT_STATS_OK)
                goto finish;
            writer->pc = 12;
            if (stats_map_make(writer->registry, writer->memory, &writer->options, &writer->unsent,
                               &error) != MC_CLIENT_STATS_OK)
                goto finish;
            writer->pc = 23;
            writer->dirty = false;
            writer->pc = 28;
            if ((source_mode ? stats_source_directory(writer, &report, &error)
                             : stats_directory(writer, directory, &path, &report, &error)) !=
                MC_CLIENT_STATS_OK)
                goto finish;
            writer->retained_syncher =
                stats_alloc(writer->memory, sizeof(*writer->retained_syncher), &error);
            if (writer->retained_syncher == NULL) {
                if (source_mode) {
                    error.class_name = "xi";
                    error.method = "<init>";
                    error.descriptor = "(Lgr;Ljava/io/File;)V";
                    error.bytecode = 175;
                }
                goto finish;
            }
            *writer->retained_syncher = (stats_syncher){.pc = 26};
            if ((source_mode
                     ? stats_source_make_files(writer, session, &error)
                     : stats_make_files(writer, session, path, &error)) != MC_CLIENT_STATS_OK) {
                if (source_mode)
                    stats_source_frame(&error, "xi", "<init>", "(Lgr;Ljava/io/File;)V", 182);
                goto finish;
            }
            writer->retained_syncher->pc = 516;
            writer->retained_syncher->session = temporary;
            temporary = NULL;
            writer->retained_syncher->pc = 521;
            const bool unsent_exists = stats_owner_file_exists(writer, 0, &error);
            if (error.status != MC_CLIENT_STATS_OK) {
                if (source_mode) {
                    error.class_name = "cj";
                    error.method = "<init>";
                    error.descriptor = "(Lgr;Lxi;Ljava/io/File;)V";
                    error.bytecode = 528;
                    stats_source_frame(&error, "xi", "<init>", "(Lgr;Ljava/io/File;)V", 182);
                }
                goto finish;
            }
            if (unsent_exists) {
                mc_client_stats_map *map = NULL;
                if (stats_read_file_set(writer, 0, &map, &report, &error) != MC_CLIENT_STATS_OK) {
                    if (source_mode) {
                        stats_source_frame(&error, "cj", "<init>", "(Lgr;Lxi;Ljava/io/File;)V",
                                           548);
                        stats_source_frame(&error, "xi", "<init>", "(Lgr;Ljava/io/File;)V", 182);
                    }
                    goto finish;
                }
                const mc_client_stats_status status = stats_apply_map(writer, map, 0, &error);
                mc_client_stats_map_retire(&map);
                if (status != MC_CLIENT_STATS_OK) {
                    if (source_mode) {
                        stats_source_frame(&error, "cj", "<init>", "(Lgr;Lxi;Ljava/io/File;)V",
                                           551);
                        stats_source_frame(&error, "xi", "<init>", "(Lgr;Ljava/io/File;)V", 182);
                    }
                    goto finish;
                }
            }
            if (stats_start_worker(writer, false, NULL, &error) != MC_CLIENT_STATS_OK) {
                if (source_mode) {
                    stats_source_frame(&error, "cj", "<init>", "(Lgr;Lxi;Ljava/io/File;)V", 555);
                    stats_source_frame(&error, "xi", "<init>", "(Lgr;Ljava/io/File;)V", 182);
                }
                goto finish;
            }
            writer->retained_syncher->pc = 558;
            writer->syncher = writer->retained_syncher;
            writer->pc = 185;
        finish:
            stats_free(path);
            mc_player_session_retire(&temporary);
            writer->phase = error.status == MC_CLIENT_STATS_OK
                                ? MC_CLIENT_STATS_READY
                                : MC_CLIENT_STATS_FAILED_CONSTRUCTION;
            if (source_mode && writer->memory->source != NULL && error.status != MC_CLIENT_STATS_OK)
                stats_source_publish_failure(writer, &error);
        }
        stats_leave(writer);
    }
    if (out_report != NULL)
        *out_report = report;
    if (out_error != NULL)
        *out_error = error;
    if (source_mode)
        stats_source_scope_close(&source_diagnostic);
    return error.status;
}

mc_client_stats_status mc_client_stats_writer_construct(mc_client_stats_writer *writer,
                                                        mc_player_session *session,
                                                        const char *directory,
                                                        mc_client_stats_report *report,
                                                        mc_client_stats_error *error) {
    return construct(writer, session, directory, NULL, NULL, NULL, false, report, error);
}

mc_client_stats_status mc_client_stats_writer_construct_file(
    mc_client_stats_writer *writer, mc_client_stats_classes *classes, mc_player_session *session,
    mc_java_files *files, mc_java_file *parent, mc_client_stats_report *report,
    mc_client_stats_error *error) {
    return construct(writer, session, NULL, classes, files, parent, true, report, error);
}

bool mc_client_stats_writer_read_file_state(mc_client_stats_writer *writer,
                                            mc_client_stats_file_state *out) {
    if (writer == NULL || out == NULL || mtx_lock(&writer->mutex) != thrd_success)
        return false;
    stats_source *source = writer->memory->source;
    if (source == NULL || writer->entered || writer->workers_started != writer->workers_joined) {
        (void)mtx_unlock(&writer->mutex);
        return false;
    }
    mc_client_stats_file_state result = {
        .classes = source->classes,
        .files = source->files,
        .parent = source->parent,
        .directory = source->directory,
        .pending_file = source->pending_file,
        .listing = source->listing,
        .listing_index = source->listing_index,
        .pending_name = source->pending_name,
        .pending_builder = source->builder != NULL,
        .pending_reader = source->current_reader != NULL && source->current_reader->reader != NULL,
        .pending_stream =
            source->current_reader == NULL ? NULL : source->current_reader->input.file,
        .retiring = source->retiring,
        .failure = &source->failure,
        .cleanup_failure = &source->cleanup_failure};
    for (size_t i = 0; i < 6; ++i)
        result.children[i] = source->children[i];
    const int unlocked = mtx_unlock(&writer->mutex);
    if (unlocked != thrd_success)
        return false;
    *out = result;
    return true;
}

bool mc_client_stats_writer_read_state(mc_client_stats_writer *writer, mc_client_stats_state *out) {
    if (writer == NULL || out == NULL)
        return false;
    (void)mtx_lock(&writer->mutex);
    stats_syncher *syncher = writer->retained_syncher;
    *out = (mc_client_stats_state){.phase = writer->phase,
                                   .writer_pc = writer->pc,
                                   .total_identity = writer->total,
                                   .unsent_identity = writer->unsent,
                                   .synchronizer_identity = writer->syncher,
                                   .total_count = mc_client_stats_map_size(writer->total),
                                   .unsent_count = mc_client_stats_map_size(writer->unsent),
                                   .dirty = writer->dirty,
                                   .workers_started = writer->workers_started,
                                   .workers_completed = writer->workers_completed,
                                   .workers_joined = writer->workers_joined,
                                   .allocation_bytes = stats_memory_bytes(writer->memory),
                                   .last_worker_error = writer->last_worker_error,
                                   .last_worker_report = writer->last_worker_report};
    if (syncher != NULL) {
        out->synchronizer_pc = syncher->pc;
        out->busy = syncher->busy;
        out->pending_main_identity = syncher->pending_main;
        out->pending_other_identity = syncher->pending_other;
        out->delay = syncher->delay;
        out->other_delay = syncher->other_delay;
        out->session = syncher->session;
        for (size_t i = 0; i < 6; ++i) {
            if (writer->memory->source != NULL)
                out->file_identities[i] = writer->memory->source->children[i];
            else if (syncher->files[i] != NULL) {
                out->file_identities[i] = syncher->files[i];
                out->file_paths_utf8[i] = syncher->files[i]->path;
            }
        }
    }
    (void)mtx_unlock(&writer->mutex);
    return true;
}

static bool total_ready(mc_client_stats_writer *writer, mc_client_stats_error *error,
                        const char *descriptor) {
    if (writer->total != NULL)
        return true;
    stats_exception(error, MC_CLIENT_STATS_NULL_REFERENCE, "Stats source null total Map", "xi", "a",
                    descriptor, 5);
    return false;
}

mc_client_stats_status mc_client_stats_writer_contains(mc_client_stats_writer *writer,
                                                       const mc_statistic *key, bool *out,
                                                       mc_client_stats_error *out_error) {
    mc_client_stats_error error;
    stats_error_clear(&error);
    if (out == NULL)
        stats_fail(&error, MC_CLIENT_STATS_INVALID_ARGUMENT, "Stats contains native output", NULL,
                   NULL, NULL, 0);
    else if (stats_enter(writer, &error)) {
        if (total_ready(writer, &error, "(Lny;)Z")) {
            int32_t ignored;
            (void)mc_client_stats_map_get(writer->total, key, out, &ignored, &error);
        }
        stats_leave(writer);
    }
    if (out_error != NULL)
        *out_error = error;
    return error.status;
}

mc_client_stats_status mc_client_stats_writer_prerequisite(mc_client_stats_writer *writer,
                                                           const mc_statistic *key, bool *out,
                                                           mc_client_stats_error *out_error) {
    mc_client_stats_error error;
    stats_error_clear(&error);
    if (out == NULL)
        stats_fail(&error, MC_CLIENT_STATS_INVALID_ARGUMENT, "Stats prerequisite native output",
                   NULL, NULL, NULL, 0);
    else if (stats_enter(writer, &error)) {
        if (key == NULL)
            stats_exception(&error, MC_CLIENT_STATS_NULL_REFERENCE, "Stats source null achievement",
                            "xi", "b", "(Lny;)Z", 1);
        else if (!stats_statistic_valid(writer->registry, key) ||
                 mc_statistic_achievement_fields_get(key) == NULL)
            stats_fail(&error, MC_CLIENT_STATS_INVALID_ARGUMENT,
                       "Stats prerequisite achievement namespace/type", NULL, NULL, NULL, 0);
        else {
            const mc_statistic *parent = mc_statistic_achievement_fields_get(key)->c;
            if (parent == NULL)
                *out = true;
            else if (total_ready(writer, &error, "(Lny;)Z")) {
                int32_t ignored;
                (void)mc_client_stats_map_get(writer->total, parent, out, &ignored, &error);
            }
        }
        stats_leave(writer);
    }
    if (out_error != NULL)
        *out_error = error;
    return error.status;
}

mc_client_stats_status mc_client_stats_writer_value(mc_client_stats_writer *writer,
                                                    const mc_statistic *key, int32_t *out,
                                                    mc_client_stats_error *out_error) {
    mc_client_stats_error error;
    stats_error_clear(&error);
    if (out == NULL)
        stats_fail(&error, MC_CLIENT_STATS_INVALID_ARGUMENT, "Stats value native output", NULL,
                   NULL, NULL, 0);
    else if (stats_enter(writer, &error)) {
        if (total_ready(writer, &error, "(Lvr;)I")) {
            bool ignored;
            (void)mc_client_stats_map_get(writer->total, key, &ignored, out, &error);
        }
        stats_leave(writer);
    }
    if (out_error != NULL)
        *out_error = error;
    return error.status;
}

mc_client_stats_status mc_client_stats_writer_increment(mc_client_stats_writer *writer,
                                                        const mc_statistic *key, int32_t value,
                                                        mc_client_stats_error *out_error) {
    mc_client_stats_error error;
    stats_error_clear(&error);
    if (stats_enter(writer, &error)) {
        if (writer->unsent == NULL)
            stats_exception(&error, MC_CLIENT_STATS_NULL_REFERENCE, "Stats source null unsent Map",
                            "xi", "a", "(Ljava/util/Map;Lvr;I)V", 2);
        else if (stats_map_add(writer->unsent, key, value, &error) == MC_CLIENT_STATS_OK) {
            if (writer->total == NULL)
                stats_exception(&error, MC_CLIENT_STATS_NULL_REFERENCE,
                                "Stats source null total Map", "xi", "a", "(Ljava/util/Map;Lvr;I)V",
                                2);
            else if (stats_map_add(writer->total, key, value, &error) == MC_CLIENT_STATS_OK)
                writer->dirty = true;
        }
        stats_leave(writer);
    }
    if (out_error != NULL)
        *out_error = error;
    return error.status;
}

mc_client_stats_status mc_client_stats_writer_copy_unsent(mc_client_stats_writer *writer,
                                                          mc_client_stats_map **out,
                                                          mc_client_stats_error *out_error) {
    mc_client_stats_error error;
    stats_error_clear(&error);
    if (out == NULL)
        stats_fail(&error, MC_CLIENT_STATS_INVALID_ARGUMENT, "Stats clone native output", NULL,
                   NULL, NULL, 0);
    else if (stats_enter(writer, &error)) {
        if (writer->unsent == NULL)
            stats_exception(&error, MC_CLIENT_STATS_NULL_REFERENCE, "Stats source null clone Map",
                            "xi", "a", "()Ljava/util/Map;", 8);
        else
            (void)stats_map_clone(writer->unsent, out, &error);
        stats_leave(writer);
    }
    if (out_error != NULL)
        *out_error = error;
    return error.status;
}

static mc_client_stats_status service_pending(mc_client_stats_writer *writer, bool other,
                                              mc_client_stats_error *error) {
    stats_syncher *syncher = writer->syncher;
    mc_client_stats_map **field = other ? &syncher->pending_other : &syncher->pending_main;
    const bool present = *field != NULL; /* Source volatile null test. */
    if (!present)
        return MC_CLIENT_STATS_OK;
    (void)mtx_unlock(&writer->mutex);
    (void)mtx_lock(&writer->mutex);
    mc_client_stats_map *argument = *field; /* Distinct source argument reload. */
    if (argument != NULL && mc_client_stats_map_retain(argument, error) != MC_CLIENT_STATS_OK)
        return error->status;
    const mc_client_stats_status status = stats_apply_map(writer, argument, other ? 2 : 1, error);
    if (status == MC_CLIENT_STATS_OK) {
        (void)mtx_unlock(&writer->mutex);
        (void)mtx_lock(&writer->mutex);
        /* Clear the actual current field only after successful application,
         * preserving the source store even when another worker replaced it. */
        mc_client_stats_map_retire(field);
    }
    mc_client_stats_map_retire(&argument);
    return status;
}

mc_client_stats_status mc_client_stats_writer_service(mc_client_stats_writer *writer,
                                                      mc_client_stats_report *out_report,
                                                      mc_client_stats_error *out_error) {
    mc_client_stats_error error;
    stats_error_clear(&error);
    mc_client_stats_report report = {0};
    if (stats_enter(writer, &error)) {
        stats_syncher *syncher = writer->syncher;
        if (syncher == NULL)
            stats_exception(&error, MC_CLIENT_STATS_NULL_REFERENCE,
                            "Stats source null synchronizer", "xi", "d", "()V",
                            writer->dirty ? 11 : 32);
        else {
            if (writer->dirty && syncher->delay <= 0 && !syncher->busy &&
                syncher->pending_other == NULL) {
                mc_client_stats_map *copy = NULL;
                if (stats_map_clone(writer->unsent, &copy, &error) == MC_CLIENT_STATS_OK)
                    (void)stats_start_worker(writer, true, copy, &error);
                mc_client_stats_map_retire(&copy);
            }
            if (error.status == MC_CLIENT_STATS_OK) {
                if (syncher->delay > 0)
                    --syncher->delay;
                if (syncher->other_delay > 0)
                    --syncher->other_delay;
                (void)service_pending(writer, true, &error);
                if (error.status == MC_CLIENT_STATS_OK)
                    (void)service_pending(writer, false, &error);
            }
        }
        stats_leave(writer);
    }
    if (out_report != NULL)
        *out_report = report;
    if (out_error != NULL)
        *out_error = error;
    return error.status;
}

mc_client_stats_status mc_client_stats_writer_start_read(mc_client_stats_writer *writer,
                                                         mc_client_stats_error *out_error) {
    mc_client_stats_error error;
    stats_error_clear(&error);
    if (stats_enter(writer, &error)) {
        if (writer->syncher == NULL)
            stats_fail(&error, MC_CLIENT_STATS_INVALID_PHASE, "Stats refresh requires published CJ",
                       NULL, NULL, NULL, 0);
        else
            (void)stats_start_worker(writer, false, NULL, &error);
        stats_leave(writer);
    }
    if (out_error != NULL)
        *out_error = error;
    return error.status;
}

mc_client_stats_status mc_client_stats_writer_save(mc_client_stats_writer *writer,
                                                   mc_client_stats_report *out_report,
                                                   mc_client_stats_error *out_error) {
    mc_client_stats_error error;
    stats_error_clear(&error);
    mc_client_stats_report report = {0};
    if (stats_enter(writer, &error)) {
        const bool source_mode = writer->memory->source != NULL;
        stats_source_error_scope source_diagnostic;
        if (source_mode)
            stats_source_scope_open(&source_diagnostic, &error);
        {
            mc_client_stats_map *copy = NULL;
            if (writer->unsent == NULL)
                stats_exception(&error, MC_CLIENT_STATS_NULL_REFERENCE,
                                "Stats source null clone Map", "xi", "a", "()Ljava/util/Map;", 8);
            else if (stats_map_clone(writer->unsent, &copy, &error) == MC_CLIENT_STATS_OK) {
                if (writer->syncher == NULL) {
                    stats_exception(&error, MC_CLIENT_STATS_NULL_REFERENCE,
                                    "Stats source null synchronizer", "xi", "c", "()V", 8);
                    goto saved;
                }
                int countdown = 30;
                while (writer->syncher->busy && --countdown > 0) {
                    (void)mtx_unlock(&writer->mutex);
                    struct timespec duration = {0, 100000000L}, remaining = {0};
                    int slept;
                    do {
                        slept = thrd_sleep(&duration, &remaining);
                        duration = remaining;
                    } while (slept == -1); /* OS signals are not Java Thread.interrupt. */
                    (void)mtx_lock(&writer->mutex);
                    if (slept != 0) {
                        error.system_error = slept;
                        stats_fail(&error, MC_CLIENT_STATS_THREAD_FAILED,
                                   "Stats native sleep failure", "cj", "b", "(Ljava/util/Map;)V",
                                   20);
                    }
                    if (error.status != MC_CLIENT_STATS_OK)
                        break;
                }
                if (error.status == MC_CLIENT_STATS_OK) {
                    writer->syncher->busy = true;
                    (void)mtx_unlock(&writer->mutex);
                    mc_client_stats_status status =
                        stats_write_file_set(writer, copy, 0, &report, &error);
                    if (status == MC_CLIENT_STATS_SOURCE_EXCEPTION)
                        status = stats_catch(writer, &report, &error);
                    (void)status;
                    (void)mtx_lock(&writer->mutex);
                    if (writer->memory->source == NULL || error.status == MC_CLIENT_STATS_OK)
                        writer->syncher->busy = false;
                    else {
                        stats_source_frame(&error, "cj", "b", "(Ljava/util/Map;)V", 53);
                        stats_source_publish_failure(writer, &error);
                    }
                }
            }
        saved:
            mc_client_stats_map_retire(&copy);
        }
        if (source_mode)
            stats_source_scope_close(&source_diagnostic);
        stats_leave(writer);
    }
    if (out_report != NULL)
        *out_report = report;
    if (out_error != NULL)
        *out_error = error;
    return error.status;
}
