#ifndef MC_CLIENT_RENDER_ENGINE_COLORIZER_H
#define MC_CLIENT_RENDER_ENGINE_COLORIZER_H
#include "color_array.h"

typedef enum mc_client_colorizer_kind {
    MC_CLIENT_COLORIZER_WATER,
    MC_CLIENT_COLORIZER_GRASS,
    MC_CLIENT_COLORIZER_FOLIAGE,
    MC_CLIENT_COLORIZER_COUNT
} mc_client_colorizer_kind;

typedef enum mc_client_colorizer_phase {
    MC_CLIENT_COLORIZER_RAW,
    MC_CLIENT_COLORIZER_CONSTRUCTING,
    MC_CLIENT_COLORIZER_READY,
    MC_CLIENT_COLORIZER_FAILED
} mc_client_colorizer_phase;

typedef enum mc_client_colorizer_status {
    MC_CLIENT_COLORIZER_OK,
    MC_CLIENT_COLORIZER_INVALID_ARGUMENT,
    MC_CLIENT_COLORIZER_INVALID_PHASE,
    MC_CLIENT_COLORIZER_BUSY,
    MC_CLIENT_COLORIZER_LIMIT,
    MC_CLIENT_COLORIZER_ARRAY_FAILED,
    MC_CLIENT_COLORIZER_CLASS_FAILED,
    MC_CLIENT_COLORIZER_UNSUPPORTED_FP
} mc_client_colorizer_status;

typedef struct mc_client_colorizer_options {
    mc_client_color_array_limits initial_array[MC_CLIENT_COLORIZER_COUNT];
} mc_client_colorizer_options;

typedef struct mc_client_colorizer_error {
    mc_client_colorizer_status status;
    const char *operation;
    mc_client_graphics_site site;
    mc_client_color_array_error array;
    mc_client_render_engine_status owner;
    bool native_policy;
} mc_client_colorizer_error;

typedef struct mc_client_colorizer_state {
    mc_client_colorizer_phase phase;
    const mc_client_color_array *a;                /* Actual static field, borrowed. */
    const mc_client_color_array *failed_candidate; /* Native prefix, not field a. */
    unsigned int last_store_pc;
    mc_client_colorizer_error failure;
} mc_client_colorizer_state;

mc_client_colorizer_options mc_client_colorizer_default_options(void);
/* Cold states are embedded in the real classes namespace. These entries perform
 * only actual hw/ia/jh initialization/setter bodies; no ge/nw/GL/FP use. Native
 * options fix once for the namespace and do not activate any class. Caller
 * keeps namespace and incoming array live/exclusive. Parent startup frames
 * (MC271/283/295) are separate from these exact child frames. */
mc_client_colorizer_status mc_client_colorizer_initialize(mc_client_render_engine_classes *,
                                                          mc_client_colorizer_kind,
                                                          const mc_client_colorizer_options *,
                                                          mc_client_colorizer_error *);
mc_client_colorizer_status mc_client_colorizer_set(mc_client_render_engine_classes *,
                                                   mc_client_colorizer_kind,
                                                   mc_client_color_array *nullable_array,
                                                   const mc_client_colorizer_options *,
                                                   mc_client_colorizer_error *);
bool mc_client_colorizer_inspect(const mc_client_render_engine_classes *, mc_client_colorizer_kind,
                                 mc_client_colorizer_state *);

typedef enum mc_client_colorizer_read_kind {
    MC_CLIENT_COLORIZER_SAMPLE,
    MC_CLIENT_COLORIZER_PINE,
    MC_CLIENT_COLORIZER_BIRCH,
    MC_CLIENT_COLORIZER_DEFAULT_FOLIAGE
} mc_client_colorizer_read_kind;

/* Exact IA/JH scalar bodies after the caller's reached class active use.
 * No array copies or masking: the returned int retains all 32 source bits.
 * The namespace and actual array remain live/exclusive through this call. */
mc_client_colorizer_status mc_client_colorizer_read_ready(mc_client_render_engine_classes *,
                                                          mc_client_colorizer_kind,
                                                          mc_client_colorizer_read_kind,
                                                          double temperature, double rainfall,
                                                          int32_t *, mc_client_colorizer_error *);

/* Native upper-layer entry: one actual class active use followed by the same
 * scalar body. The private class owner installs this callback so simulation
 * callers need no rendering-library dependency or global namespace binding. */
typedef mc_client_colorizer_status (*mc_client_colorizer_reader)(mc_client_render_engine_classes *,
                                                                 mc_client_colorizer_kind,
                                                                 mc_client_colorizer_read_kind,
                                                                 double, double, int32_t *,
                                                                 mc_client_colorizer_error *);
mc_client_colorizer_status mc_client_colorizer_read(mc_client_render_engine_classes *,
                                                    mc_client_colorizer_kind,
                                                    mc_client_colorizer_read_kind, double, double,
                                                    int32_t *, mc_client_colorizer_error *);
#endif
