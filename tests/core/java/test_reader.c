#include "mc/core/java/reader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COUNT(array) (sizeof(array) / sizeof((array)[0]))
#define ARENA_BLOCKS 65536u

static unsigned int checks;
static bool cleanup_failed;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

/* Portable fundamental alignment, including CRTs without max_align_t. */
typedef union arena_alignment {
    long double floating;
    void *pointer;
    uint64_t integer;
} arena_alignment;

typedef struct arena {
    void **blocks;
    size_t count, requests, bytes, fail_at;
    bool requested_failure, invalid, malloc_failure;
} arena;

/* Real, distinct, uncleared allocations retained until every view retires. */
static void *arena_allocate(void *context, size_t size)
{
    arena *a = context;
    ++a->requests;
    if (a->fail_at != 0 && a->requests == a->fail_at) {
        a->requested_failure = true;
        return NULL;
    }
    if (a->blocks == NULL || a->count == ARENA_BLOCKS || size > SIZE_MAX - a->bytes) {
        a->invalid = true;
        return NULL;
    }
    void *block = malloc(size == 0 ? 1 : size);
    if (block == NULL) {
        a->malloc_failure = true;
        return NULL;
    }
    if ((uintptr_t)block % _Alignof(arena_alignment) != 0) {
        a->invalid = true;
        free(block);
        return NULL;
    }
    /* malloc's simultaneously live objects are disjoint. The ledger keeps
     * every block live, without quadratic scans on one-byte long-line fills. */
    a->blocks[a->count++] = block;
    a->bytes += size;
    return block;
}

static bool arena_healthy(const arena *a)
{
    return !a->invalid && !a->malloc_failure;
}

static void arena_release(arena *a)
{
    for (size_t i = 0; i < a->count; ++i)
        free(a->blocks[i]);
    free(a->blocks);
    *a = (arena){0};
}

typedef struct byte_stream {
    const uint8_t *bytes;
    size_t length, offset, chunk, reads, ready_calls, closes, fail_read;
    bool advertise, invalid;
    mc_java_reader_status close_status;
} byte_stream;

static mc_java_properties_status read_bytes(void *context, uint8_t *out, size_t capacity,
                                            size_t *count)
{
    byte_stream *s = context;
    ++s->reads;
    if (out == NULL || count == NULL || capacity == 0 || capacity > 8192 || s->offset > s->length) {
        s->invalid = true;
        return MC_JAVA_PROPERTIES_IO_ERROR;
    }
    *count = 0;
    if (s->fail_read != 0 && s->reads == s->fail_read)
        return MC_JAVA_PROPERTIES_IO_ERROR;
    size_t take = s->length - s->offset;
    if (take > s->chunk)
        take = s->chunk;
    if (take > capacity)
        take = capacity;
    if (take != 0)
        memcpy(out, s->bytes + s->offset, take);
    s->offset += take;
    *count = take;
    return MC_JAVA_PROPERTIES_OK;
}

static bool ready_bytes(void *context)
{
    byte_stream *s = context;
    ++s->ready_calls;
    return s->advertise && s->offset < s->length;
}

static mc_java_reader_status close_bytes(void *context)
{
    byte_stream *s = context;
    ++s->closes;
    return s->close_status;
}

typedef struct fixture {
    arena storage;
    byte_stream stream;
    mc_java_properties_allocator allocator;
    mc_java_reader_input input;
    mc_java_reader *reader;
} fixture;

static void fixture_prepare(fixture *f, const uint8_t *bytes, size_t length, size_t chunk,
                            bool advertise)
{
    *f = (fixture){0};
    /* Test bookkeeping has its own finite heap array, outside production's
     * allocator-request sequence. Its failure cannot pass as an injected one. */
    f->storage.blocks = malloc(ARENA_BLOCKS * sizeof(*f->storage.blocks));
    if (f->storage.blocks == NULL)
        f->storage.malloc_failure = true;
    f->stream = (byte_stream){.bytes = bytes,
                              .length = length,
                              .chunk = chunk,
                              .advertise = advertise,
                              .close_status = MC_JAVA_READER_OK};
    f->allocator = (mc_java_properties_allocator){&f->storage, arena_allocate};
    f->input = (mc_java_reader_input){{&f->stream, read_bytes}, ready_bytes, close_bytes};
}

static mc_java_reader_status fixture_create(fixture *f, mc_java_reader_charset charset)
{
    return mc_java_reader_create(&f->allocator, &f->input, charset, &f->reader);
}

static bool fixture_healthy(const fixture *f)
{
    return arena_healthy(&f->storage) && !f->stream.invalid;
}

