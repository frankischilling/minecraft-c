#include "internal.h"
#include <limits.h>
#include <string.h>

typedef enum json_kind {
    JSON_OBJECT,
    JSON_ARRAY,
    JSON_STRING,
    JSON_NUMBER,
    JSON_TRUE,
    JSON_FALSE,
    JSON_NULL
} json_kind;
typedef struct json_node json_node;

typedef struct json_field {
    struct json_field *next;
    mc_client_stats_string key;
    json_node *value;
} json_field;

struct json_node {
    json_kind kind;
    mc_client_stats_string text;
    json_node *next, *children, *last;
    json_field *fields, *fields_last;
};

typedef struct json_parser {
    stats_arena arena;
    mc_client_stats_string input;
    size_t offset, nodes, maximum_nodes, maximum_depth;
    mc_client_stats_error *error;
} json_parser;

static bool equal(mc_client_stats_string a, mc_client_stats_string b) {
    return a.length == b.length && (a.length == 0 || memcmp(a.units, b.units, a.length * 2u) == 0);
}

static bool ascii_equal(mc_client_stats_string value, const char *text) {
    size_t i = 0;
    while (text[i] != '\0') {
        if (i >= value.length || value.units[i] != (unsigned char)text[i])
            return false;
        ++i;
    }
    return i == value.length;
}

static bool syntax(json_parser *parser) {
    parser->error->text_offset = parser->offset;
    stats_exception(parser->error, MC_CLIENT_STATS_JSON_SYNTAX, "Argo JSON syntax", "hx", "a",
                    "(Ljava/lang/String;)Lqe;", 9);
    return false;
}

static void whitespace(json_parser *p) {
    while (p->offset < p->input.length) {
        const uint16_t c = p->input.units[p->offset];
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n')
            break;
        ++p->offset;
    }
}

static void *allocate(json_parser *p, size_t bytes) {
    void *storage = stats_arena_allocate(&p->arena, bytes);
    if (storage == NULL)
        stats_fail(p->error, p->arena.failure, "Argo native temporary allocation", NULL, NULL, NULL,
                   0);
    return storage;
}

static json_node *node(json_parser *p, json_kind kind) {
    if (p->nodes >= p->maximum_nodes) {
        stats_fail(p->error, MC_CLIENT_STATS_LIMIT, "Argo native node count", NULL, NULL, NULL, 0);
        return NULL;
    }
    json_node *result = allocate(p, sizeof(*result));
    if (result != NULL) {
        *result = (json_node){.kind = kind};
        ++p->nodes;
    }
    return result;
}

