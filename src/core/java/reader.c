#include "mc/core/java/reader.h"

#include <limits.h>
#include <string.h>

_Static_assert(CHAR_BIT == 8, "InputStreamReader decoding requires eight-bit bytes");

#define READER_BUFFER_SIZE 8192u

/* StringBuilder.toString uses StringLatin1/StringUTF16.newString, whose
 * zero-length result is the shared empty literal. The direct char[] String
 * constructor in BufferedReader's other branch still creates a fresh owner. */
static const mc_java_reader_line empty_literal = {.units = NULL, .length = 0};

struct mc_java_reader {
    mc_java_properties_allocator allocator;
    mc_java_reader_input input;
    mc_java_reader_charset charset;
    bool closed;
    bool skip_lf;
    uint8_t *bytes;
    size_t byte_position;
    size_t byte_limit;
    uint16_t *characters;
    size_t character_position;
    size_t character_limit;
};

/* Older MSVC diagnoses these standard C17 flexible arrays as extension C4200.
 * Keep the layouts and scope the compatibility guard to these two types. */
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4200)
#endif
typedef struct line_piece {
    struct line_piece *next;
    size_t length;
    uint16_t units[];
} line_piece;

typedef struct line_storage {
    mc_java_reader_line view;
    uint16_t units[];
} line_storage;
#ifdef _MSC_VER
#pragma warning(pop)
#endif

typedef enum decode_kind {
    DECODE_VALUE,
    DECODE_UNDERFLOW,
    DECODE_OVERFLOW,
    DECODE_MALFORMED
} decode_kind;

typedef struct decode_item {
    decode_kind kind;
    size_t consumed;
    size_t produced;
    uint16_t first, second;
} decode_item;

static bool continuation(uint8_t byte) {
    return (byte & 0xc0u) == 0x80u;
}

static bool bad_second_of_three(uint8_t first, uint8_t second) {
    return (first == 0xe0u && (second & 0xe0u) == 0x80u) || !continuation(second);
}

static bool bad_second_of_four(uint8_t first, uint8_t second) {
    return (first == 0xf0u && (second < 0x90u || second > 0xbfu)) ||
           (first == 0xf4u && (second & 0xf0u) != 0x80u) || !continuation(second);
}

static decode_item flow(size_t available, size_t required) {
    return (decode_item){.kind = available < required ? DECODE_UNDERFLOW : DECODE_OVERFLOW};
}

/* The array-backed UTF_8 decoder decides malformed prefix length before the
 * CharsetDecoder replacement wrapper advances the input. Output-space checks
 * deliberately retain their source order, including the four-byte branch. */
static decode_item utf8_item(const uint8_t *bytes, size_t available, size_t room) {
    const uint8_t first = bytes[0];
    if (first < 0x80u) {
        if (room == 0)
            return flow(available, 1);
        return (decode_item){.kind = DECODE_VALUE, .consumed = 1, .produced = 1, .first = first};
    }
    if (first >= 0xc2u && first <= 0xdfu) {
        if (available < 2u || room == 0)
            return flow(available, 2);
        if (!continuation(bytes[1]))
            return (decode_item){.kind = DECODE_MALFORMED, .consumed = 1};
        const uint32_t unit = ((uint32_t)(first & 0x1fu) << 6) | (bytes[1] & 0x3fu);
        return (decode_item){
            .kind = DECODE_VALUE, .consumed = 2, .produced = 1, .first = (uint16_t)unit};
    }
    if (first >= 0xe0u && first <= 0xefu) {
        if (available < 3u || room == 0) {
            if (available > 1u && bad_second_of_three(first, bytes[1]))
                return (decode_item){.kind = DECODE_MALFORMED, .consumed = 1};
            return flow(available, 3);
        }
        if (bad_second_of_three(first, bytes[1]))
            return (decode_item){.kind = DECODE_MALFORMED, .consumed = 1};
        if (!continuation(bytes[2]))
            return (decode_item){.kind = DECODE_MALFORMED, .consumed = 2};
        const uint32_t unit = ((uint32_t)(first & 0x0fu) << 12) |
                              ((uint32_t)(bytes[1] & 0x3fu) << 6) | (bytes[2] & 0x3fu);
        if (unit >= 0xd800u && unit <= 0xdfffu)
            return (decode_item){.kind = DECODE_MALFORMED, .consumed = 3};
        return (decode_item){
            .kind = DECODE_VALUE, .consumed = 3, .produced = 1, .first = (uint16_t)unit};
    }
    if (first >= 0xf0u && first <= 0xf7u) {
        if (available < 4u || room < 2u) {
            if (first > 0xf4u || (available > 1u && bad_second_of_four(first, bytes[1])))
                return (decode_item){.kind = DECODE_MALFORMED, .consumed = 1};
            if (available > 2u && !continuation(bytes[2]))
                return (decode_item){.kind = DECODE_MALFORMED, .consumed = 2};
            return flow(available, 4);
        }
        const uint32_t codepoint = ((uint32_t)(first & 0x07u) << 18) |
                                   ((uint32_t)(bytes[1] & 0x3fu) << 12) |
                                   ((uint32_t)(bytes[2] & 0x3fu) << 6) | (bytes[3] & 0x3fu);
        if (!continuation(bytes[1]) || !continuation(bytes[2]) || !continuation(bytes[3]) ||
            codepoint < 0x10000u || codepoint > 0x10ffffu) {
            size_t consumed = 3;
            if (first > 0xf4u || bad_second_of_four(first, bytes[1]))
                consumed = 1;
            else if (!continuation(bytes[2]))
                consumed = 2;
            return (decode_item){.kind = DECODE_MALFORMED, .consumed = consumed};
        }
        return (decode_item){.kind = DECODE_VALUE,
                             .consumed = 4,
                             .produced = 2,
                             .first = (uint16_t)(0xd800u + ((codepoint - 0x10000u) >> 10)),
                             .second = (uint16_t)(0xdc00u + (codepoint & 0x3ffu))};
    }
    return (decode_item){.kind = DECODE_MALFORMED, .consumed = 1};
}

