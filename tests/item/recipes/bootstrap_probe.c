#include "mc/item/recipes/recipes.h"
#include "mc/resources/files/files.h"
#include "mc/statistics/achievements/achievements.h"
#include "mc/statistics/registry/registry.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#define REFERENCE_LIMIT 16384u
#define RECIPE_LIMIT 4096u

typedef struct references {
    const mc_item_stack *values[REFERENCE_LIMIT];
    size_t count;
} references;

static bool reference(references *refs, const mc_item_stack *stack)
{
    if (stack == NULL)
        return printf("\tNULL") >= 0;
    size_t i = 0;
    while (i < refs->count && refs->values[i] != stack)
        ++i;
    if (i == refs->count) {
        if (refs->count == REFERENCE_LIMIT)
            return false;
        refs->values[refs->count++] = stack;
    }
    return printf("\tR:%zu", i + 1) >= 0;
}

static bool crafting_graph(const mc_crafting_manager *manager, references *refs)
{
    if (manager == NULL)
        return printf("CRAFTING\tNULL\n") >= 0;
    const size_t count = mc_crafting_manager_size(manager);
    if (count > RECIPE_LIMIT || printf("CRAFTING\t%zu\n", count) < 0)
        return false;
    for (size_t i = 0; i < count; ++i) {
        const mc_recipe *recipe = NULL;
        if (mc_crafting_manager_get(manager, i, &recipe) != MC_REGISTRY_OK || recipe == NULL)
            return false;
        size_t length;
        const mc_shaped_recipe_fields *shaped = mc_recipe_shaped_fields_get(recipe);
        const mc_shapeless_recipe_fields *shapeless = mc_recipe_shapeless_fields_get(recipe);
        if (shaped != NULL) {
            length = mc_recipe_shaped_array_length(recipe);
            if (printf("RECIPE\t%zu\tshaped\t%" PRId32 "\t%" PRId32 "\t%" PRId32 "\t%zu", i,
                       shaped->a, shaped->b, shaped->c, length) < 0)
                return false;
        } else if (shapeless != NULL) {
            if (shapeless->b == NULL)
                return false;
            length = mc_recipe_ingredient_list_size(shapeless->b);
            if (printf("RECIPE\t%zu\tshapeless\tNULL\tNULL\tNULL\t%zu", i, length) < 0)
                return false;
        } else
            return false;
        if (length > RECIPE_LIMIT || !reference(refs, mc_recipe_output(recipe)) ||
            putchar('\n') == EOF)
            return false;
        for (size_t j = 0; j < length; ++j) {
            const mc_item_stack *stack = NULL;
            if (shaped != NULL) {
                if (shaped->d == NULL)
                    return false;
                stack = shaped->d[j];
            } else if (mc_recipe_ingredient_list_get(shapeless->b, j, &stack) != MC_REGISTRY_OK)
                return false;
            if (printf("INGREDIENT\t%zu\t%zu", i, j) < 0 || !reference(refs, stack) ||
                putchar('\n') == EOF)
                return false;
        }
    }
    return true;
}

static bool smelting_graph(const mc_smelting_manager *manager, references *refs)
{
    if (manager == NULL)
        return printf("SMELTING\tNULL\n") >= 0;
    const size_t count = mc_smelting_manager_size(manager);
    if (count > RECIPE_LIMIT || printf("SMELTING\t%zu\n", count) < 0)
        return false;
    int32_t *keys = count == 0 ? NULL : malloc(count * sizeof(*keys));
    size_t copied = 0;
    bool ok = count == 0 || keys != NULL;
    if (ok)
        ok = mc_smelting_manager_keys(manager, keys, count, &copied) == MC_REGISTRY_OK &&
             copied == count;
    for (size_t i = 0; ok && i < count; ++i) {
        const mc_item_stack *stack = NULL;
        ok = mc_smelting_manager_find(manager, keys[i], &stack) == MC_REGISTRY_OK &&
             printf("SMELT\t%zu\t%" PRId32, i, keys[i]) >= 0 && reference(refs, stack) &&
             putchar('\n') != EOF;
    }
    free(keys);
    return ok;
}

static char *root_argument(int argc, char **argv)
{
    if (argc == 4) {
        size_t length = strlen(argv[3]);
        if (length == 0 || length > 32768)
            return NULL;
        char *copy = malloc(length + 1);
        if (copy != NULL)
            memcpy(copy, argv[3], length + 1);
        return copy;
    }
    if (argc != 5 || strcmp(argv[3], "--root-hex") != 0)
        return NULL;
    size_t length = strlen(argv[4]);
    if (length == 0 || length > 65536 || length % 2 != 0)
        return NULL;
    char *root = malloc(length / 2 + 1);
    if (root == NULL)
        return NULL;
    for (size_t i = 0; i < length / 2; ++i) {
        unsigned int value = 0;
        for (size_t j = 0; j < 2; ++j) {
            unsigned char c = (unsigned char)argv[4][2 * i + j];
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
                free(root);
                return NULL;
            }
            value = value * 16u + (c <= '9' ? (unsigned int)c - (unsigned int)'0'
                                            : (unsigned int)c - (unsigned int)'a' + 10u);
        }
        if (value == 0) {
            free(root);
            return NULL;
        }
        root[i] = (char)(unsigned char)value;
    }
    root[length / 2] = 0;
    return root;
}

