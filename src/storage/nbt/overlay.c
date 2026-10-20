#include "mc/storage/nbt/overlay.h"

#include <stdbool.h>

static mc_nbt_status result(mc_nbt_error *error, mc_nbt_status status)
{
    if (error != NULL)
        *error = (mc_nbt_error){status, 0};
    return status;
}

static mc_nbt_status name_admit(mc_nbt_string name)
{
    if (name.length > UINT16_MAX)
        return MC_NBT_INVALID_LENGTH;
    if (name.length != 0 && name.units == NULL)
        return MC_NBT_INVALID_TREE;
    return MC_NBT_OK;
}

static mc_nbt_status compound_admit(const mc_nbt_tag *compound)
{
    if (compound->type != MC_NBT_COMPOUND)
        return MC_NBT_ROOT_TYPE;
    mc_nbt_status status = name_admit(compound->name);
    if (status != MC_NBT_OK)
        return status;
    const size_t count = compound->value.compound.length;
    if (count > SIZE_MAX / sizeof(*compound->value.compound.items))
        return MC_NBT_LIMIT;
    if (count != 0 && compound->value.compound.items == NULL)
        return MC_NBT_INVALID_TREE;
    for (size_t index = 0; index < count; ++index) {
        const mc_nbt_tag *child = compound->value.compound.items[index];
        if (child == NULL || child->type == MC_NBT_END)
            return MC_NBT_INVALID_TREE;
        if ((unsigned int)child->type > (unsigned int)MC_NBT_COMPOUND)
            return MC_NBT_INVALID_TYPE;
        status = name_admit(child->name);
        if (status != MC_NBT_OK)
            return status;
    }
    return MC_NBT_OK;
}

static bool names_equal(mc_nbt_string left, mc_nbt_string right)
{
    if (left.length != right.length)
        return false;
    for (size_t index = 0; index < left.length; ++index)
        if (left.units[index] != right.units[index])
            return false;
    return true;
}

static const mc_nbt_tag *find(const mc_nbt_tag *compound, mc_nbt_string name)
{
    if (compound == NULL)
        return NULL;
    for (size_t index = 0; index < compound->value.compound.length; ++index) {
        const mc_nbt_tag *child = compound->value.compound.items[index];
        if (names_equal(child->name, name))
            return child;
    }
    return NULL;
}

mc_nbt_status mc_nbt_compound_overlay_count(const mc_nbt_tag *source, const mc_nbt_tag *fields,
                                            size_t *out, mc_nbt_error *error)
{
    if (fields == NULL || out == NULL)
        return result(error, MC_NBT_INVALID_ARGUMENT);
    mc_nbt_status status = compound_admit(fields);
    if (status != MC_NBT_OK)
        return result(error, status);
    if (source != NULL) {
        status = compound_admit(source);
        if (status != MC_NBT_OK)
            return result(error, status);
    }
    size_t count = source == NULL ? 0 : source->value.compound.length;
    for (size_t index = 0; index < fields->value.compound.length; ++index) {
        if (find(source, fields->value.compound.items[index]->name) == NULL) {
            if (count == SIZE_MAX / sizeof(const mc_nbt_tag *))
                return result(error, MC_NBT_LIMIT);
            ++count;
        }
    }
    *out = count;
    return result(error, MC_NBT_OK);
}

mc_nbt_status mc_nbt_compound_overlay(const mc_nbt_tag *source, const mc_nbt_tag *fields,
                                      const mc_nbt_tag **scratch, size_t scratch_count,
                                      mc_nbt_tag *out, mc_nbt_error *error)
{
    if (out == NULL)
        return result(error, MC_NBT_INVALID_ARGUMENT);
    size_t count;
    const mc_nbt_status status = mc_nbt_compound_overlay_count(source, fields, &count, error);
    if (status != MC_NBT_OK)
        return status;
    if (scratch_count > SIZE_MAX / sizeof(*scratch))
        return result(error, MC_NBT_LIMIT);
    if (scratch == NULL && scratch_count != 0)
        return result(error, MC_NBT_INVALID_ARGUMENT);
    if (scratch_count < count)
        return result(error, MC_NBT_BUFFER_TOO_SMALL);

    /* All fallible admission precedes writes. Immutable unique-name inputs
     * make the two passes agree without an allocation or mutable key index. */
    size_t used = 0;
    if (source != NULL) {
        for (size_t index = 0; index < source->value.compound.length; ++index) {
            const mc_nbt_tag *old = source->value.compound.items[index];
            const mc_nbt_tag *replacement = find(fields, old->name);
            scratch[used++] = replacement == NULL ? old : replacement;
        }
    }
    for (size_t index = 0; index < fields->value.compound.length; ++index) {
        const mc_nbt_tag *field = fields->value.compound.items[index];
        if (find(source, field->name) == NULL)
            scratch[used++] = field;
    }
    *out = (mc_nbt_tag){.type = MC_NBT_COMPOUND,
                        .name = source == NULL ? fields->name : source->name,
                        .value.compound = {.items = scratch, .length = used}};
    return result(error, MC_NBT_OK);
}
