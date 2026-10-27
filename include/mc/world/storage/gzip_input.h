#ifndef MC_WORLD_STORAGE_GZIP_INPUT_H
#define MC_WORLD_STORAGE_GZIP_INPUT_H
#include "mc/storage/compression/inflater.h"
#include "mc/storage/nbt/input.h"
#include "mc/world/storage/java_file.h"
typedef struct mc_gzip_input mc_gzip_input;

typedef enum mc_gzip_phase {
    MC_GZIP_RAW,
    MC_GZIP_CONSTRUCTING,
    MC_GZIP_READY,
    MC_GZIP_FAILED,
    MC_GZIP_RETIRING
} mc_gzip_phase;

typedef enum mc_gzip_status {
    MC_GZIP_OK,
    MC_GZIP_ARGUMENT,
    MC_GZIP_STATE,
    MC_GZIP_BUSY,
    MC_GZIP_LIMIT,
    MC_GZIP_ALLOCATION,
    MC_GZIP_SOURCE_IO,
    MC_GZIP_SOURCE_FORMAT,
    MC_GZIP_SOURCE_EOF,
    MC_GZIP_SOURCE_NULL,
    MC_GZIP_NATIVE_PROVIDER
} mc_gzip_status;

typedef struct mc_gzip_options {
    size_t maximum_owner_bytes, maximum_inflater_bytes, maximum_compressed_bytes,
        maximum_output_bytes, maximum_members;
} mc_gzip_options;

typedef struct mc_gzip_error {
    mc_gzip_status status;
    const char *operation, *source_class, *source_method;
    int source_pc;
    bool source_exception;
    char message[128];
    mc_java_file_error file;
    mc_raw_inflater_error inflater;
} mc_gzip_error;

typedef struct mc_gzip_state {
    mc_gzip_phase phase;
    size_t holds, owner_bytes, members, fill_length, read_calls;
    bool active, retiring, source_closed, super_closed, eos, reach_eof, uses_default_inflater;
    mc_java_file_stream *source_in, *pending_input;
    const mc_raw_inflater *source_inf, *pending_inflater;
    const uint8_t *single_byte, *skip_buffer, *fill_buffer, *tmp_buffer;
    const uint32_t *crc;
    uint64_t compressed_read, output_returned;
    size_t trailer_bytes_taken, last_native_written;
    int32_t source_fill_length;
    int last_store_pc;
    const char *last_store_class;
    bool ignored_next_header;
} mc_gzip_state;

mc_gzip_options mc_gzip_default_options(void);
mc_gzip_status mc_gzip_input_allocate(const mc_gzip_options *options, mc_gzip_input **out,
                                      mc_gzip_error *error);
mc_gzip_status mc_gzip_input_construct(mc_gzip_input *raw, mc_java_file_stream *actual_input,
                                       mc_gzip_error *error);
mc_gzip_status mc_gzip_input_retain(mc_gzip_input *input, mc_gzip_error *error);
/* Real incremental read: no drain/read-all. Source EOS is tested before the
 * inherited len0/array checks. Native output written before a zlib error is
 * reported in state/result only, NEVER acknowledged as a successful read. */
mc_gzip_status mc_gzip_input_read(mc_gzip_input *input, uint8_t *bytes, size_t capacity,
                                  mc_nbt_input_result *out, mc_gzip_error *error);
mc_gzip_status mc_gzip_input_read_byte(mc_gzip_input *input, int *out, mc_gzip_error *error);
mc_gzip_status mc_gzip_input_source_close(mc_gzip_input *input, mc_gzip_error *error);
mc_gzip_status mc_gzip_input_release(mc_gzip_input **occurrence, mc_gzip_error *error);
mc_gzip_status mc_gzip_input_state_read(const mc_gzip_input *input, mc_gzip_state *out,
                                        mc_gzip_error *error);
/* Borrows this actual owner; caller retains it through reader retirement. */
mc_nbt_byte_input mc_gzip_input_byte_port(mc_gzip_input *input);
/* Last read/construct cause and last close cause are independent: successful
 * finally-close never erases a preceding read failure. */
mc_gzip_status mc_gzip_input_last_error(const mc_gzip_input *input, mc_gzip_error *out);
mc_gzip_status mc_gzip_input_last_close_error(const mc_gzip_input *input, mc_gzip_error *out);
#endif
