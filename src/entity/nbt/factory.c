#include "mc/entity/nbt/factory.h"

#include "../../platform/text/stdout.h"
#include "../entity_internal.h"
#include "mc/entity/minecart/minecart.h"

#include <string.h>

static const uint16_t id_units[] = {'i', 'd'};

static mc_entity_nbt_status finish(mc_entity_nbt_error frame, mc_entity_nbt_error *error) {
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

static mc_entity_nbt_status registry_failure(mc_entity_nbt_error *frame) {
    frame->initializer_bytecode = frame->registry.initializer_bytecode;
    frame->registration_bytecode = frame->registry.registration_bytecode;
    const mc_entity_registry_status status = frame->registry.status;
    return fail(frame,
                status == MC_ENTITY_REGISTRY_LIMIT               ? MC_ENTITY_NBT_LIMIT
                : status == MC_ENTITY_REGISTRY_ALLOCATION_FAILED ? MC_ENTITY_NBT_ALLOCATION_FAILED
                                                                 : MC_ENTITY_NBT_REGISTRY_FAILED,
                "EntityList active use");
}

static mc_entity_nbt_status construction_failure(mc_entity_nbt_error *frame) {
    const mc_entity_status status = frame->construction.status;
    return fail(frame,
                status == MC_ENTITY_LIMIT               ? MC_ENTITY_NBT_LIMIT
                : status == MC_ENTITY_ALLOCATION_FAILED ? MC_ENTITY_NBT_ALLOCATION_FAILED
                : status == MC_ENTITY_UNSUPPORTED_FP    ? MC_ENTITY_NBT_UNSUPPORTED_FP
                                                        : MC_ENTITY_NBT_ENTITY_FAILED,
                "Entity NBT factory constructor");
}

mc_entity_nbt_status mc_entity_save_nbt(mc_entity *entity, const mc_nbt_limits *limits,
                                        bool *serialized, mc_nbt_tree **out,
                                        mc_entity_nbt_error *error) {
    mc_entity_nbt_error frame = mc_entity_nbt_empty_error();
    if (entity == NULL || serialized == NULL || out == NULL) {
        fail(&frame, MC_ENTITY_NBT_INVALID_ARGUMENT, "Entity NBT Boolean save arguments");
        return finish(frame, error);
    }
    const mc_entity_status held = mc_entity_retain(entity);
    if (held != MC_ENTITY_OK) {
        frame.construction.status = held;
        construction_failure(&frame);
        frame.operation = "Entity NBT Boolean save ownership";
        return finish(frame, error);
    }
    mc_nbt_tree *leaf = NULL, *result = NULL;
    const mc_entity_class_literal *type = entity->class_literal;
    mc_nbt_string name;
    frame.wrapper_bytecode = 1;
    frame.name_bytecode = 1;
    /* The final aj/ag wrapper invokes jc/ga.b at1. Its active use initializes
     * EntityList before that callee's map.get7 body can be reached. */
    if (mc_entity_classes_initialize_registry(entity->classes, &frame.registry) !=
        MC_ENTITY_REGISTRY_OK) {
        registry_failure(&frame);
        goto done;
    }
    frame.name_bytecode = 7;
    if (mc_entity_registry_class_name(entity->classes, type, &name, &frame.registry) !=
        MC_ENTITY_REGISTRY_OK) {
        registry_failure(&frame);
        goto done;
    }
    frame.wrapper_bytecode = 6;
    if (entity->base.be || name.units == NULL) {
        *serialized = false;
        frame = mc_entity_nbt_empty_error();
        goto done;
    }
    frame.wrapper_bytecode = 22;
    const mc_nbt_tag id = {.type = MC_NBT_STRING, .name = {id_units, 2}, .value.string = name};
    frame.wrapper_bytecode = 27;
    mc_entity_nbt_error child;
    if (mc_entity_build_nbt(entity, limits, &leaf, &child) != MC_ENTITY_NBT_OK) {
        frame = child;
        frame.wrapper_bytecode = 27;
        frame.name_bytecode = 7;
        goto done;
    }
    const mc_nbt_tag *root = mc_nbt_tree_root(leaf);
    size_t field_count = 0;
    if (mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_ITEM))
        field_count = 10;
    else if (mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_FALLING_SAND))
        field_count = 8;
    else if (mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_MINECART) && root != NULL &&
             root->type == MC_NBT_COMPOUND &&
             (root->value.compound.length == 8 || root->value.compound.length == 9 ||
              root->value.compound.length == 11))
        field_count = root->value.compound.length;
    if (field_count == 0 || root == NULL || root->type != MC_NBT_COMPOUND ||
        root->value.compound.length != field_count) {
        fail(&frame, MC_ENTITY_NBT_NBT_FAILED, "Entity NBT Boolean save semantic shape");
        goto done;
    }
    const mc_nbt_tag *items[12] = {&id};
    for (size_t index = 0; index < field_count; ++index)
        items[index + 1] = root->value.compound.items[index];
    const mc_nbt_tag compound = {.type = MC_NBT_COMPOUND,
                                 .value.compound = {.items = items, .length = field_count + 1u}};
    mc_nbt_limits selected = limits == NULL ? mc_nbt_default_limits() : *limits;
    const size_t retained = mc_nbt_tree_allocation_bytes(leaf);
    if (retained > selected.max_allocation_bytes) {
        fail(&frame, MC_ENTITY_NBT_LIMIT, "Entity NBT Boolean save retained tree budget");
        goto done;
    }
    selected.max_allocation_bytes -= retained;
    const mc_nbt_status copied =
        mc_nbt_tree_copy_compound(&compound, &selected, &result, &frame.nbt);
    if (copied != MC_NBT_OK) {
        fail(&frame,
             copied == MC_NBT_LIMIT               ? MC_ENTITY_NBT_LIMIT
             : copied == MC_NBT_ALLOCATION_FAILED ? MC_ENTITY_NBT_ALLOCATION_FAILED
                                                  : MC_ENTITY_NBT_NBT_FAILED,
             "Entity NBT Boolean save independent tree");
        goto done;
    }
    *out = result;
    *serialized = true;
    frame = mc_entity_nbt_empty_error();
