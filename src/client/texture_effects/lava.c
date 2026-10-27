#include "lava_construction.h"
#include "mc/block/registry/runtime.h"
#include <float.h>
#include <stdlib.h>
#include <string.h>
_Static_assert(sizeof(float) == 4, "cg constructor float array storage");
_Static_assert(FLT_RADIX == 2 && FLT_MANT_DIG == 24 && FLT_EVAL_METHOD == 0,
               "cg animation binary32 evaluation profile");

struct mc_client_lava_texture_effect {
    mc_registry_classes *registry;
    mc_lava_texture_state state;
    size_t maximum;
    mc_java_random_profile *profile; /* Actual shared Math owner; borrowed. */
    const mc_math_trig *trig;
    bool animating;
};

static mc_lava_texture_status fail(mc_lava_texture_error *out, mc_lava_texture_status status,
                                   const char *operation, const char *source, int32_t pc,
                                   int32_t cg_pc) {
    if (out != NULL) {
        out->status = status;
        out->operation = operation;
        out->source_class = source;
        out->source_pc = pc;
        out->cg_pc = cg_pc;
    }
    return status;
}

static mc_lava_texture_status
allocate_actual(mc_registry_classes *registry, mc_java_random_profile *profile,
                const mc_math_trig *trig, bool require_trig, const mc_lava_texture_options *options,
                mc_client_lava_texture_effect **out, mc_lava_texture_error *error) {
    if (error != NULL)
        *error = (mc_lava_texture_error){.source_pc = -1, .cg_pc = -1};
    if (registry == NULL || profile == NULL || (require_trig && trig == NULL) || out == NULL)
        return fail(error, MC_LAVA_TEXTURE_INVALID_ARGUMENT,
                    "lava texture native allocation arguments", NULL, -1, -1);
    mc_client_lava_texture_effect *owner = calloc(1, sizeof(*owner));
    if (owner == NULL)
        return fail(error, MC_LAVA_TEXTURE_ALLOCATION_FAILED, "lava texture native owner", NULL, -1,
                    -1);
    mc_registry_error cause = {0};
    if (mc_registry_classes_retain(registry, MC_REGISTRY_CLIENT, profile, &cause) !=
        MC_REGISTRY_OK) {
        free(owner);
        if (error != NULL)
            error->registry = cause;
        return fail(error, MC_LAVA_TEXTURE_REGISTRY_FAILED, "lava texture Registry namespace", NULL,
                    -1, -1);
    }
    owner->registry = registry;
    owner->profile = profile;
    owner->trig = trig;
    owner->maximum =
        options == NULL || options->max_array_bytes == 0 ? 5120 : options->max_array_bytes;
    *out = owner;
    return MC_LAVA_TEXTURE_OK;
}

mc_lava_texture_status
mc_client_lava_texture_allocate(mc_registry_classes *registry, mc_java_random_profile *profile,
                                const mc_math_trig *trig, const mc_lava_texture_options *options,
                                mc_client_lava_texture_effect **out, mc_lava_texture_error *error) {
    return allocate_actual(registry, profile, trig, true, options, out, error);
}

mc_lava_texture_status mc_client_lava_texture_allocate_constructor(
    mc_registry_classes *registry, mc_java_random_profile *profile,
    const mc_lava_texture_options *options, mc_client_lava_texture_effect **out,
    mc_lava_texture_error *error) {
    return allocate_actual(registry, profile, NULL, false, options, out, error);
}

mc_lava_texture_status mc_client_lava_texture_bind_trig(mc_client_lava_texture_effect *owner,
                                                        const mc_math_trig *trig,
                                                        mc_lava_texture_error *error) {
    if (error != NULL)
        *error = (mc_lava_texture_error){.source_pc = -1, .cg_pc = -1};
    if (owner == NULL || trig == NULL)
        return fail(error, MC_LAVA_TEXTURE_INVALID_ARGUMENT, "lava native trig binding arguments",
                    NULL, -1, -1);
    if (owner->animating || owner->state.phase == MC_LAVA_TEXTURE_CONSTRUCTING)
        return fail(error, MC_LAVA_TEXTURE_BUSY, "lava native trig binding entered", NULL, -1, -1);
    if (owner->state.phase != MC_LAVA_TEXTURE_READY)
        return fail(error, MC_LAVA_TEXTURE_INVALID_PHASE,
                    "lava native trig binding requires constructed effect", NULL, -1, -1);
    if (owner->trig != NULL && owner->trig != trig)
        return fail(error, MC_LAVA_TEXTURE_INVALID_PHASE,
                    "lava native trig namespace already bound", NULL, -1, -1);
    owner->trig = trig;
    return MC_LAVA_TEXTURE_OK;
}

