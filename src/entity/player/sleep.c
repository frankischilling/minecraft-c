#include "mc/entity/player/sleep.h"
#include "../../block/registry/registry_internal.h"
#include "../../block/registry/runtime_internal.h"
#include "../../item/render/internal.h"
#include "../../world/runtime/bed_occupancy_execution_internal.h"
#include "../../world/runtime/bed_spawn_execution_internal.h"
#include "../../world/runtime/runtime_internal.h"
#include "../classes/classes_internal.h"
#include "../construction_internal.h"
#include "../position_internal.h"
#include "mc/core/java/java.h"
#include "mc/entity/player/coordinates.h"
#include "mc/world/runtime/bed_spawn.h"
#include "mc/world/runtime/bed_state.h"
#include "mc/world/runtime/construction.h"
#include "mc/world/runtime/players.h"
#include "mc/world/runtime/render_entities.h"
#include "mc/world/storage/source_chunk_loader.h"
#include "nbt_internal.h"
#include "player_internal.h"
#include "sleep_execution_internal.h"
#include "tick_math_internal.h"
#include <stdlib.h>
#include <string.h>

enum { SLEEP_WORLDS = 8, SLEEP_RECEIPTS = 4 };

typedef struct mc_player_wake_execution {
    mc_entity_tick_execution *execution;
    mc_world_runtime_classes *code_classes;
    mc_world_runtime_execution_code code;
    mc_world_runtime_execution_result result;
    mc_world_block_execution scalar;
    mc_world_bed_spawn_execution bed;
    mc_world_mutation_execution_call *mutation;
    mc_entity_tick_execution_world_token world;
    bool world_open;
} mc_player_wake_execution;

struct mc_player_sleep_call {
    mc_player_wake_execution *owned;
    mc_entity *player;
    mc_player_sleep_context context;
    mc_player_sleep_state state;
    mc_player_sleep_error error;
    mc_player_sleep_cleanup cleanup;
    mc_world_runtime *worlds[SLEEP_WORLDS];
    size_t world_count;
    mc_registry_classes *registry;
    mc_world_runtime_acquisition_error *query;
    mc_world_source_chunk_error *source;
    mc_world_access_failure receipts[SLEEP_RECEIPTS];
    size_t receipt_count;
    mc_world_placement_error *occupancy;
    mc_world_bed_spawn_error *bed;
    mc_world_player_sleep_error world_sleep;
    mc_entity_source_position_error position;
    struct mc_player_nbt_coordinate *local4, *local5, *late, *candidate;
    mc_player_coordinates *exit;
};

static bool hold_world(mc_player_sleep_call *, mc_world_runtime *, unsigned int);

static bool fail(mc_player_sleep_error *error, mc_player_sleep_status status, const char *operation,
                 unsigned int pc, bool native) {
    error->status = status;
    error->operation = operation;
    error->site = (mc_player_site){"gs", "b", "(III)Lcw;", pc};
    error->native_policy = native;
    return false;
}

static bool null_reference(mc_player_sleep_error *error, const char *operation, unsigned int pc) {
    error->source_exception = true;
    error->exception_class = "java/lang/NullPointerException";
    return fail(error, MC_PLAYER_SLEEP_SOURCE_NULL, operation, pc, false);
}

static bool world_fields(mc_entity *player, mc_world_runtime_fields *out,
                         mc_player_sleep_error *error, unsigned int pc,
                         mc_player_sleep_call *call) {
    mc_world_runtime *world = player->base.world;
    if (world == NULL)
        return null_reference(error, "GS sleep actual World null receiver", pc);
    if (call != NULL && !hold_world(call, world, pc))
        return false;
    const mc_world_storage_status status = mc_world_runtime_fields_get(world, out);
    if (status != MC_WORLD_STORAGE_OK) {
        error->world = (mc_world_storage_error){status, "GS sleep actual World fields", 0};
        return fail(error, MC_PLAYER_SLEEP_CHILD_FAILED, error->world.operation, pc, true);
    }
    return true;
}

static bool result(mc_entity *player, const char *field, unsigned int pc, unsigned int returned_pc,
                   const mc_player_sleep_result **out, mc_player_sleep_error *error) {
    const mc_player_sleep_result *value = NULL;
    const mc_player_sleep_result_status status =
        mc_player_sleep_result_static_get(player->classes, field, &value, &error->result);
    if (status != MC_PLAYER_SLEEP_RESULT_OK) {
        error->child = error->result.site;
        const mc_player_sleep_status mapped =
            status == MC_PLAYER_SLEEP_RESULT_LIMIT               ? MC_PLAYER_SLEEP_LIMIT
            : status == MC_PLAYER_SLEEP_RESULT_ALLOCATION_FAILED ? MC_PLAYER_SLEEP_ALLOCATION_FAILED
            : status == MC_PLAYER_SLEEP_RESULT_CLASS_FAILED      ? MC_PLAYER_SLEEP_CLASS_FAILED
            : status == MC_PLAYER_SLEEP_RESULT_BUSY              ? MC_PLAYER_SLEEP_BUSY
                                                                 : MC_PLAYER_SLEEP_CHILD_FAILED;
        return fail(error, mapped, error->result.operation, pc, error->result.native_policy);
    }
    *out = value;
    error->site = (mc_player_site){"gs", "b", "(III)Lcw;", returned_pc};
    error->returned = true;
    return true;
}

static bool too_far(double position, int32_t coordinate, double limit) {
    double distance = at_sub(position, (double)coordinate);
    uint64_t bits;
    memcpy(&bits, &distance, sizeof(bits));
    bits &= UINT64_C(0x7fffffffffffffff); /* Actual Math.abs, including -0 and NaN. */
    memcpy(&distance, &bits, sizeof(distance));
    /* Java dcmpl returns -1 for NaN: these source branches continue. */
    return distance > limit;
}

