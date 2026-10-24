#include "mutation_internal.h"

#include "../../../block/registry/registry_internal.h"
#include "../../../block/registry/runtime_internal.h"
#include "../construction_internal.h"
#include "../presence_internal.h"
#include "mc/block/registry/falling.h"
#include "mc/block/registry/runtime.h"
#include "mc/core/java/java.h"
#include "mc/world/runtime/blocks.h"
#include "mc/world/runtime/scheduling.h"

#include <float.h>

#if !defined(_M_X64) && !defined(_M_AMD64) && !defined(__x86_64__)
#error "Falling Block centers require x86-64/SSE2"
#endif
#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) ||       \
    (defined(_M_FP_FAST) && _M_FP_FAST)
#error "Falling Block centers require strict floating transformations"
#endif
#include <emmintrin.h>
_Static_assert(FLT_RADIX == 2 && FLT_MANT_DIG == 24 && DBL_MANT_DIG == 53 && sizeof(float) == 4 &&
                   sizeof(double) == 8 && FLT_EVAL_METHOD == 0,
               "Falling Block centers require source-width binary32/binary64");

typedef enum falling_method {
    FALLING_ADDED,
    FALLING_NEIGHBOR,
    FALLING_PRIVATE,
    FALLING_PREDICATE
} falling_method;

typedef struct falling_frame {
    mc_world_runtime *world;
    mc_registry_classes *classes;
    const mc_block *block;
    mc_world_mutation_error *error;
    falling_method method;
    unsigned int tick_bytecode;
} falling_frame;

static const char *falling_method_name(const falling_frame *frame) {
    const bool client = frame->classes->options.side == MC_REGISTRY_CLIENT;
    switch (frame->method) {
        case FALLING_ADDED:
            return client ? "c(Lfd;III)V" : "c(Ldj;III)V";
        case FALLING_NEIGHBOR:
            return client ? "b(Lfd;IIII)V" : "b(Ldj;IIII)V";
        case FALLING_PRIVATE:
            return client ? "h(Lfd;III)V" : "g(Ldj;III)V";
        case FALLING_PREDICATE:
            return client ? "c_(Lfd;III)Z" : "c_(Ldj;III)Z";
    }
    return NULL;
}

static void falling_at(const falling_frame *frame, unsigned int pc) {
    frame->error->hook_class = frame->classes->options.side == MC_REGISTRY_CLIENT ? "gk" : "ei";
    frame->error->hook_method = falling_method_name(frame);
    frame->error->hook_bytecode = pc;
    if (frame->tick_bytecode != 0)
        frame->error->falling_tick_bytecode = frame->tick_bytecode;
    if (frame->method == FALLING_PRIVATE)
        frame->error->falling_bytecode = pc;
    else if (frame->method == FALLING_PREDICATE)
        frame->error->falling_predicate_bytecode = pc;
}

static bool falling_return(const falling_frame *frame) {
    frame->error->hook_class = NULL;
    frame->error->hook_method = NULL;
    frame->error->hook_bytecode = 0;
    frame->error->falling_tick_bytecode = 0;
    frame->error->falling_bytecode = 0;
    frame->error->falling_predicate_bytecode = 0;
    return true;
}

static bool falling_fail(const falling_frame *frame, unsigned int pc,
                         mc_world_storage_status status, const char *operation) {
    falling_at(frame, pc);
    return mc_world_mutation_fail(frame->world, frame->error, status, operation);
}

static bool falling_storage_fail(const falling_frame *frame, unsigned int pc,
                                 mc_world_storage_error cause) {
    falling_at(frame, pc);
    frame->error->status = cause.status;
    frame->error->operation = cause.operation;
    frame->error->cause = cause;
    if (frame->world != NULL)
        frame->world->last_error = cause;
    return false;
}

/* Keep the current private caller, independent predicate PC and deepest
 * concrete hook. This is the existing finite projection, not a source stack
 * or a gameplay recursion limit. lower may alias the destination. */
static bool falling_nested(const falling_frame *frame, unsigned int pc,
                           const mc_world_mutation_error *lower) {
    const mc_world_mutation_error captured = *lower;
    *frame->error = captured;
    if (captured.cause_hook_class == NULL) {
        frame->error->cause_hook_class = captured.hook_class;
        frame->error->cause_hook_method = captured.hook_method;
        frame->error->cause_hook_bytecode = captured.hook_bytecode;
    }
    falling_at(frame, pc);
    if (frame->world != NULL)
        frame->world->last_error = captured.cause;
    return false;
}

