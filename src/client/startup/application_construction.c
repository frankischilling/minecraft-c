#include "application_construction_internal.h"
#include "mc/block/registry/runtime.h"
#include <stdlib.h>
#include <string.h>

mc_client_application_construction_status
application_constructor_error(mc_client_application_construction_error *error,
                              mc_client_application_construction_status status,
                              mc_client_application_child child, int32_t pc,
                              const char *operation) {
    if (error != NULL) {
        error->status = status;
        error->child = child;
        error->minecraft_pc = pc;
        error->operation = operation;
    }
    return status;
}

void application_constructor_copy_error(mc_client_application_construction_error *out,
                                        const mc_client_application_construction_error *error) {
    if (out != NULL)
        memcpy(out, error, sizeof(*out));
}

static void source_store(client_application *app, int32_t pc) {
    mc_client_application_construction *m = app->construction;
    m->stores[m->source_stores++] = pc; /* Fixed actual constructor has fewer than64 stores. */
    m->last_source_store = pc;
}

static mc_client_application_construction_status
failed(client_application *app, mc_client_application_child child, int32_t pc,
       const char *operation, mc_client_application_construction_error *out) {
    app->constructor_phase = MC_CLIENT_APP_FAILED;
    application_constructor_error(&app->construction->failure, MC_CLIENT_APP_CHILD_FAILED, child,
                                  pc, operation);
    application_constructor_copy_error(out, &app->construction->failure);
    return MC_CLIENT_APP_CHILD_FAILED;
}

