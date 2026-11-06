#include "mc/core/java/writer.h"

#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_WIN32) && !defined(__linux__)
#error "The writer probe host profile is defined only for Windows and Linux"
#endif

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <fcntl.h>
#include <io.h>
#include <wchar.h>
#include <windows.h>
#endif

#define MAX_PLAN (1024u * 1024u)
#define MAX_OUTPUT (32u * 1024u * 1024u)
#define MAX_UNITS 32768u
#define MAX_OPS 64u
#define MAX_FAULTS 32u
#define MAX_EVENTS 128u
#define MAX_ATTEMPT 65536u
#define MAX_SINK (1024u * 1024u)
#define MAX_HEAP (24u * 1024u * 1024u)
#define MAX_BLOCKS 256u

typedef enum method { WRITE, FLUSH, CLOSE } method;

typedef enum opcode {
    OP_WRITE,
    OP_WRITE_CHARS,
    OP_WRITE_INT,
    OP_PRINT,
    OP_PRINTLN,
    OP_PRINT_NULL,
    OP_PRINTLN_NULL,
    OP_NEWLINE,
    OP_FLUSH,
    OP_CLOSE,
    OP_CHECK_ERROR,
    OP_INTERRUPT,
    OP_CLEAR_INTERRUPT
} opcode;

static const char *const methods[] = {"write", "flush", "close"};
static const char *const operations[] = {
    "WRITE",       "WRITE_CHARS",  "WRITE_INT",      "PRINT", "PRINTLN",
    "PRINT_NULL",  "PRINTLN_NULL", "NEWLINE",        "FLUSH", "CLOSE",
    "CHECK_ERROR", "INTERRUPT",    "CLEAR_INTERRUPT"};

typedef struct fault {
    method method;
    size_t occurrence, prefix;
    bool interrupted, used;
    char message[160];
} fault;

typedef struct operation {
    opcode code;
    uint16_t *text;
    size_t length;
    int32_t integer;
} operation;

typedef struct event {
    method method;
    size_t ordinal, occurrence, attempted, committed, sink_size_after;
    uint8_t *bytes;
    int fault_index;
    bool closed_before, closed_after, complete;
    mc_java_writer_status status;
    const char *cause;
} event;

typedef struct snapshot {
    mc_java_writer_state writer;
    size_t sink_length, counts[3];
    bool sink_closed;
} snapshot;

typedef struct owned_block {
    void *pointer;
    size_t bytes;
} owned_block;

typedef struct output {
    FILE *file;
    size_t bytes;
    bool failed;
    int system_error;
    const char *operation;
} output;

typedef struct probe {
    uint8_t *raw, *sink;
    char *parse;
    size_t raw_length, sink_length;
    char id[81];
    bool print_mode, sink_closed, retiring, allocation_failed, retirement_event_recorded;
    mc_java_reader_charset charset;
    const char *charset_name;
    operation ops[MAX_OPS];
    size_t op_count;
    fault faults[MAX_FAULTS];
    size_t fault_count;
    event events[MAX_EVENTS];
    size_t event_count, counts[3], native_cleanup_closes, retirement_callback_attempts;
    event retirement_event;
    size_t gap_count, non_io_statuses;
    owned_block blocks[MAX_BLOCKS];
    size_t block_count, heap_bytes;
    const char *failure;
    int failure_system_error;
    mc_java_writer *writer;
} probe;

static bool require(probe *p, bool value, const char *reason) {
    if (!value && p->failure == NULL)
        p->failure = reason;
    return value;
}

static void fail_io(probe *p, const char *reason, int system_error) {
    if (p->failure == NULL) {
        p->failure = reason;
        p->failure_system_error = system_error;
    }
}

static void output_failure(output *out, const char *operation_name, int system_error) {
    if (!out->failed) {
        out->failed = true;
        out->system_error = system_error;
        out->operation = operation_name;
    }
}

static void *allocate(probe *p, size_t size) {
    if (!require(p, size <= MAX_HEAP - p->heap_bytes && p->block_count < MAX_BLOCKS,
                 "observer aggregate allocation bound")) {
        p->allocation_failed = true;
        return NULL;
    }
    void *value = malloc(size == 0 ? 1 : size);
    if (!require(p, value != NULL, "observer allocation failed")) {
        p->allocation_failed = true;
        return NULL;
    }
    p->blocks[p->block_count++] = (owned_block){value, size};
    p->heap_bytes += size;
    return value;
}

static void release(probe *p) {
    if (p == NULL)
        return;
    for (size_t i = p->block_count; i != 0; --i)
        free(p->blocks[i - 1u].pointer);
    free(p);
}

static bool diagnostic(const char *reason, int system_error) {
    const int result =
        fprintf(stderr, "NATIVE_WRITER_PROBE: %s; system_error=%d\n", reason, system_error);
    const bool written = result >= 0;
    const bool flushed = fflush(stderr) == 0;
    return written && flushed;
}

static void bytes(output *out, const void *data, size_t length) {
    if (out->failed)
        return;
    if (length > MAX_OUTPUT - out->bytes) {
        output_failure(out, "observer output byte bound", 0);
        return;
    }
    errno = 0;
    const size_t done = fwrite(data, 1, length, out->file);
    out->bytes += done;
    if (done != length) {
        output_failure(out, "observer fwrite failed", errno);
    }
}

static void text(output *out, const char *value) {
    bytes(out, value, strlen(value));
}

static void boolean(output *out, bool value) {
    text(out, value ? "true" : "false");
}

static void number(output *out, uint64_t value) {
    char buffer[32];
    size_t length = 0;
    do {
        buffer[sizeof(buffer) - 1u - length++] = (char)('0' + value % 10u);
        value /= 10u;
    } while (value != 0);
    bytes(out, buffer + sizeof(buffer) - length, length);
}

