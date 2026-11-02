/* Nonempty RA.a(F) branch and its actual RA.a(LS,IZ) child. The same
 * retained RA draw owns every reached Engine/Registry/brightness occurrence. */
static bool held_registry(mc_client_held_item_draw_call *c, unsigned int pc) {
    if (c->item_registry_held)
        return true;
    /* Native association with the genuine RA's still-live App graph, not a
     * source App field invocation or an alternate registry construction. */
    const client_application *application = c->state.owner->source->a;
    if (application == NULL)
        return fail(c, MC_CLIENT_HELD_ITEM_DRAW_CHILD_FAILED,
                    "held Registry native App association", pc, true);
    c->item_registry = application->registry;
    if (mc_registry_classes_retain(c->item_registry, MC_REGISTRY_CLIENT,
                                   c->state.owner->context.services->profile,
                                   &c->failure.registry) != MC_REGISTRY_OK)
        return fail(c, MC_CLIENT_HELD_ITEM_DRAW_CHILD_FAILED, "held actual Registry occurrence", pc,
                    true);
    c->item_registry_held = true;
    return true;
}

static bool held_stack_item(mc_client_held_item_draw_call *c, unsigned int active_pc,
                            unsigned int id_pc, unsigned int at_pc, const mc_item **out) {
    if (!held_registry(c, active_pc))
        return false;
    const mc_registry_item_array *array;
    if (mc_registry_item_c_get(c->item_registry, &array, &c->failure.registry) != MC_REGISTRY_OK)
        return fail(c, MC_CLIENT_HELD_ITEM_DRAW_CHILD_FAILED, "held actual GM.c active use",
                    active_pc, true);
    int32_t id;
    c->failure.stack = mc_item_stack_source_id_get(c->state.stack, &id);
    if (c->failure.stack != MC_ITEM_STACK_OK)
        return fail(c, MC_CLIENT_HELD_ITEM_DRAW_CHILD_FAILED, "held actual Stack ID read", id_pc,
                    true);
    if (mc_registry_item_c_at(c->item_registry, array, id, out, &c->failure.registry) !=
        MC_REGISTRY_OK)
        return fail(c, MC_CLIENT_HELD_ITEM_DRAW_CHILD_FAILED, "held actual GM.c aaload", at_pc,
                    true);
    return true;
}

static bool held_color(mc_client_held_item_draw_call *c) {
    const mc_item *item;
    if (!held_stack_item(c, 126, 131, 134, &item))
        return false;
    int32_t damage, color;
    c->failure.stack = mc_item_stack_source_damage_get(c->state.stack, &damage);
    if (c->failure.stack != MC_ITEM_STACK_OK)
        return fail(c, MC_CLIENT_HELD_ITEM_DRAW_CHILD_FAILED, "held actual Stack metadata read",
                    137, true);
    if (item == NULL)
        return fail(c, MC_CLIENT_HELD_ITEM_DRAW_SOURCE_NULL, "held Item color null receiver", 140,
                    false);
    c->failure.item = &c->item_error;
    if (!mc_item_render_color(c->item_registry, item, damage, &color, &c->item_error)) {
        c->failure.child =
            (mc_client_graphics_site){c->item_error.site.class_name, c->item_error.site.method,
                                      c->item_error.site.descriptor, c->item_error.site.pc};
        return fail(c, MC_CLIENT_HELD_ITEM_DRAW_DEPENDENCY, c->item_error.operation, 140,
                    c->item_error.native_policy);
    }
    c->state.item_color_returned = true;
    if (!fp(c, 157))
        return false;
    const float red = at_fdiv((float)(((uint32_t)color >> 16u) & 255u), 255.0f);
    const float green = at_fdiv((float)(((uint32_t)color >> 8u) & 255u), 255.0f);
    const float blue = at_fdiv((float)((uint32_t)color & 255u), 255.0f);
    return gl(c,
              mc_client_display_source_color(
                  device(c), at_fmul(c->state.brightness, red), at_fmul(c->state.brightness, green),
                  at_fmul(c->state.brightness, blue), 1, &c->failure.display),
              203);
}

