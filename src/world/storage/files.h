#ifndef MC_WORLD_STORAGE_FILES_H
#define MC_WORLD_STORAGE_FILES_H

#include "mc/world/storage/status.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MC_WS_PATH_MAX_BYTES 32768u
#define MC_WS_FILE_MAX_BYTES UINT32_C(0x7ffff000)

typedef struct mc_ws_file_bytes {
    uint8_t *data;
    size_t length;
} mc_ws_file_bytes;

/* Private native-file helpers, without world/schema/session ownership. Paths
 * are nonempty NUL-terminated UTF-8 strings, at most PATH_MAX_BYTES excluding
 * NUL. Invalid UTF-8 is rejected, without locale conversion or normalization.
 * Windows uses strict UTF-8 -> UTF-16 and wide CRT/native APIs; converted paths
 * including NUL must fit 32768 UTF-16 units. Native component/total limits still
 * apply. POSIX uses the UTF-8 filename bytes directly.
 *
 * Grant exclusive access to paths/files throughout each call. These helpers
 * provide no containment, symlink/reparse-point protection or race exclusion.
 * Read/write/replace operate on ordinary regular files; exists also accepts
 * directories. Outputs remain unchanged on
 * failure; error is optional, must be disjoint from inputs/outputs, and receives
 * a static operation diagnostic. Successful calls reset error to OK/NULL/0.
 * system_error is errno (also Windows CRT errno) for operations ending .errno,
 * GetLastError for .win32, and zero for validation/resource failures.
 * Allocator/stdio implementation overhead is outside explicit byte bounds.
 * Closing still occurs after a prior failure; the first error is retained.
 * An exclusive-write cleanup failure supersedes its earlier I/O error. */

/* Joins a root and a nonempty native-relative leaf, inserting one separator if
 * needed. Does not normalize components or reject '..'; the owning layer must
 * validate its names. The returned UTF-8 string is owned and released by free.
 * out is required and must not overlap either path. */
mc_world_storage_status mc_ws_path_join(const char *root, const char *leaf, char **out,
                                        mc_world_storage_error *error);

/* Creates missing directory components; existing components must be directories.
 * Earlier newly created directories remain after a later failure. Windows drive
 * roots and UNC share roots are verified, not created. Drive-relative paths and
 * extended/device namespace prefixes are explicitly unsupported by mkdirs;
 * UNC parsing is supported but requires separate network-environment testing.
 * This function never removes or replaces a preexisting path. */
mc_world_storage_status mc_ws_mkdirs(const char *path, mc_world_storage_error *error);

/* Reads the full regular file, checking size and the final EOF/stat for observed
 * growth/truncation. max_bytes must not exceed FILE_MAX_BYTES. The exact result
 * allocation is <= max_bytes, plus bounded temporary path storage. Empty files
 * return {NULL,0}. out is required; free any prior owned bytes before reuse.
 * Detectable concurrent changes fail; equal-size edits remain excluded by the
 * caller's exclusive-access precondition. No streaming, parsing or I/O retries. */
mc_world_storage_status mc_ws_read_file(const char *path, size_t max_bytes, mc_ws_file_bytes *out,
                                        mc_world_storage_error *error);
void mc_ws_free_file_bytes(mc_ws_file_bytes *bytes);

/* Reads exactly the first length bytes of a regular file, ignoring all trailing
 * bytes without allocating or reading the rest. length is bounded by
 * FILE_MAX_BYTES; NULL output is valid only for zero length. The file must still
 * exist and be regular for a zero-length request. Short reads and close failures
 * leave the entire output unchanged. Uses at most length temporary data bytes
 * plus bounded path storage. Output must be disjoint from path and error. */
mc_world_storage_status mc_ws_read_prefix(const char *path, uint8_t *output, size_t length,
                                          mc_world_storage_error *error);

/* Binary write, checking write/flush/close. NULL bytes is valid only for zero
 * length; length above FILE_MAX_BYTES returns LIMIT. exclusive uses C11 wbx
 * (wide CRT on Windows), with no fallback to an overwriting mode. Open EEXIST
 * returns IO_ERROR with errno, and never removes the preexisting path.
 * If exclusive open succeeded but a later step fails, only that newly created
 * file is removed; cleanup failure is reported. Nonexclusive writes may leave
 * a partial/truncated target on failure. No fsync/durability promise. */
mc_world_storage_status mc_ws_write_file(const char *path, const uint8_t *bytes, size_t length,
                                         bool exclusive, mc_world_storage_error *error);

/* Checked overwrite rename of regular files: POSIX rename (errno), Windows
 * MoveFileExW(MOVEFILE_REPLACE_EXISTING) (GetLastError). No cross-volume copy,
 * temporary file, fsync or recovery behavior is added. */
mc_world_storage_status mc_ws_replace_file(const char *source, const char *target,
                                           mc_world_storage_error *error);

/* Removes a file, never a directory. Missing yields MISSING unless missing_ok;
 * path_exists instead reports missing as successful false. Native access and
 * malformed-parent failures remain errors. out is required and unchanged on
 * failure. Both operations follow native symlink behavior. */
mc_world_storage_status mc_ws_remove_file(const char *path, bool missing_ok,
                                          mc_world_storage_error *error);
mc_world_storage_status mc_ws_path_exists(const char *path, bool *out,
                                          mc_world_storage_error *error);

#endif
