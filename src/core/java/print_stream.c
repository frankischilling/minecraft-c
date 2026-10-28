#include "print_stream_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

mc_java_print_status mc_ps_fail(mc_java_print_error *error, mc_java_print_status status,
                                const char *operation) {
    error->status = status;
    error->operation = operation;
    error->native_policy = status != MC_JAVA_PRINT_OK;
    return status;
}

static bool ps_lock(mc_java_print_stream *s, mc_java_print_error *error) {
    if (s == NULL) {
        mc_ps_fail(error, MC_JAVA_PRINT_INVALID_ARGUMENT, "null native PrintStream owner");
        return false;
    }
    if (mtx_trylock(&s->monitor) != thrd_success) {
        mc_ps_fail(error, MC_JAVA_PRINT_BUSY, "PrintStream source monitor unavailable");
        return false;
    }
    return true;
}

static bool ps_enter(mc_java_print_stream *s, mc_java_print_error *error) {
    if (!ps_lock(s, error))
        return false;
    if (s->active || s->phase != MC_JAVA_PRINT_READY) {
        mc_ps_fail(error, s->active ? MC_JAVA_PRINT_BUSY : MC_JAVA_PRINT_INVALID_PHASE,
                   "PrintStream source operation admission");
        mtx_unlock(&s->monitor);
        return false;
    }
    s->active = true;
    s->source_thread = thrd_current();
    s->nested_failure = false;
    return true;
}

static mc_java_print_status ps_leave(mc_java_print_stream *s, mc_java_print_error *error) {
    if (error->status != MC_JAVA_PRINT_OK) {
        s->failure = *error;
        s->phase = MC_JAVA_PRINT_FAILED;
    }
    s->active = false;
    mtx_unlock(&s->monitor);
    return error->status;
}

static void record_io(mc_java_print_stream *s, const mc_java_writer_error *error) {
    s->trouble = true;
    if (s->first_io.status == MC_JAVA_WRITER_OK)
        s->first_io = *error;
}

static mc_java_writer_status physical_write(mc_java_print_stream *s, const uint8_t *bytes,
                                            size_t length, mc_java_writer_error *error) {
    size_t physical = 0;
    bool known = false;
    const mc_java_writer_status result =
        s->native_write(s->native_context, &s->native, bytes, length, &physical, &known, error);
    if (!known || physical > SIZE_MAX - s->physical)
        s->physical_known = false;
    if (known && physical <= SIZE_MAX - s->physical)
        s->physical += physical;
    /* Production kernel completes the real byte[] write or throws. This is
     * not the legacy callback that accepts a successful short physical write. */
    if (result == MC_JAVA_WRITER_OK && (!known || physical != length)) {
        *error = (mc_java_writer_error){
            .status = MC_JAVA_WRITER_INVALID_ARGUMENT,
            .operation = "native stdout reported inconsistent normal byte write"};
        return error->status;
    }
    return result;
}

static mc_java_writer_status byte_flush(mc_java_print_stream *s, mc_java_writer_error *error) {
    if (s->byte_count == 0)
        return MC_JAVA_WRITER_OK;
    const mc_java_writer_status status = physical_write(s, s->bytes, s->byte_count, error);
    if (status == MC_JAVA_WRITER_OK)
        s->byte_count = 0; /* BufferedOutputStream count resets after normal write only. */
    return status;
}

static mc_java_writer_status byte_write(mc_java_print_stream *s, const uint8_t *bytes,
                                        size_t length, mc_java_writer_error *error) {
    mc_java_writer_status status;
    if (length >= sizeof(s->bytes)) {
        status = byte_flush(s, error);
        if (status != MC_JAVA_WRITER_OK)
            return status;
        return physical_write(s, bytes, length, error);
    }
    if (length > sizeof(s->bytes) - s->byte_count) {
        status = byte_flush(s, error);
        if (status != MC_JAVA_WRITER_OK)
            return status;
    }
    if (length != 0)
        memcpy(s->bytes + s->byte_count, bytes, length);
    s->byte_count += length;
    return MC_JAVA_WRITER_OK;
}

static bool nested_admitted(mc_java_print_stream *s) {
    return s->active && thrd_equal(s->source_thread, thrd_current()) != 0;
}

