#include "mutation_internal.h"

#include "../../../block/registry/registry_internal.h"
#include "../../../block/registry/runtime_internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/core/java/java.h"
#include "mc/world/dimension/dimension.h"
#include "mc/world/runtime/blocks.h"
#include "mc/world/runtime/scheduling.h"

#if !defined(_M_X64) && !defined(_M_AMD64) && !defined(__x86_64__)
#error "Fluid floating operations require x86-64/SSE2"
#endif
#include <emmintrin.h>

typedef struct fluid_frame {
    mc_world_runtime *world;
    mc_block *block;
    const char *class_name, *method;
    mc_world_mutation_error *error;
} fluid_frame;

static fluid_frame fluid_frame_make(mc_world_runtime *world, const mc_block *block,
                                    const char *client_class, const char *server_class,
                                    const char *client_method, const char *server_method,
                                    mc_world_mutation_error *error) {
    const bool client = block->classes->options.side == MC_REGISTRY_CLIENT;
    /* Registry arena allocations are mutable actual source objects. const
     * on the dispatch argument is a borrowed view, not immutable storage. */
    return (fluid_frame){world, (mc_block *)block, client ? client_class : server_class,
                         client ? client_method : server_method, error};
}

static void fluid_at(const fluid_frame *frame, unsigned int pc) {
    frame->error->hook_class = frame->class_name;
    frame->error->hook_method = frame->method;
    frame->error->hook_bytecode = pc;
}

static bool fluid_return(const fluid_frame *frame) {
    frame->error->hook_class = NULL;
    frame->error->hook_method = NULL;
    frame->error->hook_bytecode = 0;
    return true;
}

static bool fluid_fail(const fluid_frame *frame, unsigned int pc, const char *operation) {
    fluid_at(frame, pc);
    return mc_world_mutation_fail(frame->world, frame->error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                  operation);
}

static bool fluid_storage_fail(const fluid_frame *frame, unsigned int pc,
                               mc_world_storage_error cause) {
    fluid_at(frame, pc);
    frame->error->status = cause.status;
    frame->error->operation = cause.operation;
    frame->error->cause = cause;
    frame->world->last_error = cause;
    return false;
}

/* One current caller plus the deepest concrete hook, not a recursive stack.
 * lower may alias frame->error, so capture every field before restoration. */
static bool fluid_nested(const fluid_frame *frame, unsigned int pc,
                         const mc_world_mutation_error *lower) {
    const mc_world_mutation_error captured = *lower;
    *frame->error = captured;
    if (captured.cause_hook_class == NULL) {
        frame->error->cause_hook_class = captured.hook_class;
        frame->error->cause_hook_method = captured.hook_method;
        frame->error->cause_hook_bytecode = captured.hook_bytecode;
    }
    fluid_at(frame, pc);
    frame->world->last_error = captured.cause;
    return false;
}

static bool fluid_registry_fail(const fluid_frame *frame, unsigned int pc,
                                const mc_registry_error *cause, const char *operation) {
    fluid_at(frame, pc);
    return mc_world_mutation_registry_fail(frame->world, frame->error, cause, operation);
}

static mc_material *fluid_material_static(const fluid_frame *frame, mc_material_id id) {
    return mc_materials_get(frame->block->classes->options.materials, id);
}

static bool fluid_id(const fluid_frame *frame, unsigned int pc, int32_t x, int32_t y, int32_t z,
                     int32_t *out) {
    mc_world_storage_error cause = {0};
    fluid_at(frame, pc);
    if (mc_world_runtime_block_get(frame->world, x, y, z, out, &cause) != MC_WORLD_STORAGE_OK)
        return fluid_storage_fail(frame, pc, cause);
    return true;
}

static bool fluid_metadata(const fluid_frame *frame, unsigned int pc, int32_t x, int32_t y,
                           int32_t z, int32_t *out) {
    mc_world_storage_error cause = {0};
    fluid_at(frame, pc);
    if (mc_world_runtime_metadata_get(frame->world, x, y, z, out, &cause) != MC_WORLD_STORAGE_OK)
        return fluid_storage_fail(frame, pc, cause);
    return true;
}

/* Actual World material body, fd.f/dj.d(III): ID4; air Material.a14;
 * otherwise live m18/index23/selected bA24. Never read a stale World cause. */
static bool fluid_material(const fluid_frame *frame, unsigned int pc, int32_t x, int32_t y,
                           int32_t z, mc_material **out) {
    mc_world_mutation_error lower = mc_world_mutation_empty_error();
    mc_world_storage_error cause = {0};
    int32_t id;
    lower.bytecode = 4;
    if (mc_world_runtime_block_get(frame->world, x, y, z, &id, &cause) != MC_WORLD_STORAGE_OK) {
        lower.status = cause.status;
        lower.operation = cause.operation;
        lower.cause = cause;
        return fluid_nested(frame, pc, &lower);
    }
    mc_registry_classes *classes = mc_world_runtime_registry(frame->world);
    mc_registry_error registry = mc_world_mutation_empty_registry();
    lower.bytecode = id == 0 ? 14u : 18u;
    if (classes == NULL) {
        registry.status = MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY;
        registry.message = "World fluid material Registry binding";
        (void)mc_world_mutation_registry_fail(frame->world, &lower, &registry, registry.message);
        return fluid_nested(frame, pc, &lower);
    }
    mc_material *material = NULL;
    mc_registry_status status;
    if (id == 0) {
        status = mc_registry_material_get(classes, MC_MATERIAL_a, &material, &registry);
    } else {
        mc_registry_block_m_view array;
        status = mc_registry_block_m_get(classes, &array, &registry);
        if (status == MC_REGISTRY_OK) {
            const mc_block *selected;
            lower.bytecode = 23;
            status = mc_registry_block_m_at(&array, id, &selected, &registry);
            if (status == MC_REGISTRY_OK) {
                lower.bytecode = 24;
                if (selected == NULL) {
                    registry.status = MC_REGISTRY_NULL_REFERENCE;
                    registry.message = "World material Block receiver";
                    (void)mc_world_mutation_registry_fail(frame->world, &lower, &registry,
                                                          "World material registry");
                    return fluid_nested(frame, pc, &lower);
                }
                material = selected->fields.bA;
            }
        }
    }
    if (status != MC_REGISTRY_OK) {
        (void)mc_world_mutation_registry_fail(frame->world, &lower, &registry,
                                              "World fluid material Registry");
        return fluid_nested(frame, pc, &lower);
    }
    /* Match the existing actual World material operation's success diagnostic. */
    (void)mc_world_runtime_registry_result(frame->world, status, &registry,
                                           "World material registry", &cause);
    *out = material; /* Nullable bA is a normal field result. */
    return true;
}