static void signed_number(output *out, int64_t value) {
    if (value < 0) {
        text(out, "-");
        number(out, (uint64_t)(-(value + 1)) + 1u);
    } else {
        number(out, (uint64_t)value);
    }
}

static const char digits[] = "0123456789abcdef";

static void hex(output *out, const uint8_t *value, size_t length) {
    char buffer[4096];
    text(out, "\"");
    for (size_t offset = 0; offset < length;) {
        size_t count = length - offset;
        if (count > sizeof(buffer) / 2u)
            count = sizeof(buffer) / 2u;
        for (size_t i = 0; i < count; ++i) {
            buffer[2u * i] = digits[value[offset + i] >> 4];
            buffer[2u * i + 1u] = digits[value[offset + i] & 15u];
        }
        bytes(out, buffer, 2u * count);
        offset += count;
    }
    text(out, "\"");
}

static void units(output *out, const uint16_t *value, size_t length) {
    char buffer[4096];
    text(out, "\"");
    for (size_t offset = 0; offset < length;) {
        size_t count = length - offset;
        if (count > sizeof(buffer) / 4u)
            count = sizeof(buffer) / 4u;
        for (size_t i = 0; i < count; ++i)
            for (size_t n = 0; n < 4u; ++n)
                buffer[4u * i + n] = digits[(value[offset + i] >> (12u - 4u * n)) & 15u];
        bytes(out, buffer, 4u * count);
        offset += count;
    }
    text(out, "\"");
}

/* Every dynamic string here is an admitted ASCII identifier or a public
 * operation diagnostic, not UTF16 text. Actual UTF16 and path bytes use hex. */
static void string(output *out, const char *value) {
    if (value == NULL) {
        text(out, "null");
        return;
    }
    text(out, "\"");
    for (size_t i = 0; value[i] != '\0'; ++i) {
        const unsigned int c = (unsigned char)value[i];
        if (i >= 256u || c >= 128u) {
            output_failure(out, "observer ASCII diagnostic bound", 0);
            return;
        }
        if (c == '"' || c == '\\') {
            const char pair[2] = {'\\', (char)c};
            bytes(out, pair, sizeof(pair));
        } else if (c < 32u) {
            const char escaped[6] = {'\\', 'u', '0', '0', digits[c >> 4], digits[c & 15u]};
            bytes(out, escaped, sizeof(escaped));
        } else {
            const char byte = (char)c;
            bytes(out, &byte, 1);
        }
    }
    text(out, "\"");
}

static bool row_end(output *out) {
    text(out, "\n");
    if (!out->failed) {
        errno = 0;
        if (fflush(out->file) != 0) {
            output_failure(out, "observer fflush failed", errno);
        }
    }
    return !out->failed;
}

static bool decimal(const char *value, int32_t minimum, int32_t maximum, int32_t *out) {
    const bool negative = value[0] == '-';
    const char *p = value + (negative ? 1u : 0u);
    if (*p == '\0' || (*p == '0' && (negative || p[1] != '\0')))
        return false;
    const uint32_t limit = negative ? UINT32_C(2147483648) : INT32_MAX;
    uint32_t result = 0;
    for (; *p != '\0'; ++p) {
        if (*p < '0' || *p > '9')
            return false;
        const uint32_t digit = (uint32_t)(*p - '0');
        if (result > (limit - digit) / 10u)
            return false;
        result = result * 10u + digit;
    }
    const int32_t signed_value =
        negative ? (result == UINT32_C(2147483648) ? INT32_MIN : -(int32_t)result)
                 : (int32_t)result;
    if (signed_value < minimum || signed_value > maximum)
        return false;
    *out = signed_value;
    return true;
}

static int nibble(char value) {
    if (value >= '0' && value <= '9')
        return value - '0';
    if (value >= 'a' && value <= 'f')
        return value - 'a' + 10;
    return -1;
}

static bool hex_length(const char *value, size_t *length) {
    const size_t count = strlen(value);
    if (count % 4u != 0 || count / 4u > MAX_UNITS)
        return false;
    for (size_t i = 0; i < count; ++i)
        if (nibble(value[i]) < 0)
            return false;
    *length = count / 4u;
    return true;
}

static uint16_t hex_unit(const char *value) {
    uint16_t result = 0;
    for (size_t i = 0; i < 4u; ++i)
        result = (uint16_t)((uint32_t)result * 16u + (unsigned int)nibble(value[i]));
    return result;
}

