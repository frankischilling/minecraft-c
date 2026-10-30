#include "../../../entity/entity_internal.h"
#include "../../../entity/player/tick_math_internal.h"
#include "../../application_state.h"
#include "../../display/camera_surface.h"
#include "../../display/fog_surface.h"
#include "../../display/source_surface.h"
#include "../../render/manager/internal.h"
#include "../../render_engine/image_download.h"
#include "../../render_engine/lighting.h"
#include "../../render_engine/texture_lookup.h"
#include "held_item_draw_internal.h"
#include "internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/client/render/model_draw.h"
#include "mc/core/math/math.h"
#include "mc/world/runtime/entities.h"
#include <stdlib.h>
#include <string.h>

struct mc_client_held_item_draw_call {
    mc_client_held_item_draw_state state;
    mc_client_held_item_draw_error failure;
    mc_client_held_item_draw_error cleanup;
    mc_client_lighting_error lighting;
    mc_client_download_texture_error download;
    mc_client_texture_lookup_error lookup_cleanup;
    mc_client_model_draw_error model;
    mc_math_classes *math;
    mc_entity *players[8];
    size_t player_count;
    mc_world_runtime *world;
    mc_client_render_engine *engine;
    mc_client_render_manager *manager;
    mc_client_managed_renderer *renderer;
    bool owner_held, entered, math_held, world_held, stack_held, lookup_owned;
};

static mc_client_graphics_site where(const mc_client_held_item_draw_call *c, unsigned int pc) {
    return (mc_client_graphics_site){"ra", c->state.overlays ? "b" : "a", "(F)V", (int32_t)pc};
}

static mc_client_held_item_draw_status admission(mc_client_held_item_draw_error *out,
                                                 mc_client_held_item_draw_status status,
                                                 const char *operation) {
    if (out != NULL) {
        memset(out, 0, sizeof(*out));
        out->status = status;
        out->operation = operation;
        out->native_policy = status != MC_CLIENT_HELD_ITEM_DRAW_OK;
        out->site.pc = out->child.pc = -1;
    }
    return status;
}

static bool fail(mc_client_held_item_draw_call *c, mc_client_held_item_draw_status status,
                 const char *operation, unsigned int pc, bool native) {
    c->failure.status = status;
    c->failure.operation = operation;
    c->failure.site = where(c, pc);
    c->failure.native_policy = native;
    c->state.source_pc = pc;
    return false;
}

static bool fp(mc_client_held_item_draw_call *c, unsigned int pc) {
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u ||
           fail(c, MC_CLIENT_HELD_ITEM_DRAW_UNSUPPORTED_FP, "held draw strict floating controls",
                pc, true);
}

static mc_client_display *device(mc_client_held_item_draw_call *c) {
    return c->state.owner->context.services->display;
}

static mc_client_render_engine_classes *graphics(mc_client_held_item_draw_call *c) {
    return c->state.owner->context.services->graphics;
}

static mc_entity_classes *entities(mc_client_held_item_draw_call *c) {
    return c->state.owner->context.services->models->entities;
}

static bool gl(mc_client_held_item_draw_call *c, bool result, unsigned int pc) {
    if (!result)
        return fail(c, MC_CLIENT_HELD_ITEM_DRAW_CHILD_FAILED, c->failure.display.operation, pc,
                    true);
    ++c->state.gl_calls;
    c->state.source_pc = pc;
    return true;
}

static bool push(mc_client_held_item_draw_call *c, unsigned int pc) {
    if (!gl(c, mc_client_display_source_push(device(c), &c->failure.display), pc))
        return false;
    ++c->state.matrix_pushes;
    return true;
}

static bool pop(mc_client_held_item_draw_call *c, unsigned int pc) {
    if (!gl(c, mc_client_display_source_pop(device(c), &c->failure.display), pc))
        return false;
    ++c->state.matrix_pops;
    return true;
}

static bool rotate(mc_client_held_item_draw_call *c, float angle, float x, float y, float z,
                   unsigned int pc) {
    return gl(c, mc_client_display_source_rotate(device(c), angle, x, y, z, &c->failure.display),
              pc);
}

static bool translate(mc_client_held_item_draw_call *c, float x, float y, float z,
                      unsigned int pc) {
    return gl(c, mc_client_display_source_translate(device(c), x, y, z, &c->failure.display), pc);
}

static bool scale_one(mc_client_held_item_draw_call *c, unsigned int pc) {
    return gl(c, mc_client_display_source_scale(device(c), 1.0, 1.0, 1.0, &c->failure.display), pc);
}

static client_application *app(mc_client_held_item_draw_call *c, unsigned int pc) {
    client_application *a = c->state.owner->source->a;
    if (a == NULL)
        fail(c, MC_CLIENT_HELD_ITEM_DRAW_SOURCE_NULL, "actual RA null App receiver", pc, false);
    return a;
}

