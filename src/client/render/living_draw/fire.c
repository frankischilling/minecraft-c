#include "internal.h"

#include "block/registry/registry_internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/core/java/java.h"

static const uint16_t terrain_units[] = {'/', 't', 'e', 'r', 'r', 'a',
                                         'i', 'n', '.', 'p', 'n', 'g'};
static const mc_player_string terrain = {SIZE_MAX, sizeof(terrain_units) / sizeof(*terrain_units),
                                         terrain_units};

static void fire_at(ld_frame *f, int32_t pc) {
    ld_at(f, "bw", "a", "(Lsn;DDDF)V", pc);
}

static bool fire_registry(ld_frame *f, const mc_block **out) {
    /* This retained namespace association is not the nullable actual BW.b
     * field. Source does not read BW.b until its later texture helper. */
    mc_registry_classes *classes = f->renderer->parent->classes->registry;
    fire_at(f, 6);
    if (classes == NULL)
        return ld_fail(f, MC_CLIENT_LIVING_DRAW_STATE, "Fire actual Registry namespace");
    if (mc_registry_block_static_get(classes, "as", out, &f->error->detail.registry) !=
        MC_REGISTRY_OK) {
        f->error->selected_child = MC_CLIENT_LIVING_DRAW_CHILD_REGISTRY;
        ld_registry_classify(f, classes, &f->error->detail.registry);
        return ld_fail(f, MC_CLIENT_LIVING_DRAW_CHILD_FAILED, "Fire actual UU.as active use");
    }
    return true;
}

static bool initial_uv(ld_frame *f, mc_client_living_fire_state *s) {
    fire_at(f, 36);
    if (!ld_fp(f))
        return false;
    s->initial_uv[0] = at_fdiv((float)s->atlas_x, 256.0f);
    s->initial_uv[1] = at_fdiv(at_fadd((float)s->atlas_x, 15.99f), 256.0f);
    s->initial_uv[2] = at_fdiv((float)s->atlas_y, 256.0f);
    s->initial_uv[3] = at_fdiv(at_fadd((float)s->atlas_y, 15.99f), 256.0f);
    memcpy(s->uv, s->initial_uv, sizeof(s->uv));
    return true;
}

static bool layer_uv(ld_frame *f, mc_client_living_fire_state *s) {
    /* Java irem/idiv apply to the live wrapping loop counter. The row offset
     * belongs to V only; the atlas texture is not advanced or queried again. */
    const bool upper = s->layer % 2 != 0;
    fire_at(f, upper ? 250 : 209);
    if (!ld_fp(f))
        return false;
    s->uv[0] = at_fdiv((float)s->atlas_x, 256.0f);
    s->uv[1] = at_fdiv(at_fadd((float)s->atlas_x, 15.99f), 256.0f);
    const int32_t row = mc_java_i32_add(s->atlas_y, upper ? 16 : 0);
    s->uv[2] = at_fdiv((float)row, 256.0f);
    s->uv[3] = at_fdiv(at_fadd((float)row, 15.99f), 256.0f);
    if ((s->layer / 2) % 2 == 0) {
        const float temporary = s->uv[1];
        s->uv[1] = s->uv[0];
        s->uv[0] = temporary;
    }
    return true;
}

static bool fire_vertex(ld_frame *f, mc_client_tessellator *nw, mc_client_living_fire_state *s,
                        bool left, bool top, float u, float v, int32_t subtract_pc,
                        int32_t invoke_pc) {
    fire_at(f, subtract_pc);
    if (!ld_fp(f))
        return false;
    const double x = at_widen(at_fsub(left ? at_fneg(s->half_width) : s->half_width, 0.0f));
    const double y = at_widen(at_fsub(top ? 1.4f : 0.0f, s->y_offset));
    const double z = at_widen(s->depth), uu = at_widen(u), vv = at_widen(v);
    fire_at(f, invoke_pc);
    if (!ld_work(f))
        return false;
    s->last_vertex_arguments[0] = x;
    s->last_vertex_arguments[1] = y;
    s->last_vertex_arguments[2] = z;
    s->last_vertex_arguments[3] = uu;
    s->last_vertex_arguments[4] = vv;
    s->last_vertex_pc = invoke_pc;
    s->last_vertex_returned = false;
    ++s->vertex_attempts;
    if (!ld_graphics(f, mc_client_nw_vertex_uv(nw, x, y, z, uu, vv, &f->error->detail.graphics)))
        return false;
    s->last_vertex_returned = true;
    ++s->vertices;
    return true;
}

