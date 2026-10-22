#include "rail_power_search_internal.h"
#include "wrapper_transport_internal.h"

#include "../../../../block/registry/registry_internal.h"
#include "../../collision/blocks_internal.h"
#include "../../power/power_internal.h"
#include "../mutation_internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/core/java/java.h"

typedef enum rail_search_method { RAIL_SEARCH, RAIL_VERIFY } rail_search_method;

typedef struct rail_search_frame {
    mc_world_runtime *world;
    const mc_block *block;
    int32_t x, y, z, metadata, depth, axis;
    bool forward;
    rail_search_method method;
    mc_rail_power_search_error *error;
} rail_search_frame;

mc_rail_power_search_error mc_rail_power_search_empty_error(void) {
    return (mc_rail_power_search_error){.mutation = mc_world_mutation_empty_error(),
                                        .access = mc_world_collision_empty_error(),
                                        .power = mc_world_power_empty_error()};
}

static mc_rail_power_search_site *search_site(const rail_search_frame *frame) {
    return frame->method == RAIL_SEARCH ? &frame->error->search : &frame->error->verify;
}

static void search_at(const rail_search_frame *frame, unsigned int pc) {
    const bool client = frame->block->classes->options.side == MC_REGISTRY_CLIENT;
    const char *method = frame->method == RAIL_SEARCH
                             ? (client ? "a(Lfd;IIIIZI)Z" : "a(Ldj;IIIIZI)Z")
                             : (client ? "a(Lfd;IIIZII)Z" : "a(Ldj;IIIZII)Z");
    const mc_rail_power_search_site site = {.entered = true,
                                            .declaring_class = client ? "pc" : "jj",
                                            .method = method,
                                            .bytecode = pc,
                                            .x = frame->x,
                                            .y = frame->y,
                                            .z = frame->z,
                                            .metadata = frame->metadata,
                                            .depth = frame->depth,
                                            .axis = frame->axis,
                                            .forward = frame->forward};
    *search_site(frame) = site;
    frame->error->selected |=
        frame->method == RAIL_SEARCH ? MC_WORLD_RAIL_SITE_SEARCH : MC_WORLD_RAIL_SITE_VERIFY;
    frame->error->mutation.hook_class = site.declaring_class;
    frame->error->mutation.hook_method = site.method;
    frame->error->mutation.hook_bytecode = pc;
}

static void search_cause(const rail_search_frame *frame) {
    if ((frame->error->selected & MC_WORLD_RAIL_SITE_SEARCH_CAUSE) == 0) {
        frame->error->cause = *search_site(frame);
        frame->error->selected |= MC_WORLD_RAIL_SITE_SEARCH_CAUSE;
        frame->error->origin =
            frame->method == RAIL_SEARCH ? MC_WORLD_RAIL_SITE_SEARCH : MC_WORLD_RAIL_SITE_VERIFY;
    }
}

static bool search_storage(const rail_search_frame *frame, unsigned int pc,
                           mc_world_storage_status status, mc_world_storage_error cause) {
    search_at(frame, pc);
    search_cause(frame);
    frame->error->mutation.status = status;
    frame->error->mutation.operation = cause.operation;
    frame->error->mutation.cause = cause;
    if (frame->world != NULL)
        frame->world->last_error = cause;
    return false;
}

static bool search_nested(const rail_search_frame *frame, unsigned int pc) {
    mc_world_mutation_error *const error = &frame->error->mutation;
    if (error->cause_hook_class == NULL) {
        error->cause_hook_class = error->hook_class;
        error->cause_hook_method = error->hook_method;
        error->cause_hook_bytecode = error->hook_bytecode;
    }
    search_at(frame, pc);
    return false;
}

static void search_nearby(mc_world_mutation_error *error, const mc_world_nearby_error *child) {
    error->nearby = *child;
    error->query = child->query;
    error->registry = child->query.registry;
    error->population = child->query.population;
    error->entity = child->entity;
    error->tile = child->tile;
    error->batch = child->batch;
    error->load_entity = child->load_entity;
    error->activation_bytecode = child->activation_bytecode;
    error->activation_world_bytecode = child->activation_world_bytecode;
    error->load_bytecode = child->load_bytecode;
    error->load_builder_bytecode = child->load_builder_bytecode;
}

static bool search_read(const rail_search_frame *frame, unsigned int pc, bool metadata,
                        int32_t *out) {
    mc_world_collision_error child = mc_world_collision_empty_error();
    search_at(frame, pc);
    const mc_world_storage_status status = mc_world_collision_block_value_held(
        frame->world, frame->x, frame->y, frame->z, metadata, out, &child);
    if (status == MC_WORLD_STORAGE_OK)
        return true;
    frame->error->access = child;
    frame->error->mutation.rail_access = child;
    search_nearby(&frame->error->mutation, &child.nearby);
    return search_storage(frame, pc, status, child.nearby.query.cause);
}

