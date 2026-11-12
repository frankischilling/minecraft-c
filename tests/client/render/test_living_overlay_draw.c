/* The ordinary prepared creature fixture supplies genuine owners and authored
 * Worlds. Its baseline entry remains compiled but is not executed here. */
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#include "client/display/display.h"
static bool original_framebuffer_create(const char *, int, int, bool, mc_client_display **,
                                        mc_client_display_error *);
int mc_living_overlay_creature_fixture_main(int argc, char **argv);
#define MC_CREATURE_DRAW_MAIN mc_living_overlay_creature_fixture_main
#define MC_CREATURE_DISPLAY_CREATE original_framebuffer_create
#include "test_creature_draw.c"
/* As in the device setup contracts, compile the real Display implementation
 * once in this executable. Interceptions still call its actual native GL. */
#include "../../../src/client/display/display.c"

static bool original_framebuffer_create(const char *title, int width, int height, bool hidden,
                                        mc_client_display **out, mc_client_display_error *error) {
    /* Same native framebuffer request as SDK create(), without claiming an
     * application startup or publishing invented SDK/Canvas source fields. */
    return create_display_profile(title, width, height, hidden, DISPLAY_FRAMEBUFFER_SDK_DEFAULT,
                                  out, error);
}

typedef struct overlay_batch {
    GLint depth;
    GLsizei vertices;
    float color[4];
    bool texture, alpha;
} overlay_batch;

static mc_client_gl_draw_arrays_fn overlay_native_draw;
static mc_client_display *overlay_display;
static overlay_batch overlay_batches[1024];
static size_t overlay_count;
static bool overlay_overflow;
static mc_client_gl_depth_function_fn overlay_native_depth;
static bool overlay_poison_fp;

static void GLAPIENTRY overlay_depth(GLenum function) {
    overlay_native_depth(function);
    if (function == GL_EQUAL && overlay_poison_fp) {
        overlay_poison_fp = false;
        _mm_setcsr(0x9fc0u); /* Authored native-host FTZ/DAZ failure, not a game input. */
    }
}

static void GLAPIENTRY overlay_draw(GLenum mode, GLint first, GLsizei count) {
    GLint depth = 0;
    overlay_display->gl.GetIntegerv(GL_DEPTH_FUNC, &depth);
    if (depth == GL_EQUAL) {
        if (overlay_count == MOB_COUNT(overlay_batches))
            overlay_overflow = true;
        else {
            overlay_batch *b = &overlay_batches[overlay_count++];
            b->depth = depth;
            b->vertices = count;
            memcpy(b->color, overlay_display->source.color, sizeof(b->color));
            b->texture = overlay_display->source.texture_enabled;
            b->alpha = overlay_display->source.alpha_test;
        }
    }
    overlay_native_draw(mode, first, count); /* Observe, never replace a draw. */
}

static int overlay_receipt(mob_fixture *f, unsigned int index, const char *name,
                           const mc_client_living_overlay_state *s, bool expected, bool secondary) {
    CHECK(s->entered == expected && s->primary_attempted == expected &&
          s->primary_returned == expected && s->returned == expected);
    if (expected) {
        CHECK(s->primary.model == f->renderer->primary && s->primary.returned &&
              s->primary.pose_returned && s->primary.compiled_parts == 0 &&
              s->primary.part_calls == s->primary.part_returns);
        CHECK(s->primary.argument_count == 6);
        for (size_t pass = 0; pass < 4; ++pass) {
            CHECK(s->pass_attempted[pass] && s->pass_result[pass] == (secondary && pass == 0) &&
                  s->pass_render_returned[pass] == (secondary && pass == 0));
            if (s->pass_render_returned[pass])
                CHECK(s->pass_render[pass].model == f->renderer->secondary &&
                      s->pass_render[pass].returned && s->pass_render[pass].compiled_parts == 0 &&
                      s->pass_render[pass].argument_count == 6);
        }
    }
    fprintf(f->output,
            "{\"event\":\"overlay\",\"case\":%u,\"kind\":\"%s\",\"entered\":%s,\"rgba\":[", index,
            name, expected ? "true" : "false");
    for (size_t i = 0; i < 4; ++i)
        fprintf(f->output, "%s\"%08" PRIx32 "\"", i ? "," : "", mob_fbits(s->rgba[i]));
    fprintf(f->output, "],\"primary_parts\":%u,\"primary_model\":", s->primary.part_calls);
    mob_ref(f->output, s->primary.model, s->primary.runtime_class);
    fputs(",\"primary_arguments\":[", f->output);
    for (size_t i = 0; i < s->primary.argument_count; ++i)
        fprintf(f->output, "%s\"%08" PRIx32 "\"", i ? "," : "", mob_fbits(s->primary.arguments[i]));
    fputs("],\"passes\":[", f->output);
    for (size_t i = 0; i < 4; ++i)
        fprintf(f->output, "%s%s", i ? "," : "", s->pass_result[i] ? "true" : "false");
    fputs("],\"secondary_models\":[", f->output);
    for (size_t i = 0; i < 4; ++i) {
        if (i)
            fputc(',', f->output);
        mob_ref(f->output, s->pass_render[i].model, s->pass_render[i].runtime_class);
    }
    fputs("],\"secondary_arguments\":[", f->output);
    for (size_t i = 0; i < 4; ++i) {
        fprintf(f->output, "%s[", i ? "," : "");
        for (size_t j = 0; j < s->pass_render[i].argument_count; ++j)
            fprintf(f->output, "%s\"%08" PRIx32 "\"", j ? "," : "",
                    mob_fbits(s->pass_render[i].arguments[j]));
        fputc(']', f->output);
    }
    fputs("]}\n", f->output);
    return 0;
}

