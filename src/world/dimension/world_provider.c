#include "mc/world/dimension/world_provider.h"

#include "mc/block/registry/runtime.h"
#include "mc/core/java/java.h"
#include "mc/world/dimension/world_chunk_manager.h"
#include "mc/world/runtime/blocks.h"
#include "mc/world/runtime/runtime.h"

#include "../../block/registry/registry_internal.h"
#include "../../block/registry/runtime_internal.h"
#include "../biome/biome_internal.h"
#include "../biome/sampling_internal.h"
#include "../runtime/runtime_internal.h"
#include "world_provider_internal.h"

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>

#if !defined(_M_X64) && !defined(_M_AMD64) && !defined(__x86_64__)
#error "Dimension numeric construction requires x86-64/SSE2"
#endif
#if !defined(_MSC_VER) && !defined(__SSE2__)
#error "Dimension numeric construction requires SSE2"
#endif
#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) ||       \
    (defined(_M_FP_FAST) && _M_FP_FAST)
#error "Dimension numeric construction requires strict floating transformations"
#endif
#include <emmintrin.h>

_Static_assert(CHAR_BIT == 8 && sizeof(float) == sizeof(uint32_t) &&
                   sizeof(double) == sizeof(uint64_t),
               "binary32/binary64 storage required");
_Static_assert(FLT_RADIX == 2 && FLT_MANT_DIG == 24 && FLT_MAX_EXP == 128 && DBL_MANT_DIG == 53 &&
                   DBL_MAX_EXP == 1024 && FLT_EVAL_METHOD == 0,
               "strict binary32/binary64 evaluation required");

typedef struct dimension_ledger {
    size_t holds, live_bytes, max_bytes, buffer_limit;
} dimension_ledger;

struct mc_world_chunk_manager {
    dimension_ledger *ledger;
    size_t holds, bytes;
    bool fixed;
    mc_registry_classes *registry;
    mc_biome_reference_array *reference_d;
};

typedef struct noise_backed_biome {
    mc_world_chunk_manager base;
    mc_biome_memory memory;
    mc_biome_manager manager;
} noise_backed_biome;

typedef struct fixed_biome {
    mc_world_chunk_manager base;
    mc_biome_fixed_storage storage;
    mc_biome_id projection_e;
} fixed_biome;

struct mc_world_provider {
    dimension_ledger *ledger;
    size_t holds;
    mc_world_provider_kind kind;
    mc_world_provider_phase phase;
    unsigned int constructor_pc, bind_pc, setup_pc;
    mc_world_runtime *a;
    mc_world_chunk_manager *b;
    bool c, d, e;
    float *f;
    int32_t g;
    float *h;
};

static mc_world_provider_status result(mc_world_provider_error *error,
                                       mc_world_provider_status status, const char *operation,
                                       unsigned int bytecode) {
    if (error != NULL)
        *error = (mc_world_provider_error){
            .status = status, .operation = operation, .bytecode = bytecode};
    return status;
}

static bool valid_kind(mc_world_provider_kind kind) {
    return kind == MC_WORLD_PROVIDER_OVERWORLD || kind == MC_WORLD_PROVIDER_NETHER ||
           kind == MC_WORLD_PROVIDER_SKY;
}

static bool valid_fp(void) {
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u;
}

static mc_world_provider_status request(dimension_ledger *ledger, size_t bytes, void **out) {
    if (bytes > ledger->max_bytes - ledger->live_bytes)
        return MC_WORLD_PROVIDER_LIMIT;
    void *allocation = malloc(bytes);
    if (allocation == NULL)
        return MC_WORLD_PROVIDER_ALLOCATION_FAILED;
    ledger->live_bytes += bytes;
    *out = allocation;
    return MC_WORLD_PROVIDER_OK;
}

static void retire(dimension_ledger *ledger, void *allocation, size_t bytes) {
    free(allocation);
    ledger->live_bytes -= bytes;
}

static void ledger_release(dimension_ledger *ledger) {
    --ledger->holds;
    if (ledger->holds == 0)
        free(ledger);
}

const char *mc_world_provider_status_string(mc_world_provider_status status) {
    switch (status) {
        case MC_WORLD_PROVIDER_OK:
            return "ok";
        case MC_WORLD_PROVIDER_INVALID_ARGUMENT:
            return "invalid argument";
        case MC_WORLD_PROVIDER_INVALID_PHASE:
            return "invalid phase";
        case MC_WORLD_PROVIDER_LIMIT:
            return "resource limit";
        case MC_WORLD_PROVIDER_ALLOCATION_FAILED:
            return "allocation failed";
        case MC_WORLD_PROVIDER_WORLD_FAILED:
            return "world metadata failed";
        case MC_WORLD_CHUNK_MANAGER_FAILED:
            return "biome construction failed";
        case MC_WORLD_PROVIDER_UNSUPPORTED_FP:
            return "unsupported floating-point environment";
        case MC_WORLD_PROVIDER_REGISTRY_FAILED:
            return "block registry failed";
        case MC_WORLD_PROVIDER_MATH_FAILED:
            return "MathHelper class failed";
    }
    return "unknown dimension status";
}

mc_world_provider_limits mc_world_provider_default_limits(void) {
    return (mc_world_provider_limits){.max_direct_allocation_bytes = 4u * 1024u * 1024u,
                                      .max_biome_buffer_bytes = MC_BIOME_DEFAULT_ALLOCATION_BYTES};
}

mc_world_provider_status mc_world_provider_allocate(mc_world_provider_kind kind,
                                                    const mc_world_provider_limits *limits,
                                                    mc_world_provider **out,
                                                    mc_world_provider_error *error) {
    if (!valid_kind(kind) || out == NULL)
        return result(error, MC_WORLD_PROVIDER_INVALID_ARGUMENT, "dimension allocation", 0);
    mc_world_provider_limits admitted =
        limits == NULL ? mc_world_provider_default_limits() : *limits;
    if (sizeof(dimension_ledger) > admitted.max_direct_allocation_bytes ||
        sizeof(mc_world_provider) > admitted.max_direct_allocation_bytes - sizeof(dimension_ledger))
        return result(error, MC_WORLD_PROVIDER_LIMIT, "dimension allocation", 0);
    dimension_ledger *ledger = malloc(sizeof(*ledger));
    if (ledger == NULL)
        return result(error, MC_WORLD_PROVIDER_ALLOCATION_FAILED, "dimension ledger allocation", 0);
    *ledger = (dimension_ledger){.holds = 1,
                                 .live_bytes = sizeof(*ledger),
                                 .max_bytes = admitted.max_direct_allocation_bytes,
                                 .buffer_limit = admitted.max_biome_buffer_bytes};
    void *allocation;
    mc_world_provider_status status = request(ledger, sizeof(mc_world_provider), &allocation);
    if (status != MC_WORLD_PROVIDER_OK) {
        ledger_release(ledger);
        return result(error, status, "dimension allocation", 0);
    }
    mc_world_provider *dimension = allocation;
    *dimension = (mc_world_provider){
        .ledger = ledger, .holds = 1, .kind = kind, .phase = MC_WORLD_PROVIDER_RAW};
    *out = dimension;
    return result(error, MC_WORLD_PROVIDER_OK, NULL, 0);
}

