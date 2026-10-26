#include "loading_internal.h"

#include "../../chunk/object_internal.h"
#include "../../storage/objects_internal.h"
#include "../runtime_internal.h"
#include "mc/entity/nbt/storage.h"

#include <limits.h>
#include <string.h>

static void game_load_error_init(mc_world_game_load_error *error) {
    static const mc_world_game_load_error zero = {0};
    *error = zero;
    mc_live_tile_entities_error_init(&error->tile_entity);
    mc_world_entity_load_error_init(&error->entity);
}

static const mc_nbt_tag *field(const mc_nbt_tag *compound, const char *name) {
    uint16_t units[16];
    const size_t length = strlen(name);
    for (size_t index = 0; index < length; ++index)
        units[index] = (uint16_t)(unsigned char)name[index];
    return mc_nbt_compound_get(compound, units, length);
}

static mc_world_storage_status fail(mc_world_game_load_error *frame, mc_world_storage_status status,
                                    const char *operation) {
    frame->cause = (mc_world_storage_error){status, operation, 0};
    return status;
}

static mc_world_storage_status entity_failure(mc_world_game_load_error *frame,
                                              mc_entity_nbt_status status, const char *operation) {
    return fail(frame,
                status == MC_ENTITY_NBT_LIMIT               ? MC_WORLD_STORAGE_LIMIT
                : status == MC_ENTITY_NBT_ALLOCATION_FAILED ? MC_WORLD_STORAGE_ALLOCATION_FAILED
                                                            : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                operation);
}

static mc_world_storage_status import_entities(mc_world_runtime *world, mc_world_store *store,
                                               mc_world_store_game_stage *stage,
                                               const mc_world_store_game_stage_view *view,
                                               mc_world_game_load_error *frame) {
    frame->builder_bytecode = 231;
    const mc_nbt_tag *records = field(view->level, "Entities");
    if (records != NULL && records->type != MC_NBT_LIST)
        return fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "GAME Entities list getter");
    frame->builder_bytecode = 248;
    const size_t count = records == NULL ? 0 : records->value.list.length;
    if (count > (size_t)INT32_MAX)
        return fail(frame, MC_WORLD_STORAGE_LIMIT, "GAME Entities list length");
    for (size_t index = 0; index < count; ++index) {
        mc_world_entity_load_error_init(&frame->entity);
        frame->entity.index = index;
        frame->builder_bytecode = 258;
        if (records->value.list.items == NULL)
            return fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                        "GAME Entity List native storage");
        const mc_nbt_tag *record = records->value.list.items[index];
        frame->builder_bytecode = 261;
        /* Java checkcast accepts NULL; the actual factory owns that later
         * getter failure and its separate catch/read diagnostics. */
        if (record != NULL && record->type != MC_NBT_COMPOUND)
            return fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "GAME Entity compound cast");
        frame->builder_bytecode = 269;
        mc_entity *entity = NULL;
        mc_entity_nbt_status child = mc_entity_factory_nbt(mc_world_runtime_entities(world), world,
                                                           record, &entity, &frame->entity.factory);
        if (child != MC_ENTITY_NBT_OK)
            return entity_failure(frame, child, "GAME Entity NBT factory");

        /* Every normal factory result sets q, including an unknown-id NULL.
         * Direct Chunk adoption does not publish into World.b or invoke the
         * provider. Its actual membership stores and hold precede provenance. */
        frame->builder_bytecode = 277;
        view->state->has_entities = true;
        mc_world_storage_status status = MC_WORLD_STORAGE_OK;
        if (entity != NULL) {
            frame->builder_bytecode = 289;
            status = mc_world_chunk_insert_entity(view->chunk, entity, &frame->entity.insertion);
            if (status != MC_WORLD_STORAGE_OK)
                frame->cause = frame->entity.insertion.cause;
            else {
                child = mc_entity_preserve_source(entity, record, &view->limits,
                                                  &frame->entity.provenance);
                if (child != MC_ENTITY_NBT_OK)
                    status = entity_failure(frame, child, "GAME Entity native provenance");
            }
        }
        if (status == MC_WORLD_STORAGE_OK) {
            status = mc_world_store_game_stage_record_entity_result(
                store, stage, index, entity != NULL, &frame->entity.native);
            if (status != MC_WORLD_STORAGE_OK)
                frame->cause = frame->entity.native;
        }
        /* The factory-local hold spans adoption AND independent provenance.
         * A late failure keeps the Chunk's real hold and completed source
         * effects; only this local hold is balanced here, exactly once. */
        mc_entity_release(entity);
        if (status != MC_WORLD_STORAGE_OK)
            return status;
    }
    mc_world_entity_load_error_init(&frame->entity);
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status import_tile_entities(const mc_world_store_game_stage_view *view,
                                                    mc_world_game_load_error *frame) {
    frame->builder_bytecode = 301;
    const mc_nbt_tag *records = field(view->level, "TileEntities");
    if (records != NULL && records->type != MC_NBT_LIST)
        return fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "GAME TileEntities list getter");
    frame->builder_bytecode = 318;
    const size_t count = records == NULL ? 0 : records->value.list.length;
    if (count > (size_t)INT32_MAX)
        return fail(frame, MC_WORLD_STORAGE_LIMIT, "GAME TileEntities list length");
    /* The actual empty loop performs no factory/import or Map allocation.
     * In particular, it does not invent a provenance journal for zero records. */
    if (count == 0)
        return MC_WORLD_STORAGE_OK;
    const mc_world_storage_status status = mc_live_tile_entities_load_records(
        view->chunk, records, &view->limits, &frame->tile_entity);
    if (status != MC_WORLD_STORAGE_OK) {
        frame->builder_bytecode = frame->tile_entity.bytecode;
        frame->cause = frame->tile_entity.cause;
    }
    return status;
}

