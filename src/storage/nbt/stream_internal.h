/* Included only by the canonical nbt.c after its existing private types. */
#ifndef MC_STORAGE_NBT_STREAM_INTERNAL_H
#define MC_STORAGE_NBT_STREAM_INTERNAL_H
#include "mc/storage/nbt/input.h"
typedef struct nbt_source_collection nbt_source_collection;
typedef struct nbt_source_tag nbt_source_tag;

struct nbt_source_collection {
    nbt_source_collection *previous;
    const mc_nbt_tag *tag;
    bool list;
    const mc_nbt_tag **items;
    size_t count, capacity;
    nbt_compound_builder map;
};

struct nbt_source_tag {
    nbt_source_tag *next;
    mc_nbt_tag *tag;
    nbt_source_collection *collection;
    bool name_published, array_published;
    const void *array;
    size_t array_length;
    int last_store;
    const char *last_store_class;
};

struct mc_nbt_stream {
    mc_nbt_stream_phase phase;
    bool active, constructed;
    size_t maximum_owner_bytes, owner_bytes, read_calls, close_calls;
    mc_nbt_byte_input input;
    mc_nbt_limits limits;
    uint8_t *bytearr, *read_buffer;
    uint16_t *chararr;
    size_t byte_capacity, char_capacity;
    int last_data_store;
    size_t position;
    mc_nbt_document *document;
    mc_nbt_tag *pending_tag;
    nbt_source_tag *records, *last_tag;
    nbt_source_collection *last_collection;
    int last_collection_operation;
    const void *last_array;
    size_t last_array_length;
    const char *context_class, *context_method;
    int context_pc;
    mc_nbt_stream_error error;
};
#endif
