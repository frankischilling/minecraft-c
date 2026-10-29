#include "../application_state.h"
#include "../display/source_surface.h"
#include "../render_engine/internal.h"
#include "render_global_internal.h"

#include <string.h>

static mc_client_graphics_site ctor_site(unsigned int pc) {
    return (mc_client_graphics_site){"n", "<init>", "(Lnet/minecraft/client/Minecraft;Lji;)V",
                                     (int32_t)pc};
}

static void *allocate(mc_client_render_global *o, size_t bytes, unsigned int pc) {
    o->last_constructor_pc = pc;
    void *result =
        mc_client_render_arena_allocate(&o->arena, bytes, ctor_site(pc), &o->failure.graphics);
    if (result == NULL)
        (void)mc_render_global_graphics(o, pc);
    return result;
}

static bool lists(mc_client_render_global *o, int32_t count, unsigned int pc, int32_t *out) {
    o->last_constructor_pc = pc;
    return mc_client_ge_generate_lists(o->classes, o->display, count, ctor_site(pc), out,
                                       &o->failure.graphics) ||
           mc_render_global_graphics(o, pc);
}

static bool queries(mc_client_render_global *o) {
    o->last_constructor_pc = 210;
    if (!mc_client_ge_int_clear(o->fields.c, ctor_site(210), &o->failure.graphics))
        return mc_render_global_graphics(o, 210);
    o->last_constructor_pc = 220;
    mc_client_direct_int_buffer *v = NULL;
    if (!mc_client_ge_d(o->classes, &o->arena, 262144, ctor_site(220), &v, &o->failure.graphics))
        return mc_render_global_graphics(o, 220);
    o->fields.v = v;
    o->last_store_pc = 223;
    o->last_constructor_pc = 230;
    if (!mc_client_ge_int_clear(o->fields.v, ctor_site(230), &o->failure.graphics))
        return mc_render_global_graphics(o, 230);
    o->last_constructor_pc = 239;
    if (!mc_client_ge_int_position(o->fields.v, 0, ctor_site(239), &o->failure.graphics))
        return mc_render_global_graphics(o, 239);
    o->last_constructor_pc = 252;
    if (!mc_client_ge_int_limit(o->fields.v, 262144, ctor_site(252), &o->failure.graphics))
        return mc_render_global_graphics(o, 252);
    o->last_constructor_pc = 260;
    void *storage = NULL;
    size_t remaining = 0;
    if (!mc_client_ge_int_writable_remaining(o->fields.v, &storage, &remaining, ctor_site(260),
                                             &o->failure.graphics))
        return mc_render_global_graphics(o, 260);
    const bool generated = mc_client_display_source_gen_queries(o->display, storage, remaining,
                                                                &o->queries, &o->failure.display);
    mc_client_display_query_state state;
    if (mc_client_display_query_names_inspect(o->queries, &state))
        o->queries_issued = state.issued;
    return generated || mc_render_global_display(o, 260);
}