static bool prefix(mc_entity *player, int32_t x, int32_t y, int32_t z,
                   const mc_player_sleep_result **out, mc_player_sleep_error *error,
                   mc_player_sleep_call *call) {
    mc_world_runtime_fields world;
    if (!world_fields(player, &world, error, 4, call))
        return false;
    if (!world.B) {
        bool sleeping;
        if (mc_player_is_sleeping(player, &sleeping, &error->sleeping) != MC_PLAYER_OK) {
            error->child = error->sleeping.site;
            (void)fail(error, MC_PLAYER_SLEEP_CHILD_FAILED, error->sleeping.operation, 11, true);
            return false;
        }
        if (sleeping) {
            (void)result(player, "e", 24, 27, out, error);
            return false;
        }
        /* DC inherits LS.W: be1 before Y8; no World/FP child is added. */
        if (player->base.be || player->local_player->fields.living.Y <= 0) {
            (void)result(player, "e", 24, 27, out, error);
            return false;
        }
        if (!world_fields(player, &world, error, 32, call))
            return false;
        if (world.t == NULL) {
            (void)null_reference(error, "GS sleep actual provider null receiver", 35);
            return false;
        }
        mc_world_provider_fields provider;
        const mc_world_provider_status provider_status =
            mc_world_provider_fields_get(world.t, &provider);
        if (provider_status != MC_WORLD_PROVIDER_OK) {
            error->provider = (mc_world_provider_error){
                .status = provider_status, .operation = "GS sleep actual provider fields"};
            (void)fail(error, MC_PLAYER_SLEEP_CHILD_FAILED, error->provider.operation, 35, true);
            return false;
        }
        if (provider.c) {
            (void)result(player, "b", 41, 44, out, error);
            return false;
        }
        if (!world_fields(player, &world, error, 49, call))
            return false;
        if (world.f < 4) {
            (void)result(player, "c", 55, 58, out, error);
            return false;
        }
        if ((_mm_getcsr() & 0xffc0u) != 0x1f80u) {
            error->entity.status = MC_ENTITY_UNSUPPORTED_FP;
            (void)fail(error, MC_PLAYER_SLEEP_CHILD_FAILED, "GS sleep strict distance FP controls",
                       65, true);
            return false;
        }
        if (too_far(player->base.x, x, 3.0) || too_far(player->base.y, y, 2.0) ||
            too_far(player->base.z, z, 3.0)) {
            (void)result(player, "d", 110, 113, out, error);
            return false;
        }
    }
    return true;
}

mc_player_sleep_status mc_player_try_sleep(mc_entity *player, int32_t x, int32_t y, int32_t z,
                                           const mc_player_sleep_result **out,
                                           mc_player_sleep_error *out_error) {
    mc_player_sleep_error error = {0};
    if (out == NULL)
        (void)fail(&error, MC_PLAYER_SLEEP_ARGUMENT, "GS sleep native output", 0, true);
    else if (player == NULL)
        (void)null_reference(&error, "GS sleep null invocation receiver", 0);
    else if (!mc_entity_is_kind(player, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER) ||
             player->classes->source != MC_ENTITY_CLIENT)
        (void)fail(&error, MC_PLAYER_SLEEP_ARGUMENT, "GS sleep canonical client LocalPlayer", 0,
                   true);
    else {
        const mc_entity_status status = mc_entity_retain(player);
        if (status != MC_ENTITY_OK) {
            error.entity.status = status;
            (void)fail(&error,
                       status == MC_ENTITY_LIMIT ? MC_PLAYER_SLEEP_LIMIT : MC_PLAYER_SLEEP_BUSY,
                       "GS sleep native Player occurrence", 0, true);
        } else {
            if (prefix(player, x, y, z, out, &error, NULL))
                (void)fail(&error, MC_PLAYER_SLEEP_UNIMPLEMENTED,
                           "GS sleep actual resize descendant", 119, true);
            mc_entity_release(player);
        }
    }
    if (out_error != NULL)
        *out_error = error;
    return error.status;
}

static mc_player_site call_site(const mc_player_sleep_call *c, unsigned int pc) {
    return (mc_player_site){"gs", c->state.wake ? "a" : "b", c->state.wake ? "(ZZZ)V" : "(III)Lcw;",
                            pc};
}

static void at(mc_player_sleep_call *c, unsigned int pc) {
    c->state.source_pc = pc;
    c->error.site = call_site(c, pc);
    c->error.child = (mc_player_site){0};
}

static bool call_fail(mc_player_sleep_call *c, mc_player_sleep_status status, const char *operation,
                      unsigned int pc, bool native) {
    c->error.status = status;
    c->error.operation = operation;
    c->error.site = call_site(c, pc);
    c->state.source_pc = pc;
    c->error.native_policy = native;
    if (status == MC_PLAYER_SLEEP_SOURCE_NULL) {
        c->error.source_exception = true;
        c->error.exception_class = "java/lang/NullPointerException";
    }
    return false;
}

static mc_player_sleep_status world_status(mc_world_storage_status status) {
    return status == MC_WORLD_STORAGE_LIMIT               ? MC_PLAYER_SLEEP_LIMIT
           : status == MC_WORLD_STORAGE_ALLOCATION_FAILED ? MC_PLAYER_SLEEP_ALLOCATION_FAILED
           : status == MC_WORLD_STORAGE_BUSY              ? MC_PLAYER_SLEEP_BUSY
                                                          : MC_PLAYER_SLEEP_CHILD_FAILED;
}

static bool hold_world(mc_player_sleep_call *c, mc_world_runtime *world, unsigned int pc) {
    if (c->world_count == SLEEP_WORLDS)
        return call_fail(c, MC_PLAYER_SLEEP_LIMIT, "Sleep reached World occurrence extent", pc,
                         true);
    if (!mc_world_runtime_binding_retain(world)) {
        c->error.world = world->last_error;
        return call_fail(c, world_status(c->error.world.status), c->error.world.operation, pc,
                         true);
    }
    c->worlds[c->world_count++] = world;
    return true;
}

static bool capture_world(mc_player_sleep_call *c, unsigned int pc, bool nullable,
                          mc_world_runtime **out) {
    at(c, pc);
    mc_world_runtime *world = c->player->base.world;
    if (world == NULL && !nullable)
        return call_fail(c, MC_PLAYER_SLEEP_SOURCE_NULL, "Sleep actual current World null receiver",
                         pc, false);
    if (world != NULL && !hold_world(c, world, pc))
        return false;
    *out = world;
    return true;
}

