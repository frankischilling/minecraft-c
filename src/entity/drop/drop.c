#include "internal.h"

#include "../../block/registry/registry_internal.h"
#include "../entity_internal.h"
#include "../player/tick_math_internal.h"
#include "mc/block/registry/runtime.h"

#include <stdlib.h>
#include <string.h>

static const char *const numeric_descriptor = "(IIF)Lhl;";
static const char *const stack_descriptor = "(Liz;F)Lhl;";

static void at(mc_entity_drop_call *call, bool numeric, unsigned int pc) {
    call->error.site =
        (mc_entity_drop_site){"sn", "a", numeric ? numeric_descriptor : stack_descriptor, pc};
    if (numeric)
        call->state.numeric_pc = call->error.numeric_pc = pc;
    else
        call->state.stack_pc = call->error.stack_pc = pc;
}

static bool fail(mc_entity_drop_call *call, mc_entity_status status, const char *operation,
                 bool native_policy) {
    call->error.status = status;
    call->error.operation = operation;
    call->error.native_policy = native_policy;
    return false;
}

static bool null_reference(mc_entity_drop_call *call, const char *operation) {
    call->error.source_exception = true;
    call->error.exception_class = "java/lang/NullPointerException";
    return fail(call, MC_ENTITY_STATE_ERROR, operation, false);
}

static mc_entity_status storage_status(mc_world_storage_status status) {
    return status == MC_WORLD_STORAGE_LIMIT               ? MC_ENTITY_LIMIT
           : status == MC_WORLD_STORAGE_ALLOCATION_FAILED ? MC_ENTITY_ALLOCATION_FAILED
           : status == MC_WORLD_STORAGE_BUSY              ? MC_ENTITY_BUSY
                                                          : MC_ENTITY_WORLD_FAILED;
}

static mc_entity_status stack_status(mc_item_stack_status status) {
    return status == MC_ITEM_STACK_ALLOCATION_FAILED ? MC_ENTITY_ALLOCATION_FAILED
           : status == MC_ITEM_STACK_LIMIT           ? MC_ENTITY_LIMIT
                                                     : MC_ENTITY_ITEM_FAILED;
}

static mc_entity_status admission(mc_entity_drop_error *error, mc_entity_status status,
                                  const char *operation) {
    if (error != NULL)
        *error = (mc_entity_drop_error){
            .status = status, .operation = operation, .native_policy = status != MC_ENTITY_OK};
    return status;
}

/* Retain only this invocation's immutable producer receipt. If native receipt
 * admission refuses, the original source error and owner slots remain intact;
 * checked retirement retries this capture before releasing their graph. */
static bool publication_capture(mc_entity_drop_call *call) {
    if (call->publication.world_access_failure.receipt == NULL || call->receipt.receipt != NULL)
        return true;
    const mc_world_storage_status status =
        mc_world_access_failure_retain(&call->publication.world_access_failure, &call->receipt);
    if (status == MC_WORLD_STORAGE_OK)
        return true;
    call->cleanup.status = storage_status(status);
    call->cleanup.operation = "Entity drop exact publication receipt capture";
    call->cleanup.native_policy = true;
    return false;
}

static bool publish(mc_entity_drop_call *call) {
    at(call, false, 35);
    mc_world_runtime *world = call->state.actor->base.world;
    call->state.publication_world = world;
    if (world != NULL) {
        if (!mc_world_runtime_binding_retain(world))
            return fail(call, MC_ENTITY_LIMIT, "Entity drop publication World occurrence", true);
        call->publication_world_held = true;
    }
    at(call, false, 39);
    if (world == NULL)
        return null_reference(call, "Entity drop actual publication World null receiver");
    call->state.publication_entered = true;
    call->error.publication = &call->publication;
    const mc_world_storage_status status = mc_world_runtime_insert_entity(
        world, call->state.item, &call->state.inserted, &call->publication);
    if (status != MC_WORLD_STORAGE_OK) {
        fail(call, storage_status(status), call->publication.operation, true);
        call->error.child = (mc_entity_drop_site){"fd", "b", "(Lsn;)Z", call->publication.bytecode};
        const char *exception_class = NULL;
        if (status != MC_WORLD_STORAGE_LIMIT && status != MC_WORLD_STORAGE_ALLOCATION_FAILED &&
            status != MC_WORLD_STORAGE_BUSY &&
            mc_world_access_failure_source_exception(&call->publication.world_access_failure,
                                                     &exception_class)) {
            call->error.source_exception = true;
            call->error.exception_class = exception_class;
            call->error.native_policy = false;
        }
    } else
        call->state.publication_returned = true;
    if (!publication_capture(call)) {
        if (status == MC_WORLD_STORAGE_OK)
            fail(call, call->cleanup.status, call->cleanup.operation, true);
        return false;
    }
    /* The source pop42 discards an ordinary false World.b result. */
    return status == MC_WORLD_STORAGE_OK;
}

