#include "sdk_queue.h"
#include "mc/core/java/java.h"

#include <stdlib.h>
#include <string.h>

struct mc_client_sdk_bytes {
    uint8_t *data;
    size_t capacity, position, limit;
};

struct mc_client_sdk_queue {
    int32_t event_size;
    mc_client_sdk_bytes *buffer;
};

static mc_client_sdk_queue_status failed(mc_client_sdk_queue_error *e,
                                         mc_client_sdk_queue_status status, const char *method,
                                         const char *descriptor, int32_t pc) {
    if (e != NULL)
        *e = (mc_client_sdk_queue_error){status, method, descriptor, pc};
    return status;
}

mc_client_sdk_queue_status mc_client_sdk_bytes_create(size_t capacity, mc_client_sdk_bytes **out) {
    if (out == NULL)
        return MC_CLIENT_SDK_QUEUE_INVALID_ARGUMENT;
    if (capacity > 16u * 1024u * 1024u)
        return MC_CLIENT_SDK_QUEUE_LIMIT;
    mc_client_sdk_bytes *b = calloc(1, sizeof(*b));
    if (b == NULL)
        return MC_CLIENT_SDK_QUEUE_ALLOCATION_FAILED;
    b->data = calloc(capacity == 0 ? 1 : capacity, 1);
    if (b->data == NULL) {
        free(b);
        return MC_CLIENT_SDK_QUEUE_ALLOCATION_FAILED;
    }
    b->capacity = b->limit = capacity;
    *out = b;
    return MC_CLIENT_SDK_QUEUE_OK;
}

void mc_client_sdk_bytes_free(mc_client_sdk_bytes **slot) {
    if (slot != NULL && *slot != NULL) {
        free((*slot)->data);
        free(*slot);
        *slot = NULL;
    }
}

bool mc_client_sdk_bytes_inspect(const mc_client_sdk_bytes *b, mc_client_sdk_bytes_state *out) {
    if (b == NULL || out == NULL)
        return false;
    *out = (mc_client_sdk_bytes_state){b->data, b->capacity, b->position, b->limit};
    return true;
}

void mc_client_sdk_bytes_clear(mc_client_sdk_bytes *b) {
    b->position = 0;
    b->limit = b->capacity;
}

void mc_client_sdk_bytes_flip(mc_client_sdk_bytes *b) {
    b->limit = b->position;
    b->position = 0;
}

void mc_client_sdk_bytes_compact(mc_client_sdk_bytes *b) {
    const size_t remaining = b->limit - b->position;
    memmove(b->data, b->data + b->position, remaining);
    b->position = remaining;
    b->limit = b->capacity;
}

bool mc_client_sdk_bytes_limit(mc_client_sdk_bytes *b, size_t limit) {
    if (b == NULL || limit > b->capacity)
        return false;
    b->limit = limit;
    if (b->position > limit)
        b->position = limit;
    return true;
}

bool mc_client_sdk_bytes_write(mc_client_sdk_bytes *b, const uint8_t *in, size_t count) {
    if (b == NULL || (in == NULL && count != 0) || count > b->limit - b->position)
        return false;
    if (count != 0)
        memmove(b->data + b->position, in, count);
    b->position += count;
    return true;
}

bool mc_client_sdk_bytes_position(mc_client_sdk_bytes *b, size_t position) {
    if (b == NULL || position > b->limit)
        return false;
    b->position = position;
    return true;
}

bool mc_client_sdk_bytes_read(mc_client_sdk_bytes *b, uint8_t *out, size_t count) {
    if (b == NULL || (out == NULL && count != 0) || count > b->limit - b->position)
        return false;
    if (count != 0)
        memcpy(out, b->data + b->position, count);
    b->position += count;
    return true;
}

size_t mc_client_sdk_bytes_remaining(const mc_client_sdk_bytes *b) {
    return b == NULL ? 0 : b->limit - b->position;
}

bool mc_client_sdk_bytes_set(mc_client_sdk_bytes *b, size_t index, uint8_t value) {
    if (b == NULL || index >= b->limit)
        return false;
    b->data[index] = value;
    return true;
}