static mc_world_runtime *current_world(mc_player_sleep_call *c, unsigned int pc) {
    mc_world_runtime *world = NULL;
    return capture_world(c, pc, false, &world) ? world : NULL;
}

static mc_entity_tick_execution_site wake_site(unsigned int pc) {
    return (mc_entity_tick_execution_site){"gs", "a", "(ZZZ)V", (int)pc};
}

static bool capture_wake_world(mc_player_sleep_call *c, unsigned int pc, mc_world_runtime **out) {
    mc_player_wake_execution *w = c->owned;
    if (w == NULL)
        return capture_world(c, pc, true, out);
    at(c, pc);
    mc_world_runtime *world = c->player->base.world;
    const mc_world_storage_status status =
        w->world_open
            ? MC_WORLD_STORAGE_BUSY
            : mc_entity_tick_execution_push_world(w->execution, world, wake_site(pc), &w->world);
    if (status != MC_WORLD_STORAGE_OK) {
        mc_world_runtime_execution_result_note(&w->result, status, "Wake actual World capture",
                                               wake_site(pc));
        return call_fail(c, world_status(status), "Wake actual World capture", pc, true);
    }
    w->world_open = true;
    *out = world;
    return true;
}

static bool wake_records_pending(const mc_world_runtime_execution_result *result) {
    if (result->root_pending)
        return true;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_LOADERS; ++i)
        if (result->pending[i].used)
            return true;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_WORLDS; ++i)
        if (result->closes[i].used)
            return true;
    return false;
}

static bool wake_same_world(const mc_entity_tick_execution_world_token *a,
                            const mc_entity_tick_execution_world_token *b) {
    return a->owner == b->owner && a->slot == b->slot && a->generation == b->generation;
}

static mc_world_storage_status close_wake_world(mc_player_wake_execution *w, bool failed) {
    if (!w->world_open)
        return MC_WORLD_STORAGE_OK;
    mc_entity_tick_execution_world_token current;
    if (wake_records_pending(&w->result) ||
        !mc_entity_tick_execution_current_world(w->execution, &current) ||
        !wake_same_world(&w->world, &current))
        return MC_WORLD_STORAGE_BUSY;
    const mc_world_storage_status status =
        mc_entity_tick_execution_pop_world(w->execution, &w->world, failed);
    const mc_entity_tick_execution_world_slot *slot = &w->execution->world_slots[w->world.slot];
    if (slot->generation == w->world.generation &&
        (slot->phase == MC_ENTITY_TICK_EXECUTION_WORLD_FREE ||
         slot->phase == MC_ENTITY_TICK_EXECUTION_WORLD_RETAINED))
        w->world_open = false;
    return status;
}

static bool wake_world_returned(mc_player_sleep_call *c, unsigned int pc) {
    if (c->owned == NULL)
        return true;
    mc_player_wake_execution *w = c->owned;
    const mc_world_storage_status status =
        w->result.ownership_status != MC_WORLD_STORAGE_OK ? w->result.ownership_status
        : w->result.selection_ready || w->execution->phase != MC_ENTITY_TICK_EXECUTION_OPEN
            ? MC_WORLD_STORAGE_BUSY
            : close_wake_world(w, false);
    if (status == MC_WORLD_STORAGE_OK)
        return true;
    mc_world_runtime_execution_result_note(&c->owned->result, status,
                                           "Wake actual World checked close", wake_site(pc));
    return call_fail(c, MC_PLAYER_SLEEP_OK, "Wake actual World checked close", pc, true);
}

static bool registry_failure(mc_player_sleep_call *c, unsigned int pc,
                             mc_registry_classes *classes) {
    mc_item_render_throwable throwable = MC_ITEM_RENDER_NO_THROWABLE;
    mc_item_render_registry_classification(classes, &c->error.registry, &throwable,
                                           &c->error.exception_class);
    c->error.source_exception = throwable == MC_ITEM_RENDER_SOURCE_EXCEPTION;
    c->error.source_error = throwable == MC_ITEM_RENDER_SOURCE_ERROR;
    return call_fail(c, world_status(mc_world_runtime_registry_status(c->error.registry.status)),
                     c->error.registry.message, pc, throwable == MC_ITEM_RENDER_NO_THROWABLE);
}

static bool code_registry(mc_player_sleep_call *c, unsigned int pc) {
    at(c, pc);
    if (c->registry != NULL)
        return true;
    mc_registry_classes *registry = c->context.code_registry;
    if (registry == NULL || registry->entities != c->player->classes)
        return call_fail(c, MC_PLAYER_SLEEP_ARGUMENT, "Sleep actual defining Registry dependency",
                         pc, true);
    if (mc_registry_classes_retain(registry, MC_REGISTRY_CLIENT, c->player->classes->profile,
                                   &c->error.registry) != MC_REGISTRY_OK)
        return registry_failure(c, pc, registry);
    c->registry = registry;
    return true;
}

static bool capture_query(mc_player_sleep_call *c) {
    if (c->query == NULL)
        return true;
    if (c->query->source_chunk != NULL && c->source == NULL) {
        c->source = malloc(sizeof(*c->source));
        if (c->source == NULL) {
            c->cleanup.world = (mc_world_storage_error){MC_WORLD_STORAGE_ALLOCATION_FAILED,
                                                        "Sleep retained loader diagnostic", 0};
            return false;
        }
        *c->source = *c->query->source_chunk;
        c->query->source_chunk = c->source;
    }
    const mc_world_access_failure *receipts[] = {
        &c->query->batch.publication.world_access_failure,
        &c->query->tile_entity.world_access_failure,
        &c->query->tile_entity.publication.world_access_failure,
        &c->query->tile_entity.batch.publication.world_access_failure};
    for (size_t i = 0; i < sizeof(receipts) / sizeof(*receipts); ++i) {
        const mc_world_access_failure *receipt = receipts[i];
        if (receipt->receipt == NULL)
            continue;
        bool seen = false;
        for (size_t j = 0; j < c->receipt_count; ++j)
            if (c->receipts[j].receipt == receipt->receipt)
                seen = true;
        if (seen)
            continue;
        if (c->receipt_count == SLEEP_RECEIPTS) {
            c->cleanup.world =
                (mc_world_storage_error){MC_WORLD_STORAGE_LIMIT, "Sleep query producer extent", 0};
            return false;
        }
        const mc_world_storage_status status =
            mc_world_access_failure_retain(receipt, &c->receipts[c->receipt_count]);
        if (status != MC_WORLD_STORAGE_OK) {
            c->cleanup.world = (mc_world_storage_error){status, "Sleep retained query producer", 0};
            return false;
        }
        ++c->receipt_count;
    }
    return true;
}

