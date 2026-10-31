#ifndef MC_CLIENT_RENDER_ENGINE_IMAGE_BUFFER_H
#define MC_CLIENT_RENDER_ENGINE_IMAGE_BUFFER_H

#include "mc/client/render_engine/render_engine.h"

typedef struct mc_client_image_buffer mc_client_image_buffer;

typedef enum mc_client_image_buffer_status {
    MC_CLIENT_IMAGE_BUFFER_OK,
    MC_CLIENT_IMAGE_BUFFER_INVALID_ARGUMENT,
    MC_CLIENT_IMAGE_BUFFER_INVALID_PHASE,
    MC_CLIENT_IMAGE_BUFFER_LIMIT,
    MC_CLIENT_IMAGE_BUFFER_ALLOCATION_FAILED,
    MC_CLIENT_IMAGE_BUFFER_BUSY,
    MC_CLIENT_IMAGE_BUFFER_IMAGE_FAILED
} mc_client_image_buffer_status;

typedef struct mc_client_image_buffer_error {
    mc_client_image_buffer_status status;
    const char *operation, *source_method, *source_descriptor;
    unsigned int source_pc;
    bool native_policy;
    mc_client_render_engine_status image;
} mc_client_image_buffer_error;

typedef struct mc_client_image_buffer_limits {
    /* Per canonical output image, including its real header and 8192 pixel
     * bytes. Zero admits no output. This is native policy, not Java OOME. */
    size_t max_image_bytes;
} mc_client_image_buffer_limits;

typedef struct mc_client_image_buffer_state {
    size_t native_holds;
    bool active;
    const uint32_t *a; /* Actual rr.a aliases this canonical image's pixels. */
    int32_t b, c;
    const mc_client_argb_image *array_owner, *pending_candidate;
    unsigned int last_source_pc;
    mc_client_image_buffer_error failure;
} mc_client_image_buffer_state;

mc_client_image_buffer_limits mc_client_image_buffer_default_limits(void);
/* Actual rr constructor calls Object only; fields start NULL/0/0. This cold
 * CPU owner does not activate Engine classes or touch a World, GL or network.
 * Native occurrence accounting is thread safe. Keep a live occurrence while
 * retaining/releasing another; final release must not overlap borrowed views.
 * A worker retains before start and releases during checked terminal cleanup. */
mc_client_image_buffer_status
mc_client_image_buffer_create(const mc_client_image_buffer_limits *limits,
                              mc_client_image_buffer **out, mc_client_image_buffer_error *error);
mc_client_image_buffer_status mc_client_image_buffer_retain(mc_client_image_buffer *owner,
                                                            mc_client_image_buffer_error *error);
mc_client_image_buffer_status mc_client_image_buffer_release(mc_client_image_buffer **slot,
                                                             mc_client_image_buffer_error *error);

/* Actual rr.a(BufferedImage): NULL input returns owned_out=NULL without
 * changing rr fields. Otherwise b9/c15 precede the canonical 64x32 ARGB image
 * constructor31; source draw45 clips/pads at (0,0), with SrcOver integer alpha
 * quantization, and never scales. Source masks run77/88/99 in order. rr.a67
 * aliases returned pixels, protected by an independent native image hold.
 * Input/output must be genuine constructed canonical ARGB owners. Caller
 * keeps image ownership/exclusive pixel access synchronized with this call
 * and processor retirement. Canonical image occurrence accounting is atomic;
 * the RR fields and image pixels still require the synchronization below.
 *
 * A per-call atomic admission guard rejects overlapping process calls BUSY.
 * Failure preserves owned_out and reached fields/candidate. A later nonnull
 * call retires any unpublished prior candidate before its new source work;
 * final release retires both candidate and current array occurrence. Native
 * bounds/allocation errors do not pretend to be source Exceptions. Output and
 * error storage must be disjoint from every owner, pixel view and each other. */
mc_client_image_buffer_status mc_client_image_buffer_process(mc_client_image_buffer *owner,
                                                             mc_client_argb_image *nullable_input,
                                                             mc_client_argb_image **owned_out,
                                                             mc_client_image_buffer_error *error);

/* Passive borrow, requiring exclusive access or an exited/joined worker.
 * It must not overlap process/final release. Views expire on the next nonnull
 * process or final release; retain images independently for longer use. */
bool mc_client_image_buffer_inspect(const mc_client_image_buffer *owner,
                                    mc_client_image_buffer_state *out);

#endif