static void fixture_release(fixture *f)
{
    if (f->reader != NULL) {
        mc_java_reader_status status = mc_java_reader_close(f->reader);
        if (status != MC_JAVA_READER_OK && status != MC_JAVA_READER_IO_ERROR &&
            !(status == MC_JAVA_READER_UNIMPLEMENTED_CLOSE && f->input.close == NULL)) {
            fprintf(stderr, "unexpected reader cleanup status %d\n", (int)status);
            cleanup_failed = true;
        }
        f->reader = NULL;
    }
    arena_release(&f->storage);
}

static bool units_are(const mc_java_reader_line *line, const uint16_t *units, size_t length)
{
    if (line == NULL || line->length != length || (length != 0 && line->units == NULL))
        return false;
    for (size_t i = 0; i < length; ++i)
        if (line->units[i] != units[i])
            return false;
    return true;
}

static bool text_is(const mc_java_reader_line *line, const char *text)
{
    size_t length = strlen(text);
    if (line == NULL || line->length != length || (length != 0 && line->units == NULL))
        return false;
    for (size_t i = 0; i < length; ++i)
        if (line->units[i] != (unsigned char)text[i])
            return false;
    return true;
}

static bool repeated(const mc_java_reader_line *line, uint16_t value, size_t length)
{
    if (line == NULL || line->length != length || (length != 0 && line->units == NULL))
        return false;
    for (size_t i = 0; i < length; ++i)
        if (line->units[i] != value)
            return false;
    return true;
}

static int test_invalid(void)
{
    int result = 1;
    static const uint8_t bytes[] = "canary\nnext\n";
    fixture f = {0};
    fixture_prepare(&f, bytes, sizeof(bytes) - 1, SIZE_MAX, false);
    CHECK(fixture_create(&f, MC_JAVA_READER_UTF8) == MC_JAVA_READER_OK);
    CHECK(f.stream.reads == 0 && f.stream.offset == 0 && f.stream.closes == 0);
    mc_java_reader *out = f.reader;
    size_t requests = f.storage.requests;
    const mc_java_properties_allocator bad_allocator = {&f.storage, NULL};
    mc_java_reader_input bad_input = f.input;
    bad_input.bytes.read = NULL;
    CHECK(mc_java_reader_create(NULL, &f.input, MC_JAVA_READER_UTF8, &out) ==
          MC_JAVA_READER_INVALID_ARGUMENT);
    CHECK(out == f.reader);
    CHECK(mc_java_reader_create(&bad_allocator, &f.input, MC_JAVA_READER_UTF8, &out) ==
          MC_JAVA_READER_INVALID_ARGUMENT);
    CHECK(out == f.reader);
    CHECK(mc_java_reader_create(&f.allocator, NULL, MC_JAVA_READER_UTF8, &out) ==
          MC_JAVA_READER_INVALID_ARGUMENT);
    CHECK(out == f.reader);
    CHECK(mc_java_reader_create(&f.allocator, &bad_input, MC_JAVA_READER_UTF8, &out) ==
          MC_JAVA_READER_INVALID_ARGUMENT);
    CHECK(out == f.reader);
    CHECK(mc_java_reader_create(&f.allocator, &f.input, MC_JAVA_READER_UTF8, NULL) ==
          MC_JAVA_READER_INVALID_ARGUMENT);
    CHECK(mc_java_reader_create(&f.allocator, &f.input, (mc_java_reader_charset)-1, &out) ==
          MC_JAVA_READER_INVALID_ARGUMENT);
    CHECK(out == f.reader);
    CHECK(mc_java_reader_create(&f.allocator, &f.input, (mc_java_reader_charset)99, &out) ==
          MC_JAVA_READER_INVALID_ARGUMENT);
    CHECK(out == f.reader);
    CHECK(f.storage.requests == requests && f.stream.reads == 0 && f.stream.closes == 0);
    CHECK(mc_java_reader_read_line(f.reader, NULL) == MC_JAVA_READER_INVALID_ARGUMENT);
    CHECK(f.stream.reads == 0 && f.storage.requests == requests);
    const mc_java_reader_line *line = NULL;
    CHECK(mc_java_reader_read_line(f.reader, &line) == MC_JAVA_READER_OK &&
          text_is(line, "canary"));
    const mc_java_reader_line *saved = line;
    CHECK(mc_java_reader_read_line(NULL, &line) == MC_JAVA_READER_INVALID_ARGUMENT &&
          line == saved);
    CHECK(mc_java_reader_close(NULL) == MC_JAVA_READER_INVALID_ARGUMENT);
    CHECK(text_is(saved, "canary") && fixture_healthy(&f));
    CHECK(mc_java_reader_default_charset(NULL) == MC_JAVA_READER_INVALID_ARGUMENT);
    mc_java_reader_charset charset = MC_JAVA_READER_DEFAULT;
    mc_java_reader_status status = mc_java_reader_default_charset(&charset);
    if (status == MC_JAVA_READER_OK)
        CHECK(charset == MC_JAVA_READER_UTF8 || charset == MC_JAVA_READER_WINDOWS1252 ||
              charset == MC_JAVA_READER_ISO8859_1 || charset == MC_JAVA_READER_US_ASCII);
    else
        CHECK(status == MC_JAVA_READER_UNSUPPORTED_CHARSET && charset == MC_JAVA_READER_DEFAULT);
    result = 0;
cleanup:
    fixture_release(&f);
    return result;
}

