#include "plants_internal.h"

#include <stdlib.h>

mc_plant_frame mc_plant_frame_make(mc_world_runtime *world, const mc_block *block,
                                   mc_world_mutation_error *error, const char *client_class,
                                   const char *server_class, const char *client_method,
                                   const char *server_method) {
    const bool client = block->classes->options.side == MC_REGISTRY_CLIENT;
    return (mc_plant_frame){world, block, error, client ? client_class : server_class,
                            client ? client_method : server_method};
}

void mc_plant_at(const mc_plant_frame *frame, unsigned int pc) {
    frame->error->hook_class = frame->declaring_class;
    frame->error->hook_method = frame->method;
    frame->error->hook_bytecode = pc;
}

bool mc_plant_done(const mc_plant_frame *frame) {
    frame->error->hook_class = NULL;
    frame->error->hook_method = NULL;
    frame->error->hook_bytecode = 0;
    return true;
}

bool mc_plant_fail(const mc_plant_frame *frame, unsigned int pc, const char *operation) {
    mc_plant_at(frame, pc);
    return mc_world_mutation_fail(frame->world, frame->error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                  operation);
}

bool mc_plant_storage_fail(const mc_plant_frame *frame, unsigned int pc,
                           mc_world_storage_error cause) {
    mc_plant_at(frame, pc);
    frame->error->status = cause.status;
    frame->error->operation = cause.operation;
    frame->error->cause = cause;
    if (frame->world != NULL)
        frame->world->last_error = cause;
    return false;
}

bool mc_plant_nested(const mc_plant_frame *frame, unsigned int pc,
                     const mc_world_mutation_error *lower) {
    const mc_world_mutation_error captured = *lower;
    *frame->error = captured;
    if (captured.cause_hook_class == NULL) {
        frame->error->cause_hook_class = captured.hook_class;
        frame->error->cause_hook_method = captured.hook_method;
        frame->error->cause_hook_bytecode = captured.hook_bytecode;
    }
    return mc_plant_storage_fail(frame, pc, captured.cause);
}

bool mc_plant_random(const mc_plant_frame *frame, unsigned int pc, mc_java_random *random,
                     int32_t bound, int32_t *out) {
    mc_plant_at(frame, pc);
    if (random == NULL)
        return mc_plant_fail(frame, pc, "Plant tick Random receiver is null");
    if (!mc_java_random_next_bounded(random, bound, out))
        return mc_plant_fail(frame, pc, "Plant tick Random bound");
    return true;
}

bool mc_plant_block(const mc_plant_frame *frame, unsigned int pc, int32_t x, int32_t y, int32_t z,
                    int32_t *out) {
    mc_world_storage_error cause = {0};
    mc_plant_at(frame, pc);
    return mc_world_runtime_block_get(frame->world, x, y, z, out, &cause) == MC_WORLD_STORAGE_OK ||
           mc_plant_storage_fail(frame, pc, cause);
}

bool mc_plant_metadata(const mc_plant_frame *frame, unsigned int pc, int32_t x, int32_t y,
                       int32_t z, int32_t *out) {
    mc_world_storage_error cause = {0};
    mc_plant_at(frame, pc);
    return mc_world_runtime_metadata_get(frame->world, x, y, z, out, &cause) ==
               MC_WORLD_STORAGE_OK ||
           mc_plant_storage_fail(frame, pc, cause);
}

bool mc_plant_air(const mc_plant_frame *frame, unsigned int pc, int32_t x, int32_t y, int32_t z,
                  bool *out) {
    int32_t id;
    if (!mc_plant_block(frame, pc, x, y, z, &id))
        return false;
    *out = id == 0;
    return true;
}

bool mc_plant_survives(const mc_plant_frame *frame, unsigned int pc, int32_t x, int32_t y,
                       int32_t z, bool *out) {
    mc_plant_at(frame, pc);
    mc_world_plant_error lower = {0};
    if (mc_world_runtime_plant_survives(frame->world, frame->block, x, y, z, out, &lower) ==
        MC_WORLD_STORAGE_OK)
        return true;
    frame->error->plant = lower;
    frame->error->registry = lower.registry;
    frame->error->query = lower.query;
    frame->error->material = lower.material;
    frame->error->population = lower.query.population;
    return mc_plant_storage_fail(frame, pc, lower.cause);
}

bool mc_plant_light(const mc_plant_frame *frame, unsigned int pc, int32_t x, int32_t y, int32_t z,
                    int32_t *out) {
    mc_plant_at(frame, pc);
    if (mc_world_runtime_block_light(frame->world, x, y, z, out, &frame->error->query) ==
        MC_WORLD_STORAGE_OK)
        return true;
    frame->error->registry = frame->error->query.registry;
    frame->error->population = frame->error->query.population;
    return mc_plant_storage_fail(frame, pc, frame->error->query.cause);
}

static mc_world_mutation_error *write_workspace(const mc_plant_frame *frame, unsigned int pc) {
    mc_plant_at(frame, pc);
    mc_world_mutation_error *lower = malloc(sizeof(*lower));
    if (lower == NULL) {
        const mc_world_storage_error cause = {MC_WORLD_STORAGE_ALLOCATION_FAILED,
                                              "Plant mutation diagnostic allocation", 0};
        mc_plant_storage_fail(frame, pc, cause);
    } else {
        mc_world_mutation_error_init(lower);
    }
    return lower;
}

