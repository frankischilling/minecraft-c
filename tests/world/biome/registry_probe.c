#include "mc/resources/files/files.h"
#include "mc/world/biome/registry.h"

#include "../../../src/block/registry/registry_internal.h"
#include "../../../src/world/biome/registry_internal.h"

#include <inttypes.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REQUEST_LIMIT (256u * 1024u)
#define OUTPUT_LIMIT (4u * 1024u * 1024u)
#define OPERATION_LIMIT 4096u
#define REFERENCE_LIMIT 512u
#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

_Static_assert(CHAR_BIT == 8 && sizeof(float) == sizeof(uint32_t) &&
                   sizeof(double) == sizeof(uint64_t),
               "raw binary32/binary64 query storage required");

typedef enum operation_kind {
    OP_FLOAT,
    OP_DOUBLE,
    OP_BYTE,
    OP_TABLE,
    OP_REBUILD,
    OP_SNAPSHOT
} operation_kind;

typedef struct operation {
    operation_kind kind;
    char *fields[5];
    uint64_t first, second;
    int32_t integer;
} operation;

typedef struct requests {
    char *bytes;
    operation values[OPERATION_LIMIT];
    size_t count;
} requests;

typedef enum reference_kind {
    REF_BIOME,
    REF_STRING,
    REF_LIST,
    REF_RECORD,
    REF_CLASS,
    REF_TABLE
} reference_kind;

typedef struct references {
    const void *values[REFERENCE_LIMIT];
    reference_kind kinds[REFERENCE_LIMIT];
    size_t count;
} references;

typedef struct writer {
    FILE *file;
    size_t bytes;
} writer;

typedef struct empty_stream {
    bool opened, closed;
} empty_stream;

typedef struct empty_resources {
    empty_stream streams[3];
    mc_registry_resources source;
} empty_resources;

static FILE *open_file(const char *path, const char *mode)
{
#ifdef _MSC_VER
    FILE *file = NULL;
    return fopen_s(&file, path, mode) == 0 ? file : NULL;
#else
    return fopen(path, mode);
#endif
}

static mc_java_properties_status empty_read(void *context, uint8_t *out, size_t capacity,
                                            size_t *count)
{
    const empty_stream *stream = context;
    if (stream == NULL || out == NULL || capacity == 0 || count == NULL || !stream->opened ||
        stream->closed)
        return MC_JAVA_PROPERTIES_IO_ERROR;
    *count = 0;
    return MC_JAVA_PROPERTIES_OK;
}

