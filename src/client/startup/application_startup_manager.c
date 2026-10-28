#include "application_startup_internal.h"

#define MC_CLASS "net/minecraft/client/Minecraft"

static mc_client_startup_status manager_failure(mc_client_application_startup *s) {
    const mc_client_manager_error *e = &s->failure.manager;
    return application_startup_fail(&s->failure, MC_CLIENT_STARTUP_MANAGER_FAILED,
                                    "actual TH class/constructor or captured f publication",
                                    e->source_class, e->source_method, e->source_descriptor,
                                    e->pc < 0 ? 0u : (unsigned int)e->pc);
}

mc_client_startup_status application_startup_manager(mc_client_application_startup *s) {
    client_application *app = s->app;
    s->failure.caller = (mc_client_application_owner_site){MC_CLASS, "a", "()V", 310};
    /* Cold native association is SAME App model/Registry/profile namespace.
     * Only the following static_get is actual TH source active use. */
    if (mc_client_render_manager_classes_bind(app->render_classes, app->registry, app->profile,
                                              &s->options.manager, &s->manager_classes,
                                              &s->failure.manager) != MC_CLIENT_MANAGER_OK)
        return manager_failure(s);
    if (mc_client_render_manager_consumer_attach(s->manager_classes, &s->manager_consumer,
                                                 &s->failure.manager) != MC_CLIENT_MANAGER_OK)
        return manager_failure(s);
    if (mc_client_render_manager_static_get(s->manager_classes, &s->manager_capture,
                                            &s->failure.manager) != MC_CLIENT_MANAGER_OK)
        return manager_failure(s);
    s->last_manager_pc = 310; /* Capture actual possibly-null A, not a later reread. */
    s->failure.caller.pc = 313;
    const mc_client_entity_renderer_services services = {app->render_classes, s->engine_classes,
                                                         app->profile, s->display};
    if (mc_client_held_item_allocate(&services, &s->options.entity_renderer,
                                     &s->manager_hand_candidate,
                                     &s->failure.entity_renderer) != MC_CLIENT_ENTITY_RENDERER_OK)
        goto hand_failure;
    s->failure.caller.pc = 318;
    if (mc_client_held_item_construct(s->manager_hand_candidate, app,
                                      &s->failure.entity_renderer) != MC_CLIENT_ENTITY_RENDERER_OK)
        goto hand_failure;
    s->last_manager_pc = 318;
    s->failure.caller.pc = 321;
    if (mc_client_render_manager_publish_hand(s->manager_classes, s->manager_capture,
                                              &s->manager_hand_candidate,
                                              &s->failure.manager) != MC_CLIENT_MANAGER_OK)
        return manager_failure(s);
    s->manager_f_stored = true;
    s->last_manager_pc = 321;
    /* Source field is TH.f, not App.t or PX.c. App last store remains307. */
    s->failure.caller.pc = 324;
    return application_startup_fail(
        &s->failure, MC_CLIENT_STARTUP_UNIMPLEMENTED,
        "next unexecuted MC324; no Registry/loader/frame/draw body entered", MC_CLASS, "a", "()V",
        324);
hand_failure:
    return application_startup_fail(
        &s->failure, MC_CLIENT_STARTUP_ENTITY_RENDERER_FAILED,
        "actual independent MC RA313/318 construction", s->failure.entity_renderer.site.class_name,
        s->failure.entity_renderer.site.method, s->failure.entity_renderer.site.descriptor,
        s->failure.entity_renderer.site.pc < 0 ? 0u
                                               : (unsigned int)s->failure.entity_renderer.site.pc);
}
