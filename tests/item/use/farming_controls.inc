/* Authored actual-owner controls for the pinned LA/RK branches. WorldAccess
 * callbacks below are declared fixture inputs, not original observations. */
#include "mc/item/combat/combat.h"
#ifdef MC_FARMING_TERMINAL_INVENTORY
#include "client/application_state.h"
#include "client/gui/source_screen_initialize.h"
#include "client/gui/source_workbench_screen_internal.h"
#include "entity/player/terminal_inventory.h"
#endif

typedef struct farming_observer {
    mc_item_stack *stack;
    mc_chunk_state *chunk;
    mc_entity *player;
    mc_item_use_call **call;
    int32_t ground, count, damage;
    size_t cells, ranges, sounds;
    size_t player_holds;
    bool valid, expect_sound, fail_sound, fail_cell;
    bool change_remote, busy_retire, busy_returned;
} farming_observer;

static size_t farming_index(int32_t y) {
    return ((size_t)8 * 16u + 8u) * 128u + (size_t)y;
}

static bool farming_stack(mc_item_stack *stack, int32_t id, int32_t count, int32_t damage) {
    mc_item_stack_fields fields;
    return mc_item_stack_fields_get(stack, &fields) == MC_ITEM_STACK_OK && fields.id == id &&
           fields.count == count && fields.damage == damage && fields.animation == 5;
}

