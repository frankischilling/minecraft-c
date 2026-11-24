#include "mc/core/java/properties.h"

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

#define MAX_INPUT_BYTES 262144u
#define MAX_STRING_UNITS 65536u
#define MAX_ROWS 65536u
#define MAX_OUTPUT_BYTES (16u * 1024u * 1024u)
#define MAX_TRANSPORT_BYTES (32u * 1024u * 1024u)
#define MAX_LINE_BYTES (2u * MAX_INPUT_BYTES + 512u)
#define MAX_ARENA_BYTES (128u * 1024u * 1024u)
#define MAX_OWNERS 320u
#define MAX_STEPS 3u
#define MAX_QUERIES 64u
#define MAX_IDENTITIES 512u
#define MAX_CELLS 512u

typedef struct arena_node {
    void *memory;
    struct arena_node *next;
} arena_node;

typedef struct arena {
    arena_node *first;
    size_t bytes;
} arena;

/* Match the production C17 flexible-array layout on older MSVC as well. */
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4200)
#endif
typedef struct helper_string {
    size_t length;
    uint16_t units[];
} helper_string;
#ifdef _MSC_VER
#pragma warning(pop)
#endif

typedef enum identity_kind { PROPERTY_STRING, FALLBACK_STRING } identity_kind;

typedef struct identity {
    identity_kind kind;
    const void *reference;
} identity;

typedef enum query_mode { QUERY_ONE, QUERY_TWO_NULL, QUERY_TWO_VALUE } query_mode;

typedef struct query {
    const helper_string *key;
    const helper_string *fallback;
    query_mode mode;
} query;

typedef struct finite_stream {
    const uint8_t *bytes;
    size_t length;
    size_t chunk;
    int64_t fail_after;
    size_t consumed;
    size_t bulk_calls;
    bool invalid;
} finite_stream;

typedef struct owner {
    bool active;
    size_t id;
    size_t declared_steps;
    size_t steps;
    size_t chunk;
    arena memory;
    mc_java_properties *properties;
    identity identities[MAX_IDENTITIES];
    size_t identity_count;
    bool pending;
    const uint8_t *input;
    size_t input_length;
    int64_t fail_after;
    query queries[MAX_QUERIES];
    size_t query_count;
} owner;

typedef struct output {
    char *bytes;
    size_t length;
    size_t rows;
} output;

typedef struct cell {
    const mc_java_properties_string *key;
    const mc_java_properties_string *value;
    mc_java_properties_string_view key_view;
} cell;

static void *arena_allocate(void *context, size_t bytes) {
    arena *memory = context;
    if (bytes == 0 || bytes > MAX_ARENA_BYTES - sizeof(arena_node))
        return NULL;
    const size_t charge = bytes + sizeof(arena_node);
    if (memory->bytes > MAX_ARENA_BYTES - charge)
        return NULL;
    arena_node *node = malloc(sizeof(*node));
    if (node == NULL)
        return NULL;
    void *allocation = malloc(bytes);
    if (allocation == NULL) {
        free(node);
        return NULL;
    }
    node->memory = allocation;
    node->next = memory->first;
    memory->first = node;
    memory->bytes += charge;
    /* Production storage is deliberately not cleared or otherwise prepared. */
    return allocation;
}

static void retire_owner(owner *current) {
    arena_node *node = current->memory.first;
    while (node != NULL) {
        arena_node *next = node->next;
        free(node->memory);
        free(node);
        node = next;
    }
    *current = (owner){0};
}

static bool append_format(output *out, const char *format, ...) {
    const size_t remaining = MAX_OUTPUT_BYTES - out->length + 1u;
    va_list arguments;
    va_start(arguments, format);
    const int length = vsnprintf(out->bytes + out->length, remaining, format, arguments);
    va_end(arguments);
    if (length < 0 || (size_t)length >= remaining)
        return false;
    out->length += (size_t)length;
    return true;
}

static bool finish_row(output *out) {
    if (out->rows >= MAX_ROWS || !append_format(out, "\n"))
        return false;
    ++out->rows;
    return true;
}

static bool row(output *out, const char *format, ...) {
    const size_t remaining = MAX_OUTPUT_BYTES - out->length + 1u;
    va_list arguments;
    va_start(arguments, format);
    const int length = vsnprintf(out->bytes + out->length, remaining, format, arguments);
    va_end(arguments);
    if (length < 0 || (size_t)length >= remaining)
        return false;
    out->length += (size_t)length;
    return finish_row(out);
}