int main(int argc, char **argv)
{
    int result = 1;
    mc_resource_files *files = NULL;
    mc_materials *materials = NULL;
    mc_java_random_profile *profile = NULL;
    mc_registry_classes *owner = NULL;
    references *refs = NULL;
    mc_registry_error error = {0};
    char *root = NULL;
#ifdef _WIN32
    if (_setmode(_fileno(stdout), _O_BINARY) < 0 || _setmode(_fileno(stderr), _O_BINARY) < 0)
        goto cleanup;
#endif
    if ((argc != 4 && argc != 5) ||
        (strcmp(argv[1], "client") != 0 && strcmp(argv[1], "server") != 0))
        goto cleanup;
    mc_registry_status (*entry)(mc_registry_classes *, mc_registry_error *) = NULL;
    if (strcmp(argv[2], "stats") == 0)
        entry = mc_registry_classes_initialize_statistics;
    if (strcmp(argv[2], "block") == 0)
        entry = mc_registry_classes_initialize_blocks;
    if (strcmp(argv[2], "item") == 0)
        entry = mc_registry_classes_initialize_items;
    if (strcmp(argv[2], "crafting") == 0)
        entry = mc_registry_classes_initialize_crafting;
    if (strcmp(argv[2], "smelting") == 0)
        entry = mc_registry_classes_initialize_smelting;
    root = root_argument(argc, argv);
    if (entry == NULL || root == NULL || mc_resource_files_create(root, &files) != MC_REGISTRY_OK ||
        !mc_materials_create(&materials) ||
        mc_java_random_profile_create(SIZE_MAX, &profile) != MC_JAVA_RANDOM_PROFILE_OK)
        goto cleanup;
    const mc_registry_options options = {strcmp(argv[1], "client") == 0 ? MC_REGISTRY_CLIENT
                                                                        : MC_REGISTRY_SERVER,
                                         profile, materials, 0, mc_resource_files_source(files)};
    if (mc_registry_classes_allocate(&options, &owner, &error) != MC_REGISTRY_OK)
        goto cleanup;
    const mc_registry_status status = entry(owner, &error);
    mc_registry_crafting_view crafting;
    mc_registry_smelting_view smelting;
    mc_registry_statistic_view stats;
    mc_registry_achievement_view achievements;
    if (mc_registry_classes_inspect_crafting(owner, &crafting, NULL) != MC_REGISTRY_OK ||
        mc_registry_classes_inspect_smelting(owner, &smelting, NULL) != MC_REGISTRY_OK ||
        mc_registry_classes_inspect_statistics(owner, &stats, NULL) != MC_REGISTRY_OK ||
        mc_registry_classes_inspect_achievements(owner, &achievements, NULL) != MC_REGISTRY_OK)
        goto cleanup;
    /* Native state is a separate binding, never an original field projection. */
    if (fprintf(stderr, "NATIVE\t%d\t%d\t%d\t%d\t%d\t%d\t%zu\t%s\t%d\n", (int)status,
                (int)stats.phase, (int)stats.initializer_pc, (int)achievements.phase,
                (int)crafting.phase, (int)smelting.phase, stats.map_size,
                error.constructor_class == NULL ? "NULL" : error.constructor_class,
                (int)error.constructor_pc) < 0)
        goto cleanup;
    if (status != MC_REGISTRY_OK && status != MC_REGISTRY_NULL_REFERENCE &&
        status != MC_REGISTRY_FAILED_INITIALIZATION)
        goto cleanup;
    refs = calloc(1, sizeof(*refs));
    if (refs == NULL || printf("RECIPE_CATALOG_V1\t%s\t%s\n", argv[1], argv[2]) < 0 ||
        !crafting_graph(crafting.a, refs) || !smelting_graph(smelting.a, refs))
        goto cleanup;
    for (size_t i = 0; i < refs->count; ++i) {
        mc_item_stack_fields fields;
        mc_item_stack_phase phase;
        if (mc_item_stack_fields_get(refs->values[i], &fields) != MC_ITEM_STACK_OK ||
            mc_item_stack_phase_get(refs->values[i], &phase) != MC_ITEM_STACK_OK ||
            phase != MC_ITEM_STACK_READY ||
            printf("STACK\t%zu\t%" PRId32 "\t%" PRId32 "\t%" PRId32 "\t%" PRId32 "\n", i + 1,
                   fields.count, fields.animation, fields.id, fields.damage) < 0)
            goto cleanup;
    }
    if (printf("END\n") < 0 || fflush(stdout) != 0 || ferror(stdout) || fflush(stderr) != 0 ||
        ferror(stderr))
        goto cleanup;
    result = 0;
cleanup:
    free(root);
    free(refs);
    if (mc_registry_classes_free(owner, NULL) != MC_REGISTRY_OK)
        return 1;
    if (files != NULL && mc_resource_files_free(files) != MC_REGISTRY_OK)
        result = 1;
    mc_materials_destroy(materials);
    mc_java_random_profile_free(profile);
    return result;
}
