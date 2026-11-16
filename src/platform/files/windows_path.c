#include "directory.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <stdlib.h>
#include <string.h>
#include <windows.h>

static mc_file_io_status result(mc_file_io_error *error, mc_file_io_status status,
                                const char *operation, DWORD code) {
    if (error != NULL)
        *error = (mc_file_io_error){status, (int)code, operation, true};
    return status;
}

/* Keep source-facing paths intact. Only the private native transport uses an
 * absolute extended name, including for temporary siblings of short targets. */
mc_file_io_status mc_file_native_extend(wchar_t **owned, bool *extended, mc_file_io_error *error) {
    wchar_t *input = *owned;
    if (wcslen(input) >= 4u && input[0] == L'\\' && input[1] == L'\\' &&
        (input[2] == L'?' || input[2] == L'.') && input[3] == L'\\') {
        *extended = false;
        return MC_FILE_IO_OK;
    }
    DWORD capacity = GetFullPathNameW(input, 0, NULL, NULL);
    if (capacity == 0)
        return result(error, MC_FILE_IO_NATIVE_ERROR, "path.absolute.win32", GetLastError());
    if (capacity <= MAX_PATH - 12u) {
        *extended = false;
        return MC_FILE_IO_OK;
    }
    if (capacity > 32768u)
        return result(error, MC_FILE_IO_LIMIT, "path.extended_length", 0);
    wchar_t *full = malloc(((size_t)capacity + 8u) * sizeof(*full));
    if (full == NULL)
        return result(error, MC_FILE_IO_ALLOCATION_FAILED, "path.absolute.allocate", 0);
    DWORD length = GetFullPathNameW(input, capacity, full + 8, NULL);
    if (length == 0 || length >= capacity) {
        DWORD code = length == 0 ? GetLastError() : ERROR_INSUFFICIENT_BUFFER;
        free(full);
        return result(error, MC_FILE_IO_NATIVE_ERROR, "path.absolute.win32", code);
    }
    /* Extended paths do not trim component suffixes. Avoid addressing a
     * different object for these legacy aliases. Dot/parent components were
     * already resolved by GetFullPathNameW. */
    for (size_t i = 0; i < length; ++i) {
        if ((full[8u + i] == L'.' || full[8u + i] == L' ') &&
            (i + 1u == length || full[9u + i] == L'\\')) {
            free(full);
            return result(error, MC_FILE_IO_UNSUPPORTED_PATH, "path.extended_component_alias", 0);
        }
    }
    bool unc = length >= 2u && full[8] == L'\\' && full[9] == L'\\';
    size_t prefix = unc ? 8u : 4u;
    size_t skip = unc ? 2u : 0u;
    if ((size_t)length - skip + prefix >= 32768u) {
        free(full);
        return result(error, MC_FILE_IO_LIMIT, "path.extended_length", 0);
    }
    memmove(full + prefix, full + 8u + skip, (length - skip + 1u) * sizeof(*full));
    memcpy(full, unc ? L"\\\\?\\UNC\\" : L"\\\\?\\", prefix * sizeof(*full));
    free(input);
    *owned = full;
    *extended = true;
    return MC_FILE_IO_OK;
}
#endif