/* 1 is a complete literal-LF row, 0 is clean EOF, -1 is invalid/failed input. */
static int read_row(FILE *file, char *line, size_t *total) {
    size_t length = 0;
    for (;;) {
        const int c = fgetc(file);
        if (c == EOF)
            return ferror(file) || length != 0 ? -1 : 0;
        if (*total >= MAX_TRANSPORT_BYTES)
            return -1;
        ++*total;
        if (c == '\n') {
            line[length] = '\0';
            return 1;
        }
        if ((c != '\t' && (c < 32 || c > 126)) || length >= MAX_LINE_BYTES)
            return -1;
        line[length++] = (char)c;
    }
}

static size_t fields(char *line, char **parts) {
    size_t count = 1;
    parts[0] = line;
    for (char *cursor = line; *cursor != '\0'; ++cursor) {
        if (*cursor == '\t') {
            if (count == 9u)
                return 0;
            *cursor = '\0';
            parts[count++] = cursor + 1;
        }
    }
    return count;
}

static bool number(const char *text, int64_t minimum, int64_t maximum, int64_t *out) {
    const char *digits = text;
    if (*digits == '-') {
        if (minimum >= 0)
            return false;
        ++digits;
    }
    if (*digits < '0' || *digits > '9' || (digits[0] == '0' && digits[1] != '\0'))
        return false;
    for (const char *cursor = digits; *cursor != '\0'; ++cursor) {
        if (*cursor < '0' || *cursor > '9')
            return false;
    }
    if (text[0] == '-' && digits[0] == '0')
        return false;
    errno = 0;
    char *end = NULL;
    const intmax_t value = strtoimax(text, &end, 10);
    if (errno == ERANGE || end == text || *end != '\0' || value < minimum || value > maximum)
        return false;
    *out = (int64_t)value;
    return true;
}

static bool expected_number(const char *text, size_t expected) {
    int64_t value = 0;
    return number(text, 0, INT32_MAX, &value) && (uint64_t)value == (uint64_t)expected;
}

static int nibble(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return -1;
}

static bool byte_input(arena *memory, const char *text, size_t length, const uint8_t **out) {
    if (length == 0) {
        if (strcmp(text, "-") != 0)
            return false;
        *out = NULL;
        return true;
    }
    if (length > MAX_INPUT_BYTES || strlen(text) != length * 2u)
        return false;
    uint8_t *bytes = arena_allocate(memory, length);
    if (bytes == NULL)
        return false;
    for (size_t i = 0; i < length; ++i) {
        const int high = nibble(text[2u * i]);
        const int low = nibble(text[2u * i + 1u]);
        if (high < 0 || low < 0)
            return false;
        bytes[i] = (uint8_t)(high * 16 + low);
    }
    *out = bytes;
    return true;
}

static bool string_input(arena *memory, const char *text, size_t length,
                         const helper_string **out) {
    if (length > MAX_STRING_UNITS || length > (SIZE_MAX - sizeof(helper_string)) / sizeof(uint16_t))
        return false;
    if ((length == 0 && strcmp(text, "-") != 0) || (length != 0 && strlen(text) != length * 4u))
        return false;
    helper_string *string =
        arena_allocate(memory, sizeof(*string) + length * sizeof(*string->units));
    if (string == NULL)
        return false;
    string->length = length;
    for (size_t i = 0; i < length; ++i) {
        uint32_t unit = 0;
        for (size_t digit = 0; digit < 4u; ++digit) {
            const int value = nibble(text[4u * i + digit]);
            if (value < 0)
                return false;
            unit = (unit << 4) + (uint32_t)value;
        }
        string->units[i] = (uint16_t)unit;
    }
    *out = string;
    return true;
}

static mc_java_properties_status stream_read(void *context, uint8_t *destination, size_t capacity,
                                             size_t *count) {
    finite_stream *stream = context;
    if (stream->bulk_calls >= MAX_INPUT_BYTES + 8u || capacity > 8192u || destination == NULL ||
        count == NULL) {
        stream->invalid = true;
        return MC_JAVA_PROPERTIES_INVALID_ARGUMENT;
    }
    ++stream->bulk_calls;
    *count = 0;
    if (capacity == 0)
        return MC_JAVA_PROPERTIES_OK;
    if (stream->fail_after >= 0 && stream->consumed >= (size_t)stream->fail_after)
        return MC_JAVA_PROPERTIES_IO_ERROR;
    if (stream->consumed == stream->length)
        return MC_JAVA_PROPERTIES_OK;
    size_t take = stream->length - stream->consumed;
    if (take > capacity)
        take = capacity;
    if (take > stream->chunk)
        take = stream->chunk;
    if (stream->fail_after >= 0) {
        const size_t until_failure = (size_t)stream->fail_after - stream->consumed;
        if (take > until_failure)
            take = until_failure;
    }
    if (take == 0) {
        stream->invalid = true;
        return MC_JAVA_PROPERTIES_INVALID_ARGUMENT;
    }
    memcpy(destination, stream->bytes + stream->consumed, take);
    stream->consumed += take;
    *count = take;
    return MC_JAVA_PROPERTIES_OK;
}

