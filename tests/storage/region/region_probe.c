#include "mc/storage/compression/compression.h"
#include "mc/storage/nbt/nbt.h"
#include "mc/storage/region/region.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PROBE_FILE_BYTES (16u * 1024u * 1024u)
#define PROBE_RAW_BYTES (1024u * 1024u)

static char request[32768];

static void print_hex(const uint8_t *bytes, size_t length)
{
    putchar('"');
    for (size_t i = 0; i < length; ++i)
        printf("%02x", (unsigned int)bytes[i]);
    putchar('"');
}

static int coordinate(const char *text, int32_t *out)
{
    char *end = NULL;
    errno = 0;
    long value = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value < INT32_MIN || value > INT32_MAX)
        return 0;
    *out = (int32_t)value;
    return 1;
}

static int load_file(const char *path, uint8_t **bytes, size_t *length)
{
    FILE *file = NULL;
#ifdef _MSC_VER
    if (fopen_s(&file, path, "rb") != 0)
        return 0;
#else
    file = fopen(path, "rb");
#endif
    if (file == NULL)
        return 0;
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return 0;
    }
    long count = ftell(file);
    if (count < 0 || (unsigned long)count > PROBE_FILE_BYTES || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return 0;
    }
    size_t size = (size_t)count;
    uint8_t *data = malloc(size == 0 ? 1 : size);
    if (data == NULL) {
        fclose(file);
        return 0;
    }
    int valid = fread(data, 1, size, file) == size && fgetc(file) == EOF && !ferror(file);
    if (fclose(file) != 0)
        valid = 0;
    if (!valid) {
        free(data);
        return 0;
    }
    *bytes = data;
    *length = size;
    return 1;
}

