#ifndef MC_PLATFORM_FILES_BYTE_OUTPUT_H
#define MC_PLATFORM_FILES_BYTE_OUTPUT_H
#include "directory.h"
#include <stdio.h>

typedef struct mc_byte_output_error {
    int system_error, cleanup_system_error;
    bool consumed, cleanup_failed;
} mc_byte_output_error;

/* Concrete native operations only. Caller owns validation, path conversion,
 * allocation and lifetime. Open and setvbuf remain separate: legacy Writer
 * frees its temporary native path between them. No charset, buffering layer,
 * fsync, callback table, source File or Java exception is introduced here. */
bool mc_byte_output_open(const mc_file_native_char *, FILE **, mc_byte_output_error *);
bool mc_byte_output_unbuffered(FILE **, mc_byte_output_error *);
bool mc_byte_output_write(FILE *, const uint8_t *, size_t, size_t *, mc_byte_output_error *);
void mc_byte_output_flush(FILE *);
/* Always consumes/nulls this FILE occurrence before the real close, including
 * error. A reported failure never grants permission to close a reused handle. */
bool mc_byte_output_close(FILE **, mc_byte_output_error *);

typedef enum mc_byte_output_path_kind {
    MC_BYTE_OUTPUT_PATH_ALLOWED,
    MC_BYTE_OUTPUT_PATH_SPECIAL
} mc_byte_output_path_kind;

/* Source-FOS native regular-file profile only. Missing, regular and directory
 * reach actual open (directory keeps its real open failure). This is a racy
 * special-file safety preflight, not a Source File attribute or race exclusion.
 * Legacy Writer never calls it. */
mc_byte_output_path_kind mc_byte_output_path_profile(const mc_file_native_char *, int *);
#endif