static bool enroll(owner *current, identity_kind kind, const void *reference, size_t *id) {
    if (reference == NULL) {
        *id = 0;
        return true;
    }
    for (size_t i = 0; i < current->identity_count; ++i) {
        if (current->identities[i].kind == kind && current->identities[i].reference == reference) {
            *id = i + 1u;
            return true;
        }
    }
    if (current->identity_count == MAX_IDENTITIES)
        return false;
    current->identities[current->identity_count] = (identity){.kind = kind, .reference = reference};
    ++current->identity_count;
    *id = current->identity_count;
    return true;
}

static bool token(char *buffer, size_t capacity, size_t id) {
    const int length =
        id == 0 ? snprintf(buffer, capacity, "NULL") : snprintf(buffer, capacity, "R:%zu", id);
    return length >= 0 && (size_t)length < capacity;
}

static int compare_cells(const void *left, const void *right) {
    const cell *a = left;
    const cell *b = right;
    const size_t length =
        a->key_view.length < b->key_view.length ? a->key_view.length : b->key_view.length;
    for (size_t i = 0; i < length; ++i) {
        if (a->key_view.units[i] < b->key_view.units[i])
            return -1;
        if (a->key_view.units[i] > b->key_view.units[i])
            return 1;
    }
    if (a->key_view.length < b->key_view.length)
        return -1;
    return a->key_view.length > b->key_view.length ? 1 : 0;
}

static bool string_row(output *out, size_t owner_id, size_t step, size_t id,
                       mc_java_properties_string_view view) {
    static const char hex[] = "0123456789abcdef";
    if (view.length > MAX_STRING_UNITS ||
        !append_format(out, "STRING\t%zu\t%zu\t%zu\t%zu\t", owner_id, step, id, view.length))
        return false;
    if (view.length == 0) {
        if (!append_format(out, "-"))
            return false;
    } else {
        if (view.length > (MAX_OUTPUT_BYTES - out->length) / 4u)
            return false;
        for (size_t i = 0; i < view.length; ++i) {
            const uint16_t unit = view.units[i];
            out->bytes[out->length++] = hex[(unit >> 12) & 15u];
            out->bytes[out->length++] = hex[(unit >> 8) & 15u];
            out->bytes[out->length++] = hex[(unit >> 4) & 15u];
            out->bytes[out->length++] = hex[unit & 15u];
        }
        out->bytes[out->length] = '\0';
    }
    return finish_row(out);
}

static bool snapshot(owner *current, output *out) {
    const size_t count = mc_java_properties_size(current->properties);
    if (count > MAX_CELLS)
        return false;
    cell cells[MAX_CELLS];
    for (size_t i = 0; i < count; ++i) {
        if (mc_java_properties_entry(current->properties, i, &cells[i].key, &cells[i].value) !=
                MC_JAVA_PROPERTIES_OK ||
            !mc_java_properties_string_read(cells[i].key, &cells[i].key_view) ||
            cells[i].key_view.length > MAX_STRING_UNITS)
            return false;
    }
    qsort(cells, count, sizeof(*cells), compare_cells);
    if (!row(out, "SNAPSHOT\t%zu\t%zu\t%zu", current->id, current->steps, count))
        return false;
    for (size_t i = 0; i < count; ++i) {
        size_t key_id = 0, value_id = 0;
        char key_token[24], value_token[24];
        if (!enroll(current, PROPERTY_STRING, cells[i].key, &key_id) ||
            !enroll(current, PROPERTY_STRING, cells[i].value, &value_id) ||
            !token(key_token, sizeof(key_token), key_id) ||
            !token(value_token, sizeof(value_token), value_id) ||
            !row(out, "ENTRY\t%zu\t%zu\t%s\t%s", current->id, current->steps, key_token,
                 value_token))
            return false;
    }
    for (size_t i = 0; i < current->query_count; ++i) {
        const query *request = &current->queries[i];
        const mc_java_properties_string *returned = NULL;
        if (mc_java_properties_get(current->properties, request->key->units, request->key->length,
                                   &returned) != MC_JAVA_PROPERTIES_OK)
            return false;
        const bool use_fallback = request->mode != QUERY_ONE && returned == NULL;
        size_t returned_id = 0;
        if (use_fallback) {
            if (!enroll(current, FALLBACK_STRING, request->fallback, &returned_id))
                return false;
        } else if (!enroll(current, PROPERTY_STRING, returned, &returned_id)) {
            return false;
        }
        char returned_token[24];
        const char *same = request->mode == QUERY_ONE ? "NA" : use_fallback ? "1" : "0";
        if (!token(returned_token, sizeof(returned_token), returned_id) ||
            !row(out, "GET\t%zu\t%zu\t%zu\t%s\t%s", current->id, current->steps, i, returned_token,
                 same))
            return false;
    }
    if (!row(out, "STRINGS\t%zu\t%zu\t%zu", current->id, current->steps, current->identity_count))
        return false;
    for (size_t i = 0; i < current->identity_count; ++i) {
        const identity *held = &current->identities[i];
        mc_java_properties_string_view view;
        if (held->kind == PROPERTY_STRING) {
            if (!mc_java_properties_string_read(held->reference, &view))
                return false;
        } else {
            const helper_string *fallback = held->reference;
            view = (mc_java_properties_string_view){.units = fallback->units,
                                                    .length = fallback->length};
        }
        if (!string_row(out, current->id, current->steps, i + 1u, view))
            return false;
    }
    return row(out, "END_LOAD\t%zu\t%zu", current->id, current->steps);
}

