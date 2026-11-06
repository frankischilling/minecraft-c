#define MC_TEXTURE_EFFECT_CPU_MAIN mc_effect_unused_cpu_fixture_main
#include "test_effects.c"
#undef MC_TEXTURE_EFFECT_CPU_MAIN
#include "client/application_state.h"
#include "client/render_engine/effects.h"

static bool registrations(fx_effects *f) {
    mc_client_render_engine_classes *classes = NULL;
    mc_client_default_font *font = NULL;
    mc_client_render_engine *engine = NULL;
    NEED(mc_client_render_engine_classes_create(NULL, &classes) == MC_CLIENT_RENDER_ENGINE_OK);
    NEED(mc_client_default_font_create(32u * 1024u * 1024u, &font) == MC_CLIENT_FONT_OK);
    NEED(mc_client_render_engine_allocate(classes, font, NULL, &engine) ==
         MC_CLIENT_RENDER_ENGINE_OK);
    NEED(mc_client_render_engine_construct(engine, NULL, NULL, NULL) == MC_CLIENT_RENDER_ENGINE_OK);
    mc_client_effect_registration_error *e = calloc(1, sizeof(*e));
    NEED(e != NULL);
    mc_client_water_texture_effect *water = NULL;
    mc_client_lava_texture_effect *lava = NULL;
    NEED(mc_client_water_texture_allocate(f->registry, f->profile, NULL, &water, NULL) ==
         MC_WATER_TEXTURE_OK);
    NEED(mc_client_water_texture_construct(water, NULL) == MC_WATER_TEXTURE_OK);
    NEED(mc_client_lava_texture_allocate(f->registry, f->profile, f->trig, NULL, &lava, NULL) ==
         MC_LAVA_TEXTURE_OK);
    NEED(mc_client_lava_texture_construct(lava, NULL) == MC_LAVA_TEXTURE_OK);
    /* Native prepared-state fixture on the SAME complete App storage. It is
     * RAW with a declared stable borrow, not a completed Minecraft ctor. */
    client_application *app = calloc(1, sizeof(*app));
    NEED(app != NULL);
    app->water_effect = water;
    app->ae = lava;
    mc_client_texture_effect *ordered[9] = {mc_client_lava_texture_as_effect(app->ae),
                                            mc_client_water_texture_as_effect(app->water_effect)};
    const mc_texture_effect_kind kinds[] = {
        MC_TEXTURE_EFFECT_PORTAL,        MC_TEXTURE_EFFECT_COMPASS,      MC_TEXTURE_EFFECT_WATCH,
        MC_TEXTURE_EFFECT_FLOWING_WATER, MC_TEXTURE_EFFECT_FLOWING_LAVA, MC_TEXTURE_EFFECT_FIRE,
        MC_TEXTURE_EFFECT_FIRE};
    const mc_texture_effect_services service = services(f);
    for (size_t n = 0; n < 7; ++n) {
        NEED(mc_client_texture_effect_allocate(&service, kinds[n], NULL, &ordered[n + 2],
                                               &e->child) == MC_TEXTURE_EFFECT_OK);
        NEED(mc_client_texture_effect_construct(ordered[n + 2], app, n == 6 ? 1 : 0, &e->child) ==
             MC_TEXTURE_EFFECT_OK);
    }
    for (size_t n = 0; n < 9; ++n) {
        NEED(mc_client_render_engine_register_effect(engine, ordered[n], 0, e));
        NEED(e->appended && e->child_returned && e->source_returned && e->ji_pc == 15);
        const mc_client_texture_effect *actual = NULL;
        NEED(mc_client_render_engine_effect_at(engine, n, &actual) && actual == ordered[n]);
        size_t count;
        NEED(mc_client_render_list_count(engine->fields.g, &count) && count == n + 1);
    }
    NEED(ordered[0] == mc_client_lava_texture_as_effect(app->ae) &&
         ordered[1] == mc_client_water_texture_as_effect(app->water_effect));
    NEED(ordered[7] != ordered[8]);
    mc_texture_effect_state fire0, fire1;
    NEED(mc_client_texture_effect_inspect(ordered[7], &fire0) &&
         mc_client_texture_effect_inspect(ordered[8], &fire1));
    NEED(fire0.g != fire1.g && fire0.h != fire1.h &&
         fire1.fields.b == mc_java_i32_add(fire0.fields.b, 16));
    NEED(mc_client_render_engine_register_effect(engine, ordered[2], 0, e));
    NEED(!mc_client_render_engine_register_effect(engine, NULL, 0, e));
    NEED(e->appended && !e->source_returned && e->ji_pc == 12 &&
         e->child.status == MC_TEXTURE_EFFECT_NULL_REFERENCE);
    size_t count;
    NEED(mc_client_render_list_count(engine->fields.g, &count) && count == 11);
    NEED(!mc_client_render_engine_register_effect(engine, ordered[2], 11, e) && !e->appended);
    NEED(mc_client_render_list_count(engine->fields.g, &count) && count == 11);
    mc_client_texture_effect *raw = NULL;
    NEED(mc_client_texture_effect_allocate(&service, MC_TEXTURE_EFFECT_FIRE, NULL, &raw,
                                           &e->child) == MC_TEXTURE_EFFECT_OK);
    NEED(!mc_client_render_engine_register_effect(engine, raw, 0, e) && e->appended &&
         e->ji_pc == 12 && e->child.status == MC_TEXTURE_EFFECT_INVALID_PHASE);
    NEED(mc_client_texture_effect_release(&raw, &e->child) == MC_TEXTURE_EFFECT_OK && raw == NULL);
    /* Real immutable Math-owner quota, as in the existing water regression.
     * No Random callback, phase substitution or fake successful tick. */
    mc_java_random_profile *limited = NULL;
    mc_registry_classes *cold_registry = NULL;
    NEED(mc_java_random_profile_create(sizeof(mc_java_random_profile), &limited) ==
         MC_JAVA_RANDOM_PROFILE_OK);
    const mc_registry_options cold_options = {.side = MC_REGISTRY_CLIENT,
                                              .random_profile = limited,
                                              .materials = f->materials,
                                              .resources = mc_resource_files_source(f->files)};
    NEED(mc_registry_classes_allocate(&cold_options, &cold_registry, NULL) == MC_REGISTRY_OK);
    const mc_texture_effect_services cold_service = {cold_registry, limited, f->trig, f->files,
                                                     mc_resource_files_source(f->files)};
    mc_client_texture_effect *cold = NULL;
    NEED(mc_client_texture_effect_allocate(&cold_service, MC_TEXTURE_EFFECT_FIRE, NULL, &cold,
                                           &e->child) == MC_TEXTURE_EFFECT_OK);
    NEED(mc_client_texture_effect_construct(cold, app, 0, &e->child) == MC_TEXTURE_EFFECT_OK);
    mc_texture_effect_state cold_before;
    NEED(mc_client_texture_effect_inspect(cold, &cold_before));
    NEED(atomic_load(&limited->holder_phase) == MC_JAVA_RANDOM_HOLDER_COLD);
    NEED(!mc_client_render_engine_register_effect(engine, cold, 0, e) && e->appended &&
         e->ji_pc == 12);
    NEED(e->child.status == MC_TEXTURE_EFFECT_RANDOM_FAILED && e->child.source_pc == 164 &&
         e->child.random == MC_JAVA_RANDOM_PROFILE_LIMIT);
    mc_texture_effect_state retained;
    NEED(mc_client_texture_effect_inspect(cold, &retained) &&
         retained.phase == MC_TEXTURE_EFFECT_READY && retained.last_store_pc == 147 &&
         retained.g == cold_before.g && retained.h == cold_before.h && retained.native_holds == 2);
    NEED(!mc_client_render_engine_register_effect(engine, cold, 0, e) && e->appended &&
         e->ji_pc == 12 && e->child.random == MC_JAVA_RANDOM_PROFILE_HOLDER_FAILED);
    NEED(mc_client_texture_effect_inspect(cold, &retained) && retained.native_holds == 3);
    NEED(mc_client_texture_effect_release(&cold, &e->child) == MC_TEXTURE_EFFECT_OK &&
         cold == NULL);
    NEED(mc_client_render_engine_release(engine) == MC_CLIENT_RENDER_ENGINE_BUSY);
    /* Stop at a real active native effect, preserving exact retirement
     * progress/occurrence. This is an owner guard, not a fake game child. */
    ordered[0]->active = true;
    NEED(!mc_client_render_engine_retire_effects(engine, e) && engine->effect_release_index == 0);
    ordered[0]->active = false;
    NEED(mc_client_render_engine_retire_effects(engine, e) && engine->effects_retired);
    NEED(mc_registry_classes_free(cold_registry, NULL) == MC_REGISTRY_OK);
    mc_java_random_profile_free(limited);
    NEED(mc_client_render_engine_retire_effects(engine, e));
    NEED(!mc_client_render_engine_register_effect(engine, ordered[2], 0, e) && !e->appended);
    for (size_t n = 2; n < 9; ++n)
        NEED(mc_client_texture_effect_release(&ordered[n], &e->child) == MC_TEXTURE_EFFECT_OK &&
             ordered[n] == NULL);
    NEED(mc_client_water_texture_retire(&water, NULL) == MC_WATER_TEXTURE_OK && water == NULL);
    NEED(mc_client_lava_texture_retire(&lava, NULL) == MC_LAVA_TEXTURE_OK && lava == NULL);
    free(app);
    NEED(mc_client_render_engine_release(engine) == MC_CLIENT_RENDER_ENGINE_OK);
    NEED(mc_client_default_font_release(font) == MC_CLIENT_FONT_OK);
    NEED(mc_client_render_engine_classes_release(classes) == MC_CLIENT_RENDER_ENGINE_OK);
    free(e);
    return true;
}

