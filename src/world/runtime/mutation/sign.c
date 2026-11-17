#include "mutation_internal.h"
#include "world_execution_internal.h"

#include "../../../block/registry/registry_internal.h"
#include "../collision/blocks_internal.h"
#include "mc/core/java/java.h"

#include <stdlib.h>

typedef union sign_scratch {
    mc_world_collision_error value;
    mc_world_mutation_error mutation;
} sign_scratch;

typedef struct sign_frame {
    mc_world_runtime *world;
    const mc_block *block;
    mc_world_mutation_error *error;
    sign_scratch *scratch;
    mc_world_mutation_execution_call *owned;
} sign_frame;

static mc_entity_tick_execution_site sign_site(unsigned int pc) {
    return (mc_entity_tick_execution_site){"uj", "b", "(Lfd;IIII)V", (int)pc};
}

static void sign_at(sign_frame *f, unsigned int pc) {
    f->error->hook_class = "uj";
    f->error->hook_method = "b(Lfd;IIII)V";
    f->error->hook_bytecode = pc;
}

static bool sign_null(sign_frame *f, unsigned int pc, const char *operation) {
    sign_at(f, pc);
    f->error->sign_source_null = true;
    return mc_world_mutation_fail(f->world, f->error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                  operation);
}

static bool sign_storage(sign_frame *f, mc_world_storage_error cause) {
    f->error->status = cause.status;
    f->error->operation = cause.operation;
    f->error->cause = cause;
    if (f->world != NULL)
        f->world->last_error = cause;
    return false;
}

static bool sign_workspace(sign_frame *f) {
    if (f->scratch == NULL)
        f->scratch = malloc(sizeof(*f->scratch));
    return f->scratch != NULL ||
           mc_world_mutation_fail(f->world, f->error, MC_WORLD_STORAGE_ALLOCATION_FAILED,
                                  "Sign reached child diagnostic allocation");
}

static bool sign_metadata(sign_frame *f, unsigned int pc, int32_t x, int32_t y, int32_t z,
                          int32_t *out) {
    sign_at(f, pc);
    if (f->world == NULL)
        return sign_null(f, pc, "Sign actual null World metadata receiver");
    if (f->owned != NULL)
        return mc_world_mutation_execution_get(f->owned, f->world, x, y, z, true, out, f->error,
                                               sign_site(pc));
    if (!sign_workspace(f))
        return false;
    mc_world_collision_error *child = &f->scratch->value;
    mc_world_collision_error_init(child);
    if (mc_world_collision_block_value_held(f->world, x, y, z, true, out, child) ==
        MC_WORLD_STORAGE_OK)
        return true;
    f->error->nearby = child->nearby;
    f->error->query = child->nearby.query;
    f->error->population = child->nearby.query.population;
    f->error->entity = child->nearby.entity;
    f->error->tile_entity = child->nearby.tile_entity;
    f->error->batch = child->nearby.batch;
    f->error->load_entity = child->nearby.load_entity;
    f->error->activation_bytecode = child->nearby.activation_bytecode;
    f->error->activation_world_bytecode = child->nearby.activation_world_bytecode;
    f->error->load_bytecode = child->nearby.load_bytecode;
    f->error->load_builder_bytecode = child->nearby.load_builder_bytecode;
    f->error->registry = child->nearby.query.registry;
    return sign_storage(f, child->nearby.query.cause);
}

static bool sign_material(sign_frame *f, unsigned int get_pc, unsigned int invoke_pc, int32_t x,
                          int32_t y, int32_t z, bool *out) {
    sign_at(f, get_pc);
    if (f->world == NULL)
        return sign_null(f, get_pc, "Sign actual null World material receiver");
    mc_material *material = NULL;
    if (f->owned != NULL) {
        if (!mc_world_mutation_execution_material(f->owned, f->world, x, y, z, &material, f->error,
                                                  sign_site(get_pc)))
            return false;
    } else {
        mc_world_material_error child = {0};
        if (mc_world_runtime_material_get_detailed(f->world, x, y, z, &material, &child) !=
            MC_WORLD_STORAGE_OK) {
            f->error->material = child;
            f->error->registry = child.registry;
            return sign_storage(f, child.cause);
        }
    }
    sign_at(f, invoke_pc);
    if (material == NULL)
        return sign_null(f, invoke_pc, "Sign actual null Material.a receiver");
    *out = mc_material_a(material);
    return true;
}

