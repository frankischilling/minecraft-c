/* Genuine MC/Arrow owners and authored caller inputs. The draw observer copies
 * actual NW/Display state synchronously and delegates every real GL call. */
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#include "client/display/display.h"
static bool arrow_display_create(const char *, int, int, bool, mc_client_display **,
                                 mc_client_display_error *);
int mc_arrow_draw_creature_fixture_main(int argc, char **argv);
#define MC_CREATURE_DRAW_MAIN mc_arrow_draw_creature_fixture_main
#define MC_CREATURE_DISPLAY_CREATE arrow_display_create
#include "../../../src/client/display/display.c"
#include "client/render/arrow_draw_internal.h"
#include "entity/arrow/internal.h"
#include "test_creature_draw.c"

static bool arrow_display_create(const char *title, int width, int height, bool hidden,
                                 mc_client_display **out, mc_client_display_error *error) {
    return create_display_profile(title, width, height, hidden, DISPLAY_FRAMEBUFFER_SDK_DEFAULT,
                                  out, error);
}

static mc_client_managed_renderer *arrow_renderer(mc_client_render_manager *manager) {
    for (size_t i = 0; i < manager->allocated_renderers; ++i)
        if (!strcmp(manager->renderers[i]->source_class, "mc"))
            return manager->renderers[i];
    return NULL;
}

static int arrow_nw_cpu(void) {
    mc_client_render_engine_classes *classes = NULL, *replacement_classes = NULL, *failed = NULL;
    CHECK(mc_client_render_engine_classes_create(NULL, &classes) == MC_CLIENT_RENDER_ENGINE_OK &&
          mc_client_render_engine_classes_create(NULL, &replacement_classes) ==
              MC_CLIENT_RENDER_ENGINE_OK);
    mc_client_tessellator *local = NULL, *replacement = NULL, *current = NULL;
    mc_client_graphics_error error;
    const mc_client_graphics_site caller = {"fixture", "nw", "()V", 0};
    CHECK(mc_client_nw_active_use(classes, caller, &local, &error) && local &&
          mc_client_nw_active_use(replacement_classes, caller, &replacement, &error) &&
          replacement && local != replacement && classes->nw.published == local &&
          classes->nw.candidate == NULL);
    classes->nw.a = NULL;
    CHECK(mc_client_nw_active_use(classes, caller, &current, &error) && current == NULL &&
          mc_client_nw_translation(local, 3, 4, 5, &error));
    mc_client_nw_state state;
    CHECK(mc_client_nw_inspect(classes, &state) && state.a == NULL &&
          state.published_owner == local && state.failed_candidate == NULL && state.fields.s == 3 &&
          state.fields.t == 4 && state.fields.u == 5 && state.constructor_bytes != 0 &&
          state.constructor_allocations != 0);
    classes->nw.a = replacement; /* Borrowed genuine receiver; no native owner transfer. */
    CHECK(mc_client_nw_active_use(classes, caller, &current, &error) && current == replacement &&
          mc_client_nw_begin_quads(local, &error) &&
          mc_client_nw_vertex_uv(local, 1, 2, 3, .25, .5, &error) &&
          mc_client_nw_inspect(classes, &state) && state.a == replacement &&
          state.published_owner == local && state.fields.h == 1 && state.fields.o == 8 &&
          state.fields.g == local->fields.g && state.fields.g != replacement->fields.g);
    const mc_client_nw_fields replacement_prefix = replacement->fields;
    CHECK(mc_client_render_engine_classes_release(classes) == MC_CLIENT_RENDER_ENGINE_OK &&
          !memcmp(&replacement_prefix, &replacement->fields, sizeof(replacement_prefix)) &&
          replacement_classes->nw.published == replacement &&
          mc_client_nw_begin_quads(replacement, &error) &&
          mc_client_nw_vertex_uv(replacement, 6, 7, 8, .75, .5, &error) &&
          replacement->fields.h == 1 && replacement->fields.o == 8 &&
          mc_client_render_engine_classes_release(replacement_classes) ==
              MC_CLIENT_RENDER_ENGINE_OK);
    const mc_client_render_engine_options zero = {0};
    CHECK(mc_client_render_engine_classes_allocate_context(NULL, &zero, NULL, &failed) ==
          MC_CLIENT_RENDER_ENGINE_OK);
    current = NULL;
    CHECK(!mc_client_nw_active_use(failed, caller, &current, &error) && current == NULL &&
          failed->nw.phase == MC_CLIENT_RENDER_ENGINE_FAILED && failed->nw.published == NULL &&
          failed->nw.candidate != NULL);
    const mc_client_nw_fields failed_prefix = failed->nw.candidate->fields;
    CHECK(!mc_client_nw_begin_quads(failed->nw.candidate, &error) && error.native_policy &&
          error.status == MC_CLIENT_GRAPHICS_INVALID_PHASE &&
          !memcmp(&failed_prefix, &failed->nw.candidate->fields, sizeof(failed_prefix)) &&
          mc_client_render_engine_classes_release(failed) == MC_CLIENT_RENDER_ENGINE_OK);
    return 0;
}