static bool search_registry(const rail_search_frame *frame, unsigned int pc,
                            const mc_registry_error *child, const char *operation) {
    search_at(frame, pc);
    search_cause(frame);
    frame->error->mutation.registry = *child;
    if (frame->world == NULL) {
        const mc_world_storage_status status = mc_world_runtime_registry_status(child->status);
        return search_storage(frame, pc, status, (mc_world_storage_error){status, operation, 0});
    }
    const mc_world_storage_status status = mc_world_runtime_registry_result(
        frame->world, child->status, child, operation, &frame->error->mutation.cause);
    frame->error->mutation.status = status;
    frame->error->mutation.operation = frame->error->mutation.cause.operation;
    return false;
}

static bool search_live_u(const rail_search_frame *frame, int32_t *out) {
    const mc_block *selected = NULL;
    mc_registry_error child = mc_world_mutation_empty_error().registry;
    search_at(frame, 12);
    if (mc_registry_block_static_get(frame->block->classes, "U", &selected, &child) !=
        MC_REGISTRY_OK)
        return search_registry(frame, 12, &child, "Powered rail search live U active use");
    search_at(frame, 15);
    if (selected == NULL) {
        child.status = MC_REGISTRY_NULL_REFERENCE;
        child.message = "Powered rail search live U bn receiver is null";
        return search_registry(frame, 15, &child, child.message);
    }
    *out = selected->fields.bn;
    return true;
}

static bool search_power(const rail_search_frame *frame, unsigned int pc, int32_t y, bool *out) {
    mc_world_power_error child = mc_world_power_empty_error();
    search_at(frame, pc);
    const mc_world_storage_status status =
        mc_world_power_neighbors_held(frame->world, frame->x, y, frame->z, out, &child);
    if (status == MC_WORLD_STORAGE_OK)
        return true;
    frame->error->power = child;
    frame->error->mutation.power = child;
    search_nearby(&frame->error->mutation, &child.access);
    frame->error->mutation.registry = child.registry;
    frame->error->mutation.support_bytecode = child.support.bytecode;
    return search_storage(frame, pc, status, child.cause);
}

static bool search_kernel(rail_search_frame frame, bool *out);
static bool verify_kernel(rail_search_frame frame, bool *out);

/* pc/jj.a(World,IIIIZI)Z. These are source local variables, not a table of
 * predicted graph results. Default shapes6/7 retain the current coordinates
 * and their masked axis. Only ascending branches disable the below attempt. */
static bool search_kernel(rail_search_frame frame, bool *out) {
    search_at(&frame, 0);
    if (frame.depth >= 8) {
        *out = false;
        return true; /* false8 before every World getter and class active use. */
    }
    frame.axis = frame.metadata & INT32_C(7);
    search_at(&frame, 13);
    bool below = true;
    switch (frame.axis) {
        case 0:
            frame.z = frame.forward ? mc_java_i32_add(frame.z, 1) : mc_java_i32_sub(frame.z, 1);
            break;
        case 1:
            frame.x = frame.forward ? mc_java_i32_sub(frame.x, 1) : mc_java_i32_add(frame.x, 1);
            break;
        case 2:
            if (frame.forward) {
                frame.x = mc_java_i32_sub(frame.x, 1);
            } else {
                frame.x = mc_java_i32_add(frame.x, 1);
                frame.y = mc_java_i32_add(frame.y, 1);
                below = false;
            }
            frame.axis = 1;
            break;
        case 3:
            if (frame.forward) {
                frame.x = mc_java_i32_sub(frame.x, 1);
                frame.y = mc_java_i32_add(frame.y, 1);
                below = false;
            } else {
                frame.x = mc_java_i32_add(frame.x, 1);
            }
            frame.axis = 1;
            break;
        case 4:
            if (frame.forward) {
                frame.z = mc_java_i32_add(frame.z, 1);
            } else {
                frame.z = mc_java_i32_sub(frame.z, 1);
                frame.y = mc_java_i32_add(frame.y, 1);
                below = false;
            }
            frame.axis = 0;
            break;
        case 5:
            if (frame.forward) {
                frame.z = mc_java_i32_add(frame.z, 1);
                frame.y = mc_java_i32_add(frame.y, 1);
                below = false;
            } else {
                frame.z = mc_java_i32_sub(frame.z, 1);
            }
            frame.axis = 0;
            break;
        default:
            break;
    }
    rail_search_frame verifier = frame;
    verifier.method = RAIL_VERIFY;
    verifier.metadata = 0; /* The verifier has not read its metadata local. */
    bool found;
    search_at(&frame, 207);
    if (!verify_kernel(verifier, &found))
        return search_nested(&frame, 207);
    if (found) {
        *out = true;
        return true;
    }
    if (below) {
        verifier.y = mc_java_i32_sub(frame.y, 1);
        search_at(&frame, 234);
        if (!verify_kernel(verifier, &found))
            return search_nested(&frame, 234);
        if (found) {
            *out = true;
            return true;
        }
    }
    *out = false;
    return true;
}