static bool held_child_fail(mc_client_held_item_draw_call *c, const char *operation,
                            unsigned int pc, bool source_null) {
    c->failure.child = (mc_client_graphics_site){"ra", "a", "(Lls;Liz;)V", (int32_t)pc};
    return fail(
        c, source_null ? MC_CLIENT_HELD_ITEM_DRAW_SOURCE_NULL : MC_CLIENT_HELD_ITEM_DRAW_DEPENDENCY,
        operation, 1074, !source_null);
}

/* Entered RA.a(LS,IZ)75 after its real push0. The parent already owns the
 * actual RA/Player/Stack/Registry/graphics graph through checked retirement. */
static bool held_flat_child(mc_client_held_item_draw_call *c) {
    c->state.flat_item = true;
    int32_t id;
    c->failure.stack = mc_item_stack_source_id_get(c->state.stack, &id);
    if (c->failure.stack != MC_ITEM_STACK_OK)
        return held_child_fail(c, "held Item second Stack ID read", 76, false);
    const bool terrain = id < 256;
    const unsigned int app_pc = terrain ? 92u : 113u;
    const unsigned int lookup_pc = terrain ? 97u : 118u;
    const unsigned int bind_pc = terrain ? 100u : 121u;
    client_application *application = c->state.owner->source->a; /* Fresh RA.a89/110. */
    if (application == NULL)
        return held_child_fail(c, "held Item App.p null receiver", app_pc, true);
    c->engine = application->render_engine;
    if (c->engine == NULL)
        return held_child_fail(c, "held Item captured Engine lookup null receiver", lookup_pc,
                               true);
    if (mc_client_render_engine_retain(c->engine) != MC_CLIENT_RENDER_ENGINE_OK) {
        c->engine = NULL;
        return held_child_fail(c, "held Item captured Engine native occurrence", lookup_pc, false);
    }
    static const uint16_t terrain_key[] = {'/', 't', 'e', 'r', 'r', 'a',
                                           'i', 'n', '.', 'p', 'n', 'g'};
    static const uint16_t items_key[] = {'/', 'g', 'u', 'i', '/', 'i', 't',
                                         'e', 'm', 's', '.', 'p', 'n', 'g'};
    const mc_texture_pack_text key =
        terrain
            ? (mc_texture_pack_text){terrain_key, sizeof terrain_key / sizeof *terrain_key, false}
            : (mc_texture_pack_text){items_key, sizeof items_key / sizeof *items_key, false};
    const bool foreign = mc_client_texture_lookup_pending(c->engine);
    mc_client_texture texture;
    const mc_client_texture_lookup_status looked = mc_client_render_engine_texture(
        c->engine, device(c), NULL, key, &texture, &c->download.lookup);
    c->failure.texture = &c->download.lookup;
    c->lookup_owned = !foreign && mc_client_texture_lookup_pending(c->engine);
    if (looked != MC_CLIENT_LOOKUP_OK) {
        (void)held_child_fail(c, c->download.lookup.operation, lookup_pc,
                              c->download.lookup.status == MC_CLIENT_LOOKUP_NULL_REFERENCE);
        c->failure.native_policy = c->download.lookup.native_policy;
        return false;
    }
    if (!mc_client_display_source_bind_id(device(c), texture, &c->failure.display))
        return held_child_fail(c, c->failure.display.operation, bind_pc, false);
    ++c->state.gl_calls;
    const mc_client_graphics_site capture = {"ra", "a", "(Lls;Liz;)V", 124};
    if (!mc_client_nw_active_use(graphics(c), capture, &c->item_nw, &c->failure.graphics)) {
        (void)held_child_fail(c, c->failure.graphics.operation, 124, false);
        c->failure.native_policy = c->failure.graphics.native_policy;
        return false;
    }
    /* The actual LS receiver is evaluated after the captured NW local, and
     * its real virtual icon call sees the same captured Stack at130. */
    c->item_icon_error = calloc(1, sizeof(*c->item_icon_error));
    if (c->item_icon_error == NULL)
        return held_child_fail(c, "held Item icon diagnostic allocation", 130, false);
    c->failure.item_icon_error = c->item_icon_error;
    if (!mc_entity_render_item_icon(c->state.player, c->item_registry,
                                    (mc_item_stack *)c->state.stack, &c->state.item_icon,
                                    c->item_icon_error)) {
        const bool null =
            c->item_icon_error->source_exception && c->item_icon_error->exception_class != NULL &&
            strcmp(c->item_icon_error->exception_class, "java/lang/NullPointerException") == 0;
        (void)held_child_fail(c, c->item_icon_error->operation, 130, null);
        c->failure.native_policy = c->item_icon_error->native_policy;
        return false;
    }
    c->state.item_icon_returned = true;
    c->item_geometry = calloc(1, sizeof(*c->item_geometry));
    if (c->item_geometry == NULL)
        return held_child_fail(c, "held Item geometry diagnostic allocation", 133, false);
    c->failure.item_geometry = c->item_geometry;
    const bool returned = entity_renderer_held_bow_draw(NULL, c->state.item_icon, c->item_nw,
                                                        graphics(c), device(c), c->item_geometry);
    if (c->item_geometry->state.rescale_enabled)
        c->state.rescale_enabled = !c->item_geometry->state.rescale_disabled;
    if (!returned) {
        const mc_held_bow_error *e = c->item_geometry;
        c->failure.graphics = e->graphics;
        c->failure.display = e->display;
        const unsigned int pc = e->site.pc < 0 ? 133u : (unsigned int)e->site.pc;
        (void)held_child_fail(c, e->operation, pc, e->source_null_invocation);
        if (e->status == MC_HELD_BOW_GRAPHICS)
            c->failure.native_policy = e->graphics.native_policy;
        return false;
    }
    c->state.item_geometry_returned = true;
    c->state.faces = 66;
    c->state.vertices = 264;
    return true; /* The shared source fragment already popped RA's push0 at967. */
}

