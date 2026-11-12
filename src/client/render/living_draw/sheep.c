#include "internal.h"

bool ld_sheep_pass(ld_frame *f, int32_t pass, float partial, bool *out) {
    mc_client_living_draw_error *e = f->error;
    ld_at(f, "xy", "a", "(Lls;IF)Z", 2);
    if (f->actor != NULL && !mc_entity_is_kind(f->actor, MC_ENTITY_CLASS_KIND_SHEEP)) {
        e->throwable = MC_CLIENT_LIVING_DRAW_EXCEPTION;
        e->source_exception = true;
        e->exception_class = "java/lang/ClassCastException";
        return ld_fail(f, MC_CLIENT_LIVING_DRAW_SOURCE_EXCEPTION, "Sheep pass actual DL checkcast");
    }
    ld_at(f, "xy", "a", "(Ldl;IF)Z", 1);
    if (pass != 0) {
        *out = false;
        return true;
    }
    ld_at(f, "xy", "a", "(Ldl;IF)Z", 5);
    if (f->actor == NULL)
        return ld_null(f, "Sheep pass null sheared invoke");
    bool sheared;
    if (!ld_entity(f, mc_entity_render_sheep_sheared(f->actor, &sheared, &e->detail.entity)))
        return false;
    if (sheared) {
        *out = false;
        return true;
    }
    ld_at(f, "xy", "a", "(Ldl;IF)Z", 14);
    if (!ld_texture(f, ld_fur_literal(), 14))
        return false;
    ld_at(f, "xy", "a", "(Ldl;IF)Z", 19);
    float brightness;
    if (!ld_brightness(f, partial, &brightness))
        return false;
    ld_at(f, "xy", "a", "(Ldl;IF)Z", 25);
    int32_t color;
    if (!ld_entity(f, mc_entity_render_sheep_color(f->actor, &color, &e->detail.entity)))
        return false;
    float rgb[3];
    static const int32_t fields[] = {32, 43, 54}, rows[] = {37, 48, 59}, values[] = {39, 50, 61};
    for (size_t i = 0; i < 3; ++i) {
        ld_at(f, "xy", "a", "(Ldl;IF)Z", fields[i]);
        mc_mob_class_namespace *ns = f->actor->classes->mobs;
        /* A genuine allocated DL ran its class initializer before Entity ID.
         * READY is immutable; this is actual a, not a palette snapshot. */
        if (ns == NULL || ns->sheep_phase != MC_ENTITY_CLASS_READY)
            return ld_fail(f, MC_CLIENT_LIVING_DRAW_STATE,
                           "Sheep source class initialization provenance");
        const mc_mob_sheep_colors *table = ns->sheep_a;
        ld_at(f, "xy", "a", "(Ldl;IF)Z", rows[i]);
        if (table == NULL)
            return ld_null(f, "Sheep color null outer array");
        if (color < 0 || (size_t)color >= table->length) {
            e->throwable = MC_CLIENT_LIVING_DRAW_EXCEPTION;
            e->source_exception = true;
            e->exception_class = "java/lang/ArrayIndexOutOfBoundsException";
            return ld_fail(f, MC_CLIENT_LIVING_DRAW_SOURCE_EXCEPTION,
                           "Sheep color actual row index");
        }
        if ((size_t)color >= 16)
            return ld_fail(f, MC_CLIENT_LIVING_DRAW_STATE, "Sheep native outer array extent");
        const mc_mob_sheep_color_row *row = table->rows[(size_t)color];
        ld_at(f, "xy", "a", "(Ldl;IF)Z", values[i]);
        if (row == NULL)
            return ld_null(f, "Sheep color null row array");
        if (i >= row->length) {
            e->throwable = MC_CLIENT_LIVING_DRAW_EXCEPTION;
            e->source_exception = true;
            e->exception_class = "java/lang/ArrayIndexOutOfBoundsException";
            return ld_fail(f, MC_CLIENT_LIVING_DRAW_SOURCE_EXCEPTION,
                           "Sheep color component index");
        }
        if (!ld_fp(f))
            return false;
        rgb[i] = at_fmul(brightness, row->values[i]);
    }
    ld_at(f, "xy", "a", "(Ldl;IF)Z", 63);
    if (!ld_display(f, mc_client_display_source_color(f->scope->model.display, rgb[0], rgb[1],
                                                      rgb[2], 1.0f, &e->detail.display)))
        return false;
    *out = true;
    return true;
}