mc_world_provider_status mc_world_provider_construct(mc_world_provider *raw,
                                                     mc_world_provider_error *error) {
    if (raw == NULL)
        return result(error, MC_WORLD_PROVIDER_INVALID_ARGUMENT, "dimension constructor", 0);
    if (raw->phase != MC_WORLD_PROVIDER_RAW)
        return result(error, MC_WORLD_PROVIDER_INVALID_PHASE, "dimension constructor", 0);
    raw->phase = MC_WORLD_PROVIDER_CONSTRUCTING;
    raw->c = false;
    raw->constructor_pc = 6;
    raw->d = false;
    raw->constructor_pc = 11;
    raw->e = false;
    raw->constructor_pc = 16;
    void *allocation;
    mc_world_provider_status status = request(raw->ledger, 16u * sizeof(float), &allocation);
    if (status != MC_WORLD_PROVIDER_OK) {
        raw->phase = MC_WORLD_PROVIDER_FAILED_CONSTRUCTION;
        return result(error, status, "dimension brightness allocation", 22);
    }
    float *brightness = allocation;
    for (size_t i = 0; i < 16u; ++i)
        brightness[i] = 0.0f;
    raw->f = brightness;
    raw->constructor_pc = 24;
    raw->g = 0;
    raw->constructor_pc = 29;
    status = request(raw->ledger, 4u * sizeof(float), &allocation);
    if (status != MC_WORLD_PROVIDER_OK) {
        raw->phase = MC_WORLD_PROVIDER_FAILED_CONSTRUCTION;
        return result(error, status, "dimension color allocation", 34);
    }
    float *colors = allocation;
    for (size_t i = 0; i < 4u; ++i)
        colors[i] = 0.0f;
    raw->h = colors;
    raw->constructor_pc = 36;
    raw->phase = MC_WORLD_PROVIDER_READY;
    raw->constructor_pc = 39;
    return result(error, MC_WORLD_PROVIDER_OK, NULL, 0);
}

mc_world_provider_status mc_world_provider_create(int32_t dimension_id,
                                                  const mc_world_provider_limits *limits,
                                                  mc_world_provider **out,
                                                  mc_world_provider_error *error) {
    if (out == NULL)
        return result(error, MC_WORLD_PROVIDER_INVALID_ARGUMENT, "dimension factory", 0);
    mc_world_provider_kind kind;
    if (dimension_id == -1)
        kind = MC_WORLD_PROVIDER_NETHER;
    else if (dimension_id == 0)
        kind = MC_WORLD_PROVIDER_OVERWORLD;
    else if (dimension_id == 1)
        kind = MC_WORLD_PROVIDER_SKY;
    else {
        *out = NULL;
        return result(error, MC_WORLD_PROVIDER_OK, NULL, 0);
    }
    mc_world_provider *dimension;
    mc_world_provider_status status = mc_world_provider_allocate(kind, limits, &dimension, error);
    if (status != MC_WORLD_PROVIDER_OK)
        return status;
    status = mc_world_provider_construct(dimension, error);
    if (status != MC_WORLD_PROVIDER_OK) {
        mc_world_provider_release(dimension);
        return status;
    }
    *out = dimension;
    return MC_WORLD_PROVIDER_OK;
}

mc_world_provider_status mc_world_provider_retain(mc_world_provider *dimension) {
    if (dimension == NULL)
        return MC_WORLD_PROVIDER_INVALID_ARGUMENT;
    if (dimension->holds == SIZE_MAX)
        return MC_WORLD_PROVIDER_LIMIT;
    ++dimension->holds;
    return MC_WORLD_PROVIDER_OK;
}

float *mc_world_provider_sunrise_storage(mc_world_provider *dimension) {
    return dimension == NULL ? NULL : dimension->h;
}

void mc_world_provider_release(mc_world_provider *dimension) {
    if (dimension == NULL)
        return;
    --dimension->holds;
    if (dimension->holds != 0)
        return;
    dimension_ledger *ledger = dimension->ledger;
    mc_world_chunk_manager_release(dimension->b);
    mc_world_provider_unbind(dimension);
    if (dimension->f != NULL)
        retire(ledger, dimension->f, 16u * sizeof(float));
    if (dimension->h != NULL)
        retire(ledger, dimension->h, 4u * sizeof(float));
    retire(ledger, dimension, sizeof(*dimension));
    ledger_release(ledger);
}

mc_world_provider_status mc_world_chunk_manager_retain(mc_world_chunk_manager *biome) {
    if (biome == NULL)
        return MC_WORLD_PROVIDER_INVALID_ARGUMENT;
    if (biome->holds == SIZE_MAX)
        return MC_WORLD_PROVIDER_LIMIT;
    ++biome->holds;
    return MC_WORLD_PROVIDER_OK;
}

void mc_world_chunk_manager_release(mc_world_chunk_manager *biome) {
    if (biome == NULL)
        return;
    --biome->holds;
    if (biome->holds != 0)
        return;
    dimension_ledger *ledger = biome->ledger;
    size_t bytes = biome->bytes;
    if (!biome->fixed) {
        mc_biome_manager_free(&((noise_backed_biome *)biome)->manager);
    } else {
        mc_biome_fixed_storage *storage = &((fixed_biome *)biome)->storage;
        mc_biome_double_buffer_free(&storage->a);
        mc_biome_double_buffer_free(&storage->b);
        mc_biome_double_buffer_free(&storage->c);
        mc_biome_buffer_free(&storage->numerical_d);
    }
    /* Each GAME manager owns exactly this borrow. Retirement cannot invoke
     * class use or callbacks and is valid even inside another entered flow. */
    if (biome->registry != NULL)
        (void)mc_registry_runtime_borrow_release(biome->registry);
    retire(ledger, biome, bytes);
    ledger_release(ledger);
}

mc_world_provider_status mc_world_provider_fields_get(const mc_world_provider *dimension,
                                                      mc_world_provider_fields *out) {
    if (dimension == NULL || out == NULL)
        return MC_WORLD_PROVIDER_INVALID_ARGUMENT;
    *out = (mc_world_provider_fields){.a = dimension->a,
                                      .b = dimension->b,
                                      .c = dimension->c,
                                      .d = dimension->d,
                                      .e = dimension->e,
                                      .f = dimension->f,
                                      .g = dimension->g,
                                      .h = dimension->h};
    return MC_WORLD_PROVIDER_OK;
}

