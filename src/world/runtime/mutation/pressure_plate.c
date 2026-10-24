#include "mutation_internal.h"

#include "../../../block/registry/registry_internal.h"
#include "../../../block/registry/runtime_internal.h"
#include "../collision/blocks_internal.h"
#include "mc/core/java/java.h"
#include "mc/world/runtime/collision/class_entities.h"
#include "mc/world/runtime/scheduling.h"

#include <float.h>

#if !defined(_M_X64) && !defined(_M_AMD64) && !defined(__x86_64__)
#error "Pressure plate coordinates require x86-64/SSE2"
#endif
#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) ||       \
    (defined(_M_FP_FAST) && _M_FP_FAST)
#error "Pressure plate coordinates require strict floating transformations"
#endif
#include <emmintrin.h>
_Static_assert(FLT_RADIX == 2 && FLT_MANT_DIG == 24 && DBL_MANT_DIG == 53 && sizeof(float) == 4 &&
                   sizeof(double) == 8 && FLT_EVAL_METHOD == 0,
               "Pressure plate coordinates require source-width binary32/binary64");

typedef enum pressure_method { PRESSURE_PRIVATE, PRESSURE_TICK } pressure_method;

typedef struct pressure_frame {
    mc_world_runtime *world;
    const mc_block *block;
    mc_world_mutation_error *error;
    pressure_method method;
} pressure_frame;

typedef struct pressure_box {
    mc_entity_classes *classes;
    mc_entity_bounds *bounds;
} pressure_box;

static void pressure_at(const pressure_frame *frame, unsigned int pc) {
    const bool client = frame->block->classes->options.side == MC_REGISTRY_CLIENT;
    frame->error->hook_class = client ? "bv" : "bc";
    frame->error->hook_bytecode = pc;
    if (frame->method == PRESSURE_PRIVATE) {
        frame->error->hook_method = client ? "h(Lfd;III)V" : "g(Ldj;III)V";
        frame->error->pressure_bytecode = pc;
    } else {
        frame->error->hook_method =
            client ? "a(Lfd;IIILjava/util/Random;)V" : "a(Ldj;IIILjava/util/Random;)V";
        frame->error->pressure_tick_bytecode = pc;
    }
}

static bool pressure_done(mc_world_mutation_error *error) {
    error->hook_class = NULL;
    error->hook_method = NULL;
    error->hook_bytecode = 0;
    error->pressure_contact_bytecode = 0;
    error->pressure_tick_bytecode = 0;
    error->pressure_bytecode = 0;
    error->pressure_neighbor_bytecode = 0;
    error->pressure_removed_bytecode = 0;
    return true;
}

static bool pressure_fail(const pressure_frame *frame, unsigned int pc,
                          mc_world_storage_status status, const char *operation) {
    pressure_at(frame, pc);
    return mc_world_mutation_fail(frame->world, frame->error, status, operation);
}

static bool pressure_storage_fail(const pressure_frame *frame, unsigned int pc,
                                  mc_world_storage_error cause) {
    pressure_at(frame, pc);
    frame->error->status = cause.status;
    frame->error->operation = cause.operation;
    frame->error->cause = cause;
    if (frame->world != NULL)
        frame->world->last_error = cause;
    return false;
}

/* Preserve explicit enclosing pressure calls when a fresh child replaces the
 * record. Current/deepest concrete hooks remain a finite diagnostic projection,
 * not a recursive source stack or a limit on synchronous gameplay work. */
