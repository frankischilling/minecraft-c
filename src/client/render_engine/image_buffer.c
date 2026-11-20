#include "image_buffer.h"
#include "internal.h"

#include <stdatomic.h>
#include <stdlib.h>

struct mc_client_image_buffer {
    atomic_size_t holds;
    atomic_bool active;
    mc_client_image_buffer_limits limits;
    const uint32_t *a;
    int32_t b, c;
    mc_client_argb_image *array_owner, *pending;
    unsigned int last_source_pc;
    mc_client_image_buffer_error failure;
};

static mc_client_image_buffer_status fail(mc_client_image_buffer_error *error,
                                          mc_client_image_buffer_status status,
                                          const char *operation, unsigned int pc) {
    if (error != NULL)
        *error = (mc_client_image_buffer_error){
            .status = status,
            .operation = operation,
            .source_method = pc == 0 ? NULL : "a",
            .source_descriptor =
                pc == 0 ? NULL : "(Ljava/awt/image/BufferedImage;)Ljava/awt/image/BufferedImage;",
            .source_pc = pc,
            .native_policy = true};
    return status;
}

static mc_client_image_buffer_status success(mc_client_image_buffer_error *error) {
    if (error != NULL)
        *error = (mc_client_image_buffer_error){0};
    return MC_CLIENT_IMAGE_BUFFER_OK;
}

mc_client_image_buffer_limits mc_client_image_buffer_default_limits(void) {
    return (mc_client_image_buffer_limits){sizeof(mc_client_argb_image) +
                                           64u * 32u * sizeof(uint32_t)};
}

mc_client_image_buffer_status
mc_client_image_buffer_create(const mc_client_image_buffer_limits *limits,
                              mc_client_image_buffer **out, mc_client_image_buffer_error *error) {
    if (out == NULL)
        return fail(error, MC_CLIENT_IMAGE_BUFFER_INVALID_ARGUMENT, "rr native owner output", 0);
    mc_client_image_buffer *owner = calloc(1, sizeof(*owner));
    if (owner == NULL)
        return fail(error, MC_CLIENT_IMAGE_BUFFER_ALLOCATION_FAILED, "rr native owner allocation",
                    0);
    atomic_init(&owner->holds, 1);
    atomic_init(&owner->active, false);
    owner->limits = limits == NULL ? mc_client_image_buffer_default_limits() : *limits;
    owner->last_source_pc = 4; /* Actual Object-only rr constructor return. */
    *out = owner;
    return success(error);
}

mc_client_image_buffer_status mc_client_image_buffer_retain(mc_client_image_buffer *owner,
                                                            mc_client_image_buffer_error *error) {
    if (owner == NULL)
        return fail(error, MC_CLIENT_IMAGE_BUFFER_INVALID_ARGUMENT, "rr native occurrence", 0);
    size_t observed = atomic_load_explicit(&owner->holds, memory_order_acquire);
    for (;;) {
        if (observed == 0)
            return fail(error, MC_CLIENT_IMAGE_BUFFER_INVALID_PHASE, "rr consumed native owner", 0);
        if (observed == SIZE_MAX)
            return fail(error, MC_CLIENT_IMAGE_BUFFER_LIMIT, "rr native occurrence limit", 0);
        if (atomic_compare_exchange_weak_explicit(&owner->holds, &observed, observed + 1,
                                                  memory_order_acq_rel, memory_order_acquire))
            return success(error);
    }
}

mc_client_image_buffer_status mc_client_image_buffer_release(mc_client_image_buffer **slot,
                                                             mc_client_image_buffer_error *error) {
    if (slot == NULL)
        return fail(error, MC_CLIENT_IMAGE_BUFFER_INVALID_ARGUMENT, "rr native occurrence slot", 0);
    mc_client_image_buffer *owner = *slot;
    if (owner == NULL)
        return success(error);
    size_t observed = atomic_load_explicit(&owner->holds, memory_order_acquire);
    for (;;) {
        if (observed == 0)
            return fail(error, MC_CLIENT_IMAGE_BUFFER_INVALID_PHASE, "rr consumed native owner", 0);
        if (observed == 1 && atomic_load_explicit(&owner->active, memory_order_acquire))
            return fail(error, MC_CLIENT_IMAGE_BUFFER_BUSY, "rr active final native occurrence", 0);
        if (atomic_compare_exchange_weak_explicit(&owner->holds, &observed, observed - 1,
                                                  memory_order_acq_rel, memory_order_acquire))
            break;
    }
    *slot = NULL;
    if (observed == 1) {
        mc_client_argb_image_release(owner->pending);
        mc_client_argb_image_release(owner->array_owner);
        free(owner);
    }
    return success(error);
}

