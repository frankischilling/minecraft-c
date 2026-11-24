#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "resources/assets.h"

#include "mc/resources/files/files.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <wchar.h>
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

/* Authored resources exercise the real shared Registry, CoreReader, PNG decoder
 * and AppFileSource. MemorySource faults are declared callback outcomes, not
 * allocator failures or observations of an original client. */
static unsigned int checks;
#define COUNT(values) (sizeof(values) / sizeof((values)[0]))
#define REQUIRE(condition)                                                                         \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

typedef struct fixture {
    mc_materials *materials;
    mc_java_random_profile *profile;
    mc_registry_classes *classes;
    mc_client_assets *assets;
} fixture;

static bool fixture_create(fixture *f, const mc_registry_resources *source) {
    if (!mc_materials_create(&f->materials) ||
        mc_java_random_profile_create(1024u * 1024u, &f->profile) != MC_JAVA_RANDOM_PROFILE_OK)
        return false;
    const mc_registry_options options = {.side = MC_REGISTRY_CLIENT,
                                         .random_profile = f->profile,
                                         .materials = f->materials,
                                         .resources = source};
    return mc_registry_classes_allocate(&options, &f->classes, NULL) == MC_REGISTRY_OK &&
           mc_client_assets_create(source, f->classes, &f->assets) == MC_CLIENT_ASSETS_OK;
}

static bool fixture_free(fixture *f) {
    mc_client_assets_free(f->assets);
    f->assets = NULL;
    if (mc_registry_classes_free(f->classes, NULL) != MC_REGISTRY_OK)
        return false; /* Preserve dependencies if their real borrower refused. */
    f->classes = NULL;
    mc_materials_destroy(f->materials);
    mc_java_random_profile_free(f->profile);
    *f = (fixture){0};
    return true;
}

static bool fresh_assets(fixture *f, const mc_registry_resources *source) {
    mc_client_assets_free(f->assets);
    f->assets = NULL;
    return mc_client_assets_create(source, f->classes, &f->assets) == MC_CLIENT_ASSETS_OK;
}

static bool text_is(mc_font_text text, const uint16_t *units, size_t length) {
    return text.length == length &&
           (length == 0 ||
            (text.units != NULL && memcmp(text.units, units, length * sizeof(*units)) == 0));
}

static bool ascii_is(mc_font_text text, const char *expected) {
    const size_t length = strlen(expected);
    if (text.length != length || (length != 0 && text.units == NULL))
        return false;
    for (size_t i = 0; i < length; ++i)
        if (text.units[i] != (unsigned char)expected[i])
            return false;
    return true;
}

static bool error_clear(const mc_client_assets_error *error) {
    return error->status == MC_CLIENT_ASSETS_OK && error->operation == NULL &&
           error->resource_status == MC_REGISTRY_OK && error->close_status == MC_REGISTRY_OK &&
           error->read_status == MC_JAVA_PROPERTIES_OK &&
           error->reader_status == MC_JAVA_READER_OK && error->image_status == MC_IMAGE_OK;
}

enum { SOURCE_ENTRIES = 16, SOURCE_STREAMS = 64, SOURCE_EVENTS = 2048 };

typedef struct memory_entry {
    const char *name;
    const uint8_t *bytes;
    size_t length, chunk, fail_offset;
    mc_registry_status open_status, close_status;
    mc_java_properties_status read_status;
    bool ready, no_reader;
    unsigned int invalid_count; /* 1: overcount; 2: nonzero count with error. */
} memory_entry;

typedef struct source_event {
    char kind; /* O=open, R=read, E=EOF, F=read fault, C=close. */
    size_t stream, offset, count;
    const char *name;
} source_event;

typedef struct memory_source memory_source;

typedef struct memory_stream {
    memory_source *source;
    memory_entry *entry;
    size_t index, offset, reads, closes;
    bool closed;
} memory_stream;

struct memory_source {
    mc_registry_resources api;
    memory_entry entries[SOURCE_ENTRIES];
    memory_stream streams[SOURCE_STREAMS];
    source_event events[SOURCE_EVENTS];
    size_t entry_count, stream_count, event_count, opens, closes, retired_streams;
    bool invalid, retired;
    mc_registry_classes *busy_probe;
    mc_registry_status busy_singleton, busy_wrapper;
};

static void event(memory_source *source, char kind, size_t stream, const char *name, size_t offset,
                  size_t count) {
    if (source->event_count == SOURCE_EVENTS) {
        source->invalid = true;
        return;
    }
    source->events[source->event_count++] = (source_event){kind, stream, offset, count, name};
}

static mc_java_properties_status memory_read(void *context, uint8_t *out, size_t capacity,
                                             size_t *count) {
    memory_stream *stream = context;
    memory_source *source = stream->source;
    if (out == NULL || count == NULL || capacity == 0 || source->retired) {
        source->invalid = true;
        return MC_JAVA_PROPERTIES_INVALID_ARGUMENT;
    }
    ++stream->reads;
    *count = 0;
    memory_entry *entry = stream->entry;
    if (stream->closed)
        return MC_JAVA_PROPERTIES_IO_ERROR;
    if (entry->invalid_count != 0) {
        *count = entry->invalid_count == 1u ? capacity + 1u : 1u;
        event(source, 'F', stream->index, entry->name, stream->offset, *count);
        return entry->invalid_count == 1u ? MC_JAVA_PROPERTIES_OK : MC_JAVA_PROPERTIES_IO_ERROR;
    }
    if (stream->offset >= entry->fail_offset) {
        event(source, 'F', stream->index, entry->name, stream->offset, 0);
        return entry->read_status;
    }
    size_t take = entry->length - stream->offset;
    if (take > entry->chunk)
        take = entry->chunk;
    if (take > capacity)
        take = capacity;
    if (take > entry->fail_offset - stream->offset)
        take = entry->fail_offset - stream->offset;
    if (take != 0)
        memcpy(out, entry->bytes + stream->offset, take);
    event(source, take == 0 ? 'E' : 'R', stream->index, entry->name, stream->offset, take);
    stream->offset += take;
    *count = take;
    return MC_JAVA_PROPERTIES_OK;
}

static mc_registry_status memory_open(void *context, const char *name,
                                      mc_java_properties_input *out) {
    memory_source *source = context;
    if (name == NULL || out == NULL || source->retired) {
        source->invalid = true;
        return MC_REGISTRY_INVALID_ARGUMENT;
    }
    if (source->busy_probe != NULL) {
        /* An adversarial public reentry request must fail before admitting
         * another initializer or recursively opening resources. */
        mc_registry_classes *classes = source->busy_probe;
        source->busy_probe = NULL;
        source->busy_singleton =
            mc_registry_classes_initialize_translation_singleton(classes, NULL);
        source->busy_wrapper = mc_registry_classes_initialize_translation(classes, NULL);
    }
    ++source->opens;
    event(source, 'O', source->stream_count, name, 0, 0);
    for (size_t i = 0; i < source->entry_count; ++i) {
        memory_entry *entry = &source->entries[i];
        if (strcmp(name, entry->name) != 0)
            continue;
        if (entry->open_status != MC_REGISTRY_OK)
            return entry->open_status;
        if (source->stream_count == SOURCE_STREAMS) {
            source->invalid = true;
            return MC_REGISTRY_LIMIT;
        }
        memory_stream *stream = &source->streams[source->stream_count];
        *stream =
            (memory_stream){.source = source, .entry = entry, .index = source->stream_count++};
        *out = (mc_java_properties_input){stream, entry->no_reader ? NULL : memory_read};
        return MC_REGISTRY_OK;
    }
    return MC_REGISTRY_NULL_REFERENCE;
}

static memory_stream *owned_stream(memory_source *source, const mc_java_properties_input *input) {
    for (size_t i = 0; input != NULL && i < source->stream_count; ++i) {
        memory_stream *stream = &source->streams[i];
        if (input->context == stream &&
            input->read == (stream->entry->no_reader ? NULL : memory_read))
            return stream;
    }
    source->invalid = true;
    return NULL;
}