static bool pressure_nested(const pressure_frame *frame, unsigned int pc,
                            const mc_world_mutation_error *lower) {
    const unsigned int contact = frame->error->pressure_contact_bytecode;
    const unsigned int tick = frame->error->pressure_tick_bytecode;
    const unsigned int helper = frame->error->pressure_bytecode;
    const unsigned int neighbor = frame->error->pressure_neighbor_bytecode;
    const unsigned int removed = frame->error->pressure_removed_bytecode;
    const mc_world_mutation_error captured = *lower;
    *frame->error = captured;
    if (captured.cause_hook_class == NULL) {
        frame->error->cause_hook_class = captured.hook_class;
        frame->error->cause_hook_method = captured.hook_method;
        frame->error->cause_hook_bytecode = captured.hook_bytecode;
    }
    if (contact != 0)
        frame->error->pressure_contact_bytecode = contact;
    if (tick != 0)
        frame->error->pressure_tick_bytecode = tick;
    if (helper != 0)
        frame->error->pressure_bytecode = helper;
    if (neighbor != 0)
        frame->error->pressure_neighbor_bytecode = neighbor;
    if (removed != 0)
        frame->error->pressure_removed_bytecode = removed;
    pressure_at(frame, pc);
    if (frame->world != NULL)
        frame->world->last_error = captured.cause;
    return false;
}

static bool pressure_registry_fail(const pressure_frame *frame, unsigned int pc,
                                   mc_registry_status status, const mc_registry_error *cause,
                                   const char *operation) {
    pressure_at(frame, pc);
    frame->error->registry = *cause;
    frame->error->status = mc_world_runtime_registry_result(frame->world, status, cause, operation,
                                                            &frame->error->cause);
    frame->error->operation = frame->error->cause.operation;
    return false;
}

/* getfield precedes the declaring class's getstatic active use each time.
 * Capture the nullable receiver field BEFORE initialization, then compare the
 * actual references; neither ordinal dispatch nor an else-if is equivalent. */
static bool pressure_category_equal(const pressure_frame *frame, unsigned int pc,
                                    unsigned int category, bool *out) {
    mc_registry_classes *const classes = frame->block->classes;
    const mc_pressure_category *const actual = frame->block->subtype.bv.a;
    pressure_at(frame, pc);
    if (classes->entered != 0) {
        mc_registry_error cause = mc_world_mutation_empty_registry();
        cause.status = MC_REGISTRY_BUSY;
        cause.message = "Pressure category Registry entered";
        return pressure_registry_fail(frame, pc, cause.status, &cause, cause.message);
    }
    ++classes->entered;
    const mc_registry_status status = mc_registry_pressure_initialize(classes);
    --classes->entered;
    if (status != MC_REGISTRY_OK)
        return pressure_registry_fail(frame, pc, status, &classes->last_error,
                                      "Pressure category declaring class active use");
    const mc_pressure_category *const expected = category == 0   ? classes->pressure_a
                                                 : category == 1 ? classes->pressure_b
                                                                 : classes->pressure_c;
    *out = actual == expected;
    return true;
}

static bool pressure_metadata(const pressure_frame *frame, unsigned int pc, int32_t x, int32_t y,
                              int32_t z, int32_t *out) {
    mc_world_collision_error lower = mc_world_collision_empty_error();
    pressure_at(frame, pc);
    if (mc_world_collision_block_value_held(frame->world, x, y, z, true, out, &lower) ==
        MC_WORLD_STORAGE_OK)
        return true;
    frame->error->nearby = lower.nearby;
    frame->error->query = lower.nearby.query;
    frame->error->registry = lower.nearby.query.registry;
    frame->error->population = lower.nearby.query.population;
    return pressure_storage_fail(frame, pc, lower.nearby.query.cause);
}

static bool pressure_floating(const pressure_frame *frame, unsigned int pc) {
    pressure_at(frame, pc);
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u ||
           pressure_fail(frame, pc, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                         "Pressure plate coordinate floating environment");
}

