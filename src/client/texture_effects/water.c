#include "mc/client/texture_effects/water.h"
#include "mc/block/registry/runtime.h"
#include <stdlib.h>
_Static_assert(sizeof(float) == 4, "vs constructor float array storage");

struct mc_client_water_texture_effect {
    mc_registry_classes *registry;
    mc_water_texture_state state;
    size_t maximum;
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
    owner->registry = registry;
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
    if (owner->state.phase == MC_WATER_TEXTURE_CONSTRUCTING)
        return fail(error, MC_WATER_TEXTURE_BUSY, "water texture constructor entered", NULL, -1,
                    -1);
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
