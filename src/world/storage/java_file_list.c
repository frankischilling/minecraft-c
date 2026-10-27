#ifndef _WIN32
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#endif
#include "java_file_internal.h"
#include "java_file_text.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <dirent.h>
#endif

static mc_java_file_status append_name(mc_java_file_array *a, mc_world_info_name *name,
                                       mc_java_file_error *e) {
    if (a->names_count >= a->files->options.max_entries || a->names_count >= INT32_MAX)
        return mc_file_result(e, MC_JAVA_FILE_LIMIT, "File enumeration count", 0);
    if (a->names_count == a->names_capacity) {
        size_t next = a->names_capacity == 0              ? 16u
                      : a->names_capacity > SIZE_MAX / 2u ? SIZE_MAX
                                                          : a->names_capacity * 2u;
        if (next > a->files->options.max_entries)
            next = a->files->options.max_entries;
        if (next > SIZE_MAX / sizeof(*a->names))
            return mc_file_result(e, MC_JAVA_FILE_LIMIT, "File enumeration storage", 0);
        mc_world_info_name **grown = realloc(a->names, next * sizeof(*grown));
        if (grown == NULL)
            return mc_file_result(e, MC_JAVA_FILE_ALLOCATION_FAILED, "File enumeration storage", 0);
        a->names = grown;
        a->names_capacity = next;
    }
    a->names[a->names_count++] = name;
    return MC_JAVA_FILE_OK;
}
#ifdef _WIN32
static mc_java_file_status native_name(mc_java_files *files, const uint16_t *u, size_t n,
                                       mc_world_info_name **out, mc_java_file_error *e) {
    mc_world_info_status s =
        mc_world_info_name_copy((mc_nbt_string){u, n}, files->options.max_string_bytes, out);
    return s == MC_WORLD_INFO_OK ? MC_JAVA_FILE_OK
                                 : mc_file_result(e,
                                                  s == MC_WORLD_INFO_LIMIT ? MC_JAVA_FILE_LIMIT
                                                  : s == MC_WORLD_INFO_ALLOCATION_FAILED
                                                      ? MC_JAVA_FILE_ALLOCATION_FAILED
                                                      : MC_JAVA_FILE_STATE_ERROR,
                                                  "File native name String", 0);
}
#endif
static mc_java_file_status enumerate(mc_java_file *f, mc_java_file_array *a,
                                     const mc_file_native_char *path, bool *source_null,
                                     mc_java_file_error *e) {
    mc_java_file_status status = MC_JAVA_FILE_OK;
#ifdef _WIN32
    size_t n = wcslen(path);
    if (n == 0) {
        *source_null = true;
        return MC_JAVA_FILE_OK;
    }
    DWORD attributes = GetFileAttributesW(path);
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        if (attributes == INVALID_FILE_ATTRIBUTES)
            f->state.last_native_error = (int)GetLastError();
        *source_null = true;
        return MC_JAVA_FILE_OK;
    }
    if (n > (SIZE_MAX / sizeof(wchar_t)) - 3u)
        return mc_file_result(e, MC_JAVA_FILE_LIMIT, "File enumeration path", 0);
    wchar_t *pattern = malloc((n + 3u) * sizeof(*pattern));
    if (pattern == NULL)
        return mc_file_result(e, MC_JAVA_FILE_ALLOCATION_FAILED, "File enumeration path", 0);
    memcpy(pattern, path, n * sizeof(*pattern));
    while (n > 0 && pattern[n - 1u] == L' ')
        --n;
    if (n == 0) {
        free(pattern);
        return mc_file_result(e, MC_JAVA_FILE_UNSUPPORTED_PROVIDER,
                              "File all-space native directory", 0);
    }
    bool root_like = (n == 1 && pattern[0] == L'\\') || (n == 2 && pattern[1] == L':') ||
                     (n == 3 && pattern[1] == L':' && pattern[2] == L'\\');
    if (!root_like)
        pattern[n++] = L'\\';
    pattern[n++] = L'*';
    pattern[n] = L'\0';
    WIN32_FIND_DATAW data;
    HANDLE search = FindFirstFileW(pattern, &data);
    DWORD code = GetLastError();
    free(pattern);
    if (search == INVALID_HANDLE_VALUE) {
        f->state.last_native_error = (int)code;
        *source_null = code != ERROR_FILE_NOT_FOUND;
        return MC_JAVA_FILE_OK;
    }
    a->issued_native_close_token = search;
    for (;;) {
        if (wcscmp(data.cFileName, L".") != 0 && wcscmp(data.cFileName, L"..") != 0) {
            mc_world_info_name *name = NULL;
            status = native_name(f->files, (const uint16_t *)data.cFileName, wcslen(data.cFileName),
                                 &name, e);
            if (status == MC_JAVA_FILE_OK)
                status = append_name(a, name, e);
            if (status != MC_JAVA_FILE_OK) {
                mc_world_info_name_release(name);
                break;
            }
        }
        if (!FindNextFileW(search, &data)) {
            code = GetLastError();
            if (code != ERROR_NO_MORE_FILES) {
                f->state.last_native_error = (int)code;
                *source_null = true;
            }
            break;
        }
    }
    a->native_close_issued = true;
    if (!FindClose(search)) {
        a->native_close_failed = true;
        if (status == MC_JAVA_FILE_OK)
            status = mc_file_result(e, MC_JAVA_FILE_UNSUPPORTED_PROVIDER,
                                    "File native enumeration close", (int)GetLastError());
    }