mc_client_application_construction_status mc_client_application_constructor_allocate(
    const mc_client_application_constructor_services *services,
    const mc_client_application_constructor_options *options, client_application **out,
    mc_client_application_construction_error *error) {
    if (error != NULL) {
        memset(error, 0, sizeof(*error));
        error->minecraft_pc = -1;
    }
    if (services == NULL || out == NULL || services->application_classes == NULL ||
        services->gui_classes == NULL || services->registry == NULL || services->entities == NULL ||
        services->render_classes == NULL || services->profile == NULL ||
        services->sound_shared == NULL || services->sound_backend == NULL)
        return application_constructor_error(error, MC_CLIENT_APP_INVALID_ARGUMENT,
                                             MC_CLIENT_APP_CHILD_NONE, -1,
                                             "actual App native services");
    if (options != NULL && options->maximum_native_bytes != 0 &&
        options->maximum_native_bytes <
            sizeof(client_application) + sizeof(mc_client_application_construction))
        return application_constructor_error(error, MC_CLIENT_APP_LIMIT, MC_CLIENT_APP_CHILD_NONE,
                                             -1, "actual App native memory policy");
    /* Real source class initialization precedes source instance/new storage.
     * Its retained failed prefix remains on the actual namespace owner. */
    if (mc_client_application_classes_active_use(services->application_classes,
                                                 error == NULL ? NULL : &error->cause.owner) !=
        MC_CLIENT_APPLICATION_OWNER_OK)
        return application_constructor_error(error, MC_CLIENT_APP_CHILD_FAILED,
                                             MC_CLIENT_APP_CHILD_CLASS, -1,
                                             "actual Minecraft class active use");
    client_application *app = calloc(1, sizeof(*app));
    mc_client_application_construction *m = calloc(1, sizeof(*m));
    if (app == NULL || m == NULL) {
        free(m);
        free(app);
        return application_constructor_error(error, MC_CLIENT_APP_ALLOCATION_FAILED,
                                             MC_CLIENT_APP_CHILD_NONE, -1,
                                             "actual App/heap diagnostics");
    }
    app->construction = m;
    m->last_source_store = -1;
    m->failure.minecraft_pc = -1;
    m->retirement.minecraft_pc = -1;
    if (options != NULL)
        m->options = *options;
    if (m->options.sound_pool_bytes == 0)
        m->options.sound_pool_bytes = 1024u * 1024u;
    app->profile = services->profile; /* Native shared profile only, no source field. */
    app->registry = services->registry;
    app->entities = services->entities;
    app->render_classes = services->render_classes;
    m->gui = services->gui_classes;
    m->sound_shared = services->sound_shared;
    m->sound_backend = services->sound_backend;
    mc_client_application_construction_error *e = &m->failure;
    if (mc_client_application_lifetime_register(app, &e->cause.owner) !=
        MC_CLIENT_APPLICATION_OWNER_OK) {
        application_constructor_error(e, MC_CLIENT_APP_CHILD_FAILED, MC_CLIENT_APP_CHILD_CLASS, -1,
                                      "actual App atomic lifetime registration");
        goto native_failed;
    }
    if (mc_registry_classes_retain(app->registry, MC_REGISTRY_CLIENT, app->profile,
                                   &e->cause.registry) != MC_REGISTRY_OK) {
        application_constructor_error(e, MC_CLIENT_APP_CHILD_FAILED, MC_CLIENT_APP_CHILD_REGISTRY,
                                      -1, "actual App Registry native occurrence");
        goto native_failed;
    }
    m->registry_hold = true;
    e->cause.entity_status =
        mc_entity_classes_retain(app->entities, MC_ENTITY_CLIENT, app->profile);
    if (e->cause.entity_status != MC_ENTITY_OK) {
        application_constructor_error(e, MC_CLIENT_APP_CHILD_FAILED, MC_CLIENT_APP_CHILD_ENTITY, -1,
                                      "actual App Entity native occurrence");
        goto native_failed;
    }
    m->entity_hold = true;
    e->cause.model_status = mc_client_render_classes_retain(app->render_classes, app->profile);
    if (e->cause.model_status != MC_CLIENT_MODEL_OK) {
        application_constructor_error(e, MC_CLIENT_APP_CHILD_FAILED,
                                      MC_CLIENT_APP_CHILD_RENDER_CLASSES, -1,
                                      "actual App render classes native occurrence");
        goto native_failed;
    }
    m->render_hold = true;
    e->cause.loading.status = mc_client_loading_classes_retain(m->gui);
    if (e->cause.loading.status != MC_CLIENT_LOADING_OK) {
        application_constructor_error(e, MC_CLIENT_APP_CHILD_FAILED, MC_CLIENT_APP_CHILD_LOADING,
                                      -1, "actual App GUI classes native occurrence");
        goto native_failed;
    }
    m->gui_hold = true;
    if (mc_client_application_namespace_attach(services->application_classes, app,
                                               &e->cause.owner) != MC_CLIENT_APPLICATION_OWNER_OK) {
        application_constructor_error(e, MC_CLIENT_APP_CHILD_FAILED, MC_CLIENT_APP_CHILD_CLASS, -1,
                                      "actual App namespace native borrow");
        goto native_failed;
    }
    memset(e, 0, sizeof(*e));
    e->minecraft_pc = -1;
    *out = app;
    application_constructor_copy_error(error, e);
    return MC_CLIENT_APP_OK;
native_failed:
    /* No source constructor ran. Cold caller-owned services have an external
     * occurrence; balancing these new occurrences does not retire them. */
    application_constructor_copy_error(error, e);
    if (m->gui_hold)
        (void)mc_client_loading_classes_release(m->gui);
    if (m->render_hold)
        (void)mc_client_render_classes_release(app->render_classes);
    if (m->entity_hold)
        (void)mc_entity_classes_release(app->entities);
    if (m->registry_hold)
        (void)mc_registry_classes_release(app->registry, NULL);
    if (app->native_lifetime_phase == MC_CLIENT_APPLICATION_LIFETIME_REGISTERED)
        (void)mc_client_application_lifetime_retire(app, NULL);
    free(m);
    free(app);
    return MC_CLIENT_APP_CHILD_FAILED;
}

