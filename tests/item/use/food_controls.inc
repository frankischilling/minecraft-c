/* Authored operands on genuine owners. No statistic entry is substituted:
 * neither original in-air body nor its Stack wrapper has a statistic call. */
static bool food_controls(fixture *f, mc_entity *player) {
    static const struct {
        int32_t id, count, health, period, resistance, expected_count, expected_health,
            expected_resistance;
    } cases[] = {
        {260, 2, 1, 20, 71, 1, 5, 10},
        {297, 1, 19, 21, 71, 0, 20, 10},
        {319, 0, 20, -3, 71, -1, 20, -1},
        {320, -7, 0, 20, 71, -8, 0, 71},
        {322, INT32_MIN, -1, 20, 71, INT32_MAX, -1, 71},
        {349, 1, INT32_MAX, INT32_MIN, 71, 0, INT32_MIN + 1, INT32_MIN / 2},
        {350, 2, 21, 20, 71, 1, 20, 10},
        {357, 8, 17, 20, 71, 7, 18, 10},
        {282, 1, 5, 20, 71, 0, 15, 10},
        {282, 3, 20, 20, 71, 2, 20, 10},
        {282, 0, 0, 20, 71, -1, 0, 71},
    };

    const mc_living_entity_fields original = player->local_player->fields.living;
    const int32_t original_by = player->base.by;
    const size_t player_holds = player->holds;
    for (size_t i = 0; i < sizeof cases / sizeof *cases; ++i) {
        mc_item_stack *stack = NULL;
        CHECK(use_stack(cases[i].id, cases[i].count, INT32_MIN, &stack));
        CHECK(mc_item_stack_source_animation_set(stack, 9) == MC_ITEM_STACK_OK);
        player->local_player->fields.living.Y = cases[i].health;
        player->local_player->fields.living.E = cases[i].period;
        player->base.by = cases[i].resistance;
        mc_item_use_call *call = NULL;
        mc_item_use_state state;
        mc_item_use_error error;
        CHECK(mc_item_use_in_air_begin(f->registry, stack, NULL, player, &call, &error) ==
              MC_ITEM_USE_OK);
        CHECK(mc_item_use_inspect(call, &state) && state.returned && state.count_read &&
              state.count_stored && state.healing_entered && state.healing_returned &&
              state.count_before == cases[i].count &&
              state.count_after == cases[i].expected_count && state.source_pc == 10 &&
              !state.statistic_entered && !state.sound_returned && error.status == MC_ITEM_USE_OK &&
              !error.source_exception && !error.native_policy &&
              player->local_player->fields.living.Y == cases[i].expected_health &&
              player->base.by == cases[i].expected_resistance && player->holds == player_holds + 1);
        mc_item_stack_fields input, result;
        CHECK(mc_item_stack_fields_get(stack, &input) == MC_ITEM_STACK_OK &&
              input.id == cases[i].id && input.count == cases[i].expected_count &&
              input.damage == INT32_MIN && input.animation == 9);
        CHECK(mc_item_stack_fields_get(state.result_stack, &result) == MC_ITEM_STACK_OK);
        if (cases[i].id == 282) {
            CHECK(state.allocated_stack != NULL && state.result_stack == state.allocated_stack &&
                  state.result_stack != stack && state.bowl_captured && result.id == 281 &&
                  result.count == 1 && result.damage == 0 && result.animation == 0 &&
                  mc_item_stack_source(state.result_stack) == NULL);
        } else {
            CHECK(state.result_stack == stack && state.allocated_stack == NULL &&
                  !state.bowl_captured && result.count == input.count);
        }
        mc_item_stack *returned = state.result_stack;
        CHECK(mc_item_stack_retain(returned) == MC_ITEM_STACK_OK);
        mc_item_stack_release(stack);
        CHECK(use_retire(&call) && player->holds == player_holds &&
              mc_item_stack_fields_get(returned, &result) == MC_ITEM_STACK_OK);
        mc_item_stack_release(returned);
    }
    /* Player's invocation happens after the count store, including soup's
     * superclass. The failed body has not reached BZ's allocation. */
    static const int32_t null_player_ids[] = {260, 282, 357};
    for (size_t i = 0; i < sizeof null_player_ids / sizeof *null_player_ids; ++i) {
        mc_item_stack *stack = NULL;
        CHECK(use_stack(null_player_ids[i], 1, 0, &stack));
        mc_item_use_call *call = NULL;
        mc_item_use_state state;
        mc_item_use_error error;
        CHECK(mc_item_use_in_air_begin(f->registry, stack, NULL, NULL, &call, &error) ==
              MC_ITEM_USE_SOURCE_NULL);
        int32_t count;
        CHECK(mc_item_use_inspect(call, &state) && !state.returned && state.count_stored &&
              !state.healing_entered && state.allocated_stack == NULL &&
              state.result_stack == NULL && error.source_exception && !error.native_policy &&
              error.site.pc == 15 && strcmp(error.site.class_name, "yw") == 0 &&
              strcmp(error.exception_class, "java/lang/NullPointerException") == 0 &&
              mc_item_stack_source_count_get(stack, &count) == MC_ITEM_STACK_OK && count == 0);
        CHECK(use_retire(&call));
        mc_item_stack_release(stack);
    }
    /* No native READY gate is inserted before inherited healing. */
    mc_entity *raw = NULL;
    CHECK(mc_local_player_allocate(f->entities, &raw, NULL) == MC_ENTITY_OK);
    mc_item_stack *stack = NULL;
    CHECK(use_stack(260, 1, 0, &stack));
    mc_item_use_call *call = NULL;
    mc_item_use_state state;
    CHECK(mc_item_use_in_air_begin(f->registry, stack, NULL, raw, &call, NULL) == MC_ITEM_USE_OK &&
          mc_item_use_inspect(call, &state) && state.healing_returned &&
          raw->phase == MC_ENTITY_RAW && raw->local_player->fields.living.Y == 0);
    CHECK(use_retire(&call));
    mc_entity_release(raw);
    mc_item_stack_release(stack);

    /* A native retention refusal remains after consumption, before healing. */
    CHECK(use_stack(260, 2, 0, &stack));
    const size_t saved_holds = player->holds;
    player->holds = SIZE_MAX;
    mc_item_use_error error;
    const mc_item_use_status limit =
        mc_item_use_in_air_begin(f->registry, stack, NULL, player, &call, &error);
    player->holds = saved_holds;
    CHECK(limit == MC_ITEM_USE_LIMIT && mc_item_use_inspect(call, &state) && state.count_stored &&
          !state.healing_entered && error.native_policy && !error.source_exception &&
          error.site.pc == 15 && state.count_after == 1 && use_retire(&call));
    mc_item_stack_release(stack);

    /* Project the actual mutable static cell only as authored native input.
     * The new RAW Stack and completed heal survive a null constructor operand. */
    CHECK(use_stack(282, 2, 0, &stack));
    player->local_player->fields.living.Y = 5;
    player->local_player->fields.living.E = 20;
    player->base.by = 71;
    mc_item *bowl = f->registry->items.named[25];
    f->registry->items.named[25] = NULL;
    const mc_item_use_status null_bowl =
        mc_item_use_in_air_begin(f->registry, stack, NULL, player, &call, &error);
    f->registry->items.named[25] = bowl;
    CHECK(null_bowl == MC_ITEM_USE_SOURCE_NULL && mc_item_use_inspect(call, &state) &&
          !state.returned && state.count_after == 1 && state.healing_returned &&
          state.bowl_captured && state.allocated_stack != NULL && state.result_stack == NULL &&
          player->local_player->fields.living.Y == 15 && player->base.by == 10 &&
          error.source_exception && !error.native_policy && error.site.pc == 15 &&
          strcmp(error.site.class_name, "bz") == 0 && error.child.pc == 2 &&
          strcmp(error.child.method, "<init>") == 0);
    mc_item_stack_phase phase;
    CHECK(mc_item_stack_phase_get(state.allocated_stack, &phase) == MC_ITEM_STACK_OK &&
          phase == MC_ITEM_STACK_RAW && use_retire(&call));
    mc_item_stack_release(stack);
    player->local_player->fields.living = original;
    player->base.by = original_by;
    CHECK(player->holds == player_holds);
    return true;
}
