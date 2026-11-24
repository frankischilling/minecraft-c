/* Genuine prepared owners; authored inputs and passive observations of real
 * NW calls. No original observation is an execution input. */
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#include "client/display/display.h"
static bool billboard_display_create(const char *, int, int, bool, mc_client_display **,
                                     mc_client_display_error *);
int mc_burning_billboard_creature_fixture_main(int argc, char **argv);
#define MC_CREATURE_DRAW_MAIN mc_burning_billboard_creature_fixture_main
#define MC_CREATURE_DISPLAY_CREATE billboard_display_create
#include "client/render_engine/nw_internal.h"
#include "entity/construction_internal.h"
#include "test_creature_draw.c"
/* The actual Display implementation is compiled once, as in overlay/device
 * contracts. Interceptions delegate to its unchanged native GL functions. */
#include "../../../src/client/display/display.c"

static bool billboard_display_create(const char *title, int width, int height, bool hidden,
                                     mc_client_display **out, mc_client_display_error *error) {
    return create_display_profile(title, width, height, hidden, DISPLAY_FRAMEBUFFER_SDK_DEFAULT,
                                  out, error);
}

typedef struct billboard_batch {
    mc_client_nw_fields fields;
    uint32_t words[1024];
    float modelview[16], projection[16], color[4];
    GLint first;
    GLsizei count;
    GLenum mode;
    mc_client_texture texture;
    size_t matrix_depth;
    bool fire, lighting, texture_enabled, alpha;
} billboard_batch;

static mob_fixture *billboard_observed;
static mc_client_gl_draw_arrays_fn billboard_native_draw;
static mc_client_gl_bind_texture_fn billboard_native_bind;
static billboard_batch billboard_batches[8];
static size_t billboard_count;
static bool billboard_overflow, billboard_poison;
static mc_client_texture billboard_poison_texture;

static int billboard_device(mob_fixture *f) {
    const mc_client_display_info *device = mc_client_display_info_get(f->display);
    mc_client_framebuffer_info format;
    CHECK(device && mc_client_display_framebuffer_info_get(f->display, &format, NULL) &&
          format.alpha_bits == 0 && format.depth_bits > 0);
    fputs("{\"event\":\"device\",\"vendor\":", f->output);
    mob_string(f->output, device->vendor);
    fputs(",\"renderer\":", f->output);
    mob_string(f->output, device->renderer);
    fputs(",\"version\":", f->output);
    mob_string(f->output, device->version);
    fprintf(f->output,
            ",\"major\":%d,\"minor\":%d,\"profile\":%d,\"framebuffer\":{\"red\":%d,"
            "\"green\":%d,\"blue\":%d,\"alpha\":%d,\"depth\":%d,\"stencil\":%d,"
            "\"double_buffer\":%s}}\n",
            device->gl_major, device->gl_minor, device->profile, format.red_bits, format.green_bits,
            format.blue_bits, format.alpha_bits, format.depth_bits, format.stencil_bits,
            format.double_buffer ? "true" : "false");
    return 0;
}

static void GLAPIENTRY billboard_bind(GLenum target, GLuint texture) {
    billboard_native_bind(target, texture);
    if (billboard_poison && target == GL_TEXTURE_2D && texture == billboard_poison_texture) {
        billboard_poison = false;
        _mm_setcsr(0x9fc0u); /* External native-host control fault after real bind. */
    }
}

static void GLAPIENTRY billboard_draw(GLenum mode, GLint first, GLsizei count) {
    mob_fixture *f = billboard_observed;
    mc_client_nw_state nw;
    if (!mc_client_nw_inspect(f->runtime.graphics, &nw) || nw.a == NULL || nw.fields.o < 0 ||
        nw.fields.o > (int32_t)MOB_COUNT(billboard_batches[0].words) || nw.fields.g == NULL ||
        billboard_count == MOB_COUNT(billboard_batches)) {
        billboard_overflow = true;
    } else {
        billboard_batch *b = &billboard_batches[billboard_count++];
        *b = (billboard_batch){0};
        b->fields = nw.fields;
        memcpy(b->words, nw.fields.g, (size_t)nw.fields.o * sizeof(*b->words));
        memcpy(b->modelview, f->display->source.modelview.column_major, sizeof(b->modelview));
        memcpy(b->projection, f->display->source.projection, sizeof(b->projection));
        memcpy(b->color, f->display->source.color, sizeof(b->color));
        b->first = first;
        b->count = count;
        b->mode = mode;
        b->texture = f->display->source.bound_texture;
        b->matrix_depth = f->display->source.depth + 1;
        b->lighting = f->display->source.lighting_enabled;
        b->texture_enabled = f->display->source.texture_enabled;
        b->alpha = f->display->source.alpha_test;
        /* Read the entered call's passive receipt directly. Public inspection
         * correctly refuses reentry while the actual call is executing. */
        b->fire = f->call != NULL && f->call->post.state.fire.begin_returned;
    }
    billboard_native_draw(mode, first, count);
}

