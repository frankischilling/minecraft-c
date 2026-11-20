#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include "../../platform/files/directory.h"
#include "internal.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#ifdef _WIN32
static wchar_t *wide_path(const char *path, mc_client_stats_error *error) {
    const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, NULL, 0);
    if (count <= 0) {
        stats_fail(error, MC_CLIENT_STATS_UNSUPPORTED_PROFILE, "Stats native UTF8 path conversion",
                   NULL, NULL, NULL, 0);
        return NULL;
    }
    wchar_t *wide = malloc((size_t)count * sizeof(*wide));
    if (wide == NULL)
        stats_fail(error, MC_CLIENT_STATS_ALLOCATION_FAILED, "Stats native path conversion storage",
                   NULL, NULL, NULL, 0);
    if (wide != NULL &&
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wide, count) == 0) {
        free(wide);
        stats_fail(error, MC_CLIENT_STATS_UNSUPPORTED_PROFILE, "Stats native UTF8 path conversion",
                   NULL, NULL, NULL, 0);
        return NULL;
    }
    return wide;
}

static char *utf8_name(stats_memory *memory, const wchar_t *name, mc_client_stats_error *error) {
    const int count =
        WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, name, -1, NULL, 0, NULL, NULL);
    if (count <= 0) {
        stats_fail(error, MC_CLIENT_STATS_UNSUPPORTED_PROFILE, "Stats directory name encoding",
                   "xi", "<init>", "(Lgr;Ljava/io/File;)V", 78);
        return NULL;
    }
    char *text = stats_alloc(memory, (size_t)count, error);
    if (text != NULL)
        (void)WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, name, -1, text, count, NULL, NULL);
    return text;
}
#endif