static mc_java_writer_status encoder_write(void *context, const uint8_t *bytes, size_t length,
                                           mc_java_source_write_result *result,
                                           mc_java_writer_error *error) {
    mc_java_print_stream *s = context;
    *result = (mc_java_source_write_result){0};
    if (!nested_admitted(s)) {
        *error = (mc_java_writer_error){
            .status = MC_JAVA_WRITER_BUSY,
            .operation = "PrintStream private source dispatch without held source monitor"};
        return error->status;
    }
    const size_t before = s->physical;
    const bool known_before = s->physical_known;
    s->nested_site = (mc_java_print_site){"java/io/PrintStream", "write", "([BII)V", 16};
    mc_java_writer_status status = byte_write(s, bytes, length, error);
    if (status == MC_JAVA_WRITER_OK && s->auto_flush) {
        s->nested_site.pc = 30;
        status = byte_flush(s, error); /* Actual PrintStream.write(byte[],off,len). */
    }
    result->physical_bytes_known = known_before && s->physical_known;
    if (result->physical_bytes_known)
        result->physical_bytes = s->physical - before;
    if (status == MC_JAVA_WRITER_IO_ERROR) {
        record_io(s, error);
        result->swallowed_io = true;
        result->swallowed_error = *error;
        result->source_returned = true;
        ++s->byte_returns;
        return MC_JAVA_WRITER_OK;
    }
    if (status == MC_JAVA_WRITER_INTERRUPTED_IO) {
        /* Source catch must invoke a real Java Thread owner. No such child
         * has been supplied to this finite base-stream namespace yet. */
        s->interrupted_thread_pending = true;
        s->interruption_in_write = true;
        s->nested_failure = true;
        return status;
    }
    if (status == MC_JAVA_WRITER_OK) {
        result->source_returned = true;
        ++s->byte_returns;
    } else
        s->nested_failure = true;
    return status;
}

static mc_java_writer_status encoder_flush(void *context, mc_java_writer_error *error) {
    mc_java_print_stream *s = context;
    if (!nested_admitted(s)) {
        *error = (mc_java_writer_error){
            .status = MC_JAVA_WRITER_BUSY,
            .operation = "PrintStream private flush without held source monitor"};
        return error->status;
    }
    const mc_java_writer_status status = byte_flush(s, error);
    if (status == MC_JAVA_WRITER_IO_ERROR) {
        record_io(s, error);
        return MC_JAVA_WRITER_OK;
    }
    return status;
}

static mc_java_writer_status encoder_detach(void *context, mc_java_writer_error *error) {
    mc_java_print_stream *s = context;
    if (s->phase != MC_JAVA_PRINT_RETIRING || !nested_admitted(s)) {
        *error = (mc_java_writer_error){
            .status = MC_JAVA_WRITER_BUSY,
            .operation = "PrintStream encoder detach outside final native retirement"};
        return error->status;
    }
    *error = (mc_java_writer_error){0};
    return MC_JAVA_WRITER_OK;
}

mc_java_print_status mc_java_print_stream_allocate(mc_java_print_stream **out,
                                                   mc_java_print_error *error) {
    if (error == NULL)
        return MC_JAVA_PRINT_INVALID_ARGUMENT;
    *error = (mc_java_print_error){0};
    if (out == NULL)
        return mc_ps_fail(error, MC_JAVA_PRINT_INVALID_ARGUMENT, "PrintStream allocation slot");
    mc_java_print_stream *s = calloc(1, sizeof(*s));
    if (s == NULL)
        return mc_ps_fail(error, MC_JAVA_PRINT_ALLOCATION_FAILED, "PrintStream native owner");
    if (mtx_init(&s->monitor, mtx_recursive) != thrd_success) {
        free(s);
        return mc_ps_fail(error, MC_JAVA_PRINT_NATIVE_FAILURE, "PrintStream recursive monitor");
    }
    s->holds = 1;
    s->physical_known = true;
    s->native_write = mc_ps_native_stdout_write;
    *out = s;
    return MC_JAVA_PRINT_OK;
}