mc_world_provider_status mc_world_provider_state_get(const mc_world_provider *dimension,
                                                     mc_world_provider_state *out) {
    if (dimension == NULL || out == NULL)
        return MC_WORLD_PROVIDER_INVALID_ARGUMENT;
    *out = (mc_world_provider_state){.kind = dimension->kind,
                                     .phase = dimension->phase,
                                     .constructor_pc = dimension->constructor_pc,
                                     .bind_pc = dimension->bind_pc,
                                     .setup_pc = dimension->setup_pc,
                                     .holds = dimension->holds,
                                     .direct_allocation_bytes = dimension->ledger->live_bytes};
    return MC_WORLD_PROVIDER_OK;
}

mc_world_runtime *mc_world_provider_world(const mc_world_provider *dimension) {
    return dimension == NULL ? NULL : dimension->a;
}

size_t mc_world_provider_hold_count(const mc_world_provider *dimension) {
    return dimension == NULL ? 0 : dimension->holds;
}

void mc_world_provider_unbind(mc_world_provider *dimension) {
    if (dimension != NULL) {
        mc_world_runtime *previous = dimension->a;
        dimension->a = NULL;
        mc_world_runtime_binding_release(previous);
    }
}

mc_world_provider_status mc_world_chunk_manager_fields_get(const mc_world_chunk_manager *biome,
                                                           mc_world_chunk_manager_fields *out) {
    if (biome == NULL || out == NULL)
        return MC_WORLD_PROVIDER_INVALID_ARGUMENT;
    mc_world_chunk_manager_fields fields = {.fixed = biome->fixed, .holds = biome->holds};
    if (biome->fixed) {
        const fixed_biome *fixed = (const fixed_biome *)biome;
        fields.a = &fixed->storage.a;
        fields.b = &fixed->storage.b;
        fields.c = &fixed->storage.c;
        fields.d = &fixed->storage.numerical_d;
        fields.fixed_e = fixed->projection_e;
        fields.fixed_f = fixed->storage.f;
        fields.fixed_g = fixed->storage.g;
    } else {
        noise_backed_biome *noise_backed = (noise_backed_biome *)(void *)biome;
        fields.noise_backed = &noise_backed->manager;
        fields.a = &noise_backed->manager.a;
        fields.b = &noise_backed->manager.b;
        fields.c = &noise_backed->manager.c;
        fields.d = &noise_backed->manager.d;
    }
    *out = fields;
    return MC_WORLD_PROVIDER_OK;
}

static mc_world_provider_status new_biome(mc_world_provider *dimension, bool fixed,
                                          mc_world_chunk_manager **out) {
    dimension_ledger *ledger = dimension->ledger;
    if (ledger->holds == SIZE_MAX)
        return MC_WORLD_PROVIDER_LIMIT;
    size_t bytes = fixed ? sizeof(fixed_biome) : sizeof(noise_backed_biome);
    void *allocation;
    mc_world_provider_status status = request(ledger, bytes, &allocation);
    if (status != MC_WORLD_PROVIDER_OK)
        return status;
    ++ledger->holds;
    mc_world_chunk_manager base = {.ledger = ledger, .holds = 1, .bytes = bytes, .fixed = fixed};
    if (fixed) {
        fixed_biome *biome = allocation;
        *biome = (fixed_biome){.base = base};
        (void)mc_biome_memory_init(&biome->storage.memory, ledger->buffer_limit);
        *out = &biome->base;
    } else {
        noise_backed_biome *biome = allocation;
        *biome = (noise_backed_biome){.base = base};
        *out = &biome->base;
    }
    return MC_WORLD_PROVIDER_OK;
}

mc_world_provider_status mc_world_chunk_manager_fixed_create_game(
    mc_world_provider *allocation_owner, const mc_world_provider_registry_binding *binding,
    const mc_biome *selected, double temperature, double rainfall, mc_world_chunk_manager **out,
    mc_world_provider_error *error) {
    if (allocation_owner == NULL || binding == NULL || out == NULL ||
        (selected != NULL && selected->classes != binding->classes))
        return result(error, MC_WORLD_PROVIDER_INVALID_ARGUMENT,
                      "fixed GAME biome constructor arguments", 0);
    const mc_world_provider_registry_binding captured = *binding;
    mc_world_provider_status status = mc_world_provider_retain(allocation_owner);
    if (status != MC_WORLD_PROVIDER_OK)
        return result(error, status, "fixed GAME biome allocation owner hold", 0);
    mc_registry_error registry_error = {0};
    if (mc_registry_classes_retain(captured.classes, captured.side, captured.profile,
                                   &registry_error) != MC_REGISTRY_OK) {
        status = result(error,
                        registry_error.status == MC_REGISTRY_INVALID_ARGUMENT
                            ? MC_WORLD_PROVIDER_INVALID_ARGUMENT
                            : MC_WORLD_PROVIDER_REGISTRY_FAILED,
                        "fixed GAME biome registry borrow", 0);
        if (error != NULL)
            error->registry = registry_error;
        mc_world_provider_release(allocation_owner);
        return status;
    }
    mc_world_chunk_manager *replacement = NULL;
    status = new_biome(allocation_owner, true, &replacement);
    if (status != MC_WORLD_PROVIDER_OK) {
        (void)mc_registry_runtime_borrow_release(captured.classes);
        mc_world_provider_release(allocation_owner);
        return result(error, status, "fixed GAME biome allocation", 0);
    }
    replacement->registry = captured.classes;
    fixed_biome *biome = (fixed_biome *)replacement;
    /* XV/PH's protected constructor only invokes Object1 and returns4.
     * HD/ET then stores its actual declaring fields at6/11/17. No class
     * active use, World access, numerical operation or entropy is reached. */
    biome->storage.e = selected;
    biome->storage.f = temperature;
    biome->storage.g = rainfall;
    if (selected != NULL) {
        for (size_t i = 0; i < 13u; ++i)
            if (selected == captured.classes->biomes.named[i]) {
                biome->projection_e = (mc_biome_id)(i + 1u);
                break;
            }
    }
    *out = replacement;
    mc_world_provider_release(allocation_owner);
    return result(error, MC_WORLD_PROVIDER_OK, "fixed GAME biome constructor return", 20);
}

