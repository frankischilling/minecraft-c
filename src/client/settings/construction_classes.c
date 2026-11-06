#include "construction_internal.h"

#include <stdlib.h>
#include <string.h>

static const size_t lengths[] = {4, 4, 4, 3};
static const mc_game_option_id table_options[] = {
    MC_GAME_OPTION_RENDER_DISTANCE, MC_GAME_OPTION_DIFFICULTY, MC_GAME_OPTION_GUI_SCALE,
    MC_GAME_OPTION_FRAMERATE_LIMIT};

mc_game_settings_status settings_source_error(mc_game_settings_source_error *error,
                                              mc_game_settings_status status, const char *operation,
                                              const char *class_name, const char *method,
                                              const char *descriptor, int32_t pc) {
    error->native.status = status;
    error->native.operation = operation;
    error->site = (mc_game_settings_source_site){class_name, method, descriptor, pc};
    return status;
}

static mc_game_settings_status class_lock(mc_game_settings_classes *classes,
                                          mc_game_settings_source_error *error) {
    if (classes == NULL)
        return settings_source_error(error, MC_GAME_SETTINGS_INVALID_ARGUMENT,
                                     "kv native class owner", NULL, NULL, NULL, -1);
    const int locked = mtx_lock(&classes->mutex);
    if (locked != thrd_success) {
        error->thread_error = locked;
        return settings_source_error(error, MC_GAME_SETTINGS_THREAD_ERROR,
                                     "kv native class mutex lock", NULL, NULL, NULL, -1);
    }
    return MC_GAME_SETTINGS_OK;
}

mc_game_settings_class_options mc_game_settings_class_default_options(void) {
    return (mc_game_settings_class_options){4u * 1048576u, 4, 4u * 1048576u};
}

mc_game_settings_status mc_game_settings_classes_create(
    mc_java_files *files, const mc_game_settings_profile *output_profile,
    const mc_game_settings_class_options *options, mc_game_settings_classes **out,
    mc_game_settings_source_error *error) {
    mc_game_settings_source_error failure = {0};
    if (error != NULL)
        *error = failure;
    mc_game_settings_source_error *e = error == NULL ? &failure : error;
    mc_game_settings_profile selected = {MC_JAVA_READER_DEFAULT,
#ifdef _WIN32
                                         MC_JAVA_WRITER_CRLF
#else
                                         MC_JAVA_WRITER_LF
#endif
    };
    if (output_profile != NULL)
        selected = *output_profile;
    if (files == NULL || out == NULL || (unsigned int)selected.charset > MC_JAVA_READER_US_ASCII ||
        (selected.lines != MC_JAVA_WRITER_LF && selected.lines != MC_JAVA_WRITER_CRLF))
        return settings_source_error(e, MC_GAME_SETTINGS_INVALID_ARGUMENT, "kv native class inputs",
                                     NULL, NULL, NULL, -1);
    mc_game_settings_classes *classes = calloc(1, sizeof(*classes));
    if (classes == NULL)
        return settings_source_error(e, MC_GAME_SETTINGS_ALLOCATION_FAILED,
                                     "kv native class storage", NULL, NULL, NULL, -1);
    const int initialized = mtx_init(&classes->mutex, mtx_plain);
    if (initialized != thrd_success) {
        free(classes);
        e->thread_error = initialized;
        return settings_source_error(e, MC_GAME_SETTINGS_THREAD_ERROR, "kv native class mutex",
                                     NULL, NULL, NULL, -1);
    }
    if (mc_java_files_retain(files, &e->file) != MC_JAVA_FILE_OK) {
        mtx_destroy(&classes->mutex);
        free(classes);
        return settings_source_error(e, MC_GAME_SETTINGS_FILE_ERROR,
                                     "kv actual File namespace occurrence", NULL, NULL, NULL, -1);
    }
    classes->holds = 1;
    classes->files = files;
    classes->options = options == NULL ? mc_game_settings_class_default_options() : *options;
    classes->output_profile = selected;
    *out = classes;
    return MC_GAME_SETTINGS_OK;
}

