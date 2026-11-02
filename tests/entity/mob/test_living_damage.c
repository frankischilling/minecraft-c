/* Authored actual-owner damage controls, not captured original gameplay.
 * The included Wolf/World helpers construct genuine owners; their main and
 * historical frontier assertions are not executed by this fixture. */
int mc_living_wolf_fixture_main(int, char **);
#define MC_WOLF_ATTACK_BODY_MAIN_NAME mc_living_wolf_fixture_main
#include "../test_wolf_attack.c"
#undef MC_WOLF_ATTACK_BODY_MAIN_NAME
#undef main

#include "mc/world/runtime/world_access.h"

#undef CHECK
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            fprintf(stderr, "Living damage check%u line%d: %s\n", checks, __LINE__, #c);           \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

typedef struct living_fixture {
    random_fixture base;
    mc_entity *wolf, *sheep;
    mc_world_access *observer;
    size_t sounds, target_holds, source_holds;
    bool fail_sound, bad_callback;
    int32_t callback_health, callback_hurt, callback_resistance;
    double callback_velocity[3];
    float volume, pitch;
    const mc_registry_string *name;
    mc_entity *current_source;
    const char *expected_sound;
    float expected_volume;

    struct {
        char name[48];
        double position[3];
        float volume, pitch;
    } sound_rows[8];

    size_t sound_rows_count;
} living_fixture;

static mc_world_storage_status living_sound(void *context, mc_world_runtime *world,
                                            const mc_registry_string *name, double x, double y,
                                            double z, float volume, float pitch,
                                            mc_world_storage_error *error) {
    living_fixture *f = context;
    const char *expected = f->expected_sound == NULL ? "mob.sheep" : f->expected_sound;
    mc_registry_string_view text = {0};
    ++f->sounds;
    mc_entity *target = f->sheep, *source = f->current_source;
    mc_mob_error retirement;
    mc_entity *alias = target;
    if (world != f->base.world || target == NULL || !mc_registry_string_read(name, &text) ||
        text.length != strlen(expected) || x != target->base.x ||
        y != target->base.y - (double)target->base.offset || z != target->base.z ||
        volume != f->expected_volume || target->holds <= f->target_holds ||
        (source != NULL && source->holds <= f->source_holds) ||
        mc_world_access_free(f->observer, NULL) != MC_WORLD_STORAGE_BUSY ||
        mc_mob_retire(&alias, &retirement) != MC_MOB_BUSY || alias != target)
        f->bad_callback = true;
    if (!f->bad_callback)
        for (size_t i = 0; i < text.length; ++i)
            if (text.units[i] != (uint16_t)expected[i])
                f->bad_callback = true;
    f->name = name;
    if (f->sound_rows_count >= COUNT(f->sound_rows) || text.length >= sizeof(f->sound_rows[0].name))
        f->bad_callback = true;
    else {
        const size_t row = f->sound_rows_count++;
        for (size_t i = 0; i < text.length; ++i)
            f->sound_rows[row].name[i] = (char)text.units[i];
        f->sound_rows[row].name[text.length] = '\0';
        f->sound_rows[row].position[0] = x;
        f->sound_rows[row].position[1] = y;
        f->sound_rows[row].position[2] = z;
        f->sound_rows[row].volume = volume;
        f->sound_rows[row].pitch = pitch;
    }
    f->volume = volume;
    f->pitch = pitch;
    f->callback_health = target->mob->living.Y;
    f->callback_hurt = target->mob->living.aa;
    f->callback_resistance = target->base.by;
    f->callback_velocity[0] = target->base.velocity_x;
    f->callback_velocity[1] = target->base.velocity_y;
    f->callback_velocity[2] = target->base.velocity_z;
    if (f->fail_sound) {
        *error =
            (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR, "authored Living sound refusal", 0};
        return error->status;
    }
    *error = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

static bool living_prepare(living_fixture *f, const char *resources, const char *directory) {
    CHECK(setup(&f->base, resources, directory) &&
          wolf_actor(&f->base, MC_ENTITY_CLASS_KIND_WOLF, &f->wolf) &&
          wolf_actor(&f->base, MC_ENTITY_CLASS_KIND_SHEEP, &f->sheep));
    const mc_world_access_callbacks callbacks = {.sound = living_sound};
    CHECK(mc_world_access_create(&callbacks, f, &f->observer, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(f->base.world, f->observer, NULL) ==
              MC_WORLD_STORAGE_OK);
    f->target_holds = f->sheep->holds;
    f->source_holds = f->wolf->holds;
    f->current_source = f->wolf;
    f->expected_volume = 1.0f;
    return true;
}

/* Explicit independent-call operands. No returned World/damage result is
 * supplied. A failed call is observed before another case is initialized. */
static bool living_reset(living_fixture *f) {
    CHECK(wolf_attack_reset(&f->base, f->wolf, f->sheep, 12345, true, -3, 0, -4));
    /* wolf_attack_reset places Wolf at8.5 and Sheep at5.5/4.5. Use the
     * declared comparison positions directly after that helper's real setup. */
    CHECK(mc_entity_set_position(f->sheep, 8.5, 64.5, 8.5) == MC_ENTITY_OK &&
          mc_entity_set_position(f->wolf, 11.5, 64.5, 12.5) == MC_ENTITY_OK);
    mc_living_entity_fields *v = &f->sheep->mob->living;
    v->Y = 20;
    v->Z = 17;
    v->E = 20;
    f->sheep->base.by = 0;
    v->au = 0;
    v->av = 17;
    v->aa = 3;
    v->ab = 4;
    v->ac = 99;
    v->al = .25f;
    v->ah = false;
    f->sheep->base.bb = false;
    f->sheep->base.be = false;
    f->sheep->base.yaw = 30;
    f->sheep->base.offset = .125f;
    f->sheep->base.velocity_x = .25;
    f->sheep->base.velocity_y = -.375;
    f->sheep->base.velocity_z = -.125;
    f->wolf->base.yaw = 15;
    f->wolf->base.velocity_x = -.5;
    f->wolf->base.velocity_y = .75;
    f->wolf->base.velocity_z = 1.25;
    f->wolf->base.bb = f->wolf->base.be = false;
    f->wolf->mob->living.ae = 123;
    mc_java_random_reseed(f->sheep->random, 38);
    mc_java_random_reseed(f->wolf->random, 12345);
    mc_java_random_reseed(f->base.world->fields.r, 2468);
    mc_java_random shared;
    mc_java_random_seed(&shared, 0);
    CHECK(f->base.profile->math != NULL);
    atomic_store(&f->base.profile->math->state, shared.state);
    f->current_source = f->wolf;
    f->target_holds = f->sheep->holds;
    f->source_holds = f->wolf->holds;
    f->sound_rows_count = 0;
    f->bad_callback = f->fail_sound = false;
    return true;
}

static bool living_normal_and_repeated(living_fixture *f) {
    CHECK(living_reset(f));
    mc_entity_effect_error e;
    bool answer = false;
    const uint64_t source_random = f->wolf->random->state;
    const uint64_t world_random = f->base.world->fields.r->state;
    const uint64_t shared_random = atomic_load(&f->base.profile->math->state);
    CHECK(mc_entity_effect_damage_with_math(f->sheep, f->base.registry, f->base.math, f->wolf, 4,
                                            &answer, &e) == MC_ENTITY_OK &&
          answer && e.living_damage.returned && e.living_damage.living_returned &&
          e.living_damage.wrapper_pc == 6 && e.living_damage.living_pc == 363 &&
          e.living_damage.full_hit && e.living_damage.direction_entered &&
          e.living_damage.direction_returned && e.living_damage.source_read &&
          e.living_damage.source_hold_acquired && e.living_damage.source_hold_released &&
          e.living_damage.source_identity == (uintptr_t)f->wolf &&
          e.living_damage.knockback_entered && e.living_damage.knockback_returned &&
          e.living_damage.knockback_pc == 121 && e.living_damage.shared_calls == 0 &&
          e.living_damage.private_calls == 2 && e.living_damage.sound_returned &&
          f->sheep->mob->living.Y == 16 && f->sheep->mob->living.Z == 20 &&
          f->sheep->mob->living.au == 4 && f->sheep->base.by == 20 &&
          f->sheep->mob->living.aa == 10 && f->sheep->mob->living.ab == 10 &&
          float_bits(f->sheep->mob->living.ac) == UINT32_C(0x41b90a72) &&
          double_bits(f->sheep->base.velocity_x) == UINT64_C(0xbfbd70a3e6666666) &&
          double_bits(f->sheep->base.velocity_y) == UINT64_C(0x3fcb333340000000) &&
          double_bits(f->sheep->base.velocity_z) == UINT64_C(0xbfd87ae14ccccccd) &&
          f->sheep->holds == f->target_holds && f->wolf->holds == f->source_holds &&
          f->wolf->random->state == source_random &&
          f->base.world->fields.r->state == world_random &&
          atomic_load(&f->base.profile->math->state) == shared_random && f->sound_rows_count == 1 &&
          !f->bad_callback);
    const mc_entity_base_fields after = f->sheep->base;
    const uint64_t private_random = f->sheep->random->state;
    CHECK(mc_entity_effect_damage_with_math(f->sheep, NULL, NULL, f->wolf, 3, &answer, &e) ==
              MC_ENTITY_OK &&
          !answer && e.living_damage.returned && e.living_damage.living_pc == 59 &&
          !e.living_damage.source_read && !e.living_damage.source_hold_acquired &&
          f->sheep->mob->living.Y == 16 && f->sheep->random->state == private_random &&
          f->sound_rows_count == 1 && f->sheep->base.velocity_x == after.velocity_x &&
          f->sheep->base.velocity_y == after.velocity_y &&
          f->sheep->base.velocity_z == after.velocity_z);
    CHECK(mc_entity_effect_damage_with_math(f->sheep, NULL, NULL, f->wolf, 7, &answer, &e) ==
              MC_ENTITY_OK &&
          answer && !e.living_damage.full_hit && e.living_damage.living_pc == 363 &&
          !e.living_damage.source_read && !e.living_damage.source_hold_acquired &&
          !e.living_damage.knockback_entered && f->sheep->mob->living.Y == 13 &&
          f->sheep->mob->living.au == 7 && float_bits(f->sheep->mob->living.ac) == 0 &&
          f->sheep->random->state == private_random && f->sound_rows_count == 1 &&
          f->sheep->base.velocity_x == after.velocity_x &&
          f->sheep->base.velocity_y == after.velocity_y &&
          f->sheep->base.velocity_z == after.velocity_z && !f->bad_callback);
    return true;
}

static bool living_gates_and_failures(living_fixture *f) {
    mc_entity_effect_error e;
    bool answer = true;
    CHECK(living_reset(f));
    f->base.world->fields.B = true;
    const mc_entity_status remote =
        mc_entity_effect_damage_with_math(f->sheep, NULL, NULL, f->wolf, 4, &answer, &e);
    f->base.world->fields.B = false;
    CHECK(remote == MC_ENTITY_OK && !answer && e.living_damage.living_pc == 11 &&
          !e.living_damage.source_read && f->sheep->mob->living.av == 17 &&
          f->sheep->mob->living.Y == 20 && f->sound_rows_count == 0);
    CHECK(living_reset(f));
    f->sheep->mob->living.Y = 0;
    CHECK(mc_entity_effect_damage_with_math(f->sheep, NULL, NULL, f->wolf, 4, &answer, &e) ==
              MC_ENTITY_OK &&
          !answer && e.living_damage.living_pc == 25 && !e.living_damage.source_read &&
          f->sheep->mob->living.av == 0 && f->sound_rows_count == 0);
    CHECK(living_reset(f));
    mc_entity *null_world = NULL;
    const mc_entity_class_literal *literal = NULL;
    mc_mob_error construction;
    CHECK(mc_entity_class_literal_get(f->base.entities, MC_ENTITY_CLASS_KIND_SHEEP, &literal) ==
              MC_ENTITY_OK &&
          mc_mob_allocate(f->base.entities, f->base.registry, literal, &null_world,
                          &construction) == MC_MOB_OK &&
          mc_mob_construct(null_world, NULL, &construction) == MC_MOB_OK);
    answer = true;
    CHECK(mc_entity_effect_damage_with_math(null_world, NULL, NULL, f->wolf, 4, &answer, &e) ==
              MC_ENTITY_STATE_ERROR &&
          answer && e.living_damage.source_exception && !e.living_damage.native_policy &&
          e.living_damage.living_pc == 4 && !e.living_damage.source_read &&
          null_world->base.world == NULL && mc_mob_retire(&null_world, &construction) == MC_MOB_OK);
    CHECK(living_reset(f));
    answer = true;
    CHECK(mc_entity_effect_damage_in_context(f->sheep, f->base.registry, f->wolf, 4, &answer, &e) ==
              MC_ENTITY_STATE_ERROR &&
          answer && e.living_damage.living_pc == 244 && e.cause == MC_ENTITY_EFFECT_CAUSE_MATH &&
          e.living_damage.native_policy && !e.living_damage.source_exception &&
          e.living_damage.source_hold_acquired && e.living_damage.source_hold_released &&
          e.living_damage.knockback_entered && !e.living_damage.knockback_returned &&
          e.living_damage.knockback_pc == 9 && f->sheep->mob->living.Y == 16 && f->sheep->base.bb &&
          double_bits(f->sheep->base.velocity_x) == double_bits(.25) &&
          double_bits(f->sheep->base.velocity_y) == double_bits(-.375) &&
          double_bits(f->sheep->base.velocity_z) == double_bits(-.125) &&
          f->sheep->holds == f->target_holds && f->wolf->holds == f->source_holds &&
          f->sound_rows_count == 0);
    CHECK(living_reset(f));
    mc_math_classes *failed_math = NULL;
    CHECK(mc_math_classes_create(SIZE_MAX, &failed_math, NULL) == MC_MATH_CLASS_OK);
    const unsigned int csr = _mm_getcsr();
    _mm_setcsr(csr | 0x40u);
    const mc_math_class_status initialized = mc_math_classes_ensure(failed_math, NULL);
    _mm_setcsr(csr);
    CHECK(initialized == MC_MATH_CLASS_UNSUPPORTED_FP);
    answer = true;
    const mc_entity_status math_failure = mc_entity_effect_damage_with_math(
        f->sheep, f->base.registry, failed_math, f->wolf, 4, &answer, &e);
    CHECK(math_failure != MC_ENTITY_OK && answer && e.cause == MC_ENTITY_EFFECT_CAUSE_MATH &&
          e.living_damage.living_pc == 244 && e.living_damage.knockback_pc == 9 &&
          e.living_damage.source_hold_released && f->sheep->base.velocity_x == .25 &&
          f->sheep->mob->living.Y == 16 && f->sound_rows_count == 0 &&
          mc_math_classes_release(&failed_math, NULL) == MC_MATH_CLASS_OK && failed_math == NULL);
    CHECK(living_reset(f));
    answer = true;
    _mm_setcsr(csr | 0x40u);
    const mc_entity_status fp_failure = mc_entity_effect_damage_with_math(
        f->sheep, f->base.registry, f->base.math, f->wolf, 4, &answer, &e);
    _mm_setcsr(csr);
    CHECK(fp_failure == MC_ENTITY_UNSUPPORTED_FP && answer && e.living_damage.living_pc == 38 &&
          e.living_damage.native_policy && !e.living_damage.source_read &&
          f->sheep->mob->living.al == 1.5f && f->sheep->mob->living.av == 0 &&
          f->sheep->mob->living.Y == 20 && f->sheep->base.by == 0 && f->sound_rows_count == 0);
    CHECK(living_reset(f));
    mc_java_random *allocation = f->sheep->random;
    f->sheep->random = NULL;
    answer = true;
    const mc_entity_status private_failure = mc_entity_effect_damage_with_math(
        f->sheep, f->base.registry, f->base.math, f->wolf, 4, &answer, &e);
    const bool private_prefix =
        answer && e.living_damage.source_exception && !e.living_damage.native_policy &&
        e.living_damage.living_pc == 343 && e.living_damage.source_hold_released &&
        e.living_damage.private_calls == 0 && e.living_damage.private_attempts == 1 &&
        e.living_damage.knockback_returned && f->sheep->mob->living.Y == 16 &&
        f->sheep->random == NULL;
    f->sheep->random = allocation; /* Owned allocation restored only after observation. */
    CHECK(private_failure == MC_ENTITY_STATE_ERROR && private_prefix);
    CHECK(living_reset(f));
    f->fail_sound = true;
    answer = true;
    const mc_entity_status sound_failure = mc_entity_effect_damage_with_math(
        f->sheep, f->base.registry, f->base.math, f->wolf, 4, &answer, &e);
    f->fail_sound = false;
    CHECK(sound_failure == MC_ENTITY_WORLD_FAILED && answer && e.living_damage.living_pc == 359 &&
          e.living_damage.native_policy && !e.living_damage.source_exception &&
          e.living_damage.source_hold_released && e.living_damage.knockback_returned &&
          !e.living_damage.sound_returned && e.world.cause.status == MC_WORLD_STORAGE_IO_ERROR &&
          e.world.world_access_failure.receipt == NULL && f->sound_rows_count == 1 &&
          !f->bad_callback && f->callback_health == 16 &&
          double_bits(f->callback_velocity[0]) == UINT64_C(0xbfbd70a3e6666666) &&
          f->sheep->holds == f->target_holds && f->wolf->holds == f->source_holds);
    CHECK(living_reset(f));
    f->sheep->mob->living.Y = 4;
    answer = true;
    CHECK(mc_entity_effect_damage_with_math(f->sheep, f->base.registry, f->base.math, f->wolf, 4,
                                            &answer, &e) == MC_ENTITY_STATE_ERROR &&
          answer && e.cause == MC_ENTITY_EFFECT_CAUSE_UNSUPPORTED_VIRTUAL &&
          e.living_damage.death_invoke_reached && e.living_damage.living_pc == 316 &&
          e.living_damage.sound_returned && e.living_damage.source_hold_released &&
          f->sheep->mob->living.Y == 0 && !f->sheep->mob->living.ah && !f->sheep->base.be &&
          f->sound_rows_count == 1 && !f->bad_callback);
    return true;
}

static uint32_t living_next(uint64_t *state, unsigned int width) {
    *state = (*state * UINT64_C(0x5deece66d) + UINT64_C(11)) & UINT64_C(0xffffffffffff);
    return (uint32_t)(*state >> (48u - width));
}

static double living_next_double(uint64_t *state) {
    const uint64_t high = living_next(state, 26), low = living_next(state, 27);
    return (double)((high << 27u) | low) / 9007199254740992.0;
}

static bool living_nullable_and_self_source(living_fixture *f) {
    CHECK(living_reset(f));
    f->current_source = NULL;
    mc_entity_effect_error e;
    bool answer = false;
    CHECK(mc_entity_effect_damage_in_context(f->sheep, f->base.registry, NULL, 4, &answer, &e) ==
              MC_ENTITY_OK &&
          answer && e.living_damage.returned && e.living_damage.shared_calls == 1 &&
          !e.living_damage.source_read && !e.living_damage.knockback_entered &&
          f->sheep->base.velocity_x == .25 && f->sheep->base.velocity_y == -.375 &&
          f->sheep->base.velocity_z == -.125 && f->sound_rows_count == 1 && !f->bad_callback);
    CHECK(living_reset(f));
    f->current_source = f->sheep;
    f->source_holds = f->target_holds;
    uint64_t expected_state = UINT64_C(0x5deece66d), iterations = 0;
    double draws[4];
    volatile double dx, dz, squared;
    do {
        for (size_t i = 0; i < 4; ++i)
            draws[i] = living_next_double(&expected_state);
        volatile double x_difference = draws[0] - draws[1], z_difference = draws[2] - draws[3];
        dx = x_difference * .01;
        dz = z_difference * .01;
        volatile double xx = dx * dx, zz = dz * dz;
        squared = xx + zz;
        ++iterations;
        CHECK(iterations < 1024); /* Independent oracle bound, never a production retry limit. */
    } while (squared < 1.0e-4);
    CHECK(mc_entity_effect_damage_with_math(f->sheep, f->base.registry, f->base.math, f->sheep, 4,
                                            &answer, &e) == MC_ENTITY_OK &&
          answer && e.living_damage.source_identity == (uintptr_t)f->sheep &&
          e.living_damage.source_hold_acquired && e.living_damage.source_hold_released &&
          e.living_damage.direction_iterations == iterations &&
          e.living_damage.shared_calls == iterations * 4 &&
          e.living_damage.direction_draw_mask == 15 &&
          double_bits(e.living_damage.direction_x) == double_bits(dx) &&
          double_bits(e.living_damage.direction_z) == double_bits(dz) &&
          atomic_load(&f->base.profile->math->state) == expected_state &&
          f->sheep->holds == f->target_holds && f->sound_rows_count == 1 && !f->bad_callback);
    for (size_t i = 0; i < 4; ++i)
        CHECK(double_bits(e.living_damage.direction_draws[i]) == double_bits(draws[i]));
    return true;
}

static bool living_concrete_sounds(living_fixture *f) {
    static const mc_entity_class_kind kinds[] = {MC_ENTITY_CLASS_KIND_PIG, MC_ENTITY_CLASS_KIND_COW,
                                                 MC_ENTITY_CLASS_KIND_CHICKEN};
    static const char *const names[] = {"mob.pig", "mob.cowhurt", "mob.chickenhurt"};
    mc_entity *sheep = f->sheep;
    for (size_t i = 0; i < COUNT(kinds); ++i) {
        CHECK(wolf_actor(&f->base, kinds[i], &f->sheep) && living_reset(f));
        f->expected_sound = names[i];
        bool answer = false;
        mc_entity_effect_error e;
        const float volume = kinds[i] == MC_ENTITY_CLASS_KIND_COW ? .4f : 1.0f;
        f->expected_volume = volume;
        CHECK(mc_entity_effect_damage_with_math(f->sheep, f->base.registry, f->base.math, f->wolf,
                                                4, &answer, &e) == MC_ENTITY_OK &&
              answer && e.living_damage.returned && e.living_damage.sound_returned &&
              strcmp(e.living_damage.sound_literal, names[i]) == 0 &&
              e.living_damage.volume == volume && f->sound_rows_count == 1 && !f->bad_callback &&
              f->sheep->holds == f->target_holds && f->wolf->holds == f->source_holds);
        mc_mob_error construction;
        CHECK(mc_mob_retire(&f->sheep, &construction) == MC_MOB_OK);
    }
    f->sheep = sheep;
    f->expected_sound = NULL;
    f->expected_volume = 1.0f;
    return true;
}

/* Pig inherits LS directly. A late failure must name that actual LS invoke,
 * while Sheep failures retain their distinct DL invokespecial caller3. */
static bool living_direct_failure_site(living_fixture *f) {
    mc_entity *sheep = f->sheep;
    CHECK(wolf_actor(&f->base, MC_ENTITY_CLASS_KIND_PIG, &f->sheep) && living_reset(f));
    mc_mob_tick_error *error = calloc(1, sizeof(*error));
    CHECK(error != NULL && mt_wolf_tame_component(f->wolf, f->base.registry, f->base.math, NULL,
                                                  false, error) == MC_ENTITY_OK);
    const uint64_t private_random = f->sheep->random->state;
    const uint64_t source_random = f->wolf->random->state;
    const uint64_t world_random = f->base.world->fields.r->state;
    const uint64_t shared_random = atomic_load(&f->base.profile->math->state);
    const size_t sounds = f->sounds;
    const mc_entity_status status =
        mt_wolf_attack_component(f->wolf, f->base.registry, NULL, NULL, f->sheep, 1.f, error);
    const mc_entity_effect_error *effect = &error->detail.effect;
    CHECK(status == MC_ENTITY_STATE_ERROR && error->site.pc == 195 &&
          error->site.class_name != NULL && strcmp(error->site.class_name, "gi") == 0 &&
          error->child == MC_MOB_TICK_CHILD_EFFECT && error->child_site.class_name != NULL &&
          strcmp(error->child_site.class_name, "ls") == 0 && error->child_site.pc == 244 &&
          !error->source_exception && effect->status == MC_ENTITY_STATE_ERROR &&
          effect->cause == MC_ENTITY_EFFECT_CAUSE_MATH && effect->source_bytecode == 244 &&
          effect->living_damage.living_pc == 244 && effect->living_damage.knockback_pc == 9 &&
          effect->math.status == MC_MATH_CLASS_INVALID_ARGUMENT && effect->math.source_pc == -1 &&
          effect->living_damage.source_hold_acquired &&
          effect->living_damage.source_hold_released && !effect->living_damage.returned &&
          !effect->living_damage.result_stored && !effect->living_damage.knockback_returned &&
          f->wolf->mob->living.ae == 20 && f->sheep->mob->living.Y == 18 &&
          f->sheep->mob->living.Z == 20 && f->sheep->mob->living.au == 2 &&
          f->sheep->mob->living.aa == 10 && f->sheep->mob->living.ab == 10 &&
          f->sheep->base.by == 20 && f->sheep->base.bb &&
          double_bits(f->sheep->base.velocity_x) == double_bits(.25) &&
          double_bits(f->sheep->base.velocity_y) == double_bits(-.375) &&
          double_bits(f->sheep->base.velocity_z) == double_bits(-.125) &&
          f->sheep->random->state == private_random && f->wolf->random->state == source_random &&
          f->base.world->fields.r->state == world_random &&
          atomic_load(&f->base.profile->math->state) == shared_random && f->sounds == sounds &&
          f->sound_rows_count == 0 && f->sheep->holds == f->target_holds &&
          f->wolf->holds == f->source_holds);
    free(error);
    mc_mob_error construction;
    CHECK(mc_mob_retire(&f->sheep, &construction) == MC_MOB_OK);
    f->sheep = sheep;
    f->target_holds = sheep->holds;
    return true;
}

/* Written before DL/LS nonnull-source admission. The old client reaches
 * GI195 -> actual Sheep DL.a3 and retains that unsupported child. */
static bool living_bite_red(living_fixture *f) {
    CHECK(wolf_attack_reset(&f->base, f->wolf, f->sheep, 38, true, 1, 0, 0));
    mc_mob_tick_error *e = calloc(1, sizeof(*e));
    CHECK(e != NULL && mt_wolf_tame_component(f->wolf, f->base.registry, f->base.math, NULL, false,
                                              e) == MC_ENTITY_OK);
    mc_java_random_reseed(f->sheep->random, 77);
    const int32_t health = f->sheep->mob->living.Y;
    const size_t sounds = f->sounds;
    const mc_entity_status status =
        mt_wolf_attack_component(f->wolf, f->base.registry, f->base.math, NULL, f->sheep, 1.f, e);
    if (status != MC_ENTITY_OK)
        fprintf(stderr,
                "Living bite actual GI%u child=%s.%s:%d status=%d effect=%s damage-pc=%u "
                "source-id=%" PRIuPTR " target-health=%" PRId32 "\n",
                e->site.pc, e->child_site.class_name == NULL ? "none" : e->child_site.class_name,
                e->child_site.method == NULL ? "none" : e->child_site.method, e->child_site.pc,
                (int)status,
                e->detail.effect.operation == NULL ? "none" : e->detail.effect.operation,
                e->detail.effect.damage_bytecode, e->detail.effect.damage_source_identity,
                f->sheep->mob->living.Y);
    CHECK(status == MC_ENTITY_OK && wolf_returned(e) && f->wolf->mob->living.ae == 20 &&
          f->sheep->mob->living.Y == health - 2 && f->sheep->mob->living.Z == health &&
          f->sheep->mob->living.au == 2 && f->sheep->mob->living.aa == 10 &&
          f->sheep->mob->living.ab == 10 && f->sheep->base.by == 20 && f->sounds == sounds + 1 &&
          !f->bad_callback && f->callback_health == health - 2 && f->callback_hurt == 10 &&
          f->callback_resistance == 20 && f->sheep->holds == f->target_holds &&
          f->wolf->holds == f->source_holds);
    free(e);
    puts("LIVING_BITE returned=1 wild-damage=2 real-wolf-caller=1 sheep-sound=1 held-owners=1");
    return true;
}

typedef struct living_reference_profile {
    int32_t path, amount, health, by, au, world, source, random, bounds, point;
    uint32_t yaw;
    uint64_t velocity_y;
    int32_t math_seed, private_seed, tame, sound;
} living_reference_profile;

#define LIVING_PROFILE(amount_, health_, by_, au_, world_, source_, random_, bounds_, point_)      \
    {                                                                                              \
        0, amount_, health_, by_, au_, world_, source_, random_, bounds_, point_, 0x41f00000,      \
            UINT64_C(0xbfd8000000000000), 0, 38, 0, 0                                              \
    }
static const living_reference_profile living_profiles[] = {
    LIVING_PROFILE(4, 20, 0, 0, 0, 0, 0, 0, 0),
    LIVING_PROFILE(4, 20, 0, 0, 0, 0, 0, 0, 1),
    {0, 4, 20, 0, 0, 0, 0, 0, 0, 2, 0xc2340000, UINT64_C(0x3fe8000000000000), 0, 38, 0, 0},
    LIVING_PROFILE(3, 20, 11, 4, 0, 0, 0, 0, 0),
    LIVING_PROFILE(4, 20, 11, 4, 0, 0, 0, 0, 0),
    LIVING_PROFILE(7, 20, 11, 4, 0, 0, 0, 0, 0),
    LIVING_PROFILE(4, 20, 10, 4, 0, 0, 0, 0, 0),
    LIVING_PROFILE(4, 20, 0, 0, 1, 0, 0, 0, 0),
    LIVING_PROFILE(4, 0, 0, 0, 0, 0, 0, 0, 0),
    LIVING_PROFILE(4, 20, 0, 0, 2, 0, 0, 0, 0),
    LIVING_PROFILE(4, 20, 0, 0, 0, 0, 1, 0, 0),
    LIVING_PROFILE(4, 20, 0, 0, 0, 1, 0, 0, 0),
    LIVING_PROFILE(7, 20, 11, 4, 0, 1, 0, 0, 0),
    LIVING_PROFILE(4, 20, 0, 0, 0, 0, 0, 0, 3),
    {0, 4, 20, 0, 0, 0, 0, 0, 0, 4, 0x41f00000, UINT64_C(0xbfd8000000000000), 1, 38, 0, 0},
    LIVING_PROFILE(4, 20, 0, 0, 0, 0, 0, 0, 5),
    LIVING_PROFILE(4, 20, 0, 0, 0, 0, 0, 1, 0),
    LIVING_PROFILE(-3, 20, 0, 0, 0, 0, 0, 0, 0),
    LIVING_PROFILE(0, 20, 0, 0, 0, 0, 0, 0, 0),
    LIVING_PROFILE(4, 4, 0, 0, 0, 0, 0, 0, 0),
    LIVING_PROFILE(4, 4, 0, 0, 0, 0, 1, 0, 0),
    {0, 4, 20, 0, 0, 0, 0, 0, 0, 0, 0x41f00000, UINT64_C(0xbfd8000000000000), 0, 38, 0, 1},
    {1, 2, 20, 0, 0, 0, 0, 0, 0, 0, 0x41f00000, UINT64_C(0xbfd8000000000000), 0, 38, 0, 0},
    {1, 4, 20, 0, 0, 0, 0, 0, 0, 0, 0x41f00000, UINT64_C(0xbfd8000000000000), 0, 38, 1, 0},
    {2, 4, 20, 0, 0, 0, 0, 0, 0, 0, 0x41f00000, UINT64_C(0xbfd8000000000000), 0, 38, 0, 0}};
#undef LIVING_PROFILE

static const char *living_boolean(bool value) {
    return value ? "true" : "false";
}

static void living_reference_state(FILE *out, const living_fixture *f, bool source_unchanged) {
    const mc_entity *p = f->sheep;
    const mc_living_entity_fields *v = &p->mob->living;
    fprintf(out,
            "{\"health\":[%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId32
            ",%" PRId32 ",%" PRId32 "],\"animation\":[\"%08" PRIx32 "\",\"%08" PRIx32
            "\"],\"velocity\":",
            v->Y, v->Z, v->E, p->base.by, v->au, v->av, v->aa, v->ab, float_bits(v->ac),
            float_bits(v->al));
    const double velocity[] = {p->base.velocity_x, p->base.velocity_y, p->base.velocity_z};
    wolf_reference_vector(out, velocity, COUNT(velocity));
    fprintf(out, ",\"attacked\":%s,\"dying\":%s,\"position\":", living_boolean(p->base.bb),
            living_boolean(v->ah));
    const double position[] = {p->base.x, p->base.y, p->base.z};
    wolf_reference_vector(out, position, COUNT(position));
    fprintf(out, ",\"yaw\":\"%08" PRIx32 "\",\"bounds\":", float_bits(p->base.yaw));
    wolf_reference_box(out, p->bounds);
    fputs(",\"random\":", out);
    if (p->random == NULL)
        fputs("null", out);
    else
        fprintf(out, "\"%016" PRIx64 "\"", p->random->state);
    fprintf(out,
            ",\"math_random\":\"%016" PRIx64 "\",\"world_random\":\"%016" PRIx64
            "\",\"source_random\":\"%016" PRIx64 "\",\"source_cooldown\":%" PRId32
            ",\"source_unchanged\":%s,\"sounds\":[",
            atomic_load(&f->base.profile->math->state), f->base.world->fields.r->state,
            f->wolf->random->state, f->wolf->mob->living.ae, living_boolean(source_unchanged));
    for (size_t i = 0; i < f->sound_rows_count; ++i) {
        fprintf(out, "%s{\"name\":\"%s\",\"position\":", i == 0 ? "" : ",", f->sound_rows[i].name);
        wolf_reference_vector(out, f->sound_rows[i].position, 3);
        fprintf(out, ",\"volume\":\"%08" PRIx32 "\",\"pitch\":\"%08" PRIx32 "\"}",
                float_bits(f->sound_rows[i].volume), float_bits(f->sound_rows[i].pitch));
    }
    fputs("]}", out);
}

static bool living_reference_rows(living_fixture *f, const char *destination) {
    static const double points[][2] = {{3, 4}, {-2.8, 3.3},  {-3, -4},
                                       {0, 0}, {.005, .005}, {.01, 0}};
    FILE *out = open_file(destination, "wb");
    mc_mob_tick_error *attack = calloc(1, sizeof(*attack));
    CHECK(out != NULL && attack != NULL);
    size_t row = 0;
    for (size_t index = 0; index < COUNT(living_profiles); ++index) {
        const living_reference_profile *p = &living_profiles[index];
        CHECK(living_reset(f));
        CHECK(mc_entity_set_position(f->wolf, 8.5 + points[p->point][0], 64.5,
                                     8.5 + points[p->point][1]) == MC_ENTITY_OK &&
              mt_wolf_tame_component(f->wolf, f->base.registry, f->base.math, NULL, p->tame != 0,
                                     attack) == MC_ENTITY_OK);
        mc_living_entity_fields *v = &f->sheep->mob->living;
        v->Y = p->health;
        f->sheep->base.by = p->by;
        v->au = p->au;
        memcpy(&f->sheep->base.yaw, &p->yaw, sizeof(p->yaw));
        memcpy(&f->sheep->base.velocity_y, &p->velocity_y, sizeof(p->velocity_y));
        wolf_reference_set_box(f->sheep->bounds, 8, 64, 8, 9, 65, 9);
        wolf_reference_set_box(f->wolf->bounds, f->wolf->base.x - .5, 64, f->wolf->base.z - .5,
                               f->wolf->base.x + .5, 65, f->wolf->base.z + .5);
        mc_entity_watch_record *wool = wolf_attack_flags(f->sheep);
        CHECK(wool != NULL && wool->type == 0);
        wool->byte_value = (int8_t)((uint8_t)wool->byte_value | 16u);
        mc_java_random_reseed(f->sheep->random, p->private_seed);
        mc_java_random seed;
        mc_java_random_seed(&seed, p->math_seed);
        atomic_store(&f->base.profile->math->state, seed.state);
        mc_java_random *random = f->sheep->random;
        mc_entity_bounds *box = f->sheep->bounds;
        if (p->random != 0)
            f->sheep->random = NULL;
        if (p->bounds != 0)
            f->sheep->bounds = NULL;
        f->base.world->fields.B = p->world == 1;
        if (p->world == 2)
            f->sheep->base.world = NULL;
        f->current_source = p->source == 1 ? NULL : f->wolf;
        f->fail_sound = p->sound != 0;
        const mc_entity_base_fields source_before = f->wolf->base;
        const uint64_t source_random = f->wolf->random->state;
        mc_entity_bounds *source_box = f->wolf->bounds;
        const mc_entity_bounds source_bounds = *source_box;
        const int32_t source_health = f->wolf->mob->living.Y;
        const int32_t amounts[] = {4, 3, 7};
        const size_t steps = p->path == 2 ? COUNT(amounts) : 1;
        for (size_t step = 0; step < steps; ++step) {
            const int32_t amount = p->path == 2 ? amounts[step] : p->amount;
            mc_entity_effect_error error = {0};
            bool answer = false;
            const mc_entity_status status =
                p->path == 1
                    ? mt_wolf_attack_component(f->wolf, f->base.registry, f->base.math, NULL,
                                               f->sheep, 1.f, attack)
                    : mc_entity_effect_damage_with_math(f->sheep, f->base.registry, f->base.math,
                                                        f->current_source, amount, &answer, &error);
            const char *scope = "RETURN", *exception = "NONE";
            if (status != MC_ENTITY_OK) {
                if (index == 19) {
                    CHECK(error.living_damage.death_invoke_reached &&
                          error.living_damage.living_pc == 316 &&
                          error.cause == MC_ENTITY_EFFECT_CAUSE_UNSUPPORTED_VIRTUAL);
                    scope = "DEATH_ENTRY";
                    exception = "UNIMPLEMENTED";
                } else if (index == 21) {
                    CHECK(status == MC_ENTITY_WORLD_FAILED &&
                          error.world.cause.status == MC_WORLD_STORAGE_IO_ERROR &&
                          error.living_damage.native_policy &&
                          !error.living_damage.source_exception);
                    scope = "EXCEPTION";
                    exception = "IO";
                } else {
                    CHECK(p->path != 1 && error.living_damage.source_exception &&
                          !error.living_damage.native_policy &&
                          strcmp(error.living_damage.exception_class,
                                 "java/lang/NullPointerException") == 0);
                    scope = "EXCEPTION";
                    exception = "NPE";
                }
            }
            const mc_entity_base_fields *s = &f->wolf->base;
            const bool unchanged =
                s->x == source_before.x && s->y == source_before.y && s->z == source_before.z &&
                s->velocity_x == source_before.velocity_x &&
                s->velocity_y == source_before.velocity_y &&
                s->velocity_z == source_before.velocity_z && s->yaw == source_before.yaw &&
                s->aX == source_before.aX && s->be == source_before.be &&
                s->bb == source_before.bb && s->world == f->base.world &&
                f->wolf->random->state == source_random &&
                f->wolf->mob->living.Y == source_health && f->wolf->bounds == source_box &&
                source_box->min_x == source_bounds.min_x &&
                source_box->min_y == source_bounds.min_y &&
                source_box->min_z == source_bounds.min_z &&
                source_box->max_x == source_bounds.max_x &&
                source_box->max_y == source_bounds.max_y &&
                source_box->max_z == source_bounds.max_z;
            CHECK(unchanged && !f->bad_callback && f->sheep->holds == f->target_holds &&
                  f->wolf->holds == f->source_holds);
            fprintf(out,
                    "{\"case\":%zu,\"profile\":%zu,\"step\":%zu,\"input\":[%" PRId32 ",%" PRId32
                    ",%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId32
                    ",%" PRId32 ",%" PRId32 ",\"%08" PRIx32 "\",\"%016" PRIx64 "\",%" PRId32
                    ",%" PRId32 ",%" PRId32 ",%" PRId32 "],\"amount\":%" PRId32
                    ",\"scope\":\"%s\",\"result\":%s,\"exception\":\"%s\",\"state\":",
                    row++, index, step, p->path, p->amount, p->health, p->by, p->au, p->world,
                    p->source, p->random, p->bounds, p->point, p->yaw, p->velocity_y, p->math_seed,
                    p->private_seed, p->tame, p->sound, amount, scope,
                    status == MC_ENTITY_OK && p->path != 1 ? living_boolean(answer) : "null",
                    exception);
            living_reference_state(out, f, unchanged);
            fputs(",\"completion\":null}\n", out);
            CHECK(fflush(out) == 0);
        }
        /* Nullable input cells are restored solely after every recorded result
         * so the next independent case and final owners can be disposed. */
        f->sheep->random = random;
        f->sheep->bounds = box;
        f->sheep->base.world = f->base.world;
        f->base.world->fields.B = false;
        f->fail_sound = false;
    }
    CHECK(row == 27 && fclose(out) == 0);
    free(attack);
    return true;
}

static bool living_dispose(living_fixture *f) {
    bool removed = false;
    CHECK(mc_world_runtime_remove_world_access(f->base.world, f->observer, &removed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          removed && mc_world_access_free(f->observer, NULL) == MC_WORLD_STORAGE_OK);
    mc_mob_tick_error *cleanup = calloc(1, sizeof(*cleanup));
    mc_mob_error error;
    CHECK(cleanup != NULL && mc_mob_tick_cleanup(f->wolf, cleanup) == MC_ENTITY_OK &&
          mc_mob_retire(&f->wolf, &error) == MC_MOB_OK &&
          mc_mob_retire(&f->sheep, &error) == MC_MOB_OK);
    free(cleanup);
    return teardown(&f->base);
}

int main(int argc, char **argv) {
    if (argc != 3 && !(argc == 5 && strcmp(argv[3], "--reference") == 0))
        return 2;
    living_fixture *f = calloc(1, sizeof(*f));
    if (f == NULL || !living_prepare(f, argv[1], argv[2]) ||
        !(argc == 5 ? living_reference_rows(f, argv[4])
                    : living_bite_red(f) && living_normal_and_repeated(f) &&
                          living_gates_and_failures(f) && living_nullable_and_self_source(f) &&
                          living_concrete_sounds(f) && living_direct_failure_site(f)) ||
        !living_dispose(f)) {
        fputs("Living damage failure: actual graph retained; no cleanup credit\n", stderr);
        return 1;
    }
    free(f);
    printf("PASS: %u Living damage source-order/ownership checks\n", checks);
    return 0;
}
