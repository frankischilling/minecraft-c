#include "mc/block/registry/registry.h"

#include "../../../src/block/registry/redstone_set_internal.h"

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { MAX_KEYS = 256, MAX_STEPS = 256, MAX_LINE = 128, MAX_INPUT = 65536 };

typedef struct identity_request {
    mc_registry_redstone_position key;
    bool supplied;
    int32_t value;
} identity_request;

typedef struct probe {
    mc_registry_classes *classes;
    mc_registry_redstone_set *set;
    mc_registry_redstone_snapshot *snapshot;
    bool has_hash[MAX_KEYS + 1];
    int32_t hashes[MAX_KEYS + 1];
    size_t hash_count, request_count, reported_requests;
    identity_request requests[MAX_KEYS];
    const void *last_table;
    uint32_t last_capacity;
    uint64_t table_generation, snapshot_generation;
    size_t steps;
} probe;

static mc_registry_status measured_identity(void *context, const mc_registry_redstone_position *key,
                                            int32_t *out) {
    probe *p = context;
    if (p == NULL || key == NULL || out == NULL || p->request_count == MAX_KEYS)
        return MC_REGISTRY_LIMIT;
    identity_request *request = &p->requests[p->request_count++];
    request->key = *key;
    request->supplied =
        key->identity > 0 && key->identity <= MAX_KEYS && p->has_hash[(size_t)key->identity];
    if (!request->supplied)
        return MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY;
    request->value = p->hashes[(size_t)key->identity];
    *out = request->value;
    return MC_REGISTRY_OK;
}

static void print_key(const mc_registry_redstone_position *key) {
    (void)printf("{\"serial\":%" PRIu64 ",\"x\":%" PRId32 ",\"y\":%" PRId32 ",\"z\":%" PRId32
                 ",\"identity_hash_ready\":%s,\"identity_hash\":",
                 key->identity, key->x, key->y, key->z,
                 key->identity_hash_ready ? "true" : "false");
    if (key->identity_hash_ready)
        (void)printf("%" PRId32, key->identity_hash);
    else
        (void)fputs("null", stdout);
    (void)putchar('}');
}

static bool state(probe *p, const mc_registry_redstone_set_view *view) {
    uint32_t load_factor;
    _Static_assert(sizeof(load_factor) == sizeof(view->load_factor), "binary32 load factor");
    memcpy(&load_factor, &view->load_factor, sizeof(load_factor));
    if (view->size > MAX_KEYS || view->capacity > 1024 ||
        (view->table == NULL) != (view->capacity == 0))
        return false;
    if (view->table != p->last_table || view->capacity != p->last_capacity) {
        ++p->table_generation;
        p->last_table = view->table;
        p->last_capacity = view->capacity;
    }
    (void)printf("{\"set_address\":\"%" PRIxPTR "\",\"map_address\":\"%" PRIxPTR
                 "\",\"table_address\":\"%" PRIxPTR "\",\"table_generation\":%" PRIu64
                 ",\"size\":%zu,\"capacity\":%" PRIu32 ",\"threshold\":%" PRId32
                 ",\"mod_count\":%" PRId32 ",\"load_factor_bits\":%" PRIu32
                 ",\"constructed_keys\":%" PRIu64 ",\"nodes\":[",
                 (uintptr_t)p->set, (uintptr_t)view->map, (uintptr_t)view->table,
                 p->table_generation, view->size, view->capacity, view->threshold, view->mod_count,
                 load_factor, view->constructed_keys);
    for (size_t i = 0; i < view->size; ++i) {
        mc_registry_redstone_node_view node;
        if (mc_registry_redstone_set_node_at(p->set, i, &node) != MC_REGISTRY_OK)
            return false;
        if (i != 0)
            (void)putchar(',');
        (void)printf("{\"identity\":%" PRIu64 ",\"next\":%" PRIu64 ",\"previous\":%" PRIu64
                     ",\"parent\":%" PRIu64 ",\"left\":%" PRIu64 ",\"right\":%" PRIu64
                     ",\"hash\":%" PRIu32 ",\"bucket\":%" PRIu32 ",\"tree\":%s,\"red\":%s,\"key\":",
                     node.identity, node.next, node.previous, node.parent, node.left, node.right,
                     node.hash, node.bucket, node.tree ? "true" : "false",
                     node.red ? "true" : "false");
        print_key(&node.key);
        (void)putchar('}');
    }
    (void)fputs("],\"snapshot\":", stdout);
    if (p->snapshot == NULL)
        (void)fputs("null", stdout);
    else {
        const size_t count = mc_registry_redstone_snapshot_size(p->snapshot);
        if (count > MAX_KEYS)
            return false;
        (void)printf("{\"address\":\"%" PRIxPTR "\",\"generation\":%" PRIu64
                     ",\"size\":%zu,\"keys\":[",
                     (uintptr_t)p->snapshot, p->snapshot_generation, count);
        for (size_t i = 0; i < count; ++i) {
            mc_registry_redstone_position key;
            if (mc_registry_redstone_snapshot_at(p->snapshot, i, &key) != MC_REGISTRY_OK)
                return false;
            if (i != 0)
                (void)putchar(',');
            print_key(&key);
        }
        (void)fputs("]}", stdout);
    }
    (void)putchar('}');
    return !ferror(stdout);
}

