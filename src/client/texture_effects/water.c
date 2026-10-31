#include "mc/client/texture_effects/water.h"
#include "base.h"
#include "mc/block/registry/runtime.h"
#include <float.h>
#include <stdlib.h>
#include <string.h>
_Static_assert(sizeof(float) == 4, "vs constructor float array storage");
_Static_assert(FLT_RADIX == 2 && FLT_MANT_DIG == 24 && FLT_EVAL_METHOD == 0,
               "vs animation binary32 evaluation profile");

struct mc_client_water_texture_effect {
    mc_client_texture_effect base;
    mc_registry_classes *registry;
    mc_water_texture_state state;
    size_t maximum;
    mc_java_random_profile *profile; /* Actual shared Math owner; borrowed. */
    bool animating;
};

static mc_water_texture_status fail(mc_water_texture_error *out, mc_water_texture_status status,
                                    const char *operation, const char *source, int32_t pc,
                                    int32_t vs_pc) {
    if (out != NULL) {
        out->status = status;
        out->operation = operation;
        out->source_class = source;
        out->source_pc = pc;
        out->vs_pc = vs_pc;
    }
    return status;
}

mc_water_texture_status mc_client_water_texture_allocate(mc_registry_classes *registry,
                                                         mc_java_random_profile *profile,
                                                         const mc_water_texture_options *options,
                                                         mc_client_water_texture_effect **out,
                                                         mc_water_texture_error *error) {
    if (error != NULL)
        *error = (mc_water_texture_error){.source_pc = -1, .vs_pc = -1};
    if (registry == NULL || profile == NULL || out == NULL)
        return fail(error, MC_WATER_TEXTURE_INVALID_ARGUMENT,
                    "water texture native allocation arguments", NULL, -1, -1);
    mc_client_water_texture_effect *owner = calloc(1, sizeof(*owner));
    if (owner == NULL)
        return fail(error, MC_WATER_TEXTURE_ALLOCATION_FAILED, "water texture native owner", NULL,
                    -1, -1);
    mc_registry_error cause = {0};
    if (mc_registry_classes_retain(registry, MC_REGISTRY_CLIENT, profile, &cause) !=
        MC_REGISTRY_OK) {
        free(owner);
        if (error != NULL)
            error->registry = cause;
        return fail(error, MC_WATER_TEXTURE_REGISTRY_FAILED, "water texture Registry namespace",
                    NULL, -1, -1);
    }
    owner->base.kind = MC_TEXTURE_EFFECT_WATER;
    owner->base.holds = 1;
    owner->registry = registry;
    owner->profile = profile;
    owner->maximum =
        options == NULL || options->max_array_bytes == 0 ? 5120 : options->max_array_bytes;
    *out = owner;
    return MC_WATER_TEXTURE_OK;
}

static void *array(mc_client_water_texture_effect *owner, size_t bytes, const char *source,
                   int32_t pc, int32_t vs_pc, mc_water_texture_error *error) {
    if (bytes > owner->maximum - owner->state.array_bytes) {
        fail(error, MC_WATER_TEXTURE_LIMIT, "water texture native array quota", source, pc, vs_pc);
        return NULL;
    }
    void *storage = calloc(1, bytes);
    if (storage == NULL) {
        fail(error, MC_WATER_TEXTURE_ALLOCATION_FAILED, "water texture array allocation", source,
             pc, vs_pc);
        return NULL;
    }
    owner->state.array_bytes += bytes;
    return storage;
}

