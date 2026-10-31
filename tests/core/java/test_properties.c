#include "mc/core/java/properties.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COUNT(array) (sizeof(array) / sizeof((array)[0]))
#define ARENA_BLOCKS 4096u

static unsigned int checks;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

typedef struct arena {
    void *blocks[ARENA_BLOCKS];
    size_t sizes[ARENA_BLOCKS];
    size_t count, requests, bytes, fail_at;
    bool requested_failure, invalid_request, malloc_failure;
} arena;

/* Some Windows CRT headers omit max_align_t. These fundamental members cover
 * the pointer, size and UTF16 storage types admitted by this test arena. */
typedef union arena_alignment {
    long double floating;
    void *pointer;
    uint64_t integer;
} arena_alignment;

/* Real arena storage: malloc supplies distinct live, max-aligned blocks. Do
 * not clear returned memory; production must initialize all of its own state. */
static void *arena_allocate(void *context, size_t size) {
    arena *a = context;
    ++a->requests;
    if (a->fail_at != 0 && a->requests == a->fail_at) {
        a->requested_failure = true;
        return NULL;
    }
    if (a->count == ARENA_BLOCKS || size > SIZE_MAX - a->bytes) {
        a->invalid_request = true;
        return NULL;
    }
    void *block = malloc(size == 0 ? 1 : size);
    if (block == NULL) {
        a->malloc_failure = true;
        return NULL;
    }
    if ((uintptr_t)block % _Alignof(arena_alignment) != 0) {
        free(block);
        a->invalid_request = true;
        return NULL;
    }
    for (size_t i = 0; i < a->count; ++i)
        if (a->blocks[i] == block) {
            a->invalid_request = true;
            return NULL;
        }
    a->blocks[a->count] = block;
    a->sizes[a->count++] = size;
    a->bytes += size;
    return block;
}

static void arena_release(arena *a) {
    for (size_t i = 0; i < a->count; ++i)
        free(a->blocks[i]);
    *a = (arena){0};
}

static bool arena_healthy(const arena *a) {
    return !a->invalid_request && !a->malloc_failure;
}

typedef struct byte_stream {
    const uint8_t *bytes;
    size_t length, offset, chunk, calls, fail_call, largest_request;
    bool invalid_request;
} byte_stream;

/* Finite actual byte-stream adapter. A selected read fails before returning
 * any bytes; successful reads may be short, including a zero-byte EOF. */
static mc_java_properties_status stream_read(void *context, uint8_t *destination, size_t capacity,
                                             size_t *count) {
    byte_stream *s = context;
    ++s->calls;
    if (capacity == 0 || capacity > 8192 || destination == NULL || count == NULL) {
        s->invalid_request = true;
        return MC_JAVA_PROPERTIES_IO_ERROR;
    }
    if (capacity > s->largest_request)
        s->largest_request = capacity;
    *count = 0;
    if (s->fail_call != 0 && s->calls == s->fail_call)
        return MC_JAVA_PROPERTIES_IO_ERROR;
    size_t take = s->length - s->offset;
    if (take > s->chunk)
        take = s->chunk;
    if (take > capacity)
        take = capacity;
    if (take != 0)
        memcpy(destination, s->bytes + s->offset, take);
    s->offset += take;
    *count = take;
    return MC_JAVA_PROPERTIES_OK;
}

static mc_java_properties_status load_bytes(mc_java_properties *properties, const uint8_t *bytes,
                                            size_t length, size_t chunk, size_t fail_call,
                                            byte_stream *observed) {
    byte_stream stream = {bytes, length, 0, chunk, 0, fail_call, 0, false};
    const mc_java_properties_input input = {&stream, stream_read};
    mc_java_properties_status status = mc_java_properties_load(properties, &input);
    if (observed != NULL)
        *observed = stream;
    return status;
}

static mc_java_properties_status load_text(mc_java_properties *properties, const char *text,
                                           size_t chunk, size_t fail_call, byte_stream *observed) {
    return load_bytes(properties, (const uint8_t *)text, strlen(text), chunk, fail_call, observed);
}