static void *array(mc_client_lava_texture_effect *owner, size_t bytes, const char *source,
                   int32_t pc, int32_t cg_pc, mc_lava_texture_error *error) {
    if (bytes > owner->maximum - owner->state.array_bytes) {
        fail(error, MC_LAVA_TEXTURE_LIMIT, "lava texture native array quota", source, pc, cg_pc);
        return NULL;
    }
    void *storage = calloc(1, bytes);
    if (storage == NULL) {
        fail(error, MC_LAVA_TEXTURE_ALLOCATION_FAILED, "lava texture array allocation", source, pc,
             cg_pc);
        return NULL;
    }
    owner->state.array_bytes += bytes;
    return storage;
}

mc_lava_texture_status mc_client_lava_texture_construct(mc_client_lava_texture_effect *owner,
                                                        mc_lava_texture_error *out) {
    mc_lava_texture_error error = {.source_pc = -1, .cg_pc = -1};
    if (owner == NULL)
        fail(&error, MC_LAVA_TEXTURE_INVALID_ARGUMENT, "lava texture receiver", NULL, -1, -1);
    else if (owner->state.phase != MC_LAVA_TEXTURE_RAW)
        fail(&error,
             owner->state.phase == MC_LAVA_TEXTURE_CONSTRUCTING ? MC_LAVA_TEXTURE_BUSY
                                                                : MC_LAVA_TEXTURE_INVALID_PHASE,
             "lava texture constructor phase", NULL, -1, -1);
    else {
        owner->state.phase = MC_LAVA_TEXTURE_CONSTRUCTING;
        const mc_block *block = NULL;
        if (mc_registry_block_static_get(owner->registry, "D", &block, &error.registry) !=
            MC_REGISTRY_OK)
            fail(&error, MC_LAVA_TEXTURE_REGISTRY_FAILED, "cg actual uu.D active use", "cg", 1, 1);
        else if (block == NULL)
            fail(&error, MC_LAVA_TEXTURE_NULL_REFERENCE, "cg null D texture receiver", "cg", 4, 4);
        else {
            const int32_t texture = mc_block_fields_get(block)->bm; /* cg4 argument before aw7. */
            mc_lava_texture_fields *f = &owner->state.fields;
            /* aw Object1; actual array8 and fields10,15,20,25,30,35. */
            f->a = array(owner, 1024, "aw", 8, 7, &error);
            if (f->a != NULL) {
                f->a_length = 1024;
                f->c = false;
                f->d = 0;
                f->e = 1;
                f->f = 0;
                f->b = texture;
                const float **slots[] = {&f->g, &f->h, &f->i, &f->j};
                size_t *lengths[] = {&f->g_length, &f->h_length, &f->i_length, &f->j_length};
                /* Exact cg newarray PCs. Their completed publications are
                 * g16,h25,i34,j43; instruction widths are not a stride. */
                static const int32_t newarray_pcs[] = {14, 23, 32, 41};
                for (size_t n = 0; n < 4; ++n) {
                    *slots[n] = array(owner, 256 * sizeof(float), "cg", newarray_pcs[n],
                                      newarray_pcs[n], &error);
                    if (*slots[n] == NULL)
                        break;
                    *lengths[n] = 256;
                }
            }
        }
        owner->state.phase =
            error.status == MC_LAVA_TEXTURE_OK ? MC_LAVA_TEXTURE_READY : MC_LAVA_TEXTURE_FAILED;
    }
    if (out != NULL)
        *out = error;
    return error.status;
}