/* The measured selected JDK loop first rounds a straight channel to 8-bit
 * premultiplied storage, then uses its 24-bit reciprocal-table division. The
 * second step differs from direct rounded division at exact half values.
 * All products here are bounded unsigned arithmetic, with no host FP state. */
static uint32_t composed_channel(uint32_t alpha, uint32_t channel) {
    const uint32_t premultiplied = (alpha * channel + 127u) / 255u;
    const uint64_t reciprocal = ((UINT64_C(255) << 24) + alpha / 2u) / alpha;
    return (uint32_t)((premultiplied * reciprocal + (UINT64_C(1) << 23)) >> 24);
}

static uint32_t src_over_clear(uint32_t argb) {
    const uint32_t alpha = argb >> 24;
    if (alpha == 0)
        return 0;
    if (alpha == 255)
        return argb;
    return (alpha << 24) | (composed_channel(alpha, (argb >> 16) & 255u) << 16) |
           (composed_channel(alpha, (argb >> 8) & 255u) << 8) |
           composed_channel(alpha, argb & 255u);
}

static bool transparent(const uint32_t *pixels, int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
    for (int32_t x = x0; x < x1; ++x)
        for (int32_t y = y0; y < y1; ++y)
            if ((pixels[(size_t)y * 64u + (size_t)x] >> 24) < 128u)
                return true;
    return false;
}

static void opaque(uint32_t *pixels, int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
    for (int32_t x = x0; x < x1; ++x)
        for (int32_t y = y0; y < y1; ++y)
            pixels[(size_t)y * 64u + (size_t)x] |= UINT32_C(0xff000000);
}

static bool final_scan(const uint32_t *pixels) {
    bool found = false;
    for (int32_t x = 32; x < 64; ++x)
        for (int32_t y = 0; y < 16; ++y)
            if ((pixels[(size_t)y * 64u + (size_t)x] >> 24) < 128u)
                found = true;
    return found;
}

static mc_client_image_buffer_status image_failure(mc_client_image_buffer_error *error,
                                                   mc_client_render_engine_status image,
                                                   const char *operation, unsigned int pc) {
    const mc_client_image_buffer_status status =
        image == MC_CLIENT_RENDER_ENGINE_LIMIT ? MC_CLIENT_IMAGE_BUFFER_LIMIT
        : image == MC_CLIENT_RENDER_ENGINE_ALLOCATION_FAILED
            ? MC_CLIENT_IMAGE_BUFFER_ALLOCATION_FAILED
        : image == MC_CLIENT_RENDER_ENGINE_INVALID_PHASE ? MC_CLIENT_IMAGE_BUFFER_INVALID_PHASE
                                                         : MC_CLIENT_IMAGE_BUFFER_IMAGE_FAILED;
    fail(error, status, operation, pc);
    error->image = image;
    return status;
}

