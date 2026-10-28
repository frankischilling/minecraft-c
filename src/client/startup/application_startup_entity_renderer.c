#include "application_startup_internal.h"

#define MC_CLASS "net/minecraft/client/Minecraft"

static mc_client_startup_status renderer_failure(mc_client_application_startup *s) {
    const mc_client_graphics_site *site = &s->failure.entity_renderer.site;
    return application_startup_fail(
        &s->failure, MC_CLIENT_STARTUP_ENTITY_RENDERER_FAILED, "actual px construction",
        site->pc < 0 ? NULL : site->class_name, site->pc < 0 ? NULL : site->method,
        site->pc < 0 ? NULL : site->descriptor, site->pc < 0 ? 0u : (unsigned int)site->pc);
}

mc_client_startup_status application_startup_entity_renderer(mc_client_application_startup *s) {
    client_application *app = s->app;
    /* new299 performs px active use before RAW native allocation. Existing
     * model namespace is the one already used by this actual App constructor. */
    s->failure.caller = (mc_client_application_owner_site){MC_CLASS, "a", "()V", 299};
    if (app->t != NULL || s->entity_renderer_candidate != NULL)
        return application_startup_fail(&s->failure, MC_CLIENT_STARTUP_INVALID_PHASE,
                                        "startup unadopted t/candidate at reached new299", NULL,
                                        NULL, NULL, 0);
    if (mc_client_entity_renderer_class_active_use(
            app->render_classes, &s->failure.entity_renderer) != MC_CLIENT_ENTITY_RENDERER_OK)
        return renderer_failure(s);
    const mc_client_entity_renderer_services services = {app->render_classes, s->engine_classes,
                                                         app->profile, s->display};
    if (mc_client_entity_renderer_allocate(
            &services, &s->options.entity_renderer, &s->entity_renderer_candidate,
            &s->failure.entity_renderer) != MC_CLIENT_ENTITY_RENDERER_OK)
        return renderer_failure(s);
    s->failure.caller.pc = 304;
    if (mc_client_entity_renderer_construct(s->entity_renderer_candidate, app,
                                            &s->failure.entity_renderer) !=
        MC_CLIENT_ENTITY_RENDERER_OK)
        return renderer_failure(s);
    app->t = s->entity_renderer_candidate; /* Actual completed constructor then putfield307. */
    s->t_stored = true;
    s->last_startup_store = 307;
    return application_startup_manager(s);
}
