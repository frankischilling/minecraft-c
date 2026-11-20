#include "tnt_internal.h"

#include "../../../../block/registry/registry_internal.h"
#include "../../../../block/registry/runtime_internal.h"
#include "../../construction_internal.h"
#include "../mutation_internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/entity/primed_tnt/construction.h"
#include "mc/world/runtime/movement.h"

#include <float.h>
#include <stdlib.h>

#if !defined(_M_X64) && !defined(_M_AMD64) && !defined(__x86_64__)
#error "TNT centers require x86-64/SSE2"
#endif
#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) ||       \
    (defined(_M_FP_FAST) && _M_FP_FAST)
#error "TNT centers require strict floating transformations"
#endif
#include <emmintrin.h>
_Static_assert(FLT_RADIX == 2 && FLT_MANT_DIG == 24 && DBL_MANT_DIG == 53 && sizeof(float) == 4 &&
                   sizeof(double) == 8 && FLT_EVAL_METHOD == 0,
               "TNT centers require source-width evaluation");

static void at(mc_world_mutation_error *error, unsigned int pc) {
    error->hook_class = "ah";
    error->hook_method = "c(Lfd;IIII)V";
    error->hook_bytecode = pc;
}

static bool done(mc_world_mutation_error *error) {
    error->hook_class = NULL;
    error->hook_method = NULL;
    error->hook_bytecode = 0;
    return true;
}

static bool failed(mc_world_runtime *world, mc_world_mutation_error *error,
                   mc_world_storage_status status, const char *operation, unsigned int pc) {
    at(error, pc);
    return mc_world_mutation_fail(world, error, status, operation);
}

static bool cause(mc_world_runtime *world, mc_world_mutation_error *error,
                  mc_world_storage_error lower, unsigned int pc) {
    at(error, pc);
    error->status = lower.status;
    error->operation = lower.operation;
    error->cause = lower;
    if (world != NULL)
        world->last_error = lower;
    return false;
}

static bool entity_failed(mc_world_runtime *world, mc_world_mutation_error *error,
                          mc_entity_status status, unsigned int pc) {
    const mc_world_storage_status mapped =
        status == MC_ENTITY_LIMIT               ? MC_WORLD_STORAGE_LIMIT
        : status == MC_ENTITY_ALLOCATION_FAILED ? MC_WORLD_STORAGE_ALLOCATION_FAILED
        : status == MC_ENTITY_BUSY              ? MC_WORLD_STORAGE_BUSY
        : status == MC_ENTITY_INVALID_ARGUMENT  ? MC_WORLD_STORAGE_INVALID_ARGUMENT
        : status == MC_ENTITY_WORLD_FAILED && error->entity.world != MC_WORLD_STORAGE_OK
            ? error->entity.world
            : MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    return failed(
        world, error, mapped,
        error->entity.operation == NULL ? "TNT actual Entity child" : error->entity.operation, pc);
}

static bool drop(mc_world_runtime *world, const mc_block *block, int32_t x, int32_t y, int32_t z,
                 mc_world_mutation_error *error) {
    mc_item_stack *stack = NULL;
    at(error, 21);
    error->item = mc_item_stack_allocate(&stack);
    if (error->item != MC_ITEM_STACK_OK)
        return failed(world, error,
                      error->item == MC_ITEM_STACK_ALLOCATION_FAILED
                          ? MC_WORLD_STORAGE_ALLOCATION_FAILED
                          : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "TNT actual new ItemStack", 21);
    const mc_block *selected;
    mc_registry_error registry = mc_world_mutation_empty_registry();
    at(error, 25);
    if (mc_registry_block_static_get(block->classes, "an", &selected, &registry) !=
        MC_REGISTRY_OK) {
        mc_item_stack_release(stack);
        return mc_world_mutation_registry_fail(world, error, &registry, "TNT live Block.an");
    }
    at(error, 28);
    if (selected == NULL) {
        registry.status = MC_REGISTRY_NULL_REFERENCE;
        registry.message = "TNT live Block.an bn receiver";
        mc_item_stack_release(stack);
        return mc_world_mutation_registry_fail(world, error, &registry, registry.message);
    }
    const int32_t id = selected->fields.bn;
    at(error, 33);
    error->item = mc_item_stack_construct(stack, id, 1, 0);
    if (error->item != MC_ITEM_STACK_OK) {
        mc_item_stack_release(stack);
        return failed(world, error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "TNT actual ItemStack constructor", 33);
    }
    at(error, 36);
    mc_world_mutation_error *lower = malloc(sizeof(*lower));
    if (lower == NULL) {
        mc_item_stack_release(stack);
        return failed(world, error, MC_WORLD_STORAGE_ALLOCATION_FAILED,
                      "TNT drop diagnostic allocation", 36);
    }
    mc_world_mutation_error_init(lower);
    const bool complete = mc_world_mutation_drop_stack(world, block, x, y, z, stack, lower);
    mc_item_stack_release(stack);
    if (!complete) {
        *error = *lower;
        if (error->cause_hook_class == NULL) {
            error->cause_hook_class = error->hook_class;
            error->cause_hook_method = error->hook_method;
            error->cause_hook_bytecode = error->hook_bytecode;
        }
        at(error, 36);
    }
    free(lower);
    return complete;
}