static bool run_load(owner *current, output *out) {
    finite_stream stream = {.bytes = current->input,
                            .length = current->input_length,
                            .chunk = current->chunk,
                            .fail_after = current->fail_after};
    const mc_java_properties_input input = {.context = &stream, .read = stream_read};
    const mc_java_properties_status status = mc_java_properties_load(current->properties, &input);
    const char *category;
    if (stream.invalid)
        return false;
    if (status == MC_JAVA_PROPERTIES_OK)
        category = "OK";
    else if (status == MC_JAVA_PROPERTIES_IO_ERROR)
        category = "IO";
    else if (status == MC_JAVA_PROPERTIES_MALFORMED_UNICODE)
        category = "MALFORMED";
    else
        return false;
    if (!row(out, "LOAD\t%zu\t%zu\t%s\t%zu\t%zu\t0\t%zu\t0\t0", current->id, current->steps,
             category, stream.consumed, stream.bulk_calls, stream.bulk_calls))
        return false;
    return snapshot(current, out);
}

static bool case_label(const char *label) {
    const size_t length = strlen(label);
    if (length == 0 || length > 64u)
        return false;
    for (size_t i = 0; i < length; ++i) {
        if (!((label[i] >= 'a' && label[i] <= 'z') || (label[i] >= '0' && label[i] <= '9') ||
              label[i] == '-'))
            return false;
    }
    return true;
}

