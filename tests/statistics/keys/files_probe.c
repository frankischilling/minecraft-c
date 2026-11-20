#include "mc/resources/files/files.h"
#include "mc/statistics/keys/keys.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#define MAX_ENTRIES 4096u

typedef struct cell {
    int32_t id;
    const mc_registry_string *value;
} cell;

static const mc_registry_string *held[MAX_ENTRIES];
static size_t held_count, rows, queries;

static int compare_cells(const void *left, const void *right) {
    const int32_t a = ((const cell *)left)->id, b = ((const cell *)right)->id;
    return a < b ? -1 : a != b;
}

static size_t identity(const mc_registry_string *string) {
    for (size_t i = 0; i < held_count; ++i)
        if (held[i] == string)
            return i + 1;
    if (string == NULL || held_count == MAX_ENTRIES)
        return 0;
    held[held_count++] = string;
    return held_count;
}

static bool query(mc_registry_classes *classes, int32_t id) {
    const mc_registry_string *value = NULL;
    if (mc_registry_classes_statistic_key(classes, id, &value, NULL) != MC_REGISTRY_OK)
        return false;
    printf("QUERY\t%" PRId32 "\t", id);
    if (value == NULL)
        fputs("NULL\n", stdout);
    else {
        const size_t reference = identity(value);
        if (reference == 0)
            return false;
        printf("R:%zu\n", reference);
    }
    ++queries;
    ++rows;
    return true;
}

static char *root_argument(int argc, char **argv) {
    if (argc == 3) {
        const size_t length = strlen(argv[2]);
        char *copy = malloc(length + 1);
        if (copy != NULL)
            memcpy(copy, argv[2], length + 1);
        return copy;
    }
    if (argc != 4 || strcmp(argv[2], "--root-hex") != 0)
        return NULL;
    const size_t length = strlen(argv[3]);
    if (length == 0 || length % 2 != 0)
        return NULL;
    char *root = malloc(length / 2 + 1);
    if (root == NULL)
        return NULL;
    for (size_t i = 0; i < length / 2; ++i) {
        unsigned int value = 0;
        for (size_t j = 0; j < 2; ++j) {
            const unsigned char c = (unsigned char)argv[3][2 * i + j];
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
                free(root);
                return NULL;
            }
            value =
                value * 16u + (c <= '9' ? (unsigned int)(c - '0') : (unsigned int)(c - 'a') + 10u);
        }
        if (value == 0) {
            free(root);
            return NULL;
        }
        root[i] = (char)(unsigned char)value;
    }
    root[length / 2] = '\0';
    return root;
}

