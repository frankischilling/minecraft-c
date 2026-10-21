#include "keys_internal.h"

#include "../../block/registry/registry_internal.h"
#include "mc/core/java/reader.h"

#include <limits.h>

typedef struct key_entry {
    int32_t id;
    const mc_registry_string *value;
    struct key_entry *next;
} key_entry;

typedef struct key_map {
    key_entry *first, *last;
    size_t size;
} key_map;

typedef struct key_stream {
    const mc_registry_resources *resources;
    mc_java_properties_input input;
    mc_registry_status close_status;
} key_stream;

struct mc_statistic_keys {
    mc_registry_classes *classes;
    key_map *b;
    key_stream stream;
    mc_java_reader *reader;
    mc_registry_status reader_arena_failure;
    const mc_registry_string *empty;
    int32_t constructor_pc;
    mc_registry_status caught_status;
    int32_t caught_pc;
};

typedef struct key_part {
    const uint16_t *units;
    size_t length;
    const mc_registry_string *string;
} key_part;

static mc_registry_status diagnostic(mc_registry_error *error, mc_registry_status status,
                                     const char *message) {
    if (error != NULL)
        *error = (mc_registry_error){.status = status,
                                     .message = message,
                                     .initializer_pc = -1,
                                     .block_id = -1,
                                     .constructor_pc = -1,
                                     .profile_status = MC_JAVA_RANDOM_PROFILE_OK,
                                     .active_initializer_pc = -1,
                                     .item_argument = -1};
    return status;
}

static mc_registry_status constructor_error(mc_statistic_keys *keys) {
    keys->classes->last_error.constructor_class = "b";
    keys->classes->last_error.constructor_pc = keys->constructor_pc;
    return keys->classes->last_error.status;
}

static mc_registry_status constructor_fail(mc_statistic_keys *keys, mc_registry_status status,
                                           const char *message) {
    (void)mc_registry_fail(keys->classes, status, message, NULL);
    return constructor_error(keys);
}

static mc_registry_status caught_exception(mc_statistic_keys *keys, mc_registry_status status,
                                           int32_t pc) {
    keys->caught_status = status;
    keys->caught_pc = pc;
    keys->constructor_pc = 89;
    keys->constructor_pc = 91;
    return constructor_fail(keys, MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY,
                            "Statistic keys caught Exception.printStackTrace at91 unfinished");
}

static void *reader_allocate(void *context, size_t bytes) {
    mc_statistic_keys *keys = context;
    void *storage = mc_registry_allocate(keys->classes, bytes);
    if (storage == NULL) {
        keys->reader_arena_failure = keys->classes->last_error.status;
        (void)constructor_error(keys);
    }
    return storage;
}

static mc_java_properties_status stream_read(void *context, uint8_t *destination, size_t capacity,
                                             size_t *count) {
    key_stream *stream = context;
    return stream->input.read(stream->input.context, destination, capacity, count);
}

static bool stream_ready(void *context) {
    const key_stream *stream = context;
    return stream->resources->ready(stream->resources->context, &stream->input);
}

static mc_java_reader_status stream_close(void *context) {
    key_stream *stream = context;
    stream->close_status = stream->resources->close(stream->resources->context, &stream->input);
    if (stream->close_status == MC_REGISTRY_OK)
        return MC_JAVA_READER_OK;
    if (stream->close_status == MC_REGISTRY_IO_FAILED)
        return MC_JAVA_READER_IO_ERROR;
    return MC_JAVA_READER_INVALID_ARGUMENT;
}

static mc_registry_status reader_failure(mc_statistic_keys *keys, mc_java_reader_status status,
                                         bool catch_io) {
    switch (status) {
        case MC_JAVA_READER_OK:
            return MC_REGISTRY_OK;
        case MC_JAVA_READER_ALLOCATION_FAILED:
            if (keys->reader_arena_failure != MC_REGISTRY_OK)
                return constructor_error(keys);
            return constructor_fail(
                keys, MC_REGISTRY_ALLOCATION_FAILED,
                "Statistic keys native reader/default charset allocation failed");
        case MC_JAVA_READER_LIMIT:
            return constructor_fail(keys, MC_REGISTRY_LIMIT, "Statistic keys native reader bound");
        case MC_JAVA_READER_IO_ERROR:
            if (catch_io)
                return caught_exception(keys, MC_REGISTRY_IO_FAILED, keys->constructor_pc);
            return constructor_fail(keys, MC_REGISTRY_IO_FAILED,
                                    "Statistic keys native reader creation failed");
        case MC_JAVA_READER_UNSUPPORTED_CHARSET:
            return constructor_fail(keys, MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY,
                                    "Statistic keys native default charset unsupported");
        case MC_JAVA_READER_UNIMPLEMENTED_CLOSE:
            return constructor_fail(keys, MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY,
                                    "Statistic keys real resource close unavailable");
        default:
            return constructor_fail(keys, MC_REGISTRY_INVALID_ARGUMENT,
                                    "Statistic keys native reader operation");
    }
}