static mc_game_settings_status literal(mc_game_settings_classes *classes, const char *ascii,
                                       mc_world_info_name **out,
                                       mc_game_settings_source_error *error, int32_t pc) {
    uint16_t units[64];
    const size_t length = strlen(ascii);
    if (length > sizeof(units) / sizeof(units[0]))
        return settings_source_error(error, MC_GAME_SETTINGS_LIMIT, "kv native literal bound", "kv",
                                     "<clinit>", "()V", pc);
    for (size_t index = 0; index < length; ++index)
        units[index] = (unsigned char)ascii[index];
    const mc_world_info_status status = mc_world_info_name_copy(
        (mc_nbt_string){units, length}, classes->options.maximum_string_bytes, out);
    if (status != MC_WORLD_INFO_OK)
        return settings_source_error(
            error,
            status == MC_WORLD_INFO_LIMIT ? MC_GAME_SETTINGS_LIMIT
                                          : MC_GAME_SETTINGS_ALLOCATION_FAILED,
            "kv actual ldc native String resolution", "kv", "<clinit>", "()V", pc);
    return MC_GAME_SETTINGS_OK;
}

mc_game_settings_status mc_game_settings_classes_active_use(mc_game_settings_classes *classes,
                                                            mc_game_settings_source_error *error) {
    mc_game_settings_source_error local = {0};
    mc_game_settings_source_error *e = error == NULL ? &local : error;
    *e = (mc_game_settings_source_error){0};
    if (class_lock(classes, e) != MC_GAME_SETTINGS_OK)
        return e->native.status;
    if (classes->phase == MC_GAME_SETTINGS_CLASS_READY) {
        (void)mtx_unlock(&classes->mutex);
        return MC_GAME_SETTINGS_OK;
    }
    if (classes->phase == MC_GAME_SETTINGS_CLASS_PHASE_ERRONEOUS) {
        *e = classes->failure;
        e->native.status = MC_GAME_SETTINGS_CLASS_ERRONEOUS;
        (void)mtx_unlock(&classes->mutex);
        return e->native.status;
    }
    if (classes->phase != MC_GAME_SETTINGS_CLASS_COLD) {
        (void)mtx_unlock(&classes->mutex);
        return settings_source_error(e, MC_GAME_SETTINGS_STATE_ERROR, "kv native class phase", NULL,
                                     NULL, NULL, -1);
    }
    classes->phase = MC_GAME_SETTINGS_CLASS_INITIALIZING;
    static const int32_t allocations[] = {1, 28, 55, 82};
    static const int32_t first_literals[] = {6, 33, 60, 87};
    static const unsigned int stores[] = {24, 51, 78, 100};
    for (size_t table = 0; table < 4; ++table) {
        const size_t bytes = lengths[table] * sizeof(*classes->pending_array);
        if (classes->arrays >= classes->options.maximum_arrays ||
            bytes > classes->options.maximum_array_bytes ||
            classes->array_bytes > classes->options.maximum_array_bytes - bytes) {
            settings_source_error(e, MC_GAME_SETTINGS_LIMIT, "kv actual array native limit", "kv",
                                  "<clinit>", "()V", allocations[table]);
            goto failed;
        }
        classes->pending_array = calloc(lengths[table], sizeof(*classes->pending_array));
        if (classes->pending_array == NULL) {
            settings_source_error(e, MC_GAME_SETTINGS_ALLOCATION_FAILED,
                                  "kv actual distinct String array allocation", "kv", "<clinit>",
                                  "()V", allocations[table]);
            goto failed;
        }
        classes->pending_length = lengths[table];
        ++classes->arrays;
        classes->array_bytes += bytes;
        for (size_t index = 0; index < lengths[table]; ++index) {
            const char *key = NULL;
            /* Read the existing immutable key facts. This is not a Source ht
             * active use, and no second translated-label table is installed. */
            if (mc_game_option_value_key(mc_game_option_by_id((int32_t)table_options[table]),
                                         (int32_t)index, &key) != MC_GAME_OPTION_OK) {
                settings_source_error(e, MC_GAME_SETTINGS_STATE_ERROR,
                                      "kv existing static key facts", NULL, NULL, NULL, -1);
                goto failed;
            }
            if (literal(classes, key, &classes->pending_array[index], e,
                        first_literals[table] + 5 * (int32_t)index) != MC_GAME_SETTINGS_OK)
                goto failed;
        }
        classes->tables[table] = classes->pending_array;
        classes->pending_array = NULL;
        classes->pending_length = 0;
        classes->last_store_pc = stores[table];
    }
    classes->phase = MC_GAME_SETTINGS_CLASS_READY;
    (void)mtx_unlock(&classes->mutex);
    return MC_GAME_SETTINGS_OK;
failed:
    classes->phase = MC_GAME_SETTINGS_CLASS_PHASE_ERRONEOUS;
    classes->failure = *e;
    (void)mtx_unlock(&classes->mutex);
    return e->native.status;
}

