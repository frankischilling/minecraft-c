#include "source_terrain_internal.h"

#include "../../entity/player/tick_math_internal.h"
#include "../application_state.h"
#include "../display/fog_surface.h"
#include "../display/occlusion.h"
#include "../display/source_surface.h"
#include "../display/startup_surface.h"
#include "../display/world_frame_surface.h"
#include "../render/model_graph_internal.h"
#include "../render_engine/ge.h"
#include "mc/client/world/entity_renderer.h"

#include <limits.h>

static mc_client_graphics_site site(const mc_client_render_global_submit_call *c, unsigned int pc) {
    mc_client_graphics_site result = c->frames[c->depth - 1];
    result.pc = (int32_t)pc;
    return result;
}

static bool cell(mc_client_render_global_submit_call *c, int32_t index, unsigned int array_pc,
                 unsigned int field_pc, mc_client_source_dk **out) {
    if (!mc_terrain_array(c, true, index, array_pc, out))
        return false;
    return *out != NULL || mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                                           "actual N sorted DK field receiver", field_pc, false);
}

static bool sorted_length(mc_client_render_global_submit_call *c, unsigned int pc, int32_t *out) {
    const mc_client_render_global *n = c->state.owner;
    if (n->fields.n == NULL)
        return mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                               "actual N sorted array length receiver", pc, false);
    if (n->n_length > INT32_MAX)
        return mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE,
                               "N sorted array native length", pc, true);
    *out = (int32_t)n->n_length;
    return true;
}

static bool clear_result(mc_client_render_global_submit_call *c, unsigned int pc) {
    mc_terrain_pc(c, pc);
    return mc_terrain_graphics(
        c, mc_client_ge_int_clear(c->state.owner->fields.c, site(c, pc), &c->failure.graphics), pc);
}

static bool get_result(mc_client_render_global_submit_call *c, int32_t id, uint32_t pname,
                       unsigned int pc) {
    void *storage = NULL;
    size_t remaining = 0;
    mc_terrain_pc(c, pc);
    if (!mc_terrain_graphics(c,
                             mc_client_ge_int_writable_remaining(c->state.owner->fields.c, &storage,
                                                                 &remaining, site(c, pc),
                                                                 &c->failure.graphics),
                             pc))
        return false;
    const bool ok =
        mc_client_display_source_query_result(c->state.owner->display, c->state.owner->queries, id,
                                              pname, storage, remaining, &c->failure.display);
    /* The distinct rejection occurs only after actual context, Names/ID,
     * function and pointer admission. This is LWJGL's reached checkBuffer1,
     * rather than an earlier native provenance failure with the same span. */
    if (!ok && remaining == 0 && c->failure.display.status == MC_CLIENT_DISPLAY_BUFFER_TOO_SMALL)
        return mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_SOURCE_ILLEGAL_ARGUMENT,
                               "actual LWJGL query-result IntBuffer minimum1", pc, false);
    return mc_terrain_display(c, ok, pc);
}

static bool read_result(mc_client_render_global_submit_call *c, unsigned int pc, int32_t *out) {
    mc_terrain_pc(c, pc);
    return mc_terrain_graphics(
        c,
        mc_client_ge_int_get(c->state.owner->fields.c, 0, out, site(c, pc), &c->failure.graphics),
        pc);
}

