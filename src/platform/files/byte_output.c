#include "byte_output.h"
#include <errno.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <share.h>
#endif

bool mc_byte_output_open(const mc_file_native_char *path, FILE **out, mc_byte_output_error *error) {
    *error = (mc_byte_output_error){0};
    errno = 0;
#ifdef _WIN32
    FILE *file = _wfsopen(path, L"wb", _SH_DENYNO);
#else
    FILE *file = fopen(path, "wb");
#endif
    const int saved = errno;
    if (file == NULL) {
        error->system_error = saved;
        return false;
    }
    *out = file;
    return true;
}

bool mc_byte_output_unbuffered(FILE **file, mc_byte_output_error *error) {
    *error = (mc_byte_output_error){0};
    if (setvbuf(*file, NULL, _IONBF, 0) == 0)
        return true;
    const int saved = errno;
    FILE *closing = *file;
    *file = NULL;
    errno = 0;
    const bool failed = fclose(closing) != 0;
    const int close_error = errno;
    *error = (mc_byte_output_error){saved, failed ? close_error : 0, true, failed};
    return false;
}

bool mc_byte_output_write(FILE *file, const uint8_t *bytes, size_t length, size_t *committed,
                          mc_byte_output_error *error) {
    *error = (mc_byte_output_error){0};
    errno = 0;
    *committed = fwrite(bytes, 1, length, file);
    const int saved = errno;
    if (*committed != length && (ferror(file) || *committed == 0)) {
        error->system_error = saved;
        return false;
    }
    return true;
}

void mc_byte_output_flush(FILE *file) {
    /* Actual OutputStream.flush is empty. Unbuffered admission guarantees that
     * there are no extra C bytes to flush. Also valid for an already closed
     * concrete source FOS; legacy Writer keeps its own closed check. */
    (void)file;
}

bool mc_byte_output_close(FILE **slot, mc_byte_output_error *error) {
    *error = (mc_byte_output_error){0};
    FILE *file = *slot;
    *slot = NULL;
    if (file == NULL)
        return true;
    error->consumed = true;
    errno = 0;
    const int code = fclose(file);
    error->system_error = code == 0 ? 0 : errno;
    return code == 0;
}

mc_byte_output_path_kind mc_byte_output_path_profile(const mc_file_native_char *path, int *code) {
    *code = 0;
    errno = 0;
#ifdef _WIN32
    struct _stat64 state;
    const int result = _wstat64(path, &state);
    const bool allowed = result == 0 && ((state.st_mode & _S_IFMT) == _S_IFREG ||
                                         (state.st_mode & _S_IFMT) == _S_IFDIR);
#else
    struct stat state;
    const int result = stat(path, &state);
    const bool allowed = result == 0 && (S_ISREG(state.st_mode) || S_ISDIR(state.st_mode));
#endif
    if (result == 0)
        return allowed ? MC_BYTE_OUTPUT_PATH_ALLOWED : MC_BYTE_OUTPUT_PATH_SPECIAL;
    *code = errno;
    /* ENOTDIR, permission and other errors still reach real fopen, retaining
     * its own error. Only a known special object is rejected by this profile. */
    return MC_BYTE_OUTPUT_PATH_ALLOWED;
}