static int arrow_cpu(const char *resources, FILE *output) {
    fixture runtime;
    CHECK(fixture_open(&runtime, resources) == 0);
    /* The usual client enters Block before TH's later GM static use. Keep
     * that genuine initializer outside the Arrow method under observation. */
    CHECK(mc_registry_classes_initialize_blocks(runtime.services.registry, NULL) == MC_REGISTRY_OK);
    mc_client_render_manager_classes *classes = NULL;
    mc_client_render_manager *manager = NULL;
    mc_client_manager_error *manager_error = calloc(1, sizeof(*manager_error));
    CHECK(manager_error);
    mc_client_manager_status manager_status = mc_client_render_manager_classes_bind(
        runtime.services.render_classes, runtime.services.registry, runtime.services.profile, NULL,
        &classes, manager_error);
    if (manager_status == MC_CLIENT_MANAGER_OK)
        manager_status = mc_client_render_manager_static_get(classes, &manager, manager_error);
    if (manager_status != MC_CLIENT_MANAGER_OK)
        fprintf(stderr,
                "Arrow CPU manager status=%u operation=%s source=%s pc=%d TH=%d model=%u "
                "registry=%u registry_operation=%s\n",
                (unsigned int)manager_status,
                manager_error->operation == NULL ? "<none>" : manager_error->operation,
                manager_error->source_class == NULL ? "<none>" : manager_error->source_class,
                manager_error->pc, manager_error->th_pc, (unsigned int)manager_error->model.status,
                (unsigned int)manager_error->registry.status,
                manager_error->registry.message == NULL ? "<none>"
                                                        : manager_error->registry.message);
    CHECK(manager_status == MC_CLIENT_MANAGER_OK);
    free(manager_error);
    mc_client_managed_renderer *renderer = arrow_renderer(manager);
    mc_entity *actor = NULL, *other = NULL;
    CHECK(renderer && renderer->primary == NULL && renderer->secondary == NULL &&
          mc_arrow_allocate(runtime.services.entities, &actor, NULL) == MC_ENTITY_OK &&
          mc_arrow_construct_world(actor, NULL, NULL) == MC_ENTITY_OK &&
          mc_falling_entity_allocate(runtime.services.entities, &other, NULL) == MC_ENTITY_OK);
    mc_client_draw_scope scope = {0}; /* No entered capture scope or service exists. */
    mc_client_living_draw_error *error = calloc(1, sizeof(*error));
    CHECK(error);
    const mc_client_graphics_site caller = {"fixture", "arrow", "()V", 0};
    CHECK(!mc_client_arrow_draw_entered(&scope, renderer, other, 0, 0, 0, 0, 0, caller, error) &&
          error->site.pc == 2 && error->source_exception && !error->native_policy &&
          !strcmp(error->exception_class, "java/lang/ClassCastException") &&
          !strcmp(error->site.descriptor, "(Lsn;DDDFF)V"));
    CHECK(!mc_client_arrow_draw_entered(&scope, renderer, NULL, 0, 0, 0, 0, 0, caller, error) &&
          error->state.arrow.bridge_cast_returned && error->site.pc == 1 &&
          error->parent.pc == 14 && error->source_exception &&
          !strcmp(error->exception_class, "java/lang/NullPointerException") &&
          !strcmp(error->site.descriptor, "(Lsl;DDDFF)V"));
    CHECK(
        !mc_client_arrow_draw_typed_entered(&scope, renderer, NULL, 0, 0, 0, 0, 0, caller, error) &&
        error->site.pc == 1 && error->parent.pc == -1 && !error->state.arrow.bridge_cast_returned &&
        error->source_exception);
    mc_arrow_entity_state *storage = actor->arrow;
    actor->arrow = NULL; /* Deliberate native representation fault, not a Java field. */
    CHECK(!mc_client_arrow_draw_entered(&scope, renderer, actor, 0, 0, 0, 0, 0, caller, error) &&
          error->site.pc == 1 && error->native_policy && !error->source_exception &&
          error->throwable == MC_CLIENT_LIVING_DRAW_NO_THROWABLE &&
          error->exception_class == NULL && error->child.pc == -1);
    actor->arrow = storage; /* End only the native control fault for owner disposal. */
    const uint32_t negative_zero = UINT32_C(0x80000000);
    actor->base.aU = 0;
    memcpy(&actor->base.aV, &negative_zero, sizeof(negative_zero));
    actor->arrow->fields.b = INT32_MAX;
    const mc_client_nw_namespace before = runtime.graphics->nw;
    const uint64_t entropy = actor->random->state;
    CHECK(mc_client_arrow_draw_entered(&scope, renderer, actor, INFINITY, NAN, -INFINITY, NAN, NAN,
                                       caller, error) &&
          error->site.pc == 18 && error->state.arrow.early_returned &&
          error->state.arrow.previous_yaw_read && error->state.arrow.previous_pitch_read &&
          !error->state.arrow.texture_returned && !error->state.arrow.matrix_pushed &&
          !error->state.arrow.nw_captured && !error->state.arrow.shake_read &&
          error->state.texture_call_count == 0 && error->selected_child == 0 &&
          error->throwable == MC_CLIENT_LIVING_DRAW_NO_THROWABLE &&
          error->exception_class == NULL && !error->source_exception && !error->native_policy &&
          !memcmp(&before, &runtime.graphics->nw, sizeof(before)) &&
          actor->random->state == entropy && scope.work == 0);
    actor->base.aU = NAN;
    CHECK(!mc_client_arrow_draw_entered(&scope, renderer, actor, 0, 0, 0, 0, 0, caller, error) &&
          !error->state.arrow.early_returned && error->state.arrow.previous_yaw_read &&
          !error->state.arrow.previous_pitch_read && error->state.texture_call_count == 1 &&
          error->selected_child == MC_CLIENT_LIVING_DRAW_CHILD_CAPTURE && error->native_policy &&
          error->throwable == MC_CLIENT_LIVING_DRAW_NO_THROWABLE && !error->source_exception &&
          error->exception_class == NULL);
    actor->base.aU = 0;
    actor->base.aV = NAN;
    CHECK(!mc_client_arrow_draw_entered(&scope, renderer, actor, 0, 0, 0, 0, 0, caller, error) &&
          !error->state.arrow.early_returned && error->state.arrow.previous_pitch_read &&
          error->state.texture_call_count == 1 &&
          error->selected_child == MC_CLIENT_LIVING_DRAW_CHILD_CAPTURE);
    CHECK(!mc_client_arrow_draw_entered(NULL, renderer, actor, 0, 0, 0, 0, 0, caller, error) &&
          error->native_policy && !error->source_exception && error->exception_class == NULL &&
          error->throwable == MC_CLIENT_LIVING_DRAW_NO_THROWABLE && !error->state.arrow.entered);
    CHECK(arrow_nw_cpu() == 0);
    fputs("{\"event\":\"cpu\",\"bridge_cast_pc\":2,\"null_pc\":1,\"storage_native\":true,"
          "\"signed_zero_return_pc\":18,\"services_reached\":false,\"reused_error_current\":true,"
          "\"nan_guard_proceeds\":true,\"guard_short_circuit\":true,\"nw_owned_local\":true,"
          "\"nw_borrowed_replacement_retired\":true,\"nw_failed_candidate_rejected\":true}\n",
          output);
    free(error);
    mc_entity_release(other);
    mc_entity_release(actor);
    CHECK(mc_client_render_manager_classes_retire(&classes, NULL) == MC_CLIENT_MANAGER_OK &&
          fixture_close(&runtime) == 0);
    return 0;
}

