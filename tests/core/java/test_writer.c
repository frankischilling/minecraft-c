#include "mc/core/java/writer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define CAPACITY 65536u

static unsigned int checks;
#define CHECK(test)                                                                                \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(test)) {                                                                             \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #test);                             \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

/* Authored byte sink exercises the public stream seam. This is explicitly
 * not a filesystem or original-FileWriter observation. Real file tests are
 * separate. Error callbacks actually append their declared committed prefix. */
typedef struct sink {
    uint8_t bytes[CAPACITY];
    size_t length, chunk, writes, flushes, closes;
    size_t fail_write, fail_flush, failed_prefix;
    mc_java_writer_status write_failure, flush_failure, close_failure;
    bool invalid_count, zero_progress, invalid, reenter;
    size_t requests[8];
    mc_java_writer **owner;
    mc_java_writer_status reentry_write, reentry_retire;
    bool reentry_state;
} sink;

static mc_java_writer_status sink_write(void *context, const uint8_t *bytes, size_t length,
                                        size_t *committed, mc_java_writer_error *error) {
    sink *s = context;
    ++s->writes;
    if (s->writes <= COUNT(s->requests))
        s->requests[s->writes - 1u] = length;
    if (bytes == NULL || length == 0 || length > 8192u || committed == NULL) {
        s->invalid = true;
        return MC_JAVA_WRITER_IO_ERROR;
    }
    if (s->reenter) {
        s->reenter = false;
        const uint16_t x = 'x';
        mc_java_writer_state before = {0};
        s->reentry_state = mc_java_writer_read_state(*s->owner, &before);
        s->reentry_write = mc_java_writer_write(*s->owner, &x, 1, NULL);
        s->reentry_retire = mc_java_writer_retire(s->owner, NULL, NULL);
    }
    if (s->invalid_count) {
        *committed = length + 1u;
        return MC_JAVA_WRITER_OK;
    }
    if (s->zero_progress) {
        *committed = 0;
        return MC_JAVA_WRITER_OK;
    }
    const bool fails = s->fail_write != 0 && s->writes == s->fail_write;
    size_t take = fails ? s->failed_prefix : length;
    if (take > length)
        take = length;
    if (!fails && s->chunk != 0 && take > s->chunk)
        take = s->chunk;
    if (take > CAPACITY - s->length) {
        s->invalid = true;
        return MC_JAVA_WRITER_IO_ERROR;
    }
    memcpy(s->bytes + s->length, bytes, take);
    s->length += take;
    *committed = take;
    if (fails) {
        *error = (mc_java_writer_error){
            .status = s->write_failure, .system_error = 87, .operation = "authored write failure"};
        return s->write_failure;
    }
    return MC_JAVA_WRITER_OK;
}

static mc_java_writer_status sink_flush(void *context, mc_java_writer_error *error) {
    sink *s = context;
    ++s->flushes;
    if (s->fail_flush != 0 && s->flushes == s->fail_flush) {
        *error = (mc_java_writer_error){
            .status = s->flush_failure, .system_error = 88, .operation = "authored flush failure"};
        return s->flush_failure;
    }
    return MC_JAVA_WRITER_OK;
}

static mc_java_writer_status sink_close(void *context, mc_java_writer_error *error) {
    sink *s = context;
    ++s->closes;
    if (s->close_failure != MC_JAVA_WRITER_OK)
        *error = (mc_java_writer_error){
            .status = s->close_failure, .system_error = 99, .operation = "authored close failure"};
    return s->close_failure;
}

static mc_java_writer_status create(sink *s, mc_java_reader_charset charset,
                                    mc_java_writer_mode mode, mc_java_writer_lines lines,
                                    mc_java_writer **out) {
    const mc_java_writer_output output = {s, sink_write, sink_flush, sink_close};
    const mc_java_writer_profile profile = {charset, mode, lines};
    return mc_java_writer_create(&output, &profile, out, NULL);
}

