#include "mc/core/java/reader.h"

#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

/* These bound the public synthetic replay, not the game or reader API. */
#define MAX_TRANSPORT_BYTES (4u * 1024u * 1024u)
#define MAX_OUTPUT_BYTES (4u * 1024u * 1024u)
#define MAX_ROWS 65536u
#define MAX_OWNERS 1024u
#define MAX_INPUT_BYTES 32768u
#define MAX_LINE_UNITS 32768u
#define MAX_CONTROLS 16u
#define MAX_LINE_BYTES (2u * MAX_INPUT_BYTES + 128u)
#define MAX_ARENA_BYTES (16u * 1024u * 1024u)
#define MAX_CALLBACKS 262144u

typedef enum operation { READ_LINE, CLOSE } operation;

typedef struct declaration {
    char label[65];
    const char *charset_name, *shape_name, *availability_name;
    mc_java_reader_charset charset;
    size_t shape, length, controls;
    int64_t fail_after;
    bool close_fault;
    uint8_t *bytes;
    operation actions[MAX_CONTROLS];
} declaration;

typedef struct trace {
    declaration *cases;
    size_t owners, actions, reads, closes;
} trace;

typedef struct arena_node {
    void *memory;
    struct arena_node *next;
} arena_node;

typedef struct arena {
    arena_node *first;
    size_t bytes;
    bool limited, allocation_failed;
} arena;

typedef struct finite_stream {
    const declaration *input;
    size_t consumed, bulk_calls, available_calls, close_calls;
    bool closed, invalid;
} finite_stream;

typedef struct output {
    char *bytes;
    size_t length, rows;
} output;

static bool fail(const char *message)
{
    fprintf(stderr, "reader probe: %s\n", message);
    return false;
}

static bool increment(size_t *value)
{
    if (*value == SIZE_MAX)
        return false;
    ++*value;
    return true;
}

static bool callback(finite_stream *stream, size_t *counter)
{
    if (*counter >= MAX_CALLBACKS || !increment(counter)) {
        stream->invalid = true;
        return false;
    }
    return true;
}

static void *arena_allocate(void *context, size_t bytes)
{
    arena *memory = context;
    /* At least one byte also supplies distinct storage for a zero-size
     * request under the public allocator contract. No storage is cleared. */
    if (bytes == 0)
        bytes = 1;
    if (bytes > MAX_ARENA_BYTES - sizeof(arena_node) ||
        memory->bytes > MAX_ARENA_BYTES - sizeof(arena_node) - bytes) {
        memory->limited = true;
        return NULL;
    }
    arena_node *node = malloc(sizeof(*node));
    if (node == NULL) {
        memory->allocation_failed = true;
        return NULL;
    }
    void *allocation = malloc(bytes);
    if (allocation == NULL) {
        free(node);
        memory->allocation_failed = true;
        return NULL;
    }
    *node = (arena_node){.memory = allocation, .next = memory->first};
    memory->first = node;
    memory->bytes += sizeof(*node) + bytes;
    /* The caller arena promises storage, not preinitialized reader state. */
    return allocation;
}

static void arena_free(arena *memory)
{
    arena_node *node = memory->first;
    while (node != NULL) {
        arena_node *next = node->next;
        free(node->memory);
        free(node);
        node = next;
    }
    *memory = (arena){0};
}

static const char *status_name(mc_java_reader_status status)
{
    switch (status) {
        case MC_JAVA_READER_OK:
            return "OK";
        case MC_JAVA_READER_INVALID_ARGUMENT:
            return "INVALID_ARGUMENT";
        case MC_JAVA_READER_ALLOCATION_FAILED:
            return "ALLOCATION_FAILED";
        case MC_JAVA_READER_LIMIT:
            return "LIMIT";
        case MC_JAVA_READER_IO_ERROR:
            return "IO_ERROR";
        case MC_JAVA_READER_UNSUPPORTED_CHARSET:
            return "UNSUPPORTED_CHARSET";
        case MC_JAVA_READER_UNIMPLEMENTED_CLOSE:
            return "UNIMPLEMENTED_CLOSE";
        default:
            return "UNKNOWN_NATIVE_STATUS";
    }
}