mc_game_settings_status mc_game_settings_classes_retain(mc_game_settings_classes *classes,
                                                        mc_game_settings_source_error *error) {
    mc_game_settings_source_error local = {0};
    mc_game_settings_source_error *e = error == NULL ? &local : error;
    *e = (mc_game_settings_source_error){0};
    if (class_lock(classes, e) != MC_GAME_SETTINGS_OK)
        return e->native.status;
    mc_game_settings_status status = MC_GAME_SETTINGS_OK;
    if (classes->phase == MC_GAME_SETTINGS_CLASS_RETIRING)
        status = MC_GAME_SETTINGS_STATE_ERROR;
    else if (classes->holds == SIZE_MAX)
        status = MC_GAME_SETTINGS_LIMIT;
    else
        ++classes->holds;
    (void)mtx_unlock(&classes->mutex);
    if (status != MC_GAME_SETTINGS_OK)
        settings_source_error(e, status, "kv native class occurrence admission", NULL, NULL, NULL,
                              -1);
    return status;
}

mc_game_settings_status settings_class_options_name(mc_game_settings_classes *classes,
                                                    mc_world_info_name **out,
                                                    mc_game_settings_source_error *error) {
    if (out == NULL)
        return settings_source_error(error, MC_GAME_SETTINGS_INVALID_ARGUMENT,
                                     "kv options literal output", NULL, NULL, NULL, -1);
    if (class_lock(classes, error) != MC_GAME_SETTINGS_OK)
        return error->native.status;
    if (classes->phase != MC_GAME_SETTINGS_CLASS_READY) {
        (void)mtx_unlock(&classes->mutex);
        return settings_source_error(error, MC_GAME_SETTINGS_STATE_ERROR,
                                     "kv options literal class phase", NULL, NULL, NULL, -1);
    }
    if (classes->options_literal == NULL &&
        literal(classes, "options.txt", &classes->options_literal, error, 366) !=
            MC_GAME_SETTINGS_OK) {
        error->site.method = "<init>";
        error->site.descriptor = "(Lnet/minecraft/client/Minecraft;Ljava/io/File;)V";
        (void)mtx_unlock(&classes->mutex);
        return error->native.status;
    }
    *out = classes->options_literal;
    (void)mtx_unlock(&classes->mutex);
    return MC_GAME_SETTINGS_OK;
}

mc_game_option_status settings_class_value_key(mc_game_settings_classes *classes,
                                               mc_game_option_id id, int32_t index,
                                               mc_game_settings_text *out) {
    if (classes == NULL || out == NULL || mtx_lock(&classes->mutex) != thrd_success)
        return MC_GAME_OPTION_INVALID_ARGUMENT;
    size_t table = 4;
    for (size_t item = 0; item < 4; ++item)
        if (table_options[item] == id)
            table = item;
    mc_game_option_status status = MC_GAME_OPTION_OK;
    if (classes->phase != MC_GAME_SETTINGS_CLASS_READY || table == 4)
        status = MC_GAME_OPTION_INVALID_ARGUMENT;
    else if (index < 0 || (size_t)index >= lengths[table])
        status = MC_GAME_OPTION_SOURCE_INDEX_OUT_OF_BOUNDS;
    else {
        const mc_nbt_string name = mc_world_info_name_text(classes->tables[table][(size_t)index]);
        *out = (mc_game_settings_text){name.units, name.length, false};
    }
    (void)mtx_unlock(&classes->mutex);
    return status;
}

static mc_java_writer_status stdout_write(void *context, const uint8_t *bytes, size_t length,
                                          size_t *committed, mc_java_writer_error *error) {
    mc_game_settings_classes *classes = context;
    bool failed = false;
    int native_error = 0;
    if (!classes->stdout_borrowed ||
        mc_text_stdout_write(&classes->stdout_stream, bytes, length, committed, &failed,
                             &native_error) != MC_TEXT_STDOUT_OK) {
        classes->native_stdout_failed = true;
        *error = (mc_java_writer_error){.status = MC_JAVA_WRITER_INVALID_ARGUMENT,
                                        .operation = "kv native stdout write control"};
        return error->status;
    }
    if (failed) {
        *error = (mc_java_writer_error){.status = MC_JAVA_WRITER_IO_ERROR,
                                        .system_error = native_error,
                                        .operation = "kv actual stdout descriptor write"};
        return error->status;
    }
    return MC_JAVA_WRITER_OK;
}

static mc_java_writer_status stdout_flush(void *context, mc_java_writer_error *error) {
    const mc_game_settings_classes *classes = context;
    if (classes->stdout_borrowed)
        return MC_JAVA_WRITER_OK; /* Unbuffered sink; encoder owns all pending bytes. */
    *error = (mc_java_writer_error){.status = MC_JAVA_WRITER_IO_ERROR,
                                    .operation = "kv native stdout borrow ended"};
    return error->status;
}