static bool fluid_static_id(const fluid_frame *frame, const char *name, unsigned int load_pc,
                            unsigned int field_pc, int32_t *out) {
    const mc_block *selected;
    mc_registry_error registry = mc_world_mutation_empty_registry();
    fluid_at(frame, load_pc);
    if (mc_registry_block_static_get(frame->block->classes, name, &selected, &registry) !=
        MC_REGISTRY_OK)
        return fluid_registry_fail(frame, load_pc, &registry, "Fluid live Block static");
    if (selected == NULL)
        return fluid_fail(frame, field_pc, "Fluid live Block field receiver");
    *out = selected->fields.bn;
    return true;
}

static bool fluid_array_block(const fluid_frame *frame, unsigned int load_pc, unsigned int index_pc,
                              int32_t id, const mc_block **out) {
    mc_registry_block_m_view array;
    mc_registry_error registry = mc_world_mutation_empty_registry();
    fluid_at(frame, load_pc);
    if (mc_registry_block_m_get(frame->block->classes, &array, &registry) != MC_REGISTRY_OK)
        return fluid_registry_fail(frame, load_pc, &registry, "Fluid live Block.m");
    fluid_at(frame, index_pc);
    if (mc_registry_block_m_at(&array, id, out, &registry) != MC_REGISTRY_OK)
        return fluid_registry_fail(frame, index_pc, &registry, "Fluid captured Block.m element");
    return true;
}

static bool fluid_write_id(const fluid_frame *frame, unsigned int pc, int32_t x, int32_t y,
                           int32_t z, int32_t id) {
    bool ignored;
    mc_world_mutation_error lower = mc_world_mutation_empty_error();
    fluid_at(frame, pc);
    if (mc_world_runtime_block_set(frame->world, x, y, z, id, &ignored, &lower) !=
        MC_WORLD_STORAGE_OK)
        return fluid_nested(frame, pc, &lower);
    return true;
}

static bool fluid_write(const fluid_frame *frame, unsigned int pc, int32_t x, int32_t y, int32_t z,
                        int32_t id, int32_t metadata, bool quiet) {
    bool ignored;
    mc_world_mutation_error lower = mc_world_mutation_empty_error();
    fluid_at(frame, pc);
    const mc_world_storage_status status =
        quiet ? mc_world_runtime_block_metadata_set_quiet(frame->world, x, y, z, id, metadata,
                                                          &ignored, &lower)
              : mc_world_runtime_block_metadata_set(frame->world, x, y, z, id, metadata, &ignored,
                                                    &lower);
    if (status != MC_WORLD_STORAGE_OK)
        return fluid_nested(frame, pc, &lower);
    return true;
}

static bool fluid_write_metadata(const fluid_frame *frame, unsigned int pc, int32_t x, int32_t y,
                                 int32_t z, int32_t metadata) {
    mc_world_mutation_error lower = mc_world_mutation_empty_error();
    fluid_at(frame, pc);
    if (mc_world_runtime_metadata_set(frame->world, x, y, z, metadata, &lower) !=
        MC_WORLD_STORAGE_OK)
        return fluid_nested(frame, pc, &lower);
    return true;
}

static bool fluid_observe(const fluid_frame *frame, unsigned int pc, int32_t x, int32_t y,
                          int32_t z, bool range) {
    mc_world_mutation_error lower = mc_world_mutation_empty_error();
    fluid_at(frame, pc);
    const bool complete =
        range ? mc_world_mutation_observe_range(frame->world, x, y, z, x, y, z, &lower)
              : mc_world_mutation_observe_cell(frame->world, x, y, z, &lower);
    return complete ? true : fluid_nested(frame, pc, &lower);
}

static bool fluid_schedule(const fluid_frame *frame, unsigned int pc, int32_t x, int32_t y,
                           int32_t z, int32_t id, int32_t delay) {
    mc_world_schedule_error scheduled;
    fluid_at(frame, pc);
    if (mc_world_runtime_schedule(frame->world, x, y, z, id, delay, &scheduled) ==
        MC_WORLD_STORAGE_OK)
        return true;
    mc_world_mutation_error lower = scheduled.tick;
    lower.status = scheduled.status;
    lower.operation = scheduled.operation;
    lower.cause = scheduled.cause;
    lower.registry = scheduled.registry;
    lower.schedule_bytecode = scheduled.bytecode;
    lower.record = scheduled.record;
    lower.membership = scheduled.membership;
    lower.ordered = scheduled.ordered;
    if (scheduled.presence_bytecode != 0)
        lower.presence_bytecode = scheduled.presence_bytecode;
    if (scheduled.provider_bytecode != 0)
        lower.provider_bytecode = scheduled.provider_bytecode;
    return fluid_nested(frame, pc, &lower);
}