static bool encoding_partitions(void) {
    static const uint16_t input[] = {'A',    0,      0x7f,   0x80, 0xe9,  0x20ac,
                                     0xd83d, 0xde00, 0xd800, 'Z',  0xdc00};
    static const uint8_t utf8[] = {'A',  0,    0x7f, 0xc2, 0x80, 0xc3, 0xa9, 0xe2, 0x82,
                                   0xac, 0xf0, 0x9f, 0x98, 0x80, '?',  'Z',  '?'};
    static const uint8_t cp[] = {'A', 0, 0x7f, '?', 0xe9, 0x80, '?', '?', 'Z', '?'};
    static const uint8_t iso[] = {'A', 0, 0x7f, 0x80, 0xe9, '?', '?', '?', 'Z', '?'};
    static const uint8_t ascii[] = {'A', 0, 0x7f, '?', '?', '?', '?', '?', 'Z', '?'};
    const uint8_t *expected[] = {utf8, cp, iso, ascii};
    const size_t sizes[] = {sizeof(utf8), sizeof(cp), sizeof(iso), sizeof(ascii)};
    sink *s = malloc(sizeof(*s));
    mc_java_writer *writer = NULL;
    bool ok = false;
    CHECK(s != NULL);
    for (unsigned int charset = MC_JAVA_READER_UTF8; charset <= MC_JAVA_READER_US_ASCII;
         ++charset) {
        for (unsigned int splits = 0; splits < (1u << (COUNT(input) - 1u)); ++splits) {
            *s = (sink){.chunk = 7};
            CHECK(create(s, (mc_java_reader_charset)charset, MC_JAVA_WRITER_OUTPUT_STREAM,
                         MC_JAVA_WRITER_LF, &writer) == MC_JAVA_WRITER_OK);
            CHECK(s->writes == 0 && s->closes == 0 && s->flushes == 0);
            size_t begin = 0;
            for (size_t i = 0; i < COUNT(input); ++i) {
                if (i + 1u == COUNT(input) || (splits & (1u << i)) != 0) {
                    CHECK(mc_java_writer_write(writer, input + begin, i + 1u - begin, NULL) ==
                          MC_JAVA_WRITER_OK);
                    CHECK(mc_java_writer_flush(writer, NULL) == MC_JAVA_WRITER_OK);
                    begin = i + 1u;
                }
            }
            CHECK(mc_java_writer_close(writer, NULL) == MC_JAVA_WRITER_OK);
            CHECK(s->length == sizes[charset - MC_JAVA_READER_UTF8] &&
                  memcmp(s->bytes, expected[charset - MC_JAVA_READER_UTF8], s->length) == 0);
            mc_java_writer_state state = {0};
            CHECK(mc_java_writer_read_state(writer, &state) && state.encoder_closed &&
                  state.source_close_reached && state.sink_close_attempted && !state.native_file);
            CHECK(mc_java_writer_close(writer, NULL) == MC_JAVA_WRITER_OK && s->closes == 1);
            mc_java_writer_retirement retired = {0};
            CHECK(mc_java_writer_retire(&writer, &retired, NULL) == MC_JAVA_WRITER_OK &&
                  retired.source_close_reached && !retired.additional_sink_close && !s->invalid);
        }
    }
    ok = true;
cleanup:
    (void)mc_java_writer_retire(&writer, NULL, NULL);
    free(s);
    return ok;
}

static bool buffer_boundaries(void) {
    uint16_t *units = malloc(8194u * sizeof(*units));
    sink *s = malloc(sizeof(*s));
    mc_java_writer *writer = NULL;
    bool ok = false;
    CHECK(units != NULL && s != NULL);
    for (size_t i = 0; i < 8192u; ++i)
        units[i] = 'A';
    units[8192] = 0xd800;
    for (unsigned int charset = MC_JAVA_READER_UTF8; charset <= MC_JAVA_READER_US_ASCII;
         ++charset) {
        *s = (sink){0};
        CHECK(create(s, (mc_java_reader_charset)charset, MC_JAVA_WRITER_OUTPUT_STREAM,
                     MC_JAVA_WRITER_LF, &writer) == MC_JAVA_WRITER_OK);
        CHECK(mc_java_writer_write(writer, units, 8193, NULL) == MC_JAVA_WRITER_OK);
        const bool drains_first =
            charset == MC_JAVA_READER_WINDOWS1252 || charset == MC_JAVA_READER_ISO8859_1;
        CHECK(s->writes == (drains_first ? 1u : 0u));
        CHECK(mc_java_writer_write(writer, NULL, 0, NULL) == MC_JAVA_WRITER_OK);
        CHECK(mc_java_writer_flush(writer, NULL) == MC_JAVA_WRITER_OK && s->length == 8192);
        CHECK(mc_java_writer_close(writer, NULL) == MC_JAVA_WRITER_OK && s->length == 8193 &&
              s->bytes[8192] == '?');
        for (size_t i = 0; i < 8192u; ++i)
            CHECK(s->bytes[i] == 'A');
        CHECK(mc_java_writer_retire(&writer, NULL, NULL) == MC_JAVA_WRITER_OK);
    }
    *s = (sink){0};
    units[8191] = 0xd83d;
    units[8192] = 0xde00;
    CHECK(create(s, MC_JAVA_READER_UTF8, MC_JAVA_WRITER_OUTPUT_STREAM, MC_JAVA_WRITER_LF,
                 &writer) == MC_JAVA_WRITER_OK);
    CHECK(mc_java_writer_write(writer, units, 8193, NULL) == MC_JAVA_WRITER_OK);
    CHECK(s->writes == 1 && s->requests[0] == 8191 && s->length == 8191);
    CHECK(mc_java_writer_close(writer, NULL) == MC_JAVA_WRITER_OK && s->length == 8195);
    CHECK(memcmp(s->bytes + 8191, "\xf0\x9f\x98\x80", 4) == 0);
    ok = true;
cleanup:
    (void)mc_java_writer_retire(&writer, NULL, NULL);
    free(units);
    free(s);
    return ok;
}