static mc_java_print_status construct(mc_java_print_stream *s, mc_java_reader_charset charset,
                                      bool native_profile, bool auto_flush,
                                      mc_java_print_error *error) {
    if (!ps_lock(s, error))
        return error->status;
    if (s->active || s->phase != MC_JAVA_PRINT_RAW) {
        mc_ps_fail(error, s->active ? MC_JAVA_PRINT_BUSY : MC_JAVA_PRINT_INVALID_PHASE,
                   "PrintStream constructor phase");
        mtx_unlock(&s->monitor);
        return error->status;
    }
    s->phase = MC_JAVA_PRINT_CONSTRUCTING;
    s->active = true;
    s->source_thread = thrd_current();
    if (mc_text_stdout_capture(&s->native) != MC_TEXT_STDOUT_OK) {
        mc_ps_fail(error, MC_JAVA_PRINT_NATIVE_FAILURE, "capture actual stdout handle");
        return ps_leave(s, error);
    }
    if (native_profile) {
        if (mc_ps_native_profile(&s->profile, error) != MC_JAVA_PRINT_OK)
            return ps_leave(s, error);
    } else {
        if (charset < MC_JAVA_READER_UTF8 || charset > MC_JAVA_READER_US_ASCII) {
            mc_ps_fail(error, MC_JAVA_PRINT_UNSUPPORTED_CHARSET, "explicit Charset admission");
            return ps_leave(s, error);
        }
        s->profile.charset = charset;
    }
    /* Shared native representation of actual FilterOutputStream.out and the
     * default false trouble/closing fields, then autoFlush, then charOut,
     * then textOut. No pending child is silently discarded on failure. */
    s->out_present = true;
    s->auto_flush = auto_flush;
    const mc_java_source_encoder_output output = {s, encoder_write, encoder_flush, encoder_detach};
    if (mc_java_source_encoder_create(&output, s->profile.charset, &s->encoder, &error->encoder) !=
        MC_JAVA_WRITER_OK) {
        mc_ps_fail(error, MC_JAVA_PRINT_NATIVE_FAILURE, "PrintStream OutputStreamWriter child");
        return ps_leave(s, error);
    }
    s->text = malloc(8192u * sizeof(*s->text));
    if (s->text == NULL) {
        mc_ps_fail(error, MC_JAVA_PRINT_ALLOCATION_FAILED, "PrintStream BufferedWriter8192 child");
        return ps_leave(s, error);
    }
    s->phase = MC_JAVA_PRINT_READY;
    return ps_leave(s, error);
}

mc_java_print_status mc_java_print_stream_construct_stdout(mc_java_print_stream *s,
                                                           mc_java_print_error *error) {
    if (error == NULL)
        return MC_JAVA_PRINT_INVALID_ARGUMENT;
    *error = (mc_java_print_error){0};
    return construct(s, MC_JAVA_READER_DEFAULT, true, true, error);
}

mc_java_print_status mc_java_print_stream_construct_encoded_stdout(mc_java_print_stream *s,
                                                                   mc_java_reader_charset charset,
                                                                   bool auto_flush,
                                                                   mc_java_print_error *error) {
    if (error == NULL)
        return MC_JAVA_PRINT_INVALID_ARGUMENT;
    *error = (mc_java_print_error){0};
    return construct(s, charset, false, auto_flush, error);
}

mc_java_print_status mc_java_print_stream_retain(mc_java_print_stream *s,
                                                 mc_java_print_error *error) {
    if (error == NULL)
        return MC_JAVA_PRINT_INVALID_ARGUMENT;
    *error = (mc_java_print_error){0};
    if (!ps_lock(s, error))
        return error->status;
    if (s->phase == MC_JAVA_PRINT_RETIRING || s->holds == SIZE_MAX)
        mc_ps_fail(error, s->holds == SIZE_MAX ? MC_JAVA_PRINT_LIMIT : MC_JAVA_PRINT_INVALID_PHASE,
                   "PrintStream retain admission");
    else
        ++s->holds;
    mtx_unlock(&s->monitor);
    return error->status;
}

static mc_java_writer_status text_flush(mc_java_print_stream *s, mc_java_writer_error *error) {
    if (s->text_count == 0)
        return MC_JAVA_WRITER_OK;
    const mc_java_writer_status status =
        mc_java_source_encoder_write(s->encoder, s->text, s->text_count, error);
    if (status == MC_JAVA_WRITER_OK)
        s->text_count = 0;
    return status;
}

