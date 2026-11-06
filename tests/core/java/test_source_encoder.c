/* Source-void encoder CONTRACT controls with real unbuffered temporary-file
 * writes. The authored fault below occurs after a measured real prefix and
 * supplies explicit returned/thrown outcomes; it is not a replacement game,
 * JDK PrintStream implementation, fake successful native sink or parity proof.
 * Compile exact Writer TU once; linker-map must exclude a second writer.o. */
#include "../../../src/core/java/writer.c"

static unsigned checks;
#define CHECK(value)                                                                               \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(value)) {                                                                            \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #value);                            \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

typedef struct sink {
    FILE *file;
    mc_java_source_encoder *owner;
    size_t prefix_limit;
    unsigned writes, flushes, retires;
    bool fault, swallow, probe_reentry, nested_busy, retire_busy, bad_result;
    unsigned contradictory_outcome;
    bool unknown_progress, retire_error, retire_probe;
    bool retire_write_busy, retire_again_busy, retire_context_live;
} sink;

static mc_java_writer_status native_prefix(sink *s, const uint8_t *bytes, size_t length,
                                           size_t *committed, mc_java_writer_error *error) {
    mc_byte_output_error native = {0};
    const size_t count = length < s->prefix_limit ? length : s->prefix_limit;
    const bool ok = mc_byte_output_write(s->file, bytes, count, committed, &native);
    if (!ok)
        return result(error, MC_JAVA_WRITER_IO_ERROR, native.system_error, "real test file write");
    return MC_JAVA_WRITER_OK;
}

static mc_java_writer_status source_write(void *context, const uint8_t *bytes, size_t length,
                                          mc_java_source_write_result *out,
                                          mc_java_writer_error *error) {
    sink *s = context;
    ++s->writes;
    size_t committed = 0;
    mc_java_writer_status status = native_prefix(s, bytes, length, &committed, error);
    *out = (mc_java_source_write_result){.physical_bytes_known = true, .physical_bytes = committed};
    if (status != MC_JAVA_WRITER_OK)
        return status;
    if (s->probe_reentry) {
        s->probe_reentry = false;
        const uint16_t x = 'X';
        mc_java_writer_error nested = {0};
        s->nested_busy =
            mc_java_source_encoder_write(s->owner, &x, 1, &nested) == MC_JAVA_WRITER_BUSY;
    }
    if (s->unknown_progress) {
        /* The actual prefix exists, but this native-control outcome cannot
         * publish a reliable count. It must never be inspected as known0. */
        out->physical_bytes_known = false;
        out->physical_bytes = 0;
        return result(error, MC_JAVA_WRITER_LIMIT, 0,
                      "authored native failure after actual prefix; count unavailable");
    }
    if (s->contradictory_outcome != 0) {
        out->source_returned = s->contradictory_outcome != 3;
        if (s->contradictory_outcome != 1) {
            out->swallowed_io = true;
            out->swallowed_error = (mc_java_writer_error){
                .status = MC_JAVA_WRITER_IO_ERROR,
                .system_error = EIO,
                .operation = "reported swallowed IO despite non-OK callback"};
        }
        return result(error, MC_JAVA_WRITER_IO_ERROR, EIO,
                      "authored contradictory callback after actual prefix");
    }
    if (s->bad_result)
        return MC_JAVA_WRITER_OK;
    if (s->fault) {
        const mc_java_writer_error injected = {MC_JAVA_WRITER_IO_ERROR, EIO,
                                               "authored IO after actual file prefix", false, 0};
        if (!s->swallow) {
            *error = injected;
            return injected.status;
        }
        out->swallowed_io = true;
        out->swallowed_error = injected;
    }
    out->source_returned = true;
    return MC_JAVA_WRITER_OK;
}

static mc_java_writer_status source_flush_sink(void *context, mc_java_writer_error *error) {
    sink *s = context;
    ++s->flushes;
    mc_byte_output_flush(s->file);
    return result(error, MC_JAVA_WRITER_OK, 0, NULL);
}

