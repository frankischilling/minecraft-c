#ifndef MC_CLIENT_DISPLAY_SOURCE_INTERNAL_H
#define MC_CLIENT_DISPLAY_SOURCE_INTERNAL_H
#include "fog_surface.h"
#include "matrix_source.h"
#include "source_surface.h"

enum { SOURCE_RANGES = 128, SOURCE_COMMAND_BYTES = 32 * 1024 * 1024, SOURCE_MATRIX_DEPTH = 32 };

typedef enum mc_client_source_command_kind {
    SOURCE_COMMAND_DRAW,
    SOURCE_COMMAND_COLOR,
    SOURCE_COMMAND_TRANSLATE
} mc_client_source_command_kind;

typedef struct mc_client_source_command {
    struct mc_client_source_command *next;
    mc_client_source_command_kind kind;
    size_t charge;

    union {
        struct {
            uint8_t *bytes;
            size_t size;
            int32_t count, mode;
            unsigned arrays;
        } draw;

        float vector[4];
    } value;
} mc_client_source_command;

typedef struct mc_client_source_definition {
    mc_client_source_command *first, *last;
    size_t commands, bytes;
} mc_client_source_definition;

typedef struct mc_client_source_list {
    mc_client_source_command *first, *last;
    mc_client_source_definition *pending;
    bool defined, building;
} mc_client_source_list;

struct mc_client_display_list_range {
    struct mc_client_display_list_range *next;
    mc_client_display *display;
    int32_t base, count;
    size_t max_commands, command_count;
    mc_client_source_list *lists;
};

struct mc_client_display_context_lease {
    mc_client_display *display;
    uint64_t generation;
};

struct mc_client_display_texture_lease {
    struct mc_client_display_texture_lease *next;
    mc_client_display *display;
    uint64_t generation;
    mc_client_texture id;
    bool active;
};

typedef struct mc_client_source_gpu_vertex {
    float position[3], uv[2], color[4];
} mc_client_source_gpu_vertex;

typedef struct mc_client_source_state {
    bool initialized, loading_ready, texture_enabled, alpha_test, color_known,
        internal_resource_bind;
    bool lighting_enabled, fog_enabled;
    mc_client_source_fog_state fog;
    uint64_t fog_capability_generation;
    bool shade_flat; /* Source GL_FLAT admits constant color within each decoded primitive. */
    unsigned arrays, pointers;
    const void *array_buffers[4];
    uint64_t array_contexts[4];
    size_t array_offsets[4];
    float color[4], projection[16];
    mc_client_modelview modelview, stack[SOURCE_MATRIX_DEPTH];
    size_t depth, active_calls, context_holds, range_count, command_bytes, texture_lease_count;
    uint64_t next_texture_generation;
    uint64_t next_context_generation;
    uint64_t bound_texture_generation;
    mc_client_texture bound_texture;
    bool texture_binding_known;
    uint32_t next_list_base;
    mc_client_display_list_range *ranges, *compiling_range;
    mc_client_source_list *compiling;
    mc_client_display_texture_lease *texture_leases;
    uint32_t vertex_shader, fragment_shader, program, vao, vbo;
    unsigned gpu_phase;
    mc_client_display_error gpu_failure;
    int projection_uniform, modelview_uniform, textured_uniform, alpha_uniform;
    int fog_enabled_uniform, fog_mode_uniform, fog_distance_uniform;
    int fog_color_uniform, fog_density_uniform, fog_start_uniform, fog_end_uniform;
    size_t vbo_bytes;
    bool vbo_capacity_uncertain;
    int32_t matrix_mode;
    mc_client_modelview projection_stack[SOURCE_MATRIX_DEPTH];
    size_t projection_depth;
} mc_client_source_state;
#endif
