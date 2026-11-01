/* Actual private UQ.a(IIIF)V. The enclosing UQ call owns every captured
 * Stack/Font/Registry and any failed Item child until checked retirement. */
static void item_inventory_snapshot(mc_client_gui_ingame_draw_call *c) {
    const mc_cv_context *inventory = c->item_error.inventory;
    if (inventory == NULL || c->failure.item_inventory.valid)
        return;
    mc_client_gui_ingame_inventory_receipt *out = &c->failure.item_inventory;
    *out = (mc_client_gui_ingame_inventory_receipt){
        .valid = true,
        .failed = inventory->failed,
        .returned = inventory->returned,
        .native_policy = inventory->failure.native_policy,
        .status = inventory->failure.status,
        .operation = inventory->failure.operation,
        .site = {inventory->failure.source_class, inventory->failure.source_method,
                 inventory->failure.source_descriptor, (int32_t)inventory->failure.source_pc},
        .child = inventory->failure.child,
        .depth = inventory->depth,
        .faces = inventory->inventory_faces,
        .source_pc = inventory->source_pc,
        .last_store_pc = inventory->last_store_pc,
        .graphics = inventory->failure.graphics,
        .display = inventory->failure.display};
    _Static_assert(sizeof out->frames / sizeof *out->frames >=
                       sizeof inventory->frames / sizeof *inventory->frames,
                   "HUD copied CV frame extent");
    for (size_t i = 0; i < inventory->depth; ++i) {
        const mc_cv_frame *frame = &inventory->frames[i];
        out->frames[i] = (mc_client_graphics_site){frame->class_name, frame->method,
                                                   frame->descriptor, (int32_t)frame->pc};
    }
}

static void item_inventory_detach(mc_client_gui_ingame_draw_call *c) {
    /* Child retirement has consumed its actual CV allocation. The immutable
     * terminal failure and current cleanup result keep copied scalar/source
     * diagnostics, never a pointer into that released native workspace. */
    c->item_error.inventory = NULL;
    c->item_cleanup.inventory = NULL;
    c->item_retirement.inventory = NULL;
    c->cleanup.item = NULL;
}

static bool slot_fail(mc_client_gui_ingame_draw_call *c, const char *operation, unsigned int pc,
                      bool native) {
    return child_fail(
        c, native ? MC_CLIENT_GUI_INGAME_DRAW_DEPENDENCY : MC_CLIENT_GUI_INGAME_DRAW_SOURCE_NULL,
        operation, 1015, "uq", "a", "(IIIF)V", pc, native);
}

static bool slot_gl(mc_client_gui_ingame_draw_call *c, bool ok, unsigned int pc) {
    if (!ok)
        return slot_fail(c, c->failure.display.operation, pc, true);
    ++c->state.gl_calls;
    return true;
}

static bool slot_item_failure(mc_client_gui_ingame_draw_call *c, const char *operation,
                              unsigned int pc, bool native, const char *exception) {
    const bool null = exception != NULL && strcmp(exception, "java/lang/NullPointerException") == 0;
    return child_fail(c,
                      null     ? MC_CLIENT_GUI_INGAME_DRAW_SOURCE_NULL
                      : native ? MC_CLIENT_GUI_INGAME_DRAW_DEPENDENCY
                               : MC_CLIENT_GUI_INGAME_DRAW_CHILD_FAILED,
                      operation, 1015, "uq", "a", "(IIIF)V", pc, native);
}

static client_application *slot_app(mc_client_gui_ingame_draw_call *c, unsigned int pc) {
    client_application *a = c->state.owner->fields.g;
    if (a == NULL)
        slot_fail(c, "actual UQ slot null App receiver", pc, false);
    return a;
}

static bool slot_font_hold(mc_client_gui_ingame_draw_call *c, mc_client_bitmap_font *font,
                           unsigned int pc) {
    if (font == NULL || font == c->font)
        return true;
    for (size_t i = 0; i < c->slot_font_count; ++i)
        if (c->slot_fonts[i] == font)
            return true;
    if (c->slot_font_count == sizeof c->slot_fonts / sizeof *c->slot_fonts ||
        mc_client_bitmap_font_retain(font) != MC_CLIENT_BITMAP_FONT_OK)
        return slot_fail(c, "HUD actual slot Font occurrence", pc, true);
    c->slot_fonts[c->slot_font_count++] = font;
    return true;
}

static bool slot_registry_hold(mc_client_gui_ingame_draw_call *c, mc_registry_classes *registry,
                               unsigned int pc) {
    for (size_t i = 0; i < c->slot_registry_count; ++i)
        if (c->slot_registries[i] == registry)
            return true;
    if (c->slot_registry_count == sizeof c->slot_registries / sizeof *c->slot_registries ||
        mc_registry_classes_retain(registry, MC_REGISTRY_CLIENT, c->state.owner->classes->profile,
                                   &c->failure.registry) != MC_REGISTRY_OK)
        return slot_fail(c, "HUD actual slot Registry occurrence", pc, true);
    c->slot_registries[c->slot_registry_count++] = registry;
    return true;
}

