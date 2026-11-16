#include "mutation_internal.h"
#include "world_execution_internal.h"

#include "../../../block/registry/registry_internal.h"
#include "../../../block/registry/runtime_internal.h"
#include "../../storage/objects_internal.h"
#include "../blocks_execution_internal.h"
#include "../construction_internal.h"
#include "../execution_children_internal.h"
#include "../plants_execution_internal.h"
#include "../registry_execution_internal.h"
#include "../scheduling_execution_internal.h"
#include "../solid_support_execution_internal.h"
#include "../tile_entities/execution_internal.h"
#include "chunk_execution_internal.h"
#include "drops/execution_internal.h"
#include "mc/core/java/java.h"
#include "mc/world/runtime/blocks.h"

#include <stdlib.h>
#include <string.h>

typedef struct mutation_presentation {
    mc_world_runtime *world;
    mc_world_presentation_execution frame;
    mc_world_mutation_error error;
    mc_world_mutation_chunk_scope scope;
    mc_world_storage_status status;
    mc_java_random_profile_status math_status;
    bool invoking, returned, completed, particle;
} mutation_presentation;

struct mc_world_mutation_execution_call {
    mc_world_mutation_execution_context context;
    mc_world_access_failure_lease *lease;
    mc_world_mutation_error error;
    mc_world_block_execution scalar;
    mc_world_notification_execution observer;
    mc_world_runtime_acquisition_error acquisition;
    mc_world_mutation_chunk_execution *chunk;
    mc_world_mutation_execution_call *child;
    mc_world_mutation_drop_execution *drop;
    mc_world_solid_support_execution *support;
    mc_world_plant_execution *plant;
    mc_world_schedule_execution *schedule;
    mc_world_material_execution *material;
    mc_live_tile_entity_error *tile_added;
    mutation_presentation *presentation;
    mc_world_storage_status support_status;
    mc_world_storage_status plant_status;
    mc_world_storage_status schedule_status;
    mc_world_storage_status material_status;
    mc_world_storage_status tile_added_status;
    bool active, close_done, support_returned, plant_returned, tile_added_completed;
    bool tile_added_source_null;
};

static const mc_entity_tick_execution_site metadata_site = {"fd", "d", "(IIII)V", 0};
static const mc_entity_tick_execution_site block_site = {"fd", "f", "(IIII)Z", 0};

static bool records_quiet(const mc_world_runtime_execution_result *result) {
    if (result->root_pending)
        return false;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_LOADERS; ++i)
        if (result->pending[i].used)
            return false;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_WORLDS; ++i)
        if (result->closes[i].used)
            return false;
    return true;
}

static bool same_world(const mc_entity_tick_execution_world_token *a,
                       const mc_entity_tick_execution_world_token *b) {
    return a->owner == b->owner && a->slot == b->slot && a->generation == b->generation;
}

static bool current(const mc_world_mutation_execution_context *context, mc_world_runtime *world,
                    mc_world_access_failure_lease **expected_lease,
                    mc_entity_tick_execution_site site) {
    mc_world_runtime_execution_result *result = context->result;
    if (result->ownership_status != MC_WORLD_STORAGE_OK)
        return false;
    if (result->selection_ready || result->math_failed || !records_quiet(result)) {
        mc_world_runtime_execution_result_note(
            result, MC_WORLD_STORAGE_BUSY, "World mutation canonical result still in use", site);
        return false;
    }
    mc_entity_tick_execution_world_token token;
    if (!mc_entity_tick_execution_current_world(context->execution, &token)) {
        mc_world_runtime_execution_result_note(result, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                               "World mutation checked current scope", site);
        return false;
    }
    if (!same_world(&context->world, &token)) {
        mc_world_runtime_execution_result_note(result, MC_WORLD_STORAGE_BUSY,
                                               "World mutation exact evaluated scope", site);
        return false;
    }
    mc_world_access_failure_lease *lease = NULL;
    const mc_world_storage_status status =
        mc_world_execution_child_lease(context->execution, world, &lease);
    mc_world_runtime_execution_result_note(result, status, "World mutation checked World lease",
                                           site);
    if (status != MC_WORLD_STORAGE_OK)
        return false;
    if (*expected_lease != NULL && *expected_lease != lease) {
        mc_world_runtime_execution_result_note(result, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                               "World mutation exact producer lease changed", site);
        return false;
    }
    *expected_lease = lease;
    return true;
}

mc_registry_classes *
mc_world_mutation_execution_registry(const mc_world_mutation_execution_call *call) {
    return call->context.code_classes->registry;
}

mc_world_runtime_execution_result *
mc_world_mutation_execution_result_of(const mc_world_mutation_execution_call *call) {
    return call == NULL ? NULL : call->context.result;
}

bool mc_world_mutation_execution_current(const mc_world_mutation_execution_call *call,
                                         mc_world_runtime *world,
                                         mc_entity_tick_execution_site site) {
    if (call == NULL)
        return false;
    mc_world_access_failure_lease *lease = call->lease;
    return current(&call->context, world, &lease, site);
}

bool mc_world_mutation_execution_material(mc_world_mutation_execution_call *call,
                                          mc_world_runtime *world, int32_t x, int32_t y, int32_t z,
                                          mc_material **out, mc_world_mutation_error *error,
                                          mc_entity_tick_execution_site site) {
    if (!mc_world_mutation_execution_current(call, world, site))
        return false;
    if (out == NULL || error == NULL || (call->material != NULL && !call->material->completed)) {
        mc_world_runtime_execution_result_note(call->context.result, MC_WORLD_STORAGE_BUSY,
                                               "Material requires fresh retained work", site);
        return false;
    }
    if (call->material == NULL)
        call->material = calloc(1, sizeof(*call->material));
    if (call->material == NULL) {
        mc_world_runtime_execution_result_note(call->context.result,
                                               MC_WORLD_STORAGE_ALLOCATION_FAILED,
                                               "Material retained frame allocation", site);
        return false;
    }
    mc_world_material_execution_init(call->material, call->context.execution,
                                     call->context.code_classes, call->context.code, site,
                                     call->context.result);
    const bool completed = mc_world_runtime_material_get_retained(call->material, world, x, y, z,
                                                                  out, &call->material_status);
    if (completed)
        return true;
    if (call->material->native_stopped ||
        call->context.result->ownership_status != MC_WORLD_STORAGE_OK)
        return false;
    error->material = call->material->detail;
    error->registry = call->material->detail.registry;
    error->status = call->material->detail.status;
    error->operation = call->material->detail.operation;
    error->cause = call->material->detail.cause;
    return false;
}

static mc_world_storage_status presentation_note(mc_world_mutation_execution_call *call,
                                                 mc_world_storage_status status,
                                                 const char *operation,
                                                 mc_entity_tick_execution_site site) {
    mc_world_runtime_execution_result_note(call->context.result, status, operation, site);
    return status;
}

static bool presentation_phase(const mc_world_mutation_execution_call *call,
                               mc_entity_tick_execution_world_slot_phase *out) {
    const mutation_presentation *p = call->presentation;
    const mc_entity_tick_execution *execution = call->context.execution;
    const mc_entity_tick_execution_world_token *token = &p->scope.token;
    if (!p->scope.created || token->owner != execution || token->generation == 0 ||
        token->slot >= MC_ENTITY_TICK_EXECUTION_WORLDS ||
        execution->world_slots[token->slot].generation != token->generation)
        return false;
    *out = execution->world_slots[token->slot].phase;
    return true;
}

static mc_world_storage_status presentation_pop(mc_world_mutation_execution_call *call) {
    mutation_presentation *p = call->presentation;
    const mc_world_storage_status status = mc_entity_tick_execution_pop_world(
        call->context.execution, &p->scope.token, p->scope.failed);
    mc_entity_tick_execution_world_slot_phase phase;
    p->scope.consumed =
        presentation_phase(call, &phase) && (phase == MC_ENTITY_TICK_EXECUTION_WORLD_FREE ||
                                             phase == MC_ENTITY_TICK_EXECUTION_WORLD_RETAINED);
    (void)presentation_note(call, status, "Presentation checked World close", p->scope.site);
    if (status == MC_WORLD_STORAGE_OK && !p->scope.consumed)
        return presentation_note(call, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                 "Presentation checked World close consumption", p->scope.site);
    return status;
}

