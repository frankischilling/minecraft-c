/* Synchronous source children of the SAME retained px.a(FJ) call. This file
 * neither creates another render owner nor supplies authored source inputs. */
static bool sel_fail(mc_client_world_frame_call *c, mc_world_frame_selection_work *s,
                     mc_client_world_frame_status status, const char *operation, const char *method,
                     const char *descriptor, unsigned int pc, bool native) {
    s->failure.operation = operation;
    s->failure.site = (mc_client_graphics_site){"n", method, descriptor, (int32_t)pc};
    s->failure.native_policy = native;
    if (s->outline)
        s->state.outline_pc = pc;
    else
        s->state.overlay_pc = pc;
    c->failure.selection = &s->failure;
    c->failure.status = status;
    c->failure.operation = operation;
    c->failure.site = s->failure.site;
    c->failure.child = s->failure.child.class_name == NULL ? s->failure.site : s->failure.child;
    c->failure.native_policy = native;
    c->state.source_pc = s->parent_pc;
    return false;
}

static bool sel_stop(mc_client_world_frame_call *c, mc_world_frame_selection_work *s,
                     mc_client_world_frame_status status, const char *operation, unsigned int pc,
                     bool native) {
    return sel_fail(c, s, status, operation, s->outline ? "b" : "a", "(Lgs;Lvf;ILiz;F)V", pc,
                    native);
}

static bool sel_fp(mc_client_world_frame_call *c, mc_world_frame_selection_work *s,
                   unsigned int pc) {
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u ||
           sel_stop(c, s, MC_CLIENT_WORLD_FRAME_UNSUPPORTED_FP,
                    "selection strict floating controls", pc, true);
}

static bool sel_gl(mc_client_world_frame_call *c, mc_world_frame_selection_work *s, bool ok,
                   unsigned int pc) {
    if (!ok)
        return sel_stop(c, s,
                        s->failure.display.status == MC_CLIENT_DISPLAY_UNSUPPORTED_FP
                            ? MC_CLIENT_WORLD_FRAME_UNSUPPORTED_FP
                            : MC_CLIENT_WORLD_FRAME_NATIVE_FAILED,
                        s->failure.display.operation, pc, true);
    if (s->outline)
        s->state.outline_pc = pc;
    else
        s->state.overlay_pc = pc;
    return true;
}

static bool sel_nw(mc_client_world_frame_call *c, mc_world_frame_selection_work *s, bool ok,
                   unsigned int pc) {
    if (ok)
        return true;
    s->failure.child = s->failure.graphics.site;
    return sel_stop(c, s, MC_CLIENT_WORLD_FRAME_CHILD_FAILED, s->failure.graphics.operation, pc,
                    s->failure.graphics.native_policy);
}

static bool sel_block(mc_client_world_frame_call *c, mc_world_frame_selection_work *s, int32_t id,
                      const mc_block **out, unsigned int pc) {
    const mc_registry_status status =
        mc_registry_block_get(c->state.app->registry, id, out, &s->failure.registry);
    return status == MC_REGISTRY_OK || sel_stop(c, s, MC_CLIENT_WORLD_FRAME_CHILD_FAILED,
                                                "selection actual Block.m array access", pc, true);
}

static bool sel_hit(mc_client_world_frame_call *c, mc_world_frame_selection_work *s,
                    unsigned int argument, mc_entity_hit_result_view *out, unsigned int pc) {
    if (s->hits[argument] == NULL)
        return sel_stop(c, s, MC_CLIENT_WORLD_FRAME_SOURCE_NULL,
                        "selection actual Hit field receiver", pc, false);
    const mc_entity_hit_status status = mc_entity_hit_result_inspect(s->hits[argument], out);
    return status == MC_ENTITY_HIT_OK || sel_stop(c, s, MC_CLIENT_WORLD_FRAME_NATIVE_FAILED,
                                                  "selection live Hit native admission", pc, true);
}

static bool sel_id(mc_client_world_frame_call *c, mc_world_frame_selection_work *s,
                   mc_entity_hit_result_view *hit, unsigned int pc) {
    mc_world_runtime *world = c->state.render_global->fields.k;
    if (world == NULL)
        return sel_stop(c, s, MC_CLIENT_WORLD_FRAME_SOURCE_NULL,
                        "selection actual World block receiver", pc, false);
    const mc_world_storage_status status = mc_world_runtime_block_get(
        world, hit->fields.b, hit->fields.c, hit->fields.d, &s->state.block_id, &s->failure.world);
    return status == MC_WORLD_STORAGE_OK ||
           sel_stop(c, s, MC_CLIENT_WORLD_FRAME_CHILD_FAILED, s->failure.world.operation, pc, true);
}

