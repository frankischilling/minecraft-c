#include "detector_internal.h"
#include "wrapper_transport_internal.h"

#include "../../../../block/registry/registry_internal.h"
#include "../../collision/blocks_internal.h"
#include "../mutation_internal.h"
#include "mc/core/java/java.h"
#include "mc/world/runtime/collision/class_entities.h"

#include <float.h>

#if !defined(_M_X64) && !defined(_M_AMD64) && !defined(__x86_64__)
#error "Detector coordinates require x86-64/SSE2"
#endif
#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) ||       \
    (defined(_M_FP_FAST) && _M_FP_FAST)
#error "Detector coordinates require strict floating transformations"
#endif
#include <emmintrin.h>
_Static_assert(FLT_RADIX == 2 && FLT_MANT_DIG == 24 && DBL_MANT_DIG == 53 && sizeof(float) == 4 &&
                   sizeof(double) == 8 && FLT_EVAL_METHOD == 0,
               "Detector coordinates require source-width binary32/binary64");

typedef enum detector_method { DETECTOR_CONTACT, DETECTOR_TICK, DETECTOR_UPDATE } detector_method;

typedef struct detector_frame {
    mc_world_runtime *world;
    const mc_block *block;
    int32_t x, y, z;
    detector_method method;
    mc_world_detector_error *error;
} detector_frame;

mc_world_detector_error mc_world_mutation_detector_empty_error(void) {
    const mc_world_mutation_error mutation = mc_world_mutation_empty_error();
    return (mc_world_detector_error){
        .mutation = mutation,
        .metadata = mc_world_collision_empty_error(),
        .scheduled = {.registry = mutation.registry, .tick = mutation}};
}

static mc_world_detector_site *detector_site(const detector_frame *frame) {
    switch (frame->method) {
        case DETECTOR_CONTACT:
            return &frame->error->contact;
        case DETECTOR_TICK:
            return &frame->error->tick;
        default:
            return &frame->error->update;
    }
}

static uint32_t detector_bit(const detector_frame *frame) {
    return frame->method == DETECTOR_CONTACT ? MC_WORLD_RAIL_SITE_DETECTOR_CONTACT
           : frame->method == DETECTOR_TICK  ? MC_WORLD_RAIL_SITE_DETECTOR_TICK
                                             : MC_WORLD_RAIL_SITE_DETECTOR_UPDATE;
}

static void detector_at(const detector_frame *frame, unsigned int pc) {
    const bool client = frame->block->classes->options.side == MC_REGISTRY_CLIENT;
    const char *method;
    if (frame->method == DETECTOR_CONTACT)
        method = client ? "a(Lfd;IIILsn;)V" : "a(Ldj;IIILlq;)V";
    else if (frame->method == DETECTOR_TICK)
        method = client ? "a(Lfd;IIILjava/util/Random;)V" : "a(Ldj;IIILjava/util/Random;)V";
    else
        method = client ? "f(Lfd;IIII)V" : "f(Ldj;IIII)V";
    const mc_world_detector_site site = {
        true, client ? "ph" : "jn", method, pc, frame->x, frame->y, frame->z};
    *detector_site(frame) = site;
    frame->error->selected |= detector_bit(frame);
    frame->error->mutation.hook_class = site.declaring_class;
    frame->error->mutation.hook_method = site.method;
    frame->error->mutation.hook_bytecode = pc;
}

static void detector_cause(const detector_frame *frame) {
    if ((frame->error->selected & MC_WORLD_RAIL_SITE_DETECTOR_CAUSE) == 0) {
        frame->error->cause = *detector_site(frame);
        frame->error->selected |= MC_WORLD_RAIL_SITE_DETECTOR_CAUSE;
        frame->error->origin = detector_bit(frame);
    }
}

static bool detector_fail(const detector_frame *frame, unsigned int pc,
                          mc_world_storage_status status, const char *operation) {
    detector_at(frame, pc);
    detector_cause(frame);
    return mc_world_mutation_fail(frame->world, &frame->error->mutation, status, operation);
}

static bool detector_storage(const detector_frame *frame, unsigned int pc,
                             mc_world_storage_status status, mc_world_storage_error child) {
    detector_at(frame, pc);
    detector_cause(frame);
    frame->error->mutation.status = status;
    frame->error->mutation.operation = child.operation;
    frame->error->mutation.cause = child;
    if (frame->world != NULL)
        frame->world->last_error = child;
    return false;
}

/* Lower calls receive fresh storage. A failed child replaces only the lower
 * mutation projection; the private enclosing detector sites remain retained.
 * Copy before assignment so an aliased caller cannot erase its own child. */