static bool construct(mc_client_render_global *o, client_application *app,
                      mc_client_render_engine *engine) {
    o->last_constructor_pc = 1; /* Actual Object.<init>. */
    mc_client_render_list *list = allocate(o, sizeof(*o->fields.a), 5);
    if (list == NULL)
        return false;
    o->last_constructor_pc = 9; /* Same concrete empty List representation. */
    o->fields.a = list;
    o->last_store_pc = 12;
    list = allocate(o, sizeof(*o->fields.m), 16);
    if (list == NULL)
        return false;
    o->last_constructor_pc = 20;
    o->fields.m = list;
    o->last_store_pc = 23;
    o->fields.w = false;
    o->last_store_pc = 28;
    o->fields.x = 0;
    o->last_store_pc = 33;
    o->fields.H = -1;
    o->last_store_pc = 38;
    o->fields.I = 2;
    o->last_store_pc = 43;
    int32_t *b = allocate(o, 50000u * sizeof(*b), 49);
    if (b == NULL)
        return false;
    o->fields.b = b;
    o->last_store_pc = 51;
    o->last_constructor_pc = 57;
    mc_client_direct_int_buffer *c = NULL;
    if (!mc_client_ge_d(o->classes, &o->arena, 64, ctor_site(57), &c, &o->failure.graphics))
        return mc_render_global_graphics(o, 57);
    o->fields.c = c;
    o->last_store_pc = 60;
    list = allocate(o, sizeof(*o->fields.S), 64);
    if (list == NULL)
        return false;
    o->last_constructor_pc = 68;
    o->fields.S = list;
    o->last_store_pc = 71;
    o->pending_T = allocate(o, 4u * sizeof(*o->pending_T), 76);
    if (o->pending_T == NULL)
        return false;
    static const unsigned int news[] = {81, 91, 101, 111};
    static const unsigned int ctors[] = {85, 95, 105, 115};
    static const unsigned int stores[] = {88, 98, 108, 118};
    for (size_t index = 0; index < 4; ++index) {
        o->pending_batch = allocate(o, sizeof(*o->pending_batch), news[index]);
        if (o->pending_batch == NULL)
            return false;
        o->last_constructor_pc = ctors[index];
        if (!mc_render_batch_construct(o, o->pending_batch, ctors[index]))
            return false;
        ++o->constructed_batches;
        o->pending_T[index] = o->pending_batch; /* Actual aastore, not T publication. */
        o->pending_batch = NULL;
        ++o->published_batches;
        o->last_constructor_pc = stores[index];
    }
    o->fields.T = o->pending_T;
    o->pending_T = NULL;
    o->last_store_pc = 119;
    o->fields.d = 0;
    o->last_store_pc = 124;
    int32_t generated = 0;
    if (!lists(o, 1, 129, &generated))
        return false;
    o->fields.e = generated;
    o->last_store_pc = 132;
    const uint64_t distance_bits = UINT64_C(0xc0c3878000000000);
    memcpy(&o->fields.f, &distance_bits, sizeof(distance_bits));
    o->last_store_pc = 139;
    memcpy(&o->fields.g, &distance_bits, sizeof(distance_bits));
    o->last_store_pc = 146;
    memcpy(&o->fields.h, &distance_bits, sizeof(distance_bits));
    o->last_store_pc = 153;
    o->fields.j = 0;
    o->last_store_pc = 158;
    o->fields.t = app;
    o->last_store_pc = 163;
    o->fields.l = engine;
    o->last_store_pc = 168;
    if (!lists(o, 786432, 182, &generated))
        return false;
    o->fields.s = generated;
    o->last_store_pc = 185;
    o->last_constructor_pc = 190;
    if (app == NULL) {
        mc_render_global_fail(&o->failure, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                              "actual Minecraft.n receiver", "<init>", 190, false);
        return false;
    }
    if (app->constructor_phase == MC_CLIENT_APP_RETIRING ||
        app->native_lifetime_phase == MC_CLIENT_APPLICATION_LIFETIME_RETIRED) {
        mc_render_global_fail(&o->failure, MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE,
                              "native actual App borrow at Minecraft.n", "<init>", 190, true);
        return false;
    }
    /* Complete actual Minecraft.n()Lcx; getter: S read1, areturn4. No
     * proxy/default CX, App namespace activation or class/context query here. */
    mc_client_source_cx *captured_cx = app->S;
    o->last_constructor_pc = 193;
    if (captured_cx == NULL) {
        mc_render_global_fail(&o->failure, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                              "actual cx.a receiver after Minecraft.n", "<init>", 193, false);
        return false;
    }
    bool supported = false;
    if (mc_client_cx_supported(captured_cx, o->display, &supported, &o->failure.cx) !=
        MC_CLIENT_CX_OK) {
        mc_render_global_fail(&o->failure, MC_CLIENT_RENDER_GLOBAL_CX_FAILED,
                              o->failure.cx.operation, "<init>", 193, true);
        return false;
    }
    o->fields.w = supported;
    o->last_store_pc = 196;
    if (o->fields.w && !queries(o))
        return false;
    if (!lists(o, 3, 265, &generated))
        return false;
    o->fields.y = generated;
    o->last_store_pc = 268;
    o->last_constructor_pc = 271;
    if (!mc_client_display_source_push(o->display, &o->failure.display))
        return mc_render_global_display(o, 271);
    o->matrix_pushed = true;
    o->last_constructor_pc = 281;
    if (!mc_client_display_lists_begin(o->display, o->fields.y, &o->failure.display))
        return mc_render_global_display(o, 281);
    o->open_list = o->fields.y;
    o->last_constructor_pc = 285;
    o->failure.enclosing_pc = 285;
    if (!mc_render_global_stars(o))
        return false;
    o->failure.enclosing_pc = 0;
    o->failure.source_method = "<init>";
    o->failure.source_descriptor = "(Lnet/minecraft/client/Minecraft;Lji;)V";
    o->last_constructor_pc = 288;
    if (!mc_client_display_lists_end(o->display, &o->failure.display))
        return mc_render_global_display(o, 288);
    o->open_list = 0;
    o->last_constructor_pc = 291;
    if (!mc_client_display_source_pop(o->display, &o->failure.display))
        return mc_render_global_display(o, 291);
    o->matrix_pushed = false;
    o->last_constructor_pc = 294;
    if (!mc_client_nw_active_use(o->classes, ctor_site(294), &o->tessellator, &o->failure.graphics))
        return mc_render_global_graphics(o, 294);
    return mc_render_global_sky(o);
}

mc_client_render_global_status
mc_client_render_global_construct(mc_client_render_global *o, client_application *app,
                                  mc_client_render_engine *engine, mc_client_display *display,
                                  mc_client_render_global_error *error) {
    mc_client_render_global_error e = {0};
    if (o == NULL) {
        mc_render_global_fail(&e, MC_CLIENT_RENDER_GLOBAL_INVALID_ARGUMENT,
                              "native RenderGlobal constructor receiver", NULL, 0, true);
    } else if (o->active || o->phase != MC_CLIENT_RENDER_GLOBAL_RAW) {
        mc_render_global_fail(&e, MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE,
                              "RenderGlobal one-shot constructor admission", NULL, 0, true);
    } else {
        o->active = true;
        o->phase = MC_CLIENT_RENDER_GLOBAL_CONSTRUCTING;
        o->display = display;
        o->failure = (mc_client_render_global_error){.source_class = "n",
                                                     .source_method = "<init>",
                                                     .source_descriptor =
                                                         "(Lnet/minecraft/client/Minecraft;Lji;)V"};
        const bool complete = construct(o, app, engine);
        o->phase = complete ? MC_CLIENT_RENDER_GLOBAL_READY : MC_CLIENT_RENDER_GLOBAL_FAILED;
        o->source_returned = complete;
        o->failure.source_returned = complete;
        o->active = false;
        e = o->failure;
    }
    if (error != NULL)
        *error = e;
    return e.status;
}