static bool sel_interpolation(mc_client_world_frame_call *c, mc_world_frame_selection_work *s,
                              unsigned int pc) {
    mc_entity *player = c->state.camera;
    if (player == NULL)
        return sel_stop(c, s, MC_CLIENT_WORLD_FRAME_SOURCE_NULL,
                        "selection actual Player coordinate receiver", pc, false);
    if (!sel_fp(c, s, pc + 12u))
        return false;
    /* The source reloads each previous field for subtraction. There is no
     * effectful child between these reads; preserve the exact arithmetic. */
    s->state.camera[0] = interpolate(player->base.bl, player->base.x, c->state.partial);
    s->state.camera[1] = interpolate(player->base.bm, player->base.y, c->state.partial);
    s->state.camera[2] = interpolate(player->base.bn, player->base.z, c->state.partial);
    return true;
}

static bool sel_texture(mc_client_world_frame_call *c, mc_world_frame_selection_work *s) {
    mc_client_render_global *n = c->state.render_global;
    mc_client_render_engine *engine = n->fields.l;
    if (engine == NULL)
        return sel_stop(c, s, MC_CLIENT_WORLD_FRAME_SOURCE_NULL,
                        "selection actual texture Engine receiver", 77, false);
    if (mc_client_render_engine_retain(engine) != MC_CLIENT_RENDER_ENGINE_OK)
        return sel_stop(c, s, MC_CLIENT_WORLD_FRAME_BUSY,
                        "selection texture Engine native occurrence", 77, true);
    s->engine = engine;
    static const uint16_t key[] = {'/', 't', 'e', 'r', 'r', 'a', 'i', 'n', '.', 'p', 'n', 'g'};
    const bool pending = mc_client_texture_lookup_pending(engine);
    mc_client_texture id;
    const mc_client_texture_lookup_status status = mc_client_render_engine_texture(
        engine, n->display, NULL, (mc_texture_pack_text){key, sizeof key / sizeof(*key), false},
        &id, &s->texture);
    s->lookup_owned = !pending && mc_client_texture_lookup_pending(engine);
    if (status != MC_CLIENT_LOOKUP_OK) {
        s->failure.texture = &s->texture;
        s->failure.child = s->texture.site;
        return sel_stop(c, s, MC_CLIENT_WORLD_FRAME_CHILD_FAILED, s->texture.operation, 77,
                        s->texture.native_policy);
    }
    if (!sel_gl(c, s, mc_client_display_source_bind_id(n->display, id, &s->failure.display), 87))
        return false;
    s->state.texture_bound = true;
    return true;
}

