#include "rail_lifecycle_internal.h"
#include "wrapper_transport_internal.h"

#include "../../../../block/registry/registry_internal.h"
#include "../../../../block/registry/runtime_internal.h"
#include "../../block_queries_internal.h"
#include "../../collision/blocks_internal.h"
#include "../../power/power_internal.h"
#include "../mutation_internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/core/java/java.h"

#include <string.h>

typedef enum rail_lifecycle_method { RAIL_ADDED, RAIL_SHAPE, RAIL_NEIGHBOR } rail_lifecycle_method;

typedef struct rail_lifecycle_frame {
    mc_world_runtime *world;
    const mc_block *block;
    int32_t x, y, z;
    rail_lifecycle_method method;
    mc_rail_lifecycle_error *error;
} rail_lifecycle_frame;

/* Keep complete diagnostic reset in caller storage rather than returning an
 * automatic lifecycle aggregate at each nested dispatcher/body boundary. */
void mc_world_mutation_rail_error_init(mc_rail_lifecycle_error *out) {
    static const mc_rail_lifecycle_error zero = {0};
    memcpy(out, &zero, sizeof(*out));
    out->mutation = mc_world_mutation_empty_error();
    out->metadata = mc_world_collision_empty_error();
    out->power = mc_world_power_empty_error();
    out->support.registry = out->mutation.registry;
    out->support_access = mc_world_nearby_empty_error();
    out->connections.mutation = out->mutation;
    out->search.mutation = out->mutation;
    out->search.access = out->metadata;
    out->search.power = out->power;
}

mc_rail_lifecycle_error mc_world_mutation_rail_empty_error(void) {
    mc_rail_lifecycle_error result;
    mc_world_mutation_rail_error_init(&result);
    return result;
}

static mc_rail_lifecycle_site *rail_site(const rail_lifecycle_frame *frame) {
    switch (frame->method) {
        case RAIL_ADDED:
            return &frame->error->added;
        case RAIL_SHAPE:
            return &frame->error->shape;
        case RAIL_NEIGHBOR:
            return &frame->error->neighbor;
    }
    return &frame->error->neighbor;
}

static uint32_t rail_bit(const rail_lifecycle_frame *frame) {
    return frame->method == RAIL_ADDED   ? MC_WORLD_RAIL_SITE_ADDED
           : frame->method == RAIL_SHAPE ? MC_WORLD_RAIL_SITE_SHAPE
                                         : MC_WORLD_RAIL_SITE_NEIGHBOR;
}

static void rail_at(const rail_lifecycle_frame *frame, unsigned int pc) {
    const bool client = frame->block->classes->options.side == MC_REGISTRY_CLIENT;
    const char *method;
    switch (frame->method) {
        case RAIL_ADDED:
            method = client ? "c(Lfd;III)V" : "c(Ldj;III)V";
            break;
        case RAIL_SHAPE:
            method = client ? "a(Lfd;IIIZ)V" : "a(Ldj;IIIZ)V";
            break;
        case RAIL_NEIGHBOR:
            method = client ? "b(Lfd;IIII)V" : "b(Ldj;IIII)V";
            break;
        default:
            method = NULL;
            break;
    }
    const mc_rail_lifecycle_site site = {
        true, client ? "pc" : "jj", method, pc, frame->x, frame->y, frame->z};
    *rail_site(frame) = site;
    frame->error->selected |= rail_bit(frame);
    frame->error->mutation.hook_class = site.declaring_class;
    frame->error->mutation.hook_method = site.method;
    frame->error->mutation.hook_bytecode = pc;
}

static void rail_cause(const rail_lifecycle_frame *frame) {
    if ((frame->error->selected & MC_WORLD_RAIL_SITE_LIFECYCLE_CAUSE) == 0) {
        frame->error->cause = *rail_site(frame);
        frame->error->selected |= MC_WORLD_RAIL_SITE_LIFECYCLE_CAUSE;
        frame->error->origin = rail_bit(frame);
    }
}

static bool rail_fail(const rail_lifecycle_frame *frame, unsigned int pc,
                      mc_world_storage_status status, const char *operation) {
    rail_at(frame, pc);
    rail_cause(frame);
    return mc_world_mutation_fail(frame->world, &frame->error->mutation, status, operation);
}