mc_client_application_construction_status
mc_client_application_constructor_construct(client_application *app,
                                            const mc_client_application_constructor_arguments *args,
                                            mc_client_application_construction_error *out) {
    if (out != NULL) {
        memset(out, 0, sizeof(*out));
        out->minecraft_pc = -1;
    }
    if (app == NULL || args == NULL || app->construction == NULL)
        return application_constructor_error(out, MC_CLIENT_APP_INVALID_ARGUMENT,
                                             MC_CLIENT_APP_CHILD_NONE, -1,
                                             "actual App constructor arguments");
    if (app->constructor_phase != MC_CLIENT_APP_RAW || !app->source_namespace_borrowed)
        return application_constructor_error(out, MC_CLIENT_APP_INVALID_PHASE,
                                             MC_CLIENT_APP_CHILD_NONE, -1,
                                             "actual App one-shot constructor");
    mc_client_application_construction *m = app->construction;
    mc_client_application_construction_error *e = &m->failure;
    memset(e, 0, sizeof(*e));
    e->minecraft_pc = -1;
    app->constructor_phase = MC_CLIENT_APP_CONSTRUCTING;
    /* Object.<init>1 has no instance field effects. Component argument1 is unused. */
    app->Q = false;
    source_store(app, 6);
    app->R = false;
    source_store(app, 11);
    memset(&e->cause, 0, sizeof(e->cause));
    if (mc_timer_allocate_raw(m->options.timer_bytes, &m->timer_candidate, &e->cause.timer) !=
        MC_TIMER_CONSTRUCTION_OK)
        return failed(app, MC_CLIENT_APP_CHILD_TIMER, 15, "Minecraft actual new py", out);
    memset(&e->cause, 0, sizeof(e->cause));
    if (mc_timer_construct_system(m->timer_candidate, 20.0f, &app->clock, &e->cause.timer) !=
        MC_TIMER_CONSTRUCTION_OK)
        return failed(app, MC_CLIENT_APP_CHILD_TIMER, 21, "Minecraft actual py constructor", out);
    app->timer = m->timer_candidate;
    m->timer_candidate = NULL;
    source_store(app, 24);
    app->session = NULL;
    source_store(app, 29);
    app->n = true;
    source_store(app, 34);
    atomic_store_explicit(&app->o, false, memory_order_seq_cst);
    source_store(app, 39);
    app->r = NULL;
    source_store(app, 44);
    memset(&e->cause, 0, sizeof(e->cause));
    e->cause.loading.status =
        mc_client_loading_screen_allocate(m->gui, &app->loading_screen_retained);
    if (e->cause.loading.status != MC_CLIENT_LOADING_OK)
        return failed(app, MC_CLIENT_APP_CHILD_LOADING, 48, "Minecraft actual new mk", out);
    memset(&e->cause, 0, sizeof(e->cause));
    if (mc_client_loading_screen_construct(app->loading_screen_retained, app, &e->cause.loading) !=
        MC_CLIENT_LOADING_OK)
        return failed(app, MC_CLIENT_APP_CHILD_LOADING, 53, "Minecraft actual mk constructor", out);
    app->loading_screen = app->loading_screen_retained;
    source_store(app, 56);
    app->V = 0;
    source_store(app, 61);
    app->W = 0;
    source_store(app, 66);
    memset(&e->cause, 0, sizeof(e->cause));
    if (mc_client_achievement_notice_allocate(app->render_classes, app->registry, app->profile,
                                              &m->notice_candidate,
                                              &e->cause.notice) != MC_CLIENT_NOTICE_OK)
        return failed(app, MC_CLIENT_APP_CHILD_NOTICE, 70, "Minecraft actual new vu", out);
    memset(&e->cause, 0, sizeof(e->cause));
    if (mc_client_achievement_notice_construct(m->notice_candidate, app, &e->cause.notice) !=
        MC_CLIENT_NOTICE_OK)
        return failed(app, MC_CLIENT_APP_CHILD_NOTICE, 75, "Minecraft actual vu constructor", out);
    app->achievement_notice = m->notice_candidate;
    m->notice_candidate = NULL;
    source_store(app, 78);
    app->w = false;
    source_store(app, 83);
    memset(&e->cause, 0, sizeof(e->cause));
    if (mc_client_biped_allocate(app->render_classes, m->options.biped_bytes, &m->biped_candidate,
                                 &e->cause.biped) != MC_CLIENT_MODEL_OK)
        return failed(app, MC_CLIENT_APP_CHILD_BIPED, 87, "Minecraft actual new distinct fh", out);
    memset(&e->cause, 0, sizeof(e->cause));
    if (mc_client_biped_construct_zero(m->biped_candidate, &e->cause.biped) != MC_CLIENT_MODEL_OK)
        return failed(app, MC_CLIENT_APP_CHILD_BIPED, 92, "Minecraft actual distinct fh(0)", out);
    app->x = m->biped_candidate;
    m->biped_candidate = NULL;
    source_store(app, 95);
    app->y = NULL;
    source_store(app, 100);
    memset(&e->cause, 0, sizeof(e->cause));
    if (mc_sound_manager_allocate(m->sound_shared, &m->sound_candidate, &e->cause.sound_raw) !=
        MC_SOUND_OK)
        return failed(app, MC_CLIENT_APP_CHILD_SOUND, 104, "Minecraft actual new yo", out);
    memset(&e->cause, 0, sizeof(e->cause));
    if (mc_sound_manager_construct(m->sound_candidate, app->profile, m->options.sound_pool_bytes,
                                   &e->cause.sound) != MC_SOUND_OK)
        return failed(app, MC_CLIENT_APP_CHILD_SOUND, 108, "Minecraft actual yo constructor", out);
    app->B = m->sound_candidate;
    m->sound_candidate = NULL;
    source_store(app, 111);
    memset(&e->cause, 0, sizeof(e->cause));
    if (mc_client_water_texture_allocate(app->registry, app->profile, &m->options.water,
                                         &app->water_effect_retained,
                                         &e->cause.water) != MC_WATER_TEXTURE_OK)
        return failed(app, MC_CLIENT_APP_CHILD_WATER, 115, "Minecraft actual new vs", out);
    memset(&e->cause, 0, sizeof(e->cause));
    if (mc_client_water_texture_construct(app->water_effect_retained, &e->cause.water) !=
        MC_WATER_TEXTURE_OK)
        return failed(app, MC_CLIENT_APP_CHILD_WATER, 119, "Minecraft actual vs constructor", out);
    app->water_effect = app->water_effect_retained;
    source_store(app, 122);
    memset(&e->cause, 0, sizeof(e->cause));
    if (mc_client_lava_texture_allocate_constructor(app->registry, app->profile, &m->options.lava,
                                                    &m->lava_candidate,
                                                    &e->cause.lava) != MC_LAVA_TEXTURE_OK)
        return failed(app, MC_CLIENT_APP_CHILD_LAVA, 126, "Minecraft actual new cg", out);
    memset(&e->cause, 0, sizeof(e->cause));
    if (mc_client_lava_texture_construct(m->lava_candidate, &e->cause.lava) != MC_LAVA_TEXTURE_OK)
        return failed(app, MC_CLIENT_APP_CHILD_LAVA, 130, "Minecraft actual cg constructor", out);
    app->ae = m->lava_candidate;
    m->lava_candidate = NULL;
    source_store(app, 133);
    atomic_store_explicit(&app->J, true, memory_order_seq_cst);
    source_store(app, 138);
    mc_player_string *empty = mc_client_loading_classes_empty(m->gui);
    memset(&e->cause, 0, sizeof(e->cause));
    e->cause.string = empty == NULL ? MC_PLAYER_INVALID_ARGUMENT : mc_player_string_retain(empty);
    if (e->cause.string != MC_PLAYER_OK)
        return failed(app, MC_CLIENT_APP_CHILD_STRING, 142, "Minecraft canonical empty ldc", out);
    app->K = empty;
    source_store(app, 144);
    app->L = false;
    source_store(app, 149);
    app->M = INT64_C(-1);
    source_store(app, 156);
    app->N = false;
    source_store(app, 161);
    app->ag = 0;
    source_store(app, 166);
    app->O = false;
    source_store(app, 171);
    int64_t now;
    memset(&e->cause, 0, sizeof(e->cause));
    e->cause.clock = mc_platform_current_time_millis(&now);
    if (e->cause.clock != MC_PLATFORM_MILLIS_OK)
        return failed(app, MC_CLIENT_APP_CHILD_CLOCK, 175, "Minecraft actual wall clock", out);
    app->P = now;
    source_store(app, 178);
    app->ah = 0;
    source_store(app, 183);
    memset(&e->cause, 0, sizeof(e->cause));
    if (mc_client_application_statistics_active_use(app, &e->cause.statistics) !=
        MC_CLIENT_STATISTICS_OWNER_OK)
        return failed(app, MC_CLIENT_APP_CHILD_STATISTICS, 186,
                      "Minecraft actual jl class active use", out);
    app->Y = args->height;
    source_store(app, 192);
    app->Q = args->fullscreen;
    source_store(app, 198);
    app->A = args->applet;
    source_store(app, 203);
    memset(&e->cause, 0, sizeof(e->cause));
    if (mc_client_timer_hack_allocate(&m->worker, &e->cause.owner) !=
        MC_CLIENT_APPLICATION_OWNER_OK)
        return failed(app, MC_CLIENT_APP_CHILD_WORKER, 206, "Minecraft actual new kg", out);
    mc_player_string *name = NULL;
    memset(&e->cause, 0, sizeof(e->cause));
    if (mc_client_application_classes_timer_name(app->source_namespace, &name, &e->cause.owner) !=
        MC_CLIENT_APPLICATION_OWNER_OK)
        return failed(app, MC_CLIENT_APP_CHILD_STRING, 211, "Minecraft actual Thread-name ldc",
                      out);
    memset(&e->cause, 0, sizeof(e->cause));
    if (mc_client_timer_hack_construct(m->worker, app, name, &e->cause.owner) !=
        MC_CLIENT_APPLICATION_OWNER_OK)
        return failed(app, MC_CLIENT_APP_CHILD_WORKER, 213, "Minecraft actual kg constructor", out);
    /* Source pop216: no source App field receives this real retained worker. */
    app->m = args->canvas;
    source_store(app, 219);
    app->d = args->width;
    source_store(app, 225);
    app->e = args->height;
    source_store(app, 231);
    app->Q = args->fullscreen;
    source_store(app, 237);
    if (app->A != NULL) {
        mc_player_string *literal = NULL;
        memset(&e->cause, 0, sizeof(e->cause));
        if (mc_client_application_classes_applet_true(
                app->source_namespace, &literal, &e->cause.owner) != MC_CLIENT_APPLICATION_OWNER_OK)
            return failed(app, MC_CLIENT_APP_CHILD_STRING, 244, "Minecraft actual true ldc", out);
        memset(&e->cause, 0, sizeof(e->cause));
        if (mc_client_application_classes_applet_standalone(
                app->source_namespace, &literal, &e->cause.owner) != MC_CLIENT_APPLICATION_OWNER_OK)
            return failed(app, MC_CLIENT_APP_CHILD_STRING, 247, "Minecraft actual stand-alone ldc",
                          out);
        e->frontier =
            (mc_client_application_frontier){"net/minecraft/client/MinecraftApplet", "getParameter",
                                             "(Ljava/lang/String;)Ljava/lang/String;", -1};
        app->constructor_phase = MC_CLIENT_APP_FAILED;
        application_constructor_error(
            e, MC_CLIENT_APP_UNIMPLEMENTED, MC_CLIENT_APP_CHILD_PLATFORM, 249,
            "actual nonnull Applet.getParameter platform owner unfinished");
        application_constructor_copy_error(out, e);
        return MC_CLIENT_APP_UNIMPLEMENTED;
    }
    app->n = false;
    source_store(app, 260);
    memset(&e->cause, 0, sizeof(e->cause));
    if (mc_client_application_classes_publish(app->source_namespace, app, &e->cause.owner) !=
        MC_CLIENT_APPLICATION_OWNER_OK)
        return failed(app, MC_CLIENT_APP_CHILD_CLASS, 264,
                      "Minecraft actual source-global a publication", out);
    source_store(app, 264);
    app->constructor_phase = MC_CLIENT_APP_READY; /* Actual source return267, not startup/run. */
    memset(e, 0, sizeof(*e));
    e->minecraft_pc = -1;
    application_constructor_copy_error(out, e);
    return MC_CLIENT_APP_OK;
}

bool mc_client_application_constructor_inspect(const client_application *app,
                                               mc_client_application_constructor_state *out) {
    if (app == NULL || out == NULL || app->construction == NULL)
        return false;
    const mc_client_application_construction *m = app->construction;
    *out = (mc_client_application_constructor_state){
        .phase = app->constructor_phase,
        .fields = app->constructor_phase == MC_CLIENT_APP_RETIRING ? NULL : app,
        .source_stores = m->source_stores,
        .last_source_store = m->last_source_store,
        .source_store_pcs = m->stores,
        .failure = &m->failure,
        .retirement = &m->retirement,
        .worker = m->worker,
        .namespace_borrowed = app->source_namespace_borrowed,
        .static_holds = atomic_load_explicit(&app->source_static_holds, memory_order_seq_cst)};
    return true;
}
