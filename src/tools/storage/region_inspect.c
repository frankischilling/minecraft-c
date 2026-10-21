#include "mc/storage/compression/compression.h"
#include "mc/storage/nbt/nbt.h"
#include "mc/storage/region/region.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <wchar.h>
typedef wchar_t cli_char;
#else
typedef char cli_char;
#endif

#define SUMMARY_NAME_UNITS 128u
#define SUMMARY_CHILDREN 32u
#define SUMMARY_DEPTH 2u

typedef struct inspect_options {
    const cli_char *path;
    size_t max_input;
    size_t max_output;
    size_t max_allocation;
    int32_t x;
    int32_t z;
    bool selected;
    bool all;
} inspect_options;

typedef enum input_status { INPUT_OK, INPUT_IO, INPUT_ALLOCATION, INPUT_LIMIT } input_status;

typedef struct selected_record {
    mc_region_status record_status;
    bool compression_attempted;
    mc_compression_status compression_status;
    mc_compression_buffer decoded;
    size_t compressed_length;
    size_t compressed_consumed;
    bool nbt_attempted;
    mc_nbt_status nbt_status;
    mc_nbt_document *document;
    size_t nbt_consumed;
    mc_nbt_error nbt_error;
} selected_record;

static bool cli_equal(const cli_char *argument, const char *ascii) {
    size_t i = 0;
    while (ascii[i] != '\0') {
        if (argument[i] != (cli_char)(unsigned char)ascii[i]) {
            return false;
        }
        ++i;
    }
    return argument[i] == 0;
}

static bool parse_size(const cli_char *text, size_t *value) {
    if (*text == 0) {
        return false;
    }
    size_t result = 0;
    for (size_t i = 0; text[i] != 0; ++i) {
        if (text[i] < (cli_char)'0' || text[i] > (cli_char)'9') {
            return false;
        }
        size_t digit = (size_t)(text[i] - (cli_char)'0');
        if (result > (SIZE_MAX - digit) / 10u) {
            return false;
        }
        result = result * 10u + digit;
    }
    *value = result;
    return true;
}

static void usage(FILE *output) {
    fputs("Usage: mc_region_inspect [--all] [--max-input-bytes N]\n"
          "                         [--max-output-bytes N] [--max-allocation-bytes N]\n"
          "                         FILE [X Z]\n"
          "       mc_region_inspect --help\n"
          "\n"
          "Read a region file without modifying it. Default output lists present entries.\n"
          "--all includes missing entries. X and Z select one local entry in 0..31 and\n"
          "decode its compressed compound; selection takes precedence over --all.\n"
          "Use -- before a filename beginning with a dash. Byte limits are decimal.\n"
          "Defaults: input 64 MiB, decompressed output 16 MiB, allocation 64 MiB.\n"
          "The input buffer, compression allocations, and NBT allocations have independent\n"
          "limits; these limits do not describe total process memory.\n"
          "Root summaries show at most 128 name units, 32 children and two child levels.\n"
          "Exit: 0 clean; 1 inspection diagnostic; 2 usage/I/O/allocation/limit;\n"
          "      3 selected entry missing. Extra compressed/decompressed bytes are diagnostics.\n",
          output);
}

static int parse_options(int argc, cli_char **argv, inspect_options *options) {
    const cli_char *positionals[3] = {NULL, NULL, NULL};
    size_t positional_count = 0;
    bool options_finished = false;
    options->max_input = 64u * 1024u * 1024u;
    options->max_output = 16u * 1024u * 1024u;
    options->max_allocation = 64u * 1024u * 1024u;
    for (int i = 1; i < argc; ++i) {
        const cli_char *argument = argv[i];
        if (!options_finished && cli_equal(argument, "--")) {
            options_finished = true;
        } else if (!options_finished && cli_equal(argument, "--help")) {
            usage(stdout);
            return 0;
        } else if (!options_finished && cli_equal(argument, "--all")) {
            options->all = true;
        } else if (!options_finished && (cli_equal(argument, "--max-input-bytes") ||
                                         cli_equal(argument, "--max-output-bytes") ||
                                         cli_equal(argument, "--max-allocation-bytes"))) {
            if (i == argc - 1) {
                return 2;
            }
            size_t value;
            if (!parse_size(argv[++i], &value)) {
                return 2;
            }
            if (cli_equal(argument, "--max-input-bytes")) {
                options->max_input = value;
            } else if (cli_equal(argument, "--max-output-bytes")) {
                options->max_output = value;
            } else {
                options->max_allocation = value;
            }
        } else {
            if ((!options_finished && argument[0] == (cli_char)'-') || positional_count == 3) {
                return 2;
            }
            positionals[positional_count++] = argument;
        }
    }
    if (positional_count != 1 && positional_count != 3) {
        return 2;
    }
    options->path = positionals[0];
    if (positional_count == 3) {
        size_t x;
        size_t z;
        if (!parse_size(positionals[1], &x) || !parse_size(positionals[2], &z) || x >= 32 ||
            z >= 32) {
            return 2;
        }
        options->x = (int32_t)x;
        options->z = (int32_t)z;
        options->selected = true;
    }
    return -1;
}