static bool presentation_current(mc_world_mutation_execution_call *call,
                                 mc_entity_tick_execution_site site) {
    if (call == NULL)
        return false;
    mutation_presentation *p = call->presentation;
    if (p == NULL || !p->scope.created || p->scope.attempted || p->invoking || p->returned) {
        (void)presentation_note(call, MC_WORLD_STORAGE_BUSY,
                                "Presentation requires its entered argument frame", site);
        return false;
    }
    mc_world_mutation_execution_context context = call->context;
    context.world = p->scope.token;
    return current(&context, p->world, &p->frame.lease, site);
}

bool mc_world_mutation_execution_presentation_begin(mc_world_mutation_execution_call *call,
                                                    mc_world_runtime *world,
                                                    mc_world_mutation_error *error,
                                                    mc_entity_tick_execution_site site) {
    if (!mc_world_mutation_execution_current(call, world, site))
        return false;
    mutation_presentation *p = call->presentation;
    if (error == NULL || (p != NULL && (!p->completed || !p->scope.consumed))) {
        (void)presentation_note(call, MC_WORLD_STORAGE_BUSY,
                                "Presentation prior invocation remains retained", site);
        return false;
    }
    if (p == NULL) {
        p = calloc(1, sizeof(*p));
        if (p == NULL) {
            (void)presentation_note(call, MC_WORLD_STORAGE_ALLOCATION_FAILED,
                                    "Presentation retained invocation allocation", site);
            return false;
        }
        call->presentation = p;
    }
    mc_entity_tick_execution_world_token token;
    const mc_world_storage_status status =
        mc_entity_tick_execution_push_world(call->context.execution, world, site, &token);
    if (presentation_note(call, status, "Presentation actual World operand capture", site) !=
        MC_WORLD_STORAGE_OK)
        return false;
    memset(p, 0, sizeof(*p));
    p->world = world;
    p->frame.execution = call->context.execution;
    p->frame.owned = call->context.result;
    p->frame.receiver_site = site;
    p->scope = (mc_world_mutation_chunk_scope){.token = token, .site = site, .created = true};
    mc_world_mutation_error_init(&p->error);
    return true;
}

bool mc_world_mutation_execution_literal(mc_world_mutation_execution_call *call,
                                         mc_registry_classes *classes, const char *text,
                                         const mc_registry_string **out,
                                         mc_world_mutation_error *error,
                                         mc_entity_tick_execution_site site) {
    if (!presentation_current(call, site))
        return false;
    if (classes != call->context.code_classes->registry || classes == NULL || text == NULL ||
        out == NULL || error == NULL) {
        (void)presentation_note(call, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                "Presentation literal defining Registry graph", site);
        return false;
    }
    mutation_presentation *p = call->presentation;
    mc_registry_error registry = mc_world_mutation_empty_registry();
    const mc_registry_string *literal;
    const mc_registry_status status =
        mc_registry_runtime_literal_ascii(classes, text, &literal, &registry);
    if (status != MC_REGISTRY_OK) {
        if (p->world != NULL)
            (void)mc_world_mutation_registry_fail(p->world, error, &registry,
                                                  "Fluid source literal resolution");
        else {
            error->registry = registry;
            error->status = mc_world_runtime_registry_status(status);
            error->operation = "Fluid source literal resolution";
            error->cause = (mc_world_storage_error){error->status, error->operation, 0};
        }
        p->error = *error;
        return false;
    }
    if (!mc_registry_runtime_literal_owned(classes, literal)) {
        (void)presentation_note(call, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                "Presentation actual literal pool ownership", site);
        return false;
    }
    *out = literal;
    return true;
}

bool mc_world_mutation_execution_math(mc_world_mutation_execution_call *call,
                                      mc_java_random_profile *profile, double *out,
                                      mc_world_mutation_error *error,
                                      mc_entity_tick_execution_site site) {
    if (!presentation_current(call, site))
        return false;
    mc_world_runtime_classes *classes = call->context.code_classes;
    if (profile == NULL || profile != classes->profile || classes->registry == NULL ||
        profile != classes->registry->options.random_profile || out == NULL || error == NULL) {
        (void)presentation_note(call, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                "Presentation actual Math.random profile graph", site);
        return false;
    }
    mutation_presentation *p = call->presentation;
    double value;
    p->math_status = mc_java_math_random(profile, &value);
    if (p->world != NULL)
        p->world->last_random_status = p->math_status;
    if (p->math_status != MC_JAVA_RANDOM_PROFILE_OK) {
        const mc_world_storage_status mapped =
            p->math_status == MC_JAVA_RANDOM_PROFILE_LIMIT ? MC_WORLD_STORAGE_LIMIT
            : p->math_status == MC_JAVA_RANDOM_PROFILE_ALLOCATION_FAILED
                ? MC_WORLD_STORAGE_ALLOCATION_FAILED
            : p->math_status == MC_JAVA_RANDOM_PROFILE_INVALID_ARGUMENT
                ? MC_WORLD_STORAGE_INVALID_ARGUMENT
                : MC_WORLD_STORAGE_UNSUPPORTED_STATE;
        (void)mc_world_mutation_fail(p->world, error, mapped, "Fluid actual Math.random");
        p->error = *error;
        (void)presentation_note(call, mapped, "Fluid actual Math.random", site);
        return false;
    }
    if (!presentation_current(call, site))
        return false;
    *out = value;
    return true;
}

static bool presentation_finish(mc_world_mutation_execution_call *call, bool completed,
                                mc_world_mutation_error *error,
                                mc_entity_tick_execution_site site) {
    mutation_presentation *p = call->presentation;
    p->invoking = false;
    p->returned = true;
    if (!completed)
        *error = p->error;
    if (completed && (call->context.result->ownership_status != MC_WORLD_STORAGE_OK ||
                      call->context.result->selection_ready || call->context.result->math_failed ||
                      !records_quiet(call->context.result))) {
        (void)presentation_note(call, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                "Presentation completed child result not quiet", site);
        completed = false;
    }
    p->scope.site = site;
    p->scope.attempted = true;
    p->scope.failed = !completed;
    p->scope.initial_close_status = presentation_pop(call);
    p->completed = completed && p->scope.initial_close_status == MC_WORLD_STORAGE_OK;
    return p->completed;
}

bool mc_world_mutation_execution_sound(mc_world_mutation_execution_call *call,
                                       mc_world_runtime *world, double x, double y, double z,
                                       const mc_registry_string *name, float volume, float pitch,
                                       mc_world_mutation_error *error,
                                       mc_entity_tick_execution_site site) {
    if (!presentation_current(call, site))
        return false;
    mutation_presentation *p = call->presentation;
    if (world != p->world || error == NULL) {
        (void)presentation_note(call, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                                "Presentation exact sound operand and output", site);
        return false;
    }
    p->invoking = true;
    p->frame.receiver_site = site;
    const bool completed = mc_world_runtime_sound_in_execution(
        &p->frame, world, x, y, z, name, volume, pitch, &p->error, &p->status);
    return presentation_finish(call, completed, error, site);
}

bool mc_world_mutation_execution_particle(mc_world_mutation_execution_call *call,
                                          mc_world_runtime *world, const mc_registry_string *name,
                                          double x, double y, double z, double vx, double vy,
                                          double vz, mc_world_mutation_error *error,
                                          mc_entity_tick_execution_site site) {
    if (!presentation_current(call, site))
        return false;
    mutation_presentation *p = call->presentation;
    if (world != p->world || error == NULL) {
        (void)presentation_note(call, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                                "Presentation exact particle operand and output", site);
        return false;
    }
    p->particle = true;
    p->invoking = true;
    p->frame.receiver_site = site;
    const bool completed = mc_world_runtime_particle_frame_in_execution(
        &p->frame, world, name, x, y, z, vx, vy, vz, &p->error, &p->status);
    return presentation_finish(call, completed, error, site);
}

mc_world_storage_status
mc_world_mutation_execution_presentation_abort(mc_world_mutation_execution_call *call) {
    if (call == NULL || call->presentation == NULL)
        return MC_WORLD_STORAGE_OK;
    mutation_presentation *p = call->presentation;
    if (p->invoking)
        return MC_WORLD_STORAGE_BUSY;
    if (p->scope.consumed)
        return MC_WORLD_STORAGE_OK;
    if (p->scope.attempted)
        return p->scope.initial_close_status;
    p->scope.attempted = true;
    p->scope.failed = true;
    p->scope.initial_close_status = presentation_pop(call);
    return p->scope.initial_close_status;
}

