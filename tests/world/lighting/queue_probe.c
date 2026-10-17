#include "mc/world/lighting/queue.h"

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PROBE_CAPACITY 32768u

static mc_light_region storage[PROBE_CAPACITY];
static bool token_error;

static bool token(char *value, size_t capacity)
{
    int c;
    do {
        c = getchar();
    } while (c != EOF && isspace((unsigned char)c));
    if (c == EOF)
        return false;
    size_t length = 0;
    do {
        if (length + 1u >= capacity) {
            token_error = true;
            return false;
        }
        value[length++] = (char)c;
        c = getchar();
    } while (c != EOF && !isspace((unsigned char)c));
    value[length] = '\0';
    return true;
}

static bool integer(int32_t *value)
{
    char input[32], *end;
    if (!token(input, sizeof(input)))
        return false;
    errno = 0;
    long long parsed = strtoll(input, &end, 10);
    if (errno != 0 || end == input || *end != '\0' || parsed < INT32_MIN || parsed > INT32_MAX)
        return false;
    *value = (int32_t)parsed;
    return true;
}

static bool bounds(mc_light_bounds *value)
{
    return integer(&value->min_x) && integer(&value->min_y) && integer(&value->min_z) &&
           integer(&value->max_x) && integer(&value->max_y) && integer(&value->max_z);
}

static bool region(mc_light_region *value)
{
    int32_t kind;
    if (!integer(&kind) || (kind != 0 && kind != 1) || !bounds(&value->bounds))
        return false;
    value->kind = (mc_light_kind)kind;
    return true;
}

static void print_bounds(const mc_light_bounds *value)
{
    printf(" %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32, value->min_x,
           value->min_y, value->min_z, value->max_x, value->max_y, value->max_z);
}

int main(void)
{
    char command[8];
    mc_light_queue queue;
    if (!mc_light_queue_init(&queue, storage, PROBE_CAPACITY))
        return 2;
    size_t requests = 0;
    while (token(command, sizeof(command))) {
        if (++requests > 4096u)
            return 2;
        if (strcmp(command, "M") == 0) {
            mc_light_bounds stored, requested;
            if (!bounds(&stored) || !bounds(&requested))
                return 2;
            bool accepted = mc_light_bounds_merge(&stored, &requested);
            printf("M %d", accepted ? 1 : 0);
            print_bounds(&stored);
            putchar('\n');
        } else if (strcmp(command, "Q") == 0) {
            int32_t count;
            if (!integer(&count) || count < 0 || count > (int32_t)PROBE_CAPACITY)
                return 2;
            for (int32_t i = 0; i < count; ++i) {
                if (!region(&storage[(size_t)i]))
                    return 2;
            }
            queue.count = (size_t)count;
            printf("Q %zu\n", queue.count);
        } else if (strcmp(command, "E") == 0) {
            int32_t coalesce;
            mc_light_region requested;
            if (!integer(&coalesce) || (coalesce != 0 && coalesce != 1) || !region(&requested))
                return 2;
            mc_light_queue_result result =
                mc_light_queue_enqueue(&queue, &requested, coalesce != 0);
            if (result != MC_LIGHT_QUEUE_MERGED && result != MC_LIGHT_QUEUE_APPENDED)
                return 1;
            printf("E %zu", queue.count);
            for (size_t i = 0; i < queue.count; ++i) {
                printf(" %d", (int)queue.entries[i].kind);
                print_bounds(&queue.entries[i].bounds);
            }
            putchar('\n');
        } else {
            return 2;
        }
    }
    return token_error || ferror(stdin) || ferror(stdout) ? 2 : 0;
}
