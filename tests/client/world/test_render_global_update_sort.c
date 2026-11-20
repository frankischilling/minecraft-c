/* Authored JDK sort-kernel controls. RAW canonical DK structs provide distinct
 * sortable identities only; no constructor/World/MD/source-call parity is
 * inferred. The production sorter copies pointers and invokes this callback. */
#if defined(_MSC_VER)
/* These portable stdio reads contain numeric conversions only. */
#define _CRT_SECURE_NO_WARNINGS
#endif
#include "client/world/render_global_update_sort.h"
#include "client/world/source_chunk_renderer_internal.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct trace_entry {
    int left, right, result;
} trace_entry;

typedef struct sort_control {
    mc_client_source_dk *values;
    mc_client_render_global_error *error;
    int mode, stop, calls;
    uint64_t hash;
    trace_entry *trace;
    size_t capacity;
    bool failed;
} sort_control;

static void hash_value(sort_control *c, int value) {
    c->hash = (c->hash ^ (uint64_t)(int64_t)value) * UINT64_C(1099511628211);
}

static bool compare(void *context, mc_client_source_dk *left, mc_client_source_dk *right,
                    int *out) {
    sort_control *c = context;
    if ((size_t)c->calls == c->capacity) {
        c->failed = true;
        return false;
    }
    trace_entry *entry = &c->trace[c->calls++];
    entry->left = (int)(left - c->values);
    entry->right = (int)(right - c->values);
    hash_value(c, entry->left);
    hash_value(c, entry->right);
    if (c->calls == c->stop) {
        entry->result = 91;
        hash_value(c, 91);
        c->error->status = MC_CLIENT_RENDER_GLOBAL_CX_FAILED;
        c->error->operation = "authored comparator stop";
        c->error->child.pc = 12345;
        return false;
    }
    const int a = left->fields.q, b = right->fields.q;
    int order;
    if (c->mode == 0) {
        if (left->fields.o != right->fields.o)
            order = left->fields.o ? 1 : -1;
        else if (a != b)
            order = a < b ? 1 : -1;
        else
            order = left->fields.w < right->fields.w ? 1 : -1;
    } else if (c->mode == 1) {
        order = a < b ? -1 : a > b ? 1 : 0;
    } else if (c->mode == 2) {
        const int relation = (a - b + 3072) % 3;
        order = relation == 1 ? 1 : relation == 2 ? -1 : 0;
    } else {
        order = a < b ? 1 : -1;
    }
    entry->result = order;
    hash_value(c, order);
    *out = order;
    return true;
}

static bool expected(FILE *file, int count, int status, const sort_control *c,
                     mc_client_source_dk *const *array) {
    int wanted_status, wanted_calls;
    uint64_t wanted_hash;
    if (fscanf(file, "%d%d%" SCNx64, &wanted_status, &wanted_calls, &wanted_hash) != 3 ||
        wanted_status != status || wanted_calls != c->calls || wanted_hash != c->hash)
        return false;
    for (int i = 0; i < count; ++i) {
        int wanted;
        if (fscanf(file, "%d", &wanted) != 1 || wanted != array[i] - c->values)
            return false;
    }
    for (int i = 0; i < c->calls; ++i) {
        trace_entry wanted;
        if (fscanf(file, "%d%d%d", &wanted.left, &wanted.right, &wanted.result) != 3 ||
            wanted.left != c->trace[i].left || wanted.right != c->trace[i].right ||
            wanted.result != c->trace[i].result)
            return false;
    }
    return true;
}

static bool observe(int count, int status, const sort_control *c,
                    mc_client_source_dk *const *array) {
    if (printf("%d %d %016" PRIx64, status, c->calls, c->hash) < 0)
        return false;
    for (int i = 0; i < count; ++i)
        if (printf(" %d", (int)(array[i] - c->values)) < 0)
            return false;
    for (int i = 0; i < c->calls; ++i)
        if (printf(" %d %d %d", c->trace[i].left, c->trace[i].right, c->trace[i].result) < 0)
            return false;
    return putchar('\n') != EOF;
}

