/* Authored controls over genuine owners. Original Zombie observations are
 * compared separately by test_zombie_null_damage.c and its packet reader. */
int mc_monster_wolf_fixture_main(int, char **);
#define MC_WOLF_ATTACK_BODY_MAIN_NAME mc_monster_wolf_fixture_main
#include "../test_wolf_attack.c"
#undef MC_WOLF_ATTACK_BODY_MAIN_NAME
#undef main

#include "entity/mob/references_internal.h"
#include "mc/entity/mount/mount.h"
#include "mc/world/runtime/world_access.h"

#undef CHECK
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            fprintf(stderr, "Monster damage check%u line%d: %s\n", checks, __LINE__, #c);          \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

typedef struct monster_fixture {
    random_fixture base;
    mc_entity *actor, *source, *old;
    mc_world_access *observer;
    const char *sound;
    size_t sounds, source_holds, actor_holds;
    bool fail_sound, mount_in_sound, bad_callback;
} monster_fixture;

static mc_world_storage_status monster_sound(void *context, mc_world_runtime *world,
                                             const mc_registry_string *name, double x, double y,
                                             double z, float volume, float pitch,
                                             mc_world_storage_error *error) {
    monster_fixture *f = context;
    mc_registry_string_view text = {0};
    ++f->sounds;
    if (world != f->base.world || !mc_registry_string_read(name, &text) ||
        text.length != strlen(f->sound) || x != f->actor->base.x ||
        y != f->actor->base.y - (double)f->actor->base.offset || z != f->actor->base.z ||
        volume != 1.0f || !(pitch >= .8f && pitch <= 1.2f) || f->actor->mob->living.Y != 16 ||
        f->actor->mob->living.aa != 10 || f->actor->mob->creature_target != f->old ||
        f->source->holds <= f->source_holds || f->actor->holds <= f->actor_holds)
        f->bad_callback = true;
    if (!f->bad_callback)
        for (size_t i = 0; i < text.length; ++i)
            if (text.units[i] != (uint16_t)f->sound[i])
                f->bad_callback = true;
    if (f->mount_in_sound) {
        mc_entity *scope[] = {f->actor, f->source};
        if (mc_entity_mount(f->source, f->actor, scope, COUNT(scope), NULL) != MC_ENTITY_OK)
            f->bad_callback = true;
    }
    *error = (mc_world_storage_error){
        .status = f->fail_sound ? MC_WORLD_STORAGE_IO_ERROR : MC_WORLD_STORAGE_OK,
        .operation = f->fail_sound ? "authored Monster sound failure" : NULL};
    return error->status;
}

static bool reset(monster_fixture *f) {
    CHECK(mc_mob_creature_target_store(f->actor, f->old, NULL) == MC_ENTITY_OK);
    CHECK(mc_entity_set_position(f->actor, 8.5, 64.5, 8.5) == MC_ENTITY_OK &&
          mc_entity_set_position(f->source, 11.5, 64.5, 12.5) == MC_ENTITY_OK);
    mc_living_entity_fields *v = &f->actor->mob->living;
    v->Y = 20;
    v->E = 20;
    v->au = 0;
    v->aa = v->ab = 0;
    f->actor->base.by = 0;
    f->actor->base.yaw = 30.0f;
    f->actor->base.velocity_x = .25;
    f->actor->base.velocity_y = -.375;
    f->actor->base.velocity_z = -.125;
    f->actor->base.bb = false;
    f->base.world->fields.B = false;
    f->fail_sound = f->mount_in_sound = f->bad_callback = false;
    f->sounds = 0;
    f->source_holds = f->source->holds;
    f->actor_holds = f->actor->holds;
    return true;
}

static bool paths(monster_fixture *f) {
    mc_entity_effect_error e;
    bool answer = false;
    CHECK(reset(f));
    const size_t old_holds = f->old->holds;
    const uint64_t source_random = f->source->random->state;
    CHECK(mc_entity_effect_damage_with_math(f->actor, f->base.registry, f->base.math, f->source, 4,
                                            &answer, &e) == MC_ENTITY_OK &&
          answer && e.living_damage.returned && e.living_damage.wrapper_pc == 38 &&
          e.living_damage.direction_returned && e.living_damage.knockback_returned &&
          e.living_damage.private_calls == 2 && e.living_damage.shared_calls == 0 &&
          f->sounds == 1 && !f->bad_callback && f->actor->mob->living.Y == 16 &&
          f->actor->mob->creature_target == f->source &&
          f->actor->mob->creature_target_hold == f->source &&
          f->source->holds == f->source_holds + 1 && f->old->holds == old_holds - 1 &&
          f->source->random->state == source_random);
    printf("MONSTER_DAMAGE_RETURN {\"sound\":\"%s\",\"result\":true,\"health\":%" PRId32
           ",\"previous_health\":%" PRId32 ",\"resistance\":%" PRId32 ",\"last_damage\":%" PRId32
           ",\"hurt\":%" PRId32 ",\"max_hurt\":%" PRId32 ",\"angle\":\"%08" PRIx32
           "\",\"velocity\":[\"%016" PRIx64 "\",\"%016" PRIx64 "\",\"%016" PRIx64
           "\"],\"attacked\":%s,\"target_is_source\":%s}\n",
           f->sound, f->actor->mob->living.Y, f->actor->mob->living.Z, f->actor->base.by,
           f->actor->mob->living.au, f->actor->mob->living.aa, f->actor->mob->living.ab,
           float_bits(f->actor->mob->living.ac), double_bits(f->actor->base.velocity_x),
           double_bits(f->actor->base.velocity_y), double_bits(f->actor->base.velocity_z),
           f->actor->base.bb ? "true" : "false",
           f->actor->mob->creature_target == f->source ? "true" : "false");
    CHECK(mc_entity_effect_damage_with_math(f->actor, NULL, NULL, f->old, 4, &answer, &e) ==
              MC_ENTITY_OK &&
          !answer && e.living_damage.wrapper_pc == 40 && !e.living_damage.source_read &&
          f->actor->mob->creature_target == f->source && f->sounds == 1);
    /* A stronger repeated hit returns true without direction, sound or Math,
     * but must still replace the actual target with its owning occurrence. */
    CHECK(mc_entity_effect_damage_with_math(f->actor, NULL, NULL, f->old, 7, &answer, &e) ==
              MC_ENTITY_OK &&
          answer && e.living_damage.wrapper_pc == 38 && !e.living_damage.source_read &&
          f->actor->mob->living.Y == 13 && f->actor->mob->creature_target == f->old &&
          f->source->holds == f->source_holds && f->old->holds == old_holds && f->sounds == 1);

    /* Self comparison must skip even a malformed, unreached target slot. */
    f->actor->mob->creature_target = f->source;
    CHECK(mc_entity_effect_damage_with_math(f->actor, NULL, NULL, f->actor, 8, &answer, &e) ==
              MC_ENTITY_OK &&
          answer && e.living_damage.wrapper_pc == 38 &&
          f->actor->mob->creature_target == f->source &&
          f->actor->mob->creature_target_hold == f->old);
    f->actor->mob->creature_target = f->old;

    /* Native target provenance is checked at the reached store, after the
     * stronger-hit health change. Failure cannot publish a Boolean or undo it. */
    f->actor->mob->creature_target = f->source;
    answer = false;
    CHECK(mc_entity_effect_damage_with_math(f->actor, NULL, NULL, f->source, 9, &answer, &e) ==
              MC_ENTITY_STATE_ERROR &&
          !answer && e.living_damage.living_returned && !e.living_damage.returned &&
          e.living_damage.wrapper_pc == 34 && e.living_damage.health_stored &&
          f->actor->mob->living.Y == 11 && f->actor->mob->creature_target == f->source &&
          f->actor->mob->creature_target_hold == f->old && f->sounds == 1);
    f->actor->mob->creature_target = f->old;

    for (unsigned int passenger = 0; passenger < 2; ++passenger) {
        CHECK(reset(f));
        mc_entity *scope[] = {f->actor, f->source};
        mc_entity *rider = passenger ? f->source : f->actor;
        mc_entity *vehicle = passenger ? f->actor : f->source;
        CHECK(mc_entity_mount(rider, vehicle, scope, COUNT(scope), NULL) == MC_ENTITY_OK);
        f->source_holds = f->source->holds;
        f->actor_holds = f->actor->holds;
        CHECK(mc_entity_effect_damage_with_math(f->actor, f->base.registry, f->base.math, f->source,
                                                4, &answer, &e) == MC_ENTITY_OK &&
              answer && e.living_damage.wrapper_pc == 26 && !f->bad_callback &&
              f->actor->mob->creature_target == f->old);
        CHECK(mc_entity_mount(rider, NULL, scope, COUNT(scope), NULL) == MC_ENTITY_OK);
    }
    CHECK(reset(f));
    f->mount_in_sound = true;
    CHECK(mc_entity_effect_damage_with_math(f->actor, f->base.registry, f->base.math, f->source, 4,
                                            &answer, &e) == MC_ENTITY_OK &&
          answer && !f->bad_callback && e.living_damage.wrapper_pc == 26 &&
          f->actor->mob->creature_target == f->old);
    mc_entity *scope[] = {f->actor, f->source};
    CHECK(mc_entity_mount(f->source, NULL, scope, COUNT(scope), NULL) == MC_ENTITY_OK);

    CHECK(reset(f));
    f->fail_sound = true;
    answer = false;
    CHECK(mc_entity_effect_damage_with_math(f->actor, f->base.registry, f->base.math, f->source, 4,
                                            &answer, &e) != MC_ENTITY_OK &&
          !answer && !f->bad_callback && e.living_damage.health_stored &&
          !e.living_damage.returned && f->actor->mob->living.Y == 16 &&
          f->actor->mob->creature_target == f->old && f->source->holds == f->source_holds);

    CHECK(reset(f));
    f->base.world->fields.B = true;
    CHECK(mc_entity_effect_damage_with_math(f->actor, NULL, NULL, f->source, 4, &answer, &e) ==
              MC_ENTITY_OK &&
          !answer && e.living_damage.wrapper_pc == 40 && f->sounds == 0 &&
          f->actor->mob->creature_target == f->old && f->actor->mob->living.Y == 20);
    f->base.world->fields.B = false;
    CHECK(mc_mob_creature_target_store(f->actor, NULL, NULL) == MC_ENTITY_OK);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3)
        return 2;
    monster_fixture *f = calloc(1, sizeof(*f));
    if (f == NULL || !setup(&f->base, argv[1], argv[2]) ||
        !wolf_actor(&f->base, MC_ENTITY_CLASS_KIND_COW, &f->source) ||
        !wolf_actor(&f->base, MC_ENTITY_CLASS_KIND_PIG, &f->old))
        return 1;
    const mc_world_access_callbacks callbacks = {.sound = monster_sound};
    if (mc_world_access_create(&callbacks, f, &f->observer, NULL) != MC_WORLD_STORAGE_OK ||
        mc_world_runtime_add_world_access(f->base.world, f->observer, NULL) != MC_WORLD_STORAGE_OK)
        return 1;
    const mc_entity_class_kind kinds[] = {
        MC_ENTITY_CLASS_KIND_ZOMBIE, MC_ENTITY_CLASS_KIND_SKELETON, MC_ENTITY_CLASS_KIND_CREEPER,
        MC_ENTITY_CLASS_KIND_SPIDER};
    const char *sounds[] = {"mob.zombiehurt", "mob.skeletonhurt", "mob.creeper", "mob.spider"};
    for (size_t i = 0; i < COUNT(kinds); ++i) {
        f->sound = sounds[i];
        if (!wolf_actor(&f->base, kinds[i], &f->actor) || !paths(f) ||
            mc_mob_retire(&f->actor, NULL) != MC_MOB_OK)
            return 1;
    }
    bool removed = false;
    if (mc_world_runtime_remove_world_access(f->base.world, f->observer, &removed, NULL) !=
            MC_WORLD_STORAGE_OK ||
        !removed || mc_world_access_free(f->observer, NULL) != MC_WORLD_STORAGE_OK ||
        mc_mob_retire(&f->source, NULL) != MC_MOB_OK || mc_mob_retire(&f->old, NULL) != MC_MOB_OK ||
        !teardown(&f->base))
        return 1;
    free(f);
    printf("PASS: %u Monster damage source-order/ownership checks\n", checks);
    return 0;
}