static bool emit(probe *p, const char *operation, bool argument, uint64_t serial, int32_t x,
                 int32_t y, int32_t z, bool has_result, bool changed, mc_registry_status status) {
    mc_registry_redstone_set_view view;
    if (p->steps == MAX_STEPS || mc_registry_redstone_set_inspect(p->set, &view) != MC_REGISTRY_OK)
        return false;
    if (argument && serial == 0)
        serial = view.constructed_keys;
    (void)printf(
        "{\"kind\":\"step\",\"ordinal\":%zu,\"operation\":\"%s\",\"status\":%d,\"argument\":",
        p->steps++, operation, (int)status);
    if (argument)
        (void)printf("{\"serial\":%" PRIu64 ",\"x\":%" PRId32 ",\"y\":%" PRId32 ",\"z\":%" PRId32
                     "}",
                     serial, x, y, z);
    else
        (void)fputs("null", stdout);
    (void)fputs(",\"result\":", stdout);
    (void)fputs(has_result && status == MC_REGISTRY_OK ? (changed ? "true" : "false") : "null",
                stdout);
    (void)fputs(",\"identity_requests\":[", stdout);
    for (size_t i = p->reported_requests; i < p->request_count; ++i) {
        const identity_request *request = &p->requests[i];
        if (i != p->reported_requests)
            (void)putchar(',');
        (void)printf("{\"index\":%zu,\"serial\":%" PRIu64 ",\"x\":%" PRId32 ",\"y\":%" PRId32
                     ",\"z\":%" PRId32 ",\"already_ready\":%s,\"supplied\":%s,\"value\":",
                     i, request->key.identity, request->key.x, request->key.y, request->key.z,
                     request->key.identity_hash_ready ? "true" : "false",
                     request->supplied ? "true" : "false");
        if (request->supplied)
            (void)printf("%" PRId32, request->value);
        else
            (void)fputs("null", stdout);
        (void)putchar('}');
    }
    p->reported_requests = p->request_count;
    (void)fputs("],\"state\":", stdout);
    if (!state(p, &view))
        return false;
    (void)fputs("}\n", stdout);
    return !ferror(stdout);
}

static bool uint_token(const char *text, uint64_t *out) {
    if (text == NULL || *text == '\0')
        return false;
    for (const char *p = text; *p != '\0'; ++p)
        if (*p < '0' || *p > '9')
            return false;
    errno = 0;
    char *end;
    const uintmax_t value = strtoumax(text, &end, 10);
    if (errno != 0 || *end != '\0' || value > UINT64_MAX)
        return false;
    *out = (uint64_t)value;
    return true;
}

static bool int_token(const char *text, int32_t *out) {
    if (text == NULL || *text == '\0')
        return false;
    const char *p = text;
    if (*p == '-')
        ++p;
    if (*p == '\0')
        return false;
    for (; *p != '\0'; ++p)
        if (*p < '0' || *p > '9')
            return false;
    errno = 0;
    char *end;
    const intmax_t value = strtoimax(text, &end, 10);
    if (errno != 0 || *end != '\0' || value < INT32_MIN || value > INT32_MAX)
        return false;
    *out = (int32_t)value;
    return true;
}