static mc_registry_status memory_close(void *context, const mc_java_properties_input *input) {
    memory_source *source = context;
    memory_stream *stream = owned_stream(source, input);
    if (stream == NULL || source->retired)
        return MC_REGISTRY_INVALID_ARGUMENT;
    ++source->closes;
    ++stream->closes;
    event(source, 'C', stream->index, stream->entry->name, stream->offset, 0);
    if (stream->closed) {
        source->invalid = true; /* These asset paths must close exactly once. */
        return MC_REGISTRY_OK;
    }
    stream->closed = true;
    return stream->entry->close_status;
}

static bool memory_ready(void *context, const mc_java_properties_input *input) {
    memory_source *source = context;
    memory_stream *stream = owned_stream(source, input);
    return stream != NULL && !source->retired && !stream->closed && stream->entry->ready &&
           stream->offset < stream->entry->length;
}

static void source_init(memory_source *source) {
    *source = (memory_source){0};
    source->api = (mc_registry_resources){source, memory_open, memory_close, memory_ready};
}

static memory_entry *add(memory_source *source, const char *name, const void *bytes,
                         size_t length) {
    if (source->entry_count == SOURCE_ENTRIES) {
        source->invalid = true;
        return NULL;
    }
    memory_entry *entry = &source->entries[source->entry_count++];
    *entry = (memory_entry){.name = name,
                            .bytes = bytes,
                            .length = length,
                            .chunk = 8192,
                            .fail_offset = SIZE_MAX,
                            .read_status = MC_JAVA_PROPERTIES_IO_ERROR};
    return entry;
}

static void source_retire(memory_source *source) {
    for (size_t i = 0; i < source->stream_count; ++i)
        if (!source->streams[i].closed) {
            source->streams[i].closed = true;
            ++source->retired_streams;
        }
    source->retired = true;
}

typedef struct png_buffer {
    uint8_t bytes[256];
    size_t length;
} png_buffer;

static void u32(uint8_t *out, uint32_t value) {
    out[0] = (uint8_t)(value >> 24);
    out[1] = (uint8_t)(value >> 16);
    out[2] = (uint8_t)(value >> 8);
    out[3] = (uint8_t)value;
}

static bool png_chunk(png_buffer *out, const char *kind, const uint8_t *bytes, size_t length) {
    if (length > sizeof(out->bytes) - 12u || out->length > sizeof(out->bytes) - length - 12u)
        return false;
    uint8_t *next = out->bytes + out->length;
    u32(next, (uint32_t)length);
    memcpy(next + 4, kind, 4);
    if (length != 0)
        memcpy(next + 8, bytes, length);
    uLong checksum = crc32(0, next + 4, 4);
    if (length != 0)
        checksum = crc32(checksum, bytes, (uInt)length);
    u32(next + 8 + length, (uint32_t)checksum);
    out->length += length + 12u;
    return true;
}

/* A single authored, unfiltered scanline; zlib is not a PNG encoder. */
static bool png_make(png_buffer *out, uint32_t width, uint8_t depth, uint8_t color,
                     const uint8_t *row, size_t row_length) {
    static const uint8_t signature[] = {137, 80, 78, 71, 13, 10, 26, 10};
    memcpy(out->bytes, signature, sizeof(signature));
    out->length = sizeof(signature);
    uint8_t header[13] = {0}, compressed[128];
    u32(header, width);
    u32(header + 4, 1);
    header[8] = depth;
    header[9] = color;
    uLongf size = sizeof(compressed);
    return compress2(compressed, &size, row, (uLong)row_length, Z_BEST_SPEED) == Z_OK &&
           png_chunk(out, "IHDR", header, sizeof(header)) &&
           png_chunk(out, "IDAT", compressed, (size_t)size) && png_chunk(out, "IEND", NULL, 0);
}

static const uint8_t rgba_row[] = {0, 1, 2, 3, 0, 250, 128, 64, 255};
static const char english[] = "menu.singleplayer=Solo\nshared=first\n"
                              "unicode=\\u00e9\\ud83d\\ude00\nempty=\n";
static const char statistics[] = "shared=second\nstat.native=Count\n";

static int test_labels(void) {
    int result = 1;
    fixture f = {0};
    memory_source source;
    source_init(&source);
    REQUIRE(add(&source, "/lang/en_US.lang", english, sizeof(english) - 1u) != NULL);
    REQUIRE(add(&source, "/lang/stats_US.lang", statistics, sizeof(statistics) - 1u) != NULL);
    REQUIRE(fixture_create(&f, &source.api));
    REQUIRE(source.opens == 0 && source.event_count == 0);
    mc_registry_translation_view translation;
    REQUIRE(mc_registry_classes_inspect_translation(f.classes, &translation, NULL) ==
            MC_REGISTRY_OK);
    REQUIRE(translation.singleton_phase == MC_REGISTRY_COLD &&
            translation.wrapper_phase == MC_REGISTRY_COLD);
    mc_client_assets_error error = {.status = MC_CLIENT_ASSETS_LIMIT};
    mc_font_text label = {0};
    REQUIRE(mc_client_assets_label(f.assets, "menu.singleplayer", &label, &error) ==
            MC_CLIENT_ASSETS_OK);
    REQUIRE(ascii_is(label, "Solo") && error_clear(&error));
    REQUIRE(source.opens == 2 && source.stream_count == 2 && source.closes == 0);
    REQUIRE(strcmp(source.streams[0].entry->name, "/lang/en_US.lang") == 0 &&
            strcmp(source.streams[1].entry->name, "/lang/stats_US.lang") == 0);
    REQUIRE(mc_registry_classes_inspect_translation(f.classes, &translation, NULL) ==
            MC_REGISTRY_OK);
    REQUIRE(translation.singleton_phase == MC_REGISTRY_READY &&
            translation.wrapper_phase == MC_REGISTRY_COLD && translation.singleton_a != NULL &&
            translation.wrapper_a == NULL);
    REQUIRE(mc_client_assets_label(f.assets, "shared", &label, NULL) == MC_CLIENT_ASSETS_OK &&
            ascii_is(label, "second"));
    const uint16_t key[] = {'s', 'h', 'a', 'r', 'e', 'd'};
    const mc_registry_string *string = NULL;
    mc_registry_string_view actual;
    REQUIRE(mc_translation_property(translation.singleton_a, key, COUNT(key), &string, NULL) ==
                MC_REGISTRY_OK &&
            mc_registry_string_read(string, &actual));
    REQUIRE(label.units == actual.units && label.length == actual.length);
    char absent[] = "absent.key";
    REQUIRE(mc_client_assets_label(f.assets, absent, &label, NULL) == MC_CLIENT_ASSETS_OK &&
            ascii_is(label, "absent.key"));
    const mc_font_text fallback = label;
    absent[0] = 'X';
    REQUIRE(mc_client_assets_label(f.assets, "absent.key", &label, NULL) == MC_CLIENT_ASSETS_OK &&
            label.units == fallback.units);
    REQUIRE(mc_client_assets_label(f.assets, "empty", &label, NULL) == MC_CLIENT_ASSETS_OK &&
            label.length == 0);
    REQUIRE(mc_client_assets_label(f.assets, "", &label, NULL) == MC_CLIENT_ASSETS_OK &&
            label.length == 0);
    const uint16_t unicode[] = {0x00e9, 0xd83d, 0xde00};
    REQUIRE(mc_client_assets_label(f.assets, "unicode", &label, NULL) == MC_CLIENT_ASSETS_OK &&
            text_is(label, unicode, COUNT(unicode)));
    const mc_font_text retained = label;
    const size_t events = source.event_count;
    REQUIRE(mc_client_assets_label(f.assets, "unicode", &label, NULL) == MC_CLIENT_ASSETS_OK &&
            label.units == retained.units);
    REQUIRE(source.event_count == events);
    char long_key[MC_CLIENT_ASSETS_MAX_NAME_BYTES + 2u];
    memset(long_key, 'x', sizeof(long_key));
    long_key[sizeof(long_key) - 1u] = 0;
    REQUIRE(mc_client_assets_label(f.assets, long_key, &label, &error) == MC_CLIENT_ASSETS_LIMIT &&
            label.units == retained.units);
    REQUIRE(mc_client_assets_label(f.assets, "\xc3\xa9", &label, &error) ==
                MC_CLIENT_ASSETS_INVALID_ARGUMENT &&
            label.units == retained.units);
    mc_client_assets_free(f.assets);
    f.assets = NULL;
    REQUIRE(!source.retired && source.closes == 0 && text_is(retained, unicode, COUNT(unicode)));
    REQUIRE(mc_registry_classes_inspect_translation(f.classes, &translation, NULL) ==
                MC_REGISTRY_OK &&
            translation.singleton_phase == MC_REGISTRY_READY);
    REQUIRE(mc_java_random_profile_allocation_bytes(f.profile) != 0 &&
            mc_materials_get(f.materials, MC_MATERIAL_a) != NULL);
    REQUIRE(mc_client_assets_create(&source.api, f.classes, &f.assets) == MC_CLIENT_ASSETS_OK);
    REQUIRE(mc_client_assets_label(f.assets, "unicode", &label, NULL) == MC_CLIENT_ASSETS_OK &&
            label.units == retained.units && source.event_count == events);
    result = 0;
cleanup:
    if (!fixture_free(&f))
        return 1;
    source_retire(&source);
    if (source.invalid || (result == 0 && source.retired_streams != 2))
        result = 1;
    return result;
}