static int overlay_case(mob_fixture *f, unsigned int index, const char *directory) {
    static const unsigned int profiles[] = {0, 2, 4, 11, 11, 12, 0};
    CHECK(mob_select(f, profiles[index]) == 0 && mob_frame(f, index) == 0);
    mc_living_entity_fields *living = &f->actor->mob->living;
    const bool hurt = index < 3 || index == 4;
    const bool packed = index == 3 || index == 4;
    living->aa = index == 6 ? -1 : (hurt && index != 2 ? 5 : 0);
    living->ad = index == 6 ? -1 : (index == 2 ? 5 : 0);
    if (strcmp(f->kind, "gb") == 0) {
        f->actor->mob->creeper_a = packed ? 6 : 0;
        f->actor->mob->creeper_b = packed ? 5 : 0;
    }
    const int32_t aa = living->aa, ad = living->ad;
    CHECK(mob_pixels(f, directory, index, true) == 0);
    CHECK(mob_emit_state(f, index, "before") == 0);
    const uint64_t own = f->actor->random->state, camera = f->camera->random->state,
                   world = f->world->fields.r->state,
                   shared = f->runtime.services.profile->math->state;
    overlay_count = 0;
    overlay_overflow = false;
    overlay_display = f->display;
    overlay_native_draw = f->display->gl.DrawArrays;
    f->display->gl.DrawArrays = overlay_draw;
    const mc_client_manager_draw_services services = {f->math, f->runtime.graphics, f->display,
                                                      f->runtime.services.application_classes};
    const mc_client_manager_status result = mc_client_render_manager_draw_begin(
        f->manager, f->actor, .375f, &services, NULL, &f->call, f->draw_error);
    f->display->gl.DrawArrays = overlay_native_draw;
    if (result != MC_CLIENT_MANAGER_OK)
        fprintf(stderr, "Overlay case%u: %s.%s:%d %s\n", index, f->draw_error->site.class_name,
                f->draw_error->site.method, f->draw_error->site.pc, f->draw_error->operation);
    CHECK(result == MC_CLIENT_MANAGER_OK && f->call && !overlay_overflow);
    CHECK(mob_pixels(f, directory, index, false) == 0);
    mc_client_manager_draw_state state;
    CHECK(mc_client_render_manager_draw_inspect(f->call, &state) && state.returned &&
          state.living && state.living->returned && state.post_draw_returned);
    const mc_client_living_draw_state *s = state.living;
    CHECK(s->overlay_state_restored == (hurt || packed));
    CHECK(overlay_receipt(f, index, "hurt", &s->hurt_overlay, hurt, index < 2) == 0);
    CHECK(overlay_receipt(f, index, "packed", &s->color_overlay, packed, false) == 0);
    if (hurt)
        CHECK(s->hurt_overlay.rgba[1] == 0 && s->hurt_overlay.rgba[2] == 0 &&
              mob_fbits(s->hurt_overlay.rgba[3]) == mob_fbits(.4f));
    if (packed)
        CHECK((uint32_t)s->color == UINT32_C(0x09ffffff) && s->color_overlay.rgba[0] == 1 &&
              s->color_overlay.rgba[1] == 1 && s->color_overlay.rgba[2] == 1);
    CHECK((hurt || packed) == (overlay_count != 0));
    for (size_t i = 0; i < overlay_count; ++i)
        CHECK(overlay_batches[i].depth == GL_EQUAL && overlay_batches[i].vertices == 6 &&
              !overlay_batches[i].texture && !overlay_batches[i].alpha);
    GLint depth;
    f->display->gl.GetIntegerv(GL_DEPTH_FUNC, &depth);
    CHECK(depth == GL_LEQUAL && f->display->source.texture_enabled &&
          f->display->source.alpha_test);
    CHECK(f->actor->random->state == own && f->camera->random->state == camera &&
          f->world->fields.r->state == world && f->runtime.services.profile->math->state == shared);
    CHECK(living->aa == aa && living->ad == ad);
    fprintf(f->output,
            "{\"event\":\"draw\",\"case\":%u,\"aa\":%d,\"ad\":%d,\"color\":%d,"
            "\"batches\":%zu,\"gl\":",
            index, aa, ad, s->color, overlay_count);
    CHECK(mob_emit_gl(f) == 0);
    if (index == 0) {
        const mc_client_display_info *device = mc_client_display_info_get(f->display);
        CHECK(device != NULL);
        fputs(",\"device\":{\"vendor\":", f->output);
        mob_string(f->output, device->vendor);
        fputs(",\"renderer\":", f->output);
        mob_string(f->output, device->renderer);
        fputs(",\"version\":", f->output);
        mob_string(f->output, device->version);
        fprintf(f->output, ",\"major\":%d,\"minor\":%d,\"profile\":%d}", device->gl_major,
                device->gl_minor, device->profile);
        mc_client_framebuffer_info format;
        CHECK(mc_client_display_framebuffer_info_get(f->display, &format, NULL) &&
              format.alpha_bits == 0 && format.depth_bits > 0);
        fprintf(f->output,
                ",\"framebuffer\":{\"red\":%d,\"green\":%d,\"blue\":%d,\"alpha\":%d,"
                "\"depth\":%d,\"stencil\":%d,\"double_buffer\":%s}",
                format.red_bits, format.green_bits, format.blue_bits, format.alpha_bits,
                format.depth_bits, format.stencil_bits, format.double_buffer ? "true" : "false");
    }
    fputs("}\n", f->output);
    uint32_t gl_error = UINT32_MAX;
    CHECK(mc_client_display_source_get_error(f->display, &gl_error, NULL) && gl_error == 0);
    CHECK(mob_emit_state(f, index, "after") == 0);
    CHECK(mc_client_render_manager_draw_retire(&f->call, f->draw_error) == MC_CLIENT_MANAGER_OK &&
          f->call == NULL);
    return 0;
}