static bool query(mc_player_sleep_call *c, mc_world_runtime *world, int32_t x, int32_t y, int32_t z,
                  bool metadata, unsigned int pc, int32_t *out) {
    c->query = calloc(1, sizeof(*c->query));
    if (c->query == NULL)
        return call_fail(c, MC_PLAYER_SLEEP_ALLOCATION_FAILED, "Sleep reached query frame", pc,
                         true);
    c->error.acquisition = c->query;
    const mc_world_storage_status status =
        metadata
            ? mc_world_runtime_metadata_get_detailed(world, x, y, z, out, &c->error.world, c->query)
            : mc_world_runtime_block_get_detailed(world, x, y, z, out, &c->error.world, c->query);
    if (status != MC_WORLD_STORAGE_OK) {
        (void)call_fail(c, world_status(status), c->error.world.operation, pc, true);
        c->error.registry = c->query->registry;
        if (c->query->registry.status != MC_REGISTRY_OK)
            (void)registry_failure(c, pc, mc_world_runtime_registry(world));
    }
    const bool captured = capture_query(c);
    if (!captured && status == MC_WORLD_STORAGE_OK)
        return call_fail(c, world_status(c->cleanup.world.status), c->cleanup.world.operation, pc,
                         true);
    if (status != MC_WORLD_STORAGE_OK) {
        for (size_t i = 0; i < c->receipt_count; ++i)
            if (mc_world_access_failure_source_exception(&c->receipts[i],
                                                         &c->error.exception_class)) {
                c->error.source_exception = true;
                c->error.source_error = c->error.native_policy = false;
                break;
            }
    }
    return status == MC_WORLD_STORAGE_OK && captured;
}

static bool wake_block_get(mc_player_sleep_call *c, mc_world_runtime *world,
                           const mc_player_coordinates *position, int32_t *out) {
    at(c, 48);
    if (world == NULL)
        return call_fail(c, MC_PLAYER_SLEEP_SOURCE_NULL, "Wake actual World block receiver", 48,
                         false);
    mc_player_wake_execution *w = c->owned;
    if (w == NULL)
        return query(c, world, position->x, position->y, position->z, false, 48, out);
    mc_world_block_execution_init(&w->scalar, w->execution, w->code, wake_site(48), &w->result);
    mc_world_storage_status raw = MC_WORLD_STORAGE_OK;
    if (!mc_world_runtime_block_get_in_execution(&w->scalar, world, position->x, position->y,
                                                 position->z, out, &c->error.world, &raw)) {
        if (w->scalar.get_returned && w->scalar.get_status != MC_CHUNK_ACQUIRE_OK)
            c->error.acquisition = &w->scalar.acquisition;
        return call_fail(c, raw == MC_WORLD_STORAGE_OK ? MC_PLAYER_SLEEP_OK : world_status(raw),
                         c->error.world.operation != NULL ? c->error.world.operation
                                                          : w->result.ownership_error.operation,
                         48, true);
    }
    return wake_world_returned(c, 48);
}

static bool fp(mc_player_sleep_call *c, unsigned int pc) {
    if ((_mm_getcsr() & 0xffc0u) == 0x1f80u)
        return true;
    c->error.entity.status = MC_ENTITY_UNSUPPORTED_FP;
    return call_fail(c, MC_PLAYER_SLEEP_CHILD_FAILED, "Sleep reached float argument controls", pc,
                     true);
}

static float from_i32(int32_t value) {
    return _mm_cvtss_f32(_mm_cvtsi32_ss(_mm_setzero_ps(), value));
}

static void size(mc_player_sleep_call *c, float width, float height, unsigned int pc) {
    at(c, pc);
    c->error.size_wrapper = (mc_player_site){"ls", "b", "(FF)V", 3};
    mc_entity_source_size(c->player, width, height);
    c->error.size_leaf = (mc_player_site){"sn", "b", "(FF)V", 10};
}

static bool position(mc_player_sleep_call *c, double x, double y, double z, unsigned int pc) {
    at(c, pc);
    c->error.position = &c->position;
    const mc_entity_status status =
        mc_entity_source_position_checked(c->player, x, y, z, &c->position);
    if (status == MC_ENTITY_OK)
        return true;
    c->error.child = (mc_player_site){"sn", "e", "(DDD)V", c->position.bytecode};
    c->error.entity.status = status;
    return call_fail(
        c, c->position.source_null ? MC_PLAYER_SLEEP_SOURCE_NULL : MC_PLAYER_SLEEP_CHILD_FAILED,
        c->position.operation, pc, c->position.native_policy);
}

static bool update_sleeping(mc_player_sleep_call *c, mc_world_runtime *world, unsigned int pc) {
    at(c, pc);
    c->error.world_sleep = &c->world_sleep;
    const mc_world_storage_status status =
        mc_world_runtime_update_player_sleeping(world, &c->world_sleep);
    if (status == MC_WORLD_STORAGE_OK)
        return true;
    c->error.world = c->world_sleep.cause;
    c->error.child = c->world_sleep.player_bytecode != 0
                         ? (mc_player_site){"gs", "N", "()Z", c->world_sleep.player_bytecode}
                         : (mc_player_site){"fd", "y", "()V", c->world_sleep.bytecode};
    (void)call_fail(c,
                    c->world_sleep.source_null ? MC_PLAYER_SLEEP_SOURCE_NULL : world_status(status),
                    c->world_sleep.operation, pc, c->world_sleep.native_policy);
    if (c->world_sleep.source_cast) {
        c->error.source_exception = true;
        c->error.exception_class = "java/lang/ClassCastException";
    }
    return false;
}