static bool held_block_child(mc_client_held_item_draw_call *c) {
    if (!gl(c, mc_client_display_source_push(device(c), &c->failure.display), 1074))
        return false;
    int32_t id;
    c->failure.stack = mc_item_stack_source_id_get(c->state.stack, &id);
    if (c->failure.stack != MC_ITEM_STACK_OK)
        return held_child_fail(c, "held Block actual Stack ID read", 4, false);
    if (id >= 256)
        return held_flat_child(c);
    mc_registry_block_m_view array;
    if (mc_registry_block_m_get(c->item_registry, &array, &c->failure.registry) != MC_REGISTRY_OK)
        return held_child_fail(c, "held Block actual UU.m active use", 13, false);
    c->failure.stack = mc_item_stack_source_id_get(c->state.stack, &id);
    if (c->failure.stack != MC_ITEM_STACK_OK)
        return held_child_fail(c, "held Block second Stack ID read", 17, false);
    const mc_block *block;
    if (mc_registry_block_m_at(&array, id, &block, &c->failure.registry) != MC_REGISTRY_OK)
        return held_child_fail(c, "held Block actual UU.m aaload", 20, false);
    if (block == NULL)
        return held_child_fail(c, "held Block render-type null receiver", 21, true);
    mc_block_render_description description;
    if (mc_block_render_describe(block, &description) != MC_REGISTRY_OK)
        return held_child_fail(c, "held Block actual render-type body", 21, false);
    if (!mc_cv_inventory_type_3d(description.type))
        return held_flat_child(c);
    client_application *a = app(c, 1074);
    if (a == NULL)
        return held_child_fail(c, "held Block App.p null receiver", 37, true);
    c->engine = a->render_engine;
    if (c->engine == NULL)
        return held_child_fail(c, "held Block Engine lookup null receiver", 42, true);
    if (mc_client_render_engine_retain(c->engine) != MC_CLIENT_RENDER_ENGINE_OK) {
        c->engine = NULL;
        return held_child_fail(c, "held Block actual Engine occurrence", 42, false);
    }
    static const uint16_t key[] = {'/', 't', 'e', 'r', 'r', 'a', 'i', 'n', '.', 'p', 'n', 'g'};
    const bool foreign = mc_client_texture_lookup_pending(c->engine);
    mc_client_texture texture;
    const mc_client_texture_lookup_status looked = mc_client_render_engine_texture(
        c->engine, device(c), NULL, (mc_texture_pack_text){key, sizeof key / sizeof *key, false},
        &texture, &c->download.lookup);
    c->failure.texture = &c->download.lookup;
    c->lookup_owned = !foreign && mc_client_texture_lookup_pending(c->engine);
    if (looked != MC_CLIENT_LOOKUP_OK)
        return held_child_fail(c, c->download.lookup.operation, 42,
                               c->download.lookup.status == MC_CLIENT_LOOKUP_NULL_REFERENCE);
    if (!gl(c, mc_client_display_source_bind_id(device(c), texture, &c->failure.display), 1074))
        return false;
    mc_client_render_blocks *fields = c->state.owner->source->e; /* Captured before arguments. */
    if (mc_registry_block_m_get(c->item_registry, &array, &c->failure.registry) != MC_REGISTRY_OK)
        return held_child_fail(c, "held inventory actual UU.m active use", 52, false);
    c->failure.stack = mc_item_stack_source_id_get(c->state.stack, &id);
    if (c->failure.stack != MC_ITEM_STACK_OK ||
        mc_registry_block_m_at(&array, id, &block, &c->failure.registry) != MC_REGISTRY_OK)
        return held_child_fail(c, "held inventory actual Block array element", 59, false);
    int32_t metadata;
    c->failure.stack = mc_item_stack_source_damage_get(c->state.stack, &metadata);
    if (c->failure.stack != MC_ITEM_STACK_OK)
        return held_child_fail(c, "held inventory actual Stack metadata", 61, false);
    c->item_brightness = calloc(1, sizeof(*c->item_brightness));
    if (c->item_brightness == NULL)
        return held_child_fail(c, "held inventory brightness diagnostic allocation", 66, false);
    c->failure.item_brightness = c->item_brightness;
    mc_entity_render_error_init(c->item_brightness);
    float brightness;
    if (!mc_entity_render_brightness(c->state.player, c->math, 1.0f, &brightness,
                                     c->item_brightness))
        return held_child_fail(
            c, c->item_brightness->operation, 66,
            c->item_brightness->source_exception && c->item_brightness->exception_class != NULL &&
                strcmp(c->item_brightness->exception_class, "java/lang/NullPointerException") == 0);
    if (fields == NULL)
        return held_child_fail(c, "held actual inventory CV null receiver", 69, true);
    c->inventory = calloc(1, sizeof(*c->inventory));
    if (c->inventory == NULL)
        return held_child_fail(c, "held inventory diagnostic allocation", 69, false);
    *c->inventory = (mc_cv_context){.fields = fields,
                                    .registry = c->item_registry,
                                    .models = c->state.owner->context.services->models,
                                    .graphics = graphics(c),
                                    .display = device(c)};
    c->failure.inventory = c->inventory;
    if (!mc_cv_inventory(c->inventory, block, metadata, brightness))
        return held_child_fail(c, c->inventory->failure.operation, 69,
                               c->inventory->failure.status ==
                                       MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE &&
                                   !c->inventory->failure.native_policy);
    c->state.block_item = c->state.block_inventory_returned = true;
    c->state.faces = (unsigned int)c->inventory->inventory_faces;
    c->state.vertices = c->state.faces * 4u;
    return gl(c, mc_client_display_source_pop(device(c), &c->failure.display), 1074);
}

