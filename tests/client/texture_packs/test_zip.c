#include "zip.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t checks;
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "ZIP check:%d: %s\n", __LINE__, #condition);                           \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)

static texture_zip_limits normal_limits(void) {
    return (texture_zip_limits){.max_archive_bytes = 1048576,
                                .max_entries = 64,
                                .max_encoded_entry_bytes = 65536,
                                .max_decoded_entry_bytes = 65536,
                                .max_stream_allocation_bytes = 262144};
}

static texture_zip_status open_archive(const char *directory, const char *name,
                                       const texture_zip_limits *limits, texture_zip **out,
                                       texture_zip_error *error) {
    char path[4096];
    const int length = snprintf(path, sizeof(path), "%s/%s.zip", directory, name);
    CHECK(length > 0 && (size_t)length < sizeof(path));
    FILE *file = fopen(path, "rb");
    CHECK(file != NULL);
    return texture_zip_open(file, limits, out, error);
}

static texture_zip_stream *entry(texture_zip *zip, const char *name) {
    texture_zip_stream *stream = NULL;
    texture_zip_error error = {0};
    CHECK(texture_zip_lookup(zip, (const uint8_t *)name, strlen(name), &stream, &error) ==
          TEXTURE_ZIP_OK);
    CHECK(error.status == TEXTURE_ZIP_OK && stream != NULL);
    return stream;
}

static void read_part(texture_zip_stream *stream, size_t capacity, const char *expected) {
    uint8_t bytes[32] = {0};
    CHECK(capacity <= sizeof(bytes));
    size_t count = SIZE_MAX;
    texture_zip_error error = {0};
    CHECK(texture_zip_read(stream, bytes, capacity, &count, &error) == TEXTURE_ZIP_OK);
    CHECK(error.status == TEXTURE_ZIP_OK && count == strlen(expected));
    CHECK(memcmp(bytes, expected, count) == 0);
}

static void normal_case(const char *directory, const char *name, const char *expected,
                        size_t count) {
    const texture_zip_limits limits = normal_limits();
    texture_zip *zip = NULL;
    texture_zip_error error = {0};
    CHECK(open_archive(directory, name, &limits, &zip, &error) == TEXTURE_ZIP_OK);
    CHECK(zip != NULL && texture_zip_count(zip) == count);
    texture_zip_entry record = {0};
    CHECK(texture_zip_entry_at(zip, 0, &record));
    CHECK(record.name_bytes >= 5 && memcmp(record.name, "a.txt", 5) == 0);
    CHECK(!texture_zip_entry_at(zip, count, &record));
    texture_zip_stream *missing = (texture_zip_stream *)(uintptr_t)1;
    CHECK(texture_zip_lookup(zip, (const uint8_t *)"absent", 6, &missing, &error) ==
          TEXTURE_ZIP_OK);
    CHECK(missing == NULL);
    texture_zip_stream *stream = entry(zip, "a.txt");
    CHECK(texture_zip_available(stream) == strlen(expected));
    read_part(stream, 0, "");
    CHECK(texture_zip_available(stream) == strlen(expected));
    read_part(stream, 32, expected);
    read_part(stream, 32, "");
    CHECK(texture_zip_available(stream) == 0);
    CHECK(texture_zip_stream_release(stream, &error) == TEXTURE_ZIP_OK);
    CHECK(texture_zip_release(zip, &error) == TEXTURE_ZIP_OK);
}

static void lifetime_case(const char *directory, const char *name, bool deflated) {
    const texture_zip_limits limits = normal_limits();
    texture_zip *zip = NULL;
    texture_zip_error error = {0};
    CHECK(open_archive(directory, name, &limits, &zip, &error) == TEXTURE_ZIP_OK);
    texture_zip_stream *first = entry(zip, "a.txt"), *second = entry(zip, "pack.txt");
    read_part(first, 2, "fi");
    CHECK(texture_zip_available(first) == 3);
    read_part(second, 3, "one");
    /* Both streams keep the actual archive alive after its caller hold ends. */
    CHECK(texture_zip_release(zip, &error) == TEXTURE_ZIP_OK);
    read_part(first, 8, "rst");
    read_part(second, 8, "\r\ntwo\n");
    CHECK(texture_zip_stream_close(first, &error) == TEXTURE_ZIP_OK);
    CHECK(texture_zip_stream_close(first, &error) == TEXTURE_ZIP_OK);
    if (deflated) {
        size_t count = SIZE_MAX;
        uint8_t byte = 0;
        CHECK(texture_zip_read(first, &byte, 0, &count, &error) == TEXTURE_ZIP_SOURCE_IO);
        CHECK(count == SIZE_MAX);
        CHECK(texture_zip_read(first, &byte, 1, &count, &error) == TEXTURE_ZIP_SOURCE_IO);
    } else {
        read_part(first, 0, "");
        read_part(first, 1, "");
    }
    CHECK(texture_zip_stream_release(first, &error) == TEXTURE_ZIP_OK);
    CHECK(texture_zip_stream_release(second, &error) == TEXTURE_ZIP_OK);

    CHECK(open_archive(directory, name, &limits, &zip, &error) == TEXTURE_ZIP_OK);
    first = entry(zip, "a.txt");
    CHECK(texture_zip_close(zip, &error) == TEXTURE_ZIP_OK);
    CHECK(texture_zip_available(first) == 0);
    size_t count = SIZE_MAX;
    uint8_t byte = 0;
    CHECK(texture_zip_read(first, &byte, 0, &count, &error) == TEXTURE_ZIP_SOURCE_IO);
    CHECK(texture_zip_read(first, &byte, 1, &count, &error) == TEXTURE_ZIP_SOURCE_IO);
    CHECK(count == SIZE_MAX);
    texture_zip_stream *unpublished = (texture_zip_stream *)(uintptr_t)1;
    CHECK(texture_zip_lookup(zip, (const uint8_t *)"a.txt", 5, &unpublished, &error) ==
          TEXTURE_ZIP_SOURCE_IO);
    CHECK(unpublished == (texture_zip_stream *)(uintptr_t)1);
    CHECK(texture_zip_release(zip, &error) == TEXTURE_ZIP_OK);
    CHECK(texture_zip_stream_release(first, &error) == TEXTURE_ZIP_OK);
}

