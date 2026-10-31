#include "mc/world/runtime/random_display.h"

#include "../../block/registry/registry_internal.h"
#include "../../block/registry/runtime_internal.h"
#include "construction_internal.h"
#include "mc/world/runtime/block_random_display.h"
#include "mc/world/storage/source_chunk_loader.h"
#include "runtime_internal.h"

#include <stdlib.h>
#include <string.h>

struct mc_world_random_display_call {
    mc_world_random_display_state state;
    mc_world_random_display_error error;
    mc_java_random random;
    mc_world_runtime_acquisition_error acquisition;
    mc_world_source_chunk_error source_chunk;
    mc_registry_error registry;
    mc_world_random_display_block_error block;
    mc_world_access_failure *additional_captures;
    size_t capture_count;
    bool world_held, source_chunk_copied;
};

static mc_world_storage_status rejected(mc_world_random_display_error *error,
                                        mc_world_storage_status status, const char *operation) {
    if (error != NULL) {
        memset(error, 0, sizeof(*error));
        error->status = status;
        error->operation = operation;
        error->native_policy = status != MC_WORLD_STORAGE_OK;
        error->cause = (mc_world_storage_error){status, operation, 0};
    }
    return status;
}

static void at(mc_world_random_display_call *c, unsigned int pc) {
    c->state.source_pc = c->error.bytecode = pc;
    c->error.child = MC_WORLD_RANDOM_DISPLAY_NONE;
    c->error.child_class = c->error.child_method = c->error.child_descriptor = NULL;
    c->error.child_bytecode = 0;
    c->error.acquisition = NULL;
    c->error.registry = NULL;
    c->error.block = NULL;
}

static bool fail(mc_world_random_display_call *c, mc_world_storage_status status,
                 const char *operation, bool native, const char *exception) {
    c->error.status = status;
    c->error.operation = operation;
    c->error.native_policy = native;
    c->error.exception_class = exception;
    c->error.cause = (mc_world_storage_error){status, operation, 0};
    return false;
}

static bool storage_fail(mc_world_random_display_call *c, mc_world_storage_error cause) {
    (void)fail(c, cause.status, cause.operation, true, NULL);
    c->error.cause = cause;
    return false;
}

static bool source_null(mc_world_random_display_call *c, const char *operation) {
    return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE, operation, false,
                "java/lang/NullPointerException");
}

static const char *registry_source_exception(mc_registry_status status) {
    switch (status) {
        case MC_REGISTRY_NULL_REFERENCE:
            return "java/lang/NullPointerException";
        case MC_REGISTRY_INDEX_OUT_OF_RANGE:
            return "java/lang/ArrayIndexOutOfBoundsException";
        case MC_REGISTRY_ILLEGAL_ARGUMENT:
        case MC_REGISTRY_DUPLICATE_BLOCK:
        case MC_REGISTRY_DUPLICATE_STATISTIC:
            return "java/lang/IllegalArgumentException";
        default:
            return NULL;
    }
}

static bool same_registry_cause(const mc_registry_error *a, const mc_registry_error *b,
                                bool cached) {
    return (cached ? a->status == MC_REGISTRY_FAILED_INITIALIZATION : a->status == b->status) &&
           a->message == b->message && a->initializer_pc == b->initializer_pc &&
           a->block_id == b->block_id && a->constructor_class == b->constructor_class &&
           a->constructor_pc == b->constructor_pc && a->profile_status == b->profile_status &&
           a->active_initializer_class == b->active_initializer_class &&
           a->active_initializer_pc == b->active_initializer_pc &&
           a->item_argument == b->item_argument &&
           a->format_error.status == b->format_error.status &&
           a->format_error.offset == b->format_error.offset &&
           a->format_error.length == b->format_error.length &&
           a->format_error.value == b->format_error.value &&
           a->format_error.flags == b->format_error.flags &&
           a->format_error.conversion == b->format_error.conversion &&
           a->format_error.date_time == b->format_error.date_time;
}

static void registry_exception(mc_world_random_display_call *c, const mc_registry_error *error) {
    const mc_registry_classes *classes = c->state.world->classes->registry;
    const bool cached = error->status == MC_REGISTRY_FAILED_INITIALIZATION;
    const mc_registry_error *first = classes == NULL ? NULL : &classes->block_failure;
    const bool block_failure =
        first != NULL && classes->blocks_phase == MC_REGISTRY_FAILED &&
        classes->block_states[MC_BLOCK_BASE].phase == MC_REGISTRY_FAILED &&
        same_registry_cause(first, &classes->block_states[MC_BLOCK_BASE].failure, false) &&
        same_registry_cause(error, first, cached);
    const char *exception = NULL;
    if (block_failure) {
        if (registry_source_exception(first->status) != NULL)
            exception =
                cached ? "java/lang/NoClassDefFoundError" : "java/lang/ExceptionInInitializerError";
    } else if (!cached && error->active_initializer_class == NULL) {
        exception = registry_source_exception(error->status);
    }
    /* Unknown declaring-class failures and native limits/frontiers remain
     * native. Keep the actual Registry record and immutable first cause. */
    if (exception != NULL) {
        c->error.native_policy = false;
        c->error.exception_class = exception;
    }
}

