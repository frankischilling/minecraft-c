#ifndef MC_CLIENT_RENDER_ENGINE_COLOR_LOOKUP_H
#define MC_CLIENT_RENDER_ENGINE_COLOR_LOOKUP_H
#include "color_array.h"
#include "texture_lookup.h"

typedef struct mc_client_color_lookup_options {
    size_t max_cache_entries, max_key_units, max_prefix_bytes;
    mc_client_color_array_limits arrays;
} mc_client_color_lookup_options;

/* The common resource error retains actual method/descriptor and complete
 * independent Pack/array/cleanup causes. No texture source frame is invented. */
typedef mc_client_texture_lookup_error mc_client_color_lookup_error;
mc_client_color_lookup_options mc_client_color_lookup_default_options(void);

/* Actual ji.a(String)[I over captured l/Pack and c Map. No Display argument,
 * ge/nw/GL/Settings/FP call, extra cache or image-plane alias. A successful
 * return is an independently held occurrence of the SAME array held by c.
 * Source calls stop at actual strip52/diagnostic193/provider frontiers. Native
 * options fix once; pending texture/color attempts block both later operations.
 * Caller owns out/error disjoint storage and keeps all actual dependencies
 * live/exclusive through retained failures. Out stays unchanged on failure.
 * Use the shared explicit pending/metadata retirement before final CPU release. */
mc_client_texture_lookup_status
mc_client_render_engine_colors(mc_client_render_engine *, const mc_client_color_lookup_options *,
                               mc_texture_pack_text nullable_key, mc_client_color_array **out,
                               mc_client_color_lookup_error *);
/* Borrow actual c value, without source capture/resource/class use. */
bool mc_client_render_engine_cached_colors(const mc_client_render_engine *, mc_texture_pack_text,
                                           bool *found, mc_client_color_array **borrowed_array);
#endif
