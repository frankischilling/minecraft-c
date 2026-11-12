/* Included by test_movement.c after the real fixture and input helper.
 * Every target is a genuinely constructed canonical Item/Falling owner.
 * Temporary watcher/hold/relationship/FP faults are restored before CHECK. */
#include "../../../src/entity/movement/effects_internal.h"

/* These are fresh authored native reads on the real constructed watcher.
 * The independent expectations describe the former getter's exact writes;
 * materialization below never invokes the source getter a second time. */
static bool movement_watcher_leaf(mc_entity *entity, mc_entity_effect_error *error) {
    bool ok = false;
    mc_entity_watcher *const watcher = entity->watcher;
    mc_entity_watch_map *const map = watcher->map;
    mc_entity_watch_node *const node = map->head;
    mc_entity_watch_record *const record = node->record;
    const mc_entity_watch_record saved = *record;
    const size_t map_size = map->size, holds = entity->holds;
    const mc_java_random random = *entity->random;
    static const char sentinel[] = "watcher leaf unrelated diagnostic sentinel";

    static const struct {
        bool returned;
        uint16_t pc;
        mc_entity_watcher_byte_stage stage;
        mc_entity_effect_cause cause;
        const char *operation;
    } cases[] = {{true, 0, MC_ENTITY_WATCHER_BYTE_STAGE_NONE, MC_ENTITY_EFFECT_CAUSE_NONE, NULL},
                 {true, 0, MC_ENTITY_WATCHER_BYTE_STAGE_NONE, MC_ENTITY_EFFECT_CAUSE_NONE, NULL},
                 {false, 5, MC_ENTITY_WATCHER_BYTE_STAGE_FLAG,
                  MC_ENTITY_EFFECT_CAUSE_NULL_REFERENCE, "Entity flag null watcher"},
                 {false, 8, MC_ENTITY_WATCHER_BYTE_STAGE_WATCHER,
                  MC_ENTITY_EFFECT_CAUSE_NULL_REFERENCE, "Watcher null Map receiver"},
                 {false, 16, MC_ENTITY_WATCHER_BYTE_STAGE_WATCHER,
                  MC_ENTITY_EFFECT_CAUSE_NULL_REFERENCE, "Watcher null record value receiver"},
                 {false, 22, MC_ENTITY_WATCHER_BYTE_STAGE_WATCHER,
                  MC_ENTITY_EFFECT_CAUSE_NULL_REFERENCE, "Watcher null boxed Byte unbox receiver"},
                 {false, 19, MC_ENTITY_WATCHER_BYTE_STAGE_WATCHER,
                  MC_ENTITY_EFFECT_CAUSE_CLASS_CAST, "Watcher actual boxed Byte cast"},
                 {false, 19, MC_ENTITY_WATCHER_BYTE_STAGE_WATCHER,
                  MC_ENTITY_EFFECT_CAUSE_UNSUPPORTED_WATCHER_VALUE,
                  "Watcher unsupported typed Byte value projection"},
                 {false, 22, MC_ENTITY_WATCHER_BYTE_STAGE_WATCHER,
                  MC_ENTITY_EFFECT_CAUSE_NULL_REFERENCE, "Watcher null boxed Byte unbox receiver"},
                 {false, 16, MC_ENTITY_WATCHER_BYTE_STAGE_WATCHER,
                  MC_ENTITY_EFFECT_CAUSE_NULL_REFERENCE, "Watcher null record value receiver"},
                 {false, 8, MC_ENTITY_WATCHER_BYTE_STAGE_WATCHER, MC_ENTITY_EFFECT_CAUSE_OWNER,
                  "Watcher Map node ownership invariant"}};

    CHECK(saved.payload_kind == MC_ENTITY_WATCH_PAYLOAD_BYTE && map_size != 0);
    for (size_t i = 0; i < COUNT(cases); ++i) {
        mc_entity_effect_error_init(error); /* Reuse the existing fixture output. */
        error->status = MC_ENTITY_LIMIT;
        error->cause = MC_ENTITY_EFFECT_CAUSE_OWNER;
        error->operation = sentinel;
        error->source_class = sentinel;
        error->source_bytecode = 611;
        error->flag_bytecode = 612;
        error->watcher_bytecode = 613;
        error->watcher_index = 614;
        error->entity.status = MC_ENTITY_BUSY;
        record->byte_value = -37;
        if (i == 1)
            record->type = 2; /* Metadata changes no actual Object class. */
        else if (i == 2)
            entity->watcher = NULL;
        else if (i == 3)
            watcher->map = NULL;
        else if (i == 4)
            node->record = NULL;
        else if (i == 5)
            record->payload_kind = MC_ENTITY_WATCH_PAYLOAD_NULL;
        else if (i == 6)
            record->payload_kind = MC_ENTITY_WATCH_PAYLOAD_INTEGER;
        else if (i == 7)
            record->payload_kind = (mc_entity_watch_payload_kind)1;
        else if (i == 8) {
            record->payload_kind = MC_ENTITY_WATCH_PAYLOAD_STRING;
            record->string_value = NULL;
        } else if (i == 10)
            map->size = 0;
        const mc_entity_watch_record input_record = *record;
        const int32_t index = i == 9 ? 91 : 0;
        int8_t value = 37;
        mc_entity_watcher_byte_error leaf;
        const bool returned = mc_entity_watcher_byte_read_entered(entity, index, &value, &leaf);
        mc_entity_effect_watcher_byte_apply(&leaf, error);
        const bool source_unchanged = entity->holds == holds &&
                                      memcmp(record, &input_record, sizeof(input_record)) == 0 &&
                                      memcmp(entity->random, &random, sizeof(random)) == 0;
        /* End every declared fault before any assertion/fixture exit. */
        entity->watcher = watcher;
        watcher->map = map;
        node->record = record;
        map->size = map_size;
        *record = saved;
        CHECK(
            returned == cases[i].returned && source_unchanged && value == (returned ? -37 : 37) &&
            leaf.index == (returned ? -1 : index) &&
            leaf.status == (returned ? MC_ENTITY_OK : MC_ENTITY_STATE_ERROR) &&
            leaf.stage == cases[i].stage && leaf.pc == cases[i].pc &&
            leaf.cause == cases[i].cause &&
            (returned ? leaf.operation == NULL : strcmp(leaf.operation, cases[i].operation) == 0));
        CHECK(error->watcher_index == (returned ? -1 : index) &&
              error->status == (returned ? MC_ENTITY_LIMIT : MC_ENTITY_STATE_ERROR) &&
              error->cause == (returned ? MC_ENTITY_EFFECT_CAUSE_OWNER : cases[i].cause) &&
              error->source_class == sentinel && error->entity.status == MC_ENTITY_BUSY &&
              error->source_bytecode == (returned ? 611 : cases[i].pc) &&
              error->flag_bytecode ==
                  (cases[i].stage == MC_ENTITY_WATCHER_BYTE_STAGE_FLAG ? cases[i].pc : 612) &&
              error->watcher_bytecode ==
                  (cases[i].stage == MC_ENTITY_WATCHER_BYTE_STAGE_WATCHER ? cases[i].pc : 613) &&
              (returned ? error->operation == sentinel
                        : strcmp(error->operation, cases[i].operation) == 0));
        /* Overlay failure materialization uses the same already-returned
         * leaf, with all full diagnostic defaults explicitly initialized. */
        mc_entity_effect_error_init(error);
        mc_entity_effect_watcher_byte_apply(&leaf, error);
        CHECK(error->watcher_index == leaf.index && error->status == leaf.status &&
              error->cause == leaf.cause && error->source_bytecode == leaf.pc &&
              error->flag_bytecode ==
                  (leaf.stage == MC_ENTITY_WATCHER_BYTE_STAGE_FLAG ? leaf.pc : 0) &&
              error->watcher_bytecode ==
                  (leaf.stage == MC_ENTITY_WATCHER_BYTE_STAGE_WATCHER ? leaf.pc : 0) &&
              error->math.source_pc == -1 && error->statistic_error.writer.index == SIZE_MAX &&
              error->entity.status == MC_ENTITY_OK);
    }
    ok = true;
cleanup:
    return ok;
}