static bool coordinate_failure(mc_player_sleep_call *c, mc_player_status status, unsigned int pc) {
    return call_fail(c,
                     status == MC_PLAYER_LIMIT               ? MC_PLAYER_SLEEP_LIMIT
                     : status == MC_PLAYER_ALLOCATION_FAILED ? MC_PLAYER_SLEEP_ALLOCATION_FAILED
                                                             : MC_PLAYER_SLEEP_CHILD_FAILED,
                     c->error.coordinate.operation, pc, true);
}

static void entry(mc_player_sleep_call *c, int32_t x, int32_t y, int32_t z) {
    if (!prefix(c->player, x, y, z, &c->state.result, &c->error, c))
        return;
    mc_entity *p = c->player;
    size(c, .2f, .2f, 119);
    p->base.offset = .2f;
    mc_world_runtime *world = current_world(c, 135);
    if (world == NULL)
        return;
    bool present = false;
    const mc_world_storage_status presence =
        mc_world_runtime_render_block_present(world, x, y, z, &present, &c->error.world);
    if (presence != MC_WORLD_STORAGE_OK) {
        (void)call_fail(c, world_status(presence), c->error.world.operation, 135, true);
        return;
    }
    float offset_x = .5f, offset_z = .5f;
    if (present) {
        world = current_world(c, 148);
        int32_t metadata = 0;
        if (world == NULL || !query(c, world, x, y, z, true, 148, &metadata) ||
            !code_registry(c, 155))
            return;
        if (mc_registry_bed_active_use(c->registry, &c->error.registry) != MC_REGISTRY_OK) {
            (void)registry_failure(c, 155, c->registry);
            return;
        }
        const int32_t direction = (int32_t)((uint32_t)metadata & 3u);
        if (direction == 0)
            offset_z = .9f;
        else if (direction == 2)
            offset_z = .1f;
        else if (direction == 1)
            offset_x = .1f;
        else
            offset_x = .9f;
        at(c, 228);
        mc_player_fields *fields = &p->local_player->fields.player;
        fields->w = fields->y = 0.0f;
        if (direction == 0)
            fields->y = -1.8f;
        else if (direction == 2)
            fields->y = 1.8f;
        else if (direction == 1)
            fields->w = 1.8f;
        else
            fields->w = -1.8f;
    }
    if (!fp(c, present ? 233u : 258u))
        return;
    const double px = at_widen(at_fadd(from_i32(x), offset_x));
    const double py = at_widen(at_fadd(from_i32(y), .9375f));
    const double pz = at_widen(at_fadd(from_i32(z), offset_z));
    if (!position(c, px, py, pz, present ? 250u : 275u))
        return;
    p->local_player->fields.player.u = true;
    p->local_player->fields.player.private_a = 0;
    at(c, 289);
    const mc_player_status allocated = mc_player_coordinate_new_held(
        p, c->error.site, "GS sleep actual BR allocation", &c->candidate, &c->error.coordinate);
    if (allocated != MC_PLAYER_OK) {
        (void)coordinate_failure(c, allocated, 289);
        return;
    }
    c->candidate->value = (mc_player_coordinates){x, y, z};
    mc_player_nbt_coordinate_store_held(p, c->candidate, &p->local_player->fields.player.v);
    c->candidate = NULL;
    p->base.velocity_y = 0.0;
    p->base.velocity_z = 0.0;
    p->base.velocity_x = 0.0;
    world = current_world(c, 321);
    if (world == NULL)
        return;
    if (!world->fields.B) {
        world = current_world(c, 331);
        if (world == NULL || !update_sleeping(c, world, 331))
            return;
    }
    at(c, 334);
    (void)result(p, "a", 334, 337, &c->state.result, &c->error);
}

static bool pin(mc_player_sleep_call *c, const mc_player_coordinates *value,
                struct mc_player_nbt_coordinate **slot, unsigned int pc) {
    const mc_player_status status =
        mc_player_coordinate_pin_held(c->player, value, slot, &c->error.coordinate);
    return status == MC_PLAYER_OK || coordinate_failure(c, status, pc);
}

static bool discard_local5(mc_player_sleep_call *c) {
    if (c->local5 == NULL)
        return true;
    const mc_player_status status = mc_player_coordinate_unpin_admit_held(c->player, c->local5, 1);
    if (status != MC_PLAYER_OK) {
        c->error.coordinate.status = status;
        c->error.coordinate.operation = "Wake overwritten local coordinate ownership";
        return coordinate_failure(c, status, 106);
    }
    mc_player_coordinate_unpin_commit_held(c->player, &c->local5);
    return true;
}