int main(int argc, char **argv) {
    int result = 1;
    FILE *file = NULL;
    char *line = NULL;
    output out = {0};
    owner current = {0};
    size_t total_input = 0, input_rows = 0;
    size_t owners = 0, loads = 0, queries = 0;
    bool complete = false;
    if (argc != 2)
        goto cleanup;
#ifdef _WIN32
    if (_setmode(_fileno(stdout), _O_BINARY) == -1)
        goto cleanup;
#endif
#ifdef _MSC_VER
    if (fopen_s(&file, argv[1], "rb") != 0)
        goto cleanup;
#else
    file = fopen(argv[1], "rb");
#endif
    line = malloc((size_t)MAX_LINE_BYTES + 1u);
    out.bytes = malloc((size_t)MAX_OUTPUT_BYTES + 1u);
    if (file == NULL || line == NULL || out.bytes == NULL)
        goto cleanup;
    if (read_row(file, line, &total_input) != 1 || strcmp(line, "PROPERTIES_INPUT_V1") != 0)
        goto cleanup;
    ++input_rows;
    if (!row(&out, "PROPERTIES_NATIVE_V1"))
        goto cleanup;
    while (!complete) {
        if (read_row(file, line, &total_input) != 1 || ++input_rows > MAX_ROWS)
            goto cleanup;
        char *part[9];
        const size_t count = fields(line, part);
        if (count == 0)
            goto cleanup;
        if (strcmp(part[0], "CASE") == 0) {
            int64_t chunk = 0, steps = 0;
            if (count != 5u || current.active || owners >= MAX_OWNERS ||
                !expected_number(part[1], owners) || !case_label(part[2]) ||
                !number(part[4], 1, MAX_STEPS, &steps))
                goto cleanup;
            if (strcmp(part[3], "ALL") == 0)
                chunk = INT32_MAX;
            else if (!number(part[3], 1, INT32_MAX, &chunk))
                goto cleanup;
            current.active = true;
            current.id = owners;
            current.chunk = (size_t)chunk;
            current.declared_steps = (size_t)steps;
            const mc_java_properties_allocator allocator = {.context = &current.memory,
                                                            .allocate = arena_allocate};
            if (mc_java_properties_create(&allocator, &current.properties) !=
                    MC_JAVA_PROPERTIES_OK ||
                !row(&out, "CASE\t%zu\t%s\t%s\t%zu", current.id, part[2], part[3],
                     current.declared_steps))
                goto cleanup;
        } else if (strcmp(part[0], "INPUT") == 0) {
            int64_t length = 0, failure = 0;
            if (count != 6u || !current.active || current.pending ||
                current.steps >= current.declared_steps || !expected_number(part[1], current.id) ||
                !expected_number(part[2], current.steps) ||
                !number(part[4], 0, MAX_INPUT_BYTES, &length) ||
                !number(part[3], -1, length, &failure) ||
                !byte_input(&current.memory, part[5], (size_t)length, &current.input))
                goto cleanup;
            current.input_length = (size_t)length;
            current.fail_after = failure;
            current.query_count = 0;
            current.pending = true;
        } else if (strcmp(part[0], "QUERY") == 0) {
            int64_t key_length = 0, fallback_length = 0;
            if (count != 9u || !current.active || !current.pending ||
                current.query_count >= MAX_QUERIES || !expected_number(part[1], current.id) ||
                !expected_number(part[2], current.steps) ||
                !expected_number(part[3], current.query_count) ||
                !number(part[4], 0, MAX_STRING_UNITS, &key_length) ||
                !number(part[7], -1, MAX_STRING_UNITS, &fallback_length))
                goto cleanup;
            query request = {0};
            if (strcmp(part[6], "ONE") == 0)
                request.mode = QUERY_ONE;
            else if (strcmp(part[6], "TWO_NULL") == 0)
                request.mode = QUERY_TWO_NULL;
            else if (strcmp(part[6], "TWO_VALUE") == 0)
                request.mode = QUERY_TWO_VALUE;
            else
                goto cleanup;
            if (!string_input(&current.memory, part[5], (size_t)key_length, &request.key))
                goto cleanup;
            if (request.mode == QUERY_TWO_VALUE) {
                if (fallback_length < 0 ||
                    !string_input(&current.memory, part[8], (size_t)fallback_length,
                                  &request.fallback))
                    goto cleanup;
            } else if (fallback_length != -1 || strcmp(part[8], "NULL") != 0) {
                goto cleanup;
            }
            current.queries[current.query_count++] = request;
        } else if (strcmp(part[0], "RUN") == 0) {
            if (count != 3u || !current.active || !current.pending ||
                !expected_number(part[1], current.id) || !expected_number(part[2], current.steps) ||
                !run_load(&current, &out))
                goto cleanup;
            if (queries > SIZE_MAX - current.query_count || loads == SIZE_MAX)
                goto cleanup;
            queries += current.query_count;
            ++loads;
            ++current.steps;
            current.pending = false;
        } else if (strcmp(part[0], "END_CASE") == 0) {
            if (count != 2u || !current.active || current.pending ||
                current.steps != current.declared_steps || !expected_number(part[1], current.id) ||
                !row(&out, "END_CASE\t%zu", current.id))
                goto cleanup;
            retire_owner(&current);
            ++owners;
        } else if (strcmp(part[0], "END") == 0) {
            int64_t original_rows = 0;
            if (count != 5u || current.active || !expected_number(part[1], owners) ||
                !expected_number(part[2], loads) || !expected_number(part[3], queries) ||
                !number(part[4], 0, MAX_ROWS, &original_rows) ||
                read_row(file, line, &total_input) != 0)
                goto cleanup;
            (void)original_rows;
            const size_t rows_before_footer = out.rows;
            if (!row(&out, "END\t%zu\t%zu\t%zu\t%zu", owners, loads, queries, rows_before_footer))
                goto cleanup;
            complete = true;
        } else {
            goto cleanup;
        }
    }
    if (fclose(file) != 0) {
        file = NULL;
        goto cleanup;
    }
    file = NULL;
    if (fwrite(out.bytes, 1, out.length, stdout) != out.length || fflush(stdout) != 0 ||
        ferror(stdout))
        goto cleanup;
    result = 0;
cleanup:
    if (result != 0)
        fprintf(stderr,
                "properties probe: rejected input or native observation failure at row %zu\n",
                input_rows);
    if (file != NULL)
        fclose(file);
    retire_owner(&current);
    free(line);
    free(out.bytes);
    return result;
}