static bool fluid_bounded(const fluid_frame *frame, unsigned int pc, mc_java_random *random,
                          int32_t bound, int32_t *out) {
    fluid_at(frame, pc);
    if (random == NULL)
        return fluid_fail(frame, pc, "Fluid Random method receiver");
    if (!mc_java_random_next_bounded(random, bound, out))
        return fluid_fail(frame, pc, "Fluid Random bound");
    return true;
}

static int32_t fluid_delay(const fluid_frame *frame) {
    if (frame->block->fields.bA == fluid_material_static(frame, MC_MATERIAL_g))
        return 5;
    if (frame->block->fields.bA == fluid_material_static(frame, MC_MATERIAL_h))
        return 30;
    return 0;
}

static bool fluid_raw_level(mc_world_runtime *world, mc_block *block, int32_t x, int32_t y,
                            int32_t z, int32_t *out, mc_world_mutation_error *error) {
    const fluid_frame frame =
        fluid_frame_make(world, block, "rp", "kv", "h(Lfd;III)I", "g(Ldj;III)I", error);
    mc_material *material;
    if (!fluid_material(&frame, 5, x, y, z, &material))
        return false;
    if (material != block->fields.bA) {
        *out = -1;
        return fluid_return(&frame);
    }
    if (!fluid_metadata(&frame, 22, x, y, z, out))
        return false;
    return fluid_return(&frame);
}

static bool fluid_minimum(mc_world_runtime *world, mc_block *block, int32_t x, int32_t y, int32_t z,
                          int32_t best, int32_t *out, mc_world_mutation_error *error) {
    const fluid_frame frame =
        fluid_frame_make(world, block, "om", "jc", "f(Lfd;IIII)I", "f(Ldj;IIII)I", error);
    int32_t level;
    if (!fluid_raw_level(world, block, x, y, z, &level, error))
        return fluid_nested(&frame, 6, error);
    if (level < 0) {
        *out = best;
        return fluid_return(&frame);
    }
    if (level == 0) {
        fluid_at(&frame, 31);
        block->subtype.om.a = mc_java_i32_add(block->subtype.om.a, 1);
    }
    if (level >= 8)
        level = 0;
    *out = best < 0 || level < best ? level : best;
    return fluid_return(&frame);
}

static void fluid_direction(int32_t direction, int32_t x, int32_t z, int32_t *next_x,
                            int32_t *next_z) {
    *next_x = x;
    *next_z = z;
    if (direction == 0)
        *next_x = mc_java_i32_sub(*next_x, 1);
    if (direction == 1)
        *next_x = mc_java_i32_add(*next_x, 1);
    if (direction == 2)
        *next_z = mc_java_i32_sub(*next_z, 1);
    if (direction == 3)
        *next_z = mc_java_i32_add(*next_z, 1);
}

static bool fluid_blocked(mc_world_runtime *world, mc_block *block, int32_t x, int32_t y, int32_t z,
                          bool *out, mc_world_mutation_error *error) {
    const fluid_frame frame =
        fluid_frame_make(world, block, "om", "jc", "l(Lfd;III)Z", "k(Ldj;III)Z", error);
    int32_t id;
    if (!fluid_id(&frame, 5, x, y, z, &id))
        return false;
    static const char *const names[5] = {"aF", "aM", "aE", "aG", "aY"};
    static const unsigned int load_pc[5] = {12, 23, 34, 45, 56};
    static const unsigned int field_pc[5] = {15, 26, 37, 48, 59};
    for (size_t i = 0; i < 5; ++i) {
        int32_t special;
        if (!fluid_static_id(&frame, names[i], load_pc[i], field_pc[i], &special))
            return false;
        if (id == special) {
            *out = true;
            return fluid_return(&frame);
        }
    }
    if (id == 0) {
        *out = false;
        return fluid_return(&frame);
    }
    const mc_block *selected;
    if (!fluid_array_block(&frame, 74, 79, id, &selected))
        return false;
    if (selected == NULL)
        return fluid_fail(&frame, 80, "Fluid blocked Block receiver");
    mc_material *material = selected->fields.bA;
    if (material == NULL)
        return fluid_fail(&frame, 87, "Fluid blocked Material receiver");
    *out = mc_material_c(material);
    return fluid_return(&frame);
}

static bool fluid_eligible(mc_world_runtime *world, mc_block *block, int32_t x, int32_t y,
                           int32_t z, bool *out, mc_world_mutation_error *error) {
    const fluid_frame frame =
        fluid_frame_make(world, block, "om", "jc", "m(Lfd;III)Z", "l(Ldj;III)Z", error);
    mc_material *material;
    if (!fluid_material(&frame, 5, x, y, z, &material))
        return false;
    if (material == block->fields.bA || material == fluid_material_static(&frame, MC_MATERIAL_h)) {
        *out = false;
        return fluid_return(&frame);
    }
    bool blocked;
    if (!fluid_blocked(world, block, x, y, z, &blocked, error))
        return fluid_nested(&frame, 37, error);
    *out = !blocked;
    return fluid_return(&frame);
}