bool mc_terrain_poll(mc_client_render_global_submit_call *c, int32_t start, int32_t end,
                     unsigned int caller_pc) {
    mc_terrain_pc(c, caller_pc);
    if (!mc_terrain_push(c, "n", "a", "(II)V"))
        return false;
    for (int32_t index = start; index < end; index = mc_java_i32_add(index, 1)) {
        c->state.index = index;
        mc_client_source_dk *dk;
        if (!cell(c, index, 12, 13, &dk))
            return false;
        if (!dk->fields.y)
            continue;
        if (!clear_result(c, 23) || !cell(c, index, 32, 33, &dk) ||
            !get_result(c, dk->fields.z, UINT32_C(34919), 42))
            return false;
        ++c->state.query_polls;
        int32_t available;
        if (!read_result(c, 50, &available))
            return false;
        if (available == 0)
            continue;
        if (!cell(c, index, 61, 63, &dk))
            return false;
        dk->fields.y = false;
        mc_terrain_store(c, 63);
        if (!clear_result(c, 70) || !cell(c, index, 79, 80, &dk) ||
            !get_result(c, dk->fields.z, UINT32_C(34918), 89))
            return false;
        /* The DK receiver is evaluated before the fresh N.c/get0 operand. */
        if (!mc_terrain_array(c, true, index, 97, &dk))
            return false;
        int32_t samples;
        if (!read_result(c, 103, &samples) || !mc_terrain_dk_receiver(c, dk, 114))
            return false;
        dk->fields.x = samples != 0;
        mc_terrain_store(c, 114);
    }
    mc_terrain_pc(c, 123);
    mc_terrain_pop(c);
    return true;
}

static bool begin_query(mc_client_render_global_submit_call *c, int32_t id) {
    mc_terrain_pc(c, 743);
    const bool ok = mc_client_display_source_begin_query(
        c->state.owner->display, c->state.owner->queries, id, &c->query, &c->failure.display);
    /* A native pending/active occurrence survives even when the wrapper
     * cannot return normally. It blocks deletion until checked retirement. */
    mc_client_display_active_query_state state;
    c->state.query_active = c->query != NULL &&
                            mc_client_display_active_query_inspect(c->query, &state) &&
                            state.begin_issued && !state.source_ended && !state.native_aborted;
    if (!mc_terrain_display(c, ok, 743))
        return false;
    ++c->state.queries_issued;
    return true;
}

static bool end_query(mc_client_render_global_submit_call *c) {
    mc_terrain_pc(c, 758);
    if (!mc_terrain_display(c, mc_client_display_source_end_query(c->query, &c->failure.display),
                            758))
        return false;
    c->state.query_active = false;
    return true;
}

static bool ended_query_release(mc_client_render_global_submit_call *c) {
    if (mc_client_display_active_query_retire(&c->query, &c->cleanup.display))
        return true;
    c->cleanup.status = MC_CLIENT_RENDER_GLOBAL_CLEANUP_FAILED;
    c->cleanup.operation = c->cleanup.display.operation;
    c->cleanup.native_policy = true;
    c->failure.cleanup_display = c->cleanup.display;
    return mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_CLEANUP_FAILED, c->cleanup.display.operation,
                           769, true);
}

static bool interval(mc_client_render_global_submit_call *c, mc_client_source_dk *dk, int32_t index,
                     bool *scheduled) {
    if (!mc_terrain_distance(c, dk, 581, &c->state.distance) || !mc_terrain_math(c, 584))
        return false;
    mc_terrain_pc(c, 584);
    if (!mc_terrain_push(c, "in", "c", "(F)F") || !mc_terrain_fp(c, 1))
        return false;
    c->state.distance = at_root(at_widen(c->state.distance));
    mc_terrain_pc(c, 6);
    mc_terrain_pop(c);
    if (!mc_terrain_fp(c, 594))
        return false;
    c->state.interval = mc_java_float_to_i32(at_fadd(1.0f, at_fdiv(c->state.distance, 128.0f)));
    mc_terrain_pc(c, 596);
    int32_t tick_remainder, index_remainder;
    if (!mc_java_i32_rem(c->state.owner->fields.x, c->state.interval, &tick_remainder))
        return mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_SOURCE_ARITHMETIC,
                               "actual N query scheduling remainder by zero", 605, false);
    if (!mc_java_i32_rem(index, c->state.interval, &index_remainder))
        return mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_SOURCE_ARITHMETIC,
                               "actual N query index remainder by zero", 610, false);
    *scheduled = tick_remainder == index_remainder;
    return true;
}