mc_world_provider_status mc_world_provider_assign_chunk_manager_game(
    mc_world_provider *dimension, mc_world_chunk_manager *biome, mc_world_provider_error *error) {
    if (dimension == NULL)
        return result(error, MC_WORLD_PROVIDER_INVALID_ARGUMENT, "dimension GAME biome assignment",
                      0);
    if (dimension->phase != MC_WORLD_PROVIDER_READY)
        return result(error, MC_WORLD_PROVIDER_INVALID_PHASE, "dimension GAME biome assignment", 0);
    if (biome != NULL) {
        mc_registry_classes *expected = mc_world_runtime_registry(dimension->a);
        if (expected == NULL && dimension->b != NULL)
            expected = dimension->b->registry;
        if (biome->registry == NULL || (expected != NULL && biome->registry != expected))
            return result(error, MC_WORLD_PROVIDER_INVALID_ARGUMENT,
                          "dimension GAME biome assignment registry", 0);
    }
    if (biome == dimension->b)
        return result(error, MC_WORLD_PROVIDER_OK, NULL, 0);
    if (biome != NULL) {
        const mc_world_provider_status status = mc_world_chunk_manager_retain(biome);
        if (status != MC_WORLD_PROVIDER_OK)
            return result(error, status, "dimension GAME assigned biome hold", 0);
    }
    mc_world_chunk_manager *previous = dimension->b;
    dimension->b = biome;
    mc_world_chunk_manager_release(previous);
    return result(error, MC_WORLD_PROVIDER_OK, NULL, 0);
}

mc_world_provider_status mc_world_provider_initialize_light_table(mc_world_provider *dimension,
                                                                  mc_world_provider_error *error) {
    if (dimension == NULL)
        return result(error, MC_WORLD_PROVIDER_INVALID_ARGUMENT, "dimension light table", 0);
    if (dimension->phase != MC_WORLD_PROVIDER_READY || dimension->f == NULL)
        return result(error, MC_WORLD_PROVIDER_INVALID_PHASE, "dimension light table", 0);
    if (!valid_fp())
        return result(error, MC_WORLD_PROVIDER_UNSUPPORTED_FP, "dimension light FP admission", 0);
    float ambient = dimension->kind == MC_WORLD_PROVIDER_NETHER ? 0.1f : 0.05f;
    for (int32_t i = 0; i <= 15; ++i) {
        volatile float fraction = (float)i;
        fraction = fraction / 15.0f;
        fraction = 1.0f - fraction;
        volatile float numerator = 1.0f - fraction;
        volatile float denominator = fraction * 3.0f;
        denominator = denominator + 1.0f;
        volatile float value = numerator / denominator;
        volatile float range = 1.0f - ambient;
        value = value * range;
        value = value + ambient;
        dimension->f[i] = value;
    }
    return result(error, MC_WORLD_PROVIDER_OK, NULL, 0);
}

mc_world_provider_status mc_world_provider_bind(mc_world_provider *dimension,
                                                mc_world_runtime *world,
                                                mc_world_provider_error *error) {
    if (dimension == NULL)
        return result(error, MC_WORLD_PROVIDER_INVALID_ARGUMENT, "dimension bind", 0);
    if (dimension->phase != MC_WORLD_PROVIDER_READY)
        return result(error, MC_WORLD_PROVIDER_INVALID_PHASE, "dimension bind", 0);
    if (!valid_fp())
        return result(error, MC_WORLD_PROVIDER_UNSUPPORTED_FP, "dimension bind FP admission", 0);
    if (world != dimension->a) {
        if (world != NULL && !mc_world_runtime_binding_retain(world)) {
            mc_world_storage_error world_error = *mc_world_runtime_last_error(world);
            mc_world_provider_status status = world_error.status == MC_WORLD_STORAGE_LIMIT
                                                  ? MC_WORLD_PROVIDER_LIMIT
                                                  : MC_WORLD_PROVIDER_WORLD_FAILED;
            (void)result(error, status, "dimension World retain", 2);
            if (error != NULL)
                error->world = world_error;
            return status;
        }
        mc_world_runtime *previous = dimension->a;
        dimension->a = world;
        dimension->bind_pc = 2;
        mc_world_runtime_binding_release(previous);
    } else {
        dimension->a = world;
        dimension->bind_pc = 2;
    }
    dimension->setup_pc = 0;
    mc_world_chunk_manager *replacement;
    bool fixed = dimension->kind != MC_WORLD_PROVIDER_OVERWORLD;
    mc_world_provider_status status = new_biome(dimension, fixed, &replacement);
    if (status != MC_WORLD_PROVIDER_OK)
        return result(error, status, "dimension biome allocation", 1);
    if (fixed) {
        fixed_biome *biome = (fixed_biome *)replacement;
        biome->projection_e =
            dimension->kind == MC_WORLD_PROVIDER_NETHER ? MC_BIOME_HELL : MC_BIOME_SKY;
        biome->storage.f = dimension->kind == MC_WORLD_PROVIDER_NETHER ? 1.0 : 0.5;
        biome->storage.g = 0.0;
    } else {
        noise_backed_biome *biome = (noise_backed_biome *)replacement;
        mc_biome_status biome_status =
            mc_biome_memory_init(&biome->memory, dimension->ledger->buffer_limit);
        for (unsigned int i = 0; i < 3u && biome_status == MC_BIOME_OK; ++i) {
            mc_level_info info;
            mc_world_storage_error world_error;
            mc_world_storage_status world_status =
                mc_world_runtime_read_info(dimension->a, &info, &world_error);
            if (world_status != MC_WORLD_STORAGE_OK) {
                mc_world_chunk_manager_release(replacement);
                status = result(error, MC_WORLD_PROVIDER_WORLD_FAILED, "biome World seed",
                                14u + i * 27u);
                if (error != NULL)
                    error->world = world_error;
                return status;
            }
            biome_status = mc_biome_manager_construct_channel(&biome->manager, info.seed,
                                                              (mc_biome_construction_channel)i);
        }
        if (biome_status == MC_BIOME_OK)
            biome_status = mc_biome_manager_finish_construction(&biome->manager, &biome->memory);
        if (biome_status != MC_BIOME_OK) {
            mc_world_chunk_manager_release(replacement);
            status =
                result(error, MC_WORLD_CHUNK_MANAGER_FAILED, "dimension biome numerical init", 6);
            if (error != NULL)
                error->biome = biome_status;
            return status;
        }
    }
    mc_world_chunk_manager *previous = dimension->b;
    dimension->b = replacement;
    dimension->setup_pc = fixed ? (dimension->kind == MC_WORLD_PROVIDER_NETHER ? 13u : 15u) : 12u;
    mc_world_chunk_manager_release(previous);
    if (dimension->kind == MC_WORLD_PROVIDER_NETHER) {
        dimension->c = true;
        dimension->setup_pc = 18;
        dimension->d = true;
        dimension->setup_pc = 23;
        dimension->e = true;
        dimension->setup_pc = 28;
        dimension->g = -1;
        dimension->setup_pc = 33;
    } else if (dimension->kind == MC_WORLD_PROVIDER_SKY) {
        dimension->g = 1;
        dimension->setup_pc = 20;
    }
    dimension->bind_pc = 6;
    status = mc_world_provider_initialize_light_table(dimension, error);
    if (status != MC_WORLD_PROVIDER_OK)
        return status;
    dimension->bind_pc = 10;
    dimension->bind_pc = 13;
    return result(error, MC_WORLD_PROVIDER_OK, NULL, 0);
}

