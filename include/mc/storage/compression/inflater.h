#ifndef MC_STORAGE_COMPRESSION_INFLATER_H
#define MC_STORAGE_COMPRESSION_INFLATER_H
#include "mc/storage/compression/compression.h"
#include <stdbool.h>
typedef struct mc_raw_inflater mc_raw_inflater;

typedef enum mc_raw_inflater_phase {
    MC_RAW_INFLATER_RAW,
    MC_RAW_INFLATER_READY,
    MC_RAW_INFLATER_FAILED,
    MC_RAW_INFLATER_RETIRING
} mc_raw_inflater_phase;

typedef enum mc_raw_inflater_status {
    MC_RAW_INFLATER_OK,
    MC_RAW_INFLATER_ARGUMENT,
    MC_RAW_INFLATER_STATE,
    MC_RAW_INFLATER_BUSY,
    MC_RAW_INFLATER_LIMIT,
    MC_RAW_INFLATER_ALLOCATION,
    MC_RAW_INFLATER_DATA_ERROR,
    MC_RAW_INFLATER_BACKEND
} mc_raw_inflater_status;

typedef struct mc_raw_inflater_error {
    mc_raw_inflater_status status;
    mc_compression_status compression;
    const char *operation;
    int backend_result;
    char message[128];
    bool message_truncated;
} mc_raw_inflater_error;

typedef struct mc_raw_inflater_state {
    mc_raw_inflater_phase phase;
    bool active, armed, finished, needs_dictionary;
    size_t remaining, live_allocation_bytes;
    uint64_t bytes_read, bytes_written;
} mc_raw_inflater_state;

/* Separate native RAW and genuine inflateInit2(-15). Budget includes descriptor
 * and SAME legacy zlib allocation headers/state. Failed construction is owned. */
mc_raw_inflater_status mc_raw_inflater_allocate(size_t maximum_bytes, mc_raw_inflater **out,
                                                mc_raw_inflater_error *error);
mc_raw_inflater_status mc_raw_inflater_construct(mc_raw_inflater *raw,
                                                 mc_raw_inflater_error *error);
/* Actual new Inflater() uses the same owner/backend with nowrap=false. */
mc_raw_inflater_status mc_raw_inflater_construct_wrapped(mc_raw_inflater *,
                                                         mc_raw_inflater_error *);
/* Input is borrowed until consumed/reset/end; replacing unread input fails.
 * Pull records actual native output even on data error, never a successful Java
 * read acknowledgement on error. No copying/read-ahead/whole-file buffer. */
mc_raw_inflater_status mc_raw_inflater_set_input(mc_raw_inflater *owner, const uint8_t *data,
                                                 size_t length, mc_raw_inflater_error *error);
mc_raw_inflater_status mc_raw_inflater_pull(mc_raw_inflater *owner, uint8_t *out, size_t capacity,
                                            size_t *written, mc_raw_inflater_error *error);
mc_raw_inflater_status mc_raw_inflater_reset(mc_raw_inflater *owner, mc_raw_inflater_error *error);
mc_raw_inflater_status mc_raw_inflater_end(mc_raw_inflater *owner, mc_raw_inflater_error *error);
mc_raw_inflater_status mc_raw_inflater_state_read(const mc_raw_inflater *owner,
                                                  mc_raw_inflater_state *out,
                                                  mc_raw_inflater_error *error);
/* Success consumes/nulls occurrence. BUSY/error preserves it and completed
 * effects. An issued end loses its close right before zlib; retry never ends
 * stale state or double frees. */
mc_raw_inflater_status mc_raw_inflater_release(mc_raw_inflater **occurrence,
                                               mc_raw_inflater_error *error);
#endif