mc_water_texture_status mc_client_water_texture_construct(mc_client_water_texture_effect *owner,
                                                          mc_water_texture_error *out) {
    mc_water_texture_error error = {.source_pc = -1, .vs_pc = -1};
    if (owner == NULL)
        fail(&error, MC_WATER_TEXTURE_INVALID_ARGUMENT, "water texture receiver", NULL, -1, -1);
    else if (owner->state.phase != MC_WATER_TEXTURE_RAW)
        fail(&error,
             owner->state.phase == MC_WATER_TEXTURE_CONSTRUCTING ? MC_WATER_TEXTURE_BUSY
                                                                 : MC_WATER_TEXTURE_INVALID_PHASE,
             "water texture constructor phase", NULL, -1, -1);
    else {
        owner->state.phase = MC_WATER_TEXTURE_CONSTRUCTING;
        const mc_block *block = NULL;
        if (mc_registry_block_static_get(owner->registry, "B", &block, &error.registry) !=
            MC_REGISTRY_OK)
            fail(&error, MC_WATER_TEXTURE_REGISTRY_FAILED, "vs actual uu.B active use", "vs", 1, 1);
        else if (block == NULL)
            fail(&error, MC_WATER_TEXTURE_NULL_REFERENCE, "vs null B texture receiver", "vs", 4, 4);
        else {
            const int32_t texture = mc_block_fields_get(block)->bm; /* vs4 argument before aw7. */
            mc_water_texture_fields *f = &owner->state.fields;
            mc_aw_error aw = {0};
            const mc_aw_slots aw_slots = {&f->a, &f->a_length, &f->b, &f->c, &f->d, &f->e, &f->f};
            const bool base_done = mc_texture_aw_construct(aw_slots, owner->maximum,
                                                           &owner->state.array_bytes, texture, &aw);
            if (!base_done)
                fail(&error, aw.limit ? MC_WATER_TEXTURE_LIMIT : MC_WATER_TEXTURE_ALLOCATION_FAILED,
                     aw.limit ? "water texture native array quota"
                              : "water texture array allocation",
                     "aw", 8, 7);
            if (base_done) {
                const float **slots[] = {&f->g, &f->h, &f->i, &f->j};
                size_t *lengths[] = {&f->g_length, &f->h_length, &f->i_length, &f->j_length};
                /* Exact vs newarray PCs. Their completed publications are
                 * g16,h25,i34,j43; instruction widths are not a stride. */
                static const int32_t newarray_pcs[] = {14, 23, 32, 41};
                for (size_t n = 0; n < 4; ++n) {
                    *slots[n] = array(owner, 256 * sizeof(float), "vs", newarray_pcs[n],
                                      newarray_pcs[n], &error);
                    if (*slots[n] == NULL)
                        break;
                    *lengths[n] = 256;
                }
                if (error.status == MC_WATER_TEXTURE_OK)
                    f->k = 0; /* vs48. */
            }
        }
        owner->state.phase =
            error.status == MC_WATER_TEXTURE_OK ? MC_WATER_TEXTURE_READY : MC_WATER_TEXTURE_FAILED;
    }
    if (out != NULL)
        *out = error;
    return error.status;
}

bool mc_client_water_texture_read(const mc_client_water_texture_effect *owner,
                                  mc_water_texture_state *out) {
    if (owner == NULL || out == NULL)
        return false;
    *out = owner->state;
    return true;
}

static bool animation_access(const void *storage, size_t length, size_t index, int32_t pc,
                             mc_water_texture_error *error) {
    if (storage == NULL) {
        fail(error, MC_WATER_TEXTURE_NULL_REFERENCE, "water animation null array", "vs", pc, pc);
        return false;
    }
    if (index >= length) {
        fail(error, MC_WATER_TEXTURE_INDEX_OUT_OF_RANGE, "water animation array index", "vs", pc,
             pc);
        return false;
    }
    return true;
}

static bool animation_load(const float *storage, size_t length, size_t index, int32_t pc,
                           float *value, mc_water_texture_error *error) {
    if (!animation_access(storage, length, index, pc, error))
        return false;
    *value = storage[index];
    return true;
}

static bool animation_store(const float *storage, size_t length, size_t index, float value,
                            int32_t pc, mc_water_texture_error *error) {
    if (!animation_access(storage, length, index, pc, error))
        return false;
    ((float *)storage)[index] = value; /* Real retained mutable array; view is readonly. */
    return true;
}