static bool detector_nested(const detector_frame *frame, unsigned int pc,
                            const mc_world_mutation_error *lower) {
    const mc_world_mutation_error captured = *lower;
    frame->error->mutation = captured;
    if (captured.cause_hook_class == NULL) {
        frame->error->mutation.cause_hook_class = captured.hook_class;
        frame->error->mutation.cause_hook_method = captured.hook_method;
        frame->error->mutation.cause_hook_bytecode = captured.hook_bytecode;
    }
    detector_at(frame, pc);
    detector_cause(frame);
    if (frame->world != NULL)
        frame->world->last_error = captured.cause;
    return false;
}

static bool detector_update_failed(const detector_frame *caller) {
    mc_world_mutation_error *const lower = &caller->error->mutation;
    if (lower->cause_hook_class == NULL) {
        lower->cause_hook_class = lower->hook_class;
        lower->cause_hook_method = lower->hook_method;
        lower->cause_hook_bytecode = lower->hook_bytecode;
    }
    detector_at(caller, 35);
    return false;
}

static mc_world_storage_status detector_entity_status(mc_entity_status status) {
    switch (status) {
        case MC_ENTITY_OK:
            return MC_WORLD_STORAGE_OK;
        case MC_ENTITY_LIMIT:
            return MC_WORLD_STORAGE_LIMIT;
        case MC_ENTITY_ALLOCATION_FAILED:
            return MC_WORLD_STORAGE_ALLOCATION_FAILED;
        case MC_ENTITY_BUSY:
            return MC_WORLD_STORAGE_BUSY;
        case MC_ENTITY_INVALID_ARGUMENT:
            return MC_WORLD_STORAGE_INVALID_ARGUMENT;
        default:
            return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    }
}

static void detector_nearby(mc_world_mutation_error *projection,
                            const mc_world_nearby_error *child) {
    projection->nearby = *child;
    projection->query = child->query;
    projection->registry = child->query.registry;
    projection->population = child->query.population;
    projection->entity = child->entity;
    projection->tile_entity = child->tile_entity;
    projection->batch = child->batch;
    projection->load_entity = child->load_entity;
    projection->activation_bytecode = child->activation_bytecode;
    projection->activation_world_bytecode = child->activation_world_bytecode;
    projection->load_bytecode = child->load_bytecode;
    projection->load_builder_bytecode = child->load_builder_bytecode;
}

static bool detector_metadata(const detector_frame *frame, int32_t *out) {
    mc_world_collision_error child = mc_world_collision_empty_error();
    detector_at(frame, 13);
    const mc_world_storage_status status = mc_world_collision_block_value_held(
        frame->world, frame->x, frame->y, frame->z, true, out, &child);
    if (status == MC_WORLD_STORAGE_OK)
        return true;
    frame->error->metadata = child;
    frame->error->mutation.rail_access = child;
    detector_nearby(&frame->error->mutation, &child.nearby);
    return detector_storage(frame, 13, status, child.nearby.query.cause);
}

static bool detector_class(const detector_frame *frame, const mc_entity_class_literal **out) {
    detector_at(frame, 23);
    mc_entity_classes *const classes = frame->block->classes->entities;
    if (classes == NULL)
        return detector_fail(frame, 23, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                             "Detector Minecart Class defining owner binding");
    const mc_entity_status status =
        mc_entity_class_literal_get(classes, MC_ENTITY_CLASS_KIND_MINECART, out);
    if (status == MC_ENTITY_OK)
        return true;
    frame->error->mutation.entity =
        (mc_entity_error){.status = status, .operation = "Detector Minecart Class literal"};
    return detector_fail(frame, 23, detector_entity_status(status),
                         "Detector Minecart Class literal");
}

static bool detector_box(const detector_frame *frame, mc_entity_classes **pool_owner,
                         mc_entity_bounds **out) {
    /* Class23 has already completed. The first reached conversion is i2f27. */
    detector_at(frame, 27);
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
        return detector_fail(frame, 27, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                             "Detector coordinate floating environment");
    const double min_x = (double)((float)frame->x + 0.125f);
    const double min_y = (double)frame->y;
    const double min_z = (double)((float)frame->z + 0.125f);
    const double max_x = (double)((float)mc_java_i32_add(frame->x, 1) - 0.125f);
    const double max_y = (double)frame->y + 0.25;
    const double max_z = (double)((float)mc_java_i32_add(frame->z, 1) - 0.125f);
    detector_at(frame, 64);
    /* Actual eq/cz belongs to the defining Block's loaded Entity classes, not
     * to whichever Registry happens to own the supplied World. */
    mc_entity_classes *const owner = frame->block->classes->entities;
    if (owner == NULL)
        return detector_fail(frame, 64, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                             "Detector pooled bounds defining owner binding");
    mc_entity_bounds_error child = {0};
    mc_entity_bounds *bounds = NULL;
    const mc_entity_status status =
        mc_entity_bounds_pool_get(owner, min_x, min_y, min_z, max_x, max_y, max_z, &bounds, &child);
    if (status != MC_ENTITY_OK) {
        frame->error->mutation.bounds = child;
        return detector_fail(frame, 64, detector_entity_status(status),
                             child.operation == NULL ? "Detector pooled bounds factory"
                                                     : child.operation);
    }
    *pool_owner = owner;
    *out = bounds;
    return true;
}

