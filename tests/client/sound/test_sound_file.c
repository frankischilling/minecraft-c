#include "mc/client/sound/sound_file.h"
#include "mc/client/sound/sound_pool.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <share.h>
#include <windows.h>
#endif

static unsigned int checks;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "sound File check failed at line %d: %s\n", __LINE__, #condition);     \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)

static char *path(const char *root, const char *name) {
    const size_t size = strlen(root) + strlen(name) + 2u;
    char *value = malloc(size);
    CHECK(value != NULL);
    const int count = snprintf(value, size, "%s/%s", root, name);
    CHECK(count >= 0 && (size_t)count + 1u == size);
    return value;
}

static mc_sound_file *resolve(const char *name) {
    mc_sound_file *file = NULL;
    mc_sound_error error;
    CHECK(mc_sound_file_resolve(name, &file, &error) == MC_SOUND_OK);
    CHECK(file != NULL && error.status == MC_SOUND_OK && error.operation == NULL);
    return file;
}

static void suffix(mc_sound_file *file, const uint16_t *expected, size_t length) {
    mc_sound_file_identity identity;
    CHECK(mc_sound_file_read_identity(file, &identity));
    CHECK(!identity.url_path.is_null && identity.url_path.length >= length);
    CHECK(memcmp(identity.url_path.units + identity.url_path.length - length, expected,
                 length * sizeof(*expected)) == 0);
}

static void url_paths(const char *root) {
    char *name = path(root, "missing #?%[]{}|^`<>\".ogg");
    mc_sound_file *file = resolve(name);
    static const uint16_t quoted[] = {
        '/', 'm', 'i', 's', 's', 'i', 'n', 'g', '%', '2', '0', '%', '2', '3', '%', '3', 'F', '%',
        '2', '5', '%', '5', 'B', '%', '5', 'D', '%', '7', 'B', '%', '7', 'D', '%', '7', 'C', '%',
        '5', 'E', '%', '6', '0', '%', '3', 'C', '%', '3', 'E', '%', '2', '2', '.', 'o', 'g', 'g'};
    suffix(file, quoted, sizeof(quoted) / sizeof(*quoted));
    mc_sound_file_free(file);
    free(name);

    name = path(root, "caf\xc3\xa9-\xf0\x9f\x9a\x80-\xc2\xa0-\xe2\x80\xa8.ogg");
    file = resolve(name);
    static const uint16_t unicode[] = {'/', 'c', 'a', 'f', 0xe9, '-', 0xd83d, 0xde80, '-', '%',
                                       'C', '2', '%', 'A', '0',  '-', '%',    'E',    '2', '%',
                                       '8', '0', '%', 'A', '8',  '.', 'o',    'g',    'g'};
    suffix(file, unicode, sizeof(unicode) / sizeof(*unicode));
    mc_sound_file_free(file);
    free(name);

    name = path(root, "a//./b/../not-present.ogg/");
    file = resolve(name);
    static const uint16_t dots[] = {'/', 'a', '/', '.', '/', 'b', '/', '.', '.', '/', 'n', 'o', 't',
                                    '-', 'p', 'r', 'e', 's', 'e', 'n', 't', '.', 'o', 'g', 'g'};
    suffix(file, dots, sizeof(dots) / sizeof(*dots));
    mc_sound_file_free(file);
    free(name);

    file = resolve(root);
    static const uint16_t slash[] = {'/'};
    suffix(file, slash, 1u);
    mc_sound_file_free(file);

    name = path(root, "-_.!~*'():@&=+$,;.ogg");
    file = resolve(name);
    static const uint16_t allowed[] = {'/', '-', '_', '.', '!', '~', '*', '\'', '(', ')', ':',
                                       '@', '&', '=', '+', '$', ',', ';', '.',  'o', 'g', 'g'};
    suffix(file, allowed, sizeof(allowed) / sizeof(*allowed));
    mc_sound_file_free(file);
    free(name);
}

static const uint8_t payload[] = {0, 1, 127, 128, 255, 79, 103, 103, 83, 0, 2, 42, 0, 255, 128, 64};

static FILE *writer(const char *name) {
#ifdef _WIN32
    const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name, -1, NULL, 0);
    CHECK(count > 0);
    wchar_t *wide = malloc((size_t)count * sizeof(*wide));
    CHECK(wide != NULL);
    CHECK(MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name, -1, wide, count) == count);
    FILE *file = _wfsopen(wide, L"r+b", _SH_DENYNO);
    free(wide);
#else
    FILE *file = fopen(name, "r+b");
#endif
    CHECK(file != NULL);
    return file;
}

static mc_sound_stream *open_stream(const char *root, const char *leaf, bool mus, size_t limit) {
    char *name = path(root, leaf);
    mc_sound_file *file = resolve(name);
    mc_sound_stream *stream = NULL;
    mc_sound_error error;
    CHECK(mc_sound_stream_open(file, mus, limit, &stream, &error) == MC_SOUND_OK);
    CHECK(stream != NULL && error.status == MC_SOUND_OK);
    /* Actual stream lifetime is independent of the resolved identity. */
    mc_sound_file_free(file);
    free(name);
    return stream;
}

