/* Reuse the genuine App/Session/World fixture; its observation main is not run. */
#define main mc_player_statistics_observation_main
#include "test_player_dispatch.c"
#undef main
#include "../../../src/world/runtime/runtime_internal.h"
#include "../../../src/world/storage/objects_internal.h"
#include "mc/block/registry/step_sound.h"
#include "mc/entity/living/travel.h"
#include "mc/entity/movement/effects.h"
#include "mc/entity/tick/environment.h"
#include "mc/world/runtime/mutations.h"
#include "mc/world/runtime/world_access.h"

static bool value(fixture *, size_t index, int32_t expected);

typedef struct damage_sound_observer {
    mc_world_runtime *world;
    size_t sounds;
    bool valid;
} damage_sound_observer;

static mc_world_storage_status damage_sound(void *context, mc_world_runtime *world,
                                            const mc_registry_string *name, double x, double y,
                                            double z, float volume, float pitch,
                                            mc_world_storage_error *error) {
    damage_sound_observer *o = context;
    (void)x;
    (void)y;
    (void)z;
    (void)error;
    const char expected[] = "random.hurt";
    o->valid = o->valid && world == o->world && name != NULL && name->length == 11 &&
               volume == 1.0f && pitch > 0;
    if (name != NULL && name->length == 11)
        for (size_t i = 0; i < 11; ++i)
            o->valid = o->valid && name->units[i] == (uint16_t)expected[i];
    ++o->sounds;
    return MC_WORLD_STORAGE_OK;
}

