#include "mc/client/gui/gui_ingame_draw.h"
#include "../../entity/entity_internal.h"
#include "../../entity/player/tick_math_internal.h"
#include "../../item/registry/registry_internal.h"
#include "../application_state.h"
#include "../display/camera_surface.h"
#include "../display/fog_surface.h"
#include "../render/model_graph_internal.h"
#include "../render_engine/lighting.h"
#include "../render_engine/nw.h"
#include "../render_engine/texture_lookup.h"
#include "bitmap_font.h"
#include "gui_ingame_internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/client/controller/singleplayer.h"
#include "mc/client/world/overlay_setup.h"
#include "mc/core/math/math.h"
#include "mc/item/registry/runtime.h"
#include "mc/world/runtime/entities.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

struct mc_client_gui_ingame_draw_call {
    mc_client_gui_ingame_draw_state state;
    mc_client_gui_ingame_draw_error failure, cleanup;
    mc_client_render_engine_classes *graphics;
    mc_math_classes *math;
    mc_client_display *display;
    mc_client_tessellator *nw;
    mc_client_overlay_setup_state projection;
    mc_client_overlay_setup_error projection_error;
    mc_client_lighting_error lighting;
    mc_client_application_owner_error application_error;
    mc_client_bitmap_font *font;
    mc_client_bitmap_font_error font_error;
    mc_entity *players[12];
    mc_world_runtime *worlds[4];
    mc_game_settings *settings[8];
    mc_client_render_engine *engines[3];
    mc_client_texture_lookup_error textures[3], texture_cleanup;
    mc_client_singleplayer_controller *controller;
    size_t player_count, world_count, settings_count, engine_count;
    bool lookup_owned[3], owner_held, graphics_held, math_held, flag;
};

static mc_client_gui_ingame_draw_status admission(mc_client_gui_ingame_draw_error *out,
                                                  mc_client_gui_ingame_draw_status status,
                                                  const char *operation) {
    if (out != NULL) {
        memset(out, 0, sizeof(*out));
        out->status = status;
        out->operation = operation;
        out->native_policy = status != MC_CLIENT_GUI_INGAME_DRAW_OK;
        out->site.pc = out->child.pc = -1;
    }
    return status;
}

static mc_client_graphics_site site(unsigned int pc) {
    return (mc_client_graphics_site){"uq", "a", "(FZII)V", (int32_t)pc};
}

static bool fail(mc_client_gui_ingame_draw_call *c, mc_client_gui_ingame_draw_status status,
                 const char *operation, unsigned int pc, bool native) {
    c->failure.status = status;
    c->failure.operation = operation;
    c->failure.site = site(pc);
    c->failure.native_policy = native;
    c->state.source_pc = pc;
    return false;
}

static bool child_fail(mc_client_gui_ingame_draw_call *c, mc_client_gui_ingame_draw_status status,
                       const char *operation, unsigned int parent, const char *cl,
                       const char *method, const char *descriptor, unsigned int pc, bool native) {
    c->failure.child = (mc_client_graphics_site){cl, method, descriptor, (int32_t)pc};
    return fail(c, status, operation, parent, native);
}

static bool fp(mc_client_gui_ingame_draw_call *c, unsigned int pc) {
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u || fail(c, MC_CLIENT_GUI_INGAME_DRAW_UNSUPPORTED_FP,
                                                       "HUD strict floating controls", pc, true);
}

static bool gl(mc_client_gui_ingame_draw_call *c, bool ok, unsigned int pc) {
    if (!ok)
        return fail(c, MC_CLIENT_GUI_INGAME_DRAW_CHILD_FAILED, c->failure.display.operation, pc,
                    true);
    ++c->state.gl_calls;
    c->state.source_pc = pc;
    return true;
}

static bool graphical(mc_client_gui_ingame_draw_call *c, bool ok, unsigned int pc) {
    if (ok)
        return true;
    c->failure.child = c->failure.graphics.site;
    return fail(c, MC_CLIENT_GUI_INGAME_DRAW_CHILD_FAILED, c->failure.graphics.operation, pc,
                c->failure.graphics.native_policy);
}

static client_application *app(mc_client_gui_ingame_draw_call *c, unsigned int pc) {
    client_application *a = c->state.owner->fields.g;
    if (a == NULL)
        fail(c, MC_CLIENT_GUI_INGAME_DRAW_SOURCE_NULL, "actual UQ null App receiver", pc, false);
    return a;
}

static bool hold_player(mc_client_gui_ingame_draw_call *c, mc_entity *p, unsigned int pc) {
    if (p == NULL)
        return true;
    for (size_t i = 0; i < c->player_count; ++i)
        if (c->players[i] == p)
            return true;
    if (c->player_count == sizeof(c->players) / sizeof(*c->players))
        return fail(c, MC_CLIENT_GUI_INGAME_DRAW_DEPENDENCY, "HUD reached Player occurrence bound",
                    pc, true);
    c->failure.entity = mc_entity_retain(p);
    if (c->failure.entity != MC_ENTITY_OK)
        return fail(c, MC_CLIENT_GUI_INGAME_DRAW_CHILD_FAILED, "HUD actual Player occurrence", pc,
                    true);
    c->players[c->player_count++] = p;
    return true;
}

static mc_entity *player(mc_client_gui_ingame_draw_call *c, unsigned int app_pc, unsigned int pc) {
    client_application *a = app(c, app_pc);
    if (a == NULL)
        return NULL;
    if (a->h == NULL) {
        fail(c, MC_CLIENT_GUI_INGAME_DRAW_SOURCE_NULL, "actual UQ null Player receiver", pc, false);
        return NULL;
    }
    return hold_player(c, a->h, pc) ? a->h : NULL;
}

static bool player_fields(mc_client_gui_ingame_draw_call *c, mc_entity *p,
                          mc_local_player_snapshot *out, unsigned int pc) {
    c->failure.entity = mc_local_player_get_snapshot(p, out);
    return c->failure.entity == MC_ENTITY_OK ||
           fail(c, MC_CLIENT_GUI_INGAME_DRAW_DEPENDENCY, "HUD actual concrete dc fields", pc, true);
}