static bool detector_query(const detector_frame *frame, const mc_entity_class_literal *target,
                           const mc_entity_bounds *bounds, mc_world_class_entity_list **out) {
    mc_world_nearby_error child = mc_world_nearby_empty_error();
    detector_at(frame, 67);
    const mc_world_storage_status status =
        mc_world_runtime_class_entities(frame->world, target, bounds, out, &child);
    if (status == MC_WORLD_STORAGE_OK)
        return true;
    detector_nearby(&frame->error->mutation, &child);
    return detector_storage(frame, 67, status, child.query.cause);
}

static bool detector_change(const detector_frame *frame, bool pressed, int32_t saved_metadata) {
    const unsigned int metadata_pc = pressed ? 105u : 166u;
    const unsigned int id_pc = pressed ? 114u : 175u;
    const unsigned int neighbors_pc = pressed ? 117u : 178u;
    const unsigned int below_id_pc = pressed ? 128u : 189u;
    const unsigned int below_pc = pressed ? 131u : 192u;
    const unsigned int range_pc = pressed ? 143u : 204u;
    const int32_t value = pressed ? saved_metadata | INT32_C(8) : saved_metadata & INT32_C(7);
    mc_world_mutation_error child = mc_world_mutation_empty_error();
    detector_at(frame, metadata_pc);
    if (mc_world_runtime_metadata_set(frame->world, frame->x, frame->y, frame->z, value, &child) !=
        MC_WORLD_STORAGE_OK)
        return detector_nested(frame, metadata_pc, &child);

    detector_at(frame, id_pc);
    const int32_t current_id = frame->block->fields.bn;
    child = mc_world_mutation_empty_error();
    detector_at(frame, neighbors_pc);
    if (!mc_world_mutation_neighbors(frame->world, frame->x, frame->y, frame->z, current_id,
                                     &child))
        return detector_nested(frame, neighbors_pc, &child);

    const int32_t below = mc_java_i32_sub(frame->y, 1);
    detector_at(frame, below_id_pc);
    const int32_t below_id = frame->block->fields.bn;
    child = mc_world_mutation_empty_error();
    detector_at(frame, below_pc);
    if (!mc_world_mutation_neighbors(frame->world, frame->x, below, frame->z, below_id, &child))
        return detector_nested(frame, below_pc, &child);

    child = mc_world_mutation_empty_error();
    detector_at(frame, range_pc);
    if (!mc_world_mutation_observe_range(frame->world, frame->x, frame->y, frame->z, frame->x,
                                         frame->y, frame->z, &child))
        return detector_nested(frame, range_pc, &child);
    return true;
}

static bool detector_schedule(const detector_frame *frame) {
    detector_at(frame, 218);
    const int32_t id = frame->block->fields.bn;
    detector_at(frame, 222);
    /* Complete PH_JN e()/c(): bipush20, ireturn2. No constructor or World read. */
    const int32_t delay = 20;
    mc_world_schedule_error child = {0};
    detector_at(frame, 225);
    if (mc_world_runtime_schedule(frame->world, frame->x, frame->y, frame->z, id, delay, &child) ==
        MC_WORLD_STORAGE_OK)
        return true;
    frame->error->scheduled = child;
    mc_world_mutation_error lower = child.tick;
    lower.schedule = (mc_world_schedule_prefix){.status = child.status,
                                                .operation = child.operation,
                                                .bytecode = child.bytecode,
                                                .presence_bytecode = child.presence_bytecode,
                                                .provider_bytecode = child.provider_bytecode,
                                                .cause = child.cause,
                                                .record = child.record,
                                                .membership = child.membership,
                                                .ordered = child.ordered,
                                                .registry = child.registry};
    lower.status = child.status;
    lower.operation = child.operation;
    lower.cause = child.cause;
    lower.registry = child.registry;
    lower.schedule_bytecode = child.bytecode;
    lower.record = child.record;
    lower.membership = child.membership;
    lower.ordered = child.ordered;
    if (child.presence_bytecode != 0)
        lower.presence_bytecode = child.presence_bytecode;
    if (child.provider_bytecode != 0)
        lower.provider_bytecode = child.provider_bytecode;
    return detector_nested(frame, 225, &lower);
}