mc_client_sdk_queue_status mc_client_sdk_queue_create(int32_t size, mc_client_sdk_queue **out,
                                                      mc_client_sdk_queue_error *e) {
    if (e != NULL)
        *e = (mc_client_sdk_queue_error){0};
    if (out == NULL)
        return failed(e, MC_CLIENT_SDK_QUEUE_INVALID_ARGUMENT, "<init>", "(I)V", -1);
    mc_client_sdk_queue *q = calloc(1, sizeof(*q));
    if (q == NULL)
        return failed(e, MC_CLIENT_SDK_QUEUE_ALLOCATION_FAILED, "<init>", "(I)V", -1);
    q->event_size = size;                                /* actual event_size6 before allocate15 */
    const int32_t capacity = mc_java_i32_mul(200, size); /* actual imul14, including wrap */
    if (capacity < 0) {
        free(q);
        return failed(e, MC_CLIENT_SDK_QUEUE_SOURCE_NEGATIVE_CAPACITY, "<init>", "(I)V", 15);
    }
    const mc_client_sdk_queue_status status =
        mc_client_sdk_bytes_create((size_t)capacity, &q->buffer);
    if (status != MC_CLIENT_SDK_QUEUE_OK) {
        free(q);
        return failed(e, status, "<init>", "(I)V", 15);
    }
    *out = q;
    return MC_CLIENT_SDK_QUEUE_OK;
}

void mc_client_sdk_queue_free(mc_client_sdk_queue **slot) {
    if (slot != NULL && *slot != NULL) {
        mc_client_sdk_bytes_free(&(*slot)->buffer);
        free(*slot);
        *slot = NULL;
    }
}

mc_client_sdk_queue_status mc_client_sdk_queue_put(mc_client_sdk_queue *q, mc_client_sdk_bytes *in,
                                                   bool *result, mc_client_sdk_queue_error *e) {
    if (e != NULL)
        *e = (mc_client_sdk_queue_error){0};
    if (q == NULL || in == NULL || result == NULL || q->buffer == in)
        return failed(e, MC_CLIENT_SDK_QUEUE_INVALID_ARGUMENT, "putEvent",
                      "(Ljava/nio/ByteBuffer;)Z", -1);
    if ((int32_t)mc_client_sdk_bytes_remaining(in) != q->event_size)
        return failed(e, MC_CLIENT_SDK_QUEUE_SOURCE_EVENT_SIZE, "putEvent",
                      "(Ljava/nio/ByteBuffer;)Z", 11);
    if (mc_client_sdk_bytes_remaining(q->buffer) < (size_t)q->event_size) {
        *result = false; /* actual return78, no put/position change */
        return MC_CLIENT_SDK_QUEUE_OK;
    }
    memcpy(q->buffer->data + q->buffer->position, in->data + in->position, (size_t)q->event_size);
    q->buffer->position += (size_t)q->event_size;
    in->position += (size_t)q->event_size;
    *result = true;
    return MC_CLIENT_SDK_QUEUE_OK;
}

mc_client_sdk_queue_status mc_client_sdk_queue_copy(mc_client_sdk_queue *q,
                                                    mc_client_sdk_bytes *out,
                                                    mc_client_sdk_queue_error *e) {
    if (e != NULL)
        *e = (mc_client_sdk_queue_error){0};
    if (q == NULL || out == NULL || q->buffer == out)
        return failed(e, MC_CLIENT_SDK_QUEUE_INVALID_ARGUMENT, "copyEvents",
                      "(Ljava/nio/ByteBuffer;)V", -1);
    mc_client_sdk_bytes *b = q->buffer;
    mc_client_sdk_bytes_flip(b); /* flip4 */
    const size_t old_limit = b->limit;
    size_t count = mc_client_sdk_bytes_remaining(b);
    if (count > mc_client_sdk_bytes_remaining(out)) {
        count = mc_client_sdk_bytes_remaining(out);
        b->limit = b->position + count; /* limit46 */
    }
    memcpy(out->data + out->position, b->data + b->position, count); /* put55 */
    out->position += count;
    b->position += count;
    b->limit = old_limit;           /* restore64 */
    mc_client_sdk_bytes_compact(b); /* compact72 */
    return MC_CLIENT_SDK_QUEUE_OK;
}

void mc_client_sdk_queue_clear(mc_client_sdk_queue *q) {
    mc_client_sdk_bytes_clear(q->buffer);
}

bool mc_client_sdk_queue_inspect(const mc_client_sdk_queue *q, mc_client_sdk_bytes_state *out) {
    return q != NULL && mc_client_sdk_bytes_inspect(q->buffer, out);
}