static bool hold_player(mc_client_held_item_draw_call *c, mc_entity *p, unsigned int pc) {
    if (p == NULL)
        return true;
    for (size_t i = 0; i < c->player_count; ++i)
        if (c->players[i] == p)
            return true;
    if (c->player_count == sizeof(c->players) / sizeof(*c->players))
        return fail(c, MC_CLIENT_HELD_ITEM_DRAW_DEPENDENCY,
                    "held draw distinct reached Player occurrence limit", pc, true);
    c->failure.entity = mc_entity_retain(p);
    if (c->failure.entity != MC_ENTITY_OK)
        return fail(c, MC_CLIENT_HELD_ITEM_DRAW_CHILD_FAILED, "held draw actual Player occurrence",
                    pc, true);
    c->players[c->player_count++] = p;
    return true;
}

static mc_entity *fresh_player(mc_client_held_item_draw_call *c, unsigned int app_pc,
                               unsigned int null_pc) {
    client_application *a = app(c, app_pc);
    if (a == NULL)
        return NULL;
    mc_entity *p = a->h;
    if (p == NULL) {
        fail(c, MC_CLIENT_HELD_ITEM_DRAW_SOURCE_NULL, "actual RA null Player receiver", null_pc,
             false);
        return NULL;
    }
    return hold_player(c, p, null_pc) ? p : NULL;
}

static bool math_use(mc_client_held_item_draw_call *c, unsigned int pc) {
    if (!c->math_held) {
        if (mc_math_classes_retain(c->math, &c->failure.math) != MC_MATH_CLASS_OK)
            return fail(c, MC_CLIENT_HELD_ITEM_DRAW_CHILD_FAILED, "held draw Math occurrence", pc,
                        true);
        c->math_held = true;
    }
    if (mc_math_classes_ensure(c->math, &c->failure.math) != MC_MATH_CLASS_OK)
        return fail(c, MC_CLIENT_HELD_ITEM_DRAW_CHILD_FAILED, "held draw Math active use", pc,
                    true);
    return fp(c, pc);
}

static bool swing(mc_client_held_item_draw_call *c, mc_entity *p, unsigned int pc, float *out) {
    mc_local_player_snapshot s;
    c->failure.entity = mc_local_player_get_snapshot(p, &s);
    if (c->failure.entity != MC_ENTITY_OK)
        return fail(c, MC_CLIENT_HELD_ITEM_DRAW_DEPENDENCY,
                    "actual LS.d swing concrete Player fields", pc, true);
    if (!fp(c, pc))
        return false;
    float delta = at_fsub(s.living.X, s.living.W);
    if (delta < 0.0f)
        delta = at_fadd(delta, 1.0f);
    *out = at_fadd(s.living.W, at_fmul(delta, c->state.partial));
    c->state.swing = *out;
    c->state.swing_returned = true;
    return true;
}

/* Actual DS.b sets KO.m=0 then FH.b(0,0,0,0,0,1/16). Every reached
 * trigonometric operation still reads the canonical Math table. Mutable riding,
 * held-item and sneaking model flags remain the actual FH fields. */
