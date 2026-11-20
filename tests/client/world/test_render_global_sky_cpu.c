/* Genuine native owners; prepared source references qualify only the early
 * n.a(F) prefix, not App/World/N construction or a completed ordinary draw. */
#include "client/application_state.h"
#include "client/world/render_global_internal.h"
#include "client/world/render_global_sky_internal.h"
#include "world/runtime/runtime_internal.h"

#include <emmintrin.h>
#include <stdio.h>
#include <stdlib.h>

static unsigned int checks;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            fprintf(stderr, "Sky CPU check%u line%d: %s\n", checks, __LINE__, #x);                 \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

int main(void) {
    mc_client_render_engine_classes *graphics = NULL;
    mc_client_render_global *owner = NULL;
    mc_java_random_profile *profile = NULL;
    mc_math_classes *math = NULL;
    mc_world_runtime_classes *world_classes = NULL;
    mc_world_runtime *world = NULL;
    mc_world_provider *dimension = NULL;
    mc_math_trig *table = malloc(sizeof(*table));
    client_application *app = calloc(1, sizeof(*app));
    mc_client_render_global_sky_error *error = calloc(1, sizeof(*error));
    CHECK(table != NULL && app != NULL && error != NULL && mc_math_trig_init(table));
    CHECK(mc_java_random_profile_create(SIZE_MAX, &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_math_classes_create(SIZE_MAX, &math, NULL) == MC_MATH_CLASS_OK);
    CHECK(mc_client_render_engine_classes_create(NULL, &graphics) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_render_global_allocate(graphics, NULL, &owner, NULL) ==
          MC_CLIENT_RENDER_GLOBAL_OK);
    CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, profile, table, &world_classes,
                                          NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA, world_classes,
                                          NULL, &world, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_provider_create(-1, NULL, &dimension, NULL) == MC_WORLD_PROVIDER_OK);
    CHECK(mc_world_provider_bind(dimension, NULL, NULL) == MC_WORLD_PROVIDER_OK);
    CHECK(mc_client_application_lifetime_register(app, NULL) == MC_CLIENT_APPLICATION_OWNER_OK);
    mc_client_render_global_sky_call *call = NULL, *other = NULL;
    mc_client_render_global_sky_state state;
    for (unsigned int mode = 0; mode < 4; ++mode) {
        owner->fields.t = mode == 0 ? NULL : app;
        app->active_world = mode >= 2 ? world : NULL;
        if (mode == 3) {
            CHECK(mc_world_provider_retain(dimension) == MC_WORLD_PROVIDER_OK);
            world->fields.t = dimension;
        }
        const unsigned int saved = _mm_getcsr();
        _mm_setcsr((saved & ~0x6000u) | 0x2000u);
        const mc_client_render_global_status status =
            mc_client_render_global_sky_begin(owner, math, .375f, &call, error);
        _mm_setcsr(saved);
        CHECK(call != NULL && mc_client_render_global_sky_inspect(call, &state));
        CHECK(status ==
              (mode == 3 ? MC_CLIENT_RENDER_GLOBAL_OK : MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE));
        CHECK(state.source_pc == (mode == 0 ? 4u : mode == 1 ? 7u : mode == 2 ? 10u : 16u));
        CHECK(state.gl_calls == 0 && state.draws == 0 && state.vertices == 0 &&
              state.returned == (mode == 3) && state.nether_skipped == (mode == 3) &&
              !state.sky_returned && !state.top_called && !state.sunrise_returned &&
              owner->display == NULL && owner->fields.k == NULL && owner->holds == 2 &&
              owner->sky_call == call);
        CHECK(mode == 3 || !error->native_policy);
        CHECK(mc_client_render_global_sky_begin(owner, math, 0, &other, error) ==
                  MC_CLIENT_RENDER_GLOBAL_BUSY &&
              other == NULL);
        CHECK(mc_client_render_global_retire(&owner, NULL) == MC_CLIENT_RENDER_GLOBAL_BUSY &&
              owner != NULL);
        mc_math_class_state math_state;
        mc_client_nw_state nw;
        CHECK(mc_math_classes_state(math, &math_state, NULL) == MC_MATH_CLASS_OK &&
              math_state.phase == MC_MATH_CLASS_COLD);
        CHECK(mc_client_nw_inspect(graphics, &nw) && nw.phase == MC_CLIENT_RENDER_ENGINE_RAW &&
              nw.a == NULL);
        printf("SKY_PREFIX %u %u\n", mode, state.returned ? 1u : 0u);
        CHECK(mc_client_render_global_sky_retire(&call, error) == MC_CLIENT_RENDER_GLOBAL_OK &&
              call == NULL && owner->holds == 1 && owner->sky_call == NULL);
    }
    owner->fields.t = NULL;
    app->active_world = NULL;
    CHECK(mc_client_render_global_retire(&owner, NULL) == MC_CLIENT_RENDER_GLOBAL_OK &&
          owner == NULL);
    CHECK(mc_client_render_engine_classes_release(graphics) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_application_lifetime_retire(app, NULL) == MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(mc_world_runtime_free(world, NULL) == MC_WORLD_STORAGE_OK);
    mc_world_provider_release(dimension);
    CHECK(mc_world_runtime_classes_free(world_classes, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_math_classes_release(&math, NULL) == MC_MATH_CLASS_OK);
    mc_java_random_profile_free(profile);
    free(table);
    free(app);
    free(error);
    printf("Source sky CPU: %u checks\n", checks);
    return 0;
}
