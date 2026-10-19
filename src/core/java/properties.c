#include "mc/core/java/properties.h"

#include <limits.h>
#include <string.h>

_Static_assert(CHAR_BIT == 8, "Properties InputStream decoding requires eight-bit bytes");

/* Older MSVC diagnoses this standard C17 flexible array as extension C4200.
 * Keep the allocation layout and scope the compatibility guard to this type. */
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4200)
#endif
struct mc_java_properties_string {
    size_t length;
    uint16_t units[];
};
#ifdef _MSC_VER
#pragma warning(pop)
#endif

typedef struct properties_entry {
    const mc_java_properties_string *key;
    const mc_java_properties_string *value;
    struct properties_entry *next;
} properties_entry;

struct mc_java_properties {
    mc_java_properties_allocator allocator;
    properties_entry *first;
    properties_entry *last;
    size_t size;
};

typedef struct properties_line_reader {
    mc_java_properties *properties;
    mc_java_properties_input input;
    uint8_t *bytes;
    uint16_t *line;
    size_t capacity;
    size_t input_offset;
    size_t input_limit;
} properties_line_reader;

static bool key_equal(const mc_java_properties_string *string, const uint16_t *units, size_t length)
{
    if (string->length != length)
        return false;
    for (size_t i = 0; i < length; ++i) {
        if (string->units[i] != units[i])
            return false;
    }
    return true;
}

static properties_entry *find_entry(const mc_java_properties *properties, const uint16_t *key,
                                    size_t length)
{
    for (properties_entry *entry = properties->first; entry != NULL; entry = entry->next) {
        if (key_equal(entry->key, key, length))
            return entry;
    }
    return NULL;
}

mc_java_properties_status mc_java_properties_create(const mc_java_properties_allocator *allocator,
                                                    mc_java_properties **out)
{
    if (allocator == NULL || allocator->allocate == NULL || out == NULL)
        return MC_JAVA_PROPERTIES_INVALID_ARGUMENT;
    const mc_java_properties_allocator selected = *allocator;
    mc_java_properties *properties = selected.allocate(selected.context, sizeof(*properties));
    if (properties == NULL)
        return MC_JAVA_PROPERTIES_ALLOCATION_FAILED;
    *properties =
        (mc_java_properties){.allocator = selected, .first = NULL, .last = NULL, .size = 0};
    *out = properties;
    return MC_JAVA_PROPERTIES_OK;
}

/* This is the reached ArraysSupport.newLength(old, 1, old) policy. The
 * widened arithmetic selects the same positive Java-int results without
 * evaluating a signed-overflow expression in C. */
static mc_java_properties_status grow_line(properties_line_reader *reader)
{
    const uint64_t old_length = (uint64_t)reader->capacity;
    const uint64_t soft_maximum = (uint64_t)INT32_MAX - UINT64_C(8);
    const uint64_t preferred = old_length + old_length;
    uint64_t next_length;
    if (preferred > 0 && preferred <= soft_maximum) {
        next_length = preferred;
    } else {
        const uint64_t minimum = old_length + UINT64_C(1);
        if (minimum > (uint64_t)INT32_MAX)
            return MC_JAVA_PROPERTIES_LIMIT;
        next_length = minimum <= soft_maximum ? soft_maximum : minimum;
    }
    if (next_length > (uint64_t)(SIZE_MAX / sizeof(*reader->line)))
        return MC_JAVA_PROPERTIES_LIMIT;
    const size_t capacity = (size_t)next_length;
    uint16_t *line = reader->properties->allocator.allocate(reader->properties->allocator.context,
                                                            capacity * sizeof(*line));
    if (line == NULL)
        return MC_JAVA_PROPERTIES_ALLOCATION_FAILED;
    memcpy(line, reader->line, reader->capacity * sizeof(*line));
    reader->line = line;
    reader->capacity = capacity;
    return MC_JAVA_PROPERTIES_OK;
}

