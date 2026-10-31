#include "mc/world/runtime/client_tick_state.h"

#include "../../entity/entity_internal.h"
#include "chunk_provider_internal.h"
#include "construction_internal.h"
#include "entity_publication_internal.h"
#include "mc/core/java/java.h"
#include "mc/core/math/math.h"
#include "mc/world/storage/source_chunk_loader.h"

#include <emmintrin.h>
#include <stdlib.h>
#include <string.h>

#if !defined(__SSE2__) && !defined(_M_X64)
#error "World refresh Entity requires the qualified x86-64/SSE2 profile"
#endif
#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) ||       \
    (defined(_M_FP_FAST) && _M_FP_FAST)
#error "World refresh Entity requires strict floating transformations"
#endif

static mc_world_storage_status field_result(mc_world_client_tick_state_error *out,
                                            mc_world_storage_status status, const char *operation,
                                            unsigned int pc) {
    if (out != NULL)
        *out = (mc_world_client_tick_state_error){
            status, operation, pc, status != MC_WORLD_STORAGE_OK, {status, operation, 0}};
    return status;
}

static mc_world_storage_status admit(mc_world_runtime *world, mc_world_storage_error *cause) {
    if (world == NULL || world->construction.mode != MC_WORLD_RUNTIME_GAME ||
        world->construction.phase == MC_WORLD_RUNTIME_RETIRED || world->classes == NULL ||
        world->classes->side != MC_WORLD_INFO_CLIENT) {
        *cause = (mc_world_storage_error){world == NULL ? MC_WORLD_STORAGE_INVALID_ARGUMENT
                                                        : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                          "client World tick actual owner", 0};
        return cause->status;
    }
    return mc_world_runtime_call_enter(world, cause);
}

mc_world_storage_status
mc_world_runtime_client_difficulty_store(mc_world_runtime *world, int32_t difficulty,
                                         mc_world_client_tick_state_error *error) {
    mc_world_storage_error cause;
    const mc_world_storage_status status = admit(world, &cause);
    if (status != MC_WORLD_STORAGE_OK)
        return field_result(error, status, cause.operation, 0);
    world->fields.q = difficulty; /* Minecraft.k1133 or remote override1151. */
    mc_world_runtime_call_leave(world);
    return field_result(error, MC_WORLD_STORAGE_OK, NULL, 0);
}

mc_world_storage_status
mc_world_runtime_client_lightning_decrement(mc_world_runtime *world,
                                            mc_world_client_tick_state_error *error) {
    mc_world_storage_error cause;
    const mc_world_storage_status status = admit(world, &cause);
    if (status != MC_WORLD_STORAGE_OK)
        return field_result(error, status, cause.operation, 0);
    if (world->fields.n > 0)
        world->fields.n = mc_java_i32_sub(world->fields.n, 1); /* Minecraft.k1209. */
    mc_world_runtime_call_leave(world);
    return field_result(error, MC_WORLD_STORAGE_OK, NULL, 0);
}

mc_world_storage_status
mc_world_runtime_client_spawn_flags(mc_world_runtime *world, bool hostile, bool peaceful,
                                    mc_world_client_tick_state_error *error) {
    mc_world_storage_error cause;
    const mc_world_storage_status status = admit(world, &cause);
    if (status != MC_WORLD_STORAGE_OK)
        return field_result(error, status, cause.operation, 0);
    world->fields.N = hostile;  /* Actual fd.a(ZZ)V putfield2. */
    world->fields.O = peaceful; /* Actual putfield7. */
    mc_world_runtime_call_leave(world);
    return field_result(error, MC_WORLD_STORAGE_OK, NULL, 10);
}

struct mc_world_refresh_entity_call {
    mc_world_refresh_entity_state state;
    mc_world_refresh_entity_error error;
    mc_world_runtime_acquisition_error acquisition;
    mc_world_source_chunk_error source_chunk;
    mc_world_entity_error publication;
    bool world_held, entity_held;
};

static mc_world_storage_status rejected(mc_world_refresh_entity_error *error,
                                        mc_world_storage_status status, const char *operation) {
    if (error != NULL)
        *error = (mc_world_refresh_entity_error){.status = status,
                                                 .operation = operation,
                                                 .native_policy = status != MC_WORLD_STORAGE_OK,
                                                 .cause = {status, operation, 0}};
    return status;
}

