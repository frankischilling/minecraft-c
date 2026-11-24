#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
/* This descriptor provider uses the POSIX.1-2008 atomic O_CLOEXEC open flag.
 * Declare its real feature profile before any direct or transitive header. */
#define _POSIX_C_SOURCE 200809L
#endif
#include "mc/world/storage/java_random_access.h"
#include "java_file_native.h"
#include <limits.h>
#include <stdlib.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

struct mc_java_random_access {
    mc_java_random_access_state state;
#ifdef _WIN32
    HANDLE descriptor;
#else
    int descriptor;
#endif
};

static mc_java_file_status result(mc_java_file_error *e, mc_java_file_status s, const char *op,
                                  int code) {
    if (e != NULL)
        *e = (mc_java_file_error){s, op, code};
    return s;
}

static int native_error(void) {
#ifdef _WIN32
    return (int)GetLastError();
#else
    return errno;
#endif
}

static mc_java_file_status admit(mc_java_random_access *r, mc_java_file_error *e) {
    if (r == NULL)
        return result(e, MC_JAVA_FILE_INVALID_ARGUMENT, "RandomAccessFile receiver", 0);
    if (r->state.active)
        return result(e, MC_JAVA_FILE_BUSY, "RandomAccessFile active", 0);
    if (r->state.phase != MC_JAVA_FILE_READY || !r->state.native_open)
        return result(e, MC_JAVA_FILE_STATE_ERROR, "RandomAccessFile closed/nonready", 0);
    return MC_JAVA_FILE_OK;
}

mc_java_file_status mc_java_random_access_allocate(mc_java_random_access **out,
                                                   mc_java_file_error *e) {
    if (out == NULL)
        return result(e, MC_JAVA_FILE_INVALID_ARGUMENT, "RandomAccessFile new output", 0);
    mc_java_random_access *r = calloc(1, sizeof(*r));
    if (r == NULL)
        return result(e, MC_JAVA_FILE_ALLOCATION_FAILED, "RandomAccessFile new", 0);
    *out = r;
    return result(e, MC_JAVA_FILE_OK, NULL, 0);
}