static bool translate(mc_client_render_global_submit_call *c, mc_client_source_dk *dk) {
    if (!mc_terrain_fp(c, 631))
        return false;
    const float x = at_narrow(at_sub((double)dk->fields.i, c->state.render_x));
    const float y = at_narrow(at_sub((double)dk->fields.j, c->state.render_y));
    const float z = at_narrow(at_sub((double)dk->fields.k, c->state.render_z));
    const float dx = at_fsub(x, c->state.translate_x);
    const float dy = at_fsub(y, c->state.translate_y);
    const float dz = at_fsub(z, c->state.translate_z);
    /* Java fcmpl makes NaN take the nonzero branch, as does != here. */
    if (dx != 0.0f || dy != 0.0f || dz != 0.0f) {
        mc_terrain_pc(c, 707);
        if (!mc_terrain_display(c,
                                mc_client_display_source_translate(c->state.owner->display, dx, dy,
                                                                   dz, &c->failure.display),
                                707) ||
            !mc_terrain_fp(c, 714))
            return false;
        c->state.translate_x = at_fadd(c->state.translate_x, dx);
        c->state.translate_y = at_fadd(c->state.translate_y, dy);
        c->state.translate_z = at_fadd(c->state.translate_z, dz);
    }
    return true;
}

static bool issue_range(mc_client_render_global_submit_call *c, int32_t start, int32_t end) {
    for (int32_t index = start; index < end; index = mc_java_i32_add(index, 1)) {
        c->state.index = index;
        mc_client_source_dk *dk;
        bool empty;
        if (!cell(c, index, 502, 503, &dk))
            return false;
        mc_terrain_pc(c, 503);
        if (!mc_terrain_child(c, mc_source_dk_empty(dk, &empty, &c->failure), 503))
            return false;
        if (empty) {
            if (!cell(c, index, 515, 517, &dk))
                return false;
            dk->fields.o = false;
            mc_terrain_store(c, 517);
            continue;
        }
        if (!cell(c, index, 529, 530, &dk))
            return false;
        if (!dk->fields.o) {
            if (!cell(c, index, 542, 544, &dk))
                return false;
            dk->fields.x = true;
            mc_terrain_store(c, 544);
        }
        if (!cell(c, index, 553, 554, &dk))
            return false;
        if (!dk->fields.o)
            continue;
        if (!cell(c, index, 566, 567, &dk))
            return false;
        if (dk->fields.y)
            continue;
        if (!cell(c, index, 579, 581, &dk))
            return false;
        bool scheduled;
        if (!interval(c, dk, index, &scheduled))
            return false;
        if (!scheduled)
            continue;
        if (!cell(c, index, 620, 625, &dk) || !translate(c, dk) || !cell(c, index, 739, 740, &dk) ||
            !begin_query(c, dk->fields.z) || !cell(c, index, 752, 753, &dk))
            return false;
        mc_terrain_pc(c, 753);
        if (!mc_terrain_child(c, mc_source_dk_call_bounds(dk, &c->failure), 753) || !end_query(c) ||
            !cell(c, index, 767, 769, &dk))
            return false;
        dk->fields.y = true;
        mc_terrain_store(c, 769);
        if (!ended_query_release(c))
            return false;
    }
    return true;
}

static bool disable_for_queries(mc_client_render_global_submit_call *c) {
    mc_client_display *display = c->state.owner->display;
    return mc_terrain_display(
               c, mc_client_display_source_texture_enable(display, false, &c->failure.display),
               433) &&
           mc_terrain_display(c,
                              mc_client_display_source_disable_unlit_feature(
                                  display, MC_CLIENT_SOURCE_LIGHTING, &c->failure.display),
                              439) &&
           mc_terrain_display(
               c, mc_client_display_source_alpha_test(display, false, &c->failure.display), 445) &&
           mc_terrain_display(
               c, mc_client_display_source_fog_enable(display, false, &c->failure.display), 451) &&
           mc_terrain_display(c,
                              mc_client_display_source_color_mask(display, false, false, false,
                                                                  false, &c->failure.display),
                              458) &&
           mc_terrain_display(
               c, mc_client_display_source_depth_mask(display, false, &c->failure.display), 462);
}