static mc_registry_status substring(mc_statistic_keys *keys, const uint16_t *units, size_t length,
                                    const mc_registry_string **out) {
    if (length == 0) {
        if (keys->empty == NULL) {
            mc_registry_status status = mc_registry_literal_ascii(keys->classes, "", &keys->empty);
            if (status != MC_REGISTRY_OK)
                return constructor_error(keys);
        }
        *out = keys->empty;
        return MC_REGISTRY_OK;
    }
    mc_registry_string *string = mc_registry_allocate(keys->classes, sizeof(*string));
    if (string == NULL)
        return constructor_error(keys);
    /* A new immutable String identity may share its retained native line's
     * backing units; Java's compact-string allocation graph is not exposed. */
    string->units = units;
    string->length = length;
    *out = string;
    return MC_REGISTRY_OK;
}

static mc_registry_status split_line(mc_statistic_keys *keys, const mc_java_reader_line *line,
                                     key_part **out, size_t *count) {
    size_t commas = 0;
    for (size_t i = 0; i < line->length; ++i) {
        if (line->units[i] == ',') {
            if (commas == (size_t)INT32_MAX - 1u)
                return constructor_fail(keys, MC_REGISTRY_LIMIT,
                                        "Statistic keys native split count bound");
            ++commas;
        }
    }
    const size_t parts = commas + 1u;
    if (parts > SIZE_MAX / sizeof(key_part))
        return constructor_fail(keys, MC_REGISTRY_LIMIT,
                                "Statistic keys native split allocation bound");
    key_part *result = mc_registry_allocate(keys->classes, parts * sizeof(*result));
    if (result == NULL)
        return constructor_error(keys);
    if (commas == 0) {
        /* String.split's no-match branch returns this original line, even
         * when it is empty. No empty-substring literal is substituted. */
        result[0] = (key_part){.units = line->units, .length = line->length, .string = NULL};
        *out = result;
        *count = 1;
        return MC_REGISTRY_OK;
    }
    size_t start = 0, part = 0;
    for (size_t end = 0;; ++end) {
        if (end == line->length || line->units[end] == ',') {
            const mc_registry_string *string = NULL;
            const mc_registry_status status =
                substring(keys, line->units + start, end - start, &string);
            if (status != MC_REGISTRY_OK)
                return status;
            result[part++] =
                (key_part){.units = string->units, .length = string->length, .string = string};
            if (end == line->length)
                break;
            start = end + 1u;
        }
    }
    size_t retained = parts;
    while (retained != 0 && result[retained - 1u].length == 0)
        --retained;
    *out = result;
    *count = retained;
    return MC_REGISTRY_OK;
}

static int32_t decimal_digit(uint16_t unit) {
    /* Actual installed-JDK17 Character.digit(char,10) observations: every
     * BMP unit, identical in the three retained JVM modes. */
    static const uint16_t starts[37] = {
        0x0030, 0x0660, 0x06f0, 0x07c0, 0x0966, 0x09e6, 0x0a66, 0x0ae6, 0x0b66, 0x0be6,
        0x0c66, 0x0ce6, 0x0d66, 0x0de6, 0x0e50, 0x0ed0, 0x0f20, 0x1040, 0x1090, 0x17e0,
        0x1810, 0x1946, 0x19d0, 0x1a80, 0x1a90, 0x1b50, 0x1bb0, 0x1c40, 0x1c50, 0xa620,
        0xa8d0, 0xa900, 0xa9d0, 0xa9f0, 0xaa50, 0xabf0, 0xff10};
    for (size_t i = 0; i < sizeof(starts) / sizeof(starts[0]); ++i) {
        if (unit >= starts[i] && (uint32_t)unit - starts[i] < 10u)
            return (int32_t)((uint32_t)unit - starts[i]);
    }
    return -1;
}

