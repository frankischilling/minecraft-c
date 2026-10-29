#include "mc/world/storage/java_file_writer.h"

#include <stdlib.h>
#include <string.h>

struct mc_java_typed_file_writer {
    mc_java_typed_file_writer_options options;
    mc_java_files *files; /* Borrowed through RAW and pending constructor states. */
    mc_java_file_output *output;
    mc_java_writer *writer;
    mc_java_typed_file_writer_phase phase;
    bool active, construct_entered, output_returned, writer_returned, retiring_callback;
    bool writer_state_present;
    mc_java_writer_state writer_state;
    mc_java_output_error last_output, first_output_failure;
    mc_java_typed_file_writer_error failure, last_error, cleanup;
    mc_java_output_error call_first_failure, call_native_failure;
    bool restored_native_failure;
};

static mc_java_writer_status output_status(mc_java_output_status status) {
    switch (status) {
        case MC_JAVA_OUTPUT_OK:
            return MC_JAVA_WRITER_OK;
        case MC_JAVA_OUTPUT_BUSY:
            return MC_JAVA_WRITER_BUSY;
        case MC_JAVA_OUTPUT_LIMIT:
            return MC_JAVA_WRITER_LIMIT;
        case MC_JAVA_OUTPUT_ALLOCATION:
            return MC_JAVA_WRITER_ALLOCATION_FAILED;
        case MC_JAVA_OUTPUT_SOURCE_IO:
            return MC_JAVA_WRITER_IO_ERROR;
        default:
            /* Native policy/state/provider failures must not enter PrintWriter's
             * IOException swallowing path. The full typed cause remains below. */
            return MC_JAVA_WRITER_INVALID_ARGUMENT;
    }
}

static void remember_output(mc_java_typed_file_writer *owner, const mc_java_output_error *error) {
    owner->last_output = *error;
    if (error->status != MC_JAVA_OUTPUT_OK &&
        owner->first_output_failure.status == MC_JAVA_OUTPUT_OK)
        owner->first_output_failure = *error;
}

/* These records describe only the admitted current operation. Historical
 * first_output_failure remains a diagnostic and never restores a status. */
static void begin_callbacks(mc_java_typed_file_writer *owner) {
    owner->call_first_failure = (mc_java_output_error){0};
    owner->call_native_failure = (mc_java_output_error){0};
    owner->restored_native_failure = false;
}

/* The unchanged legacy Writer accepts only OK and IO callback statuses. Restore
 * a typed native result only when that core selected its normalized callback
 * contract failure from THIS call. In particular, a selected/swallowed primary
 * IO result wins over a later native close refusal. */
static mc_java_writer_status selected_callback_result(mc_java_typed_file_writer *owner,
                                                      mc_java_writer_status status,
                                                      mc_java_writer_error *writer) {
    if (status != MC_JAVA_WRITER_INVALID_ARGUMENT || writer->operation == NULL ||
        strcmp(writer->operation, "Writer output callback contract") != 0 ||
        owner->call_native_failure.status == MC_JAVA_OUTPUT_OK)
        return status;
    const mc_java_output_error *native = &owner->call_native_failure;
    const mc_java_writer_status mapped = output_status(native->status);
    owner->restored_native_failure = true;
    *writer = (mc_java_writer_error){.status = mapped,
                                     .system_error = native->cause.system_error,
                                     .operation = native->cause.operation};
    return mapped;
}

static mc_java_writer_status callback_result(mc_java_typed_file_writer *owner,
                                             mc_java_output_status status,
                                             mc_java_output_error output,
                                             mc_java_writer_error *error) {
    remember_output(owner, &output);
    if (status != MC_JAVA_OUTPUT_OK) {
        if (owner->call_first_failure.status == MC_JAVA_OUTPUT_OK)
            owner->call_first_failure = output;
        if (status != MC_JAVA_OUTPUT_SOURCE_IO &&
            owner->call_native_failure.status == MC_JAVA_OUTPUT_OK)
            owner->call_native_failure = output;
    }
    const mc_java_writer_status mapped = output_status(status);
    if (error != NULL)
        *error = (mc_java_writer_error){.status = mapped,
                                        .system_error = output.cause.system_error,
                                        .operation = output.cause.operation};
    return mapped;
}

static mc_java_writer_status write_bytes(void *context, const uint8_t *bytes, size_t length,
                                         size_t *committed, mc_java_writer_error *error) {
    mc_java_typed_file_writer *owner = context;
    mc_java_output_error output = {0};
    const mc_java_output_status status =
        mc_java_file_output_write_span(owner->output, bytes, length, &output);
    *committed = output.committed;
    return callback_result(owner, status, output, error);
}

