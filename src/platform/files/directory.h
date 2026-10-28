#ifndef MC_PLATFORM_FILES_DIRECTORY_H
#define MC_PLATFORM_FILES_DIRECTORY_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef _WIN32
#include <wchar.h>
typedef wchar_t mc_file_native_char;
#else
typedef char mc_file_native_char;
#endif
typedef enum mc_file_io_status {
    MC_FILE_IO_OK,
    MC_FILE_IO_INVALID_ARGUMENT,
    MC_FILE_IO_ALLOCATION_FAILED,
    MC_FILE_IO_LIMIT,
    MC_FILE_IO_UNSUPPORTED_FILENAME_ENCODING,
    MC_FILE_IO_UNSUPPORTED_PATH,
    MC_FILE_IO_NATIVE_ERROR,
    MC_FILE_IO_CREATION_FALSE
} mc_file_io_status;

typedef struct mc_file_io_error {
    mc_file_io_status status;
    int system_error;
    const char *operation;
    bool written; /* distinguishes unchanged old error from a completed result */
} mc_file_io_error;

typedef struct mc_file_io_state {
    size_t exists_calls, mkdir_calls, canonicalize_calls;
    int last_native_error;
    bool provider_frontier;         /* JDK final-attribute cases not completed by this transport */
    bool stop_at_provider_frontier; /* Source File only; legacy caller leaves false */
} mc_file_io_state;

mc_file_io_status mc_file_text_length(const char *input, size_t *length, mc_file_io_error *error);
/* Shared lexical algorithm. Buffers have at least length+1 units; no encoding,
 * allocation, I/O, dot removal or canonicalization. Returns output length. */
size_t mc_file_normalize_bytes(const char *input, size_t length, char *output);
size_t mc_file_normalize_units(const uint16_t *input, size_t length, uint16_t *output);
/* Shared real native primitive bodies, extracted from GameDirectory. No OS
 * home, shell/Ole, SDL, graphics, audio, default-directory selection or stronger
 * creation-failed policy. Existing native-provider limits remain explicit. */
mc_file_io_status mc_file_native_path(const char *utf8, mc_file_native_char **out,
                                      mc_file_io_error *error);
bool mc_file_native_exists(const mc_file_native_char *path, mc_file_io_state *state);
/* Exactly one mkdir/rename attempt; no parent creation or path reconstruction. */
bool mc_file_native_mkdir(const mc_file_native_char *path, mc_file_io_state *state);
bool mc_file_native_rename(const mc_file_native_char *from, const mc_file_native_char *to,
                           int *system_error);
mc_file_io_status mc_file_native_mkdirs(const mc_file_native_char *path, size_t depth,
                                        bool *created, mc_file_io_state *state,
                                        mc_file_io_error *error);

/* One independently reached native query per call; no cross-call snapshot.
 * Ordinary provider absence/failure gives value0. Native provider/policy faults
 * preserve out and use the existing typed by-value error. bool kinds use0/1. */
typedef enum mc_file_attribute_kind {
    MC_FILE_ATTRIBUTE_DIRECTORY,
    MC_FILE_ATTRIBUTE_REGULAR,
    MC_FILE_ATTRIBUTE_LENGTH,
    MC_FILE_ATTRIBUTE_LAST_MODIFIED
} mc_file_attribute_kind;

mc_file_io_status mc_file_native_attribute(const mc_file_native_char *path,
                                           mc_file_attribute_kind kind, int64_t *out,
                                           mc_file_io_state *state, mc_file_io_error *error);
#ifdef _WIN32
mc_file_io_status mc_file_native_wide_copy(const wchar_t *input, wchar_t **out,
                                           mc_file_io_error *error);
#endif
#endif