static bool exists_path(const char *path, bool *is_directory, mc_client_stats_error *error) {
#ifdef _WIN32
    wchar_t *wide = wide_path(path, error);
    if (wide == NULL)
        return false;
    const DWORD attributes = GetFileAttributesW(wide);
    free(wide);
    if (attributes == INVALID_FILE_ATTRIBUTES)
        return false;
    if (is_directory != NULL)
        *is_directory = (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
    (void)error;
    struct stat information;
    if (stat(path, &information) != 0)
        return false;
    if (is_directory != NULL)
        *is_directory = S_ISDIR(information.st_mode);
#endif
    return true;
}

bool stats_file_exists(const stats_file *file, mc_client_stats_error *error) {
    return file != NULL && exists_path(file->path, NULL, error);
}

static void mkdir_path(const char *path, mc_client_stats_error *error) {
#ifdef _WIN32
    wchar_t *wide = wide_path(path, error);
    if (wide != NULL) {
        mc_file_io_state state = {0};
        (void)mc_file_native_mkdir(wide, &state);
        free(wide);
    }
#else
    (void)error;
    mc_file_io_state state = {0};
    (void)mc_file_native_mkdir(path, &state);
#endif
}

static void rename_path(const char *from, const char *to, mc_client_stats_error *error) {
#ifdef _WIN32
    wchar_t *first = wide_path(from, error), *second = first == NULL ? NULL : wide_path(to, error);
    if (first != NULL && second != NULL)
        (void)mc_file_native_rename(first, second, NULL);
    free(first);
    free(second);
#else
    (void)error;
    (void)mc_file_native_rename(from, to, NULL);
#endif
}

static void delete_path(const char *path, mc_client_stats_error *error) {
#ifdef _WIN32
    wchar_t *wide = wide_path(path, error);
    if (wide != NULL) {
        const DWORD attributes = GetFileAttributesW(wide);
        if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
            (void)RemoveDirectoryW(wide);
        else
            (void)DeleteFileW(wide);
        free(wide);
    }
#else
    (void)error;
    (void)remove(path);
#endif
}

static FILE *open_read(const char *path, mc_client_stats_error *error) {
#ifdef _WIN32
    wchar_t *wide = wide_path(path, error);
    if (wide == NULL)
        return NULL;
    FILE *stream = _wfopen(wide, L"rb");
    free(wide);
    return stream;
#else
    (void)error;
    FILE *stream = fopen(path, "rb");
    if (stream != NULL) {
        struct stat information;
        if (fstat(fileno(stream), &information) != 0) {
            const int failure = errno;
            (void)fclose(stream);
            errno = failure;
            return NULL;
        }
        if (S_ISDIR(information.st_mode)) {
            (void)fclose(stream);
            errno = EISDIR;
            return NULL;
        }
    }
    return stream;
#endif
}

static char *join_path(stats_memory *memory, const char *parent, const char *name,
                       mc_client_stats_error *error) {
    const size_t first = parent == NULL ? 0 : strlen(parent), second = strlen(name);
    if (first > 1048576u || second > 1048576u || first + second > 1048574u) {
        stats_fail(error, MC_CLIENT_STATS_LIMIT, "Stats native path bound", NULL, NULL, NULL, 0);
        return NULL;
    }
    bool slash = first != 0 && parent[first - 1] != '/';
#ifdef _WIN32
    slash = slash && parent[first - 1] != '\\';
#endif
    char *path = stats_alloc(memory, first + (slash ? 1u : 0u) + second + 1u, error);
    if (path != NULL) {
        if (first != 0)
            memcpy(path, parent, first);
        if (slash)
            path[first] = '/';
        memcpy(path + first + (slash ? 1u : 0u), name, second + 1u);
    }
    return path;
}

static mc_client_stats_status relocate(mc_client_stats_writer *writer, const char *directory,
                                       const char *target_directory, const char *name,
                                       mc_client_stats_report *report,
                                       mc_client_stats_error *error) {
    const size_t length = strlen(name);
    if (length < 10 || memcmp(name, "stats_", 6) != 0 || memcmp(name + length - 4, ".dat", 4) != 0)
        return MC_CLIENT_STATS_OK;
    char *target = join_path(writer->memory, target_directory, name, error);
    if (target == NULL)
        return error->status;
    mc_client_stats_status status = MC_CLIENT_STATS_OK;
    const bool target_exists = exists_path(target, NULL, error);
    if (error->status != MC_CLIENT_STATS_OK) {
        stats_free(target);
        return error->status;
    }
    if (!target_exists) {
        const mc_client_stats_diagnostic event = {.kind = MC_CLIENT_STATS_RELOCATING,
                                                  .path_utf8 = name};
        status = stats_emit(writer, &writer->options, &event, report, error);
        if (status == MC_CLIENT_STATS_OK) {
            char *source = join_path(writer->memory, directory, name, error);
            if (source == NULL)
                status = error->status;
            else {
                rename_path(source, target, error);
                status = error->status;
                stats_free(source);
            }
        }
    }
    stats_free(target);
    return status;
}

typedef struct directory_name {
    struct directory_name *next;
    char *text;
} directory_name;

static mc_client_stats_status keep_name(mc_client_stats_writer *writer, const char *text,
                                        directory_name ***tail, mc_client_stats_error *error) {
    const size_t length = strlen(text);
    if (length < 10 || memcmp(text, "stats_", 6) != 0 || memcmp(text + length - 4, ".dat", 4) != 0)
        return MC_CLIENT_STATS_OK;
    directory_name *name = stats_alloc(writer->memory, sizeof(*name), error);
    if (name == NULL)
        return error->status;
    *name = (directory_name){0};
    name->text = stats_alloc(writer->memory, length + 1u, error);
    if (name->text == NULL) {
        stats_free(name);
        return error->status;
    }
    memcpy(name->text, text, length + 1u);
    **tail = name;
    *tail = &name->next;
    return MC_CLIENT_STATS_OK;
}

mc_client_stats_status stats_directory(mc_client_stats_writer *writer, const char *directory,
                                       char **out, mc_client_stats_report *report,
                                       mc_client_stats_error *error) {
    char *target = join_path(writer->memory, directory, "stats", error);
    if (target == NULL)
        return error->status;
    writer->pc = 38;
    mc_client_stats_status status = MC_CLIENT_STATS_OK;
    directory_name *names = NULL, **tail = &names;
    const bool target_exists = exists_path(target, NULL, error);
    if (error->status != MC_CLIENT_STATS_OK) {
        status = error->status;
        goto finish;
    }
    if (!target_exists)
        mkdir_path(target, error);
    if (error->status != MC_CLIENT_STATS_OK) {
        status = error->status;
        goto finish;
    }
    writer->pc = 50;
    if (directory == NULL) {
        status = stats_exception(error, MC_CLIENT_STATS_NULL_REFERENCE,
                                 "Stats source null directory listFiles", "xi", "<init>",
                                 "(Lgr;Ljava/io/File;)V", 55);
        goto finish;
    }
#ifdef _WIN32
    char *pattern = join_path(writer->memory, directory, "*", error);
    if (pattern == NULL) {
        status = error->status;
        goto finish;
    }
    wchar_t *wide = wide_path(pattern, error);
    stats_free(pattern);
    if (wide == NULL) {
        status = error->status;
        goto finish;
    }
    WIN32_FIND_DATAW entry;
    HANDLE search = FindFirstFileW(wide, &entry);
    const DWORD search_error = search == INVALID_HANDLE_VALUE ? GetLastError() : ERROR_SUCCESS;
    free(wide);
    if (search == INVALID_HANDLE_VALUE) {
        bool is_directory = false;
        const bool exists =
            search_error == ERROR_FILE_NOT_FOUND && exists_path(directory, &is_directory, error);
        if (error->status != MC_CLIENT_STATS_OK) {
            status = error->status;
            goto finish;
        }
        if (!exists || !is_directory) {
            status = stats_exception(error, MC_CLIENT_STATS_NULL_REFERENCE,
                                     "Stats source null directory listing", "xi", "<init>",
                                     "(Lgr;Ljava/io/File;)V", 62);
            goto finish;
        }
    } else {
        do {
            char *name = utf8_name(writer->memory, entry.cFileName, error);
            if (name == NULL) {
                status = error->status;
                break;
            }
            status = keep_name(writer, name, &tail, error);
            stats_free(name);
            if (status != MC_CLIENT_STATS_OK)
                break;
        } while (FindNextFileW(search, &entry));
        if (status == MC_CLIENT_STATS_OK && GetLastError() != ERROR_NO_MORE_FILES)
            status = stats_exception(error, MC_CLIENT_STATS_NULL_REFERENCE,
                                     "Stats source null directory listing", "xi", "<init>",
                                     "(Lgr;Ljava/io/File;)V", 62);
        (void)FindClose(search);
    }
#else
    DIR *search = opendir(directory);
    if (search == NULL) {
        status = stats_exception(error, MC_CLIENT_STATS_NULL_REFERENCE,
                                 "Stats source null directory listing", "xi", "<init>",
                                 "(Lgr;Ljava/io/File;)V", 62);
        goto finish;
    }
    struct dirent *entry;
    for (;;) {
        errno = 0;
        entry = readdir(search);
        if (entry == NULL) {
            if (errno != 0)
                status = stats_exception(error, MC_CLIENT_STATS_NULL_REFERENCE,
                                         "Stats source null directory listing", "xi", "<init>",
                                         "(Lgr;Ljava/io/File;)V", 62);
            break;
        }
        status = keep_name(writer, entry->d_name, &tail, error);
        if (status != MC_CLIENT_STATS_OK)
            break;
    }
    (void)closedir(search);
#endif
    /* Java listFiles55 completes its entire provider-order array before any
     * relocation. Never mutate the directory while its native scan is live. */
    if (status == MC_CLIENT_STATS_OK) {
        writer->pc = 62;
        for (directory_name *name = names; name != NULL; name = name->next) {
            status = relocate(writer, directory, target, name->text, report, error);
            if (status != MC_CLIENT_STATS_OK)
                break;
        }
    }
finish:
    while (names != NULL) {
        directory_name *next = names->next;
        stats_free(names->text);
        stats_free(names);
        names = next;
    }
    if (status == MC_CLIENT_STATS_OK) {
        *out = target;
        writer->pc = 174;
    } else
        stats_free(target);
    return status;
}

mc_client_stats_status stats_make_files(mc_client_stats_writer *writer, mc_player_session *session,
                                        const char *directory, mc_client_stats_error *error) {
    stats_syncher *syncher = writer->retained_syncher;
    static const char *const suffixes[6] = {"_unsent.dat", ".dat",        "_unsent.old",
                                            ".old",        "_unsent.tmp", ".tmp"};
    static const unsigned int stores[6] = {68, 110, 152, 194, 236, 278};
    if (session == NULL)
        return stats_exception(error, MC_CLIENT_STATS_NULL_REFERENCE, "Stats source null Session",
                               "cj", "<init>", "(Lgr;Lxi;Ljava/io/File;)V", 48);
    for (size_t i = 0; i < 6; ++i) {
        mc_player_string_view username;
        if (!mc_player_string_read(session->username, &username))
            return stats_exception(error, MC_CLIENT_STATS_NULL_REFERENCE,
                                   "Stats source null username", "cj", "<init>",
                                   "(Lgr;Lxi;Ljava/io/File;)V", 51u + (unsigned int)i * 42u);
        if (writer->options.name_case != MC_CLIENT_STATS_NAME_ENGLISH_ASCII)
            return stats_fail(error, MC_CLIENT_STATS_UNSUPPORTED_PROFILE,
                              "Stats username Locale profile", "cj", "<init>",
                              "(Lgr;Lxi;Ljava/io/File;)V", 51u + (unsigned int)i * 42u);
        if (username.length > writer->options.limits.max_text_units || username.length > 1048500u)
            return stats_fail(error, MC_CLIENT_STATS_LIMIT, "Stats filename text bound", "cj",
                              "<init>", "(Lgr;Lxi;Ljava/io/File;)V", 51u + (unsigned int)i * 42u);
        for (size_t j = 0; j < username.length; ++j) {
            if (username.units[j] > 127)
                return stats_fail(error, MC_CLIENT_STATS_UNSUPPORTED_PROFILE,
                                  "Stats nonASCII default Locale lowercasing", "cj", "<init>",
                                  "(Lgr;Lxi;Ljava/io/File;)V", 51u + (unsigned int)i * 42u);
            if (username.units[j] == 0 || username.units[j] == '/' || username.units[j] == '\\' ||
                username.units[j] == ':')
                return stats_fail(error, MC_CLIENT_STATS_UNSUPPORTED_PROFILE,
                                  "Stats native filename component profile", "cj", "<init>",
                                  "(Lgr;Lxi;Ljava/io/File;)V", 65u + (unsigned int)i * 42u);
        }
        const size_t suffix = strlen(suffixes[i]);
        char *name = stats_alloc(writer->memory, username.length + suffix + 7u, error);
        if (name == NULL)
            return error->status;
        memcpy(name, "stats_", 6);
        for (size_t j = 0; j < username.length; ++j) {
            const uint16_t unit = username.units[j];
            name[6u + j] = (char)(unit >= 'A' && unit <= 'Z' ? unit + ('a' - 'A') : unit);
        }
        memcpy(name + 6u + username.length, suffixes[i], suffix + 1u);
        stats_file *file = stats_alloc(writer->memory, sizeof(*file), error);
        if (file == NULL) {
            stats_free(name);
            return error->status;
        }
        file->path = join_path(writer->memory, directory, name, error);
        stats_free(name);
        if (file->path == NULL) {
            stats_free(file);
            return error->status;
        }
        syncher->files[i] = file;
        syncher->pc = stores[i];
    }
    mc_player_string_view username;
    (void)mc_player_string_read(session->username, &username);
    bool different = false;
    for (size_t j = 0; j < username.length; ++j)
        if (username.units[j] >= 'A' && username.units[j] <= 'Z')
            different = true;
    if (different) {
        for (size_t i = 0; i < 6; ++i) {
            const size_t suffix = strlen(suffixes[i]);
            char *name = stats_alloc(writer->memory, username.length + suffix + 7u, error);
            if (name == NULL)
                return error->status;
            memcpy(name, "stats_", 6);
            for (size_t j = 0; j < username.length; ++j)
                name[6u + j] = (char)username.units[j];
            memcpy(name + 6u + username.length, suffixes[i], suffix + 1u);
            char *old = join_path(writer->memory, directory, name, error);
            stats_free(name);
            if (old == NULL)
                return error->status;
            bool is_directory = false;
            const bool old_exists = exists_path(old, &is_directory, error);
            if (error->status == MC_CLIENT_STATS_OK && old_exists && !is_directory) {
                const bool target_exists = stats_file_exists(syncher->files[i], error);
                if (error->status == MC_CLIENT_STATS_OK && !target_exists)
                    rename_path(old, syncher->files[i]->path, error);
            }
            stats_free(old);
            if (error->status != MC_CLIENT_STATS_OK)
                return error->status;
        }
    }
    syncher->pc = 514;
    return MC_CLIENT_STATS_OK;
}

void stats_files_clear(stats_syncher *syncher) {
    if (syncher == NULL)
        return;
    for (size_t i = 0; i < 6; ++i)
        if (syncher->files[i] != NULL) {
            stats_free(syncher->files[i]->path);
            stats_free(syncher->files[i]);
        }
}

mc_client_stats_status stats_catch(mc_client_stats_writer *writer, mc_client_stats_report *report,
                                   mc_client_stats_error *error) {
    const mc_client_stats_error cause = *error;
    report->file_exception_caught = true;
    report->caught = cause;
    const mc_client_stats_diagnostic event = {.kind = MC_CLIENT_STATS_CAUGHT_EXCEPTION,
                                              .cause = cause};
    const mc_client_stats_status status =
        stats_emit(writer, &writer->options, &event, report, error);
    if (status == MC_CLIENT_STATS_OK)
        stats_error_clear(error);
    return status;
}

static mc_java_properties_status read_bytes(void *context, uint8_t *bytes, size_t capacity,
                                            size_t *count) {
    stats_read_stream *stream = context;
    if (stream->source) {
        *count = 0;
        if (stream->file_status != MC_JAVA_FILE_OK)
            return MC_JAVA_PROPERTIES_IO_ERROR;
        mc_java_file_read observed = {0};
        stream->file_status =
            mc_java_file_stream_read(stream->file, bytes, capacity, &observed, &stream->file_error);
        stream->system_error = stream->file_error.system_error;
        if (stream->file_status != MC_JAVA_FILE_OK)
            return MC_JAVA_PROPERTIES_IO_ERROR;
        *count = observed.count;
        return MC_JAVA_PROPERTIES_OK;
    }
    *count = fread(bytes, 1, capacity, stream->stream);
    if (ferror(stream->stream)) {
        stream->system_error = errno;
        return MC_JAVA_PROPERTIES_IO_ERROR;
    }
    return MC_JAVA_PROPERTIES_OK;
}

static mc_java_reader_status close_stream(void *context) {
    stats_read_stream *stream = context;
    if (stream->source) {
        stream->file_status = mc_java_file_stream_source_close(stream->file, &stream->file_error);
        stream->system_error = stream->file_error.system_error;
        return stream->file_status == MC_JAVA_FILE_OK ? MC_JAVA_READER_OK : MC_JAVA_READER_IO_ERROR;
    }
    const int status = fclose(stream->stream);
    stream->stream = NULL;
    if (status != 0) {
        stream->system_error = errno;
        return MC_JAVA_READER_IO_ERROR;
    }
    return MC_JAVA_READER_OK;
}

static bool source_ready(void *context) {
    stats_read_stream *stream = context;
    if (stream->file_status != MC_JAVA_FILE_OK)
        return false;
    size_t available = 0;
    mc_java_file_error cause = {0};
    const mc_java_file_status status =
        mc_java_file_stream_available(stream->file, &available, &cause);
    if (status == MC_JAVA_FILE_IO_ERROR)
        return false; /* Actual StreamDecoder readiness catch. */
    if (status != MC_JAVA_FILE_OK) {
        stream->file_status = status;
        stream->file_error = cause;
        stream->system_error = cause.system_error;
        return false;
    }
    return available != 0;
}

mc_client_stats_status stats_read_file_set(mc_client_stats_writer *writer, size_t dat_index,
                                           mc_client_stats_map **out,
                                           mc_client_stats_report *report,
                                           mc_client_stats_error *error) {
    stats_source *source = writer->memory->source;
    const bool typed = source != NULL;
    stats_syncher *syncher = writer->retained_syncher;
    size_t selected = SIZE_MAX, selected_order = 0;
    static const size_t source_order[3] = {0, 4, 2}; /* Actual CJ dat/tmp/old order, BOTH APIs. */
    static const unsigned int exists_pc[3] = {1, 14, 27}, read_pc[3] = {9, 22, 35};
    for (size_t order = 0; order < 3; ++order) {
        const size_t index = dat_index + source_order[order];
        const bool exists = stats_owner_file_exists(writer, index, error);
        if (error->status != MC_CLIENT_STATS_OK) {
            if (typed) {
                error->class_name = "cj";
                error->method = "a";
                error->descriptor = "(Ljava/io/File;Ljava/io/File;Ljava/io/File;)Ljava/util/Map;";
                error->bytecode = exists_pc[order];
            }
            return error->status;
        }
        if (exists) {
            selected = index;
            selected_order = order;
            break;
        }
    }
    if (selected == SIZE_MAX) {
        *out = NULL;
        return MC_CLIENT_STATS_OK;
    }
    stats_read_stream local_stream = {0};
    stats_arena local_arena = {.memory = writer->memory};
    stats_builder local_text = {.memory = writer->memory,
                                .maximum = writer->options.limits.max_text_units};
    mc_java_reader *local_reader = NULL;
    stats_source_reader *owned = NULL;
    stats_read_stream *stream = &local_stream;
    stats_arena *arena = &local_arena;
    stats_builder *text = &local_text;
    mc_java_reader **reader = &local_reader;
    if (typed) {
        owned = stats_alloc(writer->memory, sizeof(*owned), error);
        if (owned == NULL)
            return error->status;
        *owned = (stats_source_reader){
            .next = source->readers,
            .input = {.source = true},
            .arena = {.memory = writer->memory},
            .text = {.memory = writer->memory, .maximum = writer->options.limits.max_text_units}};
        source->readers = owned;
        source->current_reader = owned;
        stream = &owned->input;
        arena = &owned->arena;
        text = &owned->text;
        reader = &owned->reader;
        mc_java_file_error child = {0};
        mc_java_file_status fs = mc_java_file_stream_allocate(source->files, &stream->file, &child);
        if (fs == MC_JAVA_FILE_OK)
            fs = mc_java_file_stream_construct(stream->file, source->children[selected], &child);
        stream->file_status = fs;
        stream->file_error = child;
        stream->system_error = child.system_error;
        if (fs != MC_JAVA_FILE_OK) {
            stats_source_child(error, child);
            error->system_error = child.system_error;
            if (fs == MC_JAVA_FILE_IO_ERROR) {
                stats_exception(error, MC_CLIENT_STATS_SOURCE_IO, "Stats actual FileReader open",
                                "cj", "a", "(Ljava/io/File;)Ljava/util/Map;", 11);
                stats_source_caught(writer, report, error, 82);
            } else
                stats_source_file_error(error, fs, child, "cj", "a",
                                        "(Ljava/io/File;)Ljava/util/Map;", 11);
            stats_source_frame(error, "cj", "a",
                               "(Ljava/io/File;Ljava/io/File;Ljava/io/File;)Ljava/util/Map;",
                               read_pc[selected_order]);
            return error->status;
        }
    } else {
        stream->stream = open_read(syncher->files[selected]->path, error);
        if (error->status != MC_CLIENT_STATS_OK)
            return error->status;
        if (stream->stream == NULL) {
            stats_exception(error, MC_CLIENT_STATS_SOURCE_IO, "Stats FileReader open", "cj", "a",
                            "(Ljava/io/File;)Ljava/util/Map;", 11);
            error->system_error = errno;
            const mc_client_stats_status status = stats_catch(writer, report, error);
            if (status == MC_CLIENT_STATS_OK)
                *out = NULL;
            return status;
        }
    }
    const mc_java_properties_allocator allocator = {arena, stats_arena_allocate};
    const mc_java_reader_input input = {
        .bytes = {stream, read_bytes}, .ready = typed ? source_ready : NULL, .close = close_stream};
    mc_java_reader_status rs =
        mc_java_reader_create(&allocator, &input, writer->options.charset, reader);
    if (typed && rs == MC_JAVA_READER_OK)
        owned->source_local_published = true; /* astore17. */
    mc_client_stats_status status = MC_CLIENT_STATS_OK;
    if (rs == MC_JAVA_READER_OK) {
        for (;;) {
            const mc_java_reader_line *line = NULL;
            rs = mc_java_reader_read_line(*reader, &line);
            /* A non-IO availability policy fault must not apply another line. */
            if (typed && stream->file_status != MC_JAVA_FILE_OK)
                break;
            if (rs != MC_JAVA_READER_OK || line == NULL)
                break;
            status = stats_append(text, line->units, line->length, error);
            if (status != MC_CLIENT_STATS_OK)
                break;
        }
    }
    mc_client_stats_map *map = NULL;
    if (typed && stream->file_status != MC_JAVA_FILE_OK &&
        stream->file_status != MC_JAVA_FILE_IO_ERROR)
        status = stats_source_file_error(error, stream->file_status, stream->file_error, "cj", "a",
                                         "(Ljava/io/File;)Ljava/util/Map;", 31);
    else if (status == MC_CLIENT_STATS_OK && rs == MC_JAVA_READER_OK &&
             (!typed || stream->file_status == MC_JAVA_FILE_OK)) {
        status = stats_parse(writer->registry, writer->memory, &writer->options, writer,
                             (mc_client_stats_string){text->units, text->length, false}, &map,
                             report, error);
        if (typed && status != MC_CLIENT_STATS_OK)
            stats_source_frame(error, "cj", "a", "(Ljava/io/File;)Ljava/util/Map;", 54);
    } else if (status == MC_CLIENT_STATS_OK) {
        error->reader = rs;
        if (rs == MC_JAVA_READER_IO_ERROR ||
            (typed && stream->file_status == MC_JAVA_FILE_IO_ERROR)) {
            stats_exception(error, MC_CLIENT_STATS_SOURCE_IO, "Stats BufferedReader read", "cj",
                            "a", "(Ljava/io/File;)Ljava/util/Map;", 31);
            error->system_error = stream->system_error;
            if (typed)
                stats_source_child(error, stream->file_error);
        } else
            stats_fail(error,
                       arena->failure != MC_CLIENT_STATS_OK ? arena->failure
                                                            : MC_CLIENT_STATS_UNSUPPORTED_PROFILE,
                       "Stats native Reader", "cj", "a", "(Ljava/io/File;)Ljava/util/Map;", 14);
        status = error->status;
    }
    if (typed) {
        owned->result = map;
        if (status == MC_CLIENT_STATS_SOURCE_EXCEPTION)
            status = stats_source_caught(writer, report, error, 82);
        if (status == MC_CLIENT_STATS_OK) {
            const mc_java_reader_status closed = mc_java_reader_close(*reader);
            if (stream->file_status != MC_JAVA_FILE_OK &&
                stream->file_status != MC_JAVA_FILE_IO_ERROR)
                status = stats_source_file_error(error, stream->file_status, stream->file_error,
                                                 "cj", "a", "(Ljava/io/File;)Ljava/util/Map;", 64);
            else if (closed != MC_JAVA_READER_OK) {
                stats_exception(error, MC_CLIENT_STATS_SOURCE_IO,
                                "Stats actual BufferedReader close", "cj", "a",
                                "(Ljava/io/File;)Ljava/util/Map;", 64);
                error->system_error = stream->system_error;
                stats_source_child(error, stream->file_error);
                status = stats_source_caught(writer, report, error, 74);
            }
        }
        if (status == MC_CLIENT_STATS_OK) {
            *out = owned->result;
            owned->result = NULL;
        } else {
            stats_source_frame(error, "cj", "a",
                               "(Ljava/io/File;Ljava/io/File;Ljava/io/File;)Ljava/util/Map;",
                               read_pc[selected_order]);
        }
        /* Native retention defers all extra cleanup to joined retirement. A
         * missing caught diagnostic never grants a later source finally/close. */
        return status;
    }
    if (status == MC_CLIENT_STATS_SOURCE_EXCEPTION)
        status = stats_catch(writer, report, error);
    mc_client_stats_error close_error;
    stats_error_clear(&close_error);
    const mc_java_reader_status closed =
        *reader != NULL ? mc_java_reader_close(*reader) : close_stream(stream);
    if (closed != MC_JAVA_READER_OK) {
        stats_exception(&close_error, MC_CLIENT_STATS_SOURCE_IO, "Stats BufferedReader close", "cj",
                        "a", "(Ljava/io/File;)Ljava/util/Map;", 64);
        close_error.system_error = stream->system_error;
        const mc_client_stats_status close_status = stats_catch(writer, report, &close_error);
        if (status == MC_CLIENT_STATS_OK && close_status != MC_CLIENT_STATS_OK) {
            status = close_status;
            *error = close_error;
        }
    }
    stats_builder_clear(text);
    stats_arena_clear(arena);
    if (status == MC_CLIENT_STATS_OK)
        *out = map;
    else
        mc_client_stats_map_retire(&map);
    return status;
}

mc_client_stats_status stats_write_file_set(mc_client_stats_writer *writer,
                                            const mc_client_stats_map *map, size_t dat_index,
                                            mc_client_stats_report *report,
                                            mc_client_stats_error *error) {
    if (writer->memory->source != NULL)
        return stats_source_write_file_set(writer, map, dat_index, report, error);
    stats_syncher *syncher = writer->retained_syncher;
    const stats_file *dat = syncher->files[dat_index], *old = syncher->files[dat_index + 2u],
                     *temporary = syncher->files[dat_index + 4u];
    const mc_java_writer_profile profile = {writer->options.charset, MC_JAVA_WRITER_PRINT,
                                            MC_JAVA_WRITER_CRLF};
    mc_java_writer *output = NULL;
    mc_java_writer_error cause;
    mc_java_writer_status ws = mc_java_file_writer_open(temporary->path, &profile, &output, &cause);
    if (ws != MC_JAVA_WRITER_OK) {
        error->writer = ws;
        error->system_error = cause.system_error;
        if (ws == MC_JAVA_WRITER_IO_ERROR)
            return stats_exception(
                error, MC_CLIENT_STATS_SOURCE_IO, "Stats FileWriter open", "cj", "a",
                "(Ljava/util/Map;Ljava/io/File;Ljava/io/File;Ljava/io/File;)V", 10);
        return stats_fail(error,
                          ws == MC_JAVA_WRITER_LIMIT ? MC_CLIENT_STATS_LIMIT
                          : ws == MC_JAVA_WRITER_ALLOCATION_FAILED
                              ? MC_CLIENT_STATS_ALLOCATION_FAILED
                              : MC_CLIENT_STATS_UNSUPPORTED_PROFILE,
                          "Stats native FileWriter", "cj", "a",
                          "(Ljava/util/Map;Ljava/io/File;Ljava/io/File;Ljava/io/File;)V", 10);
    }
    mc_client_stats_text *text = NULL;
    mc_player_string_view username;
    const bool exists = mc_player_string_read(syncher->session->username, &username);
    static const uint16_t local[] = {'l', 'o', 'c', 'a', 'l'};
    mc_client_stats_status status;
    if (map == NULL)
        status = stats_exception(
            error, MC_CLIENT_STATS_NULL_REFERENCE, "Stats serializer null Map", "xi", "a",
            "(Ljava/lang/String;Ljava/lang/String;Ljava/util/Map;)Ljava/lang/String;", 89);
    else
        status = mc_client_stats_serialize(map,
                                           (mc_client_stats_string){exists ? username.units : NULL,
                                                                    exists ? username.length : 0,
                                                                    !exists},
                                           (mc_client_stats_string){local, 5, false}, &text, error);
    if (status == MC_CLIENT_STATS_OK) {
        const mc_java_properties_string_view view = {text->units, text->length};
        ws = mc_java_writer_print(output, &view, &cause);
        if (ws != MC_JAVA_WRITER_OK) {
            error->writer = ws;
            error->system_error = cause.system_error;
            status = stats_fail(error,
                                ws == MC_JAVA_WRITER_LIMIT ? MC_CLIENT_STATS_LIMIT
                                                           : MC_CLIENT_STATS_IO_FAILED,
                                "Stats native PrintWriter print", "cj", "a",
                                "(Ljava/util/Map;Ljava/io/File;Ljava/io/File;Ljava/io/File;)V", 33);
        }
    }
    mc_client_stats_text_retire(&text);
    ws = mc_java_writer_close(output, &cause);
    mc_java_writer_state state;
    if (mc_java_writer_read_state(output, &state) && state.trouble)
        report->print_trouble = true;
    if (status == MC_CLIENT_STATS_OK && ws != MC_JAVA_WRITER_OK) {
        error->writer = ws;
        error->system_error = cause.system_error;
        status =
            stats_fail(error, MC_CLIENT_STATS_IO_FAILED, "Stats native PrintWriter close", "cj",
                       "a", "(Ljava/util/Map;Ljava/io/File;Ljava/io/File;Ljava/io/File;)V", 38);
    }
    ws = mc_java_writer_retire(&output, NULL, &cause);
    if (status == MC_CLIENT_STATS_OK && ws != MC_JAVA_WRITER_OK) {
        error->writer = ws;
        error->system_error = cause.system_error;
        status = stats_fail(error, MC_CLIENT_STATS_IO_FAILED, "Stats native Writer retirement",
                            NULL, NULL, NULL, 0);
    }
    if (status != MC_CLIENT_STATS_OK)
        return status;
    const bool old_exists = stats_file_exists(old, error);
    if (error->status != MC_CLIENT_STATS_OK)
        return error->status;
    if (old_exists)
        delete_path(old->path, error);
    if (error->status != MC_CLIENT_STATS_OK)
        return error->status;
    const bool dat_exists = stats_file_exists(dat, error);
    if (error->status != MC_CLIENT_STATS_OK)
        return error->status;
    if (dat_exists)
        rename_path(dat->path, old->path, error);
    if (error->status != MC_CLIENT_STATS_OK)
        return error->status;
    rename_path(temporary->path, dat->path, error);
    return error->status;
}