static bool prime(mc_world_runtime *world, const mc_block *block, int32_t x, int32_t y, int32_t z,
                  mc_world_mutation_error *error) {
    at(error, 42);
    mc_entity_classes *classes = world->classes == NULL ? NULL : world->classes->entities;
    if (classes == NULL)
        return failed(world, error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "TNT actual defining EntityClasses binding", 42);
    mc_entity *entity = NULL;
    mc_entity_status status = mc_primed_tnt_entity_allocate(classes, &entity, &error->entity);
    if (status != MC_ENTITY_OK)
        return entity_failed(world, error, status, 42);
    /* new42 and its real class use precede every center conversion. */
    at(error, 48);
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u) {
        mc_entity_release(entity);
        return failed(world, error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "TNT center floating environment", 48);
    }
    const float center_x = (float)x + 0.5f;
    const float center_y = (float)y + 0.5f;
    const float center_z = (float)z + 0.5f;
    at(error, 66);
    status = mc_primed_tnt_entity_construct(entity, world, (double)center_x, (double)center_y,
                                            (double)center_z, world->classes->trig, &error->entity);
    if (status != MC_ENTITY_OK) {
        mc_entity_release(entity);
        return entity_failed(world, error, status, 66);
    }
    at(error, 74);
    bool inserted;
    const mc_world_storage_status published =
        mc_world_runtime_insert_entity(world, entity, &inserted, &error->publication);
    if (published != MC_WORLD_STORAGE_OK) {
        /* Forward this actual failed producer occurrence to synchronous Block
         * callers; no ambient World/observer diagnostic is used to select it. */
        error->world_access_bytecode = error->publication.world_access_bytecode;
        error->world_access_failure = error->publication.world_access_failure;
        (void)cause(world, error, error->publication.cause, 74);
        mc_entity_release(entity);
        return false;
    }
    /* The source discards publication's Boolean and still sounds its actual
     * local Entity. Keep that occurrence through literal and argument reads. */
    const mc_registry_string *sound;
    mc_registry_error registry = mc_world_mutation_empty_registry();
    at(error, 81);
    if (mc_registry_runtime_literal_ascii(block->classes, "random.fuse", &sound, &registry) !=
        MC_REGISTRY_OK) {
        mc_entity_release(entity);
        return mc_world_mutation_registry_fail(world, error, &registry,
                                               "TNT random.fuse literal resolution");
    }
    at(error, 85);
    mc_world_movement_error *lower = malloc(sizeof(*lower));
    if (lower == NULL) {
        mc_entity_release(entity);
        return failed(world, error, MC_WORLD_STORAGE_ALLOCATION_FAILED,
                      "TNT sound diagnostic allocation", 85);
    }
    mc_world_movement_error_init(lower);
    const mc_world_storage_status sounded =
        mc_world_runtime_entity_sound(world, entity, sound, 1.0f, 1.0f, lower);
    if (sounded != MC_WORLD_STORAGE_OK) {
        error->world_access_bytecode = lower->world_access_bytecode;
        error->world_access_failure = lower->world_access_failure;
        if (lower->entity.status != MC_ENTITY_OK)
            error->entity = lower->entity;
        (void)cause(world, error, lower->cause, 85);
    }
    free(lower);
    mc_entity_release(entity);
    return sounded == MC_WORLD_STORAGE_OK;
}

bool mc_world_mutation_tnt_activate(mc_world_runtime *world, const mc_block *block, int32_t x,
                                    int32_t y, int32_t z, int32_t flags,
                                    mc_world_mutation_error *error) {
    if (error == NULL)
        return false;
    if (block == NULL || block->kind != MC_BLOCK_TNT || block->classes == NULL ||
        block->classes->options.side != MC_REGISTRY_CLIENT)
        return failed(world, error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "TNT actual client activation receiver", 0);
    at(error, 1);
    if (world == NULL)
        return failed(world, error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "TNT activation World.B receiver", 1);
    if (world->fields.B)
        return done(error);
    const bool complete =
        (flags & 1) == 0 ? drop(world, block, x, y, z, error) : prime(world, block, x, y, z, error);
    return complete && done(error);
}
