#ifndef MC_CLIENT_DISPLAY_SOURCE_LIGHTING_STATE_H
#define MC_CLIENT_DISPLAY_SOURCE_LIGHTING_STATE_H

#include "fog_surface.h"
#include <string.h>

/* Shared by immediate/replayed Color and source array completion. Helpers
 * change only the canonical source state and do not enter a context or draw.
 * The numeric selectors are the original GL enums; this state-only header
 * does not introduce an SDL/GL dependency into passive inspection callers. */
static inline bool source_material_face_selected(const mc_client_source_fog_state *state,
                                                 size_t side) {
    return state->material_face == 1032 || state->material_face == (side == 0 ? 1028 : 1029);
}

static inline bool source_material_property_tracked(const mc_client_source_fog_state *state,
                                                    size_t side, int32_t pname) {
    return state->color_material && source_material_face_selected(state, side) &&
           (state->material_mode == pname ||
            (state->material_mode == 5634 && (pname == 4608 || pname == 4609)));
}

static inline void source_material_color_property(float destination[4], bool *knowledge,
                                                  const float rgba[4], bool known) {
    if (known)
        memcpy(destination, rgba, 4 * sizeof(float));
    *knowledge = known;
}

static inline void source_material_color(mc_client_source_fog_state *state, const float rgba[4],
                                         bool known) {
    if (!state->color_material)
        return;
    float values[4];
    if (known)
        memcpy(values, rgba, sizeof(values));
    for (size_t side = 0; side < 2; ++side) {
        mc_client_source_material_state *material = &state->materials[side];
        if (source_material_property_tracked(state, side, 4608))
            source_material_color_property(material->ambient, &material->ambient_known, values,
                                           known);
        if (source_material_property_tracked(state, side, 4609))
            source_material_color_property(material->diffuse, &material->diffuse_known, values,
                                           known);
        if (source_material_property_tracked(state, side, 4610))
            source_material_color_property(material->specular, &material->specular_known, values,
                                           known);
        if (source_material_property_tracked(state, side, 5632))
            source_material_color_property(material->emission, &material->emission_known, values,
                                           known);
    }
}

static inline void source_material_color_unknown(mc_client_source_fog_state *state) {
    source_material_color(state, NULL, false);
}

/* Called once from the canonical source initializer. All defaults are known;
 * array effects can later invalidate only the attributes/properties they use. */
static inline void source_material_initialize(mc_client_source_fog_state *state) {
    const mc_client_source_material_state initial = {.ambient = {0.2f, 0.2f, 0.2f, 1},
                                                     .diffuse = {0.8f, 0.8f, 0.8f, 1},
                                                     .specular = {0, 0, 0, 1},
                                                     .emission = {0, 0, 0, 1},
                                                     .shininess = 0,
                                                     .color_indexes = {0, 1, 1},
                                                     .ambient_known = true,
                                                     .diffuse_known = true,
                                                     .specular_known = true,
                                                     .emission_known = true,
                                                     .shininess_known = true,
                                                     .color_indexes_known = true};
    state->materials[0] = initial;
    state->materials[1] = initial;
    state->color_material = false;
    state->material_face = 1032; /* GL_FRONT_AND_BACK. */
    state->material_mode = 5634; /* GL_AMBIENT_AND_DIFFUSE. */
    state->normal[0] = state->normal[1] = 0;
    state->normal[2] = 1;
    state->normal_known = true;
    state->normalize_enabled = false;
    state->rescale_normal_enabled = false;
    state->light_model_local_viewer = false;
    state->light_model_two_side = false;
    state->light_model_color_control = 33273; /* GL_SINGLE_COLOR. */
}

#endif