bool ld_fire(ld_frame *f, double x, double y, double z, float partial) {
    (void)partial; /* Actual last argument is evaluated by BW.b97 but never read here. */
    mc_client_living_draw_error *e = f->error;
    mc_client_living_fire_state *s = &e->state.fire;
    const mc_client_graphics_site parent = e->parent;
    e->parent = e->site; /* Actual enclosing BW.b97 invocation. */
    s->entered = true;
    s->receiver_identity = (uintptr_t)f->renderer;
    s->actor_identity = (uintptr_t)f->actor;
    fire_at(f, 3);
    if (!ld_capture(f, mc_client_draw_capture_graphics(f->scope, &e->detail.capture)))
        return false;
    if (!ld_display(f, mc_client_display_source_lighting_enable(f->scope->model.display, false,
                                                                &e->detail.display)))
        return false;
    s->lighting_disabled = true;
    const mc_block *fire = NULL;
    if (!fire_registry(f, &fire))
        return false;
    s->block_static_returned = true;
    s->block_identity = (uintptr_t)fire;
    fire_at(f, 9);
    if (fire == NULL)
        return ld_null(f, "Fire null UU.as texture receiver");
    s->texture = fire->fields.bm;
    s->texture_field_read = true;
    s->atlas_x = (int32_t)(((uint32_t)s->texture & 15u) << 4);
    s->atlas_y = (int32_t)((uint32_t)s->texture & 240u);
    if (!initial_uv(f, s))
        return false;
    fire_at(f, 69);
    if (!ld_display(f, mc_client_display_source_push(f->scope->model.display, &e->detail.display)))
        return false;
    s->matrix_pushed = true;
    fire_at(f, 73);
    if (!ld_fp(f))
        return false;
    const float xx = at_narrow(x), yy = at_narrow(y), zz = at_narrow(z);
    fire_at(f, 80);
    if (!ld_display(f, mc_client_display_source_translate(f->scope->model.display, xx, yy, zz,
                                                          &e->detail.display)))
        return false;
    s->position_translated = true;
    fire_at(f, 84);
    if (f->actor == NULL)
        return ld_null(f, "Fire null width Entity receiver");
    s->width = f->actor->base.width;
    fire_at(f, 89);
    if (!ld_fp(f))
        return false;
    s->scale = at_fmul(s->width, 1.4f);
    fire_at(f, 98);
    if (!ld_display(f, mc_client_display_source_scale(f->scope->model.display, at_widen(s->scale),
                                                      at_widen(s->scale), at_widen(s->scale),
                                                      &e->detail.display)))
        return false;
    s->scaled = true;
    fire_at(f, 104);
    if (!ld_texture(f, &terrain, 104))
        return false;
    s->texture_returned = true;
    fire_at(f, 107);
    mc_client_tessellator *nw = NULL;
    if (!ld_graphics(f, mc_client_nw_active_use(f->scope->model.graphics, e->site, &nw,
                                                &e->detail.graphics)))
        return false;
    s->nw_captured = true;
    s->nw_identity = (uintptr_t)nw;
    s->half_width = .5f;
    fire_at(f, 120);
    if (f->actor == NULL)
        return ld_null(f, "Fire null height Entity receiver");
    const float height = f->actor->base.height;
    s->height_read = true;
    fire_at(f, 125);
    if (!ld_fp(f))
        return false;
    s->initial_height = s->remaining_height = at_fdiv(height, s->scale);
    fire_at(f, 129);
    if (f->actor == NULL)
        return ld_null(f, "Fire null position Entity receiver");
    const double entity_y = f->actor->base.y;
    const mc_entity_bounds *bounds = f->actor->bounds;
    s->bounds_identity = (uintptr_t)bounds;
    fire_at(f, 136);
    if (bounds == NULL)
        return ld_null(f, "Fire null actual Entity bounds");
    const double minimum_y = bounds->min_y;
    s->bounds_read = true;
    fire_at(f, 139);
    if (!ld_fp(f))
        return false;
    s->initial_y_offset = s->y_offset = at_narrow(at_sub(entity_y, minimum_y));
    fire_at(f, 147);
    mc_client_render_manager *manager = ld_manager(f, 147);
    if (manager == NULL)
        return false;
    s->yaw_manager_identity = (uintptr_t)manager;
    s->view_yaw = manager->fields.i;
    fire_at(f, 154);
    if (!ld_display(f,
                    mc_client_display_source_rotate(f->scope->model.display, at_fneg(s->view_yaw),
                                                    0, 1, 0, &e->detail.display)))
        return false;
    s->rotated = true;
    fire_at(f, 163);
    if (!ld_fp(f))
        return false;
    const int32_t layers_for_offset = mc_java_float_to_i32(s->remaining_height);
    s->z_offset = at_fadd(-.3f, at_fmul((float)layers_for_offset, .02f));
    fire_at(f, 169);
    if (!ld_display(f, mc_client_display_source_translate(f->scope->model.display, 0, 0,
                                                          s->z_offset, &e->detail.display)))
        return false;
    s->depth_translated = true;
    fire_at(f, 176);
    if (!ld_display(f, mc_client_display_source_color(f->scope->model.display, 1, 1, 1, 1,
                                                      &e->detail.display)))
        return false;
    s->color_returned = true;
    s->depth = 0;
    s->layer = 0;
    fire_at(f, 187);
    s->begin_attempted = true;
    if (nw == NULL)
        return ld_null(f, "Fire null actual Tessellator begin invoke");
    if (!ld_graphics(f, mc_client_nw_begin_quads(nw, &e->detail.graphics)))
        return false;
    s->begin_returned = true;
    for (;;) {
        fire_at(f, 193);
        if (!ld_fp(f))
            return false;
        ++s->loop_tests;
        if (!(s->remaining_height > 0.0f))
            break;
        fire_at(f, 197);
        if (!ld_work(f) || !layer_uv(f, s))
            return false;
        if (!fire_vertex(f, nw, s, false, false, s->uv[1], s->uv[3], 316, 332) ||
            !fire_vertex(f, nw, s, true, false, s->uv[0], s->uv[3], 342, 358) ||
            !fire_vertex(f, nw, s, true, true, s->uv[0], s->uv[2], 368, 385) ||
            !fire_vertex(f, nw, s, false, true, s->uv[1], s->uv[2], 394, 411))
            return false;
        fire_at(f, 418);
        if (!ld_fp(f))
            return false;
        s->remaining_height = at_fsub(s->remaining_height, .45f);
        s->y_offset = at_fsub(s->y_offset, .45f);
        s->half_width = at_fmul(s->half_width, .9f);
        s->depth = at_fadd(s->depth, .03f);
        s->layer = mc_java_i32_add(s->layer, 1);
        ++s->layers;
    }
    fire_at(f, 450);
    mc_client_nw_fields fields;
    if (!mc_client_nw_fields_get(nw, &fields))
        return ld_fail(f, MC_CLIENT_LIVING_DRAW_STATE, "Fire actual NW receipt receiver");
    s->nw_words_identity = (uintptr_t)fields.g;
    s->nw_words_before_draw = fields.o;
    s->nw_vertices_before_draw = fields.h;
    s->nw_mode = fields.r;
    s->nw_writing_before_draw = fields.w;
    s->draw_attempted = true;
    if (!ld_graphics(f, mc_client_nw_draw(nw, f->scope->model.display, &e->detail.graphics)))
        return false;
    s->draw_returned = true;
    if (!mc_client_nw_fields_get(nw, &fields))
        return ld_fail(f, MC_CLIENT_LIVING_DRAW_STATE, "Fire returned NW receipt receiver");
    s->nw_writing_after_draw = fields.w;
    fire_at(f, 453);
    if (!ld_display(f, mc_client_display_source_pop(f->scope->model.display, &e->detail.display)))
        return false;
    s->matrix_popped = true;
    fire_at(f, 459);
    if (!ld_display(f, mc_client_display_source_lighting_enable(f->scope->model.display, true,
                                                                &e->detail.display)))
        return false;
    s->lighting_enabled = true;
    fire_at(f, 462);
    s->returned = true;
    e->parent = parent;
    return true;
}