static bool sel_overlay(mc_client_world_frame_call *c, mc_world_frame_selection_work *s,
                        mc_math_classes *math) {
    mc_client_render_global *n = c->state.render_global;
    mc_client_display *d = n->display;
    if (!sel_nw(c, s,
                mc_client_nw_active_use(n->classes,
                                        (mc_client_graphics_site){"n", "a", "(Lgs;Lvf;ILiz;F)V", 0},
                                        &s->nw, &s->failure.graphics),
                0) ||
        !sel_gl(c, s, mc_client_display_source_blend_enable(d, true, &s->failure.display), 8) ||
        !sel_gl(c, s, mc_client_display_source_alpha_test(d, true, &s->failure.display), 14) ||
        !sel_gl(c, s, mc_client_display_source_blend_function(d, 770, 1, &s->failure.display), 21))
        return false;
    s->failure.clock = mc_platform_current_time_millis(&s->state.wall_clock);
    if (s->failure.clock != MC_PLATFORM_MILLIS_OK)
        return sel_stop(c, s, MC_CLIENT_WORLD_FRAME_NATIVE_FAILED,
                        "selection actual wall-clock query", 27, true);
    s->state.clock_sampled = true;
    if (!sel_fp(c, s, 30))
        return false;
    const float clock = _mm_cvtss_f32(_mm_cvtsi64_ss(_mm_setzero_ps(), s->state.wall_clock));
    const float angle = _mm_cvtss_f32(_mm_div_ss(_mm_set_ss(clock), _mm_set_ss(100.0f)));
    if (mc_math_classes_ensure(math, &s->failure.math) != MC_MATH_CLASS_OK)
        return sel_stop(c, s, MC_CLIENT_WORLD_FRAME_CHILD_FAILED, s->failure.math.operation, 34,
                        true);
    if (!sel_fp(c, s, 34))
        return false;
    const float sine = mc_math_sin(mc_math_classes_table(math), angle);
    const __m128 scaled = _mm_mul_ss(_mm_set_ss(sine), _mm_set_ss(0.2f));
    const __m128 shifted = _mm_add_ss(scaled, _mm_set_ss(0.4f));
    s->state.pulse_alpha = _mm_cvtss_f32(_mm_mul_ss(shifted, _mm_set_ss(0.5f)));
    if (!sel_gl(
            c, s,
            mc_client_display_source_color(d, 1, 1, 1, s->state.pulse_alpha, &s->failure.display),
            46))
        return false;
    s->state.progress = n->fields.i;
    if (!sel_fp(c, s, 58))
        return false;
    /* fcmpl58 yields -1 for NaN; ifle494 skips positive crack work. */
    if (n->fields.i > 0.0f) {
        if (!sel_gl(c, s, mc_client_display_source_blend_function(d, 774, 768, &s->failure.display),
                    68) ||
            !sel_texture(c, s) ||
            !sel_gl(c, s, mc_client_display_source_color(d, 1, 1, 1, 0.5f, &s->failure.display),
                    95) ||
            !sel_gl(c, s, mc_client_display_source_push(d, &s->failure.display), 98))
            return false;
        s->state.matrix_pushed = true;
        mc_entity_hit_result_view hit;
        if (!sel_hit(c, s, 0, &hit, 106) || !sel_id(c, s, &hit, 117))
            return false;
        const mc_block *block = NULL;
        if (s->state.block_id > 0 && !sel_block(c, s, s->state.block_id, &block, 132))
            return false;
        if (!sel_gl(c, s, mc_client_display_source_alpha_test(d, false, &s->failure.display),
                    142) ||
            !sel_gl(c, s, mc_client_display_source_polygon_offset(d, -3, -3, &s->failure.display),
                    149) ||
            !sel_gl(c, s,
                    mc_client_display_source_polygon_offset_fill(d, true, &s->failure.display),
                    154))
            return false;
        s->state.polygon_offset_enabled = true;
        if (!sel_interpolation(c, s, 158))
            return false;
        if (block == NULL && mc_registry_block_static_get(c->state.app->registry, "u", &block,
                                                          &s->failure.registry) != MC_REGISTRY_OK)
            return sel_stop(c, s, MC_CLIENT_WORLD_FRAME_CHILD_FAILED,
                            "selection actual fallback Block.u", 225, true);
        if (!sel_gl(c, s, mc_client_display_source_alpha_test(d, true, &s->failure.display), 230) ||
            !sel_nw(c, s, mc_client_nw_begin_quads(s->nw, &s->failure.graphics), 235) ||
            !sel_nw(c, s,
                    mc_client_nw_translation(s->nw, -s->state.camera[0], -s->state.camera[1],
                                             -s->state.camera[2], &s->failure.graphics),
                    249) ||
            !sel_nw(c, s, mc_client_nw_disable_color(s->nw, &s->failure.graphics), 254))
            return false;
        /* Capture the actual CV receiver before evaluating the remaining
         * operands, then invoke its texture-override body at287. */
        mc_client_render_blocks *cv = n->fields.u;
        s->state.crack_renderer = cv;
        s->state.crack_block = block;
        if (!sel_hit(c, s, 0, &hit, 264))
            return false;
        s->state.x = hit.fields.b;
        s->state.y = hit.fields.c;
        s->state.z = hit.fields.d;
        if (!sel_fp(c, s, 284))
            return false;
        s->state.crack_texture =
            mc_java_i32_add(240, mc_java_float_to_i32(_mm_cvtss_f32(
                                     _mm_mul_ss(_mm_set_ss(n->fields.i), _mm_set_ss(10.0f)))));
        if (cv == NULL)
            return sel_stop(c, s, MC_CLIENT_WORLD_FRAME_SOURCE_NULL, "selection actual CV receiver",
                            287, false);
        s->cv = calloc(1, sizeof(*s->cv));
        if (s->cv == NULL)
            return sel_stop(c, s, MC_CLIENT_WORLD_FRAME_ALLOCATION,
                            "selection retained CV World workspace", 287, true);
        s->state.crack_context = s->cv;
        const mc_client_render_global_status captured =
            mc_render_global_cv_capture_world(n, cv, &s->cv_world, &s->cv_owner);
        if (captured != MC_CLIENT_RENDER_GLOBAL_OK) {
            s->failure.cv_owner = &s->cv_owner;
            return sel_stop(c, s,
                            captured == MC_CLIENT_RENDER_GLOBAL_BUSY ? MC_CLIENT_WORLD_FRAME_BUSY
                            : captured == MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY
                                ? MC_CLIENT_WORLD_FRAME_DEPENDENCY
                                : MC_CLIENT_WORLD_FRAME_NATIVE_FAILED,
                            s->cv_owner.operation, 287, true);
        }
        /* Native code-owner bindings belong to the same admitted App/PX/N
         * namespace. CV.c itself stays untouched and is reread at each XP call. */
        s->cv->fields = cv;
        s->cv->world = s->cv_world;
        s->cv->registry = c->state.app->registry;
        s->cv->models = c->state.owner->services.models;
        s->cv->graphics = n->classes;
        s->cv->application = c->state.app;
        s->cv->tessellator = s->nw;
        if (!mc_cv_texture_override(s->cv, block, s->state.x, s->state.y, s->state.z,
                                    s->state.crack_texture)) {
            s->failure.cv = s->cv;
            const mc_client_render_global_error *e = &s->cv->failure;
            s->failure.child =
                e->child.class_name != NULL
                    ? e->child
                    : (mc_client_graphics_site){e->source_class, e->source_method,
                                                e->source_descriptor, (int32_t)e->source_pc};
            return sel_stop(c, s,
                            e->status == MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY
                                ? MC_CLIENT_WORLD_FRAME_DEPENDENCY
                            : e->status == MC_CLIENT_RENDER_GLOBAL_UNSUPPORTED_FP
                                ? MC_CLIENT_WORLD_FRAME_UNSUPPORTED_FP
                            : e->status == MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE
                                ? MC_CLIENT_WORLD_FRAME_SOURCE_NULL
                                : MC_CLIENT_WORLD_FRAME_CHILD_FAILED,
                            e->operation, 287, e->native_policy);
        }
        s->state.crack_cv_returned = true;
        if (!sel_nw(c, s, mc_client_nw_draw(s->nw, d, &s->failure.graphics), 292))
            return false;
        s->state.crack_draw_returned = true;
        if (!sel_nw(c, s, mc_client_nw_translation(s->nw, 0, 0, 0, &s->failure.graphics), 300) ||
            !sel_gl(c, s, mc_client_display_source_alpha_test(d, false, &s->failure.display),
                    306) ||
            !sel_gl(c, s, mc_client_display_source_polygon_offset(d, 0, 0, &s->failure.display),
                    311) ||
            !sel_gl(c, s,
                    mc_client_display_source_polygon_offset_fill(d, false, &s->failure.display),
                    316))
            return false;
        s->state.polygon_offset_enabled = false;
        if (!sel_gl(c, s, mc_client_display_source_alpha_test(d, true, &s->failure.display), 322) ||
            !sel_gl(c, s, mc_client_display_source_depth_mask(d, true, &s->failure.display), 326) ||
            !sel_gl(c, s, mc_client_display_source_pop(d, &s->failure.display), 329))
            return false;
        s->state.matrix_pushed = false;
        s->state.crack_state_restored = true;
    }
    if (!sel_gl(c, s, mc_client_display_source_blend_enable(d, false, &s->failure.display), 497) ||
        !sel_gl(c, s, mc_client_display_source_alpha_test(d, false, &s->failure.display), 503))
        return false;
    s->state.overlay_pc = 506;
    s->state.overlay_returned = true;
    return true;
}