static bool hold_world(mc_client_gui_ingame_draw_call *c, mc_world_runtime *w, unsigned int pc) {
    if (w == NULL)
        return true;
    for (size_t i = 0; i < c->world_count; ++i)
        if (c->worlds[i] == w)
            return true;
    if (c->world_count == sizeof(c->worlds) / sizeof(*c->worlds) ||
        !mc_world_runtime_binding_retain(w))
        return fail(c, MC_CLIENT_GUI_INGAME_DRAW_CHILD_FAILED, "HUD World occurrence", pc, true);
    c->worlds[c->world_count++] = w;
    return true;
}

static bool settings(mc_client_gui_ingame_draw_call *c, client_application *a,
                     mc_game_settings_state *out, unsigned int pc) {
    mc_game_settings *s = a->settings;
    if (s == NULL)
        return fail(c, MC_CLIENT_GUI_INGAME_DRAW_SOURCE_NULL, "actual UQ null Settings receiver",
                    pc, false);
    bool held = false;
    for (size_t i = 0; i < c->settings_count; ++i)
        held |= c->settings[i] == s;
    if (!held) {
        if (c->settings_count == sizeof(c->settings) / sizeof(*c->settings) ||
            mc_game_settings_borrow(s, &c->failure.settings) != MC_GAME_SETTINGS_OK)
            return fail(c, MC_CLIENT_GUI_INGAME_DRAW_CHILD_FAILED, "HUD Settings occurrence", pc,
                        true);
        c->settings[c->settings_count++] = s;
    }
    return mc_game_settings_read_state(s, out) ||
           fail(c, MC_CLIENT_GUI_INGAME_DRAW_CHILD_FAILED, "HUD actual Settings fields", pc, true);
}

static bool math_hold(mc_client_gui_ingame_draw_call *c, unsigned int pc) {
    if (!c->math_held) {
        if (mc_math_classes_retain(c->math, &c->failure.math) != MC_MATH_CLASS_OK)
            return fail(c, MC_CLIENT_GUI_INGAME_DRAW_CHILD_FAILED, "HUD Math occurrence", pc, true);
        c->math_held = true;
    }
    return true;
}

static bool math_use(mc_client_gui_ingame_draw_call *c, unsigned int pc) {
    return math_hold(c, pc) &&
           ((mc_math_classes_ensure(c->math, &c->failure.math) == MC_MATH_CLASS_OK) ||
            fail(c, MC_CLIENT_GUI_INGAME_DRAW_CHILD_FAILED, "HUD Math active use", pc, true)) &&
           fp(c, pc);
}

static mc_inventory_player *inventory(mc_client_gui_ingame_draw_call *c, unsigned int app_pc,
                                      unsigned int player_pc) {
    mc_entity *p = player(c, app_pc, player_pc);
    mc_local_player_snapshot s;
    if (p == NULL || !player_fields(c, p, &s, player_pc))
        return NULL;
    return s.player.c;
}

static bool inventory_state(mc_client_gui_ingame_draw_call *c, mc_inventory_player *inv,
                            mc_inventory_player_state *out, unsigned int pc) {
    if (inv == NULL)
        return fail(c, MC_CLIENT_GUI_INGAME_DRAW_SOURCE_NULL, "actual UQ null Inventory receiver",
                    pc, false);
    return mc_inventory_player_read_state(inv, out) ||
           fail(c, MC_CLIENT_GUI_INGAME_DRAW_CHILD_FAILED, "HUD actual Inventory fields", pc, true);
}

static bool texture(mc_client_gui_ingame_draw_call *c, const char *literal, unsigned int app_pc,
                    unsigned int invoke, unsigned int bind) {
    client_application *a = app(c, app_pc);
    if (a == NULL)
        return false;
    mc_client_render_engine *engine = a->render_engine;
    if (engine == NULL)
        return fail(c, MC_CLIENT_GUI_INGAME_DRAW_SOURCE_NULL, "actual HUD null Engine receiver",
                    invoke, false);
    if (c->engine_count == sizeof(c->engines) / sizeof(*c->engines) ||
        mc_client_render_engine_retain(engine) != MC_CLIENT_RENDER_ENGINE_OK)
        return fail(c, MC_CLIENT_GUI_INGAME_DRAW_CHILD_FAILED, "HUD Engine occurrence", invoke,
                    true);
    const size_t slot = c->engine_count++;
    c->engines[slot] = engine;
    uint16_t units[64];
    size_t length = strlen(literal);
    if (length > sizeof(units) / sizeof(*units))
        return fail(c, MC_CLIENT_GUI_INGAME_DRAW_DEPENDENCY, "HUD literal bound", invoke, true);
    for (size_t i = 0; i < length; ++i)
        units[i] = (unsigned char)literal[i];
    const mc_texture_pack_text key = {units, length, false};
    const bool foreign = mc_client_texture_lookup_pending(engine);
    mc_client_texture id;
    const mc_client_texture_lookup_status status =
        mc_client_render_engine_texture(engine, c->display, NULL, key, &id, &c->textures[slot]);
    c->lookup_owned[slot] = !foreign && mc_client_texture_lookup_pending(engine);
    if (status != MC_CLIENT_LOOKUP_OK) {
        c->failure.texture = &c->textures[slot];
        c->failure.child = c->textures[slot].site;
        return fail(c, MC_CLIENT_GUI_INGAME_DRAW_CHILD_FAILED, c->textures[slot].operation, invoke,
                    c->textures[slot].native_policy);
    }
    return gl(c, mc_client_display_source_bind_id(c->display, id, &c->failure.display), bind);
}

static bool nw_at(mc_client_gui_ingame_draw_call *c, unsigned int pc) {
    return graphical(
        c, mc_client_nw_active_use(c->graphics, site(pc), &c->nw, &c->failure.graphics), pc);
}

/* Actual ub.b(IIIIII), including Java int additions before conversion and
 * binary32 texture-coordinate products. No GUI bridge, color or state reset. */
