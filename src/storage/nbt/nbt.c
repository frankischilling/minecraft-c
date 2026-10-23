#include "mc/storage/nbt/nbt.h"

#include "mc/storage/nbt/array.h"

#include "mc/core/java/java.h"
#include "mc/core/text/mutf8.h"

#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define NBT_MAX_DEPTH 128u

_Static_assert(CHAR_BIT == 8, "NBT requires eight-bit bytes");

/* A union header keeps arena payloads aligned for tags, pointer arrays and
 * decoded units. Old buffers remain owned after growth and replacement. */
typedef union nbt_allocation nbt_allocation;

union nbt_allocation {
    struct {
        nbt_allocation *next;
    } link;

    mc_nbt_tag alignment;
};

typedef struct nbt_arena {
    nbt_allocation *allocations;
    size_t bytes;
    size_t limit;
} nbt_arena;

struct mc_nbt_document {
    nbt_arena arena;
    mc_nbt_tag *root;
    size_t holds;
    mc_nbt_byte_array *arrays;
};

struct mc_nbt_byte_array {
    size_t holds, length, allocation_bytes;
    uint8_t *data;
    mc_nbt_document *document;
    const mc_nbt_tag *tag;
    mc_nbt_byte_array *next;
};

typedef struct nbt_reader {
    const uint8_t *input;
    size_t length;
    size_t position;
    size_t tags;
    mc_nbt_limits limits;
    mc_nbt_document *document;
    mc_nbt_error error;
} nbt_reader;

typedef struct nbt_compound_builder {
    const mc_nbt_tag **items;
    size_t length;
    size_t capacity;
    /* Zero means unused; other values are item indices plus one. */
    size_t *index;
    size_t index_capacity;
} nbt_compound_builder;

typedef struct nbt_validator {
    size_t length;
    size_t tags;
    mc_nbt_limits limits;
    nbt_arena scratch;
    const mc_nbt_tag *ancestors[NBT_MAX_DEPTH];
    mc_nbt_error error;
} nbt_validator;

static void nbt_set_error(mc_nbt_error *error, mc_nbt_status status, size_t offset) {
    if (error != NULL) {
        error->status = status;
        error->offset = offset;
    }
}

static bool nbt_limits_valid(const mc_nbt_limits *limits) {
    return limits->max_depth != 0 && limits->max_depth <= NBT_MAX_DEPTH;
}

mc_nbt_limits mc_nbt_default_limits(void) {
    const mc_nbt_limits limits = {
        64u * 1024u * 1024u, 64u * 1024u * 1024u, 128u * 1024u * 1024u, 1000000u, 1000000u, 64u};
    return limits;
}

const char *mc_nbt_status_string(mc_nbt_status status) {
    switch (status) {
        case MC_NBT_OK:
            return "ok";
        case MC_NBT_INVALID_ARGUMENT:
            return "invalid argument";
        case MC_NBT_TRUNCATED:
            return "truncated input";
        case MC_NBT_INVALID_TYPE:
            return "invalid tag type";
        case MC_NBT_INVALID_LENGTH:
            return "invalid length";
        case MC_NBT_INVALID_UTF:
            return "invalid modified UTF-8";
        case MC_NBT_LIMIT:
            return "resource limit";
        case MC_NBT_ALLOCATION_FAILED:
            return "allocation failed";
        case MC_NBT_INVALID_TREE:
            return "invalid tag tree";
        case MC_NBT_BUFFER_TOO_SMALL:
            return "output buffer too small";
        case MC_NBT_ROOT_TYPE:
            return "root is not a compound";
    }
    return "unknown NBT status";
}

static void nbt_arena_free(nbt_arena *arena) {
    nbt_allocation *allocation = arena->allocations;
    while (allocation != NULL) {
        nbt_allocation *next = allocation->link.next;
        free(allocation);
        allocation = next;
    }
    arena->allocations = NULL;
}

static void *nbt_arena_allocate(nbt_arena *arena, size_t count, size_t size,
                                mc_nbt_status *status) {
    if (size != 0 && count > SIZE_MAX / size) {
        *status = MC_NBT_LIMIT;
        return NULL;
    }
    size_t bytes = count * size;
    if (bytes > SIZE_MAX - sizeof(nbt_allocation)) {
        *status = MC_NBT_LIMIT;
        return NULL;
    }
    size_t total = sizeof(nbt_allocation) + bytes;
    if (total > arena->limit - arena->bytes) {
        *status = MC_NBT_LIMIT;
        return NULL;
    }
    nbt_allocation *allocation = malloc(total);
    if (allocation == NULL) {
        *status = MC_NBT_ALLOCATION_FAILED;
        return NULL;
    }
    allocation->link.next = arena->allocations;
    arena->allocations = allocation;
    arena->bytes += total;
    void *payload = allocation + 1;
    memset(payload, 0, bytes);
    return payload;
}