static bool parse_integer(const key_part *part, int32_t *out) {
    if (part->length == 0)
        return false;
    bool negative = false;
    size_t offset = 0;
    int32_t limit = -INT32_MAX;
    const uint16_t first = part->units[0];
    if (first < '0') {
        if (first == '-') {
            negative = true;
            limit = INT32_MIN;
        } else if (first != '+') {
            return false;
        }
        if (part->length == 1u)
            return false;
        ++offset;
    }
    const int32_t multiplication_minimum = limit / 10;
    int32_t result = 0;
    while (offset < part->length) {
        const int32_t digit = decimal_digit(part->units[offset++]);
        if (digit < 0 || result < multiplication_minimum)
            return false;
        result *= 10;
        if (result < limit + digit)
            return false;
        result -= digit;
    }
    *out = negative ? result : -result;
    return true;
}

static key_entry *find_entry(const key_map *map, int32_t id) {
    for (key_entry *entry = map->first; entry != NULL; entry = entry->next) {
        if (entry->id == id)
            return entry;
    }
    return NULL;
}

static mc_registry_status put(mc_statistic_keys *keys, key_map *map, int32_t id,
                              const mc_registry_string *value) {
    key_entry *entry = find_entry(map, id);
    if (entry != NULL) {
        entry->value = value;
        return MC_REGISTRY_OK;
    }
    if (map->size == SIZE_MAX)
        return constructor_fail(keys, MC_REGISTRY_LIMIT, "Statistic keys native map size bound");
    entry = mc_registry_allocate(keys->classes, sizeof(*entry));
    if (entry == NULL)
        return constructor_error(keys);
    *entry = (key_entry){.id = id, .value = value, .next = NULL};
    if (map->last == NULL)
        map->first = entry;
    else
        map->last->next = entry;
    map->last = entry;
    ++map->size;
    return MC_REGISTRY_OK;
}

static mc_registry_status construct(mc_statistic_keys *keys) {
    mc_registry_classes *classes = keys->classes;
    keys->constructor_pc = 1;
    keys->constructor_pc = 5;
    key_map *map = mc_registry_allocate(classes, sizeof(*map));
    if (map == NULL)
        return constructor_error(keys);
    keys->constructor_pc = 9;
    *map = (key_map){0};
    keys->constructor_pc = 12;
    keys->b = map;
    /* These are actual source new sites. The native reader's combined
     * allocation API runs at its reached constructor call after open. */
    keys->constructor_pc = 15;
    keys->constructor_pc = 19;
    keys->constructor_pc = 25;
    const mc_registry_string *resource_name = NULL;
    mc_registry_status status =
        mc_registry_literal_ascii(classes, "/achievement/map.txt", &resource_name);
    if (status != MC_REGISTRY_OK)
        return constructor_error(keys);
    (void)resource_name;
    keys->constructor_pc = 27;
    const mc_registry_resources *resources = classes->options.resources;
    if (resources == NULL || resources->open == NULL)
        return constructor_fail(keys, MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY,
                                "Statistic keys Class.getResourceAsStream source unavailable");
    mc_java_properties_input input = {0};
    status = resources->open(resources->context, "/achievement/map.txt", &input);
    if (status != MC_REGISTRY_OK && status != MC_REGISTRY_NULL_REFERENCE)
        return constructor_fail(keys, status, "Statistic keys native resource open failed");
    keys->constructor_pc = 30;
    if (status == MC_REGISTRY_NULL_REFERENCE)
        return caught_exception(keys, MC_REGISTRY_NULL_REFERENCE, 30);
    if (input.read == NULL)
        return constructor_fail(keys, MC_REGISTRY_INVALID_ARGUMENT,
                                "Statistic keys resource supplied no byte reader");
    keys->stream =
        (key_stream){.resources = resources, .input = input, .close_status = MC_REGISTRY_OK};
    const mc_java_properties_allocator allocator = {.context = keys, .allocate = reader_allocate};
    const mc_java_reader_input source = {.bytes = {.context = &keys->stream, .read = stream_read},
                                         .ready = resources->ready == NULL ? NULL : stream_ready,
                                         .close = resources->close == NULL ? NULL : stream_close};
    keys->reader_arena_failure = MC_REGISTRY_OK;
    const mc_java_reader_status created =
        mc_java_reader_create(&allocator, &source, MC_JAVA_READER_DEFAULT, &keys->reader);
    if (created != MC_JAVA_READER_OK)
        return reader_failure(keys, created, false);
    keys->constructor_pc = 33;
    for (;;) {
        keys->constructor_pc = 38;
        const mc_java_reader_line *line = NULL;
        keys->reader_arena_failure = MC_REGISTRY_OK;
        const mc_java_reader_status read = mc_java_reader_read_line(keys->reader, &line);
        if (read != MC_JAVA_READER_OK)
            return reader_failure(keys, read, true);
        if (line == NULL)
            break;
        keys->constructor_pc = 47;
        const mc_registry_string *comma = NULL;
        status = mc_registry_literal_ascii(classes, ",", &comma);
        if (status != MC_REGISTRY_OK)
            return constructor_error(keys);
        (void)comma;
        keys->constructor_pc = 49;
        key_part *parts = NULL;
        size_t count = 0;
        status = split_line(keys, line, &parts, &count);
        if (status != MC_REGISTRY_OK)
            return status;
        keys->constructor_pc = 55;
        if (count == 0)
            return caught_exception(keys, MC_REGISTRY_INDEX_OUT_OF_RANGE, 55);
        keys->constructor_pc = 56;
        int32_t id = 0;
        if (!parse_integer(&parts[0], &id))
            return caught_exception(keys, MC_REGISTRY_ILLEGAL_ARGUMENT, 56);
        keys->constructor_pc = 62;
        key_map *destination = keys->b;
        keys->constructor_pc = 67; /* Primitive ID represents reached Integer.valueOf. */
        keys->constructor_pc = 72;
        if (count < 2u)
            return caught_exception(keys, MC_REGISTRY_INDEX_OUT_OF_RANGE, 72);
        const mc_registry_string *value = parts[1].string;
        keys->constructor_pc = 73;
        if (destination == NULL)
            return caught_exception(keys, MC_REGISTRY_NULL_REFERENCE, 73);
        status = put(keys, destination, id, value);
        if (status != MC_REGISTRY_OK)
            return status;
    }
    keys->constructor_pc = 83;
    keys->reader_arena_failure = MC_REGISTRY_OK;
    const mc_java_reader_status closed = mc_java_reader_close(keys->reader);
    if (keys->stream.close_status != MC_REGISTRY_OK &&
        keys->stream.close_status != MC_REGISTRY_IO_FAILED)
        return constructor_fail(keys, keys->stream.close_status,
                                "Statistic keys native resource close failed");
    if (closed != MC_JAVA_READER_OK)
        return reader_failure(keys, closed, true);
    keys->constructor_pc = 94;
    return MC_REGISTRY_OK;
}