static bool rect(mc_client_gui_ingame_draw_call *c, int32_t x, int32_t y, int32_t u, int32_t v,
                 int32_t width, int32_t height, unsigned int pc) {
    if (!nw_at(c, pc))
        return false;
    if (!mc_client_nw_begin_quads(c->nw, &c->failure.graphics))
        goto graphics_failed;
    for (size_t i = 0; i < 4; ++i) {
        if (!fp(c, pc))
            return false;
        const int32_t dx = mc_java_i32_add(x, i == 1 || i == 2 ? width : 0);
        const int32_t dy = mc_java_i32_add(y, i < 2 ? height : 0);
        const int32_t du = mc_java_i32_add(u, i == 1 || i == 2 ? width : 0);
        const int32_t dv = mc_java_i32_add(v, i < 2 ? height : 0);
        if (!mc_client_nw_vertex_uv(
                c->nw, (double)dx, (double)dy, at_widen(c->state.owner->fields.k),
                at_widen(at_fmul((float)du, 0.00390625f)),
                at_widen(at_fmul((float)dv, 0.00390625f)), &c->failure.graphics))
            goto graphics_failed;
        ++c->state.vertices;
    }
    if (!mc_client_nw_draw(c->nw, c->display, &c->failure.graphics))
        goto graphics_failed;
    ++c->state.textured_quads;
    c->state.source_pc = pc;
    return true;
graphics_failed:
    c->failure.child = c->failure.graphics.site;
    return fail(c, MC_CLIENT_GUI_INGAME_DRAW_CHILD_FAILED, c->failure.graphics.operation, pc,
                c->failure.graphics.native_policy);
}

/* Complete inherited sn.a(F), whose unused partial argument is still passed.
 * Area presence and the subsequent brightness use separately loaded World
 * receivers, preserving callbacks and the actual bE floor. */
static bool brightness(mc_client_gui_ingame_draw_call *c, mc_entity *p, float *out) {
    if (!math_use(c, 84))
        return false;
    const int32_t x = mc_math_floor_double(p->base.x);
    if (p->bounds == NULL)
        return child_fail(c, MC_CLIENT_GUI_INGAME_DRAW_SOURCE_NULL, "actual sn.a null bounds", 84,
                          "sn", "a", "(F)F", 12, false);
    const double delta = at_mul(at_sub(p->bounds->max_y, p->bounds->min_y), 0.66);
    const int32_t y =
        mc_math_floor_double(at_add(at_sub(p->base.y, at_widen(p->base.offset)), delta));
    const int32_t z = mc_math_floor_double(p->base.z);
    mc_world_runtime *w = p->base.world;
    if (!hold_world(c, w, 84))
        return false;
    const mc_entity_bounds *b = p->bounds;
    int32_t bounds[6] = {mc_math_floor_double(b->min_x), mc_math_floor_double(b->min_y),
                         mc_math_floor_double(b->min_z), mc_math_floor_double(b->max_x),
                         mc_math_floor_double(b->max_y), mc_math_floor_double(b->max_z)};
    if (w == NULL)
        return child_fail(c, MC_CLIENT_GUI_INGAME_DRAW_SOURCE_NULL, "actual sn.a null World", 84,
                          "sn", "a", "(F)F", 118, false);
    bool loaded;
    if (mc_world_runtime_area_loaded(w, bounds[0], bounds[1], bounds[2], bounds[3], bounds[4],
                                     bounds[5], &loaded, &c->failure.world) != MC_WORLD_STORAGE_OK)
        return child_fail(c, MC_CLIENT_GUI_INGAME_DRAW_CHILD_FAILED, "actual sn.a World area child",
                          84, "sn", "a", "(F)F", 118, true);
    float value = p->base.bE;
    if (loaded) {
        w = p->base.world;
        if (!hold_world(c, w, 84))
            return false;
        if (w == NULL)
            return child_fail(c, MC_CLIENT_GUI_INGAME_DRAW_SOURCE_NULL,
                              "actual sn.a brightness World", 84, "sn", "a", "(F)F", 133, false);
        if (mc_world_runtime_brightness(w, x, y, z, &value, &c->failure.brightness) !=
            MC_WORLD_STORAGE_OK)
            return child_fail(c, MC_CLIENT_GUI_INGAME_DRAW_CHILD_FAILED,
                              "actual sn.a brightness child", 84, "sn", "a", "(F)F", 133, true);
        if (value < p->base.bE)
            value = p->base.bE;
    }
    *out = value;
    return true;
}

static bool vignette(mc_client_gui_ingame_draw_call *c, float value) {
    if (!fp(c, 91))
        return false;
    value = at_fsub(1.0f, value);
    if (value < 0)
        value = 0;
    if (value > 1)
        value = 1;
    mc_client_gui_ingame_fields *f = &c->state.owner->fields;
    f->c = at_narrow(at_add(at_widen(f->c), at_mul(at_widen(at_fsub(value, f->c)), 0.01)));
    c->state.vignette = f->c;
    if (!gl(c, mc_client_display_source_depth_test(c->display, false, &c->failure.display), 91) ||
        !gl(c, mc_client_display_source_depth_mask(c->display, false, &c->failure.display), 91) ||
        !gl(c, mc_client_display_source_blend_function(c->display, 0, 769, &c->failure.display),
            91) ||
        !gl(c, mc_client_display_source_color(c->display, f->c, f->c, f->c, 1, &c->failure.display),
            91) ||
        !texture(c, "%blur%/misc/vignette.png", 91, 91, 91) || !nw_at(c, 91) ||
        !graphical(c, mc_client_nw_begin_quads(c->nw, &c->failure.graphics), 91))
        return false;
    for (size_t i = 0; i < 4; ++i) {
        if (!fp(c, 91))
            return false;
        const int32_t x = i == 1 || i == 2 ? c->state.resolution.width : 0;
        const int32_t y = i < 2 ? c->state.resolution.height : 0;
        if (!graphical(c,
                       mc_client_nw_vertex_uv(c->nw, (double)x, (double)y, -90,
                                              i == 1 || i == 2 ? 1 : 0, i < 2 ? 1 : 0,
                                              &c->failure.graphics),
                       91))
            return false;
        ++c->state.vertices;
    }
    if (!graphical(c, mc_client_nw_draw(c->nw, c->display, &c->failure.graphics), 91))
        return false;
    ++c->state.textured_quads;
    if (!gl(c, mc_client_display_source_depth_mask(c->display, true, &c->failure.display), 91) ||
        !gl(c, mc_client_display_source_depth_test(c->display, true, &c->failure.display), 91) ||
        !gl(c, mc_client_display_source_color(c->display, 1, 1, 1, 1, &c->failure.display), 91) ||
        !gl(c, mc_client_display_source_blend_function(c->display, 770, 771, &c->failure.display),
            91))
        return false;
    c->state.vignette_returned = true;
    return true;
}