/* Actual byte-close forwarding only. The same concrete Files source and real
 * InputStream context remain in use; no image/game callback is substituted. */
static mc_registry_status (*effect_actual_close)(void *, const mc_java_properties_input *);
static unsigned effect_close_calls;
static bool effect_close_fail_once, effect_real_close_returned;

static mc_registry_status effect_forward_close(void *context,
                                               const mc_java_properties_input *input) {
    ++effect_close_calls;
    const mc_registry_status status = effect_actual_close(context, input);
    if (status == MC_REGISTRY_OK && effect_close_fail_once) {
        effect_close_fail_once = false;
        effect_real_close_returned = true;
        return MC_REGISTRY_IO_FAILED;
    }
    return status;
}

static bool effect_png16(const char *path) {
    const size_t total = (256u * 8u + 1u) * 256u;
    uint8_t *raw = calloc(1, total);
    NEED(raw != NULL);
    uLongf size = compressBound((uLong)total);
    uint8_t *encoded = malloc((size_t)size);
    NEED(encoded != NULL);
    const int status = compress2(encoded, &size, raw, (uLong)total, Z_BEST_SPEED);
    free(raw);
    NEED(status == Z_OK);
    FILE *file = fopen(path, "wb");
    NEED(file != NULL);
    static const uint8_t signature[] = {137, 80, 78, 71, 13, 10, 26, 10};
    uint8_t header[13] = {0};
    fx_be32(header, 256);
    fx_be32(header + 4, 256);
    header[8] = 16;
    header[9] = 6;
    const bool written =
        fwrite(signature, 1, 8, file) == 8 && fx_chunk(file, "IHDR", header, sizeof(header)) &&
        fx_chunk(file, "IDAT", encoded, (size_t)size) && fx_chunk(file, "IEND", NULL, 0);
    free(encoded);
    const int closed = fclose(file);
    NEED(written && closed == 0);
    return true;
}