static bool parse_plan(probe *p) {
    if (!require(p, p->raw_length != 0 && p->raw[p->raw_length - 1u] == '\n',
                 "invalid plan: final LF"))
        return false;
    for (size_t i = 0; i < p->raw_length; ++i)
        if (!require(p, p->raw[i] != 0 && p->raw[i] < 128u && p->raw[i] != '\r',
                     "invalid plan: ASCII/LF framing"))
            return false;
    p->parse = allocate(p, p->raw_length + 1u);
    if (p->parse == NULL)
        return false;
    memcpy(p->parse, p->raw, p->raw_length);
    p->parse[p->raw_length] = '\0';
    size_t line_number = 0;
    char *line = p->parse;
    for (size_t i = 0; i < p->raw_length; ++i) {
        if (p->parse[i] != '\n')
            continue;
        p->parse[i] = '\0';
        char *fields[6] = {line};
        size_t count = 1;
        for (char *c = line; *c != '\0'; ++c)
            if (*c == '\t') {
                if (!require(p, count < 6u, "invalid plan: field count"))
                    return false;
                *c = '\0';
                fields[count++] = c + 1;
            }
        if (line_number == 0) {
            if (!require(p, count == 1 && strcmp(fields[0], "JAVA_WRITER_INPUT_V1") == 0,
                         "invalid plan: header"))
                return false;
        } else if (line_number == 1) {
            if (!require(p, count == 2 && strcmp(fields[0], "case") == 0,
                         "invalid plan: case field"))
                return false;
            const size_t length = strlen(fields[1]);
            if (!require(p, length != 0 && length <= 80u, "invalid plan: case length"))
                return false;
            for (size_t n = 0; n < length; ++n) {
                const char c = fields[1][n];
                if (!require(
                        p, (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || (n != 0 && c == '-'),
                        "invalid plan: case identifier"))
                    return false;
            }
            memcpy(p->id, fields[1], length + 1u);
        } else if (line_number == 2) {
            if (!require(p,
                         count == 2 && strcmp(fields[0], "mode") == 0 &&
                             (strcmp(fields[1], "stream") == 0 || strcmp(fields[1], "print") == 0),
                         "invalid plan: mode"))
                return false;
            p->print_mode = strcmp(fields[1], "print") == 0;
        } else if (line_number == 3) {
            if (!require(p, count == 2 && strcmp(fields[0], "charset") == 0,
                         "invalid plan: charset field"))
                return false;
            static const char *const names[] = {"UTF-8", "windows-1252", "ISO-8859-1", "US-ASCII"};
            static const mc_java_reader_charset charsets[] = {
                MC_JAVA_READER_UTF8, MC_JAVA_READER_WINDOWS1252, MC_JAVA_READER_ISO8859_1,
                MC_JAVA_READER_US_ASCII};
            size_t found = 0;
            while (found < 4u && strcmp(fields[1], names[found]) != 0)
                ++found;
            if (!require(p, found < 4u, "invalid plan: charset"))
                return false;
            p->charset = charsets[found];
            p->charset_name = names[found];
        } else if (strcmp(fields[0], "fault") == 0) {
            if (!require(p, count == 5 && p->op_count == 0 && p->fault_count < MAX_FAULTS,
                         "invalid plan: faults before operations"))
                return false;
            size_t kind = 0;
            while (kind < 3u && strcmp(fields[1], methods[kind]) != 0)
                ++kind;
            int32_t occurrence = 0, prefix = 0;
            if (!require(
                    p,
                    kind < 3u && decimal(fields[2], 0, 127, &occurrence) &&
                        decimal(fields[4], 0, 65536, &prefix) &&
                        (strcmp(fields[3], "io") == 0 || strcmp(fields[3], "interrupt") == 0) &&
                        (kind == WRITE || prefix == 0),
                    "invalid plan: fault declaration"))
                return false;
            for (size_t n = 0; n < p->fault_count; ++n)
                if (!require(p,
                             p->faults[n].method != (method)kind ||
                                 p->faults[n].occurrence != (size_t)occurrence,
                             "invalid plan: duplicate fault"))
                    return false;
            fault *f = &p->faults[p->fault_count++];
            *f = (fault){.method = (method)kind,
                         .occurrence = (size_t)occurrence,
                         .prefix = (size_t)prefix,
                         .interrupted = strcmp(fields[3], "interrupt") == 0};
            const int message = snprintf(f->message, sizeof(f->message), "authored sink %s/%s/%zu",
                                         p->id, methods[kind], f->occurrence);
            if (!require(p, message >= 0 && (size_t)message < sizeof(f->message),
                         "observer diagnostic bound"))
                return false;
        } else {
            if (!require(p, count >= 2 && strcmp(fields[0], "op") == 0 && p->op_count < MAX_OPS,
                         "invalid plan: operation record"))
                return false;
            size_t code = 0;
            while (code < sizeof(operations) / sizeof(operations[0]) &&
                   strcmp(fields[1], operations[code]) != 0)
                ++code;
            if (!require(p, code < sizeof(operations) / sizeof(operations[0]),
                         "invalid plan: operation name"))
                return false;
            operation *op = &p->ops[p->op_count++];
            op->code = (opcode)code;
            const bool typed = code == OP_WRITE || code == OP_WRITE_CHARS || code == OP_PRINT ||
                               code == OP_PRINTLN;
            if (typed) {
                int32_t repeat = 0;
                size_t pattern = 0, suffix = 0;
                if (!require(p,
                             count == 5 && decimal(fields[2], 0, MAX_UNITS, &repeat) &&
                                 hex_length(fields[3], &pattern) &&
                                 (strcmp(fields[4], "-") == 0 || hex_length(fields[4], &suffix)),
                             "invalid plan: counted UTF16 input"))
                    return false;
                if (!require(p, pattern == 0 || (size_t)repeat <= (MAX_UNITS - suffix) / pattern,
                             "invalid plan: expanded UTF16 bound"))
                    return false;
                op->length = pattern * (size_t)repeat + suffix;
                if (op->length != 0) {
                    op->text = allocate(p, op->length * sizeof(*op->text));
                    if (op->text == NULL)
                        return false;
                }
                size_t written = 0;
                for (int32_t n = 0; n < repeat; ++n)
                    for (size_t unit = 0; unit < pattern; ++unit)
                        op->text[written++] = hex_unit(fields[3] + 4u * unit);
                for (size_t unit = 0; unit < suffix; ++unit)
                    op->text[written++] = hex_unit(fields[4] + 4u * unit);
            } else if (code == OP_WRITE_INT) {
                if (!require(p,
                             count == 3 && decimal(fields[2], INT32_MIN, INT32_MAX, &op->integer),
                             "invalid plan: signed write integer"))
                    return false;
            } else if (!require(p, count == 2, "invalid plan: no-argument operation")) {
                return false;
            }
            const bool print_only =
                (code >= OP_PRINT && code <= OP_NEWLINE) || code == OP_CHECK_ERROR;
            if (!require(p, p->print_mode || !print_only,
                         "invalid plan: PrintWriter operation in stream mode"))
                return false;
        }
        ++line_number;
        line = p->parse + i + 1u;
    }
    return require(p, line_number >= 5 && p->op_count != 0,
                   "invalid plan: complete nonempty operations");
}

static mc_java_writer_status bridge_failure(probe *p, event *e, mc_java_writer_error *error) {
    static const char reason[] = "native observer bridge failure; source outcome unqualified";
    if (e != NULL) {
        e->status = MC_JAVA_WRITER_IO_ERROR;
        e->cause = reason;
        e->sink_size_after = p->sink_length;
        e->closed_after = p->sink_closed;
    }
    if (error != NULL)
        *error = (mc_java_writer_error){.status = MC_JAVA_WRITER_IO_ERROR, .operation = reason};
    return MC_JAVA_WRITER_IO_ERROR;
}

static mc_java_writer_status sink_call(probe *p, method which, const uint8_t *data, size_t length,
                                       size_t *committed, mc_java_writer_error *error) {
    if (committed != NULL)
        *committed = 0;
    if (p->retiring) {
        ++p->retirement_callback_attempts;
        if (!require(p, p->retirement_callback_attempts == 1,
                     "native retirement callback observation bound"))
            return bridge_failure(p, NULL, error);
        event *e = &p->retirement_event;
        *e = (event){.method = which,
                     .ordinal = p->event_count,
                     .attempted = length,
                     .fault_index = -1,
                     .closed_before = p->sink_closed};
        p->retirement_event_recorded = true;
        if (!require(p, which == CLOSE && length == 0,
                     "unexpected non-close native retirement callback"))
            return bridge_failure(p, e, error);
        ++p->native_cleanup_closes;
        p->sink_closed = true;
        e->sink_size_after = p->sink_length;
        e->closed_after = p->sink_closed;
        e->complete = true;
        e->status = MC_JAVA_WRITER_OK;
        if (error != NULL)
            *error = (mc_java_writer_error){.status = MC_JAVA_WRITER_OK};
        return MC_JAVA_WRITER_OK;
    }
    const size_t occurrence = p->counts[which]++;
    if (!require(p, p->event_count < MAX_EVENTS && length <= MAX_ATTEMPT,
                 "observer sink event/argument bound"))
        return bridge_failure(p, NULL, error);
    event *e = &p->events[p->event_count];
    *e = (event){.method = which,
                 .ordinal = p->event_count++,
                 .occurrence = occurrence,
                 .attempted = length,
                 .fault_index = -1,
                 .closed_before = p->sink_closed};
    if (length != 0) {
        e->bytes = allocate(p, length);
        if (e->bytes == NULL)
            return bridge_failure(p, e, error);
        memcpy(e->bytes, data, length);
    }
    for (size_t i = 0; i < p->fault_count; ++i)
        if (p->faults[i].method == which && p->faults[i].occurrence == occurrence)
            e->fault_index = (int)i;
    fault *f = e->fault_index < 0 ? NULL : &p->faults[(size_t)e->fault_index];
    mc_java_writer_status status = MC_JAVA_WRITER_OK;
    const char *cause = NULL;
    size_t actual = 0;
    if (p->sink_closed && which != CLOSE) {
        status = MC_JAVA_WRITER_IO_ERROR;
        cause = "authored sink already closed";
    } else {
        if (f != NULL) {
            if (!require(p, f->prefix <= length,
                         "observer fault prefix exceeds actual callback argument"))
                return bridge_failure(p, e, error);
            f->used = true;
            status = f->interrupted ? MC_JAVA_WRITER_INTERRUPTED_IO : MC_JAVA_WRITER_IO_ERROR;
            cause = f->message;
        }
        if (which == WRITE) {
            actual = f == NULL ? length : f->prefix;
            if (!require(p, actual <= MAX_SINK - p->sink_length, "observer committed sink bound"))
                return bridge_failure(p, e, error);
            if (actual != 0)
                memcpy(p->sink + p->sink_length, data, actual);
            p->sink_length += actual;
        }
        if (which == CLOSE)
            p->sink_closed = true;
    }
    if (committed != NULL)
        *committed = actual;
    if (error != NULL)
        *error = (mc_java_writer_error){.status = status, .operation = cause};
    e->committed = actual;
    e->sink_size_after = p->sink_length;
    e->closed_after = p->sink_closed;
    e->complete = true;
    e->status = status;
    e->cause = cause;
    return status;
}

static mc_java_writer_status sink_write(void *context, const uint8_t *data, size_t length,
                                        size_t *committed, mc_java_writer_error *error) {
    return sink_call(context, WRITE, data, length, committed, error);
}

static mc_java_writer_status sink_flush(void *context, mc_java_writer_error *error) {
    return sink_call(context, FLUSH, NULL, 0, NULL, error);
}

static mc_java_writer_status sink_close(void *context, mc_java_writer_error *error) {
    return sink_call(context, CLOSE, NULL, 0, NULL, error);
}

static bool read_snapshot(probe *p, snapshot *out) {
    if (!require(p, mc_java_writer_read_state(p->writer, &out->writer),
                 "public Writer state read failed"))
        return false;
    out->sink_length = p->sink_length;
    memcpy(out->counts, p->counts, sizeof(out->counts));
    out->sink_closed = p->sink_closed;
    return true;
}

static void emit_error(output *out, const mc_java_writer_error *error) {
    text(out, "{\"status\":");
    number(out, (unsigned int)error->status);
    text(out, ",\"system_error\":");
    signed_number(out, error->system_error);
    text(out, ",\"operation\":");
    string(out, error->operation);
    text(out, ",\"native_cleanup_failed\":");
    boolean(out, error->native_cleanup_failed);
    text(out, ",\"native_cleanup_system_error\":");
    signed_number(out, error->native_cleanup_system_error);
    text(out, "}");
}

static void emit_snapshot(probe *p, output *out, const snapshot *s) {
    text(out, "{\"native_charset_value\":");
    number(out, (unsigned int)s->writer.charset);
    text(out, ",\"encoder_closed\":");
    boolean(out, s->writer.encoder_closed);
    text(out, ",\"native_print_writer_out_present\":");
    boolean(out, s->writer.print_writer_out_present);
    text(out, ",\"native_trouble\":");
    boolean(out, s->writer.trouble);
    text(out, ",\"print_writer\":");
    if (p->print_mode) {
        text(out, "{\"out_present\":");
        boolean(out, s->writer.print_writer_out_present);
        text(out, ",\"trouble\":");
        boolean(out, s->writer.trouble);
        text(out, ",\"auto_flush_profile\":false}");
    } else
        text(out, "null");
    text(out, ",\"native_interrupted_write_request\":");
    boolean(out, s->writer.interrupted_write);
    text(out, ",\"source_close_reached\":");
    boolean(out, s->writer.source_close_reached);
    text(out, ",\"sink_close_attempted\":");
    boolean(out, s->writer.sink_close_attempted);
    text(out, ",\"native_file\":");
    boolean(out, s->writer.native_file);
    text(out, ",\"first_io_error\":");
    emit_error(out, &s->writer.first_io_error);
    text(out, ",\"secondary_close_error\":");
    emit_error(out, &s->writer.secondary_close_error);
    text(out, ",\"sink_closed\":");
    boolean(out, s->sink_closed);
    text(out, ",\"sink_bytes\":");
    number(out, s->sink_length);
    text(out, ",\"sink_hex\":");
    hex(out, p->sink, s->sink_length);
    text(out, ",\"sink_call_counts\":{\"write\":");
    number(out, s->counts[WRITE]);
    text(out, ",\"flush\":");
    number(out, s->counts[FLUSH]);
    text(out, ",\"close\":");
    number(out, s->counts[CLOSE]);
    text(out, "},\"thread_interrupted\":null,\"thread_flags_observed\":false}");
}

static void emit_fault(output *out, const fault *f) {
    text(out, "{\"method\":");
    string(out, methods[f->method]);
    text(out, ",\"occurrence\":");
    number(out, f->occurrence);
    text(out, ",\"kind\":");
    string(out, f->interrupted ? "interrupt" : "io");
    text(out, ",\"prefix\":");
    number(out, f->prefix);
    text(out, "}");
}

static void emit_input(output *out, const operation *op) {
    text(out, "{\"op\":");
    string(out, operations[op->code]);
    text(out, ",\"utf16_hex\":");
    if (op->code == OP_WRITE || op->code == OP_WRITE_CHARS || op->code == OP_PRINT ||
        op->code == OP_PRINTLN)
        units(out, op->text, op->length);
    else
        text(out, "null");
    text(out, ",\"integer\":");
    if (op->code == OP_WRITE_INT)
        signed_number(out, op->integer);
    else
        text(out, "null");
    text(out, "}");
}

static void emit_event(probe *p, output *out, const event *e, bool source_phase) {
    text(out, "{\"ordinal\":");
    number(out, e->ordinal);
    text(out, ",\"phase\":");
    string(out, source_phase ? "source-operation" : "native-retirement");
    text(out, ",\"method\":");
    string(out, methods[e->method]);
    text(out, ",\"occurrence\":");
    if (source_phase)
        number(out, e->occurrence);
    else
        text(out, "null");
    text(out, ",\"native_retirement_occurrence\":");
    if (source_phase)
        text(out, "null");
    else
        number(out, 0);
    text(out, ",\"variant\":");
    string(out, e->method == WRITE ? "array" : "none");
    text(out, ",\"attempted_bytes\":");
    number(out, e->attempted);
    text(out, ",\"attempted_hex\":");
    if (e->attempted != 0 && e->bytes == NULL)
        text(out, "null");
    else
        hex(out, e->bytes, e->attempted);
    text(out, ",\"sink_closed_before\":");
    boolean(out, e->closed_before);
    text(out, ",\"configured_fault\":");
    if (e->fault_index >= 0)
        emit_fault(out, &p->faults[(size_t)e->fault_index]);
    else
        text(out, "null");
    text(out, ",\"complete\":");
    boolean(out, e->complete);
    text(out, ",\"committed_bytes\":");
    number(out, e->committed);
    text(out, ",\"sink_size_after\":");
    number(out, e->sink_size_after);
    text(out, ",\"sink_closed_after\":");
    boolean(out, e->closed_after);
    text(out, ",\"native_status\":");
    number(out, (unsigned int)e->status);
    text(out, ",\"outcome\":");
    string(out, !e->complete                     ? "observer-failure"
                : e->status == MC_JAVA_WRITER_OK ? "return"
                                                 : "native-io-error");
    text(out, ",\"error_kind\":");
    string(out, !e->complete || e->status == MC_JAVA_WRITER_OK ? NULL
                : e->status == MC_JAVA_WRITER_INTERRUPTED_IO   ? "interrupt"
                                                               : "io");
    text(out, ",\"operation_diagnostic\":");
    string(out, e->cause);
    text(out, ",\"thread_flags_observed\":false,\"thread_interrupted_on_entry\":null,\"thread_"
              "interrupted_on_exit\":null}");
}

static void emit_events(probe *p, output *out, size_t begin) {
    text(out, "[");
    for (size_t i = begin; i < p->event_count; ++i) {
        if (i != begin)
            text(out, ",");
        emit_event(p, out, &p->events[i], true);
    }
    text(out, "]");
}

static mc_java_writer_status execute(probe *p, const operation *op, bool *returned_boolean,
                                     bool *has_boolean, mc_java_writer_error *error) {
    const mc_java_properties_string_view view = {op->text, op->length};
    *has_boolean = false;
    switch (op->code) {
        case OP_WRITE:
        case OP_WRITE_CHARS:
            return mc_java_writer_write(p->writer, op->text, op->length, error);
        case OP_WRITE_INT: {
            const uint16_t unit = (uint16_t)(uint32_t)op->integer;
            return mc_java_writer_write(p->writer, &unit, 1, error);
        }
        case OP_PRINT:
            return mc_java_writer_print(p->writer, &view, error);
        case OP_PRINTLN:
            return mc_java_writer_println(p->writer, &view, error);
        case OP_PRINT_NULL:
            return mc_java_writer_print(p->writer, NULL, error);
        case OP_PRINTLN_NULL:
            return mc_java_writer_println(p->writer, NULL, error);
        case OP_NEWLINE: {
            const mc_java_properties_string_view empty = {NULL, 0};
            return mc_java_writer_println(p->writer, &empty, error);
        }
        case OP_FLUSH:
            return mc_java_writer_flush(p->writer, error);
        case OP_CLOSE:
            return mc_java_writer_close(p->writer, error);
        case OP_CHECK_ERROR: {
            const mc_java_writer_status status =
                mc_java_writer_check_error(p->writer, returned_boolean, error);
            *has_boolean = status == MC_JAVA_WRITER_OK;
            return status;
        }
        case OP_INTERRUPT:
        case OP_CLEAR_INTERRUPT:
            ++p->gap_count;
            return MC_JAVA_WRITER_OK; /* Not emitted as a successful public/source call. */
    }
    return MC_JAVA_WRITER_INVALID_ARGUMENT;
}

static bool observe(probe *p, output *out, char **argv) {
    text(out, "{\"type\":\"header\",\"schema\":2,\"scope\":\"native-public-Java-writer-input-only-"
              "cut\",\"case\":");
    string(out, p->id);
    text(out, ",\"mode\":");
    string(out, p->print_mode ? "print" : "stream");
    text(out, ",\"charset_input\":");
    string(out, p->charset_name);
    text(out, ",\"plan_bytes\":");
    number(out, p->raw_length);
    text(out, ",\"plan_raw_hex\":");
    hex(out, p->raw, p->raw_length);
    text(out, ",\"input_argv_UTF8_hex\":[");
    for (size_t i = 0; i < 3u; ++i) {
        if (i != 0)
            text(out, ",");
        hex(out, (const uint8_t *)argv[i], strlen(argv[i]));
    }
#ifdef _WIN32
    const mc_java_writer_lines lines = MC_JAVA_WRITER_CRLF;
    text(out, "],\"line_separator_utf16_hex\":\"000d000a\",\"native_platform\":\"Windows\"");
#else
    const mc_java_writer_lines lines = MC_JAVA_WRITER_LF;
    text(out, "],\"line_separator_utf16_hex\":\"000a\",\"native_platform\":\"Linux\"");
#endif
    text(out, ",\"line_profile\":\"native-platform-profile; no source observation "
              "installed\",\"fault_inputs\":[");
    for (size_t i = 0; i < p->fault_count; ++i) {
        if (i != 0)
            text(out, ",");
        emit_fault(out, &p->faults[i]);
    }
    text(out, "],\"operation_inputs\":[");
    for (size_t i = 0; i < p->op_count; ++i) {
        if (i != 0)
            text(out, ",");
        emit_input(out, &p->ops[i]);
    }
    text(out, "],\"Thread_flags_supported\":false,\"native_interruption_semantics\":\"public "
              "sticky request only; no Thread flag simulation or "
              "clearing\",\"NEWLINE_native_route\":\"public println(empty view); additional "
              "zero-length write check\",\"observer_transport\":\"exclusive unbuffered file or "
              "binary stdout; POSIX SIGPIPE temporarily ignored for checked pipe "
              "writes\",\"unpaired_JDK_diagnostics\":[\"private encoder buffers\",\"reference "
              "aliases\",\"Throwable graphs\",\"effective Thread "
              "flags\"],\"GameSettings_or_whole_client_qualified\":false,\"original_native_"
              "comparison_qualified\":false}");
    if (!row_end(out))
        return false;
    const mc_java_writer_output sink = {p, sink_write, sink_flush, sink_close};
    const mc_java_writer_profile profile = {
        p->charset, p->print_mode ? MC_JAVA_WRITER_PRINT : MC_JAVA_WRITER_OUTPUT_STREAM, lines};
    mc_java_writer_error error = {0};
    const mc_java_writer_status created =
        mc_java_writer_create(&sink, &profile, &p->writer, &error);
    if (!require(p, created == MC_JAVA_WRITER_OK && p->writer != NULL,
                 "public Writer construction failed")) {
        text(out, "{\"type\":\"construction-failure\",\"native_status\":");
        number(out, (unsigned int)created);
        text(out, ",\"native_error\":");
        emit_error(out, &error);
        text(out, "}");
        (void)row_end(out);
        return false;
    }
    snapshot state = {0};
    if (!read_snapshot(p, &state))
        return false;
    text(out, "{\"type\":\"constructed\",\"native_status\":");
    number(out, (unsigned int)created);
    text(out, ",\"state\":");
    emit_snapshot(p, out, &state);
    text(out, "}");
    if (!row_end(out))
        return false;
    for (size_t i = 0; i < p->op_count; ++i) {
        const operation *op = &p->ops[i];
        snapshot before = {0}, after = {0};
        if (!read_snapshot(p, &before))
            return false;
        const size_t first_event = p->event_count;
        bool result = false, has_result = false;
        error = (mc_java_writer_error){0};
        const mc_java_writer_status status = execute(p, op, &result, &has_result, &error);
        if (!read_snapshot(p, &after))
            return false;
        const bool thread_gap = op->code == OP_INTERRUPT || op->code == OP_CLEAR_INTERRUPT;
        if (!thread_gap && status != MC_JAVA_WRITER_OK && status != MC_JAVA_WRITER_IO_ERROR &&
            status != MC_JAVA_WRITER_INTERRUPTED_IO)
            ++p->non_io_statuses;
        text(out, "{\"type\":");
        string(out, p->failure == NULL ? "operation" : "observer-failure");
        text(out, ",\"ordinal\":");
        number(out, i);
        text(out, ",\"input\":");
        emit_input(out, op);
        text(out, ",\"before\":");
        emit_snapshot(p, out, &before);
        text(out, ",\"outcome\":");
        string(out, thread_gap                    ? "unpaired-thread-control"
                    : status == MC_JAVA_WRITER_OK ? "return"
                                                  : "native-error-status");
        text(out, ",\"public_call_performed\":");
        boolean(out, !thread_gap);
        text(out, ",\"native_status\":");
        if (thread_gap)
            text(out, "null");
        else
            number(out, (unsigned int)status);
        text(out, ",\"native_error\":");
        if (thread_gap)
            text(out, "null");
        else
            emit_error(out, &error);
        text(out, ",\"return_value_observed\":");
        boolean(out, has_result);
        text(out, ",\"return_value\":");
        if (has_result)
            boolean(out, result);
        else
            text(out, "null");
        text(out, ",\"sink_events\":");
        emit_events(p, out, first_event);
        text(out, ",\"after\":");
        emit_snapshot(p, out, &after);
        text(out, ",\"observer_failure\":");
        string(out, p->failure);
        text(out, ",\"bridge_failure_status_may_have_affected_writer\":");
        boolean(out, p->failure != NULL);
        text(out, "}");
        if (!row_end(out) || p->failure != NULL)
            return false;
    }
    snapshot final_state = {0};
    if (!read_snapshot(p, &final_state))
        return false;
    text(out, "{\"type\":\"terminal\",\"status\":0,\"operations\":");
    number(out, p->op_count);
    text(out, ",\"unpaired_thread_controls\":");
    number(out, p->gap_count);
    text(out, ",\"native_non_io_statuses\":");
    number(out, p->non_io_statuses);
    text(out, ",\"fault_usage\":[");
    for (size_t i = 0; i < p->fault_count; ++i) {
        if (i != 0)
            text(out, ",");
        text(out, "{\"input\":");
        emit_fault(out, &p->faults[i]);
        text(out, ",\"used\":");
        boolean(out, p->faults[i].used);
        text(out, "}");
    }
    text(out, "],\"final_state\":");
    emit_snapshot(p, out, &final_state);
    text(out, ",\"source_close_inserted_by_helper\":false,\"native_retirement_started\":false,"
              "\"whole_writer_or_GameSettings_qualified\":false,\"comparison_qualified\":false}");
    if (!row_end(out))
        return false;
    p->retiring = true;
    mc_java_writer_retirement retirement = {0};
    error = (mc_java_writer_error){0};
    const mc_java_writer_status retired = mc_java_writer_retire(&p->writer, &retirement, &error);
    const bool retired_ok = retired == MC_JAVA_WRITER_OK && p->writer == NULL && p->failure == NULL;
    (void)require(p, retired_ok, "native Writer retirement failed");
    text(out, "{\"type\":\"native-retirement\",\"complete\":");
    boolean(out, retired_ok);
    text(out, ",\"native_status\":");
    number(out, (unsigned int)retired);
    text(out, ",\"native_error\":");
    emit_error(out, &error);
    text(out, ",\"writer_owner_released\":");
    boolean(out, p->writer == NULL);
    text(out, ",\"retirement_outputs_observed\":");
    boolean(out, p->writer == NULL);
    text(out, ",\"observer_failure\":");
    string(out, p->failure);
    text(out, ",\"source_close_reached\":");
    if (p->writer == NULL)
        boolean(out, retirement.source_close_reached);
    else
        text(out, "null");
    text(out, ",\"additional_sink_close\":");
    if (p->writer == NULL)
        boolean(out, retirement.additional_sink_close);
    else
        text(out, "null");
    text(out, ",\"cleanup_close_calls\":");
    number(out, p->native_cleanup_closes);
    text(out, ",\"callback_attempts\":");
    number(out, p->retirement_callback_attempts);
    text(out, ",\"callback_observation_complete\":");
    boolean(out, p->retirement_callback_attempts <= 1);
    text(out, ",\"callbacks\":[");
    if (p->retirement_event_recorded)
        emit_event(p, out, &p->retirement_event, false);
    text(out, "]");
    text(out, ",\"source_fault_schedule_applied\":false,\"source_call_counts_modified\":false,"
              "\"sink_closed_after\":");
    boolean(out, p->sink_closed);
    text(out, ",\"sink_bytes_after\":");
    number(out, p->sink_length);
    text(out, ",\"comparison_qualified\":false}");
    return row_end(out) && retired_ok;
}

static FILE *open_utf8(const char *path, const char *mode) {
    FILE *file = NULL;
#ifdef _WIN32
    size_t length = 0;
    while (length <= MAX_PLAN && path[length] != '\0')
        ++length;
    if (length == 0 || length > MAX_PLAN)
        return NULL;
    const int count =
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, (int)length, NULL, 0);
    if (count == 0)
        return NULL;
    if ((size_t)count > SIZE_MAX / sizeof(wchar_t) - 1u)
        return NULL;
    wchar_t *native = malloc(((size_t)count + 1u) * sizeof(*native));
    if (native == NULL)
        return NULL;
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, (int)length, native, count) ==
        count) {
        native[count] = L'\0';
        const wchar_t *wide_mode = strcmp(mode, "rb") == 0 ? L"rb" : L"wbx";
#ifdef _MSC_VER
        if (_wfopen_s(&file, native, wide_mode) != 0)
            file = NULL;
#else
        file = _wfopen(native, wide_mode);
#endif
    }
    free(native);