static bool stack_item(mc_client_gui_ingame_draw_call *c, mc_item_stack *stack,
                       const mc_item **out) {
    client_application *a = c->state.owner->fields.g;
    const mc_registry_item_array *array;
    if (mc_registry_item_c_get(a->registry, &array, &c->failure.registry) != MC_REGISTRY_OK)
        return fail(c, MC_CLIENT_GUI_INGAME_DRAW_CHILD_FAILED, "actual Armor Item.c active use",
                    470, true);
    mc_item_stack_fields fields;
    if (mc_item_stack_fields_get(stack, &fields) != MC_ITEM_STACK_OK)
        return fail(c, MC_CLIENT_GUI_INGAME_DRAW_CHILD_FAILED, "actual Armor Stack fields", 470,
                    true);
    if (mc_registry_item_c_at(a->registry, array, fields.id, out, &c->failure.registry) !=
        MC_REGISTRY_OK)
        return fail(c, MC_CLIENT_GUI_INGAME_DRAW_CHILD_FAILED, "actual Armor Item.c element", 470,
                    true);
    return true;
}

static bool armor(mc_client_gui_ingame_draw_call *c, mc_inventory_player *inv) {
    mc_inventory_player_state s;
    if (!inventory_state(c, inv, &s, 470))
        return false;
    if (s.armor_slots == NULL)
        return child_fail(c, MC_CLIENT_GUI_INGAME_DRAW_SOURCE_NULL, "actual ix.f null armor array",
                          470, "ix", "f", "()I", 9, false);
    int32_t defense = 0, remaining = 0, maximum = 0;
    for (size_t i = 0; i < 4; ++i) {
        if (!inventory_state(c, inv, &s, 470))
            return false;
        mc_item_stack *stack = s.armor_slots[i];
        if (stack == NULL)
            continue;
        const mc_item *item;
        if (!stack_item(c, stack, &item))
            return false;
        if (item == NULL || item->kind != MC_ITEM_ARMOR)
            continue;
        if (!stack_item(c, s.armor_slots[i], &item))
            return false;
        if (item == NULL)
            return fail(c, MC_CLIENT_GUI_INGAME_DRAW_SOURCE_NULL,
                        "actual Armor durability Item receiver", 470, false);
        const int32_t max = item->fields.bh;
        mc_item_stack_fields fields;
        if (mc_item_stack_fields_get(s.armor_slots[i], &fields) != MC_ITEM_STACK_OK)
            return fail(c, MC_CLIENT_GUI_INGAME_DRAW_CHILD_FAILED, "actual Armor damage fields",
                        470, true);
        remaining = mc_java_i32_add(remaining, mc_java_i32_sub(max, fields.damage));
        maximum = mc_java_i32_add(maximum, max);
        if (!stack_item(c, s.armor_slots[i], &item))
            return false;
        if (item == NULL || item->kind != MC_ITEM_ARMOR)
            return fail(c, MC_CLIENT_GUI_INGAME_DRAW_DEPENDENCY, "actual Armor checkcast WA", 470,
                        true);
        defense = mc_java_i32_add(defense, item->subtype.wa.bl);
    }
    c->state.armor = 0;
    if (maximum != 0) {
        int32_t quotient;
        if (!mc_java_i32_div(mc_java_i32_mul(mc_java_i32_sub(defense, 1), remaining), maximum,
                             &quotient))
            return fail(c, MC_CLIENT_GUI_INGAME_DRAW_CHILD_FAILED, "actual ix.f armor division",
                        470, false);
        c->state.armor = mc_java_i32_add(quotient, 1);
    }
    return true;
}