static void sel_capture_outline(mc_client_world_frame_call *c, mc_world_frame_selection_work *s,
                                unsigned int index) {
    mc_client_selection_outline_batch *b = &s->state.outline_batches[index];
    mc_client_nw_fields fields;
    if (mc_client_nw_fields_get(s->nw, &fields)) {
        b->nw = s->nw;
        b->mode = fields.r;
        b->translation[0] = fields.s;
        b->translation[1] = fields.t;
        b->translation[2] = fields.u;
        b->texture_coordinates = fields.m;
        b->vertex_colors = fields.l;
        b->normals = fields.n;
        b->color_disabled = fields.q;
        const mc_client_direct_byte_buffer *bytes = fields.d;
        const mc_client_direct_int_buffer *words = fields.e;
        if (bytes != NULL && words != NULL && words->bytes == bytes &&
            words->position <= words->limit && words->limit <= words->capacity &&
            words->position <= 64u && words->position % 8u == 0 &&
            words->position <= bytes->capacity / sizeof(uint32_t) &&
            bytes->position <= bytes->limit && bytes->limit <= bytes->capacity &&
            (bytes->data != NULL || words->position == 0)) {
            b->byte_position = bytes->position;
            b->byte_limit = bytes->limit;
            b->byte_capacity = bytes->capacity;
            b->words = words->position;
            b->vertices = b->words / 8u;
            b->submitted_bytes = b->words * sizeof(uint32_t);
            if (b->submitted_bytes != 0)
                memcpy(b->raw_words, bytes->data, b->submitted_bytes);
            b->captured = true;
        }
    }
    mc_client_source_draw_state draw;
    if (mc_client_display_source_draw_inspect(c->state.render_global->display, &draw)) {
        memcpy(b->color, draw.color, sizeof(b->color));
        b->line_width = draw.line_width;
        b->color_known = draw.color_known;
        b->texture_enabled = draw.texture_enabled;
        b->alpha_test = draw.alpha_test;
        b->lighting_enabled = draw.lighting_enabled;
        b->depth_mask_known = draw.depth_mask_known;
        b->requested_depth_mask = draw.requested_depth_mask;
        b->blend_known = draw.blend_known;
        b->requested_blend_enabled = draw.requested_blend_enabled;
        b->blend_function_known = draw.blend_function_known;
        b->requested_blend_source = draw.requested_blend_source;
        b->requested_blend_destination = draw.requested_blend_destination;
        b->draw_state_captured = true;
    }
    mc_client_source_fog_state fog;
    if (mc_client_display_source_fog_inspect(c->state.render_global->display, &fog)) {
        b->fog_enabled = fog.enabled;
        b->fog_captured = true;
    }
}