static mc_java_writer_status flush_bytes(void *context, mc_java_writer_error *error) {
    mc_java_typed_file_writer *owner = context;
    mc_java_output_error output = {0};
    const mc_java_output_status status = mc_java_file_output_flush(owner->output, &output);
    return callback_result(owner, status, output, error);
}

static mc_java_writer_status close_bytes(void *context, mc_java_writer_error *error) {
    mc_java_typed_file_writer *owner = context;
    mc_java_output_error output = {0};
    const mc_java_output_status status =
        owner->retiring_callback ? mc_java_file_output_release(&owner->output, &output)
                                 : mc_java_file_output_source_close(owner->output, &output);
    return callback_result(owner, status, output, error);
}

static void snapshot_writer(mc_java_typed_file_writer *owner) {
    mc_java_writer_state state;
    if (owner->writer != NULL && mc_java_writer_read_state(owner->writer, &state)) {
        owner->writer_state = state;
        owner->writer_state_present = true;
    }
}

static mc_java_writer_status finish(mc_java_typed_file_writer *owner, mc_java_writer_status status,
                                    mc_java_writer_error writer, bool source_exception,
                                    mc_java_writer_retirement retirement,
                                    mc_java_typed_file_writer_error *out) {
    mc_java_typed_file_writer_error error = {0};
    error.status = status;
    error.writer = writer;
    error.retirement = retirement;
    error.output = owner->restored_native_failure                   ? owner->call_native_failure
                   : owner->last_output.status != MC_JAVA_OUTPUT_OK ? owner->last_output
                                                                    : owner->first_output_failure;
    error.operation = writer.operation != NULL ? writer.operation : error.output.cause.operation;
    error.source_exception = source_exception;
    owner->last_error = error;
    owner->active = false;
    if (out != NULL)
        *out = error;
    return status;
}

static mc_java_writer_status argument(mc_java_typed_file_writer_error *out,
                                      mc_java_writer_status status, const char *operation) {
    if (out != NULL)
        *out =
            (mc_java_typed_file_writer_error){.status = status,
                                              .operation = operation,
                                              .writer = {.status = status, .operation = operation}};
    return status;
}

static mc_java_writer_status enter(mc_java_typed_file_writer *owner,
                                   mc_java_typed_file_writer_error *error) {
    if (owner == NULL)
        return argument(error, MC_JAVA_WRITER_INVALID_ARGUMENT, "typed FileWriter owner");
    if (owner->active || owner->phase == MC_JAVA_TYPED_FILE_WRITER_CONSTRUCTING)
        return argument(error, MC_JAVA_WRITER_BUSY, "typed FileWriter active");
    if (owner->phase != MC_JAVA_TYPED_FILE_WRITER_READY || owner->writer == NULL)
        return argument(error, MC_JAVA_WRITER_INVALID_ARGUMENT, "typed FileWriter not ready");
    owner->active = true;
    return MC_JAVA_WRITER_OK;
}

mc_java_typed_file_writer_options mc_java_typed_file_writer_default_options(void) {
    return (mc_java_typed_file_writer_options){64u * 1024u, mc_java_output_default_options()};
}

mc_java_writer_status mc_java_typed_file_writer_allocate(
    mc_java_files *files, const mc_java_typed_file_writer_options *supplied,
    mc_java_typed_file_writer **out, mc_java_typed_file_writer_error *error) {
    if (files == NULL || out == NULL)
        return argument(error, MC_JAVA_WRITER_INVALID_ARGUMENT, "typed FileWriter allocation");
    const mc_java_typed_file_writer_options options =
        supplied == NULL ? mc_java_typed_file_writer_default_options() : *supplied;
    if (sizeof(mc_java_typed_file_writer) > options.max_owner_bytes)
        return argument(error, MC_JAVA_WRITER_LIMIT, "typed FileWriter native owner bound");
    mc_java_typed_file_writer *owner = calloc(1, sizeof(*owner));
    if (owner == NULL)
        return argument(error, MC_JAVA_WRITER_ALLOCATION_FAILED, "typed FileWriter allocation");
    owner->files = files;
    owner->options = options;
    owner->phase = MC_JAVA_TYPED_FILE_WRITER_RAW;
    *out = owner;
    return argument(error, MC_JAVA_WRITER_OK, NULL);
}

