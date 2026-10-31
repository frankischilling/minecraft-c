#ifndef MC_CLIENT_DISPLAY_SOURCE_INTERNAL_H
#define MC_CLIENT_DISPLAY_SOURCE_INTERNAL_H
#include "fog_surface.h"
#include "matrix_source.h"
#include "source_lighting_state.h"
#include "source_surface.h"
#include <string.h>

enum { SOURCE_RANGES = 128, SOURCE_COMMAND_BYTES = 32 * 1024 * 1024, SOURCE_MATRIX_DEPTH = 32 };

typedef enum mc_client_source_command_kind {
    SOURCE_COMMAND_DRAW,
    SOURCE_COMMAND_COLOR,
    SOURCE_COMMAND_TRANSLATE,
    SOURCE_COMMAND_PUSH,
    SOURCE_COMMAND_POP,
    SOURCE_COMMAND_SCALE
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
        double scale[3];
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
    float position[3], uv[2], color[4], normal[3];
} mc_client_source_gpu_vertex;

typedef enum mc_client_source_attribute_profile {
    SOURCE_ATTRIBUTE_PROFILE_UNKNOWN,
    SOURCE_ATTRIBUTE_PROFILE_NVIDIA_581_42,
    SOURCE_ATTRIBUTE_PROFILE_MESA_25_2_8
} mc_client_source_attribute_profile;

/* Mesa's active fixed-size float attributes are retained independently from
 * their committed current values. Bits select normal, color and six material
 * properties per face; the shared source owner retains all bytes/knowledge. */
typedef struct mc_client_source_attribute_staging {
    uint32_t enabled, flags;
    float color[4], normal[3];
    bool color_known, normal_known;
    mc_client_source_material_state materials[2];
} mc_client_source_attribute_staging;

typedef struct mc_client_source_state {
    bool initialized, loading_ready, texture_enabled, alpha_test, color_known,
        internal_resource_bind;
    bool lighting_enabled, fog_enabled;
    mc_client_source_fog_state fog;
    uint64_t fog_capability_generation;
    uint64_t attribute_profile_generation;
    mc_client_source_attribute_profile attribute_profile;
    mc_client_source_attribute_staging attribute_staging;
    bool shade_flat, alpha_function_set;
    float alpha_reference;
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
    int projection_uniform, modelview_uniform, textured_uniform, alpha_uniform,
        alpha_reference_uniform;
    int clamp_s_uniform, clamp_t_uniform;
    int fog_enabled_uniform, fog_mode_uniform, fog_distance_uniform;
    int fog_color_uniform, fog_density_uniform, fog_start_uniform, fog_end_uniform;
    int lighting_uniform, normal_matrix_uniform, normalize_uniform, rescale_uniform;
    int normal_scale_uniform, lights_uniform, global_ambient_uniform;
    int local_viewer_uniform, two_side_uniform, separate_specular_uniform;
    int color_material_uniform, material_face_uniform, material_mode_uniform;
    int light_position_uniform[2], light_ambient_uniform[2], light_diffuse_uniform[2];
    int light_specular_uniform[2], material_ambient_uniform[2], material_diffuse_uniform[2];
    int material_specular_uniform[2], material_emission_uniform[2], material_shininess_uniform[2];
    int flat_uniform;
    size_t vbo_bytes;
    bool vbo_capacity_uncertain;
    int32_t matrix_mode;
    bool matrix_changed_since_push[2]; /* MODELVIEW, PROJECTION; one flag per stack. */
    mc_client_modelview projection_stack[SOURCE_MATRIX_DEPTH];
    size_t projection_depth;
} mc_client_source_state;

/* Called once by source_initialize, before the owner is published initialized.
 * These are the legacy GL initial values, not the parameters written by U.b. */
static inline void source_lighting_initialize(mc_client_source_state *state) {
    state->fog.light_parameters[0] = (mc_client_source_light_state){.ambient = {0, 0, 0, 1},
                                                                    .diffuse = {1, 1, 1, 1},
                                                                    .specular = {1, 1, 1, 1},
                                                                    .position = {0, 0, 1, 0},
                                                                    .position_eye = {0, 0, 1, 0}};
    state->fog.light_parameters[1] = (mc_client_source_light_state){.ambient = {0, 0, 0, 1},
                                                                    .diffuse = {0, 0, 0, 1},
                                                                    .specular = {0, 0, 0, 1},
                                                                    .position = {0, 0, 1, 0},
                                                                    .position_eye = {0, 0, 1, 0}};
    const float ambient[4] = {0.2f, 0.2f, 0.2f, 1};
    memcpy(state->fog.light_model_ambient, ambient, sizeof(ambient));
}
#endif
