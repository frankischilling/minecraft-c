#include "source_inventory_drawing_internal.h"

#include <stdlib.h>
#include <string.h>

bool mc_inventory_draw_fail(mc_client_inventory_draw_call *c, mc_client_screen_status status,
                            const char *operation, const char *owner, const char *method,
                            const char *descriptor, unsigned int pc, bool native) {
    c->state.source_pc = pc;
    mc_screen_fail(&c->error.screen, status, operation, owner, method, descriptor, pc);
    c->error.screen.native_policy = native;
    return false;
}

const char *mc_inventory_draw_descriptor(const mc_client_inventory_draw_call *c,
                                         const char *owner) {
    if (strcmp(owner, "ue") == 0 || strcmp(owner, "oo") == 0)
        return "(F)V";
    if (strcmp(owner, "ub") == 0)
        return "(IIIIII)V";
    return c->id_descriptor == NULL ? "(IIF)V" : c->id_descriptor;
}

bool mc_inventory_draw_gl(mc_client_inventory_draw_call *c, bool ok, const char *owner,
                          unsigned int pc) {
    if (ok)
        return true;
    return mc_inventory_draw_fail(c, MC_CLIENT_SCREEN_TEXTURE_FAILED, c->error.display.operation,
                                  owner, "a", mc_inventory_draw_descriptor(c, owner), pc, true);
}

bool mc_inventory_draw_fp(mc_client_inventory_draw_call *c, const char *owner, unsigned int pc) {
    if ((_mm_getcsr() & 0xffc0u) == 0x1f80u)
        return true;
    return mc_inventory_draw_fail(c, MC_CLIENT_SCREEN_UNIMPLEMENTED_DEPENDENCY,
                                  "Inventory strict native floating environment", owner, "a",
                                  mc_inventory_draw_descriptor(c, owner), pc, true);
}

bool mc_inventory_draw_player_capture(mc_client_inventory_draw_call *c, const char *owner,
                                      unsigned int pc, mc_entity **out) {
    const char *desc = strcmp(owner, "ue") == 0 ? "(F)V" : "(IIF)V";
    client_application *app = c->owner->base.fields.b;
    if (app == NULL) {
        mc_inventory_draw_fail(c, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE,
                               "Inventory actual App null field receiver", owner, "a", desc, pc,
                               false);
        return false;
    }
    mc_entity *p = app->h;
    if (p == NULL) {
        *out = NULL;
        return true;
    }
    if (c->player_count == sizeof(c->players) / sizeof(*c->players) ||
        !mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER) || p->local_player == NULL) {
        mc_inventory_draw_fail(c, MC_CLIENT_SCREEN_UNIMPLEMENTED_DEPENDENCY,
                               "Inventory native represented Player storage bound", owner, "a",
                               desc, pc, true);
        return false;
    }
    c->error.entity = mc_entity_retain(p);
    if (c->error.entity != MC_ENTITY_OK) {
        mc_inventory_draw_fail(c, MC_CLIENT_SCREEN_BUSY, "Inventory captured Player hold", owner,
                               "a", desc, pc, true);
        return false;
    }
    c->players[c->player_count++] = p;
    *out = p;
    return true;
}

mc_entity *mc_inventory_draw_player(mc_client_inventory_draw_call *c, const char *owner,
                                    unsigned int pc) {
    mc_entity *p = NULL;
    if (!mc_inventory_draw_player_capture(c, owner, pc, &p))
        return NULL;
    if (p == NULL)
        mc_inventory_draw_fail(c, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE,
                               "Inventory actual Player null field receiver", owner, "a",
                               strcmp(owner, "ue") == 0 ? "(F)V" : "(IIF)V", pc, false);
    return p;
}