void mc_nbt_document_free(mc_nbt_document *document) {
    if (document != NULL && --document->holds == 0) {
        nbt_arena_free(&document->arena);
        free(document);
    }
}

const mc_nbt_tag *mc_nbt_document_root(const mc_nbt_document *document) {
    return document == NULL ? NULL : document->root;
}

mc_nbt_status mc_nbt_byte_array_create(size_t length, const uint8_t *source,
                                       size_t max_allocation_bytes, mc_nbt_byte_array **out) {
    if (out == NULL)
        return MC_NBT_INVALID_ARGUMENT;
    if (length > INT32_MAX)
        return MC_NBT_INVALID_LENGTH;
    if (length > SIZE_MAX - sizeof(mc_nbt_byte_array))
        return MC_NBT_LIMIT;
    const size_t bytes = sizeof(mc_nbt_byte_array) + length;
    if (bytes > max_allocation_bytes)
        return MC_NBT_LIMIT;
    mc_nbt_byte_array *array = malloc(bytes);
    if (array == NULL)
        return MC_NBT_ALLOCATION_FAILED;
    *array = (mc_nbt_byte_array){
        .holds = 1, .length = length, .allocation_bytes = bytes, .data = (uint8_t *)(array + 1)};
    if (length != 0) {
        if (source == NULL)
            memset(array->data, 0, length);
        else
            memcpy(array->data, source, length);
    }
    *out = array;
    return MC_NBT_OK;
}

mc_nbt_status mc_nbt_byte_array_retain(mc_nbt_byte_array *array) {
    if (array == NULL)
        return MC_NBT_INVALID_ARGUMENT;
    if (array->holds == SIZE_MAX)
        return MC_NBT_LIMIT;
    ++array->holds;
    return MC_NBT_OK;
}

void mc_nbt_byte_array_release(mc_nbt_byte_array *array) {
    if (array == NULL || --array->holds != 0)
        return;
    mc_nbt_document *document = array->document;
    if (document != NULL) {
        mc_nbt_byte_array **link = &document->arrays;
        while (*link != array)
            link = &(*link)->next;
        *link = array->next;
        document->arena.bytes -= array->allocation_bytes;
    }
    free(array);
    mc_nbt_document_free(document);
}

uint8_t *mc_nbt_byte_array_data(const mc_nbt_byte_array *array) {
    return array == NULL ? NULL : array->data;
}

size_t mc_nbt_byte_array_length(const mc_nbt_byte_array *array) {
    return array == NULL ? 0 : array->length;
}

size_t mc_nbt_byte_array_allocation_bytes(const mc_nbt_byte_array *array) {
    return array == NULL ? 0 : array->allocation_bytes;
}

/* Only immutable parser-owned structure is traversed. The foreign candidate is
 * never dereferenced, including when it points inside an arena allocation. */
static const mc_nbt_tag *nbt_document_find_tag(const mc_nbt_tag *actual,
                                               const mc_nbt_tag *candidate) {
    if (actual == candidate)
        return actual;
    const mc_nbt_tag *const *items;
    size_t length;
    if (actual->type == MC_NBT_COMPOUND) {
        items = actual->value.compound.items;
        length = actual->value.compound.length;
    } else if (actual->type == MC_NBT_LIST) {
        items = actual->value.list.items;
        length = actual->value.list.length;
    } else {
        return NULL;
    }
    for (size_t i = 0; i < length; ++i) {
        const mc_nbt_tag *found = nbt_document_find_tag(items[i], candidate);
        if (found != NULL)
            return found;
    }
    return NULL;
}

mc_nbt_status mc_nbt_document_byte_array(mc_nbt_document *document, const mc_nbt_tag *tag,
                                         mc_nbt_byte_array **out) {
    if (document == NULL || tag == NULL || out == NULL)
        return MC_NBT_INVALID_ARGUMENT;
    const mc_nbt_tag *actual = nbt_document_find_tag(document->root, tag);
    if (actual == NULL || actual->type != MC_NBT_BYTE_ARRAY)
        return MC_NBT_INVALID_ARGUMENT;
    for (mc_nbt_byte_array *array = document->arrays; array != NULL; array = array->next) {
        if (array->tag == actual) {
            const mc_nbt_status status = mc_nbt_byte_array_retain(array);
            if (status == MC_NBT_OK)
                *out = array;
            return status;
        }
    }
    if (document->holds == SIZE_MAX ||
        sizeof(mc_nbt_byte_array) > document->arena.limit - document->arena.bytes)
        return MC_NBT_LIMIT;
    mc_nbt_byte_array *array = malloc(sizeof(*array));
    if (array == NULL)
        return MC_NBT_ALLOCATION_FAILED;
    *array = (mc_nbt_byte_array){.holds = 1,
                                 .length = actual->value.byte_array.length,
                                 .allocation_bytes = sizeof(*array),
                                 .data = (uint8_t *)actual->value.byte_array.data,
                                 .document = document,
                                 .tag = actual,
                                 .next = document->arrays};
    ++document->holds;
    document->arena.bytes += sizeof(*array);
    document->arrays = array;
    *out = array;
    return MC_NBT_OK;
}