#else
    DIR *directory = opendir(path);
    if (directory == NULL) {
        f->state.last_native_error = errno;
        *source_null = true;
        return MC_JAVA_FILE_OK;
    }
    a->issued_native_close_token = directory;
    for (;;) {
        errno = 0;
        struct dirent *entry = readdir(directory);
        if (entry == NULL) {
            if (errno != 0)
                status = mc_file_result(e, MC_JAVA_FILE_UNSUPPORTED_PROVIDER,
                                        "File partial native enumeration", errno);
            break;
        }
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        mc_world_info_name *name = NULL;
        status = mc_file_decode_utf8(entry->d_name, f->files->options.max_string_bytes, &name, e);
        if (status == MC_JAVA_FILE_OK)
            status = append_name(a, name, e);
        if (status != MC_JAVA_FILE_OK) {
            mc_world_info_name_release(name);
            break;
        }
    }
    a->native_close_issued = true;
    if (closedir(directory) != 0) {
        a->native_close_failed = true;
        if (status == MC_JAVA_FILE_OK)
            status = mc_file_result(e, MC_JAVA_FILE_UNSUPPORTED_PROVIDER,
                                    "File native enumeration close", errno);
    }
#endif
    return status;
}

mc_java_file_status mc_java_file_list(mc_java_file *f, mc_java_file_array **out,
                                      mc_java_file_error *e) {
    if (out == NULL)
        return mc_file_result(e, MC_JAVA_FILE_INVALID_ARGUMENT, "File list output", 0);
    mc_java_file_status status = mc_file_ready(f, e);
    if (status != MC_JAVA_FILE_OK)
        return status;
    if (f->failed_array != NULL)
        return mc_file_result(e, MC_JAVA_FILE_BUSY, "File pending failed enumeration", 0);
    if (mc_file_invalid(f)) {
        *out = NULL;
        return mc_file_result(e, MC_JAVA_FILE_OK, NULL, 0);
    }
    f->state.active = true;
    mc_java_file_array *a = calloc(1, sizeof(*a));
    if (a == NULL) {
        f->state.active = false;
        return mc_file_result(e, MC_JAVA_FILE_ALLOCATION_FAILED,
                              "File enumeration native candidate", 0);
    }
    status = mc_java_files_retain(f->files, e);
    if (status != MC_JAVA_FILE_OK) {
        free(a);
        f->state.active = false;
        return status;
    }
    a->files = f->files;
    a->holds = 1;
    a->active = true;
    f->failed_array = a;
    f->state.has_failed_array = true;
    mc_file_native_char *path = NULL;
    status = mc_file_native(f, &path, e);
    bool source_null = false;
    if (status == MC_JAVA_FILE_OK)
        status = enumerate(f, a, path, &source_null, e);
    free(path);
    if (status != MC_JAVA_FILE_OK)
        goto failed;
    if (source_null) {
        a->active = false;
        f->state.active = false;
        status = mc_java_file_clear_failure(f, e);
        if (status == MC_JAVA_FILE_OK)
            *out = NULL;
        return status;
    }
    /* FileSystem.list has now completed its String[] and native close. Only
     * now allocate the actual File[] and each private child in source order. */
    a->count = a->names_count;
    if (a->count > SIZE_MAX / sizeof(*a->items)) {
        status = mc_file_result(e, MC_JAVA_FILE_LIMIT, "File[] storage", 0);
        goto failed;
    }
    if (a->count != 0) {
        a->items = calloc(a->count, sizeof(*a->items));
        if (a->items == NULL) {
            status = mc_file_result(e, MC_JAVA_FILE_ALLOCATION_FAILED, "File[] storage", 0);
            goto failed;
        }
    }
    a->source_array_allocated = true;
    for (size_t i = 0; i < a->count; ++i) {
        status = mc_java_file_allocate(f->files, &a->pending_file, e);
        if (status != MC_JAVA_FILE_OK)
            goto failed;
        status = mc_file_construct_private_child(a->pending_file, f, a->names[i], e);
        if (status != MC_JAVA_FILE_OK)
            goto failed;
        a->items[i] = a->pending_file;
        a->pending_file = NULL;
        ++a->constructed;
    }
    for (size_t i = 0; i < a->names_count; ++i)
        mc_world_info_name_release(a->names[i]);
    free(a->names);
    a->names = NULL;
    a->names_count = 0;
    a->names_capacity = 0;
    a->active = false;
    f->failed_array = NULL;
    f->state.has_failed_array = false;
    f->state.active = false;
    *out = a;
    return mc_file_result(e, MC_JAVA_FILE_OK, NULL, 0);
failed:
    a->active = false;
    f->state.active = false;
    return status;
}

