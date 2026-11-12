/* Entered UQ.a(FZII)V closed-chat branch. The actual admitted DA kinds are
 * disjoint from GC, so its instanceof1808 selects ten lines and false. */
static bool filled_rectangle_child(mc_client_gui_ingame_draw_call *c, bool ok, unsigned int pc,
                                   bool graphics, unsigned int parent) {
    if (ok) {
        if (!graphics)
            ++c->state.gl_calls;
        return true;
    }
    return child_fail(c, MC_CLIENT_GUI_INGAME_DRAW_CHILD_FAILED,
                      graphics ? c->failure.graphics.operation : c->failure.display.operation,
                      parent, "ub", "a", "(IIIII)V", pc,
                      graphics ? c->failure.graphics.native_policy : true);
}

/* Actual shared ub.a(IIIII)V, reached by both sleep1120 and chat2052. */
static bool filled_rectangle(mc_client_gui_ingame_draw_call *c, int32_t left, int32_t top,
                             int32_t right, int32_t bottom, uint32_t color, unsigned int parent) {
    if (left < right) {
        const int32_t swap = left;
        left = right;
        right = swap;
    }
    if (top < bottom) {
        const int32_t swap = top;
        top = bottom;
        bottom = swap;
    }
    if (!fp(c, parent))
        return false;
    const float alpha = at_fdiv((float)((color >> 24) & 255u), 255.0f);
    const float red = at_fdiv((float)((color >> 16) & 255u), 255.0f);
    const float green = at_fdiv((float)((color >> 8) & 255u), 255.0f);
    const float blue = at_fdiv((float)(color & 255u), 255.0f);
    if (!filled_rectangle_child(
            c,
            mc_client_nw_active_use(c->graphics,
                                    (mc_client_graphics_site){"ub", "a", "(IIIII)V", 86}, &c->nw,
                                    &c->failure.graphics),
            86, true, parent) ||
        !filled_rectangle_child(
            c, mc_client_display_source_blend_enable(c->display, true, &c->failure.display), 94,
            false, parent) ||
        !filled_rectangle_child(
            c, mc_client_display_source_texture_enable(c->display, false, &c->failure.display), 100,
            false, parent) ||
        !filled_rectangle_child(
            c, mc_client_display_source_blend_function(c->display, 770, 771, &c->failure.display),
            109, false, parent) ||
        !filled_rectangle_child(c,
                                mc_client_display_source_color(c->display, red, green, blue, alpha,
                                                               &c->failure.display),
                                120, false, parent) ||
        !filled_rectangle_child(c, mc_client_nw_begin_quads(c->nw, &c->failure.graphics), 125, true,
                                parent))
        return false;
    const int32_t x[] = {left, right, right, left};
    const int32_t y[] = {bottom, bottom, top, top};
    const unsigned int pc[] = {136, 147, 157, 167};
    for (size_t i = 0; i < 4; ++i) {
        if (!filled_rectangle_child(
                c,
                mc_client_nw_vertex(c->nw, (double)x[i], (double)y[i], 0.0, &c->failure.graphics),
                pc[i], true, parent))
            return false;
        ++c->state.vertices;
    }
    if (!filled_rectangle_child(c, mc_client_nw_draw(c->nw, c->display, &c->failure.graphics), 172,
                                true, parent) ||
        !filled_rectangle_child(
            c, mc_client_display_source_texture_enable(c->display, true, &c->failure.display), 178,
            false, parent) ||
        !filled_rectangle_child(
            c, mc_client_display_source_blend_enable(c->display, false, &c->failure.display), 184,
            false, parent))
        return false;
    return true;
}

static bool chat_lines(mc_client_gui_ingame_draw_call *c) {
    mc_client_source_uq *owner = c->state.owner;
    for (int32_t index = 0;; index = mc_java_i32_add(index, 1)) {
        mc_client_gui_ingame_list *list = owner->fields.e;
        if (list == NULL)
            return fail(c, MC_CLIENT_GUI_INGAME_DRAW_SOURCE_NULL,
                        "actual UQ chat List.size receiver", 1865, false);
        if (list->size > list->capacity || list->size > INT32_MAX ||
            (list->size != 0 && list->entries == NULL))
            return fail(c, MC_CLIENT_GUI_INGAME_DRAW_DEPENDENCY,
                        "HUD native chat List storage invariant", 1865, true);
        if ((size_t)index >= list->size || index >= 10)
            return true;
        void *entry = list->entries[index];
        mc_gui_chat_message *message = mc_gui_chat_message_find(owner, entry);
        if (entry == NULL)
            return fail(c, MC_CLIENT_GUI_INGAME_DRAW_SOURCE_NULL, "UQ actual SW age null receiver",
                        1894, false);
        if (message == NULL)
            return fail(c, MC_CLIENT_GUI_INGAME_DRAW_DEPENDENCY, "UQ foreign native SW owner/type",
                        1891, true);
        ++c->state.chat_entries;
        if (message->b >= 200)
            continue;
        if (!fp(c, 1929))
            return false;
        double fade = at_div((double)message->b, 200.0);
        fade = at_sub(1.0, fade);
        fade = at_mul(fade, 10.0);
        if (fade < 0.0)
            fade = 0.0;
        if (fade > 1.0)
            fade = 1.0;
        fade = at_mul(fade, fade);
        const int32_t alpha = (int32_t)at_mul(255.0, fade);
        if (alpha <= 0)
            continue;
        const int32_t x = 2, y = mc_java_i32_mul(mc_java_i32_sub(0, index), 9);
        mc_player_string_view text;
        const bool null = message->a.player == NULL && message->a.registry == NULL;
        if (!null && !mc_gui_chat_text_read(message->a, &text))
            return fail(c, MC_CLIENT_GUI_INGAME_DRAW_DEPENDENCY,
                        "UQ native immutable SW String view", 2022, true);
        if (!filled_rectangle(c, x, mc_java_i32_sub(y, 1), mc_java_i32_add(x, 320),
                              mc_java_i32_add(y, 8), (uint32_t)(alpha / 2) << 24, 2052))
            return false;
        ++c->state.chat_backgrounds;
        if (!gl(c, mc_client_display_source_blend_enable(c->display, true, &c->failure.display),
                2058))
            return false;
        if (c->font == NULL)
            return fail(c, MC_CLIENT_GUI_INGAME_DRAW_SOURCE_NULL,
                        "actual UQ captured Font.draw null receiver", 2077, false);
        if (mc_client_bitmap_font_draw_shadow_view(c->font, null ? NULL : &text, x, y,
                                                   UINT32_C(0x00ffffff) + ((uint32_t)alpha << 24),
                                                   &c->font_error) != MC_CLIENT_BITMAP_FONT_OK)
            return child_fail(c, MC_CLIENT_GUI_INGAME_DRAW_CHILD_FAILED, c->font_error.operation,
                              2077, "sj", c->font_error.method, c->font_error.descriptor,
                              (unsigned int)c->font_error.pc, c->font_error.native_policy);
        const unsigned int drawn = c->state.chat_draws++;
        c->state.chat_age[drawn] = message->b;
        c->state.chat_alpha[drawn] = alpha;
    }
}