static bool native_failure(size_t owner, size_t control, mc_java_reader_status status,
                           const arena *memory)
{
    fprintf(stderr,
            "reader probe: native failure owner=%zu control=%zu status=%s "
            "arena_limit=%d arena_allocation=%d\n",
            owner, control, status_name(status), memory->limited ? 1 : 0,
            memory->allocation_failed ? 1 : 0);
    return false;
}

/* 1 is a complete LF row, 0 clean EOF, -1 invalid transport or I/O. */
static int read_row(FILE *file, char *line, size_t *total, size_t *rows)
{
    size_t length = 0;
    for (;;) {
        int c = fgetc(file);
        if (c == EOF)
            return ferror(file) || length != 0 ? -1 : 0;
        if (*total >= MAX_TRANSPORT_BYTES || !increment(total))
            return -1;
        if (c == '\n') {
            if (*rows >= MAX_ROWS || !increment(rows))
                return -1;
            line[length] = '\0';
            return 1;
        }
        if ((c != '\t' && (c < 32 || c > 126)) || length >= MAX_LINE_BYTES)
            return -1;
        line[length++] = (char)c;
    }
}

static size_t fields(char *line, char *parts[7])
{
    size_t count = 1;
    parts[0] = line;
    for (char *cursor = line; *cursor != '\0'; ++cursor) {
        if (*cursor == '\t') {
            if (count == 7)
                return 0;
            *cursor = '\0';
            parts[count++] = cursor + 1;
        }
    }
    for (size_t i = 0; i < count; ++i)
        if (parts[i][0] == '\0')
            return 0;
    return count;
}

static bool number(const char *text, int64_t minimum, int64_t maximum, int64_t *out)
{
    const char *digits = text;
    if (*digits == '-') {
        if (minimum >= 0)
            return false;
        ++digits;
    }
    if (*digits < '0' || *digits > '9' || (digits[0] == '0' && digits[1] != '\0'))
        return false;
    for (const char *cursor = digits; *cursor != '\0'; ++cursor)
        if (*cursor < '0' || *cursor > '9')
            return false;
    if (text[0] == '-' && digits[0] == '0')
        return false;
    errno = 0;
    char *end = NULL;
    intmax_t parsed = strtoimax(text, &end, 10);
    if (errno == ERANGE || end == text || *end != '\0' || parsed < minimum || parsed > maximum)
        return false;
    *out = (int64_t)parsed;
    return true;
}

static bool expected_number(const char *text, size_t expected)
{
    int64_t parsed = 0;
    return number(text, 0, INT32_MAX, &parsed) && (uint64_t)parsed == (uint64_t)expected;
}

static bool label(const char *text)
{
    size_t length = strlen(text);
    if (length == 0 || length > 64)
        return false;
    for (size_t i = 0; i < length; ++i)
        if (!((text[i] >= 'a' && text[i] <= 'z') || (text[i] >= '0' && text[i] <= '9') ||
              text[i] == '-'))
            return false;
    return true;
}

static bool charset(declaration *item, const char *name)
{
    static const char *const names[] = {"UTF-8", "windows-1252", "ISO-8859-1", "US-ASCII"};
    static const mc_java_reader_charset values[] = {MC_JAVA_READER_UTF8, MC_JAVA_READER_WINDOWS1252,
                                                    MC_JAVA_READER_ISO8859_1,
                                                    MC_JAVA_READER_US_ASCII};
    for (size_t i = 0; i < sizeof(values) / sizeof(*values); ++i)
        if (strcmp(name, names[i]) == 0) {
            item->charset_name = names[i];
            item->charset = values[i];
            return true;
        }
    return false;
}