static bool slot_error(mc_client_inventory_draw_call *c, mc_inventory_status status,
                       unsigned int pc, const char *descriptor) {
    if (status == MC_INVENTORY_OK)
        return true;
    c->error.screen.parent_site = (mc_client_screen_site){"id", "a", descriptor, pc};
    const mc_inventory_site s = c->error.slot.site;
    if (s.class_name == NULL)
        return mc_inventory_draw_fail(
            c,
            c->error.slot.source_exception ? MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE
                                           : MC_CLIENT_SCREEN_APP_FAILED,
            c->error.slot.operation, "id", "a", descriptor, pc, c->error.slot.native_policy);
    return mc_inventory_draw_fail(c, MC_CLIENT_SCREEN_APP_FAILED, c->error.slot.operation,
                                  s.class_name, s.method, s.descriptor, (unsigned int)s.pc,
                                  c->error.slot.native_policy);
}

static bool hold_stack(mc_client_inventory_draw_call *c, const mc_item_stack *s, unsigned int pc) {
    if (s == NULL)
        return true;
    if (c->stack_count == sizeof(c->stacks) / sizeof(*c->stacks))
        return mc_inventory_draw_fail(c, MC_CLIENT_SCREEN_LIMIT, "Inventory Stack occurrence bound",
                                      "id", "a", "(IIF)V", pc, true);
    c->error.stack = mc_item_stack_retain((mc_item_stack *)s);
    if (c->error.stack != MC_ITEM_STACK_OK)
        return mc_inventory_draw_fail(c, MC_CLIENT_SCREEN_BUSY, "Inventory captured Stack hold",
                                      "id", "a", "(IIF)V", pc, true);
    c->stacks[c->stack_count++] = (mc_item_stack *)s;
    return true;
}

typedef struct item_arguments {
    mc_client_item_renderer *renderer;
    mc_client_bitmap_font *font;
    mc_client_render_engine *engine;
} item_arguments;

static bool capture_item_arguments(mc_client_inventory_draw_call *c, unsigned int app_pc,
                                   item_arguments *out) {
    out->renderer = mc_client_screen_inventory_item_renderer(c->owner->base.classes);
    out->font = c->owner->base.fields.g;
    client_application *app = c->owner->base.fields.b;
    if (app == NULL)
        return mc_inventory_draw_fail(c, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE,
                                      "ID actual JI argument App null receiver", "id", "a",
                                      c->id_descriptor, app_pc, false);
    out->engine = app->render_engine;
    return true;
}

static bool item(mc_client_inventory_draw_call *c, const item_arguments *arguments,
                 const mc_item_stack *stack, int32_t x, int32_t y, unsigned int pc) {
    mc_client_source_id *o = c->owner;
    if (arguments->renderer == NULL)
        return mc_inventory_draw_fail(c, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE,
                                      "ID actual BB GUI null receiver", "id", "a", c->id_descriptor,
                                      pc, false);
    if (mc_client_item_gui_begin(arguments->renderer, c->services.graphics,
                                 o->base.classes->registry, c->services.display, arguments->font,
                                 arguments->engine, (mc_item_stack *)stack, x, y, &c->item,
                                 &c->error.item) != MC_CLIENT_ITEM_GUI_OK)
        return mc_inventory_draw_fail(c, MC_CLIENT_SCREEN_TEXTURE_FAILED, c->error.item.operation,
                                      "id", "a", c->id_descriptor, pc, c->error.item.native_policy);
    ++c->state.item_draws;
    /* One actual BB workspace at a time. Successful children have returned;
     * copied counters remain while failed children retain their typed views. */
    if (mc_client_item_gui_retire(&c->item, &c->cleanup.item) != MC_CLIENT_ITEM_GUI_OK)
        return mc_inventory_draw_fail(c, MC_CLIENT_SCREEN_BUSY,
                                      "Inventory completed BB native retirement", "id", "a",
                                      "(IIF)V", pc, true);
    c->error.item.inventory = NULL;
    return true;
}