done:
    mc_nbt_tree_free(leaf);
    mc_entity_release(entity);
    return finish(frame, error);
}

static bool read_id(const mc_nbt_tag *compound, mc_nbt_string *out, mc_entity_nbt_error *frame) {
    frame->field = "id";
    if (compound == NULL || compound->type != MC_NBT_COMPOUND) {
        fail(frame, compound == NULL ? MC_ENTITY_NBT_NULL_REFERENCE : MC_ENTITY_NBT_WRONG_TAG,
             "Entity NBT factory Compound getter");
        return false;
    }
    const mc_nbt_tag *tag = mc_nbt_compound_get(compound, id_units, 2);
    if (tag != NULL && tag->type != MC_NBT_STRING) {
        fail(frame, MC_ENTITY_NBT_WRONG_TAG, "Entity NBT factory String getter");
        return false;
    }
    *out = tag == NULL ? (mc_nbt_string){0} : tag->value.string;
    if (out->length != 0 && out->units == NULL) {
        fail(frame, MC_ENTITY_NBT_INVALID_ARGUMENT, "Entity NBT factory String span");
        return false;
    }
    return true;
}

static bool skipped(const mc_nbt_tag *compound, mc_entity_nbt_error *frame) {
    frame->factory_bytecode = 72;
    mc_text_stdout stream;
    if (mc_text_stdout_capture(&stream) != MC_TEXT_STDOUT_OK) {
        fail(frame, MC_ENTITY_NBT_UNSUPPORTED_TYPE, "Entity NBT factory stdout capture");
        return false;
    }
    frame->factory_bytecode = 90;
    mc_nbt_string name;
    if (!read_id(compound, &name, frame))
        return false;
    static const char prefix[] = "Skipping Entity with id ";
    char line[64];
    const size_t prefix_length = sizeof(prefix) - 1;
    frame->factory_bytecode = 99;
    if (name.length > sizeof(line) - prefix_length) {
        fail(frame, MC_ENTITY_NBT_UNSUPPORTED_TYPE, "Entity NBT factory diagnostic length");
        return false;
    }
    memcpy(line, prefix, prefix_length);
    for (size_t index = 0; index < name.length; ++index) {
        if (name.units[index] > 127u) {
            fail(frame, MC_ENTITY_NBT_UNSUPPORTED_TYPE, "Entity NBT factory diagnostic encoding");
            return false;
        }
        line[prefix_length + index] = (char)name.units[index];
    }
    bool io_failed;
    if (mc_text_stdout_println(&stream, line, prefix_length + name.length, &io_failed) !=
        MC_TEXT_STDOUT_OK) {
        fail(frame, MC_ENTITY_NBT_UNSUPPORTED_TYPE, "Entity NBT factory diagnostic println");
        return false;
    }
    /* The source method does not inspect PrintStream.checkError. This API has
     * no stream-state query; no per-Entity or per-class substitute is stored. */
    (void)io_failed;
    return true;
}

