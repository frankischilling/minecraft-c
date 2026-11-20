#include "../mutation_internal.h"
#include "stairs_internal.h"

#include "../../../../block/registry/registry_internal.h"

typedef struct stairs_frame {
    mc_world_runtime *world;
    const mc_block *block;
    mc_world_mutation_error *error;
    const char *method;
} stairs_frame;

static void stairs_at(const stairs_frame *f, unsigned int pc) {
    f->error->hook_class = "ss";
    f->error->hook_method = f->method;
    f->error->hook_bytecode = pc;
}

static bool stairs_done(mc_world_mutation_error *error) {
    error->hook_class = error->hook_method = NULL;
    error->hook_bytecode = 0;
    error->stairs_source_null = false;
    return true;
}

static bool stairs_fail(const stairs_frame *f, unsigned int pc, const char *operation) {
    stairs_at(f, pc);
    return mc_world_mutation_fail(f->world, f->error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                  operation);
}

static bool stairs_entry(const stairs_frame *f) {
    if (f->error == NULL)
        return false;
    if (f->block == NULL || f->block->classes == NULL || f->block->kind != MC_BLOCK_STAIRS ||
        f->block->classes->options.side != MC_REGISTRY_CLIENT)
        return stairs_fail(f, 0, "Stairs actual client concrete receiver");
    return true;
}

static bool stairs_target(const stairs_frame *f, const mc_block *wrapped, unsigned int pc) {
    stairs_at(f, pc);
    if (wrapped == NULL) {
        f->error->stairs_source_null = true;
        return stairs_fail(f, pc, "Stairs captured wrapped Block invocation receiver is null");
    }
    if (wrapped->classes != f->block->classes)
        return stairs_fail(f, pc, "Stairs wrapped Block defining Registry graph");
    return true;
}

static bool stairs_nested(const stairs_frame *f, unsigned int pc) {
    if (f->error->cause_hook_class == NULL) {
        f->error->cause_hook_class = f->error->hook_class;
        f->error->cause_hook_method = f->error->hook_method;
        f->error->cause_hook_bytecode = f->error->hook_bytecode;
    }
    stairs_at(f, pc);
    return false;
}

bool mc_world_mutation_stairs_neighbor(mc_world_runtime *world, const mc_block *block, int32_t x,
                                       int32_t y, int32_t z, int32_t source_id,
                                       mc_world_mutation_error *error) {
    const stairs_frame f = {world, block, error, "b(Lfd;IIII)V"};
    if (!stairs_entry(&f))
        return false;
    (void)x;
    (void)y;
    (void)z;
    (void)source_id;
    /* SS has no override of this descriptor. Its inherited UU.b returns0,
     * without reading a, World, coordinates or the changed Block ID. */
    return stairs_done(error);
}

bool mc_world_mutation_stairs_added(mc_world_runtime *world, const mc_block *block, int32_t x,
                                    int32_t y, int32_t z, mc_world_mutation_error *error) {
    const stairs_frame f = {world, block, error, "c(Lfd;III)V"};
    if (!stairs_entry(&f))
        return false;
    stairs_at(&f, 7);
    if (!mc_world_mutation_stairs_neighbor(world, block, x, y, z, 0, error))
        return stairs_nested(&f, 7);
    stairs_at(&f, 11);
    const mc_block *wrapped = block->subtype.ss.a;
    if (!stairs_target(&f, wrapped, 19))
        return false;
    if (!mc_world_mutation_block_added(world, wrapped, x, y, z, error))
        return stairs_nested(&f, 19);
    return stairs_done(error);
}

bool mc_world_mutation_stairs_removed(mc_world_runtime *world, const mc_block *block, int32_t x,
                                      int32_t y, int32_t z, mc_world_mutation_error *error) {
    const stairs_frame f = {world, block, error, "b(Lfd;III)V"};
    if (!stairs_entry(&f))
        return false;
    stairs_at(&f, 1);
    const mc_block *wrapped = block->subtype.ss.a;
    if (!stairs_target(&f, wrapped, 9))
        return false;
    if (!mc_world_mutation_block_removed(world, wrapped, x, y, z, error))
        return stairs_nested(&f, 9);
    return stairs_done(error);
}

bool mc_world_mutation_stairs_tick(mc_world_runtime *world, const mc_block *block, int32_t x,
                                   int32_t y, int32_t z, mc_java_random *random,
                                   mc_world_mutation_error *error) {
    const stairs_frame f = {world, block, error, "a(Lfd;IIILjava/util/Random;)V"};
    if (!stairs_entry(&f))
        return false;
    stairs_at(&f, 1);
    const mc_block *wrapped = block->subtype.ss.a;
    if (!stairs_target(&f, wrapped, 11))
        return false;
    if (!mc_world_mutation_block_tick(world, wrapped, x, y, z, random, error))
        return stairs_nested(&f, 11);
    return stairs_done(error);
}
