#include "mc/resources/files/files.h"
#include "mc/statistics/registry/registry.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#define COMMON 23u
#define MAX_UNITS 65536u

typedef struct common_row {
    int32_t id;
    const mc_registry_string *name, *key;
    bool flag;
    char formatter;
} common_row;

static char *root_argument(int argc, char **argv)
{
    if (argc == 3) {
        const size_t length = strlen(argv[2]);
        if (length == SIZE_MAX)
            return NULL;
        char *copy = malloc(length + 1u);
        if (copy != NULL)
            memcpy(copy, argv[2], length + 1u);
        return copy;
    }
    if (argc != 4 || strcmp(argv[2], "--root-hex") != 0)
        return NULL;
    const size_t length = strlen(argv[3]);
    if (length == 0 || length % 2u != 0)
        return NULL;
    char *root = malloc(length / 2u + 1u);
    if (root == NULL)
        return NULL;
    for (size_t i = 0; i < length / 2u; ++i) {
        unsigned int value = 0;
        for (size_t j = 0; j < 2u; ++j) {
            const unsigned char digit = (unsigned char)argv[3][2u * i + j];
            if (!((digit >= '0' && digit <= '9') || (digit >= 'a' && digit <= 'f'))) {
                free(root);
                return NULL;
            }
            value = value * 16u + (digit <= '9' ? (unsigned int)(digit - '0')
                                                : (unsigned int)(digit - 'a') + 10u);
        }
        if (value == 0) {
            free(root);
            return NULL;
        }
        root[i] = (char)(unsigned char)value;
    }
    root[length / 2u] = '\0';
    return root;
}

static bool string_valid(const mc_registry_string *string)
{
    if (string == NULL)
        return true;
    mc_registry_string_view view;
    return mc_registry_string_read(string, &view) && view.length <= MAX_UNITS &&
           (view.length == 0 || view.units != NULL);
}

static bool print_string(const mc_registry_string *string)
{
    if (string == NULL) {
        fputs("NULL", stdout);
        return true;
    }
    mc_registry_string_view view;
    if (!mc_registry_string_read(string, &view) || view.length > MAX_UNITS ||
        (view.length != 0 && view.units == NULL))
        return false;
    if (view.length == 0)
        fputc('-', stdout);
    for (size_t i = 0; i < view.length; ++i)
        printf("%04x", (unsigned int)view.units[i]);
    return true;
}