/* pc/jj.a(World,IIIZII)Z. ID5 precedes live U active use even for air;
 * metadata26 occurs only after matching that actual live bn. */
static bool verify_kernel(rail_search_frame frame, bool *out) {
    search_at(&frame, 0);
    int32_t id, powered_id;
    if (!search_read(&frame, 5, false, &id) || !search_live_u(&frame, &powered_id))
        return false;
    if (id != powered_id) {
        *out = false;
        return true;
    }
    int32_t metadata;
    if (!search_read(&frame, 26, true, &metadata))
        return false;
    frame.metadata = metadata;
    const int32_t shape = metadata & INT32_C(7);
    if (frame.axis == 1 && (shape == 0 || shape == 4 || shape == 5)) {
        *out = false;
        return true; /* false62. */
    }
    if (frame.axis == 0 && (shape == 1 || shape == 2 || shape == 3)) {
        *out = false;
        return true; /* false87. */
    }
    if ((metadata & INT32_C(8)) == 0) {
        *out = false;
        return true;
    }
    bool powered;
    if (!search_power(&frame, 101, frame.y, &powered))
        return false;
    if (!powered && !search_power(&frame, 114, mc_java_i32_add(frame.y, 1), &powered))
        return false;
    if (powered) {
        *out = true;
        return true; /* true121, no recursive read. */
    }
    rail_search_frame next = frame;
    next.method = RAIL_SEARCH;
    next.metadata = metadata; /* The actual raw metadata, not shape. */
    next.depth = mc_java_i32_add(frame.depth, 1);
    next.axis = 0; /* Native diagnostic context until source masking. */
    search_at(&frame, 136);
    if (!search_kernel(next, out))
        return search_nested(&frame, 136);
    return true;
}

static bool search_admit(const mc_block *block, const bool *out,
                         mc_rail_power_search_error *error) {
    if (out == NULL)
        return mc_world_mutation_fail(NULL, &error->mutation, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                                      "Powered rail search output argument");
    if (block == NULL || block->classes == NULL)
        return mc_world_mutation_fail(NULL, &error->mutation, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                      "Powered rail search invocation receiver/defining owner");
    return true;
}

static mc_world_storage_status search_result(bool completed, bool value,
                                             const mc_rail_power_search_error *local, bool *out,
                                             mc_rail_power_search_error *error) {
    if (!completed) {
        *error = *local;
        mc_rail_power_search_error_publish(error);
        return error->mutation.status;
    }
    *out = value;
    *error = mc_rail_power_search_empty_error();
    return MC_WORLD_STORAGE_OK;
}

mc_world_storage_status mc_rail_power_search_held(mc_world_runtime *world, const mc_block *block,
                                                  int32_t x, int32_t y, int32_t z, int32_t metadata,
                                                  bool forward, int32_t depth, bool *out,
                                                  mc_rail_power_search_error *error) {
    if (error == NULL)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    mc_rail_power_search_error local = mc_rail_power_search_empty_error();
    const rail_search_frame frame = {world, block, x,       y,           z,     metadata,
                                     depth, 0,     forward, RAIL_SEARCH, &local};
    bool value = false;
    const bool completed = search_admit(block, out, &local) && search_kernel(frame, &value);
    return search_result(completed, value, &local, out, error);
}

mc_world_storage_status mc_rail_power_verify_held(mc_world_runtime *world, const mc_block *block,
                                                  int32_t x, int32_t y, int32_t z, bool forward,
                                                  int32_t depth, int32_t axis, bool *out,
                                                  mc_rail_power_search_error *error) {
    if (error == NULL)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    mc_rail_power_search_error local = mc_rail_power_search_empty_error();
    const rail_search_frame frame = {world, block, x,       y,           z,     0,
                                     depth, axis,  forward, RAIL_VERIFY, &local};
    bool value = false;
    const bool completed = search_admit(block, out, &local) && verify_kernel(frame, &value);
    return search_result(completed, value, &local, out, error);
}