static mc_java_properties_status lookup(const mc_java_properties *properties, const char *key,
                                        const mc_java_properties_string **out) {
    uint16_t units[128];
    size_t length = strlen(key);
    if (length > COUNT(units))
        return MC_JAVA_PROPERTIES_INVALID_ARGUMENT;
    for (size_t i = 0; i < length; ++i)
        units[i] = (uint16_t)(unsigned char)key[i];
    return mc_java_properties_get(properties, units, length, out);
}

static bool units_are(const mc_java_properties_string *string, const uint16_t *units,
                      size_t length) {
    mc_java_properties_string_view view;
    if (!mc_java_properties_string_read(string, &view) || view.length != length ||
        (length != 0 && view.units == NULL))
        return false;
    for (size_t i = 0; i < length; ++i)
        if (view.units[i] != units[i])
            return false;
    return true;
}

static bool text_is(const mc_java_properties_string *string, const char *text) {
    mc_java_properties_string_view view;
    size_t length = strlen(text);
    if (!mc_java_properties_string_read(string, &view) || view.length != length ||
        (length != 0 && view.units == NULL))
        return false;
    for (size_t i = 0; i < length; ++i)
        if (view.units[i] != (uint16_t)(unsigned char)text[i])
            return false;
    return true;
}

static bool property_is(const mc_java_properties *properties, const char *key, const char *value) {
    const mc_java_properties_string *actual = NULL;
    return lookup(properties, key, &actual) == MC_JAVA_PROPERTIES_OK && text_is(actual, value);
}

static bool missing(const mc_java_properties *properties, const char *key) {
    const mc_java_properties_string *actual = NULL;
    return lookup(properties, key, &actual) == MC_JAVA_PROPERTIES_OK && actual == NULL;
}

static int arguments(void) {
    int result = 1;
    arena a = {0};
    const mc_java_properties_allocator allocator = {&a, arena_allocate};
    const mc_java_properties_allocator bad_allocator = {&a, NULL};
    mc_java_properties *properties = NULL;
    CHECK(mc_java_properties_create(&allocator, &properties) == MC_JAVA_PROPERTIES_OK);
    mc_java_properties *owner_out = properties;
    size_t requests = a.requests;
    CHECK(mc_java_properties_create(NULL, &owner_out) == MC_JAVA_PROPERTIES_INVALID_ARGUMENT);
    CHECK(owner_out == properties);
    CHECK(mc_java_properties_create(&bad_allocator, &owner_out) ==
          MC_JAVA_PROPERTIES_INVALID_ARGUMENT);
    CHECK(owner_out == properties);
    CHECK(mc_java_properties_create(&allocator, NULL) == MC_JAVA_PROPERTIES_INVALID_ARGUMENT);
    CHECK(a.requests == requests && mc_java_properties_size(properties) == 0);
    byte_stream stream = {(const uint8_t *)"k=v\n", 4, 0, 1, 0, 0, 0, false};
    const mc_java_properties_input input = {&stream, stream_read}, bad_input = {&stream, NULL};
    CHECK(mc_java_properties_load(NULL, &input) == MC_JAVA_PROPERTIES_INVALID_ARGUMENT);
    CHECK(mc_java_properties_load(properties, NULL) == MC_JAVA_PROPERTIES_INVALID_ARGUMENT);
    CHECK(mc_java_properties_load(properties, &bad_input) == MC_JAVA_PROPERTIES_INVALID_ARGUMENT);
    CHECK(stream.calls == 0 && a.requests == requests);
    CHECK(load_text(properties, "k=v\n", 1, 0, NULL) == MC_JAVA_PROPERTIES_OK);
    const mc_java_properties_string *key = NULL, *value = NULL;
    CHECK(mc_java_properties_entry(properties, 0, &key, &value) == MC_JAVA_PROPERTIES_OK);
    CHECK(text_is(key, "k") && text_is(value, "v"));
    const mc_java_properties_string *out = value, *key_out = key, *value_out = value;
    const uint16_t k[] = {'k'};
    CHECK(mc_java_properties_get(NULL, k, COUNT(k), &out) == MC_JAVA_PROPERTIES_INVALID_ARGUMENT);
    CHECK(out == value);
    CHECK(mc_java_properties_get(properties, NULL, 1, &out) == MC_JAVA_PROPERTIES_INVALID_ARGUMENT);
    CHECK(out == value);
    CHECK(mc_java_properties_get(properties, k, COUNT(k), NULL) ==
          MC_JAVA_PROPERTIES_INVALID_ARGUMENT);
    CHECK(mc_java_properties_entry(NULL, 0, &key_out, &value_out) ==
          MC_JAVA_PROPERTIES_INVALID_ARGUMENT);
    CHECK(key_out == key && value_out == value);
    CHECK(mc_java_properties_entry(properties, 0, NULL, &value_out) ==
          MC_JAVA_PROPERTIES_INVALID_ARGUMENT);
    CHECK(value_out == value);
    CHECK(mc_java_properties_entry(properties, 0, &key_out, NULL) ==
          MC_JAVA_PROPERTIES_INVALID_ARGUMENT);
    CHECK(key_out == key);
    CHECK(mc_java_properties_entry(properties, 0, &out, &out) ==
          MC_JAVA_PROPERTIES_INVALID_ARGUMENT);
    CHECK(out == value);
    CHECK(mc_java_properties_entry(properties, 1, &key_out, &value_out) ==
          MC_JAVA_PROPERTIES_INDEX_OUT_OF_RANGE);
    CHECK(key_out == key && value_out == value);
    CHECK(mc_java_properties_entry(properties, SIZE_MAX, &key_out, &value_out) ==
          MC_JAVA_PROPERTIES_INDEX_OUT_OF_RANGE);
    CHECK(key_out == key && value_out == value);
    mc_java_properties_string_view view = {k, 123};
    CHECK(!mc_java_properties_string_read(NULL, &view));
    CHECK(view.units == k && view.length == 123);
    CHECK(!mc_java_properties_string_read(value, NULL));
    CHECK(lookup(properties, "absent", &out) == MC_JAVA_PROPERTIES_OK && out == NULL);
    CHECK(mc_java_properties_get(properties, NULL, 0, &out) == MC_JAVA_PROPERTIES_OK &&
          out == NULL);
    CHECK(arena_healthy(&a));
    result = 0;
cleanup:
    arena_release(&a);
    return result;
}