static int test_line_ends(size_t chunk, bool advertise)
{
    int result = 1;
    static const uint8_t bytes[] = "\r\n\nsame\rsame\n\rZ\r\n\n";
    static const char *const expected[] = {"", "", "same", "same", "", "Z", ""};
    fixture f = {0};
    const mc_java_reader_line *held[COUNT(expected)] = {0};
    const uint16_t *units[COUNT(expected)] = {0};
    fixture_prepare(&f, bytes, sizeof(bytes) - 1, chunk, advertise);
    CHECK(fixture_create(&f, MC_JAVA_READER_UTF8) == MC_JAVA_READER_OK);
    CHECK(f.stream.reads == 0 && f.stream.closes == 0);
    for (size_t i = 0; i < COUNT(expected); ++i) {
        CHECK(mc_java_reader_read_line(f.reader, &held[i]) == MC_JAVA_READER_OK);
        CHECK(text_is(held[i], expected[i]));
        units[i] = held[i]->units;
        for (size_t j = 0; j < i; ++j) {
            /* Skipping a separately buffered CRLF's LF creates an empty
             * StringBuilder. Its empty toString result is the same literal
             * at lines1 and6 for this one-byte, non-advertised stream. */
            const bool shared_empty = chunk == 1 && !advertise && i == 6 && j == 1;
            CHECK(shared_empty ? held[i] == held[j] : held[i] != held[j]);
        }
        for (size_t j = 0; j <= i; ++j)
            CHECK(text_is(held[j], expected[j]) && held[j]->units == units[j]);
    }
    const mc_java_reader_line *out = held[0];
    CHECK(mc_java_reader_read_line(f.reader, &out) == MC_JAVA_READER_OK && out == NULL);
    CHECK(f.stream.offset == sizeof(bytes) - 1 && f.stream.closes == 0);
    CHECK(mc_java_reader_close(f.reader) == MC_JAVA_READER_OK && f.stream.closes == 1);
    CHECK(mc_java_reader_close(f.reader) == MC_JAVA_READER_OK && f.stream.closes == 1);
    size_t reads = f.stream.reads, requests = f.storage.requests;
    out = held[2];
    CHECK(mc_java_reader_read_line(f.reader, &out) == MC_JAVA_READER_IO_ERROR && out == held[2]);
    CHECK(f.stream.reads == reads && f.storage.requests == requests);
    for (size_t i = 0; i < COUNT(expected); ++i)
        CHECK(text_is(held[i], expected[i]) && held[i]->units == units[i]);
    CHECK(fixture_healthy(&f));
    result = 0;
cleanup:
    fixture_release(&f);
    return result;
}

static int test_ready(size_t chunk, int ready_mode)
{
    int result = 1;
    static const uint8_t bytes[] = "one\ntwo\n";
    fixture f = {0};
    fixture_prepare(&f, bytes, sizeof(bytes) - 1, chunk, ready_mode == 1);
    if (ready_mode == 2)
        f.input.ready = NULL;
    CHECK(fixture_create(&f, MC_JAVA_READER_UTF8) == MC_JAVA_READER_OK);
    const mc_java_reader_line *first = NULL, *second = NULL;
    CHECK(mc_java_reader_read_line(f.reader, &first) == MC_JAVA_READER_OK && text_is(first, "one"));
    size_t first_consumed = ready_mode == 1 ? 8 : chunk == 1 ? 4 : chunk == 7 ? 7 : 8;
    size_t first_reads = ready_mode == 1 ? (chunk == 1   ? 8
                                            : chunk == 7 ? 2
                                                         : 1)
                         : chunk == 1    ? 4
                                         : 1;
    CHECK(f.stream.offset == first_consumed && f.stream.reads == first_reads);
    CHECK(f.stream.ready_calls == (ready_mode == 2 ? 0 : first_reads));
    CHECK(mc_java_reader_read_line(f.reader, &second) == MC_JAVA_READER_OK &&
          text_is(second, "two"));
    CHECK(first != second && text_is(first, "one"));
    size_t data_reads = chunk == 1 ? 8 : chunk == 7 ? 2 : 1;
    CHECK(f.stream.reads == data_reads && f.stream.offset == 8);
    CHECK(f.stream.ready_calls == (ready_mode == 2 ? 0 : data_reads));
    const mc_java_reader_line *out = first;
    CHECK(mc_java_reader_read_line(f.reader, &out) == MC_JAVA_READER_OK && out == NULL);
    CHECK(f.stream.reads == data_reads + 1 && f.stream.offset == 8 && f.stream.closes == 0);
    CHECK(mc_java_reader_read_line(f.reader, &out) == MC_JAVA_READER_OK && out == NULL);
    CHECK(f.stream.reads == data_reads + 2 && f.stream.offset == 8);
    CHECK(text_is(first, "one") && text_is(second, "two") && fixture_healthy(&f));
    result = 0;
cleanup:
    fixture_release(&f);
    return result;
}