static mc_world_storage_status pressure_bounds_status(mc_entity_status status) {
    switch (status) {
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

static bool pressure_pooled(const pressure_frame *frame, unsigned int arithmetic_pc,
                            unsigned int factory_pc, int32_t x, int32_t y, int32_t z,
                            pressure_box *out) {
    if (!pressure_floating(frame, arithmetic_pc))
        return false;
    const double min_x = (double)((float)x + 0.125f);
    const double min_y = (double)y;
    const double min_z = (double)((float)z + 0.125f);
    const double max_x = (double)((float)mc_java_i32_add(x, 1) - 0.125f);
    const double max_y = (double)y + 0.25;
    const double max_z = (double)((float)mc_java_i32_add(z, 1) - 0.125f);
    pressure_at(frame, factory_pc);
    /* The defining Block classes own eq/cz, independently of the supplied
     * World's Registry. Retire the returned hold through this exact pool. */
    mc_entity_classes *const classes = frame->block->classes->entities;
    if (classes == NULL)
        return pressure_fail(frame, factory_pc, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                             "Pressure bounds defining classes binding");
    mc_entity_bounds *bounds = NULL;
    const mc_entity_status status = mc_entity_bounds_pool_get(
        classes, min_x, min_y, min_z, max_x, max_y, max_z, &bounds, &frame->error->bounds);
    if (status != MC_ENTITY_OK)
        return pressure_fail(frame, factory_pc, pressure_bounds_status(status),
                             frame->error->bounds.operation == NULL
                                 ? "Pressure pooled bounds factory"
                                 : frame->error->bounds.operation);
    *out = (pressure_box){classes, bounds};
    return true;
}

static bool pressure_nearby(const pressure_frame *frame, const pressure_box *box,
                            mc_world_entity_list_view *out) {
    pressure_at(frame, 82);
    if (mc_world_runtime_nearby_entities(frame->world, NULL, box->bounds, out,
                                         &frame->error->nearby) == MC_WORLD_STORAGE_OK)
        return true;
    frame->error->query = frame->error->nearby.query;
    frame->error->registry = frame->error->nearby.query.registry;
    frame->error->population = frame->error->nearby.query.population;
    frame->error->entity = frame->error->nearby.entity;
    return pressure_storage_fail(frame, 82, frame->error->nearby.query.cause);
}

static bool pressure_class_literal(const pressure_frame *frame, unsigned int pc,
                                   mc_entity_class_kind kind, const mc_entity_class_literal **out) {
    pressure_at(frame, pc);
    mc_entity_classes *const classes = frame->block->classes->entities;
    if (classes == NULL)
        return pressure_fail(frame, pc, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                             "Pressure Class literal defining classes binding");
    const mc_entity_status status = mc_entity_class_literal_get(classes, kind, out);
    if (status == MC_ENTITY_OK)
        return true;
    frame->error->entity =
        (mc_entity_error){.status = status, .operation = "Pressure Class literal"};
    return pressure_fail(frame, pc, pressure_bounds_status(status), "Pressure Class literal");
}

static bool pressure_class_query(const pressure_frame *frame, unsigned int pc,
                                 const mc_entity_class_literal *target, const pressure_box *box,
                                 mc_world_class_entity_list **selected,
                                 mc_world_entity_list_view *untyped) {
    mc_world_class_entity_list *result = NULL;
    pressure_at(frame, pc);
    if (mc_world_runtime_class_entities(frame->world, target, box->bounds, &result,
                                        &frame->error->nearby) != MC_WORLD_STORAGE_OK) {
        frame->error->query = frame->error->nearby.query;
        frame->error->registry = frame->error->nearby.query.registry;
        frame->error->population = frame->error->nearby.query.population;
        frame->error->entity = frame->error->nearby.entity;
        return pressure_storage_fail(frame, pc, frame->error->nearby.query.cause);
    }
    /* local8 is overwritten only after the new query returns. Its old result
     * stays held throughout that query, including overlapping native charges. */
    mc_world_class_entity_list_release(*selected);
    mc_world_entity_list_view_release(untyped);
    *selected = result;
    return true;
}

static bool pressure_change(const pressure_frame *frame, bool pressed, int32_t x, int32_t y,
                            int32_t z) {
    const unsigned int metadata_pc = pressed ? 234u : 320u;
    const unsigned int neighbor_pc = pressed ? 246u : 332u;
    const unsigned int below_pc = pressed ? 260u : 346u;
    const unsigned int range_pc = pressed ? 272u : 358u;
    mc_world_mutation_error lower = mc_world_mutation_empty_error();
    pressure_at(frame, metadata_pc);
    if (mc_world_runtime_metadata_set(frame->world, x, y, z, pressed ? 1 : 0, &lower) !=
        MC_WORLD_STORAGE_OK)
        return pressure_nested(frame, metadata_pc, &lower);

    pressure_at(frame, pressed ? 243u : 329u);
    const int32_t current_id = frame->block->fields.bn;
    pressure_at(frame, neighbor_pc);
    if (!mc_world_mutation_neighbors(frame->world, x, y, z, current_id, &lower))
        return pressure_nested(frame, neighbor_pc, &lower);
    const int32_t below = mc_java_i32_sub(y, 1);
    pressure_at(frame, pressed ? 257u : 343u);
    const int32_t below_id = frame->block->fields.bn;
    pressure_at(frame, below_pc);
    if (!mc_world_mutation_neighbors(frame->world, x, below, z, below_id, &lower))
        return pressure_nested(frame, below_pc, &lower);
    pressure_at(frame, range_pc);
    if (!mc_world_mutation_observe_range(frame->world, x, y, z, x, y, z, &lower))
        return pressure_nested(frame, range_pc, &lower);

    if (!pressure_floating(frame, pressed ? 277u : 363u))
        return false;
    const double sound_x = (double)x + 0.5;
    const double sound_y = (double)y + 0.1;
    const double sound_z = (double)z + 0.5;
    const unsigned int literal_pc = pressed ? 295u : 381u;
    const unsigned int sound_pc = pressed ? 301u : 387u;
    mc_registry_error registry = mc_world_mutation_empty_registry();
    const mc_registry_string *name;
    pressure_at(frame, literal_pc);
    const mc_registry_status status =
        mc_registry_runtime_literal_ascii(frame->block->classes, "random.click", &name, &registry);
    if (status != MC_REGISTRY_OK)
        return pressure_registry_fail(frame, literal_pc, status, &registry,
                                      "Pressure click String literal");
    pressure_at(frame, sound_pc);
    if (!mc_world_mutation_sound(frame->world, sound_x, sound_y, sound_z, name, 0.3f,
                                 pressed ? 0.6f : 0.5f, &lower))
        return pressure_nested(frame, sound_pc, &lower);
    return true;
}

static bool pressure_schedule(const pressure_frame *frame, int32_t x, int32_t y, int32_t z) {
    pressure_at(frame, 401);
    const int32_t id = frame->block->fields.bn;
    pressure_at(frame, 405);
    const int32_t delay = 20; /* Complete bv.e/bc.c: bipush20, ireturn2. */
    mc_world_schedule_error scheduled;
    pressure_at(frame, 408);
    if (mc_world_runtime_schedule(frame->world, x, y, z, id, delay, &scheduled) ==
        MC_WORLD_STORAGE_OK)
        return true;
    mc_world_mutation_error lower = scheduled.tick;
    lower.schedule = (mc_world_schedule_prefix){.status = scheduled.status,
                                                .operation = scheduled.operation,
                                                .bytecode = scheduled.bytecode,
                                                .presence_bytecode = scheduled.presence_bytecode,
                                                .provider_bytecode = scheduled.provider_bytecode,
                                                .cause = scheduled.cause,
                                                .record = scheduled.record,
                                                .membership = scheduled.membership,
                                                .ordered = scheduled.ordered,
                                                .registry = scheduled.registry};
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
    return pressure_nested(frame, 408, &lower);
}

bool mc_world_mutation_pressure_update(mc_world_runtime *world, const mc_block *block, int32_t x,
                                       int32_t y, int32_t z, mc_world_mutation_error *error) {
    const pressure_frame frame = {world, block, error, PRESSURE_PRIVATE};
    int32_t metadata;
    if (!pressure_metadata(&frame, 5, x, y, z, &metadata))
        return false;
    const bool previously_on = metadata == 1;
    pressure_box boxes[3] = {{0}}; /* The three actual factory sites, not a work cap. */
    mc_world_entity_list_view list = {0};
    mc_world_class_entity_list *class_list = NULL;
    bool equal;
    bool completed = false;
    if (!pressure_category_equal(&frame, 33, 0, &equal))
        goto done;
    if (equal) {
        if (!pressure_pooled(&frame, 42, 79, x, y, z, &boxes[0]) ||
            !pressure_nearby(&frame, &boxes[0], &list))
            goto done;
    }
    if (!pressure_category_equal(&frame, 91, 1, &equal))
        goto done;
    if (equal) {
        const mc_entity_class_literal *target;
        if (!pressure_class_literal(&frame, 98, MC_ENTITY_CLASS_KIND_LIVING_ENTITY, &target) ||
            !pressure_pooled(&frame, 101, 138, x, y, z, &boxes[1]) ||
            !pressure_class_query(&frame, 141, target, &boxes[1], &class_list, &list))
            goto done;
    }
    if (!pressure_category_equal(&frame, 150, 2, &equal))
        goto done;
    if (equal) {
        const mc_entity_class_literal *target;
        if (!pressure_class_literal(&frame, 157, MC_ENTITY_CLASS_KIND_PLAYER, &target) ||
            !pressure_pooled(&frame, 160, 197, x, y, z, &boxes[2]) ||
            !pressure_class_query(&frame, 200, target, &boxes[2], &class_list, &list))
            goto done;
    }

    pressure_at(&frame, 207);
    size_t count;
    if (class_list == NULL && list.owner == NULL) {
        pressure_fail(&frame, 207, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "Pressure query List size receiver is null");
        goto done;
    }
    const mc_world_storage_status status =
        class_list != NULL ? mc_world_class_entity_list_count(class_list, &count)
                           : mc_world_entity_list_view_count(&list, &count);
    if (status != MC_WORLD_STORAGE_OK) {
        pressure_fail(&frame, 207, status, "Pressure query List size");
        goto done;
    }
    const bool found = count > 0;
    if (found && !previously_on && !pressure_change(&frame, true, x, y, z))
        goto done;
    if (!found && previously_on && !pressure_change(&frame, false, x, y, z))
        goto done;
    if (found && !pressure_schedule(&frame, x, y, z))
        goto done;
    completed = pressure_done(error);

done:
    mc_world_class_entity_list_release(class_list);
    mc_world_entity_list_view_release(&list);
    for (size_t i = 0; i < sizeof(boxes) / sizeof(boxes[0]); ++i)
        if (boxes[i].bounds != NULL)
            (void)mc_entity_bounds_release(boxes[i].classes, boxes[i].bounds);
    return completed;
}

bool mc_world_mutation_pressure_tick(mc_world_runtime *world, const mc_block *block, int32_t x,
                                     int32_t y, int32_t z, mc_java_random *random,
                                     mc_world_mutation_error *error) {
    (void)random; /* The source tick never reads this nullable argument. */
    const pressure_frame frame = {world, block, error, PRESSURE_TICK};
    pressure_at(&frame, 1);
    if (world == NULL)
        return pressure_fail(&frame, 1, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                             "Pressure tick World B receiver");
    if (world->fields.B)
        return pressure_done(error);
    int32_t metadata;
    if (!pressure_metadata(&frame, 13, x, y, z, &metadata))
        return false;
    if (metadata == 0)
        return pressure_done(error);
    pressure_at(&frame, 26);
    if (!mc_world_mutation_pressure_update(world, block, x, y, z, error))
        return pressure_nested(&frame, 26, error);
    return pressure_done(error);
}
