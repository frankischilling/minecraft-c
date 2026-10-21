/* Included by test_movement.c after the real fixture and input helper.
 * Every target is a genuinely constructed canonical Item/Falling owner.
 * Temporary watcher/hold/relationship/FP faults are restored before CHECK. */
static bool movement_effects(mc_entity *entity, mc_entity *other, bool item) {
    bool ok = false;
    mc_entity_effect_error error;
    CHECK(movement_input(entity, 8.5, 8.0, 8.5));
    CHECK(entity->base.world == NULL && entity->watcher != NULL && entity->watcher->map != NULL &&
          entity->watcher->map->head != NULL && entity->watcher->map->head->record != NULL);
    mc_entity_watch_record *const record = entity->watcher->map->head->record;
    const int8_t saved_byte = record->byte_value;
    const int32_t bits[] = {-1, 0, 1, 7, 8, 31, 32, 39};
    const bool expected[] = {true, false, false, true, true, true, false, true};
    for (size_t i = 0; i < COUNT(bits); ++i) {
        bool actual = !expected[i];
        record->byte_value = INT8_MIN;
        const mc_entity_status status = mc_entity_effect_flag(entity, bits[i], &actual, &error);
        record->byte_value = saved_byte;
        CHECK(status == MC_ENTITY_OK && actual == expected[i] && error.status == MC_ENTITY_OK &&
              error.source_bytecode == 0 && entity->holds == 1);
    }
    int8_t byte = 37;
    CHECK(mc_entity_effect_watcher_byte(entity, 91, &byte, &error) == MC_ENTITY_STATE_ERROR &&
          byte == 37 && error.watcher_bytecode == 16 && error.watcher_index == 91);
    mc_entity_watch_node *const node = entity->watcher->map->head;
    node->record = NULL;
    const mc_entity_status missing = mc_entity_effect_watcher_byte(entity, 0, &byte, &error);
    node->record = record;
    CHECK(missing == MC_ENTITY_STATE_ERROR && byte == 37 && error.watcher_bytecode == 16 &&
          error.cause == MC_ENTITY_EFFECT_CAUSE_NULL_REFERENCE);
    const int32_t saved_type = record->type;
    record->type = 1;
    const mc_entity_status wrong_type = mc_entity_effect_watcher_byte(entity, 0, &byte, &error);
    record->type = saved_type;
    CHECK(wrong_type == MC_ENTITY_STATE_ERROR && byte == 37 && error.watcher_bytecode == 19 &&
          error.cause == MC_ENTITY_EFFECT_CAUSE_UNSUPPORTED_WATCHER_VALUE);
    mc_entity_watch_map *const map = entity->watcher->map;
    entity->watcher->map = NULL;
    const mc_entity_status no_map = mc_entity_effect_watcher_byte(entity, 0, &byte, &error);
    entity->watcher->map = map;
    CHECK(no_map == MC_ENTITY_STATE_ERROR && byte == 37 && error.watcher_bytecode == 8);
    mc_entity_watcher *const watcher = entity->watcher;
    entity->watcher = NULL;
    bool flag = true;
    const mc_entity_status no_watcher = mc_entity_effect_sneaking(entity, &flag, &error);
    entity->watcher = watcher;
    CHECK(no_watcher == MC_ENTITY_STATE_ERROR && flag && error.source_bytecode == 2 &&
          error.flag_bytecode == 5 && entity->holds == 1);

    const size_t holds = entity->holds;
    entity->holds = SIZE_MAX;
    const mc_entity_status hold_failed = mc_entity_effect_watcher_byte(entity, 0, &byte, &error);
    entity->holds = holds;
    CHECK(hold_failed == MC_ENTITY_LIMIT && byte == 37 && error.entity.status == MC_ENTITY_LIMIT);

    entity->base.bk = 1.0f;
    CHECK(mc_entity_effect_fall_state(entity, -0.25, false, &error) == MC_ENTITY_OK &&
          entity->base.bk == 1.25f);
    CHECK(mc_entity_effect_fall_state(entity, 0.5, false, &error) == MC_ENTITY_OK &&
          entity->base.bk == 1.25f);
    entity->base.aG = other; /* Explicit unsupported relationship projection. */
    const mc_entity_status passenger = mc_entity_effect_fall_state(entity, 0, true, &error);
    entity->base.aG = NULL;
    CHECK(passenger == MC_ENTITY_STATE_ERROR && entity->base.bk == 1.25f &&
          error.source_bytecode == 18 && error.fall_callback_bytecode == 12 &&
          error.cause == MC_ENTITY_EFFECT_CAUSE_UNSUPPORTED_RELATIONSHIP);
    CHECK(mc_entity_effect_fall(entity, 3.5f, &error) == MC_ENTITY_OK && entity->base.bk == 1.25f &&
          !entity->base.be && entity->base.world == NULL);
    CHECK(mc_entity_effect_fall_state(entity, 0, true, &error) == MC_ENTITY_OK &&
          entity->base.bk == 0.0f);

    bool result = true;
    const size_t other_holds = other->holds;
    other->holds = SIZE_MAX; /* Damage's actual source argument is unused. */
    const mc_entity_status damaged = mc_entity_effect_damage(entity, other, 1, &result, &error);
    other->holds = other_holds;
    CHECK(damaged == MC_ENTITY_OK && !result && entity->base.bb && !entity->base.be &&
          entity->item.f == (item ? 4 : 0) && entity->base.world == NULL);
    entity->base.bb = entity->base.be = false;
    if (item)
        entity->item.f = 5;
    CHECK(mc_entity_effect_damage(entity, NULL, INT32_MIN, &result, &error) == MC_ENTITY_OK &&
          !result && entity->base.bb && entity->base.be == item &&
          entity->item.f == (item ? INT32_MIN + 5 : 0));
    entity->base.bb = entity->base.be = false;
    entity->base.bC = true;
    if (item)
        entity->item.f = 5;
    CHECK(mc_entity_effect_fire_damage(entity, 1, &error) == MC_ENTITY_OK &&
          entity->base.bb == item && entity->item.f == (item ? 4 : 0));
    entity->base.bb = false;
    CHECK(mc_entity_effect_attacked(entity, &error) == MC_ENTITY_OK && entity->base.bb &&
          entity->base.world == NULL);
    CHECK(mc_entity_effect_dead(entity, &error) == MC_ENTITY_OK && entity->base.be);
    bool walks = true;
    CHECK(mc_entity_effect_walks(entity, &walks, &error) == MC_ENTITY_OK && !walks &&
          mc_entity_effect_contact_portal(entity, &error) == MC_ENTITY_OK && entity->holds == 1);

    /* bx short-circuits before World and FP; no replacement rain answer. */
    entity->base.bx = true;
    const unsigned int mxcsr = _mm_getcsr();
    _mm_setcsr(mxcsr | 0x40u);
    bool wet = false;
    const mc_entity_status wet_short = mc_entity_effect_wet(entity, &wet, &error);
    _mm_setcsr(mxcsr);
    CHECK(wet_short == MC_ENTITY_OK && wet && error.status == MC_ENTITY_OK);
    entity->base.bx = false;
    CHECK(mc_entity_effect_wet(entity, &wet, &error) == MC_ENTITY_WORLD_FAILED && wet &&
          error.source_bytecode == 32 && error.cause == MC_ENTITY_EFFECT_CAUSE_NULL_REFERENCE &&
          entity->holds == 1 && entity->watcher == watcher && record->byte_value == saved_byte);
    ok = true;
cleanup:
    return ok;
}