static int zero_alpha_control(mob_fixture *f) {
    CHECK(mob_select(f, 12) == 0 && mob_frame(f, 0) == 0);
    f->actor->mob->living.aa = f->actor->mob->living.ad = 0;
    f->actor->mob->creeper_a = f->actor->mob->creeper_b = -14;
    const mc_client_manager_draw_services services = {f->math, f->runtime.graphics, f->display,
                                                      f->runtime.services.application_classes};
    CHECK(mc_client_render_manager_draw_begin(f->manager, f->actor, .375f, &services, NULL,
                                              &f->call, f->draw_error) == MC_CLIENT_MANAGER_OK);
    mc_client_manager_draw_state state;
    CHECK(mc_client_render_manager_draw_inspect(f->call, &state) && state.returned &&
          state.living && (uint32_t)state.living->color == UINT32_C(0x00ffffff) &&
          !state.living->hurt_overlay.entered && !state.living->color_overlay.entered &&
          !state.living->overlay_state_restored);
    CHECK(mc_client_render_manager_draw_retire(&f->call, f->draw_error) == MC_CLIENT_MANAGER_OK);
    fputs("{\"event\":\"control\",\"name\":\"zero_alpha_white_rgb\",\"returned\":true}\n",
          f->output);
    return 0;
}