static bool registry_fail(mc_world_random_display_call *c) {
    c->error.child = MC_WORLD_RANDOM_DISPLAY_REGISTRY;
    c->error.registry = &c->registry;
    (void)fail(c, mc_world_runtime_registry_status(c->registry.status), c->registry.message, true,
               NULL);
    registry_exception(c, &c->registry);
    return false;
}

static bool capture(mc_world_random_display_call *c, const mc_world_access_failure *failure) {
    if (failure->receipt == NULL)
        return true;
    if (c->error.world_access.receipt == failure->receipt)
        return true;
    for (size_t i = 0; i < c->capture_count; ++i)
        if (c->additional_captures[i].receipt == failure->receipt)
            return true;
    mc_world_access_failure *destination = &c->error.world_access;
    if (destination->receipt != NULL) {
        if (c->capture_count >= SIZE_MAX / sizeof(*c->additional_captures))
            return fail(c, MC_WORLD_STORAGE_LIMIT, "World display failure capture count", true,
                        NULL);
        mc_world_access_failure *grown =
            realloc(c->additional_captures, (c->capture_count + 1u) * sizeof(*grown));
        if (grown == NULL)
            return fail(c, MC_WORLD_STORAGE_ALLOCATION_FAILED,
                        "World display failure capture allocation", true, NULL);
        c->additional_captures = grown;
        destination = &grown[c->capture_count];
        *destination = (mc_world_access_failure){0};
    }
    const mc_world_storage_status held = mc_world_access_failure_retain(failure, destination);
    if (held != MC_WORLD_STORAGE_OK)
        return fail(c, held, "World display failure receipt retain", true, NULL);
    if (destination != &c->error.world_access)
        ++c->capture_count;
    const char *exception = NULL;
    if (mc_world_access_failure_source_exception(failure, &exception)) {
        c->error.native_policy = false;
        c->error.exception_class = exception;
    }
    return true;
}

static void source_chunk(mc_world_random_display_call *c,
                         const mc_world_source_chunk_error **view) {
    if (*view == NULL)
        return;
    if (!c->source_chunk_copied) {
        c->source_chunk = **view;
        c->source_chunk_copied = true;
    }
    *view = &c->source_chunk;
    if (c->source_chunk.source.source_exception) {
        c->error.native_policy = false;
        c->error.exception_class = c->source_chunk.source.exception_class;
    }
}

static bool tile_captures(mc_world_random_display_call *c, const mc_live_tile_entity_error *tile) {
    return capture(c, &tile->world_access_failure) &&
           capture(c, &tile->publication.world_access_failure) &&
           capture(c, &tile->batch.publication.world_access_failure);
}

static bool acquisition_fail(mc_world_random_display_call *c, mc_world_storage_error cause) {
    c->error.child = MC_WORLD_RANDOM_DISPLAY_ACQUISITION;
    c->error.acquisition = &c->acquisition;
    (void)storage_fail(c, cause);
    registry_exception(c, &c->acquisition.registry);
    source_chunk(c, &c->acquisition.source_chunk);
    if (!capture(c, &c->acquisition.batch.publication.world_access_failure) ||
        !tile_captures(c, &c->acquisition.tile_entity))
        return false;
    return false;
}

static bool nearby_captures(mc_world_random_display_call *c, mc_world_nearby_error *nearby) {
    registry_exception(c, &nearby->query.registry);
    source_chunk(c, &nearby->query.source_chunk);
    return capture(c, &nearby->batch.publication.world_access_failure) &&
           tile_captures(c, &nearby->tile_entity);
}

static bool mutation_captures(mc_world_random_display_call *c, mc_world_mutation_error *mutation) {
    registry_exception(c, &mutation->registry);
    source_chunk(c, &mutation->query.source_chunk);
    return capture(c, &mutation->world_access_failure) &&
           capture(c, &mutation->batch.publication.world_access_failure) &&
           tile_captures(c, &mutation->tile_entity);
}