static mc_client_manager_status billboard_begin(mob_fixture *f,
                                                const mc_client_manager_draw_limits *limits) {
    const mc_client_manager_draw_services services = {f->math, f->runtime.graphics, f->display,
                                                      f->runtime.services.application_classes};
    return mc_client_render_manager_post_draw_begin(f->manager, f->renderer, f->actor, 0, 0, 0, 30,
                                                    .375f, &services, limits, &f->call,
                                                    f->draw_error);
}

static int billboard_emit_batch(mob_fixture *f, unsigned int index, size_t ordinal,
                                const billboard_batch *b) {
    const mc_client_nw_fields *n = &b->fields;
    fprintf(f->output,
            "{\"event\":\"batch\",\"case\":%u,\"ordinal\":%zu,\"fire\":%s,"
            "\"mode\":%u,\"first\":%d,\"count\":%d,\"nw\":{\"h\":%d,\"o\":%d,\"p\":%d,"
            "\"r\":%d,\"w\":%s,\"l\":%s,\"m\":%s,\"n\":%s,\"q\":%s,\"translation\":[",
            index, ordinal, b->fire ? "true" : "false", b->mode, b->first, b->count, n->h, n->o,
            n->p, n->r, n->w ? "true" : "false", n->l ? "true" : "false", n->m ? "true" : "false",
            n->n ? "true" : "false", n->q ? "true" : "false");
    const double translation[] = {n->s, n->t, n->u};
    for (size_t i = 0; i < MOB_COUNT(translation); ++i)
        fprintf(f->output, "%s\"%016" PRIx64 "\"", i ? "," : "", mob_dbits(translation[i]));
    fputs("]},\"words\":[", f->output);
    for (int32_t i = 0; i < n->o; ++i)
        fprintf(f->output, "%s\"%08" PRIx32 "\"", i ? "," : "", b->words[i]);
    fputs("],\"modelview\":[", f->output);
    for (size_t i = 0; i < MOB_COUNT(b->modelview); ++i)
        fprintf(f->output, "%s\"%08" PRIx32 "\"", i ? "," : "", mob_fbits(b->modelview[i]));
    fputs("],\"projection\":[", f->output);
    for (size_t i = 0; i < MOB_COUNT(b->projection); ++i)
        fprintf(f->output, "%s\"%08" PRIx32 "\"", i ? "," : "", mob_fbits(b->projection[i]));
    fputs("],\"color\":[", f->output);
    for (size_t i = 0; i < MOB_COUNT(b->color); ++i)
        fprintf(f->output, "%s\"%08" PRIx32 "\"", i ? "," : "", mob_fbits(b->color[i]));
    fprintf(f->output,
            "],\"texture\":%u,\"matrix_depth\":%zu,\"lighting\":%s,\"texture_enabled\":%s,"
            "\"alpha\":%s}\n",
            b->texture, b->matrix_depth, b->lighting ? "true" : "false",
            b->texture_enabled ? "true" : "false", b->alpha ? "true" : "false");
    return 0;
}