static bool execute_stack(mc_entity_drop_call *call) {
    at(call, false, 0);
    const mc_entity_status allocated =
        mc_item_entity_allocate(call->state.actor->classes, &call->state.item, &call->error.entity);
    if (allocated != MC_ENTITY_OK)
        return fail(call, allocated, "Entity drop actual RAW Item allocation", true);
    call->state.item_allocated = true;
    at(call, false, 5);
    mc_world_runtime *world = call->state.actor->base.world;
    call->state.constructor_world = world;
    if (world != NULL) {
        if (!mc_world_runtime_binding_retain(world))
            return fail(call, MC_ENTITY_LIMIT, "Entity drop constructor World occurrence", true);
        call->constructor_world_held = true;
    }
    call->state.x = call->state.actor->base.x;
    const double y = call->state.actor->base.y;
    at(call, false, 17);
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
        return fail(call, MC_ENTITY_UNSUPPORTED_FP, "Entity drop strict Y/offset arithmetic", true);
    const double offset = at_widen(call->state.offset);
    at(call, false, 18);
    call->state.y = at_add(y, offset);
    call->state.z = call->state.actor->base.z;
    at(call, false, 24);
    call->error.child = (mc_entity_drop_site){"hl", "<init>", "(Lfd;DDDLiz;)V", 0};
    const mc_entity_status constructed =
        mc_item_entity_construct(call->state.item, world, call->state.x, call->state.y,
                                 call->state.z, call->state.stack, &call->error.entity);
    if (constructed != MC_ENTITY_OK)
        return fail(call, constructed, "Entity drop actual Item constructor", true);
    call->state.item_constructed = true;
    call->error.child = (mc_entity_drop_site){0};
    at(call, false, 31);
    const mc_entity_status delay = mc_entity_item_pickup_delay_set(call->state.item, 10);
    if (delay != MC_ENTITY_OK)
        return fail(call, delay, "Entity drop actual Item pickup-delay store", true);
    call->state.pickup_delay_stored = true;
    if (!publish(call))
        return false;
    at(call, false, 44);
    return true;
}

static bool execute(mc_entity_drop_call *call) {
    at(call, call->state.numeric, 0);
    if (call->state.actor == NULL)
        return null_reference(call, "Entity drop null invocation receiver");
    if (call->state.numeric) {
        at(call, true, 1);
        call->error.stack = mc_item_stack_allocate(&call->state.stack);
        if (call->error.stack != MC_ITEM_STACK_OK)
            return fail(call, stack_status(call->error.stack),
                        "Entity drop actual RAW Stack allocation", true);
        call->stack_held = call->state.stack_allocated = true;
        at(call, true, 8);
        call->error.child = (mc_entity_drop_site){"iz", "<init>", "(III)V", 0};
        call->error.stack =
            mc_item_stack_construct(call->state.stack, call->state.id, call->state.count, 0);
        if (call->error.stack != MC_ITEM_STACK_OK)
            return fail(call, stack_status(call->error.stack),
                        "Entity drop actual numeric Stack constructor", true);
        call->state.stack_constructed = true;
        call->error.child = (mc_entity_drop_site){0};
        at(call, true, 12);
    }
    if (!execute_stack(call))
        return false;
    if (call->state.numeric)
        at(call, true, 15);
    return true;
}

static bool receiver(const mc_entity *actor) {
    return actor->classes != NULL && actor->classes->source == MC_ENTITY_CLIENT &&
           (mc_entity_is_kind(actor, MC_ENTITY_CLASS_KIND_FALLING_SAND) ||
            mc_entity_is_kind(actor, MC_ENTITY_CLASS_KIND_SHEEP) ||
            mc_entity_is_kind(actor, MC_ENTITY_CLASS_KIND_PIG) ||
            mc_entity_is_kind(actor, MC_ENTITY_CLASS_KIND_COW) ||
            mc_entity_is_kind(actor, MC_ENTITY_CLASS_KIND_CHICKEN) ||
            mc_entity_is_kind(actor, MC_ENTITY_CLASS_KIND_ZOMBIE) ||
            mc_entity_is_kind(actor, MC_ENTITY_CLASS_KIND_SPIDER) ||
            mc_entity_is_kind(actor, MC_ENTITY_CLASS_KIND_SKELETON) ||
            mc_entity_is_kind(actor, MC_ENTITY_CLASS_KIND_CREEPER) ||
            mc_entity_is_kind(actor, MC_ENTITY_CLASS_KIND_SLIME) ||
            mc_entity_is_kind(actor, MC_ENTITY_CLASS_KIND_SQUID) ||
            mc_entity_is_kind(actor, MC_ENTITY_CLASS_KIND_GHAST));
}

