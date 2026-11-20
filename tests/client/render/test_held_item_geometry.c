/* Genuine Player/Registry and NW CPU owners. The same entered geometry groups
 * feed the production RA draw; this component executes no Display or GL. */
int mc_held_geometry_player_fixture_main(int argc, char **argv);
#define main mc_held_geometry_player_fixture_main
#include "../statistics/test_player_dispatch.c"
#undef main

#include "../../support/java_random.h"
#include "client/world/entity_renderer/held_bow_geometry_internal.h"
#include "entity/render/internal.h"
#include "item/registry/registry_internal.h"
#include "item/stack/source_internal.h"
#include "world/runtime/runtime_internal.h"
#include <inttypes.h>
#include <xmmintrin.h>

static uint32_t geometry_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static bool geometry_group(int32_t icon, unsigned int batch, bool emit) {
    mc_client_render_engine_classes *graphics = NULL;
    mc_client_tessellator *nw = NULL;
    mc_client_graphics_error *error = calloc(1, sizeof(*error));
    mc_held_bow_error *geometry = calloc(1, sizeof(*geometry));
    CHECK(error != NULL && geometry != NULL &&
          mc_client_render_engine_classes_allocate_context(NULL, NULL, NULL, &graphics) ==
              MC_CLIENT_RENDER_ENGINE_OK &&
          mc_client_nw_active_use(
              graphics, (mc_client_graphics_site){"ra", "a", "(Lls;Liz;)V", 124}, &nw, error));
    CHECK(entity_renderer_held_item_geometry_batch(icon, batch, nw, graphics, geometry));
    const size_t vertices = batch < 2 ? 4u : 64u;
    const size_t words = batch < 2 ? 48u : 768u;
    mc_client_nw_fields actual;
    CHECK(mc_client_nw_fields_get(nw, &actual) && actual.w && actual.m && actual.n && !actual.l &&
          actual.h == (int32_t)(vertices / 4u * 6u) && actual.o == (int32_t)words &&
          geometry->state.begin_returns == 1 && geometry->state.normal_returns == 1 &&
          geometry->state.vertex_returns == vertices && geometry->state.draw_attempts == 0 &&
          !geometry->state.fragment_completed && !geometry->state.pop_returned &&
          !geometry->state.rescale_enabled);
    /* Java's signed division/remainder and multiply wrap remain observable
     * for icons outside the ordinary atlas, with every f32 step preserved. */
    const int32_t column = mc_java_i32_mul(icon % 16, 16);
    const int32_t row = mc_java_i32_mul(icon / 16, 16);
    CHECK(geometry_bits(geometry->state.u0) == geometry_bits(((float)column + 0.0f) / 256.0f) &&
          geometry_bits(geometry->state.u1) == geometry_bits(((float)column + 15.99f) / 256.0f) &&
          geometry_bits(geometry->state.v0) == geometry_bits(((float)row + 0.0f) / 256.0f) &&
          geometry_bits(geometry->state.v1) == geometry_bits(((float)row + 15.99f) / 256.0f));
    for (size_t i = 0; i < words / 8u; ++i) {
        /* No Item geometry color call writes slot5; source NW leaves slot7
         * and duplicated vertices' normal slot6 untouched. Fresh Java/C int
         * arrays start zero, rather than receiving fabricated normal words. */
        CHECK(actual.g[i * 8u + 5u] == 0 && actual.g[i * 8u + 7u] == 0);
        if (i % 6u == 3u || i % 6u == 4u)
            CHECK(actual.g[i * 8u + 6u] == 0);
    }
    if (emit) {
        printf("HELD_ITEM_GEOMETRY {\"icon\":%d,\"batch\":%u,\"words\":[", icon, batch);
        for (size_t i = 0; i < words; ++i)
            printf("%s\"%08" PRIx32 "\"", i ? "," : "", (uint32_t)actual.g[i]);
        puts("]}");
    }
    CHECK(mc_client_render_engine_classes_release(graphics) == MC_CLIENT_RENDER_ENGINE_OK);
    free(error);
    free(geometry);
    return true;
}