static int billboard_input(mob_fixture *f, unsigned int index) {
    CHECK(f->actor->bounds != NULL && mob_fbits(f->manager->fields.i) == UINT32_C(0x42870000));
    fprintf(f->output,
            "{\"event\":\"input\",\"case\":%u,\"actor\":\"%s\",\"width\":\"%08" PRIx32
            "\",\"height\":\"%08" PRIx32 "\",\"y\":\"%016" PRIx64 "\",\"min_y\":\"%016" PRIx64
            "\",\"view_yaw\":\"%08" PRIx32 "\",\"shadow_size\":\"%08" PRIx32 "\",\"bv\":%d}\n",
            index, f->kind, mob_fbits(f->actor->base.width), mob_fbits(f->actor->base.height),
            mob_dbits(f->actor->base.y), mob_dbits(f->actor->bounds->min_y),
            mob_fbits(f->manager->fields.i), mob_fbits(f->renderer->shadow_size),
            f->actor->base.bv);
    return 0;
}

static int billboard_terrain(mob_fixture *f, unsigned int index,
                             const mc_client_living_draw_state *post) {
    CHECK(post->texture_call_count >= 1);
    const mc_client_living_draw_texture_call *t =
        &post->texture_calls[post->texture_call_count - 1];
    static const uint16_t key[] = {'/', 't', 'e', 'r', 'r', 'a', 'i', 'n', '.', 'p', 'n', 'g'};
    CHECK(t->key_length == MOB_COUNT(key) && t->key_stored == MOB_COUNT(key) &&
          !memcmp(t->key_units, key, sizeof(key)) && t->lookup_returned && t->bind_returned &&
          t->returned && !t->failed && t->caller.pc == 104);
    mc_client_texture_source_state texture;
    CHECK(mc_client_display_texture_state_inspect(f->display, (mc_client_texture)t->texture_id,
                                                  &texture) &&
          texture.known && texture.image_uploaded && texture.wrap_s == GL_REPEAT &&
          texture.wrap_t == GL_REPEAT && texture.min_filter == GL_NEAREST &&
          texture.mag_filter == GL_NEAREST);
    fprintf(f->output,
            "{\"event\":\"terrain\",\"case\":%u,\"name\":%d,\"calls\":%zu,\"wrap_s\":%d,"
            "\"wrap_t\":%d,\"min\":%d,\"mag\":%d,\"keys\":[",
            index, t->texture_id, post->texture_call_count, texture.wrap_s, texture.wrap_t,
            texture.min_filter, texture.mag_filter);
    for (size_t i = 0; i < post->texture_call_count; ++i) {
        const mc_client_living_draw_texture_call *call = &post->texture_calls[i];
        CHECK(call->key_stored == call->key_length && call->returned && !call->failed);
        fprintf(f->output, "%s\"", i ? "," : "");
        for (size_t j = 0; j < call->key_stored; ++j) {
            CHECK(call->key_units[j] >= 32 && call->key_units[j] < 127 &&
                  call->key_units[j] != '\\' && call->key_units[j] != '"');
            fputc((int)call->key_units[j], f->output);
        }
        fputc('"', f->output);
    }
    fputs("]", f->output);
    if (index == 0) {
        const mc_client_direct_byte_buffer *transfer = f->app->render_engine->fields.f;
        CHECK(transfer && transfer->limit == 256u * 256u * 4u);
        fputs(",\"transfer_hex\":\"", f->output);
        for (size_t byte = 0; byte < transfer->limit; ++byte)
            fprintf(f->output, "%02x", transfer->data[byte]);
        fputc('"', f->output);
    }
    fputs("}\n", f->output);
    return 0;
}