static bool stats(mc_client_gui_ingame_draw_call *c) {
    mc_entity *p = player(c, 372, 375);
    if (p == NULL)
        return false;
    c->state.blinking = p->base.by / 3 % 2 == 1;
    p = player(c, 397, 400);
    if (p == NULL)
        return false;
    if (p->base.by < 10)
        c->state.blinking = false;
    p = player(c, 415, 418);
    mc_local_player_snapshot snapshot;
    if (p == NULL || !player_fields(c, p, &snapshot, 418))
        return false;
    c->state.health = snapshot.living.Y;
    p = player(c, 427, 430);
    if (p == NULL || !player_fields(c, p, &snapshot, 430))
        return false;
    c->state.previous_health = snapshot.living.Z;
    const int32_t seed = mc_java_i32_mul(c->state.owner->fields.h, 312871);
    if (c->state.owner->fields.f == NULL)
        return fail(c, MC_CLIENT_GUI_INGAME_DRAW_SOURCE_NULL, "actual UQ Random.setSeed receiver",
                    447, false);
    mc_java_random_reseed(c->state.owner->fields.f, seed);
    c->state.seed = seed;
    c->state.random_seeded = true;
    client_application *a = app(c, 454);
    if (a == NULL)
        return false;
    if (a->controller == NULL)
        return fail(c, MC_CLIENT_GUI_INGAME_DRAW_SOURCE_NULL,
                    "actual ob.d null controller receiver", 457, false);
    if (mc_client_singleplayer_controller_retain(a->controller) != MC_SINGLEPLAYER_CONTROLLER_OK)
        return fail(c, MC_CLIENT_GUI_INGAME_DRAW_CHILD_FAILED, "HUD actual Controller occurrence",
                    457, true);
    c->controller = a->controller;
    c->state.stats_enabled = true; /* Actual inherited ob.d() is literal iconst_1/ireturn. */
    mc_inventory_player *inv = inventory(c, 467, 470);
    if (c->failure.status != MC_CLIENT_GUI_INGAME_DRAW_OK || !armor(c, inv))
        return false;
    const int32_t center = c->state.resolution.width / 2;
    for (int32_t i = 0; i < 10; ++i) {
        int32_t y = mc_java_i32_sub(c->state.resolution.height, 32);
        const int32_t point = i * 2 + 1;
        if (c->state.armor > 0) {
            const int32_t x =
                mc_java_i32_sub(mc_java_i32_sub(mc_java_i32_add(center, 91), i * 8), 9);
            const int32_t u = point < c->state.armor ? 34 : point == c->state.armor ? 25 : 16;
            if (!rect(c, x, y, u, 9, 9, 9,
                      point < c->state.armor    ? 539u
                      : point == c->state.armor ? 566u
                                                : 593u))
                return false;
            ++c->state.armor_icons;
        }
        const int32_t x = mc_java_i32_add(mc_java_i32_sub(center, 91), i * 8);
        if (c->state.health <= 4) {
            int32_t jitter;
            if (c->state.owner->fields.f == NULL)
                return fail(c, MC_CLIENT_GUI_INGAME_DRAW_SOURCE_NULL,
                            "actual HUD Random.nextInt receiver", 635, false);
            (void)mc_java_random_next_bounded(c->state.owner->fields.f, 2, &jitter);
            y = mc_java_i32_add(y, jitter);
        }
        if (!rect(c, x, y, 16 + (c->state.blinking ? 9 : 0), 0, 9, 9, 659))
            return false;
        ++c->state.heart_backgrounds;
        if (c->state.blinking && point <= c->state.previous_health) {
            if (!rect(c, x, y, point < c->state.previous_health ? 70 : 79, 0, 9, 9,
                      point < c->state.previous_health ? 690u : 716u))
                return false;
            ++c->state.heart_foregrounds;
        }
        if (point <= c->state.health) {
            if (!rect(c, x, y, point < c->state.health ? 52 : 61, 0, 9, 9,
                      point < c->state.health ? 742u : 768u))
                return false;
            ++c->state.heart_foregrounds;
        }
    }
    a = app(c, 781);
    if (a == NULL)
        return false;
    p = a->h; /* Material active use precedes a null virtual receiver. */
    if (!hold_player(c, p, 781))
        return false;
    mc_material *water = NULL;
    if (mc_registry_material_get(a->registry, MC_MATERIAL_g, &water, &c->failure.registry) !=
        MC_REGISTRY_OK)
        return fail(c, MC_CLIENT_GUI_INGAME_DRAW_CHILD_FAILED, "actual HUD LN.g active use", 784,
                    true);
    if (p == NULL)
        return fail(c, MC_CLIENT_GUI_INGAME_DRAW_SOURCE_NULL, "actual HUD immersion receiver", 787,
                    false);
    if (mc_entity_inside_material(p, water, &c->state.water, &c->failure.material) != MC_ENTITY_OK)
        return fail(c, MC_CLIENT_GUI_INGAME_DRAW_CHILD_FAILED, "actual HUD material immersion", 787,
                    true);
    if (c->state.water)
        return fail(c, MC_CLIENT_GUI_INGAME_DRAW_DEPENDENCY,
                    "actual HUD air-bubble arithmetic and draws", 814, true);
    c->state.stats_returned = true;
    return true;
}