static int pixel_control(mob_fixture *f) {
    CHECK(mob_select(f, 0) == 0);
    f->actor->mob->living.ad = 0;
    const size_t bytes = 256u * 256u * 4u;
    uint8_t *before = malloc(bytes), *after = malloc(bytes);
    CHECK(before && after);
    mob_read_fn read = (mob_read_fn)SDL_GL_GetProcAddress("glReadPixels");
    mob_finish_fn finish = (mob_finish_fn)SDL_GL_GetProcAddress("glFinish");
    CHECK(read && finish);
    const mc_client_manager_draw_services services = {f->math, f->runtime.graphics, f->display,
                                                      f->runtime.services.application_classes};
    for (unsigned int i = 0; i < 2; ++i) {
        f->actor->mob->living.aa = i ? 5 : 0;
        CHECK(mob_frame(f, 0) == 0);
        CHECK(mc_client_render_manager_draw_begin(f->manager, f->actor, .375f, &services, NULL,
                                                  &f->call, f->draw_error) == MC_CLIENT_MANAGER_OK);
        finish();
        read(0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, i ? after : before);
        CHECK(mc_client_render_manager_draw_retire(&f->call, f->draw_error) ==
              MC_CLIENT_MANAGER_OK);
    }
    size_t changed = 0, redder = 0, unchanged = 0;
    for (size_t i = 0; i < bytes; i += 4) {
        if (memcmp(before + i, after + i, 4) == 0)
            ++unchanged;
        else {
            ++changed;
            if (after[i] >= before[i] && after[i + 1] < before[i + 1])
                ++redder;
        }
    }
    CHECK(changed > 16 && redder > 16 && unchanged > 10000);
    fprintf(f->output,
            "{\"event\":\"control\",\"name\":\"visible_hurt_pixels\",\"changed\":%zu,"
            "\"redder\":%zu,\"unchanged\":%zu}\n",
            changed, redder, unchanged);
    free(after);
    free(before);
    return 0;
}

static int floating_prefix_control(mob_fixture *f) {
    CHECK(mob_select(f, 0) == 0 && mob_frame(f, 0) == 0);
    f->actor->mob->living.aa = 5;
    f->actor->mob->living.ad = 0;
    const mc_client_manager_draw_services services = {f->math, f->runtime.graphics, f->display,
                                                      f->runtime.services.application_classes};
    const unsigned int controls = _mm_getcsr();
    overlay_native_depth = f->display->gl.DepthFunc;
    overlay_poison_fp = true;
    f->display->gl.DepthFunc = overlay_depth;
    const mc_client_manager_status result = mc_client_render_manager_draw_begin(
        f->manager, f->actor, .375f, &services, NULL, &f->call, f->draw_error);
    /* End the external native-host fault after the one call. No source field,
     * draw phase, timer, random stream or model owner is repaired or retried. */
    _mm_setcsr(controls);
    f->display->gl.DepthFunc = overlay_native_depth;
    CHECK(!overlay_poison_fp && result != MC_CLIENT_MANAGER_OK && f->call &&
          f->draw_error->native_policy && !f->draw_error->source_exception &&
          f->draw_error->living && f->draw_error->living->site.pc == 483 &&
          f->draw_error->living->status == MC_CLIENT_LIVING_DRAW_STATE);
    const mc_client_living_draw_state *s = &f->draw_error->living->state;
    CHECK(s->hurt_overlay.entered && !s->hurt_overlay.primary_attempted &&
          !s->hurt_overlay.primary_returned && !s->overlay_state_restored);
    GLint depth = 0;
    f->display->gl.GetIntegerv(GL_DEPTH_FUNC, &depth);
    CHECK(depth == GL_EQUAL && !f->display->source.texture_enabled &&
          !f->display->source.alpha_test && f->display->source.depth == 1);
    CHECK(mc_client_render_manager_draw_retire(&f->call, f->draw_error) == MC_CLIENT_MANAGER_OK &&
          f->call == NULL && f->display->source.depth == 1);
    fputs("{\"event\":\"control\",\"name\":\"floating_prefix\",\"pc\":483,"
          "\"source_prefix_preserved\":true}\n",
          f->output);
    return 0;
}

int main(int argc, char **argv) {
    CHECK(argc == 5);
    SDL_SetMainReady();
    mob_fixture *f = calloc(1, sizeof(*f));
    CHECK(f);
    char path[4096];
    CHECK(snprintf(path, sizeof(path), "%s/overlay.ndjson", argv[4]) > 0);
    f->output = fopen(path, "wb");
    CHECK(f->output && mob_open(f, argv[1], argv[2], argv[3]) == 0 && mob_textures(f) == 0 &&
          mob_prepare(f) == 0);
    for (unsigned int index = 0; index < 7; ++index)
        CHECK(overlay_case(f, index, argv[4]) == 0);
    CHECK(zero_alpha_control(f) == 0 && pixel_control(f) == 0 && floating_prefix_control(f) == 0);
    CHECK(!ferror(f->output) && fclose(f->output) == 0 && mob_close(f) == 0);
    free(f);
    printf("Living overlays: %u checks;7 genuine profiles;3 native controls\n", checks);
    return 0;
}
