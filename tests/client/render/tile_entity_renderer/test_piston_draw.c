/* Authored direct HY gate/failure controls. No GL or gameplay is credited. */
#define MC_PICKING_BODY_MAIN_NAME mc_piston_draw_picking_fixture_main
int mc_piston_draw_picking_fixture_main(int argc, char **argv);
#include "../../world/test_picking.c"
#undef main

#include "client/render/tile_entity_renderer/draw_internal.h"
#include "client/render_engine/nw.h"

#include <xmmintrin.h>

static bool piston_input(mc_tile_entity *tile, int32_t id, uint32_t progress) {
    static const uint16_t names[5][9] = {{'b', 'l', 'o', 'c', 'k', 'I', 'd'},
                                         {'b', 'l', 'o', 'c', 'k', 'D', 'a', 't', 'a'},
                                         {'f', 'a', 'c', 'i', 'n', 'g'},
                                         {'p', 'r', 'o', 'g', 'r', 'e', 's', 's'},
                                         {'e', 'x', 't', 'e', 'n', 'd', 'i', 'n', 'g'}};
    const size_t lengths[] = {7, 9, 6, 8, 9};
    mc_nbt_tag fields[5] = {0};
    const mc_nbt_tag *items[5];
    for (size_t i = 0; i < 5; ++i) {
        fields[i].name = (mc_nbt_string){names[i], lengths[i]};
        fields[i].type = i < 3 ? MC_NBT_INT : i == 3 ? MC_NBT_FLOAT : MC_NBT_BYTE;
        items[i] = &fields[i];
    }
    fields[0].value.int_value = id;
    fields[2].value.int_value = 3;
    fields[3].value.float_bits = progress;
    fields[4].value.byte_value = 1;
    const mc_nbt_tag compound = {.type = MC_NBT_COMPOUND, .value.compound = {items, 5}};
    CHECK(mc_tile_entity_read_nbt(tile, &compound, NULL) == MC_TILE_ENTITY_OK);
    return true;
}

