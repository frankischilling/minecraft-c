/* Qualification must prove one included real PrintStream TU and no extracted
 * archive print_stream.o. Faults forward real stdout writes before failing. */
#include <stdlib.h>
static void *observed_malloc(size_t);
#define malloc observed_malloc
#include "../../../src/core/java/print_stream.c"
#undef malloc
#include "print_stream_test_io.h"

static bool fail_text_once;

static void *observed_malloc(size_t bytes) {
    if (fail_text_once) {
        fail_text_once = false;
        return NULL;
    }
    return malloc(bytes);
}

static unsigned int checks;
#define NEED(x)                                                                                    \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            fprintf(stderr, "%s:%d %s\n", __FILE__, __LINE__, #x);                                 \
            goto done;                                                                             \
        }                                                                                          \
    } while (0)
#ifdef _WIN32
static const uint8_t line[] = {'\r', '\n'};
#else
static const uint8_t line[] = {'\n'};
#endif

typedef struct fault_state {
    mc_java_print_stream *stream;
    mc_java_system_output *system;
    mc_java_print_stream *replacement;
    bool once, unknown, interrupt, rebind, reentered, retired_busy;
} fault_state;

static mc_java_writer_status real_prefix_fault(void *context, const mc_text_stdout *stream,
                                               const uint8_t *bytes, size_t length,
                                               size_t *physical, bool *known,
                                               mc_java_writer_error *e) {
    fault_state *f = context;
    if (!f->once)
        return mc_ps_native_stdout_write(NULL, stream, bytes, length, physical, known, e);
    f->once = false;
    mc_java_print_error nested;
    const mc_java_properties_string_view empty = {NULL, 0};
    f->reentered = mc_java_print_stream_println(f->stream, &empty, &nested) == MC_JAVA_PRINT_BUSY;
    mc_java_print_stream *alias = f->stream;
    /* For this control the sole ordinary occurrence is held by the caller. */
    f->retired_busy =
        mc_java_print_stream_release(&alias, &nested) == MC_JAVA_PRINT_BUSY && alias == f->stream;
    if (f->rebind &&
        mc_java_system_output_bind(f->system, f->replacement, &nested) != MC_JAVA_PRINT_OK) {
        *e = (mc_java_writer_error){.status = MC_JAVA_WRITER_INVALID_ARGUMENT,
                                    .operation = "real rebind fault control failed"};
        *known = false;
        return e->status;
    }
    const size_t prefix = length < 2u ? length : 2u;
    const mc_java_writer_status real =
        mc_ps_native_stdout_write(NULL, stream, bytes, prefix, physical, known, e);
    if (real != MC_JAVA_WRITER_OK)
        return real;
    if (f->unknown)
        *known = false;
    *e = (mc_java_writer_error){.status = f->unknown     ? MC_JAVA_WRITER_INVALID_ARGUMENT
                                          : f->interrupt ? MC_JAVA_WRITER_INTERRUPTED_IO
                                                         : MC_JAVA_WRITER_IO_ERROR,
                                .system_error = 123,
                                .operation = "native fault after a genuine written prefix"};
    return e->status;
}

