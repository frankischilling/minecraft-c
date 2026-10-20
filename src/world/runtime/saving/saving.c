#include "saving_internal.h"

#include "../../chunk/object_state_internal.h"
#include "../../storage/objects_internal.h"
#include "../runtime_internal.h"

#include <limits.h>

mc_world_game_save_error mc_world_game_save_empty_error(void)
{
    return (mc_world_game_save_error){.status = MC_WORLD_STORAGE_OK,
                                      .cause = {MC_WORLD_STORAGE_OK, NULL, 0},
                                      .entity_slice = SIZE_MAX,
                                      .entity_index = SIZE_MAX,
                                      .tile = mc_live_tiles_empty_error()};
}

static mc_world_storage_status fail(mc_world_game_save_error *frame, mc_world_storage_status status,
                                    const char *operation)
{
    frame->status = status;
    frame->cause = (mc_world_storage_error){status, operation, 0};
    return status;
}

/* Only actual source dereferences occur here. Nullable byte arrays are not
 * dereferenced by the original put: keep them for the later tree/codec error.
 * The lower composer consumes these same current projections, not canonical
 * substitute arrays. Private mutable-NBT allocation prefixes remain separate
 * from this independent native-result representation. */
static mc_world_storage_status source_prefix(mc_world_runtime *world, mc_world_chunk *chunk,
                                             int64_t *time, mc_world_game_save_error *frame)
{
    frame->serializer_bytecode = 28;
    if (mc_world_info_time_get(world->fields.x, time) != MC_WORLD_INFO_OK)
        return fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "GAME save World time receiver");
    mc_world_chunk_fields fields;
    (void)mc_world_chunk_fields_get(chunk, &fields);
    frame->serializer_bytecode = 51;
    if (fields.e == NULL)
        return fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "GAME save Data nibble receiver");
    frame->serializer_bytecode = 64;
    if (fields.f == NULL)
        return fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                    "GAME save SkyLight nibble receiver");
    frame->serializer_bytecode = 77;
    if (fields.g == NULL)
        return fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                    "GAME save BlockLight nibble receiver");
    /* j/k/b/h/n are current field reads, without nullable receiver calls.
     * The exact held header and its borrowed spans stay unchanged through the
     * subsequent output copy. q105 follows all those source reads. */
    mc_world_chunk_state(chunk)->has_entities = false;
    frame->serializer_bytecode = 105;
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status entities(mc_world_chunk *chunk, mc_world_game_save_error *frame)
{
    frame->serializer_bytecode = 125;
    mc_world_chunk_list_array *array = chunk->m;
    if (array == NULL)
        return fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "GAME save Entity array length");
    if (array->length > CHUNK_ENTITY_SLICES)
        return fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                    "GAME save finite Entity array extent");
    for (size_t slice = 0; slice < array->length; ++slice) {
        frame->entity_slice = slice;
        frame->serializer_bytecode = 136;
        /* The source reloads m for the selected slice. No callback or mutable
         * external operation intervenes, but keep the actual projection. */
        const mc_world_chunk_entity_list *list = chunk->m->items[slice];
        if (list == NULL)
            return fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                        "GAME save Entity slice iterator");
        if (list->count > list->capacity || list->capacity > (size_t)INT32_MAX ||
            list->capacity > SIZE_MAX / sizeof(*list->items) ||
            (list->capacity != 0 && list->items == NULL))
            return fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                        "GAME save Entity List native shape");
        if (list->count != 0) {
            frame->entity_index = 0;
            /* The original first occurrence sets q before its actual virtual
             * serializer, even when that later serializer returns false.
             * Retained opaque Entities are never used as a live replacement. */
            mc_entity *entity = list->items[0];
            mc_world_chunk_state(chunk)->has_entities = true; /* q167 */
            frame->serializer_bytecode = 183;
            return fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                        entity == NULL ? "GAME save Entity receiver"
                                       : "GAME live Entity serializer");
        }
    }
    frame->entity_slice = frame->entity_index = SIZE_MAX;
    frame->serializer_bytecode = 208;
    return MC_WORLD_STORAGE_OK;
}