typedef struct arrow_batch {
    mc_client_nw_fields fields;
    uint32_t words[48];
    float modelview[16], projection[16], color[4], normal[3];
    size_t depth;
    GLenum mode;
    GLint first;
    GLsizei count;
    mc_client_texture texture;
    bool rescale;
} arrow_batch;

static mob_fixture *arrow_observed;
static mc_client_tessellator *arrow_local;
static mc_client_gl_draw_arrays_fn arrow_native_draw;
static mc_client_gl_bind_texture_fn arrow_native_bind;
static arrow_batch arrow_batches[6];
static size_t arrow_batch_count;
static bool arrow_overflow, arrow_replace_singleton, arrow_poison, arrow_live_angles;

static void GLAPIENTRY arrow_bind(GLenum target, GLuint texture) {
    arrow_native_bind(target, texture);
    if (target == GL_TEXTURE_2D && arrow_poison) {
        arrow_poison = false;
        _mm_setcsr(0x9fc0u); /* External host FP fault after the actual texture bind. */
    }
    if (target == GL_TEXTURE_2D && arrow_live_angles) {
        arrow_live_angles = false;
        arrow_observed->actor->base.aU = 350;
        arrow_observed->actor->base.yaw = 10;
        arrow_observed->actor->base.aV = -20;
        arrow_observed->actor->base.aT = 40;
    }
}

