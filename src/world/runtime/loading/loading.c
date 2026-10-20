#include "loading_internal.h"

#include "../../storage/objects_internal.h"
#include "../runtime_internal.h"

#include <limits.h>
#include <string.h>

static const mc_nbt_tag *field(const mc_nbt_tag *compound, const char *name)
{
    uint16_t units[16];
    const size_t length = strlen(name);
    for (size_t index = 0; index < length; ++index)
        units[index] = (uint16_t)(unsigned char)name[index];
    return mc_nbt_compound_get(compound, units, length);
}

static mc_world_storage_status fail(mc_world_game_load_error *frame, mc_world_storage_status status,
                                    const char *operation)
{
    frame->cause = (mc_world_storage_error){status, operation, 0};
    return status;
}

static mc_world_storage_status import_entities(const mc_nbt_tag *level,
                                               mc_world_game_load_error *frame)
{
    frame->builder_bytecode = 231;
    const mc_nbt_tag *records = field(level, "Entities");
    if (records != NULL && records->type != MC_NBT_LIST)
        return fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "GAME Entities list getter");
    frame->builder_bytecode = 248;
    const size_t count = records == NULL ? 0 : records->value.list.length;
    if (count > (size_t)INT32_MAX)
        return fail(frame, MC_WORLD_STORAGE_LIMIT, "GAME Entities list length");
    if (count != 0) {
        frame->builder_bytecode = 261;
        const mc_nbt_tag *record = records->value.list.items[0];
        if (record == NULL || record->type != MC_NBT_COMPOUND)
            return fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "GAME Entity compound cast");
        frame->builder_bytecode = 269;
        /* A constructor alone does not provide class lookup or semantic NBT
         * read. Stop at the reached factory, before source q=true277. */
        return fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "GAME Entity NBT factory");
    }
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status import_tiles(const mc_world_store_game_stage_view *view,
                                            mc_world_game_load_error *frame)
{
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
    const mc_world_storage_status status =
        mc_live_tiles_load_records(view->chunk, records, &view->limits, &frame->tile);
    if (status != MC_WORLD_STORAGE_OK) {
        frame->builder_bytecode = frame->tile.bytecode;
        frame->cause = frame->tile.cause;
    }
    return status;
}

mc_world_storage_status mc_world_runtime_load_game(mc_world_runtime *world, int32_t x, int32_t z,
                                                   mc_chunk_state **out,
                                                   mc_world_game_load_error *error)
{
    mc_world_game_load_error frame = {.status = MC_WORLD_STORAGE_OK,
                                      .cause = {MC_WORLD_STORAGE_OK, NULL, 0},
                                      .tile = mc_live_tiles_empty_error()};
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
        frame.status = import_entities(view.level, &frame);
    }
    if (frame.status == MC_WORLD_STORAGE_OK)
        frame.status = import_tiles(&view, &frame);
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
        frame = (mc_world_game_load_error){.status = MC_WORLD_STORAGE_OK,
                                           .cause = {MC_WORLD_STORAGE_OK, NULL, 0},
                                           .tile = mc_live_tiles_empty_error()};
    }
    mc_world_runtime_call_leave(world);
    if (error != NULL)
        *error = frame;
    return frame.status;
}
