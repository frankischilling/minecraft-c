#include "mc/resources/files/files.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <wchar.h>
#include <windows.h>
#endif

typedef struct resource_stream {
    FILE *file;
    bool pending_error;
    struct resource_stream *next;
} resource_stream;

struct mc_resource_files {
    mc_registry_resources source;
    char *root;
    size_t root_length;
    resource_stream *streams;
};

static bool utf8(const unsigned char *text, size_t length)
{
    for (size_t i = 0; i < length;) {
        const unsigned int first = text[i++];
        if (first < 128u)
            continue;
        size_t following;
        unsigned int lower = 128u, upper = 191u;
        if (first >= 194u && first <= 223u) {
            following = 1;
        } else if (first >= 224u && first <= 239u) {
            following = 2;
            if (first == 224u)
                lower = 160u;
            if (first == 237u)
                upper = 159u;
        } else if (first >= 240u && first <= 244u) {
            following = 3;
            if (first == 240u)
                lower = 144u;
            if (first == 244u)
                upper = 143u;
        } else {
            return false;
        }
        if (following > length - i || text[i] < lower || text[i] > upper)
            return false;
        ++i;
        for (size_t j = 1; j < following; ++j, ++i)
            if (text[i] < 128u || text[i] > 191u)
                return false;
    }
    return true;
}

static bool resource_name(const char *name)
{
    if (name == NULL || name[0] != '/' || name[1] == '\0')
        return false;
    size_t start = 1;
    for (size_t i = 1;; ++i) {
        const unsigned char c = (unsigned char)name[i];
        if (c == '/' || c == 0) {
            const size_t length = i - start;
            if (length == 0 || (length == 1 && name[start] == '.') ||
                (length == 2 && name[start] == '.' && name[start + 1] == '.'))
                return false;
            if (c == 0)
                return true;
            start = i + 1;
        } else if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                     c == '.' || c == '_' || c == '-')) {
            return false;
        }
    }
}

static mc_java_properties_status read_bytes(void *context, uint8_t *destination, size_t capacity,
                                            size_t *count)
{
    resource_stream *stream = context;
    if (destination == NULL || count == NULL || capacity == 0)
        return MC_JAVA_PROPERTIES_INVALID_ARGUMENT;
    *count = 0;
    if (stream->pending_error)
        return MC_JAVA_PROPERTIES_IO_ERROR;
    const size_t actual = fread(destination, 1, capacity, stream->file);
    if (ferror(stream->file)) {
        stream->pending_error = true;
        if (actual == 0)
            return MC_JAVA_PROPERTIES_IO_ERROR;
    }
    *count = actual;
    return MC_JAVA_PROPERTIES_OK;
}

static mc_registry_status open_file(const char *path, FILE **out)
{
    FILE *file = NULL;
    int code;
#ifdef _WIN32
    const size_t length = strlen(path);
    if (length > (size_t)INT_MAX)
        return MC_REGISTRY_LIMIT;
    const int units =
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, (int)length, NULL, 0);
    if (units == 0)
        return MC_REGISTRY_INVALID_ARGUMENT;
    if ((size_t)units > SIZE_MAX / sizeof(wchar_t) - 1)
        return MC_REGISTRY_LIMIT;
    wchar_t *native = malloc(((size_t)units + 1) * sizeof(*native));
    if (native == NULL)
        return MC_REGISTRY_ALLOCATION_FAILED;
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, (int)length, native, units) !=
        units) {
        free(native);
        return MC_REGISTRY_INVALID_ARGUMENT;
    }
    native[units] = L'\0';
#ifdef _MSC_VER
    code = (int)_wfopen_s(&file, native, L"rb");
#else
    errno = 0;
    file = _wfopen(native, L"rb");
    code = errno;
#endif
    free(native);
#else
    errno = 0;
    file = fopen(path, "rb");
    code = errno;
#endif
    if (file == NULL)
        return code == ENOENT || code == ENOTDIR ? MC_REGISTRY_NULL_REFERENCE
                                                 : MC_REGISTRY_IO_FAILED;
    *out = file;
    return MC_REGISTRY_OK;
}

static mc_registry_status open_resource(void *context, const char *name,
                                        mc_java_properties_input *out)
{
    mc_resource_files *files = context;
    if (out == NULL || !resource_name(name))
        return MC_REGISTRY_INVALID_ARGUMENT;
    const size_t name_length = strlen(name);
    size_t skip = files->root[files->root_length - 1] == '/' ? 1u : 0u;
#ifdef _WIN32
    if (files->root[files->root_length - 1] == '\\')
        skip = 1;
    /* A bare drive root denotes that drive's current directory. Adding a
     * slash would silently change C: into the absolute C:/ directory. */
    if (files->root_length == 2 && files->root[1] == ':' &&
        ((files->root[0] >= 'A' && files->root[0] <= 'Z') ||
         (files->root[0] >= 'a' && files->root[0] <= 'z')))
        skip = 1;
#endif
    const size_t suffix_length = name_length - skip;
    if (files->root_length > SIZE_MAX - suffix_length - 1)
        return MC_REGISTRY_LIMIT;
    char *path = malloc(files->root_length + suffix_length + 1);
    if (path == NULL)
        return MC_REGISTRY_ALLOCATION_FAILED;
    memcpy(path, files->root, files->root_length);
    memcpy(path + files->root_length, name + skip, suffix_length + 1);
    FILE *file = NULL;
    mc_registry_status status = open_file(path, &file);
    free(path);
    if (status != MC_REGISTRY_OK)
        return status;
    resource_stream *stream = malloc(sizeof(*stream));
    if (stream == NULL) {
        (void)fclose(file);
        return MC_REGISTRY_ALLOCATION_FAILED;
    }
    *stream = (resource_stream){.file = file, .pending_error = false, .next = files->streams};
    files->streams = stream;
    *out = (mc_java_properties_input){.context = stream, .read = read_bytes};
    return MC_REGISTRY_OK;
}

mc_registry_status mc_resource_files_create(const char *root, mc_resource_files **out)
{
    if (root == NULL || out == NULL || root[0] == '\0')
        return MC_REGISTRY_INVALID_ARGUMENT;
    const size_t length = strlen(root);
    if (length == SIZE_MAX)
        return MC_REGISTRY_LIMIT;
    if (!utf8((const unsigned char *)root, length))
        return MC_REGISTRY_INVALID_ARGUMENT;
    mc_resource_files *files = malloc(sizeof(*files));
    if (files == NULL)
        return MC_REGISTRY_ALLOCATION_FAILED;
    char *copy = malloc(length + 1);
    if (copy == NULL) {
        free(files);
        return MC_REGISTRY_ALLOCATION_FAILED;
    }
    memcpy(copy, root, length + 1);
    *files = (mc_resource_files){.source = {.context = files, .open = open_resource},
                                 .root = copy,
                                 .root_length = length,
                                 .streams = NULL};
    *out = files;
    return MC_REGISTRY_OK;
}

const mc_registry_resources *mc_resource_files_source(mc_resource_files *files)
{
    return files == NULL ? NULL : &files->source;
}

mc_registry_status mc_resource_files_free(mc_resource_files *files)
{
    if (files == NULL)
        return MC_REGISTRY_OK;
    mc_registry_status status = MC_REGISTRY_OK;
    resource_stream *stream = files->streams;
    while (stream != NULL) {
        resource_stream *next = stream->next;
        if (fclose(stream->file) != 0)
            status = MC_REGISTRY_IO_FAILED;
        free(stream);
        stream = next;
    }
    free(files->root);
    free(files);
    return status;
}