static bool item_slot(mc_client_gui_ingame_draw_call *c, int32_t index, int32_t x, int32_t y) {
    client_application *a = slot_app(c, 4);
    if (a == NULL)
        return false;
    mc_entity *p = a->h;
    if (p == NULL)
        return slot_fail(c, "actual UQ slot null Player receiver", 7, false);
    if (!hold_player(c, p, 1015))
        return false;
    mc_local_player_snapshot player_state;
    if (!player_fields(c, p, &player_state, 1015))
        return false;
    mc_inventory_player *inv = player_state.player.c;
    if (inv == NULL)
        return slot_fail(c, "actual UQ slot null Inventory receiver", 10, false);
    mc_inventory_player_state inventory_fields;
    if (!inventory_state(c, inv, &inventory_fields, 1015))
        return false;
    if (inventory_fields.main_slots == NULL)
        return slot_fail(c, "actual UQ slot null main array", 14, false);
    mc_item_stack *stack = inventory_fields.main_slots[index];
    if (stack == NULL) {
        ++c->state.empty_slots;
        return true;
    }
    c->failure.stack = mc_item_stack_retain(stack);
    if (c->failure.stack != MC_ITEM_STACK_OK)
        return slot_fail(c, "HUD actual captured slot Stack occurrence", 23, true);
    c->slot_stacks[c->slot_stack_count++] = stack;
    mc_item_stack_fields fields;
    c->failure.stack = mc_item_stack_fields_get(stack, &fields);
    if (c->failure.stack != MC_ITEM_STACK_OK)
        return slot_fail(c, "actual UQ slot Stack animation field", 25, true);
    if (!fp(c, 1015))
        return false;
    const float animation = at_fsub((float)fields.animation, c->state.partial);
    if (animation > 0) {
        if (!slot_gl(c, mc_client_display_source_push(c->display, &c->failure.display), 41))
            return false;
        const float scale = at_fadd(1, at_fdiv(animation, 5));
        if (!slot_gl(c,
                     mc_client_display_source_translate(c->display, (float)mc_java_i32_add(x, 8),
                                                        (float)mc_java_i32_add(y, 12), 0,
                                                        &c->failure.display),
                     64) ||
            !slot_gl(c,
                     mc_client_display_source_scale(c->display, at_fdiv(1, scale),
                                                    at_fdiv(at_fadd(scale, 1), 2), 1,
                                                    &c->failure.display),
                     78) ||
            !slot_gl(c,
                     mc_client_display_source_translate(
                         c->display, (float)mc_java_i32_neg(mc_java_i32_add(x, 8)),
                         (float)mc_java_i32_neg(mc_java_i32_add(y, 12)), 0, &c->failure.display),
                     94))
            return false;
        ++c->state.animated_slots;
    }
    mc_client_item_renderer *renderer = c->state.owner->classes->d; /* getstatic97 */
    a = slot_app(c, 104);
    if (a == NULL)
        return false;
    mc_client_bitmap_font *font = a->font_renderer;
    if (!slot_font_hold(c, font, 104))
        return false;
    a = slot_app(c, 111);
    if (a == NULL)
        return false;
    mc_client_render_engine *engine = a->render_engine;
    if (renderer == NULL)
        return slot_fail(c, "actual UQ slot BB draw null receiver", 118, false);
    if (!slot_registry_hold(c, a->registry, 118))
        return false;
    if (c->item_call != NULL)
        return slot_fail(c, "HUD preceding Item draw still owned", 118, true);
    /* Each actual invocation owns its receipt. A later slot must not reuse
     * an earlier slot's returned CV snapshot when this child fails. */
    c->failure.item_inventory = (mc_client_gui_ingame_inventory_receipt){0};
    c->failure.item = &c->item_error;
    if (mc_client_item_gui_begin(renderer, c->graphics, a->registry, c->display, font, engine,
                                 stack, x, y, &c->item_call,
                                 &c->item_error) != MC_CLIENT_ITEM_GUI_OK)
        return slot_item_failure(c, c->item_error.operation, 118, c->item_error.native_policy,
                                 c->item_error.exception_class);
    mc_client_item_gui_state item_state;
    if (!mc_client_item_gui_inspect(c->item_call, &item_state) || !item_state.returned)
        return slot_fail(c, "HUD actual Item draw returned receipt", 118, true);
    ++c->state.item_slots;
    c->state.item_faces += (unsigned int)item_state.block_faces;
    item_inventory_snapshot(c);
    if (mc_client_item_gui_retire(&c->item_call, &c->item_cleanup) != MC_CLIENT_ITEM_GUI_OK) {
        c->failure.item = &c->item_cleanup;
        return slot_fail(c, "HUD completed Item draw checked retirement", 118, true);
    }
    item_inventory_detach(c);
    c->failure.item = NULL;
    if (animation > 0 &&
        !slot_gl(c, mc_client_display_source_pop(c->display, &c->failure.display), 128))
        return false;
    renderer = c->state.owner->classes->d; /* Fresh getstatic131. */
    a = slot_app(c, 138);
    if (a == NULL)
        return false;
    font = a->font_renderer;
    if (!slot_font_hold(c, font, 138))
        return false;
    a = slot_app(c, 145);
    if (a == NULL)
        return false;
    /* The JI argument is evaluated here even though BB.b does not use it. */
    engine = a->render_engine;
    (void)engine;
    if (renderer == NULL)
        return slot_fail(c, "actual UQ slot BB overlay null receiver", 152, false);
    if (!slot_registry_hold(c, a->registry, 152))
        return false;
    mc_client_item_overlay_context *overlay = calloc(1, sizeof(*overlay));
    if (overlay == NULL)
        return slot_fail(c, "HUD reached Item overlay diagnostic allocation", 152, true);
    c->item_overlays[c->overlay_count++] = overlay;
    *overlay = (mc_client_item_overlay_context){.owner = renderer,
                                                .registry = a->registry,
                                                .font = font,
                                                .display = c->display,
                                                .stack = stack,
                                                .x = x,
                                                .y = y};
    c->failure.item_overlay = overlay;
    if (!mc_client_item_overlay_draw(overlay))
        return slot_item_failure(c, overlay->operation, 152, overlay->native_policy,
                                 overlay->exception_class);
    ++c->state.item_overlays;
    c->failure.item_overlay = NULL;
    return true;
}
