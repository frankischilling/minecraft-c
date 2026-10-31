/* Authored source-only controls: no execution claim. Every File/OS query below
 * is genuine; compile wrappers forward actual calls and are native policy only. */
#ifndef _WIN32
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#endif
#include "../../../src/world/storage/java_file_internal.h"
#include "../../../src/world/storage/java_file_native.h"
#include "../../../src/world/storage/java_file_text.h"
#include "mc/world/storage/java_file.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <wchar.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#endif
static size_t checks;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            fprintf(stderr, "File attributes check:%d: %s\n", __LINE__, #x);                       \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)

static mc_world_info_name *copy_name(mc_nbt_string text) {
    mc_world_info_name *name = NULL;
    CHECK(mc_world_info_name_copy(text, SIZE_MAX, &name) == MC_WORLD_INFO_OK);
    return name;
}

static mc_world_info_name *ascii(const char *text) {
    size_t n = strlen(text);
    uint16_t u[64];
    CHECK(n <= 64);
    for (size_t i = 0; i < n; ++i) {
        CHECK((unsigned char)text[i] < 128);
        u[i] = (uint16_t)(unsigned char)text[i];
    }
    return copy_name((mc_nbt_string){u, n});
}

static mc_java_file *file(mc_java_files *files, mc_java_file *parent, mc_world_info_name *name) {
    mc_java_file *f = NULL;
    mc_java_file_error e;
    CHECK(mc_java_file_allocate(files, &f, &e) == MC_JAVA_FILE_OK);
    CHECK((parent == NULL ? mc_java_file_construct_string(f, name, &e)
                          : mc_java_file_construct_child(f, parent, name, &e)) == MC_JAVA_FILE_OK);
    return f;
}

static mc_file_native_char *path(mc_java_file *f) {
    mc_file_native_char *p = NULL;
    mc_java_file_error e;
    CHECK(mc_java_file_native_path_copy(f, &p, &e) == MC_JAVA_FILE_OK && p != NULL);
    return p;
}

static void write_file(mc_java_file *f, bool append) {
    mc_file_native_char *p = path(f);
#ifdef _WIN32
    FILE *out = _wfopen(p, append ? L"ab" : L"wb");
#else
    FILE *out = fopen(p, append ? "ab" : "wb");
#endif
    free(p);
    CHECK(out != NULL);
    const uint8_t a[] = {1, 2, 3}, b[] = {4, 5, 6, 7, 8};
    const uint8_t *bytes = append ? b : a;
    size_t n = append ? sizeof(b) : sizeof(a);
    CHECK(fwrite(bytes, 1, n, out) == n);
    CHECK(fclose(out) == 0);
}

static mc_java_file *child(mc_java_files *files, mc_java_file *parent, const char *name) {
    mc_world_info_name *text = ascii(name);
    mc_java_file *result = file(files, parent, text);
    mc_world_info_name_release(text);
    return result;
}

static void deletion(mc_java_files *files, mc_java_file *root) {
    mc_java_file_error error;
    mc_java_file *regular = child(files, root, "delete-file.bin");
    bool result = true;
    CHECK(mc_java_file_delete(regular, &result, &error) == MC_JAVA_FILE_OK && !result);
    write_file(regular, false);
    mc_file_native_char *native = path(regular);
#ifdef _WIN32
    CHECK(SetFileAttributesW(native, FILE_ATTRIBUTE_READONLY) != 0);
#else
    CHECK(chmod(native, 0444) == 0);
#endif
    free(native);
    CHECK(mc_java_file_delete(regular, &result, &error) == MC_JAVA_FILE_OK && result);
    result = true;
    CHECK(mc_java_file_exists(regular, &result, &error) == MC_JAVA_FILE_OK && !result);
    CHECK(mc_java_file_delete(regular, &result, &error) == MC_JAVA_FILE_OK && !result);
    mc_java_file *directory = child(files, root, "delete-directory");
    CHECK(mc_java_file_mkdir(directory, &result, &error) == MC_JAVA_FILE_OK && result);
    mc_java_file *entry = child(files, directory, "entry.bin");
    write_file(entry, false);
    result = true;
    CHECK(mc_java_file_delete(directory, &result, &error) == MC_JAVA_FILE_OK && !result);
    CHECK(mc_java_file_exists(entry, &result, &error) == MC_JAVA_FILE_OK && result);
    CHECK(mc_java_file_delete(entry, &result, &error) == MC_JAVA_FILE_OK && result);
    CHECK(mc_java_file_delete(directory, &result, &error) == MC_JAVA_FILE_OK && result);
    CHECK(mc_java_file_delete(directory, &result, &error) == MC_JAVA_FILE_OK && !result);
    CHECK(mc_java_file_release(&entry, &error) == MC_JAVA_FILE_OK);
    CHECK(mc_java_file_release(&directory, &error) == MC_JAVA_FILE_OK);
    CHECK(mc_java_file_release(&regular, &error) == MC_JAVA_FILE_OK);
}

static void set_time(mc_java_file *f, int64_t milliseconds) {
    mc_file_native_char *p = path(f);
#ifdef _WIN32
    HANDLE h = CreateFileW(p, FILE_WRITE_ATTRIBUTES,
                           FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    uint64_t ticks = (uint64_t)(milliseconds + INT64_C(11644473600000)) * UINT64_C(10000) + 9000;
    FILETIME time = {(DWORD)ticks, (DWORD)(ticks >> 32u)};
    CHECK(SetFileTime(h, NULL, NULL, &time) != 0);
    CHECK(CloseHandle(h) != 0);
#else
    int64_t seconds = milliseconds / 1000, remainder = milliseconds % 1000;
    if (remainder < 0) {
        --seconds;
        remainder += 1000;
    }
    struct timespec times[2] = {
        {.tv_nsec = UTIME_OMIT},
        {.tv_sec = (time_t)seconds, .tv_nsec = (long)(remainder * 1000000 + 900000)}};
    CHECK(utimensat(AT_FDCWD, p, times, 0) == 0);
#endif
    free(p);
}

static int64_t native_directory_length(mc_java_file *f) {
    mc_file_native_char *p = path(f);
    int64_t value;
#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA data;
    CHECK(GetFileAttributesExW(p, GetFileExInfoStandard, &data) != 0);
    value = (int64_t)(((uint64_t)data.nFileSizeHigh << 32u) | data.nFileSizeLow);
#else
    struct stat information;
    CHECK(stat(p, &information) == 0);
    value = (int64_t)information.st_size;
#endif
    free(p);
    return value;
}
#ifdef MC_FILE_ATTRIBUTE_FAULT_CONTROL
static mc_java_file **watched;
static size_t reentries;

static void reenter(void) {
    if (watched == NULL)
        return;
    mc_java_file *before = *watched;
    mc_java_file_error e;
    mc_java_file_state state;
    CHECK(mc_java_file_read_state(before, &state, &e) == MC_JAVA_FILE_OK && state.active);
    ++reentries;
    CHECK(mc_java_file_release(watched, &e) == MC_JAVA_FILE_BUSY && *watched == before);
    CHECK(mc_java_file_retain(before, &e) == MC_JAVA_FILE_BUSY);
    int64_t value = 91;
    CHECK(mc_java_file_length(before, &value, &e) == MC_JAVA_FILE_BUSY && value == 91);
    mc_file_native_char dummy = 0, *p = &dummy;
    CHECK(mc_java_file_native_path_copy(before, &p, &e) == MC_JAVA_FILE_BUSY && p == &dummy);
}
#ifdef _WIN32
static bool fail_close;
static size_t close_calls;

BOOL mc_test_file_time(HANDLE h, LPFILETIME created, LPFILETIME accessed, LPFILETIME modified) {
    reenter();
    return GetFileTime(h, created, accessed, modified);
}

BOOL mc_test_file_close(HANDLE h) {
    ++close_calls;
    reenter();
    BOOL closed = CloseHandle(h);
    if (fail_close && closed) {
        fail_close = false;
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    return closed;
}
#else
int mc_test_file_stat(const char *p, struct stat *information) {
    reenter();
    return stat(p, information);
}
#endif
#endif
static void rejected(mc_java_file *f, mc_java_file_status expected) {
    mc_java_file_error e;
    bool b = true;
    int64_t n = 71;
    mc_file_native_char dummy = 0, *p = &dummy;
    CHECK(mc_java_file_is_file(f, &b, &e) == expected && b);
    CHECK(mc_java_file_length(f, &n, &e) == expected && n == 71);
    CHECK(mc_java_file_last_modified(f, &n, &e) == expected && n == 71);
    CHECK(mc_java_file_native_path_copy(f, &p, &e) == expected && p == &dummy);
}

static void run(mc_nbt_string text) {
    mc_java_files *files = NULL;
    mc_java_file_error e;
    CHECK(mc_java_files_create(NULL, &files, &e) == MC_JAVA_FILE_OK);
    mc_world_info_name *root_name = copy_name(text);
    mc_java_file *root = file(files, NULL, root_name);
    mc_world_info_name_release(root_name);
    bool exists = true;
    CHECK(mc_java_file_exists(root, &exists, &e) == MC_JAVA_FILE_OK && !exists);
    bool made = false;
    CHECK(mc_java_file_mkdirs(root, &made, &e) == MC_JAVA_FILE_OK && made);
    const uint16_t unicode[] = {'a', 't',    't',    'r', '-', 0x03a9, '-', 0x00e9,
                                '-', 0xd83d, 0xde00, '.', 'b', 'i',    'n'};
    mc_world_info_name *name =
                           copy_name((mc_nbt_string){unicode, sizeof(unicode) / sizeof(*unicode)}),
                       *absent_name = ascii("absent");
    mc_java_file *regular = file(files, root, name), *absent = file(files, root, absent_name);
    mc_world_info_name_release(name);
    mc_world_info_name_release(absent_name);
    bool b = true;
    int64_t number = 19;
    CHECK(mc_java_file_is_file(absent, &b, &e) == MC_JAVA_FILE_OK && !b);
    CHECK(mc_java_file_length(absent, &number, &e) == MC_JAVA_FILE_OK && number == 0);
    number = 19;
    CHECK(mc_java_file_last_modified(absent, &number, &e) == MC_JAVA_FILE_OK && number == 0);
    CHECK(mc_java_file_is_file(root, &b, &e) == MC_JAVA_FILE_OK && !b);
    CHECK(mc_java_file_is_directory(root, &b, &e) == MC_JAVA_FILE_OK && b);
    CHECK(mc_java_file_length(root, &number, &e) == MC_JAVA_FILE_OK &&
          number == native_directory_length(root));
    write_file(regular, false);
    CHECK(mc_java_file_is_file(regular, &b, &e) == MC_JAVA_FILE_OK && b);
    CHECK(mc_java_file_length(regular, &number, &e) == MC_JAVA_FILE_OK && number == 3);
    /* Mutate the SAME owned fixture between independently reached operations. */
    write_file(regular, true);
    CHECK(mc_java_file_length(regular, &number, &e) == MC_JAVA_FILE_OK && number == 8);
    CHECK(mc_java_file_is_directory(regular, &b, &e) == MC_JAVA_FILE_OK && !b);
    set_time(regular, INT64_C(1600000000123));
    CHECK(mc_java_file_last_modified(regular, &number, &e) == MC_JAVA_FILE_OK &&
          number == INT64_C(1600000000123));
    set_time(regular, -1);
    CHECK(mc_java_file_last_modified(regular, &number, &e) == MC_JAVA_FILE_OK && number == -1);
    set_time(regular, INT64_C(1600000002345));
    CHECK(mc_java_file_last_modified(regular, &number, &e) == MC_JAVA_FILE_OK &&
          number == INT64_C(1600000002345));
    mc_java_file_state before, after;
    CHECK(mc_java_file_read_state(regular, &before, &e) == MC_JAVA_FILE_OK);
    mc_file_native_char *native = path(regular);
    CHECK(mc_java_file_read_state(regular, &after, &e) == MC_JAVA_FILE_OK &&
          after.path == before.path && after.holds == before.holds &&
          after.exists_calls == before.exists_calls && after.mkdir_calls == before.mkdir_calls &&
          after.canonicalize_calls == before.canonicalize_calls &&
          after.last_native_error == before.last_native_error && !after.active);
    mc_java_file_stream *stream = NULL;
    CHECK(mc_java_file_stream_allocate(files, &stream, &e) == MC_JAVA_FILE_OK &&
          mc_java_file_stream_construct(stream, regular, &e) == MC_JAVA_FILE_OK);
    CHECK(mc_java_file_stream_source_close(stream, &e) == MC_JAVA_FILE_OK &&
          mc_java_file_stream_release(&stream, &e) == MC_JAVA_FILE_OK);
    CHECK(mc_java_file_length(regular, &number, &e) == MC_JAVA_FILE_OK && number == 8);
#ifdef MC_FILE_ATTRIBUTE_FAULT_CONTROL
    watched = &regular;
    size_t entered = reentries;
    CHECK(mc_java_file_last_modified(regular, &number, &e) == MC_JAVA_FILE_OK &&
          reentries > entered);
#ifdef _WIN32
    size_t closed = close_calls;
    fail_close = true;
    number = 42;
    CHECK(mc_java_file_last_modified(regular, &number, &e) == MC_JAVA_FILE_IO_ERROR &&
          number == 42 &&
          strcmp(e.operation, "File lastModified native temporary-handle close") == 0);
    CHECK(close_calls == closed + 1);
    CHECK(mc_java_file_read_state(regular, &after, &e) == MC_JAVA_FILE_OK &&
          after.phase == MC_JAVA_FILE_READY && !after.active);
    CHECK(mc_java_file_last_modified(regular, &number, &e) == MC_JAVA_FILE_OK &&
          number == INT64_C(1600000002345) && close_calls == closed + 2);
#endif
    watched = NULL;
#ifdef _WIN32
    size_t retirement_close_count = close_calls;
#endif
#endif
    mc_java_file *raw = NULL;
    deletion(files, root);
    CHECK(mc_java_file_allocate(files, &raw, &e) == MC_JAVA_FILE_OK);
    rejected(raw, MC_JAVA_FILE_STATE_ERROR);
    CHECK(mc_java_file_construct_string(raw, NULL, &e) == MC_JAVA_FILE_SOURCE_NULL_STRING);
    rejected(raw, MC_JAVA_FILE_STATE_ERROR);
    CHECK(mc_java_file_release(&raw, &e) == MC_JAVA_FILE_OK);
    rejected(NULL, MC_JAVA_FILE_INVALID_ARGUMENT);
    CHECK(mc_java_file_length(NULL, NULL, &e) == MC_JAVA_FILE_INVALID_ARGUMENT &&
          strcmp(e.operation, "File length output") == 0);
    mc_java_file_options options = mc_java_file_default_options();
    options.max_string_bytes = 1;
    mc_java_files *bounded = NULL;
    CHECK(mc_java_files_create(&options, &bounded, &e) == MC_JAVA_FILE_OK);
    mc_java_file *limited = file(bounded, NULL, mc_java_file_path(regular));
    rejected(limited, MC_JAVA_FILE_LIMIT);
    b = true;
    CHECK(mc_java_file_delete(limited, &b, &e) == MC_JAVA_FILE_LIMIT && b);
    uint16_t nul = 0;
    mc_world_info_name *nul_name = copy_name((mc_nbt_string){&nul, 1});
    mc_java_file *invalid = file(bounded, NULL, nul_name);
    mc_world_info_name_release(nul_name);
    b = true;
    number = 9;
    CHECK(mc_java_file_is_file(invalid, &b, &e) == MC_JAVA_FILE_OK && !b);
    b = true;
    CHECK(mc_java_file_delete(invalid, &b, &e) == MC_JAVA_FILE_OK && !b);
    CHECK(mc_java_file_length(invalid, &number, &e) == MC_JAVA_FILE_OK && number == 0);
    CHECK(mc_java_file_last_modified(invalid, &number, &e) == MC_JAVA_FILE_OK && number == 0);
    CHECK(mc_java_file_release(&invalid, &e) == MC_JAVA_FILE_OK &&
          mc_java_file_release(&limited, &e) == MC_JAVA_FILE_OK &&
          mc_java_files_release(&bounded, &e) == MC_JAVA_FILE_OK);
    CHECK(mc_java_file_retain(regular, &e) == MC_JAVA_FILE_OK);
    mc_java_file *alias = regular;
    CHECK(mc_java_file_release(&regular, &e) == MC_JAVA_FILE_OK && regular == NULL);
    CHECK(mc_java_file_is_file(alias, &b, &e) == MC_JAVA_FILE_OK && b);
    CHECK(mc_java_file_release(&alias, &e) == MC_JAVA_FILE_OK && alias == NULL);
#if defined(MC_FILE_ATTRIBUTE_FAULT_CONTROL) && defined(_WIN32)
    CHECK(close_calls == retirement_close_count);
#endif
/* Independent malloc result stays valid after every File occurrence ended. */
#ifdef _WIN32
    FILE *input = _wfopen(native, L"rb");
#else
    FILE *input = fopen(native, "rb");
#endif
    CHECK(input != NULL && fgetc(input) == 1);
    CHECK(fclose(input) == 0);
    free(native);
    CHECK(mc_java_file_release(&absent, &e) == MC_JAVA_FILE_OK &&
          mc_java_file_release(&root, &e) == MC_JAVA_FILE_OK &&
          mc_java_files_release(&files, &e) == MC_JAVA_FILE_OK);
    printf("File attributes: %zu checks; independent real queries and native lifetime\n", checks);
}

static int arguments(void) {
    fputs("File attributes input: expected absent owned fixture root\n", stderr);
    return 2;
}
#ifdef _WIN32
int wmain(int argc, wchar_t **argv) {
    if (argc != 2)
        return arguments();
    _Static_assert(sizeof(wchar_t) == sizeof(uint16_t), "Windows UTF16 argv");
    run((mc_nbt_string){(const uint16_t *)argv[1], wcslen(argv[1])});
    return 0;
}
#else
int main(int argc, char **argv) {
    if (argc != 2)
        return arguments();
    mc_world_info_name *name = NULL;
    mc_java_file_error e;
    if (mc_file_decode_utf8(argv[1], SIZE_MAX, &name, &e) != MC_JAVA_FILE_OK)
        return arguments();
    run(mc_world_info_name_text(name));
    mc_world_info_name_release(name);
    return 0;
}
#endif