static bool fluid_distance(mc_world_runtime *world, mc_block *block, int32_t x, int32_t y,
                           int32_t z, int32_t depth, int32_t previous, int32_t *out,
                           mc_world_mutation_error *error) {
    const fluid_frame frame =
        fluid_frame_make(world, block, "om", "jc", "b(Lfd;IIIII)I", "b(Ldj;IIIII)I", error);
    int32_t best = 1000;
    for (int32_t direction = 0; direction < 4; ++direction) {
        if ((direction == 0 && previous == 1) || (direction == 1 && previous == 0) ||
            (direction == 2 && previous == 3) || (direction == 3 && previous == 2))
            continue;
        int32_t nx, nz;
        fluid_direction(direction, x, z, &nx, &nz);
        bool blocked;
        if (!fluid_blocked(world, block, nx, y, nz, &blocked, error))
            return fluid_nested(&frame, 125, error);
        if (blocked)
            continue;
        mc_material *material;
        if (!fluid_material(&frame, 141, nx, y, nz, &material))
            return false;
        if (material == block->fields.bA) {
            int32_t metadata;
            if (!fluid_metadata(&frame, 158, nx, y, nz, &metadata))
                return false;
            if (metadata == 0)
                continue;
        }
        if (!fluid_blocked(world, block, nx, mc_java_i32_sub(y, 1), nz, &blocked, error))
            return fluid_nested(&frame, 177, error);
        if (!blocked) {
            *out = depth;
            return fluid_return(&frame);
        }
        if (depth < 4) {
            int32_t distance;
            if (!fluid_distance(world, block, nx, y, nz, mc_java_i32_add(depth, 1), direction,
                                &distance, error))
                return fluid_nested(&frame, 206, error);
            if (distance < best)
                best = distance;
        }
    }
    *out = best;
    return fluid_return(&frame);
}

static bool fluid_routes(mc_world_runtime *world, mc_block *block, int32_t x, int32_t y, int32_t z,
                         bool **out, mc_world_mutation_error *error) {
    const fluid_frame frame =
        fluid_frame_make(world, block, "om", "jc", "k(Lfd;III)[Z", "j(Ldj;III)[Z", error);
    for (int32_t direction = 0; direction < 4; ++direction) {
        fluid_at(&frame, 18);
        if (block->subtype.om.c == NULL)
            return fluid_fail(&frame, 18, "Fluid route cost array");
        block->subtype.om.c[direction] = 1000;
        int32_t nx, nz;
        fluid_direction(direction, x, z, &nx, &nz);
        bool blocked;
        if (!fluid_blocked(world, block, nx, y, nz, &blocked, error))
            return fluid_nested(&frame, 72, error);
        if (blocked)
            continue;
        mc_material *material;
        if (!fluid_material(&frame, 88, nx, y, nz, &material))
            return false;
        if (material == block->fields.bA) {
            int32_t metadata;
            if (!fluid_metadata(&frame, 105, nx, y, nz, &metadata))
                return false;
            if (metadata == 0)
                continue;
        }
        if (!fluid_blocked(world, block, nx, mc_java_i32_sub(y, 1), nz, &blocked, error))
            return fluid_nested(&frame, 124, error);
        if (!blocked) {
            fluid_at(&frame, 137);
            if (block->subtype.om.c == NULL)
                return fluid_fail(&frame, 137, "Fluid route cost array");
            block->subtype.om.c[direction] = 0;
        } else {
            /* Source c142 is captured before recursive argument evaluation;
             * array null is tested at the later store161, after recursion. */
            int32_t *const costs = block->subtype.om.c;
            int32_t distance;
            if (!fluid_distance(world, block, nx, y, nz, 1, direction, &distance, error))
                return fluid_nested(&frame, 158, error);
            fluid_at(&frame, 161);
            if (costs == NULL)
                return fluid_fail(&frame, 161, "Fluid captured route cost array");
            costs[direction] = distance;
        }
    }
    if (block->subtype.om.c == NULL)
        return fluid_fail(&frame, 173, "Fluid route minimum array");
    int32_t best = block->subtype.om.c[0];
    for (int32_t i = 1; i < 4; ++i) {
        if (block->subtype.om.c == NULL)
            return fluid_fail(&frame, 191, "Fluid route minimum array");
        if (block->subtype.om.c[i] < best) {
            if (block->subtype.om.c == NULL)
                return fluid_fail(&frame, 203, "Fluid route minimum array");
            best = block->subtype.om.c[i];
        }
    }
    for (int32_t i = 0; i < 4; ++i) {
        bool *const flags = block->subtype.om.b; /* Actual capture222. */
        if (block->subtype.om.c == NULL)
            return fluid_fail(&frame, 233, "Fluid route comparison array");
        const bool selected = block->subtype.om.c[i] == best;
        fluid_at(&frame, 244);
        if (flags == NULL)
            return fluid_fail(&frame, 244, "Fluid route flag array");
        flags[i] = selected;
    }
    *out = block->subtype.om.b; /* Source reload252, no new array. */
    return fluid_return(&frame);
}

static bool fluid_flow_to_still(mc_world_runtime *world, mc_block *block, int32_t x, int32_t y,
                                int32_t z, mc_world_mutation_error *error) {
    const fluid_frame frame =
        fluid_frame_make(world, block, "om", "jc", "j(Lfd;III)V", "i(Ldj;III)V", error);
    int32_t metadata;
    if (!fluid_metadata(&frame, 5, x, y, z, &metadata))
        return false;
    const int32_t id = mc_java_i32_add(block->fields.bn, 1);
    if (!fluid_write(&frame, 23, x, y, z, id, metadata, true))
        return false;
    if (!fluid_observe(&frame, 36, x, y, z, true))
        return false;
    if (!fluid_observe(&frame, 44, x, y, z, false))
        return false;
    return fluid_return(&frame);
}