static void sel_pool_cursor(mc_world_frame_selection_work *s, unsigned int index) {
    mc_entity_class_snapshot snapshot;
    if (mc_entity_classes_get_snapshot(s->pool_classes, &snapshot) == MC_ENTITY_OK) {
        s->state.bounds_cursor[index] = snapshot.bounds_pool_cursor;
        s->state.bounds_cursor_captured[index] = true;
    }
}

static bool sel_lines(mc_client_world_frame_call *c, mc_world_frame_selection_work *s) {
    mc_client_render_global *n = c->state.render_global;
    if (!sel_nw(c, s,
                mc_client_nw_active_use(n->classes,
                                        (mc_client_graphics_site){"n", "a", "(Leq;)V", 0}, &s->nw,
                                        &s->failure.graphics),
                216))
        return false;
    /* Each coordinate is read from the same live pooled EQ at its source
     * vertex call. First bottom, then top, then four vertical edge pairs. */
    static const unsigned char points[18][3] = {
        {0, 0, 0}, {1, 0, 0}, {1, 0, 1}, {0, 0, 1}, {0, 0, 0}, {0, 1, 0},
        {1, 1, 0}, {1, 1, 1}, {0, 1, 1}, {0, 1, 0}, {0, 0, 0}, {0, 1, 0},
        {1, 0, 0}, {1, 1, 0}, {1, 0, 1}, {1, 1, 1}, {0, 0, 1}, {0, 1, 1}};
    static const unsigned int vertex_pc[18] = {22,  38,  54,  70,  86,  114, 130, 146, 162,
                                               178, 200, 216, 232, 248, 264, 280, 296, 312};
    static const unsigned int begin_pc[3] = {6, 96, 184}, draw_pc[3] = {90, 180, 316};
    static const unsigned int end[3] = {5, 10, 18};
    unsigned int index = 0;
    for (unsigned int batch = 0; batch < 3; ++batch) {
        if (!mc_client_nw_begin(s->nw, batch == 2 ? 1 : 3, &s->failure.graphics)) {
            s->failure.child = s->failure.graphics.site;
            return sel_fail(c, s, MC_CLIENT_WORLD_FRAME_CHILD_FAILED, s->failure.graphics.operation,
                            "a", "(Leq;)V", begin_pc[batch], s->failure.graphics.native_policy);
        }
        for (; index < end[batch]; ++index) {
            const mc_entity_bounds *box = s->boxes[2];
            if (!mc_client_nw_vertex(s->nw, points[index][0] ? box->max_x : box->min_x,
                                     points[index][1] ? box->max_y : box->min_y,
                                     points[index][2] ? box->max_z : box->min_z,
                                     &s->failure.graphics)) {
                s->failure.child = s->failure.graphics.site;
                return sel_fail(c, s, MC_CLIENT_WORLD_FRAME_CHILD_FAILED,
                                s->failure.graphics.operation, "a", "(Leq;)V", vertex_pc[index],
                                s->failure.graphics.native_policy);
            }
            ++s->state.vertices;
        }
        if (!mc_client_nw_draw(s->nw, n->display, &s->failure.graphics)) {
            s->failure.child = s->failure.graphics.site;
            return sel_fail(c, s, MC_CLIENT_WORLD_FRAME_CHILD_FAILED, s->failure.graphics.operation,
                            "a", "(Leq;)V", draw_pc[batch], s->failure.graphics.native_policy);
        }
        /* Passive native observation only: no GL/clock/source call, and its
         * availability never changes this source method's return behavior. */
        sel_capture_outline(c, s, batch);
        ++s->state.draws;
    }
    return true;
}