static bool nbt_string_equal(mc_nbt_string a, mc_nbt_string b) {
    return a.length == b.length &&
           (a.length == 0 || memcmp(a.units, b.units, a.length * sizeof(uint16_t)) == 0);
}

static size_t nbt_string_hash(mc_nbt_string string) {
    uint64_t hash = UINT64_C(14695981039346656037);
    for (size_t i = 0; i < string.length; ++i) {
        hash ^= string.units[i];
        hash *= UINT64_C(1099511628211);
    }
    return (size_t)hash;
}

const mc_nbt_tag *mc_nbt_compound_get(const mc_nbt_tag *compound, const uint16_t *name,
                                      size_t length) {
    if (compound == NULL || compound->type != MC_NBT_COMPOUND || (length != 0 && name == NULL) ||
        length > SIZE_MAX / sizeof(uint16_t) ||
        (compound->value.compound.length != 0 && compound->value.compound.items == NULL)) {
        return NULL;
    }
    mc_nbt_string key = {name, length};
    for (size_t i = 0; i < compound->value.compound.length; ++i) {
        const mc_nbt_tag *child = compound->value.compound.items[i];
        if (child != NULL && (child->name.length == 0 || child->name.units != NULL) &&
            nbt_string_equal(key, child->name)) {
            return child;
        }
    }
    return NULL;
}

static bool nbt_reader_fail(nbt_reader *reader, mc_nbt_status status, size_t offset) {
    if (reader->error.status == MC_NBT_OK) {
        nbt_set_error(&reader->error, status, offset);
    }
    return false;
}

static void *nbt_reader_allocate(nbt_reader *reader, size_t count, size_t size) {
    mc_nbt_status status = MC_NBT_OK;
    void *payload = nbt_arena_allocate(&reader->document->arena, count, size, &status);
    if (payload == NULL) {
        nbt_reader_fail(reader, status, reader->position);
    }
    return payload;
}

static bool nbt_reader_available(nbt_reader *reader, size_t count) {
    if (count > reader->length - reader->position) {
        return nbt_reader_fail(reader, MC_NBT_TRUNCATED, reader->position);
    }
    return true;
}

static bool nbt_read_u8(nbt_reader *reader, uint8_t *value) {
    if (!nbt_reader_available(reader, 1)) {
        return false;
    }
    *value = reader->input[reader->position++];
    return true;
}

static bool nbt_read_u16(nbt_reader *reader, uint16_t *value) {
    if (!nbt_reader_available(reader, 2)) {
        return false;
    }
    const uint8_t *bytes = reader->input + reader->position;
    *value = (uint16_t)((uint16_t)bytes[0] << 8u | bytes[1]);
    reader->position += 2;
    return true;
}

static bool nbt_read_u32(nbt_reader *reader, uint32_t *value) {
    if (!nbt_reader_available(reader, 4)) {
        return false;
    }
    uint32_t bits = 0;
    for (size_t i = 0; i < 4; ++i) {
        bits = bits << 8u | reader->input[reader->position + i];
    }
    reader->position += 4;
    *value = bits;
    return true;
}

static bool nbt_read_u64(nbt_reader *reader, uint64_t *value) {
    if (!nbt_reader_available(reader, 8)) {
        return false;
    }
    uint64_t bits = 0;
    for (size_t i = 0; i < 8; ++i) {
        bits = bits << 8u | reader->input[reader->position + i];
    }
    reader->position += 8;
    *value = bits;
    return true;
}

static bool nbt_read_count(nbt_reader *reader, size_t *count) {
    size_t offset = reader->position;
    uint32_t bits;
    if (!nbt_read_u32(reader, &bits)) {
        return false;
    }
    int32_t value = mc_java_i32_narrow((int64_t)bits);
    if (value < 0) {
        return nbt_reader_fail(reader, MC_NBT_INVALID_LENGTH, offset);
    }
    if ((size_t)value > reader->limits.max_elements) {
        return nbt_reader_fail(reader, MC_NBT_LIMIT, offset);
    }
    *count = (size_t)value;
    return true;
}

static bool nbt_read_string(nbt_reader *reader, mc_nbt_string *string) {
    uint16_t wire_length;
    if (!nbt_read_u16(reader, &wire_length) || !nbt_reader_available(reader, wire_length)) {
        return false;
    }
    const uint8_t *bytes = reader->input + reader->position;
    size_t length;
    if (mc_mutf8_decoded_length(bytes, wire_length, &length) != MC_MUTF8_OK) {
        return nbt_reader_fail(reader, MC_NBT_INVALID_UTF, reader->position);
    }
    uint16_t *units = NULL;
    if (length != 0) {
        units = nbt_reader_allocate(reader, length, sizeof(*units));
        if (units == NULL) {
            return false;
        }
        size_t decoded;
        if (mc_mutf8_decode(bytes, wire_length, units, length, &decoded) != MC_MUTF8_OK) {
            return nbt_reader_fail(reader, MC_NBT_INVALID_UTF, reader->position);
        }
    }
    string->units = units;
    string->length = length;
    reader->position += wire_length;
    return true;
}