static mc_java_properties_status refill(properties_line_reader *reader, size_t *limit)
{
    size_t count = 0;
    const mc_java_properties_status status =
        reader->input.read(reader->input.context, reader->bytes, 8192u, &count);
    if (count > 8192u)
        return MC_JAVA_PROPERTIES_INVALID_ARGUMENT;
    if (status == MC_JAVA_PROPERTIES_IO_ERROR) {
        if (count != 0)
            return MC_JAVA_PROPERTIES_INVALID_ARGUMENT;
        return status;
    }
    if (status != MC_JAVA_PROPERTIES_OK)
        return MC_JAVA_PROPERTIES_INVALID_ARGUMENT;
    reader->input_limit = count;
    *limit = count;
    return MC_JAVA_PROPERTIES_OK;
}

static bool properties_space(uint16_t c)
{
    return c == ' ' || c == '\t' || c == '\f';
}

static mc_java_properties_status read_line(properties_line_reader *reader, size_t *length,
                                           bool *present)
{
    size_t used = 0;
    size_t offset = reader->input_offset;
    size_t limit = reader->input_limit;
    bool skip_space = true;
    bool continuation_start = false;
    bool odd_backslash = false;

    for (;;) {
        if (offset >= limit) {
            const mc_java_properties_status status = refill(reader, &limit);
            if (status != MC_JAVA_PROPERTIES_OK)
                return status;
            if (limit == 0) {
                *present = used != 0;
                *length = odd_backslash ? used - 1u : used;
                return MC_JAVA_PROPERTIES_OK;
            }
            offset = 0;
        }
        const uint16_t c = reader->bytes[offset++];
        if (skip_space) {
            if (properties_space(c))
                continue;
            if (!continuation_start && (c == '\r' || c == '\n'))
                continue;
            skip_space = false;
            continuation_start = false;
        }
        if (used == 0 && (c == '#' || c == '!')) {
            bool terminated = false;
            while (!terminated) {
                while (offset < limit) {
                    const uint8_t b = reader->bytes[offset++];
                    if (b == '\r' || b == '\n') {
                        terminated = true;
                        break;
                    }
                }
                if (!terminated && offset == limit) {
                    const mc_java_properties_status status = refill(reader, &limit);
                    if (status != MC_JAVA_PROPERTIES_OK)
                        return status;
                    if (limit == 0) {
                        *present = false;
                        *length = 0;
                        return MC_JAVA_PROPERTIES_OK;
                    }
                    offset = 0;
                }
            }
            skip_space = true;
            continue;
        }
        if (c != '\n' && c != '\r') {
            reader->line[used++] = c;
            /* The source grows immediately after the write that fills the
             * array, even if the next byte will terminate the logical line. */
            if (used == reader->capacity) {
                const mc_java_properties_status status = grow_line(reader);
                if (status != MC_JAVA_PROPERTIES_OK)
                    return status;
            }
            odd_backslash = c == '\\' ? !odd_backslash : false;
            continue;
        }
        if (used == 0) {
            skip_space = true;
            continue;
        }
        /* EOL refill precedes both continuation handling and line return.
         * An I/O failure here therefore prevents this line's map update. */
        if (offset >= limit) {
            const mc_java_properties_status status = refill(reader, &limit);
            if (status != MC_JAVA_PROPERTIES_OK)
                return status;
            offset = 0;
            if (limit == 0) {
                *present = true;
                *length = odd_backslash ? used - 1u : used;
                return MC_JAVA_PROPERTIES_OK;
            }
        }
        if (odd_backslash) {
            --used;
            skip_space = true;
            continuation_start = true;
            odd_backslash = false;
            if (c == '\r' && reader->bytes[offset] == '\n')
                ++offset;
        } else {
            reader->input_offset = offset;
            *length = used;
            *present = true;
            return MC_JAVA_PROPERTIES_OK;
        }
    }
}

/* A validation/count pass precedes the immutable String allocation. The same
 * decoder fills that String on a second pass; no mutable buffer is exposed. */
