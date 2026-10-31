#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#include "mc/client/settings/game_settings.h"

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if !defined(_WIN32) && !defined(__linux__)
#error "This observation profile is defined for native Windows and Linux"
#endif
_Static_assert(CHAR_BIT == 8 && sizeof(uint16_t) == 2 && sizeof(float) == sizeof(uint32_t),
               "Native observation requires eight-bit bytes, UTF16 and binary32 storage");
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <fcntl.h>
#include <io.h>
#include <wchar.h>
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#define PLAN_LIMIT 1048576u
#define INPUT_LIMIT 65536u
#define FILE_LIMIT 262144u
#define RESULT_LIMIT 8388608u
#define PATH_LIMIT 4095u
#define ALIAS_LIMIT 512u
#define ARRAY_LIMIT 32u
#define DIAGNOSTIC_LIMIT 128u
#define DIAGNOSTIC_BYTES 8388608u

typedef enum input_kind {
    INPUT_ABSENT,
    INPUT_DIRECTORY,
    INPUT_RAW,
    INPUT_DEFAULT_TEXT,
    INPUT_KEEP
} input_kind;

typedef struct input_plan {
    input_kind kind;
    uint16_t text[INPUT_LIMIT];
    size_t units;
    uint8_t bytes[INPUT_LIMIT];
    size_t count;
} input_plan;

typedef struct file_view {
    enum { OBS_FILE_ABSENT, OBS_FILE_DIRECTORY, OBS_FILE_REGULAR } kind;

    uint8_t bytes[FILE_LIMIT + 1u];
    size_t count;
} file_view;

typedef struct diagnostic_view {
    mc_game_settings_diagnostic event;
    uint16_t *line;
    char message[513];
} diagnostic_view;

typedef struct binding_view {
    mc_key_binding *owner;
    mc_game_settings_text name;
    int32_t code;
} binding_view;

typedef struct state_view {
    bool present;
    mc_game_settings_state state;
    binding_view bindings[10u + ARRAY_LIMIT];
    size_t count;
} state_view;

typedef struct probe {
    uint8_t raw[PLAN_LIMIT + 1u];
    char parse[PLAN_LIMIT + 1u];
    size_t raw_length;
    char id[49];
    bool noarg;
    mc_game_settings_profile profile;
    const char *charset;
    input_plan inputs[2];
    char scratch[PATH_LIMIT + 1u], first[PATH_LIMIT + 1u], fresh[PATH_LIMIT + 1u];
    char options[PATH_LIMIT + 1u], fresh_options[PATH_LIMIT + 1u];
    FILE *output;
    size_t written, rows;
    bool stdout_mode, output_failed;
    const char *failure;
    int system_error;
    uint32_t win32_error;
    const void *aliases[ALIAS_LIMIT];
    size_t alias_count;
    diagnostic_view diagnostics[DIAGNOSTIC_LIMIT];
    size_t diagnostic_count, diagnostic_bytes;
    mc_game_settings *owners[2];
    mc_java_writer *preparation_writer;
    size_t stages, inputs_done, retirements;
    file_view before, after;
    state_view snapshot;
} probe;

static bool require(probe *p, bool condition, const char *reason) {
    if (!condition && p->failure == NULL) {
        p->failure = reason;
        p->system_error = errno;
    }
    return condition;
}

static bool put(probe *p, const void *bytes, size_t count) {
    if (p->output_failed)
        return false;
    if (!require(p, count <= RESULT_LIMIT - p->written, "complete JSON output bound")) {
        p->output_failed = true;
        return false;
    }
    errno = 0;
    if (fwrite(bytes, 1, count, p->output) != count) {
        require(p, false, "JSON fwrite");
        p->output_failed = true;
        return false;
    }
    p->written += count;
    return true;
}

static bool text(probe *p, const char *ascii) {
    return put(p, ascii, strlen(ascii));
}

static bool format(probe *p, const char *pattern, ...) {
    char buffer[1024];
    va_list arguments;
    va_start(arguments, pattern);
    const int count = vsnprintf(buffer, sizeof(buffer), pattern, arguments);
    va_end(arguments);
    if (!require(p, count >= 0 && (size_t)count < sizeof(buffer), "bounded JSON scalar format")) {
        p->output_failed = true;
        return false;
    }
    return put(p, buffer, (size_t)count);
}

static bool quoted(probe *p, const char *ascii) {
    if (ascii == NULL)
        return text(p, "null");
    if (!require(p, strlen(ascii) <= 512u, "bounded ASCII diagnostic")) {
        p->output_failed = true;
        return false;
    }
    if (!text(p, "\""))
        return false;
    for (size_t i = 0; ascii[i] != 0; ++i) {
        const unsigned char value = (unsigned char)ascii[i];
        if (value == '"' || value == '\\') {
            const char escaped[] = {'\\', (char)value};
            if (!put(p, escaped, sizeof(escaped)))
                return false;
        } else if (value < 32u || value > 126u) {
            if (!format(p, "\\u%04x", (unsigned int)value))
                return false;
        } else if (!put(p, &value, 1u)) {
            return false;
        }
    }
    return text(p, "\"");
}

static bool hex(probe *p, const uint8_t *bytes, size_t count) {
    static const char digits[] = "0123456789abcdef";
    if (!text(p, "\""))
        return false;
    char buffer[1024];
    size_t position = 0;
    while (position < count) {
        size_t chunk = count - position;
        if (chunk > sizeof(buffer) / 2u)
            chunk = sizeof(buffer) / 2u;
        for (size_t i = 0; i < chunk; ++i) {
            buffer[2u * i] = digits[bytes[position + i] >> 4u];
            buffer[2u * i + 1u] = digits[bytes[position + i] & 15u];
        }
        if (!put(p, buffer, 2u * chunk))
            return false;
        position += chunk;
    }
    return text(p, "\"");
}

static bool units(probe *p, const uint16_t *value, size_t count) {
    if (!text(p, "\""))
        return false;
    for (size_t i = 0; i < count; ++i)
        if (!format(p, "%04x", (unsigned int)value[i]))
            return false;
    return text(p, "\"");
}

static bool end_row(probe *p) {
    if (!text(p, "}\n"))
        return false;
    errno = 0;
    if (!require(p, fflush(p->output) == 0, "JSON fflush")) {
        p->output_failed = true;
        return false;
    }
    ++p->rows;
    return true;
}

static const char *boolean(bool value) {
    return value ? "true" : "false";
}

#ifdef _WIN32
static bool wide_path(probe *p, const char *path, wchar_t *native, size_t count) {
    if (!require(p, strlen(path) <= PATH_LIMIT, "native UTF8 filename bound"))
        return false;
    const int result =
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, native, (int)count);
    if (result == 0)
        p->win32_error = GetLastError();
    return require(p, result > 0, "native UTF8 filename conversion");
}
#endif
static FILE *open_file(probe *p, const char *path, const char *mode) {
    FILE *file = NULL;
#ifdef _WIN32
    wchar_t native[PATH_LIMIT + 1u], native_mode[8];
    if (!wide_path(p, path, native, sizeof(native) / sizeof(native[0])))
        return NULL;
    size_t i = 0;
    while (mode[i] != 0 && i + 1u < sizeof(native_mode) / sizeof(native_mode[0])) {
        native_mode[i] = (uint8_t)mode[i];
        ++i;
    }
    native_mode[i] = 0;
    errno = 0;
    if (_wfopen_s(&file, native, native_mode) != 0)
        file = NULL;
#else
    (void)p;
    errno = 0;
    file = fopen(path, mode);
#endif
    return file;
}