typedef struct utf8_case {
    uint8_t bytes[12];
    size_t byte_count;
    uint16_t expected[10];
    size_t unit_count;
} utf8_case;

/* Literal finite decoder vectors from UTF_8.Decoder and CharsetDecoder's
 * replacement/consumption rules. No test decoder computes expected output. */
static const utf8_case utf8_cases[] = {
    {{0xc2, 0xa2, 0xe2, 0x82, 0xac, 0xf0, 0x9f, 0x98, 0x80},
     9,
     {0x00a2, 0x20ac, 0xd83d, 0xde00},
     4},
    {{0xef, 0xbb, 0xbf, 0, 0xef, 0xbf, 0xbf, 0xf4, 0x8f, 0xbf, 0xbf},
     11,
     {0xfeff, 0, 0xffff, 0xdbff, 0xdfff},
     5},
    {{0xc2, 0x80, 0xdf, 0xbf, 0xe0, 0xa0, 0x80}, 7, {0x0080, 0x07ff, 0x0800}, 3},
    {{0xc0, 0xaf}, 2, {0xfffd, 0xfffd}, 2},
    {{0xc1, 0xbf, 0x80, 0xff}, 4, {0xfffd, 0xfffd, 0xfffd, 0xfffd}, 4},
    {{0xc2, 'A'}, 2, {0xfffd, 'A'}, 2},
    {{0xe0, 0x80, 0x80}, 3, {0xfffd, 0xfffd, 0xfffd}, 3},
    {{0xe1, 0x80, 'A'}, 3, {0xfffd, 'A'}, 2},
    {{0xe1, 'A', 0x80}, 3, {0xfffd, 'A', 0xfffd}, 3},
    {{0xed, 0xa0, 0x80, 0xed, 0xbf, 0xbf}, 6, {0xfffd, 0xfffd}, 2},
    {{0xf0, 0x80, 0x80, 0x80}, 4, {0xfffd, 0xfffd, 0xfffd, 0xfffd}, 4},
    {{0xf4, 0x90, 0x80, 0x80}, 4, {0xfffd, 0xfffd, 0xfffd, 0xfffd}, 4},
    {{0xf5, 0x80, 0x80, 0x80}, 4, {0xfffd, 0xfffd, 0xfffd, 0xfffd}, 4},
    {{0xf0, 'A', 0x80, 0x80}, 4, {0xfffd, 'A', 0xfffd, 0xfffd}, 4},
    {{0xf0, 0x90, 'A', 0x80}, 4, {0xfffd, 'A', 0xfffd}, 3},
    {{0xf0, 0x90, 0x80, 'A'}, 4, {0xfffd, 'A'}, 2},
    {{0xc2}, 1, {0xfffd}, 1},
    {{0xe1, 0x80}, 2, {0xfffd}, 1},
    {{0xf0, 0x90, 0x80}, 3, {0xfffd}, 1},
    {{0xe0, 0x80}, 2, {0xfffd, 0xfffd}, 2},
    {{0xf4, 0x90}, 2, {0xfffd, 0xfffd}, 2},
    {{0xe1, 0x80, '\n', 'Z'}, 4, {0xfffd}, 1},
    {{0xf0, 0x90, 0x80, '\r', 'Z'}, 5, {0xfffd}, 1}};

