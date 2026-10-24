#ifndef MC_PLATFORM_TEXT_STDOUT_H
#define MC_PLATFORM_TEXT_STDOUT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct mc_text_stdout {
    intptr_t handle;
} mc_text_stdout;

typedef enum mc_text_stdout_status {
    MC_TEXT_STDOUT_OK,
    MC_TEXT_STDOUT_INVALID_ARGUMENT,
    MC_TEXT_STDOUT_NATIVE_FAILURE
} mc_text_stdout_status;

/* Capture the default process stdout handle without owning or duplicating it.
 * A missing/closed handle is retained for the later real write attempt. */
mc_text_stdout_status mc_text_stdout_capture(mc_text_stdout *out);

/* Write at most 64 ASCII bytes, followed by the platform line separator,
 * without allocation or CRT text translation. ascii may be NULL only when
 * length is zero. On OK, io_failed reports a suppressed underlying I/O error.
 * On a native error, outputs are unchanged; any bytes already written remain.
 *
 * This models the reached default PrintStream ASCII println operation only.
 * The caller formats the complete line and retains any sticky error state.
 * Concurrent writes/handle replacement, System.setOut, arbitrary encodings,
 * Unicode and Java thread-interruption semantics are outside this interface. */
mc_text_stdout_status mc_text_stdout_println(const mc_text_stdout *stream, const char *ascii,
                                             size_t length, bool *io_failed);

#endif