static mc_world_storage_status farming_cell(void *context, mc_world_runtime *world, int32_t x,
                                            int32_t y, int32_t z, mc_world_storage_error *error) {
    (void)world;
    (void)x;
    (void)y;
    (void)z;
    farming_observer *o = context;
    ++o->cells;
    o->valid = o->valid && (!o->expect_sound || o->sounds == 1) &&
               (o->player == NULL || o->player->holds == o->player_holds);
    if (o->fail_cell) {
        if (error != NULL)
            *error = (mc_world_storage_error){MC_WORLD_STORAGE_LIMIT,
                                              "authored farming cell refusal", 0};
        return MC_WORLD_STORAGE_LIMIT;
    }
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status farming_range(void *context, mc_world_runtime *world, int32_t x,
                                             int32_t y, int32_t z, int32_t max_x, int32_t max_y,
                                             int32_t max_z, mc_world_storage_error *error) {
    (void)world;
    (void)x;
    (void)y;
    (void)z;
    (void)max_x;
    (void)max_y;
    (void)max_z;
    (void)error;
    ++((farming_observer *)context)->ranges;
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status farming_sound(void *context, mc_world_runtime *world,
                                             const mc_registry_string *name, double x, double y,
                                             double z, float volume, float pitch,
                                             mc_world_storage_error *error) {
    farming_observer *o = context;
    ++o->sounds;
    static const char expected[] = "step.gravel";
    o->valid = o->valid && o->expect_sound && o->cells == 0 && o->ranges == 0 && o->sounds == 1 &&
               name != NULL && name->length == sizeof(expected) - 1u && x == 8.5 && y == 64.5 &&
               z == 8.5 && volume == 1.0f && pitch == 0.8f &&
               o->chunk->data->blocks[farming_index(64)] == o->ground &&
               farming_stack(o->stack, 290, o->count, o->damage) &&
               (o->player == NULL || o->player->holds == o->player_holds);
    for (size_t i = 0; name != NULL && i < name->length && i < sizeof(expected) - 1u; ++i)
        o->valid = o->valid && name->units[i] == (uint16_t)expected[i];
    if (o->busy_retire) {
        mc_item_use_call *before = *o->call;
        mc_item_use_error busy;
        o->busy_returned = mc_item_use_retire(o->call, &busy) == MC_ITEM_USE_BUSY &&
                           *o->call == before && busy.cleanup.status == MC_WORLD_STORAGE_BUSY;
        o->valid = o->valid && o->busy_returned;
    }
    /* The actual LA source loads B only after this callback returns. */
    if (o->change_remote)
        world->fields.B = true;
    if (o->fail_sound) {
        if (error != NULL)
            *error =
                (mc_world_storage_error){MC_WORLD_STORAGE_LIMIT, "authored Hoe sound refusal", 0};
        return MC_WORLD_STORAGE_LIMIT;
    }
    return MC_WORLD_STORAGE_OK;
}

static bool farming_seeds(fixture *f, mc_chunk_state *chunk, mc_entity *player,
                          farming_observer *observer) {
    static const struct {
        int32_t ground, above, face, count, y, after;
        bool remote, result;
    } cases[] = {
        {60, 0, 0, 2, 64, 2, false, false},  {60, 0, INT32_MIN, -3, 64, -3, false, false},
        {2, 0, 1, 2, 64, 2, false, false},   {60, 1, 1, 2, 64, 2, false, false},
        {60, 0, 1, 2, 64, 1, false, true},   {60, 0, 1, 0, 64, -1, false, true},
        {60, 0, 1, -3, 64, -4, false, true}, {60, 0, 1, INT32_MIN, 64, INT32_MAX, false, true},
        {60, 0, 1, 2, 64, 1, true, true},    {60, 0, 1, 2, 127, 1, false, true}};

    for (size_t i = 0; i < sizeof(cases) / sizeof(*cases); ++i) {
        const size_t index = farming_index(cases[i].y);
        const uint8_t ground = chunk->data->blocks[index];
        const uint8_t above = cases[i].y < 127 ? chunk->data->blocks[index + 1u] : 0;
        chunk->data->blocks[index] = (uint8_t)cases[i].ground;
        if (cases[i].y < 127)
            chunk->data->blocks[index + 1u] = (uint8_t)cases[i].above;
        mc_item_stack *stack = NULL;
        CHECK(use_stack(295, cases[i].count, -7, &stack) &&
              mc_item_stack_source_animation_set(stack, 5) == MC_ITEM_STACK_OK);
        *observer = (farming_observer){.stack = stack,
                                       .chunk = chunk,
                                       .player = player,
                                       .player_holds = player->holds,
                                       .valid = true};
        mc_item_use_call *call = NULL;
        mc_item_use_error error;
        mc_item_use_state state;
        const size_t bindings = f->world->persistent_bindings;
        const size_t player_holds = player->holds;
        const bool remote = f->world->fields.B;
        mc_statistic *const statistic = f->registry->statistics.E[295];
        f->registry->statistics.E[295] = NULL;
        f->world->fields.B = cases[i].remote;
        const mc_item_use_status status = mc_item_use_on_block_begin(
            f->registry, stack, player, f->world, 8, cases[i].y, 8, cases[i].face, &call, &error);
        f->registry->statistics.E[295] = statistic;
        f->world->fields.B = remote;
        CHECK(status == MC_ITEM_USE_OK && mc_item_use_inspect(call, &state) && state.returned &&
              !state.active && state.result == cases[i].result && state.source_pc == 39 &&
              state.item_resolved && state.count_stored == cases[i].result &&
              state.count_read == cases[i].result && state.statistic_entered == cases[i].result &&
              state.statistic_returned == cases[i].result && observer->valid &&
              observer->sounds == 0 && farming_stack(stack, 295, cases[i].after, -7) &&
              player->holds == player_holds + (cases[i].result ? 1u : 0u));
        if (cases[i].result) {
            CHECK(state.setter_returned && state.setter_value == (cases[i].y < 127) &&
                  error.mutation != NULL &&
                  (cases[i].y == 127 || chunk->data->blocks[index + 1u] == 59) &&
                  chunk->data->blocks[index] == 60 &&
                  f->world->persistent_bindings == bindings + 1u);
        } else {
            CHECK(!state.setter_returned && !state.statistic_entered &&
                  chunk->data->blocks[index] == cases[i].ground &&
                  (cases[i].y == 127 || chunk->data->blocks[index + 1u] == cases[i].above) &&
                  f->world->persistent_bindings == bindings + (cases[i].face == 1 ? 1u : 0u));
        }
        CHECK(use_retire(&call) && f->world->persistent_bindings == bindings &&
              player->holds == player_holds);
        mc_item_stack_release(stack);
        chunk->data->blocks[index] = ground;
        if (cases[i].y < 127)
            chunk->data->blocks[index + 1u] = above;
    }
    /* RK's face refusal precedes every body argument read, including World. */
    mc_item_stack *stack = NULL;
    mc_item_use_call *call = NULL;
    mc_item_use_state state;
    CHECK(use_stack(295, 0, INT32_MIN, &stack));
    CHECK(mc_item_use_on_block_begin(f->registry, stack, NULL, NULL, INT32_MIN, INT32_MAX,
                                     INT32_MIN, 0, &call, NULL) == MC_ITEM_USE_OK &&
          mc_item_use_inspect(call, &state) && state.returned && !state.result &&
          !state.count_read && !state.statistic_entered && use_retire(&call));
    mc_item_stack_release(stack);
    return true;
}

static bool farming_hoes(fixture *f, mc_chunk_state *chunk, mc_entity *player,
                         farming_observer *observer) {
    static const struct {
        int32_t ground, above, face;
        bool result;
    } cases[] = {{2, 0, 0, false}, {2, 1, 1, false}, {1, 0, 1, false},
                 {2, 0, 1, true},  {3, 0, 0, true},  {3, 1, 0, true}};

    const size_t index = farming_index(64);
    for (size_t i = 0; i < sizeof(cases) / sizeof(*cases); ++i) {
        const uint8_t ground = chunk->data->blocks[index], above = chunk->data->blocks[index + 1u];
        chunk->data->blocks[index] = (uint8_t)cases[i].ground;
        chunk->data->blocks[index + 1u] = (uint8_t)cases[i].above;
        mc_item_stack *stack = NULL;
        CHECK(use_stack(290, 1, 0, &stack) &&
              mc_item_stack_source_animation_set(stack, 5) == MC_ITEM_STACK_OK);
        *observer = (farming_observer){.stack = stack,
                                       .chunk = chunk,
                                       .player = player,
                                       .player_holds = player->holds,
                                       .ground = cases[i].ground,
                                       .count = 1,
                                       .valid = true,
                                       .expect_sound = cases[i].result};
        mc_item_use_call *call = NULL;
        mc_item_use_error error;
        mc_item_use_state state;
        const size_t bindings = f->world->persistent_bindings;
        const size_t player_holds = player->holds;
        const bool remote = f->world->fields.B;
        mc_statistic *const statistic = f->registry->statistics.E[290];
        f->registry->statistics.E[290] = NULL;
        f->world->fields.B = true;
        const mc_item_use_status status = mc_item_use_on_block_begin(
            f->registry, stack, player, f->world, 8, 64, 8, cases[i].face, &call, &error);
        f->registry->statistics.E[290] = statistic;
        f->world->fields.B = remote;
        CHECK(status == MC_ITEM_USE_OK && mc_item_use_inspect(call, &state) && state.returned &&
              state.result == cases[i].result && state.source_pc == 39 && observer->valid &&
              observer->sounds == (cases[i].result ? 1u : 0u) && observer->cells == 0 &&
              !state.setter_returned && error.durability == NULL &&
              chunk->data->blocks[index] == cases[i].ground &&
              chunk->data->blocks[index + 1u] == cases[i].above &&
              farming_stack(stack, 290, 1, 0) &&
              player->holds == player_holds + (cases[i].result ? 1u : 0u) &&
              f->world->persistent_bindings == bindings + 1u && use_retire(&call) &&
              f->world->persistent_bindings == bindings && player->holds == player_holds);
        mc_item_stack_release(stack);
        chunk->data->blocks[index] = ground;
        chunk->data->blocks[index + 1u] = above;
    }
    /* A sound callback changes the actual late B field and attempts active
     * retirement. Neither action substitutes a game result or a source hook. */
    const uint8_t ground = chunk->data->blocks[index], above = chunk->data->blocks[index + 1u];
    chunk->data->blocks[index] = 3;
    chunk->data->blocks[index + 1u] = 1;
    mc_item_stack *stack = NULL;
    CHECK(use_stack(290, 1, 7, &stack) &&
          mc_item_stack_source_animation_set(stack, 5) == MC_ITEM_STACK_OK);
    mc_item_use_call *call = NULL;
    *observer = (farming_observer){.stack = stack,
                                   .chunk = chunk,
                                   .player = player,
                                   .player_holds = player->holds,
                                   .call = &call,
                                   .ground = 3,
                                   .count = 1,
                                   .damage = 7,
                                   .valid = true,
                                   .expect_sound = true,
                                   .change_remote = true,
                                   .busy_retire = true};
    const bool remote = f->world->fields.B;
    const size_t player_holds = player->holds;
    mc_statistic *const statistic = f->registry->statistics.E[290];
    f->registry->statistics.E[290] = NULL;
    f->world->fields.B = false;
    mc_item_use_error error;
    mc_item_use_state state;
    const mc_item_use_status status = mc_item_use_on_block_begin(
        f->registry, stack, player, f->world, 8, 64, 8, 0, &call, &error);
    f->registry->statistics.E[290] = statistic;
    f->world->fields.B = remote;
    CHECK(status == MC_ITEM_USE_OK && mc_item_use_inspect(call, &state) && state.returned &&
          state.result && state.sound_returned && !state.setter_returned && observer->valid &&
          observer->busy_returned && observer->cells == 0 && error.durability == NULL &&
          farming_stack(stack, 290, 1, 7) && chunk->data->blocks[index] == 3 &&
          player->holds == player_holds + 1u && use_retire(&call) && player->holds == player_holds);
    mc_item_stack_release(stack);
    chunk->data->blocks[index] = ground;
    chunk->data->blocks[index + 1u] = above;
    return true;
}

static bool farming_local_durability(fixture *f, mc_chunk_state *chunk, mc_entity *player,
                                     farming_observer *observer) {
    static const struct {
        int32_t count, damage, after_count, after_damage;
        bool broken, clamped;
    } cases[] = {{1, 0, 1, 1, false, false},
                 {2, 59, 1, 0, true, false},
                 {0, 59, 0, 0, true, true},
                 {-3, INT32_MAX, -3, INT32_MIN, false, false}};

    const size_t index = farming_index(64);
    for (size_t i = 0; i < sizeof(cases) / sizeof(*cases); ++i) {
        const uint8_t ground = chunk->data->blocks[index], above = chunk->data->blocks[index + 1u];
        chunk->data->blocks[index] = 3;
        chunk->data->blocks[index + 1u] = 0;
        mc_item_stack *stack = NULL;
        CHECK(use_stack(290, cases[i].count, cases[i].damage, &stack) &&
              mc_item_stack_source_animation_set(stack, 5) == MC_ITEM_STACK_OK);
        *observer = (farming_observer){.stack = stack,
                                       .chunk = chunk,
                                       .player = player,
                                       .player_holds = player->holds,
                                       .ground = 3,
                                       .count = cases[i].count,
                                       .damage = cases[i].damage,
                                       .valid = true,
                                       .expect_sound = true};
        mc_item_use_call *call = NULL;
        mc_item_use_error error;
        mc_item_use_state state;
        const size_t bindings = f->world->persistent_bindings;
        const size_t player_holds = player->holds;
        const bool remote = f->world->fields.B;
        mc_statistic *const used = f->registry->statistics.E[290];
        mc_statistic *const broken = f->registry->statistics.F[290];
        f->registry->statistics.E[290] = f->registry->statistics.F[290] = NULL;
        f->world->fields.B = false;
        const mc_item_use_status status = mc_item_use_on_block_begin(
            f->registry, stack, player, f->world, 8, 64, 8, 0, &call, &error);
        f->registry->statistics.E[290] = used;
        f->registry->statistics.F[290] = broken;
        f->world->fields.B = remote;
        CHECK(status == MC_ITEM_USE_OK && mc_item_use_inspect(call, &state) && state.returned &&
              state.result && state.sound_returned && state.setter_returned && state.setter_value &&
              state.statistic_returned && observer->valid && observer->sounds == 1 &&
              observer->cells != 0 && chunk->data->blocks[index] == 60 &&
              error.durability != NULL && error.durability->durability_entered &&
              error.durability->damage_stored &&
              error.durability->break_selected == cases[i].broken &&
              error.durability->count_clamped == cases[i].clamped &&
              error.durability->damage_reset == cases[i].broken &&
              farming_stack(stack, 290, cases[i].after_count, cases[i].after_damage) &&
              f->world->persistent_bindings == bindings + 1u && player->holds == player_holds + 1u);
        /* The call owns the exact Stack after the external hold is dropped. */
        mc_item_stack_release(stack);
        CHECK(farming_stack(state.stack, 290, cases[i].after_count, cases[i].after_damage) &&
              use_retire(&call) && f->world->persistent_bindings == bindings &&
              player->holds == player_holds);
        chunk->data->blocks[index] = ground;
        chunk->data->blocks[index + 1u] = above;
    }
    return true;
}

static bool farming_failures(fixture *f, mc_chunk_state *chunk, mc_entity *player,
                             farming_observer *observer) {
    const size_t index = farming_index(64);
    const uint8_t ground = chunk->data->blocks[index], above = chunk->data->blocks[index + 1u];
    for (size_t mode = 0; mode < 6; ++mode) {
        const bool seeds = mode == 0 || mode == 3;
        const int32_t id = seeds ? 295 : 290;
        const int32_t damage = mode == 5 ? 59 : 0;
        chunk->data->blocks[index] = seeds ? 60 : 3;
        chunk->data->blocks[index + 1u] = 0;
        mc_item_stack *stack = NULL;
        CHECK(use_stack(id, 2, damage, &stack) &&
              mc_item_stack_source_animation_set(stack, 5) == MC_ITEM_STACK_OK);
        *observer = (farming_observer){.stack = stack,
                                       .chunk = chunk,
                                       .player = player,
                                       .player_holds = player->holds,
                                       .ground = seeds ? 60 : 3,
                                       .count = 2,
                                       .damage = damage,
                                       .valid = true,
                                       .expect_sound = !seeds,
                                       .fail_sound = mode == 1,
                                       .fail_cell = mode == 0 || mode == 2};
        mc_item_use_call *call = NULL;
        mc_item_use_error error;
        mc_item_use_state state;
        const size_t bindings = f->world->persistent_bindings;
        const size_t player_holds = player->holds;
        const bool remote = f->world->fields.B;
        CHECK(mode < 3 || (mode < 5 ? f->registry->statistics.E[id] != NULL
                                    : f->registry->statistics.F[290] != NULL));
        f->world->fields.B = false;
        const mc_item_use_status status = mc_item_use_on_block_begin(
            f->registry, stack, player, f->world, 8, 64, 8, seeds ? 1 : 0, &call, &error);
        f->world->fields.B = remote;
        CHECK(mc_item_use_inspect(call, &state) && !state.returned && !state.active &&
              observer->valid && f->world->persistent_bindings == bindings + 1u &&
              player->holds == player_holds + (mode >= 3 ? 1u : 0u));
        if (mode < 3) {
            CHECK(status == MC_ITEM_USE_WORLD && error.native_policy && !error.source_exception &&
                  !state.count_stored && !state.statistic_entered && error.durability == NULL &&
                  farming_stack(stack, id, 2, 0));
            if (mode == 1) {
                CHECK(error.site.pc == 116 && strcmp(error.site.class_name, "la") == 0 &&
                      error.sound != NULL && error.sound->status == MC_WORLD_STORAGE_LIMIT &&
                      !state.sound_returned && !state.setter_returned && observer->cells == 0 &&
                      chunk->data->blocks[index] == 3);
            } else {
                CHECK(error.site.pc == (seeds ? 59u : 140u) &&
                      strcmp(error.site.class_name, seeds ? "rk" : "la") == 0 &&
                      error.mutation != NULL && error.mutation->status == MC_WORLD_STORAGE_LIMIT &&
                      observer->cells != 0 && chunk->data->blocks[index] == 60 &&
                      (!seeds || chunk->data->blocks[index + 1u] == 59));
            }
        } else if (mode < 5) {
            CHECK(status == MC_ITEM_USE_PLAYER && state.result && state.count_stored == seeds &&
                  state.statistic_entered && !state.statistic_returned && error.site.pc == 34 &&
                  strcmp(error.site.class_name, "iz") == 0 && error.child.pc == 85 &&
                  error.source_exception && !error.native_policy &&
                  strcmp(error.exception_class, "java/lang/NullPointerException") == 0 &&
                  farming_stack(stack, id, seeds ? 1 : 2, seeds ? 0 : 1) &&
                  chunk->data->blocks[index] == 60 &&
                  (!seeds || chunk->data->blocks[index + 1u] == 59));
        } else {
            CHECK(status == MC_ITEM_USE_PLAYER && error.site.pc == 147 &&
                  strcmp(error.site.class_name, "la") == 0 && error.durability != NULL &&
                  error.durability->damage_stored && error.durability->break_selected &&
                  error.durability->statistic_entered && !error.durability->statistic_returned &&
                  !error.durability->count_stored && !error.durability->damage_reset &&
                  !state.statistic_entered && error.source_exception && !error.native_policy &&
                  farming_stack(stack, 290, 2, 60) && chunk->data->blocks[index] == 60);
        }
        CHECK(use_retire(&call) && f->world->persistent_bindings == bindings &&
              player->holds == player_holds);
        mc_item_stack_release(stack);
    }
    for (size_t mode = 0; mode < 2; ++mode) {
        mc_item_stack *stack = NULL;
        const int32_t id = mode == 0 ? 295 : 290;
        CHECK(use_stack(id, 2, 0, &stack) &&
              mc_item_stack_source_animation_set(stack, 5) == MC_ITEM_STACK_OK);
        mc_item_use_call *call = NULL;
        mc_item_use_error error;
        mc_item_use_state state;
        const size_t holds = player->holds;
        CHECK(mc_item_use_on_block_begin(f->registry, stack, player, NULL, 8, 64, 8, 1, &call,
                                         &error) == MC_ITEM_USE_SOURCE_NULL &&
              mc_item_use_inspect(call, &state) && !state.returned && !state.active &&
              error.site.pc == (mode == 0 ? 15u : 7u) &&
              strcmp(error.site.class_name, mode == 0 ? "rk" : "la") == 0 &&
              error.source_exception && !error.native_policy && !state.count_stored &&
              !state.statistic_entered && farming_stack(stack, id, 2, 0) &&
              player->holds == holds && use_retire(&call) && player->holds == holds);
        mc_item_stack_release(stack);
    }
    /* First out-of-height read returns air; LA still reaches its second read
     * at21 before its eventual false condition. No future-array preflight. */
    for (size_t mode = 0; mode < 2; ++mode) {
        mc_item_stack *stack = NULL;
        CHECK(use_stack(mode == 0 ? 295 : 290, 2, 0, &stack) &&
              mc_item_stack_source_animation_set(stack, 5) == MC_ITEM_STACK_OK);
        *observer = (farming_observer){.stack = stack, .chunk = chunk, .valid = true};
        mc_item_use_call *call = NULL;
        mc_item_use_error error;
        uint8_t *const array = chunk->data->blocks;
        chunk->data->blocks = NULL;
        const mc_item_use_status status = mc_item_use_on_block_begin(
            f->registry, stack, player, f->world, 8, mode == 0 ? 64 : -1, 8, 1, &call, &error);
        chunk->data->blocks = array;
        CHECK(status == MC_ITEM_USE_WORLD && error.site.pc == (mode == 0 ? 15u : 21u) &&
              strcmp(error.site.class_name, mode == 0 ? "rk" : "la") == 0 &&
              error.acquisition != NULL && error.native_policy && !error.source_exception &&
              observer->sounds == 0 && observer->cells == 0 &&
              farming_stack(stack, mode == 0 ? 295 : 290, 2, 0) && use_retire(&call));
        mc_item_stack_release(stack);
    }
    chunk->data->blocks[index] = ground;
    chunk->data->blocks[index + 1u] = above;
    return true;
}

#ifdef MC_FARMING_TERMINAL_INVENTORY
/* An isolated genuine Player follows OO construction, ID.b publication and the
 * local terminal Inventory adoption protocol. This is a native owner refusal
 * control, not a claim of whole-App terminal authority or a Java exception. */
static bool farming_terminal_player(fixture *f, mc_chunk_state *chunk, farming_observer *observer) {
    const size_t live = f->entities->live_entities;
    const size_t bindings = f->world->persistent_bindings;
    client_application *app = calloc(1, sizeof(*app));
    mc_player_session *session = NULL;
    mc_entity *player = NULL;
    mc_client_render_classes *models = NULL;
    mc_client_screen_classes *screens = NULL;
    mc_client_source_oo *screen = NULL;
    CHECK(app != NULL &&
          mc_player_session_create(f->sessions, NULL, NULL, &session, NULL) == MC_PLAYER_OK &&
          mc_local_player_allocate(f->entities, &player, NULL) == MC_ENTITY_OK &&
          mc_local_player_construct(player, f->world, app, session, 0, NULL, NULL) == MC_PLAYER_OK);
    mc_player_session_retire(&session);
    /* Declared operands for the real ID.b call; ID.b performs Player.e=j. */
    app->h = player;
    app->active_world = f->world;
    CHECK(mc_client_render_classes_create(f->entities, f->profile, &models) == MC_CLIENT_MODEL_OK &&
          mc_client_screen_classes_create(f->profile, mc_resource_files_source(f->files),
                                          f->registry, &screens, NULL) == MC_CLIENT_SCREEN_OK &&
          mc_client_screen_workbench_allocate(screens, models, NULL, &screen, NULL) ==
              MC_CLIENT_SCREEN_OK &&
          mc_client_screen_workbench_construct(screen, player->local_player->fields.player.c,
                                               f->world, 8, 64, 8, NULL) == MC_CLIENT_SCREEN_OK);
    mc_client_screen_init_error initialized;
    mc_client_workbench_screen_state view;
    CHECK(mc_client_screen_initialize(mc_client_screen_workbench_base(screen), app, 427, 240,
                                      &initialized) == MC_CLIENT_SCREEN_OK &&
          mc_client_screen_workbench_inspect(screen, &view, NULL) == MC_CLIENT_SCREEN_OK &&
          view.phase == MC_CLIENT_SCREEN_READY && view.container_owned &&
          view.id.j == view.constructor_container &&
          view.id.j == player->local_player->fields.player.e &&
          view.id.j != player->local_player->fields.player.d);
    mc_player_terminal_inventory_error adopted;
    CHECK(mc_player_terminal_inventory_adopt(player, f->registry, f->world,
                                             &screen->container.captured_container,
                                             &adopted) == MC_ENTITY_OK &&
          adopted.status == MC_ENTITY_OK && adopted.inventory.frame_count == 0 &&
          screen->container.captured_container == NULL && mc_local_player_terminal_sealed(player));
    /* Adoption moved the actual owning slot. End only its native owning flag,
     * as the terminal GUI adapter does; no Source close or alias store occurs. */
    screen->container.container_owned = false;
    CHECK(mc_client_screen_workbench_retire(&screen, NULL) == MC_CLIENT_SCREEN_OK &&
          screen == NULL &&
          mc_client_screen_classes_release(&screens, NULL) == MC_CLIENT_SCREEN_OK &&
          screens == NULL && mc_client_render_classes_free(models) == MC_CLIENT_MODEL_OK &&
          mc_player_terminal_inventory_admit(player, f->registry, f->world, NULL) == MC_ENTITY_OK &&
          mc_entity_retain(player) == MC_ENTITY_BUSY);
    const size_t holds = player->holds;
    const size_t call_bindings = f->world->persistent_bindings;
    const size_t index = farming_index(64);
    const uint8_t ground = chunk->data->blocks[index], above = chunk->data->blocks[index + 1u];
    const uint8_t metadata = chunk->data->metadata[index / 2u];
    const bool remote = f->world->fields.B;
    mc_item_stack *stack = NULL;
    CHECK(use_stack(290, 2, 7, &stack) &&
          mc_item_stack_source_animation_set(stack, 5) == MC_ITEM_STACK_OK);
    *observer = (farming_observer){.stack = stack,
                                   .chunk = chunk,
                                   .player = player,
                                   .player_holds = holds,
                                   .ground = 3,
                                   .count = 2,
                                   .damage = 7,
                                   .valid = true,
                                   .expect_sound = true};
    chunk->data->blocks[index] = 3;
    chunk->data->blocks[index + 1u] = 0;
    f->world->fields.B = false;
    mc_item_use_call *call = NULL;
    mc_item_use_error error;
    mc_item_use_state state;
    const mc_item_use_status status = mc_item_use_on_block_begin(
        f->registry, stack, player, f->world, 8, 64, 8, 0, &call, &error);
    f->world->fields.B = remote;
    const bool prefix =
        mc_item_use_inspect(call, &state) && !state.active && !state.returned &&
        state.source_pc == 147 && state.sound_returned && state.setter_returned &&
        state.setter_value && !state.count_read && !state.count_stored &&
        !state.statistic_entered && !state.statistic_returned && error.site.pc == 147 &&
        strcmp(error.site.class_name, "la") == 0 && error.native_policy &&
        !error.source_exception && !error.source_error && error.exception_class == NULL &&
        error.durability == NULL && error.statistic == NULL && error.sound != NULL &&
        error.sound->status == MC_WORLD_STORAGE_OK && error.mutation != NULL &&
        error.mutation->status == MC_WORLD_STORAGE_OK && observer->valid && observer->sounds == 1 &&
        observer->cells != 0 && chunk->data->blocks[index] == 60 &&
        chunk->data->blocks[index + 1u] == 0 && farming_stack(stack, 290, 2, 7) &&
        player->holds == holds && f->world->persistent_bindings == call_bindings + 1u &&
        mc_player_terminal_inventory_admit(player, f->registry, f->world, NULL) == MC_ENTITY_OK;
    CHECK(use_retire(&call) && player->holds == holds &&
          f->world->persistent_bindings == call_bindings);
    mc_item_stack_release(stack);
    chunk->data->blocks[index] = ground;
    chunk->data->blocks[index + 1u] = above;
    chunk->data->metadata[index / 2u] = metadata;
    /* The final real Player release consumes the sealed IT -> AA -> IX graph.
     * Clean up before checking the expected status, including in the RED run. */
    mc_entity_release(player);
    app->h = NULL;
    free(app);
    CHECK(f->entities->live_entities == live && f->world->persistent_bindings == bindings);
    CHECK(prefix);
    if (status != MC_ITEM_USE_BUSY || error.status != MC_ITEM_USE_BUSY)
        fprintf(stderr, "Sealed Player retain refusal: returned=%d error=%d expected BUSY=%d\n",
                (int)status, (int)error.status, (int)MC_ITEM_USE_BUSY);
    CHECK(status == MC_ITEM_USE_BUSY && error.status == MC_ITEM_USE_BUSY);
    return true;
}
#endif

static bool farming_controls(fixture *f, mc_chunk_state *chunk, mc_entity *player) {
    farming_observer observer = {.valid = true};
    const mc_world_access_callbacks callbacks = {
        .cell = farming_cell, .range = farming_range, .sound = farming_sound};
    mc_world_access *access = NULL;
    CHECK(mc_world_access_create(&callbacks, &observer, &access, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(f->world, access, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(farming_seeds(f, chunk, player, &observer) && farming_hoes(f, chunk, player, &observer) &&
          farming_local_durability(f, chunk, player, &observer) &&
          farming_failures(f, chunk, player, &observer));
#ifdef MC_FARMING_TERMINAL_INVENTORY
    CHECK(farming_terminal_player(f, chunk, &observer));
#endif
    bool removed = false;
    CHECK(mc_world_runtime_remove_world_access(f->world, access, &removed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          removed && mc_world_access_free(access, NULL) == MC_WORLD_STORAGE_OK);
    return true;
}