static mc_registry_status empty_open(void *context, const char *name, mc_java_properties_input *out)
{
    empty_resources *resources = context;
    const char *const names[] = {"/lang/en_US.lang", "/lang/stats_US.lang", "/achievement/map.txt"};
    if (name == NULL || out == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    for (size_t i = 0; i < COUNT(names); ++i)
        if (strcmp(name, names[i]) == 0) {
            if (resources->streams[i].opened)
                return MC_REGISTRY_INVALID_ARGUMENT;
            resources->streams[i].opened = true;
            *out = (mc_java_properties_input){&resources->streams[i], empty_read};
            return MC_REGISTRY_OK;
        }
    return MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY;
}

static mc_registry_status empty_close(void *context, const mc_java_properties_input *input)
{
    empty_resources *resources = context;
    if (input == NULL || input->read != empty_read)
        return MC_REGISTRY_INVALID_ARGUMENT;
    for (size_t i = 0; i < COUNT(resources->streams); ++i)
        if (input->context == &resources->streams[i] && resources->streams[i].opened &&
            !resources->streams[i].closed) {
            resources->streams[i].closed = true;
            return MC_REGISTRY_OK;
        }
    return MC_REGISTRY_INVALID_ARGUMENT;
}

static bool empty_ready(void *context, const mc_java_properties_input *input)
{
    (void)context;
    (void)input;
    return false;
}

static bool emit(writer *out, const char *format, ...)
{
    char line[1024];
    va_list arguments;
    va_start(arguments, format);
    int length = vsnprintf(line, sizeof(line), format, arguments);
    va_end(arguments);
    if (length < 0 || (size_t)length >= sizeof(line) || (size_t)length > OUTPUT_LIMIT - out->bytes)
        return false;
    if (fwrite(line, 1, (size_t)length, out->file) != (size_t)length)
        return false;
    out->bytes += (size_t)length;
    return true;
}

static bool ref(references *refs, const void *value, reference_kind kind, char text[24],
                size_t *identity)
{
    if (value == NULL) {
        memcpy(text, "NULL", 5);
        *identity = 0;
        return true;
    }
    size_t index = 0;
    while (index < refs->count && refs->values[index] != value)
        ++index;
    if (index == refs->count) {
        if (refs->count == REFERENCE_LIMIT)
            return false;
        refs->values[index] = value;
        refs->kinds[index] = kind;
        ++refs->count;
    }
    if (refs->kinds[index] != kind)
        return false;
    *identity = index + 1;
    int length = snprintf(text, 24, "R:%zu", *identity);
    return length > 0 && length < 24;
}

static bool decimal(const char *text, int32_t minimum, int32_t maximum, int32_t *out)
{
    bool negative = *text == '-';
    const char *digits = text + (negative ? 1 : 0);
    size_t length = strlen(digits);
    if (length == 0 || length > 10 || (digits[0] == '0' && (length != 1 || negative)))
        return false;
    uint64_t magnitude = 0;
    for (size_t i = 0; i < length; ++i) {
        if (digits[i] < '0' || digits[i] > '9')
            return false;
        magnitude = magnitude * UINT64_C(10) + (uint64_t)(digits[i] - '0');
    }
    int64_t value = negative ? -(int64_t)magnitude : (int64_t)magnitude;
    if (value < minimum || value > maximum)
        return false;
    *out = (int32_t)value;
    return true;
}

static bool hexadecimal(const char *text, size_t digits, uint64_t *out)
{
    if (strlen(text) != digits)
        return false;
    uint64_t value = 0;
    for (size_t i = 0; i < digits; ++i) {
        unsigned int nibble;
        if (text[i] >= '0' && text[i] <= '9')
            nibble = (unsigned int)(text[i] - '0');
        else if (text[i] >= 'a' && text[i] <= 'f')
            nibble = (unsigned int)(text[i] - 'a') + 10u;
        else
            return false;
        value = (value << 4) | nibble;
    }
    *out = value;
    return true;
}

static bool selector(const char *text)
{
    return text[0] >= 'a' && text[0] <= 'm' && text[1] == '\0';
}

static bool label(const char *text)
{
    size_t length = strlen(text);
    if (length == 0 || length > 48 || strcmp(text, "baseline") == 0 ||
        strcmp(text, "warm-rebuild") == 0)
        return false;
    for (size_t i = 0; i < length; ++i)
        if (!((text[i] >= 'A' && text[i] <= 'Z') || (text[i] >= 'a' && text[i] <= 'z') ||
              (text[i] >= '0' && text[i] <= '9') || text[i] == '_' || text[i] == '.' ||
              text[i] == '-'))
            return false;
    return true;
}

static char *next_line(char **cursor)
{
    char *line = *cursor;
    char *end = strchr(line, '\n');
    if (end == NULL)
        return NULL;
    *end = '\0';
    *cursor = end + 1;
    return line;
}

static bool parse_requests(requests *input, const char *path)
{
    FILE *file = open_file(path, "rb");
    if (file == NULL)
        return false;
    input->bytes = malloc(REQUEST_LIMIT + 2u);
    if (input->bytes == NULL) {
        (void)fclose(file);
        return false;
    }
    size_t length = fread(input->bytes, 1, REQUEST_LIMIT + 1u, file);
    bool valid =
        !ferror(file) && length != 0 && length <= REQUEST_LIMIT && input->bytes[length - 1] == '\n';
    if (fclose(file) != 0)
        valid = false;
    if (!valid)
        return false;
    for (size_t i = 0; i < length; ++i) {
        unsigned char c = (unsigned char)input->bytes[i];
        if (c != '\n' && c != '\t' && (c < 32u || c >= 127u))
            return false;
    }
    input->bytes[length] = '\0';
    char *cursor = input->bytes;
    char *line = next_line(&cursor);
    if (line == NULL || strcmp(line, "BIOME_REGISTRY_REQUEST_V1") != 0)
        return false;
    size_t snapshots = 2;
    while ((line = next_line(&cursor)) != NULL) {
        if (strcmp(line, "END") == 0)
            return *cursor == '\0';
        if (input->count == OPERATION_LIMIT)
            return false;
        operation *op = &input->values[input->count];
        size_t columns = 1;
        op->fields[0] = line;
        for (char *at = line; *at != '\0'; ++at)
            if (*at == '\t') {
                if (columns == COUNT(op->fields))
                    return false;
                *at = '\0';
                op->fields[columns++] = at + 1;
            }
        if (columns < 2 || !label(op->fields[1]))
            return false;
        for (size_t i = 0; i < input->count; ++i)
            if (strcmp(op->fields[1], input->values[i].fields[1]) == 0)
                return false;
        if (strcmp(op->fields[0], "F") == 0 || strcmp(op->fields[0], "D") == 0) {
            op->kind = op->fields[0][0] == 'F' ? OP_FLOAT : OP_DOUBLE;
            size_t digits = op->kind == OP_FLOAT ? 8u : 16u;
            if (columns != 4 || !hexadecimal(op->fields[2], digits, &op->first) ||
                !hexadecimal(op->fields[3], digits, &op->second))
                return false;
        } else if (strcmp(op->fields[0], "SET_BYTE") == 0) {
            op->kind = OP_BYTE;
            if (columns != 5 || !selector(op->fields[2]) ||
                (strcmp(op->fields[3], "p") != 0 && strcmp(op->fields[3], "q") != 0) ||
                !decimal(op->fields[4], -128, 127, &op->integer))
                return false;
        } else if (strcmp(op->fields[0], "SET_TABLE") == 0) {
            op->kind = OP_TABLE;
            if (columns != 4 || (!selector(op->fields[3]) && strcmp(op->fields[3], "NULL") != 0) ||
                !decimal(op->fields[2], 0, 4095, &op->integer))
                return false;
        } else if (strcmp(op->fields[0], "REBUILD") == 0) {
            op->kind = OP_REBUILD;
            if (columns != 2)
                return false;
        } else if (strcmp(op->fields[0], "SNAPSHOT") == 0) {
            op->kind = OP_SNAPSHOT;
            if (columns != 2 || ++snapshots > 8)
                return false;
        } else {
            return false;
        }
        ++input->count;
    }
    return false;
}

static const char *biome_class(const mc_biome *biome, mc_registry_side side)
{
    static const char *const client[] = {"kd", "fs", "tf", "t", "ry", "yj", "rb", "g"};
    static const char *const server[] = {"gs", "dw", "ma", "m", "le", "pp", "kk", "f"};
    mc_biome_kind kind = mc_biome_kind_get(biome);
    if ((unsigned int)kind >= COUNT(client))
        return NULL;
    return side == MC_REGISTRY_CLIENT ? client[(size_t)kind] : server[(size_t)kind];
}

static bool string_units(const mc_registry_string *string, size_t *length, char text[61])
{
    mc_registry_string_view view;
    if (!mc_registry_string_read(string, &view) || view.length > 15 ||
        (view.length != 0 && view.units == NULL))
        return false;
    *length = view.length;
    if (view.length == 0) {
        memcpy(text, "-", 2);
        return true;
    }
    const char *hex = "0123456789abcdef";
    for (size_t i = 0; i < view.length; ++i)
        for (size_t j = 0; j < 4; ++j)
            text[i * 4 + j] = hex[((uint32_t)view.units[i] >> (unsigned int)((3u - j) * 4u)) & 15u];
    text[view.length * 4] = '\0';
    return true;
}

static bool snapshot(writer *out, references *refs, mc_registry_classes *owner,
                     mc_registry_side side, const char *name, bool controlled,
                     const void **original_table)
{
    mc_biome_registry_view view;
    if (mc_biome_registry_inspect(owner, &view, NULL) != MC_REGISTRY_OK ||
        view.phase != MC_REGISTRY_READY || view.x == NULL || view.x_length != 4096)
        return false;
    const mc_biome *biomes[] = {view.a, view.b, view.c, view.d, view.e, view.f, view.g,
                                view.h, view.i, view.j, view.k, view.l, view.m};
    bool seen_biomes[REFERENCE_LIMIT + 1] = {false}, seen_strings[REFERENCE_LIMIT + 1] = {false},
                                       seen_lists[REFERENCE_LIMIT + 1] = {false},
                                       seen_records[REFERENCE_LIMIT + 1] = {false},
                                       seen_classes[REFERENCE_LIMIT + 1] = {false};
    size_t biome_count = 0, string_count = 0, list_count = 0, record_count = 0, class_count = 0;
    if (!emit(out, "SNAPSHOT\t%s\n", name))
        return false;
    for (size_t i = 0; i < COUNT(biomes); ++i) {
        const mc_biome_fields *fields = mc_biome_fields_get(biomes[i]);
        const char *actual_class = biome_class(biomes[i], side);
        char biome_ref[24], name_ref[24];
        size_t biome_id, name_id;
        if (fields == NULL || fields->n == NULL || actual_class == NULL ||
            !ref(refs, biomes[i], REF_BIOME, biome_ref, &biome_id) || seen_biomes[biome_id] ||
            !ref(refs, fields->n, REF_STRING, name_ref, &name_id))
            return false;
        seen_biomes[biome_id] = true;
        ++biome_count;
        if (!emit(out, "NAMED\t%s\t%c\t%s\n", name, (int)('a' + i), biome_ref) ||
            !emit(out, "BIOME\t%s\t%s\t%s\t%s\t%" PRId32 "\t%d\t%d\t%" PRId32 "\t%d\t%d\n", name,
                  biome_ref, actual_class, name_ref, fields->o, (int)fields->p, (int)fields->q,
                  fields->r, fields->v ? 1 : 0, fields->w ? 1 : 0))
            return false;
        if (!seen_strings[name_id]) {
            char units[61];
            size_t length;
            if (!string_units(fields->n, &length, units) ||
                !emit(out, "STRING\t%s\t%s\t%zu\t%s\n", name, name_ref, length, units))
                return false;
            seen_strings[name_id] = true;
            ++string_count;
        }
        const mc_biome_spawn_list *lists[] = {fields->s, fields->t, fields->u};
        for (size_t j = 0; j < COUNT(lists); ++j) {
            char list_ref[24];
            size_t list_id, length = mc_biome_spawn_list_size(lists[j]);
            if (lists[j] == NULL || length > 5 ||
                !ref(refs, lists[j], REF_LIST, list_ref, &list_id) || seen_lists[list_id] ||
                !emit(out, "LIST\t%s\t%s\t%c\t%s\t%zu\n", name, biome_ref, (int)('s' + j), list_ref,
                      length))
                return false;
            seen_lists[list_id] = true;
            ++list_count;
            for (size_t k = 0; k < length; ++k) {
                const mc_biome_spawn *record = mc_biome_spawn_list_at(lists[j], k);
                const mc_biome_spawn_fields *spawn = mc_biome_spawn_fields_get(record);
                char record_ref[24], class_ref[24];
                size_t record_id, class_id;
                if (spawn == NULL || spawn->a == NULL ||
                    !ref(refs, record, REF_RECORD, record_ref, &record_id) ||
                    seen_records[record_id] ||
                    !ref(refs, spawn->a, REF_CLASS, class_ref, &class_id) ||
                    !emit(out, "ELEMENT\t%s\t%s\t%zu\t%s\n", name, list_ref, k, record_ref) ||
                    !emit(out, "RECORD\t%s\t%s\t%s\t%" PRId32 "\n", name, record_ref, class_ref,
                          spawn->b))
                    return false;
                seen_records[record_id] = true;
                ++record_count;
                if (!seen_classes[class_id]) {
                    const char *class_name = mc_biome_entity_class_name(spawn->a, side);
                    if (class_name == NULL ||
                        !emit(out, "ENTITY_CLASS\t%s\t%s\t%s\n", name, class_ref, class_name))
                        return false;
                    seen_classes[class_id] = true;
                    ++class_count;
                }
            }
        }
    }
    if (biome_count != 13 || string_count != 13 || list_count != 39 || record_count != 115 ||
        class_count != 13)
        return false;
    if (*original_table == NULL)
        *original_table = view.x;
    if (*original_table != view.x)
        return false;
    char table_ref[24];
    size_t table_id;
    if (!ref(refs, view.x, REF_TABLE, table_ref, &table_id) ||
        !emit(out, "TABLE\t%s\t%s\t%zu\n", name, table_ref, view.x_length))
        return false;
    for (size_t i = 0; i < view.x_length; ++i) {
        bool known = view.x[i] == NULL && controlled;
        for (size_t j = 0; j < COUNT(biomes); ++j)
            if (view.x[i] == biomes[j])
                known = true;
        char value[24];
        size_t identity;
        if (!known || !ref(refs, view.x[i], REF_BIOME, value, &identity) ||
            !emit(out, "TABLE_SLOT\t%s\t%zu\t%s\n", name, i, value))
            return false;
    }
    return emit(out, "END_SNAPSHOT\t%s\t%zu\t%zu\t%zu\t%zu\n", name, biome_count, list_count,
                record_count, class_count);
}

/* A comparison projection of actual native statuses, never C Throwable identity.
 * Resource, FP, native budget/admission and initialization failures remain fatal. */
static const char *target_exception(mc_registry_status status)
{
    if (status == MC_REGISTRY_NULL_REFERENCE)
        return "java.lang.NullPointerException";
    if (status == MC_REGISTRY_INDEX_OUT_OF_RANGE)
        return "java.lang.ArrayIndexOutOfBoundsException";
    return NULL;
}

static bool query(writer *out, references *refs, mc_registry_classes *owner, const operation *op,
                  mc_registry_error *error)
{
    const mc_biome *result = NULL;
    mc_registry_status status;
    if (op->kind == OP_FLOAT) {
        uint32_t first_bits = (uint32_t)op->first, second_bits = (uint32_t)op->second;
        float first, second;
        memcpy(&first, &first_bits, sizeof(first));
        memcpy(&second, &second_bits, sizeof(second));
        status = mc_biome_registry_classify_float(owner, first, second, &result, error);
    } else {
        double first, second;
        memcpy(&first, &op->first, sizeof(first));
        memcpy(&second, &op->second, sizeof(second));
        status = mc_biome_registry_lookup(owner, first, second, &result, error);
    }
    if (status != MC_REGISTRY_OK) {
        const char *exception = target_exception(status);
        return exception != NULL && emit(out, "QUERY\t%s\t%s\t%s\t%s\tTHROW\t%s\n", op->fields[1],
                                         op->fields[0], op->fields[2], op->fields[3], exception);
    }
    bool known = result == NULL;
    for (size_t i = 0; i < 13; ++i)
        if (result == owner->biomes.named[i])
            known = true;
    char value[24];
    size_t identity;
    return known && ref(refs, result, REF_BIOME, value, &identity) &&
           emit(out, "QUERY\t%s\t%s\t%s\t%s\tRETURN\t%s\n", op->fields[1], op->fields[0],
                op->fields[2], op->fields[3], value);
}

int main(int argc, char **argv)
{
    int result = 1;
    requests *input = NULL;
    references *refs = NULL;
    writer output = {0};
    mc_materials *materials = NULL;
    mc_java_random_profile *profile = NULL;
    mc_registry_classes *owner = NULL;
    mc_resource_files *files = NULL;
    empty_resources fallback = {0};
    mc_registry_error error = {0};
    const void *original_table = NULL;
    size_t snapshots = 0;
    if ((argc != 4 && argc != 6) ||
        (strcmp(argv[1], "client") != 0 && strcmp(argv[1], "server") != 0) ||
        (argc == 6 && strcmp(argv[4], "--resources-dir") != 0))
        goto cleanup;
    input = calloc(1, sizeof(*input));
    refs = calloc(1, sizeof(*refs));
    if (input == NULL || refs == NULL || !parse_requests(input, argv[2]))
        goto cleanup;
    output.file = open_file(argv[3], "wbx");
    if (output.file == NULL)
        goto cleanup;
    fallback.source = (mc_registry_resources){&fallback, empty_open, empty_close, empty_ready};
    const mc_registry_resources *resources = &fallback.source;
    if (argc == 6) {
        if (mc_resource_files_create(argv[5], &files) != MC_REGISTRY_OK)
            goto cleanup;
        resources = mc_resource_files_source(files);
    }
    if (!mc_materials_create(&materials) ||
        mc_java_random_profile_create(SIZE_MAX, &profile) != MC_JAVA_RANDOM_PROFILE_OK)
        goto cleanup;
    mc_registry_side side =
        strcmp(argv[1], "client") == 0 ? MC_REGISTRY_CLIENT : MC_REGISTRY_SERVER;
    const mc_registry_options options = {side, profile, materials, 0, resources};
    if (mc_registry_classes_allocate(&options, &owner, &error) != MC_REGISTRY_OK ||
        !emit(&output, "BIOME_REGISTRY_V1\t%s\n", argv[1]) ||
        !emit(&output, "POLICY\tNATIVE_TYPED_BIOME_BOOTSTRAP\tEXPLICIT_MUTATION_CONTROLS\tNOT_FULL_"
                       "WORLD_OR_GAME_STARTUP\n") ||
        mc_registry_classes_initialize_biomes(owner, &error) != MC_REGISTRY_OK ||
        !emit(&output, "BOOTSTRAP\tRETURN\n") ||
        !snapshot(&output, refs, owner, side, "baseline", false, &original_table))
        goto cleanup;
    ++snapshots;
    if (mc_biome_table_rebuild(owner, &error) != MC_REGISTRY_OK ||
        !snapshot(&output, refs, owner, side, "warm-rebuild", false, &original_table))
        goto cleanup;
    ++snapshots;
    for (size_t i = 0; i < input->count; ++i) {
        const operation *op = &input->values[i];
        switch (op->kind) {
            case OP_FLOAT:
            case OP_DOUBLE:
                if (!query(&output, refs, owner, op, &error))
                    goto cleanup;
                break;
            case OP_BYTE: {
                if (owner->entered != 0)
                    goto cleanup;
                ++owner->entered;
                mc_registry_status status = mc_registry_biome_byte_store(
                    owner, owner->biomes.named[(size_t)(op->fields[2][0] - 'a')], op->fields[3][0],
                    (int8_t)op->integer);
                --owner->entered;
                if (status != MC_REGISTRY_OK ||
                    !emit(&output, "CONTROL\t%s\tSET_BYTE\t%s\t%s\t%s\n", op->fields[1],
                          op->fields[2], op->fields[3], op->fields[4]))
                    goto cleanup;
                break;
            }
            case OP_TABLE:
                if (owner->biomes.x == NULL || owner->biomes.x_length != 4096)
                    goto cleanup;
                owner->biomes.x[(size_t)op->integer] =
                    strcmp(op->fields[3], "NULL") == 0
                        ? NULL
                        : owner->biomes.named[(size_t)(op->fields[3][0] - 'a')];
                if (!emit(&output, "CONTROL\t%s\tSET_TABLE\t%s\t%s\n", op->fields[1], op->fields[2],
                          op->fields[3]))
                    goto cleanup;
                break;
            case OP_REBUILD: {
                mc_registry_status status = mc_biome_table_rebuild(owner, &error);
                if (status == MC_REGISTRY_OK) {
                    if (!emit(&output, "CONTROL\t%s\tREBUILD\tRETURN\n", op->fields[1]))
                        goto cleanup;
                } else {
                    const char *exception = target_exception(status);
                    if (exception == NULL || !emit(&output, "CONTROL\t%s\tREBUILD\tTHROW\t%s\n",
                                                   op->fields[1], exception))
                        goto cleanup;
                }
                if (owner->biomes.x != original_table)
                    goto cleanup;
                break;
            }
            case OP_SNAPSHOT:
                if (!snapshot(&output, refs, owner, side, op->fields[1], true, &original_table))
                    goto cleanup;
                ++snapshots;
                break;
        }
    }
    if (refs->count != 194 ||
        !emit(&output, "END\t%zu\t%zu\t%zu\n", snapshots, input->count, refs->count) ||
        fflush(output.file) != 0 || ferror(output.file))
        goto cleanup;
    result = 0;
cleanup:
    if (result != 0 && owner != NULL)
        (void)fprintf(
            stderr,
            "Biome probe failed; last registry diagnostic status=%d source=%s pc=%" PRId32 "\n",
            (int)error.status, error.constructor_class == NULL ? "NULL" : error.constructor_class,
            error.constructor_pc);
    if (output.file != NULL && fclose(output.file) != 0)
        result = 1;
    if (input != NULL)
        free(input->bytes);
    free(input);
    free(refs);
    if (mc_registry_classes_free(owner, NULL) != MC_REGISTRY_OK)
        return 1; /* Retain dependencies if an unexpectedly entered owner cannot retire. */
    if (files != NULL && mc_resource_files_free(files) != MC_REGISTRY_OK)
        result = 1;
    mc_materials_destroy(materials);
    mc_java_random_profile_free(profile);
    return result;
}