static bool print_lifecycle(void) {
    sink *s = malloc(sizeof(*s));
    mc_java_writer *writer = NULL;
    bool ok = false, trouble = true;
    CHECK(s != NULL);
    *s = (sink){0};
    CHECK(create(s, MC_JAVA_READER_UTF8, MC_JAVA_WRITER_PRINT, MC_JAVA_WRITER_CRLF, &writer) ==
          MC_JAVA_WRITER_OK);
    const mc_java_properties_string_view empty = {NULL, 0};
    const uint16_t embedded[] = {'A', 0, 'B'};
    const mc_java_properties_string_view text = {embedded, COUNT(embedded)};
    CHECK(mc_java_writer_println(writer, NULL, NULL) == MC_JAVA_WRITER_OK);
    CHECK(mc_java_writer_println(writer, &empty, NULL) == MC_JAVA_WRITER_OK);
    CHECK(mc_java_writer_println(writer, &text, NULL) == MC_JAVA_WRITER_OK);
    CHECK(s->writes == 0 && s->flushes == 0 && s->length == 0);
    CHECK(mc_java_writer_check_error(writer, &trouble, NULL) == MC_JAVA_WRITER_OK && !trouble);
    static const uint8_t expected[] = {'n',  'u', 'l', 'l', '\r', '\n', '\r',
                                       '\n', 'A', 0,   'B', '\r', '\n'};
    CHECK(s->length == sizeof(expected) && memcmp(s->bytes, expected, sizeof(expected)) == 0);
    CHECK(mc_java_writer_close(writer, NULL) == MC_JAVA_WRITER_OK && s->closes == 1);
    mc_java_writer_state state = {0};
    CHECK(mc_java_writer_read_state(writer, &state) && !state.print_writer_out_present &&
          !state.trouble);
    CHECK(mc_java_writer_write(writer, NULL, 0, NULL) == MC_JAVA_WRITER_OK);
    CHECK(mc_java_writer_check_error(writer, &trouble, NULL) == MC_JAVA_WRITER_OK && trouble);
    CHECK(mc_java_writer_close(writer, NULL) == MC_JAVA_WRITER_OK && s->closes == 1);
    ok = true;
cleanup:
    (void)mc_java_writer_retire(&writer, NULL, NULL);
    free(s);
    return ok;
}