static void wake(mc_player_sleep_call *c, bool reset_timer, bool update, bool set_spawn) {
    mc_entity *p = c->player;
    size(c, .6f, 1.8f, 5);
    at(c, 9);
    p->base.offset = 1.62f; /* Actual virtual GS.E3. */
    at(c, 13);
    if (!pin(c, p->local_player->fields.player.v, &c->local4, 13))
        return;
    at(c, 19);
    if (!pin(c, p->local_player->fields.player.v, &c->local5, 19))
        return;
    if (c->local4 != NULL) {
        const mc_player_coordinates *local4 = &c->local4->value;
        mc_world_runtime *world = NULL;
        int32_t id = 0;
        if (!capture_wake_world(c, 30, &world) || !wake_block_get(c, world, local4, &id) ||
            !code_registry(c, 51))
            return;
        const mc_block *bed = NULL;
        if (mc_registry_block_static_get(c->registry, "T", &bed, &c->error.registry) !=
            MC_REGISTRY_OK) {
            (void)registry_failure(c, 51, c->registry);
            return;
        }
        at(c, 54);
        if (bed == NULL) {
            (void)call_fail(c, MC_PLAYER_SLEEP_SOURCE_NULL, "Wake actual Block.T null ID receiver",
                            54, false);
            return;
        }
        if (id == bed->fields.bn) {
            /* VE.a is static. Its World operand is captured before the
             * coordinate arguments, and its actual getter checks null at 4. */
            if (!capture_wake_world(c, 61, &world))
                return;
            at(c, 80);
            c->occupancy = calloc(1, sizeof(*c->occupancy));
            if (c->occupancy == NULL) {
                (void)call_fail(c, MC_PLAYER_SLEEP_ALLOCATION_FAILED,
                                "Wake retained occupancy frame", 80, true);
                return;
            }
            c->error.occupancy = c->occupancy;
            mc_world_storage_status occupied = MC_WORLD_STORAGE_OK;
            bool occupancy_returned;
            mc_player_wake_execution *w = c->owned;
            if (w == NULL) {
                occupied = mc_world_bed_set_occupied(c->registry, world, local4->x, local4->y,
                                                     local4->z, false, c->occupancy);
                occupancy_returned = occupied == MC_WORLD_STORAGE_OK;
            } else {
                const mc_world_mutation_execution_context context = {.execution = w->execution,
                                                                     .code_classes =
                                                                         w->code_classes,
                                                                     .code = w->code,
                                                                     .result = &w->result,
                                                                     .world = w->world};
                mc_world_bed_occupancy_execution occupancy = {
                    .context = &context, .scalar = &w->scalar, .mutation = &w->mutation};
                occupancy_returned = mc_world_bed_set_occupied_in_execution(
                    c->registry, world, local4->x, local4->y, local4->z, false, c->occupancy,
                    &occupancy, &occupied);
            }
            if (!occupancy_returned) {
                c->error.world = c->occupancy->cause;
                const bool null_receiver =
                    world == NULL && c->occupancy->source_exception && c->occupancy->site.pc == 4;
                (void)call_fail(c,
                                null_receiver                     ? MC_PLAYER_SLEEP_SOURCE_NULL
                                : occupied == MC_WORLD_STORAGE_OK ? MC_PLAYER_SLEEP_OK
                                                                  : world_status(occupied),
                                c->occupancy->operation, 80, c->occupancy->native_policy);
                c->error.child =
                    (mc_player_site){c->occupancy->site.class_name, c->occupancy->site.method,
                                     c->occupancy->site.descriptor, c->occupancy->site.pc};
                c->error.source_exception = c->occupancy->source_exception;
                c->error.source_error = c->occupancy->source_error;
                c->error.exception_class = c->occupancy->exception_class;
                return;
            }
            if (!wake_world_returned(c, 80))
                return;
            /* Reload the actual operand. Occupancy notifications can change
             * Player.World before the separate static exit search. */
            if (!capture_wake_world(c, 84, &world))
                return;
            at(c, 103);
            c->bed = malloc(sizeof(*c->bed));
            if (c->bed == NULL) {
                (void)call_fail(c, MC_PLAYER_SLEEP_ALLOCATION_FAILED,
                                "Wake retained Bed exit frame", 103, true);
                return;
            }
            mc_world_bed_spawn_error_init(c->bed);
            c->error.bed = c->bed;
            mc_world_storage_status searched = MC_WORLD_STORAGE_OK;
            bool exit_returned;
            if (w == NULL) {
                searched = mc_world_bed_spawn_exit(world, c->registry, local4->x, local4->y,
                                                   local4->z, 0, NULL, &c->exit, c->bed);
                exit_returned = searched == MC_WORLD_STORAGE_OK;
            } else {
                mc_world_bed_spawn_execution_init(&w->bed, w->execution, w->code_classes, w->code,
                                                  wake_site(84), &w->result);
                exit_returned = mc_world_bed_spawn_exit_in_execution(
                    &w->bed, world, c->registry, local4->x, local4->y, local4->z, 0, NULL, &c->exit,
                    c->bed, &searched);
            }
            if (!exit_returned) {
                c->error.world = c->bed->cause;
                const char *operation = c->bed->operation;
                if (operation == NULL && w != NULL)
                    operation = w->result.ownership_error.operation;
                const bool null_receiver =
                    world == NULL && c->bed->source_exception && c->bed->pc == 4;
                (void)call_fail(c,
                                null_receiver                     ? MC_PLAYER_SLEEP_SOURCE_NULL
                                : searched == MC_WORLD_STORAGE_OK ? MC_PLAYER_SLEEP_OK
                                                                  : world_status(searched),
                                operation, 103,
                                searched == MC_WORLD_STORAGE_OK || c->bed->native_policy);
                c->error.child =
                    (mc_player_site){c->bed->source_class, "f", "(Lfd;IIII)Lbr;", c->bed->pc};
                c->error.source_exception = c->bed->source_exception;
                c->error.source_error = c->bed->source_error;
                c->error.exception_class = c->bed->exception_class;
                return;
            }
            if (!wake_world_returned(c, 103) || !discard_local5(c))
                return;
            const mc_player_coordinates *local5 = c->exit;
            if (local5 == NULL) {
                at(c, 113);
                const mc_player_status allocated = mc_player_coordinate_new_held(
                    p, c->error.site, "Wake actual fallback BR allocation", &c->candidate,
                    &c->error.coordinate);
                if (allocated != MC_PLAYER_OK) {
                    (void)coordinate_failure(c, allocated, 113);
                    return;
                }
                c->candidate->value.x = local4->x;
                c->candidate->value.y = mc_java_i32_add(local4->y, 1);
                c->candidate->value.z = local4->z;
                local5 = &c->candidate->value;
            }
            if (!fp(c, 145))
                return;
            const double x = at_widen(at_fadd(from_i32(local5->x), .5f));
            const double y = at_widen(at_fadd(at_fadd(from_i32(local5->y), p->base.offset), .1f));
            const double z = at_widen(at_fadd(from_i32(local5->z), .5f));
            if (!position(c, x, y, z, 175))
                return;
        }
    }
    p->local_player->fields.player.u = false;
    mc_world_runtime *world = current_world(c, 187);
    if (world == NULL)
        return;
    if (!world->fields.B && update) {
        if (!capture_wake_world(c, 198, &world))
            return;
        if (world == NULL) {
            (void)call_fail(c, MC_PLAYER_SLEEP_SOURCE_NULL, "Wake actual World sleeping receiver",
                            201, false);
            return;
        }
        if (!update_sleeping(c, world, 201) || !wake_world_returned(c, 201))
            return;
    }
    at(c, reset_timer ? 210u : 219u);
    p->local_player->fields.player.private_a = reset_timer ? 0 : 100;
    if (set_spawn) {
        at(c, 228);
        const mc_player_coordinates *value = p->local_player->fields.player.v;
        if (!pin(c, value, &c->late, 228))
            return;
        at(c, 231);
        const mc_player_status status = mc_player_saved_spawn_set(p, value, &c->error.coordinate);
        if (status != MC_PLAYER_OK) {
            (void)coordinate_failure(c, status, 231);
            c->error.child = c->error.coordinate.site;
            return;
        }
    }
    at(c, 234);
    c->error.returned = true;
}

