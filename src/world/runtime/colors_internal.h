#ifndef MC_WORLD_RUNTIME_COLORS_INTERNAL_H
#define MC_WORLD_RUNTIME_COLORS_INTERNAL_H

#include "mc/world/runtime/colors.h"

typedef struct color_frame {
    mc_world_runtime *world;
    mc_math_classes *math;
    mc_world_color_error error;
    bool entered, camera_held, math_held;
    mc_entity *camera;
} color_frame;

bool mc_world_color_fail(color_frame *, const char *, unsigned int, bool);
bool mc_world_color_begin(color_frame *, mc_world_runtime *, mc_math_classes *, const void *,
                          const char *, const char *);
mc_world_storage_status mc_world_color_finish(color_frame *, mc_world_color_error *);
bool mc_world_color_fp(color_frame *, unsigned int);
bool mc_world_color_math_ready(color_frame *, unsigned int);
bool mc_world_color_angle(color_frame *, float, float *, unsigned int);
bool mc_world_color_weather(color_frame *, float, bool, float *, unsigned int);

#endif