static mc_java_writer_status text_write(mc_java_print_stream *s, const uint16_t *units,
                                        size_t length, mc_java_writer_error *error) {
    while (length != 0) {
        size_t count = 8192u - s->text_count;
        if (count > length)
            count = length;
        memcpy(s->text + s->text_count, units, count * sizeof(*units));
        s->text_count += count;
        units += count;
        length -= count;
        if (s->text_count == 8192u) {
            const mc_java_writer_status status = text_flush(s, error);
            if (status != MC_JAVA_WRITER_OK)
                return status;
        }
    }
    return MC_JAVA_WRITER_OK;
}

static mc_java_print_status complete(mc_java_print_stream *s, mc_java_writer_status status,
                                     mc_java_print_error *error) {
    if (s->nested_failure) {
        error->frames[2] = s->nested_site;
        error->frame_count = 3;
        error->site = s->nested_site;
    }
    if (status == MC_JAVA_WRITER_IO_ERROR) {
        record_io(s, &error->encoder);
        error->status = MC_JAVA_PRINT_OK;
        error->source_returned = true;
    } else if (status == MC_JAVA_WRITER_INTERRUPTED_IO) {
        s->interrupted_thread_pending = true;
        error->interrupted_io = true;
        error->site = (mc_java_print_site){
            "java/io/PrintStream", s->interruption_in_write ? "write" : "writeln",
            s->interruption_in_write ? "([BII)V" : "(Ljava/lang/String;)V",
            s->interruption_in_write ? 52u : 65u};
        error->frames[s->interruption_in_write ? 2 : 1] = error->site;
        mc_ps_fail(error, MC_JAVA_PRINT_THREAD_UNIMPLEMENTED,
                   "reached Thread.currentThread/interrupt dependency");
    } else if (status != MC_JAVA_WRITER_OK) {
        mc_ps_fail(error,
                   status == MC_JAVA_WRITER_BUSY ? MC_JAVA_PRINT_BUSY
                                                 : MC_JAVA_PRINT_NATIVE_FAILURE,
                   "PrintStream reached native encoder/output failure");
    } else {
        error->source_returned = true;
    }
    return error->status;
}

mc_java_print_status mc_java_print_stream_println(mc_java_print_stream *s,
                                                  const mc_java_properties_string_view *text,
                                                  mc_java_print_error *error) {
    static const uint16_t null_units[] = {'n', 'u', 'l', 'l'};
#ifdef _WIN32
    static const uint16_t newline[] = {'\r', '\n'};
#else
    static const uint16_t newline[] = {'\n'};
#endif
    if (error == NULL)
        return MC_JAVA_PRINT_INVALID_ARGUMENT;
    *error = (mc_java_print_error){
        .site = {"java/io/PrintStream", "println", "(Ljava/lang/String;)V", 14}};
    if (text != NULL && ((text->length != 0 && text->units == NULL) || text->length > INT32_MAX))
        return mc_ps_fail(error, MC_JAVA_PRINT_INVALID_ARGUMENT, "PrintStream counted String");
    if (!ps_enter(s, error))
        return error->status;
    error->frames[0] = error->site;
    error->frame_count = 2;
    error->site =
        (mc_java_print_site){"java/io/PrintStream", "writeln", "(Ljava/lang/String;)V", 13};
    error->frames[1] = error->site;
    mc_java_writer_status status = text_write(
        s, text == NULL ? null_units : text->units,
        text == NULL ? sizeof(null_units) / sizeof(null_units[0]) : text->length, &error->encoder);
    if (status == MC_JAVA_WRITER_OK) {
        error->site.pc = 20;
        error->frames[1] = error->site;
        status = text_write(s, newline, sizeof(newline) / sizeof(newline[0]), &error->encoder);
    }
    if (status == MC_JAVA_WRITER_OK) {
        error->site.pc = 27;
        error->frames[1] = error->site;
        status = text_flush(s, &error->encoder);
    }
    if (status == MC_JAVA_WRITER_OK) {
        error->site.pc = 34;
        error->frames[1] = error->site;
        status = mc_java_source_encoder_flush_buffer(s->encoder, &error->encoder);
    }
    if (status == MC_JAVA_WRITER_OK && s->auto_flush) {
        error->site.pc = 48;
        error->frames[1] = error->site;
        status = byte_flush(s, &error->encoder);
    }
    complete(s, status, error);
    if (error->source_returned) {
        ++s->println_returns;
        error->site =
            (mc_java_print_site){"java/io/PrintStream", "println", "(Ljava/lang/String;)V", 43};
    }
    return ps_leave(s, error);
}

