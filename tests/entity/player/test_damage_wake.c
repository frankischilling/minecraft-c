/* Actual Player damage over the existing genuine World/Bed/sleep fixture.
 * Imported test entry points are never executed. */
#define MC_SLEEP_WAKE_ENTRY mc_damage_wake_unused_sleep_main
#include "test_sleep_wake.c"
#undef MC_SLEEP_WAKE_ENTRY

#include "../../../src/entity/tick/tick_internal.h"
#include "mc/entity/movement/effects.h"

static bool dw_zero(const char *root, int32_t direction) {
    bool ok = false;
    sw_fixture s = {0};
    mc_entity_effect_error *error = calloc(1, sizeof(*error));
    char path[8192];
    const int32_t input[5] = {direction, 1, direction, 0, 0};
    CHECK(error != NULL &&
          snprintf(path, sizeof(path), "%s/direction-%" PRId32, root, direction) > 0 &&
          sw_prepare(&s, path, input));
    mc_entity *p = s.actor;
    mc_player_fields *fields = &p->local_player->fields.player;
    const mc_player_coordinates *saved = fields->private_b;
    const mc_player_coordinates *bed = fields->v;
    const size_t holds = p->holds;
    const uint64_t random = p->random->state;
    bool result = true;
    p->local_player->fields.living.av = 99;
    const mc_entity_status status =
        mc_entity_effect_damage_in_context(p, s.world->registry[0], NULL, 0, &result, error);
    if (status != MC_ENTITY_OK)
        fprintf(stderr, "damage wake status%d pc%u: %s\n", (int)status,
                (unsigned int)error->source_bytecode, error->operation);
    CHECK(status == MC_ENTITY_OK && !result && !fields->u && fields->private_a == 0 &&
          fields->private_b == saved && fields->v == bed &&
          p->local_player->fields.living.Y == 20 && p->local_player->fields.living.av == 0 &&
          p->base.width == .6f && p->base.height == 1.8f && p->base.offset == 1.62f &&
          p->holds == holds && p->random->state == random && !mc_entity_effect_has_pending(p) &&
          !s.world->worlds[0]->fields.J);
    uint8_t metadata = UINT8_MAX;
    CHECK(mc_chunk_nibbles_get(s.world->states[0]->data->metadata, MC_CHUNK_NIBBLE_BYTES, 8, 64, 8,
                               &metadata) &&
          metadata == (direction | 8));
    /* A second ordinary zero-damage call does not wake or move again. */
    const mc_entity_base_fields after = p->base;
    result = true;
    CHECK(mc_entity_effect_damage_in_context(p, NULL, NULL, 0, &result, NULL) == MC_ENTITY_OK &&
          !result && sw_base_same(&after, &p->base) && p->holds == holds);
    ok = true;
cleanup:
    if (s.actor != NULL && mc_entity_effect_has_pending(s.actor) &&
        mc_entity_effect_cleanup(s.actor, NULL) != MC_ENTITY_OK)
        ok = false;
    if (!sw_retire(&s))
        ok = false;
    free(error);
    return ok;
}