bool mc_plant_set(const mc_plant_frame *frame, unsigned int pc, int32_t x, int32_t y, int32_t z,
                  int32_t id) {
    mc_world_mutation_error *lower = write_workspace(frame, pc);
    if (lower == NULL)
        return false;
    bool ignored;
    const bool completed = mc_world_runtime_block_set(frame->world, x, y, z, id, &ignored, lower) ==
                               MC_WORLD_STORAGE_OK ||
                           mc_plant_nested(frame, pc, lower);
    free(lower);
    return completed;
}

bool mc_plant_set_metadata(const mc_plant_frame *frame, unsigned int pc, int32_t x, int32_t y,
                           int32_t z, int32_t metadata) {
    mc_world_mutation_error *lower = write_workspace(frame, pc);
    if (lower == NULL)
        return false;
    const bool completed = mc_world_runtime_metadata_set(frame->world, x, y, z, metadata, lower) ==
                               MC_WORLD_STORAGE_OK ||
                           mc_plant_nested(frame, pc, lower);
    free(lower);
    return completed;
}

/* wb.h/nw.g and ri.h/ko.g are final bodies; cactus declares the same sequence
 * directly in its neighbor override. The virtual survival and drop receiver
 * remains the captured Block even when reentry changes the current cell. */
static bool survival(const mc_plant_frame *frame, int32_t x, int32_t y, int32_t z) {
    bool supported;
    if (!mc_plant_survives(frame, 6, x, y, z, &supported))
        return false;
    if (supported)
        return mc_plant_done(frame);
    int32_t metadata;
    if (!mc_plant_metadata(frame, 23, x, y, z, &metadata))
        return false;
    mc_plant_at(frame, 26);
    if (!mc_world_mutation_drop(frame->world, frame->block, x, y, z, metadata, frame->error))
        return mc_plant_nested(frame, 26, frame->error);
    if (!mc_plant_set(frame, 35, x, y, z, 0))
        return false;
    return mc_plant_done(frame);
}

bool mc_plant_flower_survival(mc_world_runtime *world, const mc_block *block, int32_t x, int32_t y,
                              int32_t z, mc_world_mutation_error *error) {
    const mc_plant_frame frame =
        mc_plant_frame_make(world, block, error, "wb", "nw", "h(Lfd;III)V", "g(Ldj;III)V");
    return survival(&frame, x, y, z);
}

bool mc_world_mutation_plant_neighbor(mc_world_runtime *world, const mc_block *block, int32_t x,
                                      int32_t y, int32_t z, int32_t source_id,
                                      mc_world_mutation_error *error) {
    (void)source_id;
    const bool cactus = block->kind == MC_BLOCK_OR_JE;
    const bool reed = block->kind == MC_BLOCK_RI_KO;
    const mc_plant_frame frame = mc_plant_frame_make(world, block, error,
                                                     cactus ? "or"
                                                     : reed ? "ri"
                                                            : "wb",
                                                     cactus ? "je"
                                                     : reed ? "ko"
                                                            : "nw",
                                                     "b(Lfd;IIII)V", "b(Ldj;IIII)V");
    if (cactus)
        return survival(&frame, x, y, z);
    if (reed) {
        const mc_plant_frame helper =
            mc_plant_frame_make(world, block, error, "ri", "ko", "h(Lfd;III)V", "g(Ldj;III)V");
        if (!survival(&helper, x, y, z))
            return mc_plant_nested(&frame, 6, error);
    } else {
        /* The invokespecial base neighbor at8 is a proved empty body. */
        if (!mc_plant_flower_survival(world, block, x, y, z, error))
            return mc_plant_nested(&frame, 17, error);
    }
    return mc_plant_done(&frame);
}

bool mc_world_mutation_plant_tick(mc_world_runtime *world, const mc_block *block, int32_t x,
                                  int32_t y, int32_t z, mc_java_random *random,
                                  mc_world_mutation_error *error) {
    switch (block->kind) {
        case MC_BLOCK_HE_EU:
            return mc_plant_sapling_tick(world, block, x, y, z, random, error);
        case MC_BLOCK_NI_IJ:
            return mc_plant_crop_tick(world, block, x, y, z, random, error);
        case MC_BLOCK_TL_MD:
            return mc_plant_mushroom_tick(world, block, x, y, z, random, error);
        case MC_BLOCK_OR_JE:
        case MC_BLOCK_RI_KO:
            return mc_plant_upright_tick(world, block, x, y, z, error);
        default: {
            const mc_plant_frame frame = mc_plant_frame_make(world, block, error, "wb", "nw",
                                                             "a(Lfd;IIILjava/util/Random;)V",
                                                             "a(Ldj;IIILjava/util/Random;)V");
            if (!mc_plant_flower_survival(world, block, x, y, z, error))
                return mc_plant_nested(&frame, 6, error);
            return mc_plant_done(&frame);
        }
    }
}