static int syntax_chunk(size_t chunk) {
    int result = 1;
    arena a = {0};
    mc_java_properties *properties = NULL;
    const mc_java_properties_allocator allocator = {&a, arena_allocate};
    static const char corpus[] = " \t\f# comment\\\nvisible=yes\n! ignored\r\r\n"
                                 "alpha = one\rbeta:two\ngamma \t : three\r\nlonely\n =empty-key\n"
                                 "a\\ b\\:\\==keep  trailing  \n"
                                 "esc=\\t\\r\\n\\f\\z\\b\\'\\\"\\\\\n"
                                 "joined=one\\\n \t\f two\\\r\n\tthree\n"
                                 "hash=before\\\n #after\nbang=go\\\r !after\n"
                                 "blank=left\\\n\nnext=right\neven=two\\\\\ntail=value\\";
    CHECK(mc_java_properties_create(&allocator, &properties) == MC_JAVA_PROPERTIES_OK);
    byte_stream observed;
    CHECK(load_text(properties, corpus, chunk, 0, &observed) == MC_JAVA_PROPERTIES_OK);
    CHECK(observed.offset == sizeof(corpus) - 1 && !observed.invalid_request);
    CHECK(observed.largest_request <= 8192 && mc_java_properties_size(properties) == 15);
    CHECK(property_is(properties, "visible", "yes"));
    CHECK(property_is(properties, "alpha", "one"));
    CHECK(property_is(properties, "beta", "two"));
    CHECK(property_is(properties, "gamma", "three"));
    CHECK(property_is(properties, "lonely", ""));
    CHECK(property_is(properties, "", "empty-key"));
    const mc_java_properties_string *empty_key_value = NULL;
    CHECK(mc_java_properties_get(properties, NULL, 0, &empty_key_value) == MC_JAVA_PROPERTIES_OK);
    CHECK(text_is(empty_key_value, "empty-key"));
    CHECK(property_is(properties, "a b:=", "keep  trailing  "));
    CHECK(property_is(properties, "esc", "\t\r\n\fzb'\"\\"));
    CHECK(property_is(properties, "joined", "onetwothree"));
    CHECK(property_is(properties, "hash", "before#after"));
    CHECK(property_is(properties, "bang", "go!after"));
    CHECK(property_is(properties, "blank", "left"));
    CHECK(property_is(properties, "next", "right"));
    CHECK(property_is(properties, "even", "two\\"));
    CHECK(property_is(properties, "tail", "value"));
    CHECK(arena_healthy(&a));
    result = 0;
cleanup:
    arena_release(&a);
    return result;
}