static mc_world_storage_status presentation_native_close(mc_world_mutation_execution_call *call) {
    mutation_presentation *p = call->presentation;
    if (p == NULL || !p->scope.created || p->scope.consumed)
        return MC_WORLD_STORAGE_OK;
    if (p->invoking || !p->scope.attempted)
        return presentation_note(call, MC_WORLD_STORAGE_BUSY,
                                 "Presentation native close lacks recorded return", p->scope.site);
    mc_entity_tick_execution_world_slot_phase phase;
    if (!presentation_phase(call, &phase))
        return presentation_note(call, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                 "Presentation native close token identity", p->scope.site);
    if (phase == MC_ENTITY_TICK_EXECUTION_WORLD_FREE ||
        phase == MC_ENTITY_TICK_EXECUTION_WORLD_RETAINED) {
        p->scope.consumed = true;
        return MC_WORLD_STORAGE_OK;
    }
    mc_entity_tick_execution_world_token token;
    if (phase != MC_ENTITY_TICK_EXECUTION_WORLD_OPEN ||
        !mc_entity_tick_execution_current_world(call->context.execution, &token) ||
        !same_world(&p->scope.token, &token))
        return presentation_note(call, MC_WORLD_STORAGE_BUSY,
                                 "Presentation native close awaits exact current token",
                                 p->scope.site);
    return presentation_pop(call);
}

static bool in_domain(int32_t x, int32_t y, int32_t z) {
    return x >= -32000000 && x < 32000000 && z >= -32000000 && z <= 32000000 && y >= 0 && y < 128;
}

/* Ordinary wrappers project canonical success only at their returned output.
 * Retained calls still reset their own frame before raw/projection/retirement. */
typedef enum mutation_success_diagnostic {
    MUTATION_SUCCESS_PROJECT_CALLER,
    MUTATION_SUCCESS_RESET_RETAINED
} mutation_success_diagnostic;

static mc_world_storage_status result(mc_world_mutation_error *out, mc_world_mutation_error *frame,
                                      bool completed) {
    const mc_world_storage_status status = completed ? MC_WORLD_STORAGE_OK : frame->status;
    if (out != NULL) {
        if (completed)
            mc_world_mutation_error_init(out);
        else
            *out = *frame;
    }
    free(frame);
    return status;
}

static bool storage_fail(mc_world_runtime *world, mc_world_mutation_error *frame,
                         mc_world_storage_error cause) {
    frame->status = cause.status;
    frame->operation = cause.operation;
    frame->cause = cause;
    world->last_error = cause;
    return false;
}

static mc_world_storage_status admit(mc_world_runtime *world, bool has_out,
                                     mc_world_mutation_error *error) {
    mc_world_storage_status status = MC_WORLD_STORAGE_OK;
    const char *operation = NULL;
    if (world == NULL || !has_out) {
        status = MC_WORLD_STORAGE_INVALID_ARGUMENT;
        operation = "World mutation arguments";
    } else if (world->construction.mode != MC_WORLD_RUNTIME_GAME ||
               world->construction.phase == MC_WORLD_RUNTIME_RETIRED) {
        status = MC_WORLD_STORAGE_UNSUPPORTED_STATE;
        operation = "World mutation phase";
    }
    /* Native admission changes only the returned diagnostic. */
    if (status != MC_WORLD_STORAGE_OK && error != NULL) {
        mc_world_mutation_error_init(error);
        error->status = status;
        error->operation = operation;
        error->cause = (mc_world_storage_error){status, operation, 0};
    }
    return status;
}

/* Diagnostic workspaces must not accumulate on the C stack during synchronous
 * neighbor recursion. Allocation is checked before any source mutation; each
 * invocation retains its own complete diagnostic until its descendants return. */
static mc_world_mutation_error *workspace(mc_world_runtime *world, mc_world_mutation_error *error) {
    mc_world_mutation_error *frame = malloc(sizeof(*frame));
    if (frame == NULL) {
        const mc_world_storage_error cause = {MC_WORLD_STORAGE_ALLOCATION_FAILED,
                                              "World mutation diagnostic allocation", 0};
        mc_world_mutation_error_init(error);
        if (error != NULL) {
            error->status = cause.status;
            error->operation = cause.operation;
            error->cause = cause;
        }
        world->last_error = cause;
    } else {
        mc_world_mutation_error_init(frame);
    }
    return frame;
}

/* Return before a concrete setter can recurse, so acquisition diagnostics do
 * not remain in every ancestor's native stack frame. */
static bool acquisition_failure(mc_world_runtime *world, mc_chunk_acquire_status acquired,
                                const mc_world_runtime_acquisition_error *detail,
                                mc_world_mutation_error *frame) {
    frame->tile_entity = detail->tile_entity;
    frame->registry = detail->registry;
    frame->batch = detail->batch;
    frame->activation_bytecode = detail->activation_bytecode;
    frame->activation_world_bytecode = detail->activation_world_bytecode;
    frame->load_bytecode = detail->load_bytecode;
    frame->load_builder_bytecode = detail->load_builder_bytecode;
    frame->load_entity = detail->load_entity;
    frame->population = detail->population;
    if (detail->cause.status != MC_WORLD_STORAGE_OK)
        return storage_fail(world, frame, detail->cause);
    if (acquired == MC_CHUNK_ACQUIRE_LIMIT || acquired == MC_CHUNK_ACQUIRE_PARTIAL_LIMIT)
        return mc_world_mutation_fail(world, frame, MC_WORLD_STORAGE_LIMIT,
                                      "World mutation acquisition limit");
    return mc_world_mutation_fail(world, frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                  "World mutation acquisition");
}

static bool acquire(mc_world_runtime *world, int32_t x, int32_t z, mc_chunk_state **out,
                    mc_world_mutation_error *frame, bool with_metadata,
                    mc_world_mutation_execution_call *owned) {
    frame->bytecode = 48;
    if (world->fields.v == NULL) {
        frame->provider_bytecode = 6;
        return mc_world_mutation_fail(world, frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                      "World mutation provider receiver");
    }
    if (owned != NULL) {
        const mc_entity_tick_execution_site site = {"fd", with_metadata ? "a" : "c",
                                                    with_metadata ? "(IIIII)Z" : "(IIII)Z", 48};
        if (!mc_world_mutation_execution_current(owned, world, site))
            return false;
        mc_world_runtime_execution_result *result = owned->context.result;
        const mc_chunk_acquire_status status = mc_world_runtime_get_failure_in_execution(
            owned->context.execution, world, owned->context.code, mc_java_i32_shr(x, 4),
            mc_java_i32_shr(z, 4), out, &owned->acquisition, result);
        if (status != MC_CHUNK_ACQUIRE_OK)
            return acquisition_failure(world, status, &owned->acquisition, frame);
        if (result->ownership_status != MC_WORLD_STORAGE_OK)
            return false;
        if (!result->selection_ready || result->selection.status != MC_CHUNK_ACQUIRE_OK ||
            !records_quiet(result)) {
            mc_world_runtime_execution_result_note(result, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                                   "World mutation completed GET outcome", site);
            return false;
        }
        result->selection_ready = false;
        return true;
    }
    mc_world_runtime_acquisition_error detail;
    const mc_chunk_acquire_status acquired = mc_world_runtime_get_failure(
        world, mc_java_i32_shr(x, 4), mc_java_i32_shr(z, 4), out, &detail);
    if (acquired == MC_CHUNK_ACQUIRE_OK)
        return true;
    return acquisition_failure(world, acquired, &detail, frame);
}

/* Enclosing caller holds the initiating World. Acquiring c(II) is still reached
 * even when the concrete setter will return false. The captured Chunk remains
 * leased across all synchronous descendants of that one concrete invocation. */
