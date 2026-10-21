#include "mc/world/saved_data/storage.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#endif

#define PROBE_SLOTS 16u
#define PROBE_UNITS 65536u
#define PROBE_REPEAT 65538u
#define PROBE_LINE_BYTES (4u * PROBE_UNITS + 64u)
#define PROBE_COMMANDS 100000u

/* Real NULL-store owners only. Sorted map output is a logical diagnostic, not
 * a serialization traversal or a claim about Java String argument identity. */
typedef enum line_status { LINE_OK, LINE_EOF, LINE_INVALID, LINE_IO } line_status;

static line_status read_line(char *line, bool *skip_lf) {
    size_t length = 0;
    for (;;) {
        const int c = fgetc(stdin);
        if (c == EOF) {
            if (ferror(stdin))
                return LINE_IO;
            if (length == 0)
                return LINE_EOF;
            line[length] = '\0';
            return LINE_OK;
        }
        if (*skip_lf) {
            *skip_lf = false;
            if (c == '\n')
                continue;
        }
        if (c == '\r' || c == '\n') {
            *skip_lf = c == '\r';
            line[length] = '\0';
            return LINE_OK;
        }
        if (c < 0x20 || c > 0x7e || length == PROBE_LINE_BYTES)
            return LINE_INVALID;
        line[length++] = (char)c;
    }
}

static bool words_split(char *line, char *words[4], size_t *count) {
    size_t length = 1;
    words[0] = line;
    for (char *p = line; *p != '\0'; ++p) {
        if (*p != ' ')
            continue;
        if (length == 4)
            return false;
        *p = '\0';
        words[length++] = p + 1;
    }
    for (size_t i = 0; i < length; ++i)
        if (words[i][0] == '\0')
            return false;
    *count = length;
    return true;
}

static bool number(const char *text, size_t maximum, size_t *out) {
    size_t value = 0;
    if (*text == '\0')
        return false;
    for (; *text != '\0'; ++text) {
        if (*text < '0' || *text > '9')
            return false;
        const size_t digit = (size_t)(*text - '0');
        if (digit > maximum || value > (maximum - digit) / 10u)
            return false;
        value = value * 10u + digit;
    }
    *out = value;
    return true;
}

static unsigned int hex_digit(unsigned char c) {
    if (c >= '0' && c <= '9')
        return (unsigned int)(c - '0');
    if (c >= 'a' && c <= 'f')
        return (unsigned int)(c - 'a') + 10u;
    if (c >= 'A' && c <= 'F')
        return (unsigned int)(c - 'A') + 10u;
    return 16;
}

static bool parse_key(const char *text, uint16_t *units, mc_nbt_string *key,
                      const mc_nbt_string **argument) {
    if (strcmp(text, "NULL") == 0) {
        *argument = NULL;
        return true;
    }
    if (strcmp(text, "EMPTY") == 0) {
        *key = (mc_nbt_string){NULL, 0};
        *argument = key;
        return true;
    }
    const size_t bytes = strlen(text);
    if (bytes == 0 || bytes % 4u != 0 || bytes / 4u > PROBE_UNITS)
        return false;
    for (size_t i = 0; i < bytes / 4u; ++i) {
        unsigned int value = 0;
        for (size_t j = 0; j < 4; ++j) {
            const unsigned int digit = hex_digit((unsigned char)text[4u * i + j]);
            if (digit == 16)
                return false;
            value = value * 16u + digit;
        }
        units[i] = (uint16_t)value;
    }
    *key = (mc_nbt_string){units, bytes / 4u};
    *argument = key;
    return true;
}

static int entry_order(const void *left, const void *right) {
    const mc_world_saved_id *a = *(const mc_world_saved_id *const *)left;
    const mc_world_saved_id *b = *(const mc_world_saved_id *const *)right;
    if (a->null_name != b->null_name)
        return a->null_name ? -1 : 1;
    const size_t common = a->name.length < b->name.length ? a->name.length : b->name.length;
    for (size_t i = 0; i < common; ++i) {
        if (a->name.units[i] < b->name.units[i])
            return -1;
        if (a->name.units[i] > b->name.units[i])
            return 1;
    }
    return a->name.length < b->name.length ? -1 : a->name.length > b->name.length ? 1 : 0;
}

static bool print_key(const mc_world_saved_id *entry) {
    if (entry->null_name)
        return fputs("NULL", stdout) >= 0;
    if (entry->name.length == 0)
        return fputs("EMPTY", stdout) >= 0;
    static const char hex[] = "0123456789abcdef";
    for (size_t i = 0; i < entry->name.length; ++i) {
        const unsigned int unit = entry->name.units[i];
        const char text[] = {hex[unit >> 12], hex[(unit >> 8) & 15u], hex[(unit >> 4) & 15u],
                             hex[unit & 15u]};
        if (fwrite(text, 1, sizeof(text), stdout) != sizeof(text))
            return false;
    }
    return true;
}