static mc_player_sleep_status begin(mc_entity *p, int32_t x, int32_t y, int32_t z, bool waking,
                                    bool reset_timer, bool update, bool set_spawn,
                                    const mc_player_sleep_context *context,
                                    mc_player_sleep_call **out, mc_player_sleep_error *error,
                                    mc_player_wake_execution *owned) {
    mc_player_sleep_error admission = {0};
    if (out == NULL || *out != NULL) {
        admission.status = MC_PLAYER_SLEEP_ARGUMENT;
        admission.operation = "Sleep retained call output";
        admission.native_policy = true;
        if (error != NULL)
            *error = admission;
        return admission.status;
    }
    mc_player_sleep_call *c = calloc(1, sizeof(*c));
    if (c == NULL) {
        admission.status = MC_PLAYER_SLEEP_ALLOCATION_FAILED;
        admission.operation = "Sleep retained call allocation";
        admission.native_policy = true;
        if (error != NULL)
            *error = admission;
        return admission.status;
    }
    *out = c;
    c->owned = owned;
    c->state.active = true;
    c->state.wake = waking;
    c->state.receiver_identity = (uintptr_t)p;
    if (context != NULL)
        c->context = *context;
    if (p == NULL)
        (void)call_fail(c, MC_PLAYER_SLEEP_SOURCE_NULL, "Sleep null actual invocation receiver", 0,
                        false);
    else if (!mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER) ||
             p->classes->source != MC_ENTITY_CLIENT)
        (void)call_fail(c, MC_PLAYER_SLEEP_ARGUMENT, "Sleep canonical client LocalPlayer receiver",
                        0, true);
    else {
        c->error.entity.status = mc_entity_retain(p);
        if (c->error.entity.status != MC_ENTITY_OK)
            (void)call_fail(c,
                            c->error.entity.status == MC_ENTITY_LIMIT ? MC_PLAYER_SLEEP_LIMIT
                                                                      : MC_PLAYER_SLEEP_BUSY,
                            "Sleep retained Player occurrence", 0, true);
        else {
            c->player = p;
            if (waking)
                wake(c, reset_timer, update, set_spawn);
            else
                entry(c, x, y, z);
        }
    }
    c->state.active = false;
    c->state.returned = c->error.returned;
    c->state.source_pc = c->error.site.pc;
    if (error != NULL)
        *error = c->error;
    return c->error.status;
}

mc_player_sleep_status mc_player_try_sleep_begin(mc_entity *p, int32_t x, int32_t y, int32_t z,
                                                 const mc_player_sleep_context *context,
                                                 mc_player_sleep_call **out,
                                                 mc_player_sleep_error *error) {
    return begin(p, x, y, z, false, false, false, false, context, out, error, NULL);
}

mc_player_sleep_status mc_player_wake_begin(mc_entity *p, bool reset_timer, bool update,
                                            bool set_spawn, const mc_player_sleep_context *context,
                                            mc_player_sleep_call **out,
                                            mc_player_sleep_error *error) {
    return begin(p, 0, 0, 0, true, reset_timer, update, set_spawn, context, out, error, NULL);
}

mc_player_sleep_status mc_player_wake_begin_in_execution(
    mc_entity *p, const mc_player_sleep_context *context, mc_entity_tick_execution *execution,
    mc_world_runtime_classes *code_classes, mc_world_runtime_execution_code code,
    mc_player_sleep_call **out, mc_player_sleep_error *error) {
    if (execution == NULL || code_classes == NULL || out == NULL || *out != NULL || error == NULL)
        return MC_PLAYER_SLEEP_ARGUMENT;
    mc_player_wake_execution *w = calloc(1, sizeof(*w));
    if (w == NULL) {
        *error = (mc_player_sleep_error){.status = MC_PLAYER_SLEEP_ALLOCATION_FAILED,
                                         .operation = "Wake retained execution allocation",
                                         .native_policy = true};
        return error->status;
    }
    mc_world_runtime_execution_result_init(&w->result);
    w->execution = execution;
    w->code_classes = code_classes;
    w->code = code;
    const mc_player_sleep_status status =
        begin(p, 0, 0, 0, true, true, true, false, context, out, error, w);
    if (*out == NULL)
        free(w);
    return status;
}

mc_world_runtime_execution_result *mc_player_wake_execution_result(mc_player_sleep_call *c) {
    return c == NULL || c->owned == NULL ? NULL : &c->owned->result;
}

bool mc_player_wake_execution_owns_world(const mc_player_sleep_call *c,
                                         const mc_entity_tick_execution_world_token *world) {
    if (c == NULL || c->owned == NULL || world == NULL || c->state.active)
        return false;
    const mc_player_wake_execution *w = c->owned;
    return (w->world_open && wake_same_world(&w->world, world)) ||
           mc_world_mutation_execution_owns_world(w->mutation, world);
}

bool mc_player_wake_execution_source_null(const mc_player_sleep_call *c,
                                          mc_entity_tick_execution_site *site) {
    return c != NULL && c->owned != NULL && !c->state.active &&
           mc_world_mutation_execution_source_null(c->owned->mutation, site);
}

const mc_world_notification_execution *
mc_player_wake_execution_failure_observer(const mc_player_sleep_call *c) {
    return c == NULL || c->owned == NULL || c->state.active
               ? NULL
               : mc_world_mutation_execution_failure_observer(c->owned->mutation);
}

const mc_world_mutation_drop_execution *
mc_player_wake_execution_failed_drop(const mc_player_sleep_call *c) {
    return c == NULL || c->owned == NULL || c->state.active
               ? NULL
               : mc_world_mutation_execution_failed_drop(c->owned->mutation);
}

mc_world_storage_status mc_player_wake_execution_native_close(mc_player_sleep_call *c) {
    if (c == NULL || c->owned == NULL)
        return MC_WORLD_STORAGE_OK;
    mc_player_wake_execution *w = c->owned;
    if (c->state.active || wake_records_pending(&w->result))
        return MC_WORLD_STORAGE_BUSY;
    const mc_world_storage_status status = mc_world_mutation_execution_native_close(w->mutation);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    return close_wake_world(w, !c->state.returned);
}

