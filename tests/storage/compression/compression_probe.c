#include "mc/storage/compression/compression.h"
#include "mc/storage/nbt/nbt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PROBE_INPUT_BYTES (512u * 1024u)
#define PROBE_OUTPUT_BYTES (1024u * 1024u)

static char request[PROBE_INPUT_BYTES * 2u + 64u];
static uint8_t input[PROBE_INPUT_BYTES];

static int hex_digit(char digit) {
    if (digit >= '0' && digit <= '9')
        return digit - '0';
    if (digit >= 'a' && digit <= 'f')
        return digit - 'a' + 10;
    if (digit >= 'A' && digit <= 'F')
        return digit - 'A' + 10;
    return -1;
}

static void print_hex(const uint8_t *bytes, size_t length) {
    for (size_t i = 0; i < length; ++i)
        printf("%02x", (unsigned int)bytes[i]);
}

static int handle_request(char *line) {
    char *separator = strchr(line, '\t');
    mc_compression_format format;
    int encode;
    int nbt;
    if (separator == NULL)
        return 0;
    *separator = '\0';
    if (strcmp(line, "encode-gzip") == 0) {
        format = MC_COMPRESSION_GZIP;
        encode = 1;
        nbt = 0;
    } else if (strcmp(line, "encode-zlib") == 0) {
        format = MC_COMPRESSION_ZLIB;
        encode = 1;
        nbt = 0;
    } else if (strcmp(line, "decode-gzip") == 0 || strcmp(line, "decode-gzip-nbt") == 0) {
        format = MC_COMPRESSION_GZIP;
        encode = 0;
        nbt = strcmp(line, "decode-gzip-nbt") == 0;
    } else if (strcmp(line, "decode-zlib") == 0 || strcmp(line, "decode-zlib-nbt") == 0) {
        format = MC_COMPRESSION_ZLIB;
        encode = 0;
        nbt = strcmp(line, "decode-zlib-nbt") == 0;
    } else {
        return 0;
    }
    const char *hex = separator + 1;
    size_t digits = strlen(hex);
    if ((digits & 1u) != 0 || digits / 2u > PROBE_INPUT_BYTES)
        return 0;
    size_t input_length = digits / 2u;
    for (size_t i = 0; i < input_length; ++i) {
        int high = hex_digit(hex[2u * i]);
        int low = hex_digit(hex[2u * i + 1u]);
        if (high < 0 || low < 0)
            return 0;
        input[i] = (uint8_t)((unsigned int)high * 16u + (unsigned int)low);
    }
    mc_compression_limits limits = mc_compression_default_limits();
    limits.max_input_bytes = PROBE_INPUT_BYTES;
    limits.max_output_bytes = PROBE_OUTPUT_BYTES;
    mc_compression_buffer result = {0};
    size_t consumed = input_length;
    mc_compression_status status;
    if (encode)
        status = mc_compression_encode(format, input, input_length, &limits, &result);
    else
        status = mc_compression_decode(format, input, input_length, &limits, &result, &consumed);
    if (status != MC_COMPRESSION_OK) {
        printf("ERR\t%s\n", mc_compression_status_string(status));
        return 1;
    }
    uint8_t *canonical = NULL;
    size_t canonical_length = 0;
    mc_nbt_document *document = NULL;
    if (nbt) {
        mc_nbt_limits nbt_limits = mc_nbt_default_limits();
        nbt_limits.max_input_bytes = PROBE_OUTPUT_BYTES;
        nbt_limits.max_output_bytes = PROBE_OUTPUT_BYTES;
        nbt_limits.max_depth = 32;
        mc_nbt_status nbt_status =
            mc_nbt_read_compound(result.data, result.length, &nbt_limits, &document, NULL, NULL);
        if (nbt_status == MC_NBT_OK)
            nbt_status = mc_nbt_measure(mc_nbt_document_root(document), &nbt_limits,
                                        &canonical_length, NULL);
        if (nbt_status == MC_NBT_OK) {
            canonical = malloc(canonical_length);
            if (canonical == NULL) {
                mc_nbt_document_free(document);
                mc_compression_buffer_free(&result);
                fputs("probe canonical output allocation failed\n", stderr);
                return 0;
            }
            nbt_status = mc_nbt_write(mc_nbt_document_root(document), &nbt_limits, canonical,
                                      canonical_length, &canonical_length, NULL);
        }
        if (nbt_status != MC_NBT_OK) {
            printf("ERR\t%s\n", mc_nbt_status_string(nbt_status));
            free(canonical);
            mc_nbt_document_free(document);
            mc_compression_buffer_free(&result);
            return 1;
        }
    }
    printf("OK\t%zu\t", consumed);
    if (nbt)
        print_hex(canonical, canonical_length);
    else
        print_hex(result.data, result.length);
    putchar('\n');
    free(canonical);
    mc_nbt_document_free(document);
    mc_compression_buffer_free(&result);
    return 1;
}

int main(void) {
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
            fputs("probe invalid request or transport failure\n", stderr);
            return 2;
        }
    }
    if (fflush(stdout) == EOF || ferror(stdin) || ferror(stdout)) {
        fputs("probe transport failure\n", stderr);
        return 2;
    }
    return 0;
}