static bool animation_byte(const uint8_t *storage, size_t length, size_t index, int32_t value,
                           int32_t pc, mc_water_texture_error *error) {
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

mc_water_texture_status mc_client_water_texture_animate(mc_client_water_texture_effect *owner,
                                                        mc_water_texture_error *out) {
    mc_water_texture_error error = {.source_pc = -1, .vs_pc = -1};
    if (owner == NULL)
        fail(&error, MC_WATER_TEXTURE_INVALID_ARGUMENT, "water animation receiver", NULL, -1, -1);
    else if (owner->animating || owner->state.phase == MC_WATER_TEXTURE_CONSTRUCTING)
        fail(&error, MC_WATER_TEXTURE_BUSY, "water effect operation entered", NULL, -1, -1);
    else if (owner->state.phase != MC_WATER_TEXTURE_READY)
        fail(&error, MC_WATER_TEXTURE_INVALID_PHASE, "water animation READY receiver", NULL, -1,
             -1);
    else {
        owner->animating = true;
        owner->base.active = true;
        mc_water_texture_fields *f = &owner->state.fields;
        f->k = mc_java_i32_add(f->k, 1); /* dup k4, iadd6, putfield7. */
        for (int32_t x = 0; x < 16; ++x) {
            for (int32_t y = 0; y < 16; ++y) {
                float sum = 0.0f;
                for (int32_t neighbor = x - 1; neighbor <= x + 1; ++neighbor) {
                    const size_t index = ((uint32_t)neighbor & 15u) + (uint32_t)y * 16u;
                    float value;
                    if (!animation_load(f->g, f->g_length, index, 67, &value, &error))
                        goto finished;
                    sum = animation_add(sum, value); /* fadd68, binary32 fstore69. */
                }
                const size_t index = (size_t)x + (size_t)y * 16;
                const float *target = f->h; /* Nullable receiver77 before i load100. */
                const size_t target_length = f->h_length;
                const float divided = sum / 3.3f; /* fdiv89 before i read. */
                float value;
                if (!animation_load(f->i, f->i_length, index, 100, &value, &error))
                    goto finished;
                const float product = value * 0.8f;
                const float next = animation_add(divided, product);
                if (!animation_store(target, target_length, index, next, 105, &error))
                    goto finished;
            }
        }
        for (int32_t x = 0; x < 16; ++x) {
            for (int32_t y = 0; y < 16; ++y) {
                const size_t index = (size_t)x + (size_t)y * 16;
                const float *target = f->i; /* Captured i135 for dup2/fastore161. */
                const size_t target_length = f->i_length;
                float value, impulse;
                if (!animation_load(target, target_length, index, 145, &value, &error) ||
                    !animation_load(f->j, f->j_length, index, 156, &impulse, &error))
                    goto finished;
                const float product = impulse * 0.05f;
                const float next = animation_add(value, product);
                if (!animation_store(target, target_length, index, next, 161, &error) ||
                    !animation_load(f->i, f->i_length, index, 172, &value, &error))
                    goto finished;
                if (value < 0.0f && !animation_store(f->i, f->i_length, index, 0.0f, 189, &error))
                    goto finished;
                target = f->j; /* Captured j191, read201, decay then store205. */
                const size_t impulse_length = f->j_length;
                if (!animation_load(target, impulse_length, index, 201, &impulse, &error))
                    goto finished;
                const float decayed = impulse - 0.1f;
                if (!animation_store(target, impulse_length, index, decayed, 205, &error))
                    goto finished;
                double draw;
                error.random =
                    mc_java_math_random(owner->profile, &draw); /* Actual shared owner206. */
                if (error.random != MC_JAVA_RANDOM_PROFILE_OK) {
                    fail(&error, MC_WATER_TEXTURE_RANDOM_FAILED,
                         "water animation shared Math.random", "vs", 206, 206);
                    goto finished;
                }
                if (draw < 0.05 && !animation_store(f->j, f->j_length, index, 0.5f, 228, &error))
                    goto finished;
            }
        }
        const float *old_h = f->h; /* h242 -> local1; source publishes h before g. */
        const size_t old_h_length = f->h_length;
        f->h = f->g;
        f->h_length = f->g_length; /* putfield251 */
        f->g = old_h;
        f->g_length = old_h_length; /* putfield256 */
        for (size_t pixel = 0; pixel < 256; ++pixel) {
            float value;
            if (!animation_load(f->g, f->g_length, pixel, 273, &value, &error))
                goto finished;
            if (value > 1.0f)
                value = 1.0f;
            if (value < 0.0f)
                value = 0.0f;
            const float square = value * value;
            const float red_product = square * 32.0f;
            int32_t red = mc_java_float_to_i32(32.0f + red_product);
            const float green_product = square * 64.0f;
            int32_t green = mc_java_float_to_i32(50.0f + green_product);
            int32_t blue = 255;
            const float alpha_product = square * 50.0f;
            const int32_t alpha = mc_java_float_to_i32(146.0f + alpha_product);
            if (f->c) {
                int32_t weighted =
                    mc_java_i32_add(mc_java_i32_mul(red, 30), mc_java_i32_mul(green, 59));
                weighted = mc_java_i32_add(weighted, mc_java_i32_mul(blue, 11));
                int32_t converted_red, converted_green, converted_blue;
                (void)mc_java_i32_div(weighted, 100, &converted_red);
                weighted = mc_java_i32_add(mc_java_i32_mul(red, 30), mc_java_i32_mul(green, 70));
                (void)mc_java_i32_div(weighted, 100, &converted_green);
                weighted = mc_java_i32_add(mc_java_i32_mul(red, 30), mc_java_i32_mul(blue, 70));
                (void)mc_java_i32_div(weighted, 100, &converted_blue);
                red = converted_red;
                green = converted_green;
                blue = converted_blue;
            }
            if (!animation_byte(f->a, f->a_length, pixel * 4, red, 419, &error) ||
                !animation_byte(f->a, f->a_length, pixel * 4 + 1, green, 432, &error) ||
                !animation_byte(f->a, f->a_length, pixel * 4 + 2, blue, 445, &error) ||
                !animation_byte(f->a, f->a_length, pixel * 4 + 3, alpha, 458, &error))
                goto finished;
        }
    finished:
        owner->animating = false;
        owner->base.active = false;
    }
    if (out != NULL)
        *out = error;
    return error.status;
}

mc_water_texture_status mc_client_water_texture_set_anaglyph(mc_client_water_texture_effect *owner,
                                                             bool enabled,
                                                             mc_water_texture_error *out) {
    mc_water_texture_error error = {.source_pc = -1, .vs_pc = -1};
    if (owner == NULL)
        fail(&error, MC_WATER_TEXTURE_INVALID_ARGUMENT, "water aw.c field receiver", NULL, -1, -1);
    else if (owner->animating || owner->state.phase == MC_WATER_TEXTURE_CONSTRUCTING)
        fail(&error, MC_WATER_TEXTURE_BUSY, "water effect operation entered", NULL, -1, -1);
    else if (owner->state.phase != MC_WATER_TEXTURE_READY)
        fail(&error, MC_WATER_TEXTURE_INVALID_PHASE, "water aw.c READY field write", NULL, -1, -1);
    else
        owner->state.fields.c = enabled;
    if (out != NULL)
        *out = error;
    return error.status;
}

mc_water_texture_status mc_client_water_texture_retire(mc_client_water_texture_effect **slot,
                                                       mc_water_texture_error *error) {
    if (error != NULL)
        *error = (mc_water_texture_error){.source_pc = -1, .vs_pc = -1};
    if (slot == NULL)
        return fail(error, MC_WATER_TEXTURE_INVALID_ARGUMENT, "water texture retirement slot", NULL,
                    -1, -1);
    mc_client_water_texture_effect *owner = *slot;
    if (owner == NULL)
        return MC_WATER_TEXTURE_OK;
    if (owner->state.phase == MC_WATER_TEXTURE_CONSTRUCTING || owner->animating ||
        owner->base.active)
        return fail(error, MC_WATER_TEXTURE_BUSY, "water effect operation entered", NULL, -1, -1);
    if (owner->base.holds > 1) {
        --owner->base.holds;
        *slot = NULL;
        return MC_WATER_TEXTURE_OK;
    }
    mc_registry_error cause = {0};
    if (mc_registry_classes_release(owner->registry, &cause) != MC_REGISTRY_OK) {
        if (error != NULL)
            error->registry = cause;
        return fail(error, MC_WATER_TEXTURE_REGISTRY_FAILED, "water texture Registry release", NULL,
                    -1, -1);
    }
    const mc_water_texture_fields *f = &owner->state.fields;
    free((void *)f->a);
    free((void *)f->g);
    free((void *)f->h);
    free((void *)f->i);
    free((void *)f->j);
    free(owner);
    *slot = NULL;
    return MC_WATER_TEXTURE_OK;
}

mc_client_texture_effect *mc_client_water_texture_as_effect(mc_client_water_texture_effect *owner) {
    return owner == NULL ? NULL : &owner->base;
}