static bool falling_registry_fail(const falling_frame *frame, unsigned int pc,
                                  mc_registry_status status, const mc_registry_error *cause,
                                  const char *operation) {
    falling_at(frame, pc);
    frame->error->registry = *cause;
    if (frame->world != NULL) {
        frame->error->status = mc_world_runtime_registry_result(frame->world, status, cause,
                                                                operation, &frame->error->cause);
        frame->error->operation = frame->error->cause.operation;
    } else {
        frame->error->status = mc_world_runtime_registry_status(status);
        frame->error->operation = operation;
        frame->error->cause = (mc_world_storage_error){frame->error->status, operation, 0};
    }
    return false;
}

static bool falling_null(const falling_frame *frame, unsigned int pc, const char *operation) {
    mc_registry_error cause = mc_world_mutation_empty_registry();
    cause.status = MC_REGISTRY_NULL_REFERENCE;
    cause.message = operation;
    return falling_registry_fail(frame, pc, cause.status, &cause, operation);
}

/* invokestatic initializes its actual declaring class at the CALLER's invoke
 * site, before any c_ body instruction. This narrow admission has no extra
 * flag read, Block-only initialization, owner copy or synthetic class state. */
static bool falling_predicate_active_use(const falling_frame *frame) {
    mc_registry_classes *classes = frame->classes;
    mc_registry_error cause = mc_world_mutation_empty_registry();
    if (classes->entered != 0 || classes->blocks_phase == MC_REGISTRY_INITIALIZING) {
        cause.status = MC_REGISTRY_BUSY;
        cause.message = "Falling predicate Registry entered";
        return falling_registry_fail(frame, 0, cause.status, &cause, cause.message);
    }
    ++classes->entered;
    const mc_registry_status status = mc_registry_block_class_initialize(classes, MC_BLOCK_SAND);
    --classes->entered;
    if (status == MC_REGISTRY_OK)
        return true;
    cause = classes->last_error;
    return falling_registry_fail(frame, 0, status, &cause,
                                 "Falling predicate declaring class active use");
}

bool mc_world_mutation_falling_can_pass(mc_world_runtime *world, mc_registry_classes *classes,
                                        int32_t x, int32_t y, int32_t z, bool *out,
                                        mc_world_mutation_error *error) {
    if (classes == NULL || out == NULL)
        return mc_world_mutation_fail(world, error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                                      "Falling predicate native arguments");
    const falling_frame frame = {world, classes, NULL, error, FALLING_PREDICATE, 0};
    if (!falling_predicate_active_use(&frame))
        return false;

    int32_t id;
    mc_world_storage_error storage = {0};
    falling_at(&frame, 4);
    if (mc_world_runtime_block_get(world, x, y, z, &id, &storage) != MC_WORLD_STORAGE_OK)
        return falling_storage_fail(&frame, 4, storage);
    if (id == 0) {
        *out = true;
        return falling_return(&frame);
    }

    mc_registry_error registry = mc_world_mutation_empty_registry();
    const mc_block *fire;
    falling_at(&frame, 18);
    mc_registry_status status = mc_registry_block_static_get(classes, "as", &fire, &registry);
    if (status != MC_REGISTRY_OK)
        return falling_registry_fail(&frame, 18, status, &registry, "Falling live Block.as");
    falling_at(&frame, 21);
    if (fire == NULL)
        return falling_null(&frame, 21, "Falling fire Block.bn receiver");
    if (id == fire->fields.bn) {
        *out = true;
        return falling_return(&frame);
    }

    mc_registry_block_m_view array;
    falling_at(&frame, 29);
    status = mc_registry_block_m_get(classes, &array, &registry);
    if (status != MC_REGISTRY_OK)
        return falling_registry_fail(&frame, 29, status, &registry,
                                     "Falling live Block.m active use");
    const mc_block *selected;
    falling_at(&frame, 34);
    status = mc_registry_block_m_at(&array, id, &selected, &registry);
    if (status != MC_REGISTRY_OK)
        return falling_registry_fail(&frame, 34, status, &registry,
                                     "Falling captured Block.m element");
    falling_at(&frame, 35);
    if (selected == NULL)
        return falling_null(&frame, 35, "Falling selected Block.bA receiver");
    mc_material *const material = selected->fields.bA; /* Nullable identity local38. */
    mc_material *water;
    falling_at(&frame, 42);
    status = mc_registry_material_get(classes, MC_MATERIAL_g, &water, &registry);
    if (status != MC_REGISTRY_OK)
        return falling_registry_fail(&frame, 42, status, &registry, "Falling Material.g");
    if (material == water) {
        *out = true;
        return falling_return(&frame);
    }
    mc_material *lava;
    falling_at(&frame, 52);
    status = mc_registry_material_get(classes, MC_MATERIAL_h, &lava, &registry);
    if (status != MC_REGISTRY_OK)
        return falling_registry_fail(&frame, 52, status, &registry, "Falling Material.h");
    *out = material == lava;
    return falling_return(&frame);
}