static mc_java_properties_status decode_units(const uint16_t *input, size_t length,
                                              uint16_t *output, size_t *decoded_length)
{
    size_t offset = 0;
    while (offset < length) {
        if (input[offset++] == '\\')
            break;
    }
    if (offset == length) {
        if (output != NULL && length != 0)
            memcpy(output, input, length * sizeof(*output));
        *decoded_length = length;
        return MC_JAVA_PROPERTIES_OK;
    }
    --offset;
    size_t written = offset;
    if (output != NULL && written != 0)
        memcpy(output, input, written * sizeof(*output));
    while (offset < length) {
        uint16_t c = input[offset++];
        if (c == '\\') {
            /* read_line removes an unescaped trailing backslash. */
            if (offset == length)
                return MC_JAVA_PROPERTIES_MALFORMED_UNICODE;
            c = input[offset++];
            if (c == 'u') {
                if (length - offset < 4u)
                    return MC_JAVA_PROPERTIES_MALFORMED_UNICODE;
                uint32_t value = 0;
                for (unsigned int i = 0; i < 4u; ++i) {
                    const uint16_t digit = input[offset++];
                    uint32_t nibble;
                    if (digit >= '0' && digit <= '9')
                        nibble = (uint32_t)(digit - '0');
                    else if (digit >= 'a' && digit <= 'f')
                        nibble = (uint32_t)(digit - 'a') + UINT32_C(10);
                    else if (digit >= 'A' && digit <= 'F')
                        nibble = (uint32_t)(digit - 'A') + UINT32_C(10);
                    else
                        return MC_JAVA_PROPERTIES_MALFORMED_UNICODE;
                    value = (value << 4) + nibble;
                }
                c = (uint16_t)value;
            } else if (c == 't') {
                c = '\t';
            } else if (c == 'r') {
                c = '\r';
            } else if (c == 'n') {
                c = '\n';
            } else if (c == 'f') {
                c = '\f';
            }
        }
        if (output != NULL)
            output[written] = c;
        ++written;
    }
    *decoded_length = written;
    return MC_JAVA_PROPERTIES_OK;
}

static mc_java_properties_status convert_string(mc_java_properties *properties,
                                                const uint16_t *input, size_t length,
                                                const mc_java_properties_string **out)
{
    size_t decoded_length = 0;
    mc_java_properties_status status = decode_units(input, length, NULL, &decoded_length);
    if (status != MC_JAVA_PROPERTIES_OK)
        return status;
    if (decoded_length > (SIZE_MAX - sizeof(mc_java_properties_string)) / sizeof(uint16_t))
        return MC_JAVA_PROPERTIES_LIMIT;
    const size_t bytes = sizeof(mc_java_properties_string) + decoded_length * sizeof(uint16_t);
    mc_java_properties_string *string =
        properties->allocator.allocate(properties->allocator.context, bytes);
    if (string == NULL)
        return MC_JAVA_PROPERTIES_ALLOCATION_FAILED;
    size_t actual_length = 0;
    status = decode_units(input, length, string->units, &actual_length);
    if (status != MC_JAVA_PROPERTIES_OK)
        return status;
    string->length = actual_length;
    *out = string;
    return MC_JAVA_PROPERTIES_OK;
}

static mc_java_properties_status put_entry(mc_java_properties *properties,
                                           const mc_java_properties_string *key,
                                           const mc_java_properties_string *value)
{
    properties_entry *entry = find_entry(properties, key->units, key->length);
    if (entry != NULL) {
        entry->value = value;
        return MC_JAVA_PROPERTIES_OK;
    }
    if (properties->size == SIZE_MAX)
        return MC_JAVA_PROPERTIES_LIMIT;
    entry = properties->allocator.allocate(properties->allocator.context, sizeof(*entry));
    if (entry == NULL)
        return MC_JAVA_PROPERTIES_ALLOCATION_FAILED;
    *entry = (properties_entry){.key = key, .value = value, .next = NULL};
    if (properties->last == NULL)
        properties->first = entry;
    else
        properties->last->next = entry;
    properties->last = entry;
    ++properties->size;
    return MC_JAVA_PROPERTIES_OK;
}

