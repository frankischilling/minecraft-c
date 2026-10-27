#ifndef MC_CLIENT_RENDER_ENGINE_TEXTURES_H
#define MC_CLIENT_RENDER_ENGINE_TEXTURES_H

#include "../display/display.h"
#include "mc/client/render_engine/render_engine.h"
#include "source.h"

typedef enum mc_client_render_texture_status {
    MC_CLIENT_RENDER_TEXTURE_OK,
    MC_CLIENT_RENDER_TEXTURE_INVALID_ARGUMENT,
    MC_CLIENT_RENDER_TEXTURE_INVALID_PHASE,
    MC_CLIENT_RENDER_TEXTURE_LIMIT,
    MC_CLIENT_RENDER_TEXTURE_ALLOCATION_FAILED,
    MC_CLIENT_RENDER_TEXTURE_NULL_REFERENCE,
    MC_CLIENT_RENDER_TEXTURE_BUFFER_OVERFLOW,
    MC_CLIENT_RENDER_TEXTURE_DISPLAY_FAILED,
    MC_CLIENT_RENDER_TEXTURE_UNIMPLEMENTED,
    MC_CLIENT_RENDER_TEXTURE_BUSY
} mc_client_render_texture_status;

typedef struct mc_client_render_texture_options {
    size_t max_retained_ids, max_source_ledger_entries, max_image_entries;
    size_t max_upload_scratch_bytes;
} mc_client_render_texture_options;

typedef struct mc_client_render_texture_error {
    mc_client_render_texture_status status;
    const char *operation, *source_class, *source_method, *source_descriptor;
    unsigned int source_pc, register_pc;
    mc_client_display_error display;
    mc_client_render_engine_status owner;
    mc_client_texture generated_id;
    bool driver_generated, source_ledger_appended, base_uploaded, image_published;
    mc_client_graphics_error resources;
} mc_client_render_texture_error;

mc_client_render_texture_options mc_client_render_texture_default_options(void);

/* Actual ji.a(BufferedImage)I: clear e4; actual ge generation/list publication;
 * upload27; original image publication d39; return44. Nullable image reaches
 * getWidth154 after real generation, binding and parameters. NULL settings is
 * source-supported and skips anaglyph at upload261/264. Nonnull images must be
 * genuine constructed owners. Hold actual Engine/image arguments across the
 * whole call; source d gains its independent image occurrence only at39.
 *
 * First use fixes the namespace's native resource options. Later NULL options
 * reuse them; explicit changed policies reject as native admission. The source
 * ledger and separate pending native resources are not conflated: append/map
 * failures retain actual generated IDs and every completed device/buffer prefix.
 * No failed registration deletes an ID or acknowledges an unissued upload.
 * Actual classes bind one Display while resources exist, backed by those real
 * context holds. Display/native/source dependencies remain exclusive and live.
 *
 * The normal a/j/k=false constructor path is implemented. Reached four-level
 * mipmap processing remains an explicit dependency after level0; GL_CLAMP
 * stops at its first actual parameter operation. This does not implement
 * ik/String/ImageIO lookup, HTTP textures, effect registration or effect tick.
 * out is unchanged on failure. Error/storage/views must be disjoint. */
mc_client_render_texture_status
mc_client_render_engine_register_image(mc_client_render_engine *, mc_client_display *,
                                       const mc_client_render_texture_options *,
                                       mc_client_argb_image *nullable_image, mc_client_texture *out,
                                       mc_client_render_texture_error *error);

/* Explicit terminal NATIVE resource retirement, separate from source ge.a().
 * Stop source resource operations once cleanup starts. Source ledger/history
 * and image maps are preserved until their ordinary owner release. Try each
 * independent pending/registered resource; BUSY/current failures keep real
 * handles for retry. A terminal context-loss error can consume all native
 * handles; inspect class native_resource_count before final CPU release.
 * A final class/Engine release with live resources is BUSY and preserves it.
 * No GL calls follow detected context loss. Display remains alive until every
 * real generated resource is retired; then its destroy is retryable. */
mc_client_render_texture_status
mc_client_render_engine_retire_resources(mc_client_render_engine_classes *,
                                         mc_client_render_texture_error *error);

/* Selected actual d.get(Integer) inspection; borrows its counted occurrence. */
bool mc_client_render_engine_registered_image(const mc_client_render_engine *, mc_client_texture,
                                              mc_client_argb_image **out);

#endif
