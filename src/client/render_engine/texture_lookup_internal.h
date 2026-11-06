#ifndef MC_CLIENT_RENDER_ENGINE_TEXTURE_LOOKUP_INTERNAL_H
#define MC_CLIENT_RENDER_ENGINE_TEXTURE_LOOKUP_INTERNAL_H
#include "color_lookup.h"
#include "internal.h"
#include "texture_lookup.h"

typedef struct mc_client_texture_lookup_attempt {
    mc_client_texture_lookup_state state;
    mc_client_graphics_site method; /* Actual ji.a/b source identity, independent caller frames. */
    mc_client_color_array *array;   /* New/cached result occurrence; no parent backreference. */
    mc_client_texture_lookup_error failure, cleanup;
    mc_client_texture_pack_list *list;
    mc_client_texture_pack *pack;
    mc_client_texture_stream *stream;
    mc_client_argb_image *image;
    mc_client_texture_generation *generation; /* Borrowed class-owned record. */
    bool list_held, pack_held;
} mc_client_texture_lookup_attempt;

struct mc_client_texture_lookup_native {
    mc_client_texture_lookup_options options;
    mc_client_texture_lookup_attempt *pending;
    mc_client_texture_lookup_error last_cleanup;
    bool active, texture_policy_set, color_policy_set;
    mc_client_color_lookup_options color_options;
};
#endif