static void streams(const char *root) {
    for (size_t block = 1; block <= sizeof(payload) + 3u; ++block) {
        mc_sound_stream *stream = open_stream(root, "stream sample.mus", true, sizeof(payload));
        mc_sound_error error;
        CHECK(mc_sound_stream_close(stream, &error) == MC_SOUND_OK);
        size_t position = 0;
        while (position < sizeof(payload)) {
            uint8_t actual[32];
            memset(actual, 0xa5, sizeof(actual));
            size_t count = 999u;
            CHECK(mc_sound_stream_read(stream, actual, block, &count, &error) == MC_SOUND_OK);
            const size_t remaining = sizeof(payload) - position;
            const size_t expected = remaining < block ? remaining : block;
            CHECK(count == expected && memcmp(actual, payload + position, count) == 0);
            CHECK(actual[count] == 0xa5);
            position += count;
        }
        uint8_t untouched = 0xa5;
        size_t count = 999u;
        CHECK(mc_sound_stream_read(stream, &untouched, 1u, &count, &error) == MC_SOUND_OK);
        CHECK(count == 0 && untouched == 0xa5);
        CHECK(mc_sound_stream_free(stream, &error) == MC_SOUND_OK);
    }

    for (unsigned int mode = 0; mode < 2u; ++mode) {
        const bool mus = mode == 1u;
        mc_sound_stream *stream = open_stream(root, mus ? "stream sample.mus" : "plain.bin", mus,
                                              MC_SOUND_STREAM_MAX_BYTES);
        for (size_t i = 0; i < sizeof(payload); ++i) {
            int32_t value = -999;
            bool eof = true;
            CHECK(mc_sound_stream_read_byte(stream, &value, &eof, NULL) == MC_SOUND_OK);
            const int32_t expected =
                mus && payload[i] >= 128u ? (int32_t)payload[i] - 256 : (int32_t)payload[i];
            CHECK(value == expected && !eof);
        }
        int32_t value = -999;
        bool eof = false;
        CHECK(mc_sound_stream_read_byte(stream, &value, &eof, NULL) == MC_SOUND_OK);
        CHECK(value == -1 && eof);
        CHECK(mc_sound_stream_read_byte(stream, &value, &eof, NULL) == MC_SOUND_OK);
        CHECK(value == -1 && eof);
        CHECK(mc_sound_stream_close(stream, NULL) == MC_SOUND_OK);
        CHECK(mc_sound_stream_close(stream, NULL) == MC_SOUND_OK);
        CHECK(mc_sound_stream_free(stream, NULL) == MC_SOUND_OK);
    }

    mc_sound_stream *stream = open_stream(root, "plain.bin", false, sizeof(payload));
    size_t count = 17u;
    uint8_t byte = 0xa5;
    CHECK(mc_sound_stream_read(stream, NULL, 0, &count, NULL) == MC_SOUND_OK && count == 0);
    CHECK(mc_sound_stream_read(stream, NULL, 1u, &count, NULL) == MC_SOUND_INVALID_ARGUMENT);
    CHECK(count == 0 && byte == 0xa5);
    CHECK(mc_sound_stream_read(stream, &byte, 1u, NULL, NULL) == MC_SOUND_INVALID_ARGUMENT);
    CHECK(mc_sound_stream_close(stream, NULL) == MC_SOUND_OK);
    count = 99u;
    CHECK(mc_sound_stream_read(stream, &byte, 1u, &count, NULL) == MC_SOUND_IO_FAILED);
    CHECK(count == 99u && byte == 0xa5);
    CHECK(mc_sound_stream_free(stream, NULL) == MC_SOUND_OK);

    /* The source file adapter shares with writers and has no hidden stdio
     * read-ahead. Restore the authored input before the outer file audit. */
    stream = open_stream(root, "plain.bin", false, sizeof(payload));
    CHECK(mc_sound_stream_read(stream, &byte, 1u, &count, NULL) == MC_SOUND_OK && byte == 0);
    char *name = path(root, "plain.bin");
    FILE *output = writer(name);
    free(name);
    CHECK(fseek(output, 1, SEEK_SET) == 0 && fputc(91, output) == 91 && fflush(output) == 0);
    CHECK(mc_sound_stream_read(stream, &byte, 1u, &count, NULL) == MC_SOUND_OK);
    CHECK(count == 1u && byte == 91);
    CHECK(fseek(output, 1, SEEK_SET) == 0 && fputc(1, output) == 1 && fflush(output) == 0);
    CHECK(fclose(output) == 0);
    CHECK(mc_sound_stream_free(stream, NULL) == MC_SOUND_OK);
}

