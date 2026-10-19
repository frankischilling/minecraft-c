#include "../../../src/block/registry/registry_internal.h"
#include "mc/resources/files/files.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#define MAX_STRINGS 8192u

typedef struct cell {
    const mc_registry_string *key, *value;
} cell;
static const mc_registry_string *held[MAX_STRINGS];
static size_t held_count, rows;

static int compare_cells(const void *left, const void *right)
{
    const mc_registry_string *a = ((const cell *)left)->key;
    const mc_registry_string *b = ((const cell *)right)->key;
    const size_t common = a->length < b->length ? a->length : b->length;
    for (size_t i = 0; i < common; ++i)
        if (a->units[i] != b->units[i])
            return a->units[i] < b->units[i] ? -1 : 1;
    return a->length < b->length ? -1 : a->length != b->length;
}

static size_t identity(const mc_registry_string *string)
{
    for (size_t i = 0; i < held_count; ++i)
        if (held[i] == string)
            return i + 1;
    if (string == NULL || held_count == MAX_STRINGS)
        return 0;
    held[held_count++] = string;
    return held_count;
}

static void units(const mc_registry_string *string)
{
    if (string->length == 0)
        fputc('-', stdout);
    for (size_t i = 0; i < string->length; ++i)
        printf("%04x", (unsigned int)string->units[i]);
}

static const mc_registry_string *binding(const mc_translation *translation,
                                         const mc_java_properties_string *source)
{
    for (const mc_translation_string_binding *b = translation->strings; b != NULL; b = b->next)
        if (b->source == source)
            return b->string;
    return NULL;
}