static int billboard_case(mob_fixture *f, unsigned int index, const char *directory) {
    static const unsigned int selected[] = {0, 2, 4, 11, 0, 11, 11};
    static const size_t expected_layers[] = {2, 2, 2, 5, 0, 3, 4};
    CHECK(index < MOB_COUNT(selected) && mob_select(f, selected[index]) == 0 &&
          mob_frame_with_lighting(f, index, true) == 0);
    f->actor->base.bv = 20; /* Declared source input; no fire tick or watcher replacement. */
    if (index == 4)
        f->actor->base.height = 0;
    if (index >= 5) {
        f->actor->base.width = 1;
        f->actor->base.height = index == 5 ? 1.4f : 2.1f;
    }
    CHECK(billboard_input(f, index) == 0 && mob_emit_state(f, index, "before") == 0 &&
          mob_pixels(f, directory, index, true) == 0);
    const uint64_t own = f->actor->random->state, camera = f->camera->random->state,
                   world = f->world->fields.r->state,
                   shared = f->runtime.services.profile->math->state;
    const int32_t timer = f->actor->base.bv;
    const size_t borrows = atomic_load(&f->app->render_borrows);
    const size_t actor_holds = f->actor->holds, renderer_holds = f->renderer->holds,
                 manager_holds = f->manager->holds;
    billboard_observed = f;
    billboard_count = 0;
    billboard_overflow = false;
    billboard_native_draw = f->display->gl.DrawArrays;
    f->display->gl.DrawArrays = billboard_draw;
    const mc_client_manager_status result = billboard_begin(f, NULL);
    f->display->gl.DrawArrays = billboard_native_draw;
    if (result != MC_CLIENT_MANAGER_OK)
        fprintf(stderr, "Billboard case%u: %s.%s:%d %s\n", index, f->draw_error->site.class_name,
                f->draw_error->site.method, f->draw_error->site.pc, f->draw_error->operation);
    CHECK(result == MC_CLIENT_MANAGER_OK && f->call && !billboard_overflow);
    mc_client_manager_draw_state state;
    CHECK(mc_client_render_manager_draw_inspect(f->call, &state) && state.returned &&
          state.post_only && !state.primary_returned && !state.lookup_returned &&
          state.post_draw_returned && state.post_draw && state.post_draw->returned &&
          state.post_draw->fire_checked && state.post_draw->burning);
    const mc_client_living_fire_state *s = &state.post_draw->fire;
    CHECK(s->receiver_identity == (uintptr_t)f->renderer &&
          s->actor_identity == (uintptr_t)f->actor);
    CHECK(s->entered && s->lighting_disabled && s->block_static_returned && s->texture_field_read &&
          s->texture == 31 && s->atlas_x == 240 && s->atlas_y == 16 && s->matrix_pushed &&
          s->position_translated && s->scaled && s->texture_returned && s->nw_captured &&
          s->height_read && s->bounds_read && s->rotated && s->depth_translated &&
          s->color_returned && s->begin_attempted && s->begin_returned && s->draw_attempted &&
          s->draw_returned && s->matrix_popped && s->lighting_enabled && s->returned &&
          s->nw_writing_before_draw && !s->nw_writing_after_draw);
    CHECK(s->vertices == s->vertex_attempts && s->vertices == s->layers * 4 &&
          s->loop_tests == s->layers + 1 && s->remaining_height <= 0 && s->nw_mode == GL_QUADS &&
          s->nw_words_identity != 0 && s->layers == expected_layers[index] &&
          s->nw_vertices_before_draw == (int32_t)s->layers * 6 &&
          s->nw_words_before_draw == (int32_t)s->layers * 48);
    size_t fire_batches = 0;
    for (size_t i = 0; i < billboard_count; ++i) {
        const billboard_batch *b = &billboard_batches[i];
        if (b->fire) {
            ++fire_batches;
            CHECK(b->fields.h == (int32_t)s->layers * 6 && b->fields.o == (int32_t)s->layers * 48 &&
                  b->fields.r == GL_QUADS && !b->fields.w && b->fields.m && !b->fields.l &&
                  !b->fields.n && !b->fields.q && !b->lighting && b->texture_enabled && b->alpha &&
                  b->matrix_depth == 2 && b->first == 0 && b->count == (GLsizei)s->layers * 6 &&
                  b->mode == GL_TRIANGLES);
        }
        CHECK(billboard_emit_batch(f, index, i, b) == 0);
    }
    CHECK(fire_batches == (s->layers ? 1u : 0u) &&
          billboard_terrain(f, index, state.post_draw) == 0);
    CHECK(f->actor->base.bv == timer && f->actor->random->state == own &&
          f->camera->random->state == camera && f->world->fields.r->state == world &&
          f->runtime.services.profile->math->state == shared && f->display->source.depth == 0 &&
          f->display->source.projection_depth == 0 && f->display->source.lighting_enabled);
    fprintf(f->output,
            "{\"event\":\"fire\",\"case\":%u,\"layers\":%zu,\"vertices\":%zu,\"loop_tests\":%zu,"
            "\"texture\":%d,\"scale\":\"%08" PRIx32 "\",\"initial_height\":\"%08" PRIx32
            "\",\"remaining\":\"%08" PRIx32 "\",\"initial_y_offset\":\"%08" PRIx32
            "\",\"z_offset\":\"%08" PRIx32 "\",\"view_yaw\":\"%08" PRIx32 "\",\"gl\":",
            index, s->layers, s->vertices, s->loop_tests, s->texture, mob_fbits(s->scale),
            mob_fbits(s->initial_height), mob_fbits(s->remaining_height),
            mob_fbits(s->initial_y_offset), mob_fbits(s->z_offset), mob_fbits(s->view_yaw));
    CHECK(mob_emit_gl(f) == 0);
    fputs(",\"receiver\":", f->output);
    mob_ref(f->output, (const void *)s->receiver_identity, f->renderer->source_class);
    fputs(",\"actor\":", f->output);
    mob_ref(f->output, (const void *)s->actor_identity, f->kind);
    fputs("}\n", f->output);
    CHECK(mob_pixels(f, directory, index, false) == 0 && mob_emit_state(f, index, "after") == 0);
    uint32_t gl_error = UINT32_MAX;
    CHECK(mc_client_display_source_get_error(f->display, &gl_error, NULL) && gl_error == 0);
    CHECK(mc_client_render_manager_draw_retire(&f->call, f->draw_error) == MC_CLIENT_MANAGER_OK &&
          f->call == NULL && atomic_load(&f->app->render_borrows) == borrows &&
          f->actor->holds == actor_holds && f->renderer->holds == renderer_holds &&
          f->manager->holds == manager_holds);
    return 0;
}