static bool shape(declaration *item, const char *name)
{
    static const char *const names[] = {"ALL", "1", "7", "8192"};
    static const size_t values[] = {INT32_MAX, 1, 7, 8192};
    for (size_t i = 0; i < sizeof(values) / sizeof(*values); ++i)
        if (strcmp(name, names[i]) == 0) {
            item->shape_name = names[i];
            item->shape = values[i];
            return true;
        }
    return false;
}

static bool mode(declaration *item, const char *name)
{
    static const char *const names[] = {"ZERO", "REMAINING", "ERROR"};
    for (size_t i = 0; i < sizeof(names) / sizeof(*names); ++i)
        if (strcmp(name, names[i]) == 0) {
            item->availability_name = names[i];
            return true;
        }
    return false;
}

static int nibble(char value)
{
    if (value >= '0' && value <= '9')
        return value - '0';
    if (value >= 'a' && value <= 'f')
        return value - 'a' + 10;
    return -1;
}

static bool bytes(declaration *item, const char *hex)
{
    if (item->length == 0)
        return strcmp(hex, "-") == 0;
    if (strlen(hex) != 2u * item->length)
        return false;
    item->bytes = malloc(item->length);
    if (item->bytes == NULL)
        return fail("input byte allocation failed");
    for (size_t i = 0; i < item->length; ++i) {
        int high = nibble(hex[2u * i]), low = nibble(hex[2u * i + 1u]);
        if (high < 0 || low < 0)
            return false;
        item->bytes[i] = (uint8_t)(high * 16 + low);
    }
    return true;
}

static bool parse(FILE *file, trace *input, char *line)
{
    size_t total = 0, rows = 0;
    if (read_row(file, line, &total, &rows) != 1 || strcmp(line, "READER_INPUT_V1") != 0)
        return false;
    for (;;) {
        if (read_row(file, line, &total, &rows) != 1)
            return false;
        char *part[7];
        size_t count = fields(line, part);
        if (count == 0)
            return false;
        if (strcmp(part[0], "END") == 0) {
            if (count != 6 || input->owners == 0 || !expected_number(part[1], input->owners) ||
                !expected_number(part[2], input->actions) ||
                !expected_number(part[3], input->reads) ||
                !expected_number(part[4], input->closes) || !expected_number(part[5], rows - 1))
                return false;
            return read_row(file, line, &total, &rows) == 0;
        }
        if (count != 7 || strcmp(part[0], "CASE") != 0 || input->owners >= MAX_OWNERS ||
            !expected_number(part[1], input->owners) || !label(part[2]))
            return false;
        declaration *item = &input->cases[input->owners];
        int64_t controls = 0;
        if (!charset(item, part[3]) || !shape(item, part[4]) || !mode(item, part[5]) ||
            !number(part[6], 0, MAX_CONTROLS, &controls))
            return false;
        memcpy(item->label, part[2], strlen(part[2]) + 1);
        item->controls = (size_t)controls;
        if (read_row(file, line, &total, &rows) != 1)
            return false;
        count = fields(line, part);
        int64_t length = 0, failure = 0, close_fault = 0;
        if (count != 6 || strcmp(part[0], "INPUT") != 0 ||
            !expected_number(part[1], input->owners) ||
            !number(part[4], 0, MAX_INPUT_BYTES, &length) ||
            !number(part[2], -1, length, &failure) || !number(part[3], 0, 1, &close_fault))
            return false;
        item->length = (size_t)length;
        item->fail_after = failure;
        item->close_fault = close_fault != 0;
        if (!bytes(item, part[5]))
            return false;
        for (size_t i = 0; i < item->controls; ++i) {
            if (read_row(file, line, &total, &rows) != 1)
                return false;
            count = fields(line, part);
            if (count != 4 || strcmp(part[0], "QUERY") != 0 ||
                !expected_number(part[1], input->owners) || !expected_number(part[2], i))
                return false;
            if (strcmp(part[3], "READ_LINE") == 0) {
                item->actions[i] = READ_LINE;
                if (!increment(&input->reads))
                    return false;
            } else if (strcmp(part[3], "CLOSE") == 0) {
                item->actions[i] = CLOSE;
                if (!increment(&input->closes))
                    return false;
            } else {
                return false;
            }
            if (!increment(&input->actions))
                return false;
        }
        if (read_row(file, line, &total, &rows) != 1)
            return false;
        count = fields(line, part);
        if (count != 2 || strcmp(part[0], "END_CASE") != 0 ||
            !expected_number(part[1], input->owners) || !increment(&input->owners))
            return false;
    }
}

