#include "saving_internal.h"

#include "../../chunk/object_state_internal.h"
#include "../../storage/objects_internal.h"
#include "../runtime_internal.h"
#include "mc/entity/nbt/storage.h"

#include <limits.h>
#include <stdlib.h>

mc_world_game_save_error mc_world_game_save_empty_error(void) {
    return (mc_world_game_save_error){.status = MC_WORLD_STORAGE_OK,
                                      .cause = {MC_WORLD_STORAGE_OK, NULL, 0},
                                      .entity_slice = SIZE_MAX,
                                      .entity_index = SIZE_MAX,
                                      .entity = mc_entity_nbt_empty_error(),
                                      .tile_entity = mc_live_tile_entities_empty_error()};
}

static mc_world_storage_status fail(mc_world_game_save_error *frame, mc_world_storage_status status,
                                    const char *operation) {
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
                                             int64_t *time, mc_world_game_save_error *frame) {
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

typedef struct entity_record {
    mc_nbt_tree *tree;
    struct entity_record *next;
} entity_record;

static mc_world_storage_status record_failure(mc_world_game_save_error *frame, mc_nbt_status status,
                                              const char *operation) {
    frame->entity_records.status = status;
    return fail(frame,
                status == MC_NBT_LIMIT               ? MC_WORLD_STORAGE_LIMIT
                : status == MC_NBT_ALLOCATION_FAILED ? MC_WORLD_STORAGE_ALLOCATION_FAILED
                                                     : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                operation);
}

/* Do not pre-count/filter source occurrences. Each true semantic call owns an
 * independent tree and one real node; false continues without allocation. The
 * final list-pointer scratch, all nodes/trees and new list tree overlap under
 * this operation's ceiling. The storage wrapper additionally charges its own
 * current Entity/Stack/provenance graph, not unrelated World/container owners. */
static mc_world_storage_status entities(mc_world_chunk *chunk, const mc_nbt_limits *limits,
                                        mc_nbt_tree **out, mc_world_game_save_error *frame) {
    static const uint16_t name[] = {'E', 'n', 't', 'i', 't', 'i', 'e', 's'};
    entity_record *first = NULL, *last = NULL;
    size_t count = 0, live_bytes = 0;
    const mc_nbt_tag **items = NULL;
    mc_nbt_tree *result = NULL;
    for (size_t slice = 0;; ++slice) {
        frame->serializer_bytecode = 125;
        mc_world_chunk_list_array *array = chunk->m;
        if (array == NULL) {
            fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "GAME save Entity array length");
            goto done;
        }
        if (array->length > CHUNK_ENTITY_SLICES) {
            fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "GAME save finite Entity array extent");
            goto done;
        }
        if (slice >= array->length)
            break;
        frame->entity_slice = slice;
        frame->entity_index = SIZE_MAX;
        frame->serializer_bytecode = 136;
        /* The source reloads m for the selected slice. No callback or mutable
         * external operation intervenes, but keep the actual projection. */
        const mc_world_chunk_entity_list *list = chunk->m->items[slice];
        if (list == NULL) {
            fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "GAME save Entity slice iterator");
            goto done;
        }
        if (list->count > list->capacity || list->capacity > (size_t)INT32_MAX ||
            list->capacity > SIZE_MAX / sizeof(*list->items) ||
            (list->capacity != 0 && list->items == NULL)) {
            fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "GAME save Entity List native shape");
            goto done;
        }
        for (size_t index = 0; index < list->count; ++index) {
            frame->entity_index = index;
            frame->entity = mc_entity_nbt_empty_error();
            mc_entity *entity = list->items[index];
            mc_world_chunk_state(chunk)->has_entities = true; /* q167 */
            frame->serializer_bytecode = 183;
            if (entity == NULL) {
                /* The source receiver fails before entering c; no invented
                 * inner wrapper/name PC or public native argument call. */
                fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "GAME save Entity receiver");
                goto done;
            }
            mc_nbt_limits residual = *limits;
            residual.max_allocation_bytes = live_bytes <= limits->max_allocation_bytes
                                                ? limits->max_allocation_bytes - live_bytes
                                                : 0;
            bool serialized = false;
            mc_nbt_tree *record = NULL;
            const mc_entity_nbt_status saved =
                mc_entity_save_storage_nbt(entity, &residual, &serialized, &record, &frame->entity);
            if (saved != MC_ENTITY_NBT_OK) {
                fail(frame,
                     saved == MC_ENTITY_NBT_LIMIT ? MC_WORLD_STORAGE_LIMIT
                     : saved == MC_ENTITY_NBT_ALLOCATION_FAILED
                         ? MC_WORLD_STORAGE_ALLOCATION_FAILED
                         : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                     "GAME live Entity serializer");
                goto done;
            }
            if (!serialized)
                continue;
            frame->serializer_bytecode = 192;
            const size_t bytes = mc_nbt_tree_allocation_bytes(record);
            if (count >= (size_t)INT32_MAX || count >= limits->max_elements ||
                live_bytes > limits->max_allocation_bytes ||
                bytes > limits->max_allocation_bytes - live_bytes ||
                sizeof(entity_record) > limits->max_allocation_bytes - live_bytes - bytes) {
                mc_nbt_tree_free(record);
                record_failure(frame, MC_NBT_LIMIT, "GAME Entity record append budget");
                goto done;
            }
            entity_record *node = malloc(sizeof(*node));
            if (node == NULL) {
                mc_nbt_tree_free(record);
                record_failure(frame, MC_NBT_ALLOCATION_FAILED, "GAME Entity record node");
                goto done;
            }
            *node = (entity_record){.tree = record};
            if (last == NULL)
                first = node;
            else
                last->next = node;
            last = node;
            ++count;
            live_bytes += bytes + sizeof(*node);
        }
    }
    frame->entity_slice = frame->entity_index = SIZE_MAX;
    frame->entity = mc_entity_nbt_empty_error();
    frame->serializer_bytecode = 208;
    if (count > SIZE_MAX / sizeof(*items) || live_bytes > limits->max_allocation_bytes ||
        count * sizeof(*items) > limits->max_allocation_bytes - live_bytes) {
        record_failure(frame, MC_NBT_LIMIT, "GAME Entity list pointer budget");
        goto done;
    }
    const size_t pointer_bytes = count * sizeof(*items);
    if (count != 0) {
        items = malloc(pointer_bytes);
        if (items == NULL) {
            record_failure(frame, MC_NBT_ALLOCATION_FAILED, "GAME Entity list pointers");
            goto done;
        }
        size_t index = 0;
        for (const entity_record *node = first; node != NULL; node = node->next)
            items[index++] = mc_nbt_tree_root(node->tree);
    }
    const mc_nbt_tag list = {
        .type = MC_NBT_LIST,
        .name = {name, sizeof(name) / sizeof(*name)},
        .value.list = {.element_type = MC_NBT_COMPOUND, .items = items, .length = count}};
    const mc_nbt_tag *fields[] = {&list};
    const mc_nbt_tag root = {.type = MC_NBT_COMPOUND,
                             .value.compound = {fields, sizeof(fields) / sizeof(*fields)}};
    mc_nbt_limits residual = *limits;
    residual.max_allocation_bytes -= live_bytes + pointer_bytes;
    const mc_nbt_status copied =
        mc_nbt_tree_copy_compound(&root, &residual, &result, &frame->entity_records);
    if (copied != MC_NBT_OK)
        record_failure(frame, copied, "GAME Entity independent list tree");