static bool pose(mc_client_held_item_draw_call *c, mc_client_biped_model *m) {
    if (m == NULL)
        return fail(c, MC_CLIENT_HELD_ITEM_DRAW_SOURCE_NULL, "actual DS.a null model", 1363, false);
    m->m = 0.0f;
    if (m->a == NULL || m->b == NULL || m->c == NULL || m->d == NULL || m->e == NULL ||
        m->f == NULL || m->g == NULL)
        return fail(c, MC_CLIENT_HELD_ITEM_DRAW_SOURCE_NULL, "actual FH null model part", 1363,
                    false);
    if (!fp(c, 1363))
        return false;
    m->a->e = at_fdiv(0.0f, 57.295776f);
    m->a->d = at_fdiv(0.0f, 57.295776f);
    m->b->e = m->a->e;
    m->b->d = m->a->d;
    if (!math_use(c, 1363))
        return false;
    const mc_math_trig *t = mc_math_classes_table(c->math);
    const float phase = at_fmul(0.0f, 0.6662f);
    m->d->d =
        at_fmul(at_fmul(at_fmul(mc_math_cos(t, at_fadd(phase, 3.1415927f)), 2.0f), 0.0f), 0.5f);
    m->e->d = at_fmul(at_fmul(at_fmul(mc_math_cos(t, phase), 2.0f), 0.0f), 0.5f);
    m->d->f = m->e->f = 0.0f;
    m->f->d = at_fmul(at_fmul(mc_math_cos(t, phase), 1.4f), 0.0f);
    m->g->d = at_fmul(at_fmul(mc_math_cos(t, at_fadd(phase, 3.1415927f)), 1.4f), 0.0f);
    m->f->e = m->g->e = 0.0f;
    if (m->n) {
        m->d->d = at_fadd(m->d->d, -0.62831855f);
        m->e->d = at_fadd(m->e->d, -0.62831855f);
        m->f->d = m->g->d = -1.2566371f;
        m->f->e = 0.31415927f;
        m->g->e = -0.31415927f;
    }
    if (m->j)
        m->e->d = at_fsub(at_fmul(m->e->d, 0.5f), 0.31415927f);
    if (m->k)
        m->d->d = at_fsub(at_fmul(m->d->d, 0.5f), 0.31415927f);
    m->d->e = m->e->e = 0.0f;
    if (m->m > -9990.0f) {
        m->c->e = at_fmul(
            mc_math_sin(t, at_fmul(at_fmul(at_root(at_widen(m->m)), 3.1415927f), 2.0f)), 0.2f);
        m->d->c = at_fmul(mc_math_sin(t, m->c->e), 5.0f);
        m->d->a = at_fmul(at_fneg(mc_math_cos(t, m->c->e)), 5.0f);
        m->e->c = at_fmul(at_fneg(mc_math_sin(t, m->c->e)), 5.0f);
        m->e->a = at_fmul(mc_math_cos(t, m->c->e), 5.0f);
        m->d->e = at_fadd(m->d->e, m->c->e);
        m->e->e = at_fadd(m->e->e, m->c->e);
        m->e->d = at_fadd(m->e->d, m->c->e);
        float q = at_fsub(1.0f, m->m);
        q = at_fmul(q, q);
        q = at_fmul(q, q);
        q = at_fsub(1.0f, q);
        const float a = mc_math_sin(t, at_fmul(q, 3.1415927f));
        const float b = at_fmul(
            at_fmul(mc_math_sin(t, at_fmul(m->m, 3.1415927f)), at_fneg(at_fsub(m->a->d, 0.7f))),
            0.75f);
        m->d->d =
            at_narrow(at_sub(at_widen(m->d->d), at_add(at_mul(at_widen(a), 1.2), at_widen(b))));
        m->d->e = at_fadd(m->d->e, at_fmul(m->c->e, 2.0f));
        m->d->f = at_fmul(mc_math_sin(t, at_fmul(m->m, 3.1415927f)), -0.4f);
    }
    if (m->l) {
        m->c->d = 0.5f;
        m->f->d = at_fsub(m->f->d, 0.0f);
        m->g->d = at_fsub(m->g->d, 0.0f);
        m->d->d = at_fadd(m->d->d, 0.4f);
        m->e->d = at_fadd(m->e->d, 0.4f);
        m->f->c = m->g->c = 4.0f;
        m->f->b = m->g->b = 9.0f;
        m->a->b = 1.0f;
    } else {
        m->c->d = 0.0f;
        m->f->c = m->g->c = 0.0f;
        m->f->b = m->g->b = 12.0f;
        m->a->b = 0.0f;
    }
    m->d->f =
        at_fadd(m->d->f, at_fadd(at_fmul(mc_math_cos(t, at_fmul(0.0f, 0.09f)), 0.05f), 0.05f));
    m->e->f =
        at_fsub(m->e->f, at_fadd(at_fmul(mc_math_cos(t, at_fmul(0.0f, 0.09f)), 0.05f), 0.05f));
    m->d->d = at_fadd(m->d->d, at_fmul(mc_math_sin(t, at_fmul(0.0f, 0.067f)), 0.05f));
    m->e->d = at_fsub(m->e->d, at_fmul(mc_math_sin(t, at_fmul(0.0f, 0.067f)), 0.05f));
    c->state.pose_returned = true;
    return true;
}

static manager_map_entry *map_entry(manager_map *map, const mc_entity_class_literal *key) {
    if (map != NULL)
        for (size_t i = 0; i < map->capacity; ++i)
            for (manager_map_entry *p = map->buckets[i]; p != NULL; p = p->next)
                if (p->key == key)
                    return p;
    return NULL;
}