static bool restore_after_queries(mc_client_render_global_submit_call *c) {
    mc_game_settings_state settings;
    if (!mc_terrain_settings(c, 782, 785, 788, &settings))
        return false;
    mc_client_display *display = c->state.owner->display;
    bool red = true, green = true, blue = true;
    unsigned int pc = 824;
    if (settings.values.anaglyph) {
        mc_client_render_classes *models = c->state.owner->fields.t->render_classes;
        mc_terrain_pc(c, 794);
        if (mc_client_entity_renderer_class_active_use(models, NULL) !=
            MC_CLIENT_ENTITY_RENDERER_OK)
            return mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE,
                                   "actual PX defining class namespace", 794, true);
        if (models->px_b == 0) {
            red = false;
            pc = 804;
        } else {
            green = blue = false;
            pc = 814;
        }
    }
    return mc_terrain_display(c,
                              mc_client_display_source_color_mask(display, red, green, blue, true,
                                                                  &c->failure.display),
                              pc) &&
           mc_terrain_display(
               c, mc_client_display_source_depth_mask(display, true, &c->failure.display), 828) &&
           mc_terrain_display(
               c, mc_client_display_source_texture_enable(display, true, &c->failure.display),
               834) &&
           mc_terrain_display(
               c, mc_client_display_source_alpha_test(display, true, &c->failure.display), 840) &&
           mc_terrain_display(
               c, mc_client_display_source_fog_enable(display, true, &c->failure.display), 846);
}

bool mc_terrain_occlusion(mc_client_render_global_submit_call *c, int32_t *out) {
    int32_t count = 0, subset;
    int32_t start = 0, end = 16;
    c->state.start = start;
    c->state.end = end;
    if (!mc_terrain_poll(c, start, end, 357))
        return false;
    for (int32_t index = start; index < end; index = mc_java_i32_add(index, 1)) {
        c->state.index = index;
        mc_client_source_dk *dk;
        if (!cell(c, index, 377, 379, &dk))
            return false;
        dk->fields.x = true;
        mc_terrain_store(c, 379);
    }
    if (!mc_terrain_subset(c, start, end, 397, &subset))
        return false;
    count = mc_java_i32_add(count, subset);
    /* This is a do/while in the bytecode. Even a16-cell sorted array reaches
     * one empty later range, its GL masks/Push/Pop and whole subset helper. */
    do {
        start = end;
        end = mc_java_i32_mul(end, 2);
        int32_t length;
        if (!sorted_length(c, 419, &length))
            return false;
        if (end > length) {
            if (!sorted_length(c, 427, &end))
                return false;
        }
        c->state.start = start;
        c->state.end = end;
        if (!disable_for_queries(c) || !mc_terrain_poll(c, start, end, 470))
            return false;
        mc_terrain_pc(c, 473);
        if (!mc_terrain_display(
                c, mc_client_display_source_push(c->state.owner->display, &c->failure.display),
                473))
            return false;
        c->state.matrix_pushed = true;
        c->state.translate_x = c->state.translate_y = c->state.translate_z = 0.0f;
        if (!issue_range(c, start, end))
            return false;
        mc_terrain_pc(c, 778);
        if (!mc_terrain_display(
                c, mc_client_display_source_pop(c->state.owner->display, &c->failure.display), 778))
            return false;
        c->state.matrix_pushed = false;
        if (!restore_after_queries(c) || !mc_terrain_subset(c, start, end, 858, &subset))
            return false;
        count = mc_java_i32_add(count, subset);
        if (!sorted_length(c, 870, &length))
            return false;
        if (end >= length)
            break;
    } while (true);
    mc_terrain_pc(c, 874);
    *out = count;
    return true;
}