static bool falling_predicate_call(const falling_frame *frame, unsigned int pc, int32_t x,
                                   int32_t y, int32_t z, bool *out) {
    mc_world_mutation_error lower = mc_world_mutation_empty_error();
    falling_at(frame, pc);
    return mc_world_mutation_falling_can_pass(frame->world, frame->classes, x, y, z, out, &lower)
               ? true
               : falling_nested(frame, pc, &lower);
}

static bool falling_write(const falling_frame *frame, unsigned int pc, int32_t x, int32_t y,
                          int32_t z, int32_t id) {
    mc_world_mutation_error lower = mc_world_mutation_empty_error();
    bool written;
    falling_at(frame, pc);
    if (mc_world_runtime_block_set(frame->world, x, y, z, id, &written, &lower) !=
        MC_WORLD_STORAGE_OK)
        return falling_nested(frame, pc, &lower);
    return true; /* Both actual setter booleans are discarded by the source. */
}

static bool falling_region(const falling_frame *frame, int32_t x, int32_t y, int32_t z, bool *out) {
    mc_world_presence_error presence = {0};
    falling_at(frame, 67);
    if (mc_world_runtime_region_present_held(
            frame->world, mc_java_i32_sub(x, 32), mc_java_i32_sub(y, 32), mc_java_i32_sub(z, 32),
            mc_java_i32_add(x, 32), mc_java_i32_add(y, 32), mc_java_i32_add(z, 32), out,
            &presence) == MC_WORLD_STORAGE_OK)
        return true;
    frame->error->presence_bytecode = presence.bytecode;
    frame->error->provider_bytecode = presence.provider_bytecode;
    return falling_storage_fail(frame, 67, presence.cause);
}

static bool falling_entity_failed(const falling_frame *frame, unsigned int pc,
                                  mc_entity_status status) {
    mc_world_storage_status mapped;
    switch (status) {
        case MC_ENTITY_LIMIT:
            mapped = MC_WORLD_STORAGE_LIMIT;
            break;
        case MC_ENTITY_ALLOCATION_FAILED:
            mapped = MC_WORLD_STORAGE_ALLOCATION_FAILED;
            break;
        case MC_ENTITY_BUSY:
            mapped = MC_WORLD_STORAGE_BUSY;
            break;
        case MC_ENTITY_INVALID_ARGUMENT:
            mapped = MC_WORLD_STORAGE_INVALID_ARGUMENT;
            break;
        case MC_ENTITY_WORLD_FAILED:
            mapped = frame->error->entity.world == MC_WORLD_STORAGE_OK
                         ? MC_WORLD_STORAGE_UNSUPPORTED_STATE
                         : frame->error->entity.world;
            break;
        default:
            mapped = MC_WORLD_STORAGE_UNSUPPORTED_STATE;
            break;
    }
    return falling_fail(frame, pc, mapped,
                        frame->error->entity.operation == NULL ? "Falling Entity operation"
                                                               : frame->error->entity.operation);
}

static bool falling_publish(const falling_frame *frame, int32_t x, int32_t y, int32_t z) {
    falling_at(frame, 126);
    mc_entity_classes *classes =
        frame->world->classes == NULL ? NULL : frame->world->classes->entities;
    if (classes == NULL)
        return falling_fail(frame, 126, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                            "Falling EntityClasses binding is unavailable");
    mc_entity *entity = NULL;
    mc_entity_status status = mc_falling_entity_allocate(classes, &entity, &frame->error->entity);
    if (status != MC_ENTITY_OK)
        return falling_entity_failed(frame, 126, status);

    /* The real new126 precedes the FIRST i2f132 and every center operation.
     * No FP gate preempts the integer immediate-fall branch or RAW allocation. */
    falling_at(frame, 132);
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u) {
        (void)falling_fail(frame, 132, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                           "Falling Block center floating environment");
        mc_entity_release(entity);
        return false;
    }
    const float center_x = (float)x + 0.5f;
    const double position_x = (double)center_x;
    const float center_y = (float)y + 0.5f;
    const double position_y = (double)center_y;
    const float center_z = (float)z + 0.5f;
    const double position_z = (double)center_z;
    const int32_t id = frame->block->fields.bn; /* Actual late getfield151. */
    falling_at(frame, 154);
    status = mc_falling_entity_construct(entity, frame->world, position_x, position_y, position_z,
                                         id, &frame->error->entity);
    if (status != MC_ENTITY_OK) {
        (void)falling_entity_failed(frame, 154, status);
        mc_entity_release(entity);
        return false;
    }
    bool inserted;
    falling_at(frame, 162);
    const mc_world_storage_status published =
        mc_world_runtime_insert_entity(frame->world, entity, &inserted, &frame->error->publication);
    if (published != MC_WORLD_STORAGE_OK) {
        (void)falling_storage_fail(frame, 162, frame->error->publication.cause);
        mc_entity_release(entity);
        return false;
    }
    /* Source pop165 discards true/false. Only the local native hold retires;
     * any real Chunk/World publication holds survive even a later failure. */
    mc_entity_release(entity);
    return true;
}