int main(int argc, char **argv) {
    const bool checking = argc == 4 && strcmp(argv[1], "--check") == 0;
    if (!checking && !(argc == 3 && strcmp(argv[1], "--observe") == 0)) {
        fprintf(stderr, "usage: %s --check cases.txt expected.txt | --observe cases.txt\n",
                argv[0]);
        return 2;
    }
    FILE *input = fopen(argv[2], "r");
    FILE *oracle = checking ? fopen(argv[3], "r") : NULL;
    if (input == NULL || (checking && oracle == NULL)) {
        if (input != NULL)
            fclose(input);
        return 2;
    }
    unsigned cases = 0;
    int result = 0, count, mode, stop, scanned;
    while ((scanned = fscanf(input, "%d%d%d", &count, &mode, &stop)) == 3) {
        if (count < 0 || count > 4096 || mode < 0 || mode > 3 || stop < 0) {
            result = 2;
            break;
        }
        mc_client_source_dk *values = calloc((size_t)count + 1, sizeof(*values));
        mc_client_source_dk **array = calloc((size_t)count + 1, sizeof(*array));
        const size_t trace_capacity = (size_t)count * 64 + 1;
        trace_entry *trace = calloc(trace_capacity, sizeof(*trace));
        if (values == NULL || array == NULL || trace == NULL) {
            free(values);
            free(array);
            free(trace);
            result = 2;
            break;
        }
        for (int i = 0; i < count; ++i) {
            int key, ordinal, visible;
            if (fscanf(input, "%d%d%d", &key, &ordinal, &visible) != 3 || key < 0 || key > 1024 ||
                ordinal < 0 || ordinal > 1024 || (visible != 0 && visible != 1)) {
                result = 2;
                break;
            }
            values[i].fields.q = key;
            values[i].fields.w = ordinal;
            values[i].fields.o = visible != 0;
            array[i] = &values[i];
        }
        mc_client_render_global_error error = {0};
        sort_control control = {.values = values,
                                .error = &error,
                                .mode = mode,
                                .stop = stop,
                                .hash = UINT64_C(0xcbf29ce484222325),
                                .trace = trace,
                                .capacity = trace_capacity};
        if (result == 0) {
            const bool sorted =
                mc_render_global_update_sort(array, (size_t)count, &control, compare, &error);
            const int status = sorted ? 0
                               : error.status == MC_CLIENT_RENDER_GLOBAL_SOURCE_ILLEGAL_ARGUMENT
                                   ? 2
                                   : 1;
            if (control.failed ||
                (status == 1 && (error.status != MC_CLIENT_RENDER_GLOBAL_CX_FAILED ||
                                 error.child.pc != 12345 || error.operation == NULL ||
                                 strcmp(error.operation, "authored comparator stop") != 0)) ||
                (status == 2 &&
                 (error.native_policy || error.source_pc != 411 || error.child.class_name == NULL ||
                  strcmp(error.child.class_name, "java/util/TimSort") != 0 ||
                  (error.child.pc != 607 && error.child.pc != 675))) ||
                !(checking ? expected(oracle, count, status, &control, array)
                           : observe(count, status, &control, array))) {
                fprintf(stderr, "sort fixture case %u differs; status=%d calls=%d\n", cases, status,
                        control.calls);
                result = 1;
            }
        }
        free(values);
        free(array);
        free(trace);
        if (result != 0)
            break;
        ++cases;
    }
    if (result == 0 && (scanned != EOF || ferror(input) || cases == 0 ||
                        (checking && (fscanf(oracle, "%d", &count) != EOF || ferror(oracle)))))
        result = 2;
    if (fclose(input) != 0)
        result = 2;
    if (oracle != NULL && fclose(oracle) != 0)
        result = 2;
    if (result == 0 && printf("Sort kernel fixture: %u cases\n", cases) < 0)
        result = 2;
    return result;
}