static bool body(mc_client_gui_ingame_draw_call *c) {
    client_application *a = app(c, 8);
    if (a == NULL)
        return false;
    mc_game_settings *captured = a->settings;
    a = app(c, 15);
    if (a == NULL)
        return false;
    const int32_t physical_width = a->d;
    a = app(c, 22);
    if (a == NULL)
        return false;
    const int32_t physical_height = a->e;
    mc_game_settings_state s;
    if (captured == NULL)
        return child_fail(c, MC_CLIENT_GUI_INGAME_DRAW_SOURCE_NULL,
                          "actual QQ null Settings receiver", 25, "qq", "<init>", "(Lkv;II)V", 20,
                          false);
    /* The captured QQ argument is distinct from subsequent fresh g.z reads. */
    if (mc_game_settings_borrow(captured, &c->failure.settings) != MC_GAME_SETTINGS_OK)
        return fail(c, MC_CLIENT_GUI_INGAME_DRAW_CHILD_FAILED, "HUD QQ Settings occurrence", 25,
                    true);
    c->settings[c->settings_count++] = captured;
    if (!mc_game_settings_read_state(captured, &s))
        return fail(c, MC_CLIENT_GUI_INGAME_DRAW_CHILD_FAILED, "HUD captured QQ Settings fields",
                    25, true);
    if (!fp(c, 25))
        return false;
    if (!mc_scaled_resolution_init(physical_width, physical_height, s.values.gui_scale,
                                   &c->state.resolution))
        return fail(c, MC_CLIENT_GUI_INGAME_DRAW_DEPENDENCY, "actual HUD QQ native dimensions/FP",
                    25, true);
    a = app(c, 48);
    if (a == NULL)
        return false;
    if (a->font_renderer != NULL) {
        if (mc_client_bitmap_font_retain(a->font_renderer) != MC_CLIENT_BITMAP_FONT_OK)
            return fail(c, MC_CLIENT_GUI_INGAME_DRAW_CHILD_FAILED, "HUD captured Font occurrence",
                        48, true);
        c->font = a->font_renderer;
    }
    a = app(c, 57);
    if (a == NULL)
        return false;
    if (a->t == NULL)
        return fail(c, MC_CLIENT_GUI_INGAME_DRAW_SOURCE_NULL, "actual HUD PX.b null receiver", 60,
                    false);
    if (!entity_renderer_overlay_setup(a->t, &c->projection, &c->projection_error)) {
        c->failure.projection = &c->projection_error;
        c->failure.child = c->projection_error.site;
        return fail(c, MC_CLIENT_GUI_INGAME_DRAW_CHILD_FAILED, c->projection_error.operation, 60,
                    c->projection_error.native_policy);
    }
    c->state.projection_returned = true;
    if (!gl(c, mc_client_display_source_blend_enable(c->display, true, &c->failure.display), 66))
        return false;
    a = c->state.owner->fields.g;
    if (a->source_namespace == NULL)
        return fail(c, MC_CLIENT_GUI_INGAME_DRAW_DEPENDENCY, "actual Minecraft.u class namespace",
                    69, true);
    if (mc_client_application_classes_active_use(a->source_namespace, &c->application_error) !=
        MC_CLIENT_APPLICATION_OWNER_OK) {
        c->failure.application = &c->application_error;
        return fail(c, MC_CLIENT_GUI_INGAME_DRAW_CHILD_FAILED,
                    "actual Minecraft.u class active use", 69, true);
    }
    mc_client_application_class_state globals;
    if (!mc_client_application_classes_inspect(a->source_namespace, &globals))
        return fail(c, MC_CLIENT_GUI_INGAME_DRAW_CHILD_FAILED, "actual Minecraft.u static a read",
                    69, true);
    c->state.fancy = false;
    if (globals.a != NULL) {
        if (!settings(c, globals.a, &s, 69))
            return false;
        c->state.fancy = s.values.fancy_graphics;
    }
    if (c->state.fancy) {
        mc_entity *p = player(c, 80, 84);
        if (p == NULL || !brightness(c, p, &c->state.brightness) ||
            !vignette(c, c->state.brightness))
            return false;
    }
    mc_inventory_player *inv = inventory(c, 98, 101);
    mc_inventory_player_state inventory_fields;
    if (c->failure.status != MC_CLIENT_GUI_INGAME_DRAW_OK ||
        !inventory_state(c, inv, &inventory_fields, 105))
        return false;
    if (inventory_fields.armor_slots == NULL)
        return fail(c, MC_CLIENT_GUI_INGAME_DRAW_SOURCE_NULL, "actual ix.d null armor array", 105,
                    false);
    const mc_item_stack *helmet = inventory_fields.armor_slots[3];
    a = app(c, 114);
    if (a == NULL || !settings(c, a, &s, 117))
        return false;
    if (!s.values.third_person_view && helmet != NULL) {
        mc_item_stack_fields helmet_fields;
        if (mc_item_stack_fields_get(helmet, &helmet_fields) != MC_ITEM_STACK_OK)
            return fail(c, MC_CLIENT_GUI_INGAME_DRAW_CHILD_FAILED, "actual helmet Stack.c", 130,
                        true);
        const mc_block *pumpkin;
        if (mc_registry_block_static_get(a->registry, "bb", &pumpkin, &c->failure.registry) !=
            MC_REGISTRY_OK)
            return fail(c, MC_CLIENT_GUI_INGAME_DRAW_CHILD_FAILED, "actual Block.bb active use",
                        133, true);
        const mc_block_fields *block_fields = mc_block_fields_get(pumpkin);
        if (block_fields == NULL)
            return fail(c, MC_CLIENT_GUI_INGAME_DRAW_SOURCE_NULL,
                        "actual Block.bb null bn receiver", 136, false);
        if (helmet_fields.id == block_fields->bn)
            return fail(c, MC_CLIENT_GUI_INGAME_DRAW_DEPENDENCY, "actual pumpkin overlay", 147,
                        true);
    }
    mc_entity *p = player(c, 154, 157);
    mc_local_player_snapshot fields;
    if (p == NULL || !player_fields(c, p, &fields, 157))
        return false;
    const float previous = fields.player.C;
    p = player(c, 164, 167);
    if (p == NULL || !player_fields(c, p, &fields, 167))
        return false;
    const float current = fields.player.B;
    p = player(c, 174, 177);
    if (p == NULL || !player_fields(c, p, &fields, 177) || !fp(c, 180))
        return false;
    const float portal =
        at_fadd(previous, at_fmul(at_fsub(current, fields.player.C), c->state.partial));
    if (portal > 0)
        return fail(c, MC_CLIENT_GUI_INGAME_DRAW_DEPENDENCY, "actual portal overlay", 200, true);
    if (!gl(c, mc_client_display_source_color(c->display, 1, 1, 1, 1, &c->failure.display), 207) ||
        !texture(c, "/gui/gui.png", 217, 222, 225))
        return false;
    inv = inventory(c, 232, 235);
    if (c->failure.status != MC_CLIENT_GUI_INGAME_DRAW_OK)
        return false;
    c->state.owner->fields.k = -90.0f;
    const int32_t center = c->state.resolution.width / 2;
    const int32_t bottom = mc_java_i32_sub(c->state.resolution.height, 22);
    if (!rect(c, mc_java_i32_sub(center, 91), bottom, 0, 0, 182, 22, 266))
        return false;
    if (!inventory_state(c, inv, &inventory_fields, 281))
        return false;
    c->state.selected = inventory_fields.selected;
    const int32_t selected_x = mc_java_i32_add(mc_java_i32_sub(mc_java_i32_sub(center, 91), 1),
                                               mc_java_i32_mul(c->state.selected, 20));
    if (!rect(c, selected_x, mc_java_i32_sub(bottom, 1), 0, 22, 24, 22, 302))
        return false;
    c->state.hotbar_returned = true;
    if (!texture(c, "/gui/icons.png", 312, 317, 320) ||
        !gl(c, mc_client_display_source_blend_enable(c->display, true, &c->failure.display), 326) ||
        !gl(c, mc_client_display_source_blend_function(c->display, 775, 769, &c->failure.display),
            335) ||
        !rect(c, mc_java_i32_sub(center, 7), mc_java_i32_sub(c->state.resolution.height / 2, 7), 0,
              0, 16, 16, 359) ||
        !gl(c, mc_client_display_source_blend_enable(c->display, false, &c->failure.display), 365))
        return false;
    c->state.crosshair_returned = true;
    if (!stats(c) ||
        !gl(c, mc_client_display_source_blend_enable(c->display, false, &c->failure.display),
            946) ||
        !gl(c, mc_client_display_source_rescale_normal(c->display, true, &c->failure.display),
            951) ||
        !gl(c, mc_client_display_source_push(c->display, &c->failure.display), 954) ||
        !gl(c, mc_client_display_source_rotate(c->display, 120, 1, 0, 0, &c->failure.display),
            962) ||
        !math_hold(c, 965))
        return false;
    if (!mc_client_lighting_enable(c->graphics, c->state.owner->classes->models->entities, c->math,
                                   c->display, site(965), &c->lighting)) {
        c->failure.lighting = &c->lighting;
        c->failure.child = c->lighting.site;
        return fail(c, MC_CLIENT_GUI_INGAME_DRAW_CHILD_FAILED, c->lighting.operation, 965,
                    c->lighting.native_policy);
    }
    if (!gl(c, mc_client_display_source_pop(c->display, &c->failure.display), 968))
        return false;
    for (int32_t i = 0; i < 9; ++i) {
        inv = inventory(c, 1015, 1015);
        if (c->failure.status != MC_CLIENT_GUI_INGAME_DRAW_OK ||
            !inventory_state(c, inv, &inventory_fields, 1015))
            return false;
        if (inventory_fields.main_slots == NULL)
            return child_fail(c, MC_CLIENT_GUI_INGAME_DRAW_SOURCE_NULL,
                              "actual UQ slot null main array", 1015, "uq", "a", "(IIIF)V", 14,
                              false);
        if (inventory_fields.main_slots[i] != NULL)
            return child_fail(c, MC_CLIENT_GUI_INGAME_DRAW_DEPENDENCY,
                              "actual nonempty HUD item rendering", 1015, "uq", "a", "(IIIF)V", 23,
                              true);
        ++c->state.empty_slots;
    }
    c->state.slots_returned = true;
    if (!mc_client_lighting_disable(c->graphics, c->display, site(1024), &c->failure.graphics))
        return graphical(c, false, 1024);
    if (!gl(c, mc_client_display_source_rescale_normal(c->display, false, &c->failure.display),
            1029))
        return false;
    p = player(c, 1036, 1039);
    if (p == NULL || !player_fields(c, p, &fields, 1039))
        return false;
    if (fields.player.private_a > 0)
        return fail(c, MC_CLIENT_GUI_INGAME_DRAW_DEPENDENCY, "actual sleep overlay", 1048, true);
    a = app(c, 1139);
    if (a == NULL || !settings(c, a, &s, 1142))
        return false;
    if (s.values.show_debug_info)
        return fail(c, MC_CLIENT_GUI_INGAME_DRAW_DEPENDENCY, "actual debug text rendering", 1148,
                    true);
    if (c->state.owner->fields.j > 0)
        return fail(c, MC_CLIENT_GUI_INGAME_DRAW_DEPENDENCY, "actual record notice rendering", 1661,
                    true);
    a = app(c, 1805);
    if (a == NULL)
        return false;
    /* Every concrete native da subtype currently admitted is disjoint from gc;
     * null, base, menu, world selection and pause all take source false here. */
    if (!gl(c, mc_client_display_source_blend_enable(c->display, true, &c->failure.display),
            1824) ||
        !gl(c, mc_client_display_source_blend_function(c->display, 770, 771, &c->failure.display),
            1833) ||
        !gl(c, mc_client_display_source_alpha_test(c->display, false, &c->failure.display), 1839) ||
        !gl(c, mc_client_display_source_push(c->display, &c->failure.display), 1842) ||
        !fp(c, 1853) ||
        !gl(c,
            mc_client_display_source_translate(
                c->display, 0, (float)mc_java_i32_sub(c->state.resolution.height, 48), 0,
                &c->failure.display),
            1853))
        return false;
    mc_client_gui_ingame_list *list = c->state.owner->fields.e;
    if (list == NULL)
        return fail(c, MC_CLIENT_GUI_INGAME_DRAW_SOURCE_NULL, "actual UQ chat List.size receiver",
                    1865, false);
    if (list->size != 0)
        return fail(c, MC_CLIENT_GUI_INGAME_DRAW_DEPENDENCY, "actual nonempty chat rendering", 1886,
                    true);
    if (!gl(c, mc_client_display_source_pop(c->display, &c->failure.display), 2086) ||
        !gl(c, mc_client_display_source_alpha_test(c->display, true, &c->failure.display), 2092) ||
        !gl(c, mc_client_display_source_blend_enable(c->display, false, &c->failure.display), 2098))
        return false;
    c->state.chat_returned = c->state.returned = true;
    c->state.source_pc = 2101;
    return true;
}

