#ifndef MC_CLIENT_RENDER_ENGINE_EFFECT_UPLOAD_H
#define MC_CLIENT_RENDER_ENGINE_EFFECT_UPLOAD_H
#include "effects.h"
#include "texture_lookup.h"

typedef enum mc_client_effect_upload_status {
    MC_CLIENT_EFFECT_UPLOAD_OK,
    MC_CLIENT_EFFECT_UPLOAD_INVALID_ARGUMENT,
    MC_CLIENT_EFFECT_UPLOAD_INVALID_PHASE,
    MC_CLIENT_EFFECT_UPLOAD_BUSY,
    MC_CLIENT_EFFECT_UPLOAD_NULL_REFERENCE,
    MC_CLIENT_EFFECT_UPLOAD_BUFFER_FAILED,
    MC_CLIENT_EFFECT_UPLOAD_CHILD_FAILED,
    MC_CLIENT_EFFECT_UPLOAD_LOOKUP_FAILED,
    MC_CLIENT_EFFECT_UPLOAD_DISPLAY_FAILED
} mc_client_effect_upload_status;

typedef struct mc_client_effect_upload_error {
    mc_client_effect_upload_status status;
    const char *operation;
    int32_t ji_pc, aw_pc;
    size_t occurrence, updated_occurrences, atlas_uploads, separate_uploads;
    bool second_pass, source_returned;
    mc_client_render_engine_status owner;
    mc_texture_effect_error child;
    mc_client_texture_lookup_error lookup;
    mc_client_display_error display;
} mc_client_effect_upload_error;

/* Actual ji.a()V, over the retained genuine g occurrences and shared f buffer.
 * Settings and selected Pack remain the Engine's actual borrowed identities.
 * Duplicates update/upload repeatedly; NULL fails at the reached dereference.
 * The aw atlas child runs the real ji.b(String) and binds its returned ID.
 * No buffer/GL/List rollback occurs on failure. The second pass starts only
 * after every first-pass occurrence returns. Native error output belongs on
 * the heap; owners and output stay exclusive/live throughout this call. */
bool mc_client_render_engine_update_effects(mc_client_render_engine *, mc_client_display *,
                                            mc_client_effect_upload_error *);
#endif