static bool quiet(mc_world_runtime *world, int32_t x, int32_t y, int32_t z, int32_t id,
                  bool with_metadata, int32_t metadata, bool *out, mc_world_mutation_error *frame,
                  mc_world_mutation_execution_call *owned) {
    mc_chunk_state *state = NULL;
    if (!acquire(world, x, z, &state, frame, with_metadata, owned))
        return false;
    frame->bytecode = with_metadata ? 68u : 66u;
    if (state->blank) {
        *out = true; /* Actual Blank override returns true without mutation. */
        frame->bytecode = 0;
        return true;
    }

    mc_world_store *store = world->options.store;
    mc_world_chunk *chunk = mc_world_store_chunk_object(store, state);
    const int32_t local_x = (int32_t)((uint32_t)x & 15u);
    const int32_t local_z = (int32_t)((uint32_t)z & 15u);
    if (owned != NULL) {
        const mc_entity_tick_execution_site site = {"fd", with_metadata ? "a" : "c",
                                                    with_metadata ? "(IIIII)Z" : "(IIII)Z",
                                                    with_metadata ? 68 : 66};
        if (chunk == NULL) {
            mc_world_runtime_execution_result_note(owned->context.result,
                                                   MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                                   "World mutation selected Chunk identity", site);
            return false;
        }
        owned->chunk = calloc(1, sizeof(*owned->chunk));
        if (owned->chunk == NULL) {
            mc_world_runtime_execution_result_note(owned->context.result,
                                                   MC_WORLD_STORAGE_ALLOCATION_FAILED,
                                                   "World retained Chunk mutation frame", site);
            return false;
        }
        mc_world_mutation_chunk_execution_init(owned->chunk, owned->context.execution,
                                               owned->context.code, owned->context.code_classes,
                                               mc_world_mutation_execution_registry(owned), site,
                                               owned->context.world, owned->context.result);
        mc_world_storage_status raw = MC_WORLD_STORAGE_OK;
        const bool completed = with_metadata
                                   ? mc_world_mutation_chunk_block_metadata_in_execution(
                                         owned->chunk, world, store, state, chunk, local_x, y,
                                         local_z, id, metadata, out, frame, &raw)
                                   : mc_world_mutation_chunk_block_in_execution(
                                         owned->chunk, world, store, state, chunk, local_x, y,
                                         local_z, id, out, frame, &raw);
        if (completed)
            frame->bytecode = 0;
        else if (raw != MC_WORLD_STORAGE_OK)
            world->last_error = frame->cause;
        return completed;
    }
    mc_world_chunk_fields fields;
    if (chunk == NULL || mc_world_chunk_fields_get(chunk, &fields) != MC_WORLD_STORAGE_OK ||
        fields.d == NULL || fields.d->construction.mode != MC_WORLD_RUNTIME_GAME ||
        fields.d->construction.phase == MC_WORLD_RUNTIME_RETIRED || fields.d->fields.w != store)
        return mc_world_mutation_fail(world, frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                      "World mutation concrete Chunk binding");

    mc_world_storage_error cause = {0};
    if (mc_world_runtime_call_enter(fields.d, &cause) != MC_WORLD_STORAGE_OK)
        return storage_fail(world, frame, cause);
    if (mc_world_store_lease_chunk(store, state, &cause) != MC_WORLD_STORAGE_OK) {
        mc_world_runtime_call_leave(fields.d);
        return storage_fail(world, frame, cause);
    }
    const bool completed =
        with_metadata ? mc_world_mutation_chunk_block_metadata(chunk, local_x, y, local_z, id,
                                                               metadata, out, frame)
                      : mc_world_mutation_chunk_block(chunk, local_x, y, local_z, id, out, frame);
    (void)mc_world_store_release_chunk(store, state, NULL);
    mc_world_runtime_call_leave(fields.d);
    if (!completed)
        world->last_error = frame->cause;
    else
        frame->bytecode = 0;
    return completed;
}

static bool enter(mc_world_runtime *world, mc_world_mutation_error *frame,
                  mc_world_mutation_execution_call *owned, mc_entity_tick_execution_site site) {
    mc_world_storage_error cause = {0};
    const mc_world_storage_status status = mc_world_runtime_call_enter(world, &cause);
    if (status != MC_WORLD_STORAGE_OK) {
        if (owned != NULL) {
            mc_world_runtime_execution_result_note(owned->context.result, status, cause.operation,
                                                   site);
            return false;
        }
        frame->status = cause.status;
        frame->operation = cause.operation;
        frame->cause = cause;
        return false;
    }
    return true;
}

static bool notify_cell(mc_world_runtime *world, int32_t x, int32_t y, int32_t z,
                        mc_world_mutation_error *frame, mc_world_mutation_execution_call *owned) {
    if (owned == NULL)
        return mc_world_mutation_observe_cell(world, x, y, z, frame);
    const mc_entity_tick_execution_site site = {"fd", "g", "(IIII)V", 4};
    if (!mc_world_mutation_execution_current(owned, world, site))
        return false;
    mc_world_notification_execution_init_borrowed(&owned->observer, owned->context.execution, site,
                                                  owned->context.result);
    mc_world_storage_status raw = MC_WORLD_STORAGE_OK;
    return mc_world_mutation_observe_cell_in_execution(&owned->observer, world, x, y, z, frame,
                                                       &raw);
}

static bool neighbors(mc_world_runtime *world, int32_t x, int32_t y, int32_t z, int32_t id,
                      mc_world_mutation_error *frame, mc_world_mutation_execution_call *owned) {
    return owned == NULL
               ? mc_world_mutation_neighbors(world, x, y, z, id, frame)
               : mc_world_mutation_neighbors_in_execution(owned, world, x, y, z, id, frame);
}

/* fd.g invokes the cell observer at 4, then ordered neighbors at 13. */
static bool notify_and_neighbors(mc_world_runtime *world, int32_t x, int32_t y, int32_t z,
                                 int32_t id, mc_world_mutation_error *frame,
                                 mc_world_mutation_execution_call *owned) {
    return notify_cell(world, x, y, z, frame, owned) && neighbors(world, x, y, z, id, frame, owned);
}

static bool set_body(mc_world_runtime *world, int32_t x, int32_t y, int32_t z, int32_t id,
                     bool with_metadata, int32_t metadata, bool notify, bool *out,
                     mc_world_mutation_error *frame, mc_world_mutation_execution_call *owned,
                     mutation_success_diagnostic success_diagnostic) {
    const mc_entity_tick_execution_site site = {
        "fd", with_metadata ? (notify ? "b" : "a") : (notify ? "f" : "c"),
        with_metadata ? "(IIIII)Z" : "(IIII)Z", 0};
    if (!enter(world, frame, owned, site))
        return false;
    bool changed = false;
    if (notify)
        frame->wrapper_bytecode = with_metadata ? 8u : 6u;
    bool completed = quiet(world, x, y, z, id, with_metadata, metadata, &changed, frame, owned);
    if (completed && changed && notify) {
        frame->wrapper_bytecode = with_metadata ? 20u : 18u;
        completed = notify_and_neighbors(world, x, y, z, id, frame, owned);
    }
    if (completed) {
        *out = changed;
        if (success_diagnostic == MUTATION_SUCCESS_RESET_RETAINED)
            mc_world_mutation_error_init(frame);
    }
    mc_world_runtime_call_leave(world);
    return completed;
}

static mc_world_storage_status set(mc_world_runtime *world, int32_t x, int32_t y, int32_t z,
                                   int32_t id, bool with_metadata, int32_t metadata, bool notify,
                                   bool *out, mc_world_mutation_error *error) {
    const mc_world_storage_status admission = admit(world, out != NULL, error);
    if (admission != MC_WORLD_STORAGE_OK)
        return admission;
    if (!in_domain(x, y, z)) {
        *out = false;
        mc_world_mutation_error_init(error);
        return MC_WORLD_STORAGE_OK;
    }
    mc_world_mutation_error *frame = workspace(world, error);
    if (frame == NULL)
        return MC_WORLD_STORAGE_ALLOCATION_FAILED;
    const bool completed = set_body(world, x, y, z, id, with_metadata, metadata, notify, out, frame,
                                    NULL, MUTATION_SUCCESS_PROJECT_CALLER);
    return result(error, frame, completed);
}

mc_world_storage_status mc_world_runtime_block_set_quiet(mc_world_runtime *world, int32_t x,
                                                         int32_t y, int32_t z, int32_t id,
                                                         bool *out,
                                                         mc_world_mutation_error *error) {
    return set(world, x, y, z, id, false, 0, false, out, error);
}

mc_world_storage_status mc_world_runtime_block_metadata_set_quiet(mc_world_runtime *world,
                                                                  int32_t x, int32_t y, int32_t z,
                                                                  int32_t id, int32_t metadata,
                                                                  bool *out,
                                                                  mc_world_mutation_error *error) {
    return set(world, x, y, z, id, true, metadata, false, out, error);
}