static bool block_fail(mc_world_random_display_call *c) {
    c->error.child = MC_WORLD_RANDOM_DISPLAY_BLOCK;
    c->error.block = &c->block;
    c->error.child_class = c->block.site.class_name;
    c->error.child_method = c->block.site.method;
    c->error.child_descriptor = c->block.site.descriptor;
    c->error.child_bytecode = c->block.site.pc;
    (void)fail(c, c->block.status, c->block.operation, c->block.native_policy,
               c->block.exception_class);
    c->error.cause = c->block.cause;
    registry_exception(c, &c->block.registry);
    switch (c->block.child) {
        case MC_WORLD_RANDOM_DISPLAY_BLOCK_QUERY:
            (void)nearby_captures(c, &c->block.detail.query.nearby);
            break;
        case MC_WORLD_RANDOM_DISPLAY_BLOCK_MATERIAL:
            (void)nearby_captures(c, &c->block.detail.material.access.nearby);
            break;
        case MC_WORLD_RANDOM_DISPLAY_BLOCK_SUPPORT:
            (void)nearby_captures(c, &c->block.detail.support.access);
            break;
        case MC_WORLD_RANDOM_DISPLAY_BLOCK_PRESENTATION:
            (void)mutation_captures(c, &c->block.detail.presentation);
            break;
        case MC_WORLD_RANDOM_DISPLAY_BLOCK_STEP:
            if (nearby_captures(c, &c->block.detail.step.block_read.nearby))
                (void)mutation_captures(c, &c->block.detail.step.mutation);
            break;
        case MC_WORLD_RANDOM_DISPLAY_BLOCK_NONE:
            break;
    }
    return false;
}

static bool draw(mc_world_random_display_call *c, unsigned int pc, int32_t index) {
    at(c, pc);
    c->state.draw_index = index;
    mc_java_random *random = c->state.world->fields.r;
    if (random == NULL)
        return source_null(c, "World random display null World Random receiver");
    if (!mc_java_random_next_bounded(random, 16, &c->state.draws[index]))
        return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World display Random bound", false,
                    "java/lang/IllegalArgumentException");
    ++c->state.world_random_calls;
    return true;
}

static bool coordinate(mc_world_random_display_call *c, int32_t argument, int32_t index,
                       unsigned int first_pc, unsigned int second_pc, unsigned int store_pc,
                       int32_t *out) {
    if (!draw(c, first_pc, index))
        return false;
    const int32_t sum = mc_java_i32_add(argument, c->state.draws[index]);
    if (!draw(c, second_pc, index + 1))
        return false;
    *out = mc_java_i32_sub(sum, c->state.draws[index + 1]);
    at(c, store_pc);
    c->state.coordinates_stored[index / 2] = true;
    return true;
}

static bool body(mc_world_random_display_call *c) {
    at(c, 4);
    c->state.local_random_allocated = true;
    c->state.source_random = &c->random;
    at(c, 8);
    c->error.child = MC_WORLD_RANDOM_DISPLAY_DEFAULT_RANDOM;
    c->error.profile =
        mc_java_random_construct_default(c->state.world->classes->profile, &c->random);
    if (c->error.profile != MC_JAVA_RANDOM_PROFILE_OK)
        return fail(c,
                    c->error.profile == MC_JAVA_RANDOM_PROFILE_LIMIT ? MC_WORLD_STORAGE_LIMIT
                    : c->error.profile == MC_JAVA_RANDOM_PROFILE_ALLOCATION_FAILED
                        ? MC_WORLD_STORAGE_ALLOCATION_FAILED
                        : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                    "World display local default Random constructor", true, NULL);
    c->state.local_random_constructed = true;
    c->state.local_random_initial = c->random;
    at(c, 14);
    for (c->state.sample = 0; c->state.sample < 1000; ++c->state.sample) {
        ++c->state.samples_started;
        memset(c->state.coordinates_stored, 0, sizeof(c->state.coordinates_stored));
        c->state.block_read = c->state.block_selected = false;
        c->state.selected_block = NULL;
        if (!coordinate(c, c->state.argument_x, 0, 31, 41, 45, &c->state.x) ||
            !coordinate(c, c->state.argument_y, 2, 54, 64, 68, &c->state.y) ||
            !coordinate(c, c->state.argument_z, 4, 77, 87, 91, &c->state.z))
            return false;
        at(c, 100);
        mc_world_storage_error cause = {0};
        if (mc_world_runtime_block_get_detailed(c->state.world, c->state.x, c->state.y, c->state.z,
                                                &c->state.block_id, &cause,
                                                &c->acquisition) != MC_WORLD_STORAGE_OK)
            return acquisition_fail(c, cause);
        c->state.block_read = true;
        if (c->state.block_id > 0) {
            mc_registry_block_m_view array;
            at(c, 110);
            if (mc_registry_block_m_get(c->state.world->classes->registry, &array, &c->registry) !=
                MC_REGISTRY_OK)
                return registry_fail(c);
            at(c, 115);
            if (mc_registry_block_m_at(&array, c->state.block_id, &c->state.selected_block,
                                       &c->registry) != MC_REGISTRY_OK)
                return registry_fail(c);
            c->state.block_selected = true;
            at(c, 125);
            ++c->state.display_calls_started;
            if (c->state.selected_block == NULL)
                return source_null(c, "World random display null selected Block receiver");
            if (mc_world_runtime_block_random_display(c->state.world, c->state.selected_block,
                                                      c->state.x, c->state.y, c->state.z,
                                                      &c->random, &c->block) != MC_WORLD_STORAGE_OK)
                return block_fail(c);
            ++c->state.display_calls_returned;
        }
        at(c, 128);
        ++c->state.samples_returned;
    }
    at(c, 134);
    c->state.returned = true;
    memset(&c->error, 0, sizeof(c->error));
    return true;
}