static int line(FILE *input, char buffer[MAX_LINE], size_t *bytes) {
    size_t count = 0;
    for (;;) {
        const int c = fgetc(input);
        if (c == EOF)
            return !ferror(input) && count == 0 ? 0 : -1;
        if (++*bytes > MAX_INPUT || c == 0 || c > 127 || count + 1u == MAX_LINE)
            return -1;
        if (c == '\n') {
            buffer[count] = '\0';
            return 1;
        }
        if (c < 32)
            return -1;
        buffer[count++] = (char)c;
    }
}

static size_t tokens(char *buffer, char *out[5]) {
    size_t count = 0;
    char *p = buffer;
    if (*p == '\0' || *p == ' ')
        return 0;
    for (;;) {
        if (count == 5)
            return 0;
        out[count++] = p;
        while (*p != '\0' && *p != ' ')
            ++p;
        if (*p == '\0')
            return count;
        *p++ = '\0';
        if (*p == '\0' || *p == ' ')
            return 0;
    }
}

static FILE *open_input(const char *path) {
#ifdef _MSC_VER
    FILE *input = NULL;
    return fopen_s(&input, path, "rb") == 0 ? input : NULL;
#else
    return fopen(path, "rb");
#endif
}

int main(int argc, char **argv) {
    int result = 1;
    FILE *input = stdin;
    mc_materials *materials = NULL;
    mc_java_random_profile *profile = NULL;
    probe p = {0};
    char buffer[MAX_LINE], *word[5];
    size_t bytes = 0, count;
    bool begun = false, finished = false;
    if (argc == 3 && strcmp(argv[1], "--input") == 0)
        input = open_input(argv[2]);
    else if (argc != 1)
        goto cleanup;
    if (input == NULL || line(input, buffer, &bytes) != 1 || (count = tokens(buffer, word)) != 3 ||
        strcmp(word[0], "REDSTONE_SET") != 0 || strcmp(word[1], "1") != 0 ||
        (strcmp(word[2], "client") != 0 && strcmp(word[2], "server") != 0))
        goto cleanup;
    const mc_registry_side side =
        strcmp(word[2], "client") == 0 ? MC_REGISTRY_CLIENT : MC_REGISTRY_SERVER;
    for (;;) {
        const int read_status = line(input, buffer, &bytes);
        if (read_status == 0) {
            if (finished)
                break;
            goto cleanup;
        }
        if (read_status != 1 || finished || (count = tokens(buffer, word)) == 0)
            goto cleanup;
        if (!begun) {
            if (strcmp(word[0], "HASH") == 0 && count == 3) {
                uint64_t serial;
                int32_t value;
                if (!uint_token(word[1], &serial) || serial == 0 || serial > MAX_KEYS ||
                    p.has_hash[(size_t)serial] || !int_token(word[2], &value))
                    goto cleanup;
                p.has_hash[(size_t)serial] = true;
                p.hashes[(size_t)serial] = value;
                ++p.hash_count;
                continue;
            }
            if (count != 1 || strcmp(word[0], "BEGIN") != 0 ||
                mc_java_random_profile_create(4u * 1024u * 1024u, &profile) !=
                    MC_JAVA_RANDOM_PROFILE_OK ||
                !mc_materials_create(&materials))
                goto cleanup;
            const mc_registry_options options = {.side = side,
                                                 .random_profile = profile,
                                                 .materials = materials,
                                                 .max_allocation_bytes = 8u * 1024u * 1024u};
            if (mc_registry_classes_allocate(&options, &p.classes, NULL) != MC_REGISTRY_OK ||
                mc_registry_redstone_set_create(p.classes, &p.set) != MC_REGISTRY_OK ||
                mc_registry_redstone_set_identity_profile(
                    p.set, p.hash_count == 0 ? NULL : measured_identity, &p) != MC_REGISTRY_OK)
                goto cleanup;
            begun = true;
            (void)printf(
                "{\"kind\":\"header\",\"schema\":1,\"scope\":\"conditional-position-set-jdk17\","
                "\"side\":\"%s\",\"transport\":\"%s\",\"identity_input_count\":%zu}\n",
                side == MC_REGISTRY_CLIENT ? "client" : "server", argc == 1 ? "stdin" : "file",
                p.hash_count);
            continue;
        }
        mc_registry_status status = MC_REGISTRY_OK;
        bool changed = false, argument = false, has_result = false;
        int32_t x = 0, y = 0, z = 0;
        uint64_t serial = 0;
        const char *operation;
        if (count == 1 && strcmp(word[0], "INITIAL") == 0)
            operation = "initial";
        else if (count == 4 && (strcmp(word[0], "ADD") == 0 || strcmp(word[0], "REMOVE") == 0)) {
            if (!int_token(word[1], &x) || !int_token(word[2], &y) || !int_token(word[3], &z))
                goto cleanup;
            argument = has_result = true;
            if (strcmp(word[0], "ADD") == 0) {
                operation = "add_fresh";
                status = mc_registry_redstone_set_add_fresh(p.set, x, y, z, &changed);
            } else {
                operation = "remove_equal_fresh";
                status = mc_registry_redstone_set_remove_fresh(p.set, x, y, z, &changed);
            }
        } else if (count == 2 && strcmp(word[0], "REUSE") == 0) {
            if (!uint_token(word[1], &serial))
                goto cleanup;
            mc_registry_redstone_set_view view;
            bool found = false;
            if (mc_registry_redstone_set_inspect(p.set, &view) != MC_REGISTRY_OK ||
                view.size > MAX_KEYS)
                goto cleanup;
            for (size_t i = 0; i < view.size; ++i) {
                mc_registry_redstone_node_view node;
                if (mc_registry_redstone_set_node_at(p.set, i, &node) != MC_REGISTRY_OK)
                    goto cleanup;
                if (node.key.identity == serial) {
                    x = node.key.x;
                    y = node.key.y;
                    z = node.key.z;
                    serial = node.key.identity;
                    found = true;
                    break;
                }
            }
            if (!found)
                goto cleanup;
            operation = "add_same_reference";
            argument = has_result = true;
            status = mc_registry_redstone_set_readd_existing(p.set, serial, &changed);
        } else if (count == 1 && strcmp(word[0], "COPY") == 0) {
            operation = "snapshot_arraylist";
            mc_registry_redstone_snapshot *next = NULL;
            status = mc_registry_redstone_snapshot_copy(p.set, &next);
            if (status == MC_REGISTRY_OK) {
                mc_registry_redstone_snapshot_release(p.snapshot);
                p.snapshot = next;
                ++p.snapshot_generation;
            }
        } else if (count == 1 && strcmp(word[0], "CLEAR") == 0) {
            operation = "clear";
            status = mc_registry_redstone_set_clear(p.set);
        } else if (count == 1 && strcmp(word[0], "FINISH") == 0) {
            operation = "terminal_before_identity_hash";
            finished = true;
        } else
            goto cleanup;
        if (!emit(&p, operation, argument, serial, x, y, z, has_result, changed, status) ||
            status != MC_REGISTRY_OK)
            goto cleanup;
    }
    if (!begun || !finished || ferror(input))
        goto cleanup;
    mc_registry_redstone_set_view final_view;
    if (mc_registry_redstone_set_inspect(p.set, &final_view) != MC_REGISTRY_OK)
        goto cleanup;
    mc_registry_redstone_snapshot_release(p.snapshot);
    p.snapshot = NULL;
    if (mc_registry_classes_free(p.classes, NULL) != MC_REGISTRY_OK)
        goto cleanup;
    p.classes = NULL;
    (void)printf("{\"kind\":\"terminal\",\"steps\":%zu,\"constructed_keys\":%" PRIu64
                 ",\"identity_requests\":%zu,\"input_bytes\":%zu,\"snapshot_released\":true,"
                 "\"registry_released\":true}\n",
                 p.steps, final_view.constructed_keys, p.request_count, bytes);
    if (fflush(stdout) != 0 || ferror(stdout))
        goto cleanup;
    result = 0;
cleanup:
    mc_registry_redstone_snapshot_release(p.snapshot);
    if (p.classes != NULL && mc_registry_classes_free(p.classes, NULL) != MC_REGISTRY_OK)
        result = 1;
    mc_materials_destroy(materials);
    mc_java_random_profile_free(profile);
    if (input != NULL && input != stdin && fclose(input) != 0)
        result = 1;
    if (result != 0)
        (void)fputs(
            "redstone Set probe failed; retained stdout is incomplete or a failed native prefix\n",
            stderr);
    return result;
}