static int test_vector(mc_java_reader_charset charset, const uint8_t *bytes, size_t length,
                       const uint16_t *expected, size_t units, size_t chunk, bool advertise,
                       bool second_z)
{
    int result = 1;
    fixture f = {0};
    fixture_prepare(&f, bytes, length, chunk, advertise);
    CHECK(fixture_create(&f, charset) == MC_JAVA_READER_OK);
    CHECK(f.stream.reads == 0 && f.stream.closes == 0);
    const mc_java_reader_line *line = NULL;
    CHECK(mc_java_reader_read_line(f.reader, &line) == MC_JAVA_READER_OK);
    CHECK(units_are(line, expected, units));
    const uint16_t *held_units = line->units;
    const mc_java_reader_line *out = line;
    if (second_z) {
        CHECK(mc_java_reader_read_line(f.reader, &out) == MC_JAVA_READER_OK && text_is(out, "Z"));
        CHECK(out != line);
    }
    CHECK(mc_java_reader_read_line(f.reader, &out) == MC_JAVA_READER_OK && out == NULL);
    CHECK(f.stream.offset == length && f.stream.closes == 0);
    CHECK(mc_java_reader_close(f.reader) == MC_JAVA_READER_OK && f.stream.closes == 1);
    CHECK(units_are(line, expected, units) && line->units == held_units && fixture_healthy(&f));
    result = 0;
cleanup:
    fixture_release(&f);
    return result;
}

static int test_single_byte(size_t chunk, bool advertise)
{
    int result = 1;
    static const uint8_t bytes[] = {0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88,
                                    0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f, 0x90, 0x91,
                                    0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a,
                                    0x9b, 0x9c, 0x9d, 0x9e, 0x9f, 0xa0, 0xe9, 0xff, 0};
    static const uint16_t windows[] = {
        0x20ac, 0xfffd, 0x201a, 0x0192, 0x201e, 0x2026, 0x2020, 0x2021, 0x02c6,
        0x2030, 0x0160, 0x2039, 0x0152, 0xfffd, 0x017d, 0xfffd, 0xfffd, 0x2018,
        0x2019, 0x201c, 0x201d, 0x2022, 0x2013, 0x2014, 0x02dc, 0x2122, 0x0161,
        0x203a, 0x0153, 0xfffd, 0x017e, 0x0178, 0x00a0, 0x00e9, 0x00ff, 0};
    static const uint16_t latin[] = {0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88,
                                     0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f, 0x90, 0x91,
                                     0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a,
                                     0x9b, 0x9c, 0x9d, 0x9e, 0x9f, 0xa0, 0xe9, 0xff, 0};
    static const uint16_t ascii[] = {0xfffd, 0xfffd, 0xfffd, 0xfffd, 0xfffd, 0xfffd, 0xfffd, 0xfffd,
                                     0xfffd, 0xfffd, 0xfffd, 0xfffd, 0xfffd, 0xfffd, 0xfffd, 0xfffd,
                                     0xfffd, 0xfffd, 0xfffd, 0xfffd, 0xfffd, 0xfffd, 0xfffd, 0xfffd,
                                     0xfffd, 0xfffd, 0xfffd, 0xfffd, 0xfffd, 0xfffd, 0xfffd, 0xfffd,
                                     0xfffd, 0xfffd, 0xfffd, 0};
    const bool lengths_match = COUNT(bytes) == COUNT(windows) && COUNT(bytes) == COUNT(latin) &&
                               COUNT(bytes) == COUNT(ascii);
    CHECK(lengths_match);
    CHECK(test_vector(MC_JAVA_READER_WINDOWS1252, bytes, COUNT(bytes), windows, COUNT(windows),
                      chunk, advertise, false) == 0);
    CHECK(test_vector(MC_JAVA_READER_ISO8859_1, bytes, COUNT(bytes), latin, COUNT(latin), chunk,
                      advertise, false) == 0);
    CHECK(test_vector(MC_JAVA_READER_US_ASCII, bytes, COUNT(bytes), ascii, COUNT(ascii), chunk,
                      advertise, false) == 0);
    result = 0;
cleanup:
    return result;
}

static int test_empty(size_t chunk, bool advertise)
{
    int result = 1;
    fixture f = {0};
    fixture_prepare(&f, NULL, 0, chunk, advertise);
    CHECK(fixture_create(&f, MC_JAVA_READER_UTF8) == MC_JAVA_READER_OK);
    const mc_java_reader_line *out = NULL;
    CHECK(mc_java_reader_read_line(f.reader, &out) == MC_JAVA_READER_OK && out == NULL);
    CHECK(f.stream.reads == 1 && f.stream.ready_calls == 0 && f.stream.closes == 0);
    CHECK(mc_java_reader_read_line(f.reader, &out) == MC_JAVA_READER_OK && out == NULL);
    CHECK(f.stream.reads == 2 && f.stream.offset == 0 && fixture_healthy(&f));
    result = 0;
cleanup:
    fixture_release(&f);
    return result;
}