mc_java_writer_status
mc_java_typed_file_writer_construct(mc_java_typed_file_writer *owner, mc_java_file *file,
                                    const mc_java_writer_profile *supplied_profile,
                                    mc_java_typed_file_writer_error *out) {
    if (owner == NULL)
        return argument(out, MC_JAVA_WRITER_INVALID_ARGUMENT, "typed FileWriter constructor");
    if (owner->active || owner->phase == MC_JAVA_TYPED_FILE_WRITER_CONSTRUCTING)
        return argument(out, MC_JAVA_WRITER_BUSY, "typed FileWriter constructor active");
    if (owner->phase != MC_JAVA_TYPED_FILE_WRITER_RAW)
        return argument(out, MC_JAVA_WRITER_INVALID_ARGUMENT, "typed FileWriter constructor phase");
    owner->active = true;
    owner->construct_entered = true;
    owner->phase = MC_JAVA_TYPED_FILE_WRITER_CONSTRUCTING;
    mc_java_output_error output = {0};
    mc_java_output_status opened = mc_java_file_output_allocate(
        owner->files, &owner->options.file_output, &owner->output, &output);
    if (opened == MC_JAVA_OUTPUT_OK) {
        opened = mc_java_file_output_construct(owner->output, file, &output);
        if (opened != MC_JAVA_OUTPUT_OK && output.caller_count < 4)
            output.callers[output.caller_count++] =
                (mc_java_output_site){"java/io/FileWriter", "<init>", "(Ljava/io/File;)V", 6};
    }
    remember_output(owner, &output);
    mc_java_writer_error writer = {0};
    mc_java_writer_status status = output_status(opened);
    if (opened == MC_JAVA_OUTPUT_OK) {
        owner->output_returned = true;
        /* FileWriter(File) FOS constructor6 completed before OWS constructor9;
         * default charset and encoder creation occur only in that later call. */
        mc_java_writer_profile profile = {.charset = MC_JAVA_READER_DEFAULT,
                                          .mode = MC_JAVA_WRITER_PRINT,
#ifdef _WIN32
                                          .lines = MC_JAVA_WRITER_CRLF
#else
                                          .lines = MC_JAVA_WRITER_LF
#endif
        };
        if (supplied_profile != NULL)
            profile = *supplied_profile;
        if (profile.mode != MC_JAVA_WRITER_PRINT) {
            writer = (mc_java_writer_error){.status = MC_JAVA_WRITER_INVALID_ARGUMENT,
                                            .operation =
                                                "typed FileWriter requires PrintWriter profile"};
            status = writer.status;
        } else {
            const mc_java_writer_output sink = {owner, write_bytes, flush_bytes, close_bytes};
            status = mc_java_writer_create(&sink, &profile, &owner->writer, &writer);
        }
        owner->writer_returned = status == MC_JAVA_WRITER_OK;
    }
    owner->phase = status == MC_JAVA_WRITER_OK ? MC_JAVA_TYPED_FILE_WRITER_READY
                                               : MC_JAVA_TYPED_FILE_WRITER_FAILED;
    snapshot_writer(owner);
    finish(owner, status, writer, opened != MC_JAVA_OUTPUT_OK && output.cause.source_exception,
           (mc_java_writer_retirement){0}, &owner->failure);
    if (out != NULL)
        *out = owner->failure;
    return status;
}

static mc_java_writer_status text_call(mc_java_typed_file_writer *owner,
                                       const mc_java_properties_string_view *text, bool newline,
                                       mc_java_typed_file_writer_error *out) {
    mc_java_writer_status status = enter(owner, out);
    if (status != MC_JAVA_WRITER_OK)
        return status;
    begin_callbacks(owner);
    mc_java_writer_error writer = {0};
    status = newline ? mc_java_writer_println(owner->writer, text, &writer)
                     : mc_java_writer_print(owner->writer, text, &writer);
    status = selected_callback_result(owner, status, &writer);
    snapshot_writer(owner);
    return finish(owner, status, writer, false, (mc_java_writer_retirement){0}, out);
}

mc_java_writer_status mc_java_typed_file_writer_print(mc_java_typed_file_writer *owner,
                                                      const mc_java_properties_string_view *text,
                                                      mc_java_typed_file_writer_error *out) {
    return text_call(owner, text, false, out);
}

mc_java_writer_status mc_java_typed_file_writer_println(mc_java_typed_file_writer *owner,
                                                        const mc_java_properties_string_view *text,
                                                        mc_java_typed_file_writer_error *out) {
    return text_call(owner, text, true, out);
}

mc_java_writer_status mc_java_typed_file_writer_flush(mc_java_typed_file_writer *owner,
                                                      mc_java_typed_file_writer_error *out) {
    mc_java_writer_status status = enter(owner, out);
    if (status != MC_JAVA_WRITER_OK)
        return status;
    begin_callbacks(owner);
    mc_java_writer_error writer = {0};
    status = mc_java_writer_flush(owner->writer, &writer);
    status = selected_callback_result(owner, status, &writer);
    snapshot_writer(owner);
    return finish(owner, status, writer, false, (mc_java_writer_retirement){0}, out);
}

