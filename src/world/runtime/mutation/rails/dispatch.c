#include "detector_internal.h"
#include "rail_lifecycle_internal.h"

/* The ordinary dispatchers retain their enclosing World call and actual
 * receiver. Each concrete body publishes its complete flat mutation child;
 * these adapters add no source read, constructor, entry or class use. */
typedef struct rail_caller_prefix {
    unsigned int wrapper, world, chunk, neighbor_call, neighbor;
} rail_caller_prefix;

static rail_caller_prefix caller_prefix(const mc_world_mutation_error *error) {
    return (rail_caller_prefix){error->wrapper_bytecode, error->bytecode, error->chunk_bytecode,
                                error->neighbor_call_bytecode, error->neighbor_bytecode};
}

static bool dispatch_result(bool complete, mc_world_mutation_error *error,
                            const mc_world_mutation_error *lower, rail_caller_prefix caller) {
    if (complete) {
        /* A successful concrete callee clears only its own hook frame. The
         * enclosing World/Chunk/neighbor caller is still executing. */
        error->hook_class = NULL;
        error->hook_method = NULL;
        error->hook_bytecode = 0;
        return true;
    }
    *error = *lower;
    /* These legacy scalar fields define zero as unreached. Keep a reached
     * nested World operation when present; otherwise retain the actual
     * enclosing caller. No typed child or rail selection is inferred here. */
    if (error->wrapper_bytecode == 0)
        error->wrapper_bytecode = caller.wrapper;
    if (error->bytecode == 0)
        error->bytecode = caller.world;
    if (error->chunk_bytecode == 0)
        error->chunk_bytecode = caller.chunk;
    if (error->neighbor_call_bytecode == 0)
        error->neighbor_call_bytecode = caller.neighbor_call;
    if (error->neighbor_bytecode == 0)
        error->neighbor_bytecode = caller.neighbor;
    return false;
}

bool mc_world_mutation_rail_added_hook(mc_world_runtime *world, const mc_block *block, int32_t x,
                                       int32_t y, int32_t z, mc_world_mutation_error *error) {
    if (error == NULL)
        return false;
    const rail_caller_prefix caller = caller_prefix(error);
    mc_rail_lifecycle_error detail;
    mc_world_mutation_rail_error_init(&detail);
    const bool complete = mc_world_mutation_rail_added(world, block, x, y, z, &detail);
    return dispatch_result(complete, error, &detail.mutation, caller);
}

bool mc_world_mutation_rail_neighbor_hook(mc_world_runtime *world, const mc_block *block, int32_t x,
                                          int32_t y, int32_t z, int32_t source_id,
                                          mc_world_mutation_error *error) {
    if (error == NULL)
        return false;
    const rail_caller_prefix caller = caller_prefix(error);
    mc_rail_lifecycle_error detail;
    mc_world_mutation_rail_error_init(&detail);
    const bool complete =
        mc_world_mutation_rail_neighbor(world, block, x, y, z, source_id, &detail);
    return dispatch_result(complete, error, &detail.mutation, caller);
}

bool mc_world_mutation_detector_tick_hook(mc_world_runtime *world, const mc_block *block, int32_t x,
                                          int32_t y, int32_t z, mc_java_random *random,
                                          mc_world_mutation_error *error) {
    if (error == NULL)
        return false;
    const rail_caller_prefix caller = caller_prefix(error);
    mc_world_detector_error detail = mc_world_mutation_detector_empty_error();
    const bool complete = mc_world_mutation_detector_tick(world, block, x, y, z, random, &detail);
    return dispatch_result(complete, error, &detail.mutation, caller);
}