mc_java_print_status mc_java_print_stream_flush(mc_java_print_stream *s,
                                                mc_java_print_error *error) {
    if (error == NULL)
        return MC_JAVA_PRINT_INVALID_ARGUMENT;
    *error = (mc_java_print_error){.site = {"java/io/PrintStream", "flush", "()V", 12}};
    if (!ps_enter(s, error))
        return error->status;
    error->frames[0] = error->site;
    error->frame_count = 1;
    const mc_java_writer_status status = byte_flush(s, &error->encoder);
    /* flush catches all IOException, including interruption, into trouble. */
    if (status == MC_JAVA_WRITER_INTERRUPTED_IO) {
        record_io(s, &error->encoder);
        error->source_returned = true;
    } else
        complete(s, status, error);
    if (error->source_returned)
        error->site.pc = 34;
    return ps_leave(s, error);
}

bool mc_java_print_stream_inspect(mc_java_print_stream *s, mc_java_print_state *out) {
    if (s == NULL || out == NULL || mtx_trylock(&s->monitor) != thrd_success)
        return false;
    *out = (mc_java_print_state){.phase = s->phase,
                                 .holds = s->holds,
                                 .byte_count = s->byte_count,
                                 .text_count = s->text_count,
                                 .auto_flush = s->auto_flush,
                                 .trouble = s->trouble,
                                 .closing = s->closing,
                                 .out_present = s->out_present,
                                 .char_out_present = s->encoder != NULL,
                                 .text_out_present = s->text != NULL,
                                 .active = s->active,
                                 .interrupted_thread_pending = s->interrupted_thread_pending,
                                 .physical_bytes_known = s->physical_known,
                                 .physical_bytes = s->physical,
                                 .byte_write_returns = s->byte_returns,
                                 .println_returns = s->println_returns,
                                 .profile = s->profile,
                                 .first_io = s->first_io,
                                 .failure = s->failure};
    if (s->encoder != NULL)
        mc_java_source_encoder_read_state(s->encoder, &out->encoder, &out->last_write);
    mtx_unlock(&s->monitor);
    return true;
}

mc_java_print_status mc_java_print_stream_release(mc_java_print_stream **slot,
                                                  mc_java_print_error *error) {
    if (error == NULL)
        return MC_JAVA_PRINT_INVALID_ARGUMENT;
    *error = (mc_java_print_error){0};
    if (slot == NULL)
        return mc_ps_fail(error, MC_JAVA_PRINT_INVALID_ARGUMENT, "PrintStream release slot");
    mc_java_print_stream *s = *slot;
    if (s == NULL)
        return MC_JAVA_PRINT_OK;
    if (!ps_lock(s, error))
        return error->status;
    if (s->holds > 1) {
        --s->holds;
        *slot = NULL;
        mtx_unlock(&s->monitor);
        return MC_JAVA_PRINT_OK;
    }
    if (s->active) {
        mtx_unlock(&s->monitor);
        return mc_ps_fail(error, MC_JAVA_PRINT_BUSY, "active final PrintStream occurrence");
    }
    s->phase = MC_JAVA_PRINT_RETIRING;
    s->active = true;
    s->source_thread = thrd_current();
    const mc_java_writer_status status =
        mc_java_source_encoder_retire(&s->encoder, &error->encoder);
    if (status == MC_JAVA_WRITER_BUSY) {
        s->active = false;
        mtx_unlock(&s->monitor);
        return mc_ps_fail(error, MC_JAVA_PRINT_BUSY, "PrintStream encoder native detach BUSY");
    }
    if (status != MC_JAVA_WRITER_OK)
        mc_ps_fail(error, MC_JAVA_PRINT_NATIVE_FAILURE,
                   "PrintStream encoder consumed detach error");
    free(s->text);
    mtx_unlock(&s->monitor);
    mtx_destroy(&s->monitor);
    free(s);
    *slot = NULL;
    return error->status;
}
