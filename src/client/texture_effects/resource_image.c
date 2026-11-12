#include "effect_internal.h"

bool mc_texture_effect_resource(mc_texture_effect_owner *o, unsigned index, const char *name,
                                int32_t resource_pc, int32_t image_pc, int32_t rgb_pc,
                                int32_t diagnostic_pc, bool items, uint32_t *destination,
                                mc_texture_effect_error *e) {
    const char *source = mc_texture_effect_class(o->base.kind);
    mc_client_class_url_status status = mc_client_class_url_create(
        o->services.files, o->services.resources, name, &o->urls[index], &e->image);
    if (status != MC_CLIENT_CLASS_URL_OK) {
        mc_texture_effect_fail(e, MC_TEXTURE_EFFECT_IMAGE_FAILED, "effect actual class URL", source,
                               "<init>", resource_pc);
        return false;
    }
    const mc_client_image_io_limits limits = {
        o->options.max_encoded_bytes, o->options.max_decoded_bytes, o->options.max_inflater_bytes};
    status = mc_client_class_url_read_image(o->urls[index], &limits, &o->images[index], &e->image);
    if (status != MC_CLIENT_CLASS_URL_OK) {
        mc_texture_effect_fail(
            e,
            status == MC_CLIENT_CLASS_URL_SOURCE_IO ? MC_TEXTURE_EFFECT_DIAGNOSTIC_DEPENDENCY
                                                    : MC_TEXTURE_EFFECT_IMAGE_FAILED,
            status == MC_CLIENT_CLASS_URL_SOURCE_IO
                ? "caught IOException.printStackTrace remains unavailable"
                : "effect actual ImageIO URL child",
            source, "<init>", status == MC_CLIENT_CLASS_URL_SOURCE_IO ? diagnostic_pc : image_pc);
        e->cause_pc = image_pc;
        if (status == MC_CLIENT_CLASS_URL_SOURCE_IO) {
            e->dependency_class = "java/io/IOException"; /* actual invocation Methodref owner */
            e->dependency_method = "printStackTrace";
            e->dependency_descriptor = "()V";
        }
        return false;
    }
    mc_client_argb_image_state image;
    if (o->images[index] == NULL) {
        mc_texture_effect_fail(e, MC_TEXTURE_EFFECT_NULL_REFERENCE,
                               "effect BufferedImage.getRGB null receiver", source, "<init>",
                               rgb_pc);
        return false;
    }
    if (!mc_client_argb_image_inspect(o->images[index], &image)) {
        mc_texture_effect_fail(e, MC_TEXTURE_EFFECT_IMAGE_FAILED, "effect canonical image storage",
                               source, "<init>", rgb_pc);
        return false;
    }
    int32_t x = 0, y = 0;
    if (items) {
        int32_t column;
        (void)mc_java_i32_rem(o->fields.b, 16, &column);
        x = mc_java_i32_mul(column, 16);
        y = mc_java_i32_mul(o->fields.b / 16, 16);
    }
    if (destination == NULL || x < 0 || y < 0 || (uint32_t)x > image.width ||
        (uint32_t)y > image.height || image.width - (uint32_t)x < 16 ||
        image.height - (uint32_t)y < 16) {
        /* Existing getRGB NULL/partial-destination behavior remains an honest
         * JDK dependency; do not invent an exception or successful crop. */
        mc_texture_effect_fail(e, MC_TEXTURE_EFFECT_UNIMPLEMENTED,
                               "effect getRGB unsupported rectangle/destination prefix", source,
                               "<init>", rgb_pc);
        return false;
    }
    for (size_t row = 0; row < 16; ++row)
        for (size_t column = 0; column < 16; ++column)
            destination[row * 16 + column] =
                image.pixels[((size_t)(uint32_t)y + row) * image.width + (uint32_t)x + column];
    return true;
}