static int unicode_chunk(size_t chunk) {
    int result = 1;
    arena a = {0};
    mc_java_properties *properties = NULL;
    const mc_java_properties_allocator allocator = {&a, arena_allocate};
    const uint8_t latin1[] = {'l', 0xe9, '=', 0xff, 0x80, 0, '\n', 'v', 0x0b, '=', 'o', 'k', '\n'};
    const uint16_t latin_key[] = {'l', 0xe9}, latin_value[] = {0xff, 0x80, 0};
    const uint16_t vertical_key[] = {'v', 0x0b};
    const uint16_t special_key[] = {0, 0xd800}, special_value[] = {0xdc00, 0xe9, 0xffff};
    const uint16_t pair[] = {0xd83d, 0xde00};
    CHECK(mc_java_properties_create(&allocator, &properties) == MC_JAVA_PROPERTIES_OK);
    CHECK(load_bytes(properties, latin1, sizeof(latin1), chunk, 0, NULL) == MC_JAVA_PROPERTIES_OK);
    CHECK(load_text(properties,
                    "\\u0000\\uD800=\\uDC00\\u00e9\\uFFFF\npair=\\uD83D\\uDE00\n"
                    "\\u0041=encoded\nA=final\n",
                    chunk, 0, NULL) == MC_JAVA_PROPERTIES_OK);
    CHECK(mc_java_properties_size(properties) == 5);
    const mc_java_properties_string *value = NULL;
    CHECK(mc_java_properties_get(properties, latin_key, COUNT(latin_key), &value) ==
          MC_JAVA_PROPERTIES_OK);
    CHECK(units_are(value, latin_value, COUNT(latin_value)));
    CHECK(mc_java_properties_get(properties, vertical_key, COUNT(vertical_key), &value) ==
          MC_JAVA_PROPERTIES_OK);
    CHECK(text_is(value, "ok"));
    CHECK(mc_java_properties_get(properties, special_key, COUNT(special_key), &value) ==
          MC_JAVA_PROPERTIES_OK);
    CHECK(units_are(value, special_value, COUNT(special_value)));
    CHECK(mc_java_properties_get(properties, special_key, 1, &value) == MC_JAVA_PROPERTIES_OK &&
          value == NULL);
    CHECK(lookup(properties, "pair", &value) == MC_JAVA_PROPERTIES_OK &&
          units_are(value, pair, COUNT(pair)));
    CHECK(property_is(properties, "A", "final"));
    CHECK(arena_healthy(&a));
    result = 0;
cleanup:
    arena_release(&a);
    return result;
}

static int eof_chunk(size_t chunk) {
    int result = 1;
    arena a = {0};
    mc_java_properties *properties = NULL;
    const mc_java_properties_allocator allocator = {&a, arena_allocate};

    static const struct {
        const char *input, *key, *value;
    } cases[] = {{"", NULL, NULL},
                 {" \t\f\r\n\r\n", NULL, NULL},
                 {"#ignored\\", NULL, NULL},
                 {"!ignored\r\n#last", NULL, NULL},
                 {"\\", "", ""},
                 {"\\\n", "", ""},
                 {"\\\n ", NULL, NULL},
                 {"only\\", "only", ""},
                 {"end=x\\\\", "end", "x\\"},
                 {"key=value\\\n", "key", "value"}};

    for (size_t i = 0; i < COUNT(cases); ++i) {
        CHECK(mc_java_properties_create(&allocator, &properties) == MC_JAVA_PROPERTIES_OK);
        CHECK(load_text(properties, cases[i].input, chunk, 0, NULL) == MC_JAVA_PROPERTIES_OK);
        CHECK(mc_java_properties_size(properties) == (cases[i].key == NULL ? 0u : 1u));
        if (cases[i].key != NULL)
            CHECK(property_is(properties, cases[i].key, cases[i].value));
        CHECK(arena_healthy(&a));
        properties = NULL;
        arena_release(&a);
    }
    result = 0;
cleanup:
    arena_release(&a);
    return result;
}

