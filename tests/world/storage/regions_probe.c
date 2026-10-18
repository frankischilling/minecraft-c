#include "mc/world/storage/regions.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#define PROBE_DATA_BYTES (2u * 1024u * 1024u)
#define PROBE_LINE_BYTES (2u * PROBE_DATA_BYTES + 512u)
#define PROBE_INPUT_BYTES (16u * 1024u * 1024u)
#define PROBE_OUTPUT_BYTES (64u * 1024u * 1024u)
#define PROBE_COMMANDS 4096u

static bool number(const char *text, uint64_t limit, uint64_t *out)
{
    if (*text == '\0' || (text[0] == '0' && text[1] != '\0'))
        return false;
    uint64_t value = 0;
    for (; *text != '\0'; ++text) {
        if (*text < '0' || *text > '9')
            return false;
        uint64_t digit = (uint64_t)(*text - '0');
        if (value > limit / 10u || (value == limit / 10u && digit > limit % 10u))
            return false;
        value = value * 10u + digit;
    }
    *out = value;
    return true;
}

static bool coordinate(const char *text, int32_t *out)
{
    bool negative = *text == '-';
    uint64_t value;
    if (!number(text + (negative ? 1 : 0), negative ? UINT64_C(2147483648) : INT32_MAX, &value) ||
        (negative && value == 0))
        return false;
    *out = negative ? -1 - (int32_t)(value - 1u) : (int32_t)value;
    return true;
}

static int digit(unsigned char value)
{
    if (value >= '0' && value <= '9')
        return value - '0';
    if (value >= 'a' && value <= 'f')
        return value - 'a' + 10;
    return -1;
}

static bool decode(const char *text, size_t limit, uint8_t **out, size_t *length)
{
    size_t count = strcmp(text, "-") == 0 ? 0 : strlen(text);
    if (count % 2u != 0 || count / 2u > limit)
        return false;
    uint8_t *bytes = malloc(count / 2u + 1u);
    if (bytes == NULL)
        return false;
    for (size_t i = 0; i < count / 2u; ++i) {
        int high = digit((unsigned char)text[i * 2u]);
        int low = digit((unsigned char)text[i * 2u + 1u]);
        if (high < 0 || low < 0) {
            free(bytes);
            return false;
        }
        bytes[i] = (uint8_t)((unsigned)high * 16u + (unsigned)low);
    }
    bytes[count / 2u] = 0;
    *out = bytes;
    *length = count / 2u;
    return true;
}

static bool diagnostic(mc_world_storage_status status, const mc_world_storage_error *error)
{
    return error->status == status &&
           (status == MC_WORLD_STORAGE_OK
                ? error->operation == NULL && error->system_error == 0
                : error->operation != NULL && strlen(error->operation) <= 256u);
}

static void json_string(const char *text)
{
    if (text == NULL) {
        fputs("null", stdout);
        return;
    }
    putchar('"');
    for (; *text != '\0'; ++text) {
        unsigned char byte = (unsigned char)*text;
        if (byte == '"' || byte == '\\')
            putchar('\\');
        if (byte < 32u || byte >= 127u)
            printf("\\u%04x", (unsigned)byte);
        else
            putchar(byte);
    }
    putchar('"');
}

static bool response(mc_world_regions *regions, mc_world_storage_status status,
                     const mc_world_storage_error *error, const mc_compression_buffer *data,
                     const uint32_t *growth, int preserved, size_t *total)
{
    size_t length = data == NULL ? 0 : data->length;
    if (length > PROBE_DATA_BYTES || *total > PROBE_OUTPUT_BYTES - 1024u - length * 2u ||
        !diagnostic(status, error))
        return false;
    *total += 1024u + length * 2u;
    mc_world_regions_usage usage = mc_world_regions_get_usage(regions);
    printf("{\"status\":%u,\"error\":{\"status\":%u,\"operation\":", (unsigned)status,
           (unsigned)error->status);
    json_string(error->operation);
    printf(",\"system_error\":%u},\"usage\":{\"count\":%zu,\"bytes\":%zu},\"growth\":",
           (unsigned)error->system_error, usage.count, usage.bytes);
    if (growth == NULL)
        fputs("null", stdout);
    else
        printf("%u", (unsigned)*growth);
    fputs(",\"data\":", stdout);
    if (data == NULL)
        fputs("null", stdout);
    else {
        putchar('"');
        for (size_t i = 0; i < length; ++i)
            printf("%02x", (unsigned)data->data[i]);
        putchar('"');
    }
    printf(",\"outputs_preserved\":%s}\n", preserved < 0 ? "null" : preserved ? "true" : "false");
    return fflush(stdout) == 0 && !ferror(stdout);
}

