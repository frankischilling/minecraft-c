#include "mc/world/runtime/loading/player.h"

#include "../../../entity/entity_internal.h"
#include "../chunk_provider_internal.h"
#include "../construction_internal.h"
#include "../runtime_internal.h"
#include "mc/entity/nbt/nbt.h"

#include <stdlib.h>

void mc_world_player_load_error_init(mc_world_player_load_error *error) {
    static const mc_world_player_load_error zero = {0};
    if (error != NULL) {
        *error = zero;
        mc_entity_nbt_error_init(&error->nbt);
    }
}

static void fail(mc_world_player_load_error *frame, mc_world_storage_status status,
                 const char *operation) {
    frame->status = status;
    frame->operation = operation;
    frame->cause = (mc_world_storage_error){status, operation, 0};
    frame->native_policy = true;
}

static void caught(mc_world_player_load_error *frame, const char *exception_class) {
    frame->source_exception = true;
    frame->caught_exception = true;
    frame->exception_class = exception_class;
    frame->catch_bytecode = 88;
    frame->print_stack_trace_pending = true;
    frame->native_policy = false;
    /* The original cause and reached child remain complete. Printing is a
     * separate real dependency, not a successful catch/return substitute. */
}

static void null_receiver(mc_world_player_load_error *frame, const char *operation) {
    fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, operation);
    caught(frame, "java/lang/NullPointerException");
}

static mc_world_storage_status ownership_status(mc_entity_status status) {
    return status == MC_ENTITY_LIMIT               ? MC_WORLD_STORAGE_LIMIT
           : status == MC_ENTITY_ALLOCATION_FAILED ? MC_WORLD_STORAGE_ALLOCATION_FAILED
           : status == MC_ENTITY_BUSY              ? MC_WORLD_STORAGE_BUSY
                                                   : MC_WORLD_STORAGE_INVALID_ARGUMENT;
}

static const char *nbt_exception(const mc_entity_nbt_error *error) {
    switch (error->status) {
        case MC_ENTITY_NBT_NULL_REFERENCE:
            return "java/lang/NullPointerException";
        case MC_ENTITY_NBT_WRONG_TAG:
            return "java/lang/ClassCastException";
        case MC_ENTITY_NBT_INDEX_OUT_OF_BOUNDS:
            return "java/lang/IndexOutOfBoundsException";
        case MC_ENTITY_NBT_STACK_FAILED:
            return error->late_hook == MC_ENTITY_NBT_LATE_PLAYER &&
                           error->player_inventory.stack.status == MC_ITEM_STACK_INVALID_FIELD
                       ? "java/lang/ClassCastException"
                       : NULL;
        case MC_ENTITY_NBT_REGISTRY_FAILED:
            if (error->late_hook != MC_ENTITY_NBT_LATE_PLAYER)
                return NULL;
            if (error->player_inventory.registry.status == MC_REGISTRY_NULL_REFERENCE)
                return "java/lang/NullPointerException";
            if (error->player_inventory.registry.status == MC_REGISTRY_INDEX_OUT_OF_RANGE)
                return "java/lang/ArrayIndexOutOfBoundsException";
            return NULL;
        default:
            return NULL;
    }
}