done:
    free(items);
    while (first != NULL) {
        entity_record *next = first->next;
        mc_nbt_tree_free(first->tree);
        free(first);
        first = next;
    }
    if (frame->status == MC_WORLD_STORAGE_OK)
        *out = result;
    else
        mc_nbt_tree_free(result);
    return frame->status;
}

mc_world_storage_status mc_world_runtime_write_game_chunk(mc_world_runtime *world,
                                                          mc_chunk_state *state, uint32_t timestamp,
                                                          uint32_t *growth,
                                                          mc_world_game_save_error *error) {
    mc_world_game_save_error frame = mc_world_game_save_empty_error();
    mc_world_store_game_save *save = NULL;
    mc_world_store_game_save_view view = {0};
    mc_nbt_tree *entity_records = NULL, *tile_entity_records = NULL;
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
        frame.status = entities(view.chunk, &view.limits, &entity_records, &frame);
    if (frame.status == MC_WORLD_STORAGE_OK) {
        frame.serializer_bytecode = 224;
        if (view.chunk->l == NULL)
            fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "GAME save TileEntity Map values");
        else {
            mc_nbt_limits residual = view.limits;
            const size_t held_bytes = mc_nbt_tree_allocation_bytes(entity_records);
            if (held_bytes > residual.max_allocation_bytes) {
                fail(&frame, MC_WORLD_STORAGE_LIMIT, "GAME save live Entity tree budget");
                goto done;
            }
            residual.max_allocation_bytes -= held_bytes;
            frame.status = mc_live_tile_entities_build_records(
                view.chunk, &residual, &tile_entity_records, &frame.tile_entity);
            if (frame.status != MC_WORLD_STORAGE_OK) {
                frame.cause = frame.tile_entity.cause;
                if (frame.tile_entity.bytecode != 0)
                    frame.serializer_bytecode = frame.tile_entity.bytecode;
            }
        }
    }
    if (frame.status == MC_WORLD_STORAGE_OK) {
        frame.region_bytecode = 57;
        frame.serializer_bytecode = 0;
        frame.status = mc_world_store_game_save_write(
            world->fields.w, save, time, timestamp, entity_records, tile_entity_records, growth,
            &frame.cause, &frame.composition, &frame.encoding);
    }
done:
    mc_nbt_tree_free(tile_entity_records);
    mc_nbt_tree_free(entity_records);
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