static mc_java_properties_status stream_read(void *context, uint8_t *destination, size_t capacity,
                                             size_t *count)
{
    finite_stream *stream = context;
    if (!callback(stream, &stream->bulk_calls) || count == NULL || destination == NULL ||
        capacity > 8192) {
        stream->invalid = true;
        return MC_JAVA_PROPERTIES_INVALID_ARGUMENT;
    }
    *count = 0;
    if (capacity == 0)
        return MC_JAVA_PROPERTIES_OK;
    const declaration *item = stream->input;
    if (stream->closed || (item->fail_after >= 0 && stream->consumed >= (size_t)item->fail_after))
        return MC_JAVA_PROPERTIES_IO_ERROR;
    if (stream->consumed == item->length)
        return MC_JAVA_PROPERTIES_OK;
    if (stream->consumed > item->length) {
        stream->invalid = true;
        return MC_JAVA_PROPERTIES_INVALID_ARGUMENT;
    }
    size_t take = item->length - stream->consumed;
    if (take > capacity)
        take = capacity;
    if (take > item->shape)
        take = item->shape;
    if (item->fail_after >= 0 && take > (size_t)item->fail_after - stream->consumed)
        take = (size_t)item->fail_after - stream->consumed;
    if (take == 0 || take > SIZE_MAX - stream->consumed) {
        stream->invalid = true;
        return MC_JAVA_PROPERTIES_INVALID_ARGUMENT;
    }
    memcpy(destination, item->bytes + stream->consumed, take);
    stream->consumed += take;
    *count = take;
    return MC_JAVA_PROPERTIES_OK;
}

static bool stream_ready(void *context)
{
    finite_stream *stream = context;
    if (!callback(stream, &stream->available_calls))
        return false;
    /* ERROR and closed are the actual caught available-IOException case at
     * this public bool seam. No native reader exception is fabricated. */
    return !stream->closed && strcmp(stream->input->availability_name, "REMAINING") == 0 &&
           stream->consumed < stream->input->length;
}

static mc_java_reader_status stream_close(void *context)
{
    finite_stream *stream = context;
    if (!callback(stream, &stream->close_calls))
        return MC_JAVA_READER_LIMIT;
    stream->closed = true;
    return stream->input->close_fault ? MC_JAVA_READER_IO_ERROR : MC_JAVA_READER_OK;
}

static bool append(output *out, const char *format, ...)
{
    size_t remaining = MAX_OUTPUT_BYTES - out->length + 1;
    va_list arguments;
    va_start(arguments, format);
    int length = vsnprintf(out->bytes + out->length, remaining, format, arguments);
    va_end(arguments);
    if (length < 0 || (size_t)length >= remaining)
        return false;
    out->length += (size_t)length;
    return true;
}

static bool row(output *out, const char *format, ...)
{
    if (out->rows >= MAX_ROWS)
        return false;
    size_t remaining = MAX_OUTPUT_BYTES - out->length + 1;
    va_list arguments;
    va_start(arguments, format);
    int length = vsnprintf(out->bytes + out->length, remaining, format, arguments);
    va_end(arguments);
    if (length < 0 || (size_t)length >= remaining)
        return false;
    out->length += (size_t)length;
    if (!append(out, "\n"))
        return false;
    return increment(&out->rows);
}