static bool fail(mc_world_refresh_entity_call *call, mc_world_storage_status status,
                 const char *operation, unsigned int pc, bool native) {
    call->error.status = status;
    call->error.operation = operation;
    call->error.bytecode = pc;
    call->error.native_policy = native;
    call->error.cause = (mc_world_storage_error){status, operation, 0};
    call->state.source_pc = pc;
    return false;
}

static bool get(mc_world_refresh_entity_call *call, int32_t x, int32_t z) {
    mc_world_runtime *world = call->state.world;
    if (call->state.chunk_calls == 25)
        return fail(call, MC_WORLD_STORAGE_LIMIT, "World refresh wrapping loop native ceiling", 62,
                    true);
    const size_t index = call->state.chunk_calls++;
    call->state.chunks[index].x = x;
    call->state.chunks[index].z = z;
    call->state.source_pc = call->error.bytecode = 62;
    call->error.provider_bytecode = 6;
    mc_chunk_provider_fields provider;
    mc_world_storage_error cause;
    if (world->fields.v == NULL ||
        mc_chunk_provider_bind_fields(world->fields.v, &provider, &cause) != MC_WORLD_STORAGE_OK)
        return fail(call, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                    "World refresh actual ChunkProvider receiver", 62, world->fields.v != NULL);
    if (provider.world != world || provider.cache != world->acquisition.cache ||
        provider.pending != world->acquisition.pending || provider.store != world->options.store)
        return fail(call, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                    "World refresh actual ChunkProvider binding", 62, true);
    mc_chunk_state *unused = NULL;
    const mc_chunk_acquire_status acquired =
        mc_world_runtime_get_detailed(world, x, z, &unused, &call->acquisition);
    if (acquired != MC_CHUNK_ACQUIRE_OK) {
        if (call->acquisition.source_chunk != NULL) {
            call->source_chunk = *call->acquisition.source_chunk;
            call->acquisition.source_chunk = &call->source_chunk;
        }
        call->error.acquisition = &call->acquisition;
        const mc_world_storage_error child = call->acquisition.cause;
        const mc_world_storage_status status =
            child.status != MC_WORLD_STORAGE_OK ? child.status
            : acquired == MC_CHUNK_ACQUIRE_LIMIT || acquired == MC_CHUNK_ACQUIRE_PARTIAL_LIMIT
                ? MC_WORLD_STORAGE_LIMIT
                : MC_WORLD_STORAGE_UNSUPPORTED_STATE;
        fail(call, status,
             child.operation == NULL ? "World refresh Chunk acquisition" : child.operation, 62,
             true);
        if (child.status != MC_WORLD_STORAGE_OK)
            call->error.cause = child;
        return false;
    }
    call->error.provider_bytecode = 0;
    ++call->state.chunk_returns;
    return true; /* Actual returned Chunk is popped at65, even when null. */
}

static bool body(mc_world_refresh_entity_call *call) {
    mc_world_runtime *world = call->state.world;
    mc_entity *entity = call->state.entity;
    if (entity == NULL)
        return fail(call, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                    "actual fd.g Entity null coordinate receiver", 1, false);
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
        return fail(call, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                    "World refresh native floating environment", 7, true);
    const double x = _mm_cvtsd_f64(_mm_div_sd(_mm_set_sd(entity->base.x), _mm_set_sd(16.0)));
    call->state.center_x = mc_math_floor_double(x);
    const double z = _mm_cvtsd_f64(_mm_div_sd(_mm_set_sd(entity->base.z), _mm_set_sd(16.0)));
    call->state.center_z = mc_math_floor_double(z);
    const int32_t max_x = mc_java_i32_add(call->state.center_x, 2);
    const int32_t max_z = mc_java_i32_add(call->state.center_z, 2);
    size_t outer_iterations = 0;
    for (int32_t chunk_x = mc_java_i32_sub(call->state.center_x, 2); chunk_x <= max_x;
         chunk_x = mc_java_i32_add(chunk_x, 1)) {
        if (outer_iterations++ == 5)
            return fail(call, MC_WORLD_STORAGE_LIMIT,
                        "World refresh wrapping outer loop native ceiling", 33, true);
        for (int32_t chunk_z = mc_java_i32_sub(call->state.center_z, 2); chunk_z <= max_z;
             chunk_z = mc_java_i32_add(chunk_z, 1))
            if (!get(call, chunk_x, chunk_z))
                return false;
    }
    call->state.source_pc = call->error.bytecode = 83;
    mc_world_reference_list *list = world->fields.b;
    mc_world_entity_list_ledger *ledger = &world->entity_membership;
    if (list == NULL || list != ledger->list || !mc_world_runtime_entity_list_valid(world))
        return fail(call, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                    "World refresh actual loaded Entity List owner", 83, list != NULL);
    for (size_t index = 0; index < list->count; ++index) {
        ++call->state.comparisons;
        mc_entity *candidate = ledger->entries[index];
        if (candidate != NULL && candidate->classes != entity->classes)
            return fail(call, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                        "World refresh reached Entity namespace", 83, true);
        if (candidate != NULL && candidate->base.id == entity->base.id) {
            call->state.contains = true;
            break;
        }
    }
    if (!call->state.contains) {
        if (!mc_world_runtime_entity_append_owned(world, world->fields.b, &world->entity_membership,
                                                  entity, 96, &call->publication)) {
            call->error.publication = &call->publication;
            fail(call, call->publication.status, call->publication.operation, 96, true);
            call->error.cause = call->publication.cause;
            return false;
        }
        call->state.appended = true;
    }
    call->state.source_pc = 102;
    call->state.returned = true;
    return true;
}