static bool fluid_still_to_flow(mc_world_runtime *world, mc_block *block, int32_t x, int32_t y,
                                int32_t z, mc_world_mutation_error *error) {
    const fluid_frame frame =
        fluid_frame_make(world, block, "nx", "is", "j(Lfd;III)V", "i(Ldj;III)V", error);
    int32_t metadata;
    if (!fluid_metadata(&frame, 5, x, y, z, &metadata))
        return false;
    fluid_at(&frame, 12);
    world->fields.o = true;
    const int32_t id = mc_java_i32_sub(block->fields.bn, 1);
    if (!fluid_write(&frame, 28, x, y, z, id, metadata, true))
        return false;
    if (!fluid_observe(&frame, 41, x, y, z, true))
        return false;
    const int32_t scheduled_id = mc_java_i32_sub(block->fields.bn, 1);
    const int32_t delay = fluid_delay(&frame);
    if (!fluid_schedule(&frame, 59, x, y, z, scheduled_id, delay))
        return false;
    fluid_at(&frame, 64);
    world->fields.o = false;
    return fluid_return(&frame);
}

static bool fluid_fizz(mc_world_runtime *world, mc_block *block, int32_t x, int32_t y, int32_t z,
                       mc_world_mutation_error *error);

static bool fluid_spill(mc_world_runtime *world, mc_block *block, int32_t x, int32_t y, int32_t z,
                        int32_t metadata, mc_world_mutation_error *error) {
    const fluid_frame frame =
        fluid_frame_make(world, block, "om", "jc", "h(Lfd;IIII)V", "h(Ldj;IIII)V", error);
    bool eligible;
    if (!fluid_eligible(world, block, x, y, z, &eligible, error))
        return fluid_nested(&frame, 6, error);
    if (!eligible)
        return fluid_return(&frame);
    int32_t old;
    if (!fluid_id(&frame, 17, x, y, z, &old))
        return false;
    if (old > 0) {
        if (block->fields.bA == fluid_material_static(&frame, MC_MATERIAL_h)) {
            if (!fluid_fizz(world, block, x, y, z, error))
                return fluid_nested(&frame, 43, error);
        } else {
            const mc_block *selected;
            if (!fluid_array_block(&frame, 49, 54, old, &selected))
                return false;
            int32_t old_metadata;
            if (!fluid_metadata(&frame, 65, x, y, z, &old_metadata))
                return false;
            mc_world_mutation_error lower = mc_world_mutation_empty_error();
            fluid_at(&frame, 68);
            if (!mc_world_mutation_drop(world, selected, x, y, z, old_metadata, &lower))
                return fluid_nested(&frame, 68, &lower);
        }
    }
    const int32_t id = block->fields.bn;
    if (!fluid_write(&frame, 82, x, y, z, id, metadata, false))
        return false;
    return fluid_return(&frame);
}

static bool fluid_flowing_tick(mc_world_runtime *world, mc_block *block, int32_t x, int32_t y,
                               int32_t z, mc_java_random *random, mc_world_mutation_error *error) {
    const fluid_frame frame =
        fluid_frame_make(world, block, "om", "jc", "a(Lfd;IIILjava/util/Random;)V",
                         "a(Ldj;IIILjava/util/Random;)V", error);
    int32_t level;
    if (!fluid_raw_level(world, block, x, y, z, &level, error))
        return fluid_nested(&frame, 6, error);
    int32_t increment = 1;
    if (block->fields.bA == fluid_material_static(&frame, MC_MATERIAL_h)) {
        mc_world_dimension_fields dimension;
        fluid_at(&frame, 28);
        if (mc_world_dimension_fields_get(world->fields.t, &dimension) != MC_WORLD_DIMENSION_OK)
            return fluid_fail(&frame, 28, "Fluid Dimension.d receiver");
        if (!dimension.d)
            increment = 2;
    }
    bool settle = true;
    if (level > 0) {
        int32_t best = -100;
        fluid_at(&frame, 51);
        block->subtype.om.a = 0;
        static const unsigned int calls[4] = {64, 79, 94, 109};
        for (int32_t direction = 0; direction < 4; ++direction) {
            int32_t nx, nz;
            fluid_direction(direction, x, z, &nx, &nz);
            if (!fluid_minimum(world, block, nx, y, nz, best, &best, error))
                return fluid_nested(&frame, calls[direction], error);
        }
        int32_t candidate = mc_java_i32_add(best, increment);
        if (candidate >= 8 || best < 0)
            candidate = -1;
        int32_t above;
        if (!fluid_raw_level(world, block, x, mc_java_i32_add(y, 1), z, &above, error))
            return fluid_nested(&frame, 144, error);
        if (above >= 0) {
            if (!fluid_raw_level(world, block, x, mc_java_i32_add(y, 1), z, &above, error))
                return fluid_nested(&frame, 158, error);
            candidate = above >= 8 ? above : mc_java_i32_add(above, 8);
        }
        if (block->subtype.om.a >= 2 &&
            block->fields.bA == fluid_material_static(&frame, MC_MATERIAL_g)) {
            mc_material *below;
            if (!fluid_material(&frame, 209, x, mc_java_i32_sub(y, 1), z, &below))
                return false;
            if (below == NULL)
                return fluid_fail(&frame, 212, "Fluid source support Material receiver");
            if (mc_material_a(below)) {
                candidate = 0;
            } else {
                if (!fluid_material(&frame, 231, x, mc_java_i32_sub(y, 1), z, &below))
                    return false;
                if (below == block->fields.bA) {
                    int32_t center;
                    if (!fluid_metadata(&frame, 246, x, y, z, &center))
                        return false;
                    if (center == 0)
                        candidate = 0;
                }
            }
        }
        if (block->fields.bA == fluid_material_static(&frame, MC_MATERIAL_h) && level < 8 &&
            candidate < 8 && candidate > level) {
            int32_t draw;
            if (!fluid_bounded(&frame, 289, random, 4, &draw))
                return false;
            if (draw != 0) {
                candidate = level;
                settle = false;
            }
        }
        if (candidate != level) {
            level = candidate;
            if (level < 0) {
                if (!fluid_write_id(&frame, 324, x, y, z, 0))
                    return false;
            } else {
                if (!fluid_write_metadata(&frame, 338, x, y, z, level))
                    return false;
                const int32_t id = block->fields.bn;
                const int32_t delay = fluid_delay(&frame);
                if (!fluid_schedule(&frame, 354, x, y, z, id, delay))
                    return false;
                const int32_t notify_id = block->fields.bn;
                mc_world_mutation_error lower = mc_world_mutation_empty_error();
                fluid_at(&frame, 366);
                if (!mc_world_mutation_neighbors(world, x, y, z, notify_id, &lower))
                    return fluid_nested(&frame, 366, &lower);
            }
        } else if (settle) {
            if (!fluid_flow_to_still(world, block, x, y, z, error))
                return fluid_nested(&frame, 383, error);
        }
    } else {
        if (!fluid_flow_to_still(world, block, x, y, z, error))
            return fluid_nested(&frame, 395, error);
    }

    bool eligible;
    if (!fluid_eligible(world, block, x, mc_java_i32_sub(y, 1), z, &eligible, error))
        return fluid_nested(&frame, 406, error);
    if (eligible) {
        const int32_t id = block->fields.bn;
        if (level >= 8) {
            if (!fluid_write(&frame, 432, x, mc_java_i32_sub(y, 1), z, id, level, false))
                return false;
        } else {
            if (!fluid_write(&frame, 455, x, mc_java_i32_sub(y, 1), z, id,
                             mc_java_i32_add(level, 8), false))
                return false;
        }
        return fluid_return(&frame);
    }
    if (level < 0)
        return fluid_return(&frame);
    if (level != 0) {
        bool blocked;
        if (!fluid_blocked(world, block, x, mc_java_i32_sub(y, 1), z, &blocked, error))
            return fluid_nested(&frame, 480, error);
        if (!blocked)
            return fluid_return(&frame);
    }
    bool *routes;
    if (!fluid_routes(world, block, x, y, z, &routes, error))
        return fluid_nested(&frame, 492, error);
    int32_t outgoing = mc_java_i32_add(level, increment);
    if (level >= 8)
        outgoing = 1;
    if (outgoing >= 8)
        return fluid_return(&frame);
    static const unsigned int read_pc[4] = {525, 545, 565, 585};
    static const unsigned int spill_pc[4] = {539, 559, 579, 599};
    for (int32_t direction = 0; direction < 4; ++direction) {
        if (routes == NULL)
            return fluid_fail(&frame, read_pc[direction], "Fluid captured route flags");
        if (routes[direction]) {
            int32_t nx, nz;
            fluid_direction(direction, x, z, &nx, &nz);
            if (!fluid_spill(world, block, nx, y, nz, outgoing, error))
                return fluid_nested(&frame, spill_pc[direction], error);
        }
    }
    return fluid_return(&frame);
}