static int test_translation_routes(void) {
    int result = 1;
    fixture f = {0};
    memory_source source;
    source_init(&source);
    REQUIRE(add(&source, "/lang/en_US.lang", english, sizeof(english) - 1u) != NULL);
    REQUIRE(add(&source, "/lang/stats_US.lang", statistics, sizeof(statistics) - 1u) != NULL);
    REQUIRE(fixture_create(&f, &source.api));
    mc_registry_error registry_error;
    REQUIRE(mc_registry_classes_initialize_translation_singleton(NULL, &registry_error) ==
                MC_REGISTRY_INVALID_ARGUMENT &&
            registry_error.status == MC_REGISTRY_INVALID_ARGUMENT);
    mc_client_assets_error error;
    REQUIRE(mc_client_assets_initialize_translation(NULL, &error) ==
            MC_CLIENT_ASSETS_INVALID_ARGUMENT);
    REQUIRE(source.opens == 0);
    source.busy_probe = f.classes;
    REQUIRE(mc_client_assets_initialize_translation(f.assets, &error) == MC_CLIENT_ASSETS_OK &&
            error_clear(&error));
    REQUIRE(source.busy_singleton == MC_REGISTRY_BUSY && source.busy_wrapper == MC_REGISTRY_BUSY);
    mc_registry_translation_view translation;
    REQUIRE(mc_registry_classes_inspect_translation(f.classes, &translation, NULL) ==
                MC_REGISTRY_OK &&
            translation.singleton_phase == MC_REGISTRY_READY &&
            translation.singleton_initializer_pc == 10 &&
            translation.wrapper_phase == MC_REGISTRY_COLD && translation.wrapper_a == NULL);
    const size_t events = source.event_count;
    REQUIRE(mc_registry_classes_initialize_translation_singleton(f.classes, NULL) ==
                MC_REGISTRY_OK &&
            mc_client_assets_initialize_translation(f.assets, NULL) == MC_CLIENT_ASSETS_OK &&
            source.event_count == events);
    const uint16_t key[] = {'s', 'h', 'a', 'r', 'e', 'd'};
    mc_font_text direct = {0}, wrapper = {0};
    REQUIRE(mc_client_assets_label_text(f.assets, (mc_font_text){key, COUNT(key)}, false, &direct,
                                        &error) == MC_CLIENT_ASSETS_OK &&
            ascii_is(direct, "second") && error_clear(&error));
    REQUIRE(mc_registry_classes_inspect_translation(f.classes, &translation, NULL) ==
                MC_REGISTRY_OK &&
            translation.wrapper_phase == MC_REGISTRY_COLD);
    REQUIRE(mc_client_assets_label_text_wrapper(f.assets, (mc_font_text){key, COUNT(key)}, false,
                                                &wrapper, &error) == MC_CLIENT_ASSETS_OK &&
            wrapper.units == direct.units && wrapper.length == direct.length &&
            error_clear(&error));
    REQUIRE(
        mc_registry_classes_inspect_translation(f.classes, &translation, NULL) == MC_REGISTRY_OK &&
        translation.singleton_phase == MC_REGISTRY_READY &&
        translation.wrapper_phase == MC_REGISTRY_READY && translation.wrapper_initializer_pc == 6 &&
        translation.wrapper_a == translation.singleton_a && source.event_count == events);
    REQUIRE(mc_client_assets_label(f.assets, "shared", &wrapper, NULL) == MC_CLIENT_ASSETS_OK &&
            wrapper.units == direct.units && source.event_count == events);
    result = 0;
cleanup:
    if (!fixture_free(&f))
        return 1;
    source_retire(&source);
    if (source.invalid || (result == 0 && source.retired_streams != 2))
        result = 1;
    return result;
}

static int test_counted_labels(void) {
    int result = 1;
    fixture f = {0};
    memory_source source;
    uint16_t *large = NULL;
    source_init(&source);
    static const char counted[] = "a\\u0000b=nul-value\n\\ud800=high-surrogate\n=empty-key\n";
    REQUIRE(add(&source, "/lang/en_US.lang", counted, sizeof(counted) - 1u) != NULL);
    REQUIRE(add(&source, "/lang/stats_US.lang", "", 0) != NULL);
    REQUIRE(fixture_create(&f, &source.api));
    const uint16_t canary[] = {'s', 'a', 'f', 'e'};
    mc_font_text text = {canary, COUNT(canary)};
    mc_client_assets_error error;
    REQUIRE(mc_client_assets_label_text(f.assets, (mc_font_text){NULL, 1}, false, &text, &error) ==
                MC_CLIENT_ASSETS_INVALID_ARGUMENT &&
            text.units == canary && source.opens == 0);
    REQUIRE(mc_client_assets_label_text(f.assets, (mc_font_text){canary, 0}, true, &text, &error) ==
                MC_CLIENT_ASSETS_INVALID_ARGUMENT &&
            text.units == canary && source.opens == 0);
    const uint16_t nul_key[] = {'a', 0, 'b'};
    REQUIRE(mc_client_assets_label_text(f.assets, (mc_font_text){nul_key, COUNT(nul_key)}, false,
                                        &text, &error) == MC_CLIENT_ASSETS_OK &&
            ascii_is(text, "nul-value") && error_clear(&error));
    const uint16_t high_key[] = {0xd800};
    REQUIRE(mc_client_assets_label_text(f.assets, (mc_font_text){high_key, 1}, false, &text,
                                        NULL) == MC_CLIENT_ASSETS_OK &&
            ascii_is(text, "high-surrogate"));
    REQUIRE(mc_client_assets_label_text(f.assets, (mc_font_text){NULL, 0}, false, &text, NULL) ==
                MC_CLIENT_ASSETS_OK &&
            ascii_is(text, "empty-key"));
    uint16_t missing[] = {'x', 0, 0xd800, 0xdc00, 0xdfff};
    const uint16_t expected[] = {'x', 0, 0xd800, 0xdc00, 0xdfff};
    REQUIRE(mc_client_assets_label_text(f.assets, (mc_font_text){missing, COUNT(missing)}, false,
                                        &text, NULL) == MC_CLIENT_ASSETS_OK &&
            text_is(text, expected, COUNT(expected)) && text.units != missing);
    const mc_font_text fallback = text;
    memset(missing, 0, sizeof(missing));
    REQUIRE(text_is(fallback, expected, COUNT(expected)));
    REQUIRE(mc_client_assets_label_text_wrapper(f.assets, (mc_font_text){expected, COUNT(expected)},
                                                false, &text, NULL) == MC_CLIENT_ASSETS_OK &&
            text.units == fallback.units && text_is(text, expected, COUNT(expected)));
    large = malloc((MC_CLIENT_ASSETS_MAX_TEXT_UNITS + 1u) * sizeof(*large));
    REQUIRE(large != NULL);
    for (size_t i = 0; i <= MC_CLIENT_ASSETS_MAX_TEXT_UNITS; ++i)
        large[i] = 0xd800;
    REQUIRE(mc_client_assets_label_text(f.assets,
                                        (mc_font_text){large, MC_CLIENT_ASSETS_MAX_TEXT_UNITS},
                                        false, &text, NULL) == MC_CLIENT_ASSETS_OK &&
            text_is(text, large, MC_CLIENT_ASSETS_MAX_TEXT_UNITS) && text.units != large);
    const mc_font_text maximal = text;
    large[0] = 'z';
    REQUIRE(maximal.units[0] == 0xd800);
    REQUIRE(mc_client_assets_label_text_wrapper(
                f.assets, (mc_font_text){large, MC_CLIENT_ASSETS_MAX_TEXT_UNITS + 1u}, false, &text,
                &error) == MC_CLIENT_ASSETS_LIMIT &&
            text.units == maximal.units && text.length == maximal.length);
    REQUIRE(source.opens == 2 && source.closes == 0);
    result = 0;
cleanup:
    free(large);
    if (!fixture_free(&f))
        return 1;
    source_retire(&source);
    if (source.invalid || (result == 0 && source.retired_streams != 2))
        result = 1;
    return result;
}