static FILE *open_input(const cli_char *path) {
#ifdef _WIN32
#ifdef _MSC_VER
    FILE *input = NULL;
    if (_wfopen_s(&input, path, L"rb") != 0) {
        return NULL;
    }
    return input;
#else
    return _wfopen(path, L"rb");
#endif
#else
    return fopen(path, "rb");
#endif
}

static input_status read_input(const cli_char *path, size_t maximum, uint8_t **out,
                               size_t *length) {
    FILE *input = open_input(path);
    if (input == NULL) {
        return INPUT_IO;
    }
    uint8_t *data = NULL;
    size_t used = 0;
    size_t capacity = 0;
    input_status status = INPUT_OK;
    for (;;) {
        if (used == capacity) {
            /* At the configured cap this is the sole overflow byte. Elsewhere
             * it avoids allocating another buffer when EOF is exactly on a boundary. */
            int next = fgetc(input);
            if (next == EOF) {
                if (ferror(input)) {
                    status = INPUT_IO;
                }
                break;
            }
            if (used == maximum) {
                status = INPUT_LIMIT;
                break;
            }
            size_t grown = capacity == 0 ? 8192u : capacity > SIZE_MAX / 2 ? maximum : capacity * 2;
            if (grown > maximum) {
                grown = maximum;
            }
            if (grown <= capacity) {
                status = INPUT_LIMIT;
                break;
            }
            uint8_t *replacement = realloc(data, grown);
            if (replacement == NULL) {
                status = INPUT_ALLOCATION;
                break;
            }
            data = replacement;
            capacity = grown;
            data[used++] = (uint8_t)next;
        }
        size_t chunk = capacity - used;
        if (chunk > 64u * 1024u) {
            chunk = 64u * 1024u;
        }
        if (chunk == 0) {
            continue;
        }
        size_t count = fread(data + used, 1, chunk, input);
        used += count;
        if (count != chunk) {
            if (ferror(input) || !feof(input)) {
                status = INPUT_IO;
            }
            break;
        }
    }
    if (fclose(input) != 0 && status == INPUT_OK) {
        status = INPUT_IO;
    }
    if (status != INPUT_OK) {
        free(data);
        return status;
    }
    *out = data;
    *length = used;
    return INPUT_OK;
}

static size_t tag_child_count(const mc_nbt_tag *tag) {
    if (tag->type == MC_NBT_COMPOUND) {
        return tag->value.compound.length;
    }
    return tag->type == MC_NBT_LIST ? tag->value.list.length : 0;
}

