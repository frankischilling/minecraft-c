#include "color_array_internal.h"
#include "mc/core/java/java.h"
#include <stdlib.h>
#include <string.h>

static mc_client_color_array_status fail(mc_client_color_array_error *e,
                                         mc_client_color_array_status status, const char *operation,
                                         mc_client_graphics_site site, bool native) {
    e->status = status;
    e->operation = operation;
    e->site = site;
    e->native_policy = native;
    return status;
}

static void copy_error(mc_client_color_array_error *out, const mc_client_color_array_error *e) {
    if (out != NULL)
        *out = *e;
}

mc_client_color_array_limits mc_client_color_array_default_limits(void) {
    return (mc_client_color_array_limits){64u * 1024u * 1024u + sizeof(mc_client_color_array), 2};
}

mc_client_color_array_status
mc_client_color_array_create(int32_t length, const mc_client_color_array_limits *requested,
                             mc_client_graphics_site site, mc_client_color_array **candidate,
                             mc_client_color_array_error *error) {
    mc_client_color_array_error e = {.site = site, .actual_length = length};
    if (candidate == NULL || *candidate != NULL) {
        fail(&e, MC_CLIENT_COLOR_ARRAY_INVALID_ARGUMENT, "array native candidate slot", site, true);
        copy_error(error, &e);
        return e.status;
    }
    if (length < 0) {
        fail(&e, MC_CLIENT_COLOR_ARRAY_NEGATIVE_SIZE, "actual newarray negative length", site,
             false);
        copy_error(error, &e);
        return e.status;
    }
    const mc_client_color_array_limits limits =
        requested == NULL ? mc_client_color_array_default_limits() : *requested;
    const size_t count = (size_t)length;
    if (count > SIZE_MAX / sizeof(uint32_t) ||
        count * sizeof(uint32_t) > SIZE_MAX - sizeof(mc_client_color_array) ||
        count * sizeof(uint32_t) + sizeof(mc_client_color_array) > limits.max_native_bytes ||
        limits.max_allocations == 0) {
        fail(&e, MC_CLIENT_COLOR_ARRAY_LIMIT, "array native object/plane bound", site, true);
        copy_error(error, &e);
        return e.status;
    }
    mc_client_color_array *a = calloc(1, sizeof(*a));
    if (a == NULL) {
        fail(&e, MC_CLIENT_COLOR_ARRAY_ALLOCATION_FAILED, "array native object allocation", site,
             true);
        copy_error(error, &e);
        return e.status;
    }
    a->holds = 1;
    a->length = count;
    a->allocations = 1;
    a->bytes = sizeof(*a);
    *candidate = a; /* Native RAW candidate, not a completed source array store. */
    e.native_allocations = a->allocations;
    e.native_bytes = a->bytes;
    if (count != 0 && limits.max_allocations < 2) {
        fail(&e, MC_CLIENT_COLOR_ARRAY_LIMIT, "array native plane allocation bound", site, true);
        goto failed;
    }
    if (count != 0) {
        a->bits = calloc(count, sizeof(*a->bits));
        if (a->bits == NULL) {
            fail(&e, MC_CLIENT_COLOR_ARRAY_ALLOCATION_FAILED, "array native zero plane", site,
                 true);
            goto failed;
        }
        ++a->allocations;
        a->bytes += count * sizeof(*a->bits);
    }
    a->phase = MC_CLIENT_COLOR_ARRAY_READY;
    e.native_allocations = a->allocations;
    e.native_bytes = a->bytes;
    copy_error(error, &e);
    return MC_CLIENT_COLOR_ARRAY_OK;
failed:
    a->phase = MC_CLIENT_COLOR_ARRAY_FAILED;
    a->failure = e;
    copy_error(error, &e);
    return e.status;
}

mc_client_color_array_status mc_client_color_array_retain(mc_client_color_array *a) {
    if (a == NULL)
        return MC_CLIENT_COLOR_ARRAY_INVALID_ARGUMENT;
    if (a->holds == SIZE_MAX)
        return MC_CLIENT_COLOR_ARRAY_LIMIT;
    ++a->holds;
    return MC_CLIENT_COLOR_ARRAY_OK;
}

bool mc_client_color_array_can_release(const mc_client_color_array *a) {
    return mc_client_color_array_can_release_occurrences(a, 1);
}

bool mc_client_color_array_can_release_occurrences(const mc_client_color_array *a, size_t count) {
    return a == NULL || (count <= a->holds && (count < a->holds || a->active_calls == 0));
}

mc_client_color_array_status mc_client_color_array_release(mc_client_color_array **slot,
                                                           mc_client_color_array_error *error) {
    if (error != NULL)
        *error = (mc_client_color_array_error){0};
    if (slot == NULL) {
        if (error != NULL)
            fail(error, MC_CLIENT_COLOR_ARRAY_INVALID_ARGUMENT, "array native release slot",
                 (mc_client_graphics_site){0}, true);
        return MC_CLIENT_COLOR_ARRAY_INVALID_ARGUMENT;
    }
    mc_client_color_array *a = *slot;
    if (a == NULL)
        return MC_CLIENT_COLOR_ARRAY_OK;
    if (!mc_client_color_array_can_release(a)) {
        if (error != NULL)
            fail(error, MC_CLIENT_COLOR_ARRAY_BUSY, "array final native call active",
                 (mc_client_graphics_site){0}, true);
        return MC_CLIENT_COLOR_ARRAY_BUSY;
    }
    *slot = NULL;
    if (--a->holds != 0)
        return MC_CLIENT_COLOR_ARRAY_OK;
    free(a->bits);
    free(a);
    return MC_CLIENT_COLOR_ARRAY_OK;
}