mc_entity_nbt_status mc_entity_factory_nbt(mc_entity_classes *classes, mc_world_runtime *world,
                                           const mc_nbt_tag *compound, mc_entity **out,
                                           mc_entity_nbt_error *error) {
    mc_entity_nbt_error frame = mc_entity_nbt_empty_error();
    if (classes == NULL || out == NULL) {
        fail(&frame, MC_ENTITY_NBT_INVALID_ARGUMENT, "Entity NBT factory arguments");
        return finish(frame, error);
    }
    if (mc_entity_classes_initialize_registry(classes, &frame.registry) != MC_ENTITY_REGISTRY_OK) {
        registry_failure(&frame);
        return finish(frame, error);
    }
    frame.factory_bytecode = 8;
    mc_nbt_string name;
    if (!read_id(compound, &name, &frame)) {
        /* Exception.printStackTrace57 is not a normal unknown-name return.
         * Retain the failed getter independently beside that reached frontier. */
        frame.name_bytecode = 8;
        frame.factory_bytecode = 57;
        return finish(frame, error);
    }
    frame.factory_bytecode = 11;
    const mc_entity_class_literal *type = NULL;
    if (mc_entity_registry_named_class(classes, name, &type, &frame.registry) !=
        MC_ENTITY_REGISTRY_OK) {
        registry_failure(&frame);
        return finish(frame, error);
    }
    if (type == NULL) {
        if (skipped(compound, &frame)) {
            *out = NULL;
            frame = mc_entity_nbt_empty_error();
        }
        return finish(frame, error);
    }
    frame.factory_bytecode = 45;
    if (type->kind != MC_ENTITY_CLASS_KIND_ITEM &&
        type->kind != MC_ENTITY_CLASS_KIND_FALLING_SAND &&
        type->kind != MC_ENTITY_CLASS_KIND_MINECART) {
        fail(&frame, MC_ENTITY_NBT_UNSUPPORTED_TYPE, "Entity NBT factory concrete constructor");
        return finish(frame, error);
    }
    mc_entity *entity = NULL;
    mc_entity_status constructed;
    if (type->kind == MC_ENTITY_CLASS_KIND_FALLING_SAND) {
        constructed = mc_falling_entity_allocate(classes, &entity, &frame.construction);
        if (constructed == MC_ENTITY_OK)
            constructed = mc_falling_entity_construct_world(entity, world, &frame.construction);
    } else if (type->kind == MC_ENTITY_CLASS_KIND_MINECART) {
        constructed = mc_minecart_entity_allocate(classes, &entity, &frame.construction);
        if (constructed == MC_ENTITY_OK)
            constructed = mc_minecart_entity_construct_world(entity, world, &frame.construction);
    } else {
        constructed = mc_item_entity_allocate(classes, &entity, &frame.construction);
        if (constructed == MC_ENTITY_OK)
            constructed = mc_item_entity_construct_world(entity, world, &frame.construction);
    }
    if (constructed != MC_ENTITY_OK) {
        construction_failure(&frame);
        mc_entity_release(entity);
        return finish(frame, error);
    }
    mc_entity_nbt_error child;
    if (mc_entity_read_nbt(entity, compound, &child) != MC_ENTITY_NBT_OK) {
        frame = child;
        frame.factory_bytecode = 66;
        mc_entity_release(entity);
        return finish(frame, error);
    }
    *out = entity;
    return finish(mc_entity_nbt_empty_error(), error);
}