static bool overlay(mc_client_inventory_draw_call *c, const item_arguments *arguments,
                    const mc_item_stack *stack, int32_t x, int32_t y, unsigned int pc) {
    mc_client_source_id *o = c->owner;
    if (arguments->renderer == NULL)
        return mc_inventory_draw_fail(c, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE,
                                      "ID actual BB overlay null receiver", "id", "a",
                                      c->id_descriptor, pc, false);
    c->error.overlay = (mc_client_item_overlay_context){.owner = arguments->renderer,
                                                        .registry = o->base.classes->registry,
                                                        .graphics = c->services.graphics,
                                                        .display = c->services.display,
                                                        .font = arguments->font,
                                                        .stack = (mc_item_stack *)stack,
                                                        .x = x,
                                                        .y = y};
    if (!mc_client_item_overlay_draw(&c->error.overlay))
        return mc_inventory_draw_fail(c, MC_CLIENT_SCREEN_TEXTURE_FAILED,
                                      c->error.overlay.operation, "id", "a", c->id_descriptor, pc,
                                      c->error.overlay.native_policy);
    ++c->state.overlay_draws;
    return true;
}

static bool draw_slot(mc_client_inventory_draw_call *c, mc_inventory_slot *slot) {
    c->id_descriptor = "(Lgp;)V";
    if (slot == NULL)
        return mc_inventory_draw_fail(c, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE,
                                      "ID actual Slot.x null receiver", "id", "a", "(Lgp;)V", 1,
                                      false);
    mc_container_slot_state state;
    if (!mc_inventory_slot_source_state(slot, &state))
        return mc_inventory_draw_fail(c, MC_CLIENT_SCREEN_INVALID_PHASE, "ID native Slot fields",
                                      "id", "a", "(Lgp;)V", 1, true);
    const mc_item_stack *stack = NULL;
    if (!slot_error(c, mc_inventory_slot_source_get(slot, &stack, &c->error.slot), 11, "(Lgp;)V") ||
        !hold_stack(c, stack, 11))
        return false;
    if (stack == NULL) {
        int32_t icon;
        if (!slot_error(c, mc_inventory_slot_source_background(slot, &icon, &c->error.slot), 22,
                        "(Lgp;)V"))
            return false;
        if (icon >= 0) {
            mc_client_display *d = c->services.display;
            mc_client_texture texture;
            if (!mc_inventory_draw_gl(
                    c, mc_client_display_source_lighting_enable(d, false, &c->error.display), "id",
                    35))
                return false;
            mc_client_render_engine *bind_receiver = c->owner->base.fields.b->render_engine;
            if (!mc_inventory_draw_texture(c, c->owner->base.fields.b->render_engine,
                                           "/gui/items.png", "id", 54, &texture))
                return false;
            if (bind_receiver == NULL)
                return mc_inventory_draw_fail(c, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE,
                                              "ID actual captured JI bind null receiver", "id", "a",
                                              "(Lgp;)V", 57, false);
            if ((mc_java_i32_narrow((int64_t)texture) >= 0 &&
                 !mc_inventory_draw_gl(
                     c, mc_client_display_source_bind_id(d, texture, &c->error.display), "id",
                     57)) ||
                !mc_inventory_draw_tile(c, state.x, state.y, (icon % 16) * 16, (icon / 16) * 16, 16,
                                        16) ||
                !mc_inventory_draw_gl(
                    c, mc_client_display_source_lighting_enable(d, true, &c->error.display), "id",
                    87))
                return false;
            c->id_descriptor = "(IIF)V";
            return true;
        }
    }
    item_arguments arguments;
    if (!capture_item_arguments(c, 104, &arguments) ||
        !item(c, &arguments, stack, state.x, state.y, 111) ||
        !capture_item_arguments(c, 125, &arguments) ||
        !overlay(c, &arguments, stack, state.x, state.y, 132))
        return false;
    c->id_descriptor = "(IIF)V";
    return true;
}

static bool cursor(mc_client_inventory_draw_call *c, mc_inventory_player *inventory,
                   const mc_item_stack **out, unsigned int pc) {
    if (inventory == NULL)
        return mc_inventory_draw_fail(c, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE,
                                      "ID actual IX cursor null receiver", "id", "a", "(IIF)V", pc,
                                      false);
    if (mc_inventory_player_source_cursor_get(inventory, out, &c->error.inventory) !=
        MC_INVENTORY_OK)
        return mc_inventory_draw_fail(c, MC_CLIENT_SCREEN_APP_FAILED, c->error.inventory.operation,
                                      "id", "a", "(IIF)V", pc, true);
    return hold_stack(c, *out, pc);
}