static mc_java_writer_status stdout_detach(void *context, mc_java_writer_error *error) {
    mc_game_settings_classes *classes = context;
    classes->stdout_borrowed = false; /* Native borrowed occurrence only, not System.out.close. */
    (void)error;
    return MC_JAVA_WRITER_OK;
}

mc_game_settings_status settings_class_emit(mc_game_settings_classes *classes,
                                            const mc_game_settings_diagnostic *event,
                                            mc_game_settings_source_error *error) {
    if (event == NULL || event->message == NULL ||
        (event->kind != MC_GAME_SETTINGS_BAD_OPTION && event->kind != MC_GAME_SETTINGS_LOAD_FAILED))
        return settings_source_error(error, MC_GAME_SETTINGS_INVALID_ARGUMENT,
                                     "kv actual load diagnostic inputs", NULL, NULL, NULL, -1);
    if (class_lock(classes, error) != MC_GAME_SETTINGS_OK)
        return error->native.status;
    if (classes->phase != MC_GAME_SETTINGS_CLASS_READY) {
        (void)mtx_unlock(&classes->mutex);
        return settings_source_error(error, MC_GAME_SETTINGS_STATE_ERROR,
                                     "kv native stdout live class", NULL, NULL, NULL, -1);
    }
    const int32_t pc = event->kind == MC_GAME_SETTINGS_BAD_OPTION ? 477 : 496;
    if (!classes->stdout_captured) {
        if (mc_text_stdout_capture(&classes->stdout_stream) != MC_TEXT_STDOUT_OK) {
            (void)mtx_unlock(&classes->mutex);
            return settings_source_error(error, MC_GAME_SETTINGS_NATIVE_IO_ERROR,
                                         "kv native default stdout capture", "kv", "a", "()V",
                                         event->kind == MC_GAME_SETTINGS_BAD_OPTION ? 455 : 491);
        }
        classes->stdout_captured = true;
        classes->stdout_borrowed = true;
    }
    if (classes->stdout_writer == NULL) {
        const mc_java_writer_output sink = {classes, stdout_write, stdout_flush, stdout_detach};
        const mc_java_writer_profile profile = {
            classes->output_profile.charset, MC_JAVA_WRITER_PRINT, classes->output_profile.lines};
        const mc_java_writer_status status =
            mc_java_writer_create(&sink, &profile, &classes->stdout_writer, &error->output);
        if (status != MC_JAVA_WRITER_OK) {
            (void)mtx_unlock(&classes->mutex);
            return settings_source_error(
                error,
                status == MC_JAVA_WRITER_UNSUPPORTED_CHARSET ? MC_GAME_SETTINGS_UNSUPPORTED_CHARSET
                                                             : MC_GAME_SETTINGS_ALLOCATION_FAILED,
                "kv native counted UTF16 stdout encoder", "kv", "a", "()V", pc);
        }
    }
    const size_t prefix_length = strlen(event->message);
    const size_t line_length = event->kind == MC_GAME_SETTINGS_BAD_OPTION ? event->line.length : 0;
    if (line_length > MC_GAME_SETTINGS_MAX_TEXT_UNITS || prefix_length > SIZE_MAX - line_length ||
        prefix_length + line_length > SIZE_MAX / sizeof(uint16_t) ||
        prefix_length + line_length > classes->options.maximum_string_bytes / sizeof(uint16_t)) {
        (void)mtx_unlock(&classes->mutex);
        return settings_source_error(error, MC_GAME_SETTINGS_LIMIT,
                                     "kv actual diagnostic String native bound", "kv", "a", "()V",
                                     pc);
    }
    const size_t length = prefix_length + line_length;
    uint16_t *units = malloc(length * sizeof(*units));
    if (units == NULL) {
        (void)mtx_unlock(&classes->mutex);
        return settings_source_error(error, MC_GAME_SETTINGS_ALLOCATION_FAILED,
                                     "kv actual diagnostic String native allocation", "kv", "a",
                                     "()V", event->kind == MC_GAME_SETTINGS_BAD_OPTION ? 458 : 494);
    }
    for (size_t index = 0; index < prefix_length; ++index)
        units[index] = (unsigned char)event->message[index];
    if (line_length != 0)
        memcpy(units + prefix_length, event->line.units, line_length * sizeof(*units));
    const mc_java_properties_string_view text = {units, length};
    error->output_reached = true;
    mc_java_writer_status status =
        mc_java_writer_println(classes->stdout_writer, &text, &error->output);
    if (status == MC_JAVA_WRITER_OK)
        status = mc_java_writer_flush(classes->stdout_writer, &error->output);
    (void)mc_java_writer_read_state(classes->stdout_writer, &error->output_state);
    error->native_stdout_failed = classes->native_stdout_failed;
    free(units);
    (void)mtx_unlock(&classes->mutex);
    if (status != MC_JAVA_WRITER_OK || error->native_stdout_failed)
        return settings_source_error(error, MC_GAME_SETTINGS_DIAGNOSTIC_FAILED,
                                     "kv actual counted UTF16 stdout output", "kv", "a", "()V", pc);
    error->output_completed = true;
    return MC_GAME_SETTINGS_OK;
}