static void failed_read(const char *directory, const char *name) {
    const texture_zip_limits limits = normal_limits();
    texture_zip *zip = NULL;
    texture_zip_error error = {0};
    /* Source ZipFile accepts the CEN and getEntry/getInputStream before the
     * first actual read reaches a malformed local header/deflate payload. */
    CHECK(open_archive(directory, name, &limits, &zip, &error) == TEXTURE_ZIP_OK);
    texture_zip_stream *stream = entry(zip, "a.txt");
    size_t count = SIZE_MAX;
    uint8_t bytes[32] = {0};
    CHECK(texture_zip_read(stream, bytes, sizeof(bytes), &count, &error) == TEXTURE_ZIP_SOURCE_IO);
    CHECK(count == SIZE_MAX && error.status == TEXTURE_ZIP_SOURCE_IO);
    CHECK(texture_zip_stream_release(stream, &error) == TEXTURE_ZIP_OK);
    CHECK(texture_zip_release(zip, &error) == TEXTURE_ZIP_OK);
}

static void bounds(const char *directory) {
    texture_zip_limits limits = normal_limits();
    texture_zip_error error = {0};
    texture_zip *zip = (texture_zip *)(uintptr_t)1;
    limits.max_archive_bytes = 1;
    CHECK(open_archive(directory, "stored", &limits, &zip, &error) == TEXTURE_ZIP_LIMIT);
    CHECK(zip == (texture_zip *)(uintptr_t)1);
    limits = normal_limits();
    limits.max_entries = 1;
    CHECK(open_archive(directory, "stored", &limits, &zip, &error) == TEXTURE_ZIP_LIMIT);
    CHECK(zip == (texture_zip *)(uintptr_t)1);
    limits = normal_limits();
    limits.max_decoded_entry_bytes = 4;
    CHECK(open_archive(directory, "stored", &limits, &zip, &error) == TEXTURE_ZIP_OK);
    texture_zip_stream *stream = (texture_zip_stream *)(uintptr_t)1;
    CHECK(texture_zip_lookup(zip, (const uint8_t *)"a.txt", 5, &stream, &error) ==
          TEXTURE_ZIP_LIMIT);
    CHECK(stream == (texture_zip_stream *)(uintptr_t)1);
    CHECK(texture_zip_release(zip, &error) == TEXTURE_ZIP_OK);
    limits = normal_limits();
    limits.max_stream_allocation_bytes = 1;
    CHECK(open_archive(directory, "deflated", &limits, &zip, &error) == TEXTURE_ZIP_OK);
    CHECK(texture_zip_lookup(zip, (const uint8_t *)"a.txt", 5, &stream, &error) ==
          TEXTURE_ZIP_LIMIT);
    CHECK(stream == (texture_zip_stream *)(uintptr_t)1);
    CHECK(texture_zip_release(zip, &error) == TEXTURE_ZIP_OK);
    limits = normal_limits();
    CHECK(open_archive(directory, "bad-central", &limits, &zip, &error) == TEXTURE_ZIP_SOURCE_IO);
    CHECK(error.status == TEXTURE_ZIP_SOURCE_IO);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fputs("ZIP inputs: expected authored fixture directory\n", stderr);
        return 2;
    }
    normal_case(argv[1], "stored", "first", 2);
    normal_case(argv[1], "deflated", "first", 2);
    normal_case(argv[1], "duplicate", "second", 3);
    normal_case(argv[1], "bad-crc", "first", 2);
    normal_case(argv[1], "trailing", "first", 2);
    normal_case(argv[1], "directory", "", 2);
    normal_case(argv[1], "exact-before-directory", "first", 2);
    lifetime_case(argv[1], "stored", false);
    lifetime_case(argv[1], "deflated", true);
    failed_read(argv[1], "bad-local");
    failed_read(argv[1], "bad-deflate");
    bounds(argv[1]);
    printf("texture ZIP: %zu checks\n", checks);
    return 0;
}