bool entity_renderer_ds_arm_draw(mc_client_managed_renderer *renderer, mc_math_classes *math,
                                 mc_client_render_engine_classes *graphics_owner,
                                 mc_entity_classes *entity_classes, mc_client_display *display,
                                 mc_client_model_draw_error *model_error,
                                 mc_math_class_error *math_error) {
    if (model_error == NULL || math_error == NULL)
        return false;
    memset(model_error, 0, sizeof(*model_error));
    memset(math_error, 0, sizeof(*math_error));
    if (renderer == NULL || renderer->active || strcmp(renderer->source_class, "ds") != 0) {
        model_error->operation = "DS.b actual renderer admission";
        model_error->site = (mc_client_graphics_site){"ds", "b", "()V", 1};
        model_error->native_policy = renderer != NULL;
        return false;
    }
    mc_client_held_item_draw_call *c = calloc(1, sizeof(*c));
    if (c == NULL) {
        model_error->operation = "DS.b native operation storage";
        model_error->site = (mc_client_graphics_site){"ds", "b", "()V", -1};
        model_error->native_policy = true;
        return false;
    }
    c->math = math;
    c->math_held = true; /* Synchronous borrow; caller owns the occurrence. */
    renderer->active = true;
    mc_client_biped_model *m = renderer->own_model == NULL ? NULL : &renderer->own_model->fields;
    bool ok = pose(c, m);
    if (!ok) {
        model_error->operation = c->failure.operation;
        model_error->site = (mc_client_graphics_site){"ds", "b", "()V", m == NULL ? 5 : 19};
        model_error->native_policy = c->failure.native_policy;
        *math_error = c->failure.math;
    } else {
        ok = mc_client_model_part_draw(m->d, graphics_owner, entity_classes, display, 0.0625f,
                                       model_error);
    }
    renderer->active = false;
    free(c);
    return ok;
}

static bool draw_arm(mc_client_held_item_draw_call *c) {
    mc_client_render_manager_classes *classes = c->state.owner->context.services->models->th;
    if (classes == NULL)
        return fail(c, MC_CLIENT_HELD_ITEM_DRAW_DEPENDENCY, "actual TH namespace not bound", 1327,
                    true);
    if (mc_client_render_manager_static_get(classes, &c->manager, &c->failure.manager) !=
        MC_CLIENT_MANAGER_OK)
        return fail(c, MC_CLIENT_HELD_ITEM_DRAW_CHILD_FAILED, "actual TH.a active use", 1327, true);
    if (c->manager != NULL &&
        mc_client_render_manager_retain(c->manager, &c->failure.manager) != MC_CLIENT_MANAGER_OK) {
        c->manager = NULL;
        return fail(c, MC_CLIENT_HELD_ITEM_DRAW_CHILD_FAILED, "held draw TH occurrence", 1327,
                    true);
    }
    mc_entity *p = fresh_player(c, 1334, 1337);
    if (p == NULL)
        return false;
    if (c->manager == NULL)
        return fail(c, MC_CLIENT_HELD_ITEM_DRAW_SOURCE_NULL, "actual TH.a null receiver", 1337,
                    false);
    if (!mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER))
        return fail(c, MC_CLIENT_HELD_ITEM_DRAW_DEPENDENCY,
                    "actual TH lookup unsupported concrete Player Class", 1337, true);
    manager_map_entry *found = map_entry(c->manager->map, p->class_literal);
    if (found == NULL) {
        const mc_entity_class_literal *parent = NULL;
        c->failure.entity =
            mc_entity_class_literal_get(p->classes, MC_ENTITY_CLASS_KIND_PLAYER, &parent);
        if (c->failure.entity != MC_ENTITY_OK)
            return fail(c, MC_CLIENT_HELD_ITEM_DRAW_CHILD_FAILED,
                        "actual dc.getSuperclass gs Class", 1337, true);
        found = map_entry(c->manager->map, parent);
        if (found == NULL)
            return fail(c, MC_CLIENT_HELD_ITEM_DRAW_DEPENDENCY,
                        "actual TH gs parent renderer missing", 1337, true);
        if (!manager_map_put(c->manager, p->class_literal, found->owner, 28)) {
            c->failure.manager = classes->failure;
            return fail(c, MC_CLIENT_HELD_ITEM_DRAW_CHILD_FAILED,
                        "actual TH recursive Class cache put", 1337, true);
        }
        found = map_entry(c->manager->map, p->class_literal);
    }
    if (found->owner == NULL || strcmp(found->owner->source_class, "ds") != 0)
        return fail(c, MC_CLIENT_HELD_ITEM_DRAW_DEPENDENCY, "actual RA checkcast DS renderer", 1344,
                    true);
    if (mc_client_managed_renderer_retain(found->owner, &c->failure.manager) !=
        MC_CLIENT_MANAGER_OK)
        return fail(c, MC_CLIENT_HELD_ITEM_DRAW_CHILD_FAILED, "held draw DS occurrence", 1337,
                    true);
    c->renderer = found->owner;
    c->state.renderer_returned = true;
    if (!scale_one(c, 1358))
        return false;
    if (!entity_renderer_ds_arm_draw(c->renderer, c->math, graphics(c), entities(c), device(c),
                                     &c->model, &c->failure.math)) {
        c->failure.model = &c->model;
        c->failure.child = c->model.site;
        return fail(c, MC_CLIENT_HELD_ITEM_DRAW_CHILD_FAILED, c->model.operation, 1363,
                    c->model.native_policy);
    }
    c->state.pose_returned = true;
    c->state.faces = c->model.faces;
    c->state.vertices = c->model.vertices;
    c->state.arm_returned = true;
    return true;
}