static bool sel_selected_box(mc_client_world_frame_call *c, mc_world_frame_selection_work *s,
                             const mc_block *block) {
    s->pool_classes = mc_world_runtime_entities(c->state.world);
    s->selected = calloc(1, sizeof(*s->selected));
    if (s->selected == NULL)
        return sel_stop(c, s, MC_CLIENT_WORLD_FRAME_ALLOCATION,
                        "selection full selected-box diagnostic allocation", 189, true);
    s->state.selected_getter = s->selected;
    sel_pool_cursor(s, 0);
    const mc_world_storage_status selected_status =
        mc_world_collision_block_selected_box_in_classes_held(
            c->state.world->classes, c->state.render_global->fields.k, block, s->state.x,
            s->state.y, s->state.z, &s->boxes[0], s->selected);
    sel_pool_cursor(s, 1);
    if (selected_status != MC_WORLD_STORAGE_OK) {
        s->failure.selected = s->selected;
        if (s->selected->frame_count != 0) {
            const mc_world_selected_box_frame *f =
                &s->selected->frames[s->selected->frame_count - 1];
            s->failure.child = (mc_client_graphics_site){f->declaring_class, "f", "(Lfd;III)Leq;",
                                                         (int32_t)f->bytecode};
        }
        return sel_stop(c, s,
                        s->selected->source_null ? MC_CLIENT_WORLD_FRAME_SOURCE_NULL
                                                 : MC_CLIENT_WORLD_FRAME_CHILD_FAILED,
                        s->selected->operation, 189, s->selected->native_policy);
    }
    s->state.selected = s->boxes[0];
    s->state.selected_returned = true;
    if (s->boxes[0] == NULL)
        return sel_stop(c, s, MC_CLIENT_WORLD_FRAME_SOURCE_NULL,
                        "actual selected EQ expand null receiver", 201, false);
    const double epsilon = (double)0.002f;
    mc_entity_status status = mc_entity_bounds_expand(
        s->pool_classes, s->boxes[0], epsilon, epsilon, epsilon, &s->boxes[1], &s->failure.bounds);
    sel_pool_cursor(s, 2);
    if (status != MC_ENTITY_OK) {
        s->failure.native_entity = status;
        return sel_stop(c, s, MC_CLIENT_WORLD_FRAME_CHILD_FAILED, "selection actual EQ expand", 201,
                        true);
    }
    s->state.expanded = s->boxes[1];
    s->state.expanded_returned = true;
    status = mc_entity_bounds_shift(s->pool_classes, s->boxes[1], -s->state.camera[0],
                                    -s->state.camera[1], -s->state.camera[2], &s->boxes[2],
                                    &s->failure.bounds);
    sel_pool_cursor(s, 3);
    if (status != MC_ENTITY_OK) {
        s->failure.native_entity = status;
        return sel_stop(c, s, MC_CLIENT_WORLD_FRAME_CHILD_FAILED, "selection actual EQ offset", 213,
                        true);
    }
    s->state.shifted = s->boxes[2];
    s->state.shifted_returned = true;
    return sel_lines(c, s);
}