static uint16_t single_byte(mc_java_reader_charset charset, uint8_t byte) {
    static const uint16_t windows_controls[32] = {
        0x20ac, 0xfffd, 0x201a, 0x0192, 0x201e, 0x2026, 0x2020, 0x2021, 0x02c6, 0x2030, 0x0160,
        0x2039, 0x0152, 0xfffd, 0x017d, 0xfffd, 0xfffd, 0x2018, 0x2019, 0x201c, 0x201d, 0x2022,
        0x2013, 0x2014, 0x02dc, 0x2122, 0x0161, 0x203a, 0x0153, 0xfffd, 0x017e, 0x0178};
    if (charset == MC_JAVA_READER_US_ASCII && byte >= 0x80u)
        return 0xfffd;
    if (charset == MC_JAVA_READER_WINDOWS1252 && byte >= 0x80u && byte <= 0x9fu)
        return windows_controls[byte - 0x80u];
    return byte;
}

static decode_kind decode_buffer(mc_java_reader *reader, size_t *written, bool eof) {
    while (reader->byte_position < reader->byte_limit) {
        const size_t available = reader->byte_limit - reader->byte_position;
        const size_t room = READER_BUFFER_SIZE - *written;
        decode_item item;
        if (reader->charset == MC_JAVA_READER_UTF8) {
            item = utf8_item(reader->bytes + reader->byte_position, available, room);
        } else {
            if (room == 0)
                return DECODE_OVERFLOW;
            item = (decode_item){
                .kind = DECODE_VALUE,
                .consumed = 1,
                .produced = 1,
                .first = single_byte(reader->charset, reader->bytes[reader->byte_position])};
        }
        if (item.kind == DECODE_UNDERFLOW) {
            if (!eof)
                return DECODE_UNDERFLOW;
            /* CharsetDecoder converts a final incomplete prefix into one
             * malformed span covering every remaining byte. */
            item = (decode_item){.kind = DECODE_MALFORMED, .consumed = available};
        }
        if (item.kind == DECODE_OVERFLOW)
            return DECODE_OVERFLOW;
        if (item.kind == DECODE_MALFORMED) {
            if (room == 0)
                return DECODE_OVERFLOW;
            reader->characters[(*written)++] = 0xfffd;
        } else {
            reader->characters[(*written)++] = item.first;
            if (item.produced == 2u)
                reader->characters[(*written)++] = item.second;
        }
        reader->byte_position += item.consumed;
    }
    return DECODE_UNDERFLOW;
}