static bool foreground(mc_client_inventory_draw_call *c) {
    if (c->owner->base.kind == MC_CLIENT_SCREEN_WORKBENCH)
        return mc_workbench_draw_foreground(c);
    mc_client_source_id *o = c->owner;
    mc_player_string *literal = NULL;
    if (!mc_screen_lock(o->base.classes, &c->error.screen))
        return false;
    const bool found =
        mc_screen_literal_locked(o->base.classes, "Crafting", &literal, &c->error.screen);
    mtx_unlock(&o->base.classes->mutex);
    if (!found)
        return false;
    if (o->base.fields.g == NULL)
        return mc_inventory_draw_fail(c, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE,
                                      "UE Crafting actual Font null receiver", "ue", "k", "()V", 12,
                                      false);
    if (mc_client_bitmap_font_draw(o->base.fields.g, literal, 86, 16, 4210752, false,
                                   &c->error.font) != MC_CLIENT_BITMAP_FONT_OK)
        return mc_inventory_draw_fail(c, MC_CLIENT_SCREEN_FONT_FAILED, c->error.font.operation,
                                      "ue", "k", "()V", 12, c->error.font.native_policy);
    c->state.foreground_returned = true;
    return true;
}

static bool tooltip(mc_client_inventory_draw_call *c, mc_inventory_slot *hovered) {
    if (!mc_item_name_workspace_init(&c->names, 65536))
        return mc_inventory_draw_fail(c, MC_CLIENT_SCREEN_ALLOCATION_FAILED,
                                      "ID bounded native tooltip workspace", "id", "a", "(IIF)V",
                                      350, true);
    const mc_translation *translation = NULL;
    if (!mc_item_name_translation_get(c->owner->base.classes->registry, &translation,
                                      &c->error.name))
        goto failed;
    const mc_item_stack *stack = NULL;
    if (!slot_error(c, mc_inventory_slot_source_get(hovered, &stack, &c->error.slot), 367,
                    "(IIF)V") ||
        !hold_stack(c, stack, 367))
        return false;
    const mc_registry_string *raw = NULL, *translated = NULL;
    if (!mc_item_stack_render_name(c->owner->base.classes->registry, (mc_item_stack *)stack,
                                   &c->names, &raw, &c->error.name) ||
        !mc_item_name_tooltip_translate(c->owner->base.classes->registry, translation, raw,
                                        &c->names, &translated, &c->error.name))
        goto failed;
    mc_registry_string_view text;
    if (!mc_registry_string_read(translated, &text))
        return mc_inventory_draw_fail(c, MC_CLIENT_SCREEN_STRING_FAILED,
                                      "ID actual tooltip String storage", "id", "a", "(IIF)V", 389,
                                      true);
    if (text.length == 0)
        return true;
    mc_client_bitmap_font *font = c->owner->base.fields.g;
    if (font == NULL)
        return mc_inventory_draw_fail(c, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE,
                                      "ID actual tooltip Font null receiver", "id", "a", "(IIF)V",
                                      419, false);
    const mc_player_string_view view = {text.units, text.length};
    int32_t width;
    ++c->state.font_measurements;
    if (mc_client_bitmap_font_measure_view(font, &view, &width, &c->error.font) !=
        MC_CLIENT_BITMAP_FONT_OK)
        return mc_inventory_draw_fail(c, MC_CLIENT_SCREEN_FONT_FAILED, c->error.font.operation,
                                      "id", "a", "(IIF)V", 419, c->error.font.native_policy);
    const int32_t x = mc_java_i32_add(mc_java_i32_sub(c->mouse.x, c->state.origin_x), 12);
    const int32_t y = mc_java_i32_sub(mc_java_i32_sub(c->mouse.y, c->state.origin_y), 12);
    if (!mc_inventory_draw_gradient(c, mc_java_i32_sub(x, 3), mc_java_i32_sub(y, 3),
                                    mc_java_i32_add(mc_java_i32_add(x, width), 3),
                                    mc_java_i32_add(y, 11), UINT32_C(0xc0000000),
                                    UINT32_C(0xc0000000)))
        return false;
    font = c->owner->base.fields.g;
    if (font == NULL)
        return mc_inventory_draw_fail(c, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE,
                                      "ID fresh tooltip Font null receiver", "id", "a", "(IIF)V",
                                      465, false);
    if (mc_client_bitmap_font_draw_shadow_view(font, &view, x, y, UINT32_MAX, &c->error.font) !=
        MC_CLIENT_BITMAP_FONT_OK)
        return mc_inventory_draw_fail(c, MC_CLIENT_SCREEN_FONT_FAILED, c->error.font.operation,
                                      "id", "a", "(IIF)V", 465, c->error.font.native_policy);
    c->state.tooltip_drawn = true;
    return true;
failed: {
    const mc_item_render_site s = c->error.name.site;
    return mc_inventory_draw_fail(c, MC_CLIENT_SCREEN_REGISTRY_FAILED, c->error.name.operation,
                                  s.class_name, s.method, s.descriptor, (unsigned int)s.pc,
                                  c->error.name.native_policy);
}
}

