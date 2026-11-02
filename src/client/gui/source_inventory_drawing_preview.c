#include "entity/action/atan_internal.h"
#include "source_inventory_drawing_internal.h"

bool mc_inventory_draw_background(mc_client_inventory_draw_call *c) {
    mc_client_source_ue *o = c->state.receiver;
    mc_client_source_id *id = c->owner;
    if (id->base.kind != MC_CLIENT_SCREEN_INVENTORY || o == NULL || &o->container != id)
        return mc_inventory_draw_fail(c, MC_CLIENT_SCREEN_INVALID_ARGUMENT,
                                      "UE preview actual concrete receiver", NULL, NULL, NULL, 0,
                                      true);
    mc_client_display *d = c->services.display;
    mc_client_texture texture;
    if (!mc_inventory_draw_texture(c, id->base.fields.b->render_engine, "/gui/inventory.png", "ue",
                                   9, &texture))
        return false;
#define GL(expr, pc)                                                                               \
    do {                                                                                           \
        if (!mc_inventory_draw_gl(c, (expr), "ue", (pc)))                                          \
            return false;                                                                          \
    } while (0)
    GL(mc_client_display_source_color(d, 1, 1, 1, 1, &c->error.display), 17);
    /* The JI receiver is independently loaded after lookup/color. */
    if (id->base.fields.b->render_engine == NULL)
        return mc_inventory_draw_fail(c, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE,
                                      "UE actual fresh JI bind null receiver", "ue", "a", "(F)V",
                                      28, false);
    if (mc_java_i32_narrow((int64_t)texture) >= 0)
        GL(mc_client_display_source_bind_id(d, texture, &c->error.display), 28);
    const int32_t x = mc_java_i32_sub(id->base.fields.c, id->id.a) / 2;
    const int32_t y = mc_java_i32_sub(id->base.fields.d, id->id.i) / 2;
    if (!mc_inventory_draw_tile(c, x, y, 0, 0, id->id.a, id->id.i))
        return false;
    c->state.background_returned = true;
    GL(mc_client_display_source_rescale_normal(d, true, &c->error.display), 75);
    GL(mc_client_display_source_color_material_enable(d, true, &c->error.display), 81);
    GL(mc_client_display_source_push(d, &c->error.display), 84);
    if (!mc_inventory_draw_fp(c, "ue", 91))
        return false;
    GL(mc_client_display_source_translate(d, (float)mc_java_i32_add(x, 51),
                                          (float)mc_java_i32_add(y, 75), 50, &c->error.display),
       100);
    GL(mc_client_display_source_scale(d, -30, 30, 30, &c->error.display), 114);
    GL(mc_client_display_source_rotate(d, 180, 0, 0, 1, &c->error.display), 122);
    mc_entity *p = mc_inventory_draw_player(c, "ue", 132);
    if (p == NULL)
        return false;
    const float saved_body = mc_entity_living_fields(p)->H;
    p = mc_inventory_draw_player(c, "ue", 144);
    if (p == NULL)
        return false;
    const float saved_yaw = p->base.yaw;
    p = mc_inventory_draw_player(c, "ue", 156);
    if (p == NULL || !mc_inventory_draw_fp(c, "ue", 165))
        return false;
    const float saved_pitch = p->base.aT;
    const float dx = at_fsub((float)mc_java_i32_add(x, 51), o->fields.l);
    const float dy = at_fsub((float)mc_java_i32_sub(mc_java_i32_add(y, 75), 50), o->fields.m);
    GL(mc_client_display_source_rotate(d, 135, 0, 1, 0, &c->error.display), 194);
    if (!mc_client_lighting_enable(
            c->services.graphics, id->base.fields.b->entities, c->services.math, d,
            (mc_client_graphics_site){"ue", "a", "(F)V", 197}, &c->error.lighting))
        return mc_inventory_draw_fail(c, MC_CLIENT_SCREEN_TEXTURE_FAILED,
                                      c->error.lighting.operation, "ue", "a", "(F)V", 197,
                                      c->error.lighting.native_policy);
    GL(mc_client_display_source_rotate(d, -135, 0, 1, 0, &c->error.display), 205);
    if (!mc_inventory_draw_fp(c, "ue", 212))
        return false;
    float angle = at_fmul(at_fneg(at_narrow(mc_action_atan(at_widen(at_fdiv(dy, 40))))), 20);
    GL(mc_client_display_source_rotate(d, angle, 1, 0, 0, &c->error.display), 225);
    /* Each source field receiver is captured before its separate Math.atan. */
    if (!mc_inventory_draw_player_capture(c, "ue", 232, &p) || !mc_inventory_draw_fp(c, "ue", 239))
        return false;
    angle = at_fmul(at_narrow(mc_action_atan(at_widen(at_fdiv(dx, 40)))), 20);
    if (p == NULL)
        return mc_inventory_draw_fail(c, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE,
                                      "UE actual H null store receiver", "ue", "a", "(F)V", 248,
                                      false);
    mc_entity_living_fields(p)->H = angle;
    if (!mc_inventory_draw_player_capture(c, "ue", 255, &p) || !mc_inventory_draw_fp(c, "ue", 262))
        return false;
    angle = at_fmul(at_narrow(mc_action_atan(at_widen(at_fdiv(dx, 40)))), 40);
    if (p == NULL)
        return mc_inventory_draw_fail(c, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE,
                                      "UE actual aS null store receiver", "ue", "a", "(F)V", 271,
                                      false);
    p->base.yaw = angle;
    if (!mc_inventory_draw_player_capture(c, "ue", 278, &p) || !mc_inventory_draw_fp(c, "ue", 285))
        return false;
    angle = at_fmul(at_fneg(at_narrow(mc_action_atan(at_widen(at_fdiv(dy, 40))))), 20);
    if (p == NULL)
        return mc_inventory_draw_fail(c, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE,
                                      "UE actual aT null store receiver", "ue", "a", "(F)V", 295,
                                      false);
    p->base.aT = angle;
    p = mc_inventory_draw_player(c, "ue", 308);
    if (p == NULL)
        return false;
    p->base.bE = 1;
    p = mc_inventory_draw_player(c, "ue", 317);
    if (p == NULL)
        return false;
    GL(mc_client_display_source_translate(d, 0, p->base.offset, 0, &c->error.display), 321);
    mc_client_render_manager *store = NULL;
    mc_client_render_classes *models = id->base.fields.b->render_classes;
    if (models == NULL || mc_client_render_manager_static_get(
                              models->th, &store, &c->error.manager) != MC_CLIENT_MANAGER_OK)
        return mc_inventory_draw_fail(c, MC_CLIENT_SCREEN_APP_FAILED, "UE actual TH.a active use",
                                      "ue", "a", "(F)V", 324, true);
    if (store == NULL)
        return mc_inventory_draw_fail(c, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE,
                                      "UE actual TH.i null store receiver", "ue", "a", "(F)V", 329,
                                      false);
    if (mc_client_render_manager_retain(store, &c->error.manager) != MC_CLIENT_MANAGER_OK)
        return mc_inventory_draw_fail(c, MC_CLIENT_SCREEN_BUSY, "UE captured TH store owner hold",
                                      "ue", "a", "(F)V", 329, true);
    c->store_manager = store;
    store->fields.i = 180;
    mc_client_render_manager *draw = NULL;
    models = id->base.fields.b->render_classes;
    if (models == NULL || mc_client_render_manager_static_get(
                              models->th, &draw, &c->error.manager) != MC_CLIENT_MANAGER_OK)
        return mc_inventory_draw_fail(c, MC_CLIENT_SCREEN_APP_FAILED,
                                      "UE independent TH.a draw load", "ue", "a", "(F)V", 332,
                                      true);
    /* A null actor is an actual source operand. TH owns its reached failure. */
    mc_entity *actor = id->base.fields.b->h;
    const mc_client_manager_status draw_status = mc_client_render_manager_coordinates_draw_begin(
        draw, actor, 0, 0, 0, 0, 1, &c->services, NULL, &c->preview, &c->error.player);
    if (c->preview != NULL) {
        (void)mc_client_render_manager_draw_inspect(c->preview, &c->preview_state);
        c->state.preview = &c->preview_state;
    }
    if (draw_status != MC_CLIENT_MANAGER_OK)
        return mc_inventory_draw_fail(c, MC_CLIENT_SCREEN_APP_FAILED, c->error.player.operation,
                                      "ue", "a", "(F)V", 347, c->error.player.native_policy);
    c->state.preview_returned = true;
    p = mc_inventory_draw_player(c, "ue", 358);
    if (p == NULL)
        return false;
    p->base.bE = 0;
    p = mc_inventory_draw_player(c, "ue", 370);
    if (p == NULL)
        return false;
    mc_entity_living_fields(p)->H = saved_body;
    p = mc_inventory_draw_player(c, "ue", 382);
    if (p == NULL)
        return false;
    p->base.yaw = saved_yaw;
    p = mc_inventory_draw_player(c, "ue", 394);
    if (p == NULL)
        return false;
    p->base.aT = saved_pitch;
    c->state.pose_restored = true;
    GL(mc_client_display_source_pop(d, &c->error.display), 397);
    if (!mc_client_lighting_disable(c->services.graphics, d,
                                    (mc_client_graphics_site){"ue", "a", "(F)V", 400},
                                    &c->error.graphics))
        return mc_inventory_draw_fail(c, MC_CLIENT_SCREEN_TEXTURE_FAILED,
                                      c->error.graphics.operation, "ue", "a", "(F)V", 400, true);
    GL(mc_client_display_source_rescale_normal(d, false, &c->error.display), 405);
#undef GL
    return true;
}