static bool held_nonempty(mc_client_held_item_draw_call *c) {
    int32_t id;
    c->failure.stack = mc_item_stack_source_id_get(c->state.stack, &id);
    if (c->failure.stack != MC_ITEM_STACK_OK)
        return fail(c, MC_CLIENT_HELD_ITEM_DRAW_CHILD_FAILED, "held actual Map comparison Stack ID",
                    226, true);
    const mc_item *map;
    if (mc_registry_item_static_get(c->item_registry, "bb", &map, &c->failure.registry) !=
        MC_REGISTRY_OK)
        return fail(c, MC_CLIENT_HELD_ITEM_DRAW_CHILD_FAILED, "held actual GM.bb active use", 229,
                    true);
    if (map == NULL)
        return fail(c, MC_CLIENT_HELD_ITEM_DRAW_SOURCE_NULL, "held actual GM.bb ID receiver", 232,
                    false);
    const mc_item_fields *map_fields = mc_item_fields_get(map);
    if (map_fields == NULL)
        return fail(c, MC_CLIENT_HELD_ITEM_DRAW_CHILD_FAILED, "held actual Map Item storage", 232,
                    true);
    if (id == map_fields->bf)
        return fail(c, MC_CLIENT_HELD_ITEM_DRAW_DEPENDENCY, "held actual Map geometry", 238, true);
    if (!push(c, 863))
        return false;
    float s;
    if (!swing(c, c->state.player, 872, &s) || !math_use(c, 882))
        return false;
    const mc_math_trig *table = mc_math_classes_table(c->math);
    const float first = mc_math_sin(table, at_fmul(s, 3.1415927f));
    const float second = mc_math_sin(table, at_fmul(at_root(at_widen(s)), 3.1415927f));
    const float height =
        at_fmul(mc_math_sin(table, at_fmul(at_fmul(at_root(at_widen(s)), 3.1415927f), 2.0f)), 0.2f);
    if (!translate(c, at_fmul(at_fneg(second), 0.4f), height, at_fmul(at_fneg(first), 0.2f), 928) ||
        !translate(c, at_fmul(0.7f, 0.8f),
                   at_fsub(at_fmul(-0.65f, 0.8f), at_fmul(at_fsub(1.0f, c->state.equipped), 0.6f)),
                   at_fmul(-0.9f, 0.8f), 953) ||
        !rotate(c, 45, 0, 1, 0, 961) ||
        !gl(c, mc_client_display_source_rescale_normal(device(c), true, &c->failure.display), 966))
        return false;
    c->state.rescale_enabled = true;
    if (!swing(c, c->state.player, 971, &s) || !math_use(c, 984))
        return false;
    const float turn = mc_math_sin(table, at_fmul(at_fmul(s, s), 3.1415927f));
    const float tilt = mc_math_sin(table, at_fmul(at_root(at_widen(s)), 3.1415927f));
    if (!rotate(c, at_fmul(at_fneg(turn), 20), 0, 1, 0, 1011) ||
        !rotate(c, at_fmul(at_fneg(tilt), 20), 0, 0, 1, 1023) ||
        !rotate(c, at_fmul(at_fneg(tilt), 80), 1, 0, 0, 1035) ||
        !gl(c, mc_client_display_source_scale(device(c), 0.4f, 0.4f, 0.4f, &c->failure.display),
            1048))
        return false;
    const mc_item *item;
    if (!held_stack_item(c, 1053, 1053, 1053, &item))
        return false;
    if (item == NULL)
        return fail(c, MC_CLIENT_HELD_ITEM_DRAW_SOURCE_NULL,
                    "held actual rotate Item null receiver", 1056, false);
    bool rotated;
    c->failure.item = &c->item_error;
    if (!mc_item_render_rotate(c->item_registry, item, &rotated, &c->item_error))
        return fail(c, MC_CLIENT_HELD_ITEM_DRAW_DEPENDENCY, c->item_error.operation, 1056,
                    c->item_error.native_policy);
    c->state.item_rotate_returned = true;
    if (rotated && !rotate(c, 180, 0, 1, 0, 1067))
        return false;
    return held_block_child(c) && pop(c, 1077);
}
