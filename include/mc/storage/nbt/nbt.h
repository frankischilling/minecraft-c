#ifndef MC_STORAGE_NBT_NBT_H
#define MC_STORAGE_NBT_NBT_H

#include <stddef.h>
#include <stdint.h>

typedef enum mc_nbt_type {
    MC_NBT_END = 0,
    MC_NBT_BYTE = 1,
    MC_NBT_SHORT = 2,
    MC_NBT_INT = 3,
    MC_NBT_LONG = 4,
    MC_NBT_FLOAT = 5,
    MC_NBT_DOUBLE = 6,
    MC_NBT_BYTE_ARRAY = 7,
    MC_NBT_STRING = 8,
    MC_NBT_LIST = 9,
    MC_NBT_COMPOUND = 10
} mc_nbt_type;

typedef struct mc_nbt_string {
    const uint16_t *units;
    size_t length;
} mc_nbt_string;

typedef struct mc_nbt_tag mc_nbt_tag;
struct mc_nbt_tag {
    mc_nbt_type type;
    mc_nbt_string name;
    union {
        int8_t byte_value;
        int16_t short_value;
        int32_t int_value;
        int64_t long_value;
        /* Preserve input NaN payloads without evaluating signaling NaNs. The
         * normal writer canonicalizes NaNs as Java data streams do. */
        uint32_t float_bits;
        uint64_t double_bits;
        struct {
            const uint8_t *data;
            size_t length;
        } byte_array;
        mc_nbt_string string;
        struct {
            uint8_t element_type;
            const mc_nbt_tag *const *items;
            size_t length;
        } list;
        struct {
            const mc_nbt_tag *const *items;
            size_t length;
        } compound;
    } value;
};

typedef struct mc_nbt_document mc_nbt_document;

typedef struct mc_nbt_limits {
    size_t max_input_bytes;
    size_t max_output_bytes;
    size_t max_allocation_bytes;
    size_t max_tags;
    size_t max_elements; /* Per list, array, or compound's physical entry count. */
    size_t max_depth;    /* Root depth is 1. Accepted range is 1..128. */
} mc_nbt_limits;

typedef enum mc_nbt_status {
    MC_NBT_OK = 0,
    MC_NBT_INVALID_ARGUMENT,
    MC_NBT_TRUNCATED,
    MC_NBT_INVALID_TYPE,
    MC_NBT_INVALID_LENGTH,
    MC_NBT_INVALID_UTF,
    MC_NBT_LIMIT,
    MC_NBT_ALLOCATION_FAILED,
    MC_NBT_INVALID_TREE,
    MC_NBT_BUFFER_TOO_SMALL,
    MC_NBT_ROOT_TYPE
} mc_nbt_status;

typedef struct mc_nbt_error {
    mc_nbt_status status;
    size_t offset;
} mc_nbt_error;

mc_nbt_limits mc_nbt_default_limits(void);
const char *mc_nbt_status_string(mc_nbt_status status);

/* NULL limits selects defaults. Parsing one named tag leaves trailing bytes
 * untouched and optionally reports the consumed count. out is required;
 * consumed and error may be NULL. On failure, *out and *consumed (if supplied)
 * are unchanged. Error reports status plus a byte offset.
 * Allocation/tag budgets include discarded duplicate-name values and arena
 * metadata. Compound End sentinels are not value tags. The retained compound
 * has the last value of each duplicate name.
 * Documents own all decoded memory; keep borrowed views immutable until free.
 * Result/count/error storage must be mutually disjoint and must not overlap
 * input bytes or limits. */
mc_nbt_status mc_nbt_read(const uint8_t *input, size_t length, const mc_nbt_limits *limits,
                          mc_nbt_document **out, size_t *consumed, mc_nbt_error *error);
mc_nbt_status mc_nbt_read_compound(const uint8_t *input, size_t length, const mc_nbt_limits *limits,
                                   mc_nbt_document **out, size_t *consumed, mc_nbt_error *error);
const mc_nbt_tag *mc_nbt_document_root(const mc_nbt_document *document);
void mc_nbt_document_free(mc_nbt_document *document);

/* UTF-16 name lookup; NULL name is valid only for length zero. */
const mc_nbt_tag *mc_nbt_compound_get(const mc_nbt_tag *compound, const uint16_t *name,
                                      size_t length);

/* Writers borrow immutable acyclic trees, including caller-constructed trees.
 * Nonempty lists' element_type must match every child; compound entries must have unique names
 * and cannot be End tags. List child names are ignored. Empty lists write type
 * Byte (1). Compound output uses retained order; Java HashMap iteration is not
 * claimed byte-identical. Full validation precedes any output modification.
 * *written and the output buffer remain unchanged on failure. Error is optional.
 * Output/count/error storage must be mutually disjoint and must not overlap
 * limits, the borrowed tree, or its buffers.
 * No compression, stream ownership, world schema, or file writes are implicit. */
mc_nbt_status mc_nbt_measure(const mc_nbt_tag *root, const mc_nbt_limits *limits, size_t *written,
                             mc_nbt_error *error);
mc_nbt_status mc_nbt_write(const mc_nbt_tag *root, const mc_nbt_limits *limits, uint8_t *output,
                           size_t capacity, size_t *written, mc_nbt_error *error);

#endif