static bool nbt_grow_capacity(size_t current, size_t needed, size_t *capacity) {
    size_t grown = current == 0 ? 1 : current;
    while (grown < needed) {
        if (grown > SIZE_MAX / 2) {
            return false;
        }
        grown *= 2;
    }
    *capacity = grown;
    return true;
}

static bool nbt_compound_insert(nbt_reader *reader, nbt_compound_builder *builder,
                                const mc_nbt_tag *tag) {
    size_t slot = 0;
    if (builder->index_capacity != 0) {
        slot = nbt_string_hash(tag->name) & (builder->index_capacity - 1);
        while (builder->index[slot] != 0) {
            size_t item = builder->index[slot] - 1;
            if (nbt_string_equal(tag->name, builder->items[item]->name)) {
                builder->items[item] = tag;
                return true;
            }
            slot = (slot + 1) & (builder->index_capacity - 1);
        }
    }
    size_t needed = builder->length + 1;
    if (needed > builder->capacity) {
        size_t capacity;
        if (!nbt_grow_capacity(builder->capacity, needed, &capacity)) {
            return nbt_reader_fail(reader, MC_NBT_LIMIT, reader->position);
        }
        const mc_nbt_tag **items = nbt_reader_allocate(reader, capacity, sizeof(*items));
        if (items == NULL) {
            return false;
        }
        if (builder->length != 0) {
            memcpy(items, builder->items, builder->length * sizeof(*items));
        }
        builder->items = items;
        builder->capacity = capacity;
    }
    if (needed > SIZE_MAX / 2) {
        return nbt_reader_fail(reader, MC_NBT_LIMIT, reader->position);
    }
    if (needed * 2 > builder->index_capacity) {
        size_t capacity;
        if (!nbt_grow_capacity(builder->index_capacity, needed * 2, &capacity)) {
            return nbt_reader_fail(reader, MC_NBT_LIMIT, reader->position);
        }
        size_t *index = nbt_reader_allocate(reader, capacity, sizeof(*index));
        if (index == NULL) {
            return false;
        }
        for (size_t i = 0; i < builder->length; ++i) {
            size_t entry = nbt_string_hash(builder->items[i]->name) & (capacity - 1);
            while (index[entry] != 0) {
                entry = (entry + 1) & (capacity - 1);
            }
            index[entry] = i + 1;
        }
        builder->index = index;
        builder->index_capacity = capacity;
        slot = nbt_string_hash(tag->name) & (capacity - 1);
        while (index[slot] != 0) {
            slot = (slot + 1) & (capacity - 1);
        }
    }
    builder->items[builder->length] = tag;
    builder->index[slot] = needed;
    builder->length = needed;
    return true;
}

static mc_nbt_tag *nbt_read_tag(nbt_reader *reader, uint8_t type, bool named, size_t depth);

static bool nbt_read_list(nbt_reader *reader, mc_nbt_tag *tag, size_t depth) {
    size_t type_offset = reader->position;
    uint8_t type;
    size_t count;
    if (!nbt_read_u8(reader, &type) || !nbt_read_count(reader, &count)) {
        return false;
    }
    tag->value.list.element_type = type;
    tag->value.list.length = count;
    if (count == 0) {
        return true;
    }
    if (type > MC_NBT_COMPOUND) {
        return nbt_reader_fail(reader, MC_NBT_INVALID_TYPE, type_offset);
    }
    if (count > reader->limits.max_tags - reader->tags) {
        return nbt_reader_fail(reader, MC_NBT_LIMIT, reader->position);
    }
    const mc_nbt_tag **items = nbt_reader_allocate(reader, count, sizeof(*items));
    if (items == NULL) {
        return false;
    }
    tag->value.list.items = items;
    for (size_t i = 0; i < count; ++i) {
        items[i] = nbt_read_tag(reader, type, false, depth + 1);
        if (items[i] == NULL) {
            return false;
        }
    }
    return true;
}

static bool nbt_read_compound_payload(nbt_reader *reader, mc_nbt_tag *tag, size_t depth) {
    nbt_compound_builder builder = {0};
    size_t physical_count = 0;
    for (;;) {
        size_t offset = reader->position;
        uint8_t type;
        if (!nbt_read_u8(reader, &type)) {
            return false;
        }
        if (type == MC_NBT_END) {
            tag->value.compound.items = builder.items;
            tag->value.compound.length = builder.length;
            return true;
        }
        if (physical_count == reader->limits.max_elements) {
            return nbt_reader_fail(reader, MC_NBT_LIMIT, offset);
        }
        ++physical_count;
        mc_nbt_tag *child = nbt_read_tag(reader, type, true, depth + 1);
        if (child == NULL || !nbt_compound_insert(reader, &builder, child)) {
            return false;
        }
    }
}