static bool body(mc_client_inventory_draw_call *c) {
    mc_client_source_id *o = c->owner;
    if (o->base.fields.b == NULL)
        return mc_inventory_draw_fail(c, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE,
                                      "DA actual background App null receiver", "da", "a", "(I)V",
                                      4, false);
    if (o->base.fields.b->active_world == NULL)
        return mc_inventory_draw_fail(c, MC_CLIENT_SCREEN_UNIMPLEMENTED_DEPENDENCY,
                                      "Inventory DA dirt background descendant", "da", "a", "(I)V",
                                      32, true);
    if (!mc_inventory_draw_gradient(c, 0, 0, o->base.fields.c, o->base.fields.d,
                                    UINT32_C(0xc0101010), UINT32_C(0xd0101010)))
        return false;
    c->state.origin_x = mc_java_i32_sub(o->base.fields.c, o->id.a) / 2;
    c->state.origin_y = mc_java_i32_sub(o->base.fields.d, o->id.i) / 2;
    if (!(o->base.kind == MC_CLIENT_SCREEN_INVENTORY ? mc_inventory_draw_background(c)
                                                     : mc_workbench_draw_background(c)))
        return false;
    mc_client_display *d = c->services.display;
#define GL(expr, pc)                                                                               \
    do {                                                                                           \
        if (!mc_inventory_draw_gl(c, (expr), "id", (pc)))                                          \
            return false;                                                                          \
    } while (0)
    GL(mc_client_display_source_push(d, &c->error.display), 35);
    GL(mc_client_display_source_rotate(d, 120, 1, 0, 0, &c->error.display), 43);
    if (!mc_client_lighting_enable(
            c->services.graphics, o->base.fields.b->entities, c->services.math, d,
            (mc_client_graphics_site){"id", "a", "(IIF)V", 46}, &c->error.lighting))
        return mc_inventory_draw_fail(c, MC_CLIENT_SCREEN_TEXTURE_FAILED,
                                      c->error.lighting.operation, "id", "a", "(IIF)V", 46,
                                      c->error.lighting.native_policy);
    GL(mc_client_display_source_pop(d, &c->error.display), 49);
    GL(mc_client_display_source_push(d, &c->error.display), 52);
    if (!mc_inventory_draw_fp(c, "id", 58))
        return false;
    GL(mc_client_display_source_translate(d, (float)c->state.origin_x, (float)c->state.origin_y, 0,
                                          &c->error.display),
       62);
    GL(mc_client_display_source_color(d, 1, 1, 1, 1, &c->error.display), 69);
    GL(mc_client_display_source_rescale_normal(d, true, &c->error.display), 74);
    mc_inventory_slot *hovered = NULL;
    for (int32_t index = 0;; index = mc_java_i32_add(index, 1)) {
        int32_t size;
        if (o->id.j == NULL)
            return mc_inventory_draw_fail(c, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE,
                                          "ID actual Container.e null receiver", "id", "a",
                                          "(IIF)V", 89, false);
        if (!slot_error(c, mc_container_source_slots_size(o->id.j, &size, &c->error.slot), 92,
                        "(IIF)V"))
            return false;
        if (index >= size)
            break;
        if (index >= 4096)
            return mc_inventory_draw_fail(c, MC_CLIENT_SCREEN_LIMIT, "ID native Slot work bound",
                                          "id", "a", "(IIF)V", 109, true);
        mc_inventory_slot *slot = NULL;
        if (!slot_error(c, mc_container_source_slot(o->id.j, index, &slot, &c->error.slot), 109,
                        "(IIF)V") ||
            !draw_slot(c, slot))
            return false;
        ++c->state.slots_drawn;
        mc_container_slot_state s;
        if (!mc_inventory_slot_source_state(slot, &s))
            return mc_inventory_draw_fail(c, MC_CLIENT_SCREEN_INVALID_PHASE,
                                          "ID actual hover Slot fields", "id", "a", "(Lgp;II)Z", 32,
                                          true);
        /* The private hit-test recomputes origin from the fresh screen fields. */
        const int32_t local_x =
            mc_java_i32_sub(c->mouse.x, mc_java_i32_sub(o->base.fields.c, o->id.a) / 2);
        const int32_t local_y =
            mc_java_i32_sub(c->mouse.y, mc_java_i32_sub(o->base.fields.d, o->id.i) / 2);
        if (local_x >= mc_java_i32_sub(s.x, 1) && local_x < mc_java_i32_add(s.x, 17) &&
            local_y >= mc_java_i32_sub(s.y, 1) && local_y < mc_java_i32_add(s.y, 17)) {
            hovered = slot;
            c->state.hovered_slot = index;
            GL(mc_client_display_source_lighting_enable(d, false, &c->error.display), 143);
            GL(mc_client_display_source_depth_test(d, false, &c->error.display), 149);
            if (!mc_inventory_draw_gradient(c, s.x, s.y, mc_java_i32_add(s.x, 16),
                                            mc_java_i32_add(s.y, 16), UINT32_C(0x80ffffff),
                                            UINT32_C(0x80ffffff)))
                return false;
            GL(mc_client_display_source_lighting_enable(d, true, &c->error.display), 191);
            GL(mc_client_display_source_depth_test(d, true, &c->error.display), 197);
        }
    }
    mc_entity *p = mc_inventory_draw_player(c, "id", 213);
    if (p == NULL)
        return false;
    mc_inventory_player *inventory = p->local_player->fields.player.c;
    const mc_item_stack *stack = NULL;
    if (!cursor(c, inventory, &stack, 220))
        return false;
    if (stack != NULL) {
        GL(mc_client_display_source_translate(d, 0, 0, 32, &c->error.display), 230);
        const int32_t x = mc_java_i32_sub(mc_java_i32_sub(c->mouse.x, c->state.origin_x), 8);
        const int32_t y = mc_java_i32_sub(mc_java_i32_sub(c->mouse.y, c->state.origin_y), 8);
        item_arguments arguments;
        if (!capture_item_arguments(c, 244, &arguments) || !cursor(c, inventory, &stack, 249) ||
            !item(c, &arguments, stack, x, y, 266) || !capture_item_arguments(c, 280, &arguments) ||
            !cursor(c, inventory, &stack, 285) || !overlay(c, &arguments, stack, x, y, 302))
            return false;
        c->state.cursor_drawn = true;
    }
    GL(mc_client_display_source_rescale_normal(d, false, &c->error.display), 307);
    if (!mc_client_lighting_disable(c->services.graphics, d,
                                    (mc_client_graphics_site){"id", "a", "(IIF)V", 310},
                                    &c->error.graphics))
        return mc_inventory_draw_fail(c, MC_CLIENT_SCREEN_TEXTURE_FAILED,
                                      c->error.graphics.operation, "id", "a", "(IIF)V", 310, true);
    GL(mc_client_display_source_lighting_enable(d, false, &c->error.display), 316);
    GL(mc_client_display_source_depth_test(d, false, &c->error.display), 322);
    if (!foreground(c) || !cursor(c, inventory, &stack, 331))
        return false;
    if (stack == NULL && hovered != NULL) {
        bool has;
        if (!slot_error(c, mc_inventory_slot_source_has(hovered, &has, &c->error.slot), 344,
                        "(IIF)V"))
            return false;
        if (has && !tooltip(c, hovered))
            return false;
    }
    GL(mc_client_display_source_pop(d, &c->error.display), 468);
    if (o->base.fields.e == NULL)
        return mc_inventory_draw_fail(c, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE,
                                      "DA actual Button List.size null receiver", "da", "a",
                                      "(IIF)V", 9, false);
    if (o->base.fields.e->size != 0)
        return mc_inventory_draw_fail(c, MC_CLIENT_SCREEN_UNIMPLEMENTED_DEPENDENCY,
                                      "DA actual Inventory Button draw descendant", "da", "a",
                                      "(IIF)V", 41, true);
    GL(mc_client_display_source_lighting_enable(d, true, &c->error.display), 481);
    GL(mc_client_display_source_depth_test(d, true, &c->error.display), 487);
    c->state.id_returned = true;
    if (o->base.kind == MC_CLIENT_SCREEN_INVENTORY) {
        if (!mc_inventory_draw_fp(c, "ue", 9))
            return false;
        /* The admitted genuine UE begins with this exact ID subobject. */
        mc_client_source_ue *ue = c->state.receiver;
        ue->fields.l = (float)c->mouse.x;
        ue->fields.m = (float)c->mouse.y;
        c->state.mouse_stored = true;
        c->state.source_pc = o->last_pc = 19;
        c->error.screen.site = (mc_client_screen_site){"ue", "a", "(IIF)V", 19};
    } else {
        c->state.source_pc = o->last_pc = 490;
        c->error.screen.site = (mc_client_screen_site){"id", "a", "(IIF)V", 490};
    }
    c->state.returned = c->error.screen.source_returned = true;
#undef GL
    return true;
}

