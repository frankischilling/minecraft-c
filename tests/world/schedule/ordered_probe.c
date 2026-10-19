#include "mc/world/schedule/ordered.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#endif

#define PROBE_RECORD_LIMIT 128u
#define PROBE_OPERATION_LIMIT 100000u
#define PROBE_LINE_LIMIT 160u
#define PROBE_ALLOCATION_LIMIT ((size_t)128u * 1024u * 1024u)

/* Native side of ScheduledOrderedObservation's public-API protocol. */
typedef struct ordered_probe_state {
    mc_scheduled_ordered *ordered;
    mc_scheduled_tick *records[PROBE_RECORD_LIMIT];
} ordered_probe_state;

typedef enum probe_line_status {
    PROBE_LINE_OK,
    PROBE_LINE_END,
    PROBE_LINE_INVALID,
    PROBE_LINE_IO_ERROR
} probe_line_status;

static void clear_state(ordered_probe_state *state)
{
    mc_scheduled_ordered_destroy(state->ordered);
    state->ordered = NULL;
    for (size_t i = 0; i < PROBE_RECORD_LIMIT; ++i) {
        mc_scheduled_tick_release(state->records[i]);
        state->records[i] = NULL;
    }
}

static bool reset_state(ordered_probe_state *state)
{
    clear_state(state);
    return mc_scheduled_ordered_create(PROBE_OPERATION_LIMIT, PROBE_ALLOCATION_LIMIT,
                                       &state->ordered) == MC_SCHEDULED_ORDERED_OK;
}

/* BufferedReader.readLine accepts LF, CR and CRLF, excluding the terminator. */
static probe_line_status read_line(char line[PROBE_LINE_LIMIT + 1u], bool *skip_lf)
{
    size_t length = 0;
    for (;;) {
        const int character = fgetc(stdin);
        if (character == EOF) {
            if (ferror(stdin)) {
                return PROBE_LINE_IO_ERROR;
            }
            if (length == 0) {
                return PROBE_LINE_END;
            }
            line[length] = '\0';
            return PROBE_LINE_OK;
        }
        if (*skip_lf) {
            *skip_lf = false;
            if (character == '\n') {
                continue;
            }
        }
        if (character == '\r') {
            *skip_lf = true;
            line[length] = '\0';
            return PROBE_LINE_OK;
        }
        if (character == '\n') {
            line[length] = '\0';
            return PROBE_LINE_OK;
        }
        if (character < 0x20 || character > 0x7e || length == PROBE_LINE_LIMIT) {
            return PROBE_LINE_INVALID;
        }
        line[length++] = (char)character;
    }
}

/* Keep empty fields, as Java split(" ", -1) does. No command has over four. */
static bool split_words(char *line, char *words[4], size_t *out_count)
{
    size_t count = 1;
    words[0] = line;
    for (char *cursor = line; *cursor != '\0'; ++cursor) {
        if (*cursor == ' ') {
            if (count == 4u) {
                return false;
            }
            *cursor = '\0';
            words[count++] = cursor + 1;
        }
    }
    *out_count = count;
    return true;
}

static bool parse_i64(const char *text, int64_t *out)
{
    const char *cursor = text;
    if (*cursor == '+' || *cursor == '-') {
        ++cursor;
    }
    if (*cursor == '\0') {
        return false;
    }
    for (; *cursor != '\0'; ++cursor) {
        if (*cursor < '0' || *cursor > '9') {
            return false;
        }
    }
    errno = 0;
    char *end = NULL;
    const intmax_t value = strtoimax(text, &end, 10);
    if (errno == ERANGE || end == text || *end != '\0' || value < INT64_MIN || value > INT64_MAX) {
        return false;
    }
    *out = (int64_t)value;
    return true;
}

static bool parse_id(const char *text, size_t *out)
{
    int64_t value = 0;
    if (!parse_i64(text, &value) || value < 0 || value >= (int64_t)PROBE_RECORD_LIMIT) {
        return false;
    }
    *out = (size_t)value;
    return true;
}

static mc_scheduled_tick *require_record(ordered_probe_state *state, const char *text)
{
    size_t id = 0;
    return parse_id(text, &id) ? state->records[id] : NULL;
}

static bool create_record(ordered_probe_state *state, char *words[4])
{
    size_t id = 0;
    int64_t time = 0, sequence_value = 0;
    if (!parse_id(words[1], &id) || state->records[id] != NULL || !parse_i64(words[2], &time) ||
        !parse_i64(words[3], &sequence_value)) {
        return false;
    }
    mc_scheduled_tick_sequence sequence = {.next = sequence_value};
    mc_scheduled_tick *record = NULL;
    if (mc_scheduled_tick_create(&sequence, (int32_t)id, 0, 0, 1, &record) !=
        MC_SCHEDULED_TICK_OK) {
        return false;
    }
    if (mc_scheduled_tick_time_set(record, time) != MC_SCHEDULED_TICK_OK) {
        mc_scheduled_tick_release(record);
        return false;
    }
    state->records[id] = record;
    return true;
}