static mc_nbt_tag *nbt_read_tag(nbt_reader *reader, uint8_t type, bool named, size_t depth) {
    if (type > MC_NBT_COMPOUND) {
        nbt_reader_fail(reader, MC_NBT_INVALID_TYPE,
                        named ? reader->position - 1 : reader->position);
        return NULL;
    }
    if (depth > reader->limits.max_depth || reader->tags == reader->limits.max_tags) {
        nbt_reader_fail(reader, MC_NBT_LIMIT, reader->position);
        return NULL;
    }
    ++reader->tags;
    mc_nbt_tag *tag = nbt_reader_allocate(reader, 1, sizeof(*tag));
    if (tag == NULL) {
        return NULL;
    }
    tag->type = (mc_nbt_type)type;
    if (type == MC_NBT_END) {
        return tag;
    }
    if (named && !nbt_read_string(reader, &tag->name)) {
        return NULL;
    }
    uint8_t byte;
    uint16_t short_bits;
    uint32_t int_bits;
    uint64_t long_bits;
    size_t count;
    switch (tag->type) {
        case MC_NBT_END:
            break;
        case MC_NBT_BYTE:
            if (!nbt_read_u8(reader, &byte)) {
                return NULL;
            }
            tag->value.byte_value = mc_java_i8_narrow(byte);
            break;
        case MC_NBT_SHORT:
            if (!nbt_read_u16(reader, &short_bits)) {
                return NULL;
            }
            tag->value.short_value = mc_java_i16_narrow(short_bits);
            break;
        case MC_NBT_INT:
            if (!nbt_read_u32(reader, &int_bits)) {
                return NULL;
            }
            tag->value.int_value = mc_java_i32_narrow((int64_t)int_bits);
            break;
        case MC_NBT_LONG:
            if (!nbt_read_u64(reader, &long_bits)) {
                return NULL;
            }
            tag->value.long_value = long_bits <= INT64_MAX ? (int64_t)long_bits
                                                           : -1 - (int64_t)(UINT64_MAX - long_bits);
            break;
        case MC_NBT_FLOAT:
            if (!nbt_read_u32(reader, &tag->value.float_bits)) {
                return NULL;
            }
            break;
        case MC_NBT_DOUBLE:
            if (!nbt_read_u64(reader, &tag->value.double_bits)) {
                return NULL;
            }
            break;
        case MC_NBT_BYTE_ARRAY:
            if (!nbt_read_count(reader, &count) || !nbt_reader_available(reader, count)) {
                return NULL;
            }
            tag->value.byte_array.length = count;
            if (count != 0) {
                uint8_t *data = nbt_reader_allocate(reader, count, sizeof(*data));
                if (data == NULL) {
                    return NULL;
                }
                memcpy(data, reader->input + reader->position, count);
                tag->value.byte_array.data = data;
            }
            reader->position += count;
            break;
        case MC_NBT_STRING:
            if (!nbt_read_string(reader, &tag->value.string)) {
                return NULL;
            }
            break;
        case MC_NBT_LIST:
            if (!nbt_read_list(reader, tag, depth)) {
                return NULL;
            }
            break;
        case MC_NBT_COMPOUND:
            if (!nbt_read_compound_payload(reader, tag, depth)) {
                return NULL;
            }
            break;
    }
    return tag;
}

static mc_nbt_status nbt_read_document(const uint8_t *input, size_t length,
                                       const mc_nbt_limits *requested_limits, bool compound,
                                       mc_nbt_document **out, size_t *consumed,
                                       mc_nbt_error *error) {
    mc_nbt_limits limits = requested_limits == NULL ? mc_nbt_default_limits() : *requested_limits;
    if (out == NULL || (input == NULL && length != 0) || !nbt_limits_valid(&limits)) {
        nbt_set_error(error, MC_NBT_INVALID_ARGUMENT, 0);
        return MC_NBT_INVALID_ARGUMENT;
    }
    if (length > limits.max_input_bytes || sizeof(mc_nbt_document) > limits.max_allocation_bytes) {
        nbt_set_error(error, MC_NBT_LIMIT, 0);
        return MC_NBT_LIMIT;
    }
    mc_nbt_document *document = calloc(1, sizeof(*document));
    if (document == NULL) {
        nbt_set_error(error, MC_NBT_ALLOCATION_FAILED, 0);
        return MC_NBT_ALLOCATION_FAILED;
    }
    document->arena.bytes = sizeof(*document);
    document->arena.limit = limits.max_allocation_bytes;
    document->holds = 1;
    nbt_reader reader = {input, length, 0, 0, limits, document, {MC_NBT_OK, 0}};
    uint8_t type;
    if (nbt_read_u8(&reader, &type)) {
        document->root = nbt_read_tag(&reader, type, true, 1);
    }
    if (reader.error.status == MC_NBT_OK && compound && document->root->type != MC_NBT_COMPOUND) {
        nbt_reader_fail(&reader, MC_NBT_ROOT_TYPE, 0);
    }
    if (reader.error.status != MC_NBT_OK) {
        mc_nbt_status status = reader.error.status;
        nbt_set_error(error, status, reader.error.offset);
        mc_nbt_document_free(document);
        return status;
    }
    *out = document;
    if (consumed != NULL) {
        *consumed = reader.position;
    }
    nbt_set_error(error, MC_NBT_OK, reader.position);
    return MC_NBT_OK;
}