mc_client_screen_status mc_inventory_draw_begin_id(mc_client_source_id *o,
                                                   mc_client_screen_kind expected,
                                                   mc_gui_mouse_position mouse, float partial,
                                                   const mc_client_manager_draw_services *services,
                                                   mc_client_inventory_draw_call **out,
                                                   mc_client_inventory_draw_error *error) {
    if (o == NULL || out == NULL || *out != NULL || services == NULL) {
        mc_client_inventory_draw_error e = {0};
        mc_screen_fail(&e.screen, MC_CLIENT_SCREEN_INVALID_ARGUMENT,
                       "Inventory draw native arguments", NULL, NULL, NULL, 0);
        if (error != NULL)
            *error = e;
        return e.screen.status;
    }
    mc_client_inventory_draw_call *c = calloc(1, sizeof(*c));
    if (c == NULL) {
        if (error != NULL) {
            memset(error, 0, sizeof(*error));
            mc_screen_fail(&error->screen, MC_CLIENT_SCREEN_ALLOCATION_FAILED,
                           "Inventory retained native draw allocation", NULL, NULL, NULL, 0);
        }
        return MC_CLIENT_SCREEN_ALLOCATION_FAILED;
    }
    *out = c;
    c->owner = o;
    c->id_descriptor = "(IIF)V";
    c->state.screen_receiver = &o->base;
    if (expected == MC_CLIENT_SCREEN_INVENTORY && o->base.kind == MC_CLIENT_SCREEN_INVENTORY)
        c->state.receiver = (mc_client_source_ue *)o;
    c->state.hovered_slot = -1;
    c->state.failure = &c->error;
    c->services = *services;
    c->mouse = mouse;
    c->partial = partial;
    if (!mc_screen_lock(o->base.classes, &c->error.screen))
        goto done;
    if ((expected != MC_CLIENT_SCREEN_INVENTORY && expected != MC_CLIENT_SCREEN_WORKBENCH) ||
        o->base.kind != expected || o->base.phase != MC_CLIENT_SCREEN_READY ||
        o->base.active_calls != 0 || o->base.last_initialization == NULL) {
        mc_inventory_draw_fail(c, MC_CLIENT_SCREEN_INVALID_PHASE,
                               "Inventory initialized exclusive draw owner", NULL, NULL, NULL, 0,
                               true);
        mtx_unlock(&o->base.classes->mutex);
        goto done;
    }
    ++o->base.active_calls;
    c->screen_entered = true;
    if (o->base.kind == MC_CLIENT_SCREEN_WORKBENCH)
        c->state.workbench = true;
    mtx_unlock(&o->base.classes->mutex);
    c->state.active = true;
    (void)body(c);
    c->state.active = false;
done:
    if (error != NULL)
        *error = c->error;
    return c->error.screen.status;
}