static bool fluid_literal(const fluid_frame *frame, unsigned int pc, const char *text,
                          const mc_registry_string **out) {
    mc_registry_error registry = mc_world_mutation_empty_registry();
    fluid_at(frame, pc);
    if (mc_registry_runtime_literal_ascii(frame->block->classes, text, out, &registry) !=
        MC_REGISTRY_OK)
        return fluid_registry_fail(frame, pc, &registry, "Fluid source literal resolution");
    return true;
}

static bool fluid_math(const fluid_frame *frame, unsigned int pc, double *out) {
    fluid_at(frame, pc);
    const mc_java_random_profile_status status =
        mc_java_math_random(frame->block->classes->options.random_profile, out);
    frame->world->last_random_status = status;
    if (status == MC_JAVA_RANDOM_PROFILE_OK)
        return true;
    const mc_world_storage_status mapped =
        status == MC_JAVA_RANDOM_PROFILE_LIMIT               ? MC_WORLD_STORAGE_LIMIT
        : status == MC_JAVA_RANDOM_PROFILE_ALLOCATION_FAILED ? MC_WORLD_STORAGE_ALLOCATION_FAILED
        : status == MC_JAVA_RANDOM_PROFILE_INVALID_ARGUMENT  ? MC_WORLD_STORAGE_INVALID_ARGUMENT
                                                             : MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    return mc_world_mutation_fail(frame->world, frame->error, mapped, "Fluid actual Math.random");
}