static mc_java_reader_status read_bytes(mc_java_reader *reader, bool *eof) {
    const size_t remaining = reader->byte_limit - reader->byte_position;
    if (remaining >= READER_BUFFER_SIZE)
        return MC_JAVA_READER_INVALID_ARGUMENT;
    if (remaining != 0)
        memmove(reader->bytes, reader->bytes + reader->byte_position, remaining);
    /* compact followed by finally-flip preserves this remaining prefix even
     * if the real input read fails. Newly written error bytes are not exposed. */
    reader->byte_position = 0;
    reader->byte_limit = remaining;
    const size_t capacity = READER_BUFFER_SIZE - remaining;
    size_t count = 0;
    const mc_java_properties_status status = reader->input.bytes.read(
        reader->input.bytes.context, reader->bytes + remaining, capacity, &count);
    if (count > capacity)
        return MC_JAVA_READER_INVALID_ARGUMENT;
    if (status == MC_JAVA_PROPERTIES_IO_ERROR) {
        if (count != 0)
            return MC_JAVA_READER_INVALID_ARGUMENT;
        return MC_JAVA_READER_IO_ERROR;
    }
    if (status != MC_JAVA_PROPERTIES_OK)
        return MC_JAVA_READER_INVALID_ARGUMENT;
    reader->byte_limit = remaining + count;
    *eof = count == 0;
    return MC_JAVA_READER_OK;
}

/* The reached unmarked BufferedReader always asks its InputStreamReader for
 * 8192 characters. Its StreamDecoder leftover flag starts false, so neither
 * the len==1/read0 path nor a leftover-char branch is reached by this API. */
static mc_java_reader_status fill_characters(mc_java_reader *reader) {
    size_t written = 0;
    bool eof = false;
    for (;;) {
        const decode_kind kind = decode_buffer(reader, &written, eof);
        if (kind == DECODE_OVERFLOW)
            break;
        if (eof || written == READER_BUFFER_SIZE)
            break;
        if (written != 0 &&
            (reader->input.ready == NULL || !reader->input.ready(reader->input.bytes.context)))
            break;
        mc_java_reader_status status = read_bytes(reader, &eof);
        if (status != MC_JAVA_READER_OK)
            return status;
        if (eof && written == 0 && reader->byte_position == reader->byte_limit)
            break;
        /* The four admitted charset decoders have no charset-specific reset
         * state. StreamDecoder's EOF resets therefore retain no extra data. */
    }
    if (written != 0) {
        reader->character_limit = written;
        reader->character_position = 0;
    } else if (!eof) {
        return MC_JAVA_READER_INVALID_ARGUMENT;
    }
    return MC_JAVA_READER_OK;
}

static mc_java_reader_status append_piece(mc_java_reader *reader, line_piece **first,
                                          line_piece **last, size_t *total, const uint16_t *units,
                                          size_t length) {
    if (length > (size_t)INT32_MAX - *total ||
        length > (SIZE_MAX - sizeof(line_piece)) / sizeof(uint16_t))
        return MC_JAVA_READER_LIMIT;
    line_piece *piece = reader->allocator.allocate(reader->allocator.context,
                                                   sizeof(*piece) + length * sizeof(*piece->units));
    if (piece == NULL)
        return MC_JAVA_READER_ALLOCATION_FAILED;
    piece->next = NULL;
    piece->length = length;
    if (length != 0)
        memcpy(piece->units, units, length * sizeof(*units));
    if (*last == NULL)
        *first = piece;
    else
        (*last)->next = piece;
    *last = piece;
    *total += length;
    return MC_JAVA_READER_OK;
}

static mc_java_reader_status finish_line(mc_java_reader *reader, const line_piece *first,
                                         size_t prefix_length, const uint16_t *tail,
                                         size_t tail_length, const mc_java_reader_line **out) {
    if (tail_length > (size_t)INT32_MAX - prefix_length)
        return MC_JAVA_READER_LIMIT;
    const size_t length = prefix_length + tail_length;
    if (length == 0 && first != NULL) {
        *out = &empty_literal;
        return MC_JAVA_READER_OK;
    }
    if (length > (SIZE_MAX - sizeof(line_storage)) / sizeof(uint16_t))
        return MC_JAVA_READER_LIMIT;
    line_storage *storage = reader->allocator.allocate(
        reader->allocator.context, sizeof(*storage) + length * sizeof(*storage->units));
    if (storage == NULL)
        return MC_JAVA_READER_ALLOCATION_FAILED;
    size_t offset = 0;
    for (const line_piece *piece = first; piece != NULL; piece = piece->next) {
        if (piece->length != 0)
            memcpy(storage->units + offset, piece->units, piece->length * sizeof(*piece->units));
        offset += piece->length;
    }
    if (tail_length != 0)
        memcpy(storage->units + offset, tail, tail_length * sizeof(*tail));
    storage->view = (mc_java_reader_line){.units = storage->units, .length = length};
    *out = &storage->view;
    return MC_JAVA_READER_OK;
}