mc_client_screen_status mc_client_source_inventory_draw_begin(
    mc_client_source_ue *o, mc_gui_mouse_position mouse, float partial,
    const mc_client_manager_draw_services *services, mc_client_inventory_draw_call **out,
    mc_client_inventory_draw_error *error) {
    return mc_inventory_draw_begin_id(o == NULL ? NULL : &o->container, MC_CLIENT_SCREEN_INVENTORY,
                                      mouse, partial, services, out, error);
}

bool mc_client_source_inventory_draw_inspect(const mc_client_inventory_draw_call *c,
                                             mc_client_inventory_draw_state *out) {
    if (c == NULL || out == NULL)
        return false;
    *out = c->state;
    return true;
}

mc_client_screen_status
mc_client_source_inventory_draw_retire(mc_client_inventory_draw_call **slot,
                                       mc_client_inventory_draw_error *error) {
    if (slot == NULL)
        return MC_CLIENT_SCREEN_INVALID_ARGUMENT;
    mc_client_inventory_draw_call *c = *slot;
    if (c == NULL)
        return MC_CLIENT_SCREEN_OK;
    if (c->state.active)
        return MC_CLIENT_SCREEN_BUSY;
    if (mc_client_item_gui_retire(&c->item, &c->cleanup.item) != MC_CLIENT_ITEM_GUI_OK ||
        mc_client_render_manager_draw_retire(&c->preview, &c->cleanup.player) !=
            MC_CLIENT_MANAGER_OK)
        goto busy;
    c->error.item.inventory = NULL;
    c->error.player.entity = NULL;
    c->error.player.living = NULL;
    c->state.preview = NULL;
    if (c->pending_lookup != NULL) {
        if (mc_client_render_engine_retire_lookup(c->pending_lookup, &c->cleanup.texture) !=
            MC_CLIENT_LOOKUP_OK)
            goto busy;
        c->pending_lookup = NULL;
    }
    if (mc_client_render_manager_release(&c->store_manager, &c->cleanup.manager) !=
        MC_CLIENT_MANAGER_OK)
        goto busy;
    while (c->stack_count != 0)
        mc_item_stack_release(c->stacks[--c->stack_count]);
    while (c->player_count != 0)
        mc_entity_release(c->players[--c->player_count]);
    if (c->names.storage != NULL &&
        (!mc_item_name_workspace_reset(&c->names) || !mc_item_name_workspace_destroy(&c->names)))
        goto busy;
    if (c->screen_entered) {
        if (!mc_screen_lock(c->owner->base.classes, &c->cleanup.screen))
            goto busy;
        --c->owner->base.active_calls;
        c->screen_entered = false;
        mtx_unlock(&c->owner->base.classes->mutex);
    }
    free(c);
    *slot = NULL;
    return MC_CLIENT_SCREEN_OK;
busy:
    if (error != NULL)
        *error = c->cleanup;
    return MC_CLIENT_SCREEN_BUSY;
}