static void rejections(const char *root) {
    mc_sound_error error;
    mc_sound_file *file = NULL;
    CHECK(mc_sound_file_resolve(NULL, &file, &error) == MC_SOUND_SOURCE_EXCEPTION);
    CHECK(file == NULL && error.exception == MC_SOUND_EXCEPTION_NULL_FILE);
    CHECK(mc_sound_file_resolve(root, NULL, &error) == MC_SOUND_INVALID_ARGUMENT);
    CHECK(mc_sound_file_resolve("relative.ogg", &file, &error) == MC_SOUND_UNSUPPORTED_PATH);
    char *invalid = path(root, "\xc0\xaf.ogg");
    CHECK(mc_sound_file_resolve(invalid, &file, &error) == MC_SOUND_UNSUPPORTED_PATH);
    CHECK(file == NULL);
    free(invalid);
    char *name = path(root, "plain.bin");
    file = resolve(name);
    free(name);
    mc_sound_stream *stream = NULL;
    CHECK(mc_sound_stream_open(file, false, sizeof(payload) - 1u, &stream, &error) ==
          MC_SOUND_LIMIT);
    CHECK(stream == NULL);
    CHECK(mc_sound_stream_open(file, false, 0, &stream, &error) == MC_SOUND_INVALID_ARGUMENT);
    CHECK(mc_sound_stream_open(file, false, MC_SOUND_STREAM_MAX_BYTES + 1u, &stream, &error) ==
          MC_SOUND_INVALID_ARGUMENT);
    mc_sound_file_identity identity;
    CHECK(mc_sound_file_read_identity(file, &identity));
    identity.url_path.is_null = true;
    CHECK(mc_sound_stream_open_identity(&identity, true, 100u, &stream, &error) ==
              MC_SOUND_INVALID_ARGUMENT &&
          stream == NULL);
    identity.url_path.is_null = false;
    identity.url_path.length = 3u * MC_SOUND_FILE_MAX_PATH_BYTES + 6u;
    CHECK(mc_sound_stream_open_identity(&identity, true, 100u, &stream, &error) == MC_SOUND_LIMIT &&
          stream == NULL);
    mc_sound_file_free(file);
    file = resolve(root);
    CHECK(mc_sound_stream_open(file, false, 100u, &stream, &error) == MC_SOUND_IO_FAILED);
    CHECK(stream == NULL);
    mc_sound_file_free(file);
    name = path(root, "not-present.ogg");
    file = resolve(name);
    free(name);
    CHECK(mc_sound_stream_open(file, false, 100u, &stream, &error) == MC_SOUND_IO_FAILED);
    CHECK(stream == NULL);
    mc_sound_file_free(file);
    CHECK(!mc_sound_file_read_identity(NULL, NULL));
    CHECK(mc_sound_stream_free(NULL, &error) == MC_SOUND_OK);
    mc_sound_file_free(NULL);
}

static void selected_pool_file(const char *root) {
    mc_java_random_profile *profile = NULL;
    CHECK(mc_java_random_profile_create(1048576u, &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    mc_sound_pool *pool = NULL;
    CHECK(mc_sound_pool_create(profile, MC_SOUND_POOL_MAX_ALLOCATION_BYTES, &pool, NULL) ==
          MC_SOUND_OK);
    static const uint16_t name[] = {'d', 'i', 'r', '/', 'c', 'l', 'i',
                                    'c', 'k', '1', '.', 'o', 'g', 'g'};
    static const uint16_t key[] = {'d', 'i', 'r', '.', 'c', 'l', 'i', 'c', 'k'};
    const mc_sound_pool_entry *entry = NULL, *selected = NULL;
    char *filename = path(root, "stream sample.mus");
    CHECK(mc_sound_pool_add_file(pool, (mc_sound_text){name, sizeof(name) / sizeof(*name), false},
                                 filename, &entry, NULL, NULL) == MC_SOUND_OK);
    free(filename);
    CHECK(mc_sound_pool_select(pool, (mc_sound_text){key, sizeof(key) / sizeof(*key), false},
                               &selected, NULL, NULL) == MC_SOUND_OK &&
          selected == entry);
    mc_sound_pool_entry_view view;
    CHECK(mc_sound_pool_entry_read(selected, &view, NULL) == MC_SOUND_OK);
    mc_sound_stream *stream = NULL;
    CHECK(mc_sound_stream_open_identity(&view.file, true, sizeof(payload), &stream, NULL) ==
          MC_SOUND_OK);
    CHECK(mc_sound_pool_free(pool, NULL) == MC_SOUND_OK);
    mc_java_random_profile_free(profile);
    uint8_t actual[sizeof(payload)];
    size_t count;
    CHECK(mc_sound_stream_read(stream, actual, sizeof(actual), &count, NULL) == MC_SOUND_OK &&
          count == sizeof(payload) && memcmp(actual, payload, sizeof(payload)) == 0);
    CHECK(mc_sound_stream_free(stream, NULL) == MC_SOUND_OK);
}

int main(int argc, char **argv) {
    CHECK(argc == 2);
    url_paths(argv[1]);
    streams(argv[1]);
    rejections(argv[1]);
    selected_pool_file(argv[1]);
    printf("PASS: %u sound File interface checks\n", checks);
    return 0;
}
