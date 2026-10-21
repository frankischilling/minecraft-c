#include "mc/entity/nbt/storage.h"

#include "../entity_internal.h"
#include "mc/storage/nbt/overlay.h"

#include <stdlib.h>

static const uint16_t item_units[] = {'I', 't', 'e', 'm'};

static mc_entity_nbt_status publish(mc_entity_nbt_error frame, mc_entity_nbt_error *error) {
    if (error != NULL)
        *error = frame;
    return frame.status;
}

static mc_entity_nbt_status fail(mc_entity_nbt_error *frame, mc_entity_nbt_status status,
                                 const char *operation) {
    frame->status = status;
    frame->operation = operation;
    return status;
}

static mc_entity_nbt_status nbt_failure(mc_entity_nbt_error *frame, mc_nbt_status status,
                                        const char *operation) {
    return fail(frame,
                status == MC_NBT_LIMIT               ? MC_ENTITY_NBT_LIMIT
                : status == MC_NBT_ALLOCATION_FAILED ? MC_ENTITY_NBT_ALLOCATION_FAILED
                                                     : MC_ENTITY_NBT_NBT_FAILED,
                operation);
}

static mc_entity_nbt_status stack_failure(mc_entity_nbt_error *frame, mc_item_stack_status status,
                                          const char *operation) {
    frame->stack.status = status;
    frame->field = "Item";
    return fail(frame,
                status == MC_ITEM_STACK_LIMIT               ? MC_ENTITY_NBT_LIMIT
                : status == MC_ITEM_STACK_ALLOCATION_FAILED ? MC_ENTITY_NBT_ALLOCATION_FAILED
                                                            : MC_ENTITY_NBT_STACK_FAILED,
                operation);
}

static bool add_size(size_t *sum, size_t bytes) {
    if (bytes > SIZE_MAX - *sum)
        return false;
    *sum += bytes;
    return true;
}

mc_entity_nbt_status mc_entity_storage_native_bytes(const mc_entity *entity, size_t *out,
                                                    mc_entity_nbt_error *error) {
    mc_entity_nbt_error frame = mc_entity_nbt_empty_error();
    if (entity == NULL || out == NULL) {
        fail(&frame, MC_ENTITY_NBT_INVALID_ARGUMENT, "Entity storage byte inspection arguments");
        return publish(frame, error);
    }
    size_t bytes = mc_entity_native_bytes(entity);
    if (!add_size(&bytes, mc_nbt_tree_allocation_bytes(entity->storage_source))) {
        fail(&frame, MC_ENTITY_NBT_LIMIT, "Entity storage source byte extent");
        return publish(frame, error);
    }
    if (entity->stack != NULL) {
        size_t stack_bytes;
        const mc_item_stack_status inspected =
            mc_item_stack_native_bytes(entity->stack, &stack_bytes);
        if (inspected != MC_ITEM_STACK_OK) {
            stack_failure(&frame, inspected, "Entity storage Stack byte inspection");
            return publish(frame, error);
        }
        if (!add_size(&bytes, stack_bytes)) {
            fail(&frame, MC_ENTITY_NBT_LIMIT, "Entity storage Stack byte extent");
            return publish(frame, error);
        }
    }
    *out = bytes;
    return publish(frame, error);
}

const mc_nbt_tag *mc_entity_storage_source(const mc_entity *entity) {
    return entity == NULL ? NULL : mc_nbt_tree_root(entity->storage_source);
}