size_t mc_java_file_array_length(const mc_java_file_array *a) {
    return a == NULL ? 0 : a->count;
}

mc_java_file *mc_java_file_array_at(const mc_java_file_array *a, size_t i) {
    return a != NULL && i < a->constructed ? a->items[i] : NULL;
}

mc_java_file_status mc_java_file_array_retain(mc_java_file_array *a, mc_java_file_error *e) {
    if (a == NULL)
        return mc_file_result(e, MC_JAVA_FILE_INVALID_ARGUMENT, "File[] retain", 0);
    if (a->active || a->retiring)
        return mc_file_result(e, MC_JAVA_FILE_BUSY, "File[] active/terminal retain", 0);
    if (a->holds == SIZE_MAX)
        return mc_file_result(e, MC_JAVA_FILE_LIMIT, "File[] holds", 0);
    ++a->holds;
    return mc_file_result(e, MC_JAVA_FILE_OK, NULL, 0);
}

mc_java_file_status mc_java_file_array_release(mc_java_file_array **slot, mc_java_file_error *e) {
    if (slot == NULL)
        return mc_file_result(e, MC_JAVA_FILE_INVALID_ARGUMENT, "File[] release slot", 0);
    mc_java_file_array *a = *slot;
    if (a == NULL)
        return mc_file_result(e, MC_JAVA_FILE_OK, NULL, 0);
    if (a->active)
        return mc_file_result(e, MC_JAVA_FILE_BUSY, "File[] active retirement", 0);
    if (a->holds > 1) {
        --a->holds;
        *slot = NULL;
        return mc_file_result(e, MC_JAVA_FILE_OK, NULL, 0);
    }
    if (a->files->active || a->files->retiring)
        return mc_file_result(e, MC_JAVA_FILE_BUSY, "File[] namespace active retirement", 0);
    a->retiring = true;
    a->active = true;
    mc_java_file_status status = mc_java_file_release(&a->pending_file, e);
    if (status != MC_JAVA_FILE_OK) {
        a->active = false;
        return status;
    }
    for (size_t i = 0; i < a->constructed; ++i) {
        status = mc_java_file_release(&a->items[i], e);
        if (status != MC_JAVA_FILE_OK) {
            a->active = false;
            return status;
        }
    }
    for (size_t i = 0; i < a->names_count; ++i)
        mc_world_info_name_release(a->names[i]);
    free(a->names);
    free(a->items);
    a->retiring = true;
    mc_java_files *files = a->files;
    *slot = NULL;
    free(a);
    return mc_java_files_release(&files, e);
}