mc_world_storage_status
mc_world_runtime_random_display_begin(mc_world_runtime *world, int32_t x, int32_t y, int32_t z,
                                      mc_world_random_display_call **out,
                                      mc_world_random_display_error *admission) {
    if (world == NULL || out == NULL || *out != NULL)
        return rejected(admission, MC_WORLD_STORAGE_INVALID_ARGUMENT, "World display arguments");
    if (world->construction.mode != MC_WORLD_RUNTIME_GAME ||
        world->construction.phase == MC_WORLD_RUNTIME_RETIRED || world->classes == NULL ||
        world->classes->side != MC_WORLD_INFO_CLIENT)
        return rejected(admission, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                        "World display actual client GAME owner");
    if (world->persistent_bindings == SIZE_MAX)
        return rejected(admission, MC_WORLD_STORAGE_LIMIT, "World display binding ceiling");
    mc_world_random_display_call *c = calloc(1, sizeof(*c));
    if (c == NULL)
        return rejected(admission, MC_WORLD_STORAGE_ALLOCATION_FAILED,
                        "World display call allocation");
    mc_world_storage_error cause = {0};
    if (mc_world_runtime_call_enter(world, &cause) != MC_WORLD_STORAGE_OK) {
        free(c);
        return rejected(admission, cause.status, cause.operation);
    }
    if (!mc_world_runtime_binding_retain(world)) {
        const mc_world_storage_error error = *mc_world_runtime_last_error(world);
        mc_world_runtime_call_leave(world);
        free(c);
        return rejected(admission, error.status, error.operation);
    }
    c->world_held = true;
    c->state.world = world;
    c->state.argument_x = x;
    c->state.argument_y = y;
    c->state.argument_z = z;
    c->state.error = &c->error;
    c->state.active = true;
    *out = c;
    (void)rejected(admission, MC_WORLD_STORAGE_OK, NULL);
    if (!body(c) && c->error.status == MC_WORLD_STORAGE_OK)
        (void)fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                   "World display child failed without a cause", true, NULL);
    c->state.active = false;
    world->last_error = c->error.cause;
    mc_world_runtime_call_leave(world);
    return c->error.status;
}

bool mc_world_runtime_random_display_inspect(const mc_world_random_display_call *c,
                                             mc_world_random_display_state *out) {
    if (c == NULL || out == NULL)
        return false;
    *out = c->state;
    out->local_random = c->random;
    return true;
}

mc_world_storage_status
mc_world_runtime_random_display_retire(mc_world_random_display_call **slot,
                                       mc_world_random_display_error *cleanup) {
    if (slot == NULL)
        return rejected(cleanup, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                        "World display retirement slot");
    mc_world_random_display_call *c = *slot;
    if (c == NULL)
        return rejected(cleanup, MC_WORLD_STORAGE_OK, NULL);
    if (c->state.active)
        return rejected(cleanup, MC_WORLD_STORAGE_BUSY, "World display call remains active");
    mc_world_access_failure_release(&c->error.world_access);
    for (size_t i = 0; i < c->capture_count; ++i)
        mc_world_access_failure_release(&c->additional_captures[i]);
    free(c->additional_captures);
    if (c->world_held)
        mc_world_runtime_binding_release(c->state.world);
    free(c);
    *slot = NULL;
    return rejected(cleanup, MC_WORLD_STORAGE_OK, NULL);
}