static int retained_identities(void) {
    int result = 1;
    arena a = {0};
    mc_java_properties *properties = NULL;
    const mc_java_properties_allocator allocator = {&a, arena_allocate};
    CHECK(mc_java_properties_create(&allocator, &properties) == MC_JAVA_PROPERTIES_OK);
    CHECK(load_text(properties, "key=same\nempty=\n", 7, 0, NULL) == MC_JAVA_PROPERTIES_OK);
    const mc_java_properties_string *first_key = NULL, *old_value = NULL;
    const mc_java_properties_string *empty_key = NULL, *old_empty = NULL;
    CHECK(mc_java_properties_entry(properties, 0, &first_key, &old_value) == MC_JAVA_PROPERTIES_OK);
    CHECK(mc_java_properties_entry(properties, 1, &empty_key, &old_empty) == MC_JAVA_PROPERTIES_OK);
    mc_java_properties_string_view old_view, empty_view;
    CHECK(mc_java_properties_string_read(old_value, &old_view));
    CHECK(mc_java_properties_string_read(old_empty, &empty_view));
    mc_java_properties *same_owner = properties;
    CHECK(load_text(properties, "k\\u0065y=same\nempty=\nnew=value\n", 1, 0, NULL) ==
          MC_JAVA_PROPERTIES_OK);
    CHECK(properties == same_owner && mc_java_properties_size(properties) == 3);
    const mc_java_properties_string *key = NULL, *value = NULL;
    CHECK(mc_java_properties_entry(properties, 0, &key, &value) == MC_JAVA_PROPERTIES_OK);
    CHECK(key == first_key && value != old_value && text_is(value, "same"));
    CHECK(mc_java_properties_entry(properties, 1, &key, &value) == MC_JAVA_PROPERTIES_OK);
    CHECK(key == empty_key && value != old_empty && text_is(value, ""));
    CHECK(mc_java_properties_entry(properties, 2, &key, &value) == MC_JAVA_PROPERTIES_OK);
    CHECK(text_is(key, "new") && text_is(value, "value"));
    mc_java_properties_string_view view;
    CHECK(mc_java_properties_string_read(old_value, &view));
    CHECK(view.units == old_view.units && view.length == old_view.length &&
          text_is(old_value, "same"));
    CHECK(mc_java_properties_string_read(old_empty, &view));
    CHECK(view.units == empty_view.units && view.length == empty_view.length &&
          text_is(old_empty, ""));
    CHECK(text_is(first_key, "key") && text_is(empty_key, "empty") && arena_healthy(&a));
    result = 0;
cleanup:
    arena_release(&a);
    return result;
}

static int malformed_chunk(size_t chunk) {
    int result = 1;
    arena a = {0};
    mc_java_properties *properties = NULL;
    const mc_java_properties_allocator allocator = {&a, arena_allocate};
    static const char *const cases[] = {"good=first\nbad=\\u12G4\nlate=no\n",
                                        "good=first\nbad=\\u123\nlate=no\n",
                                        "good=first\n\\u12G4=bad\nlate=no\n",
                                        "good=first\nbad=\\uu1234\nlate=no\n",
                                        "good=first\nbad=\\u\n",
                                        "good=first\nbad=\\u12"};
    for (size_t i = 0; i < COUNT(cases); ++i) {
        CHECK(mc_java_properties_create(&allocator, &properties) == MC_JAVA_PROPERTIES_OK);
        CHECK(load_text(properties, "anchor=held\n", chunk, 0, NULL) == MC_JAVA_PROPERTIES_OK);
        const mc_java_properties_string *anchor = NULL;
        CHECK(lookup(properties, "anchor", &anchor) == MC_JAVA_PROPERTIES_OK);
        CHECK(load_text(properties, cases[i], chunk, 0, NULL) ==
              MC_JAVA_PROPERTIES_MALFORMED_UNICODE);
        CHECK(mc_java_properties_size(properties) == 2 && text_is(anchor, "held"));
        CHECK(property_is(properties, "anchor", "held") &&
              property_is(properties, "good", "first"));
        CHECK(missing(properties, "bad") && missing(properties, "late"));
        CHECK(load_text(properties, "after=ok\n", chunk, 0, NULL) == MC_JAVA_PROPERTIES_OK);
        CHECK(mc_java_properties_size(properties) == 3 && property_is(properties, "after", "ok"));
        CHECK(text_is(anchor, "held") && arena_healthy(&a));
        properties = NULL;
        arena_release(&a);
    }
    result = 0;
cleanup:
    arena_release(&a);
    return result;
}

