#include "application_owner_internal.h"
#include "application_startup_internal.h"

#define MC_CLASS "net/minecraft/client/Minecraft"
_Static_assert(MC_CLIENT_COLORIZER_COUNT == 3, "actual MC three literal/colorizer calls");

static mc_client_startup_status literal(mc_client_application_startup *startup, size_t index,
                                        unsigned int pc) {
    static const uint16_t water[] = {'/', 'm', 'i', 's', 'c', '/', 'w', 'a', 't', 'e',
                                     'r', 'c', 'o', 'l', 'o', 'r', '.', 'p', 'n', 'g'};
    static const uint16_t grass[] = {'/', 'm', 'i', 's', 'c', '/', 'g', 'r', 'a', 's',
                                     's', 'c', 'o', 'l', 'o', 'r', '.', 'p', 'n', 'g'};
    static const uint16_t foliage[] = {'/', 'm', 'i', 's', 'c', '/', 'f', 'o', 'l', 'i', 'a',
                                       'g', 'e', 'c', 'o', 'l', 'o', 'r', '.', 'p', 'n', 'g'};
    static const uint16_t *const units[] = {water, grass, foliage};
    static const size_t lengths[] = {sizeof(water) / sizeof(water[0]),
                                     sizeof(grass) / sizeof(grass[0]),
                                     sizeof(foliage) / sizeof(foliage[0])};
    mc_client_application_classes *classes = startup->app->source_namespace;
    if (mtx_lock(&classes->mutex) != thrd_success)
        return application_startup_fail(&startup->failure, MC_CLIENT_STARTUP_OWNER_FAILED,
                                        "color literal native namespace lock", NULL, NULL, NULL, 0);
    if (classes->native_retiring || classes->phase != MC_CLIENT_APPLICATION_CLASS_READY) {
        (void)mtx_unlock(&classes->mutex);
        return application_startup_fail(&startup->failure, MC_CLIENT_STARTUP_INVALID_PHASE,
                                        "color literal live MC namespace", NULL, NULL, NULL, 0);
    }
    if (classes->startup_color_literals[index] == NULL) {
        startup->failure.player_string = mc_player_string_create(
            units[index], lengths[index], &classes->startup_color_literals[index]);
        if (startup->failure.player_string != MC_PLAYER_OK) {
            (void)mtx_unlock(&classes->mutex);
            return application_startup_fail(&startup->failure, MC_CLIENT_STARTUP_STRING_FAILED,
                                            "actual color resource ldc", MC_CLASS, "a", "()V", pc);
        }
    }
    startup->color_literals[index] = classes->startup_color_literals[index];
    (void)mtx_unlock(&classes->mutex);
    return MC_CLIENT_STARTUP_OK;
}

static mc_client_startup_status lookup_failure(mc_client_application_startup *startup) {
    const mc_client_graphics_site *site = &startup->failure.colors.site;
    return application_startup_fail(
        &startup->failure, MC_CLIENT_STARTUP_COLORS_FAILED, "actual ji.a(String) color lookup",
        site->pc < 0 ? NULL : site->class_name, site->pc < 0 ? NULL : site->method,
        site->pc < 0 ? NULL : site->descriptor, site->pc < 0 ? 0u : (unsigned int)site->pc);
}