mc_world_provider_status
mc_world_provider_bind_game(mc_world_provider *dimension, mc_world_runtime *world,
                            const mc_world_provider_registry_binding *binding,
                            mc_world_provider_error *error) {
    if (dimension == NULL || binding == NULL)
        return result(error, MC_WORLD_PROVIDER_INVALID_ARGUMENT, "dimension GAME bind", 0);
    if (dimension->phase != MC_WORLD_PROVIDER_READY)
        return result(error, MC_WORLD_PROVIDER_INVALID_PHASE, "dimension GAME bind", 0);
    if (!valid_fp())
        return result(error, MC_WORLD_PROVIDER_UNSUPPORTED_FP, "dimension GAME bind FP admission",
                      0);
    const mc_world_provider_registry_binding captured = *binding;
    if (world != NULL && mc_world_runtime_registry(world) != captured.classes)
        return result(error, MC_WORLD_PROVIDER_INVALID_ARGUMENT, "dimension GAME World registry",
                      0);

    mc_world_provider_status status = mc_world_provider_retain(dimension);
    if (status != MC_WORLD_PROVIDER_OK)
        return result(error, status, "dimension GAME call hold", 0);
    mc_world_chunk_manager *old = dimension->b;
    bool old_held = false, registry_owned = false, world_entered = false;
    mc_world_chunk_manager *replacement = NULL;
    if (old != NULL) {
        status = mc_world_chunk_manager_retain(old);
        if (status != MC_WORLD_PROVIDER_OK) {
            (void)result(error, status, "dimension GAME preceding manager hold", 0);
            goto leave;
        }
        old_held = true;
    }
    mc_registry_error registry_error = {0};
    if (mc_registry_classes_retain(captured.classes, captured.side, captured.profile,
                                   &registry_error) != MC_REGISTRY_OK) {
        status = result(error,
                        registry_error.status == MC_REGISTRY_INVALID_ARGUMENT
                            ? MC_WORLD_PROVIDER_INVALID_ARGUMENT
                            : MC_WORLD_PROVIDER_REGISTRY_FAILED,
                        "dimension GAME registry borrow", 0);
        if (error != NULL)
            error->registry = registry_error;
        goto leave;
    }
    registry_owned = true;
    if (world != NULL) {
        mc_world_storage_error world_error = {0};
        if (mc_world_runtime_call_enter(world, &world_error) != MC_WORLD_STORAGE_OK) {
            status = result(error, MC_WORLD_PROVIDER_WORLD_FAILED,
                            "dimension GAME World call admission", 0);
            if (error != NULL)
                error->world = world_error;
            goto leave;
        }
        world_entered = true;
    }

    if (world != dimension->a) {
        if (world != NULL && !mc_world_runtime_binding_retain(world)) {
            mc_world_storage_error world_error = *mc_world_runtime_last_error(world);
            status = result(error,
                            world_error.status == MC_WORLD_STORAGE_LIMIT
                                ? MC_WORLD_PROVIDER_LIMIT
                                : MC_WORLD_PROVIDER_WORLD_FAILED,
                            "dimension GAME World retain", 2);
            if (error != NULL)
                error->world = world_error;
            goto leave;
        }
        mc_world_runtime *previous = dimension->a;
        dimension->a = world;
        dimension->bind_pc = 2;
        mc_world_runtime_binding_release(previous);
    } else {
        dimension->a = world;
        dimension->bind_pc = 2;
    }
    dimension->setup_pc = 0;
    const bool fixed = dimension->kind != MC_WORLD_PROVIDER_OVERWORLD;
    status = new_biome(dimension, fixed, &replacement);
    if (status != MC_WORLD_PROVIDER_OK) {
        (void)result(error, status, "dimension GAME biome allocation", 1);
        goto leave;
    }
    replacement->registry = captured.classes;
    registry_owned = false;
    if (fixed) {
        fixed_biome *biome = (fixed_biome *)replacement;
        const mc_biome *selected = NULL;
        dimension->setup_pc = 5;
        if (mc_biome_static_get(captured.classes,
                                dimension->kind == MC_WORLD_PROVIDER_NETHER ? 'l' : 'm', &selected,
                                &registry_error) != MC_REGISTRY_OK) {
            status = result(error, MC_WORLD_PROVIDER_REGISTRY_FAILED,
                            "dimension GAME fixed Biome static", 5);
            if (error != NULL)
                error->registry = registry_error;
            goto leave;
        }
        /* Constructor arguments were evaluated before the virtual call. A
         * synchronous callback can replace Dimension.b; it cannot substitute
         * this allocated receiver or the selected actual Biome reference. */
        biome->storage.e = selected;
        biome->storage.f = dimension->kind == MC_WORLD_PROVIDER_NETHER ? 1.0 : 0.5;
        biome->storage.g = 0.0;
        biome->projection_e =
            dimension->kind == MC_WORLD_PROVIDER_NETHER ? MC_BIOME_HELL : MC_BIOME_SKY;
    } else {
        noise_backed_biome *biome = (noise_backed_biome *)replacement;
        /* noise-backed setup evaluates Dimension.a once at6. The constructor then
         * performs three separate seed reads on that captured World. */
        mc_world_runtime *constructor_world = dimension->a;
        mc_biome_status biome_status =
            mc_biome_memory_init(&biome->memory, dimension->ledger->buffer_limit);
        for (unsigned int i = 0; i < 3u && biome_status == MC_BIOME_OK; ++i) {
            mc_level_info info;
            mc_world_storage_error world_error = {0};
            if (mc_world_runtime_read_info(constructor_world, &info, &world_error) !=
                MC_WORLD_STORAGE_OK) {
                status = result(error, MC_WORLD_PROVIDER_WORLD_FAILED, "GAME biome World seed",
                                14u + i * 27u);
                if (error != NULL)
                    error->world = world_error;
                goto leave;
            }
            biome_status = mc_biome_manager_construct_channel(&biome->manager, info.seed,
                                                              (mc_biome_construction_channel)i);
        }
        if (biome_status == MC_BIOME_OK)
            biome_status = mc_biome_manager_finish_construction(&biome->manager, &biome->memory);
        if (biome_status != MC_BIOME_OK) {
            status = result(error, MC_WORLD_CHUNK_MANAGER_FAILED,
                            "dimension GAME biome numerical init", 9);
            if (error != NULL)
                error->biome = biome_status;
            goto leave;
        }
    }

    /* Publish over the current field, not over the earlier captured manager:
     * a registry resource callback may have completed a numerical rebind. */
    mc_world_chunk_manager *previous = dimension->b;
    dimension->b = replacement;
    replacement = NULL;
    dimension->setup_pc = fixed ? (dimension->kind == MC_WORLD_PROVIDER_NETHER ? 13u : 15u) : 12u;
    mc_world_chunk_manager_release(previous);
    if (dimension->kind == MC_WORLD_PROVIDER_NETHER) {
        dimension->c = true;
        dimension->setup_pc = 18;
        dimension->d = true;
        dimension->setup_pc = 23;
        dimension->e = true;
        dimension->setup_pc = 28;
        dimension->g = -1;
        dimension->setup_pc = 33;
    } else if (dimension->kind == MC_WORLD_PROVIDER_SKY) {
        dimension->g = 1;
        dimension->setup_pc = 20;
    }
    dimension->bind_pc = 6;
    status = mc_world_provider_initialize_light_table(dimension, error);
    if (status != MC_WORLD_PROVIDER_OK)
        goto leave;
    dimension->bind_pc = 10;
    dimension->bind_pc = 13;
    status = result(error, MC_WORLD_PROVIDER_OK, NULL, 0);
leave:
    mc_world_chunk_manager_release(replacement);
    if (registry_owned)
        (void)mc_registry_runtime_borrow_release(captured.classes);
    if (world_entered)
        mc_world_runtime_call_leave(world);
    if (old_held)
        mc_world_chunk_manager_release(old);
    mc_world_provider_release(dimension);
    return status;
}