int main(int argc, char **argv) {
    int result = 1;
    mc_resource_files *files = NULL;
    mc_materials *materials = NULL;
    mc_java_random_profile *profile = NULL;
    mc_registry_classes *classes = NULL;
    char *root = NULL;
    cell *cells = NULL;
    mc_registry_error error = {0};
#ifdef _WIN32
    if (_setmode(_fileno(stdout), _O_BINARY) < 0)
        goto cleanup;
#endif
    if (argc < 3 || (strcmp(argv[1], "client") != 0 && strcmp(argv[1], "server") != 0))
        goto cleanup;
    root = root_argument(argc, argv);
    if (root == NULL || mc_resource_files_create(root, &files) != MC_REGISTRY_OK ||
        !mc_materials_create(&materials) ||
        mc_java_random_profile_create(SIZE_MAX, &profile) != MC_JAVA_RANDOM_PROFILE_OK)
        goto cleanup;
    const mc_registry_options options = {
        .side = strcmp(argv[1], "client") == 0 ? MC_REGISTRY_CLIENT : MC_REGISTRY_SERVER,
        .random_profile = profile,
        .materials = materials,
        .resources = mc_resource_files_source(files)};
    if (mc_registry_classes_allocate(&options, &classes, &error) != MC_REGISTRY_OK ||
        mc_registry_classes_initialize_statistic_keys(classes, &error) != MC_REGISTRY_OK)
        goto cleanup;
    mc_registry_statistic_key_view view;
    if (mc_registry_classes_inspect_statistic_keys(classes, &view, &error) != MC_REGISTRY_OK ||
        view.phase != MC_REGISTRY_READY || view.a == NULL || !view.map_published)
        goto cleanup;
    mc_registry_block_view before, after;
    if (mc_registry_classes_inspect_blocks(classes, &before, &error) != MC_REGISTRY_OK)
        goto cleanup;
    const size_t count = mc_statistic_keys_size(view.a);
    if (count > MAX_ENTRIES)
        goto cleanup;
    cells = calloc(count == 0 ? 1 : count, sizeof(*cells));
    if (cells == NULL)
        goto cleanup;
    for (size_t i = 0; i < count; ++i)
        if (mc_statistic_keys_entry(view.a, i, &cells[i].id, &cells[i].value) != MC_REGISTRY_OK ||
            cells[i].value == NULL)
            goto cleanup;
    qsort(cells, count, sizeof(*cells), compare_cells);
    printf("KEY_MAP_V1\nINITIALIZATION\tCOMPLETE\nORDER\tSIGNED_INT_ASCENDING\nMAP\t%"
           "zu\nSINGLETON\t1\n",
           count);
    rows = 5;
    for (size_t i = 0; i < count; ++i) {
        const size_t reference = identity(cells[i].value);
        if (reference == 0 || (i != 0 && cells[i - 1].id >= cells[i].id))
            goto cleanup;
        printf("ENTRY\t%" PRId32 "\tR:%zu\n", cells[i].id, reference);
        ++rows;
    }
    for (size_t i = 0; i < count; ++i)
        if (!query(classes, cells[i].id))
            goto cleanup;
    const int32_t boundaries[] = {INT32_MIN, INT32_MAX, 0, -1, 1, 32767, -32768};
    for (size_t i = 0; i < sizeof(boundaries) / sizeof(*boundaries); ++i)
        if (!query(classes, boundaries[i]))
            goto cleanup;
    mc_registry_statistic_key_view final;
    if (mc_registry_classes_inspect_statistic_keys(classes, &final, &error) != MC_REGISTRY_OK ||
        final.a != view.a || final.phase != MC_REGISTRY_READY ||
        mc_statistic_keys_size(final.a) != count ||
        mc_registry_classes_inspect_blocks(classes, &after, &error) != MC_REGISTRY_OK ||
        after.allocation_bytes != before.allocation_bytes || after.phase != before.phase)
        goto cleanup;
    printf("STRINGS\t%zu\n", held_count);
    ++rows;
    for (size_t i = 0; i < held_count; ++i) {
        mc_registry_string_view string;
        if (!mc_registry_string_read(held[i], &string) || string.length > 65536)
            goto cleanup;
        printf("STRING\t%zu\t%zu\t", i + 1, string.length);
        if (string.length == 0)
            fputc('-', stdout);
        for (size_t j = 0; j < string.length; ++j)
            printf("%04x", (unsigned int)string.units[j]);
        fputc('\n', stdout);
        ++rows;
    }
    fputs("SUMMARY\tPASS\n", stdout);
    ++rows;
    printf("END\t%zu\t%zu\t%zu\t%zu\n", count, queries, held_count, rows);
    if (fflush(stdout) != 0 || ferror(stdout))
        goto cleanup;
    result = 0;
cleanup:
    if (result != 0)
        fprintf(stderr, "key file probe failed: status=%d constructor=%s pc=%d\n",
                (int)error.status, error.constructor_class == NULL ? "-" : error.constructor_class,
                (int)error.constructor_pc);
    free(cells);
    free(root);
    if (mc_registry_classes_free(classes, NULL) != MC_REGISTRY_OK)
        result = 1;
    if (mc_resource_files_free(files) != MC_REGISTRY_OK)
        result = 1;
    mc_materials_destroy(materials);
    mc_java_random_profile_free(profile);
    return result;
}