mc_nbt_status mc_nbt_read(const uint8_t *input, size_t length, const mc_nbt_limits *limits,
                          mc_nbt_document **out, size_t *consumed, mc_nbt_error *error) {
    return nbt_read_document(input, length, limits, false, out, consumed, error);
}

mc_nbt_status mc_nbt_read_compound(const uint8_t *input, size_t length, const mc_nbt_limits *limits,
                                   mc_nbt_document **out, size_t *consumed, mc_nbt_error *error) {
    return nbt_read_document(input, length, limits, true, out, consumed, error);
}

static bool nbt_validator_fail(nbt_validator *validator, mc_nbt_status status) {
    if (validator->error.status == MC_NBT_OK) {
        nbt_set_error(&validator->error, status, validator->length);
    }
    return false;
}

static bool nbt_measure_add(nbt_validator *validator, size_t length) {
    if (length > validator->limits.max_output_bytes - validator->length) {
        return nbt_validator_fail(validator, MC_NBT_LIMIT);
    }
    validator->length += length;
    return true;
}

static bool nbt_measure_string(nbt_validator *validator, mc_nbt_string string) {
    if (string.length != 0 && string.units == NULL) {
        return nbt_validator_fail(validator, MC_NBT_INVALID_TREE);
    }
    if (string.length > UINT16_MAX) {
        return nbt_validator_fail(validator, MC_NBT_INVALID_LENGTH);
    }
    size_t encoded;
    if (mc_mutf8_encoded_length(string.units, string.length, &encoded) != MC_MUTF8_OK ||
        encoded > UINT16_MAX) {
        return nbt_validator_fail(validator, MC_NBT_INVALID_LENGTH);
    }
    return nbt_measure_add(validator, 2) && nbt_measure_add(validator, encoded);
}

static bool nbt_measure_tag(nbt_validator *validator, const mc_nbt_tag *tag, bool named,
                            size_t depth);

static bool nbt_measure_list(nbt_validator *validator, const mc_nbt_tag *tag, size_t depth) {
    size_t count = tag->value.list.length;
    if (count > INT32_MAX) {
        return nbt_validator_fail(validator, MC_NBT_INVALID_LENGTH);
    }
    if (count > validator->limits.max_elements ||
        count > validator->limits.max_tags - validator->tags) {
        return nbt_validator_fail(validator, MC_NBT_LIMIT);
    }
    if (count != 0 && tag->value.list.items == NULL) {
        return nbt_validator_fail(validator, MC_NBT_INVALID_TREE);
    }
    if (count != 0 && tag->value.list.element_type > MC_NBT_COMPOUND) {
        return nbt_validator_fail(validator, MC_NBT_INVALID_TYPE);
    }
    if (!nbt_measure_add(validator, 5)) {
        return false;
    }
    for (size_t i = 0; i < count; ++i) {
        const mc_nbt_tag *child = tag->value.list.items[i];
        if (child == NULL || (unsigned int)child->type != tag->value.list.element_type) {
            return nbt_validator_fail(validator, MC_NBT_INVALID_TREE);
        }
        if (!nbt_measure_tag(validator, child, false, depth + 1)) {
            return false;
        }
    }
    return true;
}