bool mc_client_lava_texture_read(const mc_client_lava_texture_effect *owner,
                                 mc_lava_texture_state *out) {
    if (owner == NULL || out == NULL)
        return false;
    *out = owner->state;
    return true;
}

static bool animation_access(const void *storage, size_t length, size_t index, int32_t pc,
                             mc_lava_texture_error *error) {
    if (storage == NULL) {
        fail(error, MC_LAVA_TEXTURE_NULL_REFERENCE, "lava animation null array", "cg", pc, pc);
        return false;
    }
    if (index >= length) {
        fail(error, MC_LAVA_TEXTURE_INDEX_OUT_OF_RANGE, "lava animation array index", "cg", pc, pc);
        return false;
    }
    return true;
}

static bool animation_load(const float *storage, size_t length, size_t index, int32_t pc,
                           float *value, mc_lava_texture_error *error) {
    if (!animation_access(storage, length, index, pc, error))
        return false;
    *value = storage[index];
    return true;
}

static bool animation_store(const float *storage, size_t length, size_t index, float value,
                            int32_t pc, mc_lava_texture_error *error) {
    if (!animation_access(storage, length, index, pc, error))
        return false;
    ((float *)storage)[index] = value; /* Real retained mutable array; view is readonly. */
    return true;
}

static bool animation_byte(const uint8_t *storage, size_t length, size_t index, int32_t value,
                           int32_t pc, mc_lava_texture_error *error) {
    if (!animation_access(storage, length, index, pc, error))
        return false;
    ((uint8_t *)storage)[index] = (uint8_t)mc_java_i8_narrow(value);
    return true;
}

static float animation_add(float left, float right) {
    /* Captured JDK17 x86-64 interpreter fadd retains the right operand's NaN
     * when both are NaNs. C's result payload is not an operand-order promise.
     * Perform the actual binary32 operation, retaining its FP effects, then
     * preserve the selected payload/sign and quiet bit explicitly. */
    uint32_t left_bits, right_bits;
    memcpy(&left_bits, &left, sizeof(left_bits));
    memcpy(&right_bits, &right, sizeof(right_bits));
    volatile float computed = left + right;
    float result = computed;
    uint32_t selected;
    if ((right_bits & UINT32_C(0x7fffffff)) > UINT32_C(0x7f800000))
        selected = right_bits | UINT32_C(0x00400000);
    else if ((left_bits & UINT32_C(0x7fffffff)) > UINT32_C(0x7f800000))
        selected = left_bits | UINT32_C(0x00400000);
    else
        return result;
    memcpy(&result, &selected, sizeof(result));
    return result;
}