static bool empty_hand(mc_client_held_item_draw_call *c) {
    if (!push(c, 1083))
        return false;
    float s;
    if (!swing(c, c->state.player, 1092, &s) || !math_use(c, 1102))
        return false;
    const mc_math_trig *t = mc_math_classes_table(c->math);
    const float f4 = mc_math_sin(t, at_fmul(s, 3.1415927f));
    const float f3 = mc_math_sin(t, at_fmul(at_root(at_widen(s)), 3.1415927f));
    const float y =
        at_fmul(mc_math_sin(t, at_fmul(at_fmul(at_root(at_widen(s)), 3.1415927f), 2.0f)), 0.4f);
    if (!translate(c, at_fmul(at_fneg(f3), 0.3f), y, at_fmul(at_fneg(f4), 0.4f), 1148) ||
        !translate(c, at_fmul(0.8f, 0.8f),
                   at_fsub(at_fmul(-0.75f, 0.8f), at_fmul(at_fsub(1.0f, c->state.equipped), 0.6f)),
                   at_fmul(-0.9f, 0.8f), 1173) ||
        !rotate(c, 45, 0, 1, 0, 1181) ||
        !gl(c, mc_client_display_source_rescale_normal(device(c), true, &c->failure.display), 1186))
        return false;
    c->state.rescale_enabled = true;
    if (!swing(c, c->state.player, 1191, &s) || !math_use(c, 1204))
        return false;
    const float second4 = mc_math_sin(t, at_fmul(at_fmul(s, s), 3.1415927f));
    const float second3 = mc_math_sin(t, at_fmul(at_root(at_widen(s)), 3.1415927f));
    if (!rotate(c, at_fmul(second3, 70.0f), 0, 1, 0, 1230) ||
        !rotate(c, at_fmul(at_fneg(second4), 20.0f), 0, 0, 1, 1242))
        return false;
    client_application *a = app(c, 1252);
    if (a == NULL)
        return false;
    mc_client_render_engine *engine = a->render_engine; /* Captured before arguments. */
    mc_entity *skin_player = fresh_player(c, 1259, 1262);
    if (skin_player == NULL)
        return false;
    const mc_player_string *url = skin_player->base.client_only.bA;
    mc_entity *default_player = fresh_player(c, 1269, 1272);
    if (default_player == NULL)
        return false;
    mc_local_player_snapshot snapshot;
    c->failure.entity = mc_local_player_get_snapshot(default_player, &snapshot);
    if (c->failure.entity != MC_ENTITY_OK)
        return fail(c, MC_CLIENT_HELD_ITEM_DRAW_DEPENDENCY,
                    "actual virtual Player.q_ texture fields", 1272, true);
    c->state.skin_arguments_read = true;
    if (engine == NULL)
        return fail(c, MC_CLIENT_HELD_ITEM_DRAW_SOURCE_NULL,
                    "actual skin texture null Engine receiver", 1275, false);
    if (mc_client_render_engine_retain(engine) != MC_CLIENT_RENDER_ENGINE_OK)
        return fail(c, MC_CLIENT_HELD_ITEM_DRAW_CHILD_FAILED, "held skin Engine occurrence", 1275,
                    true);
    c->engine = engine;
    const bool foreign = mc_client_texture_lookup_pending(engine);
    mc_client_texture texture;
    const mc_client_image_download_status status = mc_client_render_download_texture(
        engine, device(c), url, snapshot.living.O, &texture, &c->download);
    c->lookup_owned = !foreign && mc_client_texture_lookup_pending(engine);
    if (status != MC_CLIENT_IMAGE_DOWNLOAD_OK) {
        c->failure.download = &c->download;
        c->failure.texture = &c->download.lookup;
        const mc_client_image_download_site child = c->download.download.site;
        c->failure.child = (mc_client_graphics_site){child.class_name, child.method,
                                                     child.descriptor, (int32_t)child.pc};
        return fail(c,
                    status == MC_CLIENT_IMAGE_DOWNLOAD_UNIMPLEMENTED
                        ? MC_CLIENT_HELD_ITEM_DRAW_DEPENDENCY
                        : MC_CLIENT_HELD_ITEM_DRAW_CHILD_FAILED,
                    c->download.download.operation, 1275, c->download.download.native_policy);
    }
    c->state.skin_returned = true;
    if (!gl(c, mc_client_display_source_bind_id(device(c), texture, &c->failure.display), 1278) ||
        !translate(c, -1, 3.6f, 3.5f, 1287) || !rotate(c, 120, 0, 0, 1, 1295) ||
        !rotate(c, 200, 1, 0, 0, 1303) || !rotate(c, -135, 0, 1, 0, 1311) || !scale_one(c, 1317) ||
        !translate(c, 5.6f, 0, 0, 1324) || !draw_arm(c) || !pop(c, 1366))
        return false;
    return true;
}