static int hex(uint16_t c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

static bool string(json_parser *p, mc_client_stats_string *out) {
    if (p->offset >= p->input.length || p->input.units[p->offset++] != '"')
        return syntax(p);
    const size_t start = p->offset;
    size_t scan = start, length = 0;
    bool escaped = false;
    while (scan < p->input.length) {
        const uint16_t c = p->input.units[scan++];
        if (!escaped && c == '"')
            break;
        if (!escaped && c == '\\')
            escaped = true;
        else {
            escaped = false;
            ++length;
        }
    }
    if (scan == p->input.length && (scan == start || p->input.units[scan - 1] != '"' || escaped))
        return syntax(p);
    uint16_t *units = allocate(p, (length == 0 ? 1 : length) * sizeof(*units));
    if (units == NULL)
        return false;
    size_t count = 0;
    while (p->offset < p->input.length) {
        uint16_t c = p->input.units[p->offset++];
        if (c == '"') {
            *out = (mc_client_stats_string){units, count, false};
            return true;
        }
        if (c == '\\') {
            if (p->offset >= p->input.length)
                return syntax(p);
            c = p->input.units[p->offset++];
            switch (c) {
                case '"':
                case '\\':
                case '/':
                    break;
                case 'b':
                    c = '\b';
                    break;
                case 'f':
                    c = '\f';
                    break;
                case 'n':
                    c = '\n';
                    break;
                case 'r':
                    c = '\r';
                    break;
                case 't':
                    c = '\t';
                    break;
                case 'u': {
                    if (p->input.length - p->offset < 4)
                        return syntax(p);
                    uint16_t decoded = 0;
                    for (size_t i = 0; i < 4; ++i) {
                        const int digit = hex(p->input.units[p->offset++]);
                        if (digit < 0)
                            return syntax(p);
                        decoded = (uint16_t)((decoded << 4u) | (uint16_t)digit);
                    }
                    c = decoded;
                    break;
                }
                default:
                    return syntax(p);
            }
        }
        /* Supplied Argo accepts unescaped control units, including NUL. */
        units[count++] = c;
    }
    return syntax(p);
}

static json_node *value(json_parser *p, size_t depth);

static json_node *container(json_parser *p, size_t depth, bool object) {
    json_node *result = node(p, object ? JSON_OBJECT : JSON_ARRAY);
    if (result == NULL)
        return NULL;
    ++p->offset;
    whitespace(p);
    const uint16_t close = object ? '}' : ']';
    if (p->offset < p->input.length && p->input.units[p->offset] == close) {
        ++p->offset;
        return result;
    }
    for (;;) {
        mc_client_stats_string key = {0};
        if (object) {
            if (!string(p, &key))
                return NULL;
            whitespace(p);
            if (p->offset >= p->input.length || p->input.units[p->offset++] != ':') {
                syntax(p);
                return NULL;
            }
            whitespace(p);
        }
        json_node *child = value(p, depth + 1u);
        if (child == NULL)
            return NULL;
        if (object) {
            json_field *field = allocate(p, sizeof(*field));
            if (field == NULL)
                return NULL;
            *field = (json_field){NULL, key, child};
            if (result->fields_last != NULL)
                result->fields_last->next = field;
            else
                result->fields = field;
            result->fields_last = field;
        } else {
            if (result->last != NULL)
                result->last->next = child;
            else
                result->children = child;
            result->last = child;
        }
        whitespace(p);
        if (p->offset >= p->input.length) {
            syntax(p);
            return NULL;
        }
        const uint16_t next = p->input.units[p->offset++];
        if (next == close)
            return result;
        if (next != ',') {
            syntax(p);
            return NULL;
        }
        whitespace(p);
    }
}

static bool digit(uint16_t c) {
    return c >= '0' && c <= '9';
}

static json_node *value(json_parser *p, size_t depth) {
    if (depth > p->maximum_depth) {
        stats_fail(p->error, MC_CLIENT_STATS_LIMIT, "Argo native nesting bound", NULL, NULL, NULL,
                   0);
        return NULL;
    }
    whitespace(p);
    if (p->offset >= p->input.length) {
        syntax(p);
        return NULL;
    }
    uint16_t c = p->input.units[p->offset];
    if (c == '{' || c == '[')
        return container(p, depth, c == '{');
    if (c == '"') {
        json_node *n = node(p, JSON_STRING);
        return n != NULL && string(p, &n->text) ? n : NULL;
    }
    if (c == '-' || digit(c)) {
        const size_t start = p->offset;
        if (c == '-') {
            if (++p->offset >= p->input.length) {
                syntax(p);
                return NULL;
            }
            c = p->input.units[p->offset];
        }
        if (c == '0')
            ++p->offset;
        else if (c >= '1' && c <= '9') {
            do {
                ++p->offset;
            } while (p->offset < p->input.length && digit(p->input.units[p->offset]));
        } else {
            syntax(p);
            return NULL;
        }
        if (p->offset < p->input.length && p->input.units[p->offset] == '.') {
            ++p->offset;
            if (p->offset >= p->input.length || !digit(p->input.units[p->offset])) {
                syntax(p);
                return NULL;
            }
            do {
                ++p->offset;
            } while (p->offset < p->input.length && digit(p->input.units[p->offset]));
        }
        if (p->offset < p->input.length &&
            (p->input.units[p->offset] == 'e' || p->input.units[p->offset] == 'E')) {
            ++p->offset;
            if (p->offset < p->input.length &&
                (p->input.units[p->offset] == '+' || p->input.units[p->offset] == '-'))
                ++p->offset;
            if (p->offset >= p->input.length || !digit(p->input.units[p->offset])) {
                syntax(p);
                return NULL;
            }
            do {
                ++p->offset;
            } while (p->offset < p->input.length && digit(p->input.units[p->offset]));
        }
        json_node *n = node(p, JSON_NUMBER);
        if (n != NULL)
            n->text = (mc_client_stats_string){p->input.units + start, p->offset - start, false};
        return n;
    }
    static const char *const literals[] = {"true", "false", "null"};
    static const json_kind kinds[] = {JSON_TRUE, JSON_FALSE, JSON_NULL};
    for (size_t i = 0; i < 3; ++i) {
        size_t j = 0;
        while (literals[i][j] != '\0' && p->offset + j < p->input.length &&
               p->input.units[p->offset + j] == (unsigned char)literals[i][j])
            ++j;
        if (literals[i][j] == '\0') {
            p->offset += j;
            return node(p, kinds[i]);
        }
    }
    syntax(p);
    return NULL;
}

static json_node *property(json_node *object, const char *name) {
    if (object->kind != JSON_OBJECT)
        return NULL;
    json_node *result = NULL;
    for (json_field *field = object->fields; field != NULL; field = field->next)
        if (ascii_equal(field->key, name))
            result = field->value;
    return result;
}

/* oy default HashMap, kr copy, then c() copy. QA String hash/equals supplies
 * list-bin order. Raw-Comparable QA tree ties use JVM identity and stay an
 * explicit provider frontier, not a guessed sorted or first-input field. */
typedef struct field_entry {
    json_field *field;
    uint32_t hash;
    struct field_entry *next;
} field_entry;

typedef struct field_map {
    field_entry **buckets;
    size_t capacity, count, threshold;
} field_map;

static uint32_t field_hash(mc_client_stats_string key) {
    uint32_t hash = 0;
    for (size_t i = 0; i < key.length; ++i)
        hash = hash * 31u + key.units[i];
    return hash ^ (hash >> 16u);
}

static bool resize_fields(json_parser *p, field_map *map, size_t capacity) {
    if (capacity > SIZE_MAX / sizeof(field_entry *)) {
        stats_fail(p->error, MC_CLIENT_STATS_LIMIT, "Argo native field table extent", NULL, NULL,
                   NULL, 0);
        return false;
    }
    field_entry **buckets = allocate(p, capacity * sizeof(*buckets));
    if (buckets == NULL)
        return false;
    memset(buckets, 0, capacity * sizeof(*buckets));
    for (size_t i = 0; i < map->capacity; ++i) {
        field_entry *entry = map->buckets[i];
        while (entry != NULL) {
            field_entry *next = entry->next;
            const size_t index = entry->hash & (capacity - 1u);
            field_entry **tail = &buckets[index];
            while (*tail != NULL)
                tail = &(*tail)->next;
            entry->next = NULL;
            *tail = entry;
            entry = next;
        }
    }
    map->buckets = buckets;
    map->capacity = capacity;
    map->threshold = capacity - capacity / 4u;
    return true;
}

static bool field_put(json_parser *p, field_map *map, json_field *field) {
    if (map->capacity == 0 && !resize_fields(p, map, 16))
        return false;
    const uint32_t hash = field_hash(field->key);
    const size_t index = hash & (map->capacity - 1u);
    size_t count = 0;
    field_entry **tail = &map->buckets[index];
    while (*tail != NULL) {
        if ((*tail)->hash == hash && equal((*tail)->field->key, field->key)) {
            (*tail)->field = field;
            return true;
        }
        ++count;
        tail = &(*tail)->next;
    }
    field_entry *entry = allocate(p, sizeof(*entry));
    if (entry == NULL)
        return false;
    *entry = (field_entry){field, hash, NULL};
    *tail = entry;
    ++map->count;
    if (count >= 8) {
        if (map->capacity < 64) {
            if (!resize_fields(p, map, map->capacity * 2u))
                return false;
        } else {
            stats_fail(p->error, MC_CLIENT_STATS_UNSUPPORTED_PROFILE,
                       "Argo QA tree-bin JVM identity ordering", "xi", "a",
                       "(Ljava/lang/String;)Ljava/util/Map;", 98);
            return false;
        }
    }
    if (map->count > map->threshold && !resize_fields(p, map, map->capacity * 2u))
        return false;
    return true;
}

static json_field *first_field(json_parser *p, json_node *object) {
    field_map maps[3] = {{0}};
    for (json_field *field = object->fields; field != NULL; field = field->next)
        if (!field_put(p, &maps[0], field))
            return NULL;
    for (size_t stage = 1; stage < 3; ++stage) {
        if (maps[stage - 1].count != 0) {
            if (maps[stage - 1].count > (SIZE_MAX - 3u) / 4u) {
                stats_fail(p->error, MC_CLIENT_STATS_LIMIT, "Argo native copy count", NULL, NULL,
                           NULL, 0);
                return NULL;
            }
            const size_t desired = maps[stage - 1].count * 4u / 3u + 1u;
            size_t capacity = 1;
            while (capacity < desired)
                capacity *= 2u;
            if (!resize_fields(p, &maps[stage], capacity))
                return NULL;
        }
        for (size_t i = 0; i < maps[stage - 1].capacity; ++i)
            for (field_entry *entry = maps[stage - 1].buckets[i]; entry != NULL;
                 entry = entry->next)
                if (!field_put(p, &maps[stage], entry->field))
                    return NULL;
    }
    for (size_t i = 0; i < maps[2].capacity; ++i)
        if (maps[2].buckets[i] != NULL)
            return maps[2].buckets[i]->field;
    return NULL;
}

static mc_client_stats_status parse_integer(mc_client_stats_string text, int32_t *out,
                                            unsigned int pc, mc_client_stats_error *error) {
    const mc_java_integer_parse_text input = {text.units, text.length};
    const mc_java_integer_parse_status status = mc_java_integer_parse(&input, out);
    if (status == MC_JAVA_INTEGER_PARSE_OK)
        return MC_CLIENT_STATS_OK;
    if (status == MC_JAVA_INTEGER_PARSE_NUMBER_FORMAT)
        return stats_exception(error, MC_CLIENT_STATS_NUMBER_FORMAT, "Stats JSON Integer.parseInt",
                               "xi", "a", "(Ljava/lang/String;)Ljava/util/Map;", pc);
    return stats_fail(error,
                      status == MC_JAVA_INTEGER_PARSE_LIMIT ? MC_CLIENT_STATS_LIMIT
                                                            : MC_CLIENT_STATS_INVALID_ARGUMENT,
                      "Stats JSON native integer input", NULL, NULL, NULL, 0);
}

mc_client_stats_status stats_parse(mc_registry_classes *registry, stats_memory *memory,
                                   const mc_client_stats_options *options,
                                   mc_client_stats_writer *writer, mc_client_stats_string json,
                                   mc_client_stats_map **out, mc_client_stats_report *report,
                                   mc_client_stats_error *error) {
    mc_client_stats_map *map = NULL;
    mc_client_stats_status status = stats_map_make(registry, memory, options, &map, error);
    if (status != MC_CLIENT_STATS_OK)
        return status;
    json_parser p = {.arena = {.memory = memory},
                     .input = json,
                     .maximum_nodes = options->limits.max_json_nodes,
                     .maximum_depth = options->limits.max_json_depth,
                     .error = error};
    stats_builder checksum = {.memory = memory, .maximum = options->limits.max_text_units};
    if (json.is_null) {
        status = stats_exception(error, MC_CLIENT_STATS_NULL_REFERENCE,
                                 "Stats JSON StringReader null String", "hx", "a",
                                 "(Ljava/lang/String;)Lqe;", 6);
        goto finish;
    }
    if (json.length > options->limits.max_text_units || json.length > INT32_MAX) {
        status = stats_fail(error, MC_CLIENT_STATS_LIMIT, "Stats JSON native text bound", NULL,
                            NULL, NULL, 0);
        goto finish;
    }
    whitespace(&p);
    json_node *root = NULL;
    if (p.offset < json.length && (json.units[p.offset] == '{' || json.units[p.offset] == '['))
        root = value(&p, 0);
    else
        syntax(&p);
    whitespace(&p);
    if (error->status == MC_CLIENT_STATS_OK && p.offset != json.length)
        syntax(&p);
    if (error->status != MC_CLIENT_STATS_OK) {
        status = error->status;
        if (error->exception == MC_CLIENT_STATS_JSON_SYNTAX) {
            const mc_client_stats_diagnostic event = {.kind = MC_CLIENT_STATS_CAUGHT_EXCEPTION,
                                                      .cause = *error};
            stats_error_clear(error);
            if (report != NULL)
                report->syntax_caught = true;
            status = stats_emit(writer, options, &event, report, error);
        }
        goto finish;
    }
    json_node *array = property(root, "stats-change");
    if (array == NULL || array->kind != JSON_ARRAY) {
        status =
            stats_exception(error, MC_CLIENT_STATS_JSON_PATH, "Stats JSON stats-change array path",
                            "xi", "a", "(Ljava/lang/String;)Ljava/util/Map;", 43);
        goto finish;
    }
    size_t index = 0;
    for (json_node *item = array->children; item != NULL; item = item->next, ++index) {
        error->index = index;
        if (item->kind != JSON_OBJECT) {
            status = stats_exception(error, MC_CLIENT_STATS_ILLEGAL_STATE,
                                     "Stats JSON fields on non-object", "xi", "a",
                                     "(Ljava/lang/String;)Ljava/util/Map;", 81);
            goto finish;
        }
        json_field *field = first_field(&p, item);
        if (error->status != MC_CLIENT_STATS_OK) {
            status = error->status;
            goto finish;
        }
        if (field == NULL) {
            status = stats_exception(error, MC_CLIENT_STATS_NO_SUCH_ELEMENT,
                                     "Stats JSON empty entry iterator", "xi", "a",
                                     "(Ljava/lang/String;)Ljava/util/Map;", 98);
            goto finish;
        }
        int32_t id, count;
        status = parse_integer(field->key, &id, 121, error);
        if (status != MC_CLIENT_STATS_OK)
            goto finish;
        if (field->value->kind != JSON_STRING && field->value->kind != JSON_NUMBER) {
            status = stats_exception(error, MC_CLIENT_STATS_ILLEGAL_STATE,
                                     "Stats JSON value without text", "xi", "a",
                                     "(Ljava/lang/String;)Ljava/util/Map;", 136);
            goto finish;
        }
        status = parse_integer(field->value->text, &count, 139, error);
        if (status != MC_CLIENT_STATS_OK)
            goto finish;
        const mc_statistic *statistic = NULL;
        if (registry->statistics_state.phase != MC_REGISTRY_READY) {
            mc_registry_error cause;
            const mc_registry_status initialized =
                mc_registry_classes_initialize_statistics(registry, &cause);
            if (initialized != MC_REGISTRY_OK) {
                error->registry = initialized;
                status = stats_fail(error, MC_CLIENT_STATS_REGISTRY_FAILED,
                                    "Stats JSON jl active initialization", "xi", "a",
                                    "(Ljava/lang/String;)Ljava/util/Map;", 146);
                goto finish;
            }
        }
        const mc_registry_status found =
            mc_registry_classes_find_statistic(registry, id, &statistic);
        if (found != MC_REGISTRY_OK) {
            error->registry = found;
            status =
                stats_fail(error, MC_CLIENT_STATS_REGISTRY_FAILED, "Stats JSON actual ID lookup",
                           "xi", "a", "(Ljava/lang/String;)Ljava/util/Map;", 146);
            goto finish;
        }
        if (statistic == NULL) {
            const mc_client_stats_diagnostic event = {.kind = MC_CLIENT_STATS_UNKNOWN_STATISTIC,
                                                      .statistic_id = id};
            status = stats_emit(writer, options, &event, report, error);
            if (status != MC_CLIENT_STATS_OK)
                goto finish;
            continue;
        }
        /* Only immutable ID/GUID/class fields are read on workers. */
        mc_registry_string_view guid;
        const bool exists = mc_registry_string_read(statistic->fields.h, &guid);
        status = stats_string(
            &checksum,
            (mc_client_stats_string){exists ? guid.units : NULL, exists ? guid.length : 0, !exists},
            error);
        if (status == MC_CLIENT_STATS_OK)
            status = stats_ascii(&checksum, ",", error);
        if (status == MC_CLIENT_STATS_OK)
            status = stats_decimal(&checksum, count, error);
        if (status == MC_CLIENT_STATS_OK)
            status = stats_ascii(&checksum, ",", error);
        if (status == MC_CLIENT_STATS_OK)
            status = mc_client_stats_map_put(map, statistic, count, error);
        if (status != MC_CLIENT_STATS_OK)
            goto finish;
    }
    char digest_text[33];
    static const uint16_t local[] = {'l', 'o', 'c', 'a', 'l'};
    status = stats_checksum(memory, options->limits.max_text_units, options->charset,
                            (mc_client_stats_string){local, 5, false},
                            (mc_client_stats_string){checksum.units, checksum.length, false},
                            digest_text, error);
    if (status != MC_CLIENT_STATS_OK)
        goto finish;
    json_node *expected = property(root, "checksum");
    if (expected == NULL || expected->kind != JSON_STRING) {
        status =
            stats_exception(error, MC_CLIENT_STATS_JSON_PATH, "Stats JSON checksum String path",
                            "xi", "a", "(Ljava/lang/String;)Ljava/util/Map;", 266);
        goto finish;
    }
    if (!ascii_equal(expected->text, digest_text)) {
        const mc_client_stats_diagnostic event = {.kind = MC_CLIENT_STATS_CHECKSUM_MISMATCH};
        status = stats_emit(writer, options, &event, report, error);
        if (status == MC_CLIENT_STATS_OK)
            mc_client_stats_map_retire(&map);
    }
finish:
    stats_builder_clear(&checksum);
    stats_arena_clear(&p.arena);
    if (status == MC_CLIENT_STATS_OK) {
        *out = map;
        stats_error_clear(error);
    } else
        mc_client_stats_map_retire(&map);
    return status;
}

mc_client_stats_status mc_client_stats_parse(mc_registry_classes *registry,
                                             const mc_client_stats_options *input,
                                             mc_client_stats_string json, mc_client_stats_map **out,
                                             mc_client_stats_report *out_report,
                                             mc_client_stats_error *out_error) {
    mc_client_stats_error error;
    stats_error_clear(&error);
    mc_client_stats_report report = {0};
    if (registry == NULL || out == NULL ||
        (!json.is_null && json.length != 0 && json.units == NULL)) {
        stats_fail(&error, MC_CLIENT_STATS_INVALID_ARGUMENT, "Stats parse native arguments", NULL,
                   NULL, NULL, 0);
    } else {
        const mc_client_stats_options options = stats_options(input);
        stats_memory *memory = stats_memory_create(options.limits.max_allocation_bytes);
        if (memory == NULL)
            stats_fail(&error, MC_CLIENT_STATS_ALLOCATION_FAILED, "Stats parse memory owner", NULL,
                       NULL, NULL, 0);
        else {
            (void)stats_parse(registry, memory, &options, NULL, json, out, &report, &error);
            stats_memory_release(memory);
        }
    }
    if (out_report != NULL)
        *out_report = report;
    if (out_error != NULL)
        *out_error = error;
    return error.status;
}

mc_client_stats_status mc_client_stats_serialize(const mc_client_stats_map *map,
                                                 mc_client_stats_string username,
                                                 mc_client_stats_string salt,
                                                 mc_client_stats_text **out,
                                                 mc_client_stats_error *out_error) {
    mc_client_stats_error error;
    stats_error_clear(&error);
    if (out == NULL || (!username.is_null && username.length != 0 && username.units == NULL) ||
        (!salt.is_null && salt.length != 0 && salt.units == NULL)) {
        stats_fail(&error, MC_CLIENT_STATS_INVALID_ARGUMENT, "Stats serialize native arguments",
                   NULL, NULL, NULL, 0);
        if (out_error != NULL)
            *out_error = error;
        return error.status;
    }
    if (map == NULL) {
        stats_exception(
            &error, MC_CLIENT_STATS_NULL_REFERENCE, "Stats serializer null Map", "xi", "a",
            "(Ljava/lang/String;Ljava/lang/String;Ljava/util/Map;)Ljava/lang/String;", 89);
        if (out_error != NULL)
            *out_error = error;
        return error.status;
    }
    mc_client_stats_status status = stats_map_lease(map, &error);
    if (status != MC_CLIENT_STATS_OK) {
        if (out_error != NULL)
            *out_error = error;
        return status;
    }
    stats_builder text = {.memory = map->memory, .maximum = map->options.limits.max_text_units};
    stats_builder payload = {.memory = map->memory, .maximum = map->options.limits.max_text_units};
#define APP(call)                                                                                  \
    do {                                                                                           \
        status = (call);                                                                           \
        if (status != MC_CLIENT_STATS_OK)                                                          \
            goto finish;                                                                           \
    } while (0)
    APP(stats_ascii(&text, "{\r\n", &error));
    if (!username.is_null && !salt.is_null) {
        APP(stats_ascii(&text, "  \"user\":{\r\n    \"name\":\"", &error));
        APP(stats_string(&text, username, &error));
        APP(stats_ascii(&text, "\",\r\n    \"sessionid\":\"", &error));
        APP(stats_string(&text, salt, &error));
        APP(stats_ascii(&text, "\"\r\n  },\r\n", &error));
    }
    APP(stats_ascii(&text, "  \"stats-change\":[", &error));
    bool first = true;
    for (const stats_entry *entry = map->head; entry != NULL; entry = entry->next) {
        if (!first)
            APP(stats_ascii(&text, "},", &error));
        first = false;
        APP(stats_ascii(&text, "\r\n    {\"", &error));
        if (entry->key == NULL) {
            status = stats_exception(
                &error, MC_CLIENT_STATS_NULL_REFERENCE, "Stats serializer null statistic", "xi",
                "a", "(Ljava/lang/String;Ljava/lang/String;Ljava/util/Map;)Ljava/lang/String;",
                149);
            goto finish;
        }
        APP(stats_decimal(&text, entry->key->fields.e, &error));
        APP(stats_ascii(&text, "\":", &error));
        APP(stats_decimal(&text, entry->value, &error));
        mc_registry_string_view guid;
        const bool exists = mc_registry_string_read(entry->key->fields.h, &guid);
        APP(stats_string(
            &payload,
            (mc_client_stats_string){exists ? guid.units : NULL, exists ? guid.length : 0, !exists},
            &error));
        APP(stats_ascii(&payload, ",", &error));
        APP(stats_decimal(&payload, entry->value, &error));
        APP(stats_ascii(&payload, ",", &error));
    }
    if (!first)
        APP(stats_ascii(&text, "}", &error));
    char checksum[33];
    APP(stats_checksum(map->memory, map->options.limits.max_text_units, map->options.charset, salt,
                       (mc_client_stats_string){payload.units, payload.length, false}, checksum,
                       &error));
    APP(stats_ascii(&text, "\r\n  ],\r\n  \"checksum\":\"", &error));
    APP(stats_ascii(&text, checksum, &error));
    APP(stats_ascii(&text, "\"\r\n}", &error));
    APP(stats_text_finish(&text, out, &error));
finish:
#undef APP
    stats_builder_clear(&text);
    stats_builder_clear(&payload);
    stats_map_unlease(map);
    if (out_error != NULL)
        *out_error = error;
    return status;
}