static bool join(probe *p, char *out, const char *parent, const char *child) {
    const int count = snprintf(out, PATH_LIMIT + 1u, "%s/%s", parent, child);
    return require(p, count >= 0 && (size_t)count <= PATH_LIMIT, "owned child filename bound");
}

static bool file(probe *p, const char *path, file_view *out) {
    out->count = 0;
#ifdef _WIN32
    wchar_t native[PATH_LIMIT + 1u];
    if (!wide_path(p, path, native, sizeof(native) / sizeof(native[0])))
        return false;
    const DWORD attributes = GetFileAttributesW(native);
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        const DWORD code = GetLastError();
        if (code == ERROR_FILE_NOT_FOUND || code == ERROR_PATH_NOT_FOUND) {
            out->kind = OBS_FILE_ABSENT;
            return true;
        }
        p->win32_error = code;
        return require(p, false, "file attributes unavailable");
    }
    if (!require(p, (attributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0,
                 "no reparse observation path"))
        return false;
    if ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        out->kind = OBS_FILE_DIRECTORY;
        return true;
    }
#else
    struct stat information;
    errno = 0;
    if (lstat(path, &information) != 0) {
        if (errno == ENOENT || errno == ENOTDIR) {
            out->kind = OBS_FILE_ABSENT;
            return true;
        }
        return require(p, false, "file lstat");
    }
    if (S_ISDIR(information.st_mode)) {
        out->kind = OBS_FILE_DIRECTORY;
        return true;
    }
    if (!require(p, S_ISREG(information.st_mode), "ordinary nonlink observation file"))
        return false;
#endif
    FILE *stream = open_file(p, path, "rb");
    if (!require(p, stream != NULL, "observation file open"))
        return false;
    bool okay = true;
    if (!require(p, setvbuf(stream, NULL, _IONBF, 0) == 0, "observation unbuffered file"))
        okay = false;
    if (okay) {
        out->count = fread(out->bytes, 1, sizeof(out->bytes), stream);
        okay = require(p,
                       out->count <= FILE_LIMIT && !ferror(stream) && fgetc(stream) == EOF &&
                           !ferror(stream),
                       "complete bounded observation file");
    }
    errno = 0;
    if (!require(p, fclose(stream) == 0, "observation file close"))
        okay = false;
    out->kind = OBS_FILE_REGULAR;
    return okay;
}

static bool file_json(probe *p, const file_view *view) {
    if (view->kind == OBS_FILE_ABSENT)
        return text(p, "{\"kind\":\"absent\"}");
    if (view->kind == OBS_FILE_DIRECTORY)
        return text(p, "{\"kind\":\"directory\"}");
    return format(p, "{\"kind\":\"file\",\"bytes\":%zu,\"hex\":", view->count) &&
           hex(p, view->bytes, view->count) && text(p, "}");
}

static bool make_directory(probe *p, const char *path) {
#ifdef _WIN32
    wchar_t native[PATH_LIMIT + 1u];
    if (!wide_path(p, path, native, sizeof(native) / sizeof(native[0])))
        return false;
    const BOOL created = CreateDirectoryW(native, NULL);
    if (!created)
        p->win32_error = GetLastError();
    return require(p, created != 0, "fresh owned directory creation");
#else
    errno = 0;
    return require(p, mkdir(path, 0700) == 0, "fresh owned directory creation");
#endif
}