mc_lava_texture_status mc_client_lava_texture_animate(mc_client_lava_texture_effect *owner,
                                                      mc_lava_texture_error *out) {
    mc_lava_texture_error error = {.source_pc = -1, .cg_pc = -1};
    if (owner == NULL)
        fail(&error, MC_LAVA_TEXTURE_INVALID_ARGUMENT, "Lava animation receiver", NULL, -1, -1);
    else if (owner->animating || owner->state.phase == MC_LAVA_TEXTURE_CONSTRUCTING)
        fail(&error, MC_LAVA_TEXTURE_BUSY, "Lava effect operation entered", NULL, -1, -1);
    else if (owner->state.phase != MC_LAVA_TEXTURE_READY)
        fail(&error, MC_LAVA_TEXTURE_INVALID_PHASE, "Lava animation READY receiver", NULL, -1, -1);
    else if (owner->trig == NULL)
        fail(&error, MC_LAVA_TEXTURE_INVALID_PHASE, "lava native trig prerequisite unbound", NULL,
             -1, -1);
    else {
        owner->animating = true;
        mc_lava_texture_fields *f = &owner->state.fields;
        for (int32_t x = 0; x < 16; ++x) {
            for (int32_t y = 0; y < 16; ++y) {
                float sum = 0.0f;
                const float y_pi = (float)y * 3.1415927f;
                const float y_twice = y_pi * 2.0f;
                const float y_angle = y_twice / 16.0f;
                const float y_sin = mc_math_sin(owner->trig, y_angle); /* in.a28 */
                const int32_t shift_x = mc_java_float_to_i32(y_sin * 1.2f);
                const float x_pi = (float)x * 3.1415927f;
                const float x_twice = x_pi * 2.0f;
                const float x_angle = x_twice / 16.0f;
                const float x_sin = mc_math_sin(owner->trig, x_angle); /* in.a47 */
                const int32_t shift_y = mc_java_float_to_i32(x_sin * 1.2f);
                for (int32_t nx = x - 1; nx <= x + 1; ++nx) {
                    for (int32_t ny = y - 1; ny <= y + 1; ++ny) {
                        const uint32_t wrapped_x = (uint32_t)(nx + shift_x) & 15u;
                        const uint32_t wrapped_y = (uint32_t)(ny + shift_y) & 15u;
                        float value;
                        if (!animation_load(f->g, f->g_length, wrapped_x + wrapped_y * 16u, 115,
                                            &value, &error))
                            goto finished;
                        sum = animation_add(sum, value); /* fadd116 */
                    }
                }
                const size_t index = (size_t)x + (size_t)y * 16;
                const float *target = f->h; /* Receiver131 is captured before four i reads. */
                const size_t target_length = f->h_length;
                const float divided = sum / 10.0f;
                static const int32_t dx[] = {0, 1, 1, 0};
                static const int32_t dy[] = {0, 0, 1, 1};
                static const int32_t pcs[] = {164, 185, 207, 229};
                float neighbors = 0.0f;
                for (size_t n = 0; n < 4; ++n) {
                    const uint32_t ix = (uint32_t)(x + dx[n]) & 15u;
                    const uint32_t iy = (uint32_t)(y + dy[n]) & 15u;
                    float value;
                    if (!animation_load(f->i, f->i_length, ix + iy * 16u, pcs[n], &value, &error))
                        goto finished;
                    neighbors = n == 0 ? value : animation_add(neighbors, value);
                }
                const float average = neighbors / 4.0f;
                const float product = average * 0.8f;
                if (!animation_store(target, target_length, index, animation_add(divided, product),
                                     238, &error))
                    goto finished;
                target = f->i; /* Actual dup2 receiver240 before i250/j261. */
                const size_t live_i_length = f->i_length;
                float value, impulse;
                if (!animation_load(target, live_i_length, index, 250, &value, &error) ||
                    !animation_load(f->j, f->j_length, index, 261, &impulse, &error))
                    goto finished;
                const float gained = impulse * 0.01f;
                if (!animation_store(target, live_i_length, index, animation_add(value, gained),
                                     266, &error) ||
                    !animation_load(f->i, f->i_length, index, 277, &value, &error))
                    goto finished;
                if (value < 0.0f && !animation_store(f->i, f->i_length, index, 0.0f, 294, &error))
                    goto finished;
                target = f->j; /* Actual dup2 receiver296, read306, store310. */
                const size_t live_j_length = f->j_length;
                if (!animation_load(target, live_j_length, index, 306, &impulse, &error))
                    goto finished;
                const float decayed = impulse - 0.06f;
                if (!animation_store(target, live_j_length, index, decayed, 310, &error))
                    goto finished;
                double draw;
                error.random = mc_java_math_random(owner->profile, &draw);
                if (error.random != MC_JAVA_RANDOM_PROFILE_OK) {
                    fail(&error, MC_LAVA_TEXTURE_RANDOM_FAILED, "Lava shared Math.random", "cg",
                         311, 311);
                    goto finished;
                }
                if (draw < 0.005 && !animation_store(f->j, f->j_length, index, 1.5f, 333, &error))
                    goto finished;
            }
        }
        const float *old_h = f->h;
        const size_t old_h_length = f->h_length;
        f->h = f->g;
        f->h_length = f->g_length; /* Source store356 precedes g361. */
        f->g = old_h;
        f->g_length = old_h_length;
        for (size_t pixel = 0; pixel < 256; ++pixel) {
            float value;
            if (!animation_load(f->g, f->g_length, pixel, 378, &value, &error))
                goto finished;
            value *= 2.0f;
            if (value > 1.0f)
                value = 1.0f;
            if (value < 0.0f)
                value = 0.0f;
            const float red_product = value * 100.0f;
            int32_t red = mc_java_float_to_i32(animation_add(red_product, 155.0f));
            const float square = value * value;
            const int32_t original_green = mc_java_float_to_i32(square * 255.0f);
            int32_t green = original_green;
            const float cube = square * value;
            const float fourth = cube * value;
            int32_t blue = mc_java_float_to_i32(fourth * 128.0f);
            if (f->c) {
                int32_t weighted =
                    mc_java_i32_add(mc_java_i32_mul(red, 30), mc_java_i32_mul(green, 59));
                weighted = mc_java_i32_add(weighted, mc_java_i32_mul(blue, 11));
                const int32_t next_red = weighted / 100;
                const int32_t next_green =
                    mc_java_i32_add(mc_java_i32_mul(red, 30), mc_java_i32_mul(green, 70)) / 100;
                const int32_t next_blue =
                    mc_java_i32_add(mc_java_i32_mul(red, 30), mc_java_i32_mul(blue, 70)) / 100;
                red = next_red;
                green = next_green;
                blue = next_blue;
            }
            if (!animation_byte(f->a, f->a_length, pixel * 4, red, 525, &error) ||
                !animation_byte(f->a, f->a_length, pixel * 4 + 1, green, 538, &error) ||
                !animation_byte(f->a, f->a_length, pixel * 4 + 2, blue, 551, &error) ||
                !animation_byte(f->a, f->a_length, pixel * 4 + 3, -1, 562, &error))
                goto finished;
        }
    finished:
        owner->animating = false;
    }
    if (out != NULL)
        *out = error;
    return error.status;
}