static bool declaration_rows(output *out, const declaration *item, size_t owner)
{
    if (!row(out, "CASE\t%zu\t%s\t%s\t%s\t%s\t%zu", owner, item->label, item->charset_name,
             item->shape_name, item->availability_name, item->controls) ||
        !append(out, "INPUT\t%zu\t%" PRId64 "\t%d\t%zu\t", owner, item->fail_after,
                item->close_fault ? 1 : 0, item->length))
        return false;
    if (item->length == 0) {
        if (!append(out, "-"))
            return false;
    } else {
        for (size_t i = 0; i < item->length; ++i)
            if (!append(out, "%02x", (unsigned int)item->bytes[i]))
                return false;
    }
    if (!row(out, ""))
        return false;
    for (size_t i = 0; i < item->controls; ++i)
        if (!row(out, "QUERY\t%zu\t%zu\t%s", owner, i,
                 item->actions[i] == READ_LINE ? "READ_LINE" : "CLOSE"))
            return false;
    return true;
}

static bool counters(output *out, const finite_stream *stream)
{
    return append(out, "\t%zu\t%zu\t0\t%zu\t%zu\t%zu\t%d", stream->consumed, stream->bulk_calls,
                  stream->bulk_calls, stream->available_calls, stream->close_calls,
                  stream->closed ? 1 : 0) &&
           row(out, "");
}

static bool line_token(const mc_java_reader_line *line, const mc_java_reader_line **held,
                       size_t *count, char token[24])
{
    if (line == NULL) {
        memcpy(token, "NULL", sizeof("NULL"));
        return true;
    }
    size_t index = 0;
    while (index < *count && held[index] != line)
        ++index;
    if (index == *count) {
        if (*count >= MAX_CONTROLS)
            return false;
        held[index] = line;
        ++*count;
    }
    int length = snprintf(token, 24, "R:%zu", index + 1);
    return length > 0 && length < 24;
}

static bool string_rows(output *out, size_t owner, const mc_java_reader_line *const *held,
                        size_t count)
{
    if (!row(out, "STRINGS\t%zu\t%zu", owner, count))
        return false;
    for (size_t i = 0; i < count; ++i) {
        const mc_java_reader_line *line = held[i];
        if (line->length > MAX_LINE_UNITS || (line->length != 0 && line->units == NULL) ||
            !append(out, "STRING\t%zu\t%zu\t%zu\t", owner, i + 1, line->length))
            return false;
        if (line->length == 0) {
            if (!append(out, "-"))
                return false;
        } else {
            for (size_t j = 0; j < line->length; ++j)
                if (!append(out, "%04x", (unsigned int)line->units[j]))
                    return false;
        }
        if (!row(out, ""))
            return false;
    }
    return true;
}