static int io_prefix(size_t chunk, size_t committed, size_t crlf_committed) {
    int result = 1;
    arena a = {0};
    mc_java_properties *properties = NULL;
    const mc_java_properties_allocator allocator = {&a, arena_allocate};
    static const char *const lines[] = {"a=1\nb=2\nc=3\n", "a=1\rb=2\rc=3\r",
                                        "a=1\r\nb=2\r\nc=3\r\n"};
    for (size_t ending = 0; ending < COUNT(lines); ++ending) {
        CHECK(mc_java_properties_create(&allocator, &properties) == MC_JAVA_PROPERTIES_OK);
        CHECK(load_text(properties, "anchor=held\n", chunk, 0, NULL) == MC_JAVA_PROPERTIES_OK);
        byte_stream observed;
        CHECK(load_text(properties, lines[ending], chunk, 2, &observed) ==
              MC_JAVA_PROPERTIES_IO_ERROR);
        size_t expected = ending == 2 ? crlf_committed : committed;
        CHECK(observed.calls == 2 && !observed.invalid_request);
        CHECK(mc_java_properties_size(properties) == 1 + expected &&
              property_is(properties, "anchor", "held"));
        CHECK(expected >= 1 ? property_is(properties, "a", "1") : missing(properties, "a"));
        CHECK(expected >= 2 ? property_is(properties, "b", "2") : missing(properties, "b"));
        CHECK(expected >= 3 ? property_is(properties, "c", "3") : missing(properties, "c"));
        CHECK(load_text(properties, "c=recovered\n", chunk, 0, NULL) == MC_JAVA_PROPERTIES_OK);
        CHECK(property_is(properties, "c", "recovered") && arena_healthy(&a));
        properties = NULL;
        arena_release(&a);
    }
    result = 0;
cleanup:
    arena_release(&a);
    return result;
}

static int long_lines(size_t chunk) {
    int result = 1;
    arena a = {0};
    mc_java_properties *properties = NULL;
    const mc_java_properties_allocator allocator = {&a, arena_allocate};
    uint8_t bytes[10000];
    size_t length = 0;
    bytes[length++] = '#';
    for (size_t i = 0; i < 8200; ++i)
        bytes[length++] = 'x';
    memcpy(bytes + length, "\r\nlong=", 7);
    length += 7;
    for (size_t i = 0; i < 1100; ++i)
        bytes[length++] = 'v';
    memcpy(bytes + length, "\r\nend=x\n", 8);
    length += 8;
    CHECK(mc_java_properties_create(&allocator, &properties) == MC_JAVA_PROPERTIES_OK);
    byte_stream observed;
    CHECK(load_bytes(properties, bytes, length, chunk, 0, &observed) == MC_JAVA_PROPERTIES_OK);
    CHECK(observed.offset == length && !observed.invalid_request &&
          mc_java_properties_size(properties) == 2);
    const mc_java_properties_string *value = NULL;
    CHECK(lookup(properties, "long", &value) == MC_JAVA_PROPERTIES_OK);
    mc_java_properties_string_view view;
    CHECK(mc_java_properties_string_read(value, &view) && view.length == 1100 &&
          view.units != NULL);
    for (size_t i = 0; i < view.length; ++i)
        CHECK(view.units[i] == 'v');
    CHECK(property_is(properties, "end", "x") && arena_healthy(&a));
    result = 0;
cleanup:
    arena_release(&a);
    return result;
}