mc_lava_texture_status mc_client_lava_texture_set_anaglyph(mc_client_lava_texture_effect *owner,
                                                           bool enabled,
                                                           mc_lava_texture_error *out) {
    mc_lava_texture_error error = {.source_pc = -1, .cg_pc = -1};
    if (owner == NULL)
        fail(&error, MC_LAVA_TEXTURE_INVALID_ARGUMENT, "lava aw.c field receiver", NULL, -1, -1);
    else if (owner->animating || owner->state.phase == MC_LAVA_TEXTURE_CONSTRUCTING)
        fail(&error, MC_LAVA_TEXTURE_BUSY, "lava effect operation entered", NULL, -1, -1);
    else if (owner->state.phase != MC_LAVA_TEXTURE_READY)
        fail(&error, MC_LAVA_TEXTURE_INVALID_PHASE, "lava aw.c READY field write", NULL, -1, -1);
    else
        owner->state.fields.c = enabled;
    if (out != NULL)
        *out = error;
    return error.status;
}

mc_lava_texture_status mc_client_lava_texture_retire(mc_client_lava_texture_effect **slot,
                                                     mc_lava_texture_error *error) {
    if (error != NULL)
        *error = (mc_lava_texture_error){.source_pc = -1, .cg_pc = -1};
    if (slot == NULL)
        return fail(error, MC_LAVA_TEXTURE_INVALID_ARGUMENT, "lava texture retirement slot", NULL,
                    -1, -1);
    mc_client_lava_texture_effect *owner = *slot;
    if (owner == NULL)
        return MC_LAVA_TEXTURE_OK;
    if (owner->state.phase == MC_LAVA_TEXTURE_CONSTRUCTING || owner->animating)
        return fail(error, MC_LAVA_TEXTURE_BUSY, "lava effect operation entered", NULL, -1, -1);
    mc_registry_error cause = {0};
    if (mc_registry_classes_release(owner->registry, &cause) != MC_REGISTRY_OK) {
        if (error != NULL)
            error->registry = cause;
        return fail(error, MC_LAVA_TEXTURE_REGISTRY_FAILED, "lava texture Registry release", NULL,
                    -1, -1);
    }
    const mc_lava_texture_fields *f = &owner->state.fields;
    free((void *)f->a);
    free((void *)f->g);
    free((void *)f->h);
    free((void *)f->i);
    free((void *)f->j);
    free(owner);
    *slot = NULL;
    return MC_LAVA_TEXTURE_OK;
}