static bool fault_case(bool unknown, bool interrupt) {
    bool ok = false;
    print_test_io io = {0};
    mc_java_print_stream *s = NULL;
    mc_java_print_error e;
    mc_java_print_state state;
    uint8_t bytes[64];
    fault_state fault = {.once = true, .unknown = unknown, .interrupt = interrupt};
    static const uint16_t abc[] = {'a', 'b', 'c'};
    const mc_java_properties_string_view text = {abc, 3};
    NEED(print_test_redirect(&io, false));
    NEED(mc_java_print_stream_allocate(&s, &e) == MC_JAVA_PRINT_OK);
    NEED(mc_java_print_stream_construct_encoded_stdout(s, MC_JAVA_READER_UTF8, true, &e) ==
         MC_JAVA_PRINT_OK);
    fault.stream = s;
    s->native_write = real_prefix_fault;
    s->native_context = &fault;
    const mc_java_print_status status = mc_java_print_stream_println(s, &text, &e);
    NEED(fault.reentered && fault.retired_busy);
    NEED(mc_java_print_stream_inspect(s, &state));
    const size_t count = print_test_read(&io, bytes, sizeof(bytes));
    if (unknown) {
        NEED(status == MC_JAVA_PRINT_NATIVE_FAILURE && !e.source_returned);
        NEED(state.phase == MC_JAVA_PRINT_FAILED && !state.physical_bytes_known);
        NEED(!state.last_write.source_returned && !state.last_write.physical_bytes_known);
        NEED(count == 2 && memcmp(bytes, "ab", 2) == 0 && state.println_returns == 0);
    } else if (interrupt) {
        NEED(status == MC_JAVA_PRINT_THREAD_UNIMPLEMENTED && !e.source_returned);
        NEED(e.interrupted_io && e.site.pc == 52 && state.interrupted_thread_pending);
        NEED(e.frame_count == 3 && e.frames[0].pc == 14 && e.frames[1].pc == 34 &&
             e.frames[2].pc == 52);
        NEED(state.phase == MC_JAVA_PRINT_FAILED && count == 2 && state.println_returns == 0);
    } else {
        NEED(status == MC_JAVA_PRINT_OK && e.source_returned && state.trouble);
        NEED(state.phase == MC_JAVA_PRINT_READY && state.println_returns == 1);
        NEED(state.physical_bytes_known && count == 5u + sizeof(line) &&
             state.physical_bytes == count);
        NEED(memcmp(bytes, "ababc", 5) == 0 && memcmp(bytes + 5, line, sizeof(line)) == 0);
        NEED(state.first_io.status == MC_JAVA_WRITER_IO_ERROR && state.last_write.swallowed_io);
    }
    ok = true;
done:
    if (s != NULL && mc_java_print_stream_release(&s, &e) != MC_JAVA_PRINT_OK)
        ok = false;
    if (!print_test_restore(&io))
        ok = false;
    return ok;
}

static bool unicode_and_buffers(void) {
    bool ok = false;
    print_test_io io = {0};
    mc_java_print_stream *s = NULL;
    mc_java_print_error e;
    mc_java_print_state state;
    uint16_t *units = NULL;
    uint8_t *bytes = NULL;
    NEED(print_test_redirect(&io, false));
    NEED(mc_java_print_stream_allocate(&s, &e) == MC_JAVA_PRINT_OK);
    NEED(mc_java_print_stream_construct_encoded_stdout(s, MC_JAVA_READER_UTF8, false, &e) ==
         MC_JAVA_PRINT_OK);
    units = malloc(8193u * sizeof(*units));
    bytes = malloc(9000);
    NEED(units != NULL && bytes != NULL);
    for (size_t i = 0; i < 8191; ++i)
        units[i] = 'x';
    units[8191] = 0xd83d;
    units[8192] = 0xde00; /* Real pair crosses BufferedWriter's8192 boundary. */
    const mc_java_properties_string_view text = {units, 8193};
    NEED(mc_java_print_stream_println(s, &text, &e) == MC_JAVA_PRINT_OK);
    NEED(mc_java_print_stream_inspect(s, &state) && !state.auto_flush && state.byte_count > 0);
    NEED(mc_java_print_stream_flush(s, &e) == MC_JAVA_PRINT_OK);
    const size_t count = print_test_read(&io, bytes, 9000);
    NEED(count == 8195u + sizeof(line));
    for (size_t i = 0; i < 8191; ++i)
        NEED(bytes[i] == 'x');
    static const uint8_t smile[] = {0xf0, 0x9f, 0x98, 0x80};
    NEED(memcmp(bytes + 8191, smile, 4) == 0 && memcmp(bytes + 8195, line, sizeof(line)) == 0);
    NEED(mc_java_print_stream_inspect(s, &state) && state.byte_count == 0 && !state.trouble);
    static const uint16_t malformed[] = {0, 0xd800, 'z'};
    const mc_java_properties_string_view more = {malformed, 3};
    NEED(mc_java_print_stream_println(s, &more, &e) == MC_JAVA_PRINT_OK);
    NEED(mc_java_print_stream_flush(s, &e) == MC_JAVA_PRINT_OK);
    const size_t final_count = print_test_read(&io, bytes, 9000);
    NEED(final_count == count + 3u + sizeof(line));
    NEED(bytes[count] == 0 && bytes[count + 1] == '?' && bytes[count + 2] == 'z');
    NEED(memcmp(bytes + count + 3, line, sizeof(line)) == 0);
    ok = true;
done:
    free(units);
    free(bytes);
    if (s != NULL && mc_java_print_stream_release(&s, &e) != MC_JAVA_PRINT_OK)
        ok = false;
    if (!print_test_restore(&io))
        ok = false;
    return ok;
}