mc_client_image_buffer_status mc_client_image_buffer_process(mc_client_image_buffer *owner,
                                                             mc_client_argb_image *input,
                                                             mc_client_argb_image **out,
                                                             mc_client_image_buffer_error *error) {
    mc_client_image_buffer_error current = {0};
    if (out == NULL)
        return fail(error, MC_CLIENT_IMAGE_BUFFER_INVALID_ARGUMENT, "rr native image output", 0);
    mc_client_image_buffer_status status = mc_client_image_buffer_retain(owner, &current);
    if (status != MC_CLIENT_IMAGE_BUFFER_OK) {
        if (error != NULL)
            *error = current;
        return status;
    }
    bool expected = false;
    if (!atomic_compare_exchange_strong_explicit(&owner->active, &expected, true,
                                                 memory_order_acq_rel, memory_order_acquire)) {
        (void)mc_client_image_buffer_release(&owner, NULL);
        return fail(error, MC_CLIENT_IMAGE_BUFFER_BUSY, "rr process already active", 0);
    }
    bool input_held = false;
    if (input == NULL) {
        owner->last_source_pc = 5;
        *out = NULL;
        goto done;
    }
    mc_client_render_engine_status image = mc_client_argb_image_retain(input);
    if (image != MC_CLIENT_RENDER_ENGINE_OK) {
        status = image_failure(&current, image, "rr native input image occurrence", 0);
        goto done;
    }
    input_held = true;
    mc_client_argb_image_release(owner->pending);
    owner->pending = NULL;
    owner->b = 64;
    owner->last_source_pc = 9;
    owner->c = 32;
    owner->last_source_pc = 15;
    image = mc_client_argb_image_create(64, 32, owner->limits.max_image_bytes, &owner->pending);
    if (image != MC_CLIENT_RENDER_ENGINE_OK) {
        status = image_failure(&current, image, "rr actual ARGB output constructor", 31);
        goto done;
    }
    owner->last_source_pc = 36;
    mc_client_argb_image_state source;
    if (!mc_client_argb_image_inspect(input, &source) || !source.constructed || source.width == 0 ||
        source.height == 0 || source.pixels == NULL ||
        (size_t)source.width > source.pixel_count / source.height) {
        status = fail(&current, MC_CLIENT_IMAGE_BUFFER_INVALID_PHASE,
                      "rr drawImage actual constructed ARGB input", 45);
        goto done;
    }
    const uint32_t width = source.width < 64u ? source.width : 64u;
    const uint32_t height = source.height < 32u ? source.height : 32u;
    for (uint32_t y = 0; y < height; ++y)
        for (uint32_t x = 0; x < width; ++x)
            owner->pending->pixels[(size_t)y * 64u + x] =
                src_over_clear(source.pixels[(size_t)y * source.width + x]);
    owner->last_source_pc = 45;
    owner->last_source_pc = 50; /* Actual selected CPU Graphics.dispose. */
    mc_client_argb_image *old = owner->array_owner;
    owner->array_owner = owner->pending;
    owner->pending = NULL;
    owner->a = owner->array_owner->pixels; /* Actual DataBufferInt alias store67. */
    owner->last_source_pc = 67;
    mc_client_argb_image_release(old);
    uint32_t *pixels = owner->array_owner->pixels;
    opaque(pixels, 0, 0, 32, 16);
    owner->last_source_pc = 77;
    if (!transparent(pixels, 32, 0, 64, 32))
        for (int32_t x = 32; x < 64; ++x)
            for (int32_t y = 0; y < 32; ++y)
                pixels[(size_t)y * 64u + (size_t)x] &= UINT32_C(0x00ffffff);
    owner->last_source_pc = 88;
    opaque(pixels, 0, 16, 64, 32);
    owner->last_source_pc = 99;
    /* Original final scans102..242 perform no further pixel writes. */
    if (!final_scan(pixels))
        (void)final_scan(pixels);
    image = mc_client_argb_image_retain(owner->array_owner);
    if (image != MC_CLIENT_RENDER_ENGINE_OK) {
        status = image_failure(&current, image, "rr native returned image occurrence", 243);
        goto done;
    }
    owner->last_source_pc = 243;
    owner->failure = (mc_client_image_buffer_error){0};
    *out = owner->array_owner;
done:
    if (input_held)
        mc_client_argb_image_release(input);
    if (status != MC_CLIENT_IMAGE_BUFFER_OK)
        owner->failure = current;
    atomic_store_explicit(&owner->active, false, memory_order_release);
    (void)mc_client_image_buffer_release(&owner, NULL);
    if (error != NULL)
        *error = current;
    return status;
}

bool mc_client_image_buffer_inspect(const mc_client_image_buffer *owner,
                                    mc_client_image_buffer_state *out) {
    if (owner == NULL || out == NULL)
        return false;
    *out = (mc_client_image_buffer_state){
        .native_holds = atomic_load_explicit(&owner->holds, memory_order_acquire),
        .active = atomic_load_explicit(&owner->active, memory_order_acquire),
        .a = owner->a,
        .b = owner->b,
        .c = owner->c,
        .array_owner = owner->array_owner,
        .pending_candidate = owner->pending,
        .last_source_pc = owner->last_source_pc,
        .failure = owner->failure};
    return true;
}