static mc_registry_status cached_state(mc_registry_classes *classes,
                                       const mc_registry_class_state *state) {
    if (state->phase == MC_REGISTRY_FAILED) {
        classes->last_error = state->failure;
        classes->last_error.status = MC_REGISTRY_FAILED_INITIALIZATION;
        return MC_REGISTRY_FAILED_INITIALIZATION;
    }
    return MC_REGISTRY_OK;
}

mc_registry_status mc_registry_statistic_keys_active_use(mc_registry_classes *classes) {
    if (classes == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    if (classes->entered == 0)
        return mc_registry_fail(classes, MC_REGISTRY_INVALID_ARGUMENT,
                                "Statistic keys active use requires admitted initialization", NULL);
    mc_registry_class_state *state = &classes->statistic_key_state;
    if (state->phase != MC_REGISTRY_COLD)
        return cached_state(classes, state);
    state->phase = MC_REGISTRY_INITIALIZING;
    state->pc = 0;
    mc_registry_initializer frame;
    mc_registry_initializer_enter(classes, &frame, "b", "b", &state->pc);
    mc_registry_status status = MC_REGISTRY_OK;
    mc_statistic_keys *keys = mc_registry_allocate(classes, sizeof(*keys));
    if (keys == NULL) {
        status = classes->last_error.status;
        goto done;
    }
    *keys = (mc_statistic_keys){
        .classes = classes, .constructor_pc = -1, .caught_status = MC_REGISTRY_OK, .caught_pc = -1};
    classes->statistic_keys.pending = keys;
    state->pc = 4;
    status = construct(keys);
    if (status != MC_REGISTRY_OK)
        goto done;
    state->pc = 7;
    classes->statistic_keys.a = keys;
    state->pc = 10;
done:
    state->phase = status == MC_REGISTRY_OK ? MC_REGISTRY_READY : MC_REGISTRY_FAILED;
    if (status != MC_REGISTRY_OK)
        state->failure = classes->last_error;
    mc_registry_initializer_leave(classes, &frame);
    return status;
}

mc_registry_status mc_registry_statistic_key(mc_registry_classes *classes, int32_t id,
                                             const mc_registry_string **out) {
    if (classes == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    if (out == NULL)
        return mc_registry_fail(classes, MC_REGISTRY_INVALID_ARGUMENT, "Statistic key output",
                                NULL);
    mc_registry_status status = mc_registry_statistic_keys_active_use(classes);
    if (status != MC_REGISTRY_OK)
        return status;
    const mc_statistic_keys *keys = classes->statistic_keys.a;
    if (keys == NULL)
        return mc_registry_fail(classes, MC_REGISTRY_NULL_REFERENCE,
                                "Statistic key getfield3 null singleton", NULL);
    if (keys->b == NULL)
        return mc_registry_fail(classes, MC_REGISTRY_NULL_REFERENCE,
                                "Statistic key Map.get10 null map", NULL);
    const key_entry *entry = find_entry(keys->b, id);
    *out = entry == NULL ? NULL : entry->value;
    return MC_REGISTRY_OK;
}

mc_registry_status mc_registry_classes_initialize_statistic_keys(mc_registry_classes *classes,
                                                                 mc_registry_error *error) {
    if (classes == NULL)
        return diagnostic(error, MC_REGISTRY_INVALID_ARGUMENT, "Statistic keys registry owner");
    if (classes->entered != 0 || classes->statistic_key_state.phase == MC_REGISTRY_INITIALIZING)
        return diagnostic(error, MC_REGISTRY_BUSY, "registry initialization entered");
    const mc_registry_class_state *state = &classes->statistic_key_state;
    if (state->phase == MC_REGISTRY_READY)
        return diagnostic(error, MC_REGISTRY_OK, NULL);
    if (state->phase == MC_REGISTRY_FAILED) {
        if (error != NULL) {
            *error = state->failure;
            error->status = MC_REGISTRY_FAILED_INITIALIZATION;
        }
        return MC_REGISTRY_FAILED_INITIALIZATION;
    }
    ++classes->entered;
    mc_registry_status status = mc_registry_statistic_keys_active_use(classes);
    --classes->entered;
    if (status == MC_REGISTRY_OK)
        return diagnostic(error, MC_REGISTRY_OK, NULL);
    if (error != NULL)
        *error = classes->last_error;
    return status;
}

mc_registry_status mc_registry_classes_inspect_statistic_keys(const mc_registry_classes *classes,
                                                              mc_registry_statistic_key_view *out,
                                                              mc_registry_error *error) {
    if (classes == NULL || out == NULL)
        return diagnostic(error, MC_REGISTRY_INVALID_ARGUMENT, "Statistic keys view arguments");
    const mc_statistic_keys *keys = classes->statistic_keys.pending;
    *out = (mc_registry_statistic_key_view){
        .phase = classes->statistic_key_state.phase,
        .initializer_pc = classes->statistic_key_state.pc,
        .a = classes->statistic_keys.a,
        .construction = keys,
        .map_published = keys != NULL && keys->b != NULL,
        .constructor_pc = keys == NULL ? -1 : keys->constructor_pc,
        .caught_status = keys == NULL ? MC_REGISTRY_OK : keys->caught_status,
        .caught_pc = keys == NULL ? -1 : keys->caught_pc};
    return diagnostic(error, MC_REGISTRY_OK, NULL);
}

mc_registry_status mc_registry_classes_statistic_key(mc_registry_classes *classes, int32_t id,
                                                     const mc_registry_string **out,
                                                     mc_registry_error *error) {
    if (classes == NULL || out == NULL)
        return diagnostic(error, MC_REGISTRY_INVALID_ARGUMENT, "Statistic key arguments");
    if (classes->entered != 0 || classes->statistic_key_state.phase == MC_REGISTRY_INITIALIZING)
        return diagnostic(error, MC_REGISTRY_BUSY, "registry initialization entered");
    ++classes->entered;
    const mc_registry_string *value = NULL;
    mc_registry_status status = mc_registry_statistic_key(classes, id, &value);
    --classes->entered;
    if (status != MC_REGISTRY_OK) {
        if (error != NULL)
            *error = classes->last_error;
        return status;
    }
    *out = value;
    return diagnostic(error, MC_REGISTRY_OK, NULL);
}

size_t mc_statistic_keys_size(const mc_statistic_keys *keys) {
    return keys == NULL || keys->b == NULL ? 0 : keys->b->size;
}

mc_registry_status mc_statistic_keys_entry(const mc_statistic_keys *keys, size_t index, int32_t *id,
                                           const mc_registry_string **value) {
    if (keys == NULL || id == NULL || value == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    if (keys->b == NULL || index >= keys->b->size)
        return MC_REGISTRY_INDEX_OUT_OF_RANGE;
    const key_entry *entry = keys->b->first;
    while (index != 0) {
        entry = entry->next;
        --index;
    }
    *id = entry->id;
    *value = entry->value;
    return MC_REGISTRY_OK;
}