static bool rail_storage(const rail_lifecycle_frame *frame, unsigned int pc,
                         mc_world_storage_status status, mc_world_storage_error cause) {
    rail_at(frame, pc);
    rail_cause(frame);
    frame->error->mutation.status = status;
    frame->error->mutation.operation = cause.operation;
    frame->error->mutation.cause = cause;
    if (frame->world != NULL)
        frame->world->last_error = cause;
    return false;
}

/* Copy the entire lower mutation before replacing its current caller. Keep
 * that child's deepest actual hook; an unentered native NEW allocation has
 * no constructor source origin even though the graph's native allocation
 * diagnostic retains its constructor category at PC0. */
static bool rail_nested(const rail_lifecycle_frame *frame, unsigned int pc,
                        const mc_world_mutation_error *lower, bool entered_child) {
    const mc_world_mutation_error captured = *lower;
    frame->error->mutation = captured;
    if (captured.cause_hook_class == NULL && entered_child) {
        frame->error->mutation.cause_hook_class = captured.hook_class;
        frame->error->mutation.cause_hook_method = captured.hook_method;
        frame->error->mutation.cause_hook_bytecode = captured.hook_bytecode;
    }
    rail_at(frame, pc);
    rail_cause(frame);
    if (frame->world != NULL)
        frame->world->last_error = captured.cause;
    return false;
}

static void rail_nearby(mc_world_mutation_error *error, const mc_world_nearby_error *child) {
    error->nearby = *child;
    error->query = child->query;
    error->registry = child->query.registry;
    error->population = child->query.population;
    error->entity = child->entity;
    error->tile_entity = child->tile_entity;
    error->batch = child->batch;
    error->load_entity = child->load_entity;
    error->activation_bytecode = child->activation_bytecode;
    error->activation_world_bytecode = child->activation_world_bytecode;
    error->load_bytecode = child->load_bytecode;
    error->load_builder_bytecode = child->load_builder_bytecode;
}

static bool rail_metadata(const rail_lifecycle_frame *frame, unsigned int pc, int32_t *out) {
    mc_world_collision_error child = mc_world_collision_empty_error();
    rail_at(frame, pc);
    const mc_world_storage_status status = mc_world_collision_block_value_held(
        frame->world, frame->x, frame->y, frame->z, true, out, &child);
    if (status == MC_WORLD_STORAGE_OK)
        return true;
    frame->error->metadata = child;
    frame->error->mutation.rail_access = child;
    rail_nearby(&frame->error->mutation, &child.nearby);
    return rail_storage(frame, pc, status, child.nearby.query.cause);
}

static bool rail_support(const rail_lifecycle_frame *frame, unsigned int pc, int32_t x, int32_t y,
                         int32_t z, bool *out) {
    mc_world_solid_support_detail child;
    rail_at(frame, pc);
    const mc_world_storage_status status =
        mc_world_solid_support_detailed_held(frame->world, x, y, z, out, &child);
    if (status == MC_WORLD_STORAGE_OK)
        return true;
    frame->error->support = child.error;
    frame->error->mutation.rail_support = child.error;
    frame->error->support_access = child.access;
    rail_nearby(&frame->error->mutation, &child.access);
    frame->error->mutation.support_bytecode = child.error.bytecode;
    frame->error->mutation.registry = child.error.registry;
    return rail_storage(frame, pc, status, child.error.cause);
}

static bool rail_power(const rail_lifecycle_frame *frame, unsigned int pc, int32_t y, bool *out) {
    mc_world_power_error child = mc_world_power_empty_error();
    rail_at(frame, pc);
    const mc_world_storage_status status =
        mc_world_power_neighbors_held(frame->world, frame->x, y, frame->z, out, &child);
    if (status == MC_WORLD_STORAGE_OK)
        return true;
    frame->error->power = child;
    frame->error->mutation.power = child;
    rail_nearby(&frame->error->mutation, &child.access);
    frame->error->mutation.registry = child.registry;
    frame->error->mutation.support_bytecode = child.support.bytecode;
    return rail_storage(frame, pc, status, child.cause);
}

static bool rail_registry(const rail_lifecycle_frame *frame, unsigned int pc,
                          const mc_registry_error *child, const char *operation) {
    rail_at(frame, pc);
    rail_cause(frame);
    frame->error->mutation.registry = *child;
    if (frame->world == NULL) {
        const mc_world_storage_status status = mc_world_runtime_registry_status(child->status);
        return rail_storage(frame, pc, status, (mc_world_storage_error){status, operation, 0});
    }
    const mc_world_storage_status status = mc_world_runtime_registry_result(
        frame->world, child->status, child, operation, &frame->error->mutation.cause);
    frame->error->mutation.status = status;
    frame->error->mutation.operation = frame->error->mutation.cause.operation;
    return false;
}