static mc_entity_status begin(mc_registry_classes *registry, mc_entity *actor, mc_item_stack *stack,
                              bool numeric, int32_t id, int32_t count, float offset,
                              mc_entity_drop_call **out, mc_entity_drop_error *error) {
    if (out == NULL || *out != NULL)
        return admission(error, MC_ENTITY_INVALID_ARGUMENT, "Entity drop mandatory empty output");
    if (actor != NULL && !receiver(actor))
        return admission(error, MC_ENTITY_STATE_ERROR,
                         "Entity drop closed canonical client receiver");
    if (actor != NULL && actor->phase != MC_ENTITY_READY)
        return admission(error, MC_ENTITY_INVALID_PHASE, "Entity drop READY invocation receiver");
    mc_entity_drop_call *call = calloc(1, sizeof(*call));
    if (call == NULL)
        return admission(error, MC_ENTITY_ALLOCATION_FAILED, "Entity drop retained invocation");
    call->registry = registry;
    call->state = (mc_entity_drop_state){.actor = actor,
                                         .stack = stack,
                                         .numeric = numeric,
                                         .id = id,
                                         .count = count,
                                         .offset = offset,
                                         .active = true,
                                         .error = &call->error,
                                         .cleanup = &call->cleanup};
    if (actor != NULL) {
        const mc_entity_status held = mc_entity_retain(actor);
        if (held != MC_ENTITY_OK) {
            free(call);
            return admission(error, held, "Entity drop invocation receiver occurrence");
        }
        call->actor_held = true;
    }
    if (stack != NULL) {
        const mc_item_stack_status held = mc_item_stack_retain(stack);
        if (held != MC_ITEM_STACK_OK) {
            mc_entity_release(actor);
            free(call);
            return admission(error, stack_status(held), "Entity drop provided Stack occurrence");
        }
        call->stack_held = true;
    }
    if (actor != NULL && registry != NULL) {
        const mc_registry_status held = mc_registry_classes_retain(
            registry, MC_REGISTRY_CLIENT, actor->classes->profile, &call->error.registry);
        if (held != MC_REGISTRY_OK) {
            if (error != NULL)
                *error = call->error;
            if (call->stack_held)
                mc_item_stack_release(stack);
            mc_entity_release(actor);
            free(call);
            const mc_entity_status status = held == MC_REGISTRY_BUSY    ? MC_ENTITY_BUSY
                                            : held == MC_REGISTRY_LIMIT ? MC_ENTITY_LIMIT
                                                                        : MC_ENTITY_STATE_ERROR;
            if (error != NULL) {
                error->status = status;
                error->operation = "Entity drop code Registry occurrence";
                error->native_policy = true;
            }
            return status;
        }
        call->registry_held = true;
    }
    *out = call;
    call->state.returned = execute(call);
    call->state.active = false;
    if (error != NULL)
        *error = call->error;
    return call->error.status;
}

mc_entity_status mc_entity_drop_numeric_begin(mc_registry_classes *registry, mc_entity *actor,
                                              int32_t id, int32_t count, float offset,
                                              mc_entity_drop_call **out,
                                              mc_entity_drop_error *error) {
    return begin(registry, actor, NULL, true, id, count, offset, out, error);
}

mc_entity_status mc_entity_drop_stack_begin(mc_registry_classes *registry, mc_entity *actor,
                                            mc_item_stack *stack, float offset,
                                            mc_entity_drop_call **out,
                                            mc_entity_drop_error *error) {
    return begin(registry, actor, stack, false, 0, 0, offset, out, error);
}

bool mc_entity_drop_inspect(const mc_entity_drop_call *call, mc_entity_drop_state *out) {
    if (call == NULL || out == NULL)
        return false;
    *out = call->state;
    return true;
}

mc_entity_status mc_entity_drop_retire(mc_entity_drop_call **slot, mc_entity_drop_error *error) {
    if (slot == NULL)
        return admission(error, MC_ENTITY_INVALID_ARGUMENT, "Entity drop retirement owner slot");
    mc_entity_drop_call *call = *slot;
    if (call == NULL)
        return admission(error, MC_ENTITY_OK, NULL);
    if (call->state.active)
        return admission(error, MC_ENTITY_BUSY, "Entity drop active invocation retirement");
    if (!publication_capture(call))
        goto busy;
    if (call->registry_held &&
        mc_registry_classes_release(call->registry, &call->cleanup.registry) != MC_REGISTRY_OK) {
        call->cleanup.status = MC_ENTITY_BUSY;
        call->cleanup.operation = "Entity drop Registry occurrence retirement";
        call->cleanup.native_policy = true;
        goto busy;
    }
    call->registry_held = false;
    mc_world_access_failure_release(&call->receipt);
    mc_entity_release(call->state.item);
    call->state.item = NULL;
    if (call->stack_held)
        mc_item_stack_release(call->state.stack);
    call->stack_held = false;
    call->state.stack = NULL;
    if (call->constructor_world_held)
        mc_world_runtime_binding_release(call->state.constructor_world);
    call->constructor_world_held = false;
    if (call->publication_world_held)
        mc_world_runtime_binding_release(call->state.publication_world);
    call->publication_world_held = false;
    if (call->actor_held)
        mc_entity_release(call->state.actor);
    call->actor_held = false;
    free(call);
    *slot = NULL;
    return admission(error, MC_ENTITY_OK, NULL);
busy:
    if (error != NULL)
        *error = call->cleanup;
    return call->cleanup.status;
}
