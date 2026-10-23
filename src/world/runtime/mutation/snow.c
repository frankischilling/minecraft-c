#include "mutation_internal.h"

#include "../../../block/registry/registry_internal.h"
#include "../../../block/registry/runtime_internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/core/java/java.h"

typedef enum snow_method { SNOW_SUPPORT, SNOW_PRIVATE, SNOW_NEIGHBOR, SNOW_TICK } snow_method;

typedef struct snow_frame {
    mc_world_runtime *world;
    const mc_block *block;
    mc_world_mutation_error *error;
    snow_method method;
} snow_frame;

static void snow_at(const snow_frame *frame, unsigned int pc) {
    const bool client = frame->block->classes->options.side == MC_REGISTRY_CLIENT;
    mc_world_mutation_error *error = frame->error;
    error->hook_class = client ? "jr" : "gj";
    error->hook_bytecode = pc;
    switch (frame->method) {
        case SNOW_SUPPORT:
            error->hook_method = client ? "a(Lfd;III)Z" : "a(Ldj;III)Z";
            error->snow_support_bytecode = pc;
            break;
        case SNOW_PRIVATE:
            error->hook_method = client ? "h(Lfd;III)Z" : "g(Ldj;III)Z";
            error->snow_bytecode = pc;
            break;
        case SNOW_NEIGHBOR:
            error->hook_method = client ? "b(Lfd;IIII)V" : "b(Ldj;IIII)V";
            error->snow_neighbor_bytecode = pc;
            break;
        case SNOW_TICK:
            error->hook_method =
                client ? "a(Lfd;IIILjava/util/Random;)V" : "a(Ldj;IIILjava/util/Random;)V";
            error->snow_tick_bytecode = pc;
            break;
    }
}

static bool snow_done(mc_world_mutation_error *error) {
    error->hook_class = NULL;
    error->hook_method = NULL;
    error->hook_bytecode = 0;
    error->snow_neighbor_bytecode = 0;
    error->snow_bytecode = 0;
    error->snow_support_bytecode = 0;
    error->snow_tick_bytecode = 0;
    return true;
}

static bool snow_storage_fail(const snow_frame *frame, unsigned int pc,
                              mc_world_storage_error cause) {
    snow_at(frame, pc);
    frame->error->status = cause.status;
    frame->error->operation = cause.operation;
    frame->error->cause = cause;
    if (frame->world != NULL)
        frame->world->last_error = cause;
    return false;
}

static bool snow_registry_fail(const snow_frame *frame, unsigned int pc,
                               const mc_registry_error *registry, const char *operation) {
    snow_at(frame, pc);
    return mc_world_mutation_registry_fail(frame->world, frame->error, registry, operation);
}

static bool snow_null(const snow_frame *frame, unsigned int pc, const char *operation) {
    mc_registry_error registry = mc_world_mutation_empty_registry();
    registry.status = MC_REGISTRY_NULL_REFERENCE;
    registry.message = operation;
    return snow_registry_fail(frame, pc, &registry, operation);
}

/* Keep the current caller and deepest concrete hook, alongside the explicit
 * snow frames. This diagnostic projection does not cap source recursion. */
static bool snow_nested(const snow_frame *frame, unsigned int pc,
                        const mc_world_mutation_error *lower) {
    const mc_world_mutation_error captured = *lower;
    *frame->error = captured;
    if (captured.cause_hook_class == NULL) {
        frame->error->cause_hook_class = captured.hook_class;
        frame->error->cause_hook_method = captured.hook_method;
        frame->error->cause_hook_bytecode = captured.hook_bytecode;
    }
    snow_at(frame, pc);
    if (frame->world != NULL)
        frame->world->last_error = captured.cause;
    return false;
}

