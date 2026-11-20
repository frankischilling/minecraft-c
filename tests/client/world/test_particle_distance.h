#include "client/application_state.h"
#include "mc/client/particles/effect_renderer.h"

/* Enter only N's particle field-input branch, through its real WorldAccess
 * identity. The GAME World, READY Player camera and constructed DN are actual
 * owners. The cold N and borrowed App fields do not claim a completed graphics
 * or Minecraft constructor. NULL names stop before any particle allocation. */
static bool particle_distance_controls(mc_client_render_engine_classes *graphics,
                                       mc_world_runtime *world, mc_entity *camera) {
    bool result = false;
    mc_client_render_global *owner = NULL;
    mc_client_effect_renderer *renderer = NULL;
    mc_client_render_global_error *error = calloc(1, sizeof(*error));
    mc_effect_renderer_error *effects = calloc(1, sizeof(*effects));
    client_application *app = calloc(1, sizeof(*app));
    mc_world_access_failure captured = {0};
    const size_t original_memberships = world->world_accesses.count;
    const size_t original_bindings = world->persistent_bindings;
    CHECK(error != NULL && effects != NULL && app != NULL);
    CHECK(mc_client_effect_renderer_allocate(camera->classes, NULL, &renderer, effects) ==
              MC_EFFECT_RENDERER_OK &&
          mc_client_effect_renderer_construct(renderer, world, NULL, effects) ==
              MC_EFFECT_RENDERER_OK);
    app->entities = camera->classes;
    app->i = camera;
    app->effect_renderer = renderer;
    CHECK(mc_client_render_global_allocate(graphics, NULL, &owner, error) ==
          MC_CLIENT_RENDER_GLOBAL_OK);
    owner->fields.t = app;
    owner->fields.k = world;
    CHECK(mc_render_global_access_create(owner, &owner->access, error) ==
              MC_CLIENT_RENDER_GLOBAL_OK &&
          mc_render_global_access_source_add(owner->access, world, error) ==
              MC_CLIENT_RENDER_GLOBAL_OK);
    CHECK(world->world_accesses.count == original_memberships + 1u &&
          world->persistent_bindings == original_bindings + 2u);
    mc_world_access *identity = world->world_accesses.entries[original_memberships];
    CHECK(identity != NULL);
    mc_effect_renderer_snapshot before, after;
    CHECK(mc_client_effect_renderer_read(renderer, &before) && before.random != NULL);
    const mc_java_random random_before = *before.random;
    const mc_entity_base_fields *position = mc_entity_runtime_fields(camera);
    const double x = position->x, y = position->y, z = position->z;
    const uint64_t nan_bits = UINT64_C(0x7ff8000000000000);
    double nan;
    memcpy(&nan, &nan_bits, sizeof(nan));
    const double particle_x[] = {x + 17.0, x, nan, x + 16.0};
    for (size_t i = 0; i < COUNT(particle_x); ++i) {
        mc_world_storage_error cause = {0};
        mc_world_access_failure failure = {0};
        const mc_world_storage_status status = mc_world_access_call_particle(
            identity, world, NULL, particle_x[i], y, z, 0, 0, 0, &cause, &failure);
        if (i == 0) {
            /* Squared distance289 returns103 before String.equals107. */
            CHECK(status == MC_WORLD_STORAGE_OK && cause.status == MC_WORLD_STORAGE_OK &&
                  failure.receipt == NULL && failure.render_global == NULL);
        } else {
            /* Inside, NaN and exact squared distance256 all reach equals107.
             * Original dcmpl99 yields -1 for NaN, so ifle104 admits it. */
            CHECK(
                status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && failure.receiver == identity &&
                failure.receipt != NULL && failure.render_global != NULL &&
                failure.render_global->status == MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE &&
                strcmp(failure.render_global->source_class, "n") == 0 &&
                strcmp(failure.render_global->source_method, "a") == 0 &&
                strcmp(failure.render_global->source_descriptor, "(Ljava/lang/String;DDDDDD)V") ==
                    0 &&
                failure.render_global->source_pc == 107 && !failure.render_global->native_policy &&
                !failure.render_global->source_returned && failure.render_global->particle == NULL);
            const char *exception = NULL;
            CHECK(mc_world_access_failure_source_exception(&failure, &exception) &&
                  strcmp(exception, "java/lang/NullPointerException") == 0);
            if (i == 1)
                CHECK(mc_world_access_failure_retain(&failure, &captured) == MC_WORLD_STORAGE_OK);
            else
                CHECK(failure.receipt != captured.receipt && captured.render_global != NULL &&
                      captured.render_global->source_pc == 107);
        }
    }
    mc_render_global_access_state state;
    CHECK(mc_render_global_access_inspect(owner->access, &state) && state.registrations == 1 &&
          state.active_callbacks == 0 && state.retained_failures == 3);
    CHECK(mc_client_effect_renderer_read(renderer, &after) && after.random == before.random &&
          after.random->state == random_before.state && after.world == world &&
          after.layers == before.layers && !after.tick_pending);
    for (size_t i = 0; i < 4; ++i)
        CHECK(after.lists[i] == before.lists[i] && after.sizes[i] == 0 &&
              after.capacities[i] == before.capacities[i]);
    CHECK(mc_render_global_access_detach_all(owner->access, error) ==
              MC_CLIENT_RENDER_GLOBAL_BUSY &&
          world->world_accesses.count == original_memberships + 1u &&
          captured.render_global->source_pc == 107);
    mc_world_access_failure_release(&captured);
    CHECK(mc_client_render_global_retire(&owner, error) == MC_CLIENT_RENDER_GLOBAL_OK &&
          owner == NULL && world->world_accesses.count == original_memberships &&
          world->persistent_bindings == original_bindings + 1u);
    CHECK(mc_client_effect_renderer_free(&renderer) == MC_EFFECT_RENDERER_OK && renderer == NULL &&
          world->persistent_bindings == original_bindings);
    result = true;
cleanup:
    mc_world_access_failure_release(&captured);
    if (owner != NULL &&
        mc_client_render_global_retire(&owner, error) != MC_CLIENT_RENDER_GLOBAL_OK)
        result = false;
    if (mc_client_effect_renderer_free(&renderer) != MC_EFFECT_RENDERER_OK)
        result = false;
    free(error);
    free(effects);
    free(app);
    return result;
}