static void print_summary(const mc_nbt_tag *tag, size_t depth) {
    size_t name_count =
        tag->name.length < SUMMARY_NAME_UNITS ? tag->name.length : SUMMARY_NAME_UNITS;
    size_t child_count = tag_child_count(tag);
    size_t shown = depth < SUMMARY_DEPTH ? child_count : 0;
    if (shown > SUMMARY_CHILDREN) {
        shown = SUMMARY_CHILDREN;
    }
    printf("{\"type\":%u,\"name_utf16\":[", (unsigned int)tag->type);
    for (size_t i = 0; i < name_count; ++i) {
        printf("%s%u", i == 0 ? "" : ",", (unsigned int)tag->name.units[i]);
    }
    printf("],\"name_length\":%zu,\"name_truncated\":%s,\"child_count\":%zu,"
           "\"child_summary_truncated\":%s",
           tag->name.length, name_count != tag->name.length ? "true" : "false", child_count,
           shown != child_count ? "true" : "false");
    switch (tag->type) {
        case MC_NBT_BYTE:
            printf(",\"integer\":%d", (int)tag->value.byte_value);
            break;
        case MC_NBT_SHORT:
            printf(",\"integer\":%d", (int)tag->value.short_value);
            break;
        case MC_NBT_INT:
            printf(",\"integer\":%" PRId32, tag->value.int_value);
            break;
        case MC_NBT_LONG:
            printf(",\"integer\":%" PRId64, tag->value.long_value);
            break;
        case MC_NBT_FLOAT:
            printf(",\"float_bits\":\"%08" PRIx32 "\"", tag->value.float_bits);
            break;
        case MC_NBT_DOUBLE:
            printf(",\"double_bits\":\"%016" PRIx64 "\"", tag->value.double_bits);
            break;
        case MC_NBT_BYTE_ARRAY:
            printf(",\"byte_count\":%zu", tag->value.byte_array.length);
            break;
        case MC_NBT_STRING:
            printf(",\"string_length\":%zu", tag->value.string.length);
            break;
        case MC_NBT_LIST:
            printf(",\"element_type\":%u", (unsigned int)tag->value.list.element_type);
            break;
        case MC_NBT_END:
        case MC_NBT_COMPOUND:
            break;
    }
    fputs(",\"children\":[", stdout);
    for (size_t i = 0; i < shown; ++i) {
        if (i != 0) {
            putchar(',');
        }
        const mc_nbt_tag *child =
            tag->type == MC_NBT_COMPOUND ? tag->value.compound.items[i] : tag->value.list.items[i];
        print_summary(child, depth + 1);
    }
    fputs("]}", stdout);
}

static int inspect_selected(const mc_region_view *view, const inspect_options *options,
                            selected_record *selected, int exit_code) {
    mc_region_payload payload;
    selected->record_status = mc_region_record(view, options->x, options->z, &payload);
    if (selected->record_status == MC_REGION_MISSING) {
        return 3;
    }
    if (selected->record_status != MC_REGION_OK) {
        return 1;
    }
    mc_compression_limits compression_limits = mc_compression_default_limits();
    compression_limits.max_input_bytes = options->max_input;
    compression_limits.max_output_bytes = options->max_output;
    compression_limits.max_allocation_bytes = options->max_allocation;
    selected->compressed_length = payload.length;
    selected->compression_attempted = true;
    selected->compression_status = mc_compression_decode(
        payload.selector == 1 ? MC_COMPRESSION_GZIP : MC_COMPRESSION_ZLIB, payload.data,
        payload.length, &compression_limits, &selected->decoded, &selected->compressed_consumed);
    if (selected->compression_status != MC_COMPRESSION_OK) {
        return selected->compression_status == MC_COMPRESSION_LIMIT ||
                       selected->compression_status == MC_COMPRESSION_ALLOCATION_FAILED
                   ? 2
                   : 1;
    }
    if (selected->compressed_consumed != selected->compressed_length) {
        exit_code = 1;
    }
    mc_nbt_limits nbt_limits = mc_nbt_default_limits();
    nbt_limits.max_input_bytes = options->max_output;
    nbt_limits.max_output_bytes = options->max_output;
    nbt_limits.max_allocation_bytes = options->max_allocation;
    selected->nbt_attempted = true;
    selected->nbt_status =
        mc_nbt_read_compound(selected->decoded.data, selected->decoded.length, &nbt_limits,
                             &selected->document, &selected->nbt_consumed, &selected->nbt_error);
    if (selected->nbt_status != MC_NBT_OK) {
        return selected->nbt_status == MC_NBT_LIMIT ||
                       selected->nbt_status == MC_NBT_ALLOCATION_FAILED
                   ? 2
                   : 1;
    }
    return selected->nbt_consumed != selected->decoded.length ? 1 : exit_code;
}

static void print_selected(const selected_record *selected) {
    if (!selected->compression_attempted) {
        return;
    }
    printf(",\"compression\":{\"status\":\"%s\",",
           mc_compression_status_string(selected->compression_status));
    if (selected->compression_status == MC_COMPRESSION_OK) {
        printf("\"consumed\":%zu,\"trailing_bytes\":%zu,\"output_bytes\":%zu}",
               selected->compressed_consumed,
               selected->compressed_length - selected->compressed_consumed,
               selected->decoded.length);
    } else {
        fputs("\"consumed\":null,\"trailing_bytes\":null,\"output_bytes\":null}", stdout);
    }
    if (!selected->nbt_attempted) {
        fputs(",\"nbt\":{\"status\":\"not attempted\",\"consumed\":null,\"trailing_bytes\":null,"
              "\"error_offset\":null}",
              stdout);
        return;
    }
    printf(",\"nbt\":{\"status\":\"%s\",", mc_nbt_status_string(selected->nbt_status));
    if (selected->nbt_status == MC_NBT_OK) {
        printf("\"consumed\":%zu,\"trailing_bytes\":%zu,\"error_offset\":null,\"root\":",
               selected->nbt_consumed, selected->decoded.length - selected->nbt_consumed);
        print_summary(mc_nbt_document_root(selected->document), 0);
    } else {
        printf("\"consumed\":null,\"trailing_bytes\":null,\"error_offset\":%zu",
               selected->nbt_error.offset);
    }
    putchar('}');
}