static bool partial_errors(void) {
    sink *s = malloc(sizeof(*s));
    mc_java_writer *writer = NULL;
    bool ok = false;
    CHECK(s != NULL);
    *s = (sink){.fail_write = 1, .failed_prefix = 2, .write_failure = MC_JAVA_WRITER_IO_ERROR};
    CHECK(create(s, MC_JAVA_READER_UTF8, MC_JAVA_WRITER_OUTPUT_STREAM, MC_JAVA_WRITER_LF,
                 &writer) == MC_JAVA_WRITER_OK);
    const uint16_t abc[] = {'A', 'B', 'C'}, next[] = {'x', 'y', 'z', 'z'};
    mc_java_writer_error error = {0};
    CHECK(mc_java_writer_write(writer, abc, COUNT(abc), NULL) == MC_JAVA_WRITER_OK);
    CHECK(mc_java_writer_flush(writer, &error) == MC_JAVA_WRITER_IO_ERROR &&
          error.system_error == 87);
    CHECK(s->length == 2 && memcmp(s->bytes, "AB", 2) == 0 && s->flushes == 0);
    CHECK(mc_java_writer_write(writer, next, COUNT(next), NULL) == MC_JAVA_WRITER_OK);
    CHECK(s->writes == 2 && s->requests[0] == 3 && s->requests[1] == 3);
    CHECK(mc_java_writer_close(writer, NULL) == MC_JAVA_WRITER_OK);
    CHECK(s->length == 6 && memcmp(s->bytes, "ABxyzz", 6) == 0 && s->requests[2] == 1);
    CHECK(mc_java_writer_retire(&writer, NULL, NULL) == MC_JAVA_WRITER_OK);
    *s = (sink){.fail_write = 1, .failed_prefix = 1, .write_failure = MC_JAVA_WRITER_IO_ERROR};
    CHECK(create(s, MC_JAVA_READER_UTF8, MC_JAVA_WRITER_OUTPUT_STREAM, MC_JAVA_WRITER_LF,
                 &writer) == MC_JAVA_WRITER_OK);
    CHECK(mc_java_writer_write(writer, abc, COUNT(abc), NULL) == MC_JAVA_WRITER_OK);
    CHECK(mc_java_writer_flush(writer, NULL) == MC_JAVA_WRITER_IO_ERROR);
    CHECK(mc_java_writer_close(writer, NULL) == MC_JAVA_WRITER_OK);
    CHECK(s->length == 1 && s->bytes[0] == 'A' && s->writes == 1 && s->closes == 1);
    ok = true;
cleanup:
    (void)mc_java_writer_retire(&writer, NULL, NULL);
    free(s);
    return ok;
}

static bool swallowed_errors(void) {
    sink *s = malloc(sizeof(*s));
    uint16_t *text = malloc(8193u * sizeof(*text));
    mc_java_writer *writer = NULL;
    bool ok = false, trouble = true;
    CHECK(s != NULL && text != NULL);
    for (size_t i = 0; i < 8193u; ++i)
        text[i] = 'A';
    *s = (sink){.fail_write = 1, .failed_prefix = 3, .write_failure = MC_JAVA_WRITER_IO_ERROR};
    CHECK(create(s, MC_JAVA_READER_UTF8, MC_JAVA_WRITER_PRINT, MC_JAVA_WRITER_LF, &writer) ==
          MC_JAVA_WRITER_OK);
    const mc_java_properties_string_view view = {text, 8193};
    CHECK(mc_java_writer_println(writer, &view, NULL) == MC_JAVA_WRITER_OK);
    mc_java_writer_state state = {0};
    CHECK(mc_java_writer_read_state(writer, &state) && state.trouble && !state.interrupted_write);
    CHECK(s->length == 3 && s->flushes == 0);
    CHECK(mc_java_writer_close(writer, NULL) == MC_JAVA_WRITER_OK && s->length == 4 &&
          memcmp(s->bytes, "AAA\n", 4) == 0);
    CHECK(mc_java_writer_retire(&writer, NULL, NULL) == MC_JAVA_WRITER_OK);
    *s = (sink){.fail_write = 1, .write_failure = MC_JAVA_WRITER_INTERRUPTED_IO};
    CHECK(create(s, MC_JAVA_READER_UTF8, MC_JAVA_WRITER_PRINT, MC_JAVA_WRITER_LF, &writer) ==
          MC_JAVA_WRITER_OK);
    CHECK(mc_java_writer_write(writer, text, 8193, NULL) == MC_JAVA_WRITER_OK);
    CHECK(mc_java_writer_read_state(writer, &state) && !state.trouble && state.interrupted_write);
    CHECK(mc_java_writer_check_error(writer, &trouble, NULL) == MC_JAVA_WRITER_OK && !trouble);
    CHECK(mc_java_writer_retire(&writer, NULL, NULL) == MC_JAVA_WRITER_OK);
    *s = (sink){.fail_flush = 1, .flush_failure = MC_JAVA_WRITER_INTERRUPTED_IO};
    CHECK(create(s, MC_JAVA_READER_UTF8, MC_JAVA_WRITER_PRINT, MC_JAVA_WRITER_LF, &writer) ==
          MC_JAVA_WRITER_OK);
    CHECK(mc_java_writer_flush(writer, NULL) == MC_JAVA_WRITER_OK);
    CHECK(mc_java_writer_read_state(writer, &state) && state.trouble && !state.interrupted_write);
    ok = true;
cleanup:
    (void)mc_java_writer_retire(&writer, NULL, NULL);
    free(s);
    free(text);
    return ok;
}