int main(int argc, char **argv)
{
    int result = 1;
    char *root = NULL;
    mc_resource_files *files = NULL;
    mc_materials *materials = NULL;
    mc_java_random_profile *profile = NULL;
    mc_registry_classes *classes = NULL;
    mc_registry_error error = {0};
    common_row rows[COMMON];
#ifdef _WIN32
    if (_setmode(_fileno(stdout), _O_BINARY) < 0)
        goto cleanup;
#endif
    if (argc < 3 || (strcmp(argv[1], "client") != 0 && strcmp(argv[1], "server") != 0))
        goto cleanup;
    const bool client = strcmp(argv[1], "client") == 0;
    root = root_argument(argc, argv);
    if (root == NULL || mc_resource_files_create(root, &files) != MC_REGISTRY_OK ||
        !mc_materials_create(&materials) ||
        mc_java_random_profile_create(SIZE_MAX, &profile) != MC_JAVA_RANDOM_PROFILE_OK)
        goto cleanup;
    const mc_registry_options options = {.side = client ? MC_REGISTRY_CLIENT : MC_REGISTRY_SERVER,
                                         .random_profile = profile,
                                         .materials = materials,
                                         .resources = mc_resource_files_source(files)};
    if (mc_registry_classes_allocate(&options, &classes, &error) != MC_REGISTRY_OK)
        goto cleanup;
    const mc_registry_status initialized =
        mc_registry_classes_initialize_statistics(classes, &error);
    if (initialized != MC_REGISTRY_OK || error.status != MC_REGISTRY_OK)
        goto cleanup;
    mc_registry_statistic_base_view base;
    mc_registry_statistic_view statistics;
    if (mc_registry_classes_inspect_statistic_base(classes, &base, NULL) != MC_REGISTRY_OK ||
        mc_registry_classes_inspect_statistics(classes, &statistics, NULL) != MC_REGISTRY_OK ||
        base.phase != MC_REGISTRY_READY || base.initializer_pc != 51 || base.b == NULL ||
        base.c == NULL || base.b == base.c || base.i == NULL || base.j == NULL || base.k == NULL ||
        base.i == base.j || base.i == base.k || base.j == base.k ||
        statistics.phase != MC_REGISTRY_READY || statistics.initializer_pc != 635 ||
        statistics.map_size != 486 || statistics.named == NULL || statistics.b == NULL ||
        statistics.c == NULL || statistics.d == NULL || statistics.e == NULL ||
        statistics.b == statistics.c || statistics.b == statistics.d ||
        statistics.b == statistics.e || statistics.c == statistics.d ||
        statistics.c == statistics.e || statistics.d == statistics.e ||
        mc_statistic_list_size(statistics.b) != 467 ||
        mc_statistic_list_size(statistics.c) != COMMON ||
        mc_statistic_list_size(statistics.d) != 106 || mc_statistic_list_size(statistics.e) != 68 ||
        statistics.C == NULL || statistics.D == NULL || statistics.E == NULL ||
        statistics.F == NULL || statistics.G || statistics.H ||
        statistics.duplicate_message != NULL)
        goto cleanup;
    const mc_statistic_formatter *formatters[3] = {base.i, base.j, base.k};
    const mc_statistic_formatter_kind kinds[3] = {
        MC_STATISTIC_FORMAT_INTEGER, MC_STATISTIC_FORMAT_TIME, MC_STATISTIC_FORMAT_DISTANCE};
    for (size_t index = 0; index < COMMON; ++index) {
        const mc_statistic *statistic = statistics.named[index];
        if (statistic == NULL || mc_statistic_kind_get(statistic) != MC_STATISTIC_GENERAL)
            goto cleanup;
        for (size_t prior = 0; prior < index; ++prior)
            if (statistics.named[prior] == statistic)
                goto cleanup;
        const mc_statistic_fields *fields = mc_statistic_fields_get(statistic);
        const mc_statistic *mapped = NULL, *all = NULL, *general = NULL;
        if (fields == NULL || !string_valid(fields->f) || !string_valid(fields->h) ||
            mc_registry_classes_find_statistic(classes, fields->e, &mapped) != MC_REGISTRY_OK ||
            mc_statistic_list_get(statistics.b, index, &all) != MC_REGISTRY_OK ||
            mc_statistic_list_get(statistics.c, index, &general) != MC_REGISTRY_OK ||
            mapped != statistic || all != statistic || general != statistic)
            goto cleanup;
        char selected = '\0';
        for (size_t candidate = 0; candidate < 3u; ++candidate) {
            if (fields->a == formatters[candidate]) {
                mc_statistic_formatter_kind kind;
                if (mc_statistic_formatter_kind_get(fields->a, &kind) != MC_REGISTRY_OK ||
                    kind != kinds[candidate])
                    goto cleanup;
                selected = (char)('i' + (int)candidate);
            }
        }
        if (selected == '\0')
            goto cleanup;
        rows[index] = (common_row){.id = fields->e,
                                   .name = fields->f,
                                   .key = fields->h,
                                   .flag = fields->g,
                                   .formatter = selected};
    }
    printf("STATS_COMMON_V1\t%s\n", argv[1]);
    for (size_t index = 0; index < COMMON; ++index) {
        printf("STAT\t%zu\t%" PRId32 "\t", index, rows[index].id);
        if (!print_string(rows[index].name))
            goto cleanup;
        printf("\t%d\t", rows[index].flag ? 1 : 0);
        if (!print_string(rows[index].key))
            goto cleanup;
        printf("\t%c\n", rows[index].formatter);
    }
    printf("END\t%u\n", COMMON);
    if (fflush(stdout) != 0 || ferror(stdout))
        goto cleanup;
    result = 0;
cleanup:
    if (mc_registry_classes_free(classes, NULL) != MC_REGISTRY_OK)
        result = 1;
    if (mc_resource_files_free(files) != MC_REGISTRY_OK)
        result = 1;
    mc_materials_destroy(materials);
    mc_java_random_profile_free(profile);
    free(root);
    if (result != 0)
        fprintf(stderr, "statistics file probe failed: status=%d active=%s pc=%d\n",
                (int)error.status,
                error.active_initializer_class == NULL ? "-" : error.active_initializer_class,
                (int)error.active_initializer_pc);
    return result;
}