static bool rail_remote(const rail_lifecycle_frame *frame, bool *out) {
    rail_at(frame, 1);
    if (frame->world == NULL)
        return rail_fail(frame, 1, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                         "Rail World B receiver is null");
    *out = frame->world->fields.B;
    return true;
}

static bool rail_create(const rail_lifecycle_frame *frame, unsigned int new_pc,
                        unsigned int constructor_pc, mc_rail_connections **out) {
    mc_rail_connections_error child = {.mutation = mc_world_mutation_empty_error()};
    rail_at(frame, new_pc);
    const mc_world_storage_status status =
        mc_rail_connections_create(frame->block->classes, frame->block, frame->world, frame->x,
                                   frame->y, frame->z, out, &child);
    if (status == MC_WORLD_STORAGE_OK) {
        rail_at(frame, constructor_pc);
        return true;
    }
    frame->error->connections = child;
    return rail_nested(frame, child.connection.entered ? constructor_pc : new_pc, &child.mutation,
                       child.connection.entered);
}

static bool rail_shape(const rail_lifecycle_frame *frame, bool force) {
    bool remote;
    if (!rail_remote(frame, &remote))
        return false;
    if (remote)
        return true;
    mc_rail_connections *connections = NULL;
    if (!rail_create(frame, 8, 18, &connections))
        return false;
    bool completed = false;
    bool powered;
    if (!rail_power(frame, 26, frame->y, &powered))
        goto done;
    mc_rail_connections_error child = {.mutation = mc_world_mutation_empty_error()};
    rail_at(frame, 31);
    if (mc_rail_connections_reshape(connections, powered, force, &child) != MC_WORLD_STORAGE_OK) {
        frame->error->connections = child;
        rail_nested(frame, 31, &child.mutation, true);
        goto done;
    }
    completed = true;
done:
    mc_rail_connections_destroy(connections);
    return completed;
}

static bool rail_shape_call(const rail_lifecycle_frame *frame, unsigned int pc, bool force) {
    rail_at(frame, pc);
    rail_lifecycle_frame callee = *frame;
    callee.method = RAIL_SHAPE;
    if (rail_shape(&callee, force))
        return true;
    return rail_nested(frame, pc, &frame->error->mutation, true);
}

static bool rail_added(const rail_lifecycle_frame *frame) {
    bool remote;
    return rail_remote(frame, &remote) && (remote || rail_shape_call(frame, 14, true));
}

static bool rail_remove(const rail_lifecycle_frame *frame) {
    int32_t metadata;
    if (!rail_metadata(frame, 159, &metadata))
        return false;
    mc_world_mutation_error child = mc_world_mutation_empty_error();
    rail_at(frame, 162);
    if (!mc_world_mutation_drop(frame->world, frame->block, frame->x, frame->y, frame->z, metadata,
                                &child))
        return rail_nested(frame, 162, &child, true);
    child = mc_world_mutation_empty_error();
    bool ignored;
    rail_at(frame, 171);
    if (mc_world_runtime_block_set(frame->world, frame->x, frame->y, frame->z, 0, &ignored,
                                   &child) != MC_WORLD_STORAGE_OK)
        return rail_nested(frame, 171, &child, true);
    return true;
}

static bool rail_live_powered_id(const rail_lifecycle_frame *frame, int32_t *out) {
    mc_registry_error child = mc_world_mutation_empty_registry();
    const mc_block *selected = NULL;
    rail_at(frame, 182);
    if (mc_registry_block_static_get(frame->block->classes, "U", &selected, &child) !=
        MC_REGISTRY_OK)
        return rail_registry(frame, 182, &child, "Rail live U active use");
    rail_at(frame, 185);
    if (selected == NULL) {
        child.status = MC_REGISTRY_NULL_REFERENCE;
        child.message = "Rail live U bn receiver is null";
        return rail_registry(frame, 185, &child, child.message);
    }
    *out = selected->fields.bn;
    return true;
}

