#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#include <stdbool.h>
#include <stddef.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <errno.h>
#include <unistd.h>
#endif

static bool close_fault;
static size_t close_calls, close_faults;

void mc_test_region_raf_fault_once(void) {
    close_fault = true;
}

size_t mc_test_region_raf_close_calls(void) {
    return close_calls;
}

size_t mc_test_region_raf_close_faults(void) {
    return close_faults;
}

/* The real provider consumes the descriptor before the one-shot declared
 * failure. This control never reports an unperformed successful operation. */
#ifdef _WIN32
static BOOL WINAPI observed_close(HANDLE handle) {
    const BOOL actual = CloseHandle(handle);
    ++close_calls;
    if (actual && close_fault) {
        close_fault = false;
        ++close_faults;
        SetLastError(ERROR_WRITE_FAULT);
        return FALSE;
    }
    return actual;
}

#define CloseHandle observed_close
#else
static int observed_close(int descriptor) {
    const int actual = close(descriptor);
    ++close_calls;
    if (actual == 0 && close_fault) {
        close_fault = false;
        ++close_faults;
        errno = EIO;
        return -1;
    }
    return actual;
}

#define close observed_close
#endif
#include "../../../src/world/storage/java_random_access.c"
#ifdef _WIN32
#undef CloseHandle
#else
#undef close
#endif