mc_world_storage_status mc_world_runtime_block_set(mc_world_runtime *world, int32_t x, int32_t y,
                                                   int32_t z, int32_t id, bool *out,
                                                   mc_world_mutation_error *error) {
    return set(world, x, y, z, id, false, 0, true, out, error);
}

mc_world_storage_status mc_world_runtime_block_metadata_set(mc_world_runtime *world, int32_t x,
                                                            int32_t y, int32_t z, int32_t id,
                                                            int32_t metadata, bool *out,
                                                            mc_world_mutation_error *error) {
    return set(world, x, y, z, id, true, metadata, true, out, error);
}

static bool scalar_result(mc_world_mutation_execution_call *owned, mc_world_runtime *world,
                          bool completed, mc_world_storage_status raw, mc_world_storage_error cause,
                          mc_world_mutation_error *frame) {
    if (completed)
        return true;
    if (owned->scalar.get_returned && owned->scalar.get_status != MC_CHUNK_ACQUIRE_OK)
        return acquisition_failure(world, owned->scalar.get_status, &owned->scalar.acquisition,
                                   frame);
    if (raw != MC_WORLD_STORAGE_OK)
        return storage_fail(world, frame, cause);
    return false;
}

static void prepare_scalar(mc_world_mutation_execution_call *owned,
                           mc_entity_tick_execution_site site) {
    owned->scalar = (mc_world_block_execution){.execution = owned->context.execution,
                                               .code = owned->context.code,
                                               .receiver_site = site,
                                               .owned = owned->context.result};
}

bool mc_world_mutation_execution_get(mc_world_mutation_execution_call *owned,
                                     mc_world_runtime *world, int32_t x, int32_t y, int32_t z,
                                     bool metadata, int32_t *out, mc_world_mutation_error *frame,
                                     mc_entity_tick_execution_site site) {
    if (!mc_world_mutation_execution_current(owned, world, site))
        return false;
    prepare_scalar(owned, site);
    mc_world_storage_error cause = {0};
    mc_world_storage_status raw = MC_WORLD_STORAGE_OK;
    const bool completed = metadata ? mc_world_runtime_metadata_get_in_execution(
                                          &owned->scalar, world, x, y, z, out, &cause, &raw)
                                    : mc_world_runtime_block_get_in_execution(
                                          &owned->scalar, world, x, y, z, out, &cause, &raw);
    return scalar_result(owned, world, completed, raw, cause, frame);
}

static bool metadata_quiet(mc_world_runtime *world, int32_t x, int32_t y, int32_t z,
                           int32_t metadata, bool *changed, mc_world_mutation_error *frame,
                           mc_world_mutation_execution_call *owned) {
    mc_world_storage_error cause = {0};
    if (owned == NULL) {
        const mc_world_storage_status raw =
            mc_world_runtime_metadata_set_quiet(world, x, y, z, metadata, changed, &cause);
        return raw == MC_WORLD_STORAGE_OK || storage_fail(world, frame, cause);
    }
    const mc_entity_tick_execution_site site = {"fd", "d", "(IIII)V", 6};
    if (!mc_world_mutation_execution_current(owned, world, site))
        return false;
    prepare_scalar(owned, site);
    mc_world_storage_status raw = MC_WORLD_STORAGE_OK;
    const bool completed = mc_world_runtime_metadata_set_quiet_in_execution(
        &owned->scalar, world, x, y, z, metadata, changed, &cause, &raw);
    return scalar_result(owned, world, completed, raw, cause, frame);
}

static bool metadata_body(mc_world_runtime *world, int32_t x, int32_t y, int32_t z,
                          int32_t metadata, mc_world_mutation_error *frame,
                          mc_world_mutation_execution_call *owned,
                          mutation_success_diagnostic success_diagnostic) {
    if (!enter(world, frame, owned, metadata_site))
        return false;
    bool completed = false;
    bool changed;
    frame->wrapper_bytecode = 6;
    if (!metadata_quiet(world, x, y, z, metadata, &changed, frame, owned))
        goto leave;
    if (changed) {
        int32_t id;
        frame->wrapper_bytecode = 16;
        if (owned == NULL) {
            mc_world_storage_error cause = {0};
            if (mc_world_runtime_block_get(world, x, y, z, &id, &cause) != MC_WORLD_STORAGE_OK) {
                storage_fail(world, frame, cause);
                goto leave;
            }
        } else {
            const mc_entity_tick_execution_site site = {"fd", "d", "(IIII)V", 16};
            if (!mc_world_mutation_execution_get(owned, world, x, y, z, false, &id, frame, site))
                goto leave;
        }
        mc_registry_classes *classes = owned == NULL ? mc_world_runtime_registry(world)
                                                     : mc_world_mutation_execution_registry(owned);
        mc_registry_block_t_view array;
        mc_registry_error registry = mc_world_mutation_empty_registry();
        frame->wrapper_bytecode = 21;
        if (classes == NULL) {
            registry.status = MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY;
            registry.message = "World metadata Registry binding";
            mc_world_mutation_registry_fail(world, frame, &registry, registry.message);
            goto leave;
        }
        if (mc_registry_block_t_get(classes, &array, &registry) != MC_REGISTRY_OK) {
            mc_world_mutation_registry_fail(world, frame, &registry,
                                            "World metadata Block.t active use");
            goto leave;
        }
        bool observe;
        frame->wrapper_bytecode = 30;
        if (mc_registry_block_t_at(&array, (int32_t)((uint32_t)id & 255u), &observe, &registry) !=
            MC_REGISTRY_OK) {
            mc_world_mutation_registry_fail(world, frame, &registry,
                                            "World metadata Block.t element");
            goto leave;
        }
        frame->wrapper_bytecode = observe ? 40u : 52u;
        if (!(observe ? notify_and_neighbors(world, x, y, z, id, frame, owned)
                      : neighbors(world, x, y, z, id, frame, owned)))
            goto leave;
    }
    completed = true;
    if (success_diagnostic == MUTATION_SUCCESS_RESET_RETAINED)
        mc_world_mutation_error_init(frame);
leave:
    mc_world_runtime_call_leave(world);
    return completed;
}

mc_world_storage_status mc_world_runtime_metadata_set(mc_world_runtime *world, int32_t x, int32_t y,
                                                      int32_t z, int32_t metadata,
                                                      mc_world_mutation_error *error) {
    const mc_world_storage_status admission = admit(world, true, error);
    if (admission != MC_WORLD_STORAGE_OK)
        return admission;
    if (!in_domain(x, y, z)) {
        mc_world_mutation_error_init(error);
        return MC_WORLD_STORAGE_OK;
    }
    mc_world_mutation_error *frame = workspace(world, error);
    if (frame == NULL)
        return MC_WORLD_STORAGE_ALLOCATION_FAILED;
    const bool completed =
        metadata_body(world, x, y, z, metadata, frame, NULL, MUTATION_SUCCESS_PROJECT_CALLER);
    return result(error, frame, completed);
}

static bool admit_owned(const mc_world_mutation_execution_context *context, mc_world_runtime *world,
                        bool has_out, mc_world_mutation_execution_call **slot,
                        const mc_world_storage_status *raw, const mc_world_mutation_error *out,
                        mc_world_access_failure_lease **lease, mc_entity_tick_execution_site site) {
    if (context == NULL || context->result == NULL)
        return false;
    mc_world_runtime_execution_result *result = context->result;
    if (context->execution == NULL || context->execution->identity != context->execution ||
        context->code_classes == NULL || world == NULL || !has_out || slot == NULL ||
        *slot != NULL || raw == NULL || out == NULL) {
        mc_world_runtime_execution_result_note(result, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                                               "World mutation execution and fresh outputs", site);
        return false;
    }
    if (!current(context, world, lease, site))
        return false;
    const mc_world_storage_status status = admit(world, true, NULL);
    mc_world_runtime_execution_result_note(result, status, "World mutation runtime phase", site);
    return status == MC_WORLD_STORAGE_OK;
}

/* A Block hook receives World as an ordinary nullable argument. Only this
 * Chest RW body may evaluate factory15 before invoking that value at18;
 * nullable World method receivers retain admit_owned's earlier refusal. */