mc_client_gui_ingame_draw_status mc_client_gui_ingame_draw_begin(
    mc_client_source_uq *owner, mc_client_render_engine_classes *graphics, mc_math_classes *math,
    mc_client_display *display, float partial, bool flag, int32_t mouse_x, int32_t mouse_y,
    mc_client_gui_ingame_draw_call **out, mc_client_gui_ingame_draw_error *error) {
    if (owner == NULL || owner->classes == NULL || graphics == NULL || out == NULL || *out != NULL)
        return admission(error, MC_CLIENT_GUI_INGAME_DRAW_ARGUMENT, "HUD native owner/output");
    if (owner->draw_call != NULL)
        return admission(error, MC_CLIENT_GUI_INGAME_DRAW_BUSY, "HUD retained source call");
    mc_client_gui_ingame_draw_call *c = calloc(1, sizeof(*c));
    if (c == NULL)
        return admission(error, MC_CLIENT_GUI_INGAME_DRAW_ALLOCATION, "HUD retained call storage");
    admission(&c->failure, MC_CLIENT_GUI_INGAME_DRAW_OK, NULL);
    c->state.owner = owner;
    c->state.partial = partial;
    c->state.mouse_x = mouse_x;
    c->state.mouse_y = mouse_y;
    c->state.failure = &c->failure;
    c->state.cleanup = &c->cleanup;
    c->graphics = graphics;
    c->math = math;
    c->display = display;
    c->flag = flag;
    const int locked = mtx_trylock(&owner->classes->mutex);
    if (locked != thrd_success) {
        free(c);
        return admission(error, MC_CLIENT_GUI_INGAME_DRAW_BUSY, "HUD native owner monitor");
    }
    if (owner->draw_call != NULL || owner->classes->retiring ||
        owner->phase == MC_CLIENT_GUI_INGAME_RETIRING ||
        owner->phase == MC_CLIENT_GUI_INGAME_CONSTRUCTING || owner->holds == SIZE_MAX) {
        (void)mtx_unlock(&owner->classes->mutex);
        free(c);
        return admission(error, MC_CLIENT_GUI_INGAME_DRAW_BUSY, "HUD source owner occurrence");
    }
    if (mc_client_render_engine_classes_retain(graphics) != MC_CLIENT_RENDER_ENGINE_OK) {
        (void)mtx_unlock(&owner->classes->mutex);
        free(c);
        return admission(error, MC_CLIENT_GUI_INGAME_DRAW_BUSY, "HUD graphics occurrence");
    }
    c->graphics_held = true;
    ++owner->holds; /* Same native occurrence, while the actual owner monitor is held. */
    c->owner_held = true;
    owner->draw_call = c;
    c->state.active = true;
    *out = c;
    (void)body(c);
    c->state.active = false;
    (void)mtx_unlock(&owner->classes->mutex);
    if (error != NULL)
        *error = c->failure;
    return c->failure.status;
}

