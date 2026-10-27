#ifndef MC_CLIENT_RENDER_ENGINE_COLORIZER_INTERNAL_H
#define MC_CLIENT_RENDER_ENGINE_COLORIZER_INTERNAL_H
#include "colorizer.h"

typedef struct mc_client_colorizer_class {
    mc_client_colorizer_phase phase;
    mc_client_color_array *a, *candidate;
    unsigned int last_store_pc;
    mc_client_colorizer_error failure;
} mc_client_colorizer_class;

typedef struct mc_client_colorizer_namespace {
    mc_client_colorizer_class classes[MC_CLIENT_COLORIZER_COUNT];
    mc_client_colorizer_options options;
    bool options_set;
} mc_client_colorizer_namespace;

/* CPU-only final-parent helpers. They neither consume the parent nor activate
 * a class. All aliases in the namespace are counted, including failed native
 * candidates. Caller adds any other occurrences it is retiring simultaneously. */
size_t mc_client_colorizer_occurrences(const mc_client_render_engine_classes *,
                                       const mc_client_color_array *);
bool mc_client_colorizer_can_retire(const mc_client_render_engine_classes *);
void mc_client_colorizer_free_namespace(mc_client_render_engine_classes *);
#endif