bool mc_player_sleep_inspect(const mc_player_sleep_call *c, mc_player_sleep_state *out) {
    if (c == NULL || out == NULL || c->state.active)
        return false;
    *out = c->state;
    out->failure = &c->error;
    out->cleanup = &c->cleanup;
    return true;
}

static mc_player_sleep_status cleanup_fail(mc_player_sleep_call *c,
                                           mc_player_sleep_cleanup_child child,
                                           const char *operation, mc_player_sleep_cleanup *out) {
    c->cleanup.status = MC_PLAYER_SLEEP_CLEANUP_FAILED;
    c->cleanup.child = child;
    c->cleanup.operation = operation;
    if (out != NULL)
        *out = c->cleanup;
    return c->cleanup.status;
}

mc_player_sleep_status mc_player_sleep_retire(mc_player_sleep_call **slot,
                                              mc_player_sleep_cleanup *out) {
    mc_player_sleep_cleanup empty = {0};
    if (slot == NULL) {
        empty.status = MC_PLAYER_SLEEP_ARGUMENT;
        empty.operation = "Sleep retirement slot";
        if (out != NULL)
            *out = empty;
        return empty.status;
    }
    mc_player_sleep_call *c = *slot;
    if (c == NULL) {
        if (out != NULL)
            *out = empty;
        return MC_PLAYER_SLEEP_OK;
    }
    if (c->state.active) {
        c->cleanup.status = MC_PLAYER_SLEEP_BUSY;
        c->cleanup.operation = "Sleep entered native retirement";
        if (out != NULL)
            *out = c->cleanup;
        return MC_PLAYER_SLEEP_BUSY;
    }
    if (c->owned != NULL) {
        c->cleanup.world.status = mc_player_wake_execution_native_close(c);
        if (c->cleanup.world.status == MC_WORLD_STORAGE_OK)
            c->cleanup.world.status = mc_world_mutation_execution_retire(&c->owned->mutation);
        if (c->cleanup.world.status != MC_WORLD_STORAGE_OK)
            return cleanup_fail(c, MC_PLAYER_SLEEP_CLEANUP_WORLD, "Wake exact native retirement",
                                out);
    }
    if (!capture_query(c)) {
        c->cleanup.acquisition = c->query;
        return cleanup_fail(c, MC_PLAYER_SLEEP_CLEANUP_QUERY, c->cleanup.world.operation, out);
    }
    struct mc_player_nbt_coordinate *pins[] = {c->local4, c->local5, c->late};
    for (size_t i = 0; i < sizeof(pins) / sizeof(*pins); ++i) {
        if (pins[i] == NULL)
            continue;
        size_t count = 0;
        for (size_t j = 0; j < sizeof(pins) / sizeof(*pins); ++j)
            if (pins[j] == pins[i])
                ++count;
        c->cleanup.coordinate = mc_player_coordinate_unpin_admit_held(c->player, pins[i], count);
        if (c->cleanup.coordinate != MC_PLAYER_OK)
            return cleanup_fail(c, MC_PLAYER_SLEEP_CLEANUP_COORDINATE,
                                "Sleep retained coordinate pin metadata", out);
    }
    for (size_t i = 0; i < c->world_count; ++i) {
        size_t count = 0;
        for (size_t j = 0; j < c->world_count; ++j)
            if (c->worlds[j] == c->worlds[i])
                ++count;
        if (c->worlds[i]->persistent_bindings < count) {
            c->cleanup.world = (mc_world_storage_error){
                MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Sleep retained World occurrence metadata", 0};
            return cleanup_fail(c, MC_PLAYER_SLEEP_CLEANUP_WORLD, c->cleanup.world.operation, out);
        }
    }
    if (c->registry != NULL && mc_registry_runtime_borrow_release_admit(
                                   c->registry, &c->cleanup.registry) != MC_REGISTRY_OK)
        return cleanup_fail(c, MC_PLAYER_SLEEP_CLEANUP_REGISTRY,
                            "Sleep defining Registry retirement admission", out);
    if (c->occupancy != NULL && !mc_world_placement_error_retire(c->occupancy)) {
        c->cleanup.occupancy = c->occupancy;
        return cleanup_fail(c, MC_PLAYER_SLEEP_CLEANUP_OCCUPANCY,
                            "Sleep occupancy child checked retirement", out);
    }
    c->error.occupancy = NULL;
    c->cleanup.occupancy = NULL;
    if (c->bed != NULL && !mc_world_bed_spawn_error_retire(c->bed)) {
        c->cleanup.bed = c->bed;
        return cleanup_fail(c, MC_PLAYER_SLEEP_CLEANUP_BED, "Sleep Bed helper checked retirement",
                            out);
    }
    c->error.bed = NULL;
    c->cleanup.bed = NULL;
    if (c->registry != NULL) {
        const mc_registry_status status =
            mc_registry_classes_release(c->registry, &c->cleanup.registry);
        if (status != MC_REGISTRY_OK)
            return cleanup_fail(c, MC_PLAYER_SLEEP_CLEANUP_REGISTRY,
                                "Sleep defining Registry checked retirement", out);
        c->registry = NULL;
    }
    mc_player_coordinate_unpin_commit_held(c->player, &c->local4);
    mc_player_coordinate_unpin_commit_held(c->player, &c->local5);
    mc_player_coordinate_unpin_commit_held(c->player, &c->late);
    if (c->candidate != NULL)
        mc_entity_construction_retire(c->player, c->candidate, sizeof(*c->candidate));
    mc_world_bed_spawn_position_free(c->exit);
    for (size_t i = 0; i < c->receipt_count; ++i)
        mc_world_access_failure_release(&c->receipts[i]);
    free(c->query);
    free(c->source);
    free(c->occupancy);
    free(c->bed);
    while (c->world_count != 0)
        mc_world_runtime_binding_release(c->worlds[--c->world_count]);
    mc_entity_release(c->player);
    free(c->owned);
    free(c);
    *slot = NULL;
    if (out != NULL)
        *out = empty;
    return MC_PLAYER_SLEEP_OK;
}
