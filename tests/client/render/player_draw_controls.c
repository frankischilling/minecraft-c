/* Genuine completed Player/World and TH/DS constructors. No Display is
 * supplied: these controls certify only reached source prefixes, never a
 * returned GL preview, model geometry or whole App constructor. */
int mc_player_draw_unused_statistics_main(int argc, char **argv);
#define main mc_player_draw_unused_statistics_main
#include "../statistics/test_player_dispatch.c"
#undef main

#include "../../support/java_random.h"
#include "client/render/living_draw_internal.h"
#include "client/render/manager/internal.h"
#include "mc/client/input/movement_input.h"
#include "mc/entity/player/input.h"
#include "world/runtime/runtime_internal.h"

static bool prefix_case(const char *resources, unsigned int mode) {
    fixture *f = calloc(1, sizeof(*f));
    mc_client_manager_error *error = calloc(1, sizeof(*error));
    char directory[48];
    CHECK(f != NULL && error != NULL &&
          snprintf(directory, sizeof(directory), "player-draw-%u", mode) > 0 &&
          setup(f, resources, directory, true));
    mc_client_render_manager_classes *classes = NULL;
    mc_client_render_manager *manager = NULL;
    CHECK(mc_client_render_manager_classes_bind(f->app.render_classes, f->app.registry,
                                                f->app.profile, NULL, &classes,
                                                error) == MC_CLIENT_MANAGER_OK &&
          mc_client_render_manager_static_get(classes, &manager, error) == MC_CLIENT_MANAGER_OK);
    mc_client_managed_renderer *ds = NULL;
    for (size_t i = 0; i < manager->allocated_renderers; ++i)
        if (strcmp(manager->renderers[i]->source_class, "ds") == 0)
            ds = manager->renderers[i];
    CHECK(ds != NULL && ds->own_model == ds->primary && ds->g != NULL && ds->h != NULL);
    manager_model *a = ds->own_model, *g = ds->g, *h = ds->h;
    mc_movement_input *input = NULL;
    if (mode != 0) {
        CHECK(mc_movement_input_create(&input, NULL) == MC_MOVEMENT_INPUT_OK &&
              mc_local_player_set_movement_input(f->player, input, NULL) == MC_PLAYER_OK);
        if (mode >= 5) {
            const mc_movement_input_fields fields = {.sneak = true};
            CHECK(mc_movement_input_set_fields(input, &fields, NULL) == MC_MOVEMENT_INPUT_OK);
            f->player->local_player->fields.player.u = mode == 6;
        }
    }
    mc_item_stack *stack = NULL;
    if (mode >= 2)
        CHECK(mc_item_stack_allocate(&stack) == MC_ITEM_STACK_OK &&
              mc_item_stack_construct(stack, 267, 1, 37) == MC_ITEM_STACK_OK &&
              mc_inventory_player_set(f->player->local_player->fields.player.c, 0, stack, NULL) ==
                  MC_INVENTORY_OK);
    /* Distinct authored source inputs for a terminal, independent invocation. */
    if (mode == 3)
        ds->own_model = NULL;
    if (mode == 4)
        ds->h = NULL;
    const mc_java_random own = *f->player->random, world = *f->world->fields.r;
    const size_t player_holds = f->player->holds, bindings = f->world->persistent_bindings;
    mc_client_render_manager_draw_call *call = NULL;
    mc_client_manager_draw_services services = {0};
    CHECK(mc_client_render_manager_coordinates_draw_begin(manager, f->player, 1, 2, 3, 0, 1,
                                                          &services, NULL, &call,
                                                          NULL) != MC_CLIENT_MANAGER_OK &&
          call != NULL);
    mc_client_manager_draw_state state;
    CHECK(mc_client_render_manager_draw_inspect(call, &state) && state.lookup_returned &&
          state.renderer == ds && !state.returned && !state.primary_returned &&
          state.living->player.entered && state.living->player.initial_stack_returned &&
          state.living->player.initial_stack_identity == (uintptr_t)stack &&
          !state.living->player.wrapper_returned && !state.living->player.cleared_flag_stores &&
          !state.living->primary_render_returned && !state.living->player.layers_entered);
    const mc_client_living_draw_error *failure = state.failure->living;
    CHECK(failure != NULL && failure->site.pc == (mode == 0   ? 55
                                                  : mode == 3 ? 32
                                                  : mode == 4 ? 36
                                                              : 0));
    if (mode == 0 || mode == 3 || mode == 4)
        CHECK(failure->source_exception && !failure->native_policy &&
              strcmp(failure->exception_class, "java/lang/NullPointerException") == 0 &&
              strcmp(failure->site.class_name, "ds") == 0 &&
              state.living->player.held_flag_stores == (mode == 3   ? 0u
                                                        : mode == 4 ? 1u
                                                                    : 3u) &&
              state.living->player.sneak_flag_stores == 0);
    else
        CHECK(failure->native_policy && !failure->source_exception &&
              strcmp(failure->site.class_name, "gv") == 0 &&
              state.living->player.held_flag_stores == 3 &&
              state.living->player.sneak_flag_stores == 3 &&
              state.living->player.sneaking == (mode == 5));
    CHECK(a->fields.k == (mode >= 2 && mode != 3) &&
          g->fields.k == (mode >= 2 && mode != 3 && mode != 4) &&
          h->fields.k == (mode >= 2 && mode != 3 && mode != 4) && a->fields.l == (mode == 5) &&
          g->fields.l == (mode == 5) && h->fields.l == (mode == 5));
    /* Restore aliases only after this source invocation ended. No failed draw
     * is resumed, and no model flag or source effect is cleared. */
    ds->own_model = a;
    ds->h = h;
    CHECK(mc_test_java_random_equal(&own, f->player->random) &&
          mc_test_java_random_equal(&world, f->world->fields.r) &&
          f->world->persistent_bindings == bindings &&
          mc_client_render_manager_draw_retire(&call, NULL) == MC_CLIENT_MANAGER_OK &&
          call == NULL && f->player->holds == player_holds);
    if (stack != NULL) {
        mc_item_stack_fields fields;
        CHECK(mc_item_stack_fields_get(stack, &fields) == MC_ITEM_STACK_OK && fields.id == 267 &&
              fields.count == 1 && fields.damage == 37);
        mc_item_stack_release(stack);
    }
    CHECK(mc_movement_input_retire(&input, NULL) == MC_MOVEMENT_INPUT_OK);
    CHECK(mc_client_render_manager_classes_retire(&classes, error) == MC_CLIENT_MANAGER_OK &&
          teardown(f));
    free(error);
    free(f);
    return true;
}

static bool controls(const char *resources) {
    for (unsigned int mode = 0; mode < 7; ++mode)
        CHECK(prefix_case(resources, mode));
    printf("PASS: %zu Player draw prefix controls; no Display or whole-preview claim\n", checks);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3 || CD(argv[2]) != 0 || !mc_math_trig_init(&trig))
        return 2;
    return controls(argv[1]) ? 0 : 1;
}
