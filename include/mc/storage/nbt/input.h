#ifndef MC_STORAGE_NBT_INPUT_H
#define MC_STORAGE_NBT_INPUT_H
#include "mc/storage/nbt/nbt.h"
#include <stdbool.h>
typedef struct mc_nbt_stream mc_nbt_stream;

typedef struct mc_nbt_input_result {
    size_t count;
    bool eof;
} mc_nbt_input_result;

typedef struct mc_nbt_input_error {
    int transport_status, system_error;
    bool source_exception;
    const char *operation;
    char message[128];
    size_t native_written_on_error;
} mc_nbt_input_error;

typedef enum mc_nbt_input_status { MC_NBT_INPUT_OK, MC_NBT_INPUT_FAILED } mc_nbt_input_status;

typedef struct mc_nbt_byte_input {
    void *context;
    mc_nbt_input_status (*read_one)(void *context, int *out, mc_nbt_input_error *error);
    mc_nbt_input_status (*read)(void *context, uint8_t *bytes, size_t capacity,
                                mc_nbt_input_result *out, mc_nbt_input_error *error);
    mc_nbt_input_status (*close)(void *context, mc_nbt_input_error *error);
} mc_nbt_byte_input;

typedef enum mc_nbt_stream_phase {
    MC_NBT_STREAM_RAW,
    MC_NBT_STREAM_CONSTRUCTING,
    MC_NBT_STREAM_READY,
    MC_NBT_STREAM_PARSING,
    MC_NBT_STREAM_COMPLETE,
    MC_NBT_STREAM_FAILED,
    MC_NBT_STREAM_RETIRING
} mc_nbt_stream_phase;

typedef enum mc_nbt_source_fault {
    MC_NBT_SOURCE_NONE,
    MC_NBT_SOURCE_EOF,
    MC_NBT_SOURCE_UTF,
    MC_NBT_SOURCE_NEGATIVE_ARRAY,
    MC_NBT_SOURCE_NULL_TAG,
    MC_NBT_SOURCE_ROOT,
    MC_NBT_SOURCE_INPUT,
    MC_NBT_SOURCE_NULL_INPUT,
    MC_NBT_SOURCE_NULL_ARRAY
} mc_nbt_source_fault;

typedef struct mc_nbt_stream_error {
    mc_nbt_error parse;
    mc_nbt_input_error input;
    mc_nbt_source_fault source_fault;
    const char *source_class, *source_method;
    int source_pc;
    size_t utf_error_byte, utf_prefix_units;
    bool utf_partial_character;
} mc_nbt_stream_error;

typedef struct mc_nbt_stream_state {
    mc_nbt_stream_phase phase;
    bool active;
    size_t position, read_calls, close_calls, owner_bytes;
    const void *source_in;
    const uint8_t *bytearr, *read_buffer;
    const uint16_t *chararr;
    size_t bytearr_capacity, chararr_capacity;
    int last_data_input_store;
    const char *last_data_input_store_class;
    const mc_nbt_document *candidate;
    const mc_nbt_tag *last_tag, *pending_tag;
    const void *last_collection;
    size_t last_collection_count;
    const void *last_array;
    size_t last_array_length;
    int last_tag_store;
    const char *last_tag_store_class;
    int last_collection_operation_pc;
} mc_nbt_stream_state;

/* Raw identity is allocated BEFORE GZIP construction in as0. Construct after
 * real GZIP success/as12: Filter.in then byte80/char80/byte8 in JDK10/18/26
 * publication order. Byte port borrows the actual retained concrete input;
 * caller must keep it alive through final reader retirement. No callback can
 * supply tags, Info or completed children. Failed scratch/document/candidates
 * remain owned. State candidate is a CONST partial inspection view, never an
 * owned/completed document for adoption, mutation or release. Only a successful
 * parse output supplies a completed independent owned occurrence. Existing
 * memory parser/document/error layouts stay unchanged. */
mc_nbt_status mc_nbt_stream_allocate(size_t maximum_owner_bytes, mc_nbt_stream **out,
                                     mc_nbt_stream_error *error);
mc_nbt_status mc_nbt_stream_construct(mc_nbt_stream *raw, const mc_nbt_byte_input *actual_input,
                                      const mc_nbt_limits *limits, mc_nbt_stream_error *error);
/* Actual DataInputStream.readLong over the SAME constructed input/scratch.
 * Uses the parser's canonical readFully/read_buffer8 kernel without a tag,
 * document, memory input or decode copy. READY/exclusive only. A primitive
 * return or read failure leaves the constructed stream READY and retains its
 * actual partial intake/error; readLong does not poison a DataInputStream.
 * Output is unchanged on failure; source close remains a separate caller step. */
mc_nbt_status mc_nbt_stream_read_long(mc_nbt_stream *, int64_t *, mc_nbt_stream_error *);
/* Actual DataInputStream.read(byte[]): capture in, then arraylength7 before
 * nullable in.read8. One real bulk delegation even for nonnull length0; EOF
 * returns -1 and no readFully/tag/document is entered. Caller owns writable
 * declared array storage, with is_null distinguishing a Java null reference.
 * Counts/errors are retained; failure preserves out and READY like readLong. */
mc_nbt_status mc_nbt_stream_read_array(mc_nbt_stream *, uint8_t *, int32_t, bool is_null, int32_t *,
                                       mc_nbt_stream_error *);
mc_nbt_status mc_nbt_stream_parse(mc_nbt_stream *stream, bool require_compound,
                                  mc_nbt_document **out, mc_nbt_stream_error *error);
mc_nbt_status mc_nbt_stream_source_close(mc_nbt_stream *stream, mc_nbt_stream_error *error);
mc_nbt_status mc_nbt_stream_state_read(const mc_nbt_stream *stream, mc_nbt_stream_state *out,
                                       mc_nbt_stream_error *error);
mc_nbt_status mc_nbt_stream_release(mc_nbt_stream **occurrence, mc_nbt_stream_error *error);
#endif