static int test_large(size_t chunk, bool advertise, bool pair_in_line)
{
    int result = 1;
    fixture f = {0};
    uint8_t bytes[18000];
    size_t length = 0;
    for (size_t i = 0; i < 8191; ++i)
        bytes[length++] = 'a';
    if (!pair_in_line) {
        bytes[length++] = '\r';
        bytes[length++] = '\n';
    }
    bytes[length++] = 0xf0;
    bytes[length++] = 0x9f;
    bytes[length++] = 0x98;
    bytes[length++] = 0x80;
    bytes[length++] = '\n';
    for (size_t i = 0; i < 9000; ++i)
        bytes[length++] = 'b';
    fixture_prepare(&f, bytes, length, chunk, advertise);
    CHECK(fixture_create(&f, MC_JAVA_READER_UTF8) == MC_JAVA_READER_OK);
    const mc_java_reader_line *first = NULL, *pair = NULL, *last = NULL;
    CHECK(mc_java_reader_read_line(f.reader, &first) == MC_JAVA_READER_OK);
    if (pair_in_line) {
        CHECK(first != NULL && first->length == 8193 && first->units != NULL);
        for (size_t i = 0; i < 8191; ++i)
            CHECK(first->units[i] == 'a');
        CHECK(first->units[8191] == 0xd83d && first->units[8192] == 0xde00);
    } else {
        CHECK(repeated(first, 'a', 8191));
        CHECK(mc_java_reader_read_line(f.reader, &pair) == MC_JAVA_READER_OK);
        const uint16_t expected_pair[] = {0xd83d, 0xde00};
        CHECK(pair != first && units_are(pair, expected_pair, COUNT(expected_pair)));
    }
    const uint16_t *saved_units = first->units;
    CHECK(mc_java_reader_read_line(f.reader, &last) == MC_JAVA_READER_OK &&
          repeated(last, 'b', 9000));
    CHECK(last != first && last != pair);
    const mc_java_reader_line *out = first;
    CHECK(mc_java_reader_read_line(f.reader, &out) == MC_JAVA_READER_OK && out == NULL);
    CHECK(f.stream.offset == length && f.stream.closes == 0 && first->units == saved_units);
    CHECK(mc_java_reader_close(f.reader) == MC_JAVA_READER_OK && f.stream.closes == 1);
    CHECK(first->units == saved_units && repeated(last, 'b', 9000) && fixture_healthy(&f));
    if (pair_in_line) {
        for (size_t i = 0; i < 8191; ++i)
            CHECK(first->units[i] == 'a');
        CHECK(first->units[8191] == 0xd83d && first->units[8192] == 0xde00);
    } else {
        const uint16_t expected_pair[] = {0xd83d, 0xde00};
        CHECK(repeated(first, 'a', 8191) && units_are(pair, expected_pair, COUNT(expected_pair)));
    }
    result = 0;
cleanup:
    fixture_release(&f);
    return result;
}

static int test_close(void)
{
    int result = 1;
    static const uint8_t bytes[] = "a\nb\n";
    fixture normal = {0}, failed = {0}, missing = {0};
    fixture_prepare(&normal, bytes, sizeof(bytes) - 1, 1, false);
    fixture_prepare(&failed, bytes, sizeof(bytes) - 1, 1, false);
    fixture_prepare(&missing, bytes, sizeof(bytes) - 1, 1, false);
    failed.stream.close_status = MC_JAVA_READER_IO_ERROR;
    missing.input.close = NULL;
    CHECK(fixture_create(&normal, MC_JAVA_READER_UTF8) == MC_JAVA_READER_OK);
    CHECK(fixture_create(&failed, MC_JAVA_READER_UTF8) == MC_JAVA_READER_OK);
    CHECK(fixture_create(&missing, MC_JAVA_READER_UTF8) == MC_JAVA_READER_OK);
    CHECK(mc_java_reader_close(normal.reader) == MC_JAVA_READER_OK);
    CHECK(normal.stream.closes == 1 && normal.stream.reads == 0);
    CHECK(mc_java_reader_close(normal.reader) == MC_JAVA_READER_OK && normal.stream.closes == 1);
    const mc_java_reader_line *line = NULL;
    CHECK(mc_java_reader_read_line(failed.reader, &line) == MC_JAVA_READER_OK &&
          text_is(line, "a"));
    const mc_java_reader_line *held = line;
    CHECK(mc_java_reader_close(failed.reader) == MC_JAVA_READER_IO_ERROR &&
          failed.stream.closes == 1);
    size_t reads = failed.stream.reads;
    CHECK(mc_java_reader_close(failed.reader) == MC_JAVA_READER_OK && failed.stream.closes == 1);
    CHECK(mc_java_reader_read_line(failed.reader, &line) == MC_JAVA_READER_IO_ERROR &&
          line == held);
    CHECK(failed.stream.reads == reads && text_is(held, "a"));
    line = held;
    CHECK(mc_java_reader_read_line(normal.reader, &line) == MC_JAVA_READER_IO_ERROR &&
          line == held);
    CHECK(normal.stream.reads == 0);
    size_t requests = missing.storage.requests;
    CHECK(mc_java_reader_close(missing.reader) == MC_JAVA_READER_UNIMPLEMENTED_CLOSE);
    CHECK(missing.storage.requests == requests && missing.stream.reads == 0 &&
          missing.stream.closes == 0);
    CHECK(mc_java_reader_read_line(missing.reader, &line) == MC_JAVA_READER_OK &&
          text_is(line, "a"));
    const mc_java_reader_line *before = line;
    CHECK(mc_java_reader_close(missing.reader) == MC_JAVA_READER_UNIMPLEMENTED_CLOSE);
    CHECK(mc_java_reader_read_line(missing.reader, &line) == MC_JAVA_READER_OK &&
          text_is(line, "b"));
    CHECK(before != line && text_is(before, "a") && missing.stream.closes == 0);
    CHECK(fixture_healthy(&normal) && fixture_healthy(&failed) && fixture_healthy(&missing));
    result = 0;
cleanup:
    fixture_release(&normal);
    fixture_release(&failed);
    fixture_release(&missing);
    return result;
}