bool mc_client_color_array_inspect(const mc_client_color_array *a,
                                   mc_client_color_array_state *out) {
    if (a == NULL || out == NULL)
        return false;
    *out = (mc_client_color_array_state){a->phase, a->holds,  a->active_calls, a->allocations,
                                         a->bytes, a->length, a->bits,         a->failure};
    return true;
}

mc_client_color_array_status mc_client_color_array_write(mc_client_color_array *a, size_t at,
                                                         uint32_t bits) {
    if (a == NULL || at >= a->length)
        return MC_CLIENT_COLOR_ARRAY_INVALID_ARGUMENT;
    if (a->phase != MC_CLIENT_COLOR_ARRAY_READY)
        return MC_CLIENT_COLOR_ARRAY_INVALID_PHASE;
    if (a->active_calls != 0)
        return MC_CLIENT_COLOR_ARRAY_BUSY;
    a->bits[at] = bits;
    return MC_CLIENT_COLOR_ARRAY_OK;
}

static mc_client_graphics_site helper_site(bool existing, int32_t pc) {
    return (mc_client_graphics_site){"ji", existing ? "a" : "b",
                                     existing ? "(Ljava/awt/image/BufferedImage;[I)[I"
                                              : "(Ljava/awt/image/BufferedImage;)[I",
                                     pc};
}

static mc_client_color_array_status dimensions(mc_client_argb_image *image, bool existing,
                                               mc_client_argb_image_state *state,
                                               mc_client_color_array_error *e) {
    if (image == NULL)
        return fail(e, MC_CLIENT_COLOR_ARRAY_NULL_REFERENCE,
                    "actual BufferedImage.getWidth receiver", helper_site(existing, 1), false);
    if (!mc_client_argb_image_inspect(image, state) || !state->constructed)
        return fail(e, MC_CLIENT_COLOR_ARRAY_INVALID_PHASE, "canonical image native phase",
                    helper_site(existing, -1), true);
    e->width = (int32_t)state->width;
    e->site = helper_site(existing, 1);
    e->height = (int32_t)state->height;
    e->site = helper_site(existing, 6);
    return MC_CLIENT_COLOR_ARRAY_OK;
}

/* Canonical getRGB representation only: full image, offset0, stride width.
 * No encoded-image/raster guess and no source transform is substituted. */
static mc_client_color_array_status copy_pixels(const mc_client_argb_image_state *image,
                                                mc_client_color_array *a, bool existing,
                                                mc_client_color_array_error *e) {
    const int32_t pc = existing ? 20 : 26;
    if (a == NULL || (a->phase == MC_CLIENT_COLOR_ARRAY_READY && a->length < image->pixel_count))
        return fail(e, MC_CLIENT_COLOR_ARRAY_UNIMPLEMENTED,
                    "actual getRGB NULL/short destination behavior", helper_site(existing, pc),
                    false);
    if (a->phase != MC_CLIENT_COLOR_ARRAY_READY)
        return fail(e, MC_CLIENT_COLOR_ARRAY_INVALID_PHASE, "destination array native phase",
                    helper_site(existing, -1), true);
    if (a->active_calls != 0)
        return fail(e, MC_CLIENT_COLOR_ARRAY_BUSY, "destination array native call active",
                    helper_site(existing, -1), true);
    ++a->active_calls;
    if (image->pixel_count != 0)
        memcpy(a->bits, image->pixels, image->pixel_count * sizeof(*a->bits));
    --a->active_calls;
    e->site = helper_site(existing, pc);
    return MC_CLIENT_COLOR_ARRAY_OK;
}

mc_client_color_array_status mc_client_color_array_from_image(
    mc_client_argb_image *image, const mc_client_color_array_limits *limits,
    mc_client_color_array **candidate, mc_client_color_array_error *error) {
    mc_client_color_array_error e = {0};
    mc_client_argb_image_state state;
    if (candidate == NULL || *candidate != NULL) {
        fail(&e, MC_CLIENT_COLOR_ARRAY_INVALID_ARGUMENT, "new array native candidate slot",
             helper_site(false, -1), true);
        copy_error(error, &e);
        return e.status;
    }
    if (dimensions(image, false, &state, &e) != MC_CLIENT_COLOR_ARRAY_OK) {
        copy_error(error, &e);
        return e.status;
    }
    const int32_t length = mc_java_i32_mul(e.width, e.height); /* Actual imul12. */
    mc_client_color_array_error allocated = {0};
    const mc_client_color_array_status status =
        mc_client_color_array_create(length, limits, helper_site(false, 13), candidate, &allocated);
    allocated.width = e.width;
    allocated.height = e.height;
    if (status != MC_CLIENT_COLOR_ARRAY_OK) {
        copy_error(error, &allocated);
        return status;
    }
    e = allocated;
    if (copy_pixels(&state, *candidate, false, &e) != MC_CLIENT_COLOR_ARRAY_OK) {
        copy_error(error, &e);
        return e.status;
    }
    e.site = helper_site(false, 32);
    copy_error(error, &e);
    return MC_CLIENT_COLOR_ARRAY_OK;
}

mc_client_color_array_status mc_client_color_array_fill(mc_client_argb_image *image,
                                                        mc_client_color_array *array,
                                                        mc_client_color_array_error *error) {
    mc_client_color_array_error e = {0};
    mc_client_argb_image_state state;
    if (dimensions(image, true, &state, &e) != MC_CLIENT_COLOR_ARRAY_OK ||
        copy_pixels(&state, array, true, &e) != MC_CLIENT_COLOR_ARRAY_OK) {
        copy_error(error, &e);
        return e.status;
    }
    e.site = helper_site(true, 25);
    copy_error(error, &e);
    return MC_CLIENT_COLOR_ARRAY_OK;
}