#else
    file = fopen(path, mode);
#endif
    return file;
}

static int run(int argc, char **argv) {
    if (argc != 3) {
        return diagnostic("usage: PLAN FRESH_JSONL (or - for stdout)", 0) ? 2 : 1;
    }
    probe *p = calloc(1, sizeof(*p));
    if (p == NULL) {
        if (!diagnostic("observer owner allocation failed", errno))
            return 1;
        return 1;
    }
    p->heap_bytes = sizeof(*p);
    output out = {0};
    FILE *input = NULL;
    int code = 1;
    bool input_valid = false;
#if !defined(_WIN32) && defined(SIGPIPE)
    void (*previous_pipe)(int) = signal(SIGPIPE, SIG_IGN);
    if (previous_pipe == SIG_ERR) {
        p->failure = "observer SIGPIPE profile failed";
        goto cleanup;
    }
#endif
    input = open_utf8(argv[1], "rb");
    if (input == NULL) {
        fail_io(p, "plan input open failed", errno);
        goto cleanup;
    }
    p->raw = allocate(p, MAX_PLAN + 1u);
    if (p->raw == NULL)
        goto cleanup;
    p->raw_length = fread(p->raw, 1, MAX_PLAN + 1u, input);
    if (ferror(input)) {
        fail_io(p, "plan input read failed", errno);
        goto cleanup;
    }
    if (!require(p, p->raw_length <= MAX_PLAN, "invalid plan: complete bounded read")) {
        code = 2;
        goto cleanup;
    }
    FILE *input_to_close = input;
    input = NULL;
    if (fclose(input_to_close) != 0) {
        fail_io(p, "plan input close failed", errno);
        goto cleanup;
    }
    if (!parse_plan(p)) {
        code = p->allocation_failed ? 1 : 2;
        goto cleanup;
    }
    input_valid = true;
    p->sink = allocate(p, MAX_SINK);
    if (p->sink == NULL)
        goto cleanup;
    const bool standard_output = strcmp(argv[2], "-") == 0;
    out.file = standard_output ? stdout : open_utf8(argv[2], "wbx");
    if (out.file == NULL) {
        fail_io(p, "exclusive observer output open failed", errno);
        goto cleanup;
    }
#ifdef _WIN32
    if (standard_output && _setmode(_fileno(stdout), _O_BINARY) == -1) {
        p->failure = "observer binary stdout profile failed";
        goto cleanup;
    }
#endif
    if (!require(p, setvbuf(out.file, NULL, _IONBF, 0) == 0,
                 "observer unbuffered output admission failed"))
        goto cleanup;
    if (observe(p, &out, argv))
        code = 0;
cleanup:
    if (input != NULL) {
        FILE *closing = input;
        input = NULL;
        if (fclose(closing) != 0) {
            const int saved = errno;
            fail_io(p, "plan input cleanup close failed", saved);
            if (!diagnostic("secondary plan cleanup close failure", saved))
                code = 1;
            code = 1;
        }
    }
    if (p->writer != NULL) {
        p->retiring = true;
        mc_java_writer_error error = {0};
        if (mc_java_writer_retire(&p->writer, NULL, &error) != MC_JAVA_WRITER_OK ||
            p->writer != NULL) {
            require(p, false, "native owner cleanup failed");
            code = 1;
        }
    }
    if (out.file != NULL) {
        if (fflush(out.file) != 0)
            output_failure(&out, "observer final fflush failed", errno);
        FILE *closing = out.file;
        out.file = NULL;
        if (fclose(closing) != 0) {
            const int saved = errno;
            output_failure(&out, "observer fclose failed", saved);
            if (!diagnostic("observer fclose failed", saved))
                code = 1;
        }
    }
    if (out.failed) {
        fail_io(p, out.operation == NULL ? "observer output failed" : out.operation,
                out.system_error);
        if (!diagnostic(out.operation == NULL ? "observer output failed" : out.operation,
                        out.system_error))
            code = 1;
        code = 1;
    }
    if (code != 0) {
        if (!diagnostic(p->failure == NULL ? "incomplete observation" : p->failure,
                        p->failure_system_error))
            code = 1;
    }
#if !defined(_WIN32) && defined(SIGPIPE)
    if (previous_pipe != SIG_ERR && signal(SIGPIPE, previous_pipe) == SIG_ERR) {
        if (!diagnostic("observer SIGPIPE restore failed", errno))
            code = 1;
        code = 1;
    }
#endif
    if (!input_valid && code == 0)
        code = 2;
    if (p->writer == NULL)
        release(p);
    else {
        /* The public owner refused retirement. Keep its borrowed context live
         * until process exit rather than freeing behind a retained owner. */
        if (!diagnostic("Writer refused retirement; borrowed context retained", 0))
            code = 1;
        code = 1;
    }
    return code;
}

