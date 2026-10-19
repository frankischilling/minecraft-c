#include "mc/storage/nbt/tree.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* The owned tree is independent of opaque NBT document layouts. */
struct mc_nbt_tree {
    size_t allocation_bytes;
    const mc_nbt_tag *root;
};

typedef struct nbt_tree_arena {
    unsigned char *base;
    size_t used, capacity, max_depth;
} nbt_tree_arena;

static mc_nbt_status nbt_tree_error(mc_nbt_error *error, mc_nbt_status status)
{
    if (error != NULL) {
        *error = (mc_nbt_error){status, 0};
    }
    return status;
}

static mc_nbt_status nbt_tree_take(nbt_tree_arena *arena, size_t count, size_t size,
                                   size_t alignment, void **out)
{
    if (size != 0 && count > SIZE_MAX / size) {
        return MC_NBT_LIMIT;
    }
    const size_t bytes = count * size;
    const size_t remainder = arena->used % alignment;
    const size_t padding = remainder == 0 ? 0 : alignment - remainder;
    if (padding > arena->capacity - arena->used) {
        return MC_NBT_LIMIT;
    }
    const size_t position = arena->used + padding;
    if (bytes > arena->capacity - position) {
        return MC_NBT_LIMIT;
    }
    *out = arena->base == NULL ? NULL : arena->base + position;
    arena->used = position + bytes;
    return MC_NBT_OK;
}

static mc_nbt_status nbt_tree_string(nbt_tree_arena *arena, mc_nbt_string source,
                                     mc_nbt_string *out)
{
    if (source.length == 0) {
        *out = (mc_nbt_string){NULL, 0};
        return MC_NBT_OK;
    }
    if (source.units == NULL) {
        return MC_NBT_INVALID_TREE;
    }
    void *memory;
    const mc_nbt_status status =
        nbt_tree_take(arena, source.length, sizeof(uint16_t), _Alignof(uint16_t), &memory);
    if (status != MC_NBT_OK) {
        return status;
    }
    if (memory != NULL) {
        memcpy(memory, source.units, source.length * sizeof(uint16_t));
    }
    *out = (mc_nbt_string){memory, source.length};
    return MC_NBT_OK;
}

/* Validation precedes both passes. Planning and copying have identical order;
 * only the copying pass has nonnull destination storage. */
static mc_nbt_status nbt_tree_copy(nbt_tree_arena *arena, const mc_nbt_tag *source, bool named,
                                   size_t depth, const mc_nbt_tag **out)
{
    if (source == NULL || (named && source->type == MC_NBT_END)) {
        return MC_NBT_INVALID_TREE;
    }
    if ((unsigned int)source->type > (unsigned int)MC_NBT_COMPOUND) {
        return MC_NBT_INVALID_TYPE;
    }
    if (depth > arena->max_depth) {
        return MC_NBT_LIMIT;
    }
    void *memory;
    mc_nbt_status status =
        nbt_tree_take(arena, 1, sizeof(mc_nbt_tag), _Alignof(mc_nbt_tag), &memory);
    if (status != MC_NBT_OK) {
        return status;
    }
    mc_nbt_tag *tag = memory;
    if (tag != NULL) {
        /* Preserve raw representations, including signaling-NaN payload bits. */
        memcpy(tag, source, sizeof(*tag));
    }
    mc_nbt_string name = {NULL, 0};
    if (named) {
        status = nbt_tree_string(arena, source->name, &name);
        if (status != MC_NBT_OK) {
            return status;
        }
    }
    if (tag != NULL) {
        tag->name = name;
    }

    if (source->type == MC_NBT_STRING) {
        mc_nbt_string value;
        status = nbt_tree_string(arena, source->value.string, &value);
        if (status != MC_NBT_OK) {
            return status;
        }
        if (tag != NULL) {
            tag->value.string = value;
        }
    } else if (source->type == MC_NBT_BYTE_ARRAY) {
        const size_t length = source->value.byte_array.length;
        void *data = NULL;
        if (length != 0) {
            if (source->value.byte_array.data == NULL) {
                return MC_NBT_INVALID_TREE;
            }
            status = nbt_tree_take(arena, length, 1, _Alignof(uint8_t), &data);
            if (status != MC_NBT_OK) {
                return status;
            }
        }
        if (data != NULL) {
            memcpy(data, source->value.byte_array.data, length);
        }
        if (tag != NULL) {
            tag->value.byte_array.data = data;
        }
    } else if (source->type == MC_NBT_LIST || source->type == MC_NBT_COMPOUND) {
        const bool list = source->type == MC_NBT_LIST;
        const size_t length = list ? source->value.list.length : source->value.compound.length;
        const mc_nbt_tag *const *children = NULL;
        void *pointers = NULL;
        if (length != 0) {
            children = list ? source->value.list.items : source->value.compound.items;
            if (children == NULL) {
                return MC_NBT_INVALID_TREE;
            }
            if (list && source->value.list.element_type > MC_NBT_COMPOUND) {
                return MC_NBT_INVALID_TYPE;
            }
            /* Do not enter another recursive frame beyond validated depth. */
            if (depth == arena->max_depth) {
                return MC_NBT_LIMIT;
            }
            status = nbt_tree_take(arena, length, sizeof(const mc_nbt_tag *),
                                   _Alignof(const mc_nbt_tag *), &pointers);
            if (status != MC_NBT_OK) {
                return status;
            }
        }
        const mc_nbt_tag **items = pointers;
        if (tag != NULL) {
            if (list) {
                tag->value.list.items = items;
            } else {
                tag->value.compound.items = items;
            }
        }
        for (size_t i = 0; i < length; ++i) {
            if (list && (children[i] == NULL ||
                         (unsigned int)children[i]->type != source->value.list.element_type)) {
                return MC_NBT_INVALID_TREE;
            }
            const mc_nbt_tag *child;
            status = nbt_tree_copy(arena, children[i], !list, depth + 1, &child);
            if (status != MC_NBT_OK) {
                return status;
            }
            if (items != NULL) {
                items[i] = child;
            }
        }
    }
    *out = tag;
    return MC_NBT_OK;
}