static bool sel_outline(mc_client_world_frame_call *c, mc_world_frame_selection_work *s) {
    mc_client_render_global *n = c->state.render_global;
    mc_client_display *d = n->display;
    mc_entity_hit_result_view hit;
    if (!sel_hit(c, s, 1, &hit, 5))
        return false;
    mc_entity_hit_kind_class_view kinds;
    if (mc_entity_hit_kind_class_inspect(c->state.camera->classes, &kinds) != MC_ENTITY_HIT_OK ||
        kinds.phase != MC_ENTITY_CLASS_READY)
        return sel_stop(c, s, MC_CLIENT_WORLD_FRAME_NATIVE_FAILED,
                        "selection actual HitKind namespace", 8, true);
    if (hit.fields.a != kinds.a) {
        s->state.outline_pc = 235;
        s->state.outline_returned = true;
        return true;
    }
    s->state.outline_block = true;
    if (!sel_gl(c, s, mc_client_display_source_blend_enable(d, true, &s->failure.display), 17) ||
        !sel_gl(c, s, mc_client_display_source_blend_function(d, 770, 771, &s->failure.display),
                26) ||
        !sel_gl(c, s, mc_client_display_source_color(d, 0, 0, 0, 0.4f, &s->failure.display), 34) ||
        !sel_gl(c, s, mc_client_display_source_line_width(d, 2, &s->failure.display), 38) ||
        !sel_gl(c, s, mc_client_display_source_texture_enable(d, false, &s->failure.display), 44) ||
        !sel_gl(c, s, mc_client_display_source_depth_mask(d, false, &s->failure.display), 48) ||
        !sel_id(c, s, &hit, 71))
        return false;
    if (s->state.block_id > 0) {
        const mc_block *block;
        if (!sel_block(c, s, s->state.block_id, &block, 86))
            return false;
        if (block == NULL)
            return sel_stop(c, s, MC_CLIENT_WORLD_FRAME_SOURCE_NULL,
                            "selection actual Block state-shape receiver", 103, false);
        s->state.x = hit.fields.b;
        s->state.y = hit.fields.c;
        s->state.z = hit.fields.d;
        s->shape = calloc(1, sizeof(*s->shape));
        if (s->shape == NULL)
            return sel_stop(c, s, MC_CLIENT_WORLD_FRAME_ALLOCATION,
                            "selection full state-shape diagnostic allocation", 103, true);
        mc_world_collision_error_init(s->shape);
        mc_world_runtime *world = n->fields.k;
        /* This is the actual virtual Block.a(World as XP), not a renderer
         * callback or collision-box substitution. The existing explicit-code
         * helper enters a nullable World only at a reached source query. */
        const mc_world_storage_status status = mc_world_collision_block_state_shape_in_classes_held(
            c->state.world->classes, world, block, hit.fields.b, hit.fields.c, hit.fields.d,
            s->shape);
        if (status != MC_WORLD_STORAGE_OK) {
            s->failure.shape = s->shape;
            s->failure.child = (mc_client_graphics_site){s->shape->block_class, "a", "(Lxp;III)V",
                                                         (int32_t)s->shape->block_bytecode};
            return sel_stop(c, s, MC_CLIENT_WORLD_FRAME_CHILD_FAILED,
                            s->shape->nearby.query.operation, 103, true);
        }
        s->state.shape_returned = true;
        if (!sel_interpolation(c, s, 107))
            return false;
        /* Fresh m[id] at169..172 follows the Player arithmetic; source may
         * have changed statics during the earlier real child. */
        if (!sel_block(c, s, s->state.block_id, &block, 172))
            return false;
        if (block == NULL)
            return sel_stop(c, s, MC_CLIENT_WORLD_FRAME_SOURCE_NULL,
                            "selection actual Block selected-box receiver", 189, false);
        if (!sel_hit(c, s, 1, &hit, 179))
            return false;
        s->state.x = hit.fields.b;
        s->state.y = hit.fields.c;
        s->state.z = hit.fields.d;
        if (!sel_selected_box(c, s, block))
            return false;
    }
    if (!sel_gl(c, s, mc_client_display_source_depth_mask(d, true, &s->failure.display), 220) ||
        !sel_gl(c, s, mc_client_display_source_texture_enable(d, true, &s->failure.display), 226) ||
        !sel_gl(c, s, mc_client_display_source_blend_enable(d, false, &s->failure.display), 232))
        return false;
    s->state.outline_pc = 235;
    s->state.outline_returned = true;
    return true;
}

