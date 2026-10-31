#ifndef MC_CLIENT_DISPLAY_SDK_QUEUE_H
#define MC_CLIENT_DISPLAY_SDK_QUEUE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct mc_client_sdk_bytes mc_client_sdk_bytes;
typedef struct mc_client_sdk_queue mc_client_sdk_queue;

typedef enum mc_client_sdk_queue_status {
    MC_CLIENT_SDK_QUEUE_OK,
    MC_CLIENT_SDK_QUEUE_INVALID_ARGUMENT,
    MC_CLIENT_SDK_QUEUE_ALLOCATION_FAILED,
    MC_CLIENT_SDK_QUEUE_LIMIT,
    MC_CLIENT_SDK_QUEUE_SOURCE_EVENT_SIZE,
    MC_CLIENT_SDK_QUEUE_SOURCE_BUFFER_OVERFLOW,
    MC_CLIENT_SDK_QUEUE_SOURCE_BUFFER_UNDERFLOW,
    MC_CLIENT_SDK_QUEUE_SOURCE_NEGATIVE_CAPACITY
} mc_client_sdk_queue_status;

typedef struct mc_client_sdk_queue_error {
    mc_client_sdk_queue_status status;
    const char *method, *descriptor;
    int32_t pc;
} mc_client_sdk_queue_error;

typedef struct mc_client_sdk_bytes_state {
    const uint8_t *data;
    size_t capacity, position, limit;
} mc_client_sdk_bytes_state;

/* Actual heap ByteBuffers, not ge's separately owned direct-buffer namespace.
 * Native finite admission is distinct from source NIO failures. */
mc_client_sdk_queue_status mc_client_sdk_bytes_create(size_t, mc_client_sdk_bytes **);
void mc_client_sdk_bytes_free(mc_client_sdk_bytes **);
bool mc_client_sdk_bytes_inspect(const mc_client_sdk_bytes *, mc_client_sdk_bytes_state *);
void mc_client_sdk_bytes_clear(mc_client_sdk_bytes *);
void mc_client_sdk_bytes_flip(mc_client_sdk_bytes *);
void mc_client_sdk_bytes_compact(mc_client_sdk_bytes *);
bool mc_client_sdk_bytes_limit(mc_client_sdk_bytes *, size_t);
bool mc_client_sdk_bytes_position(mc_client_sdk_bytes *, size_t);
bool mc_client_sdk_bytes_write(mc_client_sdk_bytes *, const uint8_t *, size_t);
bool mc_client_sdk_bytes_read(mc_client_sdk_bytes *, uint8_t *, size_t);
bool mc_client_sdk_bytes_set(mc_client_sdk_bytes *, size_t index, uint8_t);
size_t mc_client_sdk_bytes_remaining(const mc_client_sdk_bytes *);

/* Pinned EventQueue(I): exactly 200 records, not a guessed native FIFO bound.
 * putEvent validates incoming remaining == event_size before checking space;
 * a full queue succeeds with source_result=false and consumes no input.
 * copyEvents performs exact flip/min-limit/put/restore-limit/compact, including
 * a partial record when destination remaining is not aligned to event_size. */
mc_client_sdk_queue_status mc_client_sdk_queue_create(int32_t event_size, mc_client_sdk_queue **out,
                                                      mc_client_sdk_queue_error *);
void mc_client_sdk_queue_free(mc_client_sdk_queue **);
mc_client_sdk_queue_status mc_client_sdk_queue_put(mc_client_sdk_queue *, mc_client_sdk_bytes *,
                                                   bool *source_result,
                                                   mc_client_sdk_queue_error *);
mc_client_sdk_queue_status mc_client_sdk_queue_copy(mc_client_sdk_queue *, mc_client_sdk_bytes *,
                                                    mc_client_sdk_queue_error *);
void mc_client_sdk_queue_clear(mc_client_sdk_queue *);
bool mc_client_sdk_queue_inspect(const mc_client_sdk_queue *, mc_client_sdk_bytes_state *);

#endif
