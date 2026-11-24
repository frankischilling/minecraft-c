#include "client/render_engine/image_buffer.h"

#include <stdatomic.h>
#include <stdio.h>
#include <threads.h>

static unsigned checks;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition);                        \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

static bool fill(mc_client_argb_image *image, uint32_t value) {
    mc_client_argb_image_state state;
    if (!mc_client_argb_image_inspect(image, &state))
        return false;
    for (uint32_t y = 0; y < state.height; ++y)
        for (uint32_t x = 0; x < state.width; ++x)
            if (mc_client_argb_image_set_pixel(image, (int32_t)x, (int32_t)y, value) !=
                MC_CLIENT_RENDER_ENGINE_OK)
                return false;
    return true;
}

static bool original_grid(mc_client_image_buffer *owner) {
    bool result = false;
    mc_client_argb_image *input = NULL, *output = NULL;
    mc_client_image_buffer_error error = {0};
    uint64_t hash = UINT64_C(14695981039346656037);
    CHECK(mc_client_argb_image_create(64, 32, 65536, &input) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(fill(input, UINT32_C(0xff123456)));
    for (uint32_t batch = 0; batch < 128; ++batch) {
        for (uint32_t slot = 0; slot < 512; ++slot) {
            const uint32_t code = batch * 512u + slot;
            const uint32_t alpha = code >> 8, channel = code & 255u;
            const uint32_t pixel = (alpha << 24) | (channel << 16) | ((255u - channel) << 8) |
                                   ((channel * 73u + 19u) & 255u);
            CHECK(mc_client_argb_image_set_pixel(input, (int32_t)(slot % 32u),
                                                 (int32_t)(slot / 32u),
                                                 pixel) == MC_CLIENT_RENDER_ENGINE_OK);
        }
        CHECK(mc_client_image_buffer_process(owner, input, &output, &error) ==
              MC_CLIENT_IMAGE_BUFFER_OK);
        mc_client_argb_image_state image;
        mc_client_image_buffer_state state;
        CHECK(mc_client_argb_image_inspect(output, &image) && image.width == 64 &&
              image.height == 32 && image.pixel_count == 2048 && image.native_holds == 2 &&
              mc_client_image_buffer_inspect(owner, &state) && state.a == image.pixels &&
              state.array_owner == output && state.pending_candidate == NULL &&
              state.last_source_pc == 243 && state.native_holds == 1 && !state.active);
        for (size_t i = 0; i < image.pixel_count; ++i)
            for (int shift = 24; shift >= 0; shift -= 8) {
                hash ^= (image.pixels[i] >> (unsigned)shift) & 255u;
                hash *= UINT64_C(1099511628211);
            }
        mc_client_argb_image_release(output);
        output = NULL;
    }
    /* Original pinned rr, Temurin17.0.17+10, authored TYPE_INT_ARGB and
     * TYPE_4BYTE_ABGR grids: identical 262144 output pixels per type. All 65536
     * alpha/channel pairs occur in each RGB channel. Hash bytes are ARGB BE. */
    CHECK(hash == UINT64_C(0x04e5e02bb77c37e9));
    result = true;
cleanup:
    mc_client_argb_image_release(output);
    mc_client_argb_image_release(input);
    return result;
}

typedef struct hold_job {
    mc_client_image_buffer *owner;
} hold_job;

static int holds_worker(void *opaque) {
    hold_job *job = opaque;
    for (size_t i = 0; i < 10000; ++i) {
        if (mc_client_image_buffer_retain(job->owner, NULL) != MC_CLIENT_IMAGE_BUFFER_OK)
            return 1;
        mc_client_image_buffer *extra = job->owner;
        if (mc_client_image_buffer_release(&extra, NULL) != MC_CLIENT_IMAGE_BUFFER_OK)
            return 1;
    }
    return mc_client_image_buffer_release(&job->owner, NULL) == MC_CLIENT_IMAGE_BUFFER_OK ? 0 : 1;
}

static bool concurrent_holds(mc_client_image_buffer *owner) {
    thrd_t threads[4];
    hold_job jobs[4] = {0};
    size_t started = 0;
    bool result = true;
    for (size_t i = 0; i < 4; ++i) {
        if (mc_client_image_buffer_retain(owner, NULL) != MC_CLIENT_IMAGE_BUFFER_OK) {
            result = false;
            break;
        }
        jobs[i].owner = owner;
        if (thrd_create(&threads[i], holds_worker, &jobs[i]) != thrd_success) {
            (void)mc_client_image_buffer_release(&jobs[i].owner, NULL);
            result = false;
            break;
        }
        ++started;
    }
    for (size_t i = 0; i < started; ++i) {
        int status = 1;
        if (thrd_join(threads[i], &status) != thrd_success || status != 0)
            result = false;
        if (jobs[i].owner != NULL)
            (void)mc_client_image_buffer_release(&jobs[i].owner, NULL);
    }
    return result;
}

typedef struct image_hold_control {
    atomic_size_t ready;
    atomic_bool alias_replaced, abort;
} image_hold_control;

typedef struct image_hold_job {
    image_hold_control *control;
    mc_client_argb_image *image; /* This thread's independent owned occurrence. */
} image_hold_job;

static bool fixed_image_intact(const mc_client_argb_image *image) {
    mc_client_argb_image_state state;
    return mc_client_argb_image_inspect(image, &state) && state.native_holds != 0 &&
           state.constructed && state.type == 2 && state.width == 64 && state.height == 32 &&
           state.pixel_count == 2048 && state.pixels[0] == UINT32_C(0xff123456) &&
           state.pixels[32] == 0 && state.pixels[16 * 64] == UINT32_C(0xff000000);
}

static int image_holds_worker(void *opaque) {
    image_hold_job *job = opaque;
    int result = 0;
    atomic_fetch_add_explicit(&job->control->ready, 1, memory_order_release);
    /* Main replaces the real RR alias while these readers hold the old image.
     * No RR source fields or pixels are concurrently mutated by a reader. */
    while (!atomic_load_explicit(&job->control->alias_replaced, memory_order_acquire) &&
           !atomic_load_explicit(&job->control->abort, memory_order_acquire)) {
        if (!fixed_image_intact(job->image)) {
            result = 1;
            goto done;
        }
        thrd_yield();
    }
    for (size_t i = 0;
         i < 10000u && !atomic_load_explicit(&job->control->abort, memory_order_acquire); ++i) {
        if (mc_client_argb_image_retain(job->image) != MC_CLIENT_RENDER_ENGINE_OK) {
            result = 1;
            goto done;
        }
        mc_client_argb_image *extra = job->image;
        if (!fixed_image_intact(extra))
            result = 1;
        mc_client_argb_image_release(extra);
        extra = NULL;
        if (result != 0)
            goto done;
    }
done:
    mc_client_argb_image_release(job->image);
    job->image = NULL;
    return result;
}

static bool concurrent_image_occurrences(void) {
    bool result = false;
    mc_client_image_buffer *processor = NULL;
    mc_client_argb_image *input = NULL, *fixed = NULL, *replacement = NULL;
    mc_client_image_buffer_error error;
    mc_client_image_buffer_state processor_state;
    mc_client_argb_image_state image;
    image_hold_control control;
    atomic_init(&control.ready, 0);
    atomic_init(&control.alias_replaced, false);
    atomic_init(&control.abort, false);
    thrd_t threads[4];
    image_hold_job jobs[4] = {0};
    size_t started = 0;
    CHECK(mc_client_image_buffer_create(NULL, &processor, &error) == MC_CLIENT_IMAGE_BUFFER_OK);
    CHECK(mc_client_argb_image_create(1, 1, 65536, &input) == MC_CLIENT_RENDER_ENGINE_OK &&
          mc_client_argb_image_set_pixel(input, 0, 0, UINT32_C(0xff123456)) ==
              MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_image_buffer_process(processor, input, &fixed, &error) ==
              MC_CLIENT_IMAGE_BUFFER_OK &&
          fixed_image_intact(fixed) && mc_client_argb_image_inspect(fixed, &image) &&
          image.native_holds == 2);
    for (size_t i = 0; i < 4; ++i) {
        CHECK(mc_client_argb_image_retain(fixed) == MC_CLIENT_RENDER_ENGINE_OK);
        jobs[i] = (image_hold_job){&control, fixed};
        if (thrd_create(&threads[i], image_holds_worker, &jobs[i]) != thrd_success) {
            mc_client_argb_image_release(jobs[i].image);
            jobs[i].image = NULL;
            CHECK(false);
        }
        ++started;
    }
    while (atomic_load_explicit(&control.ready, memory_order_acquire) != started)
        thrd_yield();
    /* The caller's fixed occurrence stays live through RR's real release of
     * its previous array_owner and all simultaneous inspect/retain/release. */
    CHECK(mc_client_image_buffer_process(processor, input, &replacement, &error) ==
              MC_CLIENT_IMAGE_BUFFER_OK &&
          replacement != fixed && mc_client_image_buffer_inspect(processor, &processor_state) &&
          processor_state.array_owner == replacement && fixed_image_intact(fixed));
    atomic_store_explicit(&control.alias_replaced, true, memory_order_release);
    for (size_t i = 0; i < 10000u; ++i)
        CHECK(fixed_image_intact(fixed));
    result = true;
cleanup:
    if (!result)
        atomic_store_explicit(&control.abort, true, memory_order_release);
    atomic_store_explicit(&control.alias_replaced, true, memory_order_release);
    for (size_t i = 0; i < started; ++i) {
        int status = 1;
        if (thrd_join(threads[i], &status) != thrd_success || status != 0 || jobs[i].image != NULL)
            result = false;
    }
    if (result && (!mc_client_argb_image_inspect(fixed, &image) || image.native_holds != 1 ||
                   !fixed_image_intact(fixed)))
        result = false;
    mc_client_argb_image_release(replacement);
    if (mc_client_image_buffer_release(&processor, NULL) != MC_CLIENT_IMAGE_BUFFER_OK)
        result = false;
    if (result && (!mc_client_argb_image_inspect(fixed, &image) || image.native_holds != 1 ||
                   !fixed_image_intact(fixed)))
        result = false;
    mc_client_argb_image_release(fixed);
    mc_client_argb_image_release(input);
    return result;
}

int main(void) {
    int result = 1;
    mc_client_image_buffer *owner = NULL, *limited = NULL, *external = NULL;
    mc_client_argb_image *input = NULL, *output = NULL, *previous = NULL, *nothing = NULL;
    mc_client_image_buffer_error error = {0};
    mc_client_image_buffer_state state, before;
    mc_client_argb_image_state image;
    CHECK(mc_client_image_buffer_create(NULL, &owner, &error) == MC_CLIENT_IMAGE_BUFFER_OK);
    CHECK(mc_client_image_buffer_inspect(owner, &state) && state.a == NULL && state.b == 0 &&
          state.c == 0 && state.native_holds == 1 && !state.active && state.last_source_pc == 4);
    CHECK(mc_client_image_buffer_process(owner, NULL, &nothing, &error) ==
              MC_CLIENT_IMAGE_BUFFER_OK &&
          nothing == NULL && mc_client_image_buffer_inspect(owner, &state) && state.a == NULL &&
          state.b == 0 && state.c == 0 && state.last_source_pc == 5);
    CHECK(mc_client_argb_image_create(1, 1, 65536, &input) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_argb_image_set_pixel(input, 0, 0, UINT32_C(0x01ff8020)) ==
          MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_image_buffer_process(owner, input, &output, &error) ==
          MC_CLIENT_IMAGE_BUFFER_OK);
    CHECK(mc_client_argb_image_inspect(output, &image) && image.pixels[0] == UINT32_C(0xffffff00) &&
          image.pixels[1] == UINT32_C(0xff000000) && image.pixels[32] == 0 &&
          image.pixels[16 * 64] == UINT32_C(0xff000000));
    previous = output;
    output = NULL;
    CHECK(mc_client_argb_image_set_pixel(input, 0, 0, UINT32_C(0x7fff8020)) ==
          MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_image_buffer_process(owner, input, &output, &error) ==
          MC_CLIENT_IMAGE_BUFFER_OK);
    CHECK(mc_client_argb_image_inspect(output, &image) && image.pixels[0] == UINT32_C(0xffff8120));
    CHECK(mc_client_argb_image_inspect(previous, &image) && image.native_holds == 1 &&
          image.pixels[0] == UINT32_C(0xffffff00));
    mc_client_argb_image_release(previous);
    previous = NULL;
    mc_client_argb_image_release(output);
    output = NULL;
    CHECK(mc_client_argb_image_set_pixel(input, 0, 0, UINT32_C(0xfeff8020)) ==
          MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_image_buffer_process(owner, input, &output, &error) ==
          MC_CLIENT_IMAGE_BUFFER_OK);
    CHECK(mc_client_argb_image_inspect(output, &image) && image.pixels[0] == UINT32_C(0xffff7f20));
    CHECK(mc_client_image_buffer_inspect(owner, &before));
    CHECK(mc_client_image_buffer_process(owner, NULL, &nothing, &error) ==
              MC_CLIENT_IMAGE_BUFFER_OK &&
          nothing == NULL && mc_client_image_buffer_inspect(owner, &state) && state.a == before.a &&
          state.b == before.b && state.c == before.c && state.array_owner == output);
    const mc_client_image_buffer_limits limit = {0};
    CHECK(mc_client_image_buffer_create(&limit, &limited, &error) == MC_CLIENT_IMAGE_BUFFER_OK);
    nothing = input; /* Borrowed sentinel: failure must preserve the output cell. */
    CHECK(mc_client_image_buffer_process(limited, input, &nothing, &error) ==
              MC_CLIENT_IMAGE_BUFFER_LIMIT &&
          nothing == input && error.source_pc == 31 && error.native_policy &&
          error.image == MC_CLIENT_RENDER_ENGINE_LIMIT &&
          mc_client_image_buffer_inspect(limited, &state) && state.b == 64 && state.c == 32 &&
          state.a == NULL && state.last_source_pc == 15 && state.failure.source_pc == 31 &&
          state.pending_candidate == NULL && state.native_holds == 1);
    nothing = NULL;
    CHECK(mc_client_image_buffer_process(limited, NULL, &nothing, &error) ==
              MC_CLIENT_IMAGE_BUFFER_OK &&
          mc_client_image_buffer_inspect(limited, &state) && state.b == 64 && state.c == 32 &&
          state.a == NULL && state.failure.source_pc == 31);
    mc_client_argb_image_release(input);
    input = NULL;
    mc_client_argb_image_release(output);
    output = NULL;
    CHECK(mc_client_argb_image_create(65, 33, 65536, &input) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(fill(input, UINT32_C(0xff123456)));
    CHECK(mc_client_argb_image_set_pixel(input, 64, 0, UINT32_C(0xffabcdef)) ==
          MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_argb_image_set_pixel(input, 0, 32, UINT32_C(0xffabcdef)) ==
          MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_image_buffer_process(owner, input, &output, &error) ==
          MC_CLIENT_IMAGE_BUFFER_OK);
    CHECK(mc_client_argb_image_inspect(output, &image) && image.pixels[0] == UINT32_C(0xff123456) &&
          image.pixels[40] == UINT32_C(0x00123456) &&
          image.pixels[31 * 64 + 63] == UINT32_C(0xff123456));
    mc_client_argb_image_release(output);
    output = NULL;
    CHECK(mc_client_argb_image_set_pixel(input, 63, 31, UINT32_C(0x7f123456)) ==
          MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_image_buffer_process(owner, input, &output, &error) ==
          MC_CLIENT_IMAGE_BUFFER_OK);
    CHECK(mc_client_argb_image_inspect(output, &image) &&
          image.pixels[40] == UINT32_C(0xff123456) &&
          (image.pixels[31 * 64 + 63] >> 24) ==
              255); /* Whole right half scanned before bottom opacity. */
    mc_client_argb_image_release(output);
    output = NULL;
    CHECK(mc_client_argb_image_set_pixel(input, 63, 31, UINT32_C(0x80123456)) ==
          MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_image_buffer_process(owner, input, &output, &error) ==
          MC_CLIENT_IMAGE_BUFFER_OK);
    CHECK(mc_client_argb_image_inspect(output, &image) && image.pixels[40] == UINT32_C(0x00123456));
    mc_client_argb_image_release(output);
    output = NULL;
    CHECK(mc_client_argb_image_set_pixel(input, 40, 8, UINT32_C(0x7fff8020)) ==
          MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_image_buffer_process(owner, input, &output, &error) ==
          MC_CLIENT_IMAGE_BUFFER_OK);
    CHECK(mc_client_argb_image_inspect(output, &image) &&
          image.pixels[8 * 64 + 40] == UINT32_C(0x7fff8120));
    CHECK(original_grid(owner));
    CHECK(concurrent_holds(owner));
    CHECK(concurrent_image_occurrences());
    CHECK(mc_client_image_buffer_inspect(owner, &state) && state.native_holds == 1 &&
          !state.active);
    CHECK(mc_client_image_buffer_retain(owner, &error) == MC_CLIENT_IMAGE_BUFFER_OK);
    external = owner;
    CHECK(mc_client_image_buffer_release(&owner, &error) == MC_CLIENT_IMAGE_BUFFER_OK &&
          owner == NULL);
    CHECK(mc_client_image_buffer_release(&external, &error) == MC_CLIENT_IMAGE_BUFFER_OK &&
          external == NULL);
    CHECK(mc_client_argb_image_inspect(output, &image) && image.native_holds == 1 &&
          image.pixels[8 * 64 + 40] == UINT32_C(0x7fff8120));
    result = 0;
cleanup:
    if (result != 0)
        fprintf(stderr, "rr status%d pc%u: %s\n", (int)error.status, error.source_pc,
                error.operation == NULL ? "test assertion" : error.operation);
    mc_client_argb_image_release(previous);
    mc_client_argb_image_release(output);
    mc_client_argb_image_release(input);
    if (mc_client_image_buffer_release(&external, NULL) != MC_CLIENT_IMAGE_BUFFER_OK)
        result = 1;
    if (mc_client_image_buffer_release(&limited, NULL) != MC_CLIENT_IMAGE_BUFFER_OK)
        result = 1;
    if (mc_client_image_buffer_release(&owner, NULL) != MC_CLIENT_IMAGE_BUFFER_OK)
        result = 1;
    if (result == 0 &&
        printf("Actual rr image processing: %u checks, 262144 original-grid pixels\n", checks) < 0)
        result = 1;
    return result;
}