static bool sel_invoke(mc_client_world_frame_call *c, mc_world_frame_selection_work *s,
                       mc_math_classes *math, bool outline) {
    mc_client_render_global *n = c->state.render_global;
    s->outline = outline;
    mc_client_render_global_world_state attachment = {0};
    const bool attached = n->world_call == NULL ||
                          (mc_client_render_global_world_inspect(n->world_call, &attachment) &&
                           !attachment.active && attachment.source_returned);
    if (n->active || n->world_retiring || !attached || n->clouds_call != NULL ||
        n->sky_call != NULL || n->entities_call != NULL || n->cull_call != NULL ||
        n->update_call != NULL || n->submit_call != NULL ||
        n->phase == MC_CLIENT_RENDER_GLOBAL_CONSTRUCTING ||
        n->phase == MC_CLIENT_RENDER_GLOBAL_RETIRING)
        return sel_stop(c, s, MC_CLIENT_WORLD_FRAME_BUSY,
                        "selection entered/pending/retiring N native admission", 0, true);
    n->active = true;
    const bool returned = outline ? sel_outline(c, s) : sel_overlay(c, s, math);
    n->active = false;
    return returned;
}

static bool selection_retire_all(mc_client_world_frame_call *c,
                                 mc_client_world_frame_error *error) {
    for (size_t count = c->state.selection_calls; count != 0; --count) {
        mc_world_frame_selection_work *s = &c->selections[count - 1];
        const char *operation = NULL;
        if (s->engine != NULL) {
            if (s->lookup_owned && mc_client_render_engine_retire_lookup(
                                       s->engine, &s->texture_cleanup) != MC_CLIENT_LOOKUP_OK) {
                operation = "selection owned texture lookup native retirement";
                s->cleanup.texture = &s->texture_cleanup;
            } else {
                s->lookup_owned = false;
                if (mc_client_render_engine_release(s->engine) != MC_CLIENT_RENDER_ENGINE_OK)
                    operation = "selection Engine native occurrence retirement";
                else
                    s->engine = NULL;
            }
        }
        for (size_t i = 3; operation == NULL && i != 0; --i) {
            if (s->boxes[i - 1] == NULL)
                continue;
            const mc_entity_status status =
                mc_entity_bounds_release(s->pool_classes, s->boxes[i - 1]);
            if (status != MC_ENTITY_OK) {
                s->cleanup.native_entity = status;
                operation = "selection pooled bounds native occurrence retirement";
                break;
            }
            s->boxes[i - 1] = NULL;
            if (i == 3)
                s->state.shifted = NULL;
            if (i == 2)
                s->state.expanded = NULL;
            if (i == 1)
                s->state.selected = NULL;
        }
        for (size_t i = 2; operation == NULL && i != 0; --i) {
            if (mc_entity_hit_result_release(&s->hits[i - 1], &s->cleanup.hit) !=
                MC_ENTITY_HIT_OK) {
                operation = "selection Hit native occurrence retirement";
                break;
            }
            mc_item_stack_release(s->stacks[i - 1]);
            s->stacks[i - 1] = NULL;
            if (i == 2) {
                s->state.outline_hit = NULL;
                s->state.outline_stack = NULL;
            } else {
                s->state.overlay_hit = NULL;
                s->state.overlay_stack = NULL;
            }
        }
        if (operation != NULL) {
            s->cleanup.operation = operation;
            s->cleanup.native_policy = true;
            if (error != NULL)
                *error = (mc_client_world_frame_error){.status = MC_CLIENT_WORLD_FRAME_BUSY,
                                                       .operation = operation,
                                                       .native_policy = true,
                                                       .selection = &s->cleanup};
            return false;
        }
        free(s->cv);
        s->cv = NULL;
        s->failure.cv = NULL;
        s->state.crack_context = NULL;
        if (s->cv_world != NULL) {
            mc_world_runtime_binding_release(s->cv_world);
            s->cv_world = NULL;
        }
        free(s->shape);
        s->shape = NULL;
        s->failure.shape = NULL;
        free(s->selected);
        s->selected = NULL;
        s->failure.selected = NULL;
        s->state.selected_getter = NULL;
    }
    c->state.selection_hit = NULL;
    c->state.selection_stack = NULL;
    return true;
}