mc_world_provider_status
mc_world_chunk_manager_game_fields_get(const mc_world_chunk_manager *biome,
                                       mc_world_chunk_manager_game_fields *out) {
    if (biome == NULL || out == NULL)
        return MC_WORLD_PROVIDER_INVALID_ARGUMENT;
    *out = (mc_world_chunk_manager_game_fields){
        .registry = biome->registry,
        .d = biome->reference_d,
        .fixed_e = biome->fixed ? ((const fixed_biome *)biome)->storage.e : NULL};
    return MC_WORLD_PROVIDER_OK;
}

static mc_world_provider_status biome_native_result(mc_world_chunk_manager_error *error,
                                                    mc_world_provider_status status,
                                                    mc_registry_status registry_status,
                                                    mc_biome_status climate_status,
                                                    const char *operation, unsigned int bytecode) {
    if (error != NULL)
        *error =
            (mc_world_chunk_manager_error){.status = status,
                                           .operation = operation,
                                           .bytecode = bytecode,
                                           .sampling = {.registry = {.status = registry_status,
                                                                     .message = operation,
                                                                     .initializer_pc = -1,
                                                                     .block_id = -1,
                                                                     .constructor_pc = -1,
                                                                     .active_initializer_pc = -1,
                                                                     .item_argument = -1},
                                                        .climate_status = climate_status,
                                                        .sampler_pc = -1}};
    return status;
}

static mc_world_provider_status biome_sampling_result(mc_world_chunk_manager_error *error,
                                                      const mc_biome_sampling_error *sampling,
                                                      const char *operation,
                                                      unsigned int bytecode) {
    mc_world_provider_status status =
        sampling->registry.status == MC_REGISTRY_OK ? MC_WORLD_PROVIDER_OK
        : sampling->climate_status != MC_BIOME_OK   ? MC_WORLD_CHUNK_MANAGER_FAILED
                                                    : MC_WORLD_PROVIDER_REGISTRY_FAILED;
    if (error != NULL)
        *error = (mc_world_chunk_manager_error){
            .status = status, .operation = operation, .bytecode = bytecode, .sampling = *sampling};
    return status;
}

static mc_world_provider_status biome_call_enter(mc_world_chunk_manager *biome, const void *out,
                                                 mc_world_chunk_manager_error *error) {
    if (biome == NULL || out == NULL)
        return biome_native_result(error, MC_WORLD_PROVIDER_INVALID_ARGUMENT,
                                   MC_REGISTRY_INVALID_ARGUMENT, MC_BIOME_OK,
                                   "GAME biome owner/output", 0);
    mc_world_provider_status status = mc_world_chunk_manager_retain(biome);
    if (status != MC_WORLD_PROVIDER_OK)
        return biome_native_result(error, status, MC_REGISTRY_LIMIT, MC_BIOME_OK,
                                   "GAME biome call hold", 0);
    return MC_WORLD_PROVIDER_OK;
}

static mc_world_provider_status biome_reference_capability(const mc_world_chunk_manager *biome,
                                                           mc_world_chunk_manager_error *error) {
    if (biome->registry == NULL)
        return biome_native_result(error, MC_WORLD_PROVIDER_REGISTRY_FAILED,
                                   MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY, MC_BIOME_OK,
                                   "numerical biome has no actual reference capability", 0);
    return MC_WORLD_PROVIDER_OK;
}

static mc_world_provider_status biome_sample_admitted(mc_world_chunk_manager *biome, int32_t x,
                                                      int32_t z, size_t width, size_t height,
                                                      mc_biome_reference_array *input,
                                                      mc_biome_reference_array **out,
                                                      mc_world_chunk_manager_error *error) {
    mc_world_provider_status status = biome_reference_capability(biome, error);
    if (status != MC_WORLD_PROVIDER_OK)
        return status;
    if (!biome->fixed && width != height)
        return biome_native_result(error, MC_WORLD_PROVIDER_INVALID_ARGUMENT,
                                   MC_REGISTRY_INVALID_ARGUMENT, MC_BIOME_INVALID_ARGUMENT,
                                   "noise-backed GAME biome requires square extent", 0);
    mc_biome_sampling_error sampling = {0};
    mc_biome_reference_array *sampled = NULL;
    if (biome->fixed) {
        (void)mc_registry_biome_fixed_sample(biome->registry, &((fixed_biome *)biome)->storage, x,
                                             z, width, height, input, &sampled, &sampling);
    } else {
        (void)mc_biome_sample_references(biome->registry, &((noise_backed_biome *)biome)->manager,
                                         x, z, width, input, &sampled, &sampling);
    }
    unsigned int pc = sampling.sampler_pc < 0 ? 0u : (unsigned int)sampling.sampler_pc;
    status = biome_sampling_result(error, &sampling, "GAME biome direct sample", pc);
    if (status == MC_WORLD_PROVIDER_OK)
        *out = sampled;
    return status;
}

mc_world_provider_status mc_world_chunk_manager_sample(mc_world_chunk_manager *biome, int32_t x,
                                                       int32_t z, size_t width, size_t height,
                                                       mc_biome_reference_array *input,
                                                       mc_biome_reference_array **out,
                                                       mc_world_chunk_manager_error *error) {
    mc_world_provider_status status = biome_call_enter(biome, out, error);
    if (status != MC_WORLD_PROVIDER_OK)
        return status;
    status = biome_sample_admitted(biome, x, z, width, height, input, out, error);
    mc_world_chunk_manager_release(biome);
    return status;
}