static void print_entry(const mc_region_view *view, int32_t x, int32_t z,
                        const selected_record *selected) {
    const mc_region_entry *entry = mc_region_entry_at(view, x, z);
    mc_region_payload ignored;
    mc_region_status record_status = mc_region_record(view, x, z, &ignored);
    printf("{\"x\":%" PRId32 ",\"z\":%" PRId32 ",\"location\":%" PRIu32 ",\"timestamp\":%" PRIu32
           ",\"offset\":%" PRIu64 ",\"extent_bytes\":%" PRIu64 ",\"issues\":%" PRIu32
           ",\"record_length\":%" PRIu32 ",\"selector\":%u,\"record_status\":\"%s\"",
           x, z, entry->location, entry->timestamp, entry->byte_offset, entry->extent_bytes,
           entry->issues, entry->record_length, (unsigned int)entry->selector,
           mc_region_status_string(record_status));
    if (selected != NULL) {
        print_selected(selected);
    }
    putchar('}');
}

static int inspect_file(const inspect_options *options) {
    uint8_t *input = NULL;
    size_t length = 0;
    input_status input_result = read_input(options->path, options->max_input, &input, &length);
    if (input_result != INPUT_OK) {
        const char *message = input_result == INPUT_LIMIT        ? "input byte limit exceeded"
                              : input_result == INPUT_ALLOCATION ? "input allocation failed"
                                                                 : "unable to open/read input file";
        fprintf(stderr, "mc_region_inspect: %s\n", message);
        return 2;
    }
    mc_region_limits region_limits = {options->max_input};
    mc_region_view view;
    mc_region_status status = mc_region_open(input, length, &region_limits, &view);
    if (status != MC_REGION_OK) {
        printf("{\"status\":\"%s\",\"file_bytes\":%zu}\n", mc_region_status_string(status), length);
        free(input);
        return status == MC_REGION_LIMIT ? 2 : 1;
    }
    int exit_code = view.trailing_bytes != 0 || view.invalid_entries != 0 ? 1 : 0;
    selected_record selected = {0};
    if (options->selected) {
        exit_code = inspect_selected(&view, options, &selected, exit_code);
    }
    printf("{\"status\":\"%s\",\"file_bytes\":%zu,\"trailing_bytes\":%zu,\"present_entries\":%zu,"
           "\"invalid_entries\":%zu,\"entries\":[",
           exit_code == 0 ? "ok" : "diagnostic", view.length, view.trailing_bytes,
           view.present_entries, view.invalid_entries);
    if (options->selected) {
        print_entry(&view, options->x, options->z, &selected);
    } else {
        bool first = true;
        for (size_t i = 0; i < MC_REGION_ENTRY_COUNT; ++i) {
            if (options->all || view.entries[i].location != 0) {
                if (!first) {
                    putchar(',');
                }
                first = false;
                print_entry(&view, (int32_t)(i % 32u), (int32_t)(i / 32u), NULL);
            }
        }
    }
    fputs("]}\n", stdout);
    mc_nbt_document_free(selected.document);
    mc_compression_buffer_free(&selected.decoded);
    free(input);
    return exit_code;
}

static int cli_main(int argc, cli_char **argv) {
    inspect_options options = {0};
    int result = parse_options(argc, argv, &options);
    if (result == 2) {
        usage(stderr);
    } else if (result == -1) {
        result = inspect_file(&options);
    }
    if (fflush(stdout) == EOF || ferror(stdout)) {
        fputs("mc_region_inspect: output I/O failure\n", stderr);
        return 2;
    }
    return result;
}

#ifdef _WIN32
int wmain(int argc, wchar_t **argv) {
    return cli_main(argc, argv);
}
#else
int main(int argc, char **argv) {
    return cli_main(argc, argv);
}
#endif