static char *root_argument(int argc, char **argv)
{
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

static bool invalid_names(mc_resource_files *files)
{
    const mc_registry_resources *source = mc_resource_files_source(files);
    const char *names[] = {
        NULL,   "",      "lang/en_US.lang",   "//lang", "/lang/../x", "/./x", "/lang/",
        "/C:x", "/a\\b", "/lang/en_US.lang/."};
    for (size_t i = 0; i < sizeof(names) / sizeof(*names); ++i) {
        mc_java_properties_input out = {.context = files, .read = NULL};
        if (source->open(source->context, names[i], &out) != MC_REGISTRY_INVALID_ARGUMENT ||
            out.context != files || out.read != NULL)
            return false;
    }
    return true;
}

static bool stream_lifecycle(mc_resource_files *files)
{
    const mc_registry_resources *source = mc_resource_files_source(files);
    mc_java_properties_input stream = {0};
    if (source->close == NULL || source->ready == NULL ||
        source->open(source->context, "/lang/en_US.lang", &stream) != MC_REGISTRY_OK ||
        !source->ready(source->context, &stream))
        return false;
    uint8_t byte;
    size_t count = 0;
    if (stream.read(stream.context, &byte, 1, &count) != MC_JAVA_PROPERTIES_OK || count != 1 ||
        !source->ready(source->context, &stream) ||
        source->close(source->context, &stream) != MC_REGISTRY_OK ||
        source->close(source->context, &stream) != MC_REGISTRY_OK ||
        source->ready(source->context, &stream))
        return false;
    count = 123;
    if (stream.read(stream.context, &byte, 1, &count) != MC_JAVA_PROPERTIES_IO_ERROR || count != 0)
        return false;
    mc_java_properties_input foreign = {.context = files, .read = stream.read};
    return source->close(source->context, &foreign) == MC_REGISTRY_INVALID_ARGUMENT &&
           source->close(source->context, NULL) == MC_REGISTRY_INVALID_ARGUMENT &&
           !source->ready(source->context, &foreign) && !source->ready(source->context, NULL);
}

int main(int argc, char **argv)
{
    int result = 1;
    mc_resource_files *files = NULL;
    mc_materials *materials = NULL;
    mc_java_random_profile *profile = NULL;
    mc_registry_classes *classes = NULL;
    cell *cells = NULL;
    char *root = NULL;
    mc_registry_error error = {0};
#ifdef _WIN32
    if (_setmode(_fileno(stdout), _O_BINARY) < 0)
        goto cleanup;
#endif
    if (argc < 3 || (strcmp(argv[1], "client") != 0 && strcmp(argv[1], "server") != 0))
        goto cleanup;
    root = root_argument(argc, argv);
    if (root == NULL || mc_resource_files_create(root, &files) != MC_REGISTRY_OK ||
        !invalid_names(files) || !mc_materials_create(&materials) ||
        mc_java_random_profile_create(SIZE_MAX, &profile) != MC_JAVA_RANDOM_PROFILE_OK)
        goto cleanup;
    const mc_registry_options options = {
        .side = strcmp(argv[1], "client") == 0 ? MC_REGISTRY_CLIENT : MC_REGISTRY_SERVER,
        .random_profile = profile,
        .materials = materials,
        .resources = mc_resource_files_source(files)};
    if (mc_registry_classes_allocate(&options, &classes, &error) != MC_REGISTRY_OK ||
        mc_registry_classes_initialize_translation(classes, &error) != MC_REGISTRY_OK)
        goto cleanup;
    mc_registry_translation_view view;
    if (mc_registry_classes_inspect_translation(classes, &view, &error) != MC_REGISTRY_OK ||
        view.singleton_phase != MC_REGISTRY_READY || view.wrapper_phase != MC_REGISTRY_READY ||
        view.singleton_a == NULL || view.singleton_a != view.wrapper_a || !stream_lifecycle(files))
        goto cleanup;
    const mc_translation *translation = view.singleton_a;
    const size_t count = mc_translation_property_count(translation);
    if (count > 4096)
        goto cleanup;
    cells = calloc(count == 0 ? 1 : count, sizeof(*cells));
    if (cells == NULL)
        goto cleanup;
    for (size_t i = 0; i < count; ++i) {
        const mc_java_properties_string *key = NULL, *value = NULL;
        if (mc_java_properties_entry(translation->b, i, &key, &value) != MC_JAVA_PROPERTIES_OK)
            goto cleanup;
        cells[i] = (cell){binding(translation, key), binding(translation, value)};
        if (cells[i].key == NULL || cells[i].value == NULL)
            goto cleanup;
    }
    qsort(cells, count, sizeof(*cells), compare_cells);
    printf("TRANSLATION_PROPERTIES_V1\nMAP\t%zu\nSINGLETON\t1\n", count);
    rows += 3;
    for (size_t i = 0; i < count; ++i) {
        const size_t key = identity(cells[i].key), value = identity(cells[i].value);
        if (key == 0 || value == 0)
            goto cleanup;
        printf("ENTRY\t%zu\t%zu\n", key, value);
        ++rows;
    }
    const char *queries[] = {"gui.done", "tile.stone.name", "stat.startGame",
                             "minecraft-c.absent.translation.key", ""};
    for (size_t i = 0; i < sizeof(queries) / sizeof(*queries); ++i) {
        const mc_registry_string *key = NULL, *value = NULL;
        if (mc_registry_string_ascii(classes, "", queries[i], &key) != MC_REGISTRY_OK)
            goto cleanup;
        /* Admit one private wrapper call after public initialization. */
        ++classes->entered;
        mc_registry_status status = mc_registry_translate(classes, key, &value);
        --classes->entered;
        if (status != MC_REGISTRY_OK || value == NULL)
            goto cleanup;
        const size_t reference = identity(value);
        if (reference == 0)
            goto cleanup;
        printf("QUERY\t%zu\t", key->length);
        units(key);
        printf("\t%zu\t%d\n", reference, value == key ? 1 : 0);
        ++rows;
    }
    printf("STRINGS\t%zu\n", held_count);
    ++rows;
    for (size_t i = 0; i < held_count; ++i) {
        printf("STRING\t%zu\t%zu\t", i + 1, held[i]->length);
        units(held[i]);
        fputc('\n', stdout);
        ++rows;
    }
    printf("END\t%zu\t%zu\t%zu\n", count, held_count, rows);
    if (fflush(stdout) != 0 || ferror(stdout))
        goto cleanup;
    result = 0;
cleanup:
    if (result != 0)
        fprintf(stderr, "translation file probe failed: status=%d constructor=%s pc=%d\n",
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