mc_nbt_status mc_nbt_tree_copy_compound(const mc_nbt_tag *compound,
                                        const mc_nbt_limits *requested_limits, mc_nbt_tree **out,
                                        mc_nbt_error *error)
{
    if (compound == NULL || out == NULL) {
        return nbt_tree_error(error, MC_NBT_INVALID_ARGUMENT);
    }
    const mc_nbt_limits limits =
        requested_limits == NULL ? mc_nbt_default_limits() : *requested_limits;
    size_t encoded_length;
    mc_nbt_status status = mc_nbt_measure(compound, &limits, &encoded_length, error);
    if (status != MC_NBT_OK) {
        return status;
    }
    (void)encoded_length;
    if (compound->type != MC_NBT_COMPOUND) {
        return nbt_tree_error(error, MC_NBT_ROOT_TYPE);
    }
    if (sizeof(mc_nbt_tree) > limits.max_allocation_bytes) {
        return nbt_tree_error(error, MC_NBT_LIMIT);
    }

    nbt_tree_arena measure = {NULL, sizeof(mc_nbt_tree), limits.max_allocation_bytes,
                              limits.max_depth};
    const mc_nbt_tag *ignored;
    status = nbt_tree_copy(&measure, compound, true, 1, &ignored);
    if (status != MC_NBT_OK) {
        return nbt_tree_error(error, status);
    }
    void *allocation = malloc(measure.used);
    if (allocation == NULL) {
        return nbt_tree_error(error, MC_NBT_ALLOCATION_FAILED);
    }
    nbt_tree_arena copy = {allocation, sizeof(mc_nbt_tree), measure.used, limits.max_depth};
    const mc_nbt_tag *root;
    status = nbt_tree_copy(&copy, compound, true, 1, &root);
    if (status != MC_NBT_OK || copy.used != measure.used) {
        free(allocation);
        return nbt_tree_error(error, status == MC_NBT_OK ? MC_NBT_INVALID_TREE : status);
    }

    mc_nbt_tree *tree = allocation;
    tree->allocation_bytes = measure.used;
    tree->root = root;
    *out = tree;
    return nbt_tree_error(error, MC_NBT_OK);
}

const mc_nbt_tag *mc_nbt_tree_root(const mc_nbt_tree *tree)
{
    return tree == NULL ? NULL : tree->root;
}

size_t mc_nbt_tree_allocation_bytes(const mc_nbt_tree *tree)
{
    return tree == NULL ? 0 : tree->allocation_bytes;
}

void mc_nbt_tree_free(mc_nbt_tree *tree)
{
    free(tree);
}
