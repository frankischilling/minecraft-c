/* Reuse genuine App/World/Player construction and checked retirement. The
 * renamed statistics main is not run. This is a CPU WorldAccess contract;
 * no completed Minecraft/RenderGlobal graphics constructor is claimed. */
#define main mc_redstone_factory_statistics_fixture_main
#include "../statistics/test_player_dispatch.c"
#undef main

#include "block/registry/runtime_internal.h"
#include "client/particles/effect_renderer_internal.h"
#include "client/render_engine/internal.h"
#include "client/startup/application_owner.h"
#include "client/world/render_global_access.h"
#include "client/world/render_global_internal.h"
#include "mc/entity/particles/particle.h"
#include "mc/world/runtime/presentation.h"
#include "world/runtime/mutation/mutation_internal.h"
#include "world/runtime/runtime_internal.h"

#include <float.h>
#include <math.h>

#define COUNT(array) (sizeof(array) / sizeof((array)[0]))

#define REQUIRE(expression)                                                                        \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(expression)) {                                                                       \
            fprintf(stderr, "Redstone factory:%d: %s\n", __LINE__, #expression);                   \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

static bool redstone_factory(fixture *f) {
    bool result = false;
    bool lifetime = false;
    mc_client_render_engine_classes *graphics = NULL;
    mc_client_render_global *owner = NULL;
    mc_client_effect_renderer *effects = NULL;
    mc_math_classes *math = NULL;
    mc_world_access_failure retained = {0};
    mc_client_render_global_error *global_error = calloc(1, sizeof(*global_error));
    mc_world_mutation_error *error = calloc(1, sizeof(*error));
    const size_t original_memberships = f->world->world_accesses.count;
    const size_t original_bindings = f->world->persistent_bindings;
    REQUIRE(global_error != NULL && error != NULL && original_memberships == 0);
    REQUIRE(mc_client_application_lifetime_register(&f->app, NULL) ==
            MC_CLIENT_APPLICATION_OWNER_OK);
    lifetime = true;
    REQUIRE(mc_math_classes_create(SIZE_MAX, &math, NULL) == MC_MATH_CLASS_OK &&
            mc_client_render_engine_classes_create(NULL, &graphics) == MC_CLIENT_RENDER_ENGINE_OK &&
            mc_client_effect_renderer_allocate(f->app.entities, NULL, &effects, NULL) ==
                MC_EFFECT_RENDERER_OK &&
            mc_client_effect_renderer_construct(effects, f->world, NULL, NULL) ==
                MC_EFFECT_RENDERER_OK &&
            mc_client_render_global_allocate(graphics, NULL, &owner, global_error) ==
                MC_CLIENT_RENDER_GLOBAL_OK);
    /* These are the declared source field operands of the actual N callback.
     * Camera, World, MathHelper namespace and DN are genuine owners. */
    f->app.i = f->player;
    f->app.effect_renderer = effects;
    f->app.source_math_classes = math;
    owner->fields.t = &f->app;
    owner->fields.k = f->world;
    REQUIRE(mc_render_global_access_create(owner, &owner->access, global_error) ==
                MC_CLIENT_RENDER_GLOBAL_OK &&
            mc_render_global_access_source_add(owner->access, f->world, global_error) ==
                MC_CLIENT_RENDER_GLOBAL_OK &&
            f->world->world_accesses.count == original_memberships + 1u);
    const mc_registry_string *name = NULL;
    mc_registry_error registry = {0};
    REQUIRE(mc_registry_runtime_literal_ascii(f->app.registry, "reddust", &name, &registry) ==
            MC_REGISTRY_OK);
    mc_effect_renderer_snapshot before, after;
    REQUIRE(mc_client_effect_renderer_read(effects, &before));
    for (size_t i = 0; i < 4; ++i)
        REQUIRE(before.sizes[i] == 0);
    const mc_entity_base_fields *camera = mc_entity_runtime_fields(f->player);

    /* The first green component is the exact widened source float for power15:
     * (1*1*0.7f)-0.5f. The native N signature narrows its doubles before IM. */
    static const struct {
        const char *name;
        double red, green, blue;
    } cases[] = {{"powered-Dust15", 1, 0.19999998807907104, 0},
                 {"zero-red-Torch", 0, 0, 0},
                 {"double-underflow-red-fallback", 1e-300, 0, 0},
                 {"double-overflow-red", DBL_MAX, 0, 0}};

    mc_entity *published[COUNT(cases)] = {0};
    const uint64_t renderer_random = before.random->state;
    for (size_t index = 0; index < COUNT(cases); ++index) {
        *error = mc_world_mutation_empty_error();
        const mc_world_storage_status status = mc_world_runtime_particle(
            f->world, name, camera->x, camera->y, camera->z, cases[index].red, cases[index].green,
            cases[index].blue, error);
        if (status != MC_WORLD_STORAGE_OK) {
            const mc_client_render_global_error *lower = error->world_access_failure.render_global;
            fprintf(stderr,
                    "REDSTONE_FACTORY_FAIL case=%s status=%d WorldAccess=%u operation=%s "
                    "N.a=%u child=%s\n",
                    cases[index].name, (int)status, error->world_access_bytecode,
                    error->operation == NULL ? "NULL" : error->operation,
                    lower == NULL ? 0u : lower->source_pc,
                    lower == NULL || lower->child.class_name == NULL ? "NULL"
                                                                     : lower->child.class_name);
        }
        /* Unsupported allocation or an empty successful callback must fail;
         * neither result reproduces powered Redstone gameplay. */
        REQUIRE(status == MC_WORLD_STORAGE_OK && error->world_access_failure.receipt == NULL &&
                mc_client_effect_renderer_read(effects, &after) && after.world == f->world &&
                after.layers == before.layers && after.sizes[0] == index + 1u &&
                after.sizes[1] == 0 && after.sizes[2] == 0 && after.sizes[3] == 0 &&
                after.random == before.random && after.random->state == renderer_random);
        mc_entity *particle = effects->layers[0]->elements[index];
        mc_entity_class_literal_view literal;
        mc_particle_snapshot state;
        REQUIRE(particle != NULL && particle->classes == f->app.entities);
        REQUIRE(particle->phase == MC_ENTITY_READY && particle->base.world == f->world);
        REQUIRE(mc_entity_class_literal_inspect(particle->class_literal, &literal) == MC_ENTITY_OK);
        REQUIRE(literal.owner == f->app.entities && literal.client_name != NULL &&
                strcmp(literal.client_name, "im") == 0);
        const mc_entity_class_literal *base = NULL;
        bool assignable = false;
        REQUIRE(mc_entity_is_kind(particle, MC_ENTITY_CLASS_KIND_REDSTONE_PARTICLE) &&
                mc_entity_has_particle(particle));
        REQUIRE(mc_entity_class_literal_get(f->app.entities, MC_ENTITY_CLASS_KIND_PARTICLE,
                                            &base) == MC_ENTITY_OK &&
                mc_entity_class_literal_is_assignable_from(base, particle->class_literal,
                                                           &assignable) == MC_ENTITY_OK &&
                assignable);
        REQUIRE(mc_particle_read(particle, &state) && state.phase == MC_ENTITY_READY);
        REQUIRE(particle->holds == 1);
        REQUIRE(state.particle.k == 0);
        for (size_t i = 0; i < index; ++i)
            REQUIRE(particle != published[i] && effects->layers[0]->elements[i] == published[i]);
        published[index] = particle;
        if (index == 3)
            REQUIRE(isinf(state.particle.i) && state.particle.i > 0);
        else
            REQUIRE(isfinite(state.particle.i) && state.particle.i > 0 && state.particle.i <= 1);
        if (index == 0)
            REQUIRE(isfinite(state.particle.j) && state.particle.j > 0 &&
                    state.particle.j <= (float)cases[index].green);
        else
            REQUIRE(state.particle.j == 0);
    }
    /* Exact failed String.equals invocation remains retained independently of
     * all four published IM owners. A native detach cannot consume its held
     * producer, and releasing it does not replay the source particle call. */
    *error = mc_world_mutation_empty_error();
    REQUIRE(mc_world_runtime_particle(f->world, NULL, camera->x, camera->y, camera->z, 1, 0, 0,
                                      error) == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
            error->world_access_failure.render_global != NULL &&
            error->world_access_failure.render_global->source_pc == 107 &&
            mc_world_access_failure_retain(&error->world_access_failure, &retained) ==
                MC_WORLD_STORAGE_OK);
    mc_render_global_callback_state callback;
    REQUIRE(mc_render_global_access_callback_inspect(owner->access, &retained, &callback) &&
            !callback.source_returned && callback.particle_candidate == NULL &&
            mc_render_global_access_detach_all(owner->access, global_error) ==
                MC_CLIENT_RENDER_GLOBAL_BUSY &&
            mc_client_effect_renderer_read(effects, &after) && after.sizes[0] == COUNT(cases));
    mc_world_access_failure_release(&retained);
    result = true;
cleanup:
    mc_world_access_failure_release(&retained);
    if (owner != NULL &&
        mc_client_render_global_retire(&owner, global_error) != MC_CLIENT_RENDER_GLOBAL_OK)
        result = false;
    f->app.i = NULL;
    f->app.effect_renderer = NULL;
    f->app.source_math_classes = NULL;
    if (mc_client_effect_renderer_free(&effects) != MC_EFFECT_RENDERER_OK ||
        (graphics != NULL &&
         mc_client_render_engine_classes_release(graphics) != MC_CLIENT_RENDER_ENGINE_OK) ||
        mc_math_classes_release(&math, NULL) != MC_MATH_CLASS_OK ||
        f->world->world_accesses.count != original_memberships ||
        f->world->persistent_bindings != original_bindings)
        result = false;
    if (lifetime &&
        mc_client_application_lifetime_retire(&f->app, NULL) != MC_CLIENT_APPLICATION_OWNER_OK)
        result = false;
    free(error);
    free(global_error);
    return result;
}

int main(int argc, char **argv) {
    if (argc != 3 || !mc_math_trig_init(&trig))
        return 2;
    fixture *f = calloc(1, sizeof(*f));
    if (f == NULL)
        return 2;
    if (!setup(f, argv[1], argv[2], true)) {
        free(f);
        return 1;
    }
    const bool passed = redstone_factory(f);
    const bool retired = teardown(f);
    free(f);
    if (!passed || !retired)
        return 1;
    printf("PASS: %zu Redstone factory publication checks\n", checks);
    return 0;
}