static bool print_map(mc_world_saved_data *owner, size_t slot) {
    const size_t count = mc_world_saved_data_count(owner);
    if (count > SIZE_MAX / sizeof(const mc_world_saved_id *))
        return false;
    const mc_world_saved_id **entries = count == 0 ? NULL : malloc(count * sizeof(*entries));
    if (count != 0 && entries == NULL)
        return false;
    bool ok = false;
    for (size_t i = 0; i < count; ++i) {
        entries[i] = mc_world_saved_data_id_at(owner, i);
        if (entries[i] == NULL || (entries[i]->name.length != 0 && entries[i]->name.units == NULL))
            goto cleanup;
    }
    if (count > 1)
        qsort(entries, count, sizeof(*entries), entry_order);
    if (printf("MAP\t%zu\t%zu\n", slot, count) < 0)
        goto cleanup;
    for (size_t i = 0; i < count; ++i) {
        if (printf("VALUE\t%zu\t", slot) < 0 || !print_key(entries[i]) ||
            printf("\t%d\n", (int)entries[i]->value) < 0)
            goto cleanup;
    }
    ok = fflush(stdout) == 0;
cleanup:
    free(entries);
    return ok;
}

static void free_owners(mc_world_saved_data *owners[PROBE_SLOTS]) {
    for (size_t i = 0; i < PROBE_SLOTS; ++i) {
        mc_world_saved_data_free(owners[i]);
        owners[i] = NULL;
    }
}

int main(int argc, char **argv) {
    (void)argv;
    mc_world_saved_data *owners[PROBE_SLOTS] = {0};
    char *line = NULL;
    uint16_t *units = NULL;
    size_t created = 0, advanced = 0, commands = 0;
    bool skip_lf = false;
    int result = 1;
    if (argc != 1) {
        fputs("saved-data probe: no arguments accepted\n", stderr);
        goto cleanup;
    }
#if defined(_WIN32)
    /* Expose all input bytes, including Ctrl-Z, and emit literal LF records. */
    if (_setmode(_fileno(stdin), _O_BINARY) == -1 || _setmode(_fileno(stdout), _O_BINARY) == -1) {
        fputs("saved-data probe: binary stream setup failed\n", stderr);
        goto cleanup;
    }
#endif
    line = malloc((size_t)PROBE_LINE_BYTES + 1u);
    units = malloc((size_t)PROBE_UNITS * sizeof(*units));
    if (line == NULL || units == NULL) {
        fputs("saved-data probe: input allocation failed\n", stderr);
        goto cleanup;
    }
    for (;;) {
        if (read_line(line, &skip_lf) != LINE_OK || commands == PROBE_COMMANDS) {
            fputs("saved-data probe: invalid input, missing END or command bound\n", stderr);
            goto cleanup;
        }
        ++commands;
        char *words[4] = {0};
        size_t count = 0, slot = 0, repeat = 0;
        if (!words_split(line, words, &count))
            goto invalid;
        if (strcmp(words[0], "END") == 0) {
            if (count != 1 || read_line(line, &skip_lf) != LINE_EOF)
                goto invalid;
            free_owners(owners);
            if (printf("END\t%zu\t%zu\n", created, advanced) < 0 || fflush(stdout) != 0) {
                fputs("saved-data probe: output failure\n", stderr);
                goto cleanup;
            }
            result = 0;
            break;
        }
        if (count < 2 || !number(words[1], PROBE_SLOTS - 1u, &slot))
            goto invalid;
        if (strcmp(words[0], "NEW") == 0) {
            if (count != 2 || owners[slot] != NULL)
                goto invalid;
            if (mc_world_saved_data_create(NULL, NULL, &owners[slot], NULL) != MC_WORLD_STORAGE_OK)
                goto native_failure;
            ++created;
            if (printf("NEW\t%zu\n", slot) < 0 || fflush(stdout) != 0) {
                fputs("saved-data probe: output failure\n", stderr);
                goto cleanup;
            }
            continue;
        }
        if (strcmp(words[0], "MAP") == 0) {
            if (count != 2 || owners[slot] == NULL)
                goto invalid;
            if (!print_map(owners[slot], slot)) {
                fputs("saved-data probe: snapshot or output failure\n", stderr);
                goto cleanup;
            }
            continue;
        }
        if (strcmp(words[0], "NEXT") == 0) {
            mc_nbt_string key = {0};
            const mc_nbt_string *argument = NULL;
            int32_t value = 0;
            if (count != 4 || owners[slot] == NULL || !number(words[2], PROBE_REPEAT, &repeat) ||
                repeat == 0 || !parse_key(words[3], units, &key, &argument))
                goto invalid;
            for (size_t i = 0; i < repeat; ++i)
                if (mc_world_saved_data_next_id(owners[slot], argument, &value, NULL) !=
                    MC_WORLD_STORAGE_OK)
                    goto native_failure;
            ++advanced;
            if (printf("RETURN\t%zu\t%zu\t%" PRId32 "\n", slot, repeat, value) < 0 ||
                fflush(stdout) != 0) {
                fputs("saved-data probe: output failure\n", stderr);
                goto cleanup;
            }
            continue;
        }
    invalid:
        fprintf(stderr, "saved-data probe: invalid command at %zu\n", commands);
        goto cleanup;
    native_failure:
        fprintf(stderr, "saved-data probe: native failure at %zu\n", commands);
        goto cleanup;
    }
cleanup:
    free_owners(owners);
    free(line);
    free(units);
    return result;
}