static mc_java_properties_status put_line(mc_java_properties *properties, const uint16_t *line,
                                          size_t length)
{
    size_t key_length = 0;
    size_t value_start = length;
    bool separator = false;
    bool odd_backslash = false;
    while (key_length < length) {
        const uint16_t c = line[key_length];
        if ((c == '=' || c == ':') && !odd_backslash) {
            value_start = key_length + 1u;
            separator = true;
            break;
        }
        if (properties_space(c) && !odd_backslash) {
            value_start = key_length + 1u;
            break;
        }
        odd_backslash = c == '\\' ? !odd_backslash : false;
        ++key_length;
    }
    while (value_start < length) {
        const uint16_t c = line[value_start];
        if (!properties_space(c)) {
            if (!separator && (c == '=' || c == ':'))
                separator = true;
            else
                break;
        }
        ++value_start;
    }
    const mc_java_properties_string *key = NULL;
    mc_java_properties_status status = convert_string(properties, line, key_length, &key);
    if (status != MC_JAVA_PROPERTIES_OK)
        return status;
    const mc_java_properties_string *value = NULL;
    status = convert_string(properties, line + value_start, length - value_start, &value);
    if (status != MC_JAVA_PROPERTIES_OK)
        return status;
    return put_entry(properties, key, value);
}

mc_java_properties_status mc_java_properties_load(mc_java_properties *properties,
                                                  const mc_java_properties_input *input)
{
    if (properties == NULL || input == NULL || input->read == NULL)
        return MC_JAVA_PROPERTIES_INVALID_ARGUMENT;
    properties_line_reader reader = {.properties = properties, .input = *input};
    /* The source LineReader's logical array initializer precedes its byte
     * buffer constructor allocation. Every arena allocation is retained. */
    reader.line =
        properties->allocator.allocate(properties->allocator.context, 1024u * sizeof(*reader.line));
    if (reader.line == NULL)
        return MC_JAVA_PROPERTIES_ALLOCATION_FAILED;
    reader.capacity = 1024u;
    reader.bytes = properties->allocator.allocate(properties->allocator.context, 8192u);
    if (reader.bytes == NULL)
        return MC_JAVA_PROPERTIES_ALLOCATION_FAILED;
    for (;;) {
        size_t length = 0;
        bool present = false;
        mc_java_properties_status status = read_line(&reader, &length, &present);
        if (status != MC_JAVA_PROPERTIES_OK)
            return status;
        if (!present)
            return MC_JAVA_PROPERTIES_OK;
        status = put_line(properties, reader.line, length);
        if (status != MC_JAVA_PROPERTIES_OK)
            return status;
    }
}

mc_java_properties_status mc_java_properties_get(const mc_java_properties *properties,
                                                 const uint16_t *key, size_t length,
                                                 const mc_java_properties_string **out)
{
    if (properties == NULL || out == NULL || (key == NULL && length != 0))
        return MC_JAVA_PROPERTIES_INVALID_ARGUMENT;
    if ((uintmax_t)length > (uintmax_t)INT32_MAX)
        return MC_JAVA_PROPERTIES_LIMIT;
    const properties_entry *entry = find_entry(properties, key, length);
    *out = entry == NULL ? NULL : entry->value;
    return MC_JAVA_PROPERTIES_OK;
}

bool mc_java_properties_string_read(const mc_java_properties_string *string,
                                    mc_java_properties_string_view *out)
{
    if (string == NULL || out == NULL)
        return false;
    *out = (mc_java_properties_string_view){.units = string->units, .length = string->length};
    return true;
}

size_t mc_java_properties_size(const mc_java_properties *properties)
{
    return properties == NULL ? 0 : properties->size;
}

mc_java_properties_status mc_java_properties_entry(const mc_java_properties *properties,
                                                   size_t index,
                                                   const mc_java_properties_string **key,
                                                   const mc_java_properties_string **value)
{
    if (properties == NULL || key == NULL || value == NULL || key == value)
        return MC_JAVA_PROPERTIES_INVALID_ARGUMENT;
    if (index >= properties->size)
        return MC_JAVA_PROPERTIES_INDEX_OUT_OF_RANGE;
    const properties_entry *entry = properties->first;
    while (index != 0) {
        entry = entry->next;
        --index;
    }
    *key = entry->key;
    *value = entry->value;
    return MC_JAVA_PROPERTIES_OK;
}