static bool check_arguments(mc_world_regions *regions)
{
    uint8_t marker = 73;
    mc_compression_buffer out = {&marker, 19};
    mc_world_storage_error error;
    uint32_t growth = UINT32_C(0xdeadbeef);
    mc_world_regions_usage before = mc_world_regions_get_usage(regions);
    if (mc_world_regions_read(NULL, "", 0, 0, &out, &error) != MC_WORLD_STORAGE_INVALID_ARGUMENT ||
        out.data != &marker || out.length != 19 || marker != 73 ||
        !diagnostic(MC_WORLD_STORAGE_INVALID_ARGUMENT, &error) ||
        mc_world_regions_read(regions, NULL, 0, 0, &out, &error) !=
            MC_WORLD_STORAGE_INVALID_ARGUMENT ||
        mc_world_regions_read(regions, "", 0, 0, NULL, &error) !=
            MC_WORLD_STORAGE_INVALID_ARGUMENT ||
        mc_world_regions_write(NULL, "", 0, 0, NULL, 0, 0, &growth, &error) !=
            MC_WORLD_STORAGE_INVALID_ARGUMENT ||
        mc_world_regions_write(regions, NULL, 0, 0, NULL, 0, 0, &growth, &error) !=
            MC_WORLD_STORAGE_INVALID_ARGUMENT ||
        mc_world_regions_write(regions, "", 0, 0, NULL, 1, 0, &growth, &error) !=
            MC_WORLD_STORAGE_INVALID_ARGUMENT ||
        mc_world_regions_write(regions, "", 0, 0, NULL, 0, 0, NULL, &error) !=
            MC_WORLD_STORAGE_INVALID_ARGUMENT ||
        growth != UINT32_C(0xdeadbeef))
        return false;
    mc_world_regions *owner = regions;
    mc_world_regions_limits limits = mc_world_regions_default_limits();
    if (mc_world_regions_create(NULL, NULL, &error) != MC_WORLD_STORAGE_INVALID_ARGUMENT)
        return false;
    limits.compression.max_members = 0;
    if (mc_world_regions_create(&limits, &owner, &error) != MC_WORLD_STORAGE_INVALID_ARGUMENT ||
        owner != regions)
        return false;
    limits = mc_world_regions_default_limits();
    limits.max_cached_bytes = 0;
    if (mc_world_regions_create(&limits, &owner, &error) != MC_WORLD_STORAGE_LIMIT ||
        owner != regions)
        return false;
    limits = mc_world_regions_default_limits();
    limits.image.max_image_bytes = (size_t)MC_REGION_WRITER_MAX_IMAGE_BYTES + 1u;
    if (mc_world_regions_create(&limits, &owner, &error) != MC_WORLD_STORAGE_INVALID_ARGUMENT ||
        owner != regions)
        return false;
    mc_world_regions_clear(NULL);
    mc_world_regions_free(NULL);
    mc_world_regions_usage empty = mc_world_regions_get_usage(NULL);
    mc_world_regions_usage after = mc_world_regions_get_usage(regions);
    return empty.count == 0 && empty.bytes == 0 && before.count == after.count &&
           before.bytes == after.bytes;
}