mc_client_startup_status application_startup_colors(mc_client_application_startup *startup) {
    static const unsigned int load[] = {262, 274, 286}, ldc[] = {266, 278, 290};
    static const unsigned int lookup[] = {268, 280, 292}, setter[] = {271, 283, 295};
    static const mc_client_colorizer_kind kinds[] = {
        MC_CLIENT_COLORIZER_WATER, MC_CLIENT_COLORIZER_GRASS, MC_CLIENT_COLORIZER_FOLIAGE};
    startup->colors_entered = true;
    for (size_t index = 0; index < MC_CLIENT_COLORIZER_COUNT; ++index) {
        startup->failure.caller =
            (mc_client_application_owner_site){MC_CLASS, "a", "()V", load[index]};
        startup->color_lookup_source_returned = false;
        mc_client_render_engine *engine = startup->app->render_engine; /* Actual p263/275/287. */
        startup->failure.caller.pc = ldc[index];
        if (literal(startup, index, ldc[index]) != MC_CLIENT_STARTUP_OK)
            return startup->failure.status;
        mc_player_string_view view;
        if (!mc_player_string_read(startup->color_literals[index], &view))
            return application_startup_fail(&startup->failure, MC_CLIENT_STARTUP_INVALID_PHASE,
                                            "actual MC literal immutable native view", NULL, NULL,
                                            NULL, 0);
        const mc_texture_pack_text key = {view.units, view.length, false}; /* SAME literal units. */
        startup->failure.caller.pc = lookup[index];
        if (engine == NULL) {
            startup->failure.colors.status = MC_CLIENT_LOOKUP_NULL_REFERENCE;
            startup->failure.colors.operation = "actual MC null ji invokevirtual receiver";
            startup->failure.colors.site =
                (mc_client_graphics_site){MC_CLASS, "a", "()V", (int32_t)lookup[index]};
            startup->failure.colors.native_policy = false;
            return lookup_failure(startup);
        }
        if (engine != startup->engine_candidate ||
            mc_client_render_engine_class_identity(engine) != startup->engine_classes)
            return application_startup_fail(
                &startup->failure, MC_CLIENT_STARTUP_INVALID_PHASE,
                "startup actual Engine/namespace identity changed outside owner", NULL, NULL, NULL,
                0);
        const mc_client_texture_lookup_status status =
            mc_client_render_engine_colors(engine, &startup->options.color_lookup, key,
                                           &startup->color_argument, &startup->failure.colors);
        if (status != MC_CLIENT_LOOKUP_OK) {
            mc_client_texture_lookup_state pending;
            if (mc_client_render_engine_lookup_inspect(engine, &pending))
                startup->color_lookup_source_returned = pending.source_returned;
            /* No borrowed pending views are copied into surviving storage.
             * Its actual array/Pack/stream remain owned by the retained Engine. */
            return lookup_failure(startup); /* No setter or lookup rerun after cleanup refusal. */
        }
        startup->color_lookup_source_returned = true;
        ++startup->color_lookups_completed;
        startup->last_color_lookup_pc = lookup[index];
        startup->color_results[index] = startup->color_argument; /* Borrowed SAME Engine.c array. */
        startup->failure.caller.pc = setter[index];
        if (mc_client_colorizer_set(startup->engine_classes, kinds[index], startup->color_argument,
                                    &startup->options.colorizers,
                                    &startup->failure.colorizer) != MC_CLIENT_COLORIZER_OK) {
            const mc_client_graphics_site *site = &startup->failure.colorizer.site;
            return application_startup_fail(
                &startup->failure, MC_CLIENT_STARTUP_COLORS_FAILED,
                "actual colorizer active-use/setter", site->pc < 0 ? NULL : site->class_name,
                site->pc < 0 ? NULL : site->method, site->pc < 0 ? NULL : site->descriptor,
                site->pc < 0 ? 0u : (unsigned int)site->pc);
        }
        ++startup->color_setters_completed;
        startup->last_color_setter_pc = setter[index];
        if (mc_client_color_array_release(&startup->color_argument,
                                          &startup->failure.color_argument) !=
            MC_CLIENT_COLOR_ARRAY_OK)
            return application_startup_fail(&startup->failure, MC_CLIENT_STARTUP_COLORS_FAILED,
                                            "native returned color argument occurrence drop", NULL,
                                            NULL, NULL, 0);
    }
    startup->failure.caller = (mc_client_application_owner_site){MC_CLASS, "a", "()V", 298};
    return application_startup_fail(&startup->failure, MC_CLIENT_STARTUP_UNIMPLEMENTED,
                                    "next unexecuted MC298 before px new299/constructor304/t307",
                                    MC_CLASS, "a", "()V", 298);
}