static bool detector_update(const detector_frame *frame, int32_t saved_metadata) {
    detector_at(frame, 0);
    const bool previously_on = (saved_metadata & INT32_C(8)) != 0;
    const mc_entity_class_literal *target = NULL;
    mc_entity_classes *pool_owner = NULL;
    mc_entity_bounds *bounds = NULL;
    mc_world_class_entity_list *list = NULL;
    bool complete = false;
    if (!detector_class(frame, &target) || !detector_box(frame, &pool_owner, &bounds) ||
        !detector_query(frame, target, bounds, &list))
        goto done;
    detector_at(frame, 74);
    size_t count;
    const mc_world_storage_status status = mc_world_class_entity_list_count(list, &count);
    if (status != MC_WORLD_STORAGE_OK) {
        detector_fail(frame, 74, status, "Detector captured Class query List size");
        goto done;
    }
    const bool found = count > 0;
    if (found && !previously_on && !detector_change(frame, true, saved_metadata))
        goto done;
    if (!found && previously_on && !detector_change(frame, false, saved_metadata))
        goto done;
    if (found && !detector_schedule(frame))
        goto done;
    complete = true;

done:
    mc_world_class_entity_list_release(list);
    if (bounds != NULL)
        (void)mc_entity_bounds_release(pool_owner, bounds);
    return complete;
}

static bool detector_guarded(const detector_frame *frame) {
    detector_at(frame, 1);
    if (frame->world == NULL)
        return detector_fail(frame, 1, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                             "Detector World B receiver is null");
    if (frame->world->fields.B)
        return true;
    int32_t metadata;
    if (!detector_metadata(frame, &metadata))
        return false;
    const bool on = (metadata & INT32_C(8)) != 0;
    if (frame->method == DETECTOR_CONTACT ? on : !on)
        return true;
    detector_at(frame, 35);
    detector_frame helper = *frame;
    helper.method = DETECTOR_UPDATE;
    if (!detector_update(&helper, metadata))
        return detector_update_failed(frame);
    return true;
}

static bool detector_admit(mc_world_runtime *world, const mc_block *block,
                           mc_world_detector_error *frame) {
    if (block != NULL && block->classes != NULL)
        return true;
    /* Native call admission is separate from entered source method PC0. */
    return mc_world_mutation_fail(world, &frame->mutation, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                  "Detector invocation receiver/defining owner");
}

static bool detector_result(bool completed, const mc_world_detector_error *frame,
                            mc_world_detector_error *error) {
    *error = completed ? mc_world_mutation_detector_empty_error() : *frame;
    if (!completed)
        mc_world_detector_error_publish(error);
    return completed;
}

bool mc_world_mutation_detector_update(mc_world_runtime *world, const mc_block *block, int32_t x,
                                       int32_t y, int32_t z, int32_t metadata,
                                       mc_world_detector_error *error) {
    if (error == NULL)
        return false;
    mc_world_detector_error lower = mc_world_mutation_detector_empty_error();
    const detector_frame frame = {world, block, x, y, z, DETECTOR_UPDATE, &lower};
    const bool completed =
        detector_admit(world, block, &lower) && detector_update(&frame, metadata);
    return detector_result(completed, &lower, error);
}

bool mc_world_mutation_detector_contact(mc_world_runtime *world, const mc_block *block, int32_t x,
                                        int32_t y, int32_t z, mc_entity *entity,
                                        mc_world_detector_error *error) {
    (void)entity; /* Actual contact never reads this nullable source argument. */
    if (error == NULL)
        return false;
    mc_world_detector_error lower = mc_world_mutation_detector_empty_error();
    const detector_frame frame = {world, block, x, y, z, DETECTOR_CONTACT, &lower};
    const bool completed = detector_admit(world, block, &lower) && detector_guarded(&frame);
    return detector_result(completed, &lower, error);
}

bool mc_world_mutation_detector_tick(mc_world_runtime *world, const mc_block *block, int32_t x,
                                     int32_t y, int32_t z, mc_java_random *random,
                                     mc_world_detector_error *error) {
    (void)random; /* Actual tick never reads this nullable source argument. */
    if (error == NULL)
        return false;
    mc_world_detector_error lower = mc_world_mutation_detector_empty_error();
    const detector_frame frame = {world, block, x, y, z, DETECTOR_TICK, &lower};
    const bool completed = detector_admit(world, block, &lower) && detector_guarded(&frame);
    return detector_result(completed, &lower, error);
}