static mc_world_provider_status biome_cached_admitted(mc_world_chunk_manager *biome, int32_t x,
                                                      int32_t z, size_t width, size_t height,
                                                      mc_biome_reference_array **out,
                                                      mc_world_chunk_manager_error *error) {
    mc_biome_reference_array *input = biome->reference_d;
    mc_biome_reference_array *sampled = NULL;
    mc_world_provider_status status =
        biome_sample_admitted(biome, x, z, width, height, input, &sampled, error);
    if (status != MC_WORLD_PROVIDER_OK) {
        if (error != NULL && error->bytecode != 0) {
            error->operation = "GAME biome cached direct";
            error->bytecode = 11;
        }
        return status;
    }
    biome->reference_d = sampled;
    *out = biome->reference_d;
    if (error != NULL) {
        error->operation = "GAME biome cached return";
        error->bytecode = 21;
    }
    return MC_WORLD_PROVIDER_OK;
}

mc_world_provider_status mc_world_chunk_manager_cached(mc_world_chunk_manager *biome, int32_t x,
                                                       int32_t z, size_t width, size_t height,
                                                       mc_biome_reference_array **out,
                                                       mc_world_chunk_manager_error *error) {
    mc_world_provider_status status = biome_call_enter(biome, out, error);
    if (status != MC_WORLD_PROVIDER_OK)
        return status;
    status = biome_cached_admitted(biome, x, z, width, height, out, error);
    mc_world_chunk_manager_release(biome);
    return status;
}

static mc_world_provider_status biome_point_admitted(mc_world_chunk_manager *biome, int32_t x,
                                                     int32_t z, const mc_biome **out,
                                                     mc_world_chunk_manager_error *error) {
    mc_world_provider_status status = biome_reference_capability(biome, error);
    if (status != MC_WORLD_PROVIDER_OK)
        return status;
    if (biome->fixed) {
        *out = ((fixed_biome *)biome)->storage.e;
        return biome_native_result(error, MC_WORLD_PROVIDER_OK, MC_REGISTRY_OK, MC_BIOME_OK,
                                   "fixed GAME biome point return", 4);
    }
    mc_biome_reference_array *sampled = NULL;
    status = biome_cached_admitted(biome, x, z, 1, 1, &sampled, error);
    if (status != MC_WORLD_PROVIDER_OK) {
        if (error != NULL && error->bytecode != 0) {
            error->operation = "noise-backed GAME biome point cached";
            error->bytecode = 5;
        }
        return status;
    }
    const mc_biome *const *data = mc_biome_reference_array_data(sampled);
    if (data == NULL || mc_biome_reference_array_length(sampled) == 0)
        return biome_native_result(error, MC_WORLD_PROVIDER_REGISTRY_FAILED,
                                   data == NULL ? MC_REGISTRY_NULL_REFERENCE
                                                : MC_REGISTRY_INDEX_OUT_OF_RANGE,
                                   MC_BIOME_OK, "noise-backed GAME biome point array", 9);
    *out = data[0];
    if (error != NULL) {
        error->operation = "noise-backed GAME biome point return";
        error->bytecode = 10;
    }
    return MC_WORLD_PROVIDER_OK;
}

mc_world_provider_status mc_world_chunk_manager_point(mc_world_chunk_manager *biome, int32_t x,
                                                      int32_t z, const mc_biome **out,
                                                      mc_world_chunk_manager_error *error) {
    mc_world_provider_status status = biome_call_enter(biome, out, error);
    if (status != MC_WORLD_PROVIDER_OK)
        return status;
    status = biome_point_admitted(biome, x, z, out, error);
    mc_world_chunk_manager_release(biome);
    return status;
}

mc_world_provider_status mc_world_chunk_manager_chunk(mc_world_chunk_manager *biome,
                                                      int32_t chunk_x, int32_t chunk_z,
                                                      const mc_biome **out,
                                                      mc_world_chunk_manager_error *error) {
    mc_world_provider_status status = biome_call_enter(biome, out, error);
    if (status != MC_WORLD_PROVIDER_OK)
        return status;
    if (biome->fixed) {
        status = biome_point_admitted(biome, chunk_x, chunk_z, out, error);
        if (status == MC_WORLD_PROVIDER_OK && error != NULL)
            error->operation = "fixed GAME biome chunk return";
    } else {
        const int32_t x = mc_java_i32_shl(chunk_x, 4);
        const int32_t z = mc_java_i32_shl(chunk_z, 4);
        status = biome_point_admitted(biome, x, z, out, error);
        if (error != NULL && error->bytecode != 0) {
            error->operation = status == MC_WORLD_PROVIDER_OK
                                   ? "noise-backed GAME biome chunk return"
                                   : "noise-backed GAME biome chunk point";
            error->bytecode = status == MC_WORLD_PROVIDER_OK ? 16u : 13u;
        }
    }
    mc_world_chunk_manager_release(biome);
    return status;
}

mc_world_provider_status mc_world_provider_celestial_angle(const mc_world_provider *dimension,
                                                           int64_t time, float partial_tick,
                                                           float *out,
                                                           mc_world_provider_error *error) {
    if (dimension == NULL || out == NULL)
        return result(error, MC_WORLD_PROVIDER_INVALID_ARGUMENT, "dimension celestial angle", 0);
    if (dimension->phase != MC_WORLD_PROVIDER_READY)
        return result(error, MC_WORLD_PROVIDER_INVALID_PHASE, "dimension celestial angle", 0);
    if (!valid_fp())
        return result(error, MC_WORLD_PROVIDER_UNSUPPORTED_FP, "dimension celestial FP admission",
                      0);
    if (dimension->kind == MC_WORLD_PROVIDER_NETHER) {
        *out = 0.5f;
        return result(error, MC_WORLD_PROVIDER_OK, NULL, 0);
    }
    if (dimension->kind == MC_WORLD_PROVIDER_SKY) {
        *out = 0.0f;
        return result(error, MC_WORLD_PROVIDER_OK, NULL, 0);
    }
    if (!isfinite(partial_tick))
        return result(error, MC_WORLD_PROVIDER_INVALID_ARGUMENT, "dimension finite celestial tick",
                      0);
    int64_t remainder;
    (void)mc_java_i64_rem(time, INT64_C(24000), &remainder);
    int32_t day_time = mc_java_i32_narrow(remainder);
    volatile float adjusted = (float)day_time;
    adjusted = adjusted + partial_tick;
    adjusted = adjusted / 24000.0f;
    adjusted = adjusted - 0.25f;
    if (adjusted < 0.0f)
        adjusted = adjusted + 1.0f;
    if (adjusted > 1.0f)
        adjusted = adjusted - 1.0f;
    float original = adjusted;
    volatile double angle = (double)adjusted;
    angle = angle * 3.141592653589793;
    volatile double smoothed = cos(angle);
    smoothed = smoothed + 1.0;
    smoothed = smoothed / 2.0;
    volatile float narrowed = (float)smoothed;
    narrowed = 1.0f - narrowed;
    narrowed = narrowed - original;
    narrowed = narrowed / 3.0f;
    narrowed = original + narrowed;
    *out = narrowed;
    return result(error, MC_WORLD_PROVIDER_OK, NULL, 0);
}