#ifdef _WIN32
int wmain(int argc, wchar_t **argv) {
    if (argc != 3)
        return run(argc, NULL);
    char *utf8[3] = {0};
    int code = 1;
    bool converted = false;
    for (size_t i = 0; i < 3u; ++i) {
        size_t length = 0;
        while (length <= 32768u && argv[i][length] != L'\0')
            ++length;
        if (length > 32768u)
            goto cleanup;
        const int count = length == 0 ? 0
                                      : WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, argv[i],
                                                            (int)length, NULL, 0, NULL, NULL);
        if ((length != 0 && count == 0) || count < 0 || count > 4 * 32768)
            goto cleanup;
        utf8[i] = malloc((size_t)count + 1u);
        if (utf8[i] == NULL)
            goto cleanup;
        if (count != 0 && WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, argv[i], (int)length,
                                              utf8[i], count, NULL, NULL) != count)
            goto cleanup;
        utf8[i][count] = '\0';
    }
    converted = true;
    code = run(argc, utf8);
cleanup:
    if (!converted) {
        if (!diagnostic("checked UTF16 argument conversion failed", 0))
            code = 1;
    }
    for (size_t i = 0; i < 3u; ++i)
        free(utf8[i]);
    return code;
}
#else
int main(int argc, char **argv) {
    return run(argc, argv);
}
#endif
