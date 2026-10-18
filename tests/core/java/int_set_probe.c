#include "mc/core/java/int_set.h"
#include "mc/world/cache/acquire.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ENTRIES 1024u
#define MAX_REQUESTS 16384u

static int32_t order[MAX_ENTRIES];
static int32_t pending_keys[MAX_ENTRIES];
static mc_chunk_pending pending;
static bool pending_mode;

static bool snapshot(const mc_java_int_set *set)
{
    size_t count = 0;
    int32_t first = 0;
    mc_java_int_set_status status = mc_java_int_set_first(set, &first);
    if ((pending_mode && !mc_chunk_pending_validate(&pending)) ||
        (status != MC_JAVA_INT_SET_OK && status != MC_JAVA_INT_SET_EMPTY) ||
        mc_java_int_set_copy_order(set, order, MAX_ENTRIES, &count) != MC_JAVA_INT_SET_OK ||
        count != mc_java_int_set_size(set) || (count == 0) != (status == MC_JAVA_INT_SET_EMPTY) ||
        (count != 0 && first != order[0])) {
        return false;
    }
    printf("{\"size\":%zu,\"iteration\":[", count);
    for (size_t i = 0; i < count; ++i) {
        if (!mc_java_int_set_contains(set, order[i])) {
            return false;
        }
        printf("%s%" PRId32, i == 0 ? "" : ",", order[i]);
    }
    printf("],\"fresh_first\":");
    if (count == 0) {
        printf("null}");
    } else {
        printf("%" PRId32 "}", first);
    }
    return true;
}

static bool parse_key(const char *text, int32_t *key)
{
    char *end;
    errno = 0;
    intmax_t value = strtoimax(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value < INT32_MIN || value > INT32_MAX) {
        return false;
    }
    *key = (int32_t)value;
    return true;
}

static bool respond(mc_java_int_set **set, const char *line)
{
    if (strcmp(line, "new") == 0) {
        mc_java_int_set *replacement = NULL;
        mc_java_int_set_limits limits = mc_java_int_set_default_limits();
        limits.max_entries = MAX_ENTRIES;
        if (mc_java_int_set_create(&limits, &replacement) != MC_JAVA_INT_SET_OK) {
            return false;
        }
        mc_java_int_set_free(*set);
        *set = replacement;
        if (pending_mode &&
            !mc_chunk_pending_init_ordered(&pending, pending_keys, MAX_ENTRIES, *set)) {
            return false;
        }
        bool ok = snapshot(*set);
        putchar('\n');
        return ok;
    }
    if (*set == NULL) {
        return false;
    }
    if (strcmp(line, "snapshot") == 0) {
        bool ok = snapshot(*set);
        putchar('\n');
        return ok;
    }
    bool add = strncmp(line, "add ", 4) == 0;
    bool remove = strncmp(line, "remove ", 7) == 0;
    bool take = strcmp(line, "take") == 0;
    bool clear = strcmp(line, "clear") == 0;
    int32_t key = 0;
    if ((!add && !remove && !take && !clear) ||
        ((add || remove) && !parse_key(line + (add ? 4 : 7), &key))) {
        return false;
    }
    printf("{\"before\":");
    if (!snapshot(*set)) {
        return false;
    }
    bool empty = mc_java_int_set_size(*set) == 0;
    bool changed = false;
    mc_java_int_set_status status = MC_JAVA_INT_SET_OK;
    mc_chunk_acquire_status pending_status = MC_CHUNK_ACQUIRE_OK;
    if (add) {
        if (pending_mode)
            pending_status = mc_chunk_pending_add(&pending, key, &changed);
        else
            status = mc_java_int_set_add(*set, key, &changed);
    } else if (remove) {
        if (pending_mode)
            pending_status = mc_chunk_pending_remove(&pending, key, &changed);
        else
            status = mc_java_int_set_remove(*set, key, &changed);
    } else if (take && !empty) {
        if (pending_mode) {
            pending_status = mc_chunk_pending_first(&pending, &key);
            if (pending_status == MC_CHUNK_ACQUIRE_OK)
                pending_status = mc_chunk_pending_remove(&pending, key, &changed);
        } else {
            status = mc_java_int_set_first(*set, &key);
            if (status == MC_JAVA_INT_SET_OK)
                status = mc_java_int_set_remove(*set, key, &changed);
        }
    } else if (clear) {
        if (pending_mode)
            pending_status = mc_chunk_pending_clear(&pending);
        else
            mc_java_int_set_clear(*set);
    }
    if (status != MC_JAVA_INT_SET_OK || pending_status != MC_CHUNK_ACQUIRE_OK) {
        fprintf(stderr, "set operation: %s\n", mc_java_int_set_status_string(status));
        return false;
    }
    printf(",\"empty_before_action\":%s,\"selected\":", take ? (empty ? "true" : "false") : "null");
    if (take && !empty) {
        printf("%" PRId32, key);
    } else {
        printf("null");
    }
    printf(",\"result\":%s,\"after\":",
           (take && empty) || clear ? "null" : (changed ? "true" : "false"));
    if (!snapshot(*set)) {
        return false;
    }
    printf("}\n");
    return true;
}

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "--pending") == 0)
        pending_mode = true;
    else if (argc != 1)
        return 1;
    mc_java_int_set *set = NULL;
    char line[64];
    size_t requests = 0;
    int result = 0;
    while (fgets(line, (int)sizeof(line), stdin) != NULL) {
        char *newline = strchr(line, '\n');
        if (newline == NULL || ++requests > MAX_REQUESTS) {
            result = 1;
            break;
        }
        *newline = '\0';
        size_t length = strlen(line);
        if (length != 0 && line[length - 1] == '\r') {
            line[length - 1] = '\0';
        }
        if (!respond(&set, line) || ferror(stdout)) {
            result = 1;
            break;
        }
    }
    if (ferror(stdin) || fflush(stdout) != 0) {
        result = 1;
    }
    mc_java_int_set_free(set);
    if (result != 0) {
        fprintf(stderr, "invalid request or failed integer-set probe\n");
    }
    return result;
}