mc_java_writer_status mc_java_typed_file_writer_source_close(mc_java_typed_file_writer *owner,
                                                             mc_java_typed_file_writer_error *out) {
    mc_java_writer_status status = enter(owner, out);
    if (status != MC_JAVA_WRITER_OK)
        return status;
    begin_callbacks(owner);
    mc_java_writer_error writer = {0};
    status = mc_java_writer_close(owner->writer, &writer);
    status = selected_callback_result(owner, status, &writer);
    snapshot_writer(owner);
    return finish(owner, status, writer, false, (mc_java_writer_retirement){0}, out);
}

bool mc_java_typed_file_writer_inspect(const mc_java_typed_file_writer *owner,
                                       mc_java_typed_file_writer_state *out) {
    if (owner == NULL || out == NULL || owner->active)
        return false;
    *out = (mc_java_typed_file_writer_state){.phase = owner->phase,
                                             .construct_entered = owner->construct_entered,
                                             .file_output_returned = owner->output_returned,
                                             .writer_returned = owner->writer_returned,
                                             .file_output = owner->output,
                                             .writer = owner->writer,
                                             .writer_state_present = owner->writer_state_present,
                                             .writer_state = owner->writer_state,
                                             .failure = &owner->failure,
                                             .last_error = &owner->last_error,
                                             .cleanup = &owner->cleanup,
                                             .first_output_failure = &owner->first_output_failure};
    if (owner->output != NULL)
        out->output_state_present = mc_java_file_output_inspect(owner->output, &out->output_state);
    if (owner->writer != NULL)
        out->writer_state_present = mc_java_writer_read_state(owner->writer, &out->writer_state);
    return true;
}

mc_java_writer_status mc_java_typed_file_writer_retire(mc_java_typed_file_writer **slot,
                                                       mc_java_typed_file_writer_error *out) {
    if (slot == NULL)
        return argument(out, MC_JAVA_WRITER_INVALID_ARGUMENT, "typed FileWriter retirement slot");
    mc_java_typed_file_writer *owner = *slot;
    if (owner == NULL)
        return argument(out, MC_JAVA_WRITER_OK, NULL);
    if (owner->active || owner->phase == MC_JAVA_TYPED_FILE_WRITER_CONSTRUCTING)
        return argument(out, MC_JAVA_WRITER_BUSY, "typed FileWriter retirement active");
    const mc_java_typed_file_writer_phase previous = owner->phase;
    owner->active = true;
    owner->phase = MC_JAVA_TYPED_FILE_WRITER_RETIRING;
    begin_callbacks(owner);
    mc_java_writer_error writer = {0};
    mc_java_writer_retirement retired = {0};
    mc_java_writer_status status = MC_JAVA_WRITER_OK;
    if (owner->writer != NULL) {
        snapshot_writer(owner);
        owner->retiring_callback = true;
        status = mc_java_writer_retire(&owner->writer, &retired, &writer);
        owner->retiring_callback = false;
        status = selected_callback_result(owner, status, &writer);
        if (owner->writer != NULL)
            owner->phase = previous; /* Unconsumed BUSY makes no retirement progress. */
        else if (retired.additional_sink_close && owner->writer_state_present)
            owner->writer_state.sink_close_attempted = true;
        if (status != MC_JAVA_WRITER_OK) {
            finish(owner, status, writer, false, retired, &owner->last_error);
            if (owner->cleanup.status == MC_JAVA_WRITER_OK)
                owner->cleanup = owner->last_error;
            if (out != NULL)
                *out = owner->last_error;
            return status;
        }
    }
    if (owner->output != NULL) {
        mc_java_output_error output = {0};
        const mc_java_output_status released = mc_java_file_output_release(&owner->output, &output);
        remember_output(owner, &output);
        status = output_status(released);
        if (status != MC_JAVA_WRITER_OK) {
            finish(owner, status, writer, false, retired, &owner->last_error);
            if (owner->cleanup.status == MC_JAVA_WRITER_OK)
                owner->cleanup = owner->last_error;
            if (out != NULL)
                *out = owner->last_error;
            return status;
        }
    }
    mc_java_typed_file_writer_error result;
    finish(owner, MC_JAVA_WRITER_OK, writer, false, retired, &result);
    free(owner);
    *slot = NULL;
    if (out != NULL)
        *out = result;
    return MC_JAVA_WRITER_OK;
}