static bool hand(mc_client_held_item_draw_call *c) {
    mc_client_source_ra *ra = c->state.owner->source;
    if (!fp(c, 12))
        return false;
    c->state.equipped = at_fadd(ra->d, at_fmul(at_fsub(ra->c, ra->d), c->state.partial));
    client_application *a = app(c, 21);
    if (a == NULL)
        return false;
    c->state.player = a->h; /* Captured dc may be null until aV26. */
    c->state.player_captured = true;
    if (c->state.player == NULL)
        return fail(c, MC_CLIENT_HELD_ITEM_DRAW_SOURCE_NULL,
                    "actual captured dc null pitch receiver", 26, false);
    if (!hold_player(c, c->state.player, 26))
        return false;
    mc_entity *p = c->state.player;
    if (!fp(c, 37))
        return false;
    c->state.pitch =
        at_fadd(p->base.aV, at_fmul(at_fsub(p->base.aT, p->base.aV), c->state.partial));
    if (!push(c, 43) || !rotate(c, c->state.pitch, 1, 0, 0, 51) || !fp(c, 66))
        return false;
    const float yaw =
        at_fadd(p->base.aU, at_fmul(at_fsub(p->base.yaw, p->base.aU), c->state.partial));
    if (!rotate(c, yaw, 0, 1, 0, 73))
        return false;
    if (!mc_client_lighting_enable(graphics(c), entities(c), c->math, device(c), where(c, 76),
                                   &c->lighting)) {
        c->failure.lighting = &c->lighting;
        c->failure.child = c->lighting.site;
        return fail(c, MC_CLIENT_HELD_ITEM_DRAW_CHILD_FAILED, c->lighting.operation, 76,
                    c->lighting.native_policy);
    }
    c->state.lighting_enabled = true;
    if (!pop(c, 79))
        return false;
    c->state.stack = ra->b;
    c->state.stack_captured = true;
    if (ra->b != NULL) {
        c->failure.stack = mc_item_stack_retain(ra->b);
        if (c->failure.stack != MC_ITEM_STACK_OK)
            return fail(c, MC_CLIENT_HELD_ITEM_DRAW_CHILD_FAILED, "held actual iz occurrence", 83,
                        true);
        c->stack_held = true;
    }
    a = app(c, 89);
    if (a == NULL)
        return false;
    c->world = a->active_world;
    if (c->world != NULL) {
        if (!mc_world_runtime_binding_retain(c->world))
            return fail(c, MC_CLIENT_HELD_ITEM_DRAW_CHILD_FAILED,
                        "held brightness World occurrence", 92, true);
        c->world_held = true;
    }
    if (!math_use(c, 99))
        return false;
    const int32_t x = mc_math_floor_double(p->base.x);
    if (!math_use(c, 106))
        return false;
    const int32_t y = mc_math_floor_double(p->base.y);
    if (!math_use(c, 113))
        return false;
    const int32_t z = mc_math_floor_double(p->base.z);
    if (c->world == NULL)
        return fail(c, MC_CLIENT_HELD_ITEM_DRAW_SOURCE_NULL,
                    "actual brightness null World receiver", 116, false);
    if (mc_world_runtime_brightness(c->world, x, y, z, &c->state.brightness,
                                    &c->failure.brightness) != MC_WORLD_STORAGE_OK)
        return fail(c, MC_CLIENT_HELD_ITEM_DRAW_CHILD_FAILED, "actual World brightness child", 116,
                    true);
    c->state.brightness_returned = true;
    if (c->state.stack != NULL)
        return fail(c, MC_CLIENT_HELD_ITEM_DRAW_DEPENDENCY,
                    "actual GM item-color and held item/map geometry descendants", 126, true);
    if (!gl(c,
            mc_client_display_source_color(device(c), c->state.brightness, c->state.brightness,
                                           c->state.brightness, 1, &c->failure.display),
            216))
        return false;
    c->state.color_set = c->state.empty_hand = true;
    if (!empty_hand(c) ||
        !gl(c, mc_client_display_source_rescale_normal(device(c), false, &c->failure.display),
            1371))
        return false;
    c->state.rescale_enabled = false;
    if (!mc_client_lighting_disable(graphics(c), device(c), where(c, 1374), &c->failure.graphics)) {
        c->failure.child = c->failure.graphics.site;
        return fail(c, MC_CLIENT_HELD_ITEM_DRAW_CHILD_FAILED, c->failure.graphics.operation, 1374,
                    c->failure.graphics.native_policy);
    }
    c->state.lighting_enabled = false;
    c->state.source_pc = 1377;
    c->state.returned = true;
    return true;
}