static bool burning_caller(fixture *f) {
    mc_entity *p = f->player;
    CHECK(mc_entity_set_position(p, 8, 200, 8) == MC_ENTITY_OK);
    CHECK(p->local_player->fields.living.Y == 20 && p->base.by == 0);
    /* Authored burning-timer input, then two genuine SN.U calls. The second
     * follows the first naturally, with no timer/health/Random reset. */
    p->base.bv = 20;
    damage_sound_observer observer = {.world = f->world, .valid = true};
    const mc_world_access_callbacks callbacks = {.sound = damage_sound};
    mc_world_access *access = NULL;
    CHECK(mc_world_access_create(&callbacks, &observer, &access, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_add_world_access(f->world, access, NULL) == MC_WORLD_STORAGE_OK);
    mc_entity_tick_error *tick = malloc(sizeof(*tick));
    CHECK(tick != NULL);
    const size_t holds = p->holds;
    CHECK(mc_entity_tick_base_body_in_context(p, f->app.registry, NULL, tick) == MC_ENTITY_OK);
    CHECK(p->local_player->fields.living.Y == 19 && p->base.bv == 19 && observer.valid &&
          observer.sounds == 1 && value(f, 18, 1));
    CHECK(mc_entity_tick_base_body_in_context(p, f->app.registry, NULL, tick) == MC_ENTITY_OK);
    CHECK(p->local_player->fields.living.Y == 19 && p->base.bv == 18 && observer.valid &&
          observer.sounds == 1 && value(f, 18, 1) && p->holds == holds);
    bool removed = false;
    CHECK(mc_world_runtime_remove_world_access(f->world, access, &removed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          removed);
    CHECK(mc_world_access_free(access, NULL) == MC_WORLD_STORAGE_OK);
    free(tick);
    return true;
}

typedef struct landing_observer {
    fixture *owner;
    size_t hurt, steps;
    bool valid, move_on_hurt, null_on_hurt, fail_step;
    mc_step_sound_name retained;
} landing_observer;

static bool sound_name(const mc_registry_string *name, const char *text) {
    if (name == NULL || name->length != strlen(text))
        return false;
    for (size_t i = 0; i < name->length; ++i)
        if (name->units[i] != (uint16_t)(unsigned char)text[i])
            return false;
    return true;
}

static mc_world_storage_status landing_sound(void *context, mc_world_runtime *world,
                                             const mc_registry_string *name, double x, double y,
                                             double z, float volume, float pitch,
                                             mc_world_storage_error *error) {
    landing_observer *o = context;
    mc_entity *p = o->owner->player;
    o->valid = o->valid && world == o->owner->world;
    if (sound_name(name, "random.hurt")) {
        ++o->hurt;
        if (o->move_on_hurt)
            o->valid = o->valid && mc_entity_set_position(p, 8.5, 102, 8.5) == MC_ENTITY_OK;
        if (o->null_on_hurt)
            p->base.world = NULL;
    } else {
        o->valid = o->valid && sound_name(name, "step.stone") && x == 8.5 && z == 8.5 &&
                   y == 102.0 - (double)p->base.offset && volume == .5f && pitch == .75f;
        if (o->retained.string == NULL)
            o->valid = o->valid &&
                       mc_registry_step_sound_name_retain(o->owner->app.registry, name,
                                                          &o->retained, NULL) == MC_REGISTRY_OK;
        else
            o->valid = o->valid && name != o->retained.string;
        ++o->steps;
        if (o->fail_step) {
            *error = (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR,
                                              "Authored landing sound refusal", 0};
            return error->status;
        }
    }
    return MC_WORLD_STORAGE_OK;
}

static bool landing_caller(fixture *f) {
    mc_entity *p = f->player;
    /* A real generated/cached Chunk and quiet setter supply the landing Block.
     * The hurt callback moves the same Player before the actual post-hit query. */
    bool changed = false;
    mc_world_mutation_error *mutation = malloc(sizeof(*mutation));
    mc_entity_effect_error *effect = malloc(sizeof(*effect));
    CHECK(mutation != NULL && effect != NULL);
    CHECK(mc_world_runtime_block_set_quiet(f->world, 8, 100, 8, 1, &changed, mutation) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_entity_set_position(p, 8.5, 200, 8.5) == MC_ENTITY_OK);
    landing_observer observer = {.owner = f, .valid = true, .move_on_hurt = true};
    const mc_world_access_callbacks callbacks = {.sound = landing_sound};
    mc_world_access *access = NULL;
    CHECK(mc_world_access_create(&callbacks, &observer, &access, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_add_world_access(f->world, access, NULL) == MC_WORLD_STORAGE_OK);
    const size_t holds = p->holds;
    p->base.bk = 4.25f;
    CHECK(mc_entity_effect_fall_state_in_context(p, f->app.registry, 0, true, effect) ==
          MC_ENTITY_OK);
    CHECK(p->local_player->fields.living.Y == 18 && p->base.bk == 0 && observer.valid &&
          observer.hurt == 1 && observer.steps == 1 && effect->living_bytecode == 108 &&
          p->base.x == 8.5 && p->base.y == 102 && p->base.z == 8.5 &&
          effect->living_damage.player_health.returned && effect->living_damage.sound_returned &&
          value(f, 8, 425) && value(f, 18, 2));
    CHECK(mc_entity_effect_fall_in_context(p, f->app.registry, 4.25f, effect) == MC_ENTITY_OK);
    CHECK(p->local_player->fields.living.Y == 18 && observer.valid && observer.hurt == 1 &&
          observer.steps == 2 && value(f, 8, 850) && value(f, 18, 4));
    observer.fail_step = true;
    CHECK(mc_entity_effect_fall_in_context(p, f->app.registry, 4.25f, effect) ==
          MC_ENTITY_WORLD_FAILED);
    CHECK(effect->living_bytecode == 105 && effect->cause == MC_ENTITY_EFFECT_CAUSE_WORLD &&
          effect->world.status == MC_WORLD_STORAGE_IO_ERROR && observer.valid &&
          observer.hurt == 1 && observer.steps == 3 && p->holds == holds && value(f, 8, 1275) &&
          value(f, 18, 6));
    observer.fail_step = false;
    CHECK(mc_registry_step_sound_name_release(&observer.retained, NULL) == MC_REGISTRY_OK);
    /* Above the World returns actual air and must not dispatch a landing sound. */
    CHECK(mc_entity_set_position(p, 8.5, 200, 8.5) == MC_ENTITY_OK);
    CHECK(mc_entity_effect_fall_in_context(p, f->app.registry, 4.25f, effect) == MC_ENTITY_OK);
    CHECK(observer.hurt == 1 && observer.steps == 3 && value(f, 8, 1700) && value(f, 18, 8));
    /* Independent authored full-hit input: completed hurt sound changes World
     * to null. Landing fails at61 after preserving damage/statistic stores. */
    p->base.by = 0;
    observer.null_on_hurt = true;
    p->base.bk = 4.25f;
    const mc_entity_status no_world =
        mc_entity_effect_fall_state_in_context(p, f->app.registry, 0, true, effect);
    p->base.world = f->world;
    CHECK(no_world == MC_ENTITY_STATE_ERROR && effect->living_bytecode == 61 &&
          effect->cause == MC_ENTITY_EFFECT_CAUSE_NULL_REFERENCE &&
          p->local_player->fields.living.Y == 16 && p->base.bk == 4.25f && observer.valid &&
          observer.hurt == 2 && observer.steps == 3 && value(f, 8, 2125) && value(f, 18, 10) &&
          p->holds == holds);
    p->base.bk = 0; /* Retire the authored failed-fall input before normal teardown. */
    bool removed = false;
    CHECK(mc_world_runtime_remove_world_access(f->world, access, &removed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          removed);
    CHECK(mc_world_access_free(access, NULL) == MC_WORLD_STORAGE_OK);
    free(mutation);
    free(effect);
    /* This profile created a real Chunk; the imported no-Chunk fixture has
     * nothing to retire from its native cache descriptors. Retire the same
     * containers/cache/Chunk graph here before that fixture's normal teardown. */
    CHECK(mc_world_runtime_retire_entity_containers(f->world, NULL) == MC_WORLD_STORAGE_OK);
    mc_chunk_cache *cache = f->world->options.cache;
    CHECK(mc_chunk_cache_validate(cache) && mc_chunk_pending_validate(f->world->options.pending));
    CHECK(mc_chunk_pending_clear(f->world->options.pending) == MC_CHUNK_ACQUIRE_OK);
    CHECK(mc_chunk_cache_init(cache, cache->entries, cache->capacity, cache->insertions,
                              cache->insertion_capacity));
    CHECK(mc_world_runtime_retire_world(f->world, NULL) == MC_WORLD_STORAGE_OK);
    while (mc_world_store_chunk_count(f->store) != 0) {
        mc_chunk_state *chunk = NULL;
        CHECK(mc_world_store_chunk_at(f->store, 0, &chunk, NULL) == MC_WORLD_STORAGE_OK);
        CHECK(mc_world_store_retire_chunk(f->store, chunk, NULL) == MC_WORLD_STORAGE_OK);
    }
    return true;
}

static bool value(fixture *f, size_t index, int32_t expected) {
    mc_registry_statistic_view view;
    int32_t actual = INT32_MIN;
    CHECK(mc_registry_classes_inspect_statistics(f->app.registry, &view, NULL) == MC_REGISTRY_OK);
    CHECK(mc_client_stats_writer_value(f->app.stats_writer, view.named[index], &actual, NULL) ==
          MC_CLIENT_STATS_OK);
    CHECK(actual == expected);
    return true;
}

static bool callsites(fixture *f) {
    mc_entity *p = f->player;
    const mc_player_travel_context context = {.code_registry = f->app.registry, .trig = &trig};
    mc_player_travel_error *travel = malloc(sizeof(*travel));
    mc_entity_effect_error *effect = malloc(sizeof(*effect));
    CHECK(travel != NULL && effect != NULL);
    const size_t holds = p->holds;
    CHECK(mc_player_jump(p, &context, travel) == MC_ENTITY_OK);
    CHECK(p->base.velocity_y == 0.41999998688697815 && travel->cause == MC_PLAYER_TRAVEL_NONE);
    CHECK(value(f, 15, 1));
    CHECK(mc_player_jump(p, &context, travel) == MC_ENTITY_OK && value(f, 15, 2));

    /* Above the real World height, block/material queries naturally return air.
     * This uses the constructed World, not a replacement block callback. */
    CHECK(mc_entity_set_position(p, 8, 200, 8) == MC_ENTITY_OK);
    p->base.aX = true;
    p->base.bx = false;
    CHECK(mc_player_movement_statistics(p, &context, 3, 0, 4, travel) == MC_ENTITY_OK);
    CHECK(value(f, 6, 500));
    p->base.aX = false;
    CHECK(mc_player_movement_statistics(p, &context, .26, 0, 0, travel) == MC_ENTITY_OK);
    CHECK(value(f, 10, 26));
    p->base.bx = true;
    CHECK(mc_player_movement_statistics(p, &context, 3, 7, 4, travel) == MC_ENTITY_OK);
    CHECK(value(f, 7, 500));

    /* Complete actual gs travel, including its post-move statistic dispatch. */
    p->base.bq = true;
    p->base.velocity_x = 1;
    p->base.velocity_y = p->base.velocity_z = 0;
    CHECK(mc_player_travel(p, &context, 0, 0, travel) == MC_ENTITY_OK);
    CHECK(p->base.x == 9 && p->base.y == 200 && p->base.z == 8);
    CHECK(value(f, 7, 600));

    CHECK(mc_entity_effect_fall_in_context(p, f->app.registry, 2.5f, effect) == MC_ENTITY_OK);
    CHECK(value(f, 8, 250));
    /* A completed statistic must continue into the actual passenger callback. */
    p->base.aG = p;
    const mc_entity_status passenger =
        mc_entity_effect_fall_in_context(p, f->app.registry, 2.5f, effect);
    p->base.aG = NULL;
    CHECK(passenger == MC_ENTITY_STATE_ERROR &&
          effect->cause == MC_ENTITY_EFFECT_CAUSE_UNSUPPORTED_RELATIONSHIP &&
          effect->fall_callback_bytecode == 12 && effect->statistic_amount == 250);
    CHECK(value(f, 8, 500));

    /* The real statistic precedes virtual armor and the inherited hurt body. */
    bool damaged = false;
    const int32_t health = p->local_player->fields.living.Y;
    CHECK(mc_entity_effect_damage_in_context(p, f->app.registry, NULL, 3, &damaged, effect) ==
          MC_ENTITY_OK);
    CHECK(damaged && p->local_player->fields.living.Y == health - 3 &&
          effect->cause == MC_ENTITY_EFFECT_CAUSE_NONE && effect->living_bytecode == 363 &&
          effect->source_bytecode == 161 && effect->living_damage.living_returned &&
          effect->living_damage.player_health.returned && effect->living_damage.sound_returned);
    CHECK(value(f, 18, 3));

    p->local_player->fields.minecraft = NULL;
    const mc_entity_status no_app = mc_player_jump(p, &context, travel);
    p->local_player->fields.minecraft = &f->app;
    CHECK(no_app == MC_ENTITY_STATE_ERROR && travel->cause == MC_PLAYER_TRAVEL_STATISTIC &&
          travel->player_bytecode == 9 && travel->statistic_bytecode == 85 &&
          travel->statistic_error.status == MC_PLAYER_STATISTIC_NULL_REFERENCE &&
          travel->statistic_error.site.pc == 85 && travel->statistic_amount == 1);
    CHECK(value(f, 15, 2));
    mc_client_stats_writer *writer = f->app.stats_writer;
    f->app.stats_writer = NULL;
    const mc_entity_status no_writer =
        mc_entity_effect_fall_in_context(p, f->app.registry, 2.5f, effect);
    f->app.stats_writer = writer;
    CHECK(no_writer == MC_ENTITY_STATE_ERROR &&
          effect->cause == MC_ENTITY_EFFECT_CAUSE_NULL_REFERENCE && effect->player_bytecode == 20 &&
          effect->statistic_bytecode == 90 && effect->statistic_error.site.pc == 90 &&
          effect->statistic_error.status == MC_PLAYER_STATISTIC_NULL_REFERENCE);
    CHECK(value(f, 8, 500));
    CHECK(p->holds == holds);
    free(effect);
    free(travel);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3 || CD(argv[2]) != 0 || !mc_math_trig_init(&trig)) {
        fputs("Player callsites input: resources work-directory\n", stderr);
        return 2;
    }
    fixture *f = calloc(1, sizeof(*f));
    if (f == NULL || !setup(f, argv[1], "callsites", true) || !callsites(f) || !teardown(f) ||
        !setup(f, argv[1], "burning-caller", true) || !burning_caller(f) || !teardown(f) ||
        !setup(f, argv[1], "landing-caller", true) || !landing_caller(f) || !teardown(f))
        return 1;
    free(f);
    printf("Player statistic callsites: %zu checks\n", checks);
    return 0;
}