static bool nbt_measure_compound(nbt_validator *validator, const mc_nbt_tag *tag, size_t depth) {
    size_t count = tag->value.compound.length;
    if (count > validator->limits.max_elements ||
        count > validator->limits.max_tags - validator->tags) {
        return nbt_validator_fail(validator, MC_NBT_LIMIT);
    }
    if (count != 0 && tag->value.compound.items == NULL) {
        return nbt_validator_fail(validator, MC_NBT_INVALID_TREE);
    }
    const mc_nbt_tag **index = NULL;
    size_t capacity = 0;
    if (count > 1) {
        if (count > SIZE_MAX / 2 || !nbt_grow_capacity(0, count * 2, &capacity)) {
            return nbt_validator_fail(validator, MC_NBT_LIMIT);
        }
        mc_nbt_status status = MC_NBT_OK;
        index = nbt_arena_allocate(&validator->scratch, capacity, sizeof(*index), &status);
        if (index == NULL) {
            return nbt_validator_fail(validator, status);
        }
    }
    for (size_t i = 0; i < count; ++i) {
        const mc_nbt_tag *child = tag->value.compound.items[i];
        if (child == NULL || child->type == MC_NBT_END ||
            (child->name.length != 0 && child->name.units == NULL)) {
            return nbt_validator_fail(validator, MC_NBT_INVALID_TREE);
        }
        if (child->name.length > UINT16_MAX) {
            return nbt_validator_fail(validator, MC_NBT_INVALID_LENGTH);
        }
        if (index != NULL) {
            size_t slot = nbt_string_hash(child->name) & (capacity - 1);
            while (index[slot] != NULL) {
                if (nbt_string_equal(child->name, index[slot]->name)) {
                    return nbt_validator_fail(validator, MC_NBT_INVALID_TREE);
                }
                slot = (slot + 1) & (capacity - 1);
            }
            index[slot] = child;
        }
        if (!nbt_measure_tag(validator, child, true, depth + 1)) {
            return false;
        }
    }
    return nbt_measure_add(validator, 1);
}

static bool nbt_measure_tag(nbt_validator *validator, const mc_nbt_tag *tag, bool named,
                            size_t depth) {
    if (tag == NULL) {
        return nbt_validator_fail(validator, MC_NBT_INVALID_TREE);
    }
    for (size_t i = 0; i < depth - 1; ++i) {
        if (validator->ancestors[i] == tag) {
            return nbt_validator_fail(validator, MC_NBT_INVALID_TREE);
        }
    }
    if (depth > validator->limits.max_depth || validator->tags == validator->limits.max_tags) {
        return nbt_validator_fail(validator, MC_NBT_LIMIT);
    }
    if ((unsigned int)tag->type > (unsigned int)MC_NBT_COMPOUND) {
        return nbt_validator_fail(validator, MC_NBT_INVALID_TYPE);
    }
    validator->ancestors[depth - 1] = tag;
    ++validator->tags;
    if (named && !nbt_measure_add(validator, 1)) {
        return false;
    }
    if (tag->type == MC_NBT_END) {
        return true;
    }
    if (named && !nbt_measure_string(validator, tag->name)) {
        return false;
    }
    switch (tag->type) {
        case MC_NBT_END:
            return true;
        case MC_NBT_BYTE:
            return nbt_measure_add(validator, 1);
        case MC_NBT_SHORT:
            return nbt_measure_add(validator, 2);
        case MC_NBT_INT:
        case MC_NBT_FLOAT:
            return nbt_measure_add(validator, 4);
        case MC_NBT_LONG:
        case MC_NBT_DOUBLE:
            return nbt_measure_add(validator, 8);
        case MC_NBT_BYTE_ARRAY:
            if (tag->value.byte_array.length > INT32_MAX) {
                return nbt_validator_fail(validator, MC_NBT_INVALID_LENGTH);
            }
            if (tag->value.byte_array.length > validator->limits.max_elements) {
                return nbt_validator_fail(validator, MC_NBT_LIMIT);
            }
            if (tag->value.byte_array.length != 0 && tag->value.byte_array.data == NULL) {
                return nbt_validator_fail(validator, MC_NBT_INVALID_TREE);
            }
            return nbt_measure_add(validator, 4) &&
                   nbt_measure_add(validator, tag->value.byte_array.length);
        case MC_NBT_STRING:
            return nbt_measure_string(validator, tag->value.string);
        case MC_NBT_LIST:
            return nbt_measure_list(validator, tag, depth);
        case MC_NBT_COMPOUND:
            return nbt_measure_compound(validator, tag, depth);
    }
    return nbt_validator_fail(validator, MC_NBT_INVALID_TYPE);
}

mc_nbt_status mc_nbt_measure(const mc_nbt_tag *root, const mc_nbt_limits *requested_limits,
                             size_t *written, mc_nbt_error *error) {
    mc_nbt_limits limits = requested_limits == NULL ? mc_nbt_default_limits() : *requested_limits;
    if (root == NULL || written == NULL || !nbt_limits_valid(&limits)) {
        nbt_set_error(error, MC_NBT_INVALID_ARGUMENT, 0);
        return MC_NBT_INVALID_ARGUMENT;
    }
    nbt_validator validator = {0};
    validator.limits = limits;
    validator.scratch.limit = limits.max_allocation_bytes;
    bool valid = nbt_measure_tag(&validator, root, true, 1);
    nbt_arena_free(&validator.scratch);
    if (!valid) {
        nbt_set_error(error, validator.error.status, validator.error.offset);
        return validator.error.status;
    }
    *written = validator.length;
    nbt_set_error(error, MC_NBT_OK, validator.length);
    return MC_NBT_OK;
}

static void nbt_write_u16(uint8_t *output, size_t *position, uint16_t value) {
    output[(*position)++] = (uint8_t)(value >> 8u);
    output[(*position)++] = (uint8_t)value;
}