static bool namespace_and_pipe(void) {
    bool ok = false;
    print_test_io io = {0};
    mc_java_system_output *system = NULL;
    mc_java_print_stream *first = NULL, *held = NULL, *second = NULL;
    mc_java_print_error e;
    mc_java_system_output_state ns;
    mc_java_print_state state;
    NEED(print_test_redirect(&io, true));
    NEED(mc_java_system_output_allocate(&system, &e) == MC_JAVA_PRINT_OK);
    NEED(mc_java_system_output_construct(system, &e) == MC_JAVA_PRINT_OK);
    NEED(mc_java_system_output_get(system, &first, &e) == MC_JAVA_PRINT_OK && first != NULL);
    NEED(mc_java_print_stream_println(first, NULL, &e) == MC_JAVA_PRINT_OK && e.source_returned);
    NEED(mc_java_print_stream_inspect(first, &state) && state.trouble && state.physical_bytes == 0);
    NEED(mc_java_print_stream_allocate(&second, &e) == MC_JAVA_PRINT_OK);
    NEED(mc_java_print_stream_construct_encoded_stdout(second, MC_JAVA_READER_UTF8, true, &e) ==
         MC_JAVA_PRINT_OK);
    NEED(mc_java_system_output_bind(system, second, &e) == MC_JAVA_PRINT_OK);
    NEED(mc_java_system_output_get(system, &held, &e) == MC_JAVA_PRINT_OK && held == second &&
         held != first);
    NEED(mc_java_system_output_bind(system, NULL, &e) == MC_JAVA_PRINT_OK);
    NEED(mc_java_system_output_inspect(system, &ns) && ns.out == NULL && ns.pending_old == NULL);
    NEED(mc_java_print_stream_inspect(first, &state) && state.phase == MC_JAVA_PRINT_READY &&
         state.holds == 1);
    NEED(mc_java_system_output_release(&system, &e) == MC_JAVA_PRINT_OK && system == NULL);
    NEED(mc_java_print_stream_println(held, NULL, &e) == MC_JAVA_PRINT_OK && e.source_returned);
    ok = true;
done:
    if (first != NULL && mc_java_print_stream_release(&first, &e) != MC_JAVA_PRINT_OK)
        ok = false;
    if (held != NULL && mc_java_print_stream_release(&held, &e) != MC_JAVA_PRINT_OK)
        ok = false;
    if (second != NULL && mc_java_print_stream_release(&second, &e) != MC_JAVA_PRINT_OK)
        ok = false;
    if (system != NULL && mc_java_system_output_release(&system, &e) != MC_JAVA_PRINT_OK)
        ok = false;
    if (!print_test_restore(&io))
        ok = false;
    return ok;
}

static bool failed_pending_child(void) {
    bool ok = false;
    print_test_io io = {0};
    mc_java_system_output *system = NULL;
    mc_java_print_error e;
    mc_java_system_output_state ns;
    mc_java_print_state state;
    uint8_t byte;
    NEED(print_test_redirect(&io, false));
    NEED(mc_java_system_output_allocate(&system, &e) == MC_JAVA_PRINT_OK);
    fail_text_once = true;
    NEED(mc_java_system_output_construct(system, &e) == MC_JAVA_PRINT_ALLOCATION_FAILED);
    NEED(!fail_text_once && mc_java_system_output_inspect(system, &ns));
    NEED(ns.phase == MC_JAVA_PRINT_FAILED && ns.out == NULL && ns.pending != NULL);
    NEED(mc_java_print_stream_inspect((mc_java_print_stream *)ns.pending, &state));
    NEED(state.phase == MC_JAVA_PRINT_FAILED && state.char_out_present && !state.text_out_present &&
         state.out_present);
    NEED(print_test_read(&io, &byte, 1) == 0);
    NEED(mc_java_system_output_release(&system, &e) == MC_JAVA_PRINT_OK && system == NULL);
    ok = true;
done:
    fail_text_once = false;
    if (system != NULL && mc_java_system_output_release(&system, &e) != MC_JAVA_PRINT_OK)
        ok = false;
    if (!print_test_restore(&io))
        ok = false;
    return ok;
}

int main(void) {
    if (!fault_case(false, false) || !fault_case(true, false) || !fault_case(false, true) ||
        !unicode_and_buffers() || !namespace_and_pipe() || !failed_pending_child())
        return 1;
    return printf("System PrintStream: %u checks\n", checks) < 0 ? 1 : 0;
}