mc_world_storage_status mc_world_runtime_load_game(mc_world_runtime *world, int32_t x, int32_t z,
                                                   mc_chunk_state **out,
                                                   mc_world_game_load_error *error) {
    mc_world_game_load_error frame;
    game_load_error_init(&frame);
    if (world == NULL || out == NULL) {
        frame.status = fail(&frame, MC_WORLD_STORAGE_INVALID_ARGUMENT, "GAME load arguments");
        if (error != NULL)
            *error = frame;
        return frame.status;
    }
    if (world->construction.mode != MC_WORLD_RUNTIME_GAME) {
        frame.status = fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "GAME load World mode");
        if (error != NULL)
            *error = frame;
        return frame.status;
    }
    frame.status = mc_world_runtime_call_enter(world, &frame.cause);
    if (frame.status != MC_WORLD_STORAGE_OK) {
        if (error != NULL)
            *error = frame;
        return frame.status;
    }
    mc_world_store *store = world->fields.w;
    mc_world_store_game_stage *stage = NULL;
    mc_world_store_game_stage_view view = {0};
    mc_chunk_state *state = NULL;
    bool committed = false;
    frame.status = mc_world_store_game_stage_begin(store, world, world->options.dimension, x, z,
                                                   &stage, &frame.cause);
    if (frame.status == MC_WORLD_STORAGE_OK)
        frame.status = mc_world_store_game_stage_view_get(store, stage, &view, &frame.cause);
    if (frame.status == MC_WORLD_STORAGE_OK)
        frame.status = mc_world_store_game_stage_imports_begin(store, stage, &frame.cause);
    if (frame.status == MC_WORLD_STORAGE_OK) {
        frame.region_bytecode = 143;
        frame.status = import_entities(world, store, stage, &view, &frame);
    }
    if (frame.status == MC_WORLD_STORAGE_OK)
        frame.status = import_tile_entities(&view, &frame);
    if (frame.status == MC_WORLD_STORAGE_OK) {
        mc_world_chunk_fields fields;
        frame.region_bytecode = 152;
        frame.builder_bytecode = 0;
        if (mc_world_chunk_fields_get(view.chunk, &fields) != MC_WORLD_STORAGE_OK)
            frame.status = fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                "GAME loaded Chunk coordinate receiver");
        else if (fields.j != x || fields.k != z) {
            /* The first builder and all imports have completed. The reached
             * relocation diagnostic/root edits/second builder remain absent;
             * do not normalize or silently replace the first returned owner. */
            frame.region_bytecode = 158;
            frame.status = fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                "GAME loaded Chunk relocation diagnostic");
        }
    }
    if (frame.status == MC_WORLD_STORAGE_OK) {
        frame.region_bytecode = 272;
        frame.builder_bytecode = 0;
        frame.status = mc_world_store_game_stage_finish(store, stage, &state, &frame.cause);
        committed = frame.status == MC_WORLD_STORAGE_OK;
    }
    if (stage != NULL && !committed)
        (void)mc_world_store_game_stage_fail(store, stage, frame.status, NULL);
    if (committed) {
        *out = state;
        game_load_error_init(&frame);
    }
    mc_world_runtime_call_leave(world);
    if (error != NULL)
        *error = frame;
    return frame.status;
}