static int test_null_labels(void) {
    int result = 1;
    fixture f = {0};
    memory_source source;
    source_init(&source);
    REQUIRE(add(&source, "/lang/en_US.lang", english, sizeof(english) - 1u) != NULL);
    REQUIRE(add(&source, "/lang/stats_US.lang", statistics, sizeof(statistics) - 1u) != NULL);
    REQUIRE(fixture_create(&f, &source.api));
    const uint16_t canary[] = {'s', 'a', 'f', 'e'};
    mc_font_text text = {canary, COUNT(canary)};
    mc_client_assets_error error;
    REQUIRE(mc_client_assets_label_text(f.assets, (mc_font_text){NULL, 0}, true, &text, &error) ==
                MC_CLIENT_ASSETS_NULL_REFERENCE &&
            text.units == canary && text.length == COUNT(canary) &&
            error.registry_error.status == MC_REGISTRY_NULL_REFERENCE);
    mc_registry_translation_view translation;
    REQUIRE(mc_registry_classes_inspect_translation(f.classes, &translation, NULL) ==
                MC_REGISTRY_OK &&
            translation.singleton_phase == MC_REGISTRY_READY &&
            translation.wrapper_phase == MC_REGISTRY_COLD && source.opens == 2);
    const size_t events = source.event_count;
    REQUIRE(mc_client_assets_label_text_wrapper(f.assets, (mc_font_text){NULL, 0}, true, &text,
                                                &error) == MC_CLIENT_ASSETS_NULL_REFERENCE &&
            text.units == canary && error.registry_error.status == MC_REGISTRY_NULL_REFERENCE);
    REQUIRE(mc_registry_classes_inspect_translation(f.classes, &translation, NULL) ==
                MC_REGISTRY_OK &&
            translation.singleton_phase == MC_REGISTRY_READY &&
            translation.wrapper_phase == MC_REGISTRY_READY && source.event_count == events);
    REQUIRE(mc_client_assets_label_text(f.assets, (mc_font_text){NULL, 0}, false, &text, NULL) ==
                MC_CLIENT_ASSETS_OK &&
            text.length == 0);
    REQUIRE(strcmp(mc_client_assets_status_string(MC_CLIENT_ASSETS_NULL_REFERENCE),
                   "null client translation reference") == 0);
    result = 0;
cleanup:
    if (!fixture_free(&f))
        return 1;
    source_retire(&source);
    if (source.invalid || (result == 0 && source.retired_streams != 2))
        result = 1;
    return result;
}

static int test_label_cache_bound(void) {
    int result = 1;
    fixture f = {0};
    memory_source source;
    source_init(&source);
    REQUIRE(add(&source, "/lang/en_US.lang", english, sizeof(english) - 1u) != NULL);
    REQUIRE(add(&source, "/lang/stats_US.lang", statistics, sizeof(statistics) - 1u) != NULL);
    REQUIRE(fixture_create(&f, &source.api));
    mc_font_text text = {0};
    for (size_t i = 0; i < MC_CLIENT_ASSETS_MAX_LABELS; ++i) {
        const uint16_t key[] = {0x0800, (uint16_t)i};
        REQUIRE(mc_client_assets_label_text(f.assets, (mc_font_text){key, COUNT(key)}, false, &text,
                                            NULL) == MC_CLIENT_ASSETS_OK &&
                text_is(text, key, COUNT(key)));
    }
    mc_registry_translation_view translation;
    REQUIRE(mc_registry_classes_inspect_translation(f.classes, &translation, NULL) ==
                MC_REGISTRY_OK &&
            translation.wrapper_phase == MC_REGISTRY_COLD);
    const uint16_t first[] = {0x0800, 0};
    REQUIRE(mc_client_assets_label_text_wrapper(f.assets, (mc_font_text){first, COUNT(first)},
                                                false, &text, NULL) == MC_CLIENT_ASSETS_OK &&
            text_is(text, first, COUNT(first)));
    const mc_font_text cached = text;
    const uint16_t other[] = {0x0800, MC_CLIENT_ASSETS_MAX_LABELS};
    mc_client_assets_error error;
    REQUIRE(mc_client_assets_label_text_wrapper(f.assets, (mc_font_text){other, COUNT(other)},
                                                false, &text, &error) == MC_CLIENT_ASSETS_LIMIT &&
            text.units == cached.units && text.length == cached.length);
    REQUIRE(mc_client_assets_label_text_wrapper(f.assets, (mc_font_text){NULL, 0}, true, &text,
                                                &error) == MC_CLIENT_ASSETS_NULL_REFERENCE &&
            text.units == cached.units);
    REQUIRE(mc_registry_classes_inspect_translation(f.classes, &translation, NULL) ==
                MC_REGISTRY_OK &&
            translation.wrapper_phase == MC_REGISTRY_READY && source.opens == 2);
    result = 0;
cleanup:
    if (!fixture_free(&f))
        return 1;
    source_retire(&source);
    if (source.invalid || (result == 0 && source.retired_streams != 2))
        result = 1;
    return result;
}

static bool image_pixels(const mc_image *image) {
    mc_image_view view = {0};
    return mc_image_view_get(image, &view) && view.width == 2 && view.height == 1 &&
           view.stride == 8 && memcmp(view.pixels, rgba_row + 1, 8) == 0;
}

