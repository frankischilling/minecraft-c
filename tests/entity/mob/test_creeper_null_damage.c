/* Authored component contracts, not captured-state/original observations.
 * Genuine GAME World/Chunks, canonical Creepers, normal insertion and FD tick.
 * A real eye-height solid cell reaches LS.U66; nullable health/timer/fields
 * in the separate leaf controls are declared fixture inputs only. */
int mc_creeper_damage_world_fixture_main(int argc, char **argv);
#define main mc_creeper_damage_world_fixture_main
#include "../../world/runtime/test_world_random_tick.c"
#undef main

#include "../../../src/entity/mob/internal.h"
#include "../../../src/entity/mob/references_internal.h"
#include "../../../src/world/storage/generated_internal.h"
#include "mc/entity/mob/construction.h"
#include "mc/entity/mob/tick.h"
#include "mc/entity/movement/effects.h"
#include "mc/world/runtime/entity_tick.h"

#undef CHECK
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            fprintf(stderr, "Creeper damage check%u line%d: %s\n", checks, __LINE__, #c);          \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

typedef struct creeper_fixture {
    random_fixture base;
    mc_entity *actors[16], *current;
    size_t actor_count, sounds;
    mc_world_access *observer;
    bool bad_callback, fail_sound;
    const mc_registry_string *last_name;
    float volume, pitch;
    int32_t callback_health, callback_fire, callback_hurt;
} creeper_fixture;

static mc_world_storage_status creeper_sound(void *context, mc_world_runtime *world,
                                             const mc_registry_string *name, double x, double y,
                                             double z, float volume, float pitch,
                                             mc_world_storage_error *error) {
    creeper_fixture *f = context;
    const char *expected =
        f->current != NULL && f->current->mob->living.Y <= 0 ? "mob.creeperdeath" : "mob.creeper";
    const size_t expected_length = strlen(expected);
    mc_registry_string_view text = {0};
    mc_entity *alias = f->current;
    mc_mob_error retirement;
    ++f->sounds;
    if (world != f->base.world || alias == NULL ||
        alias->class_literal->kind != MC_ENTITY_CLASS_KIND_CREEPER ||
        !mc_registry_string_read(name, &text) || text.length != expected_length ||
        x != alias->base.x || y != alias->base.y - (double)alias->base.offset ||
        z != alias->base.z || volume != 1.0f ||
        mc_world_access_free(f->observer, NULL) != MC_WORLD_STORAGE_BUSY ||
        mc_mob_retire(&alias, &retirement) != MC_MOB_BUSY || alias != f->current)
        f->bad_callback = true;
    if (!f->bad_callback)
        for (size_t i = 0; i < text.length; ++i)
            if (text.units[i] != (uint16_t)expected[i])
                f->bad_callback = true;
    f->last_name = name;
    f->volume = volume;
    f->pitch = pitch;
    f->callback_health = f->current->mob->living.Y;
    f->callback_fire = f->current->base.bv;
    f->callback_hurt = f->current->mob->living.aa;
    if (f->fail_sound) {
        *error = (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR,
                                          "authored Creeper sound refusal", 0};
        return error->status;
    }
    *error = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

static bool actor(creeper_fixture *f, mc_entity_class_kind kind, bool null_world, mc_entity **out) {
    const mc_entity_class_literal *literal = NULL;
    mc_mob_error e;
    CHECK(f->actor_count < COUNT(f->actors) &&
          mc_entity_class_literal_get(f->base.entities, kind, &literal) == MC_ENTITY_OK &&
          mc_mob_allocate(f->base.entities, f->base.registry, literal, out, &e) == MC_MOB_OK &&
          mc_mob_construct(*out, null_world ? NULL : f->base.world, &e) == MC_MOB_OK &&
          mc_entity_set_position(*out, -1.6293702524256486, 64, -119.25577395262147) ==
              MC_ENTITY_OK);
    CHECK((*out)->class_literal == literal && (*out)->phase == MC_ENTITY_READY);
    f->actors[f->actor_count++] = *out;
    return true;
}

static bool prepare(creeper_fixture *f, const char *resources, const char *directory) {
    CHECK(setup(&f->base, resources, directory));
    /* Declared difficulty input: keep the later genuine GZ peaceful-despawn
     * branch from making the returned live-Creeper assertion inapplicable. */
    f->base.world->fields.q = 1;
    for (int32_t x = -3; x <= 1; ++x)
        for (int32_t z = -10; z <= -6; ++z) {
            uint8_t *blocks = calloc(MC_CHUNK_BLOCKS, 1);
            mc_world_chunk *object = NULL;
            CHECK(blocks != NULL);
            /* Actual first SN.L sample for the independently declared actor
             * position intersects this ordinary opaque Stone cell. */
            if (x == -1 && z == -8)
                blocks[(14u * 16u + 8u) * 128u + 65u] = 1;
            CHECK(mc_world_store_create_generated(f->base.store, f->base.world, 0, x, z, &blocks,
                                                  MC_CHUNK_BLOCKS, &object,
                                                  NULL) == MC_WORLD_STORAGE_OK &&
                  blocks == NULL);
            mc_chunk_state *state = mc_world_chunk_state(object);
            CHECK(state != NULL && mc_chunk_cache_publish(f->base.provider.cache, x, z, state));
            f->base.owned[f->base.owned_count++] = state;
        }
    const mc_world_access_callbacks callbacks = {.sound = creeper_sound};
    CHECK(mc_world_access_create(&callbacks, f, &f->observer, NULL) == MC_WORLD_STORAGE_OK);
    return true;
}

/* The genuine FD update enters GB.w_ -> GZ -> LS.w_ -> LS.U, whose actual
 * suffocation query sees the authored eye-height Stone and invokes GZ at66. */
static bool world_suffocation_regression(creeper_fixture *f) {
    mc_entity *p = NULL;
    CHECK(actor(f, MC_ENTITY_CLASS_KIND_CREEPER, false, &p));
    const int32_t health = p->mob->living.Y, ticks = p->base.bt;
    const size_t holds = p->holds, sounds = f->sounds;
    const mc_entity_class_literal *literal = p->class_literal;
    f->current = p;
    bool inserted = false;
    CHECK(mc_world_runtime_insert_entity(f->base.world, p, &inserted, NULL) ==
              MC_WORLD_STORAGE_OK &&
          inserted && p->base.bF && p->holds > holds && f->base.provider.cache->count == 25);
    /* Insertion genuinely returns with no WorldAccess listener. Register the
     * sound-only contract at its actual sound-use scope, after publication. */
    CHECK(mc_world_runtime_add_world_access(f->base.world, f->observer, NULL) ==
          MC_WORLD_STORAGE_OK);
    mc_world_entity_tick_error *e = calloc(1, sizeof(*e));
    CHECK(e != NULL);
    const mc_world_entity_tick_context context = {
        .math = f->base.math,
        .player = {.travel = {.code_registry = f->base.registry,
                              .trig = mc_math_classes_table(f->base.math)}}};
    const mc_world_storage_status status =
        mc_world_runtime_update_entity(f->base.world, p, true, &context, NULL, e);
    if (status != MC_WORLD_STORAGE_OK) {
        const mc_mob_tick_error *m = &e->child_error.mob;
        fprintf(stderr, "Creeper actual FD%u Mob %s.%s:%u child%d effect=%s\n", e->bytecode,
                m->site.class_name ? m->site.class_name : "none",
                m->site.method ? m->site.method : "none", m->site.pc, (int)m->child,
                m->detail.environment.effect.operation ? m->detail.environment.effect.operation
                                                       : "none");
    }
    /* The later real SN.b nonburning branch stores -bu at1971. Constructor
     * bu is1; callback-fire below observes the earlier LS.U66 value0. */
    CHECK(status == MC_WORLD_STORAGE_OK && p->class_literal == literal && !p->base.be &&
          p->base.bt == ticks + 1 && p->base.bu == 1 &&
          p->base.bv == mc_java_i32_sub(0, p->base.bu) && p->mob->living.Y == health - 1 &&
          p->mob->living.aa == 9 && p->base.by == 19 && f->sounds == sounds + 1 &&
          !f->bad_callback && f->callback_health == health - 1 && f->callback_fire == 0 &&
          f->callback_hurt == 10 && f->last_name != NULL);
    free(e);
    return true;
}

static uint32_t next_bits(uint64_t *state, unsigned int bits) {
    *state = (*state * UINT64_C(0x5deece66d) + 11u) & UINT64_C(0xffffffffffff);
    return (uint32_t)(*state >> (48u - bits));
}

static bool fresh_and_repeated(creeper_fixture *f) {
    mc_entity *p = NULL, *target = NULL;
    CHECK(actor(f, MC_ENTITY_CLASS_KIND_CREEPER, false, &p) &&
          actor(f, MC_ENTITY_CLASS_KIND_COW, false, &target));
    mc_entity_error target_error;
    CHECK(mc_mob_creature_target_store(p, target, &target_error) == MC_ENTITY_OK);
    const size_t target_holds = target->holds, holds = p->holds, sounds = f->sounds;
    const mc_entity_class_literal *literal = p->class_literal;
    const int32_t health = p->mob->living.Y, id = p->base.id;
    const uint64_t own_before = p->random->state;
    uint64_t own = own_before, shared = atomic_load(&f->base.profile->math->state);
    const uint64_t high = next_bits(&shared, 26), low = next_bits(&shared, 27);
    const double direction = (double)((high << 27) | low) / 9007199254740992.0;
    const float a = (float)next_bits(&own, 24) / 16777216.0f;
    const float b = (float)next_bits(&own, 24) / 16777216.0f;
    volatile float difference = a - b;
    volatile float scaled = difference * .2f;
    const float pitch = scaled + 1.0f;
    f->current = p;
    bool answer = false;
    mc_entity_effect_error e;
    CHECK(mc_entity_effect_damage_in_context(p, f->base.registry, NULL, 1, &answer, &e) ==
              MC_ENTITY_OK &&
          answer && e.living_damage.returned && e.living_damage.living_returned &&
          e.living_damage.wrapper_pc == 26 && e.living_damage.living_pc == 363 &&
          e.living_damage.full_hit && e.living_damage.shared_calls == 1 &&
          e.living_damage.private_calls == 2 && e.living_damage.shared_value == direction &&
          float_bits(e.living_damage.pitch) == float_bits(pitch) &&
          e.living_damage.sound_returned && e.living_damage.sound_literal != NULL &&
          strcmp(e.living_damage.sound_literal, "mob.creeper") == 0 &&
          e.living_damage.sound_name_identity == (uintptr_t)f->last_name &&
          p->class_literal == literal && p->base.id == id && p->mob->living.Y == health - 1 &&
          p->mob->living.Z == health && p->mob->living.au == 1 && p->base.by == 20 &&
          p->mob->living.aa == 10 && p->mob->living.ab == 10 && p->holds == holds &&
          p->random->state == own && atomic_load(&f->base.profile->math->state) == shared &&
          f->sounds == sounds + 1 && !f->bad_callback && p->mob->creature_target == target &&
          target->holds == target_holds);
    CHECK(mc_entity_effect_damage_in_context(p, f->base.registry, NULL, 1, &answer, &e) ==
              MC_ENTITY_OK &&
          !answer && e.living_damage.returned && e.living_damage.wrapper_pc == 40 &&
          e.living_damage.living_pc == 59 && e.living_damage.shared_calls == 0 &&
          e.living_damage.private_calls == 0 && p->mob->living.Y == health - 1 &&
          p->random->state == own && atomic_load(&f->base.profile->math->state) == shared &&
          f->sounds == sounds + 1);
    CHECK(mc_entity_effect_damage_in_context(p, f->base.registry, NULL, 3, &answer, &e) ==
              MC_ENTITY_OK &&
          answer && !e.living_damage.full_hit && e.living_damage.returned &&
          e.living_damage.wrapper_pc == 26 && e.living_damage.living_pc == 363 &&
          e.living_damage.shared_calls == 0 && e.living_damage.private_calls == 0 &&
          p->mob->living.Y == health - 3 && p->mob->living.au == 3 && p->random->state == own &&
          atomic_load(&f->base.profile->math->state) == shared && p->holds == holds &&
          target->holds == target_holds && f->sounds == sounds + 1);
    return true;
}

static bool gates_and_frontiers(creeper_fixture *f) {
    mc_entity *p = NULL;
    mc_entity_effect_error e;
    bool answer = true;
    CHECK(actor(f, MC_ENTITY_CLASS_KIND_CREEPER, false, &p));
    f->current = p;
    const int32_t health = p->mob->living.Y;
    const uint64_t own = p->random->state, shared = atomic_load(&f->base.profile->math->state);
    f->base.world->fields.B = true;
    p->mob->living.av = 17;
    const mc_entity_status remote =
        mc_entity_effect_damage_in_context(p, NULL, NULL, 1, &answer, &e);
    f->base.world->fields.B = false;
    CHECK(remote == MC_ENTITY_OK && !answer && e.living_damage.living_pc == 11 &&
          p->mob->living.av == 17 && p->mob->living.Y == health && p->random->state == own &&
          atomic_load(&f->base.profile->math->state) == shared);
    p->mob->living.Y = 0;
    CHECK(mc_entity_effect_damage_in_context(p, NULL, NULL, 1, &answer, &e) == MC_ENTITY_OK &&
          !answer && e.living_damage.living_pc == 25 && p->mob->living.av == 0 &&
          p->random->state == own);
    CHECK(actor(f, MC_ENTITY_CLASS_KIND_CREEPER, true, &p));
    answer = true;
    CHECK(mc_entity_effect_damage_in_context(p, f->base.registry, NULL, 1, &answer, &e) ==
              MC_ENTITY_STATE_ERROR &&
          answer && e.living_damage.source_exception && !e.living_damage.native_policy &&
          e.living_damage.living_pc == 4 && p->base.world == NULL);

    CHECK(actor(f, MC_ENTITY_CLASS_KIND_CREEPER, false, &p));
    answer = true;
    CHECK(mc_entity_effect_damage_in_context(p, NULL, NULL, 1, &answer, &e) ==
              MC_ENTITY_STATE_ERROR &&
          answer && !e.living_damage.returned && e.living_damage.native_policy &&
          e.living_damage.living_pc == 332 &&
          strcmp(e.living_damage.child_site.class_name, "gb") == 0 &&
          strcmp(e.living_damage.child_site.method, "j_") == 0 &&
          e.living_damage.child_site.pc == 0);

    CHECK(actor(f, MC_ENTITY_CLASS_KIND_CREEPER, false, &p));
    f->current = p;
    mc_java_random *allocation = p->random;
    p->random = NULL; /* Declared nullable source input, observed before final disposal. */
    answer = true;
    const mc_entity_status null_random =
        mc_entity_effect_damage_in_context(p, f->base.registry, NULL, 1, &answer, &e);
    const bool preserved = p->random == NULL && answer && e.living_damage.source_exception &&
                           !e.living_damage.native_policy && e.living_damage.living_pc == 343 &&
                           e.living_damage.private_attempts == 1 &&
                           e.living_damage.private_calls == 0 &&
                           e.living_damage.sound_name_returned &&
                           strcmp(e.living_damage.sound_literal, "mob.creeper") == 0;
    /* No call is retried. Restore only the owned allocation pointer for its
     * eventual destructor, after all failed-call observations are captured. */
    p->random = allocation;
    CHECK(null_random == MC_ENTITY_STATE_ERROR && preserved);

    CHECK(actor(f, MC_ENTITY_CLASS_KIND_CREEPER, false, &p));
    f->current = p;
    answer = true;
    const int32_t before = p->mob->living.Y;
    CHECK(mc_entity_effect_damage_in_context(p, f->base.registry, p, 1, &answer, &e) ==
              MC_ENTITY_STATE_ERROR &&
          answer && !e.living_damage.entered && e.damage_source_identity == (uintptr_t)p &&
          e.damage_amount == 1 && e.damage_bytecode == 3 && p->mob->living.Y == before &&
          e.cause == MC_ENTITY_EFFECT_CAUSE_UNSUPPORTED_VIRTUAL);

    CHECK(actor(f, MC_ENTITY_CLASS_KIND_CREEPER, false, &p));
    f->current = p;
    p->mob->living.Y = 1;
    answer = true;
    const size_t sounds = f->sounds;
    CHECK(mc_entity_effect_damage_in_context(p, f->base.registry, NULL, 1, &answer, &e) ==
              MC_ENTITY_STATE_ERROR &&
          answer && !e.living_damage.returned && e.living_damage.death_invoke_reached &&
          e.living_damage.living_pc == 316 && e.living_damage.sound_returned &&
          strcmp(e.living_damage.sound_literal, "mob.creeperdeath") == 0 &&
          strcmp(e.living_damage.child_site.class_name, "gb") == 0 &&
          strcmp(e.living_damage.child_site.method, "b") == 0 &&
          e.living_damage.child_site.pc == 2 && p->mob->living.Y == 0 && !p->base.be &&
          f->sounds == sounds + 1 && !f->bad_callback &&
          e.cause == MC_ENTITY_EFFECT_CAUSE_UNSUPPORTED_VIRTUAL);

    CHECK(actor(f, MC_ENTITY_CLASS_KIND_CREEPER, false, &p));
    p->mob->living.Y = 1;
    answer = true;
    CHECK(mc_entity_effect_damage_in_context(p, NULL, NULL, 1, &answer, &e) ==
              MC_ENTITY_STATE_ERROR &&
          answer && !e.living_damage.returned && e.living_damage.living_pc == 284 &&
          strcmp(e.living_damage.child_site.class_name, "gb") == 0 &&
          strcmp(e.living_damage.child_site.method, "i") == 0 &&
          e.living_damage.child_site.pc == 0 && !e.living_damage.death_invoke_reached);

    CHECK(actor(f, MC_ENTITY_CLASS_KIND_CREEPER, false, &p));
    f->current = p;
    f->fail_sound = true;
    answer = true;
    const size_t refusal_sounds = f->sounds;
    const int32_t refusal_health = p->mob->living.Y;
    const mc_entity_status refused =
        mc_entity_effect_damage_in_context(p, f->base.registry, NULL, 1, &answer, &e);
    f->fail_sound = false;
    CHECK(refused == MC_ENTITY_WORLD_FAILED && answer && !e.living_damage.returned &&
          !e.living_damage.sound_returned && e.living_damage.living_pc == 359 &&
          e.living_damage.native_policy && !e.living_damage.source_exception &&
          e.world.cause.status == MC_WORLD_STORAGE_IO_ERROR &&
          e.world.world_access_failure.receipt == NULL && p->mob->living.Y == refusal_health - 1 &&
          f->sounds == refusal_sounds + 1 && !f->bad_callback);
    return true;
}

static bool dispose(creeper_fixture *f) {
    bool removed = false;
    CHECK(mc_world_runtime_remove_world_access(f->base.world, f->observer, &removed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          removed && mc_world_access_free(f->observer, NULL) == MC_WORLD_STORAGE_OK);
    for (size_t i = 0; i < f->actor_count; ++i)
        mc_entity_release(f->actors[i]);
    return teardown(&f->base);
}

int main(int argc, char **argv) {
    if (argc != 3)
        return 2;
    creeper_fixture *f = calloc(1, sizeof(*f));
    if (f == NULL || !prepare(f, argv[1], argv[2]) || !world_suffocation_regression(f) ||
        !fresh_and_repeated(f) || !gates_and_frontiers(f) || !dispose(f)) {
        fputs("Creeper damage failure: actual graph retained; no cleanup credit\n", stderr);
        return 1;
    }
    free(f);
    printf("PASS: %u Creeper NULL damage source-order/ownership checks\n", checks);
    return 0;
}