static mc_java_writer_status source_retire_sink(void *context, mc_java_writer_error *error) {
    sink *s = context;
    ++s->retires;
    if (s->retire_probe) {
        s->retire_probe = false;
        const uint16_t x = 'X';
        mc_java_writer_error nested = {0};
        s->retire_write_busy =
            mc_java_source_encoder_write(s->owner, &x, 1, &nested) == MC_JAVA_WRITER_BUSY;
        mc_java_source_encoder *same = s->owner;
        s->retire_again_busy =
            mc_java_source_encoder_retire(&same, &nested) == MC_JAVA_WRITER_BUSY &&
            same == s->owner;
        s->retire_context_live = s->file != NULL && s->retires == 1;
    }
    if (s->retire_busy)
        return result(error, MC_JAVA_WRITER_BUSY, 0, "owned detach busy control");
    mc_byte_output_error native = {0};
    const bool ok = mc_byte_output_close(&s->file, &native);
    if (ok && s->retire_error)
        return result(error, MC_JAVA_WRITER_IO_ERROR, EIO,
                      "authored detach error after actual consuming close");
    return result(error, ok ? MC_JAVA_WRITER_OK : MC_JAVA_WRITER_IO_ERROR, native.system_error,
                  ok ? NULL : "real test file close");
}

static mc_java_writer_status legacy_write(void *context, const uint8_t *bytes, size_t length,
                                          size_t *committed, mc_java_writer_error *error) {
    sink *s = context;
    ++s->writes;
    return native_prefix(s, bytes, length, committed, error);
}

static bool open_sink(sink *s) {
    *s = (sink){.prefix_limit = SIZE_MAX};
#ifdef _WIN32
    if (tmpfile_s(&s->file) != 0)
        return false;
#else
    s->file = tmpfile();
#endif
    return s->file != NULL && setvbuf(s->file, NULL, _IONBF, 0) == 0;
}

static bool bytes_are(sink *s, const uint8_t *expected, size_t length) {
    uint8_t bytes[16];
    if (length > sizeof bytes || fseek(s->file, 0, SEEK_SET) != 0)
        return false;
    const size_t read = fread(bytes, 1, sizeof bytes, s->file);
    const bool ok = read == length && memcmp(bytes, expected, length) == 0;
    return fseek(s->file, 0, SEEK_END) == 0 && ok;
}

