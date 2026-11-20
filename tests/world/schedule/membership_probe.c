#include "mc/world/schedule/membership.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { RECORDS = 64, OPERATIONS = 4096, LINE = 192 };

static mc_scheduled_membership *membership;
static mc_scheduled_tick *records[RECORDS];

static void clear(void) {
    mc_scheduled_membership_destroy(membership);
    membership = NULL;
    for (size_t i = 0; i < RECORDS; ++i) {
        mc_scheduled_tick_release(records[i]);
        records[i] = NULL;
    }
}

static bool output(int result) {
    const mc_scheduled_tick *entries[RECORDS];
    size_t count = 0;
    if (mc_scheduled_membership_snapshot(membership, entries, RECORDS, &count) !=
        MC_SCHEDULED_MEMBERSHIP_OK)
        return false;
    printf("{\"result\":%s,\"entries\":[", result < 0 ? "null" : result ? "true" : "false");
    for (size_t i = 0; i < count; ++i) {
        size_t slot = 0;
        while (slot < RECORDS && records[slot] != entries[i])
            ++slot;
        if (slot == RECORDS)
            return false;
        printf("%s%zu", i == 0 ? "" : ",", slot);
    }
    printf("],\"records\":[");
    bool first = true;
    for (size_t i = 0; i < RECORDS; ++i) {
        if (records[i] == NULL)
            continue;
        mc_scheduled_tick_fields fields;
        if (mc_scheduled_tick_fields_get(records[i], &fields) != MC_SCHEDULED_TICK_OK)
            return false;
        printf("%s[%zu,%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId64 ",%" PRId64 "]",
               first ? "" : ",", i, fields.x, fields.y, fields.z, fields.block, fields.due_time,
               fields.sequence);
        first = false;
    }
    puts("]}");
    return !ferror(stdout);
}

static bool number(const char *text, int64_t *out) {
    errno = 0;
    char *end;
    intmax_t value = strtoimax(text, &end, 10);
    if (errno == ERANGE || end == text || *end != '\0' || value < INT64_MIN || value > INT64_MAX)
        return false;
    *out = (int64_t)value;
    return true;
}

static bool command(char *line) {
    char *words[9];
    size_t count = 0;
    char *cursor = line;
    while (*cursor != '\0') {
        while (*cursor == ' ')
            ++cursor;
        if (*cursor == '\0')
            break;
        if (count == 9)
            return false;
        words[count++] = cursor;
        while (*cursor != '\0' && *cursor != ' ')
            ++cursor;
        if (*cursor != '\0')
            *cursor++ = '\0';
    }
    if (count == 0)
        return false;
    if (count == 1 && strcmp(words[0], "new") == 0) {
        clear();
        return mc_scheduled_membership_create(RECORDS, SIZE_MAX, &membership) ==
                   MC_SCHEDULED_MEMBERSHIP_OK &&
               output(-1);
    }
    if (membership == NULL)
        return false;
    if (count == 1 && strcmp(words[0], "snapshot") == 0)
        return output(-1);
    int64_t values[8];
    for (size_t i = 1; i < count; ++i)
        if (!number(words[i], &values[i - 1]))
            return false;
    if (count == 2 && strcmp(words[0], "shift") == 0)
        return mc_scheduled_membership_shift_time(membership, values[0]) ==
                   MC_SCHEDULED_MEMBERSHIP_OK &&
               output(-1);
    if (count < 2 || values[0] < 0 || values[0] >= RECORDS)
        return false;
    const size_t slot = (size_t)values[0];
    if (count == 8 && strcmp(words[0], "record") == 0) {
        if (records[slot] != NULL)
            return false;
        for (size_t i = 1; i <= 4; ++i)
            if (values[i] < INT32_MIN || values[i] > INT32_MAX)
                return false;
        mc_scheduled_tick_sequence sequence = {values[5]};
        return mc_scheduled_tick_create(&sequence, (int32_t)values[1], (int32_t)values[2],
                                        (int32_t)values[3], (int32_t)values[4],
                                        &records[slot]) == MC_SCHEDULED_TICK_OK &&
               mc_scheduled_tick_time_set(records[slot], values[6]) == MC_SCHEDULED_TICK_OK &&
               output(-1);
    }
    if (records[slot] == NULL)
        return false;
    if (count == 3 && strcmp(words[0], "time") == 0)
        return mc_scheduled_tick_time_set(records[slot], values[1]) == MC_SCHEDULED_TICK_OK &&
               output(-1);
    if (count != 2)
        return false;
    bool result;
    mc_scheduled_membership_status status;
    if (strcmp(words[0], "add") == 0)
        status = mc_scheduled_membership_add(membership, records[slot], &result);
    else if (strcmp(words[0], "remove") == 0)
        status = mc_scheduled_membership_remove(membership, records[slot], &result);
    else if (strcmp(words[0], "contains") == 0)
        status = mc_scheduled_membership_contains(membership, records[slot], &result);
    else
        return false;
    return status == MC_SCHEDULED_MEMBERSHIP_OK && output(result ? 1 : 0);
}

int main(void) {
    char line[LINE];
    size_t count = 0;
    int result = 0;
    while (fgets(line, sizeof(line), stdin) != NULL) {
        char *newline = strchr(line, '\n');
        if (newline == NULL || ++count > OPERATIONS) {
            result = 1;
            break;
        }
        *newline = '\0';
        size_t length = strlen(line);
        if (length != 0 && line[length - 1] == '\r')
            line[length - 1] = '\0';
        if (!command(line)) {
            result = 1;
            break;
        }
    }
    if (ferror(stdin) || ferror(stdout) || count == 0 || fflush(stdout) != 0)
        result = 1;
    clear();
    if (result != 0)
        fputs("invalid scheduled membership probe request\n", stderr);
    return result;
}