static bool execute_command(ordered_probe_state *state, char *words[4], size_t count,
                            const char **result)
{
    *result = "-";
    if (strcmp(words[0], "new") == 0) {
        return count == 1u && reset_state(state);
    }
    if (strcmp(words[0], "record") == 0) {
        return count == 4u && create_record(state, words);
    }
    if (strcmp(words[0], "add") == 0 || strcmp(words[0], "remove") == 0) {
        if (count != 2u) {
            return false;
        }
        mc_scheduled_tick *record = require_record(state, words[1]);
        if (record == NULL) {
            return false;
        }
        bool changed = false;
        const mc_scheduled_ordered_status status =
            strcmp(words[0], "add") == 0
                ? mc_scheduled_ordered_add(state->ordered, record, &changed)
                : mc_scheduled_ordered_remove(state->ordered, record, &changed);
        if (status != MC_SCHEDULED_ORDERED_OK) {
            return false;
        }
        *result = changed ? "1" : "0";
        return true;
    }
    if (strcmp(words[0], "time") == 0) {
        if (count != 3u) {
            return false;
        }
        mc_scheduled_tick *record = require_record(state, words[1]);
        int64_t time = 0;
        return record != NULL && parse_i64(words[2], &time) &&
               mc_scheduled_tick_time_set(record, time) == MC_SCHEDULED_TICK_OK;
    }
    if (strcmp(words[0], "shift") == 0) {
        int64_t delta = 0;
        if (count != 2u || !parse_i64(words[1], &delta)) {
            return false;
        }
        for (size_t i = 0; i < PROBE_RECORD_LIMIT; ++i) {
            if (state->records[i] != NULL &&
                mc_scheduled_tick_time_shift(state->records[i], delta) != MC_SCHEDULED_TICK_OK) {
                return false;
            }
        }
        return true;
    }
    return strcmp(words[0], "first") == 0 && count == 1u;
}

static bool print_snapshot(const ordered_probe_state *state, const char *command,
                           const char *result)
{
    mc_scheduled_tick *first = NULL;
    mc_scheduled_tick_fields fields = {0};
    const mc_scheduled_ordered_status status = mc_scheduled_ordered_first(state->ordered, &first);
    if (status != MC_SCHEDULED_ORDERED_OK && status != MC_SCHEDULED_ORDERED_EMPTY) {
        mc_scheduled_tick_release(first);
        return false;
    }
    if (status == MC_SCHEDULED_ORDERED_OK &&
        mc_scheduled_tick_fields_get(first, &fields) != MC_SCHEDULED_TICK_OK) {
        mc_scheduled_tick_release(first);
        return false;
    }
    mc_scheduled_tick_release(first);
    int32_t size = 0;
    if (mc_scheduled_ordered_size(state->ordered, &size) != MC_SCHEDULED_ORDERED_OK) {
        return false;
    }
    const int written =
        status == MC_SCHEDULED_ORDERED_EMPTY
            ? printf("%s %s %" PRId32 " empty\n", command, result, size)
            : printf("%s %s %" PRId32 " %" PRId32 "\n", command, result, size, fields.x);
    return written >= 0 && fflush(stdout) == 0;
}

int main(int argc, char **argv)
{
    (void)argv;
    ordered_probe_state state = {0};
    int result = 1;
    size_t operations = 0;
    bool skip_lf = false;
    char line[PROBE_LINE_LIMIT + 1u];
    if (argc != 1) {
        fputs("ordered probe: no arguments accepted\n", stderr);
        goto cleanup;
    }
#if defined(_WIN32)
    /* Keep CRT text-mode Ctrl-Z handling from hiding malformed input bytes. */
    if (_setmode(_fileno(stdin), _O_BINARY) == -1) {
        fputs("ordered probe: binary input mode failed\n", stderr);
        goto cleanup;
    }
#endif
    if (!reset_state(&state)) {
        fputs("ordered probe: collection construction failed\n", stderr);
        goto cleanup;
    }
    for (;;) {
        const probe_line_status line_status = read_line(line, &skip_lf);
        if (line_status == PROBE_LINE_END) {
            break;
        }
        if (line_status != PROBE_LINE_OK || operations == PROBE_OPERATION_LIMIT) {
            fputs("ordered probe: invalid input, read failure or input limit\n", stderr);
            goto cleanup;
        }
        ++operations;
        char *words[4] = {0};
        size_t count = 0;
        const char *command_result = NULL;
        if (!split_words(line, words, &count) ||
            !execute_command(&state, words, count, &command_result)) {
            fprintf(stderr, "ordered probe: invalid command or native failure at operation %zu\n",
                    operations);
            goto cleanup;
        }
        if (!print_snapshot(&state, words[0], command_result)) {
            fprintf(stderr, "ordered probe: snapshot or output failure at operation %zu\n",
                    operations);
            goto cleanup;
        }
    }
    if (fflush(stdout) != 0) {
        fputs("ordered probe: output flush failed\n", stderr);
        goto cleanup;
    }
    result = 0;
cleanup:
    clear_state(&state);
    return result;
}