static bool sign_nested(sign_frame *f, unsigned int pc, const mc_world_mutation_error *child) {
    if (f->error != child)
        *f->error = *child;
    if (f->error->cause_hook_class == NULL) {
        f->error->cause_hook_class = f->error->hook_class;
        f->error->cause_hook_method = f->error->hook_method;
        f->error->cause_hook_bytecode = f->error->hook_bytecode;
    }
    sign_at(f, pc);
    if (f->world != NULL)
        f->world->last_error = f->error->cause;
    return false;
}

static bool sign_neighbor(sign_frame *f, int32_t x, int32_t y, int32_t z) {
    bool supported = false;
    if (f->block->subtype.uj.b) {
        if (!sign_material(f, 17, 20, x, mc_java_i32_sub(y, 1), z, &supported))
            return false;
    } else {
        int32_t metadata;
        if (!sign_metadata(f, 37, x, y, z, &metadata))
            return false;
        /* Full equality selects one Material read. Invalid metadata has no
         * support fallback and reaches the later fresh metadata read161. */
        if (metadata >= 2 && metadata <= 5) {
            static const unsigned int get_pcs[] = {58, 83, 108, 133};
            static const unsigned int invoke_pcs[] = {61, 86, 111, 136};
            const int32_t sx = metadata == 4   ? mc_java_i32_add(x, 1)
                               : metadata == 5 ? mc_java_i32_sub(x, 1)
                                               : x;
            const int32_t sz = metadata == 2   ? mc_java_i32_add(z, 1)
                               : metadata == 3 ? mc_java_i32_sub(z, 1)
                                               : z;
            const size_t index = (size_t)metadata - 2u;
            if (!sign_material(f, get_pcs[index], invoke_pcs[index], sx, y, sz, &supported))
                return false;
        }
    }
    if (!supported) {
        int32_t metadata;
        if (!sign_metadata(f, 161, x, y, z, &metadata))
            return false;
        sign_at(f, 164);
        if (f->owned != NULL) {
            if (!mc_world_mutation_execution_drop(f->owned, f->world, f->block, x, y, z, metadata,
                                                  f->error, sign_site(164)))
                return false;
            sign_at(f, 173);
            if (!mc_world_mutation_execution_remove(f->owned, f->world, x, y, z, f->error,
                                                    sign_site(173))) {
                if (f->error->status != MC_WORLD_STORAGE_OK)
                    return sign_nested(f, 173, f->error);
                sign_at(f, 173);
                return false;
            }
        } else {
            if (!mc_world_mutation_drop(f->world, f->block, x, y, z, metadata, f->error))
                return sign_nested(f, 164, f->error);
            sign_at(f, 173);
            if (!sign_workspace(f))
                return false;
            mc_world_mutation_error *child = &f->scratch->mutation;
            mc_world_mutation_error_init(child);
            bool ignored;
            if (mc_world_runtime_block_set(f->world, x, y, z, 0, &ignored, child) !=
                MC_WORLD_STORAGE_OK)
                return sign_nested(f, 173, child);
        }
    }
    /* Actual invokespecial185 resolves to the inherited literal-return body. */
    f->error->hook_class = f->error->hook_method = NULL;
    f->error->hook_bytecode = 0;
    return true;
}

bool mc_world_mutation_sign_neighbor(mc_world_runtime *world, const mc_block *block, int32_t x,
                                     int32_t y, int32_t z, int32_t source_id,
                                     mc_world_mutation_error *error) {
    (void)source_id;
    sign_frame frame = {world, block, error, NULL, NULL};
    const bool completed = sign_neighbor(&frame, x, y, z);
    free(frame.scratch);
    return completed;
}

bool mc_world_mutation_sign_neighbor_in_execution(mc_world_mutation_execution_call *owned,
                                                  mc_world_runtime *world, const mc_block *block,
                                                  int32_t x, int32_t y, int32_t z,
                                                  int32_t source_id,
                                                  mc_world_mutation_error *error) {
    (void)source_id;
    if (!mc_world_mutation_execution_current(owned, world, sign_site(0)))
        return false;
    sign_frame frame = {world, block, error, NULL, owned};
    return sign_neighbor(&frame, x, y, z);
}