static mc_world_provider_status spawn_registry_failure(mc_world_provider_error *error,
                                                       const char *operation, unsigned int bytecode,
                                                       const mc_registry_error *registry_error) {
    mc_world_provider_status status =
        result(error, MC_WORLD_PROVIDER_REGISTRY_FAILED, operation, bytecode);
    if (error != NULL)
        error->registry = *registry_error;
    return status;
}

static mc_world_provider_status
spawn_registry_source_failure(mc_world_provider_error *error, mc_registry_status registry_status,
                              const char *operation, unsigned int bytecode, int32_t block_id) {
    mc_registry_error registry_error = {.status = registry_status,
                                        .message = operation,
                                        .initializer_pc = -1,
                                        .block_id = block_id,
                                        .constructor_pc = -1,
                                        .active_initializer_pc = -1,
                                        .item_argument = -1};
    return spawn_registry_failure(error, operation, bytecode, &registry_error);
}

static unsigned int spawn_array_failure_pc(mc_registry_status status,
                                           const mc_registry_error *registry_error,
                                           unsigned int static_pc, unsigned int array_pc) {
    /* Active-use failures retain their actual initializer class, even when
     * their first cause is NULL/index. Only the accessor's subsequent array
     * failures clear that context; native entry failures are at the static use. */
    if (registry_error->active_initializer_class == NULL &&
        (status == MC_REGISTRY_NULL_REFERENCE || status == MC_REGISTRY_INDEX_OUT_OF_RANGE))
        return array_pc;
    return static_pc;
}

static mc_world_provider_status can_spawn_admitted(mc_world_provider_kind kind,
                                                   mc_world_runtime *world,
                                                   mc_registry_classes *classes, int32_t x,
                                                   int32_t z, bool *out,
                                                   mc_world_provider_error *error) {
    int32_t top;
    mc_world_storage_error world_error = {0};
    if (mc_world_runtime_top_block(world, x, z, &top, &world_error) != MC_WORLD_STORAGE_OK) {
        mc_world_provider_status status =
            result(error, MC_WORLD_PROVIDER_WORLD_FAILED, "dimension spawn top block", 6);
        if (error != NULL)
            error->world = world_error;
        return status;
    }

    if (kind == MC_WORLD_PROVIDER_SKY && top == 0) {
        *out = false;
        return result(error, MC_WORLD_PROVIDER_OK, NULL, 0);
    }
    unsigned int static_pc = kind == MC_WORLD_PROVIDER_SKY ? 16u : 11u;
    if (classes == NULL)
        return spawn_registry_source_failure(error, MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY,
                                             "dimension spawn World registry", static_pc, -1);

    mc_registry_error registry_error = {0};
    const mc_block *block;
    mc_registry_status registry_status;
    if (kind == MC_WORLD_PROVIDER_SKY) {
        registry_status = mc_registry_block_get(classes, top, &block, &registry_error);
        if (registry_status != MC_REGISTRY_OK)
            return spawn_registry_failure(
                error, "dimension spawn Block.m",
                spawn_array_failure_pc(registry_status, &registry_error, 16, 20), &registry_error);
        if (block == NULL)
            return spawn_registry_source_failure(error, MC_REGISTRY_NULL_REFERENCE,
                                                 "dimension spawn Block.bA", 21, top);
        const mc_material *material = mc_block_fields_get(block)->bA;
        if (material == NULL)
            return spawn_registry_source_failure(error, MC_REGISTRY_NULL_REFERENCE,
                                                 "dimension spawn material c", 24, top);
        *out = mc_material_c(material);
        return result(error, MC_WORLD_PROVIDER_OK, NULL, 0);
    }

    const char *name = kind == MC_WORLD_PROVIDER_NETHER ? "A" : "F";
    registry_status = mc_registry_block_static_get(classes, name, &block, &registry_error);
    if (registry_status != MC_REGISTRY_OK)
        return spawn_registry_failure(error, "dimension spawn Block static", 11, &registry_error);
    if (block == NULL)
        return spawn_registry_source_failure(error, MC_REGISTRY_NULL_REFERENCE,
                                             "dimension spawn Block.bn", 14, -1);
    int32_t registered_id = mc_block_fields_get(block)->bn;
    if (kind == MC_WORLD_PROVIDER_OVERWORLD) {
        *out = top == registered_id;
        return result(error, MC_WORLD_PROVIDER_OK, NULL, 0);
    }
    if (top == registered_id || top == 0) {
        *out = false;
        return result(error, MC_WORLD_PROVIDER_OK, NULL, 0);
    }

    bool opaque;
    registry_status = mc_registry_block_opaque_get(classes, top, &opaque, &registry_error);
    if (registry_status != MC_REGISTRY_OK)
        return spawn_registry_failure(
            error, "dimension spawn Block.o",
            spawn_array_failure_pc(registry_status, &registry_error, 28, 32), &registry_error);
    *out = opaque;
    return result(error, MC_WORLD_PROVIDER_OK, NULL, 0);
}

mc_world_provider_status mc_world_provider_can_spawn(mc_world_provider *dimension, int32_t x,
                                                     int32_t z, bool *out,
                                                     mc_world_provider_error *error) {
    if (dimension == NULL || out == NULL)
        return result(error, MC_WORLD_PROVIDER_INVALID_ARGUMENT, "dimension can spawn", 0);
    if (dimension->phase != MC_WORLD_PROVIDER_READY)
        return result(error, MC_WORLD_PROVIDER_INVALID_PHASE, "dimension can spawn", 0);

    /* The source evaluates a once at PC1. This passive immutable binding read
     * cannot initialize Block or reject an absent capability before top6. */
    mc_world_provider_kind kind = dimension->kind;
    mc_world_runtime *world = dimension->a;
    mc_registry_classes *classes = mc_world_runtime_registry(world);
    if (world != NULL) {
        mc_world_storage_error world_error = {0};
        if (mc_world_runtime_call_enter(world, &world_error) != MC_WORLD_STORAGE_OK) {
            mc_world_provider_status status = result(error, MC_WORLD_PROVIDER_WORLD_FAILED,
                                                     "dimension spawn World call admission", 0);
            if (error != NULL)
                error->world = world_error;
            return status;
        }
    }
    mc_world_provider_status status = can_spawn_admitted(kind, world, classes, x, z, out, error);
    if (world != NULL)
        mc_world_runtime_call_leave(world);
    return status;
}
