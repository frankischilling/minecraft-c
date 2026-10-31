#ifndef MC_CLIENT_RENDER_ENGINE_RESOURCE_LOOKUP_INTERNAL_H
#define MC_CLIENT_RENDER_ENGINE_RESOURCE_LOOKUP_INTERNAL_H
#include "texture_lookup_internal.h"

mc_client_graphics_site mc_client_resource_lookup_site(const mc_client_texture_lookup_attempt *,
                                                       int32_t);
mc_client_texture_lookup_status mc_client_resource_lookup_error_at(
    const mc_client_texture_lookup_attempt *, mc_client_texture_lookup_error *,
    mc_client_texture_lookup_status, const char *, int32_t, bool native);
void mc_client_resource_lookup_touch(mc_client_texture_lookup_attempt *, int32_t);
mc_client_texture_lookup_status
mc_client_resource_lookup_capture(mc_client_render_engine *, mc_client_texture_lookup_attempt *);
mc_client_texture_lookup_status mc_client_resource_lookup_open(mc_client_texture_lookup_attempt *,
                                                               mc_texture_pack_text name,
                                                               int32_t caller_pc);
mc_client_texture_lookup_status mc_client_resource_lookup_read(mc_client_texture_lookup_attempt *,
                                                               int32_t caller_pc);
/* One concrete prefix owner/lifecycle, no callback/vtable game dispatch. */
mc_client_texture_lookup_status mc_client_resource_lookup_enter(mc_client_render_engine *,
                                                                mc_client_texture_lookup_attempt *);
mc_client_texture_lookup_status mc_client_resource_lookup_finish(mc_client_render_engine *,
                                                                 mc_client_texture_lookup_attempt *,
                                                                 mc_client_texture_lookup_status);
mc_client_texture_lookup_status
mc_client_resource_lookup_cleanup(mc_client_texture_lookup_attempt *,
                                  bool preserve_completed_color_result);
bool mc_client_resource_lookup_remaining(const mc_client_texture_lookup_attempt *);
#endif