static bool close_and_cleanup(void) {
    sink *s = malloc(sizeof(*s));
    mc_java_writer *writer = NULL;
    bool ok = false;
    CHECK(s != NULL);
    *s = (sink){.fail_write = 1,
                .write_failure = MC_JAVA_WRITER_IO_ERROR,
                .close_failure = MC_JAVA_WRITER_INTERRUPTED_IO};
    CHECK(create(s, MC_JAVA_READER_UTF8, MC_JAVA_WRITER_PRINT, MC_JAVA_WRITER_LF, &writer) ==
          MC_JAVA_WRITER_OK);
    const uint16_t high_unit = 0xd800;
    CHECK(mc_java_writer_write(writer, &high_unit, 1, NULL) == MC_JAVA_WRITER_OK);
    CHECK(mc_java_writer_flush(writer, NULL) == MC_JAVA_WRITER_OK && s->writes == 0);
    CHECK(mc_java_writer_close(writer, NULL) == MC_JAVA_WRITER_OK && s->closes == 1);
    mc_java_writer_state state = {0};
    CHECK(mc_java_writer_read_state(writer, &state) && state.encoder_closed &&
          state.print_writer_out_present && state.trouble && state.source_close_reached &&
          state.first_io_error.system_error == 87 &&
          state.secondary_close_error.system_error == 99 && !state.interrupted_write);
    CHECK(mc_java_writer_close(writer, NULL) == MC_JAVA_WRITER_OK && s->closes == 1);
    CHECK(mc_java_writer_read_state(writer, &state) && !state.print_writer_out_present &&
          state.trouble);
    CHECK(mc_java_writer_retire(&writer, NULL, NULL) == MC_JAVA_WRITER_OK);
    *s = (sink){0};
    CHECK(create(s, MC_JAVA_READER_UTF8, MC_JAVA_WRITER_PRINT, MC_JAVA_WRITER_LF, &writer) ==
          MC_JAVA_WRITER_OK);
    const uint16_t abandoned[] = {'A', 'B', 'C', 0xd800};
    CHECK(mc_java_writer_write(writer, abandoned, COUNT(abandoned), NULL) == MC_JAVA_WRITER_OK);
    mc_java_writer_retirement retired = {true, false};
    CHECK(mc_java_writer_retire(&writer, &retired, NULL) == MC_JAVA_WRITER_OK && writer == NULL &&
          !retired.source_close_reached && retired.additional_sink_close);
    CHECK(s->length == 0 && s->writes == 0 && s->flushes == 0 && s->closes == 1);
    *s = (sink){.close_failure = MC_JAVA_WRITER_IO_ERROR};
    CHECK(create(s, MC_JAVA_READER_UTF8, MC_JAVA_WRITER_PRINT, MC_JAVA_WRITER_LF, &writer) ==
          MC_JAVA_WRITER_OK);
    mc_java_writer_error error = {0};
    CHECK(mc_java_writer_retire(&writer, &retired, &error) == MC_JAVA_WRITER_IO_ERROR &&
          writer == NULL && error.system_error == 99 && retired.additional_sink_close);
    CHECK(mc_java_writer_retire(&writer, NULL, NULL) == MC_JAVA_WRITER_OK && s->closes == 1);
    ok = true;
cleanup:
    (void)mc_java_writer_retire(&writer, NULL, NULL);
    free(s);
    return ok;
}