static bool geometry_failures(void) {
    static const unsigned int begins[] = {268, 339, 427, 558, 691, 828};
    mc_client_render_engine_classes *graphics = NULL;
    mc_client_tessellator *nw = NULL;
    mc_client_graphics_error *error = calloc(1, sizeof(*error));
    mc_held_bow_error *geometry = calloc(1, sizeof(*geometry));
    CHECK(error != NULL && geometry != NULL &&
          mc_client_render_engine_classes_allocate_context(NULL, NULL, NULL, &graphics) ==
              MC_CLIENT_RENDER_ENGINE_OK &&
          mc_client_nw_active_use(
              graphics, (mc_client_graphics_site){"ra", "a", "(Lls;Liz;)V", 124}, &nw, error));
    for (unsigned int batch = 0; batch < 6; ++batch)
        CHECK(!entity_renderer_held_item_geometry_batch(66, batch, NULL, graphics, geometry) &&
              geometry->status == MC_HELD_BOW_NULL_NW && geometry->source_null_invocation &&
              geometry->site.pc == (int32_t)begins[batch] && geometry->state.begin_attempts == 1 &&
              geometry->state.begin_returns == 0 && geometry->state.vertex_attempts == 0 &&
              geometry->state.draw_attempts == 0);
    CHECK(!entity_renderer_held_item_geometry_batch(66, 6, nw, graphics, geometry) &&
          geometry->status == MC_HELD_BOW_ARGUMENT && geometry->state.begin_attempts == 0);
    const unsigned int csr = _mm_getcsr();
    _mm_setcsr(csr | 0x8040u); /* Declared native FP prerequisite refusal. */
    const bool emitted = entity_renderer_held_item_geometry_batch(66, 0, nw, graphics, geometry);
    _mm_setcsr(csr);
    mc_client_nw_fields actual;
    CHECK(!emitted && geometry->status == MC_HELD_BOW_FP && geometry->site.pc == 143 &&
          geometry->state.begin_attempts == 0 && mc_client_nw_fields_get(nw, &actual) &&
          !actual.w && actual.h == 0 && actual.o == 0);
    CHECK(entity_renderer_held_item_geometry_batch(66, 0, nw, graphics, geometry) &&
          mc_client_nw_fields_get(nw, &actual));
    int32_t before[48];
    memcpy(before, actual.g, sizeof(before));
    const mc_client_nw_fields entered = actual;
    /* A second entered call sees the real unfinished NW batch. Refusal must
     * preserve that producer's Java IllegalStateException and backing prefix,
     * without aborting, emitting a normal, or inventing a completed draw. */
    CHECK(!entity_renderer_held_item_geometry_batch(66, 1, nw, graphics, geometry) &&
          geometry->status == MC_HELD_BOW_GRAPHICS && geometry->site.pc == 339 &&
          geometry->graphics.status == MC_CLIENT_GRAPHICS_SOURCE_ILLEGAL_STATE &&
          !geometry->graphics.native_policy && geometry->graphics.site.pc == 16 &&
          geometry->state.begin_attempts == 1 && geometry->state.begin_returns == 0 &&
          geometry->state.normal_attempts == 0 && geometry->state.vertex_attempts == 0 &&
          geometry->state.draw_attempts == 0 && mc_client_nw_fields_get(nw, &actual) &&
          actual.w == entered.w && actual.h == entered.h && actual.o == entered.o &&
          actual.m == entered.m && actual.n == entered.n && actual.l == entered.l &&
          actual.r == entered.r && actual.x == entered.x &&
          memcmp(before, actual.g, sizeof(before)) == 0);
    CHECK(mc_client_render_engine_classes_release(graphics) == MC_CLIENT_RENDER_ENGINE_OK);
    free(error);
    free(geometry);
    return true;
}

