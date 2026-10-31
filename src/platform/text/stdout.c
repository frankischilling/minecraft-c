#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "stdout.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#endif

mc_text_stdout_status mc_text_stdout_capture(mc_text_stdout *out) {
    if (out == NULL)
        return MC_TEXT_STDOUT_INVALID_ARGUMENT;
#ifdef _WIN32
    const mc_text_stdout captured = {(intptr_t)GetStdHandle(STD_OUTPUT_HANDLE)};
#else
    const mc_text_stdout captured = {STDOUT_FILENO};
#endif
    *out = captured;
    return MC_TEXT_STDOUT_OK;
}

mc_text_stdout_status mc_text_stdout_println(const mc_text_stdout *stream, const char *ascii,
                                             size_t length, bool *io_failed) {
    if (stream == NULL || io_failed == NULL || length > 64 || (ascii == NULL && length != 0))
        return MC_TEXT_STDOUT_INVALID_ARGUMENT;
    char bytes[66];
    for (size_t i = 0; i < length; ++i) {
        if ((unsigned char)ascii[i] > 0x7fu)
            return MC_TEXT_STDOUT_INVALID_ARGUMENT;
        bytes[i] = ascii[i];
    }
    size_t total = length;
#ifdef _WIN32
    bytes[total++] = '\r';
#endif
    bytes[total++] = '\n';
    size_t committed = 0;
    int system_error = 0;
    return mc_text_stdout_write(stream, (const uint8_t *)bytes, total, &committed, io_failed,
                                &system_error);
}

mc_text_stdout_status mc_text_stdout_write(const mc_text_stdout *stream, const uint8_t *bytes,
                                           size_t total, size_t *committed, bool *io_failed,
                                           int *system_error) {
    if (stream == NULL || committed == NULL || io_failed == NULL || system_error == NULL ||
        (bytes == NULL && total != 0))
        return MC_TEXT_STDOUT_INVALID_ARGUMENT;
    size_t offset = 0;
    bool failed = false;
    int native_error = 0;
#ifdef _WIN32
    while (offset < total) {
        DWORD written = 0;
        const BOOL wrote = WriteFile(
            (HANDLE)stream->handle, bytes + offset,
            (DWORD)((total - offset) > UINT32_MAX ? UINT32_MAX : total - offset), &written, NULL);
        if (!wrote || written == 0) {
            native_error = wrote ? 0 : (int)GetLastError();
            failed = true;
            break;
        }
        if ((size_t)written > total - offset)
            return MC_TEXT_STDOUT_NATIVE_FAILURE;
        offset += (size_t)written;
    }
#else
    if (stream->handle < INT_MIN || stream->handle > INT_MAX)
        return MC_TEXT_STDOUT_INVALID_ARGUMENT;
    sigset_t pipe_signal, old_mask, pending;
    if (sigemptyset(&pipe_signal) != 0 || sigaddset(&pipe_signal, SIGPIPE) != 0)
        return MC_TEXT_STDOUT_NATIVE_FAILURE;
    if (pthread_sigmask(SIG_BLOCK, &pipe_signal, &old_mask) != 0)
        return MC_TEXT_STDOUT_NATIVE_FAILURE;

    /* Every path after blocking reaches mask restoration. Preserve a SIGPIPE
     * already pending before our write, including when it was already blocked. */
    mc_text_stdout_status status = MC_TEXT_STDOUT_NATIVE_FAILURE;
    if (sigpending(&pending) != 0)
        goto restore_mask;
    const int was_pending = sigismember(&pending, SIGPIPE);
    if (was_pending < 0)
        goto restore_mask;
    bool broken_pipe = false;
    while (offset < total) {
        const ssize_t written =
            write((int)stream->handle, bytes + offset,
                  total - offset > (size_t)SSIZE_MAX ? (size_t)SSIZE_MAX : total - offset);
        if (written > 0) {
            if ((size_t)written > total - offset)
                goto restore_mask;
            offset += (size_t)written;
        } else if (written < 0 && errno == EINTR) {
            continue;
        } else {
            native_error = written < 0 ? errno : 0;
            broken_pipe = written < 0 && native_error == EPIPE;
            failed = true;
            break;
        }
    }
    if (broken_pipe && was_pending == 0) {
        const struct timespec no_wait = {0, 0};
        int signal;
        do {
            signal = sigtimedwait(&pipe_signal, NULL, &no_wait);
        } while (signal < 0 && errno == EINTR);
        /* EAGAIN is valid when SIGPIPE was ignored and was never queued. */
        if (signal != SIGPIPE && !(signal < 0 && errno == EAGAIN))
            goto restore_mask;
    }
    status = MC_TEXT_STDOUT_OK;
restore_mask:
    if (pthread_sigmask(SIG_SETMASK, &old_mask, NULL) != 0)
        return MC_TEXT_STDOUT_NATIVE_FAILURE;
    if (status != MC_TEXT_STDOUT_OK)
        return status;
#endif
    *committed = offset;
    *io_failed = failed;
    *system_error = native_error;
    return MC_TEXT_STDOUT_OK;
}