bool mc_game_settings_classes_inspect(mc_game_settings_classes *classes,
                                      mc_game_settings_class_state *out) {
    if (classes == NULL || out == NULL || mtx_lock(&classes->mutex) != thrd_success)
        return false;
    *out = (mc_game_settings_class_state){.phase = classes->phase,
                                          .holds = classes->holds,
                                          .array_bytes = classes->array_bytes,
                                          .arrays = classes->arrays,
                                          .J = classes->tables[0],
                                          .K = classes->tables[1],
                                          .L = classes->tables[2],
                                          .M = classes->tables[3],
                                          .options_literal = classes->options_literal,
                                          .pending_array = classes->pending_array,
                                          .pending_length = classes->pending_length,
                                          .stdout_captured = classes->stdout_captured,
                                          .stdout_borrowed = classes->stdout_borrowed,
                                          .last_store_pc = classes->last_store_pc,
                                          .failure = &classes->failure,
                                          .retirement = &classes->retirement};
    if (classes->stdout_writer != NULL)
        (void)mc_java_writer_read_state(classes->stdout_writer, &out->stdout_state);
    (void)mtx_unlock(&classes->mutex);
    return true;
}

mc_game_settings_status mc_game_settings_classes_release(mc_game_settings_classes **slot,
                                                         mc_game_settings_source_error *error) {
    mc_game_settings_source_error local = {0};
    mc_game_settings_source_error *e = error == NULL ? &local : error;
    *e = (mc_game_settings_source_error){0};
    if (slot == NULL)
        return settings_source_error(e, MC_GAME_SETTINGS_INVALID_ARGUMENT,
                                     "kv native class release output", NULL, NULL, NULL, -1);
    mc_game_settings_classes *classes = *slot;
    if (classes == NULL)
        return MC_GAME_SETTINGS_OK;
    if (class_lock(classes, e) != MC_GAME_SETTINGS_OK)
        return e->native.status;
    if (classes->holds > 1) {
        --classes->holds;
        (void)mtx_unlock(&classes->mutex);
        *slot = NULL;
        return MC_GAME_SETTINGS_OK;
    }
    classes->phase = MC_GAME_SETTINGS_CLASS_RETIRING;
    (void)mtx_unlock(&classes->mutex);
    if (classes->stdout_writer != NULL &&
        mc_java_writer_retire(&classes->stdout_writer, NULL, &classes->retirement.output) !=
            MC_JAVA_WRITER_OK) {
        settings_source_error(&classes->retirement, MC_GAME_SETTINGS_DIAGNOSTIC_FAILED,
                              "kv native stdout encoder retirement", NULL, NULL, NULL, -1);
        *e = classes->retirement;
        return e->native.status;
    }
    if (mc_java_files_release(&classes->files, &classes->retirement.file) != MC_JAVA_FILE_OK) {
        settings_source_error(&classes->retirement,
                              classes->retirement.file.status == MC_JAVA_FILE_BUSY
                                  ? MC_GAME_SETTINGS_BUSY
                                  : MC_GAME_SETTINGS_FILE_ERROR,
                              "kv native File namespace retirement", NULL, NULL, NULL, -1);
        *e = classes->retirement;
        return e->native.status;
    }
    for (size_t table = 0; table < 4; ++table) {
        if (classes->tables[table] != NULL)
            for (size_t index = 0; index < lengths[table]; ++index)
                mc_world_info_name_release(classes->tables[table][index]);
        free(classes->tables[table]);
    }
    for (size_t index = 0; index < classes->pending_length; ++index)
        mc_world_info_name_release(classes->pending_array[index]);
    free(classes->pending_array);
    mc_world_info_name_release(classes->options_literal);
    mtx_destroy(&classes->mutex);
    free(classes);
    *slot = NULL;
    return MC_GAME_SETTINGS_OK;
}