static void GLAPIENTRY arrow_capture(GLenum mode, GLint first, GLsizei count) {
    mob_fixture *f = arrow_observed;
    mc_client_nw_fields fields;
    if (!mc_client_nw_fields_get(arrow_local, &fields) || fields.o != 48 || !fields.g ||
        arrow_batch_count == MOB_COUNT(arrow_batches))
        arrow_overflow = true;
    else {
        arrow_batch *b = &arrow_batches[arrow_batch_count++];
        *b = (arrow_batch){0};
        b->fields = fields;
        memcpy(b->words, fields.g, sizeof(b->words));
        memcpy(b->modelview, f->display->source.modelview.column_major, sizeof(b->modelview));
        memcpy(b->projection, f->display->source.projection, sizeof(b->projection));
        memcpy(b->color, f->display->source.color, sizeof(b->color));
        memcpy(b->normal, f->display->source.fog.normal, sizeof(b->normal));
        b->depth = f->display->source.depth + 1;
        b->mode = mode;
        b->first = first;
        b->count = count;
        b->texture = f->display->source.bound_texture;
        b->rescale = f->display->source.fog.rescale_normal_enabled;
    }
    arrow_native_draw(mode, first, count);
    if (arrow_replace_singleton) {
        arrow_replace_singleton = false;
        /* The immutable native namespace occurrence still owns this local. */
        f->runtime.graphics->nw.a = NULL;
    }
}

static void arrow_hex_floats(FILE *out, const float *values, size_t count) {
    fputc('[', out);
    for (size_t i = 0; i < count; ++i)
        fprintf(out, "%s\"%08" PRIx32 "\"", i ? "," : "", mob_fbits(values[i]));
    fputc(']', out);
}

