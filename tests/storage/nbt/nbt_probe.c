#include "mc/storage/nbt/nbt.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* One bounded, text-only request per line; no original runtime is linked here. */
#define PROBE_BYTES (256u * 1024u)

static char request[PROBE_BYTES * 2u + 32u];
static uint8_t input[PROBE_BYTES];
static uint8_t output[PROBE_BYTES];

static int hex_digit(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

static void hex_bytes(const uint8_t *bytes, size_t length)
{
    for (size_t i = 0; i < length; ++i)
        printf("%02x", (unsigned int)bytes[i]);
}

static void hex_units(mc_nbt_string text)
{
    putchar('"');
    for (size_t i = 0; i < text.length; ++i)
        printf("%04x", (unsigned int)text.units[i]);
    putchar('"');
}

static int compare_names(const void *a, const void *b)
{
    const mc_nbt_tag *left = *(const mc_nbt_tag *const *)a;
    const mc_nbt_tag *right = *(const mc_nbt_tag *const *)b;
    size_t common = left->name.length < right->name.length ? left->name.length : right->name.length;
    for (size_t i = 0; i < common; ++i) {
        if (left->name.units[i] != right->name.units[i])
            return left->name.units[i] < right->name.units[i] ? -1 : 1;
    }
    if (left->name.length == right->name.length)
        return 0;
    return left->name.length < right->name.length ? -1 : 1;
}

static void print_tree(const mc_nbt_tag *tag, int named)
{
    printf("[%u,", (unsigned int)tag->type);
    if (named && tag->type != MC_NBT_END)
        hex_units(tag->name);
    else
        fputs("\"\"", stdout);
    putchar(',');
    switch (tag->type) {
        case MC_NBT_END:
            fputs("null", stdout);
            break;
        case MC_NBT_BYTE:
            printf("%d", (int)tag->value.byte_value);
            break;
        case MC_NBT_SHORT:
            printf("%d", (int)tag->value.short_value);
            break;
        case MC_NBT_INT:
            printf("%" PRId32, tag->value.int_value);
            break;
        case MC_NBT_LONG:
            printf("%" PRId64, tag->value.long_value);
            break;
        case MC_NBT_FLOAT: {
            printf("\"%08" PRIx32 "\"", tag->value.float_bits);
            break;
        }
        case MC_NBT_DOUBLE: {
            printf("\"%016" PRIx64 "\"", tag->value.double_bits);
            break;
        }
        case MC_NBT_BYTE_ARRAY:
            putchar('"');
            hex_bytes(tag->value.byte_array.data, tag->value.byte_array.length);
            putchar('"');
            break;
        case MC_NBT_STRING:
            hex_units(tag->value.string);
            break;
        case MC_NBT_LIST:
            printf("[%u,[",
                   tag->value.list.length == 0 ? 1u : (unsigned int)tag->value.list.element_type);
            for (size_t i = 0; i < tag->value.list.length; ++i) {
                if (i != 0)
                    putchar(',');
                print_tree(tag->value.list.items[i], 0);
            }
            fputs("]]", stdout);
            break;
        case MC_NBT_COMPOUND: {
            size_t count = tag->value.compound.length;
            const mc_nbt_tag **sorted = NULL;
            if (count != 0) {
                sorted = malloc(count * sizeof(*sorted));
                if (sorted == NULL) {
                    fputs("probe tree allocation failed\n", stderr);
                    exit(2);
                }
                memcpy(sorted, tag->value.compound.items, count * sizeof(*sorted));
                qsort(sorted, count, sizeof(*sorted), compare_names);
            }
            putchar('[');
            for (size_t i = 0; i < count; ++i) {
                if (i != 0)
                    putchar(',');
                print_tree(sorted[i], 1);
            }
            putchar(']');
            free(sorted);
            break;
        }
        default:
            fputs("unexpected parsed tag\n", stderr);
            exit(2);
    }
    putchar(']');
}

static void handle_request(char *line)
{
    char *separator = strchr(line, '\t');
    mc_nbt_document *document = NULL;
    size_t consumed = 0;
    size_t written = 0;
    mc_nbt_limits limits = mc_nbt_default_limits();
    mc_nbt_status status;
    if (separator == NULL) {
        puts("ERR\tinvalid request");
        return;
    }
    *separator = '\0';
    if (strcmp(line, "generic") != 0 && strcmp(line, "compound") != 0) {
        puts("ERR\tinvalid request");
        return;
    }
    const char *hex = separator + 1;
    size_t digits = strlen(hex);
    if ((digits & 1u) != 0 || digits / 2u > PROBE_BYTES) {
        puts("ERR\tinvalid request");
        return;
    }
    for (size_t i = 0; i < digits / 2u; ++i) {
        int high = hex_digit(hex[2u * i]);
        int low = hex_digit(hex[2u * i + 1u]);
        if (high < 0 || low < 0) {
            puts("ERR\tinvalid request");
            return;
        }
        input[i] = (uint8_t)((unsigned int)high * 16u + (unsigned int)low);
    }
    limits.max_input_bytes = PROBE_BYTES;
    limits.max_output_bytes = PROBE_BYTES;
    limits.max_depth = 32;
    if (strcmp(line, "compound") == 0)
        status = mc_nbt_read_compound(input, digits / 2u, &limits, &document, &consumed, NULL);
    else
        status = mc_nbt_read(input, digits / 2u, &limits, &document, &consumed, NULL);
    if (status == MC_NBT_OK)
        status = mc_nbt_write(mc_nbt_document_root(document), &limits, output, sizeof(output),
                              &written, NULL);
    if (status != MC_NBT_OK) {
        printf("ERR\t%s\n", mc_nbt_status_string(status));
    } else {
        printf("OK\t%zu\t", consumed);
        hex_bytes(output, written);
        putchar('\t');
        print_tree(mc_nbt_document_root(document), 1);
        putchar('\n');
    }
    mc_nbt_document_free(document);
}

int main(void)
{
    while (fgets(request, (int)sizeof(request), stdin) != NULL) {
        size_t length = strlen(request);
        if (length != 0 && request[length - 1] == '\n') {
            request[--length] = '\0';
        } else if (!feof(stdin)) {
            int c;
            do {
                c = getchar();
            } while (c != '\n' && c != EOF);
            puts("ERR\trequest too long");
            continue;
        }
        if (length != 0 && request[length - 1] == '\r')
            request[length - 1] = '\0';
        handle_request(request);
    }
    return ferror(stdin) || ferror(stdout) ? 2 : 0;
}