static bool empty_scratch(probe *p, const char *path) {
    const size_t length = strlen(path);
    if (!require(p, length != 0 && length <= PATH_LIMIT, "scratch path bound"))
        return false;
#ifdef _WIN32
    if (!require(p, length >= 3u && path[1] == ':' && (path[2] == '/' || path[2] == '\\'),
                 "absolute local drive scratch"))
        return false;
    wchar_t native[PATH_LIMIT + 1u];
    if (!wide_path(p, path, native, sizeof(native) / sizeof(native[0])))
        return false;
    const DWORD attributes = GetFileAttributesW(native);
    if (attributes == INVALID_FILE_ATTRIBUTES)
        p->win32_error = GetLastError();
    if (!require(p,
                 attributes != INVALID_FILE_ATTRIBUTES &&
                     (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0 &&
                     (attributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0,
                 "ordinary scratch root"))
        return false;
    char pattern[PATH_LIMIT + 1u];
    if (!join(p, pattern, path, "*") ||
        !wide_path(p, pattern, native, sizeof(native) / sizeof(native[0])))
        return false;
    WIN32_FIND_DATAW entry;
    HANDLE search = FindFirstFileW(native, &entry);
    if (search == INVALID_HANDLE_VALUE) {
        const DWORD code = GetLastError();
        p->win32_error = code;
        return require(p, code == ERROR_FILE_NOT_FOUND, "scratch inventory open");
    }
    bool empty = true, complete = false;
    for (;;) {
        if (wcscmp(entry.cFileName, L".") != 0 && wcscmp(entry.cFileName, L"..") != 0) {
            empty = false;
            break;
        }
        if (!FindNextFileW(search, &entry)) {
            const DWORD code = GetLastError();
            complete = code == ERROR_NO_MORE_FILES;
            if (!complete)
                p->win32_error = code;
            break;
        }
    }
    const bool closed = FindClose(search) != 0;
    if (!closed)
        p->win32_error = GetLastError();
    return require(p, empty && complete && closed, "complete empty scratch inventory");
#else
    struct stat information;
    if (!require(p,
                 path[0] == '/' && lstat(path, &information) == 0 && S_ISDIR(information.st_mode),
                 "ordinary absolute scratch root"))
        return false;
    DIR *directory = opendir(path);
    if (!require(p, directory != NULL, "scratch inventory open"))
        return false;
    bool empty = true;
    errno = 0;
    struct dirent *entry;
    while ((entry = readdir(directory)) != NULL)
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            empty = false;
            break;
        }
    const int saved = errno;
    const bool closed = closedir(directory) == 0;
    errno = saved;
    return require(p, empty && saved == 0 && closed, "complete empty scratch inventory");
#endif
}

static int nibble(char value) {
    if (value >= '0' && value <= '9')
        return value - '0';
    if (value >= 'a' && value <= 'f')
        return value - 'a' + 10;
    return -1;
}

static bool decode_bytes(probe *p, const char *token, uint8_t *out, size_t *count) {
    if (strcmp(token, "-") == 0) {
        *count = 0;
        return true;
    }
    const size_t length = strlen(token);
    if (!require(p, length != 0 && length % 2u == 0 && length / 2u <= INPUT_LIMIT,
                 "bounded lowercase byte hex"))
        return false;
    for (size_t i = 0; i < length / 2u; ++i) {
        const int high = nibble(token[2u * i]), low = nibble(token[2u * i + 1u]);
        if (!require(p, high >= 0 && low >= 0, "lowercase byte hex"))
            return false;
        out[i] = (uint8_t)((unsigned int)high * 16u + (unsigned int)low);
    }
    *count = length / 2u;
    return true;
}

static bool decode_units(probe *p, const char *token, input_plan *input) {
    if (strcmp(token, "-") == 0) {
        input->units = 0;
        return true;
    }
    const size_t length = strlen(token);
    if (!require(p, length != 0 && length % 4u == 0 && length / 4u <= INPUT_LIMIT,
                 "bounded UTF16 hex"))
        return false;
    for (size_t i = 0; i < length / 4u; ++i) {
        unsigned int value = 0;
        for (size_t j = 0; j < 4u; ++j) {
            const int part = nibble(token[4u * i + j]);
            if (!require(p, part >= 0, "lowercase UTF16 hex"))
                return false;
            value = value * 16u + (unsigned int)part;
        }
        input->text[i] = (uint16_t)value;
    }
    input->units = length / 4u;
    /* The original helper admits authored valid UTF8 text, hence no lone
     * UTF16 surrogate. This is transport validation, not a content decoder. */
    for (size_t i = 0; i < input->units; ++i) {
        const uint16_t value = input->text[i];
        if (value >= 0xd800u && value <= 0xdbffu) {
            if (!require(p,
                         i + 1u < input->units && input->text[i + 1u] >= 0xdc00u &&
                             input->text[i + 1u] <= 0xdfffu,
                         "paired authored UTF16"))
                return false;
            ++i;
        } else if (!require(p, value < 0xdc00u || value > 0xdfffu, "no lone authored surrogate")) {
            return false;
        }
    }
    return true;
}

static size_t tokens(char *line, char **fields, size_t capacity) {
    size_t count = 0;
    if (capacity == 0)
        return 0;
    fields[count++] = line;
    for (char *next = line; *next != 0; ++next) {
        if (*next != '\t')
            continue;
        *next = 0;
        if (count == capacity)
            return capacity + 1u;
        fields[count++] = next + 1;
    }
    return count;
}

static bool parse_input(probe *p, char **fields, size_t count, input_plan *input, bool reload) {
    if (!require(p, count == 4u, "four input control fields"))
        return false;
    if (strcmp(fields[1], "absent") == 0)
        input->kind = INPUT_ABSENT;
    else if (strcmp(fields[1], "directory") == 0)
        input->kind = INPUT_DIRECTORY;
    else if (strcmp(fields[1], "raw") == 0)
        input->kind = INPUT_RAW;
    else if (strcmp(fields[1], "default") == 0)
        input->kind = INPUT_DEFAULT_TEXT;
    else if (reload && strcmp(fields[1], "keep") == 0)
        input->kind = INPUT_KEEP;
    else
        return require(p, false, "declared input control kind");
    if (input->kind == INPUT_DEFAULT_TEXT)
        return decode_units(p, fields[2], input) &&
               decode_bytes(p, fields[3], input->bytes, &input->count);
    if (!require(p, strcmp(fields[2], "-") == 0, "no undeclared UTF16 payload"))
        return false;
    if (input->kind == INPUT_RAW)
        return decode_bytes(p, fields[3], input->bytes, &input->count);
    return require(p, strcmp(fields[3], "-") == 0, "no payload for absent/directory/keep");
}

static bool parse_plan(probe *p, const char *path) {
#ifdef _WIN32
    wchar_t native[PATH_LIMIT + 1u];
    if (!wide_path(p, path, native, sizeof(native) / sizeof(native[0])))
        return false;
    const DWORD attributes = GetFileAttributesW(native);
    if (attributes == INVALID_FILE_ATTRIBUTES)
        p->win32_error = GetLastError();
    if (!require(p,
                 attributes != INVALID_FILE_ATTRIBUTES &&
                     (attributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) == 0,
                 "ordinary nonlink plan file"))
        return false;
#else
    struct stat information;
    if (!require(p, lstat(path, &information) == 0 && S_ISREG(information.st_mode),
                 "ordinary nonlink plan file"))
        return false;
#endif
    FILE *stream = open_file(p, path, "rb");
    if (!require(p, stream != NULL, "plan input open"))
        return false;
    p->raw_length = fread(p->raw, 1, sizeof(p->raw), stream);
    bool okay = require(p,
                        p->raw_length != 0 && p->raw_length <= PLAN_LIMIT && !ferror(stream) &&
                            fgetc(stream) == EOF && !ferror(stream),
                        "complete bounded plan read");
    if (!require(p, fclose(stream) == 0, "plan input close"))
        okay = false;
    if (!okay)
        return false;
    for (size_t i = 0; i < p->raw_length; ++i)
        if (!require(p,
                     p->raw[i] == '\n' || p->raw[i] == '\t' ||
                         (p->raw[i] >= 32u && p->raw[i] <= 126u),
                     "ASCII LF/tab plan bytes"))
            return false;
    if (!require(p, p->raw[p->raw_length - 1u] == '\n', "final plan LF"))
        return false;
    memcpy(p->parse, p->raw, p->raw_length);
    p->parse[p->raw_length] = 0;
    char *next = p->parse;
    for (size_t line = 0; line < 7u; ++line) {
        char *end = strchr(next, '\n');
        if (!require(p, end != NULL, "seven complete plan lines"))
            return false;
        *end = 0;
        char *fields[4];
        const size_t count = tokens(next, fields, 4u);
        if (line == 0u) {
            if (!require(p, count == 1u && strcmp(fields[0], "MC_GAME_SETTINGS_INPUT_V1") == 0,
                         "plan schema"))
                return false;
        } else if (line == 1u) {
            const size_t length = count == 2u ? strlen(fields[1]) : 0;
            if (!require(p,
                         count == 2u && strcmp(fields[0], "case") == 0 && length != 0 &&
                             length < sizeof(p->id) && fields[1][0] >= 'a' && fields[1][0] <= 'z',
                         "case id"))
                return false;
            for (size_t i = 0; i < length; ++i)
                if (!require(p,
                             (fields[1][i] >= 'a' && fields[1][i] <= 'z') ||
                                 (fields[1][i] >= '0' && fields[1][i] <= '9') ||
                                 fields[1][i] == '-',
                             "case id character"))
                    return false;
            memcpy(p->id, fields[1], length + 1u);
        } else if (line == 2u) {
            if (!require(p,
                         count == 2u && strcmp(fields[0], "constructor") == 0 &&
                             (strcmp(fields[1], "file") == 0 || strcmp(fields[1], "noarg") == 0),
                         "constructor field"))
                return false;
            p->noarg = strcmp(fields[1], "noarg") == 0;
        } else if (line == 3u) {
            if (!require(p, count == 2u && strcmp(fields[0], "charset") == 0, "charset field"))
                return false;
            if (strcmp(fields[1], "UTF-8") == 0)
                p->profile.charset = MC_JAVA_READER_UTF8;
            else if (strcmp(fields[1], "windows-1252") == 0)
                p->profile.charset = MC_JAVA_READER_WINDOWS1252;
            else if (strcmp(fields[1], "ISO-8859-1") == 0)
                p->profile.charset = MC_JAVA_READER_ISO8859_1;
            else if (strcmp(fields[1], "US-ASCII") == 0)
                p->profile.charset = MC_JAVA_READER_US_ASCII;
            else
                return require(p, false, "explicit supported charset");
            p->charset = p->profile.charset == MC_JAVA_READER_UTF8          ? "UTF-8"
                         : p->profile.charset == MC_JAVA_READER_WINDOWS1252 ? "windows-1252"
                         : p->profile.charset == MC_JAVA_READER_ISO8859_1   ? "ISO-8859-1"
                                                                            : "US-ASCII";
        } else if (line == 4u) {
            if (!require(p,
                         count == 2u && strcmp(fields[0], "lines") == 0 &&
                             (strcmp(fields[1], "LF") == 0 || strcmp(fields[1], "CRLF") == 0),
                         "line separator field"))
                return false;
            p->profile.lines =
                strcmp(fields[1], "LF") == 0 ? MC_JAVA_WRITER_LF : MC_JAVA_WRITER_CRLF;
        } else {
            if (!require(p, strcmp(fields[0], line == 5u ? "initial" : "reload") == 0,
                         "input control order") ||
                !parse_input(p, fields, count, &p->inputs[line - 5u], line == 6u))
                return false;
        }
        next = end + 1;
    }
    return require(p,
                   *next == 0 && (!p->noarg || (p->inputs[0].kind == INPUT_ABSENT &&
                                                p->inputs[1].kind == INPUT_KEEP)),
                   "exact plan end/noarg source fixture");
}

static bool alias(probe *p, const void *pointer) {
    if (pointer == NULL)
        return text(p, "null");
    for (size_t i = 0; i < p->alias_count; ++i)
        if (p->aliases[i] == pointer)
            return format(p, "%zu", i);
    if (!require(p, p->alias_count < ALIAS_LIMIT, "complete native alias table")) {
        p->output_failed = true;
        return false;
    }
    p->aliases[p->alias_count] = pointer;
    return format(p, "%zu", p->alias_count++);
}

static bool string_json(probe *p, mc_game_settings_text value) {
    if (value.is_null)
        return text(p, "null");
    return text(p, "{\"kind\":\"String\",\"native_storage_alias\":") && alias(p, value.units) &&
           format(p, ",\"length\":%zu,\"utf16_hex\":", value.length) &&
           units(p, value.units, value.length) && text(p, "}");
}

static bool valid_string(probe *p, mc_game_settings_text value) {
    return require(p,
                   value.length <= INPUT_LIMIT && (value.length == 0 || value.units != NULL) &&
                       (!value.is_null || (value.units == NULL && value.length == 0)),
                   "complete observed String view");
}

static bool capture_binding(probe *p, mc_key_binding *binding) {
    if (binding == NULL)
        return true;
    for (size_t i = 0; i < p->snapshot.count; ++i)
        if (p->snapshot.bindings[i].owner == binding)
            return true;
    if (!require(p, p->snapshot.count < 10u + ARRAY_LIMIT, "complete observed binding inventory"))
        return false;
    binding_view *view = &p->snapshot.bindings[p->snapshot.count];
    if (!require(p, mc_key_binding_read(binding, &view->name, &view->code),
                 "public KeyBinding read") ||
        !valid_string(p, view->name))
        return false;
    view->owner = binding;
    ++p->snapshot.count;
    return true;
}

static bool capture(probe *p, mc_game_settings *owner) {
    memset(&p->snapshot, 0, sizeof(p->snapshot));
    if (owner == NULL)
        return true;
    p->snapshot.present = true;
    if (!require(p, mc_game_settings_read_state(owner, &p->snapshot.state),
                 "public GameSettings read"))
        return false;
    const mc_game_settings_state *state = &p->snapshot.state;
    if (!valid_string(p, state->values.texture_pack) ||
        !valid_string(p, state->values.last_server) ||
        !require(p,
                 state->binding_count <= ARRAY_LIMIT &&
                     (state->bindings_null || state->binding_count == 0 || state->bindings != NULL),
                 "complete observed current array"))
        return false;
    if (state->options_path != NULL &&
        !require(p, strlen(state->options_path) <= PATH_LIMIT, "observed native File path bound"))
        return false;
    for (size_t i = 0; i < 10u; ++i)
        if (!capture_binding(p, state->named[i]))
            return false;
    if (!state->bindings_null)
        for (size_t i = 0; i < state->binding_count; ++i)
            if (!capture_binding(p, state->bindings[i]))
                return false;
    return true;
}

static bool binding_json(probe *p, mc_key_binding *binding) {
    if (binding == NULL)
        return text(p, "null");
    for (size_t i = 0; i < p->snapshot.count; ++i) {
        const binding_view *view = &p->snapshot.bindings[i];
        if (view->owner != binding)
            continue;
        return text(p, "{\"kind\":\"KeyBinding\",\"native_owner_alias\":") && alias(p, binding) &&
               text(p, ",\"a\":") && string_json(p, view->name) &&
               format(p, ",\"b\":%" PRId32 "}", view->code);
    }
    return require(p, false, "captured binding lookup");
}

static bool field(probe *p, const char *role, const char *kind, bool first) {
    return (first || text(p, ",")) &&
           format(p, "{\"role\":\"%s\",\"kind\":\"%s\",\"value\":", role, kind);
}

static bool float_json(probe *p, float value) {
    uint32_t bits = 0;
    memcpy(&bits, &value, sizeof(bits));
    return format(p, "{\"float_raw_bits\":\"%08" PRIx32 "\"}", bits);
}

static bool state_json(probe *p, mc_game_settings *owner) {
    if (!p->snapshot.present)
        return text(p, "null");
    const mc_game_settings_state *state = &p->snapshot.state;
    const mc_game_settings_values *values = &state->values;
    if (!text(p, "{\"native_owner_alias\":") || !alias(p, owner) ||
        !text(p, ",\"instance_fields\":["))
        return false;
    size_t fields = 0;
#define FLOAT(role, member)                                                                        \
    do {                                                                                           \
        if (!field(p, role, "float32", fields++ == 0) || !float_json(p, values->member) ||         \
            !text(p, "}"))                                                                         \
            return false;                                                                          \
    } while (0)
#define BOOL(role, member)                                                                         \
    do {                                                                                           \
        if (!field(p, role, "boolean", fields++ == 0) || !text(p, boolean(values->member)) ||      \
            !text(p, "}"))                                                                         \
            return false;                                                                          \
    } while (0)
#define INT(role, member)                                                                          \
    do {                                                                                           \
        if (!field(p, role, "int32", fields++ == 0) || !format(p, "%" PRId32 "}", values->member)) \
            return false;                                                                          \
    } while (0)
#define STRING(role, member)                                                                       \
    do {                                                                                           \
        if (!field(p, role, "String", fields++ == 0) || !string_json(p, values->member) ||         \
            !text(p, "}"))                                                                         \
            return false;                                                                          \
    } while (0)
    FLOAT("a", music_volume);
    FLOAT("b", sound_volume);
    FLOAT("c", mouse_sensitivity);
    BOOL("d", invert_mouse);
    INT("e", render_distance);
    BOOL("f", view_bobbing);
    BOOL("g", anaglyph);
    BOOL("h", advanced_opengl);
    INT("i", performance_mode);
    BOOL("j", fancy_graphics);
    BOOL("k", ambient_occlusion);
    STRING("l", texture_pack);
    for (size_t i = 0; i < 10u; ++i) {
        char role[] = {(char)('m' + (int)i), 0};
        if (!field(p, role, "KeyBinding", fields++ == 0) || !binding_json(p, state->named[i]) ||
            !text(p, "}"))
            return false;
    }
    if (!field(p, "w", "KeyBinding[]", fields++ == 0))
        return false;
    if (state->bindings_null) {
        if (!text(p, "null"))
            return false;
    } else {
        if (!text(p, "{\"native_storage_alias\":") || !alias(p, state->bindings) ||
            !text(p, ",\"entries\":["))
            return false;
        for (size_t i = 0; i < state->binding_count; ++i)
            if ((i != 0 && !text(p, ",")) || !binding_json(p, state->bindings[i]))
                return false;
        if (!text(p, "]}"))
            return false;
    }
    if (!text(p, "}") || !field(p, "x", "borrowed Minecraft", fields++ == 0))
        return false;
    if (state->minecraft == NULL) {
        if (!text(p, "null"))
            return false;
    } else if (!text(p, "{\"native_opaque_alias\":") || !alias(p, state->minecraft) ||
               !text(p, "}"))
        return false;
    if (!text(p, "}") || !field(p, "N", "File", fields++ == 0))
        return false;
    if (state->file_identity == NULL && state->options_path == NULL) {
        if (!text(p, "null"))
            return false;
    } else {
        if (!text(p, "{\"native_owner_alias\":") || !alias(p, state->file_identity) ||
            !text(p, ",\"path_utf8_hex\":"))
            return false;
        if (state->options_path == NULL) {
            if (!text(p, "null"))
                return false;
        } else if (!hex(p, (const uint8_t *)state->options_path, strlen(state->options_path)))
            return false;
        if (!text(p, "}"))
            return false;
    }
    if (!text(p, "}"))
        return false;
    INT("y", difficulty);
    BOOL("z", hide_gui);
    BOOL("A", third_person_view);
    BOOL("B", show_debug_info);
    STRING("C", last_server);
    BOOL("D", source_D);
    BOOL("E", smooth_camera);
    BOOL("F", debug_camera);
    FLOAT("G", source_G);
    FLOAT("H", source_H);
    INT("I", gui_scale);
#undef STRING
#undef INT
#undef BOOL
#undef FLOAT
    return require(p, fields == 36u, "complete public instance-role inventory") &&
           format(p,
                  "],\"instance_field_count\":%zu,\"native_binding_count\":%zu,\"native_bindings_"
                  "null\":%s,\"native_bindings_storage_present\":%s}",
                  fields, state->binding_count, boolean(state->bindings_null),
                  boolean(state->bindings != NULL));
}

static bool writer_error(probe *p, mc_java_writer_error error) {
    return format(p, "{\"status\":%d,\"system_error\":%d,\"operation\":", (int)error.status,
                  error.system_error) &&
           quoted(p, error.operation) &&
           format(p, ",\"native_cleanup_failed\":%s,\"native_cleanup_system_error\":%d}",
                  boolean(error.native_cleanup_failed), error.native_cleanup_system_error);
}

static bool writer_state(probe *p, mc_java_writer_state state) {
    return format(p,
                  "{\"charset\":%d,\"encoder_closed\":%s,\"print_writer_out_present\":%s,"
                  "\"trouble\":%s,\"interrupted_write\":%s,\"source_close_reached\":%s,\"sink_"
                  "close_attempted\":%s,\"native_file\":%s,\"first_io_error\":",
                  (int)state.charset, boolean(state.encoder_closed),
                  boolean(state.print_writer_out_present), boolean(state.trouble),
                  boolean(state.interrupted_write), boolean(state.source_close_reached),
                  boolean(state.sink_close_attempted), boolean(state.native_file)) &&
           writer_error(p, state.first_io_error) && text(p, ",\"secondary_close_error\":") &&
           writer_error(p, state.secondary_close_error) && text(p, "}");
}

static bool report_json(probe *p, mc_game_settings_report report) {
    if (!format(p,
                "{\"lines_read\":%zu,\"bad_options\":%zu,\"bytes_read\":%zu,\"println_attempts\":%"
                "zu,\"file_absent\":%s,\"source_opened\":%s,\"source_close_reached\":%s,\"outer_"
                "exception_caught\":%s,\"outer_exception\":%d,\"source_system_error\":%d,\"writer_"
                "state_present\":%s,\"writer_state\":",
                report.lines_read, report.bad_options, report.bytes_read, report.println_attempts,
                boolean(report.file_absent), boolean(report.source_opened),
                boolean(report.source_close_reached), boolean(report.outer_exception_caught),
                (int)report.outer_exception, report.source_system_error,
                boolean(report.writer_state_present)))
        return false;
    if (report.writer_state_present) {
        if (!writer_state(p, report.writer_state))
            return false;
    } else if (!text(p, "null"))
        return false;
    return format(p,
                  ",\"native_input_admission_close\":%s,\"native_additional_close\":%s,\"native_"
                  "cleanup_failed\":%s,\"native_cleanup_system_error\":%d}",
                  boolean(report.native_input_admission_close),
                  boolean(report.native_additional_close), boolean(report.native_cleanup_failed),
                  report.native_cleanup_system_error);
}

static bool settings_error(probe *p, mc_game_settings_error error) {
    return format(p, "{\"status\":%d,\"system_error\":%d,\"operation\":", (int)error.status,
                  error.system_error) &&
           quoted(p, error.operation) && text(p, "}");
}

static bool remember_operation(probe *p, const char *borrowed, char *storage, const char **out) {
    if (borrowed == NULL) {
        *out = NULL;
        return true;
    }
    size_t length = 0;
    while (length <= 512u && borrowed[length] != 0)
        ++length;
    if (!require(p, length <= 512u, "complete borrowed diagnostic text")) {
        *out = NULL;
        return false;
    }
    memcpy(storage, borrowed, length + 1u);
    *out = storage;
    return true;
}

static void clear_diagnostics(probe *p) {
    for (size_t i = 0; i < p->diagnostic_count; ++i)
        free(p->diagnostics[i].line);
    p->diagnostic_count = 0;
    p->diagnostic_bytes = 0;
}

static mc_game_settings_status diagnostic(void *context, const mc_game_settings_diagnostic *event,
                                          mc_game_settings_error *error) {
    probe *p = context;
    if (!require(p,
                 p->diagnostic_count < DIAGNOSTIC_LIMIT && event->line.length <= INPUT_LIMIT &&
                     (event->line.length == 0 || event->line.units != NULL) &&
                     event->line.length * sizeof(uint16_t) <=
                         DIAGNOSTIC_BYTES - p->diagnostic_bytes,
                 "complete diagnostic observation budget"))
        goto refused;
    diagnostic_view *view = &p->diagnostics[p->diagnostic_count];
    view->event = *event;
    view->line = NULL;
    if (!remember_operation(p, event->message, view->message, &view->event.message))
        goto refused;
    if (event->line.length != 0) {
        view->line = malloc(event->line.length * sizeof(uint16_t));
        if (!require(p, view->line != NULL, "diagnostic observation allocation"))
            goto refused;
        memcpy(view->line, event->line.units, event->line.length * sizeof(uint16_t));
        view->event.line.units = view->line;
        p->diagnostic_bytes += event->line.length * sizeof(uint16_t);
    }
    ++p->diagnostic_count;
    return MC_GAME_SETTINGS_OK;
refused:
    if (error != NULL)
        *error = (mc_game_settings_error){MC_GAME_SETTINGS_DIAGNOSTIC_FAILED, p->system_error,
                                          "native observation diagnostic capture failure"};
    return MC_GAME_SETTINGS_DIAGNOSTIC_FAILED;
}

static bool diagnostics_json(probe *p) {
    if (!text(p, "["))
        return false;
    for (size_t i = 0; i < p->diagnostic_count; ++i) {
        const mc_game_settings_diagnostic *event = &p->diagnostics[i].event;
        if ((i != 0 && !text(p, ",")) ||
            !format(p, "{\"kind\":%d,\"message\":", (int)event->kind) ||
            !quoted(p, event->message) ||
            !format(p,
                    ",\"exception\":%d,\"line_number\":%zu,\"line_is_null\":%s,\"line_length\":%zu,"
                    "\"line_utf16_hex\":",
                    (int)event->exception, event->line_number, boolean(event->line.is_null),
                    event->line.length) ||
            !units(p, event->line.units, event->line.length) ||
            !format(p, ",\"system_error\":%d}", event->system_error))
            return false;
    }
    return text(p, "]");
}

static const char *input_name(input_kind kind) {
    static const char *const names[] = {"absent", "directory", "raw", "default", "keep"};
    return names[(unsigned int)kind];
}

static bool write_raw(probe *p, const char *path, const uint8_t *bytes, size_t count,
                      const char *mode) {
    FILE *stream = open_file(p, path, mode);
    if (!require(p, stream != NULL, "owned stimulus file open"))
        return false;
    errno = 0;
    bool okay = require(p, fwrite(bytes, 1, count, stream) == count && !ferror(stream),
                        "owned stimulus file write");
    if (!require(p, fclose(stream) == 0, "owned stimulus file close"))
        okay = false;
    return okay;
}

static bool materialize(probe *p, size_t index) {
    const input_plan *input = &p->inputs[index];
    if (!file(p, p->options, &p->before))
        return false;
    mc_java_writer_status opened = MC_JAVA_WRITER_OK, wrote = MC_JAVA_WRITER_OK,
                          closed = MC_JAVA_WRITER_OK, retired_status = MC_JAVA_WRITER_OK;
    mc_java_writer_error cause = {0}, retirement_error = {0};
    mc_java_writer_state preparation_state = {0};
    char cause_operation[513], first_operation[513], second_operation[513];
    mc_java_writer_retirement retired = {0};
    bool state_present = false, prepared = true;
    bool open_attempted = false, write_attempted = false, close_attempted = false,
         retire_attempted = false;
    if (input->kind == INPUT_ABSENT || input->kind == INPUT_DIRECTORY) {
        prepared =
            require(p, p->before.kind == OBS_FILE_ABSENT, "fresh absent/directory stimulus only");
        if (prepared && input->kind == INPUT_DIRECTORY)
            prepared = make_directory(p, p->options);
    } else if (input->kind == INPUT_RAW || input->kind == INPUT_DEFAULT_TEXT) {
        prepared = require(p, p->before.kind != OBS_FILE_DIRECTORY,
                           "no replacing owned directory with payload");
        if (prepared && input->kind == INPUT_RAW) {
            prepared = write_raw(p, p->options, input->bytes, input->count, "wb");
        } else if (prepared) {
            const mc_java_writer_profile profile = {p->profile.charset,
                                                    MC_JAVA_WRITER_OUTPUT_STREAM, p->profile.lines};
            open_attempted = true;
            opened = mc_java_file_writer_open(p->options, &profile, &p->preparation_writer, &cause);
            prepared = require(p, opened == MC_JAVA_WRITER_OK, "default stimulus Writer open");
            if (prepared) {
                write_attempted = true;
                wrote =
                    mc_java_writer_write(p->preparation_writer, input->text, input->units, &cause);
                prepared = require(p, wrote == MC_JAVA_WRITER_OK, "default stimulus Writer write");
            }
            if (prepared) {
                close_attempted = true;
                closed = mc_java_writer_close(p->preparation_writer, &cause);
                prepared =
                    require(p, closed == MC_JAVA_WRITER_OK, "default stimulus Writer source close");
            }
            if (p->preparation_writer != NULL) {
                state_present =
                    mc_java_writer_read_state(p->preparation_writer, &preparation_state);
                if (!require(p, state_present, "default stimulus Writer state observation"))
                    prepared = false;
                if (!remember_operation(p, cause.operation, cause_operation, &cause.operation) ||
                    (state_present &&
                     (!remember_operation(p, preparation_state.first_io_error.operation,
                                          first_operation,
                                          &preparation_state.first_io_error.operation) ||
                      !remember_operation(p, preparation_state.secondary_close_error.operation,
                                          second_operation,
                                          &preparation_state.secondary_close_error.operation))))
                    prepared = false;
                retire_attempted = true;
                retired_status =
                    mc_java_writer_retire(&p->preparation_writer, &retired, &retirement_error);
                if (!require(p,
                             retired_status == MC_JAVA_WRITER_OK && p->preparation_writer == NULL,
                             "default stimulus Writer native retirement"))
                    prepared = false;
            }
        }
    }
    const bool observed = file(p, p->options, &p->after);
    bool same_stimulus = false;
    if (observed && (input->kind == INPUT_RAW || input->kind == INPUT_DEFAULT_TEXT)) {
        same_stimulus = p->after.kind == OBS_FILE_REGULAR && p->after.count == input->count &&
                        memcmp(p->after.bytes, input->bytes, input->count) == 0;
        if (!require(p, same_stimulus && p->after.count <= INPUT_LIMIT,
                     "actual stimulus bytes must equal independent authored input bytes before "
                     "GameSettings"))
            prepared = false;
    }
    if (!format(p,
                "{\"type\":\"input_control\",\"ordinal\":%zu,\"role\":\"%s\",\"kind\":\"%s\","
                "\"declared_stimulus_bytes\":%zu,\"declared_stimulus_hex\":",
                index, index == 0 ? "initial" : "reload", input_name(input->kind), input->count) ||
        !hex(p, input->bytes, input->count) ||
        !format(p, ",\"authored_text_units\":%zu,\"authored_text_utf16_hex\":", input->units) ||
        !units(p, input->text, input->units) || !text(p, ",\"before\":") ||
        !file_json(p, &p->before) || !text(p, ",\"after\":") ||
        !(observed ? file_json(p, &p->after) : text(p, "null")) ||
        !text(p, ",\"actual_bytes_equal_declared_stimulus\":") ||
        !text(p, input->kind == INPUT_RAW || input->kind == INPUT_DEFAULT_TEXT
                     ? boolean(same_stimulus)
                     : "null") ||
        !format(p, ",\"preparation_complete\":%s,\"preparation\":", boolean(prepared && observed)))
        return false;
    if (input->kind == INPUT_DEFAULT_TEXT) {
        if (!format(p, "{\"open_attempted\":%s,\"open_status\":", boolean(open_attempted)) ||
            !(open_attempted ? format(p, "%d", (int)opened) : text(p, "null")) ||
            !format(p, ",\"write_attempted\":%s,\"write_status\":", boolean(write_attempted)) ||
            !(write_attempted ? format(p, "%d", (int)wrote) : text(p, "null")) ||
            !format(p, ",\"close_attempted\":%s,\"close_status\":", boolean(close_attempted)) ||
            !(close_attempted ? format(p, "%d", (int)closed) : text(p, "null")) ||
            !text(p, ",\"last_error\":") ||
            !(open_attempted ? writer_error(p, cause) : text(p, "null")) ||
            !format(p,
                    ",\"state_present\":%s,\"state_before_retirement\":", boolean(state_present)) ||
            !(state_present ? writer_state(p, preparation_state) : text(p, "null")) ||
            !format(p, ",\"retirement_attempted\":%s,\"retirement_status\":",
                    boolean(retire_attempted)) ||
            !(retire_attempted ? format(p, "%d", (int)retired_status) : text(p, "null")) ||
            !text(p, ",\"retirement\":") ||
            !(retire_attempted
                  ? format(p, "{\"source_close_reached\":%s,\"additional_sink_close\":%s}",
                           boolean(retired.source_close_reached),
                           boolean(retired.additional_sink_close))
                  : text(p, "null")) ||
            !text(p, ",\"retirement_error\":") ||
            !(retire_attempted ? writer_error(p, retirement_error) : text(p, "null")) ||
            !text(p, "}"))
            return false;
    } else if (!text(p, "null"))
        return false;
    if (!end_row(p))
        return false;
    if (prepared && observed)
        ++p->inputs_done;
    return prepared && observed;
}

static bool stage(probe *p, size_t index) {
    clear_diagnostics(p);
    const mc_game_settings_diagnostics callbacks = {p, diagnostic};
    mc_game_settings_error error = {0};
    mc_game_settings_report report = {0};
    mc_game_settings_status status;
    mc_game_settings *owner;
    bool report_present = true;
    const char *name = index == 0   ? "constructor"
                       : index == 1 ? "reload"
                       : index == 2 ? "save"
                                    : "fresh_reload";
    const char *public_call = index == 0 && p->noarg     ? "mc_game_settings_create_defaults"
                              : index == 0 || index == 3 ? "mc_game_settings_create_file"
                              : index == 1               ? "mc_game_settings_load"
                                                         : "mc_game_settings_save";
    if (index == 0) {
        if (p->noarg) {
            report_present = false;
            status = mc_game_settings_create_defaults(&p->profile, &p->owners[0], &error);
        } else
            status = mc_game_settings_create_file(NULL, p->first, &p->profile, &callbacks,
                                                  &p->owners[0], &report, &error);
        owner = p->owners[0];
    } else if (index == 1) {
        status = mc_game_settings_load(p->owners[0], &callbacks, &report, &error);
        owner = p->owners[0];
    } else if (index == 2) {
        status = mc_game_settings_save(p->owners[0], &callbacks, &report, &error);
        owner = p->owners[0];
    } else {
        status = mc_game_settings_create_file(NULL, p->fresh, &p->profile, &callbacks,
                                              &p->owners[1], &report, &error);
        owner = p->owners[1];
    }
    /* Capture is complete before JSON emission; source owners and all returned
     * String/array views stay live across every subsequent stage. */
    const bool captured = capture(p, owner);
    const bool observed = file(p, index == 3 ? p->fresh_options : p->options, &p->after);
    if (!format(p,
                "{\"type\":\"stage\",\"ordinal\":%zu,\"stage\":\"%s\",\"public_call\":\"%s\","
                "\"native_status\":%d,\"native_error\":",
                index, name, public_call, (int)status) ||
        !settings_error(p, error) ||
        !format(p, ",\"report_present\":%s,\"report\":", boolean(report_present)) ||
        !(report_present ? report_json(p, report) : text(p, "null")) ||
        !text(p, ",\"diagnostics\":") || !diagnostics_json(p) || !text(p, ",\"state\":") ||
        !(captured ? state_json(p, owner) : text(p, "null")) || !text(p, ",\"options\":") ||
        !(observed ? file_json(p, &p->after) : text(p, "null")) ||
        !text(p, ",\"options_binding\":") ||
        !quoted(p,
                owner == NULL ? "unreturned-native-receiver"
                : !captured   ? "native-binding-unobserved"
                : p->snapshot.state.file_identity == NULL && p->snapshot.state.options_path == NULL
                    ? "native-N-null"
                : p->snapshot.state.file_identity != NULL && p->snapshot.state.options_path != NULL
                    ? "native-N"
                    : "native-N-inconsistent") ||
        !text(p, ",\"save_success_inferred_from_return\":false") || !end_row(p))
        return false;
    ++p->stages;
    return require(p,
                   status == MC_GAME_SETTINGS_OK && owner != NULL && captured && observed &&
                       p->failure == NULL,
                   "native stage/observation failure; complete available prefix retained");
}

static bool derive(probe *p) {
    if (!file(p, p->options, &p->before))
        return false;
    bool copied = true;
    if (p->before.kind == OBS_FILE_REGULAR)
        copied = write_raw(p, p->fresh_options, p->before.bytes, p->before.count, "wbx");
    else if (p->before.kind == OBS_FILE_DIRECTORY)
        copied = make_directory(p, p->fresh_options);
    const bool observed = file(p, p->fresh_options, &p->after);
    bool equal = false;
    if (observed)
        equal = p->before.kind == p->after.kind &&
                (p->before.kind != OBS_FILE_REGULAR ||
                 (p->before.count == p->after.count &&
                  memcmp(p->before.bytes, p->after.bytes, p->before.count) == 0));
    if (!text(p, "{\"type\":\"derived_input\",\"source\":") ||
        !quoted(p, p->noarg ? "declared scratch path; source N is null"
                            : "this native receiver's actual post-save file") ||
        !text(p, ",\"before\":") || !file_json(p, &p->before) || !text(p, ",\"fresh_copy\":") ||
        !(observed ? file_json(p, &p->after) : text(p, "null")) ||
        !format(
            p,
            ",\"copy_complete\":%s,\"actual_copy_equal\":%s,\"roundtrip_identity_assumed\":false",
            boolean(copied && observed), boolean(equal)) ||
        !end_row(p))
        return false;
    return require(p, copied && observed && equal, "actual derived file copy admission");
}

static bool header(probe *p, const char *plan, const char *result_path) {
    return text(p, "{\"type\":\"header\",\"schema\":1,\"scope\":\"native-public-GameSettings-"
                   "persistence-cut\",\"case\":") &&
           quoted(p, p->id) && text(p, ",\"constructor\":") &&
           quoted(p, p->noarg ? "noarg" : "file") && text(p, ",\"declared_charset\":") &&
           quoted(p, p->charset) && text(p, ",\"declared_lines\":") &&
           quoted(p, p->profile.lines == MC_JAVA_WRITER_LF ? "LF" : "CRLF") &&
           text(p, ",\"plan_path_utf8_hex\":") && hex(p, (const uint8_t *)plan, strlen(plan)) &&
           text(p, ",\"result_path_utf8_hex\":") &&
           hex(p, (const uint8_t *)result_path, strlen(result_path)) &&
           text(p, ",\"scratch_utf8_hex\":") &&
           hex(p, (const uint8_t *)p->scratch, strlen(p->scratch)) &&
           format(p, ",\"plan_bytes\":%zu,\"plan_raw_hex\":", p->raw_length) &&
           hex(p, p->raw, p->raw_length) &&
           text(p, ",\"input_preparation\":\"public direct Writer, compared to independently "
                   "materialized declared stimulus bytes before each settings "
                   "call\",\"alias_scope\":\"native persistent object/storage pointer equality "
                   "while both settings owners remain live; structural Java-identity adaptation "
                   "required\",\"original_only_unmatched\":[\"J,K,L,M static labels\",\"Java "
                   "Throwable/stack and PrintStream state\",\"actual Java Thread flags\",\"Java "
                   "File normalized path String identity\",\"effectful UI/audio/render "
                   "methods\"],\"source_equivalence_qualified\":false") &&
           end_row(p);
}

static int run(int argc, char **argv) {
    if (argc != 4)
        return 2;
    probe *p = calloc(1, sizeof(*p));
    if (p == NULL)
        return 1;
    int status = 2;
    bool signal_changed = false;
#ifndef _WIN32
    struct sigaction previous = {0};
#else
    int old_mode = -1;
#endif
    for (size_t i = 1; i < 4u; ++i) {
        size_t length = 0;
        while (length <= PATH_LIMIT && argv[i][length] != 0)
            ++length;
        if (!require(p, length != 0 && length <= PATH_LIMIT, "bounded nonempty UTF8 CLI path"))
            goto cleanup;
    }
    if (!parse_plan(p, argv[1]) || !empty_scratch(p, argv[2]))
        goto cleanup;
    const size_t length = strlen(argv[2]);
    memcpy(p->scratch, argv[2], length + 1u);
    if (!join(p, p->first, p->scratch, "first") || !join(p, p->fresh, p->scratch, "fresh") ||
        !join(p, p->options, p->first, "options.txt") ||
        !join(p, p->fresh_options, p->fresh, "options.txt"))
        goto cleanup;
    p->stdout_mode = strcmp(argv[3], "-") == 0;
    p->output = p->stdout_mode ? stdout : open_file(p, argv[3], "wbx");
    if (!require(p, p->output != NULL, "exclusive observation output"))
        goto cleanup;
    status = 1;
    if (!require(p, setvbuf(p->output, NULL, _IONBF, 0) == 0, "unbuffered JSON output"))
        goto cleanup;
#ifdef _WIN32
    if (p->stdout_mode) {
        old_mode = _setmode(_fileno(stdout), _O_BINARY);
        if (!require(p, old_mode != -1, "binary stdout"))
            goto cleanup;
    }
#else
    if (p->stdout_mode) {
        struct sigaction ignore = {0};
        ignore.sa_handler = SIG_IGN;
        if (!require(
                p, sigemptyset(&ignore.sa_mask) == 0 && sigaction(SIGPIPE, &ignore, &previous) == 0,
                "checked-output SIGPIPE admission"))
            goto cleanup;
        signal_changed = true;
    }
#endif
    if (!header(p, argv[1], argv[3]) || !make_directory(p, p->first) ||
        !make_directory(p, p->fresh) || !materialize(p, 0) || !stage(p, 0) || !materialize(p, 1) ||
        !stage(p, 1) || !stage(p, 2) || !derive(p) || !stage(p, 3))
        goto cleanup;
    /* Actual source cut is serialized before separate native retirement. */
    if (!format(p,
                "{\"type\":\"source_terminal\",\"status\":0,\"stage_count\":%zu,\"input_count\":%"
                "zu,\"native_alias_count\":%zu,\"source_equivalence_qualified\":false",
                p->stages, p->inputs_done, p->alias_count) ||
        !end_row(p))
        goto cleanup;
    status = 0;
cleanup:
    /* Independent retirement calls are attempted even after output/observation
     * failure. No source close/save is invented by these native cleanups. */
    if (p->preparation_writer != NULL) {
        mc_java_writer_error error = {0};
        if (mc_java_writer_retire(&p->preparation_writer, NULL, &error) != MC_JAVA_WRITER_OK)
            status = 1;
    }
    for (size_t i = 0; i < 2u; ++i) {
        if (p->owners[i] == NULL)
            continue;
        mc_game_settings_error error = {0};
        const mc_game_settings_status retired = mc_game_settings_retire(&p->owners[i], &error);
        if (p->output != NULL && !p->output_failed) {
            if (!format(p,
                        "{\"type\":\"native_retirement\",\"owner\":\"%s\",\"status\":%d,\"caller_"
                        "slot_null\":%s,\"error\":",
                        i == 0 ? "first" : "fresh", (int)retired, boolean(p->owners[i] == NULL)) ||
                !settings_error(p, error) || !end_row(p))
                status = 1;
        }
        if (retired != MC_GAME_SETTINGS_OK || p->owners[i] != NULL)
            status = 1;
        else
            ++p->retirements;
    }
    clear_diagnostics(p);
    if (p->output != NULL && !p->output_failed) {
        if (p->failure != NULL) {
            if (!text(p, "{\"type\":\"failure\",\"reason\":") || !quoted(p, p->failure) ||
                !format(p, ",\"system_error\":%d,\"win32_error\":%" PRIu32 ",\"stage_count\":%zu",
                        p->system_error, p->win32_error, p->stages) ||
                !end_row(p))
                status = 1;
        }
        const bool complete = status == 0 && p->stages == 4u && p->inputs_done == 2u &&
                              p->retirements == 2u && p->owners[0] == NULL &&
                              p->owners[1] == NULL && p->preparation_writer == NULL &&
                              p->failure == NULL;
        if (status == 0 && !complete)
            status = 1;
        if (!format(p,
                    "{\"type\":\"terminal\",\"status\":%d,\"complete\":%s,\"stage_count\":%zu,"
                    "\"input_count\":%zu,\"native_retirement_count\":%zu,\"rows_before_terminal\":%"
                    "zu,\"source_equivalence_qualified\":false",
                    status, boolean(complete), p->stages, p->inputs_done, p->retirements,
                    p->rows) ||
            !end_row(p))
            status = 1;
    }
    if (p->output != NULL) {
        errno = 0;
        const bool closed = p->stdout_mode ? fflush(p->output) == 0 : fclose(p->output) == 0;
        if (!require(p, closed, "final JSON output close/flush"))
            status = 1;
    }
#ifdef _WIN32
    (void)signal_changed;
    if (old_mode != -1 && _setmode(_fileno(stdout), old_mode) == -1)
        status = 1;
#else
    if (signal_changed && sigaction(SIGPIPE, &previous, NULL) != 0)
        status = 1;
#endif
    if (p->failure != NULL || status != 0) {
        if (fprintf(stderr, "GameSettings observer failure: %s (system_error=%d)\n",
                    p->failure != NULL ? p->failure : "native cleanup/output failure",
                    p->system_error) < 0 ||
            fflush(stderr) != 0)
            status = 1;
    }
    if (p->owners[0] == NULL && p->owners[1] == NULL && p->preparation_writer == NULL)
        free(p);
    /* A refused retirement retains the owner and its borrowed observation
     * context until process exit; never free behind an unexpectedly live owner. */
    return status;
}

#ifdef _WIN32
int wmain(int argc, wchar_t **wide) {
    if (argc != 4)
        return 2;
    char *args[4] = {NULL, NULL, NULL, NULL};
    int status = 1;
    for (size_t i = 1; i < 4u; ++i) {
        const int size =
            WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide[i], -1, NULL, 0, NULL, NULL);
        if (size <= 0 || size > (int)PATH_LIMIT + 1)
            goto cleanup;
        args[i] = malloc((size_t)size);
        if (args[i] == NULL || WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide[i], -1,
                                                   args[i], size, NULL, NULL) != size)
            goto cleanup;
    }
    status = run(argc, args);
cleanup:
    for (size_t i = 1; i < 4u; ++i)
        free(args[i]);
    return status;
}
#else
int main(int argc, char **argv) {
    return run(argc, argv);
}
#endif