int main(void) {
    int status = 1;
    sink a = {0}, b = {0}, c = {0}, extra = {0}, legacy = {0};
    mc_java_source_encoder *source = NULL;
    mc_java_writer *writer = NULL;
    mc_java_writer_error error = {0};
    mc_java_writer_state state;
    mc_java_source_write_result last;
    const uint16_t abcd[] = {'A', 'B', 'C', 'D'}, z = 'Z';
    CHECK(open_sink(&a));
    mc_java_source_encoder_output output = {&a, source_write, source_flush_sink,
                                            source_retire_sink};
    CHECK(mc_java_source_encoder_create(&output, MC_JAVA_READER_UTF8, &source, &error) ==
          MC_JAVA_WRITER_OK);
    a.owner = source;
    a.fault = a.swallow = a.probe_reentry = true;
    a.prefix_limit = 2;
    CHECK(mc_java_source_encoder_write(source, abcd, 4, &error) == MC_JAVA_WRITER_OK);
    CHECK(mc_java_source_encoder_flush_buffer(source, &error) == MC_JAVA_WRITER_OK);
    CHECK(a.writes == 1 && a.flushes == 0 && a.nested_busy);
    CHECK(source->writer->position == 0 && source->writer->limit == BYTE_CAPACITY);
    CHECK(mc_java_source_encoder_read_state(source, &state, &last));
    CHECK(last.source_returned && last.swallowed_io && last.physical_bytes_known &&
          last.physical_bytes == 2 && last.swallowed_error.status == MC_JAVA_WRITER_IO_ERROR);
    a.fault = false;
    a.prefix_limit = SIZE_MAX;
    CHECK(mc_java_source_encoder_write(source, &z, 1, &error) == MC_JAVA_WRITER_OK);
    CHECK(mc_java_source_encoder_flush_buffer(source, &error) == MC_JAVA_WRITER_OK);
    CHECK(a.writes == 2 && bytes_are(&a, (const uint8_t *)"ABZ", 3));
    CHECK(mc_java_source_encoder_flush(source, &error) == MC_JAVA_WRITER_OK && a.flushes == 1);
    a.retire_busy = true;
    mc_java_source_encoder *same = source;
    CHECK(mc_java_source_encoder_retire(&source, &error) == MC_JAVA_WRITER_BUSY && source == same);
    CHECK(source->writer->active == false && a.file != NULL && a.retires == 1);
    a.retire_busy = false;
    CHECK(mc_java_source_encoder_retire(&source, &error) == MC_JAVA_WRITER_OK && source == NULL &&
          a.file == NULL);

    CHECK(open_sink(&b));
    output.context = &b;
    CHECK(mc_java_source_encoder_create(&output, MC_JAVA_READER_UTF8, &source, &error) ==
          MC_JAVA_WRITER_OK);
    b.owner = source;
    b.fault = true;
    b.prefix_limit = 2;
    CHECK(mc_java_source_encoder_write(source, abcd, 4, &error) == MC_JAVA_WRITER_OK);
    CHECK(mc_java_source_encoder_flush_buffer(source, &error) == MC_JAVA_WRITER_IO_ERROR);
    CHECK(source->writer->position == 0 && source->writer->limit == 4);
    CHECK(mc_java_source_encoder_read_state(source, &state, &last) && !last.source_returned &&
          !last.swallowed_io && last.physical_bytes_known && last.physical_bytes == 2 &&
          bytes_are(&b, (const uint8_t *)"AB", 2));
    CHECK(mc_java_source_encoder_retire(&source, &error) == MC_JAVA_WRITER_OK);

    CHECK(open_sink(&c));
    output.context = &c;
    CHECK(mc_java_source_encoder_create(&output, MC_JAVA_READER_UTF8, &source, &error) ==
          MC_JAVA_WRITER_OK);
    c.owner = source;
    const uint16_t high_unit = 0xd83d, low_unit = 0xde00;
    CHECK(mc_java_source_encoder_write(source, &high_unit, 1, &error) == MC_JAVA_WRITER_OK);
    CHECK(mc_java_source_encoder_flush_buffer(source, &error) == MC_JAVA_WRITER_OK &&
          c.writes == 0 && c.flushes == 0);
    CHECK(source->writer->leftover);
    CHECK(mc_java_source_encoder_write(source, &low_unit, 1, &error) == MC_JAVA_WRITER_OK);
    CHECK(mc_java_source_encoder_flush_buffer(source, &error) == MC_JAVA_WRITER_OK);
    static const uint8_t smile[] = {0xf0, 0x9f, 0x98, 0x80};
    CHECK(bytes_are(&c, smile, sizeof smile) && !source->writer->leftover);
    c.bad_result = true;
    CHECK(mc_java_source_encoder_write(source, &z, 1, &error) == MC_JAVA_WRITER_OK);
    CHECK(mc_java_source_encoder_flush_buffer(source, &error) == MC_JAVA_WRITER_INVALID_ARGUMENT);
    CHECK(mc_java_source_encoder_read_state(source, &state, &last) && last.outcome_rejected &&
          !last.source_returned && !last.swallowed_io && !last.rejected_source_returned &&
          !last.rejected_swallowed_io);
    CHECK(mc_java_source_encoder_retire(&source, &error) == MC_JAVA_WRITER_OK);

    /* Each contradiction is reached after a real measured file prefix.
     * These are malformed callback-domain controls, not Java return claims. */
    for (unsigned mode = 1; mode <= 3; ++mode) {
        CHECK(open_sink(&extra));
        output.context = &extra;
        CHECK(mc_java_source_encoder_create(&output, MC_JAVA_READER_UTF8, &source, &error) ==
              MC_JAVA_WRITER_OK);
        extra.owner = source;
        extra.prefix_limit = 2;
        extra.contradictory_outcome = mode;
        CHECK(mc_java_source_encoder_write(source, abcd, 4, &error) == MC_JAVA_WRITER_OK);
        CHECK(mc_java_source_encoder_flush_buffer(source, &error) ==
              MC_JAVA_WRITER_INVALID_ARGUMENT);
        CHECK(strcmp(error.operation, "source OutputStream void-result contract") == 0);
        CHECK(source->writer->position == 0 && source->writer->limit == 4);
        CHECK(mc_java_source_encoder_read_state(source, &state, &last));
        CHECK(last.outcome_rejected && !last.source_returned && !last.swallowed_io &&
              last.rejected_source_returned == (mode != 3) &&
              last.rejected_swallowed_io == (mode != 1));
        CHECK(last.physical_bytes_known && last.physical_bytes == 2 &&
              bytes_are(&extra, (const uint8_t *)"AB", 2));
        if (mode != 1)
            CHECK(last.swallowed_error.status == MC_JAVA_WRITER_IO_ERROR &&
                  last.swallowed_error.system_error == EIO);
        CHECK(extra.writes == 1 && extra.flushes == 0);
        CHECK(mc_java_source_encoder_retire(&source, &error) == MC_JAVA_WRITER_OK &&
              source == NULL && extra.file == NULL && extra.retires == 1);
    }

    CHECK(open_sink(&extra));
    output.context = &extra;
    CHECK(mc_java_source_encoder_create(&output, MC_JAVA_READER_UTF8, &source, &error) ==
          MC_JAVA_WRITER_OK);
    extra.owner = source;
    extra.prefix_limit = 2;
    extra.unknown_progress = true;
    CHECK(mc_java_source_encoder_write(source, abcd, 4, &error) == MC_JAVA_WRITER_OK);
    CHECK(mc_java_source_encoder_flush_buffer(source, &error) == MC_JAVA_WRITER_LIMIT);
    CHECK(source->writer->position == 0 && source->writer->limit == 4);
    CHECK(mc_java_source_encoder_read_state(source, &state, &last) && !last.source_returned &&
          !last.swallowed_io && !last.physical_bytes_known && !last.outcome_rejected);
    /* The sink proves a real nonzero prefix while inspection explicitly says
     * its physical count is unknown. Do not assert the placeholder value. */
    CHECK(bytes_are(&extra, (const uint8_t *)"AB", 2) && extra.writes == 1 && extra.flushes == 0);
    CHECK(mc_java_source_encoder_retire(&source, &error) == MC_JAVA_WRITER_OK && source == NULL &&
          extra.file == NULL);

    CHECK(open_sink(&extra));
    output.context = &extra;
    CHECK(mc_java_source_encoder_create(&output, MC_JAVA_READER_UTF8, &source, &error) ==
          MC_JAVA_WRITER_OK);
    extra.owner = source;
    extra.retire_probe = extra.retire_error = true;
    CHECK(mc_java_source_encoder_write(source, abcd, 4, &error) == MC_JAVA_WRITER_OK);
    CHECK(mc_java_source_encoder_read_state(source, &state, &last) && !state.source_close_reached);
    CHECK(bytes_are(&extra, (const uint8_t *)"", 0));
    CHECK(mc_java_source_encoder_retire(&source, &error) == MC_JAVA_WRITER_IO_ERROR &&
          source == NULL && extra.file == NULL);
    CHECK(extra.retire_write_busy && extra.retire_again_busy && extra.retire_context_live &&
          extra.retires == 1 && extra.writes == 0 && extra.flushes == 0);
    CHECK(mc_java_source_encoder_retire(&source, &error) == MC_JAVA_WRITER_OK &&
          extra.retires == 1);

    /* Existing physical short-success path still loops until its actual
     * committed bytes complete the same original OutputStream write. */
    CHECK(open_sink(&legacy));
    legacy.prefix_limit = 2;
    const mc_java_writer_output old_output = {&legacy, legacy_write, source_flush_sink,
                                              source_retire_sink};
    const mc_java_writer_profile profile = {MC_JAVA_READER_UTF8, MC_JAVA_WRITER_OUTPUT_STREAM,
                                            MC_JAVA_WRITER_LF};
    CHECK(mc_java_writer_create(&old_output, &profile, &writer, &error) == MC_JAVA_WRITER_OK);
    CHECK(mc_java_writer_write(writer, abcd, 4, &error) == MC_JAVA_WRITER_OK);
    CHECK(mc_java_writer_flush(writer, &error) == MC_JAVA_WRITER_OK);
    CHECK(legacy.writes == 2 && legacy.flushes == 1 &&
          bytes_are(&legacy, (const uint8_t *)"ABCD", 4));
    CHECK(mc_java_writer_retire(&writer, NULL, &error) == MC_JAVA_WRITER_OK && writer == NULL);
    status = 0;
cleanup:
    a.retire_busy = b.retire_busy = c.retire_busy = extra.retire_busy = false;
    extra.retire_error = false;
    if (mc_java_source_encoder_retire(&source, &error) != MC_JAVA_WRITER_OK)
        status = 1;
    if (mc_java_writer_retire(&writer, NULL, &error) != MC_JAVA_WRITER_OK)
        status = 1;
    if (a.file != NULL)
        fclose(a.file);
    if (b.file != NULL)
        fclose(b.file);
    if (c.file != NULL)
        fclose(c.file);
    if (extra.file != NULL)
        fclose(extra.file);
    if (legacy.file != NULL)
        fclose(legacy.file);
    if (status == 0 && printf("Source-void encoder contracts: %u checks\n", checks) < 0)
        status = 1;
    return status;
}