static int test_images(void) {
    int result = 1;
    fixture f = {0};
    memory_source source;
    source_init(&source);
    png_buffer rgba, gray, sixteen, malformed;
    const uint8_t gray_row[] = {0, 128}, sixteen_row[] = {0, 0, 1, 0, 2, 0, 3, 0, 4};
    REQUIRE(png_make(&rgba, 2, 8, 6, rgba_row, sizeof(rgba_row)));
    REQUIRE(png_make(&gray, 1, 8, 0, gray_row, sizeof(gray_row)));
    REQUIRE(png_make(&sixteen, 1, 16, 6, sixteen_row, sizeof(sixteen_row)));
    malformed = rgba;
    malformed.bytes[malformed.length - 1u] ^= 1u;
    memory_entry *valid = add(&source, "/tiny.png", rgba.bytes, rgba.length);
    REQUIRE(valid != NULL);
    valid->chunk = 3;
    REQUIRE(add(&source, "/Tiny.png", rgba.bytes, rgba.length) != NULL);
    REQUIRE(add(&source, "/gray.png", gray.bytes, gray.length) != NULL);
    REQUIRE(add(&source, "/sixteen.png", sixteen.bytes, sixteen.length) != NULL);
    memory_entry *bad = add(&source, "/bad.png", malformed.bytes, malformed.length);
    memory_entry *fault = add(&source, "/fault.png", rgba.bytes, rgba.length);
    REQUIRE(bad != NULL && fault != NULL && fixture_create(&f, &source.api));
    const mc_image *image = NULL;
    mc_client_assets_error error;
    REQUIRE(mc_client_assets_image(f.assets, "/tiny.png", &image, &error) == MC_CLIENT_ASSETS_OK &&
            image_pixels(image) && error_clear(&error));
    REQUIRE(source.opens == 1 && source.closes == 1 && source.streams[0].closed &&
            source.streams[0].closes == 1);
    const mc_image *held = image;
    const size_t events = source.event_count;
    valid->open_status = MC_REGISTRY_IO_FAILED;
    REQUIRE(mc_client_assets_image(f.assets, "/tiny.png", &image, &error) == MC_CLIENT_ASSETS_OK &&
            image == held && source.event_count == events);
    REQUIRE(mc_client_assets_image(f.assets, "/Tiny.png", &image, &error) == MC_CLIENT_ASSETS_OK &&
            image != held && image_pixels(image));
    image = held;
    REQUIRE(mc_client_assets_image(f.assets, "/gray.png", &image, &error) ==
                MC_CLIENT_ASSETS_UNSUPPORTED_IMAGE_FORMAT &&
            image == held && error.png_bit_depth == 8 && error.png_color_type == 0);
    REQUIRE(mc_client_assets_image(f.assets, "/sixteen.png", &image, &error) ==
                MC_CLIENT_ASSETS_UNSUPPORTED_IMAGE_FORMAT &&
            image == held && error.png_bit_depth == 16 && error.png_color_type == 6);
    REQUIRE(mc_client_assets_image(f.assets, "/bad.png", &image, &error) ==
                MC_CLIENT_ASSETS_IMAGE_FAILED &&
            image == held && error.image_status == MC_IMAGE_INVALID_PNG);
    bad->bytes = rgba.bytes;
    bad->length = rgba.length;
    REQUIRE(mc_client_assets_image(f.assets, "/bad.png", &image, NULL) == MC_CLIENT_ASSETS_OK &&
            image_pixels(image));
    image = held;
    fault->fail_offset = 0;
    REQUIRE(mc_client_assets_image(f.assets, "/fault.png", &image, &error) ==
                MC_CLIENT_ASSETS_RESOURCE_FAILED &&
            error.read_status == MC_JAVA_PROPERTIES_IO_ERROR && image == held);
    fault->fail_offset = SIZE_MAX;
    fault->close_status = MC_REGISTRY_IO_FAILED;
    REQUIRE(mc_client_assets_image(f.assets, "/fault.png", &image, &error) ==
                MC_CLIENT_ASSETS_READER_FAILED &&
            error.reader_status == MC_JAVA_READER_IO_ERROR &&
            error.close_status == MC_REGISTRY_IO_FAILED && image == held);
    fault->close_status = MC_REGISTRY_OK;
    fault->invalid_count = 1;
    REQUIRE(mc_client_assets_image(f.assets, "/fault.png", &image, &error) ==
                MC_CLIENT_ASSETS_RESOURCE_FAILED &&
            error.read_status == MC_JAVA_PROPERTIES_INVALID_ARGUMENT && image == held);
    fault->invalid_count = 2;
    REQUIRE(mc_client_assets_image(f.assets, "/fault.png", &image, &error) ==
                MC_CLIENT_ASSETS_RESOURCE_FAILED &&
            error.read_status == MC_JAVA_PROPERTIES_INVALID_ARGUMENT && image == held);
    fault->invalid_count = 0;
    fault->no_reader = true;
    REQUIRE(mc_client_assets_image(f.assets, "/fault.png", &image, &error) ==
                MC_CLIENT_ASSETS_RESOURCE_FAILED &&
            error.resource_status == MC_REGISTRY_INVALID_ARGUMENT && image == held);
    fault->no_reader = false;
    REQUIRE(mc_client_assets_image(f.assets, "/fault.png", &image, NULL) == MC_CLIENT_ASSETS_OK &&
            image_pixels(image));
    image = held;
    const size_t opens = source.opens, closes = source.closes;
    REQUIRE(mc_client_assets_image(f.assets, "/absent.png", &image, &error) ==
                MC_CLIENT_ASSETS_RESOURCE_FAILED &&
            error.resource_status == MC_REGISTRY_NULL_REFERENCE && image == held &&
            source.opens == opens + 1u && source.closes == closes);
    REQUIRE(mc_client_assets_image(f.assets, "tiny.png", &image, &error) ==
                MC_CLIENT_ASSETS_INVALID_ARGUMENT &&
            image == held);
    REQUIRE(mc_client_assets_image(f.assets, "/", &image, &error) ==
                MC_CLIENT_ASSETS_INVALID_ARGUMENT &&
            image == held);
    REQUIRE(mc_client_assets_image(f.assets, "/\xc3\xa9", &image, &error) ==
                MC_CLIENT_ASSETS_INVALID_ARGUMENT &&
            image == held);
    REQUIRE(source.closes == source.stream_count);
    for (size_t i = 0; i < source.stream_count; ++i)
        REQUIRE(source.streams[i].closed && source.streams[i].closes == 1);
    REQUIRE(image_pixels(held));
    result = 0;
cleanup:
    if (!fixture_free(&f))
        return 1;
    source_retire(&source);
    if (source.invalid || source.retired_streams != 0)
        result = 1;
    return result;
}

