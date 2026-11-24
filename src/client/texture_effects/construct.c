#include "../application_access.h"
#include "effect_internal.h"

bool mc_texture_effect_constructor(mc_texture_effect_owner *o, struct client_application *app,
                                   int32_t argument, mc_texture_effect_error *e) {
    const char *source = mc_texture_effect_class(o->base.kind);
    e->source_class = source;
    int32_t texture, aw_pc;
    if (o->base.kind == MC_TEXTURE_EFFECT_COMPASS || o->base.kind == MC_TEXTURE_EFFECT_WATCH) {
        const mc_item *item = NULL;
        if (mc_registry_item_static_get(o->services.registry,
                                        o->base.kind == MC_TEXTURE_EFFECT_COMPASS ? "aO" : "aQ",
                                        &item, &e->registry) != MC_REGISTRY_OK) {
            mc_texture_effect_fail(e, MC_TEXTURE_EFFECT_REGISTRY_FAILED,
                                   "effect actual gm active use", source, "<init>", 1);
            return false;
        }
        if (item == NULL) {
            mc_texture_effect_fail(e, MC_TEXTURE_EFFECT_NULL_REFERENCE, "gm.a(0) receiver", source,
                                   "<init>", 5);
            return false;
        }
        /* Actual aO/aQ are plain gm allocations; gm.a(I) is getfield bh1,
         * ireturn4. No stack, inventory icon callback or item ID substitute. */
        texture = mc_item_fields_get(item)->bh;
        aw_pc = 8;
    } else {
        const char *name = o->base.kind == MC_TEXTURE_EFFECT_PORTAL          ? "bf"
                           : o->base.kind == MC_TEXTURE_EFFECT_FIRE          ? "as"
                           : o->base.kind == MC_TEXTURE_EFFECT_FLOWING_WATER ? "B"
                                                                             : "D";
        const mc_block *block = NULL;
        if (mc_registry_block_static_get(o->services.registry, name, &block, &e->registry) !=
            MC_REGISTRY_OK) {
            mc_texture_effect_fail(e, MC_TEXTURE_EFFECT_REGISTRY_FAILED,
                                   "effect actual uu active use", source, "<init>", 1);
            return false;
        }
        if (block == NULL) {
            mc_texture_effect_fail(e, MC_TEXTURE_EFFECT_NULL_REFERENCE, "effect actual bm receiver",
                                   source, "<init>", 4);
            return false;
        }
        texture = mc_block_fields_get(block)->bm;
        if (o->base.kind == MC_TEXTURE_EFFECT_FIRE) {
            texture = mc_java_i32_add(texture, mc_java_i32_mul(argument, 16));
            aw_pc = 12;
        } else if (o->base.kind == MC_TEXTURE_EFFECT_PORTAL)
            aw_pc = 7;
        else {
            texture = mc_java_i32_add(texture, 1);
            aw_pc = 9;
        }
    }
    const mc_aw_slots slots = {&o->fields.a, &o->fields.a_length, &o->fields.b, &o->fields.c,
                               &o->fields.d, &o->fields.e,        &o->fields.f};
    mc_aw_error aw = {0};
    if (!mc_texture_aw_construct(slots, o->options.max_array_bytes, &o->array_bytes, texture,
                                 &aw)) {
        mc_texture_effect_fail(
            e, aw.limit ? MC_TEXTURE_EFFECT_LIMIT : MC_TEXTURE_EFFECT_ALLOCATION_FAILED,
            "actual shared aw constructor array", "aw", "<init>", aw.pc);
        e->source_descriptor = "(I)V";
        e->enclosing_pc = aw_pc;
        return false;
    }
    o->last_store_pc = aw_pc;
    if (o->base.kind == MC_TEXTURE_EFFECT_PORTAL)
        return mc_texture_portal_construct(o, e);
    if (o->base.kind == MC_TEXTURE_EFFECT_COMPASS || o->base.kind == MC_TEXTURE_EFFECT_WATCH) {
        o->icon = mc_texture_effect_array(o, 256 * sizeof(*o->icon), 15, e);
        if (o->icon == NULL)
            return false;
        o->last_store_pc = 17;
        if (o->base.kind == MC_TEXTURE_EFFECT_COMPASS) {
            if (!mc_client_application_effect_borrow(app, &o->app_borrow_counted)) {
                mc_texture_effect_fail(e, MC_TEXTURE_EFFECT_INVALID_PHASE,
                                       "native App borrow before actual av.g store", "av", "<init>",
                                       22);
                return false;
            }
            o->app = app;
            o->last_store_pc = 22;
            o->fields.f = 1;
            o->last_store_pc = 27;
            return mc_texture_effect_resource(o, 0, "/gui/items.png", 34, 37, 79, 88, true, o->icon,
                                              e);
        }
        o->dial = mc_texture_effect_array(o, 256 * sizeof(*o->dial), 24, e);
        if (o->dial == NULL)
            return false;
        o->last_store_pc = 26;
        if (!mc_client_application_effect_borrow(app, &o->app_borrow_counted)) {
            mc_texture_effect_fail(e, MC_TEXTURE_EFFECT_INVALID_PHASE,
                                   "native App borrow before actual ku.g store", "ku", "<init>",
                                   31);
            return false;
        }
        o->app = app;
        o->last_store_pc = 31;
        o->fields.f = 1;
        o->last_store_pc = 36;
        if (!mc_texture_effect_resource(o, 0, "/gui/items.png", 43, 46, 88, 126, true, o->icon, e))
            return false;
        return mc_texture_effect_resource(o, 1, "/misc/dial.png", 96, 99, 117, 126, false, o->dial,
                                          e);
    }
    const size_t cells = o->base.kind == MC_TEXTURE_EFFECT_FIRE ? 320 : 256;
    float **arrays[] = {&o->g, &o->h, &o->i, &o->j};
    size_t *lengths[] = {&o->g_length, &o->h_length, &o->i_length, &o->j_length};
    static const int32_t flow_pc[] = {16, 25, 34, 43};
    static const int32_t fire_pc[] = {19, 28};
    const size_t count = o->base.kind == MC_TEXTURE_EFFECT_FIRE ? 2 : 4;
    for (size_t n = 0; n < count; ++n) {
        const int32_t pc = count == 2 ? fire_pc[n] : flow_pc[n];
        *arrays[n] = mc_texture_effect_array(o, cells * sizeof(float), pc, e);
        if (*arrays[n] == NULL)
            return false;
        *lengths[n] = cells;
        o->last_store_pc = pc + 2;
    }
    if (count == 4) {
        o->counter = 0;
        o->last_store_pc = 50;
        o->fields.e = 2;
        o->last_store_pc = 55;
    }
    return true;
}