static bool rail_search(const rail_lifecycle_frame *frame, unsigned int pc, int32_t metadata,
                        bool forward, bool *out) {
    mc_rail_power_search_error child = {.mutation = mc_world_mutation_empty_error(),
                                        .access = mc_world_collision_empty_error(),
                                        .power = mc_world_power_empty_error()};
    rail_at(frame, pc);
    if (mc_rail_power_search_held(frame->world, frame->block, frame->x, frame->y, frame->z,
                                  metadata, forward, 0, out, &child) == MC_WORLD_STORAGE_OK)
        return true;
    frame->error->search = child;
    return rail_nested(frame, pc, &child.mutation, true);
}

static bool rail_notify(const rail_lifecycle_frame *frame, unsigned int field_pc,
                        unsigned int call_pc, int32_t y) {
    rail_at(frame, field_pc);
    const int32_t id = frame->block->fields.bn;
    mc_world_mutation_error child = mc_world_mutation_empty_error();
    rail_at(frame, call_pc);
    if (!mc_world_mutation_neighbors(frame->world, frame->x, y, frame->z, id, &child))
        return rail_nested(frame, call_pc, &child, true);
    return true;
}

static bool rail_powered_neighbor(const rail_lifecycle_frame *frame, int32_t raw, int32_t shape) {
    bool powered;
    if (!rail_power(frame, 196, frame->y, &powered))
        return false;
    if (!powered && !rail_power(frame, 209, mc_java_i32_add(frame->y, 1), &powered))
        return false;
    if (!powered && !rail_search(frame, 237, raw, true, &powered))
        return false;
    if (!powered && !rail_search(frame, 253, raw, false, &powered))
        return false;
    const bool was_powered = (raw & INT32_C(8)) != 0;
    if (powered == was_powered)
        return true;
    const unsigned int write_pc = powered ? 292u : 321u;
    const int32_t metadata = powered ? shape | INT32_C(8) : shape;
    mc_world_mutation_error child = mc_world_mutation_empty_error();
    rail_at(frame, write_pc);
    if (mc_world_runtime_metadata_set(frame->world, frame->x, frame->y, frame->z, metadata,
                                      &child) != MC_WORLD_STORAGE_OK)
        return rail_nested(frame, write_pc, &child, true);
    if (!rail_notify(frame, 340, 343, mc_java_i32_sub(frame->y, 1)))
        return false;
    if (shape >= 2 && shape <= 5 && !rail_notify(frame, 378, 381, mc_java_i32_add(frame->y, 1)))
        return false;
    return true;
}

static bool rail_source_capability(const rail_lifecycle_frame *frame, int32_t source_id,
                                   bool *out) {
    mc_registry_error child = mc_world_mutation_empty_registry();
    mc_registry_block_m_view array = {0};
    rail_at(frame, 392);
    if (mc_registry_block_m_get(frame->block->classes, &array, &child) != MC_REGISTRY_OK)
        return rail_registry(frame, 392, &child, "Rail source Block.m active use");
    const mc_block *selected = NULL;
    rail_at(frame, 397);
    if (mc_registry_block_m_at(&array, source_id, &selected, &child) != MC_REGISTRY_OK)
        return rail_registry(frame, 397, &child, "Rail source Block.m element");
    mc_world_power_error power = mc_world_power_empty_error();
    rail_at(frame, 398);
    const mc_world_storage_status status =
        mc_world_block_power_capability_held(selected, out, &power);
    if (status == MC_WORLD_STORAGE_OK)
        return true;
    frame->error->power = power;
    frame->error->mutation.power = power;
    rail_nearby(&frame->error->mutation, &power.access);
    frame->error->mutation.registry = power.registry;
    return rail_storage(frame, 398, status, power.cause);
}

static bool rail_ordinary_neighbor(const rail_lifecycle_frame *frame, int32_t source_id) {
    if (source_id <= 0)
        return true;
    bool capable;
    if (!rail_source_capability(frame, source_id, &capable))
        return false;
    if (!capable)
        return true;
    rail_at(frame, 405);
    if (frame->block->subtype.pc.a)
        return true;
    mc_rail_connections *connections = NULL;
    if (!rail_create(frame, 411, 421, &connections))
        return false;
    bool completed = false;
    mc_rail_connections_error child = {.mutation = mc_world_mutation_empty_error()};
    int32_t degree;
    rail_at(frame, 424);
    if (mc_rail_connections_degree(connections, &degree, &child) != MC_WORLD_STORAGE_OK) {
        frame->error->connections = child;
        rail_nested(frame, 424, &child.mutation, true);
        goto done;
    }
    if (degree == 3 && !rail_shape_call(frame, 438, false))
        goto done;
    completed = true;
done:
    mc_rail_connections_destroy(connections);
    return completed;
}