static int test_font(void) {
    int result = 1;
    fixture f = {0};
    memory_source source;
    source_init(&source);
    const uint16_t canary[] = {'s', 'a', 'f', 'e'};
    static const uint8_t raw[] = {'#',  'k', 'e',  'e',  'p',  '\r', '\n', 'A',  0xc3, 0xa9,
                                  '\r', 'B', 0xf0, 0x9f, 0x98, 0x80, '\n', '\n', 'C',  0xff};
    static const uint16_t expected[] = {'#', 'k',    'e',    'e',  'p',  '\n', 'A',    0x00e9, '\n',
                                        'B', 0xd83d, 0xde00, '\n', '\n', 'C',  0xfffd, '\n'};
    memory_entry *entry = add(&source, "/font.txt", raw, sizeof(raw));
    REQUIRE(entry != NULL && fixture_create(&f, &source.api));
    entry->chunk = 1;
    mc_font_text text = {0};
    mc_client_assets_error error;
    REQUIRE(mc_client_assets_font_txt(f.assets, &text, &error) == MC_CLIENT_ASSETS_OK &&
            text_is(text, expected, COUNT(expected)) && error_clear(&error));
    const mc_font_text retained = text;
    const size_t events = source.event_count;
    REQUIRE(mc_client_assets_font_txt(f.assets, &text, NULL) == MC_CLIENT_ASSETS_OK &&
            text.units == retained.units && source.event_count == events);
    REQUIRE(source.closes == 1 && source.streams[0].closes == 1);
    static const uint8_t interrupted[] = "AB\npartial";
    entry->bytes = interrupted;
    entry->length = sizeof(interrupted) - 1u;
    entry->fail_offset = 6;
    REQUIRE(fresh_assets(&f, &source.api));
    REQUIRE(mc_client_assets_font_txt(f.assets, &text, &error) == MC_CLIENT_ASSETS_OK &&
            ascii_is(text, "AB\n") && error_clear(&error));
    /* Availability=true lets StreamDecoder attempt the failed read in its
     * first fill, before it publishes even the already-read AB newline. */
    entry->ready = true;
    REQUIRE(fresh_assets(&f, &source.api));
    REQUIRE(mc_client_assets_font_txt(f.assets, &text, NULL) == MC_CLIENT_ASSETS_OK &&
            text.length == 0);
    entry->ready = false;
    entry->fail_offset = SIZE_MAX;
    entry->bytes = (const uint8_t *)"AB";
    entry->length = 2;
    entry->close_status = MC_REGISTRY_IO_FAILED;
    REQUIRE(fresh_assets(&f, &source.api));
    REQUIRE(mc_client_assets_font_txt(f.assets, &text, &error) == MC_CLIENT_ASSETS_OK &&
            ascii_is(text, "AB\n") && error_clear(&error));
    entry->close_status = MC_REGISTRY_OK;
    entry->open_status = MC_REGISTRY_NULL_REFERENCE;
    REQUIRE(fresh_assets(&f, &source.api));
    const size_t closes = source.closes;
    REQUIRE(mc_client_assets_font_txt(f.assets, &text, NULL) == MC_CLIENT_ASSETS_OK &&
            text.length == 0 && source.closes == closes);
    const size_t opens = source.opens;
    entry->open_status = MC_REGISTRY_OK;
    REQUIRE(mc_client_assets_font_txt(f.assets, &text, NULL) == MC_CLIENT_ASSETS_OK &&
            text.length == 0 && source.opens == opens);
    REQUIRE(fresh_assets(&f, &source.api));
    text = (mc_font_text){canary, COUNT(canary)};
    entry->fail_offset = 0;
    entry->read_status = MC_JAVA_PROPERTIES_LIMIT;
    REQUIRE(mc_client_assets_font_txt(f.assets, &text, &error) == MC_CLIENT_ASSETS_LIMIT &&
            text.units == canary && text.length == COUNT(canary) &&
            error.read_status == MC_JAVA_PROPERTIES_LIMIT);
    entry->read_status = MC_JAVA_PROPERTIES_IO_ERROR;
    entry->fail_offset = SIZE_MAX;
    entry->no_reader = true;
    REQUIRE(mc_client_assets_font_txt(f.assets, &text, &error) ==
                MC_CLIENT_ASSETS_RESOURCE_FAILED &&
            text.units == canary && error.resource_status == MC_REGISTRY_INVALID_ARGUMENT);
    static uint8_t long_line[MC_CLIENT_ASSETS_MAX_TEXT_UNITS];
    memset(long_line, 'A', sizeof(long_line));
    entry->no_reader = false;
    entry->bytes = long_line;
    entry->length = sizeof(long_line);
    entry->chunk = 8192;
    REQUIRE(mc_client_assets_font_txt(f.assets, &text, &error) == MC_CLIENT_ASSETS_LIMIT &&
            text.units == canary && text.length == COUNT(canary));
    --entry->length;
    REQUIRE(mc_client_assets_font_txt(f.assets, &text, NULL) == MC_CLIENT_ASSETS_OK &&
            text.length == MC_CLIENT_ASSETS_MAX_TEXT_UNITS && text.units[0] == 'A' &&
            text.units[text.length - 2u] == 'A' && text.units[text.length - 1u] == '\n');
    REQUIRE(source.closes == source.stream_count);
    result = 0;
cleanup:
    if (!fixture_free(&f))
        return 1;
    source_retire(&source);
    if (source.invalid || source.retired_streams != 0)
        result = 1;
    return result;
}

static bool same_next(mc_java_random a, mc_java_random b) {
    return mc_java_random_next_i64(&a) == mc_java_random_next_i64(&b) &&
           mc_java_random_next_i64(&a) == mc_java_random_next_i64(&b);
}

static int test_failed_translation(void) {
    int result = 1;
    fixture f = {0};
    memory_source source;
    source_init(&source);
    static const char malformed[] = "menu.native=\\u12Q4\n";
    REQUIRE(add(&source, "/lang/en_US.lang", malformed, sizeof(malformed) - 1u) != NULL);
    REQUIRE(add(&source, "/lang/stats_US.lang", statistics, sizeof(statistics) - 1u) != NULL);
    REQUIRE(fixture_create(&f, &source.api));
    const uint16_t canary[] = {'s', 'a', 'f', 'e'};
    mc_font_text text = {canary, COUNT(canary)};
    mc_client_assets_error error;
    REQUIRE(mc_client_assets_label(f.assets, "menu.native", &text, &error) ==
                MC_CLIENT_ASSETS_TRANSLATION_FAILED &&
            text.units == canary && text.length == COUNT(canary) &&
            error.registry_error.status == MC_REGISTRY_ILLEGAL_ARGUMENT);
    REQUIRE(source.opens == 1 && source.closes == 0);
    mc_registry_translation_view translation;
    REQUIRE(mc_registry_classes_inspect_translation(f.classes, &translation, NULL) ==
                MC_REGISTRY_OK &&
            translation.singleton_phase == MC_REGISTRY_FAILED &&
            translation.wrapper_phase == MC_REGISTRY_COLD);
    const size_t events = source.event_count;
    REQUIRE(mc_client_assets_label(f.assets, "menu.native", &text, &error) ==
                MC_CLIENT_ASSETS_TRANSLATION_FAILED &&
            text.units == canary &&
            error.registry_error.status == MC_REGISTRY_FAILED_INITIALIZATION &&
            source.event_count == events);
    REQUIRE(mc_client_assets_label_text(f.assets, (mc_font_text){NULL, 0}, true, &text, &error) ==
                MC_CLIENT_ASSETS_TRANSLATION_FAILED &&
            text.units == canary &&
            error.registry_error.status == MC_REGISTRY_FAILED_INITIALIZATION);
    REQUIRE(mc_client_assets_label_text_wrapper(f.assets, (mc_font_text){NULL, 0}, true, &text,
                                                &error) == MC_CLIENT_ASSETS_TRANSLATION_FAILED &&
            text.units == canary &&
            error.registry_error.status == MC_REGISTRY_FAILED_INITIALIZATION);
    REQUIRE(mc_registry_classes_inspect_translation(f.classes, &translation, NULL) ==
                MC_REGISTRY_OK &&
            translation.singleton_phase == MC_REGISTRY_FAILED &&
            translation.wrapper_phase == MC_REGISTRY_FAILED && source.event_count == events);
    result = 0;
cleanup:
    if (!fixture_free(&f))
        return 1;
    source_retire(&source);
    if (source.invalid || (result == 0 && source.retired_streams != 1))
        result = 1;
    return result;
}

