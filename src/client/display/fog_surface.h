#ifndef MC_CLIENT_DISPLAY_FOG_SURFACE_H
#define MC_CLIENT_DISPLAY_FOG_SURFACE_H

#include "display.h"

/* Source legacy state translated by the canonical GL3 draw program. Calls
 * preserve the current matrices, viewport, framebuffer and sticky GL error.
 * State changes execute immediately; compiled state remains unsupported. */
bool mc_client_display_source_fog_color(mc_client_display *, const float rgba[4],
                                        mc_client_display_error *);
bool mc_client_display_source_normal(mc_client_display *, float x, float y, float z,
                                     mc_client_display_error *);
bool mc_client_display_source_normalize(mc_client_display *, bool enabled,
                                        mc_client_display_error *);
bool mc_client_display_source_rescale_normal(mc_client_display *, bool enabled,
                                             mc_client_display_error *);
bool mc_client_display_source_fog_i(mc_client_display *, int32_t pname, int32_t value,
                                    mc_client_display_error *);
bool mc_client_display_source_fog_f(mc_client_display *, int32_t pname, float value,
                                    mc_client_display_error *);
bool mc_client_display_source_nv_fog_distance(mc_client_display *, bool *advertised,
                                              mc_client_display_error *);
bool mc_client_display_source_fog_enable(mc_client_display *, bool enabled,
                                         mc_client_display_error *);
bool mc_client_display_source_color_material_enable(mc_client_display *, bool enabled,
                                                    mc_client_display_error *);
bool mc_client_display_source_color_material(mc_client_display *, int32_t face, int32_t mode,
                                             mc_client_display_error *);
/* The actual FloatBuffer wrapper requires four remaining floats for every
 * pname, including SHININESS and COLOR_INDEXES. Values are copied without a
 * cursor change. ColorMaterial-controlled properties retain their tracked
 * values; unrelated selected properties are assigned normally. */
bool mc_client_display_source_material(mc_client_display *, int32_t face, int32_t pname,
                                       const void *storage, size_t remaining,
                                       mc_client_display_error *);
bool mc_client_display_source_light_enable(mc_client_display *, unsigned int light, bool enabled,
                                           mc_client_display_error *);
/* State-only legacy lighting operations on the canonical Display. The light
 * index is 0 or 1. FloatBuffer storage starts at its current position and must
 * contain at least four remaining floats; uploads never advance that cursor.
 * POSITION retains both raw input and its current-modelview eye coordinates.
 * Lighting state is retained independently from the source draw profile. */
bool mc_client_display_source_lighting_enable(mc_client_display *, bool enabled,
                                              mc_client_display_error *);
bool mc_client_display_source_light_parameter(mc_client_display *, unsigned int light,
                                              int32_t pname, const void *storage, size_t remaining,
                                              mc_client_display_error *);
bool mc_client_display_source_light_model(mc_client_display *, int32_t pname, const void *storage,
                                          size_t remaining, mc_client_display_error *);
bool mc_client_display_source_light_model_i(mc_client_display *, int32_t pname, int32_t value,
                                            mc_client_display_error *);
bool mc_client_display_source_depth_mask(mc_client_display *, bool enabled,
                                         mc_client_display_error *);
bool mc_client_display_source_blend_enable(mc_client_display *, bool enabled,
                                           mc_client_display_error *);
bool mc_client_display_source_blend_function(mc_client_display *, int32_t source,
                                             int32_t destination, mc_client_display_error *);

typedef struct mc_client_source_light_state {
    float ambient[4], diffuse[4], specular[4];
    float position[4], position_eye[4];
} mc_client_source_light_state;

typedef struct mc_client_source_material_state {
    float ambient[4], diffuse[4], specular[4], emission[4], shininess;
    float color_indexes[3];
    bool ambient_known, diffuse_known, specular_known, emission_known, shininess_known;
    bool color_indexes_known;
} mc_client_source_material_state;

typedef struct mc_client_source_fog_state {
    bool enabled, color_material, lights[2];
    bool lighting_enabled;
    bool nv_capability_known, nv_advertised;
    int32_t mode, distance_mode, material_face, material_mode, matrix_mode;
    float density, start, end, color[4], normal[3];
    float current_color[4];
    bool current_color_known;
    mc_client_source_light_state light_parameters[2];
    float light_model_ambient[4];
    /* Front is index0, back is index1. Unknown properties retain native bytes
     * for inspection only; those bytes do not supply a known source value. */
    mc_client_source_material_state materials[2];
    bool normal_known, normalize_enabled, rescale_normal_enabled;
    bool light_model_local_viewer, light_model_two_side;
    int32_t light_model_color_control;
} mc_client_source_fog_state;

/* Passive copy; no context switch, GL query or source initialization. */
bool mc_client_display_source_fog_inspect(const mc_client_display *, mc_client_source_fog_state *);

#endif