int main(int argc, char **argv)
{
    (void)argv;
    if (argc != 1)
        return 2;
#ifdef _WIN32
    if (_setmode(_fileno(stdin), _O_BINARY) == -1 || _setmode(_fileno(stdout), _O_BINARY) == -1)
        return 1;
#endif
    char *line = malloc(PROBE_LINE_BYTES + 1u), *root = NULL;
    mc_world_regions *regions = NULL;
    mc_world_storage_error error;
    if (line == NULL || mc_world_regions_create(NULL, &regions, &error) != MC_WORLD_STORAGE_OK) {
        free(line);
        return 1;
    }
    size_t input_bytes = 0, output_bytes = 0, commands = 0;
    int result = 0;
    bool done = false;
    while (!done) {
        size_t length = 0;
        int byte;
        while ((byte = getchar()) != EOF && byte != '\n') {
            if (++input_bytes > PROBE_INPUT_BYTES || length == PROBE_LINE_BYTES || byte < 32 ||
                byte >= 127) {
                result = 2;
                goto finish;
            }
            line[length++] = (char)byte;
        }
        if (byte == EOF) {
            result = ferror(stdin) ? 1 : length != 0 ? 2 : 0;
            break;
        }
        if (++input_bytes > PROBE_INPUT_BYTES || ++commands > PROBE_COMMANDS || length == 0) {
            result = 2;
            break;
        }
        line[length] = '\0';
        char *words[9];
        size_t count = 0;
        char *start = line;
        for (char *cursor = line;; ++cursor) {
            if (*cursor != ' ' && *cursor != '\0')
                continue;
            char separator = *cursor;
            if (cursor == start || count == 9u) {
                result = 2;
                goto finish;
            }
            *cursor = '\0';
            words[count++] = start;
            start = cursor + 1;
            if (separator == '\0')
                break;
        }
        error = (mc_world_storage_error){MC_WORLD_STORAGE_OK, NULL, 0};
        mc_world_storage_status status = MC_WORLD_STORAGE_OK;
        mc_compression_buffer decoded = {0};
        const mc_compression_buffer *data = NULL;
        uint32_t growth = UINT32_C(0xdeadbeef);
        const uint32_t *returned_growth = NULL;
        int preserved = -1;
        if (strcmp(words[0], "ROOT") == 0 && count == 2) {
            uint8_t *value;
            size_t size;
            if (!decode(words[1], 32768u, &value, &size)) {
                result = 2;
                goto finish;
            }
            if (memchr(value, 0, size) != NULL) {
                free(value);
                result = 2;
                goto finish;
            }
            free(root);
            root = (char *)value;
        } else if ((strcmp(words[0], "READ") == 0 && count == 3) ||
                   (strcmp(words[0], "WRITE") == 0 && count == 5)) {
            int32_t x, z;
            if (!coordinate(words[1], &x) || !coordinate(words[2], &z)) {
                result = 2;
                goto finish;
            }
            size_t root_bytes = root == NULL ? 0 : strlen(root) + 1u;
            char *saved_root = root_bytes == 0 ? NULL : malloc(root_bytes);
            if (root_bytes != 0 && saved_root == NULL) {
                result = 1;
                goto finish;
            }
            if (root_bytes != 0)
                memcpy(saved_root, root, root_bytes);
            if (count == 3) {
                uint8_t marker = 73;
                decoded = (mc_compression_buffer){&marker, 19};
                status = mc_world_regions_read(regions, root, x, z, &decoded, &error);
                preserved = status != MC_WORLD_STORAGE_OK;
                if ((preserved &&
                     (decoded.data != &marker || decoded.length != 19 || marker != 73)) ||
                    (!preserved && decoded.data == &marker)) {
                    free(saved_root);
                    result = 1;
                    goto finish;
                }
                if (preserved)
                    decoded = (mc_compression_buffer){0};
                else
                    data = &decoded;
            } else {
                uint8_t *value;
                size_t size;
                uint64_t timestamp;
                if (!number(words[3], UINT32_MAX, &timestamp) ||
                    !decode(words[4], PROBE_DATA_BYTES, &value, &size)) {
                    free(saved_root);
                    result = 2;
                    goto finish;
                }
                uint8_t *saved = malloc(size + 1u);
                if (saved == NULL) {
                    free(value);
                    free(saved_root);
                    result = 1;
                    goto finish;
                }
                memcpy(saved, value, size + 1u);
                status = mc_world_regions_write(regions, root, x, z, value, size,
                                                (uint32_t)timestamp, &growth, &error);
                bool unchanged = memcmp(saved, value, size + 1u) == 0;
                free(saved);
                free(value);
                preserved = status != MC_WORLD_STORAGE_OK;
                if (!unchanged || (preserved && growth != UINT32_C(0xdeadbeef))) {
                    free(saved_root);
                    result = 1;
                    goto finish;
                }
                if (!preserved)
                    returned_growth = &growth;
            }
            bool root_unchanged = root_bytes == 0 || memcmp(saved_root, root, root_bytes) == 0;
            free(saved_root);
            if (!root_unchanged) {
                mc_compression_buffer_free(&decoded);
                result = 1;
                goto finish;
            }
        } else if (strcmp(words[0], "CLEAR") == 0 && count == 1) {
            mc_world_regions_clear(regions);
        } else if (strcmp(words[0], "USAGE") == 0 && count == 1) {
        } else if (strcmp(words[0], "LIMITS") == 0 && count == 8) {
            uint64_t values[7];
            for (size_t i = 0; i < 7; ++i) {
                uint64_t limit = i == 3 || i == 4 ? PROBE_DATA_BYTES
                                 : i == 6         ? 1024u
                                                  : 128u * 1024u * 1024u;
                if (!number(words[i + 1u], limit, &values[i])) {
                    result = 2;
                    goto finish;
                }
            }
            mc_world_regions_limits limits = {
                (size_t)values[0],
                {(size_t)values[1], (size_t)values[2]},
                {(size_t)values[3], (size_t)values[4], (size_t)values[5], (size_t)values[6]}};
            mc_world_regions *replacement = NULL;
            status = mc_world_regions_create(&limits, &replacement, &error);
            if (status == MC_WORLD_STORAGE_OK) {
                mc_world_regions_free(regions);
                regions = replacement;
            } else if (replacement != NULL) {
                result = 1;
                goto finish;
            }
        } else if (strcmp(words[0], "CHECK") == 0 && count == 1) {
            if (!check_arguments(regions)) {
                result = 1;
                goto finish;
            }
        } else if (strcmp(words[0], "QUIT") == 0 && count == 1) {
            done = true;
        } else {
            result = 2;
            goto finish;
        }
        bool printed =
            response(regions, status, &error, data, returned_growth, preserved, &output_bytes);
        mc_compression_buffer_free(&decoded);
        if (!printed) {
            result = 1;
            break;
        }
    }
    if (done) {
        if (getchar() != EOF)
            result = 2;
        else if (ferror(stdin))
            result = 1;
    }
finish:
    if (result == 1)
        fputs("region service probe contract or resource failure\n", stderr);
    mc_world_regions_free(regions);
    free(root);
    free(line);
    return result;
}