static int handle_request(char *line)
{
    char *x_text = strchr(line, '\t');
    if (x_text == NULL)
        return 0;
    *x_text++ = '\0';
    char *z_text = strchr(x_text, '\t');
    if (z_text == NULL)
        return 0;
    *z_text++ = '\0';
    int32_t x;
    int32_t z;
    if (!coordinate(x_text, &x) || !coordinate(z_text, &z))
        return 0;
    uint8_t *bytes = NULL;
    size_t length = 0;
    if (!load_file(line, &bytes, &length))
        return 0;

    mc_region_view view;
    mc_region_status open_status = mc_region_open(bytes, length, NULL, &view);
    const mc_region_entry *entry = NULL;
    mc_region_payload payload = {0};
    mc_region_status record_status = MC_REGION_INVALID_ARGUMENT;
    mc_compression_status compression_status = MC_COMPRESSION_INVALID_ARGUMENT;
    mc_compression_buffer raw = {0};
    size_t compressed_consumed = 0;
    mc_nbt_status nbt_status = MC_NBT_INVALID_ARGUMENT;
    size_t nbt_consumed = 0;
    mc_nbt_document *document = NULL;
    uint8_t *canonical = NULL;
    size_t canonical_length = 0;
    int compression_attempted = 0;
    int nbt_attempted = 0;
    int nbt_parsed = 0;
    if (open_status == MC_REGION_OK) {
        entry = mc_region_entry_at(&view, x, z);
        record_status = mc_region_record(&view, x, z, &payload);
        if (record_status == MC_REGION_OK) {
            mc_compression_limits limits = mc_compression_default_limits();
            limits.max_input_bytes = PROBE_FILE_BYTES;
            limits.max_output_bytes = PROBE_RAW_BYTES;
            mc_compression_format format =
                payload.selector == 1 ? MC_COMPRESSION_GZIP : MC_COMPRESSION_ZLIB;
            compression_attempted = 1;
            compression_status = mc_compression_decode(format, payload.data, payload.length,
                                                       &limits, &raw, &compressed_consumed);
            if (compression_status == MC_COMPRESSION_OK) {
                mc_nbt_limits nbt_limits = mc_nbt_default_limits();
                nbt_limits.max_input_bytes = PROBE_RAW_BYTES;
                nbt_limits.max_output_bytes = PROBE_RAW_BYTES;
                nbt_limits.max_depth = 32;
                nbt_attempted = 1;
                nbt_status = mc_nbt_read_compound(raw.data, raw.length, &nbt_limits, &document,
                                                  &nbt_consumed, NULL);
                if (nbt_status == MC_NBT_OK) {
                    nbt_parsed = 1;
                    nbt_status = mc_nbt_measure(mc_nbt_document_root(document), &nbt_limits,
                                                &canonical_length, NULL);
                }
                if (nbt_status == MC_NBT_OK) {
                    canonical = malloc(canonical_length);
                    if (canonical == NULL) {
                        mc_nbt_document_free(document);
                        mc_compression_buffer_free(&raw);
                        free(bytes);
                        return 0;
                    }
                    nbt_status = mc_nbt_write(mc_nbt_document_root(document), &nbt_limits,
                                              canonical, canonical_length, &canonical_length, NULL);
                }
            }
        }
    }

    printf("{\"open_status\":\"%s\",\"view\":", mc_region_status_string(open_status));
    if (open_status == MC_REGION_OK)
        printf("{\"length\":%zu,\"trailing_bytes\":%zu,\"present_entries\":%zu,"
               "\"invalid_entries\":%zu}",
               view.length, view.trailing_bytes, view.present_entries, view.invalid_entries);
    else
        fputs("null", stdout);
    fputs(",\"entry\":", stdout);
    if (entry != NULL)
        printf("{\"location\":%" PRIu32 ",\"timestamp\":%" PRIu32 ",\"issues\":%" PRIu32
               ",\"record_length\":%" PRIu32 ",\"byte_offset\":%" PRIu64
               ",\"extent_bytes\":%" PRIu64 ",\"selector\":%u}",
               entry->location, entry->timestamp, entry->issues, entry->record_length,
               entry->byte_offset, entry->extent_bytes, (unsigned int)entry->selector);
    else
        fputs("null", stdout);
    fputs(",\"record_status\":", stdout);
    if (open_status == MC_REGION_OK)
        printf("\"%s\"", mc_region_status_string(record_status));
    else
        fputs("null", stdout);
    fputs(",\"payload_hex\":", stdout);
    if (record_status == MC_REGION_OK)
        print_hex(payload.data, payload.length);
    else
        fputs("null", stdout);
    fputs(",\"compression_status\":", stdout);
    if (compression_attempted)
        printf("\"%s\"", mc_compression_status_string(compression_status));
    else
        fputs("null", stdout);
    fputs(",\"compressed_consumed\":", stdout);
    if (compression_attempted && compression_status == MC_COMPRESSION_OK)
        printf("%zu", compressed_consumed);
    else
        fputs("null", stdout);
    fputs(",\"compressed_trailing_bytes\":", stdout);
    if (compression_attempted && compression_status == MC_COMPRESSION_OK)
        printf("%zu", payload.length - compressed_consumed);
    else
        fputs("null", stdout);
    fputs(",\"raw_hex\":", stdout);
    if (compression_attempted && compression_status == MC_COMPRESSION_OK)
        print_hex(raw.data, raw.length);
    else
        fputs("null", stdout);
    fputs(",\"nbt_status\":", stdout);
    if (nbt_attempted)
        printf("\"%s\"", mc_nbt_status_string(nbt_status));
    else
        fputs("null", stdout);
    fputs(",\"nbt_consumed\":", stdout);
    if (nbt_parsed)
        printf("%zu", nbt_consumed);
    else
        fputs("null", stdout);
    fputs(",\"raw_trailing_bytes\":", stdout);
    if (nbt_parsed)
        printf("%zu", raw.length - nbt_consumed);
    else
        fputs("null", stdout);
    fputs(",\"canonical_hex\":", stdout);
    if (nbt_attempted && nbt_status == MC_NBT_OK)
        print_hex(canonical, canonical_length);
    else
        fputs("null", stdout);
    puts("}");
    free(canonical);
    mc_nbt_document_free(document);
    mc_compression_buffer_free(&raw);
    free(bytes);
    return 1;
}

int main(void)
{
    while (fgets(request, (int)sizeof(request), stdin) != NULL) {
        size_t length = strlen(request);
        if (length != 0 && request[length - 1u] == '\n')
            request[--length] = '\0';
        else if (!feof(stdin)) {
            fputs("probe request exceeds line limit\n", stderr);
            return 2;
        }
        if (length != 0 && request[length - 1u] == '\r')
            request[length - 1u] = '\0';
        if (!handle_request(request)) {
            fputs("probe invalid request, file read, or allocation failure\n", stderr);
            return 2;
        }
    }
    if (fflush(stdout) == EOF || ferror(stdin) || ferror(stdout)) {
        fputs("probe transport failure\n", stderr);
        return 2;
    }
    return 0;
}