static bool falling_private(const falling_frame *frame, int32_t x, int32_t y, int32_t z) {
    bool pass;
    if (!falling_predicate_call(frame, 19, x, mc_java_i32_sub(y, 1), z, &pass))
        return false;
    if (!pass || y < 0)
        return falling_return(frame);

    bool immediate;
    mc_registry_error registry = mc_world_mutation_empty_registry();
    falling_at(frame, 34);
    const mc_registry_status status =
        mc_registry_falling_flag_get(frame->classes, &immediate, &registry);
    if (status != MC_REGISTRY_OK)
        return falling_registry_fail(frame, 34, status, &registry, "Falling shared flag");
    if (!immediate) {
        bool present;
        if (!falling_region(frame, x, y, z, &present))
            return false;
        if (present)
            return falling_publish(frame, x, y, z) ? falling_return(frame) : false;
    }

    if (!falling_write(frame, 79, x, y, z, 0))
        return false;
    for (;;) {
        if (!falling_predicate_call(frame, 90, x, mc_java_i32_sub(y, 1), z, &pass))
            return false;
        if (!pass || y <= 0)
            break;
        y = mc_java_i32_sub(y, 1);
    }
    if (y > 0) {
        falling_at(frame, 116);
        const int32_t id = frame->block->fields.bn;
        if (!falling_write(frame, 119, x, y, z, id))
            return false;
    }
    return falling_return(frame);
}

static bool falling_delay(const falling_frame *frame, int32_t *out) {
    falling_at(frame, 10);
    switch (frame->block->kind) {
        case MC_BLOCK_SAND:
        case MC_BLOCK_GRAVEL:
            *out = 3; /* Actual e/client or c/server: iconst_3, ireturn1. */
            return true;
        default:
            return falling_fail(frame, 10, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                "Falling delay concrete virtual target");
    }
}

static bool falling_schedule(const falling_frame *frame, int32_t x, int32_t y, int32_t z) {
    falling_at(frame, 6);
    const int32_t id = frame->block->fields.bn;
    int32_t delay;
    if (!falling_delay(frame, &delay))
        return false;
    mc_world_schedule_error scheduled;
    falling_at(frame, 13);
    if (mc_world_runtime_schedule(frame->world, x, y, z, id, delay, &scheduled) ==
        MC_WORLD_STORAGE_OK)
        return falling_return(frame);

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
    return falling_nested(frame, 13, &lower);
}

bool mc_world_mutation_falling_added(mc_world_runtime *world, const mc_block *block, int32_t x,
                                     int32_t y, int32_t z, mc_world_mutation_error *error) {
    const falling_frame frame = {world, block->classes, block, error, FALLING_ADDED, 0};
    return falling_schedule(&frame, x, y, z);
}

bool mc_world_mutation_falling_neighbor(mc_world_runtime *world, const mc_block *block, int32_t x,
                                        int32_t y, int32_t z, int32_t source_id,
                                        mc_world_mutation_error *error) {
    (void)source_id; /* The actual inherited neighbor body never reads it. */
    const falling_frame frame = {world, block->classes, block, error, FALLING_NEIGHBOR, 0};
    return falling_schedule(&frame, x, y, z);
}

bool mc_world_mutation_falling_tick(mc_world_runtime *world, const mc_block *block, int32_t x,
                                    int32_t y, int32_t z, mc_java_random *random,
                                    mc_world_mutation_error *error) {
    (void)random; /* NULL is valid: a(World,III,Random) does not read this argument. */
    const falling_frame frame = {world, block->classes, block, error, FALLING_PRIVATE, 6};
    error->falling_tick_bytecode = 6; /* Actual invokespecial private h/g6. */
    return falling_private(&frame, x, y, z);
}