static bool fluid_fizz(mc_world_runtime *world, mc_block *block, int32_t x, int32_t y, int32_t z,
                       mc_world_mutation_error *error) {
    const fluid_frame frame =
        fluid_frame_make(world, block, "rp", "kv", "i(Lfd;III)V", "h(Ldj;III)V", error);
    /* Native FP admission is at the first reached floating operation, not at
     * tick/add/neighbor entry before earlier integer/World effects. */
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
        return fluid_fail(&frame, 2, "Fluid fizz FP admission");
    const float fx = (float)x;
    const float sx = fx + 0.5f;
    const float fy = (float)y;
    const float sy = fy + 0.5f;
    const float fz = (float)z;
    const float sz = fz + 0.5f;
    const mc_registry_string *sound;
    if (!fluid_literal(&frame, 20, "random.fizz", &sound))
        return false;
    mc_java_random *random = world->fields.r;
    fluid_at(&frame, 30);
    if (random == NULL)
        return fluid_fail(&frame, 30, "Fluid fizz Random receiver");
    const float first = mc_java_random_next_float(random);
    random = world->fields.r;
    fluid_at(&frame, 37);
    if (random == NULL)
        return fluid_fail(&frame, 37, "Fluid fizz Random receiver");
    const float second = mc_java_random_next_float(random);
    const float difference = first - second;
    const float scaled = difference * 0.8f;
    const float pitch = 2.6f + scaled;
    mc_world_mutation_error lower = mc_world_mutation_empty_error();
    fluid_at(&frame, 45);
    if (!mc_world_mutation_sound(world, (double)sx, (double)sy, (double)sz, sound, 0.5f, pitch,
                                 &lower))
        return fluid_nested(&frame, 45, &lower);
    for (int32_t i = 0; i < 8; ++i) {
        const mc_registry_string *particle;
        if (!fluid_literal(&frame, 59, "largesmoke", &particle))
            return false;
        double draw_x;
        if (!fluid_math(&frame, 63, &draw_x))
            return false;
        const double px = (double)x + draw_x;
        const double py = (double)y + 1.2;
        double draw_z;
        if (!fluid_math(&frame, 76, &draw_z))
            return false;
        const double pz = (double)z + draw_z;
        lower = mc_world_mutation_empty_error();
        fluid_at(&frame, 83);
        if (!mc_world_mutation_particle(world, particle, px, py, pz, 0.0, 0.0, 0.0, &lower))
            return fluid_nested(&frame, 83, &lower);
    }
    return fluid_return(&frame);
}

static bool fluid_mix(mc_world_runtime *world, mc_block *block, int32_t x, int32_t y, int32_t z,
                      mc_world_mutation_error *error) {
    const fluid_frame frame =
        fluid_frame_make(world, block, "rp", "kv", "j(Lfd;III)V", "i(Ldj;III)V", error);
    int32_t current;
    if (!fluid_id(&frame, 5, x, y, z, &current))
        return false;
    if (current != block->fields.bn)
        return fluid_return(&frame);
    if (block->fields.bA != fluid_material_static(&frame, MC_MATERIAL_h))
        return fluid_return(&frame);
    bool contact = false;
    static const unsigned int query_pc[5] = {41, 65, 89, 113, 137};
    for (int32_t direction = 0; direction < 5; ++direction) {
        if (contact)
            continue;
        int32_t nx = x, ny = y, nz = z;
        if (direction == 0)
            nz = mc_java_i32_sub(nz, 1);
        if (direction == 1)
            nz = mc_java_i32_add(nz, 1);
        if (direction == 2)
            nx = mc_java_i32_sub(nx, 1);
        if (direction == 3)
            nx = mc_java_i32_add(nx, 1);
        if (direction == 4)
            ny = mc_java_i32_add(ny, 1);
        mc_material *material;
        if (!fluid_material(&frame, query_pc[direction], nx, ny, nz, &material))
            return false;
        if (material == fluid_material_static(&frame, MC_MATERIAL_g))
            contact = true;
    }
    if (contact) {
        int32_t metadata;
        if (!fluid_metadata(&frame, 159, x, y, z, &metadata))
            return false;
        if (metadata == 0) {
            int32_t id;
            if (!fluid_static_id(&frame, "aq", 174, 177, &id))
                return false;
            if (!fluid_write_id(&frame, 180, x, y, z, id))
                return false;
        } else if (metadata <= 4) {
            int32_t id;
            if (!fluid_static_id(&frame, "x", 198, 201, &id))
                return false;
            if (!fluid_write_id(&frame, 204, x, y, z, id))
                return false;
        }
        if (!fluid_fizz(world, block, x, y, z, error))
            return fluid_nested(&frame, 214, error);
    }
    return fluid_return(&frame);
}

static bool fluid_common_added(mc_world_runtime *world, mc_block *block, int32_t x, int32_t y,
                               int32_t z, mc_world_mutation_error *error) {
    const fluid_frame frame =
        fluid_frame_make(world, block, "rp", "kv", "c(Lfd;III)V", "c(Ldj;III)V", error);
    if (!fluid_mix(world, block, x, y, z, error))
        return fluid_nested(&frame, 6, error);
    return fluid_return(&frame);
}

static bool fluid_common_neighbor(mc_world_runtime *world, mc_block *block, int32_t x, int32_t y,
                                  int32_t z, mc_world_mutation_error *error) {
    const fluid_frame frame =
        fluid_frame_make(world, block, "rp", "kv", "b(Lfd;IIII)V", "b(Ldj;IIII)V", error);
    if (!fluid_mix(world, block, x, y, z, error))
        return fluid_nested(&frame, 6, error);
    return fluid_return(&frame);
}

static bool fluid_flowing_added(mc_world_runtime *world, mc_block *block, int32_t x, int32_t y,
                                int32_t z, mc_world_mutation_error *error) {
    const fluid_frame frame =
        fluid_frame_make(world, block, "om", "jc", "c(Lfd;III)V", "c(Ldj;III)V", error);
    if (!fluid_common_added(world, block, x, y, z, error))
        return fluid_nested(&frame, 6, error);
    int32_t current;
    if (!fluid_id(&frame, 14, x, y, z, &current))
        return false;
    if (current == block->fields.bn) {
        const int32_t id = block->fields.bn;
        const int32_t delay = fluid_delay(&frame);
        if (!fluid_schedule(&frame, 37, x, y, z, id, delay))
            return false;
    }
    return fluid_return(&frame);
}

static bool fluid_still_neighbor(mc_world_runtime *world, mc_block *block, int32_t x, int32_t y,
                                 int32_t z, mc_world_mutation_error *error) {
    const fluid_frame frame =
        fluid_frame_make(world, block, "nx", "is", "b(Lfd;IIII)V", "b(Ldj;IIII)V", error);
    if (!fluid_common_neighbor(world, block, x, y, z, error))
        return fluid_nested(&frame, 8, error);
    int32_t current;
    if (!fluid_id(&frame, 16, x, y, z, &current))
        return false;
    if (current == block->fields.bn) {
        if (!fluid_still_to_flow(world, block, x, y, z, error))
            return fluid_nested(&frame, 32, error);
    }
    return fluid_return(&frame);
}

