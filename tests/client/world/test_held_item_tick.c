/* Prepared body receivers on genuine allocated RA/Player/Inventory owners. */
#include "../../../src/inventory/internal.h"
#include "client/application_state.h"
#include "client/world/entity_renderer/internal.h"
#include "entity/player/player_internal.h"
#include "mc/client/world/held_item_tick.h"
#include "mc/inventory/player_current.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xmmintrin.h>

static unsigned int checks;
#define CHECK(value)                                                                               \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(value)) {                                                                            \
            fprintf(stderr, "Held-item tick check%u line%d: %s\n", checks, __LINE__, #value);      \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

typedef struct fixture {
    mc_java_random_profile *profile;
    mc_entity_classes *entities;
    mc_client_render_classes *models;
    mc_client_render_engine_classes *graphics;
    mc_client_held_item *owner;
    client_application *app;
    mc_entity *player;
    mc_inventory_player *inventory;
    mc_item_stack *stacks[5];
    mc_client_held_item_tick_call *call;
} fixture;

static uint32_t bits(const float *value) {
    uint32_t result;
    memcpy(&result, value, sizeof(result));
    return result;
}

static void install(float *value, uint32_t raw) {
    memcpy(value, &raw, sizeof(raw));
}

static bool set_b(fixture *f, mc_item_stack *stack) {
    if (stack != NULL)
        CHECK(mc_item_stack_retain(stack) == MC_ITEM_STACK_OK);
    mc_item_stack_release(f->owner->source->b_occurrence);
    f->owner->source->b = f->owner->source->b_occurrence = stack;
    return true;
}

static bool open_fixture(fixture *f) {
    memset(f, 0, sizeof(*f));
    CHECK(mc_java_random_profile_create(SIZE_MAX, &f->profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, f->profile, NULL, &f->entities) ==
          MC_ENTITY_OK);
    CHECK(mc_client_render_classes_create(f->entities, f->profile, &f->models) ==
          MC_CLIENT_MODEL_OK);
    CHECK(mc_client_render_engine_classes_create(NULL, &f->graphics) == MC_CLIENT_RENDER_ENGINE_OK);
    const mc_client_entity_renderer_services services = {f->models, f->graphics, f->profile, NULL};
    CHECK(mc_client_held_item_allocate(&services, NULL, &f->owner, NULL) ==
          MC_CLIENT_ENTITY_RENDERER_OK);
    f->app = calloc(1, sizeof(*f->app));
    CHECK(f->app != NULL);
    CHECK(mc_local_player_allocate(f->entities, &f->player, NULL) == MC_ENTITY_OK);
    CHECK(mc_inventory_player_create(MC_REGISTRY_CLIENT, NULL, NULL, &f->inventory, NULL) ==
          MC_INVENTORY_OK);
    /* Actual RAW fields authored for method-body scope. Constructors, World,
     * Font/GL/resource ownership and a full PX/App frame are not claimed. */
    f->owner->source->a = f->app;
    f->app->h = f->player;
    f->player->local_player->fields.player.c = f->inventory;
    for (size_t index = 1; index < 5; ++index) {
        CHECK(mc_item_stack_allocate(&f->stacks[index]) == MC_ITEM_STACK_OK);
        CHECK(mc_item_stack_construct(f->stacks[index], index == 4 ? 2 : 1, index == 2 ? 7 : 1,
                                      index == 3 ? 4 : 3) == MC_ITEM_STACK_OK);
    }
    return true;
}

static bool close_fixture(fixture *f) {
    CHECK(mc_client_held_item_tick_retire(&f->call, NULL) == MC_CLIENT_HELD_ITEM_TICK_OK);
    f->player->local_player->fields.player.c = NULL;
    CHECK(mc_inventory_player_retire(&f->inventory, NULL) == MC_INVENTORY_OK);
    mc_entity_release(f->player);
    CHECK(mc_client_held_item_retire(&f->owner, NULL) == MC_CLIENT_ENTITY_RENDERER_OK);
    for (size_t index = 1; index < 5; ++index)
        mc_item_stack_release(f->stacks[index]);
    CHECK(mc_client_render_engine_classes_release(f->graphics) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_render_classes_free(f->models) == MC_CLIENT_MODEL_OK);
    CHECK(mc_entity_classes_free(f->entities) == MC_ENTITY_OK);
    mc_java_random_profile_free(f->profile);
    free(f->app);
    return true;
}

static bool original_rows(fixture *f) {
    static const uint32_t inputs[] = {0x00000000, 0x80000000, 0x3dcccccd, 0x3f000000,
                                      0x3f19999a, 0x3f800000, 0xbf800000, 0x7f800000,
                                      0xff800000, 0x7fc12345, 0xffc54321, 0x00000001};
    /* selected Stack, previous Stack, actual selected field, previous RA.g. */
    static const int32_t cases[][4] = {{0, 0, 0, -1}, {1, 0, 0, -1}, {1, 1, 0, 0}, {2, 1, 0, 8},
                                       {4, 1, 0, 0},  {3, 1, 0, 0},  {1, 1, 1, 0}, {0, 1, -1, 0},
                                       {0, 0, 9, -1}, {2, 1, 0, 0}};
    for (size_t mode = 0; mode < sizeof(cases) / sizeof(cases[0]); ++mode) {
        for (size_t n = 0; n < sizeof(inputs) / sizeof(inputs[0]); ++n) {
            for (int32_t slot = 0; slot < 9; ++slot)
                CHECK(mc_inventory_player_set(f->inventory, slot, NULL, NULL) == MC_INVENTORY_OK);
            f->inventory->selected = cases[mode][2];
            if (f->inventory->selected >= 0 && f->inventory->selected < 9)
                CHECK(mc_inventory_player_set(f->inventory, f->inventory->selected,
                                              f->stacks[cases[mode][0]], NULL) == MC_INVENTORY_OK);
            CHECK(set_b(f, f->stacks[cases[mode][1]]));
            f->owner->source->g = cases[mode][3];
            install(&f->owner->source->c, inputs[n]);
            install(&f->owner->source->d, UINT32_C(0x7fc23456));
            mc_client_held_item_tick_state state;
            CHECK(mc_client_held_item_tick_begin(f->owner, &f->call, NULL) ==
                  MC_CLIENT_HELD_ITEM_TICK_OK);
            CHECK(mc_client_held_item_tick_inspect(f->call, &state) && state.source_returned &&
                  state.source_pc == 212 && state.d_stored && state.current_returned &&
                  state.c_stored && !state.active && bits(&f->owner->source->d) == inputs[n]);
            int id = -1;
            for (int candidate = 0; candidate < 5; ++candidate)
                if (f->owner->source->b == f->stacks[candidate])
                    id = candidate;
            CHECK(id >= 0 && f->owner->source->b_occurrence == f->owner->source->b);
            printf("EQUIP %zu %zu %08" PRIx32 " %08" PRIx32 " %d %d\n", mode, n,
                   bits(&f->owner->source->d), bits(&f->owner->source->c), f->owner->source->g, id);
            CHECK(mc_client_held_item_tick_retire(&f->call, NULL) == MC_CLIENT_HELD_ITEM_TICK_OK);
        }
    }
    return true;
}

static bool controls(fixture *f) {
    mc_client_source_ra *ra = f->owner->source;
    mc_client_held_item_tick_error error;
    mc_client_held_item_tick_state state;
    ra->a = NULL;
    install(&ra->c, UINT32_C(0x7fa12345));
    CHECK(mc_client_held_item_tick_begin(f->owner, &f->call, &error) ==
          MC_CLIENT_HELD_ITEM_TICK_SOURCE_NULL);
    CHECK(error.site.pc == 12 && !error.native_policy &&
          mc_client_held_item_tick_inspect(f->call, &state) && state.d_stored &&
          bits(&ra->d) == UINT32_C(0x7fa12345) && !state.current_returned);
    mc_client_held_item_tick_call *saved = f->call;
    const size_t holds = f->owner->holds;
    CHECK(mc_client_held_item_tick_begin(f->owner, &f->call, &error) ==
              MC_CLIENT_HELD_ITEM_TICK_ARGUMENT &&
          f->call == saved && f->owner->holds == holds);
    CHECK(mc_client_held_item_retire(&f->owner, NULL) == MC_CLIENT_ENTITY_RENDERER_BUSY);
    mc_client_held_item_tick_call *second = NULL;
    ra->a = f->app;
    f->app->h = NULL;
    CHECK(mc_client_held_item_tick_begin(f->owner, &second, &error) ==
              MC_CLIENT_HELD_ITEM_TICK_SOURCE_NULL &&
          error.site.pc == 17);
    CHECK(mc_client_held_item_tick_inspect(f->call, &state) && state.error->site.pc == 12 &&
          state.app == NULL && state.player == NULL);
    CHECK(mc_client_held_item_tick_retire(&second, NULL) == MC_CLIENT_HELD_ITEM_TICK_OK);
    f->app->h = f->player;
    CHECK(mc_client_held_item_tick_retire(&f->call, NULL) == MC_CLIENT_HELD_ITEM_TICK_OK);
    ra->a = f->app;
    f->app->h = NULL;
    CHECK(mc_client_held_item_tick_begin(f->owner, &f->call, &error) ==
              MC_CLIENT_HELD_ITEM_TICK_SOURCE_NULL &&
          error.site.pc == 17);
    CHECK(mc_client_held_item_tick_retire(&f->call, NULL) == MC_CLIENT_HELD_ITEM_TICK_OK);
    f->app->h = f->player;
    f->player->local_player->fields.player.c = NULL;
    CHECK(mc_client_held_item_tick_begin(f->owner, &f->call, &error) ==
              MC_CLIENT_HELD_ITEM_TICK_SOURCE_NULL &&
          error.site.pc == 20);
    CHECK(mc_client_held_item_tick_retire(&f->call, NULL) == MC_CLIENT_HELD_ITEM_TICK_OK);
    f->player->local_player->fields.player.c = f->inventory;
    f->inventory->selected = 0;
    CHECK(mc_inventory_player_set(f->inventory, 0, f->stacks[2], NULL) == MC_INVENTORY_OK);
    CHECK(set_b(f, f->stacks[1]));
    ra->c = 0.5f;
    const unsigned int floating = _mm_getcsr();
    _mm_setcsr(floating | 0x8000u);
    const mc_client_held_item_tick_status status =
        mc_client_held_item_tick_begin(f->owner, &f->call, &error);
    _mm_setcsr(floating);
    CHECK(status == MC_CLIENT_HELD_ITEM_TICK_FLOATING && error.native_policy &&
          error.site.pc == 146 && ra->b == f->stacks[2] && ra->c == 0.5f && ra->d == 0.5f);
    CHECK(mc_client_held_item_tick_inspect(f->call, &state) && state.equivalent_replaced &&
          !state.c_stored && !state.source_returned);
    CHECK(mc_inventory_player_set(f->inventory, 0, NULL, NULL) == MC_INVENTORY_OK);
    mc_item_stack_fields fields;
    CHECK(mc_item_stack_fields_get(ra->b, &fields) == MC_ITEM_STACK_OK && fields.count == 7);
    CHECK(mc_item_stack_fields_get(state.previous, &fields) == MC_ITEM_STACK_OK &&
          fields.count == 1);
    CHECK(mc_client_held_item_tick_retire(&f->call, NULL) == MC_CLIENT_HELD_ITEM_TICK_OK);
    const mc_item_stack *current = f->stacks[4];
    mc_inventory_current_error current_error;
    mc_item_stack **array = f->inventory->main_slots;
    f->inventory->main_slots = NULL;
    f->inventory->selected = 9;
    CHECK(mc_inventory_player_current(f->inventory, &current, &current_error) == MC_INVENTORY_OK &&
          current == NULL && !current_error.array_read && current_error.site.pc == 27);
    f->inventory->selected = 0;
    current = f->stacks[4];
    CHECK(mc_inventory_player_current(f->inventory, &current, &current_error) ==
              MC_INVENTORY_NULL_REFERENCE &&
          current == f->stacks[4] && current_error.site.pc == 24);
    f->inventory->main_slots = array;
    f->owner->active = true;
    CHECK(mc_client_held_item_tick_begin(f->owner, &f->call, &error) ==
              MC_CLIENT_HELD_ITEM_TICK_BUSY &&
          f->call == NULL);
    f->owner->active = false;
    mc_item_stack *published = NULL;
    CHECK(mc_item_stack_allocate(&published) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(published, 5, 3, 0) == MC_ITEM_STACK_OK);
    CHECK(mc_inventory_player_set(f->inventory, 0, published, NULL) == MC_INVENTORY_OK &&
          set_b(f, NULL));
    ra->c = 0.0f;
    ra->g = -1;
    CHECK(mc_client_held_item_tick_begin(f->owner, &f->call, &error) ==
              MC_CLIENT_HELD_ITEM_TICK_OK &&
          ra->b == published && ra->b_occurrence == published && ra->g == 0);
    CHECK(mc_inventory_player_set(f->inventory, 0, NULL, NULL) == MC_INVENTORY_OK);
    mc_item_stack_release(published); /* End the last ordinary caller occurrence. */
    CHECK(mc_client_held_item_tick_retire(&f->call, NULL) == MC_CLIENT_HELD_ITEM_TICK_OK);
    CHECK(mc_item_stack_fields_get(ra->b, &fields) == MC_ITEM_STACK_OK && fields.id == 5 &&
          fields.count == 3); /* Only RA.b keeps the exact Stack live now. */
    CHECK(set_b(f, NULL));
    return true;
}

int main(void) {
    fixture f;
    if (!open_fixture(&f) || !original_rows(&f) || !controls(&f) || !close_fixture(&f))
        return 1;
    printf("%u held-item tick checks passed\n", checks);
    return 0;
}
