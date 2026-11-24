#include "../application_state.h"
#include "internal.h"

mc_texture_pack_status texture_default_construct(mc_client_texture_pack *pack,
                                                 mc_texture_pack_error *error) {
    pack->icon_id = -1; /* pa6, after actual literal-return i constructor1. */
    pack->name = texture_string_utf8("Default", pack->options.max_name_units, error);
    if (pack->name == NULL) {
        error->source_pc = 12;
        goto failed;
    }
    pack->description =
        texture_string_utf8("The default look of Minecraft", pack->options.max_name_units, error);
    if (pack->description == NULL) {
        error->source_pc = 18;
        goto failed;
    }
    static const uint16_t icon_name[] = {'/', 'p', 'a', 'c', 'k', '.', 'p', 'n', 'g'};
    mc_client_texture_stream *stream = NULL;
    if (texture_pack_base_open(pack, (mc_texture_pack_text){icon_name, 9, false}, &stream, error) !=
        MC_TEXTURE_PACK_OK) {
        error->source_pc = 26;
        goto failed;
    }
    if (stream == NULL) {
        texture_pack_fail(error, MC_TEXTURE_PACK_SOURCE_ILLEGAL_ARGUMENT,
                          "ImageIO null resource URL", "pa", "<init>", "()V", 29, 0);
        return error->status;
    }
    mc_client_argb_image *image = NULL;
    const mc_texture_pack_status read = texture_pack_image_read(stream, &image, error);
    if (read == MC_TEXTURE_PACK_OK || read == MC_TEXTURE_PACK_SOURCE_IO ||
        read == MC_TEXTURE_PACK_SOURCE_ILLEGAL_ARGUMENT) {
        /* The URL overload owns its real opened stream and closes in its JDK
         * finally. The separate ji InputStream helper has no such finally. */
        mc_texture_pack_error close_error = {0};
        const mc_texture_pack_status closed = mc_client_texture_stream_close(stream, &close_error);
        if (read == MC_TEXTURE_PACK_OK && closed != MC_TEXTURE_PACK_OK)
            *error = close_error;
        else if (closed != MC_TEXTURE_PACK_OK)
            error->native_close_status = close_error.resource_status;
    }
    mc_texture_pack_error retirement = {0};
    mc_client_texture_stream_release(stream, &retirement);
    if (error->status == MC_TEXTURE_PACK_OK && retirement.status != MC_TEXTURE_PACK_OK)
        *error = retirement;
    if (error->status == MC_TEXTURE_PACK_OK) {
        pack->icon = image; /* Actual pa.f32, only after ImageIO URL return. */
        return MC_TEXTURE_PACK_OK;
    }
    mc_client_argb_image_release(image);
    error->source_pc = 29;
    if (error->status == MC_TEXTURE_PACK_SOURCE_IO) {
        error->cause_status = error->status;
        error->source_diagnostic_reached = true;
        texture_pack_fail(error, MC_TEXTURE_PACK_DIAGNOSTIC_DEPENDENCY,
                          "pa caught IOException.printStackTrace dependency", "pa", "<init>", "()V",
                          40, 0);
        return error->status;
    }
failed:
    error->source_class = "pa";
    error->source_method = "<init>";
    error->source_descriptor = "()V";
    return error->status;
}

mc_texture_pack_status texture_pack_unload(mc_client_texture_pack *pack, client_application *app,
                                           mc_texture_pack_error *error) {
    const char *source = pack->kind == MC_TEXTURE_PACK_DEFAULT ? "pa" : "vp";
    if (pack->icon != NULL) {
        if (app == NULL)
            return texture_pack_fail(error, MC_TEXTURE_PACK_NULL_REFERENCE,
                                     "Texture pack Minecraft.p receiver", source, "b",
                                     "(Lnet/minecraft/client/Minecraft;)V", 8, 321);
        mc_client_render_engine *engine = app->render_engine;
        if (engine == NULL)
            return texture_pack_fail(error, MC_TEXTURE_PACK_NULL_REFERENCE,
                                     "Texture pack RenderEngine delete receiver", source, "b",
                                     "(Lnet/minecraft/client/Minecraft;)V", 15, 321);
        /* The genuine raw-delete source body will be linked here only after
         * the RenderEngine successor review permits the shared overlay. Native
         * resource retirement is deliberately not substituted for ji.a(int). */
        return texture_pack_fail(error, MC_TEXTURE_PACK_RENDER_DEPENDENCY,
                                 "Texture pack reached ji.a(int) source delete dependency", source,
                                 "b", "(Lnet/minecraft/client/Minecraft;)V", 15, 321);
    }
    if (pack->kind == MC_TEXTURE_PACK_ZIP) {
        /* vp.b(App)19 calls its actual b()V. Null e's NPE is caught at10;
         * nonnull e.close4 exceptions are caught too, then e is cleared13. */
        if (pack->zip != NULL) {
            texture_zip_error cause = {0};
            texture_zip_close(pack->zip, &cause);
            texture_zip_release(pack->zip, NULL);
            pack->zip = NULL;
        }
    }
    return MC_TEXTURE_PACK_OK;
}