mc_world_storage_status
mc_world_runtime_refresh_entity_begin(mc_world_runtime *world, mc_entity *entity,
                                      mc_world_refresh_entity_call **out,
                                      mc_world_refresh_entity_error *error) {
    if (out == NULL || *out != NULL)
        return rejected(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "World refresh owned output");
    mc_world_storage_error cause;
    const mc_world_storage_status admitted = admit(world, &cause);
    if (admitted != MC_WORLD_STORAGE_OK)
        return rejected(error, admitted, cause.operation);
    if (entity != NULL && entity->classes != world->classes->entities) {
        mc_world_runtime_call_leave(world);
        return rejected(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "World refresh Entity namespace");
    }
    mc_world_refresh_entity_call *call = calloc(1, sizeof(*call));
    if (call == NULL) {
        mc_world_runtime_call_leave(world);
        return rejected(error, MC_WORLD_STORAGE_ALLOCATION_FAILED, "World refresh retained call");
    }
    call->state.world = world;
    call->state.entity = entity;
    call->state.error = &call->error;
    call->state.active = true;
    *out = call;
    if (!mc_world_runtime_binding_retain(world))
        fail(call, world->last_error.status, world->last_error.operation, 0, true);
    else {
        call->world_held = true;
        const mc_entity_status held = entity == NULL ? MC_ENTITY_OK : mc_entity_retain(entity);
        if (held != MC_ENTITY_OK) {
            call->error.entity = held;
            fail(call,
                 held == MC_ENTITY_LIMIT ? MC_WORLD_STORAGE_LIMIT
                                         : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                 "World refresh Entity occurrence", 0, true);
        } else {
            call->entity_held = entity != NULL;
            (void)body(call);
        }
    }
    call->state.active = false;
    mc_world_runtime_call_leave(world);
    if (error != NULL)
        *error = call->error;
    return call->error.status;
}

bool mc_world_runtime_refresh_entity_inspect(const mc_world_refresh_entity_call *call,
                                             mc_world_refresh_entity_state *out) {
    if (call == NULL || out == NULL || call->state.active)
        return false;
    *out = call->state;
    return true;
}

mc_world_storage_status
mc_world_runtime_refresh_entity_retire(mc_world_refresh_entity_call **slot,
                                       mc_world_refresh_entity_error *error) {
    if (slot == NULL)
        return rejected(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "World refresh retirement slot");
    mc_world_refresh_entity_call *call = *slot;
    if (call == NULL)
        return rejected(error, MC_WORLD_STORAGE_OK, NULL);
    if (call->state.active)
        return rejected(error, MC_WORLD_STORAGE_BUSY, "World refresh active invocation");
    if (call->entity_held)
        mc_entity_release(call->state.entity);
    if (call->world_held)
        mc_world_runtime_binding_release(call->state.world);
    free(call);
    *slot = NULL;
    return rejected(error, MC_WORLD_STORAGE_OK, NULL);
}
