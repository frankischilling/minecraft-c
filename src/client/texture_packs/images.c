/* Proposed actual pack adapter to the single scoped ImageIO byte-input core.
 * Apply only with its reviewed complete source cohort; e remains untouched. */
#include "../images/image_io.h"
#include "internal.h"

typedef struct pack_image_input {
    mc_client_texture_stream *stream;
    mc_texture_pack_error *error;
    bool child_failed;
} pack_image_input;

static mc_client_image_io_status read_pack(void *context, uint8_t *bytes, size_t capacity,
                                           size_t *count, mc_client_image_io_error *error) {
    pack_image_input *input = context;
    const mc_texture_pack_status status =
        mc_client_texture_stream_read(input->stream, bytes, capacity, count, input->error);
    if (status == MC_TEXTURE_PACK_OK)
        return MC_CLIENT_IMAGE_IO_OK;
    input->child_failed = true;
    return mc_client_image_io_fail(
        error,
        status == MC_TEXTURE_PACK_SOURCE_IO            ? MC_CLIENT_IMAGE_IO_SOURCE_IO
        : status == MC_TEXTURE_PACK_LIMIT              ? MC_CLIENT_IMAGE_IO_LIMIT
        : status == MC_TEXTURE_PACK_ALLOCATION_FAILED  ? MC_CLIENT_IMAGE_IO_ALLOCATION_FAILED
        : status == MC_TEXTURE_PACK_BUSY               ? MC_CLIENT_IMAGE_IO_BUSY
        : status == MC_TEXTURE_PACK_UNSUPPORTED_NATIVE ? MC_CLIENT_IMAGE_IO_UNSUPPORTED_NATIVE
                                                       : MC_CLIENT_IMAGE_IO_PROVIDER_FAILED,
        input->error->operation);
}

mc_texture_pack_status texture_pack_image_read(mc_client_texture_stream *stream,
                                               mc_client_argb_image **out,
                                               mc_texture_pack_error *error) {
    pack_image_input context = {stream, error, false};
    const mc_client_image_io_input input = {&context, read_pack, stream == NULL};
    const mc_client_image_io_limits limits =
        stream == NULL
            ? (mc_client_image_io_limits){0}
            : (mc_client_image_io_limits){stream->pack->options.max_encoded_image_bytes,
                                          stream->pack->options.max_decoded_image_bytes,
                                          stream->pack->options.max_stream_allocation_bytes};
    mc_client_image_io_error cause = {0};
    const mc_client_image_io_status status = mc_client_image_io_read(&input, &limits, out, &cause);
    if (status == MC_CLIENT_IMAGE_IO_OK)
        return MC_TEXTURE_PACK_OK;
    if (context.child_failed)
        return error->status; /* exact actual child error retained */
    const mc_texture_pack_status mapped =
        status == MC_CLIENT_IMAGE_IO_SOURCE_IO           ? MC_TEXTURE_PACK_SOURCE_IO
        : status == MC_CLIENT_IMAGE_IO_LIMIT             ? MC_TEXTURE_PACK_LIMIT
        : status == MC_CLIENT_IMAGE_IO_ALLOCATION_FAILED ? MC_TEXTURE_PACK_ALLOCATION_FAILED
        : status == MC_CLIENT_IMAGE_IO_SOURCE_ILLEGAL_ARGUMENT
            ? MC_TEXTURE_PACK_SOURCE_ILLEGAL_ARGUMENT
        : status == MC_CLIENT_IMAGE_IO_INVALID_ARGUMENT   ? MC_TEXTURE_PACK_INVALID_ARGUMENT
        : status == MC_CLIENT_IMAGE_IO_BUSY               ? MC_TEXTURE_PACK_BUSY
        : status == MC_CLIENT_IMAGE_IO_UNSUPPORTED_NATIVE ? MC_TEXTURE_PACK_UNSUPPORTED_NATIVE
                                                          : MC_TEXTURE_PACK_PROVIDER_FAILED;
    texture_pack_fail(error, mapped, cause.operation, NULL, NULL, NULL, 0, 0);
    error->zlib_error = cause.zlib_error;
    return mapped;
}