static bool admit_nullable_chest_added(const mc_world_mutation_execution_context *context,
                                       mc_world_mutation_execution_call **slot,
                                       const mc_world_mutation_error *out,
                                       mc_world_access_failure_lease **lease,
                                       mc_entity_tick_execution_site site) {
    if (context == NULL || context->result == NULL)
        return false;
    if (context->execution == NULL || context->execution->identity != context->execution ||
        context->code_classes == NULL || slot == NULL || *slot != NULL || out == NULL) {
        mc_world_runtime_execution_result_note(context->result, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                                               "Chest hook execution and fresh outputs", site);
        return false;
    }
    return current(context, NULL, lease, site);
}

static mc_world_mutation_execution_call *
start_owned(const mc_world_mutation_execution_context *context,
            mc_world_mutation_execution_call **slot, mc_world_access_failure_lease *lease,
            mc_entity_tick_execution_site site) {
    mc_world_mutation_execution_call *call = calloc(1, sizeof(*call));
    if (call == NULL) {
        mc_world_runtime_execution_result_note(context->result, MC_WORLD_STORAGE_ALLOCATION_FAILED,
                                               "World retained mutation invocation", site);
        return NULL;
    }
    call->context = *context;
    call->lease = lease;
    call->active = true;
    mc_world_mutation_error_init(&call->error);
    *slot = call;
    return call;
}

static bool finish_owned(mc_world_mutation_execution_call **slot, bool completed,
                         mc_world_storage_status *raw, mc_world_mutation_error *out,
                         mc_entity_tick_execution_site site, bool project_success) {
    mc_world_mutation_execution_call *call = *slot;
    call->active = false;
    *raw = call->error.status;
    if (!completed || project_success)
        *out = call->error;
    if (!completed)
        return false;
    mc_world_runtime_execution_result *result = call->context.result;
    const mc_world_storage_status status = mc_world_mutation_execution_retire(slot);
    mc_world_runtime_execution_result_note(result, status, "World completed mutation retirement",
                                           site);
    return status == MC_WORLD_STORAGE_OK;
}

bool mc_world_runtime_metadata_set_in_execution(const mc_world_mutation_execution_context *context,
                                                mc_world_runtime *world, int32_t x, int32_t y,
                                                int32_t z, int32_t metadata,
                                                mc_world_mutation_execution_call **slot,
                                                mc_world_storage_status *raw,
                                                mc_world_mutation_error *out) {
    mc_world_access_failure_lease *lease = NULL;
    if (!admit_owned(context, world, true, slot, raw, out, &lease, metadata_site))
        return false;
    if (!in_domain(x, y, z)) {
        *raw = MC_WORLD_STORAGE_OK;
        mc_world_mutation_error_init(out);
        return true;
    }
    mc_world_mutation_execution_call *call = start_owned(context, slot, lease, metadata_site);
    if (call == NULL)
        return false;
    const bool completed = metadata_body(world, x, y, z, metadata, &call->error, call,
                                         MUTATION_SUCCESS_RESET_RETAINED);
    return finish_owned(slot, completed, raw, out, metadata_site, true);
}

static bool block_set_owned(const mc_world_mutation_execution_context *context,
                            mc_world_runtime *world, int32_t x, int32_t y, int32_t z, int32_t id,
                            bool *out, mc_world_mutation_execution_call **slot,
                            mc_world_storage_status *raw, mc_world_mutation_error *error,
                            bool project_success) {
    mc_world_access_failure_lease *lease = NULL;
    if (!admit_owned(context, world, out != NULL, slot, raw, error, &lease, block_site))
        return false;
    if (!in_domain(x, y, z)) {
        *raw = MC_WORLD_STORAGE_OK;
        *out = false;
        if (project_success)
            mc_world_mutation_error_init(error);
        return true;
    }
    mc_world_mutation_execution_call *call = start_owned(context, slot, lease, block_site);
    if (call == NULL)
        return false;
    bool changed = false;
    const bool completed = set_body(world, x, y, z, id, false, 0, true, &changed, &call->error,
                                    call, MUTATION_SUCCESS_RESET_RETAINED);
    if (!finish_owned(slot, completed, raw, error, block_site, project_success))
        return false;
    *out = changed;
    return true;
}

bool mc_world_runtime_block_set_in_execution(const mc_world_mutation_execution_context *context,
                                             mc_world_runtime *world, int32_t x, int32_t y,
                                             int32_t z, int32_t id, bool *out,
                                             mc_world_mutation_execution_call **slot,
                                             mc_world_storage_status *raw,
                                             mc_world_mutation_error *error) {
    return block_set_owned(context, world, x, y, z, id, out, slot, raw, error, true);
}

bool mc_world_mutation_execution_remove(mc_world_mutation_execution_call *call,
                                        mc_world_runtime *world, int32_t x, int32_t y, int32_t z,
                                        mc_world_mutation_error *error,
                                        mc_entity_tick_execution_site site) {
    if (!mc_world_mutation_execution_current(call, world, site))
        return false;
    bool ignored;
    mc_world_storage_status raw = MC_WORLD_STORAGE_OK;
    return block_set_owned(&call->context, world, x, y, z, 0, &ignored, &call->child, &raw, error,
                           false);
}

bool mc_world_mutation_execution_set(mc_world_mutation_execution_call *call,
                                     mc_world_runtime *world, int32_t x, int32_t y, int32_t z,
                                     int32_t id, bool *out, mc_world_mutation_error *error,
                                     mc_entity_tick_execution_site site) {
    if (!mc_world_mutation_execution_current(call, world, site))
        return false;
    mc_world_storage_status raw = MC_WORLD_STORAGE_OK;
    return block_set_owned(&call->context, world, x, y, z, id, out, &call->child, &raw, error,
                           false);
}

bool mc_world_mutation_execution_set_quiet(mc_world_mutation_execution_call *call,
                                           mc_world_runtime *world, int32_t x, int32_t y, int32_t z,
                                           int32_t id, int32_t metadata, bool *out,
                                           mc_world_mutation_error *error,
                                           mc_entity_tick_execution_site site) {
    if (!mc_world_mutation_execution_current(call, world, site))
        return false;
    mc_world_storage_status raw = MC_WORLD_STORAGE_OK;
    mc_world_access_failure_lease *lease = NULL;
    if (!admit_owned(&call->context, world, out != NULL, &call->child, &raw, error, &lease, site))
        return false;
    if (!in_domain(x, y, z)) {
        *out = false;
        mc_world_mutation_error_init(error);
        return true;
    }
    mc_world_mutation_execution_call *child =
        start_owned(&call->context, &call->child, lease, site);
    if (child == NULL)
        return false;
    bool changed = false;
    const bool completed = set_body(world, x, y, z, id, true, metadata, false, &changed,
                                    &child->error, child, MUTATION_SUCCESS_RESET_RETAINED);
    if (!finish_owned(&call->child, completed, &raw, error, site, true))
        return false;
    *out = changed;
    return true;
}

bool mc_world_mutation_execution_added(const mc_world_mutation_execution_context *context,
                                       mc_world_runtime *world, const mc_block *block, int32_t x,
                                       int32_t y, int32_t z,
                                       mc_world_mutation_execution_call **slot,
                                       mc_world_mutation_error *error,
                                       mc_entity_tick_execution_site site) {
    mc_world_storage_status raw = MC_WORLD_STORAGE_OK;
    mc_world_access_failure_lease *lease = NULL;
    const bool nullable_chest = world == NULL && block != NULL && block->kind == MC_BLOCK_CHEST &&
                                block->classes->options.side == MC_REGISTRY_CLIENT;
    if (!(nullable_chest ? admit_nullable_chest_added(context, slot, error, &lease, site)
                         : admit_owned(context, world, true, slot, &raw, error, &lease, site)))
        return false;
    mc_world_mutation_execution_call *call = start_owned(context, slot, lease, site);
    if (call == NULL)
        return false;
    const bool completed =
        mc_world_mutation_block_added_with_call(call, world, block, x, y, z, &call->error);
    return finish_owned(slot, completed, &raw, error, site, true);
}

bool mc_world_mutation_execution_observe_range(mc_world_mutation_execution_call *call,
                                               mc_world_runtime *world, int32_t min_x,
                                               int32_t min_y, int32_t min_z, int32_t max_x,
                                               int32_t max_y, int32_t max_z,
                                               mc_world_mutation_error *error,
                                               mc_entity_tick_execution_site site) {
    if (!mc_world_mutation_execution_current(call, world, site))
        return false;
    mc_world_notification_execution_init_borrowed(&call->observer, call->context.execution, site,
                                                  call->context.result);
    mc_world_storage_status raw = MC_WORLD_STORAGE_OK;
    return mc_world_mutation_observe_range_in_execution(&call->observer, world, min_x, min_y, min_z,
                                                        max_x, max_y, max_z, error, &raw);
}

