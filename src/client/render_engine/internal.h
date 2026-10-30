#ifndef MC_CLIENT_RENDER_ENGINE_INTERNAL_H
#define MC_CLIENT_RENDER_ENGINE_INTERNAL_H
#include "arena.h"
#include "colorizer_internal.h"
#include "mc/client/render_engine/render_engine.h"
#include "nw_internal.h"
#include <stdatomic.h>

typedef struct mc_client_image_download_entry mc_client_image_download_entry;
typedef struct mc_client_image_download_native mc_client_image_download_native;

typedef struct mc_client_render_map_entry {
    uint32_t key;
    mc_client_argb_image *image;
} mc_client_render_map_entry;

typedef struct mc_client_render_string_id_entry {
    uint16_t *units;
    size_t length;
    bool is_null;
    uint32_t id; /* Exact Integer payload bits, not a texture ownership hold. */
} mc_client_render_string_id_entry;

typedef struct mc_client_render_color_array_entry {
    uint16_t *units;
    size_t length;
    bool is_null;
    mc_client_color_array *array;
} mc_client_render_color_array_entry;

struct mc_client_render_map {
    size_t count, capacity;

    union {
        mc_client_render_map_entry *entries;              /* Existing d image table. */
        mc_client_render_string_id_entry *string_ids;     /* Actual b table. */
        mc_client_render_color_array_entry *color_arrays; /* Actual c table. */
        mc_client_image_download_entry *downloads;        /* Actual h String -> ek table. */
    }; /* One source Map table pointer; preserves the old constructor charge. */
};

_Static_assert(sizeof(struct mc_client_render_map) == 2 * sizeof(size_t) + sizeof(void *),
               "native x64 Map table union preserves constructor charge");

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

struct mc_client_direct_float_buffer {
    size_t position, limit, capacity;
    mc_client_direct_byte_buffer *bytes;
};

struct mc_client_argb_image {
    atomic_size_t holds;
    size_t count;
    uint32_t width, height;
    uint32_t *pixels;
    bool constructed;
};

_Static_assert(sizeof(atomic_size_t) == sizeof(size_t) &&
                   _Alignof(atomic_size_t) == _Alignof(size_t),
               "atomic image occurrences preserve the canonical constructor charge/layout");

typedef struct render_graphics {
    mc_client_argb_image *image;
    mc_client_default_font *font;
    uint32_t color;
    bool disposed;
} render_graphics;

struct mc_client_render_engine_classes {
    atomic_size_t holds; /* One canonical native occurrence account, initialized in factory. */
    mc_client_render_arena arena;
    bool ji_ready, ji_a;
    mc_client_render_engine_phase ge_phase;
    mc_client_render_list *a, *b;
    mc_client_render_engine_error failure;
    struct mc_client_render_device *device;
    size_t native_resource_count, active_resource_calls;
    bool resource_retirement_started;
    mc_client_graphics_cause ge_failure;
    mc_client_nw_namespace nw;
    mc_java_system_output *nw_system_output; /* Borrowed canonical runtime service. */
    mc_client_graphics_limits limits;
    size_t active_source_calls, font_lease_count, command_range_count;
    size_t scene_lease_count; /* Native RenderGlobal consumers, not a ge/nw source field. */
    mc_client_command_range *command_ranges;
    struct mc_client_display_context_lease *context_lease;
    mc_client_display *bound_display;
    mc_client_colorizer_namespace colorizers; /* Three independent cold source classes. */
    mc_client_render_engine_phase dk_phase;
    mc_client_tessellator *dk_D; /* Actual dk.D, borrowed same-namespace nw.a. */
    int32_t dk_b;                /* Actual dk.b chunk update count. */
    mc_client_graphics_error dk_failure;
    mc_client_render_engine_phase u_phase;
    mc_client_direct_float_buffer *u_a; /* Actual u.a, borrowed same class arena. */
    mc_client_graphics_error u_failure;
};

struct mc_client_render_engine {
    size_t holds;
    mc_client_render_arena arena;
    mc_client_render_engine_classes *classes;
    mc_client_default_font *font;
    mc_client_render_engine_phase phase;
    mc_client_render_engine_fields fields;
    mc_client_argb_image *candidate;
    render_graphics *graphics;
    unsigned int last_store_pc;
    size_t active_resource_calls;
    mc_client_render_engine_error failure;
    struct mc_client_texture_lookup_native *lookup; /* Bounded native attempt, no cache mirror. */
    bool lookup_retired; /* Explicit final native source stop, not a ji field. */
    size_t effect_release_index;
    bool effects_retiring, effects_retired; /* Native terminal ownership, not source List state. */
    mc_client_image_download_native *image_downloads;
    bool image_downloads_retired; /* Persistent native stop, even after sidecar storage ends. */
};

#endif