mc_world_storage_status mc_world_runtime_write_game_chunk(mc_world_runtime *world,
                                                          mc_chunk_state *state, uint32_t timestamp,
                                                          uint32_t *growth,
                                                          mc_world_game_save_error *error)
{
    mc_world_game_save_error frame = mc_world_game_save_empty_error();
    mc_world_store_game_save *save = NULL;
    mc_world_store_game_save_view view = {0};
    mc_nbt_tree *records = NULL;
    mc_world_info *account = NULL;
    bool entered = false, info_held = false;
    if (world == NULL || state == NULL || growth == NULL) {
        fail(&frame, MC_WORLD_STORAGE_INVALID_ARGUMENT, "GAME Chunk save arguments");
        goto done;
    }
    if (world->construction.mode != MC_WORLD_RUNTIME_GAME) {
        fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "GAME Chunk save World mode");
        goto done;
    }
    frame.status = mc_world_runtime_call_enter(world, &frame.cause);
    if (frame.status != MC_WORLD_STORAGE_OK)
        goto done;
    entered = true;
    account = world->fields.x;
    /* Native lifetime/selected-account admission, not the late source Info
     * getter. A GAME save never substitutes convenience Store metadata. */
    const mc_world_info_status held = mc_world_info_retain(account);
    if (held != MC_WORLD_INFO_OK) {
        fail(&frame,
             held == MC_WORLD_INFO_LIMIT ? MC_WORLD_STORAGE_LIMIT
                                         : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
             "GAME save Info ownership");
        goto done;
    }
    info_held = true;
    frame.region_bytecode = 1;
    frame.status = mc_world_store_game_save_begin(world->fields.w, world, state, account, &save,
                                                  &view, &frame.cause);
    if (frame.status != MC_WORLD_STORAGE_OK)
        goto done;

    /* Region51 invokes the shared serializer, which has its own session1.
     * Native buffered transport does not emulate the earlier source stream
     * construction/caught-Exception behavior at Region16..44. */
    frame.region_bytecode = 51;
    frame.serializer_bytecode = 1;
    frame.status = mc_world_store_check_session(world->fields.w, &frame.cause);
    int64_t time = 0;
    if (frame.status == MC_WORLD_STORAGE_OK)
        frame.status = source_prefix(world, view.chunk, &time, &frame);
    if (frame.status == MC_WORLD_STORAGE_OK)
        frame.status = entities(view.chunk, &frame);
    if (frame.status == MC_WORLD_STORAGE_OK) {
        frame.serializer_bytecode = 224;
        if (view.chunk->l == NULL)
            fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "GAME save Tile Map values");
        else {
            frame.status =
                mc_live_tiles_build_records(view.chunk, &view.limits, &records, &frame.tile);
            if (frame.status != MC_WORLD_STORAGE_OK) {
                frame.cause = frame.tile.cause;
                if (frame.tile.bytecode != 0)
                    frame.serializer_bytecode = frame.tile.bytecode;
            }
        }
    }
    if (frame.status == MC_WORLD_STORAGE_OK) {
        frame.region_bytecode = 57;
        frame.serializer_bytecode = 0;
        frame.status =
            mc_world_store_game_save_write(world->fields.w, save, time, timestamp, records, growth,
                                           &frame.cause, &frame.composition, &frame.encoding);
    }
done:
    mc_nbt_tree_free(records);
    if (save != NULL)
        (void)mc_world_store_game_save_end(world->fields.w, save, NULL);
    if (info_held)
        mc_world_info_release(account);
    if (entered)
        mc_world_runtime_call_leave(world);
    if (frame.status == MC_WORLD_STORAGE_OK)
        frame = mc_world_game_save_empty_error();
    if (error != NULL)
        *error = frame;
    return frame.status;
}