static bool overlays(mc_client_held_item_draw_call *c) {
    if (!gl(c, mc_client_display_source_alpha_test(device(c), false, &c->failure.display), 3))
        return false;
    c->state.alpha_disabled = true;
    mc_entity *p = fresh_player(c, 10, 13);
    if (p == NULL)
        return false;
    c->state.burning = p->base.bv > 0;
    if (!c->state.burning) {
        mc_entity_watcher_snapshot w;
        c->failure.entity = mc_entity_watcher_get_snapshot(p->watcher, &w);
        if (c->failure.entity != MC_ENTITY_OK)
            return fail(c, MC_CLIENT_HELD_ITEM_DRAW_CHILD_FAILED, "actual sn.ak watcher flag read",
                        13, true);
        c->state.burning = (w.byte_value & 1) != 0;
    }
    c->state.burning_read = true;
    if (c->state.burning)
        return fail(c, MC_CLIENT_HELD_ITEM_DRAW_DEPENDENCY,
                    "actual burning terrain lookup and RA.d fire overlay", 28, true);
    p = fresh_player(c, 48, 51);
    if (p == NULL)
        return false;
    if (mc_entity_tick_suffocating(p, &c->state.suffocating, &c->failure.tick) != MC_ENTITY_OK)
        return fail(c, MC_CLIENT_HELD_ITEM_DRAW_CHILD_FAILED, "actual Player.L suffocation child",
                    51, true);
    c->state.suffocating_read = true;
    if (c->state.suffocating)
        return fail(c, MC_CLIENT_HELD_ITEM_DRAW_DEPENDENCY,
                    "actual suffocation overlay block selection", 67, true);
    client_application *a = app(c, 363);
    if (a == NULL)
        return false;
    p = a->h; /* The nullable receiver precedes LN.g active use. */
    if (!hold_player(c, p, 363))
        return false;
    mc_material *water = NULL;
    if (mc_registry_material_get(a->registry, MC_MATERIAL_g, &water, &c->failure.registry) !=
        MC_REGISTRY_OK)
        return fail(c, MC_CLIENT_HELD_ITEM_DRAW_CHILD_FAILED, "actual LN.g active use", 366, true);
    if (p == NULL)
        return fail(c, MC_CLIENT_HELD_ITEM_DRAW_SOURCE_NULL,
                    "actual RA null Player material receiver", 369, false);
    if (mc_entity_inside_material(p, water, &c->state.water, &c->failure.material) != MC_ENTITY_OK)
        return fail(c, MC_CLIENT_HELD_ITEM_DRAW_CHILD_FAILED, "actual Player material immersion",
                    369, true);
    c->state.water_read = true;
    if (c->state.water)
        return fail(c, MC_CLIENT_HELD_ITEM_DRAW_DEPENDENCY,
                    "actual water texture lookup and RA.c overlay", 384, true);
    if (!gl(c, mc_client_display_source_alpha_test(device(c), true, &c->failure.display), 403))
        return false;
    c->state.alpha_restored = true;
    c->state.source_pc = 406;
    c->state.returned = true;
    return true;
}

static mc_client_held_item_draw_status begin(mc_client_held_item *owner, mc_math_classes *math,
                                             float partial, bool overlay, bool entered,
                                             mc_client_held_item_draw_call **out,
                                             mc_client_held_item_draw_error *error) {
    if (owner == NULL || owner->source == NULL || out == NULL || *out != NULL)
        return admission(error, MC_CLIENT_HELD_ITEM_DRAW_ARGUMENT, "held draw actual owner/output");
    if (owner->draw_call != NULL || (owner->active != entered) ||
        owner->source->phase == MC_CLIENT_ENTITY_RENDERER_CONSTRUCTING ||
        owner->source->phase == MC_CLIENT_ENTITY_RENDERER_RETIRING)
        return admission(error, MC_CLIENT_HELD_ITEM_DRAW_BUSY, "held draw actual RA admission");
    mc_client_held_item_draw_call *c = calloc(1, sizeof(*c));
    if (c == NULL)
        return admission(error, MC_CLIENT_HELD_ITEM_DRAW_ALLOCATION,
                         "held draw retained native call");
    admission(&c->failure, MC_CLIENT_HELD_ITEM_DRAW_OK, NULL);
    c->state.owner = owner;
    c->state.partial = partial;
    c->state.overlays = overlay;
    c->state.failure = &c->failure;
    c->state.cleanup = &c->cleanup;
    c->math = math;
    c->entered = entered;
    if (!entered) {
        if (mc_client_held_item_retain(owner, &c->failure.owner) != MC_CLIENT_ENTITY_RENDERER_OK) {
            free(c);
            return admission(error, MC_CLIENT_HELD_ITEM_DRAW_BUSY,
                             "held draw actual RA occurrence");
        }
        c->owner_held = true;
    }
    owner->draw_call = c;
    c->state.active = owner->active = true;
    *out = c;
    (void)(overlay ? overlays(c) : hand(c));
    c->state.active = false;
    owner->active = entered;
    if (error != NULL)
        *error = c->failure;
    return c->failure.status;
}