mc_world_storage_status mc_world_runtime_load_player(mc_world_runtime *world, mc_entity *player,
                                                     mc_world_player_load_error *error) {
    /* Registry and WorldAccess callbacks can reenter the real World. Never
     * retain this complete diagnostic workspace in every ancestor's C stack. */
    mc_world_player_load_error *frame = malloc(sizeof(*frame));
    if (frame == NULL) {
        mc_world_player_load_error_init(error);
        if (error != NULL)
            fail(error, MC_WORLD_STORAGE_ALLOCATION_FAILED, "World Player load workspace");
        return MC_WORLD_STORAGE_ALLOCATION_FAILED;
    }
    mc_world_player_load_error_init(frame);
    bool entered = false, player_held = false;
    mc_world_info *info = NULL, *clear_info = NULL;
    mc_world_info_player *tag = NULL;

    if (world == NULL) {
        frame->bytecode = 1;
        null_receiver(frame, "World Player load World receiver");
        goto done;
    }
    if (world->construction.mode != MC_WORLD_RUNTIME_GAME ||
        world->construction.phase == MC_WORLD_RUNTIME_RETIRED || world->classes == NULL ||
        world->classes->side != MC_WORLD_INFO_CLIENT) {
        fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World Player load client GAME owner");
        goto done;
    }
    if (player != NULL && (!mc_entity_is_kind(player, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER) ||
                           player->classes != world->classes->entities ||
                           player->classes->profile != world->classes->profile)) {
        fail(frame, MC_WORLD_STORAGE_INVALID_ARGUMENT, "World Player load application Player");
        goto done;
    }
    frame->status = mc_world_runtime_call_enter(world, &frame->cause);
    if (frame->status != MC_WORLD_STORAGE_OK) {
        frame->operation = frame->cause.operation;
        frame->native_policy = true;
        goto done;
    }
    entered = true;
    if (player != NULL) {
        frame->entity.status = mc_entity_retain(player);
        if (frame->entity.status != MC_ENTITY_OK) {
            frame->entity.operation = "World Player load native Player hold";
            fail(frame, ownership_status(frame->entity.status), frame->entity.operation);
            goto done;
        }
        player_held = true;
    }

    frame->bytecode = 4;
    info = world->fields.x; /* Source getfield1; not a cached Info across22. */
    if (info == NULL) {
        null_receiver(frame, "World Player load Info Player receiver");
        goto done;
    }
    frame->info = mc_world_info_retain(info);
    if (frame->info != MC_WORLD_INFO_OK) {
        info = NULL;
        fail(frame, MC_WORLD_STORAGE_LIMIT, "World Player load captured Info hold");
        goto done;
    }
    mc_world_info_player *borrowed = NULL;
    frame->info = mc_world_info_player_get(info, &borrowed);
    if (frame->info != MC_WORLD_INFO_OK) {
        fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World Player load Info Player field");
        goto done;
    }
    frame->saved_data_present = borrowed != NULL;
    if (borrowed != NULL) {
        frame->info = mc_world_info_player_retain(borrowed);
        if (frame->info != MC_WORLD_INFO_OK) {
            fail(frame, MC_WORLD_STORAGE_LIMIT, "World Player load captured Player tag hold");
            goto done;
        }
        tag = borrowed;
        frame->bytecode = 14;
        if (player == NULL) {
            null_receiver(frame, "World Player load Player NBT receiver");
            goto done;
        }
        const mc_entity_nbt_status loaded = mc_entity_read_nbt_in_context(
            player, world->classes->registry, mc_world_info_player_root(tag), NULL, &frame->nbt);
        if (loaded != MC_ENTITY_NBT_OK) {
            fail(frame,
                 loaded == MC_ENTITY_NBT_LIMIT               ? MC_WORLD_STORAGE_LIMIT
                 : loaded == MC_ENTITY_NBT_ALLOCATION_FAILED ? MC_WORLD_STORAGE_ALLOCATION_FAILED
                                                             : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                 frame->nbt.operation);
            const char *exception_class = nbt_exception(&frame->nbt);
            if (exception_class != NULL)
                caught(frame, exception_class);
            goto done;
        }
        frame->nbt_returned = true;
        frame->bytecode = 22;
        clear_info = world->fields.x; /* Actual fresh getfield18 after read14. */
        if (clear_info == NULL) {
            null_receiver(frame, "World Player load fresh Info clear receiver");
            goto done;
        }
        frame->info = mc_world_info_retain(clear_info);
        if (frame->info != MC_WORLD_INFO_OK) {
            clear_info = NULL;
            fail(frame, MC_WORLD_STORAGE_LIMIT, "World Player load fresh Info hold");
            goto done;
        }
        frame->info = mc_world_info_player_set(clear_info, NULL);
        if (frame->info != MC_WORLD_INFO_OK) {
            fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World Player load Info Player clear");
            goto done;
        }
        frame->info_cleared = true;
    }

    frame->bytecode = 29;
    const mc_chunk_provider_concrete_kind kind =
        mc_chunk_provider_concrete_kind_of(world->fields.v);
    if (kind != MC_CHUNK_PROVIDER_NULL && kind != MC_CHUNK_PROVIDER_MAP_CACHE) {
        fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World Player load kx concrete dependency");
        goto done;
    }
    /* Genuine ok is unrelated to kx. No cache-policy/grid/coordinate heuristic
     * makes that instanceof true or acknowledges its unimplemented call74. */
    frame->bytecode = 79;
    bool ignored;
    frame->status = mc_world_runtime_insert_entity(world, player, &ignored, &frame->publication);
    if (frame->status != MC_WORLD_STORAGE_OK) {
        frame->operation = frame->publication.operation;
        frame->cause = frame->publication.cause;
        frame->native_policy = true;
        const char *exception_class = NULL;
        if ((player == NULL && frame->publication.bytecode == 1) ||
            mc_world_access_failure_source_exception(&frame->publication.world_access_failure,
                                                     &exception_class))
            caught(frame,
                   exception_class == NULL ? "java/lang/NullPointerException" : exception_class);
        goto done;
    }
    frame->publication_returned = true;
    frame->bytecode = 91;
    frame->source_returned = true;
done:
    mc_world_info_player_release(tag);
    mc_world_info_release(clear_info);
    mc_world_info_release(info);
    if (player_held)
        mc_entity_release(player);
    if (entered) {
        world->last_error = frame->cause;
        mc_world_runtime_call_leave(world);
    }
    const mc_world_storage_status status = frame->status;
    if (error != NULL)
        *error = *frame;
    free(frame);
    return status;
}
