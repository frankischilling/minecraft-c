#include "saving_internal.h"

#include "../../chunk/object_state_internal.h"
#include "../../storage/objects_internal.h"
#include "../runtime_internal.h"
#include "../source_construction_internal.h"
#include "mc/entity/nbt/factory.h"
#include "mc/entity/nbt/storage.h"

#include <limits.h>
#include <stdlib.h>

mc_world_game_save_error mc_world_game_save_empty_error(void) {
    mc_world_game_save_error error =
        (mc_world_game_save_error){.status = MC_WORLD_STORAGE_OK,
                                   .cause = {MC_WORLD_STORAGE_OK, NULL, 0},
                                   .entity_slice = SIZE_MAX,
                                   .entity_index = SIZE_MAX,
                                   .entity = mc_entity_nbt_empty_error(),
                                   .tile_entity = mc_live_tile_entities_empty_error()};
    mc_world_player_metadata_error_init(&error.metadata);
    return error;
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
/* These source receivers fail in to.a BEFORE entering an Entity/Tile writer.
 * A typed marker lets the common LD catch mapping preserve that distinction. */
static void source_save_null_receiver(mc_world_game_save_error *frame) {
    frame->source_chunk.source =
        (mc_world_source_error){.status = MC_WORLD_SOURCE_SOURCE_EXCEPTION,
                                .cause_status = MC_WORLD_SOURCE_SOURCE_EXCEPTION,
                                .source_exception = true,
                                .exception_class = "java/lang/NullPointerException"};
}

static mc_world_storage_status entities(mc_world_chunk *chunk, const mc_nbt_limits *limits,
                                        bool source, mc_nbt_tree **out,
                                        mc_world_game_save_error *frame) {
    static const uint16_t name[] = {'E', 'n', 't', 'i', 't', 'i', 'e', 's'};
    entity_record *first = NULL, *last = NULL;
    size_t count = 0, live_bytes = 0;
    const mc_nbt_tag **items = NULL;
    mc_nbt_tree *result = NULL;
    for (size_t slice = 0;; ++slice) {
        frame->serializer_bytecode = 125;
        mc_world_chunk_list_array *array = chunk->m;
        if (array == NULL) {
            if (source)
                source_save_null_receiver(frame);
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
            if (source)
                source_save_null_receiver(frame);
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
                if (source)
                    source_save_null_receiver(frame);
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
                source ? mc_entity_save_nbt(entity, &residual, &serialized, &record, &frame->entity)
                       : mc_entity_save_storage_nbt(entity, &residual, &serialized, &record,
                                                    &frame->entity);
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

static void source_save_step(mc_world_game_save_error *frame) {
    frame->cause = frame->source_chunk.storage;
    if (frame->source_chunk.save_pc > 0)
        frame->region_bytecode = (unsigned)frame->source_chunk.save_pc;
    if (frame->source_chunk.serializer_pc > 0)
        frame->serializer_bytecode = (unsigned)frame->source_chunk.serializer_pc;
}

static void source_save_domain_failure(mc_world_game_save_error *frame) {
    mc_world_source_error *source = &frame->source_chunk.source;
    const bool receiver_null = source->status == MC_WORLD_SOURCE_SOURCE_EXCEPTION &&
                               source->source_exception && !source->diagnostic_frontier &&
                               source->frame_count == 0;
    if (source->status != MC_WORLD_SOURCE_OK && !receiver_null)
        return; /* Preserve a complete concrete ld/stream/session child. */
    frame->source_chunk.storage = frame->cause;
    frame->source_chunk.save_pc = (int)frame->region_bytecode;
    frame->source_chunk.serializer_pc = (int)frame->serializer_bytecode;
    *source = (mc_world_source_error){
        .status = frame->status == MC_WORLD_STORAGE_LIMIT ? MC_WORLD_SOURCE_LIMIT
                  : frame->status == MC_WORLD_STORAGE_ALLOCATION_FAILED
                      ? MC_WORLD_SOURCE_ALLOCATION
                      : MC_WORLD_SOURCE_UNIMPLEMENTED,
        .operation = frame->cause.operation,
        .native_policy = true};
    source->cause_status = source->status;
    source->frames[0] =
        (mc_world_source_site){"ld", "a", "(Lfd;Llm;)V", (int)frame->region_bytecode};
    source->frames[1] =
        (mc_world_source_site){"to", "a", "(Llm;Lfd;Lnu;)V", (int)frame->serializer_bytecode};
    source->frame_count = 2;
    /* Only evidenced returning source bodies may reach LD's catch. A missing
     * Mob/Registry writer or a native shape/allocation/opaque policy does not. */
    const bool entity_null = frame->entity.status == MC_ENTITY_NBT_NULL_REFERENCE;
    const bool spawner_null =
        frame->tile_entity.tile_entity.status == MC_TILE_ENTITY_INVALID_ARGUMENT &&
        frame->tile_entity.tile_entity.save_bytecode == 12;
    if (receiver_null || entity_null || spawner_null) {
        source->cause_status = MC_WORLD_SOURCE_SOURCE_EXCEPTION;
        source->source_exception = true;
        source->native_policy = false;
        source->exception_class =
            spawner_null ? "java/lang/IllegalArgumentException" : "java/lang/NullPointerException";
        source->caught_exception = source->diagnostic_frontier = true;
        source->prior_site = source->frames[0];
        if (spawner_null) {
            source->frames[2] = (mc_world_source_site){"cy", "b", "(Lnu;)V", 12};
            source->frames[3] =
                (mc_world_source_site){"nu", "a", "(Ljava/lang/String;Ljava/lang/String;)V", 10};
            source->frames[4] = (mc_world_source_site){"xb", "<init>", "(Ljava/lang/String;)V", 13};
            source->frame_count = 5;
            source->cause_site = source->frames[4];
        } else if (frame->entity.item_bytecode == 37) {
            source->frames[2] = (mc_world_source_site){"sn", "c", "(Lnu;)Z", 27};
            source->frames[3] = (mc_world_source_site){"sn", "d", "(Lnu;)V", 145};
            source->frames[4] = (mc_world_source_site){"hl", "b", "(Lnu;)V", 37};
            source->frame_count = 5;
            source->cause_site = source->frames[4];
        } else {
            source->cause_site = source->frames[1];
        }
        source->frames[0].pc = 102;
        source->operation = "java/lang/Exception.printStackTrace()V";
        frame->status = MC_WORLD_STORAGE_UNSUPPORTED_STATE;
        frame->cause = (mc_world_storage_error){frame->status, source->operation, 0};
        frame->source_chunk.storage = frame->cause;
    }
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
    mc_world_source_chunk_loader *loader = NULL;
    bool entered = false, info_held = false, source = false, source_body = false;
    size_t source_entity_bytes = 0;
    if (world == NULL || state == NULL || growth == NULL) {
        fail(&frame, MC_WORLD_STORAGE_INVALID_ARGUMENT, "GAME Chunk save arguments");
        goto done;
    }
    if (world->construction.mode != MC_WORLD_RUNTIME_GAME) {
        fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "GAME Chunk save World mode");
        goto done;
    }
    source = mc_world_store_is_source(world->fields.w);
    frame.status = mc_world_runtime_call_enter(world, &frame.cause);
    if (frame.status != MC_WORLD_STORAGE_OK)
        goto done;
    entered = true;
    account = world->fields.x;
    const mc_world_info_status held = mc_world_info_retain(account);
    if (held != MC_WORLD_INFO_OK) {
        fail(&frame,
             held == MC_WORLD_INFO_LIMIT ? MC_WORLD_STORAGE_LIMIT
                                         : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
             "GAME save Info ownership");
        goto done;
    }
    info_held = true;
    if (source) {
        loader = mc_world_runtime_source_loader(world);
        mc_world_source_chunk_loader_state ld;
        if (loader == NULL || !mc_world_source_chunk_loader_inspect(loader, &ld) ||
            ld.store != world->fields.w ||
            ld.regions != mc_world_store_source_regions(world->fields.w)) {
            fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "GAME save canonical source ld owner");
            goto done;
        }
    }
    frame.region_bytecode = 1; /* First actual session check, outside LD catch. */
    frame.status = mc_world_store_game_save_begin_detailed(
        world->fields.w, world, state, account, &save, &view, &frame.cause,
        source ? &frame.source_chunk.source : NULL);
    if (frame.status != MC_WORLD_STORAGE_OK) {
        if (source)
            frame.source_chunk.storage = frame.cause;
        goto done;
    }
    int64_t time = 0;
    if (source) {
        source_body = true;
        frame.region_bytecode = 16;
        frame.status = mc_world_source_chunk_loader_save_begin(
            loader, view.chunk, &view.source_output, &frame.source_chunk);
        source_save_step(&frame);
        if (frame.status != MC_WORLD_STORAGE_OK)
            goto done;
        frame.region_bytecode = 51;
        frame.serializer_bytecode = 1;
        frame.status = mc_world_source_chunk_loader_save_prefix(loader, world, view.chunk,
                                                                &frame.source_chunk);
        source_save_step(&frame);
        if (frame.status != MC_WORLD_STORAGE_OK)
            goto done;
        /* Explicit intermediate native policy. No opaque factory-NULL payload
         * is silently dropped by this fresh source composition. */
        frame.status =
            mc_world_store_game_save_source_records_admit(world->fields.w, save, &frame.cause);
    } else {
        frame.region_bytecode = 51;
        frame.serializer_bytecode = 1;
        frame.status = mc_world_store_check_session(world->fields.w, &frame.cause);
        if (frame.status == MC_WORLD_STORAGE_OK)
            frame.status = source_prefix(world, view.chunk, &time, &frame);
    }
    if (frame.status == MC_WORLD_STORAGE_OK)
        frame.status = entities(view.chunk, &view.limits, source, &entity_records, &frame);
    if (frame.status == MC_WORLD_STORAGE_OK && source) {
        source_entity_bytes = mc_nbt_tree_allocation_bytes(entity_records);
        frame.status = mc_world_source_chunk_loader_save_entities(loader, &entity_records,
                                                                  &frame.source_chunk);
        source_save_step(&frame);
    }
    if (frame.status == MC_WORLD_STORAGE_OK) {
        frame.serializer_bytecode = 224;
        if (view.chunk->l == NULL) {
            if (source)
                source_save_null_receiver(&frame);
            fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "GAME save TileEntity Map values");
        } else {
            mc_nbt_limits residual = view.limits;
            const size_t held_bytes =
                source ? source_entity_bytes : mc_nbt_tree_allocation_bytes(entity_records);
            if (held_bytes > residual.max_allocation_bytes) {
                fail(&frame, MC_WORLD_STORAGE_LIMIT, "GAME save live Entity tree budget");
                goto done;
            }
            residual.max_allocation_bytes -= held_bytes;
            frame.status =
                source ? mc_live_tile_entities_build_source_records(
                             view.chunk, &residual, &tile_entity_records, &frame.tile_entity)
                       : mc_live_tile_entities_build_records(
                             view.chunk, &residual, &tile_entity_records, &frame.tile_entity);
            if (frame.status != MC_WORLD_STORAGE_OK) {
                frame.cause = frame.tile_entity.cause;
                if (frame.tile_entity.bytecode != 0)
                    frame.serializer_bytecode = frame.tile_entity.bytecode;
            }
        }
    }
    if (frame.status == MC_WORLD_STORAGE_OK && source) {
        frame.status = mc_world_source_chunk_loader_save_tiles(loader, &tile_entity_records,
                                                               &frame.source_chunk);
        source_save_step(&frame);
        if (frame.status == MC_WORLD_STORAGE_OK) {
            frame.status = mc_world_source_chunk_loader_save_write(
                loader, world, view.chunk, &view.limits, growth, &frame.source_chunk);
            source_save_step(&frame);
        }
    } else if (frame.status == MC_WORLD_STORAGE_OK) {
        frame.region_bytecode = 57;
        frame.serializer_bytecode = 0;
        frame.status = mc_world_store_game_save_write(
            world->fields.w, save, time, timestamp, entity_records, tile_entity_records, growth,
            &frame.cause, &frame.composition, &frame.encoding);
    }
done:
    if (source_body && frame.status != MC_WORLD_STORAGE_OK) {
        source_save_domain_failure(&frame);
        (void)mc_world_source_chunk_loader_save_fail(loader, &frame.source_chunk);
    }
    mc_nbt_tree_free(tile_entity_records);
    mc_nbt_tree_free(entity_records);
    /* Native Store lease cleanup only. The retained source ld owns every
     * unfinished stream/tree; there is NO serializer finally-close here. */
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