mc_client_held_item_draw_status
mc_client_held_item_draw_begin(mc_client_held_item *owner, mc_math_classes *math, float partial,
                               bool overlay, mc_client_held_item_draw_call **out,
                               mc_client_held_item_draw_error *error) {
    return begin(owner, math, partial, overlay, false, out, error);
}

mc_client_held_item_draw_status
entity_renderer_held_draw_entered(mc_client_held_item *owner, mc_math_classes *math, float partial,
                                  bool overlay, mc_client_held_item_draw_call **out,
                                  mc_client_held_item_draw_error *error) {
    return begin(owner, math, partial, overlay, true, out, error);
}

bool mc_client_held_item_draw_inspect(const mc_client_held_item_draw_call *c,
                                      mc_client_held_item_draw_state *out) {
    if (c == NULL || out == NULL)
        return false;
    *out = c->state;
    out->faces = c->model.faces;
    out->vertices = c->model.vertices;
    return true;
}

static mc_client_held_item_draw_status cleanup_failed(mc_client_held_item_draw_call *c,
                                                      mc_client_held_item_draw_error *out,
                                                      const char *operation) {
    c->cleanup.status = MC_CLIENT_HELD_ITEM_DRAW_BUSY;
    c->cleanup.operation = operation;
    c->cleanup.native_policy = true;
    c->cleanup.site = (mc_client_graphics_site){NULL, NULL, NULL, -1};
    if (out != NULL)
        *out = c->cleanup;
    return MC_CLIENT_HELD_ITEM_DRAW_BUSY;
}

mc_client_held_item_draw_status
mc_client_held_item_draw_retire(mc_client_held_item_draw_call **slot,
                                mc_client_held_item_draw_error *error) {
    if (slot == NULL)
        return admission(error, MC_CLIENT_HELD_ITEM_DRAW_ARGUMENT, "held draw retirement slot");
    mc_client_held_item_draw_call *c = *slot;
    if (c == NULL)
        return admission(error, MC_CLIENT_HELD_ITEM_DRAW_OK, NULL);
    mc_client_held_item *owner = c->state.owner;
    if (c->state.active || (!c->entered && owner->active))
        return admission(error, MC_CLIENT_HELD_ITEM_DRAW_BUSY, "held draw active retirement");
    if (c->model.pending_list) {
        if (!mc_client_display_lists_abort(device(c), c->model.owned_list, &c->cleanup.display))
            return cleanup_failed(c, error, "held draw owned list abort");
        c->model.pending_list = false;
    }
    if (c->lookup_owned) {
        (void)mc_client_render_engine_retire_lookup(c->engine, &c->lookup_cleanup);
        c->lookup_owned = mc_client_texture_lookup_pending(c->engine);
        if (c->lookup_owned) {
            c->cleanup.texture = &c->lookup_cleanup;
            return cleanup_failed(c, error, "held draw owned texture lookup cleanup");
        }
    }
    if (c->renderer != NULL && mc_client_managed_renderer_release(
                                   &c->renderer, &c->cleanup.manager) != MC_CLIENT_MANAGER_OK)
        return cleanup_failed(c, error, "held draw DS occurrence cleanup");
    if (c->manager != NULL &&
        mc_client_render_manager_release(&c->manager, &c->cleanup.manager) != MC_CLIENT_MANAGER_OK)
        return cleanup_failed(c, error, "held draw TH occurrence cleanup");
    if (c->engine != NULL) {
        if (mc_client_render_engine_release(c->engine) != MC_CLIENT_RENDER_ENGINE_OK)
            return cleanup_failed(c, error, "held draw Engine occurrence cleanup");
        c->engine = NULL;
    }
    if (c->math_held) {
        if (mc_math_classes_release(&c->math, &c->cleanup.math) != MC_MATH_CLASS_OK)
            return cleanup_failed(c, error, "held draw Math occurrence cleanup");
        c->math_held = false;
    }
    if (c->world_held) {
        mc_world_runtime_binding_release(c->world);
        c->world_held = false;
    }
    while (c->player_count != 0)
        mc_entity_release(c->players[--c->player_count]);
    if (c->stack_held) {
        mc_item_stack_release((mc_item_stack *)c->state.stack);
        c->stack_held = false;
    }
    owner->draw_call = NULL;
    if (c->owner_held) {
        mc_client_held_item *occurrence = owner;
        if (mc_client_held_item_release(&occurrence, &c->cleanup.owner) !=
            MC_CLIENT_ENTITY_RENDERER_OK) {
            owner->draw_call = c;
            return cleanup_failed(c, error, "held draw RA occurrence cleanup");
        }
    }
    free(c);
    *slot = NULL;
    return admission(error, MC_CLIENT_HELD_ITEM_DRAW_OK, NULL);
}