static bool dw_control(const char *root, int32_t mode) {
    bool ok = false;
    sw_fixture s = {0};
    mc_entity_effect_error *error = calloc(1, sizeof(*error));
    mc_entity_effect_error *other = calloc(1, sizeof(*other));
    mc_player_tick_error *parent = calloc(1, sizeof(*parent));
    mc_entity_tick_error *classification = calloc(1, sizeof(*classification));
    char path[8192];
    const int32_t input[5] = {mode, 1, 0, 0, 0};
    CHECK(error != NULL && other != NULL && parent != NULL && classification != NULL &&
          snprintf(path, sizeof(path), "%s/control-%" PRId32, root, mode) > 0 &&
          sw_prepare(&s, path, input));
    mc_entity *p = s.actor;
    mc_world_runtime *world = s.world->worlds[0];
    mc_player_fields *fields = &p->local_player->fields.player;
    const size_t holds = p->holds;
    const uint64_t random = p->random->state;
    const mc_player_coordinates *saved = fields->private_b;
    if (mode == 0)
        p->local_player->fields.living.Y = 0;
    if (mode == 1)
        world->fields.B = true;
    if (mode == 2)
        p->base.world = NULL;
    if (mode == 3 || mode == 7 || mode >= 12)
        p->bounds = NULL;
    if (mode == 4)
        CHECK(sw_cell(s.world->states[0]->data, 8, 64, 8, 0, 0));
    if (mode == 5)
        fields->v = NULL;
    mc_entity_effect_error *output = mode == 7 || mode == 8 ? NULL : error;
    bool answer = true;
    const mc_entity_status status =
        mode == 11 ? mc_entity_effect_fire_damage_in_context(p, s.world->registry[0], 0, error)
                   : mc_entity_effect_damage_in_context(p, mode == 6 ? NULL : s.world->registry[0],
                                                        NULL, mode == 10 ? 4 : 0,
                                                        mode == 9 ? NULL : &answer, output);
    if (mode == 0 || mode == 1) {
        CHECK(status == MC_ENTITY_OK && !answer && fields->u && fields->private_a == 37 &&
              p->holds == holds && !mc_entity_effect_has_pending(p));
    } else if (mode == 2 || mode == 9) {
        CHECK(status != MC_ENTITY_OK && answer && fields->u && fields->private_a == 37 &&
              !mc_entity_effect_has_pending(p) && p->holds == holds &&
              error->source_bytecode == (mode == 2 ? 25 : 0));
    } else if (mode == 3 || mode == 6 || mode == 7 || mode >= 12) {
        CHECK(status != MC_ENTITY_OK && answer && fields->u && fields->private_a == 37 &&
              mc_entity_effect_has_pending(p) && p->holds > holds);
        if (output != NULL) {
            CHECK(error->player_wake.pending && !error->player_wake.active &&
                  !error->player_wake.returned && error->player_wake.receiver == p &&
                  error->player_wake.error != NULL && error->source_bytecode == 35);
            const mc_entity_status original_status = error->status;
            const char *const original_operation = error->operation;
            const mc_player_sleep_error *const original_error = error->player_wake.error;
            const mc_player_site original_site = error->player_wake.site;
            const bool original_source_exception = error->player_wake.source_exception;
            if (mode >= 13) {
                /* Authored cleanup envelopes carry a genuinely failed effect.
                 * These select the actual three parent cleanup sinks; the
                 * separate client fixture enters its environment sink through
                 * the real World/Player/fire call chain. */
                mc_player_tick_error_init(parent);
                mc_entity_effect_error *slot;
                mc_player_tick_effect_child selected;
                if (mode == 13) {
                    parent->child = MC_PLAYER_TICK_CHILD_ENVIRONMENT;
                    slot = &parent->child_error.environment.effect;
                    selected = MC_PLAYER_TICK_EFFECT_ENVIRONMENT;
                } else {
                    parent->child = MC_PLAYER_TICK_CHILD_ACTION;
                    parent->child_error.action.child_kind = MC_PLAYER_ACTION_CHILD_TRAVEL;
                    slot = mode == 14 ? &parent->child_error.action.travel.effect
                                      : &parent->child_error.action.travel.movement.effect;
                    selected =
                        mode == 14 ? MC_PLAYER_TICK_EFFECT_TRAVEL : MC_PLAYER_TICK_EFFECT_MOVEMENT;
                }
                *slot = *error;
                const size_t pending_holds = p->holds;
                p->holds = SIZE_MAX;
                const bool refused = !mc_player_tick_error_retire(parent);
                p->holds = pending_holds;
                CHECK(refused && parent->cleanup.child == MC_PLAYER_TICK_CLEANUP_EFFECT &&
                      parent->cleanup.effect == selected &&
                      parent->cleanup.status == MC_ENTITY_LIMIT && slot->player_wake.pending &&
                      slot->player_wake.receiver == p && slot->status == original_status &&
                      mc_player_tick_error_retire(parent) && !slot->player_wake.pending &&
                      parent->cleanup.status == MC_ENTITY_OK && !mc_entity_effect_has_pending(p));
                *error = *slot;
            }
            if (mode < 13) {
                CHECK(mc_entity_effect_damage_in_context(p, NULL, NULL, 0, &answer, other) ==
                          MC_ENTITY_BUSY &&
                      other->player_wake.pending && other->player_wake.error == original_error);
                CHECK(mc_entity_effect_dead(p, other) == MC_ENTITY_BUSY &&
                      other->player_wake.pending);
                mc_entity_tick_error_init(classification);
                const mc_entity_status busy = mc_entity_effect_damage_in_context(
                    p, NULL, NULL, 0, &answer, &classification->effect);
                CHECK(busy == MC_ENTITY_BUSY &&
                      !mc_tick_effect(classification, busy, "authored pending damage admission",
                                      501) &&
                      classification->status == MC_ENTITY_BUSY && classification->native_policy &&
                      !classification->source_exception && classification->exception_class == NULL);
            }
            if (mode == 12) {
                /* Cleanup must preserve its child even if outer effect
                 * admission fails before that child can be inspected. */
                const size_t pending_holds = p->holds;
                p->holds = SIZE_MAX;
                const bool refused = mc_entity_effect_error_retire(error);
                p->holds = pending_holds;
                CHECK(!refused && error->player_wake.pending && error->player_wake.receiver == p &&
                      error->player_wake.error == original_error &&
                      error->player_wake.cleanup_status == MC_ENTITY_LIMIT);
                struct mc_player_nbt_coordinate *node = p->local_player->nbt_coordinates;
                while (node != NULL && &node->value != fields->v)
                    node = node->next;
                CHECK(node != NULL && node->pins >= 2);
                const size_t pins = node->pins;
                node->pins = 0;
                const bool child_refused = mc_entity_effect_error_retire(error);
                node->pins = pins;
                CHECK(!child_refused && error->player_wake.pending &&
                      error->player_wake.cleanup != NULL &&
                      error->player_wake.cleanup->child == MC_PLAYER_SLEEP_CLEANUP_COORDINATE &&
                      error->status == original_status && error->operation == original_operation &&
                      error->player_wake.error == original_error);
            }
            CHECK(mc_entity_effect_error_retire(error) && !error->player_wake.pending &&
                  error->player_wake.receiver == NULL && error->player_wake.error == NULL &&
                  error->player_wake.cleanup == NULL && error->status == original_status &&
                  error->operation == original_operation &&
                  error->player_wake.site.pc == original_site.pc &&
                  error->player_wake.site.method == original_site.method &&
                  error->player_wake.source_exception == original_source_exception &&
                  p->holds == holds && !mc_entity_effect_has_pending(p));
        } else {
            CHECK(mc_entity_effect_cleanup(p, NULL) == MC_ENTITY_OK && p->holds == holds &&
                  !mc_entity_effect_has_pending(p));
        }
        CHECK(fields->u && fields->private_a == 37);
    } else if (mode == 10) {
        /* Genuine wake completes before the null-App statistic failure. */
        CHECK(status != MC_ENTITY_OK && answer && !fields->u && fields->private_a == 0 &&
              fields->private_b == saved && !mc_entity_effect_has_pending(p) && p->holds == holds);
    } else {
        CHECK(status == MC_ENTITY_OK && !fields->u && fields->private_a == 0 &&
              fields->private_b == saved && !world->fields.J && !mc_entity_effect_has_pending(p) &&
              p->holds == holds);
        if (mode != 11)
            CHECK(!answer);
    }
    CHECK(p->random->state == random);
    ok = true;
cleanup:
    if (s.actor != NULL && mc_entity_effect_has_pending(s.actor) &&
        mc_entity_effect_cleanup(s.actor, NULL) != MC_ENTITY_OK)
        ok = false;
    if (!sw_retire(&s))
        ok = false;
    free(error);
    free(other);
    free(parent);
    free(classification);
    return ok;
}

int main(int argc, char **argv) {
    int result = 1;
    char root[8192];
    if (argc != 3 || strcmp(argv[1], "damage-wake") != 0 ||
        !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 2;
    for (int32_t direction = 0; direction < 4; ++direction)
        CHECK(dw_zero(root, direction));
    for (int32_t mode = 0; mode <= 15; ++mode)
        CHECK(dw_control(root, mode));
    result = 0;
cleanup:
    if (result == 0 && printf("PASS: %u Player damage wake checks\n", checks) < 0)
        return 1;
    return result;
}
