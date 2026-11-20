/* Authored contracts for actual GI damage and its reached World/Wolf owners. */
int mc_wolf_damage_fixture_main(int, char **);
#define MC_WOLF_ATTACK_BODY_MAIN_NAME mc_wolf_damage_fixture_main
#include "../test_wolf_attack.c"
#undef MC_WOLF_ATTACK_BODY_MAIN_NAME
#undef main

#include "entity/arrow/references_internal.h"
#include "entity/mob/references_internal.h"
#include "entity/mob/wolf_damage_internal.h"
#include "entity/player/player_internal.h"
#include "entity/watcher_payload_internal.h"
#include "mc/entity/arrow/construction.h"
#include "world/storage/generated_internal.h"

#undef CHECK
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            fprintf(stderr, "Wolf damage check%u line%d: %s\n", checks, __LINE__, #c);             \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

static bool damage_case(random_fixture *f) {
    mc_entity *wolf = NULL, *source = NULL;
    CHECK(wolf_actor(f, MC_ENTITY_CLASS_KIND_WOLF, &wolf) &&
          wolf_actor(f, MC_ENTITY_CLASS_KIND_COW, &source));
    CHECK(mc_entity_set_position(wolf, 8.5, 64.5, 8.5) == MC_ENTITY_OK &&
          mc_entity_set_position(source, 11.5, 64.5, 12.5) == MC_ENTITY_OK);
    mc_entity_watch_record *flags = wolf_attack_flags(wolf);
    CHECK(flags != NULL && mc_entity_watch_byte_replace(flags, 5));
    wolf->mob->living.Y = 20;
    wolf->mob->living.E = 20;
    wolf->mob->living.au = wolf->base.by = 0;
    mc_entity_effect_error *error = calloc(1, sizeof(*error));
    CHECK(error != NULL);
    bool answer = false;
    const mc_entity_status status =
        mc_entity_effect_damage_with_math(wolf, f->registry, f->math, source, 4, &answer, error);
    if (status != MC_ENTITY_OK)
        fprintf(stderr, "Wolf damage operation=%s\n", error->operation);
    CHECK(status == MC_ENTITY_OK && answer && wolf->mob->living.Y == 18 && flags->byte_value == 4 &&
          wolf->mob->creature_target == source && error->living_damage.returned &&
          error->living_damage.wrapper_pc == 282);
    CHECK(mc_mob_retire(&wolf, NULL) == MC_MOB_OK && mc_mob_retire(&source, NULL) == MC_MOB_OK);
    free(error);
    return true;
}

typedef struct wolf_damage_fixture {
    random_fixture base;
    mc_entity *wolf, *cow, *old, *arrow, *other[4];
    mc_world_access *observer;
    mc_entity_effect_error *effect, *reentry;
    mc_player_string *owners[12];
    size_t owner_count, sounds;
    int callback;
    int32_t expected_health;
    bool bad_callback;
} wolf_damage_fixture;

static mc_entity_watch_record *wd_record(mc_entity *entity, int32_t key) {
    for (mc_entity_watch_node *node = entity->watcher->map->head; node != NULL; node = node->next)
        if (node->key == key)
            return node->record;
    return NULL;
}

static mc_world_storage_status wd_sound(void *context, mc_world_runtime *world,
                                        const mc_registry_string *name, double x, double y,
                                        double z, float volume, float pitch,
                                        mc_world_storage_error *error) {
    wolf_damage_fixture *f = context;
    mc_registry_string_view text = {0};
    mc_wolf_damage_state state;
    const char *expected = f->expected_health <= 0 ? "mob.wolf.death" : "mob.wolf.hurt";
    mc_entity *alias = f->wolf;
    if (world != f->base.world || !mc_registry_string_read(name, &text) ||
        text.length != strlen(expected) || x != f->wolf->base.x ||
        y != f->wolf->base.y - (double)f->wolf->base.offset || z != f->wolf->base.z ||
        volume != .4f || !(pitch >= .8f && pitch <= 1.2f) ||
        f->wolf->mob->living.Y != f->expected_health ||
        !mc_mob_wolf_damage_inspect(f->wolf, &state) || !state.active || !state.pending ||
        !state.parent_entered || state.parent_error == NULL ||
        mc_entity_effect_cleanup(f->wolf, f->reentry) != MC_ENTITY_BUSY ||
        mc_mob_retire(&alias, NULL) != MC_MOB_BUSY || alias != f->wolf)
        f->bad_callback = true;
    if (!f->bad_callback)
        for (size_t i = 0; i < text.length; ++i)
            if (text.units[i] != (uint16_t)expected[i])
                f->bad_callback = true;
    ++f->sounds;
    if (f->callback == 2)
        (void)mc_entity_watch_byte_replace(wd_record(f->wolf, 16), 4);
    else if (f->callback == 3)
        f->wolf->base.world = NULL;
    else if (f->callback == 4) {
        mc_entity_watch_record *record = wd_record(f->wolf, 16);
        record->payload_kind = MC_ENTITY_WATCH_PAYLOAD_INTEGER;
        record->integer_value = 4;
    }
    *error = (mc_world_storage_error){
        .status = f->callback == 1 ? MC_WORLD_STORAGE_IO_ERROR : MC_WORLD_STORAGE_OK,
        .operation = f->callback == 1 ? "authored Wolf hurt sound refusal" : NULL};
    return error->status;
}

static bool wd_publish(wolf_damage_fixture *f, mc_entity *entity, double x) {
    CHECK(mc_entity_set_position(entity, x, 64.5, 8.5) == MC_ENTITY_OK);
    bool inserted = false;
    CHECK(mc_world_runtime_insert_entity(f->base.world, entity, &inserted, NULL) ==
              MC_WORLD_STORAGE_OK &&
          inserted);
    return true;
}

static bool wd_prepare(wolf_damage_fixture *f) {
    for (int32_t x = -1; x <= 3; ++x)
        for (int32_t z = -1; z <= 1; ++z) {
            uint8_t *blocks = calloc(MC_CHUNK_BLOCKS, 1);
            mc_world_chunk *object = NULL;
            CHECK(blocks != NULL &&
                  mc_world_store_create_generated(f->base.store, f->base.world, 0, x, z, &blocks,
                                                  MC_CHUNK_BLOCKS, &object,
                                                  NULL) == MC_WORLD_STORAGE_OK &&
                  blocks == NULL);
            mc_chunk_state *chunk = mc_world_chunk_state(object);
            CHECK(chunk != NULL && mc_chunk_cache_publish(f->base.provider.cache, x, z, chunk));
            f->base.owned[f->base.owned_count++] = chunk;
        }
    CHECK(wolf_actor(&f->base, MC_ENTITY_CLASS_KIND_WOLF, &f->wolf) &&
          wolf_actor(&f->base, MC_ENTITY_CLASS_KIND_COW, &f->cow) &&
          wolf_actor(&f->base, MC_ENTITY_CLASS_KIND_PIG, &f->old) && player(&f->base));
    CHECK(wd_publish(f, f->wolf, 8.5) &&
          mc_entity_set_position(f->cow, 11.5, 64.5, 12.5) == MC_ENTITY_OK &&
          mc_entity_set_position(f->base.player, 11.5, 64.5, 12.5) == MC_ENTITY_OK);
    const double positions[] = {10.5, 12.5, 14.5, 50.5};
    for (size_t i = 0; i < COUNT(f->other); ++i)
        CHECK(wolf_actor(&f->base, MC_ENTITY_CLASS_KIND_WOLF, &f->other[i]) &&
              wd_publish(f, f->other[i], positions[i]));
    CHECK(mc_arrow_allocate(f->base.entities, &f->arrow, NULL) == MC_ENTITY_OK &&
          mc_arrow_construct_world(f->arrow, f->base.world, NULL) == MC_ENTITY_OK &&
          mc_entity_set_position(f->arrow, 11.5, 64.5, 12.5) == MC_ENTITY_OK);
    f->effect = calloc(1, sizeof(*f->effect));
    f->reentry = calloc(1, sizeof(*f->reentry));
    const mc_world_access_callbacks callbacks = {.sound = wd_sound};
    CHECK(f->effect != NULL && f->reentry != NULL &&
          mc_world_access_create(&callbacks, f, &f->observer, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(f->base.world, f->observer, NULL) ==
              MC_WORLD_STORAGE_OK);
    return true;
}

static bool wd_reset(wolf_damage_fixture *f, int8_t flags, bool old_target) {
    CHECK(!mc_entity_effect_has_pending(f->wolf));
    f->wolf->base.world = f->base.world;
    CHECK(mc_mob_creature_target_store(f->wolf, old_target ? f->old : NULL, NULL) == MC_ENTITY_OK);
    (void)mc_entity_watch_byte_replace(wd_record(f->wolf, 16), flags);
    f->wolf->watcher->dirty = false;
    f->wolf->mob->living.Y = 20;
    f->wolf->mob->living.E = 20;
    f->wolf->mob->living.au = f->wolf->base.by = 0;
    f->wolf->mob->living.ah = false;
    f->wolf->mob->living.T = -1;
    f->wolf->base.be = false;
    f->wolf->base.yaw = 30.f;
    f->wolf->base.velocity_x = .25;
    f->wolf->base.velocity_y = -.375;
    f->wolf->base.velocity_z = -.125;
    mc_java_random_reseed(f->wolf->random, 38);
    for (size_t i = 0; i < COUNT(f->other); ++i) {
        CHECK(mc_mob_creature_target_store(f->other[i], i == 2 ? f->old : NULL, NULL) ==
              MC_ENTITY_OK);
        (void)mc_entity_watch_byte_replace(wd_record(f->other[i], 16), i == 0 ? 1 : i == 1 ? 4 : 0);
    }
    CHECK(mc_arrow_shooter_store(f->arrow, NULL, NULL) == MC_ENTITY_OK);
    f->base.world->fields.B = false;
    f->callback = 0;
    f->sounds = 0;
    f->bad_callback = false;
    f->expected_health = 18;
    return true;
}

static bool wd_call(wolf_damage_fixture *f, mc_entity *source, int32_t amount,
                    mc_entity_status expected, bool *answer) {
    const mc_entity_status status = mc_entity_effect_damage_with_math(
        f->wolf, f->base.registry, f->base.math, source, amount, answer, f->effect);
    if (status != expected)
        fprintf(stderr, "Wolf matrix status=%d expected=%d pc=%u operation=%s\n", (int)status,
                (int)expected, f->effect->wolf_damage.site.pc,
                f->effect->operation == NULL ? "none" : f->effect->operation);
    CHECK(status == expected && !f->bad_callback);
    return true;
}

static bool wd_name(wolf_damage_fixture *f, const uint16_t *name, size_t length,
                    const uint16_t *owner, size_t owner_length) {
    mc_player_string *new_name = NULL, *new_owner = NULL;
    CHECK(f->owner_count < COUNT(f->owners) &&
          mc_player_string_create(name, length, &new_name) == MC_PLAYER_OK &&
          mc_player_string_create(owner, owner_length, &new_owner) == MC_PLAYER_OK);
    mc_player_string_release((mc_player_string *)f->base.player->local_player->fields.player.l);
    f->base.player->local_player->fields.player.l = new_name; /* Transfer the actual field hold. */
    f->owners[f->owner_count++] = new_owner;
    mc_entity_watch_record *record = wd_record(f->wolf, 17);
    record->payload_kind = MC_ENTITY_WATCH_PAYLOAD_STRING;
    record->string_value = new_owner; /* Declared String input with separately live backing. */
    return true;
}

static bool wd_returns(wolf_damage_fixture *f) {
    bool answer = false;
    CHECK(
        wd_reset(f, 1, false) && wd_call(f, f->cow, 4, MC_ENTITY_OK, &answer) && answer &&
        f->wolf->mob->living.Y == 18 && f->sounds == 1 &&
        f->effect->wolf_damage.parent_amount == 2 && f->effect->wolf_damage.query_returned &&
        f->effect->wolf_damage.wolves_examined == 4 && f->effect->wolf_damage.wolves_alerted == 2 &&
        f->wolf->mob->creature_target == f->cow && f->other[0]->mob->creature_target == f->cow &&
        wd_record(f->other[0], 16)->byte_value == 1 && f->other[1]->mob->creature_target == NULL &&
        f->other[2]->mob->creature_target == f->old && f->other[3]->mob->creature_target == NULL &&
        !mc_entity_effect_has_pending(f->wolf));

    CHECK(wd_reset(f, 1, false));
    f->expected_health = 16;
    CHECK(wd_call(f, f->base.player, 4, MC_ENTITY_OK, &answer) && answer &&
          f->wolf->mob->living.Y == 16 && wd_record(f->wolf, 16)->byte_value == 2 &&
          f->wolf->mob->creature_target == f->base.player &&
          f->other[0]->mob->creature_target == f->base.player &&
          wd_record(f->other[0], 16)->byte_value == 3 &&
          f->effect->wolf_damage.wolves_alerted == 1);

    CHECK(wd_reset(f, 0, false) &&
          mc_arrow_shooter_store(f->arrow, f->base.player, NULL) == MC_ENTITY_OK);
    f->expected_health = 16;
    CHECK(wd_call(f, f->arrow, 4, MC_ENTITY_OK, &answer) && answer &&
          f->effect->wolf_damage.argument_source_identity == (uintptr_t)f->arrow &&
          f->effect->wolf_damage.source_identity == (uintptr_t)f->base.player &&
          f->effect->wolf_damage.parent_amount == 4 &&
          f->wolf->mob->creature_target == f->base.player &&
          f->other[0]->mob->creature_target == f->base.player &&
          wd_record(f->other[0], 16)->byte_value == 3 && f->arrow->holds == 1);

    CHECK(wd_reset(f, 0, true));
    f->expected_health = 16;
    CHECK(wd_call(f, f->arrow, 4, MC_ENTITY_OK, &answer) && answer &&
          !f->effect->wolf_damage.query_entered && f->wolf->mob->creature_target == f->old &&
          f->other[0]->mob->creature_target == NULL && f->arrow->holds == 1);

    CHECK(wd_reset(f, 4, false));
    f->wolf->mob->living.T = 7;
    f->expected_health = 0;
    const size_t arrow_holds = f->arrow->holds;
    CHECK(wd_call(f, f->arrow, 20, MC_ENTITY_OK, &answer) && answer &&
          f->effect->wolf_damage.parent_amount == 20 && f->wolf->mob->living.Y == 0 &&
          f->wolf->mob->living.ah && f->effect->death.returned &&
          f->effect->death.source_identity == (uintptr_t)f->arrow && f->effect->death.score == 7 &&
          f->effect->death.score_callback_returned && f->effect->death.kill_callback_returned &&
          f->effect->death.drop_id == -1 && f->effect->death.drops_attempted == 0 &&
          !f->effect->death.pending && !f->effect->wolf_damage.pending &&
          !f->effect->wolf_damage.query_entered && f->wolf->mob->creature_target == f->arrow &&
          f->arrow->holds == arrow_holds + 1);

    mc_entity *item = NULL;
    CHECK(mc_item_entity_allocate(f->base.entities, &item, NULL) == MC_ENTITY_OK &&
          mc_item_entity_construct_world(item, f->base.world, NULL) == MC_ENTITY_OK &&
          mc_entity_set_position(item, 11.5, 64.5, 12.5) == MC_ENTITY_OK && wd_reset(f, 4, false));
    f->wolf->mob->living.T = 7;
    f->expected_health = 0;
    const size_t item_holds = item->holds;
    const uint64_t item_random = item->random->state;
    CHECK(wd_call(f, item, 40, MC_ENTITY_OK, &answer) && answer && f->effect->damage_amount == 40 &&
          f->effect->wolf_damage.parent_amount == 20 && f->wolf->mob->living.Y == 0 &&
          f->wolf->mob->living.ah && f->effect->death.returned &&
          f->effect->death.source_identity == (uintptr_t)item && f->effect->death.score == 7 &&
          f->effect->death.score_callback_returned && f->effect->death.kill_callback_returned &&
          f->effect->death.drop_id == -1 && f->effect->death.drops_attempted == 0 &&
          !f->effect->death.pending && !f->effect->wolf_damage.pending &&
          !f->effect->wolf_damage.query_entered && f->wolf->mob->creature_target == item &&
          item->holds == item_holds + 1 && item->random->state == item_random);
    CHECK(mc_mob_creature_target_store(f->wolf, NULL, NULL) == MC_ENTITY_OK);
    mc_entity_release(item);

    CHECK(wd_reset(f, 2, true) && wd_call(f, f->cow, 4, MC_ENTITY_OK, &answer) && answer &&
          !f->effect->wolf_damage.query_entered && f->wolf->mob->creature_target == f->cow &&
          f->other[0]->mob->creature_target == NULL);
    CHECK(wd_reset(f, 2, true) && wd_call(f, f->wolf, 4, MC_ENTITY_OK, &answer) && answer &&
          f->wolf->mob->creature_target == f->old && !f->effect->wolf_damage.query_entered);

    CHECK(wd_reset(f, 4, true));
    f->expected_health = 16;
    f->wolf->mob->creature_target =
        NULL; /* Unreached malformed slot, distinct from its live hold. */
    CHECK(wd_call(f, NULL, 4, MC_ENTITY_OK, &answer) && answer &&
          f->wolf->mob->creature_target == NULL && !f->effect->wolf_damage.query_entered);
    f->wolf->mob->creature_target = f->old;

    CHECK(wd_reset(f, 1, false));
    f->callback = 2;
    CHECK(wd_call(f, f->cow, 4, MC_ENTITY_OK, &answer) && answer &&
          wd_record(f->wolf, 16)->byte_value == 4 && !f->effect->wolf_damage.query_entered &&
          f->wolf->mob->creature_target == f->cow && f->other[0]->mob->creature_target == NULL);

    CHECK(wd_reset(f, 5, true));
    f->base.world->fields.B = true;
    answer = true;
    CHECK(mc_entity_effect_damage_with_math(f->wolf, NULL, NULL, f->cow, 4, &answer, f->effect) ==
              MC_ENTITY_OK &&
          !answer && f->wolf->mob->living.Y == 20 && wd_record(f->wolf, 16)->byte_value == 4 &&
          f->sounds == 0 && f->effect->wolf_damage.site.pc == 284 &&
          f->wolf->mob->creature_target == f->old);
    CHECK(wd_reset(f, 5, true));
    f->wolf->base.by = 20;
    f->wolf->mob->living.au = 4;
    answer = true;
    CHECK(mc_entity_effect_damage_with_math(f->wolf, NULL, NULL, f->cow, 4, &answer, f->effect) ==
              MC_ENTITY_OK &&
          !answer && f->wolf->mob->living.Y == 20 && f->sounds == 0 &&
          wd_record(f->wolf, 16)->byte_value == 4 && f->effect->wolf_damage.site.pc == 284);

    CHECK(wd_reset(f, 4, false));
    f->expected_health = 1073741844;
    CHECK(wd_call(f, f->cow, INT32_MAX, MC_ENTITY_OK, &answer) && answer &&
          f->effect->damage_amount == INT32_MAX &&
          f->effect->wolf_damage.parent_amount == -1073741824 &&
          f->wolf->mob->living.Y == 1073741844);
    CHECK(wd_reset(f, 4, false));
    f->expected_health = 20;
    CHECK(wd_call(f, f->cow, 0, MC_ENTITY_OK, &answer) && answer && f->sounds == 1 &&
          f->effect->wolf_damage.parent_amount == 0 && f->wolf->mob->living.Y == 20 &&
          f->wolf->mob->creature_target == f->cow);

    CHECK(wd_reset(f, 4, false));
    f->wolf->mob->living.Y = 2;
    f->expected_health = 0;
    CHECK(wd_call(f, f->cow, 4, MC_ENTITY_OK, &answer) && answer && f->wolf->mob->living.ah &&
          f->effect->death.returned && f->effect->death.drop_id == -1 &&
          !f->effect->death.quantity_attempted && f->effect->death.drops_attempted == 0 &&
          f->wolf->mob->creature_target == f->cow);
    return true;
}

static bool wd_owners(wolf_damage_fixture *f) {
    const uint16_t name[] = {'O', 'w', 'n', 'e', 'r', 'A'};
    const uint16_t same[] = {'o', 'W', 'N', 'E', 'R', 'a'};
    const uint16_t other[] = {'o', 't', 'h', 'e', 'r'};
    bool answer = false;
    CHECK(wd_name(f, name, COUNT(name), same, COUNT(same)) && wd_reset(f, 4, true));
    f->expected_health = 16;
    CHECK(wd_call(f, f->base.player, 4, MC_ENTITY_OK, &answer) && answer &&
          f->effect->wolf_damage.site.pc == 275 && f->wolf->mob->creature_target == f->old);
    CHECK(wd_name(f, name, COUNT(name), other, COUNT(other)) && wd_reset(f, 4, true));
    f->expected_health = 16;
    CHECK(wd_call(f, f->base.player, 4, MC_ENTITY_OK, &answer) && answer &&
          f->effect->wolf_damage.site.pc == 282 && f->wolf->mob->creature_target == f->base.player);
    const uint16_t unicode_name[] = {0xd801, 0xdc00, 0x212a};
    const uint16_t unicode_owner[] = {0xd801, 0xdc28, 'k'};
    CHECK(wd_name(f, unicode_name, COUNT(unicode_name), unicode_owner, COUNT(unicode_owner)) &&
          wd_reset(f, 4, true));
    f->expected_health = 16;
    CHECK(wd_call(f, f->base.player, 4, MC_ENTITY_OK, &answer) && answer &&
          f->effect->wolf_damage.site.pc == 275 && f->wolf->mob->creature_target == f->old);
    CHECK(wd_reset(f, 4, true));
    f->expected_health = 16;
    mc_entity_watch_record *owner = wd_record(f->wolf, 17);
    const mc_player_string *saved = owner->string_value;
    owner->string_value = NULL;
    CHECK(wd_call(f, f->base.player, 4, MC_ENTITY_OK, &answer) && answer &&
          f->wolf->mob->creature_target == f->base.player);
    owner->string_value = saved;
    return true;
}

static bool wd_failures(wolf_damage_fixture *f) {
    bool answer = false;
    CHECK(wd_reset(f, 4, true));
    mc_entity_watch_record *first = wd_record(f->wolf, 16);
    first->payload_kind = MC_ENTITY_WATCH_PAYLOAD_INTEGER;
    first->integer_value = 4;
    answer = true;
    CHECK(wd_call(f, f->cow, 4, MC_ENTITY_STATE_ERROR, &answer) && answer &&
          f->wolf->mob->living.Y == 20 && f->sounds == 0 &&
          !f->effect->wolf_damage.parent_entered && !f->effect->living_damage.entered &&
          f->effect->wolf_damage.site.pc == 2 && f->effect->wolf_damage.source_exception &&
          mc_entity_effect_error_retire(f->effect));
    CHECK(wd_reset(f, 4, true));
    f->callback = 1;
    answer = false;
    CHECK(wd_call(f, f->cow, 4, MC_ENTITY_WORLD_FAILED, &answer) && !answer &&
          f->wolf->mob->living.Y == 18 && f->effect->wolf_damage.pending &&
          !f->effect->wolf_damage.returned && f->effect->wolf_damage.native_policy &&
          !f->effect->wolf_damage.source_exception);
    const uint64_t random = f->wolf->random->state;
    bool rejected_answer = true;
    CHECK(mc_entity_effect_damage_with_math(f->wolf, f->base.registry, f->base.math, f->cow, 4,
                                            &rejected_answer, f->reentry) == MC_ENTITY_BUSY &&
          rejected_answer && f->wolf->random->state == random && f->wolf->mob->living.Y == 18 &&
          mc_entity_effect_error_retire(f->effect) && !mc_entity_effect_has_pending(f->wolf));

    CHECK(wd_reset(f, 0, false));
    f->callback = 4;
    answer = false;
    CHECK(wd_call(f, f->cow, 4, MC_ENTITY_STATE_ERROR, &answer) && !answer &&
          f->wolf->mob->living.Y == 18 && f->effect->wolf_damage.site.pc == 39 &&
          f->effect->wolf_damage.source_exception &&
          strcmp(f->effect->wolf_damage.exception_class, "java/lang/ClassCastException") == 0 &&
          mc_entity_effect_error_retire(f->effect));
    CHECK(wd_record(f->wolf, 16)->payload_kind == MC_ENTITY_WATCH_PAYLOAD_INTEGER);

    CHECK(wd_reset(f, 0, false));
    f->callback = 3;
    answer = false;
    CHECK(wd_call(f, f->cow, 4, MC_ENTITY_STATE_ERROR, &answer) && !answer &&
          f->wolf->mob->living.Y == 18 && f->effect->wolf_damage.site.pc == 152 &&
          f->effect->wolf_damage.source_exception && f->effect->wolf_damage.bounds != NULL &&
          f->effect->wolf_damage.expanded != NULL && !f->effect->wolf_damage.query_entered);
    const size_t bounds_holds = f->base.entities->bounds_holds;
    f->base.entities->bounds_holds = 0; /* Actual native ledger refusal, never a Source permit. */
    CHECK(!mc_entity_effect_error_retire(f->effect) && mc_entity_effect_has_pending(f->wolf));
    f->base.entities->bounds_holds = bounds_holds;
    CHECK(mc_entity_effect_error_retire(f->effect) && f->wolf->base.world == NULL &&
          !mc_entity_effect_has_pending(f->wolf));

    CHECK(wd_reset(f, 0, false));
    mc_entity_watch_record *bad = wd_record(f->other[1], 16);
    bad->payload_kind = MC_ENTITY_WATCH_PAYLOAD_INTEGER;
    bad->integer_value = 4;
    answer = false;
    CHECK(wd_call(f, f->cow, 4, MC_ENTITY_STATE_ERROR, &answer) && !answer &&
          f->wolf->mob->creature_target == f->cow && f->other[0]->mob->creature_target == f->cow &&
          f->effect->wolf_damage.query_returned && f->effect->wolf_damage.wolves_alerted == 2 &&
          f->effect->wolf_damage.site.pc == 195 && f->effect->wolf_damage.source_exception &&
          mc_entity_effect_error_retire(f->effect) &&
          bad->payload_kind == MC_ENTITY_WATCH_PAYLOAD_INTEGER);

    CHECK(wd_reset(f, 4, true));
    f->wolf->mob->creature_target =
        f->cow; /* Reached source slot disagrees with its native hold. */
    answer = false;
    CHECK(wd_call(f, f->cow, 4, MC_ENTITY_STATE_ERROR, &answer) && !answer &&
          f->wolf->mob->living.Y == 18 && f->effect->wolf_damage.site.pc == 278 &&
          f->effect->wolf_damage.native_policy && mc_entity_effect_error_retire(f->effect));
    CHECK(f->wolf->mob->creature_target == f->cow && f->wolf->mob->creature_target_hold == f->old);
    f->wolf->mob->creature_target = f->old;

    CHECK(wd_reset(f, 4, true));
    f->expected_health = 16;
    const mc_player_string *name = f->base.player->local_player->fields.player.l;
    f->base.player->local_player->fields.player.l = NULL;
    answer = false;
    CHECK(wd_call(f, f->base.player, 4, MC_ENTITY_STATE_ERROR, &answer) && !answer &&
          f->wolf->mob->living.Y == 16 && f->effect->wolf_damage.site.pc == 268 &&
          f->effect->wolf_damage.source_exception && mc_entity_effect_error_retire(f->effect));
    f->base.player->local_player->fields.player.l = name;

    CHECK(wd_reset(f, 4, true));
    answer = false;
    CHECK(mc_entity_effect_damage_with_math(f->wolf, f->base.registry, NULL, f->cow, 4, &answer,
                                            f->effect) == MC_ENTITY_STATE_ERROR &&
          !answer && f->wolf->mob->living.Y == 18 && f->sounds == 0 &&
          f->effect->cause == MC_ENTITY_EFFECT_CAUSE_MATH &&
          mc_entity_effect_error_retire(f->effect));

    CHECK(wd_reset(f, 4, true));
    f->wolf->mob->living.Y = 4;
    f->wolf->mob->living.T = 1;
    f->expected_health = 0;
    f->base.player->local_player->fields.player.g = 100;
    answer = false;
    CHECK(wd_call(f, f->base.player, 4, MC_ENTITY_STATE_ERROR, &answer) && !answer &&
          f->wolf->mob->living.Y == 0 && f->effect->death.pending &&
          f->effect->wolf_damage.parent_cleanup_owned &&
          f->base.player->local_player->fields.player.g == 101 &&
          mc_entity_effect_error_retire(f->effect) && !mc_entity_effect_has_pending(f->wolf) &&
          f->base.player->local_player->fields.player.g == 101);
    return true;
}

static bool wd_dispose(wolf_damage_fixture *f) {
    CHECK(!mc_entity_effect_has_pending(f->wolf));
    /* Separate terminal fixture stores run only after all source work and its
     * failure cleanup have completed; they supply no source cleanup credit. */
    CHECK(mc_mob_creature_target_store(f->wolf, NULL, NULL) == MC_ENTITY_OK);
    for (size_t i = 0; i < COUNT(f->other); ++i)
        CHECK(mc_mob_creature_target_store(f->other[i], NULL, NULL) == MC_ENTITY_OK);
    bool removed = false;
    CHECK(mc_world_runtime_remove_world_access(f->base.world, f->observer, &removed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          removed && mc_world_access_free(f->observer, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_retire_entity_containers(f->base.world, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_mob_retire(&f->wolf, NULL) == MC_MOB_OK);
    for (size_t i = 0; i < COUNT(f->other); ++i)
        CHECK(mc_mob_retire(&f->other[i], NULL) == MC_MOB_OK);
    mc_entity_release(f->arrow);
    CHECK(mc_mob_retire(&f->cow, NULL) == MC_MOB_OK && mc_mob_retire(&f->old, NULL) == MC_MOB_OK);
    for (size_t i = 0; i < f->owner_count; ++i)
        mc_player_string_release(f->owners[i]);
    free(f->effect);
    free(f->reentry);
    CHECK(teardown(&f->base));
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3)
        return 2;
    wolf_damage_fixture *f = calloc(1, sizeof(*f));
    if (f == NULL || !setup(&f->base, argv[1], argv[2]) || !damage_case(&f->base) ||
        !wd_prepare(f) || !wd_returns(f) || !wd_owners(f) || !wd_failures(f) || !wd_dispose(f))
        return 1;
    free(f);
    printf("PASS: %u Wolf damage source-order/ownership checks\n", checks);
    return 0;
}