bool mc_world_mutation_snow_can_place(mc_world_runtime *world, const mc_block *block, int32_t x,
                                      int32_t y, int32_t z, bool *out,
                                      mc_world_mutation_error *error) {
    const snow_frame frame = {world, block, error, SNOW_SUPPORT};
    const int32_t below = mc_java_i32_sub(y, 1);
    int32_t id;
    mc_world_storage_error cause = {0};
    snow_at(&frame, 7);
    if (mc_world_runtime_block_get(world, x, below, z, &id, &cause) != MC_WORLD_STORAGE_OK)
        return snow_storage_fail(&frame, 7, cause);
    if (id == 0) {
        *out = false;
        return snow_done(error);
    }

    /* Do not substitute the cached Block.o array or World solid support. This
     * is a late capture of m after the acquiring getter and a virtual call. */
    mc_registry_block_m_view array;
    mc_registry_error registry = mc_world_mutation_empty_registry();
    snow_at(&frame, 17);
    if (mc_registry_block_m_get(block->classes, &array, &registry) != MC_REGISTRY_OK)
        return snow_registry_fail(&frame, 17, &registry, "Snow support Block.m active use");
    const mc_block *support;
    snow_at(&frame, 22);
    if (mc_registry_block_m_at(&array, id, &support, &registry) != MC_REGISTRY_OK)
        return snow_registry_fail(&frame, 22, &registry, "Snow support captured Block.m element");
    snow_at(&frame, 23);
    if (support == NULL)
        return snow_null(&frame, 23, "Snow support Block opacity receiver is null");
    if (!mc_registry_block_opaque(support)) {
        *out = false;
        return snow_done(error);
    }

    /* The second query executes the actual World material body independently;
     * retaining the first selected Block's bA would skip a source acquisition. */
    mc_material *material = NULL;
    snow_at(&frame, 38);
    if (mc_world_runtime_material_get_detailed(world, x, below, z, &material, &error->material) !=
        MC_WORLD_STORAGE_OK) {
        error->registry = error->material.registry;
        return snow_storage_fail(&frame, 38, error->material.cause);
    }
    snow_at(&frame, 41);
    if (material == NULL)
        return snow_null(&frame, 41, "Snow support Material.c receiver is null");
    *out = mc_material_c(material);
    return snow_done(error);
}

/* Private h/client or g/server returns an ordinary boolean, discarded by its
 * only neighbor caller. Native completion is separate from that game value. */
static bool snow_survive(mc_world_runtime *world, const mc_block *block, int32_t x, int32_t y,
                         int32_t z, mc_world_mutation_error *error) {
    const snow_frame frame = {world, block, error, SNOW_PRIVATE};
    bool supported;
    if (!mc_world_mutation_snow_can_place(world, block, x, y, z, &supported, error))
        return snow_nested(&frame, 6, error);
    if (supported)
        return snow_done(error);

    int32_t metadata;
    mc_world_storage_error cause = {0};
    snow_at(&frame, 23);
    if (mc_world_runtime_metadata_get(world, x, y, z, &metadata, &cause) != MC_WORLD_STORAGE_OK)
        return snow_storage_fail(&frame, 23, cause);
    snow_at(&frame, 26);
    if (!mc_world_mutation_drop(world, block, x, y, z, metadata, error))
        return snow_nested(&frame, 26, error);
    bool ignored;
    mc_world_mutation_error lower = mc_world_mutation_empty_error();
    snow_at(&frame, 35);
    if (mc_world_runtime_block_set(world, x, y, z, 0, &ignored, &lower) != MC_WORLD_STORAGE_OK)
        return snow_nested(&frame, 35, &lower);
    return snow_done(error);
}

bool mc_world_mutation_snow_neighbor(mc_world_runtime *world, const mc_block *block, int32_t x,
                                     int32_t y, int32_t z, int32_t source_id,
                                     mc_world_mutation_error *error) {
    (void)source_id;
    const snow_frame frame = {world, block, error, SNOW_NEIGHBOR};
    if (!snow_survive(world, block, x, y, z, error))
        return snow_nested(&frame, 6, error);
    return snow_done(error);
}

bool mc_world_mutation_snow_tick(mc_world_runtime *world, const mc_block *block, int32_t x,
                                 int32_t y, int32_t z, mc_java_random *random,
                                 mc_world_mutation_error *error) {
    (void)random;
    const snow_frame frame = {world, block, error, SNOW_TICK};
    const mc_world_light_type *type;
    mc_world_storage_error cause = {0};
    snow_at(&frame, 1);
    if (mc_world_runtime_light_type_get(world->classes, MC_LIGHT_BLOCK, &type, &cause) !=
        MC_WORLD_STORAGE_OK)
        return snow_storage_fail(&frame, 1, cause);
    int32_t light;
    snow_at(&frame, 8);
    if (mc_world_runtime_saved_light(world, type, x, y, z, &light, &error->query) !=
        MC_WORLD_STORAGE_OK) {
        error->registry = error->query.registry;
        if (error->query.population.reached)
            error->population = error->query.population;
        return snow_storage_fail(&frame, 8, error->query.cause);
    }
    if (light <= 11)
        return snow_done(error);
    int32_t metadata;
    snow_at(&frame, 27);
    if (mc_world_runtime_metadata_get(world, x, y, z, &metadata, &cause) != MC_WORLD_STORAGE_OK)
        return snow_storage_fail(&frame, 27, cause);
    snow_at(&frame, 30);
    if (!mc_world_mutation_drop(world, block, x, y, z, metadata, error))
        return snow_nested(&frame, 30, error);
    bool ignored;
    mc_world_mutation_error lower = mc_world_mutation_empty_error();
    snow_at(&frame, 39);
    if (mc_world_runtime_block_set(world, x, y, z, 0, &ignored, &lower) != MC_WORLD_STORAGE_OK)
        return snow_nested(&frame, 39, &lower);
    return snow_done(error);
}