static bool run_case(output *out, const declaration *item, size_t owner)
{
    bool complete = false;
    arena memory = {0};
    finite_stream stream = {.input = item};
    const mc_java_properties_allocator allocator = {.context = &memory, .allocate = arena_allocate};
    const mc_java_reader_input input = {.bytes = {.context = &stream, .read = stream_read},
                                        .ready = stream_ready,
                                        .close = stream_close};
    const mc_java_reader_line *held[MAX_CONTROLS] = {0};
    size_t line_count = 0;
    mc_java_reader *reader = NULL;
    if (!declaration_rows(out, item, owner))
        goto done;
    mc_java_reader_status status =
        mc_java_reader_create(&allocator, &input, item->charset, &reader);
    if (status != MC_JAVA_READER_OK) {
        (void)native_failure(owner, 0, status, &memory);
        goto done;
    }
    if (reader == NULL || stream.invalid || memory.limited || memory.allocation_failed ||
        !append(out, "CREATE\t%zu", owner) || !counters(out, &stream))
        goto done;
    for (size_t i = 0; i < item->controls; ++i) {
        const mc_java_reader_line sentinel = {0};
        const mc_java_reader_line *line = &sentinel;
        if (item->actions[i] == READ_LINE) {
            status = mc_java_reader_read_line(reader, &line);
            if (status != MC_JAVA_READER_OK) {
                if (line != &sentinel) {
                    (void)fail("read failure changed native output");
                    goto done;
                }
                line = NULL;
            } else if (line == &sentinel) {
                (void)fail("read success left native output unchanged");
                goto done;
            }
        } else {
            line = NULL;
            status = mc_java_reader_close(reader);
        }
        if (stream.invalid || memory.limited || memory.allocation_failed ||
            (status != MC_JAVA_READER_OK && status != MC_JAVA_READER_IO_ERROR)) {
            (void)native_failure(owner, i, status, &memory);
            goto done;
        }
        const char *category = status == MC_JAVA_READER_IO_ERROR               ? "IO"
                               : item->actions[i] == READ_LINE && line == NULL ? "EOF"
                                                                               : "OK";
        char token[24];
        if (!line_token(line, held, &line_count, token) ||
            !append(out, "RESULT\t%zu\t%zu\t%s\t%s\t%s", owner, i,
                    item->actions[i] == READ_LINE ? "READ_LINE" : "CLOSE", category, token) ||
            !counters(out, &stream))
            goto done;
    }
    if (!string_rows(out, owner, held, line_count) || !row(out, "END_CASE\t%zu", owner))
        goto done;
    complete = true;
done:
    arena_free(&memory);
    return complete;
}

int main(int argc, char **argv)
{
    int result = 1;
    FILE *file = NULL;
    char *line = NULL;
    trace input = {0};
    output out = {0};
    if (argc != 2) {
        (void)fail("usage: reader_probe input.tsv");
        goto cleanup;
    }
#ifdef _WIN32
    if (_setmode(_fileno(stdout), _O_BINARY) == -1) {
        (void)fail("stdout binary mode failed");
        goto cleanup;
    }
#endif
#ifdef _MSC_VER
    if (fopen_s(&file, argv[1], "rb") != 0)
        file = NULL;
#else
    file = fopen(argv[1], "rb");
#endif
    line = malloc((size_t)MAX_LINE_BYTES + 1);
    input.cases = calloc(MAX_OWNERS, sizeof(*input.cases));
    if (file == NULL || line == NULL || input.cases == NULL) {
        (void)fail("input open/storage failed");
        goto cleanup;
    }
    /* The whole input grammar closes before any native reader or callback. */
    if (!parse(file, &input, line)) {
        (void)fail("invalid input-only replay transport/declarations/footer");
        goto cleanup;
    }
    int closed = fclose(file);
    file = NULL;
    if (closed != 0) {
        (void)fail("input close failed");
        goto cleanup;
    }
    out.bytes = malloc((size_t)MAX_OUTPUT_BYTES + 1);
    if (out.bytes == NULL) {
        (void)fail("output allocation failed");
        goto cleanup;
    }
    if (!row(&out, "READER_NATIVE_V1"))
        goto cleanup;
    for (size_t i = 0; i < input.owners; ++i)
        if (!run_case(&out, &input.cases[i], i)) {
            (void)fail("native case, identity, counter or trace bound failure");
            goto cleanup;
        }
    size_t prior_rows = out.rows;
    if (!row(&out, "END\t%zu\t%zu\t%zu\t%zu\t%zu", input.owners, input.actions, input.reads,
             input.closes, prior_rows) ||
        fwrite(out.bytes, 1, out.length, stdout) != out.length || fflush(stdout) != 0 ||
        ferror(stdout)) {
        (void)fail("output bound/write failed");
        goto cleanup;
    }
    result = 0;
cleanup:
    if (file != NULL && fclose(file) != 0)
        result = 1;
    if (input.cases != NULL)
        for (size_t i = 0; i < MAX_OWNERS; ++i)
            free(input.cases[i].bytes);
    free(input.cases);
    free(line);
    free(out.bytes);
    return result;
}
