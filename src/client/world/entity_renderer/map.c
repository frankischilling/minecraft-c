#include "internal.h"
#include <string.h>

bool entity_renderer_iy_construct(entity_renderer_context *o, mc_client_source_iy *iy,
                                  mc_game_settings *settings) {
    iy->phase = MC_CLIENT_ENTITY_RENDERER_CONSTRUCTING;
    o->failure->iy_pc = 8;
    if (mc_client_color_array_create(16384, &o->options->map_array, entity_renderer_site("iy", 8),
                                     &iy->array_candidate,
                                     &o->failure->array) != MC_CLIENT_COLOR_ARRAY_OK) {
        entity_renderer_fail(o->failure, MC_CLIENT_ENTITY_RENDERER_ARRAY_FAILED,
                             "actual iy int16384 allocation", "iy", 8,
                             o->failure->array.native_policy);
        goto failed;
    }
    iy->a = iy->array_candidate;
    iy->last_store = 10;
    iy->c = settings;
    iy->last_store = 15;
    iy->d = iy->retained_font;
    iy->last_store = 20;
    o->failure->iy_pc = 36;
    o->failure->image = mc_client_argb_image_create(128, 128, o->options->image_bytes, &iy->image);
    if (o->failure->image != MC_CLIENT_RENDER_ENGINE_OK) {
        entity_renderer_fail(o->failure, MC_CLIENT_ENTITY_RENDERER_IMAGE_FAILED,
                             "actual iy generated TYPE_INT_ARGB128 image", "iy", 36, true);
        goto failed;
    }
    o->failure->iy_pc = 39;
    if (iy->retained_engine == NULL) {
        entity_renderer_fail(o->failure, MC_CLIENT_ENTITY_RENDERER_NULL_REFERENCE,
                             "actual iy ji registration receiver", "iy", 39, false);
        goto failed;
    }
    if (mc_client_render_engine_class_identity(iy->retained_engine) != o->services->graphics) {
        entity_renderer_fail(o->failure, MC_CLIENT_ENTITY_RENDERER_INVALID_ARGUMENT,
                             "iy same actual graphics namespace", "iy", 39, true);
        goto failed;
    }
    mc_client_texture id;
    if (mc_client_render_engine_register_image(iy->retained_engine, o->services->display, NULL,
                                               iy->image, &id, &o->failure->texture) !=
        MC_CLIENT_RENDER_TEXTURE_OK) {
        entity_renderer_fail(o->failure, MC_CLIENT_ENTITY_RENDERER_GRAPHICS_FAILED,
                             "actual iy ji.a(BufferedImage)", "iy", 39, true);
        goto failed;
    }
    memcpy(&iy->b, &id, sizeof(id));
    iy->last_store = 42;
    if (!mc_client_render_engine_image_lease(iy->retained_engine, o->services->display, id,
                                             entity_renderer_site("iy", 42), &iy->lease,
                                             &o->failure->graphics)) {
        entity_renderer_fail(o->failure, MC_CLIENT_ENTITY_RENDERER_GRAPHICS_FAILED,
                             "iy actual image generation native hold", "iy", 42, true);
        goto failed;
    }
    /* Source loop is after returned registration/ID42. This actual int[] is
     * neither the ARGB plane nor ji transfer scratch and activates no colorizer. */
    for (size_t i = 0; i < 16384; ++i) {
        if (mc_client_color_array_write(iy->a, i, 0) != MC_CLIENT_COLOR_ARRAY_OK) {
            entity_renderer_fail(o->failure, MC_CLIENT_ENTITY_RENDERER_ARRAY_FAILED,
                                 "actual iy int[] zero write", "iy", 63, true);
            goto failed;
        }
        ++iy->zero_stores;
    }
    iy->phase = MC_CLIENT_ENTITY_RENDERER_READY;
    return true;
failed:
    iy->phase = MC_CLIENT_ENTITY_RENDERER_FAILED;
    return false;
}

bool entity_renderer_iy_retire(mc_client_source_iy *iy, mc_client_entity_renderer_error *e) {
    iy->phase = MC_CLIENT_ENTITY_RENDERER_RETIRING;
    if (!mc_client_font_lease_release(&iy->lease, &e->graphics)) {
        entity_renderer_fail(e, MC_CLIENT_ENTITY_RENDERER_BUSY,
                             "iy image generation native cleanup", NULL, -1, true);
        return false;
    }
    if (mc_client_color_array_release(&iy->array_candidate, &e->array) !=
        MC_CLIENT_COLOR_ARRAY_OK) {
        entity_renderer_fail(e, MC_CLIENT_ENTITY_RENDERER_BUSY,
                             "iy int[] native occurrence cleanup", NULL, -1, true);
        return false;
    }
    iy->a = NULL;
    mc_client_argb_image_release(iy->image);
    iy->image = NULL;
    e->font = mc_client_bitmap_font_release(&iy->retained_font, &e->font_cleanup);
    if (e->font != MC_CLIENT_BITMAP_FONT_OK) {
        entity_renderer_fail(e, MC_CLIENT_ENTITY_RENDERER_BUSY,
                             "iy captured q native occurrence cleanup", NULL, -1, true);
        return false;
    }
    iy->d = NULL;
    e->owner = mc_client_render_engine_release(iy->retained_engine);
    if (e->owner != MC_CLIENT_RENDER_ENGINE_OK) {
        entity_renderer_fail(e, MC_CLIENT_ENTITY_RENDERER_BUSY,
                             "iy captured p native occurrence cleanup", NULL, -1, true);
        return false;
    }
    iy->retained_engine = NULL;
    iy->c = NULL;
    return true;
}