static bool movement_effects(mc_entity *entity, mc_entity *other, bool item) {
    bool ok = false;
    mc_entity_effect_error error;
    CHECK(movement_input(entity, 8.5, 8.0, 8.5));
    CHECK(entity->base.world == NULL && entity->watcher != NULL && entity->watcher->map != NULL &&
          entity->watcher->map->head != NULL && entity->watcher->map->head->record != NULL);
    CHECK(movement_watcher_leaf(entity, &error));
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
    const mc_entity_watch_payload_kind saved_kind = record->payload_kind;
    record->payload_kind = (mc_entity_watch_payload_kind)1; /* Unrepresented native alternative. */
    const mc_entity_status wrong_type = mc_entity_effect_watcher_byte(entity, 0, &byte, &error);
    record->payload_kind = saved_kind;
    CHECK(wrong_type == MC_ENTITY_STATE_ERROR && byte == 37 && error.watcher_bytecode == 19 &&
          error.cause == MC_ENTITY_EFFECT_CAUSE_UNSUPPORTED_WATCHER_VALUE);
    /* Declared wire metadata does not select the Object returned by ma.b(). */
    const int32_t declared = record->type;
    record->type = 2;
    const mc_entity_status metadata_only = mc_entity_effect_watcher_byte(entity, 0, &byte, &error);
    record->type = declared;
    CHECK(metadata_only == MC_ENTITY_OK && byte == saved_byte &&
          record->payload_kind == saved_kind);
    byte = 37;
    record->payload_kind = MC_ENTITY_WATCH_PAYLOAD_NULL;
    const mc_entity_status null_payload = mc_entity_effect_watcher_byte(entity, 0, &byte, &error);
    record->payload_kind = saved_kind;
    CHECK(null_payload == MC_ENTITY_STATE_ERROR && byte == 37 && error.watcher_bytecode == 22 &&
          error.cause == MC_ENTITY_EFFECT_CAUSE_NULL_REFERENCE && record->type == declared);
    record->payload_kind = MC_ENTITY_WATCH_PAYLOAD_INTEGER;
    const mc_entity_status integer_payload =
        mc_entity_effect_watcher_byte(entity, 0, &byte, &error);
    record->payload_kind = saved_kind;
    CHECK(integer_payload == MC_ENTITY_STATE_ERROR && byte == 37 && error.watcher_bytecode == 19 &&
          error.cause == MC_ENTITY_EFFECT_CAUSE_CLASS_CAST && record->type == declared);
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
