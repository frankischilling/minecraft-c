#ifndef MC_CLIENT_RENDER_ENGINE_TEXTURE_CALLS_INTERNAL_H
#define MC_CLIENT_RENDER_ENGINE_TEXTURE_CALLS_INTERNAL_H
#include "textures.h"

/* Shared canonical ge texture-generation/upload bodies. No registration frame
 * is fabricated: caller and actual child are independent. The old registration
 * wrapper translates its own real12/27/39 frames into its existing error API. */
typedef struct mc_client_texture_body_error {
    mc_client_render_texture_status status;
    const char *operation, *source_class, *source_method, *source_descriptor;
    unsigned int source_pc;
    mc_client_graphics_site caller;
    mc_client_display_error display;
    mc_client_render_engine_status owner;
    mc_client_texture generated_id;
    bool driver_generated, source_ledger_appended, base_uploaded, image_published;
    mc_client_graphics_error resources;
} mc_client_texture_body_error;

/* Native policy only, no allocation/store/GL/class activation. Whole-call
 * Engine/namespace holds and exclusive active guards belong to the caller. */
bool mc_client_texture_body_admit(mc_client_render_engine *, mc_client_display *,
                                  const mc_client_render_texture_options *,
                                  mc_client_texture_body_error *);
mc_client_render_texture_options mc_client_texture_body_options(const mc_client_render_engine *);
/* Does NOT clear or advance e. Normal constructed e has one actual direct int
 * slot. Driver writes and source List.add28 occur on the genuine namespace.
 * Pending generated resources stay class-owned on every later failure.
 * out is borrowed stable device-record storage, published only on success. */
bool mc_client_texture_generate(mc_client_render_engine *, mc_client_display *,
                                const mc_client_render_texture_options *,
                                mc_client_direct_int_buffer *, mc_client_graphics_site,
                                mc_client_texture_generation **out, mc_client_texture_body_error *);
/* Actual ji.a(BufferedImage,int)V. No generation or d publication is performed.
 * Null image reaches its actual getWidth154 after bind/parameters. */
bool mc_client_texture_upload(mc_client_render_engine *, mc_client_texture_resource *,
                              mc_client_argb_image *, mc_client_graphics_site,
                              mc_client_texture_body_error *);
#endif
