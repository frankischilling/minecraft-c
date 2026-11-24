#include "ge.h"
#include "internal.h"

#include <string.h>

static bool receiver(mc_client_direct_float_buffer *buffer, mc_client_graphics_site site,
                     mc_client_graphics_error *error) {
    return buffer != NULL || mc_client_graphics_fail(error, MC_CLIENT_GRAPHICS_NULL_REFERENCE,
                                                     "actual FloatBuffer receiver", site,
                                                     false) == MC_CLIENT_GRAPHICS_OK;
}

bool mc_client_ge_float_clear(mc_client_direct_float_buffer *buffer, mc_client_graphics_site site,
                              mc_client_graphics_error *error) {
    if (!receiver(buffer, site, error))
        return false;
    buffer->position = 0;
    buffer->limit = buffer->capacity;
    return true;
}

bool mc_client_ge_float_flip(mc_client_direct_float_buffer *buffer, mc_client_graphics_site site,
                             mc_client_graphics_error *error) {
    if (!receiver(buffer, site, error))
        return false;
    buffer->limit = buffer->position;
    buffer->position = 0;
    return true;
}

bool mc_client_ge_float_put(mc_client_direct_float_buffer *buffer, float value,
                            mc_client_graphics_site site, mc_client_graphics_error *error) {
    void *storage;
    size_t remaining;
    if (!mc_client_ge_float_writable_remaining(buffer, &storage, &remaining, site, error))
        return false;
    if (remaining == 0) {
        mc_client_graphics_fail(error, MC_CLIENT_GRAPHICS_BUFFER_OVERFLOW,
                                "actual FloatBuffer.put overflow", site, false);
        return false;
    }
    memcpy(storage, &value, sizeof(value));
    ++buffer->position;
    return true;
}

bool mc_client_ge_float_limit(mc_client_direct_float_buffer *buffer, int32_t limit,
                              mc_client_graphics_site site, mc_client_graphics_error *error) {
    if (!receiver(buffer, site, error))
        return false;
    if (limit < 0 || (size_t)limit > buffer->capacity) {
        error->actual_index = limit;
        mc_client_graphics_fail(error, MC_CLIENT_GRAPHICS_SOURCE_ILLEGAL_ARGUMENT,
                                "actual FloatBuffer.limit argument", site, false);
        return false;
    }
    buffer->limit = (size_t)limit;
    if (buffer->position > buffer->limit)
        buffer->position = buffer->limit;
    return true;
}

bool mc_client_ge_float_writable_remaining(mc_client_direct_float_buffer *buffer, void **storage,
                                           size_t *remaining, mc_client_graphics_site site,
                                           mc_client_graphics_error *error) {
    if (storage == NULL || remaining == NULL) {
        mc_client_graphics_fail(error, MC_CLIENT_GRAPHICS_INVALID_ARGUMENT,
                                "native direct FloatBuffer span output", site, true);
        return false;
    }
    if (!receiver(buffer, site, error))
        return false;
    if (buffer->bytes == NULL || buffer->bytes->data == NULL || buffer->position > buffer->limit ||
        buffer->limit > buffer->capacity ||
        buffer->capacity > buffer->bytes->capacity / sizeof(float)) {
        mc_client_graphics_fail(error, MC_CLIENT_GRAPHICS_INVALID_ARGUMENT,
                                "native actual direct FloatBuffer storage invariant", site, true);
        return false;
    }
    *storage = buffer->bytes->data + buffer->position * sizeof(float);
    *remaining = buffer->limit - buffer->position;
    return true;
}

bool mc_client_ge_float_get_array(mc_client_direct_float_buffer *buffer, float *out, size_t length,
                                  mc_client_graphics_site site, mc_client_graphics_error *error) {
    if (!receiver(buffer, site, error))
        return false;
    if (out == NULL) {
        mc_client_graphics_fail(error, MC_CLIENT_GRAPHICS_NULL_REFERENCE,
                                "actual FloatBuffer.get array receiver", site, false);
        return false;
    }
    void *storage;
    size_t remaining;
    if (!mc_client_ge_float_writable_remaining(buffer, &storage, &remaining, site, error))
        return false;
    if (length > remaining) {
        mc_client_graphics_fail(error, MC_CLIENT_GRAPHICS_BUFFER_UNDERFLOW,
                                "actual FloatBuffer.get array underflow", site, false);
        return false;
    }
    memcpy(out, storage, length * sizeof(*out));
    buffer->position += length;
    return true;
}

bool mc_client_ge_float_inspect(const mc_client_direct_float_buffer *buffer,
                                mc_client_direct_buffer_state *out) {
    if (buffer == NULL || out == NULL || buffer->bytes == NULL)
        return false;
    *out = (mc_client_direct_buffer_state){
        buffer,        buffer->bytes->data, buffer->bytes,           buffer->position,
        buffer->limit, buffer->capacity,    buffer->bytes->capacity, true,
        true};
    return true;
}
