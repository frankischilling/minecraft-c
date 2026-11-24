/* Actual Player/App/writer/notice owners; the imported fixture main is not run. */
#define main mc_monster_kill_statistics_fixture_main
#include "../../client/statistics/test_player_dispatch.c"
#undef main

#include "entity/mob/death_internal.h"
#include "entity/mob/internal.h"
#include "entity/player/combat_callbacks_internal.h"
#include "mc/entity/movement/effects.h"
#include "mc/world/runtime/world_access.h"
#include "world/chunk/object_state_internal.h"
#include "world/runtime/construction_internal.h"
#include "world/runtime/runtime_internal.h"
#include "world/storage/generated_internal.h"

#include <inttypes.h>

#undef CHECK
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            fprintf(stderr, "Player Monster kill:%d: %s\n", __LINE__, #c);                         \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

static bool actor(fixture *f, mc_entity_class_kind kind, mc_entity **out) {
    const mc_entity_class_literal *type = NULL;
    CHECK(mc_entity_class_literal_get(f->app.entities, kind, &type) == MC_ENTITY_OK &&
          mc_mob_allocate(f->app.entities, f->app.registry, type, out, NULL) == MC_MOB_OK &&
          mc_mob_construct(*out, f->world, NULL) == MC_MOB_OK);
    return true;
}

static bool value(fixture *f, const mc_statistic *statistic, int32_t expected) {
    int32_t actual = -1;
    CHECK(mc_client_stats_writer_value(f->app.stats_writer_retained, statistic, &actual, NULL) ==
              MC_CLIENT_STATS_OK &&
          actual == expected);
    return true;
}

static bool kill_cases(fixture *f) {
    mc_entity_effect_error *error = calloc(1, sizeof(*error));
    CHECK(error != NULL);
    mc_player_site site;
    mc_entity *victim = NULL;
    mc_registry_achievement_view achievements;
    CHECK(mc_registry_classes_inspect_achievements(f->app.registry, &achievements, NULL) ==
          MC_REGISTRY_OK);
    const mc_statistic *parent = achievements.named[12], *enemy = achievements.named[13];
    CHECK(mc_statistic_achievement_fields_get(enemy)->c == parent);
    const int32_t score = f->player->local_player->fields.player.g;
    const size_t holds = f->player->holds;
    mc_client_stats_state before, after;
    CHECK(mc_client_stats_writer_read_state(f->app.stats_writer, &before));
    CHECK(actor(f, MC_ENTITY_CLASS_KIND_ZOMBIE, &victim));
    CHECK(mc_player_death_callback(f->player, f->app.registry, victim, 99, false, &site, error) &&
          site.pc == 14 && error->statistic == enemy && error->statistic_amount == 1 &&
          f->player->local_player->fields.player.g == score && f->player->holds == holds &&
          value(f, enemy, 0));
    CHECK(mc_client_stats_writer_read_state(f->app.stats_writer, &after) &&
          after.total_count == before.total_count && after.unsent_count == before.unsent_count &&
          after.dirty == before.dirty);
    const mc_client_achievement_notice_fields *notice =
        mc_client_achievement_notice_fields_get(f->notice_retained);
    CHECK(notice->f == NULL);

    /* The reached DC parent-presence query uses the actual current App/I.
     * Null receivers remain source failures; a missing native Registry does not. */
    f->player->local_player->fields.minecraft = NULL;
    *error = (mc_entity_effect_error){0};
    CHECK(!mc_player_death_callback(f->player, f->app.registry, victim, 0, false, &site, error) &&
          site.pc == 11 && error->statistic_error.site.pc == 28 &&
          error->living_damage.source_exception && !error->living_damage.native_policy &&
          error->living_damage.exception_class != NULL &&
          !strcmp(error->living_damage.exception_class, "java/lang/NullPointerException"));
    f->player->local_player->fields.minecraft = &f->app;
    f->app.stats_writer = NULL;
    *error = (mc_entity_effect_error){0};
    CHECK(!mc_player_death_callback(f->player, f->app.registry, victim, 0, false, &site, error) &&
          site.pc == 11 && error->statistic_error.site.pc == 35 &&
          error->living_damage.source_exception && !error->living_damage.native_policy);
    f->app.stats_writer = f->app.stats_writer_retained;
    *error = (mc_entity_effect_error){0};
    CHECK(!mc_player_death_callback(f->player, NULL, victim, 0, false, &site, error) &&
          site.pc == 8 && error->cause == MC_ENTITY_EFFECT_CAUSE_OWNER &&
          !error->living_damage.source_exception && value(f, enemy, 0));

    /* Presence with a zero value still satisfies the source prerequisite. */
    CHECK(mc_client_stats_writer_increment(f->app.stats_writer, parent, 0, NULL) ==
          MC_CLIENT_STATS_OK);
    *error = (mc_entity_effect_error){0};
    CHECK(mc_player_death_callback(f->player, f->app.registry, victim, 0, false, &site, error) &&
          site.pc == 14 && value(f, enemy, 1) && notice->f == enemy && !notice->i &&
          notice->g != 0 && f->player->local_player->fields.player.g == score);
    const int64_t notice_time = notice->g;
    f->app.achievement_notice = NULL;
    CHECK(mc_player_death_callback(f->player, f->app.registry, victim, 0, false, &site, error) &&
          value(f, enemy, 2) && notice->f == enemy && notice->g == notice_time);
    f->app.achievement_notice = f->notice_retained;
    CHECK(mc_mob_retire(&victim, NULL) == MC_MOB_OK);

    const mc_entity_class_kind kinds[] = {
        MC_ENTITY_CLASS_KIND_SPIDER, MC_ENTITY_CLASS_KIND_SKELETON, MC_ENTITY_CLASS_KIND_CREEPER};
    for (size_t i = 0; i < sizeof(kinds) / sizeof(*kinds); ++i) {
        CHECK(actor(f, kinds[i], &victim));
        CHECK(
            mc_player_death_callback(f->player, f->app.registry, victim, 0, false, &site, error) &&
            site.pc == 14 && value(f, enemy, (int32_t)i + 3) &&
            mc_mob_retire(&victim, NULL) == MC_MOB_OK);
    }

    /* Non-Monsters and null stop before any Registry/App/achievement use. */
    CHECK(actor(f, MC_ENTITY_CLASS_KIND_COW, &victim));
    f->player->local_player->fields.minecraft = NULL;
    *error = (mc_entity_effect_error){0};
    CHECK(mc_player_death_callback(f->player, NULL, victim, 0, false, &site, error) &&
          site.pc == 14 && error->statistic == NULL &&
          mc_player_death_callback(f->player, NULL, NULL, 0, false, &site, error) &&
          site.pc == 14 && value(f, enemy, 5) && f->player->holds == holds);
    f->player->local_player->fields.minecraft = &f->app;
    CHECK(mc_mob_retire(&victim, NULL) == MC_MOB_OK);
    free(error);
    return true;
}

typedef struct fatal_kill {
    fixture *owner;
    mc_entity *victim, *items[2];
    mc_world_chunk *chunk;
    mc_entity_effect_error *reentry;
    const char *sound;
    size_t added, sounds;
    bool bad_callback;
} fatal_kill;

static size_t fatal_members(const fatal_kill *f, const mc_entity *item) {
    size_t members = 0;
    for (size_t i = 0; i < f->chunk->m->length; ++i)
        for (size_t j = 0; j < f->chunk->m->items[i]->count; ++j)
            if (f->chunk->m->items[i]->items[j] == item)
                ++members;
    return members;
}

static bool fatal_world_member(const fatal_kill *f, const mc_entity *item) {
    for (size_t i = 0; i < f->owner->world->fields.b->count; ++i)
        if (f->owner->world->fields.b->items[i] == item)
            return true;
    return false;
}

static mc_world_storage_status fatal_sound(void *context, mc_world_runtime *world,
                                           const mc_registry_string *name, double x, double y,
                                           double z, float volume, float pitch,
                                           mc_world_storage_error *error) {
    fatal_kill *f = context;
    mc_registry_string_view text = {0};
    if (world != f->owner->world || !mc_registry_string_read(name, &text) ||
        text.length != strlen(f->sound) || x != f->victim->base.x ||
        y != f->victim->base.y - (double)f->victim->base.offset || z != f->victim->base.z ||
        volume != 1.f || !(pitch >= .8f && pitch <= 1.2f) || f->added != 0 ||
        f->victim->mob->living.Y != 0 || f->victim->mob->living.ah)
        f->bad_callback = true;
    for (size_t i = 0; i < text.length; ++i)
        if (i >= strlen(f->sound) || text.units[i] != (uint16_t)f->sound[i])
            f->bad_callback = true;
    ++f->sounds;
    *error = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status fatal_added(void *context, mc_world_runtime *world, mc_entity *item,
                                           mc_world_storage_error *error) {
    fatal_kill *f = context;
    mc_mob_death_state pending;
    if (world != f->owner->world || f->added >= 2 || item == NULL ||
        !mc_entity_is_kind(item, MC_ENTITY_CLASS_KIND_ITEM) || !fatal_world_member(f, item) ||
        fatal_members(f, item) != 1 || item->item.c != 10 || !f->victim->mob->living.ah ||
        f->victim->base.be || !mc_mob_death_inspect(f->victim, &pending) || !pending.active ||
        !pending.pending || !pending.score_callback_returned || !pending.kill_callback_returned ||
        f->owner->player->local_player->fields.player.g != 10 ||
        mc_entity_effect_cleanup(f->victim, f->reentry) != MC_ENTITY_BUSY)
        f->bad_callback = true;
    if (f->added < 2)
        f->items[f->added] = item;
    ++f->added;
    *error = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

static bool fatal_case(fixture *owner, mc_entity_class_kind kind, const char *name) {
    fatal_kill f = {.owner = owner,
                    .sound = kind == MC_ENTITY_CLASS_KIND_ZOMBIE ? "mob.zombiedeath"
                                                                 : "mob.skeletonhurt"};
    uint8_t *blocks = calloc(MC_CHUNK_BLOCKS, 1);
    CHECK(blocks != NULL &&
          mc_world_store_create_generated(owner->store, owner->world, 0, 0, 0, &blocks,
                                          MC_CHUNK_BLOCKS, &f.chunk, NULL) == MC_WORLD_STORAGE_OK &&
          blocks == NULL);
    const mc_chunk_acquire_context *acquisition = mc_world_runtime_acquisition(owner->world);
    CHECK(acquisition != NULL &&
          mc_chunk_cache_publish(acquisition->cache, 0, 0, mc_world_chunk_state(f.chunk)));
    CHECK(actor(owner, kind, &f.victim) &&
          mc_entity_set_position(f.victim, 8.5, 64.5, 8.5) == MC_ENTITY_OK &&
          mc_entity_set_position(owner->player, 11.5, 64.5, 12.5) == MC_ENTITY_OK);
    mc_living_entity_fields *living = &f.victim->mob->living;
    living->Y = 4;
    living->Z = 17;
    living->E = 20;
    living->aa = 3;
    living->ab = 4;
    living->ad = 7;
    living->au = 0;
    living->av = 17;
    living->T = 7;
    living->ah = false;
    f.victim->base.by = 0;
    f.victim->base.bb = f.victim->base.be = false;
    f.victim->base.yaw = 30.f;
    f.victim->base.velocity_x = .25;
    f.victim->base.velocity_y = -.375;
    f.victim->base.velocity_z = -.125;
    mc_java_random_reseed(f.victim->random, 38);
    owner->player->local_player->fields.player.g = 3;
    mc_registry_achievement_view achievements;
    mc_registry_statistic_view statistics;
    CHECK(mc_registry_classes_inspect_achievements(owner->app.registry, &achievements, NULL) ==
              MC_REGISTRY_OK &&
          mc_registry_classes_inspect_statistics(owner->app.registry, &statistics, NULL) ==
              MC_REGISTRY_OK);
    bool present = true;
    CHECK(mc_client_stats_writer_contains(owner->app.stats_writer, achievements.named[12], &present,
                                          NULL) == MC_CLIENT_STATS_OK &&
          !present && value(owner, statistics.named[20], 0) &&
          value(owner, achievements.named[13], 0));
    mc_math_classes *math = NULL;
    mc_entity_effect_error *error = calloc(1, sizeof(*error));
    f.reentry = calloc(1, sizeof(*f.reentry));
    CHECK(error != NULL && f.reentry != NULL &&
          mc_math_classes_create(SIZE_MAX, &math, NULL) == MC_MATH_CLASS_OK);
    const mc_world_access_callbacks callbacks = {.sound = fatal_sound, .entity_added = fatal_added};
    mc_world_access *access = NULL;
    CHECK(mc_world_access_create(&callbacks, &f, &access, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(owner->world, access, NULL) == MC_WORLD_STORAGE_OK);
    const size_t player_holds = owner->player->holds, victim_holds = f.victim->holds;
    bool result = false;
    const mc_entity_status status = mc_entity_effect_damage_with_math(
        f.victim, owner->app.registry, math, owner->player, 4, &result, error);
    if (status != MC_ENTITY_OK)
        fprintf(stderr, "Fatal Player/Monster: %s\n", error->operation);
    CHECK(status == MC_ENTITY_OK && result && error->death.returned &&
          error->death.score_callback_returned && error->death.kill_callback_returned &&
          error->death.score == 7 && !error->death.pending && !f.bad_callback && f.sounds == 1 &&
          f.added == (kind == MC_ENTITY_CLASS_KIND_ZOMBIE ? 1u : 2u) && living->Y == 0 &&
          living->ah && !f.victim->base.be && f.victim->holds == victim_holds &&
          owner->player->holds == player_holds + 1 &&
          f.victim->mob->creature_target == owner->player &&
          owner->player->local_player->fields.player.g == 10 &&
          value(owner, statistics.named[20], 1) && value(owner, achievements.named[13], 0));
    int32_t mob_kills = -1, kill_enemy = -1;
    CHECK(mc_client_stats_writer_value(owner->app.stats_writer, statistics.named[20], &mob_kills,
                                       NULL) == MC_CLIENT_STATS_OK &&
          mc_client_stats_writer_value(owner->app.stats_writer, achievements.named[13], &kill_enemy,
                                       NULL) == MC_CLIENT_STATS_OK);
    printf("MONSTER_DEATH {\"profile\":\"%s-fatal-player-local-38\",\"kind\":\"%s\","
           "\"mode\":\"fatal\",\"source\":\"player\",\"remote\":%s,\"seed\":38,\"result\":%s,"
           "\"dying\":%s,\"dead\":%s,\"health\":%" PRId32 ",\"random\":\"%016" PRIx64
           "\",\"items\":[",
           name, name, owner->world->fields.B ? "true" : "false", result ? "true" : "false",
           living->ah ? "true" : "false", f.victim->base.be ? "true" : "false", living->Y,
           f.victim->random->state);
    for (size_t i = 0; i < f.added; ++i) {
        mc_item_stack_fields stack;
        CHECK(mc_item_stack_fields_get(f.items[i]->stack, &stack) == MC_ITEM_STACK_OK &&
              stack.id == (kind == MC_ENTITY_CLASS_KIND_ZOMBIE ? 288
                           : i == 0                            ? 262
                                                               : 352) &&
              stack.count == 1 && stack.damage == 0);
        printf("%s[%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId32 ",%s,%zu]", i ? "," : "", stack.id,
               stack.count, stack.damage, f.items[i]->item.c,
               fatal_world_member(&f, f.items[i]) ? "true" : "false",
               fatal_members(&f, f.items[i]));
    }
    printf("],\"sounds\":[\"%s\"],\"player\":[%" PRId32 ",%" PRId32 ",%" PRId32 "]}\n", f.sound,
           owner->player->local_player->fields.player.g, mob_kills, kill_enemy);
    CHECK(fflush(stdout) == 0);
    bool removed = false;
    CHECK(mc_world_runtime_remove_world_access(owner->world, access, &removed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          removed && mc_world_access_free(access, NULL) == MC_WORLD_STORAGE_OK &&
          mc_mob_retire(&f.victim, NULL) == MC_MOB_OK && owner->player->holds == player_holds &&
          mc_math_classes_release(&math, NULL) == MC_MATH_CLASS_OK &&
          mc_world_runtime_retire_entity_containers(owner->world, NULL) == MC_WORLD_STORAGE_OK &&
          mc_chunk_pending_clear(acquisition->pending) == MC_CHUNK_ACQUIRE_OK &&
          mc_chunk_cache_init(acquisition->cache, acquisition->cache->entries,
                              acquisition->cache->capacity, acquisition->cache->insertions,
                              acquisition->cache->insertion_capacity) &&
          mc_world_store_retire_chunk(owner->store, mc_world_chunk_state(f.chunk), NULL) ==
              MC_WORLD_STORAGE_OK);
    free(error);
    free(f.reentry);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3 || CD(argv[2]) != 0 || !mc_math_trig_init(&trig))
        return 2;
    fixture *f = calloc(1, sizeof(*f));
    if (f == NULL || !setup(f, argv[1], "monster-kill", true) || !kill_cases(f) || !teardown(f) ||
        !setup(f, argv[1], "zombie-player", true) ||
        !fatal_case(f, MC_ENTITY_CLASS_KIND_ZOMBIE, "uz") || !teardown(f) ||
        !setup(f, argv[1], "skeleton-player", true) ||
        !fatal_case(f, MC_ENTITY_CLASS_KIND_SKELETON, "fr") || !teardown(f))
        return 1;
    free(f);
    printf("Player Monster kill: %zu checks\n", checks);
    return 0;
}