static int arrow_emit_batch(FILE *out, size_t ordinal, const arrow_batch *b) {
    fprintf(out,
            "{\"event\":\"batch\",\"ordinal\":%zu,\"mode\":%u,\"first\":%d,\"count\":%d,"
            "\"h\":%d,\"o\":%d,\"p\":%d,\"r\":%d,\"w\":%s,\"l\":%s,\"m\":%s,\"n\":%s,"
            "\"q\":%s,\"depth\":%zu,\"rescale\":%s,\"texture\":%u,\"translation\":["
            "\"%016" PRIx64 "\",\"%016" PRIx64 "\",\"%016" PRIx64 "\"],\"words\":[",
            ordinal, b->mode, b->first, b->count, b->fields.h, b->fields.o, b->fields.p,
            b->fields.r, b->fields.w ? "true" : "false", b->fields.l ? "true" : "false",
            b->fields.m ? "true" : "false", b->fields.n ? "true" : "false",
            b->fields.q ? "true" : "false", b->depth, b->rescale ? "true" : "false", b->texture,
            mob_dbits(b->fields.s), mob_dbits(b->fields.t), mob_dbits(b->fields.u));
    for (size_t i = 0; i < MOB_COUNT(b->words); ++i)
        fprintf(out, "%s\"%08" PRIx32 "\"", i ? "," : "", b->words[i]);
    fputs("],\"modelview\":", out);
    arrow_hex_floats(out, b->modelview, 16);
    fputs(",\"projection\":", out);
    arrow_hex_floats(out, b->projection, 16);
    fputs(",\"color\":", out);
    arrow_hex_floats(out, b->color, 4);
    fputs(",\"normal\":", out);
    arrow_hex_floats(out, b->normal, 3);
    fputs("}\n", out);
    return 0;
}