bool mc_world_mutation_execution_schedule(mc_world_mutation_execution_call *call,
                                          mc_world_runtime *world, int32_t x, int32_t y, int32_t z,
                                          int32_t block, int32_t delay,
                                          mc_world_mutation_error *error,
                                          mc_entity_tick_execution_site site) {
    if (!mc_world_mutation_execution_current(call, world, site))
        return false;
    if (call->schedule != NULL || error == NULL) {
        mc_world_runtime_execution_result_note(call->context.result, MC_WORLD_STORAGE_BUSY,
                                               "Scheduled call requires fresh retained frame",
                                               site);
        return false;
    }
    call->schedule = calloc(1, sizeof(*call->schedule));
    if (call->schedule == NULL) {
        mc_world_runtime_execution_result_note(call->context.result,
                                               MC_WORLD_STORAGE_ALLOCATION_FAILED,
                                               "Scheduled retained frame allocation", site);
        return false;
    }
    mc_world_schedule_execution_init(call->schedule, call->context.execution,
                                     call->context.code_classes, call->context.code,
                                     call->context.world, site, call->context.result);
    const bool completed = mc_world_runtime_schedule_in_execution(
        call->schedule, world, x, y, z, block, delay, &call->schedule_status);
    if (completed)
        return mc_world_schedule_execution_retire(&call->schedule) == MC_WORLD_STORAGE_OK;
    const mc_world_schedule_error *detail = &call->schedule->detail;
    if (call->schedule->native_stopped ||
        call->context.result->ownership_status != MC_WORLD_STORAGE_OK)
        return false;
    *error = detail->tick;
    error->schedule = (mc_world_schedule_prefix){.status = detail->status,
                                                 .operation = detail->operation,
                                                 .bytecode = detail->bytecode,
                                                 .presence_bytecode = detail->presence_bytecode,
                                                 .provider_bytecode = detail->provider_bytecode,
                                                 .cause = detail->cause,
                                                 .record = detail->record,
                                                 .membership = detail->membership,
                                                 .ordered = detail->ordered,
                                                 .registry = detail->registry};
    error->status = detail->status;
    error->operation = detail->operation;
    error->cause = detail->cause;
    error->registry = detail->registry;
    error->schedule_bytecode = detail->bytecode;
    error->record = detail->record;
    error->membership = detail->membership;
    error->ordered = detail->ordered;
    error->presence_bytecode = detail->presence_bytecode;
    error->provider_bytecode = detail->provider_bytecode;
    return false;
}

bool mc_world_mutation_execution_owns_world(const mc_world_mutation_execution_call *call,
                                            const mc_entity_tick_execution_world_token *world) {
    if (call == NULL || world == NULL || call->close_done)
        return false;
    if (call->presentation != NULL && !call->presentation->scope.consumed &&
        same_world(&call->presentation->scope.token, world)) {
        mc_entity_tick_execution_world_slot_phase phase;
        if (presentation_phase(call, &phase) && phase == MC_ENTITY_TICK_EXECUTION_WORLD_OPEN)
            return true;
    }
    return mc_world_mutation_drop_owns_world(call->drop, world) ||
           mc_world_mutation_execution_owns_world(call->child, world) ||
           (call->chunk != NULL &&
            mc_world_mutation_chunk_execution_owns_world(call->chunk, world));
}

bool mc_world_mutation_execution_source_null(const mc_world_mutation_execution_call *call,
                                             mc_entity_tick_execution_site *site) {
    if (call == NULL || call->active || site == NULL)
        return false;
    if (mc_world_mutation_execution_source_null(call->child, site))
        return true;
    if (call->chunk != NULL) {
        if (mc_world_mutation_execution_source_null(call->chunk->hook, site))
            return true;
        if (call->chunk->source_null) {
            *site = call->chunk->source_null_site;
            return true;
        }
    }
    if (call->tile_added_source_null) {
        *site = (mc_entity_tick_execution_site){"rw", "c", "(Lfd;III)V", 18};
        return true;
    }
    return false;
}

const mc_world_notification_execution *
mc_world_mutation_execution_failure_observer(const mc_world_mutation_execution_call *call) {
    if (call == NULL || call->active)
        return NULL;
    const mc_world_notification_execution *child =
        mc_world_mutation_execution_failure_observer(call->child);
    if (child != NULL)
        return child;
    if (call->chunk != NULL) {
        child = mc_world_mutation_chunk_failure_observer(call->chunk);
        if (child != NULL)
            return child;
    }
    return call->observer.entered && !call->observer.completed ? &call->observer : NULL;
}

bool mc_world_mutation_execution_failed_presentation(const mc_world_mutation_execution_call *call,
                                                     mc_world_mutation_presentation_failure *out) {
    if (call == NULL || call->active || out == NULL)
        return false;
    if (mc_world_mutation_execution_failed_presentation(call->child, out) ||
        (call->chunk != NULL &&
         mc_world_mutation_execution_failed_presentation(call->chunk->hook, out)))
        return true;
    const mutation_presentation *p = call->presentation;
    if (p == NULL || p->invoking || !p->returned || !p->frame.entered || p->completed)
        return false;
    *out = (mc_world_mutation_presentation_failure){.execution = &p->frame,
                                                    .error = &p->error,
                                                    .status = p->status,
                                                    .fault = p->frame.source_fault,
                                                    .particle = p->particle};
    return true;
}

const mc_world_material_execution *
mc_world_mutation_execution_failed_material(const mc_world_mutation_execution_call *call,
                                            mc_world_storage_status *raw) {
    if (call == NULL || call->active || raw == NULL)
        return NULL;
    const mc_world_material_execution *child =
        mc_world_mutation_execution_failed_material(call->child, raw);
    if (child != NULL)
        return child;
    if (call->chunk != NULL) {
        child = mc_world_mutation_execution_failed_material(call->chunk->hook, raw);
        if (child != NULL)
            return child;
    }
    if (call->material == NULL || !call->material->entered || call->material->completed)
        return NULL;
    *raw = call->material_status;
    return call->material;
}

bool mc_world_mutation_execution_chest_added(mc_world_mutation_execution_call *call,
                                             mc_world_runtime *world, const mc_block *block,
                                             int32_t x, int32_t y, int32_t z,
                                             mc_world_mutation_error *error) {
    const mc_entity_tick_execution_site site = {"rw", "c", "(Lfd;III)V", 18};
    if (!mc_world_mutation_execution_current(call, world, site))
        return false;
    if (call->tile_added != NULL || error == NULL || block == NULL ||
        block->kind != MC_BLOCK_CHEST) {
        mc_world_runtime_execution_result_note(call->context.result, MC_WORLD_STORAGE_BUSY,
                                               "Chest addition requires fresh retained child",
                                               site);
        return false;
    }
    call->tile_added = calloc(1, sizeof(*call->tile_added));
    if (call->tile_added == NULL) {
        mc_world_runtime_execution_result_note(call->context.result,
                                               MC_WORLD_STORAGE_ALLOCATION_FAILED,
                                               "World retained Chest addition child", site);
        return false;
    }
    call->tile_added_status = mc_live_tile_entities_block_added_in_execution(
        call->context.execution, world, call->context.code,
        mc_world_mutation_execution_registry(call), call->context.code_classes, site, block, x, y,
        z, call->tile_added, call->context.result, &call->tile_added_source_null);
    call->tile_added_completed = call->tile_added_status == MC_WORLD_STORAGE_OK &&
                                 call->context.result->ownership_status == MC_WORLD_STORAGE_OK;
    if (call->tile_added_completed) {
        error->hook_class = NULL;
        error->hook_bytecode = 0;
        return true;
    }
    /* Copy the actual returned child before any caller error projection. Its
     * source status is independent of the canonical native ownership result. */
    error->tile_entity = *call->tile_added;
    error->registry = call->tile_added->registry;
    error->entity = call->tile_added->entity;
    error->publication = call->tile_added->publication;
    error->status = call->tile_added_status;
    error->operation = call->tile_added->operation;
    error->cause = call->tile_added->cause;
    error->hook_class = "rw";
    error->hook_method = "c";
    error->hook_bytecode = call->tile_added->caller_bytecode != 0
                               ? call->tile_added->caller_bytecode
                               : call->tile_added->bytecode;
    return false;
}