static int test_arguments_and_callbacks(void) {
    int result = 1;
    fixture f = {0};
    memory_source source;
    source_init(&source);
    mc_registry_resources api = source.api;
    api.close = NULL;
    REQUIRE(fixture_create(&f, &api));
    mc_client_assets *unchanged = f.assets;
    REQUIRE(mc_client_assets_create(NULL, f.classes, &unchanged) ==
                MC_CLIENT_ASSETS_INVALID_ARGUMENT &&
            unchanged == f.assets);
    REQUIRE(mc_client_assets_create(&api, NULL, &unchanged) == MC_CLIENT_ASSETS_INVALID_ARGUMENT &&
            unchanged == f.assets);
    REQUIRE(mc_client_assets_create(&api, f.classes, NULL) == MC_CLIENT_ASSETS_INVALID_ARGUMENT);
    const mc_image *image = NULL;
    mc_font_text text = {0};
    mc_client_assets_error error;
    REQUIRE(mc_client_assets_image(f.assets, "/tiny.png", &image, &error) ==
                MC_CLIENT_ASSETS_RESOURCE_FAILED &&
            image == NULL && error.resource_status == MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY &&
            source.opens == 0);
    REQUIRE(mc_client_assets_font_txt(f.assets, &text, &error) ==
                MC_CLIENT_ASSETS_RESOURCE_FAILED &&
            error.resource_status == MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY && source.opens == 0);
    api.close = memory_close;
    api.open = NULL;
    REQUIRE(mc_client_assets_image(f.assets, "/tiny.png", &image, &error) ==
                MC_CLIENT_ASSETS_RESOURCE_FAILED &&
            error.resource_status == MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY && source.opens == 0);
    REQUIRE(mc_client_assets_image(NULL, "/tiny.png", &image, NULL) ==
            MC_CLIENT_ASSETS_INVALID_ARGUMENT);
    REQUIRE(mc_client_assets_image(f.assets, "/tiny.png", NULL, NULL) ==
            MC_CLIENT_ASSETS_INVALID_ARGUMENT);
    REQUIRE(mc_client_assets_label(f.assets, NULL, &text, NULL) ==
            MC_CLIENT_ASSETS_INVALID_ARGUMENT);
    REQUIRE(mc_client_assets_label(NULL, "key", &text, NULL) == MC_CLIENT_ASSETS_INVALID_ARGUMENT);
    REQUIRE(mc_client_assets_label(f.assets, "key", NULL, NULL) ==
            MC_CLIENT_ASSETS_INVALID_ARGUMENT);
    REQUIRE(mc_client_assets_font_txt(NULL, &text, NULL) == MC_CLIENT_ASSETS_INVALID_ARGUMENT);
    REQUIRE(mc_client_assets_font_txt(f.assets, NULL, NULL) == MC_CLIENT_ASSETS_INVALID_ARGUMENT);
    mc_client_string selected = {0};
    mc_java_random random;
    mc_java_random_seed(&random, 0);
    REQUIRE(mc_client_assets_choose_splash(NULL, &random, &selected, NULL) ==
            MC_CLIENT_ASSETS_INVALID_ARGUMENT);
    REQUIRE(mc_client_assets_choose_splash(f.assets, NULL, &selected, NULL) ==
            MC_CLIENT_ASSETS_INVALID_ARGUMENT);
    REQUIRE(mc_client_assets_choose_splash(f.assets, &random, NULL, NULL) ==
            MC_CLIENT_ASSETS_INVALID_ARGUMENT);
    REQUIRE(source.opens == 0 && source.event_count == 0);
    mc_registry_translation_view translation;
    REQUIRE(mc_registry_classes_inspect_translation(f.classes, &translation, NULL) ==
                MC_REGISTRY_OK &&
            translation.singleton_phase == MC_REGISTRY_COLD &&
            translation.wrapper_phase == MC_REGISTRY_COLD);
    result = 0;
cleanup:
    if (!fixture_free(&f))
        return 1;
    source_retire(&source);
    if (source.invalid || source.retired_streams != 0)
        result = 1;
    return result;
}

static int test_splashes(void) {
    int result = 1;
    fixture f = {0};
    memory_source source;
    source_init(&source);
    mc_client_string selected = {0};
    uint16_t canary[] = {'s', 'a', 'f', 'e'};
    /* Four retained entries alpha,beta,alpha,delta: duplicate/order changes
     * change the literal seed2 selection or its subsequent Random sequence. */
    static const uint8_t list[] = " \talpha\x1f\r\n\tbeta \nalpha\rdelta\n\t \n";
    memory_entry *entry = add(&source, "/title/splashes.txt", list, sizeof(list) - 1u);
    REQUIRE(entry != NULL && fixture_create(&f, &source.api));
    entry->chunk = 2;
    mc_java_random random, expected;
    mc_java_random_seed(&random, 2);
    mc_java_random_seed(&expected, 2);
    int32_t index = -1;
    REQUIRE(mc_java_random_next_bounded(&expected, 4, &index) && index == 2);
    mc_client_assets_error error;
    REQUIRE(mc_client_assets_choose_splash(f.assets, &random, &selected, &error) ==
                MC_CLIENT_ASSETS_OK &&
            ascii_is((mc_font_text){selected.units, selected.length}, "alpha") &&
            error_clear(&error));
    REQUIRE(same_next(random, expected));
    REQUIRE(source.event_count >= 2u && source.events[source.event_count - 2u].kind == 'E' &&
            source.events[source.event_count - 1u].kind == 'C');
    REQUIRE(source.streams[0].closed && source.streams[0].closes == 1);
    const uint16_t *owned = selected.units;
    entry->bytes = (const uint8_t *)"changed";
    entry->length = 7;
    REQUIRE(ascii_is((mc_font_text){owned, selected.length}, "alpha"));
    mc_client_string_free(&selected);
    REQUIRE(selected.units == NULL && selected.length == 0);
    static const uint8_t nbsp[] = {0xc2, 0xa0, 'X', 0xc2, 0xa0, '\n'};
    static const uint16_t nbsp_expected[] = {0x00a0, 'X', 0x00a0};
    entry->bytes = nbsp;
    entry->length = sizeof(nbsp);
    mc_java_random_seed(&random, 0);
    mc_java_random_seed(&expected, 0);
    REQUIRE(mc_java_random_next_bounded(&expected, 1, &index));
    REQUIRE(mc_client_assets_choose_splash(f.assets, &random, &selected, NULL) ==
                MC_CLIENT_ASSETS_OK &&
            text_is((mc_font_text){selected.units, selected.length}, nbsp_expected,
                    COUNT(nbsp_expected)) &&
            same_next(random, expected));
    mc_client_string_free(&selected);
    /* No complete EOF: even a completed first line must not trigger a draw. */
    entry->bytes = (const uint8_t *)"first\nsecond";
    entry->length = 12;
    entry->chunk = 1;
    entry->fail_offset = 8;
    mc_java_random_seed(&random, 23);
    expected = random;
    REQUIRE(mc_client_assets_choose_splash(f.assets, &random, &selected, NULL) ==
                MC_CLIENT_ASSETS_OK &&
            ascii_is((mc_font_text){selected.units, selected.length}, "missingno") &&
            same_next(random, expected));
    mc_client_string_free(&selected);
    /* A failed EOF probe after the final newline is still not complete EOF. */
    entry->bytes = (const uint8_t *)"first\n";
    entry->length = 6;
    entry->fail_offset = 6;
    REQUIRE(mc_client_assets_choose_splash(f.assets, &random, &selected, NULL) ==
                MC_CLIENT_ASSETS_OK &&
            ascii_is((mc_font_text){selected.units, selected.length}, "missingno") &&
            same_next(random, expected));
    mc_client_string_free(&selected);
    entry->fail_offset = SIZE_MAX;
    entry->bytes = (const uint8_t *)" \t\r\n";
    entry->length = 4;
    REQUIRE(mc_client_assets_choose_splash(f.assets, &random, &selected, NULL) ==
                MC_CLIENT_ASSETS_OK &&
            ascii_is((mc_font_text){selected.units, selected.length}, "missingno") &&
            same_next(random, expected));
    mc_client_string_free(&selected);
    entry->open_status = MC_REGISTRY_NULL_REFERENCE;
    const size_t closes = source.closes;
    REQUIRE(mc_client_assets_choose_splash(f.assets, &random, &selected, NULL) ==
                MC_CLIENT_ASSETS_OK &&
            ascii_is((mc_font_text){selected.units, selected.length}, "missingno") &&
            same_next(random, expected) && source.closes == closes);
    mc_client_string_free(&selected);
    entry->open_status = MC_REGISTRY_OK;
    entry->bytes = (const uint8_t *)"first\n";
    entry->length = 6;
    entry->close_status = MC_REGISTRY_IO_FAILED;
    selected = (mc_client_string){canary, COUNT(canary)};
    REQUIRE(mc_client_assets_choose_splash(f.assets, &random, &selected, &error) ==
                MC_CLIENT_ASSETS_READER_FAILED &&
            selected.units == canary && selected.length == COUNT(canary) &&
            error.reader_status == MC_JAVA_READER_IO_ERROR &&
            error.close_status == MC_REGISTRY_IO_FAILED && same_next(random, expected));
    selected = (mc_client_string){0};
    REQUIRE(source.closes == source.stream_count);
    result = 0;
cleanup:
    /* canary is never passed to the owned-string destructor on failure. */
    if (selected.units != NULL && selected.units != canary)
        mc_client_string_free(&selected);
    if (!fixture_free(&f))
        return 1;
    source_retire(&source);
    if (source.invalid || source.retired_streams != 0)
        result = 1;
    return result;
}

/* Only this test-created directory and its listed children are ever retired.
 * Windows converts the actual temporary path to UTF-8 for AppFileSource. */