mc_entity_nbt_status mc_entity_preserve_source(mc_entity *entity, const mc_nbt_tag *compound,
                                               const mc_nbt_limits *requested,
                                               mc_entity_nbt_error *error) {
    mc_entity_nbt_error frame = mc_entity_nbt_empty_error();
    if (entity == NULL || compound == NULL || compound->type != MC_NBT_COMPOUND) {
        fail(&frame, MC_ENTITY_NBT_INVALID_ARGUMENT, "Entity storage provenance arguments");
        return publish(frame, error);
    }
    const bool item_entity = mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_ITEM);
    if (!item_entity && !mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_FALLING_SAND)) {
        fail(&frame, MC_ENTITY_NBT_UNSUPPORTED_TYPE, "Entity storage provenance concrete type");
        return publish(frame, error);
    }
    if (item_entity && entity->stack == NULL) {
        frame.field = "Item";
        fail(&frame, MC_ENTITY_NBT_NULL_REFERENCE, "Entity storage provenance current Stack");
        return publish(frame, error);
    }
    mc_item_stack_status item_status = MC_ITEM_STACK_OK;
    if (item_entity) {
        mc_item_stack_phase phase;
        item_status = mc_item_stack_phase_get(entity->stack, &phase);
        if (item_status != MC_ITEM_STACK_OK || phase != MC_ITEM_STACK_READY) {
            if (item_status == MC_ITEM_STACK_OK)
                item_status = MC_ITEM_STACK_INVALID_PHASE;
            stack_failure(&frame, item_status, "Entity storage provenance successfully read Stack");
            return publish(frame, error);
        }
    }
    mc_nbt_limits limits = requested == NULL ? mc_nbt_default_limits() : *requested;
    const size_t ceiling = limits.max_allocation_bytes;
    size_t live;
    if (mc_entity_storage_native_bytes(entity, &live, &frame) != MC_ENTITY_NBT_OK)
        return publish(frame, error);
    if (live > limits.max_allocation_bytes) {
        fail(&frame, MC_ENTITY_NBT_LIMIT, "Entity storage provenance live graph budget");
        return publish(frame, error);
    }
    limits.max_allocation_bytes -= live;
    mc_nbt_tree *source = NULL;
    const mc_nbt_status copied = mc_nbt_tree_copy_compound(compound, &limits, &source, &frame.nbt);
    if (copied != MC_NBT_OK) {
        nbt_failure(&frame, copied, "Entity storage provenance independent tree");
        return publish(frame, error);
    }
    if (!item_entity) {
        /* FallingSand owns only this independent source tree. No Item getter,
         * Stack admission, Stack attachment or subtype field store is reached. */
        mc_nbt_tree *old_source = entity->storage_source;
        entity->storage_source = source;
        mc_nbt_tree_free(old_source);
        return publish(mc_entity_nbt_empty_error(), error);
    }
    const mc_nbt_tag *root = mc_nbt_tree_root(source);
    const mc_nbt_tag *item = mc_nbt_compound_get(root, item_units, 4);
    if (item != NULL && item->type != MC_NBT_COMPOUND) {
        frame.field = "Item";
        fail(&frame, MC_ENTITY_NBT_WRONG_TAG, "Entity storage provenance Item Compound");
        mc_nbt_tree_free(source);
        return publish(frame, error);
    }
    const mc_nbt_tag empty = {.type = MC_NBT_COMPOUND};
    size_t stack_bytes;
    item_status = mc_item_stack_native_bytes(entity->stack, &stack_bytes);
    if (item_status != MC_ITEM_STACK_OK) {
        stack_failure(&frame, item_status, "Entity storage provenance Stack byte inspection");
        mc_nbt_tree_free(source);
        return publish(frame, error);
    }
    const size_t new_bytes = mc_nbt_tree_allocation_bytes(source);
    /* live includes the old Entity and Stack trees. The new Entity tree stays
     * held while the Stack helper validates/copies and atomically replaces its
     * old source. Deduct everything except the Stack which that helper itself
     * deducts; no source attachment has happened at this point. */
    if (stack_bytes > live || new_bytes > ceiling - live) {
        fail(&frame, MC_ENTITY_NBT_LIMIT, "Entity storage provenance overlapping trees budget");
        mc_nbt_tree_free(source);
        return publish(frame, error);
    }
    limits = requested == NULL ? mc_nbt_default_limits() : *requested;
    limits.max_allocation_bytes -= live - stack_bytes;
    limits.max_allocation_bytes -= new_bytes;
    item_status = mc_item_stack_preserve_source(entity->stack, item == NULL ? &empty : item,
                                                &limits, &frame.stack);
    if (item_status != MC_ITEM_STACK_OK) {
        stack_failure(&frame, item_status, "Entity storage provenance Stack source copy");
        mc_nbt_tree_free(source);
        return publish(frame, error);
    }
    /* The last fallible operation succeeded. Both native attachments commit
     * together without source calls, allocation or another admission point. */
    mc_nbt_tree *old_source = entity->storage_source;
    entity->storage_source = source;
    mc_nbt_tree_free(old_source);
    return publish(mc_entity_nbt_empty_error(), error);
}