static int billboard_control(mob_fixture *f, const char *name) {
    CHECK(mob_select(f, 0) == 0 && mob_frame(f, 0) == 0);
    f->renderer->shadow_size = 0; /* Declared selected-prefix input, no shadow work. */
    f->actor->base.bv = 20;
    mc_client_tessellator *nw = NULL;
    mc_client_graphics_error graphics;
    CHECK(mc_client_nw_active_use(f->runtime.graphics, (mc_client_graphics_site){0}, &nw,
                                  &graphics) &&
          nw);
    mc_client_manager_draw_limits limits = mc_client_render_manager_draw_default_limits();
    mc_entity_bounds *displaced_bounds = NULL;
    int32_t preexisting_words[8] = {0};
    if (!strcmp(name, "work_zero"))
        limits.max_work = 0;
    else if (!strcmp(name, "work_vertex"))
        limits.max_work = 2;
    else if (!strcmp(name, "null_bounds")) {
        displaced_bounds = f->actor->bounds;
        CHECK(displaced_bounds != NULL);
        f->actor->bounds = NULL;
    } else if (!strcmp(name, "preexisting_batch")) {
        CHECK(mc_client_nw_begin_quads(nw, &graphics) &&
              mc_client_nw_vertex_uv(nw, 3, 4, 5, .25, .75, &graphics));
        memcpy(preexisting_words, nw->fields.g, sizeof(preexisting_words));
    } else if (!strcmp(name, "fp_after_bind")) {
        uint16_t units[128];
        const mc_texture_pack_text key = mob_text("/terrain.png", units);
        CHECK(mc_client_render_engine_texture(f->app->render_engine, f->display, NULL, key,
                                              &billboard_poison_texture,
                                              f->lookup_error) == MC_CLIENT_LOOKUP_OK);
        billboard_native_bind = f->display->gl.BindTexture;
        billboard_poison = true;
        f->display->gl.BindTexture = billboard_bind;
    } else if (!strcmp(name, "draw_buffer_null"))
        nw->fields.e = NULL; /* Actual nullable source field; arena still owns the buffer. */
    else if (!strcmp(name, "nan_height")) {
        const uint32_t bits = UINT32_C(0x7fc00000);
        memcpy(&f->actor->base.height, &bits, sizeof(bits));
    } else if (!strcmp(name, "infinite_height_limit")) {
        const uint32_t bits = UINT32_C(0x7f800000);
        memcpy(&f->actor->base.height, &bits, sizeof(bits));
        limits.max_work = 6;
    } else
        CHECK(false);
    const unsigned int controls = _mm_getcsr();
    const size_t borrows = atomic_load(&f->app->render_borrows);
    const size_t actor_holds = f->actor->holds, renderer_holds = f->renderer->holds,
                 manager_holds = f->manager->holds, direct_bytes = f->actor->direct_bytes;
    const uint64_t own = f->actor->random->state, camera = f->camera->random->state,
                   world = f->world->fields.r->state,
                   shared = f->runtime.services.profile->math->state;
    const mc_client_manager_status result = billboard_begin(f, &limits);
    if (!strcmp(name, "fp_after_bind")) {
        /* End only the external host fault. No source state/phase is restored. */
        _mm_setcsr(controls);
        f->display->gl.BindTexture = billboard_native_bind;
    }
    CHECK(f->call != NULL && f->actor->base.bv == 20 && f->actor->random->state == own &&
          f->camera->random->state == camera && f->world->fields.r->state == world &&
          f->runtime.services.profile->math->state == shared);
    mc_client_manager_draw_state state;
    CHECK(mc_client_render_manager_draw_inspect(f->call, &state) && state.post_only &&
          state.post_draw);
    const mc_client_living_fire_state *s = &state.post_draw->fire;
    CHECK(s->entered && s->lighting_disabled && s->scaled && s->texture_returned && s->nw_captured);
    const bool returned = !strcmp(name, "nan_height");
    CHECK((result == MC_CLIENT_MANAGER_OK) == returned && state.returned == returned &&
          s->returned == returned && s->matrix_popped == returned &&
          s->lighting_enabled == returned);
    int32_t expected_pc = -1;
    bool source = false;
    if (!strcmp(name, "work_zero")) {
        expected_pc = 197;
        CHECK(s->begin_returned && s->layers == 0 && s->vertices == 0 && nw->fields.w &&
              nw->fields.h == 0 && nw->fields.o == 0 && !s->draw_attempted);
    } else if (!strcmp(name, "work_vertex")) {
        expected_pc = 358;
        CHECK(s->begin_returned && s->layers == 0 && s->vertices == 1 && nw->fields.w &&
              nw->fields.h == 1 && nw->fields.o == 8 && s->last_vertex_pc == 332 &&
              s->last_vertex_returned && !s->draw_attempted);
    } else if (!strcmp(name, "null_bounds")) {
        expected_pc = 136;
        source = true;
        CHECK(s->height_read && !s->bounds_read && !s->rotated && !s->begin_attempted &&
              f->actor->bounds == NULL && !nw->fields.w);
    } else if (!strcmp(name, "preexisting_batch")) {
        expected_pc = 187;
        source = true;
        CHECK(s->begin_attempted && !s->begin_returned && nw->fields.w && nw->fields.h == 1 &&
              nw->fields.o == 8 &&
              !memcmp(preexisting_words, nw->fields.g, sizeof(preexisting_words)) &&
              !s->draw_attempted && f->draw_error->living->child.pc == 16 &&
              !strcmp(f->draw_error->exception_class, "java/lang/IllegalStateException"));
    } else if (!strcmp(name, "fp_after_bind")) {
        expected_pc = 125;
        CHECK(!billboard_poison && s->height_read && !s->bounds_read && !s->begin_attempted &&
              !nw->fields.w && s->initial_height == 0);
    } else if (!strcmp(name, "draw_buffer_null")) {
        expected_pc = 450;
        source = true;
        CHECK(s->begin_returned && s->draw_attempted && !s->draw_returned && !nw->fields.w &&
              nw->fields.h == 12 && nw->fields.o == 96 && s->vertices == 8 && s->layers == 2 &&
              f->draw_error->living->child.pc == 33 && nw->fields.e == NULL);
    } else if (!strcmp(name, "infinite_height_limit")) {
        expected_pc = 332;
        CHECK(s->begin_returned && !s->draw_attempted && s->layers == 1 && s->vertices == 4 &&
              nw->fields.w && nw->fields.h == 6 && nw->fields.o == 48 &&
              mob_fbits(s->remaining_height) == UINT32_C(0x7f800000));
    } else {
        CHECK(s->begin_returned && s->draw_returned && s->layers == 0 && s->vertices == 0 &&
              s->loop_tests == 1 && !nw->fields.w && nw->fields.h == 0 && nw->fields.o == 0 &&
              isnan(s->initial_height) && s->z_offset == -.3f);
    }
    if (!returned) {
        CHECK(f->draw_error->living && f->draw_error->living->site.pc == expected_pc &&
              !strcmp(f->draw_error->living->site.class_name, "bw") &&
              !strcmp(f->draw_error->living->site.descriptor, "(Lsn;DDDF)V") &&
              f->draw_error->source_exception == source &&
              f->draw_error->native_policy == !source && !s->matrix_popped && !s->lighting_enabled);
        if (source && strcmp(name, "preexisting_batch"))
            CHECK(!strcmp(f->draw_error->exception_class, "java/lang/NullPointerException"));
    }
    const size_t depth = f->display->source.depth;
    const bool lighting = f->display->source.lighting_enabled, writing = nw->fields.w;
    const int32_t vertices = nw->fields.h, words = nw->fields.o;
    CHECK(depth == (returned ? 0u : 1u) && lighting == returned);
    fprintf(f->output,
            "{\"event\":\"control\",\"name\":\"%s\",\"returned\":%s,\"pc\":%d,"
            "\"source_exception\":%s,\"native_policy\":%s,\"layers\":%zu,\"vertices\":%zu,"
            "\"nw_h\":%d,\"nw_o\":%d,\"nw_w\":%s,\"matrix_depth\":%zu,\"lighting\":%s,"
            "\"raw_words\":[",
            name, returned ? "true" : "false", expected_pc, source ? "true" : "false",
            !returned && !source ? "true" : "false", s->layers, s->vertices, vertices, words,
            writing ? "true" : "false", depth + 1, lighting ? "true" : "false");
    for (int32_t i = 0; i < words; ++i)
        fprintf(f->output, "%s\"%08" PRIx32 "\"", i ? "," : "", (uint32_t)nw->fields.g[i]);
    fputs("]", f->output);
    CHECK(mc_client_render_manager_draw_retire(&f->call, f->draw_error) == MC_CLIENT_MANAGER_OK &&
          f->call == NULL && atomic_load(&f->app->render_borrows) == borrows &&
          f->actor->holds == actor_holds && f->renderer->holds == renderer_holds &&
          f->manager->holds == manager_holds && f->display->source.depth == depth &&
          f->display->source.lighting_enabled == lighting && nw->fields.w == writing &&
          nw->fields.h == vertices && nw->fields.o == words);
    if (displaced_bounds != NULL) {
        /* Terminal native disposal consumes exactly the detached genuine
         * allocation via the existing Entity ledger. Source aW stays NULL. */
        mc_entity_construction_retire(f->actor, displaced_bounds, sizeof(*displaced_bounds));
        CHECK(f->actor->bounds == NULL &&
              f->actor->direct_bytes == direct_bytes - sizeof(*displaced_bounds));
    }
    fputs(",\"checked_retired\":true,\"prefix_preserved\":true}\n", f->output);
    return 0;
}

int main(int argc, char **argv) {
    CHECK(argc == 5 || argc == 6);
    SDL_SetMainReady();
    mob_fixture *f = calloc(1, sizeof(*f));
    CHECK(f);
    char path[4096];
    CHECK(snprintf(path, sizeof(path), "%s/billboard.ndjson", argv[4]) > 0);
    f->output = fopen(path, "wb");
    CHECK(f->output && mob_open(f, argv[1], argv[2], argv[3]) == 0 && mob_textures(f) == 0 &&
          mob_prepare(f) == 0 && billboard_device(f) == 0);
    if (argc == 6)
        CHECK(billboard_control(f, argv[5]) == 0);
    else
        for (unsigned int index = 0; index < 7; ++index)
            CHECK(billboard_case(f, index, argv[4]) == 0);
    CHECK(!ferror(f->output) && fclose(f->output) == 0 && mob_close(f) == 0);
    free(f);
    if (argc == 6)
        printf("Burning billboard control %s: %u checks;checked terminal cleanup\n", argv[5],
               checks);
    else
        printf(
            "Burning billboards: %u checks;7 genuine post-draw profiles;checked terminal cleanup\n",
            checks);
    return 0;
}