mc_java_reader_status mc_java_reader_create(const mc_java_properties_allocator *allocator,
                                            const mc_java_reader_input *input,
                                            mc_java_reader_charset charset, mc_java_reader **out) {
    if (allocator == NULL || allocator->allocate == NULL || input == NULL ||
        input->bytes.read == NULL || out == NULL ||
        (unsigned int)charset > (unsigned int)MC_JAVA_READER_US_ASCII)
        return MC_JAVA_READER_INVALID_ARGUMENT;
    mc_java_reader_charset selected = charset;
    if (selected == MC_JAVA_READER_DEFAULT) {
        const mc_java_reader_status status = mc_java_reader_default_charset(&selected);
        if (status != MC_JAVA_READER_OK)
            return status;
    }
    if (selected < MC_JAVA_READER_UTF8 || selected > MC_JAVA_READER_US_ASCII)
        return MC_JAVA_READER_UNSUPPORTED_CHARSET;
    const mc_java_properties_allocator memory = *allocator;
    const mc_java_reader_input source = *input;
    mc_java_reader *reader = memory.allocate(memory.context, sizeof(*reader));
    if (reader == NULL)
        return MC_JAVA_READER_ALLOCATION_FAILED;
    *reader = (mc_java_reader){.allocator = memory, .input = source, .charset = selected};
    reader->bytes = memory.allocate(memory.context, READER_BUFFER_SIZE);
    if (reader->bytes == NULL)
        return MC_JAVA_READER_ALLOCATION_FAILED;
    reader->characters =
        memory.allocate(memory.context, READER_BUFFER_SIZE * sizeof(*reader->characters));
    if (reader->characters == NULL)
        return MC_JAVA_READER_ALLOCATION_FAILED;
    *out = reader;
    return MC_JAVA_READER_OK;
}

mc_java_reader_status mc_java_reader_read_line(mc_java_reader *reader,
                                               const mc_java_reader_line **out) {
    if (reader == NULL || out == NULL)
        return MC_JAVA_READER_INVALID_ARGUMENT;
    if (reader->closed)
        return MC_JAVA_READER_IO_ERROR;
    line_piece *first = NULL, *last = NULL;
    size_t prefix_length = 0;
    bool omit_lf = reader->skip_lf;
    for (;;) {
        if (reader->character_position >= reader->character_limit) {
            const mc_java_reader_status status = fill_characters(reader);
            if (status != MC_JAVA_READER_OK)
                return status;
        }
        if (reader->character_position >= reader->character_limit) {
            if (prefix_length != 0)
                return finish_line(reader, first, prefix_length, NULL, 0, out);
            *out = NULL;
            return MC_JAVA_READER_OK;
        }
        if (omit_lf && reader->characters[reader->character_position] == '\n')
            ++reader->character_position;
        reader->skip_lf = false;
        omit_lf = false;
        const size_t start = reader->character_position;
        size_t end = start;
        while (end < reader->character_limit && reader->characters[end] != '\n' &&
               reader->characters[end] != '\r')
            ++end;
        reader->character_position = end;
        if (end < reader->character_limit) {
            const mc_java_reader_line *line = NULL;
            const mc_java_reader_status status = finish_line(
                reader, first, prefix_length, reader->characters + start, end - start, &line);
            if (status != MC_JAVA_READER_OK)
                return status;
            ++reader->character_position;
            if (reader->characters[end] == '\r')
                reader->skip_lf = true;
            *out = line;
            return MC_JAVA_READER_OK;
        }
        const mc_java_reader_status status = append_piece(reader, &first, &last, &prefix_length,
                                                          reader->characters + start, end - start);
        if (status != MC_JAVA_READER_OK)
            return status;
    }
}

mc_java_reader_status mc_java_reader_close(mc_java_reader *reader) {
    if (reader == NULL)
        return MC_JAVA_READER_INVALID_ARGUMENT;
    if (reader->closed)
        return MC_JAVA_READER_OK;
    if (reader->input.close == NULL)
        return MC_JAVA_READER_UNIMPLEMENTED_CLOSE;
    const mc_java_reader_status status = reader->input.close(reader->input.bytes.context);
    reader->closed = true;
    reader->characters = NULL;
    return status;
}