static int test_io(bool advertise, bool incomplete_utf8)
{
    int result = 1;
    static const uint8_t text[] = "good\npartial";
    static const uint8_t incomplete[] = {'o', 'k', '\n', 0xe1, 0x80};
    static const uint8_t canary_bytes[] = "canary\n";
    fixture f = {0}, baseline = {0};
    fixture_prepare(&baseline, canary_bytes, sizeof(canary_bytes) - 1, SIZE_MAX, false);
    fixture_prepare(&f, incomplete_utf8 ? incomplete : text,
                    incomplete_utf8 ? sizeof(incomplete) : sizeof(text) - 1,
                    incomplete_utf8 ? 3 : 5, advertise);
    f.stream.fail_read = 3;
    CHECK(fixture_create(&baseline, MC_JAVA_READER_UTF8) == MC_JAVA_READER_OK);
    CHECK(fixture_create(&f, MC_JAVA_READER_UTF8) == MC_JAVA_READER_OK);
    const mc_java_reader_line *sentinel = NULL;
    CHECK(mc_java_reader_read_line(baseline.reader, &sentinel) == MC_JAVA_READER_OK &&
          text_is(sentinel, "canary"));
    const mc_java_reader_line *out = sentinel;
    const mc_java_reader_line *previous = NULL;
    /* With an incomplete suffix, ready becomes false after read2 and the
     * already decoded first line returns. Plain text still advertises bytes
     * then, so read3 throws before that first fill can publish any chars. */
    if (!advertise || incomplete_utf8) {
        CHECK(mc_java_reader_read_line(f.reader, &out) == MC_JAVA_READER_OK);
        CHECK(text_is(out, incomplete_utf8 ? "ok" : "good"));
        previous = out;
    }
    const mc_java_reader_line *before_failure = out;
    CHECK(mc_java_reader_read_line(f.reader, &out) == MC_JAVA_READER_IO_ERROR);
    CHECK(out == before_failure && f.stream.reads == 3);
    CHECK(f.stream.offset == (incomplete_utf8 ? 5 : 10) && f.stream.closes == 0);
    if (previous != NULL)
        CHECK(text_is(previous, incomplete_utf8 ? "ok" : "good"));
    CHECK(text_is(sentinel, "canary"));
    CHECK(mc_java_reader_close(f.reader) == MC_JAVA_READER_OK && f.stream.closes == 1);
    CHECK(out == before_failure && fixture_healthy(&f) && fixture_healthy(&baseline));
    result = 0;
cleanup:
    fixture_release(&f);
    fixture_release(&baseline);
    return result;
}

static bool workload_line(const mc_java_reader_line *line, size_t index)
{
    return index == 0   ? text_is(line, "first")
           : index == 1 ? text_is(line, "second")
           : index == 2 ? repeated(line, 'q', 9000)
                        : index == 3 && text_is(line, "tail");
}