mc_entity_nbt_status mc_entity_save_storage_nbt(mc_entity *entity, const mc_nbt_limits *requested,
                                                bool *serialized, mc_nbt_tree **out,
                                                mc_entity_nbt_error *error) {
    mc_entity_nbt_error frame = mc_entity_nbt_empty_error();
    if (entity == NULL || serialized == NULL || out == NULL) {
        fail(&frame, MC_ENTITY_NBT_INVALID_ARGUMENT, "Entity storage Boolean save arguments");
        return publish(frame, error);
    }
    mc_nbt_limits limits = requested == NULL ? mc_nbt_default_limits() : *requested;
    size_t live = 0;
    mc_entity_nbt_error inspection;
    const mc_entity_nbt_status inspected =
        mc_entity_storage_native_bytes(entity, &live, &inspection);
    mc_nbt_limits semantic_limits = limits;
    semantic_limits.max_allocation_bytes =
        inspected == MC_ENTITY_NBT_OK && live <= limits.max_allocation_bytes
            ? limits.max_allocation_bytes - live
            : 0;
    mc_nbt_tree *semantic = NULL;
    bool emitted = false;
    /* No provenance validation or phase gate precedes the actual source name,
     * dead and late Stack writer. Its complete typed failure stays unchanged. */
    const mc_entity_nbt_status saved =
        mc_entity_save_nbt(entity, &semantic_limits, &emitted, &semantic, &frame);
    if (saved != MC_ENTITY_NBT_OK)
        return publish(frame, error);
    if (!emitted) {
        *serialized = false;
        return publish(mc_entity_nbt_empty_error(), error);
    }
    if (inspected != MC_ENTITY_NBT_OK) {
        mc_nbt_tree_free(semantic);
        return publish(inspection, error);
    }
    const size_t semantic_bytes = mc_nbt_tree_allocation_bytes(semantic);
    if (live > limits.max_allocation_bytes || semantic_bytes > limits.max_allocation_bytes - live) {
        fail(&frame, MC_ENTITY_NBT_LIMIT, "Entity storage save semantic tree budget");
        mc_nbt_tree_free(semantic);
        return publish(frame, error);
    }
    limits.max_allocation_bytes -= live + semantic_bytes;
    const mc_nbt_tag *known = mc_nbt_tree_root(semantic);
    const bool item_entity = mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_ITEM);
    const bool falling_entity = mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_FALLING_SAND);
    const mc_nbt_tag *known_item = item_entity ? mc_nbt_compound_get(known, item_units, 4) : NULL;
    const size_t known_count = item_entity ? 11u : 9u;
    if ((!item_entity && !falling_entity) || known == NULL || known->type != MC_NBT_COMPOUND ||
        known->value.compound.length != known_count ||
        (item_entity &&
         (known_item == NULL || known_item->type != MC_NBT_COMPOUND || entity->stack == NULL))) {
        fail(&frame, MC_ENTITY_NBT_NBT_FAILED, "Entity storage save semantic result shape");
        mc_nbt_tree_free(semantic);
        return publish(frame, error);
    }
    size_t item_count = 0, entity_count = 0;
    mc_nbt_status status = MC_NBT_OK;
    if (item_entity)
        status = mc_nbt_compound_overlay_count(mc_item_stack_source(entity->stack), known_item,
                                               &item_count, &frame.nbt);
    if (status == MC_NBT_OK)
        status = mc_nbt_compound_overlay_count(mc_entity_storage_source(entity), known,
                                               &entity_count, &frame.nbt);
    if (status != MC_NBT_OK) {
        nbt_failure(&frame, status, "Entity storage save overlay extent");
        mc_nbt_tree_free(semantic);
        return publish(frame, error);
    }
    size_t pointer_count = item_count;
    if (item_count > limits.max_elements || entity_count > limits.max_elements ||
        !add_size(&pointer_count, entity_count) ||
        pointer_count > SIZE_MAX / sizeof(const mc_nbt_tag *) ||
        pointer_count * sizeof(const mc_nbt_tag *) > limits.max_allocation_bytes) {
        fail(&frame, MC_ENTITY_NBT_LIMIT, "Entity storage save overlay pointer budget");
        mc_nbt_tree_free(semantic);
        return publish(frame, error);
    }
    const size_t scratch_bytes = pointer_count * sizeof(const mc_nbt_tag *);
    const mc_nbt_tag **scratch = malloc(scratch_bytes);
    if (scratch == NULL) {
        fail(&frame, MC_ENTITY_NBT_ALLOCATION_FAILED, "Entity storage save overlay pointers");
        mc_nbt_tree_free(semantic);
        return publish(frame, error);
    }
    mc_nbt_tag item_view, entity_view;
    if (item_entity)
        status = mc_nbt_compound_overlay(mc_item_stack_source(entity->stack), known_item, scratch,
                                         item_count, &item_view, &frame.nbt);
    mc_nbt_tree *result = NULL;
    if (status == MC_NBT_OK) {
        const mc_nbt_tag *generated_items[11];
        mc_nbt_tag fields = *known;
        if (item_entity) {
            /* A Stack's provenance root name belongs to its former container.
             * This occurrence's actual generated field owns the Item name. */
            item_view.name = known_item->name;
            for (size_t index = 0; index < known_count; ++index) {
                const mc_nbt_tag *field = known->value.compound.items[index];
                generated_items[index] = field == known_item ? &item_view : field;
            }
            fields.value.compound.items = generated_items;
        }
        status =
            mc_nbt_compound_overlay(mc_entity_storage_source(entity), &fields, scratch + item_count,
                                    entity_count, &entity_view, &frame.nbt);
        if (status == MC_NBT_OK) {
            limits.max_allocation_bytes -= scratch_bytes;
            status = mc_nbt_tree_copy_compound(&entity_view, &limits, &result, &frame.nbt);
        }
    }
    free(scratch);
    mc_nbt_tree_free(semantic);
    if (status != MC_NBT_OK) {
        nbt_failure(&frame, status, "Entity storage save independent overlay tree");
        return publish(frame, error);
    }
    *serialized = true;
    *out = result;
    return publish(mc_entity_nbt_empty_error(), error);
}