static int arrow_device(mob_fixture *f, const char *name) {
    CHECK(mob_prepare(f) == 0);
    mc_entity *actor = NULL;
    CHECK(mc_arrow_allocate(f->runtime.services.entities, &actor, NULL) == MC_ENTITY_OK &&
          mc_arrow_construct_position(actor, f->world, 8.5, 64, 8.5, NULL) == MC_ENTITY_OK);
    f->actor = actor;
    f->renderer = arrow_renderer(f->manager);
    CHECK(f->renderer && !f->renderer->primary && !f->renderer->secondary &&
          f->renderer->shadow_size == 0 && mob_frame(f, 0) == 0);
    actor->base.aU = 10;
    actor->base.yaw = 70;
    actor->base.aV = -5;
    actor->base.aT = 35;
    actor->arrow->fields.b = 0;
    mc_client_graphics_error graphics;
    CHECK(mc_client_nw_active_use(f->runtime.graphics, (mc_client_graphics_site){0}, &arrow_local,
                                  &graphics) &&
          arrow_local && mc_client_nw_translation(arrow_local, .125, -.25, .5, &graphics));
    /* Untouched NW slots remain meaningful bytes, not invented zero padding. */
    for (int32_t i = 0; i < 48; ++i)
        ((int32_t *)(void *)arrow_local->fields.g)[i] = (int32_t)UINT32_C(0x5a5aa5a5);
    mc_client_manager_draw_limits limits = mc_client_render_manager_draw_default_limits();
    mc_client_manager_draw_services services = {f->math, f->runtime.graphics, f->display,
                                                f->runtime.services.application_classes};
    bool returned = !strcmp(name, "ordinary") || !strcmp(name, "shake") ||
                    !strcmp(name, "replace_singleton") || !strcmp(name, "live_angles") ||
                    !strcmp(name, "guard");
    int32_t expected_pc = 550;
    bool source = false;
    if (!strcmp(name, "shake") || !strcmp(name, "missing_math"))
        actor->arrow->fields.b = 5;
    if (!strcmp(name, "missing_math")) {
        services.math = NULL;
        expected_pc = 194;
    } else if (!strcmp(name, "work_zero")) {
        limits.max_work = 1; /* TH lookup consumes one; first source vertex cannot enter. */
        expected_pc = 264;
    } else if (!strcmp(name, "work_vertex")) {
        limits.max_work = 2;
        expected_pc = 284;
    } else if (!strcmp(name, "preexisting_batch")) {
        CHECK(mc_client_nw_begin_quads(arrow_local, &graphics) &&
              mc_client_nw_vertex_uv(arrow_local, 3, 4, 5, .25, .75, &graphics));
        expected_pc = 244;
        source = true;
    } else if (!strcmp(name, "null_singleton")) {
        f->runtime.graphics->nw.a = NULL;
        expected_pc = 244;
        source = true;
    } else if (!strcmp(name, "draw_buffer_null")) {
        arrow_local->fields.e = NULL;
        expected_pc = 329;
        source = true;
    } else if (!strcmp(name, "null_engine")) {
        f->manager->fields.e = NULL;
        expected_pc = 11;
        source = true;
    } else if (!strcmp(name, "guard") || !strcmp(name, "guard_post_null")) {
        actor->base.aU = actor->base.aV = 0;
        expected_pc = 18;
        if (!strcmp(name, "guard_post_null")) {
            f->manager->fields.k = NULL;
            source = true;
        }
    }
    arrow_observed = f;
    arrow_native_draw = f->display->gl.DrawArrays;
    f->display->gl.DrawArrays = arrow_capture;
    arrow_native_bind = f->display->gl.BindTexture;
    f->display->gl.BindTexture = arrow_bind;
    arrow_replace_singleton = !strcmp(name, "replace_singleton");
    arrow_live_angles = !strcmp(name, "live_angles");
    if (!strcmp(name, "fp_after_bind")) {
        /* Use an actual cached authored texture so the fault follows source
         * BW/JI bind rather than an earlier upload-service bind. */
        uint16_t units[128];
        mc_client_texture texture;
        CHECK(mc_client_render_engine_texture(f->app->render_engine, f->display, NULL,
                                              mob_text("/item/arrows.png", units), &texture,
                                              f->lookup_error) == MC_CLIENT_LOOKUP_OK);
        arrow_poison = true;
        expected_pc = 29;
    }
    const mc_client_display_info *info = mc_client_display_info_get(f->display);
    CHECK(info);
    fputs("{\"event\":\"device\",\"vendor\":", f->output);
    mob_string(f->output, info->vendor);
    fputs(",\"renderer\":", f->output);
    mob_string(f->output, info->renderer);
    fputs(",\"version\":", f->output);
    mob_string(f->output, info->version);
    fputs("}\n{\"event\":\"input\",\"angles\":", f->output);
    const float angles[] = {actor->base.aU, actor->base.yaw, actor->base.aV, actor->base.aT};
    arrow_hex_floats(f->output, angles, 4);
    fprintf(f->output, ",\"shake\":%d,\"partial\":\"3ec00000\",\"yaw_argument\":\"7fc00000\"}\n",
            actor->arrow->fields.b);
    const size_t actor_holds = actor->holds, renderer_holds = f->renderer->holds,
                 manager_holds = f->manager->holds;
    const uint64_t entropy = actor->random->state;
    const unsigned int fp = _mm_getcsr();
    const mc_client_manager_status status = mc_client_render_manager_coordinates_draw_begin(
        f->manager, actor, .25, .5, -.75, NAN, .375f, &services, &limits, &f->call, f->draw_error);
    if (!strcmp(name, "fp_after_bind")) {
        _mm_setcsr(fp); /* End external fault only; never undo a source prefix. */
        CHECK(!arrow_poison);
    }
    f->display->gl.DrawArrays = arrow_native_draw;
    f->display->gl.BindTexture = arrow_native_bind;
    CHECK(f->call && !arrow_overflow && (status == MC_CLIENT_MANAGER_OK) == returned &&
          actor->random->state == entropy);
    mc_client_manager_draw_state state;
    CHECK(mc_client_render_manager_draw_inspect(f->call, &state) && state.coordinates_only &&
          !state.brightness_returned && !state.color_returned && state.living && state.post_draw);
    const mc_client_arrow_draw_state *s = &state.living->arrow;
    CHECK(s->entered && state.living->source_pc == (unsigned int)expected_pc &&
          s->returned == (returned || !strcmp(name, "guard_post_null")));
    if (!strcmp(name, "guard_post_null"))
        CHECK(state.primary_returned && !state.post_draw_returned &&
              f->draw_error->living == &f->call->post && f->draw_error->living->site.pc == 7);
    else if (!returned)
        CHECK(f->draw_error->living == &f->call->living &&
              f->draw_error->source_exception == source &&
              f->draw_error->native_policy == !source && !s->rescale_disabled && !s->matrix_popped);
    if (returned && strcmp(name, "guard")) {
        CHECK(s->draws == 6 && s->begins == 6 && s->normals == 6 && s->vertices == 24 &&
              s->shaft_rotations == 4 && s->loop_tests == 5 && arrow_batch_count == 6 &&
              s->matrix_popped && s->rescale_disabled && state.primary_returned &&
              state.post_draw_returned && mob_fbits(s->scale) == UINT32_C(0x3d666666));
        CHECK(mob_fbits(s->yaw) ==
              (!strcmp(name, "live_angles") ? UINT32_C(0x43048000) : UINT32_C(0xc2660000)));
        for (size_t i = 0; i < 6; ++i)
            CHECK(s->nw_identity == (uintptr_t)arrow_local && s->begin_returned[i] &&
                  s->draw_returned[i] && s->nw_words_before_draw[i] == 48 &&
                  s->nw_vertices_before_draw[i] == 6 && !s->nw_writing_after_draw[i]);
    }
    if (!strcmp(name, "preexisting_batch"))
        CHECK(s->begin_attempts == 1 && !s->begins && s->normals == 1 && arrow_local->fields.w &&
              arrow_local->fields.h == 1 && arrow_local->fields.o == 8 &&
              f->draw_error->living->child.pc == 16 &&
              !strcmp(f->draw_error->exception_class, "java/lang/IllegalStateException"));
    if (!strcmp(name, "null_singleton"))
        CHECK(s->nw_captured && !s->nw_identity && s->tail_translated && s->normals == 1 &&
              s->begin_attempts == 1 && !s->begins &&
              !strcmp(f->draw_error->exception_class, "java/lang/NullPointerException"));
    if (!strcmp(name, "draw_buffer_null"))
        CHECK(s->vertices == 4 && s->draw_attempts == 1 && !s->draws && !arrow_local->fields.w &&
              arrow_local->fields.h == 6 && arrow_local->fields.o == 48 &&
              f->draw_error->living->child.pc == 33);
    if (!strcmp(name, "work_zero") || !strcmp(name, "work_vertex"))
        CHECK(s->begins == 1 && !s->draw_attempts && arrow_local->fields.w &&
              s->vertices == (!strcmp(name, "work_zero") ? 0u : 1u));
    if (!strcmp(name, "live_angles"))
        CHECK(s->guard_yaw == 10 && s->yaw_reads[0] == 350 && s->yaw_reads[2] == 350 &&
              s->pitch_reads[0] == -20 && s->pitch_reads[2] == -20 && s->pitch == 2.5f);
    for (size_t i = 0; i < arrow_batch_count; ++i)
        CHECK(arrow_emit_batch(f->output, i, &arrow_batches[i]) == 0);
    if (state.living->texture_call_count != 0) {
        const mc_client_living_draw_texture_call *t = &state.living->texture_calls[0];
        static const uint16_t key[] = {'/', 'i', 't', 'e', 'm', '/', 'a', 'r',
                                       'r', 'o', 'w', 's', '.', 'p', 'n', 'g'};
        CHECK(state.living->texture_call_count == 1 && t->key_length == MOB_COUNT(key) &&
              t->key_stored == MOB_COUNT(key) && !memcmp(t->key_units, key, sizeof(key)) &&
              t->caller.pc == 22 && !strcmp(t->caller.class_name, "mc") &&
              t->lookup_site.pc == 11 && t->bind_site.pc == 14);
        fprintf(f->output,
                "{\"event\":\"texture\",\"key\":\"/item/arrows.png\",\"returned\":%s,"
                "\"name\":%d,\"caller_pc\":22,\"lookup_pc\":11,\"bind_pc\":14",
                t->returned ? "true" : "false", t->texture_id);
        if (t->returned) {
            mc_client_texture_source_state texture;
            CHECK(mc_client_display_texture_state_inspect(
                      f->display, (mc_client_texture)t->texture_id, &texture) &&
                  texture.known && texture.image_uploaded && texture.wrap_s == GL_REPEAT &&
                  texture.wrap_t == GL_REPEAT && texture.min_filter == GL_NEAREST &&
                  texture.mag_filter == GL_NEAREST);
            const mc_client_direct_byte_buffer *transfer = f->app->render_engine->fields.f;
            CHECK(transfer && transfer->limit == 32u * 32u * 4u);
            fputs(",\"transfer_hex\":\"", f->output);
            for (size_t i = 0; i < transfer->limit; ++i)
                fprintf(f->output, "%02x", transfer->data[i]);
            fputc('"', f->output);
        }
        fputs("}\n", f->output);
    }
    fprintf(f->output,
            "{\"event\":\"result\",\"name\":\"%s\",\"returned\":%s,\"pc\":%u,"
            "\"source_exception\":%s,\"native_policy\":%s,\"vertices\":%zu,\"draws\":%zu,"
            "\"depth\":%zu,\"rescale\":%s,\"nw_h\":%d,\"nw_o\":%d,\"nw_w\":%s,"
            "\"texture_calls\":%zu,\"scale\":\"%08" PRIx32 "\",\"yaw\":\"%08" PRIx32 "\","
            "\"pitch\":\"%08" PRIx32 "\",\"shake_math\":%s,\"primary_returned\":%s,"
            "\"post_returned\":%s,\"normal\":",
            name, returned ? "true" : "false", state.living->source_pc,
            f->draw_error->source_exception ? "true" : "false",
            f->draw_error->native_policy ? "true" : "false", s->vertices, s->draws,
            f->display->source.depth + 1,
            f->display->source.fog.rescale_normal_enabled ? "true" : "false", arrow_local->fields.h,
            arrow_local->fields.o, arrow_local->fields.w ? "true" : "false",
            state.living->texture_call_count, mob_fbits(s->scale), mob_fbits(s->yaw),
            mob_fbits(s->pitch), s->shake_math_returned ? "true" : "false",
            state.primary_returned ? "true" : "false", state.post_draw_returned ? "true" : "false");
    arrow_hex_floats(f->output, f->display->source.fog.normal, 3);
    const mc_client_source_state prefix = f->display->source;
    const mc_client_nw_fields nw_prefix = arrow_local->fields;
    int32_t words[48];
    memcpy(words, arrow_local->fields.g, sizeof(words));
    CHECK(mc_client_render_manager_draw_retire(&f->call, f->draw_error) == MC_CLIENT_MANAGER_OK &&
          !f->call && !memcmp(&prefix, &f->display->source, sizeof(prefix)) &&
          !memcmp(&nw_prefix, &arrow_local->fields, sizeof(nw_prefix)) &&
          !memcmp(words, arrow_local->fields.g, sizeof(words)) && actor->holds == actor_holds &&
          f->renderer->holds == renderer_holds && f->manager->holds == manager_holds);
    fputs(",\"checked_retired\":true,\"prefix_preserved\":true}\n", f->output);
    mc_entity_release(actor);
    f->actor = NULL;
    return 0;
}

int main(int argc, char **argv) {
    CHECK(argc == 4 || argc == 5 || argc == 6);
    const bool cpu = argc == 4 && !strcmp(argv[1], "--cpu");
    CHECK(cpu || argc == 5 || argc == 6);
    const char *directory = cpu ? argv[3] : argv[4];
    char path[4096];
    CHECK(snprintf(path, sizeof(path), "%s/arrow.ndjson", directory) > 0);
    FILE *output = fopen(path, "wb");
    CHECK(output);
    if (cpu)
        CHECK(arrow_cpu(argv[2], output) == 0);
    else {
        SDL_SetMainReady();
        mob_fixture *f = calloc(1, sizeof(*f));
        CHECK(f);
        f->output = output;
        CHECK(mob_open(f, argv[1], argv[2], argv[3]) == 0 &&
              arrow_device(f, argc == 6 ? argv[5] : "ordinary") == 0 && mob_close(f) == 0);
        free(f);
    }
    CHECK(!ferror(output) && fclose(output) == 0);
    printf("Arrow draw %s: %u checks;checked terminal cleanup\n",
           cpu         ? "cpu"
           : argc == 6 ? argv[5]
                       : "ordinary",
           checks);
    return 0;
}