static bool rail_neighbor(const rail_lifecycle_frame *frame, int32_t source_id) {
    bool remote;
    if (!rail_remote(frame, &remote))
        return false;
    if (remote)
        return true;
    int32_t raw;
    if (!rail_metadata(frame, 13, &raw))
        return false;
    rail_at(frame, 23);
    const int32_t shape = frame->block->subtype.pc.a ? raw & INT32_C(7) : raw;
    bool supported;
    if (!rail_support(frame, 46, frame->x, mc_java_i32_sub(frame->y, 1), frame->z, &supported))
        return false;
    bool missing = !supported;
    /* These source tests do not short circuit after missing support below. */
    if (shape == 2) {
        if (!rail_support(frame, 68, mc_java_i32_add(frame->x, 1), frame->y, frame->z, &supported))
            return false;
        missing = missing || !supported;
    }
    if (shape == 3) {
        if (!rail_support(frame, 90, mc_java_i32_sub(frame->x, 1), frame->y, frame->z, &supported))
            return false;
        missing = missing || !supported;
    }
    if (shape == 4) {
        if (!rail_support(frame, 112, frame->x, frame->y, mc_java_i32_sub(frame->z, 1), &supported))
            return false;
        missing = missing || !supported;
    }
    if (shape == 5) {
        if (!rail_support(frame, 134, frame->x, frame->y, mc_java_i32_add(frame->z, 1), &supported))
            return false;
        missing = missing || !supported;
    }
    if (missing)
        return rail_remove(frame);
    rail_at(frame, 179);
    const int32_t id = frame->block->fields.bn;
    int32_t powered_id;
    if (!rail_live_powered_id(frame, &powered_id))
        return false;
    if (id == powered_id)
        return rail_powered_neighbor(frame, raw, shape);
    return rail_ordinary_neighbor(frame, source_id);
}

static bool rail_admit(mc_world_runtime *world, const mc_block *block,
                       mc_rail_lifecycle_error *error) {
    if (block != NULL && block->classes != NULL)
        return true;
    return mc_world_mutation_fail(world, &error->mutation, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                  "Rail invocation receiver/defining owner");
}

static bool rail_result(bool completed, const mc_rail_lifecycle_error *frame,
                        mc_rail_lifecycle_error *error) {
    if (completed) {
        mc_world_mutation_rail_error_init(error);
    } else {
        *error = *frame;
        mc_rail_lifecycle_error_publish(error);
    }
    return completed;
}

bool mc_world_mutation_rail_added(mc_world_runtime *world, const mc_block *block, int32_t x,
                                  int32_t y, int32_t z, mc_rail_lifecycle_error *error) {
    if (error == NULL)
        return false;
    mc_rail_lifecycle_error local;
    mc_world_mutation_rail_error_init(&local);
    const rail_lifecycle_frame frame = {world, block, x, y, z, RAIL_ADDED, &local};
    const bool completed = rail_admit(world, block, &local) && rail_added(&frame);
    return rail_result(completed, &local, error);
}

bool mc_world_mutation_rail_shape(mc_world_runtime *world, const mc_block *block, int32_t x,
                                  int32_t y, int32_t z, bool force,
                                  mc_rail_lifecycle_error *error) {
    if (error == NULL)
        return false;
    mc_rail_lifecycle_error local;
    mc_world_mutation_rail_error_init(&local);
    const rail_lifecycle_frame frame = {world, block, x, y, z, RAIL_SHAPE, &local};
    const bool completed = rail_admit(world, block, &local) && rail_shape(&frame, force);
    return rail_result(completed, &local, error);
}

bool mc_world_mutation_rail_neighbor(mc_world_runtime *world, const mc_block *block, int32_t x,
                                     int32_t y, int32_t z, int32_t source_id,
                                     mc_rail_lifecycle_error *error) {
    if (error == NULL)
        return false;
    mc_rail_lifecycle_error local;
    mc_world_mutation_rail_error_init(&local);
    const rail_lifecycle_frame frame = {world, block, x, y, z, RAIL_NEIGHBOR, &local};
    const bool completed = rail_admit(world, block, &local) && rail_neighbor(&frame, source_id);
    return rail_result(completed, &local, error);
}