static void nbt_write_u32(uint8_t *output, size_t *position, uint32_t value) {
    for (unsigned int i = 0; i < 4; ++i) {
        output[(*position)++] = (uint8_t)(value >> ((3u - i) * 8u));
    }
}

static void nbt_write_u64(uint8_t *output, size_t *position, uint64_t value) {
    for (unsigned int i = 0; i < 8; ++i) {
        output[(*position)++] = (uint8_t)(value >> ((7u - i) * 8u));
    }
}

static void nbt_write_string(uint8_t *output, size_t *position, mc_nbt_string string) {
    size_t encoded = 0;
    size_t written = 0;
    (void)mc_mutf8_encoded_length(string.units, string.length, &encoded);
    nbt_write_u16(output, position, (uint16_t)encoded);
    (void)mc_mutf8_encode(string.units, string.length, output + *position, encoded, &written);
    *position += encoded;
}

/* Validation already established every bound, pointer and tree invariant.
 * Emission has no allocation or failure path for the immutable borrowed tree. */
static void nbt_write_tag(uint8_t *output, size_t *position, const mc_nbt_tag *tag, bool named) {
    if (named) {
        output[(*position)++] = (uint8_t)tag->type;
    }
    if (tag->type == MC_NBT_END) {
        return;
    }
    if (named) {
        nbt_write_string(output, position, tag->name);
    }
    switch (tag->type) {
        case MC_NBT_END:
            break;
        case MC_NBT_BYTE:
            output[(*position)++] = (uint8_t)tag->value.byte_value;
            break;
        case MC_NBT_SHORT:
            nbt_write_u16(output, position, (uint16_t)tag->value.short_value);
            break;
        case MC_NBT_INT:
            nbt_write_u32(output, position, (uint32_t)tag->value.int_value);
            break;
        case MC_NBT_LONG:
            nbt_write_u64(output, position, (uint64_t)tag->value.long_value);
            break;
        case MC_NBT_FLOAT: {
            uint32_t bits = tag->value.float_bits;
            if ((bits & UINT32_C(0x7f800000)) == UINT32_C(0x7f800000) &&
                (bits & UINT32_C(0x007fffff)) != 0) {
                bits = UINT32_C(0x7fc00000);
            }
            nbt_write_u32(output, position, bits);
            break;
        }
        case MC_NBT_DOUBLE: {
            uint64_t bits = tag->value.double_bits;
            if ((bits & UINT64_C(0x7ff0000000000000)) == UINT64_C(0x7ff0000000000000) &&
                (bits & UINT64_C(0x000fffffffffffff)) != 0) {
                bits = UINT64_C(0x7ff8000000000000);
            }
            nbt_write_u64(output, position, bits);
            break;
        }
        case MC_NBT_BYTE_ARRAY:
            nbt_write_u32(output, position, (uint32_t)tag->value.byte_array.length);
            if (tag->value.byte_array.length != 0) {
                memcpy(output + *position, tag->value.byte_array.data,
                       tag->value.byte_array.length);
            }
            *position += tag->value.byte_array.length;
            break;
        case MC_NBT_STRING:
            nbt_write_string(output, position, tag->value.string);
            break;
        case MC_NBT_LIST:
            output[(*position)++] =
                tag->value.list.length == 0 ? MC_NBT_BYTE : tag->value.list.element_type;
            nbt_write_u32(output, position, (uint32_t)tag->value.list.length);
            for (size_t i = 0; i < tag->value.list.length; ++i) {
                nbt_write_tag(output, position, tag->value.list.items[i], false);
            }
            break;
        case MC_NBT_COMPOUND:
            for (size_t i = 0; i < tag->value.compound.length; ++i) {
                nbt_write_tag(output, position, tag->value.compound.items[i], true);
            }
            output[(*position)++] = MC_NBT_END;
            break;
    }
}

mc_nbt_status mc_nbt_write(const mc_nbt_tag *root, const mc_nbt_limits *limits, uint8_t *output,
                           size_t capacity, size_t *written, mc_nbt_error *error) {
    if (written == NULL || (output == NULL && capacity != 0)) {
        nbt_set_error(error, MC_NBT_INVALID_ARGUMENT, 0);
        return MC_NBT_INVALID_ARGUMENT;
    }
    size_t measured;
    mc_nbt_status status = mc_nbt_measure(root, limits, &measured, error);
    if (status != MC_NBT_OK) {
        return status;
    }
    if (capacity < measured) {
        nbt_set_error(error, MC_NBT_BUFFER_TOO_SMALL, measured);
        return MC_NBT_BUFFER_TOO_SMALL;
    }
    size_t position = 0;
    nbt_write_tag(output, &position, root, true);
    *written = position;
    nbt_set_error(error, MC_NBT_OK, position);
    return MC_NBT_OK;
}
