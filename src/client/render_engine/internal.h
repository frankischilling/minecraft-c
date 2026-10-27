#ifndef MC_CLIENT_RENDER_ENGINE_INTERNAL_H
#define MC_CLIENT_RENDER_ENGINE_INTERNAL_H
#include "mc/client/render_engine/render_engine.h"

typedef struct render_allocation {
    struct render_allocation *next;

    union {
        long double align;
        void *pointer;
        uint64_t integer;
        size_t bytes;
    } payload;
} render_allocation;

typedef struct mc_client_render_map_entry {
    uint32_t key;
    mc_client_argb_image *image;
} mc_client_render_map_entry;

struct mc_client_render_map {
    size_t count, capacity;
    mc_client_render_map_entry *entries;
};

struct mc_client_render_list {
    size_t count, capacity;
    void **entries;
};

struct mc_client_direct_byte_buffer {
    size_t position, limit, capacity;
    uint8_t *data;
};

struct mc_client_direct_int_buffer {
    size_t position, limit, capacity;
    mc_client_direct_byte_buffer *bytes;
};

struct mc_client_argb_image {
    size_t holds, count;
    uint32_t width, height;
    uint32_t *pixels;
    bool constructed;
};

typedef struct render_graphics {
    mc_client_argb_image *image;
    mc_client_default_font *font;
    uint32_t color;
    bool disposed;
} render_graphics;

struct mc_client_render_engine_classes {
    size_t holds, bytes, allocations;
    bool ji_ready, ji_a;
    mc_client_render_engine_phase ge_phase;
    mc_client_render_engine_options options;
    mc_client_render_list *a, *b;
    mc_client_render_engine_error failure;
    struct mc_client_render_device *device;
    size_t native_resource_count, active_resource_calls;
    bool resource_retirement_started;
};

struct mc_client_render_engine {
    size_t holds, bytes, allocations;
    mc_client_render_engine_classes *classes;
    mc_client_default_font *font;
    mc_client_render_engine_phase phase;
    mc_client_render_engine_options options;
    mc_client_render_engine_fields fields;
    render_allocation *arena;
    mc_client_argb_image *candidate;
    render_graphics *graphics;
    unsigned int last_store_pc;
    size_t active_resource_calls;
    mc_client_render_engine_error failure;
};

#endif