static bool app_borrow_cases(fx_effects *f) {
    client_application *app = calloc(1, sizeof(*app));
    NEED(app != NULL);
    mc_client_application_owner_error app_error = {0};
    NEED(mc_client_application_lifetime_register(app, &app_error) ==
         MC_CLIENT_APPLICATION_OWNER_OK);
    const mc_texture_effect_services service = services(f);
    mc_texture_effect_error *e = calloc(1, sizeof(*e));
    NEED(e != NULL);
    mc_client_texture_effect *effect = NULL;
    NEED(mc_client_texture_effect_allocate(&service, MC_TEXTURE_EFFECT_COMPASS, NULL, &effect, e) ==
         MC_TEXTURE_EFFECT_OK);
    NEED(mc_client_texture_effect_construct(effect, app, 0, e) == MC_TEXTURE_EFFECT_OK);
    NEED(((mc_texture_effect_owner *)effect)->app_borrow_counted &&
         atomic_load(&app->effect_borrows) == 1);
    NEED(mc_client_texture_effect_retain(effect, e) == MC_TEXTURE_EFFECT_OK);
    mc_client_texture_effect *external = effect;
    NEED(mc_client_texture_effect_release(&effect, e) == MC_TEXTURE_EFFECT_OK && effect == NULL);
    NEED(atomic_load(&app->effect_borrows) == 1 &&
         mc_client_application_effect_borrows_pending(app));
    NEED(mc_client_application_lifetime_retire(app, &app_error) ==
         MC_CLIENT_APPLICATION_OWNER_BUSY);
    external->active = true;
    NEED(mc_client_texture_effect_release(&external, e) == MC_TEXTURE_EFFECT_BUSY &&
         external != NULL && atomic_load(&app->effect_borrows) == 1);
    external->active = false;
    NEED(mc_client_texture_effect_release(&external, e) == MC_TEXTURE_EFFECT_OK &&
         external == NULL);
    NEED(!mc_client_application_effect_borrows_pending(app) &&
         atomic_load(&app->effect_borrows) == 0);
    NEED(mc_client_texture_effect_release(&external, e) == MC_TEXTURE_EFFECT_OK &&
         atomic_load(&app->effect_borrows) == 0);
    NEED(mc_client_application_lifetime_retire(app, &app_error) == MC_CLIENT_APPLICATION_OWNER_OK);
    free(app);

    app = calloc(1, sizeof(*app));
    NEED(app != NULL);
    NEED(mc_client_application_lifetime_register(app, &app_error) ==
         MC_CLIENT_APPLICATION_OWNER_OK);
    char path[4096];
    NEED(fx_path(path, sizeof(path), f->root, "gui/items.png") && effect_png16(path));
    NEED(mc_client_texture_effect_allocate(&service, MC_TEXTURE_EFFECT_COMPASS, NULL, &effect, e) ==
         MC_TEXTURE_EFFECT_OK);
    NEED(mc_client_texture_effect_construct(effect, app, 0, e) == MC_TEXTURE_EFFECT_IMAGE_FAILED);
    NEED(e->image.image.status == MC_CLIENT_IMAGE_IO_UNSUPPORTED_NATIVE &&
         !e->image.source_close_reached);
    NEED(atomic_load(&app->effect_borrows) == 1 &&
         ((mc_texture_effect_owner *)effect)->app_borrow_counted);
    mc_registry_resources *transport =
        (mc_registry_resources *)(void *)mc_resource_files_source(f->files);
    effect_actual_close = transport->close;
    NEED(effect_actual_close != NULL);
    effect_close_calls = 0;
    effect_close_fail_once = true;
    effect_real_close_returned = false;
    transport->close = effect_forward_close;
    NEED(mc_client_texture_effect_release(&effect, e) == MC_TEXTURE_EFFECT_NATIVE_CLOSE_FAILED &&
         effect != NULL);
    NEED(effect_close_calls == 1 && effect_real_close_returned &&
         atomic_load(&app->effect_borrows) == 1);
    mc_texture_effect_state state;
    NEED(mc_client_texture_effect_inspect(effect, &state));
    NEED(state.phase == MC_TEXTURE_EFFECT_RETIRING &&
         state.failure->status == MC_TEXTURE_EFFECT_IMAGE_FAILED &&
         state.retirement->status == MC_TEXTURE_EFFECT_NATIVE_CLOSE_FAILED);
    NEED(mc_client_texture_effect_tick(effect, e) == MC_TEXTURE_EFFECT_INVALID_PHASE);
    NEED(mc_client_texture_effect_retain(effect, e) == MC_TEXTURE_EFFECT_INVALID_PHASE);
    NEED(mc_client_application_lifetime_retire(app, &app_error) ==
         MC_CLIENT_APPLICATION_OWNER_BUSY);
    NEED(mc_client_texture_effect_release(&effect, e) == MC_TEXTURE_EFFECT_OK && effect == NULL);
    transport->close = effect_actual_close;
    NEED(effect_close_calls == 2 && atomic_load(&app->effect_borrows) == 0);
    NEED(mc_client_application_lifetime_retire(app, &app_error) == MC_CLIENT_APPLICATION_OWNER_OK);
    free(app);

    /* This is a refused RAW/prepared native state, not synthesized successful
     * registration. Both constructors retain their arrays before g22/g31. */
    app = calloc(1, sizeof(*app));
    NEED(app != NULL);
    app->constructor_phase = MC_CLIENT_APP_RETIRING;
    const mc_texture_effect_kind kinds[] = {MC_TEXTURE_EFFECT_COMPASS, MC_TEXTURE_EFFECT_WATCH};
    const int32_t pcs[] = {22, 31}, last[] = {17, 26};
    for (size_t n = 0; n < 2; ++n) {
        NEED(mc_client_texture_effect_allocate(&service, kinds[n], NULL, &effect, e) ==
             MC_TEXTURE_EFFECT_OK);
        NEED(mc_client_texture_effect_construct(effect, app, 0, e) ==
                 MC_TEXTURE_EFFECT_INVALID_PHASE &&
             e->source_pc == pcs[n]);
        NEED(mc_client_texture_effect_inspect(effect, &state) && state.app == NULL &&
             state.fields.f == 0 && state.last_store_pc == last[n]);
        NEED(!((mc_texture_effect_owner *)effect)->app_borrow_counted);
        NEED(mc_client_texture_effect_release(&effect, e) == MC_TEXTURE_EFFECT_OK &&
             effect == NULL);
    }
    free(app);
    free(e);
    return true;
}

#ifndef MC_TEXTURE_EFFECT_REGISTRATION_MAIN
#define MC_TEXTURE_EFFECT_REGISTRATION_MAIN main
#endif
int MC_TEXTURE_EFFECT_REGISTRATION_MAIN(int argc, char **argv) {
    if (argc != 2)
        return 2;
    fx_effects f = {0};
    if (!prepare(&f, argv[1]) || !registrations(&f) || !app_borrow_cases(&f))
        return 1;
    if (mc_registry_classes_free(f.registry, NULL) != MC_REGISTRY_OK)
        return 1;
    mc_java_random_profile_free(f.profile);
    mc_materials_destroy(f.materials);
    if (mc_resource_files_free(f.files) != MC_REGISTRY_OK)
        return 1;
    free(f.trig);
    printf("Actual g registration controls: %u\n", checks);
    return 0;
}