static bool admission_and_reentry(void) {
    sink *s = malloc(sizeof(*s));
    mc_java_writer *writer = NULL;
    bool ok = false;
    CHECK(s != NULL);
    *s = (sink){0};
    mc_java_writer_output output = {s, sink_write, sink_flush, sink_close};
    mc_java_writer_profile profile = {MC_JAVA_READER_UTF8, MC_JAVA_WRITER_OUTPUT_STREAM,
                                      MC_JAVA_WRITER_LF};
    mc_java_writer *sentinel = (mc_java_writer *)s;
    output.close = NULL;
    CHECK(mc_java_writer_create(&output, &profile, &sentinel, NULL) ==
              MC_JAVA_WRITER_INVALID_ARGUMENT &&
          sentinel == (mc_java_writer *)s && s->closes == 0 && s->writes == 0);
    CHECK(create(s, MC_JAVA_READER_UTF8, MC_JAVA_WRITER_OUTPUT_STREAM, MC_JAVA_WRITER_LF,
                 &writer) == MC_JAVA_WRITER_OK);
    const uint16_t unit = 'x';
    CHECK(mc_java_writer_write(writer, NULL, 1, NULL) == MC_JAVA_WRITER_INVALID_ARGUMENT);
    CHECK(mc_java_writer_write(writer, &unit, (size_t)INT32_MAX + 1u, NULL) ==
          MC_JAVA_WRITER_LIMIT);
    CHECK(mc_java_writer_print(writer, NULL, NULL) == MC_JAVA_WRITER_INVALID_ARGUMENT);
    bool preserved = true;
    CHECK(mc_java_writer_check_error(writer, &preserved, NULL) == MC_JAVA_WRITER_INVALID_ARGUMENT &&
          preserved);
    CHECK(mc_java_writer_write(writer, &unit, 1, NULL) == MC_JAVA_WRITER_OK);
    s->reenter = true;
    s->owner = &writer;
    CHECK(mc_java_writer_flush(writer, NULL) == MC_JAVA_WRITER_OK && writer != NULL);
    CHECK(s->reentry_write == MC_JAVA_WRITER_BUSY && s->reentry_retire == MC_JAVA_WRITER_BUSY &&
          !s->reentry_state && s->length == 1 && s->bytes[0] == 'x');
    CHECK(mc_java_writer_retire(&writer, NULL, NULL) == MC_JAVA_WRITER_OK);
    *s = (sink){.invalid_count = true};
    CHECK(create(s, MC_JAVA_READER_UTF8, MC_JAVA_WRITER_PRINT, MC_JAVA_WRITER_LF, &writer) ==
          MC_JAVA_WRITER_OK);
    CHECK(mc_java_writer_write(writer, &unit, 1, NULL) == MC_JAVA_WRITER_OK);
    CHECK(mc_java_writer_flush(writer, NULL) == MC_JAVA_WRITER_INVALID_ARGUMENT);
    mc_java_writer_state state = {0};
    CHECK(mc_java_writer_read_state(writer, &state) && !state.trouble);
    CHECK(mc_java_writer_retire(&writer, NULL, NULL) == MC_JAVA_WRITER_OK);
    *s = (sink){.zero_progress = true};
    CHECK(create(s, MC_JAVA_READER_UTF8, MC_JAVA_WRITER_PRINT, MC_JAVA_WRITER_LF, &writer) ==
          MC_JAVA_WRITER_OK);
    CHECK(mc_java_writer_write(writer, &unit, 1, NULL) == MC_JAVA_WRITER_OK);
    CHECK(mc_java_writer_flush(writer, NULL) == MC_JAVA_WRITER_INVALID_ARGUMENT);
    CHECK(mc_java_writer_read_state(writer, &state) && !state.trouble);
    CHECK(mc_java_writer_retire(&writer, NULL, NULL) == MC_JAVA_WRITER_OK);
    *s = (sink){0};
    const mc_java_reader_status selected = mc_java_reader_default_charset(&profile.charset);
    const mc_java_writer_status found =
        create(s, MC_JAVA_READER_DEFAULT, MC_JAVA_WRITER_OUTPUT_STREAM, MC_JAVA_WRITER_LF, &writer);
    if (selected == MC_JAVA_READER_OK) {
        CHECK(found == MC_JAVA_WRITER_OK && mc_java_writer_read_state(writer, &state) &&
              state.charset == profile.charset);
    } else {
        CHECK(found != MC_JAVA_WRITER_OK && writer == NULL);
    }
    ok = true;
cleanup:
    (void)mc_java_writer_retire(&writer, NULL, NULL);
    free(s);
    return ok;
}

int main(void) {
    if (!encoding_partitions() || !buffer_boundaries() || !print_lifecycle() || !partial_errors() ||
        !swallowed_errors() || !close_and_cleanup() || !admission_and_reentry())
        return EXIT_FAILURE;
    printf("PASS: %u Java writer contract checks\n", checks);
    return EXIT_SUCCESS;
}