static bool icon_controls(fixture *f) {
    static const int32_t ids[] = {268, 272, 267, 276, 283, 261};
    static const int32_t icons[] = {64, 65, 66, 67, 68, 21};
    mc_entity_render_error *error = calloc(1, sizeof(*error));
    mc_item_stack *stack = NULL;
    CHECK(error != NULL);
    const mc_java_random own = *f->player->random, world_random = *f->world->fields.r;
    const size_t bindings = f->world->persistent_bindings;
    int64_t time_before, time_after;
    CHECK(mc_world_info_time_get(f->world->fields.x, &time_before) == MC_WORLD_INFO_OK);
    for (size_t i = 0; i < sizeof ids / sizeof *ids; ++i) {
        CHECK(mc_item_stack_allocate(&stack) == MC_ITEM_STACK_OK &&
              mc_item_stack_construct(stack, ids[i], 1, 250) == MC_ITEM_STACK_OK);
        mc_item_stack_fields before, after;
        CHECK(mc_item_stack_fields_get(stack, &before) == MC_ITEM_STACK_OK);
        int32_t icon = -77;
        CHECK(mc_entity_render_item_icon(f->player, f->app.registry, stack, &icon, error) &&
              icon == icons[i] && strcmp(error->site.class_name, "gs") == 0 &&
              error->site.pc == 5 && strcmp(error->parent.class_name, "gs") == 0 &&
              error->parent.pc == 2 &&
              mc_item_stack_fields_get(stack, &after) == MC_ITEM_STACK_OK &&
              memcmp(&before, &after, sizeof(before)) == 0);
        printf("HELD_ITEM_ICON id=%d icon=%d source=gs.c:2-ls.c:1 returned=gs.c:5\n", ids[i], icon);
        mc_item_stack_release(stack);
        stack = NULL;
    }
    /* These are authored source-static inputs for distinct component calls,
     * restored only after each call has returned. No running frame is repaired. */
    CHECK(mc_item_stack_allocate(&stack) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(stack, 267, 1, 250) == MC_ITEM_STACK_OK);
    mc_registry_item_array *array = f->app.registry->items.c;
    mc_item *sword = array->elements[267];
    array->elements[267] = array->elements[261];
    int32_t icon = -77;
    const bool swapped =
        mc_entity_render_item_icon(f->player, f->app.registry, stack, &icon, error);
    array->elements[267] = sword;
    CHECK(swapped && icon == 21); /* Fresh actual GM.c element, not cached Sword.bh. */
    f->app.registry->items.c = NULL;
    icon = -77;
    const bool null_array =
        mc_entity_render_item_icon(f->player, f->app.registry, stack, &icon, error);
    f->app.registry->items.c = array;
    CHECK(!null_array && icon == -77 && error->source_exception &&
          strcmp(error->exception_class, "java/lang/NullPointerException") == 0 &&
          error->parent.pc == 2 && error->site.pc == 1 && error->item.site.pc == 7);
    CHECK(!mc_entity_render_item_icon(f->player, f->app.registry, NULL, &icon, error) &&
          error->source_exception && error->parent.pc == 2 && error->site.pc == 1 &&
          strcmp(error->exception_class, "java/lang/NullPointerException") == 0);
    CHECK(mc_test_java_random_equal(&own, f->player->random) &&
          mc_test_java_random_equal(&world_random, f->world->fields.r) &&
          bindings == f->world->persistent_bindings &&
          mc_world_info_time_get(f->world->fields.x, &time_after) == MC_WORLD_INFO_OK &&
          time_before == time_after);
    mc_item_stack_release(stack);
    free(error);
    return true;
}

static bool geometry_controls(const char *resources) {
    fixture *f = calloc(1, sizeof(*f));
    CHECK(f != NULL && setup(f, resources, "geometry", true) && icon_controls(f));
    static const int32_t icons[] = {64, 65, 66, 67, 68, 21};
    for (size_t i = 0; i < sizeof icons / sizeof *icons; ++i)
        for (unsigned int batch = 0; batch < 6; ++batch)
            CHECK(geometry_group(icons[i], batch, true));
    static const int32_t edges[] = {-1, INT32_MIN, INT32_MAX};
    for (size_t i = 0; i < sizeof edges / sizeof *edges; ++i)
        for (unsigned int batch = 0; batch < 6; ++batch)
            CHECK(geometry_group(edges[i], batch, false));
    CHECK(geometry_failures() && teardown(f));
    free(f);
    printf("PASS: %zu held Item geometry/selector checks; 36 exact source groups; no Display\n",
           checks);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3 || CD(argv[2]) != 0 || !mc_math_trig_init(&trig))
        return 2;
    return geometry_controls(argv[1]) ? 0 : 1;
}