const mc_live_tile_entity_error *
mc_world_mutation_execution_failed_tile_added(const mc_world_mutation_execution_call *call,
                                              mc_world_storage_status *raw) {
    if (call == NULL || call->active || raw == NULL)
        return NULL;
    const mc_live_tile_entity_error *child =
        mc_world_mutation_execution_failed_tile_added(call->child, raw);
    if (child != NULL)
        return child;
    if (call->chunk != NULL) {
        child = mc_world_mutation_execution_failed_tile_added(call->chunk->hook, raw);
        if (child != NULL)
            return child;
    }
    if (call->tile_added == NULL || call->tile_added_completed)
        return NULL;
    *raw = call->tile_added_status;
    return call->tile_added;
}

mc_world_storage_status
mc_world_mutation_execution_native_close(mc_world_mutation_execution_call *call) {
    if (call == NULL || call->close_done)
        return MC_WORLD_STORAGE_OK;
    if (call->active || !records_quiet(call->context.result))
        return MC_WORLD_STORAGE_BUSY;
    mc_world_storage_status status = mc_world_mutation_execution_native_close(call->child);
    if (status == MC_WORLD_STORAGE_OK && call->chunk != NULL)
        status = mc_world_mutation_chunk_native_close(call->chunk);
    if (status == MC_WORLD_STORAGE_OK)
        status = mc_world_mutation_drop_native_close(call->drop);
    if (status == MC_WORLD_STORAGE_OK)
        status = presentation_native_close(call);
    call->close_done = status == MC_WORLD_STORAGE_OK;
    return status;
}

mc_world_storage_status
mc_world_mutation_execution_retire(mc_world_mutation_execution_call **slot) {
    if (slot == NULL || *slot == NULL)
        return MC_WORLD_STORAGE_OK;
    mc_world_mutation_execution_call *call = *slot;
    const mc_world_storage_status closed = mc_world_mutation_execution_native_close(call);
    if (closed != MC_WORLD_STORAGE_OK)
        return closed;
    const mc_world_storage_status child = mc_world_mutation_execution_retire(&call->child);
    if (child != MC_WORLD_STORAGE_OK)
        return child;
    const mc_world_storage_status drop = mc_world_mutation_drop_retire(&call->drop);
    if (drop != MC_WORLD_STORAGE_OK)
        return drop;
    const mc_world_storage_status schedule = mc_world_schedule_execution_retire(&call->schedule);
    if (schedule != MC_WORLD_STORAGE_OK)
        return schedule;
    const mc_world_storage_status chunk_children =
        mc_world_mutation_chunk_children_retire(call->chunk);
    if (chunk_children != MC_WORLD_STORAGE_OK)
        return chunk_children;
    free(call->chunk);
    free(call->support);
    free(call->plant);
    free(call->material);
    free(call->tile_added);
    free(call->presentation);
    free(call);
    *slot = NULL;
    return MC_WORLD_STORAGE_OK;
}

bool mc_world_mutation_execution_support(mc_world_mutation_execution_call *call,
                                         mc_world_runtime *world, int32_t x, int32_t y, int32_t z,
                                         bool *out, mc_world_storage_status *raw,
                                         const mc_world_solid_support_execution **detail,
                                         mc_entity_tick_execution_site site) {
    if (!mc_world_mutation_execution_current(call, world, site))
        return false;
    if (call->support != NULL || out == NULL || raw == NULL || detail == NULL) {
        mc_world_runtime_execution_result_note(call->context.result, MC_WORLD_STORAGE_BUSY,
                                               "World support requires fresh caller work", site);
        return false;
    }
    call->support = calloc(1, sizeof(*call->support));
    if (call->support == NULL) {
        mc_world_runtime_execution_result_note(call->context.result,
                                               MC_WORLD_STORAGE_ALLOCATION_FAILED,
                                               "World retained support frame", site);
        return false;
    }
    mc_world_solid_support_execution_init(call->support, call->context.execution,
                                          call->context.code_classes, call->context.code, site,
                                          call->context.result);
    const bool completed = mc_world_solid_support_detailed_held_in_execution(
        call->support, world, x, y, z, out, &call->support_status);
    call->support_returned = true;
    *raw = call->support_status;
    if (!completed) {
        *detail = call->support;
        return false;
    }
    free(call->support);
    call->support = NULL;
    call->support_returned = false;
    *detail = NULL;
    return true;
}

const mc_world_solid_support_execution *
mc_world_mutation_execution_failed_support(const mc_world_mutation_execution_call *call,
                                           mc_world_storage_status *raw) {
    if (call == NULL || call->active || raw == NULL)
        return NULL;
    const mc_world_solid_support_execution *child =
        mc_world_mutation_execution_failed_support(call->child, raw);
    if (child != NULL)
        return child;
    if (call->support == NULL || !call->support_returned)
        return NULL;
    *raw = call->support_status;
    return call->support;
}

bool mc_world_mutation_execution_plant_survives(mc_world_mutation_execution_call *call,
                                                mc_world_runtime *world, const mc_block *block,
                                                int32_t x, int32_t y, int32_t z, bool *out,
                                                mc_world_storage_status *raw,
                                                const mc_world_plant_execution **detail,
                                                mc_entity_tick_execution_site site) {
    if (!mc_world_mutation_execution_current(call, world, site))
        return false;
    if (call->plant != NULL || out == NULL || raw == NULL || detail == NULL) {
        mc_world_runtime_execution_result_note(call->context.result, MC_WORLD_STORAGE_BUSY,
                                               "Plant survival requires fresh caller work", site);
        return false;
    }
    call->plant = calloc(1, sizeof(*call->plant));
    if (call->plant == NULL) {
        mc_world_runtime_execution_result_note(call->context.result,
                                               MC_WORLD_STORAGE_ALLOCATION_FAILED,
                                               "World retained plant frame", site);
        return false;
    }
    mc_world_plant_execution_init(call->plant, call->context.execution, call->context.code_classes,
                                  call->context.code, call->context.world, site,
                                  call->context.result);
    const bool completed = mc_world_runtime_plant_survives_in_execution(
        call->plant, world, block, x, y, z, out, &call->plant_status);
    call->plant_returned = true;
    *raw = call->plant_status;
    if (!completed) {
        *detail = call->plant;
        return false;
    }
    free(call->plant);
    call->plant = NULL;
    call->plant_returned = false;
    *detail = NULL;
    return true;
}

const mc_world_plant_execution *
mc_world_mutation_execution_failed_plant(const mc_world_mutation_execution_call *call,
                                         mc_world_storage_status *raw) {
    if (call == NULL || call->active || raw == NULL)
        return NULL;
    const mc_world_plant_execution *child =
        mc_world_mutation_execution_failed_plant(call->child, raw);
    if (child != NULL)
        return child;
    if (call->plant == NULL || !call->plant_returned)
        return NULL;
    *raw = call->plant_status;
    return call->plant;
}

bool mc_world_mutation_execution_drop(mc_world_mutation_execution_call *call,
                                      mc_world_runtime *world, const mc_block *block, int32_t x,
                                      int32_t y, int32_t z, int32_t metadata,
                                      mc_world_mutation_error *error,
                                      mc_entity_tick_execution_site site) {
    if (!mc_world_mutation_execution_current(call, world, site))
        return false;
    mc_world_storage_status raw = MC_WORLD_STORAGE_OK;
    const bool completed = mc_world_mutation_drop_in_execution(
        &call->context, world, block, x, y, z, metadata, &call->drop, error, &raw);
    if (!completed)
        return false;
    const mc_world_storage_status status = mc_world_mutation_drop_retire(&call->drop);
    mc_world_runtime_execution_result_note(call->context.result, status,
                                           "Block completed drop retirement", site);
    return status == MC_WORLD_STORAGE_OK;
}

const mc_world_mutation_drop_execution *
mc_world_mutation_execution_failed_drop(const mc_world_mutation_execution_call *call) {
    if (call == NULL || call->active)
        return NULL;
    const mc_world_mutation_drop_execution *child =
        mc_world_mutation_execution_failed_drop(call->child);
    return child != NULL ? child : call->drop;
}