static int test_allocation_failures(void)
{
    int result = 1;
    uint8_t bytes[9032];
    size_t length = 0;
    const char *prefix = "first\nsecond\n";
    memcpy(bytes, prefix, strlen(prefix));
    length += strlen(prefix);
    for (size_t i = 0; i < 9000; ++i)
        bytes[length++] = 'q';
    memcpy(bytes + length, "\ntail\n", 6);
    length += 6;
    static const uint8_t canary_bytes[] = "canary\n";
    fixture baseline = {0}, f = {0};
    fixture_prepare(&baseline, canary_bytes, sizeof(canary_bytes) - 1, SIZE_MAX, false);
    CHECK(fixture_create(&baseline, MC_JAVA_READER_UTF8) == MC_JAVA_READER_OK);
    const mc_java_reader_line *canary = NULL;
    CHECK(mc_java_reader_read_line(baseline.reader, &canary) == MC_JAVA_READER_OK &&
          text_is(canary, "canary"));
    fixture_prepare(&f, bytes, length, SIZE_MAX, false);
    CHECK(fixture_create(&f, MC_JAVA_READER_UTF8) == MC_JAVA_READER_OK);
    size_t create_requests = f.storage.requests;
    const mc_java_reader_line *held[4] = {0};
    for (size_t i = 0; i < COUNT(held); ++i)
        CHECK(mc_java_reader_read_line(f.reader, &held[i]) == MC_JAVA_READER_OK &&
              workload_line(held[i], i));
    const mc_java_reader_line *out = canary;
    CHECK(mc_java_reader_read_line(f.reader, &out) == MC_JAVA_READER_OK && out == NULL);
    size_t requests = f.storage.requests;
    CHECK(create_requests != 0 && requests > create_requests && requests < ARENA_BLOCKS);
    CHECK(fixture_healthy(&f));
    fixture_release(&f);
    for (size_t fail_at = 1; fail_at <= requests; ++fail_at) {
        fixture_prepare(&f, bytes, length, SIZE_MAX, false);
        f.storage.fail_at = fail_at;
        mc_java_reader *created = baseline.reader;
        mc_java_reader_status status =
            mc_java_reader_create(&f.allocator, &f.input, MC_JAVA_READER_UTF8, &created);
        size_t published = 0;
        const mc_java_reader_line *saved[4] = {0};
        if (status != MC_JAVA_READER_OK) {
            CHECK(status == MC_JAVA_READER_ALLOCATION_FAILED && created == baseline.reader);
            CHECK(fail_at <= create_requests && f.stream.reads == 0 && f.stream.closes == 0);
        } else {
            CHECK(created != baseline.reader && fail_at > create_requests);
            f.reader = created;
            status = MC_JAVA_READER_OK;
            out = canary;
            for (size_t i = 0; i <= COUNT(saved); ++i) {
                const mc_java_reader_line *before = out;
                status = mc_java_reader_read_line(f.reader, &out);
                if (status != MC_JAVA_READER_OK) {
                    CHECK(status == MC_JAVA_READER_ALLOCATION_FAILED && out == before);
                    break;
                }
                CHECK(i < COUNT(saved) && out != NULL && workload_line(out, i));
                saved[published++] = out;
                for (size_t j = 0; j < published; ++j)
                    CHECK(workload_line(saved[j], j));
            }
            CHECK(status == MC_JAVA_READER_ALLOCATION_FAILED);
            CHECK(mc_java_reader_close(f.reader) == MC_JAVA_READER_OK && f.stream.closes == 1);
            for (size_t j = 0; j < published; ++j)
                CHECK(workload_line(saved[j], j));
        }
        CHECK(f.storage.requested_failure && f.storage.requests >= fail_at && fixture_healthy(&f));
        CHECK(text_is(canary, "canary"));
        fixture_release(&f);
    }
    result = 0;
cleanup:
    fixture_release(&f);
    fixture_release(&baseline);
    return result;
}

int main(void)
{
    int result = 1;
    CHECK(test_invalid() == 0);
    const size_t chunks[] = {SIZE_MAX, 1, 7, 8192};
    for (size_t i = 0; i < COUNT(chunks); ++i) {
        for (int mode = 0; mode < 3; ++mode)
            CHECK(test_ready(chunks[i], mode) == 0);
        for (int ready = 0; ready < 2; ++ready) {
            CHECK(test_empty(chunks[i], ready != 0) == 0);
            CHECK(test_line_ends(chunks[i], ready != 0) == 0);
            CHECK(test_single_byte(chunks[i], ready != 0) == 0);
            CHECK(test_large(chunks[i], ready != 0, false) == 0);
            CHECK(test_large(chunks[i], ready != 0, true) == 0);
            for (size_t c = 0; c < COUNT(utf8_cases); ++c) {
                const utf8_case *v = &utf8_cases[c];
                CHECK(test_vector(MC_JAVA_READER_UTF8, v->bytes, v->byte_count, v->expected,
                                  v->unit_count, chunks[i], ready != 0,
                                  c >= COUNT(utf8_cases) - 2) == 0);
            }
        }
    }
    CHECK(test_close() == 0);
    CHECK(test_io(false, false) == 0 && test_io(true, false) == 0);
    CHECK(test_io(false, true) == 0 && test_io(true, true) == 0);
    CHECK(test_allocation_failures() == 0 && !cleanup_failed);
    result = 0;
cleanup:
    if (result == 0 && printf("Reader native contract: %u checks passed\n", checks) < 0)
        result = 1;
    return result;
}