bool mc_client_gui_ingame_draw_inspect(const mc_client_gui_ingame_draw_call *c,
                                       mc_client_gui_ingame_draw_state *out) {
    if (c == NULL || out == NULL)
        return false;
    *out = c->state;
    return true;
}

static mc_client_gui_ingame_draw_status cleanup_failed(mc_client_gui_ingame_draw_call *c,
                                                       mc_client_gui_ingame_draw_error *out,
                                                       const char *operation) {
    c->cleanup.status = MC_CLIENT_GUI_INGAME_DRAW_BUSY;
    c->cleanup.operation = operation;
    c->cleanup.native_policy = true;
    if (out != NULL)
        *out = c->cleanup;
    return c->cleanup.status;
}

mc_client_gui_ingame_draw_status
mc_client_gui_ingame_draw_retire(mc_client_gui_ingame_draw_call **slot,
                                 mc_client_gui_ingame_draw_error *error) {
    if (slot == NULL)
        return admission(error, MC_CLIENT_GUI_INGAME_DRAW_ARGUMENT, "HUD cleanup slot");
    mc_client_gui_ingame_draw_call *c = *slot;
    if (c == NULL)
        return admission(error, MC_CLIENT_GUI_INGAME_DRAW_OK, NULL);
    if (c->state.active)
        return admission(error, MC_CLIENT_GUI_INGAME_DRAW_BUSY, "HUD active cleanup");
    for (size_t i = c->engine_count; i != 0; --i) {
        const size_t index = i - 1;
        if (c->lookup_owned[index]) {
            (void)mc_client_render_engine_retire_lookup(c->engines[index], &c->texture_cleanup);
            c->lookup_owned[index] = mc_client_texture_lookup_pending(c->engines[index]);
            if (c->lookup_owned[index]) {
                c->cleanup.texture = &c->texture_cleanup;
                return cleanup_failed(c, error, "HUD owned texture lookup cleanup");
            }
        }
        if (c->engines[index] != NULL) {
            if (mc_client_render_engine_release(c->engines[index]) != MC_CLIENT_RENDER_ENGINE_OK)
                return cleanup_failed(c, error, "HUD Engine occurrence cleanup");
            c->engines[index] = NULL;
        }
    }
    if (c->font != NULL &&
        mc_client_bitmap_font_release(&c->font, &c->font_error) != MC_CLIENT_BITMAP_FONT_OK)
        return cleanup_failed(c, error, "HUD Font occurrence cleanup");
    if (c->controller != NULL) {
        if (mc_client_singleplayer_controller_release(c->controller) !=
            MC_SINGLEPLAYER_CONTROLLER_OK)
            return cleanup_failed(c, error, "HUD Controller occurrence cleanup");
        c->controller = NULL;
    }
    while (c->settings_count != 0) {
        if (mc_game_settings_release_borrow(&c->settings[c->settings_count - 1],
                                            &c->cleanup.settings) != MC_GAME_SETTINGS_OK)
            return cleanup_failed(c, error, "HUD Settings occurrence cleanup");
        --c->settings_count;
    }
    if (c->math_held) {
        if (mc_math_classes_release(&c->math, &c->cleanup.math) != MC_MATH_CLASS_OK)
            return cleanup_failed(c, error, "HUD Math occurrence cleanup");
        c->math_held = false;
    }
    if (c->graphics_held) {
        if (mc_client_render_engine_classes_release(c->graphics) != MC_CLIENT_RENDER_ENGINE_OK)
            return cleanup_failed(c, error, "HUD graphics occurrence cleanup");
        c->graphics_held = false;
    }
    while (c->world_count != 0)
        mc_world_runtime_binding_release(c->worlds[--c->world_count]);
    while (c->player_count != 0)
        mc_entity_release(c->players[--c->player_count]);
    mc_client_source_uq *owner = c->state.owner;
    owner->draw_call = NULL;
    if (mc_client_gui_ingame_release(&c->state.owner, &c->cleanup.owner) !=
        MC_CLIENT_GUI_INGAME_OK) {
        owner->draw_call = c;
        return cleanup_failed(c, error, "HUD source owner occurrence cleanup");
    }
    free(c);
    *slot = NULL;
    return admission(error, MC_CLIENT_GUI_INGAME_DRAW_OK, NULL);
}
