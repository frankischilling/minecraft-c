#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "mc/resources/files/files.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <io.h>
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

static bool utf8(const unsigned char *text, size_t length) {
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

static bool resource_name(const char *name) {
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
                                            size_t *count) {
    resource_stream *stream = context;
    if (destination == NULL || count == NULL || capacity == 0)
        return MC_JAVA_PROPERTIES_INVALID_ARGUMENT;
    *count = 0;
    if (stream->file == NULL || stream->pending_error)
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

#ifdef _WIN32
static wchar_t *resource_native_path(const char *path, mc_registry_status *status) {
    const size_t length = strlen(path);
    if (length > (size_t)INT_MAX) {
        *status = MC_REGISTRY_LIMIT;
        return NULL;
    }
    const int units =
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, (int)length, NULL, 0);
    if (units == 0) {
        *status = MC_REGISTRY_INVALID_ARGUMENT;
        return NULL;
    }
    if ((size_t)units > SIZE_MAX / sizeof(wchar_t) - 1) {
        *status = MC_REGISTRY_LIMIT;
        return NULL;
    }
    wchar_t *native = malloc(((size_t)units + 1) * sizeof(*native));
    if (native == NULL) {
        *status = MC_REGISTRY_ALLOCATION_FAILED;
        return NULL;
    }
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, (int)length, native, units) !=
        units) {
        free(native);
        {
            *status = MC_REGISTRY_INVALID_ARGUMENT;
            return NULL;
        }
    }
    native[units] = L'\0';
    *status = MC_REGISTRY_OK;
    return native;
}
#endif

static mc_registry_status open_file(const char *path, FILE **out) {
    FILE *file = NULL;
    int code;
#ifdef _WIN32
    mc_registry_status native_status = MC_REGISTRY_OK;
    wchar_t *native = resource_native_path(path, &native_status);
    if (native == NULL)
        return native_status;
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

static mc_registry_status resource_path(mc_resource_files *files, const char *name, char **out) {
    if (files == NULL || out == NULL || !resource_name(name))
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
    *out = path;
    return MC_REGISTRY_OK;
}

static mc_registry_status open_resource(void *context, const char *name,
                                        mc_java_properties_input *out) {
    mc_resource_files *files = context;
    if (out == NULL || !resource_name(name))
        return MC_REGISTRY_INVALID_ARGUMENT;
    char *path = NULL;
    const mc_registry_status prepared = resource_path(files, name, &path);
    if (prepared != MC_REGISTRY_OK)
        return prepared;
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

static mc_registry_status close_resource(void *context, const mc_java_properties_input *input) {
    mc_resource_files *files = context;
    if (input == NULL || input->read != read_bytes)
        return MC_REGISTRY_INVALID_ARGUMENT;
    for (resource_stream *stream = files->streams; stream != NULL; stream = stream->next) {
        if (stream != input->context)
            continue;
        if (stream->file == NULL)
            return MC_REGISTRY_OK;
        FILE *file = stream->file;
        stream->file = NULL;
        return fclose(file) == 0 ? MC_REGISTRY_OK : MC_REGISTRY_IO_FAILED;
    }
    return MC_REGISTRY_INVALID_ARGUMENT;
}

static bool ready_resource(void *context, const mc_java_properties_input *input) {
    mc_resource_files *files = context;
    if (input == NULL || input->read != read_bytes)
        return false;
    for (resource_stream *stream = files->streams; stream != NULL; stream = stream->next) {
        if (stream != input->context)
            continue;
        if (stream->file == NULL || stream->pending_error || ferror(stream->file))
            return false;
            /* Query the open file and logical stdio position without consuming
             * bytes or changing its seek position. Availability failure is false,
             * matching StreamDecoder's caught available() IOException. */
#ifdef _WIN32
        struct _stat64 information;
        const __int64 position = _ftelli64(stream->file);
        return position >= 0 && _fstat64(_fileno(stream->file), &information) == 0 &&
               information.st_size > position;
#else
        struct stat information;
        const off_t position = ftello(stream->file);
        return position >= 0 && fstat(fileno(stream->file), &information) == 0 &&
               information.st_size > position;
#endif
    }
    return false;
}

mc_registry_status mc_resource_files_create(const char *root, mc_resource_files **out) {
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
    *files = (mc_resource_files){.source = {.context = files,
                                            .open = open_resource,
                                            .close = close_resource,
                                            .ready = ready_resource},
                                 .root = copy,
                                 .root_length = length,
                                 .streams = NULL};
    *out = files;
    return MC_REGISTRY_OK;
}

const mc_registry_resources *mc_resource_files_source(mc_resource_files *files) {
    return files == NULL ? NULL : &files->source;
}

mc_registry_status mc_resource_files_free(mc_resource_files *files) {
    if (files == NULL)
        return MC_REGISTRY_OK;
    mc_registry_status status = MC_REGISTRY_OK;
    resource_stream *stream = files->streams;
    while (stream != NULL) {
        resource_stream *next = stream->next;
        if (stream->file != NULL && fclose(stream->file) != 0)
            status = MC_REGISTRY_IO_FAILED;
        free(stream);
        stream = next;
    }
    free(files->root);
    free(files);
    return status;
}

mc_registry_status mc_resource_files_locate(mc_resource_files *files, const char *name,
                                            bool *found) {
    if (found == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    char *path = NULL;
    mc_registry_status status = resource_path(files, name, &path);
    if (status != MC_REGISTRY_OK)
        return status;
    bool exists = false;
#ifdef _WIN32
    wchar_t *native = resource_native_path(path, &status);
    if (native == NULL) {
        free(path);
        return status;
    }
    const DWORD attributes = GetFileAttributesW(native);
    if (attributes != INVALID_FILE_ATTRIBUTES)
        exists = true;
    else {
        const DWORD code = GetLastError();
        if (code != ERROR_FILE_NOT_FOUND && code != ERROR_PATH_NOT_FOUND)
            status = MC_REGISTRY_IO_FAILED;
    }
    free(native);
#else
    struct stat attributes;
    if (stat(path, &attributes) == 0)
        exists = true;
    else if (errno != ENOENT && errno != ENOTDIR)
        status = MC_REGISTRY_IO_FAILED;
#endif
    free(path);
    if (status == MC_REGISTRY_OK)
        *found = exists;
    return status;
}