static int allocation_failures(void) {
    int result = 1;
    arena baseline = {0}, trial = {0};
    mc_java_properties *base_owner = NULL, *owner = NULL;
    const mc_java_properties_allocator base_allocator = {&baseline, arena_allocate};
    const mc_java_properties_allocator trial_allocator = {&trial, arena_allocate};
    static const char target[] = "new=first\nanchor=replaced\nlast=end\n";
    CHECK(mc_java_properties_create(&base_allocator, &base_owner) == MC_JAVA_PROPERTIES_OK);
    size_t create_requests = baseline.requests;
    CHECK(load_text(base_owner, "anchor=held\n", 7, 0, NULL) == MC_JAVA_PROPERTIES_OK);
    size_t before_target = baseline.requests;
    CHECK(load_text(base_owner, target, 7, 0, NULL) == MC_JAVA_PROPERTIES_OK);
    size_t target_requests = baseline.requests - before_target;
    CHECK(create_requests != 0 && target_requests != 0 && arena_healthy(&baseline));
    for (size_t fail = 1; fail <= create_requests; ++fail) {
        trial.fail_at = fail;
        owner = base_owner;
        CHECK(mc_java_properties_create(&trial_allocator, &owner) ==
              MC_JAVA_PROPERTIES_ALLOCATION_FAILED);
        CHECK(owner == base_owner && trial.requested_failure && arena_healthy(&trial));
        owner = NULL;
        arena_release(&trial);
    }
    for (size_t fail = 1; fail <= target_requests; ++fail) {
        CHECK(mc_java_properties_create(&trial_allocator, &owner) == MC_JAVA_PROPERTIES_OK);
        CHECK(load_text(owner, "anchor=held\n", 7, 0, NULL) == MC_JAVA_PROPERTIES_OK);
        const mc_java_properties_string *first_key = NULL, *old_value = NULL;
        CHECK(mc_java_properties_entry(owner, 0, &first_key, &old_value) == MC_JAVA_PROPERTIES_OK);
        trial.fail_at = trial.requests + fail;
        CHECK(load_text(owner, target, 7, 0, NULL) == MC_JAVA_PROPERTIES_ALLOCATION_FAILED);
        CHECK(trial.requested_failure && arena_healthy(&trial));
        const mc_java_properties_string *key = NULL, *anchor = NULL, *new_value = NULL,
                                        *last = NULL;
        CHECK(mc_java_properties_entry(owner, 0, &key, &anchor) == MC_JAVA_PROPERTIES_OK);
        CHECK(key == first_key && text_is(old_value, "held"));
        bool replaced = text_is(anchor, "replaced");
        CHECK(replaced || (anchor == old_value && text_is(anchor, "held")));
        CHECK(lookup(owner, "new", &new_value) == MC_JAVA_PROPERTIES_OK);
        CHECK(new_value == NULL || text_is(new_value, "first"));
        CHECK(lookup(owner, "last", &last) == MC_JAVA_PROPERTIES_OK);
        CHECK(last == NULL || text_is(last, "end"));
        CHECK(new_value != NULL || (!replaced && last == NULL));
        CHECK(last == NULL || (new_value != NULL && replaced));
        size_t size = 1u + (new_value != NULL ? 1u : 0u) + (last != NULL ? 1u : 0u);
        CHECK(mc_java_properties_size(owner) == size);
        trial.fail_at = 0;
        CHECK(load_text(owner, "after=ok\n", 7, 0, NULL) == MC_JAVA_PROPERTIES_OK);
        CHECK(mc_java_properties_size(owner) == size + 1 && property_is(owner, "after", "ok"));
        CHECK(text_is(old_value, "held") && text_is(first_key, "anchor") && arena_healthy(&trial));
        owner = NULL;
        arena_release(&trial);
    }
    result = 0;
cleanup:
    arena_release(&trial);
    arena_release(&baseline);
    return result;
}

int main(void) {
    int result = 1;
    static const size_t chunks[] = {SIZE_MAX, 1, 7, 8192};
    /* Independent readLine/load0 expectations for failure on read-call2.
     * CRLF's buffered LF lets final c commit before the failing next read. */
    static const size_t committed[] = {2, 0, 1, 2}, crlf_committed[] = {3, 0, 1, 3};
    CHECK(arguments() == 0);
    for (size_t i = 0; i < COUNT(chunks); ++i) {
        CHECK(syntax_chunk(chunks[i]) == 0);
        CHECK(unicode_chunk(chunks[i]) == 0);
        CHECK(eof_chunk(chunks[i]) == 0);
        CHECK(malformed_chunk(chunks[i]) == 0);
        CHECK(io_prefix(chunks[i], committed[i], crlf_committed[i]) == 0);
        CHECK(long_lines(chunks[i]) == 0);
    }
    CHECK(retained_identities() == 0);
    CHECK(allocation_failures() == 0);
    printf("PASS: %u Properties native contract checks\n", checks);
    result = 0;
cleanup:
    return result;
}