typedef struct temporary_directory {
    char path[4096];
    bool created, lang, title;
} temporary_directory;

#ifdef _WIN32
static bool native_path(const char *utf8, wchar_t *out, int capacity) {
    return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8, -1, out, capacity) != 0;
}
#endif

static bool directory_create(const char *path) {
#ifdef _WIN32
    wchar_t native[4096];
    return native_path(path, native, (int)COUNT(native)) && CreateDirectoryW(native, NULL);
#else
    return mkdir(path, 0700) == 0;
#endif
}

static bool directory_remove(const char *path) {
#ifdef _WIN32
    wchar_t native[4096];
    return native_path(path, native, (int)COUNT(native)) && RemoveDirectoryW(native);
#else
    return rmdir(path) == 0;
#endif
}

static bool file_remove(const char *path) {
#ifdef _WIN32
    wchar_t native[4096];
    return native_path(path, native, (int)COUNT(native)) && DeleteFileW(native);
#else
    return unlink(path) == 0;
#endif
}

static bool path_join(const temporary_directory *directory, const char *name, char *out,
                      size_t capacity) {
    const int size = snprintf(out, capacity, "%s/%s", directory->path, name);
    return size >= 0 && (size_t)size < capacity;
}

static bool temporary_create(temporary_directory *directory) {
#ifdef _WIN32
    wchar_t base[MAX_PATH], path[4096];
    const DWORD count = GetTempPathW((DWORD)COUNT(base), base);
    if (count == 0 || count >= COUNT(base))
        return false;
    bool created = false;
    for (unsigned int attempt = 0; attempt < 64u; ++attempt) {
        const int size = swprintf(path, COUNT(path), L"%lsmc-assets-%lu-%u", base,
                                  (unsigned long)GetCurrentProcessId(), attempt);
        if (size < 0 || (size_t)size >= COUNT(path))
            return false;
        if (CreateDirectoryW(path, NULL)) {
            created = true;
            break;
        }
        if (GetLastError() != ERROR_ALREADY_EXISTS)
            return false;
    }
    if (!created)
        return false;
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, path, -1, directory->path,
                            (int)sizeof(directory->path), NULL, NULL) == 0) {
        (void)RemoveDirectoryW(path);
        return false;
    }
#else
    static const char pattern[] = "/tmp/mc-assets-XXXXXX";
    memcpy(directory->path, pattern, sizeof(pattern));
    if (mkdtemp(directory->path) == NULL)
        return false;
#endif
    directory->created = true;
    return true;
}

static bool file_write(const char *path, const void *bytes, size_t length) {
    FILE *file = NULL;
#ifdef _WIN32
    wchar_t native[4096];
    if (!native_path(path, native, (int)COUNT(native)))
        return false;
#ifdef _MSC_VER
    if (_wfopen_s(&file, native, L"wb") != 0)
        return false;
#else
    file = _wfopen(native, L"wb");
#endif
#else
    file = fopen(path, "wb");
#endif
    if (file == NULL)
        return false;
    const bool written = fwrite(bytes, 1, length, file) == length;
    return fclose(file) == 0 && written;
}

static int test_app_file_source(void) {
    int result = 1;
    fixture f = {0};
    mc_resource_files *files = NULL;
    temporary_directory directory = {0};
    png_buffer rgba;
    char path[4096];
    const char *const names[] = {"lang/en_US.lang", "lang/stats_US.lang", "font.txt",
                                 "title/splashes.txt", "tiny.png"};
    bool written[COUNT(names)] = {0};
    mc_client_string selected = {0};
    REQUIRE(temporary_create(&directory));
    REQUIRE(path_join(&directory, "lang", path, sizeof(path)) && directory_create(path));
    directory.lang = true;
    REQUIRE(path_join(&directory, "title", path, sizeof(path)) && directory_create(path));
    directory.title = true;
    REQUIRE(png_make(&rgba, 2, 8, 6, rgba_row, sizeof(rgba_row)));
    static const char authored_font[] = "#authored\r\n AB\n", authored_splash[] = " alpha\r\n";
    const void *const bytes[] = {english, statistics, authored_font, authored_splash, rgba.bytes};
    const size_t lengths[] = {sizeof(english) - 1u, sizeof(statistics) - 1u,
                              sizeof(authored_font) - 1u, sizeof(authored_splash) - 1u,
                              rgba.length};
    for (size_t i = 0; i < COUNT(names); ++i) {
        REQUIRE(path_join(&directory, names[i], path, sizeof(path)));
        written[i] = true; /* A short write still owns the newly created file. */
        REQUIRE(file_write(path, bytes[i], lengths[i]));
    }
    REQUIRE(mc_resource_files_create(directory.path, &files) == MC_REGISTRY_OK);
    const mc_registry_resources *source = mc_resource_files_source(files);
    REQUIRE(fixture_create(&f, source));
    mc_font_text label = {0}, font = {0};
    REQUIRE(mc_client_assets_label(f.assets, "shared", &label, NULL) == MC_CLIENT_ASSETS_OK &&
            ascii_is(label, "second"));
    const mc_image *image = NULL;
    REQUIRE(mc_client_assets_image(f.assets, "/tiny.png", &image, NULL) == MC_CLIENT_ASSETS_OK &&
            image_pixels(image));
    REQUIRE(mc_client_assets_font_txt(f.assets, &font, NULL) == MC_CLIENT_ASSETS_OK &&
            ascii_is(font, "#authored\n AB\n"));
    mc_java_random random, expected;
    mc_java_random_seed(&random, 23);
    expected = random;
    int32_t index;
    REQUIRE(mc_java_random_next_bounded(&expected, 1, &index));
    REQUIRE(mc_client_assets_choose_splash(f.assets, &random, &selected, NULL) ==
                MC_CLIENT_ASSETS_OK &&
            ascii_is((mc_font_text){selected.units, selected.length}, "alpha") &&
            same_next(random, expected));
    mc_client_assets_free(f.assets);
    f.assets = NULL;
    /* The actual source still opens/reads after its assets borrower retires. */
    mc_java_properties_input input;
    REQUIRE(source->open(source->context, "/tiny.png", &input) == MC_REGISTRY_OK);
    uint8_t signature[8];
    size_t count = 0;
    REQUIRE(input.read(input.context, signature, sizeof(signature), &count) ==
                MC_JAVA_PROPERTIES_OK &&
            count == sizeof(signature) && memcmp(signature, rgba.bytes, sizeof(signature)) == 0);
    REQUIRE(source->close(source->context, &input) == MC_REGISTRY_OK);
    count = 3;
    REQUIRE(input.read(input.context, signature, sizeof(signature), &count) ==
                MC_JAVA_PROPERTIES_IO_ERROR &&
            count == 0);
    REQUIRE(source->close(source->context, &input) == MC_REGISTRY_OK);
    result = 0;
cleanup:
    mc_client_string_free(&selected);
    if (!fixture_free(&f))
        return 1; /* Do not retire a source whose Registry refused retirement. */
    if (mc_resource_files_free(files) != MC_REGISTRY_OK)
        result = 1;
    for (size_t i = 0; i < COUNT(names); ++i)
        if (written[i] &&
            (!path_join(&directory, names[i], path, sizeof(path)) || !file_remove(path)))
            result = 1;
    if (directory.title &&
        (!path_join(&directory, "title", path, sizeof(path)) || !directory_remove(path)))
        result = 1;
    if (directory.lang &&
        (!path_join(&directory, "lang", path, sizeof(path)) || !directory_remove(path)))
        result = 1;
    if (directory.created && !directory_remove(directory.path))
        result = 1;
    return result;
}

int main(void) {
    if (test_labels() || test_translation_routes() || test_counted_labels() || test_null_labels() ||
        test_label_cache_bound() || test_images() || test_font() || test_splashes() ||
        test_failed_translation() || test_arguments_and_callbacks() || test_app_file_source())
        return 1;
    mc_client_assets_free(NULL);
    mc_client_string_free(NULL);
    printf("PASS: %u Client assets native-contract checks\n", checks);
    return 0;
}