static bool piston_gates(picking_fixture *f) {
    mc_client_tile_renderer_classes *classes = NULL;
    mc_client_tile_renderer_dispatcher *manager = NULL;
    mc_client_tile_renderer_error owner;
    CHECK(mc_client_tile_renderer_classes_bind(f->models, f->base.world_classes, f->base.profile,
                                               NULL, &classes,
                                               &owner) == MC_CLIENT_TILE_RENDERER_OK);
    CHECK(mc_client_tile_renderer_static_get(classes, &manager, &owner) ==
          MC_CLIENT_TILE_RENDERER_OK);
    mc_tile_entity *tile = NULL;
    CHECK(mc_tile_entity_allocate_in_classes(f->base.world_classes, MC_TILE_ENTITY_PISTON, &tile) ==
          MC_TILE_ENTITY_OK);
    CHECK(mc_tile_entity_piston_construct(tile, 33, 3, 3, true, false, NULL) == MC_TILE_ENTITY_OK);
    mc_client_manager_draw_services services = {.graphics = f->graphics, .math = f->math};
    const uint32_t inputs[] = {UINT32_C(0x3f800000), UINT32_C(0x40000000), UINT32_C(0x7f800000),
                               UINT32_C(0x7fc01234), UINT32_C(0xff801234)};
    const unsigned int original = _mm_getcsr();
    size_t header_bytes = 0;
    for (size_t i = 0; i < sizeof(inputs) / sizeof(*inputs); ++i) {
        CHECK(piston_input(tile, 33, inputs[i]));
        mc_client_tile_renderer_draw_call *call = NULL;
        mc_client_tile_renderer_draw_error error;
        mc_client_tile_renderer_draw_state state;
        CHECK(mc_client_tile_renderer_coordinates_draw_begin(manager, tile, 1, 2, 3, .5f, &services,
                                                             NULL, &call,
                                                             &error) == MC_CLIENT_TILE_RENDERER_OK);
        CHECK(mc_client_tile_renderer_draw_inspect(call, &state) && state.returned &&
              state.bridge_cast_returned && state.piston_progress_returned &&
              state.piston_skipped && !state.texture_returned && !state.tessellator_begun &&
              state.gl_calls == 0 && state.piston_cv_identity == 0 && state.source_pc == 25);
        if (i == 0)
            header_bytes = state.allocation_bytes;
        CHECK(state.allocation_bytes == header_bytes && call->piston == NULL);
        CHECK(mc_client_tile_renderer_classes_retire(&classes, &owner) ==
              MC_CLIENT_TILE_RENDERER_BUSY);
        CHECK(mc_client_tile_renderer_draw_retire(&call, &error) == MC_CLIENT_TILE_RENDERER_OK &&
              call == NULL);
    }
    for (int i = 0; i < 3; ++i) {
        CHECK(piston_input(tile, i == 0 ? -1 : i == 1 ? 256 : 0, 0));
        _mm_setcsr(original | UINT32_C(0x8000));
        mc_client_tile_renderer_draw_call *call = NULL;
        mc_client_tile_renderer_draw_error error;
        mc_client_tile_renderer_draw_state state;
        const mc_client_tile_renderer_status status =
            mc_client_tile_renderer_coordinates_draw_begin(manager, tile, 1, 2, 3, .5f, &services,
                                                           NULL, &call, &error);
        CHECK((_mm_getcsr() & UINT32_C(0xffc0)) ==
              ((original | UINT32_C(0x8000)) & UINT32_C(0xffc0)));
        _mm_setcsr(original);
        CHECK(mc_client_tile_renderer_draw_inspect(call, &state) && state.bridge_cast_returned &&
              !state.piston_progress_returned && state.gl_calls == 0);
        if (i < 2) {
            CHECK(status == MC_CLIENT_TILE_RENDERER_BLOCKS_FAILED && error.site.pc == 7 &&
                  error.source_exception && !error.native_policy &&
                  strcmp(error.exception_class, "java/lang/ArrayIndexOutOfBoundsException") == 0);
        } else {
            CHECK(status == MC_CLIENT_TILE_RENDERER_OK && state.returned && state.piston_skipped);
        }
        CHECK(mc_client_tile_renderer_draw_retire(&call, &error) == MC_CLIENT_TILE_RENDERER_OK);
    }
    CHECK(piston_input(tile, 33, 0));
    {
        mc_client_tile_renderer_draw_call *call = NULL;
        mc_client_tile_renderer_draw_error error;
        mc_client_tile_renderer_draw_state state;
        mc_client_tile_renderer_draw_limits limits = mc_client_tile_renderer_draw_default_limits();
        limits.max_bytes = header_bytes;
        CHECK(mc_client_tile_renderer_coordinates_draw_begin(manager, tile, 1, 2, 3, .5f, &services,
                                                             &limits, &call, &error) ==
              MC_CLIENT_TILE_RENDERER_LIMIT);
        CHECK(mc_client_tile_renderer_draw_inspect(call, &state) && error.native_policy &&
              error.site.pc == 26 && state.piston_progress_returned && !state.returned &&
              state.allocation_bytes == header_bytes && call->piston == NULL);
        CHECK(mc_client_tile_renderer_draw_retire(&call, &error) == MC_CLIENT_TILE_RENDERER_OK);
    }
    for (int i = 0; i < 2; ++i) {
        mc_client_tile_renderer_draw_call *call = NULL;
        mc_client_tile_renderer_draw_error error;
        mc_client_tile_renderer_draw_state state;
        if (i == 0)
            _mm_setcsr(original | UINT32_C(0x8000));
        const mc_client_tile_renderer_status status =
            mc_client_tile_renderer_coordinates_draw_begin(manager, tile, 1, 2, 3, .5f, &services,
                                                           NULL, &call, &error);
        _mm_setcsr(original);
        CHECK(mc_client_tile_renderer_draw_inspect(call, &state) && !state.returned &&
              state.gl_calls == 0 && !state.tessellator_begun && !state.texture_returned);
        if (i == 0) {
            CHECK(status == MC_CLIENT_TILE_RENDERER_TILE_ENTITY_FAILED && error.native_policy &&
                  error.site.pc == 18 && error.child.pc == 20 &&
                  error.piston.status == MC_TILE_ENTITY_UNSUPPORTED_FP);
        } else {
            CHECK(status == MC_CLIENT_TILE_RENDERER_NULL_REFERENCE && error.source_exception &&
                  !error.native_policy && error.site.pc == 34 && error.child.pc == 11 &&
                  state.piston_progress_returned && !state.piston_skipped);
        }
        CHECK(mc_client_tile_renderer_draw_retire(&call, &error) == MC_CLIENT_TILE_RENDERER_OK);
    }
    {
        mc_client_render_engine_classes *limited = NULL;
        const mc_client_render_engine_options zero = {0};
        mc_client_graphics_error graphics;
        mc_client_tessellator *nw = NULL;
        CHECK(mc_client_render_engine_classes_allocate_context(NULL, &zero, NULL, &limited) ==
              MC_CLIENT_RENDER_ENGINE_OK);
        CHECK(!mc_client_nw_active_use(
                  limited, (mc_client_graphics_site){"native-control", "prepare", "()V", 0}, &nw,
                  &graphics) &&
              graphics.native_policy && graphics.status == MC_CLIENT_GRAPHICS_LIMIT && nw == NULL);
        mc_client_manager_draw_services limited_services = services;
        limited_services.graphics = limited;
        mc_client_tile_renderer_draw_call *call = NULL;
        mc_client_tile_renderer_draw_error error;
        mc_client_tile_renderer_draw_state state;
        CHECK(mc_client_tile_renderer_coordinates_draw_begin(
                  manager, tile, 1, 2, 3, .5f, &limited_services, NULL, &call, &error) ==
              MC_CLIENT_TILE_RENDERER_GRAPHICS_FAILED);
        CHECK(mc_client_tile_renderer_draw_inspect(call, &state) && !state.returned &&
              state.piston_progress_returned && !state.texture_returned && state.gl_calls == 0 &&
              error.site.pc == 26 && !error.source_exception && error.exception_class == NULL &&
              error.native_policy && error.graphics.status == MC_CLIENT_GRAPHICS_CLASS_FAILED &&
              error.graphics.class_cause.status == MC_CLIENT_GRAPHICS_LIMIT &&
              error.graphics.class_cause.native_policy);
        CHECK(mc_client_tile_renderer_draw_retire(&call, &error) == MC_CLIENT_TILE_RENDERER_OK &&
              mc_client_render_engine_classes_release(limited) == MC_CLIENT_RENDER_ENGINE_OK);
    }
    mc_tile_entity_release(tile);
    CHECK(mc_client_tile_renderer_classes_retire(&classes, &owner) == MC_CLIENT_TILE_RENDERER_OK);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3)
        return 2;
    picking_fixture *f = calloc(1, sizeof(*f));
    if (f == NULL || !picking_open(f, argv[1]) || !piston_gates(f) || !picking_close(f))
        return 1;
    free(f);
    printf("PASS: %u Source Piston draw gate checks\n", checks);
    return 0;
}
