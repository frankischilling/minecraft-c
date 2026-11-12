#include "client/render_engine/lighting.h"

static int lighting_controls(void) {
    mc_client_render_engine_classes *c = NULL;
    mc_client_graphics_error error;
    mc_client_lighting_state state;
    for (size_t quota = 0; quota <= 4; ++quota) {
        const mc_client_render_engine_options options = {1024u * 1024u, quota};
        CHECK(mc_client_render_engine_classes_create(&options, &c) == MC_CLIENT_RENDER_ENGINE_OK);
        CHECK(mc_client_lighting_inspect(c, &state) && state.phase == MC_CLIENT_RENDER_ENGINE_RAW &&
              state.a == NULL && c->ge_phase == MC_CLIENT_RENDER_ENGINE_RAW &&
              c->arena.allocations == 0);
        const bool initialized = mc_client_lighting_active_use(c, caller, &error);
        CHECK(initialized == (quota == 4));
        CHECK(mc_client_lighting_inspect(c, &state) && c->arena.allocations == quota);
        if (!initialized) {
            const int32_t pc = quota == 0 ? 0 : quota == 1 ? 10 : quota == 2 ? 1 : 6;
            CHECK(state.phase == MC_CLIENT_RENDER_ENGINE_FAILED && state.a == NULL &&
                  error.status == MC_CLIENT_GRAPHICS_LIMIT && error.native_policy &&
                  error.site.pc == pc && strcmp(error.site.class_name, "ge") == 0 &&
                  error.parent.pc == 2 && strcmp(error.parent.class_name, "u") == 0);
            const mc_client_graphics_error original = *state.failure;
            c->arena.options.max_constructor_allocations = SIZE_MAX;
            CHECK(!mc_client_lighting_active_use(c, caller, &error) &&
                  error.status == MC_CLIENT_GRAPHICS_CLASS_FAILED && !error.native_policy &&
                  error.class_cause.status == MC_CLIENT_GRAPHICS_LIMIT &&
                  error.class_cause.site.pc == pc && c->arena.allocations == quota &&
                  state.failure->status == original.status &&
                  state.failure->site.pc == original.site.pc);
        } else {
            CHECK(state.phase == MC_CLIENT_RENDER_ENGINE_READY && state.a != NULL &&
                  state.a->position == 0 && state.a->limit == 16 && state.a->capacity == 16 &&
                  state.a->bytes->capacity == 64 && state.a->bytes->position == 0 && c->a != NULL &&
                  c->b != NULL && c->a->count == 0 && c->b->count == 0);
            const mc_client_direct_float_buffer *same = state.a;
            CHECK(mc_client_lighting_active_use(c, caller, &error) && c->u_a == same &&
                  c->arena.allocations == quota);
            CHECK(!mc_client_lighting_disable(c, NULL, caller, &error) &&
                  error.status == MC_CLIENT_GRAPHICS_DISPLAY_FAILED && error.native_policy &&
                  error.display.status == MC_CLIENT_DISPLAY_INVALID_ARGUMENT &&
                  strcmp(error.site.class_name, "u") == 0 && strcmp(error.site.method, "a") == 0 &&
                  error.site.pc == 3 && error.retained_count == 0 && c->u_a == same &&
                  c->u_phase == MC_CLIENT_RENDER_ENGINE_READY && c->active_source_calls == 0);
        }
        CHECK(mc_client_render_engine_classes_release(c) == MC_CLIENT_RENDER_ENGINE_OK);
        c = NULL;
    }
    for (unsigned mode = 0; mode < 2; ++mode) {
        CHECK(mc_client_render_engine_classes_create(NULL, &c) == MC_CLIENT_RENDER_ENGINE_OK);
        mc_client_direct_float_buffer *eager = NULL;
        if (mode != 0)
            CHECK(mc_client_ge_e(c, &c->arena, 16, caller, &eager, &error));
        CHECK(mc_client_lighting_active_use(c, caller, &error));
        const mc_client_direct_float_buffer *same = c->u_a;
        CHECK(same != NULL && same != eager && mc_client_lighting_active_use(c, caller, &error) &&
              c->u_a == same && same->position == 0 && same->limit == 16 && same->capacity == 16);
        for (size_t i = 0; i < same->bytes->capacity; ++i)
            CHECK(same->bytes->data[i] == 0);
        printf("LIGHTING %u 16 0 16 %u %u 1\n", mode, (unsigned)c->a->count, (unsigned)c->b->count);
        CHECK(mc_client_render_engine_classes_release(c) == MC_CLIENT_RENDER_ENGINE_OK);
        c = NULL;
    }
    /* Quota2 above reaches the same cold u allocation prefix as original OOM;
     * native quota status remains distinct from Java's exception category. */
    printf("LIGHTING_FAILURE 1 1 1\n");
    return 0;
}