mc_java_file_status mc_java_random_access_construct(mc_java_random_access *r, mc_java_file *file,
                                                    mc_java_file_error *e) {
    if (r == NULL)
        return result(e, MC_JAVA_FILE_INVALID_ARGUMENT, "RandomAccessFile constructor", 0);
    if (r->state.active)
        return result(e, MC_JAVA_FILE_BUSY, "RandomAccessFile active constructor", 0);
    if (r->state.phase != MC_JAVA_FILE_RAW)
        return result(e, MC_JAVA_FILE_STATE_ERROR, "RandomAccessFile constructor phase", 0);
    r->state.active = true;
    r->state.phase = MC_JAVA_FILE_CONSTRUCTING;
    mc_java_file_status s;
    if (file == NULL) {
        s = result(e, MC_JAVA_FILE_SOURCE_NULL_FILE, "RandomAccessFile null File", 0);
        goto failed;
    }
    s = mc_java_file_retain(file, e);
    if (s != MC_JAVA_FILE_OK)
        goto failed;
    r->state.file = file;
    mc_file_native_char *path = NULL;
    s = mc_java_file_native_path_copy(file, &path, e);
    if (s != MC_JAVA_FILE_OK)
        goto failed;
#ifdef _WIN32
    r->descriptor =
        CreateFileW(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                    OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    int code = r->descriptor == INVALID_HANDLE_VALUE ? native_error() : 0;
    free(path);
    if (r->descriptor == INVALID_HANDLE_VALUE) {
        s = result(e, MC_JAVA_FILE_IO_ERROR, "RandomAccessFile open rw", code);
        goto failed;
    }
    r->state.native_open = true;
    if (GetFileType(r->descriptor) != FILE_TYPE_DISK) {
        s = result(e, MC_JAVA_FILE_UNSUPPORTED_PROVIDER, "RandomAccessFile regular descriptor", 0);
        goto failed;
    }
#else
    r->descriptor = open(path, O_RDWR | O_CREAT | O_CLOEXEC, 0666);
    int code = r->descriptor < 0 ? native_error() : 0;
    free(path);
    if (r->descriptor < 0) {
        s = result(e, MC_JAVA_FILE_IO_ERROR, "RandomAccessFile open rw", code);
        goto failed;
    }
    r->state.native_open = true;
    struct stat stat_value;
    if (fstat(r->descriptor, &stat_value) != 0) {
        s = result(e, MC_JAVA_FILE_IO_ERROR, "RandomAccessFile fstat", native_error());
        goto failed;
    }
    if (!S_ISREG(stat_value.st_mode)) {
        s = result(e, MC_JAVA_FILE_UNSUPPORTED_PROVIDER, "RandomAccessFile regular descriptor", 0);
        goto failed;
    }
#endif
    r->state.phase = MC_JAVA_FILE_READY;
    r->state.active = false;
    return result(e, MC_JAVA_FILE_OK, NULL, 0);
failed:
    r->state.phase = MC_JAVA_FILE_FAILED;
    r->state.active = false;
    return s;
}

mc_java_file_status mc_java_random_access_length(mc_java_random_access *r, int64_t *out,
                                                 mc_java_file_error *e) {
    mc_java_file_status s = admit(r, e);
    if (s != MC_JAVA_FILE_OK)
        return s;
    if (out == NULL)
        return result(e, MC_JAVA_FILE_INVALID_ARGUMENT, "RandomAccessFile length output", 0);
    r->state.active = true;
#ifdef _WIN32
    LARGE_INTEGER n;
    bool ok = GetFileSizeEx(r->descriptor, &n) != 0;
    int64_t length = ok ? n.QuadPart : 0;
#else
    struct stat n;
    bool ok = fstat(r->descriptor, &n) == 0;
    int64_t length = ok ? (int64_t)n.st_size : 0;
#endif
    int code = ok ? 0 : native_error();
    r->state.active = false;
    if (!ok)
        return result(e, MC_JAVA_FILE_IO_ERROR, "RandomAccessFile length", code);
    *out = length;
    return result(e, MC_JAVA_FILE_OK, NULL, 0);
}

mc_java_file_status mc_java_random_access_seek(mc_java_random_access *r, int64_t offset,
                                               mc_java_file_error *e) {
    mc_java_file_status s = admit(r, e);
    if (s != MC_JAVA_FILE_OK)
        return s;
    if (offset < 0)
        return result(e, MC_JAVA_FILE_IO_ERROR, "RandomAccessFile negative seek", 0);
    r->state.active = true;
    ++r->state.seek_calls;
#ifdef _WIN32
    LARGE_INTEGER p;
    p.QuadPart = offset;
    bool ok = SetFilePointerEx(r->descriptor, p, NULL, FILE_BEGIN) != 0;
#else
    bool ok = lseek(r->descriptor, (off_t)offset, SEEK_SET) != (off_t)-1;
#endif
    int code = ok ? 0 : native_error();
    r->state.active = false;
    return result(e, ok ? MC_JAVA_FILE_OK : MC_JAVA_FILE_IO_ERROR,
                  ok ? NULL : "RandomAccessFile seek", code);
}

mc_java_file_status mc_java_random_access_read(mc_java_random_access *r, uint8_t *bytes, size_t n,
                                               mc_java_file_read *out, mc_java_file_error *e) {
    mc_java_file_status s = admit(r, e);
    if (s != MC_JAVA_FILE_OK)
        return s;
    if (out == NULL || (bytes == NULL && n != 0) || n > INT_MAX)
        return result(e, MC_JAVA_FILE_INVALID_ARGUMENT, "RandomAccessFile read arguments", 0);
    if (n == 0) {
        *out = (mc_java_file_read){0, false};
        return result(e, MC_JAVA_FILE_OK, NULL, 0);
    }
    r->state.active = true;
    ++r->state.read_calls;
#ifdef _WIN32
    DWORD count = 0;
    bool ok = ReadFile(r->descriptor, bytes, (DWORD)n, &count, NULL) != 0;
    size_t got = count;
#else
    ssize_t count;
    do {
        count = read(r->descriptor, bytes, n);
    } while (count < 0 && errno == EINTR);
    bool ok = count >= 0;
    size_t got = ok ? (size_t)count : 0;
#endif
    int code = ok ? 0 : native_error();
    r->state.bytes_read += got;
    r->state.active = false;
    if (!ok)
        return result(e, MC_JAVA_FILE_IO_ERROR, "RandomAccessFile read", code);
    *out = (mc_java_file_read){got, got == 0};
    return result(e, MC_JAVA_FILE_OK, NULL, 0);
}

mc_java_file_status mc_java_random_access_write(mc_java_random_access *r, const uint8_t *bytes,
                                                size_t n, mc_java_file_error *e) {
    mc_java_file_status s = admit(r, e);
    if (s != MC_JAVA_FILE_OK)
        return s;
    if ((bytes == NULL && n != 0) || n > INT_MAX)
        return result(e, MC_JAVA_FILE_INVALID_ARGUMENT, "RandomAccessFile write arguments", 0);
    r->state.active = true;
    ++r->state.write_calls;
    size_t position = 0;
    bool ok = true;
    while (position < n) {
#ifdef _WIN32
        DWORD count = 0;
        ok = WriteFile(r->descriptor, bytes + position, (DWORD)(n - position), &count, NULL) != 0;
        size_t wrote = count;
#else
        ssize_t count;
        do {
            count = write(r->descriptor, bytes + position, n - position);
        } while (count < 0 && errno == EINTR);
        ok = count >= 0;
        size_t wrote = ok ? (size_t)count : 0;
#endif
        position += wrote;
        r->state.bytes_written += wrote;
        if (!ok || wrote == 0) {
            ok = false;
            break;
        }
    }
    int code = ok ? 0 : native_error();
    r->state.active = false;
    return result(e, ok ? MC_JAVA_FILE_OK : MC_JAVA_FILE_IO_ERROR,
                  ok ? NULL : "RandomAccessFile write", code);
}

static mc_java_file_status close_descriptor(mc_java_random_access *r, bool source,
                                            mc_java_file_error *e) {
    if (r == NULL)
        return result(e, MC_JAVA_FILE_INVALID_ARGUMENT, "RandomAccessFile close receiver", 0);
    if (r->state.active)
        return result(e, MC_JAVA_FILE_BUSY, "RandomAccessFile active close", 0);
    if (source)
        r->state.close_reached = true;
    if (!r->state.native_open)
        return result(e, MC_JAVA_FILE_OK, NULL, 0);
    r->state.active = true;
    r->state.native_open = false;
#ifdef _WIN32
    bool ok = CloseHandle(r->descriptor) != 0;
#else
    bool ok = close(r->descriptor) == 0;
#endif
    int code = ok ? 0 : native_error();
    r->state.close_succeeded = ok;
    r->state.active = false;
    return result(e, ok ? MC_JAVA_FILE_OK : MC_JAVA_FILE_IO_ERROR,
                  ok ? NULL : "RandomAccessFile close", code);
}

mc_java_file_status mc_java_random_access_close(mc_java_random_access *r, mc_java_file_error *e) {
    return close_descriptor(r, true, e);
}

mc_java_file_status mc_java_random_access_release(mc_java_random_access **slot,
                                                  mc_java_file_error *e) {
    if (slot == NULL)
        return result(e, MC_JAVA_FILE_INVALID_ARGUMENT, "RandomAccessFile release slot", 0);
    mc_java_random_access *r = *slot;
    if (r == NULL)
        return result(e, MC_JAVA_FILE_OK, NULL, 0);
    if (r->state.active)
        return result(e, MC_JAVA_FILE_BUSY, "RandomAccessFile active release", 0);
    mc_java_file_status s = close_descriptor(r, false, e);
    if (s != MC_JAVA_FILE_OK)
        return s;
    s = mc_java_file_release(&r->state.file, e);
    if (s != MC_JAVA_FILE_OK)
        return s;
    *slot = NULL;
    free(r);
    return result(e, MC_JAVA_FILE_OK, NULL, 0);
}

bool mc_java_random_access_inspect(const mc_java_random_access *r,
                                   mc_java_random_access_state *out) {
    if (r == NULL || out == NULL || r->state.active)
        return false;
    *out = r->state;
    return true;
}