static bool fluid_ignition_material(mc_world_runtime *world, mc_block *block, int32_t x, int32_t y,
                                    int32_t z, bool *out, mc_world_mutation_error *error) {
    const fluid_frame frame =
        fluid_frame_make(world, block, "nx", "is", "k(Lfd;III)Z", "j(Ldj;III)Z", error);
    mc_material *material;
    if (!fluid_material(&frame, 5, x, y, z, &material))
        return false;
    if (material == NULL)
        return fluid_fail(&frame, 8, "Fluid ignition Material receiver");
    *out = mc_material_e(material);
    return fluid_return(&frame);
}

static bool fluid_still_tick(mc_world_runtime *world, mc_block *block, int32_t x, int32_t y,
                             int32_t z, mc_java_random *random, mc_world_mutation_error *error) {
    const fluid_frame frame =
        fluid_frame_make(world, block, "nx", "is", "a(Lfd;IIILjava/util/Random;)V",
                         "a(Ldj;IIILjava/util/Random;)V", error);
    if (block->fields.bA != fluid_material_static(&frame, MC_MATERIAL_h))
        return fluid_return(&frame);
    int32_t count;
    if (!fluid_bounded(&frame, 13, random, 3, &count))
        return false;
    for (int32_t i = 0; i < count; ++i) {
        int32_t draw;
        if (!fluid_bounded(&frame, 32, random, 3, &draw))
            return false;
        x = mc_java_i32_add(x, mc_java_i32_sub(draw, 1));
        y = mc_java_i32_add(y, 1);
        if (!fluid_bounded(&frame, 47, random, 3, &draw))
            return false;
        z = mc_java_i32_add(z, mc_java_i32_sub(draw, 1));
        int32_t id;
        if (!fluid_id(&frame, 60, x, y, z, &id))
            return false;
        if (id == 0) {
            bool ignites = false;
            static const unsigned int query_pc[6] = {78, 92, 106, 120, 134, 148};
            for (int32_t direction = 0; direction < 6; ++direction) {
                if (ignites)
                    continue;
                int32_t nx = x, ny = y, nz = z;
                if (direction < 4)
                    fluid_direction(direction, x, z, &nx, &nz);
                if (direction == 4)
                    ny = mc_java_i32_sub(ny, 1);
                if (direction == 5)
                    ny = mc_java_i32_add(ny, 1);
                if (!fluid_ignition_material(world, block, nx, ny, nz, &ignites, error))
                    return fluid_nested(&frame, query_pc[direction], error);
            }
            if (ignites) {
                int32_t fire;
                if (!fluid_static_id(&frame, "as", 159, 162, &fire))
                    return false;
                if (!fluid_write_id(&frame, 165, x, y, z, fire))
                    return false;
                return fluid_return(&frame);
            }
        } else {
            const mc_block *selected;
            if (!fluid_array_block(&frame, 170, 175, id, &selected))
                return false;
            if (selected == NULL)
                return fluid_fail(&frame, 176, "Still fluid Block receiver");
            mc_material *material = selected->fields.bA;
            if (material == NULL)
                return fluid_fail(&frame, 179, "Still fluid Material receiver");
            if (mc_material_c(material))
                return fluid_return(&frame);
        }
    }
    return fluid_return(&frame);
}

static bool fluid_receiver(mc_world_runtime *world, const mc_block *block,
                           mc_world_mutation_error *error) {
    if (block != NULL && (block->kind == MC_BLOCK_OM_JC || block->kind == MC_BLOCK_NX_IS))
        return true;
    error->hook_class = NULL;
    error->hook_method = NULL;
    error->hook_bytecode = 0;
    return mc_world_mutation_fail(world, error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                  block == NULL ? "Fluid hook receiver is null"
                                                : "Fluid hook concrete receiver kind");
}

bool mc_world_mutation_fluid_tick(mc_world_runtime *world, const mc_block *block, int32_t x,
                                  int32_t y, int32_t z, mc_java_random *random,
                                  mc_world_mutation_error *error) {
    if (!fluid_receiver(world, block, error))
        return false;
    return block->kind == MC_BLOCK_OM_JC
               ? fluid_flowing_tick(world, (mc_block *)block, x, y, z, random, error)
               : fluid_still_tick(world, (mc_block *)block, x, y, z, random, error);
}

bool mc_world_mutation_fluid_added(mc_world_runtime *world, const mc_block *block, int32_t x,
                                   int32_t y, int32_t z, mc_world_mutation_error *error) {
    if (!fluid_receiver(world, block, error))
        return false;
    return block->kind == MC_BLOCK_OM_JC
               ? fluid_flowing_added(world, (mc_block *)block, x, y, z, error)
               : fluid_common_added(world, (mc_block *)block, x, y, z, error);
}

bool mc_world_mutation_fluid_neighbor(mc_world_runtime *world, const mc_block *block, int32_t x,
                                      int32_t y, int32_t z, int32_t source_id,
                                      mc_world_mutation_error *error) {
    (void)source_id; /* Actual common liquid body does not use its fourth int. */
    if (!fluid_receiver(world, block, error))
        return false;
    return block->kind == MC_BLOCK_NX_IS
               ? fluid_still_neighbor(world, (mc_block *)block, x, y, z, error)
               : fluid_common_neighbor(world, (mc_block *)block, x, y, z, error);
}
