#ifndef MC_TEST_PRINT_STREAM_IO_H
#define MC_TEST_PRINT_STREAM_IO_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

/* Real owned file/OS-pipe handles. No byte provider or replacement stdout. */
typedef struct print_test_io {
    FILE *file;
#ifdef _WIN32
    HANDLE old, pipe_write;
#else
    int old, pipe_write;
#endif
    bool redirected, pipe;
} print_test_io;

static bool print_test_redirect(print_test_io *io, bool broken_pipe) {
    *io = (print_test_io){0};
#ifdef _WIN32
    io->old = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE next;
    if (broken_pipe) {
        HANDLE read_end;
        if (!CreatePipe(&read_end, &io->pipe_write, NULL, 0))
            return false;
        CloseHandle(read_end);
        next = io->pipe_write;
        io->pipe = true;
    } else {
        if (tmpfile_s(&io->file) != 0 || io->file == NULL)
            return false;
        next = (HANDLE)_get_osfhandle(_fileno(io->file));
    }
    if (!SetStdHandle(STD_OUTPUT_HANDLE, next)) {
        if (io->file != NULL)
            fclose(io->file);
        if (io->pipe)
            CloseHandle(io->pipe_write);
        *io = (print_test_io){0};
        return false;
    }
#else
    io->old = dup(STDOUT_FILENO);
    if (io->old < 0)
        return false;
    int next;
    if (broken_pipe) {
        int ends[2];
        if (pipe(ends) != 0) {
            close(io->old);
            return false;
        }
        close(ends[0]);
        io->pipe_write = ends[1];
        io->pipe = true;
        next = io->pipe_write;
    } else {
        io->file = tmpfile();
        if (io->file == NULL) {
            close(io->old);
            return false;
        }
        next = fileno(io->file);
    }
    if (dup2(next, STDOUT_FILENO) < 0) {
        if (io->file != NULL)
            fclose(io->file);
        if (io->pipe)
            close(io->pipe_write);
        close(io->old);
        *io = (print_test_io){0};
        return false;
    }
#endif
    io->redirected = true;
    return true;
}

static size_t print_test_read(print_test_io *io, uint8_t *bytes, size_t capacity) {
    if (io->file == NULL || fflush(io->file) != 0 || fseek(io->file, 0, SEEK_SET) != 0)
        return SIZE_MAX;
    const size_t count = fread(bytes, 1, capacity, io->file);
    return ferror(io->file) ? SIZE_MAX : count;
}

static bool print_test_restore(print_test_io *io) {
    bool ok = true;
    if (io->redirected) {
#ifdef _WIN32
        if (!SetStdHandle(STD_OUTPUT_HANDLE, io->old))
            ok = false;
#else
        if (dup2(io->old, STDOUT_FILENO) < 0)
            ok = false;
        if (close(io->old) != 0)
            ok = false;
#endif
    }
    if (io->file != NULL) {
        if (fclose(io->file) != 0)
            ok = false;
    }
    if (io->pipe) {
#ifdef _WIN32
        if (!CloseHandle(io->pipe_write))
            ok = false;
#else
        if (close(io->pipe_write) != 0)
            ok = false;
#endif
    }
    *io = (print_test_io){0};
    return ok;
}

#endif
