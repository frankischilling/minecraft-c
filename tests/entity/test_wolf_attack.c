/* Actual GI protected attack component on canonical Wolf/Sheep/Item owners.
 * The imported World fixture main is retained but never executed. */
int mc_wolf_attack_world_fixture_main(int, char **);
#define main mc_wolf_attack_world_fixture_main
#include "../world/runtime/test_world_random_tick.c"
#undef main
#ifdef MC_WOLF_ATTACK_BODY_MAIN_NAME
#define main MC_WOLF_ATTACK_BODY_MAIN_NAME
#endif
#include "../support/java_random.h"
#include "entity/mob/tick/internal.h"
#include <math.h>

#undef CHECK
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            fprintf(stderr, "Wolf attack check%u line%d: %s\n", checks, __LINE__, #c);             \
            goto done;                                                                             \
        }                                                                                          \
    } while (0)

static bool wolf_actor(random_fixture *f, mc_entity_class_kind kind, mc_entity **out) {
    const mc_entity_class_literal *type = NULL;
    mc_mob_error error;
    return mc_entity_class_literal_get(f->entities, kind, &type) == MC_ENTITY_OK &&
           mc_mob_allocate(f->entities, f->registry, type, out, &error) == MC_MOB_OK &&
           mc_mob_construct(*out, f->world, &error) == MC_MOB_OK;
}

static mc_entity_watch_record *wolf_attack_flags(mc_entity *p) {
    for (mc_entity_watch_node *n = p->watcher->map->head; n != NULL; n = n->next)
        if (n->key == 16)
            return n->record;
    return NULL;
}

static bool wolf_attack_reset(random_fixture *f, mc_entity *p, mc_entity *target, int64_t seed,
                              bool grounded, double dx, double dy, double dz) {
    return mc_entity_set_position(p, 8.5, 64.5, 8.5) == MC_ENTITY_OK &&
           mc_entity_set_position(target, 8.5 + dx, 64.5 + dy, 8.5 + dz) == MC_ENTITY_OK &&
           mc_entity_set_velocity(p, 0, .25) == MC_ENTITY_OK &&
           mc_entity_set_velocity(p, 1, -.375) == MC_ENTITY_OK &&
           mc_entity_set_velocity(p, 2, -.125) == MC_ENTITY_OK &&
           (p->base.aX = grounded, p->mob->living.ae = 123, mc_java_random_reseed(p->random, seed),
            true) &&
           p->base.world == f->world && target->base.world == f->world;
}

static bool wolf_returned(const mc_mob_tick_error *e) {
    return e->status == MC_ENTITY_OK && e->state.returned && e->state.attack.stored &&
           e->state.attack.returned && e->site.pc == 199 && !strcmp(e->site.class_name, "gi") &&
           !strcmp(e->site.method, "a") && !strcmp(e->site.descriptor, "(Lsn;F)V") &&
           !e->state.environment_returned && !e->state.action_returned &&
           !e->state.behavior_returned;
}

static bool wolf_attack_paths(random_fixture *f, mc_entity *p, mc_entity *target,
                              mc_mob_tick_error *e) {
    bool passed = false;
    const size_t actor_holds = p->holds, target_holds = target->holds;
    const size_t registry_holds = f->registry->runtime_borrows;
    mc_math_class_state math_before, math_after;
    CHECK(mc_math_classes_state(f->math, &math_before, NULL) == MC_MATH_CLASS_OK);
    float nan;
    const uint32_t nan_word = UINT32_C(0x7fc12345);
    memcpy(&nan, &nan_word, sizeof(nan));
    const float quiet[] = {1.5f, 2.f, 6.f, INFINITY, nan};
    for (size_t i = 0; i < COUNT(quiet); ++i) {
        CHECK(wolf_attack_reset(f, p, target, 0, true, 3, 0, 4));
        const mc_java_random before = *p->random;
        /* These exact branches never dereference target or require Registry,
         * Math, World, target bounds, or private Random.nextInt. */
        CHECK(mt_wolf_attack_component(p, NULL, NULL, NULL, NULL, quiet[i], e) == MC_ENTITY_OK &&
              wolf_returned(e) && e->state.concrete_random_calls == 0 &&
              mc_test_java_random_equal(p->random, &before) && p->mob->living.ae == 123 &&
              p->base.velocity_x == .25 && p->base.velocity_y == -.375 &&
              p->base.velocity_z == -.125);
    }
    for (size_t i = 0; i < 2; ++i) {
        /* Random miss still consumes once; Random hit while airborne branches
         * directly to199 without target dereference or close-attack fallback. */
        CHECK(wolf_attack_reset(f, p, target, i == 0 ? 1 : 0, i == 0, 3, 0, 4));
        mc_java_random expected = *p->random;
        int32_t choice;
        CHECK(mc_java_random_next_bounded(&expected, 10, &choice) && choice == (i == 0 ? 5 : 0));
        CHECK(mt_wolf_attack_component(p, NULL, NULL, NULL, NULL, 3.f, e) == MC_ENTITY_OK &&
              wolf_returned(e) && e->state.concrete_random_calls == 1 &&
              mc_test_java_random_equal(p->random, &expected) && p->mob->living.ae == 123 &&
              p->base.velocity_x == .25 && p->base.velocity_y == -.375 &&
              p->base.velocity_z == -.125);
    }
    /* Source double sqrt narrows once to Float before widening for division.
     * Ordinary C volatile operations independently evaluate the original order. */
    CHECK(wolf_attack_reset(f, p, target, 0, true, 3.3, 0, -2.8));
    volatile double dx = target->base.x - p->base.x, dz = target->base.z - p->base.z;
    volatile double xx = dx * dx, zz = dz * dz, squared = xx + zz;
    volatile float length = (float)sqrt(squared);
    volatile double x0 = dx / (double)length, z0 = dz / (double)length;
    volatile double x1 = x0 * .5, z1 = z0 * .5;
    volatile double x2 = x1 * .800000011920929, z2 = z1 * .800000011920929;
    volatile double x3 = .25 * .20000000298023224, z3 = -.125 * .20000000298023224;
    volatile double expected_x = x2 + x3, expected_z = z2 + z3;
    mc_java_random expected = *p->random;
    int32_t choice;
    CHECK(mc_java_random_next_bounded(&expected, 10, &choice) && choice == 0);
    const mc_java_random target_random = *target->random;
    const uint64_t world_random = f->world->fields.r->state;
    const uint64_t unique = atomic_load(&f->profile->uniquifier);
    CHECK(mt_wolf_attack_component(p, f->registry, f->math, NULL, target, 4.f, e) == MC_ENTITY_OK &&
          wolf_returned(e) && e->state.concrete_random_calls == 1 &&
          mc_test_java_random_equal(p->random, &expected) &&
          double_bits(p->base.velocity_x) == double_bits(expected_x) &&
          double_bits(p->base.velocity_z) == double_bits(expected_z) &&
          double_bits(p->base.velocity_y) == double_bits(.4000000059604645) &&
          p->mob->living.ae == 123 && mc_test_java_random_equal(target->random, &target_random) &&
          f->world->fields.r->state == world_random &&
          atomic_load(&f->profile->uniquifier) == unique);
    CHECK(wolf_attack_reset(f, p, target, 0, true, 0, 3, 0));
    CHECK(mt_wolf_attack_component(p, f->registry, f->math, NULL, target, 3.f, e) == MC_ENTITY_OK &&
          wolf_returned(e) && isnan(p->base.velocity_x) && isnan(p->base.velocity_z) &&
          p->base.velocity_y == .4000000059604645);
    CHECK(mc_math_classes_state(f->math, &math_after, NULL) == MC_MATH_CLASS_OK &&
          math_before.holds == math_after.holds && p->holds == actor_holds &&
          target->holds == target_holds && f->registry->runtime_borrows == registry_holds &&
          !p->mob->tick_active);
    passed = true;
done:
    return passed;
}

static bool wolf_attack_failures(random_fixture *f, mc_entity *p, mc_entity *target,
                                 mc_mob_tick_error *e) {
    bool passed = false;
    CHECK(wolf_attack_reset(f, p, target, 0, true, 3, 0, 4));
    mc_java_random expected = *p->random;
    int32_t choice;
    CHECK(mc_java_random_next_bounded(&expected, 10, &choice) && choice == 0);
    CHECK(mt_wolf_attack_component(p, f->registry, f->math, NULL, NULL, 3.f, e) ==
              MC_ENTITY_STATE_ERROR &&
          e->source_exception && e->site.pc == 33 &&
          !strcmp(e->exception_class, "java/lang/NullPointerException") && !e->state.returned &&
          !e->state.attack.returned && mc_test_java_random_equal(p->random, &expected) &&
          p->base.velocity_x == .25);
    CHECK(wolf_attack_reset(f, p, target, 0, true, 3, 0, 4));
    CHECK(mt_wolf_attack_component(p, NULL, NULL, NULL, target, 3.f, e) == MC_ENTITY_STATE_ERROR &&
          e->native_policy && !e->source_exception && e->site.pc == 62 &&
          e->state.concrete_random_calls == 1 && p->base.velocity_x == .25 &&
          p->base.velocity_y == -.375 && p->base.velocity_z == -.125 && p->mob->living.ae == 123);
    CHECK(wolf_attack_reset(f, p, target, 0, true, 3, 0, 4));
    const mc_java_random before = *p->random;
    const unsigned int csr = _mm_getcsr();
    _mm_setcsr(csr | 0x40u);
    const mc_entity_status status = mt_wolf_attack_component(p, NULL, NULL, NULL, target, 6.f, e);
    _mm_setcsr(csr);
    CHECK(status == MC_ENTITY_UNSUPPORTED_FP && e->site.pc == 2 && !e->state.returned &&
          mc_test_java_random_equal(p->random, &before) && p->base.velocity_x == .25 &&
          p->mob->living.ae == 123);
    CHECK(wolf_attack_reset(f, p, target, 0, true, 0, 0, 0));
    mc_entity_bounds *target_bounds = target->bounds;
    target->bounds = NULL; /* Declared nullable source field, restored only for disposal. */
    const mc_entity_status bounds_status =
        mt_wolf_attack_component(p, f->registry, f->math, NULL, target, 1.f, e);
    const bool bounds_prefix = bounds_status == MC_ENTITY_STATE_ERROR && e->source_exception &&
                               e->site.pc == 143 && !e->state.returned && p->mob->living.ae == 123;
    target->bounds = target_bounds;
    CHECK(bounds_prefix);
    /* Strict overlap boundaries skip both tame read and target damage. */
    const double saved_max = target->bounds->max_y, saved_min = target->bounds->min_y;
    target->bounds->max_y = p->bounds->min_y;
    const mc_entity_status upper_status =
        mt_wolf_attack_component(p, NULL, NULL, NULL, target, 1.f, e);
    const bool upper_prefix =
        upper_status == MC_ENTITY_OK && wolf_returned(e) && p->mob->living.ae == 123;
    target->bounds->max_y = saved_max;
    target->bounds->min_y = p->bounds->max_y;
    const mc_entity_status lower_status =
        mt_wolf_attack_component(p, NULL, NULL, NULL, target, 1.f, e);
    const bool lower_prefix =
        lower_status == MC_ENTITY_OK && wolf_returned(e) && p->mob->living.ae == 123;
    target->bounds->min_y = saved_min;
    CHECK(upper_prefix && lower_prefix);
    passed = true;
done:
    return passed;
}

static bool wolf_attack_bites(random_fixture *f, mc_entity *p, mc_entity *sheep,
                              mc_mob_tick_error *e) {
    bool passed = false;
    mc_entity *item = NULL;
    CHECK(mc_item_entity_allocate(f->entities, &item, NULL) == MC_ENTITY_OK &&
          mc_item_entity_construct(item, f->world, 8.5, 64.5, 8.5, NULL, NULL) == MC_ENTITY_OK);
    for (size_t i = 0; i < 2; ++i) {
        CHECK(wolf_attack_reset(f, p, item, 38, true, 0, 0, 0));
        CHECK(mt_wolf_tame_component(p, f->registry, f->math, NULL, i != 0, e) == MC_ENTITY_OK);
        item->item.f = 5; /* Declared actual Item health input, no damage result supplied. */
        const mc_java_random before = *p->random;
        CHECK(mt_wolf_attack_component(p, f->registry, f->math, NULL, item, 1.f, e) ==
                  MC_ENTITY_OK &&
              wolf_returned(e) && p->mob->living.ae == 20 && item->item.f == (i == 0 ? 3 : 1) &&
              !item->base.be && e->state.concrete_random_calls == 0 &&
              mc_test_java_random_equal(p->random, &before));
    }
    CHECK(wolf_attack_reset(f, p, sheep, 38, true, 0, 0, 0));
    CHECK(mt_wolf_tame_component(p, f->registry, f->math, NULL, false, e) == MC_ENTITY_OK);
    const int32_t health = sheep->mob->living.Y;
    CHECK(mt_wolf_attack_component(p, f->registry, f->math, NULL, sheep, 1.f, e) == MC_ENTITY_OK &&
          wolf_returned(e) && e->child == MC_MOB_TICK_CHILD_NONE && p->mob->living.ae == 20 &&
          sheep->mob->living.Y == health - 2 && sheep->mob->living.Z == health &&
          sheep->mob->living.au == 2 && sheep->mob->living.aa == 10 &&
          sheep->mob->living.ab == 10 && sheep->base.by == sheep->mob->living.E && sheep->base.bb);
    /* The source attack cooldown store also precedes a failed tame getter. */
    mc_entity_watch_record *flags = wolf_attack_flags(p);
    CHECK(flags != NULL);
    const int32_t type = flags->type;
    flags->type = 2;
    const mc_entity_status wrong =
        mt_wolf_attack_component(p, f->registry, f->math, NULL, sheep, 1.f, e);
    const bool wrong_prefix =
        wrong == MC_ENTITY_STATE_ERROR && e->source_exception && e->site.pc == 184 &&
        !strcmp(e->exception_class, "java/lang/ClassCastException") && e->child_site.pc == 19 &&
        p->mob->living.ae == 20 && sheep->mob->living.Y == health - 2;
    flags->type = type;
    CHECK(wrong_prefix);
    passed = true;
done:
    mc_entity_release(item);
    return passed;
}

typedef struct wolf_reference_profile {
    int64_t seed;
    uint32_t distance;
    bool ground;
    unsigned int point;
    bool tame;
    unsigned int target_mode, bounds_mode;
    int32_t cooldown;
} wolf_reference_profile;

static const wolf_reference_profile wolf_reference_profiles[] = {
    {0, 0x3fc00000, true, 0, false, 0, 0, 123},  {0, 0x40000000, true, 0, false, 0, 0, 123},
    {0, 0x40c00000, true, 0, false, 0, 0, 123},  {0, 0x7f800000, true, 0, false, 0, 0, 123},
    {0, 0x7fc12345, true, 0, false, 0, 0, 123},  {1, 0x40400000, true, 0, false, 0, 0, 123},
    {0, 0x40400000, false, 0, false, 0, 0, 123}, {0, 0x40800000, true, 1, false, 0, 0, 123},
    {0, 0x40400000, true, 2, false, 0, 0, 123},  {0, 0x40400000, true, 5, false, 0, 0, 123},
    {0, 0x40bfffff, true, 6, false, 0, 0, 123},  {0, 0x40000001, true, 0, false, 0, 0, 123},
    {38, 0x3f800000, true, 3, false, 0, 0, 123}, {38, 0x3f800000, true, 3, true, 0, 0, -7},
    {38, 0x3fbfffff, true, 3, false, 0, 0, 123}, {38, 0x3f800000, true, 3, false, 0, 3, 123},
    {38, 0x3f800000, true, 3, false, 0, 4, 123}, {0, 0x40400000, true, 0, false, 1, 0, 123},
    {38, 0x3f800000, true, 3, false, 1, 0, 123}, {38, 0x3f800000, true, 3, false, 0, 1, 123},
    {38, 0x3f800000, true, 3, false, 0, 2, 123}, {0, 0x40400000, true, 1, false, 0, 1, 123},
    {38, 0xbf800000, true, 3, false, 0, 0, 123}, {38, 0x3f800000, true, 4, false, 0, 0, 123},
};

static void wolf_reference_vector(FILE *out, const double *values, size_t count) {
    fputc('[', out);
    for (size_t i = 0; i < count; ++i)
        fprintf(out, "%s\"%016" PRIx64 "\"", i == 0 ? "" : ",", double_bits(values[i]));
    fputc(']', out);
}

static void wolf_reference_box(FILE *out, const mc_entity_bounds *box) {
    if (box == NULL) {
        fputs("null", out);
        return;
    }
    const double values[] = {box->min_x, box->min_y, box->min_z,
                             box->max_x, box->max_y, box->max_z};
    wolf_reference_vector(out, values, COUNT(values));
}

static void wolf_reference_set_box(mc_entity_bounds *box, double x0, double y0, double z0,
                                   double x1, double y1, double z1) {
    box->min_x = x0;
    box->min_y = y0;
    box->min_z = z0;
    box->max_x = x1;
    box->max_y = y1;
    box->max_z = z1;
}

/* Separate optional CPU comparison profiles. Both attack owners are genuinely
 * constructed with NULL World; the actual World fixture supplies only their
 * shared source Registry/Math class namespace. No World field is rebound. */
static bool wolf_reference_one(random_fixture *f, FILE *out, size_t index, mc_mob_tick_error *error,
                               mc_mob_tick_error *cleanup) {
    bool passed = false;
    const wolf_reference_profile *profile = &wolf_reference_profiles[index];
    static const double points[][3] = {
        {3, 0, 4},          {3.3, 0, -2.8},           {0, 3, 0}, {0, 0, 0}, {0, 2, 0},
        {-5.25, -1, 2.125}, {16777215.75, 0, -256.75}};
    mc_entity *actor = NULL, *target = NULL;
    mc_entity_bounds *actor_box = NULL, *target_box = NULL;
    const mc_entity_class_literal *type = NULL;
    mc_mob_error construction;
    CHECK(mc_entity_class_literal_get(f->entities, MC_ENTITY_CLASS_KIND_WOLF, &type) ==
              MC_ENTITY_OK &&
          mc_mob_allocate(f->entities, f->registry, type, &actor, &construction) == MC_MOB_OK &&
          mc_mob_construct(actor, NULL, &construction) == MC_MOB_OK &&
          mc_falling_entity_allocate(f->entities, &target, NULL) == MC_ENTITY_OK &&
          mc_falling_entity_construct_world(target, NULL, NULL) == MC_ENTITY_OK);
    CHECK(mt_wolf_tame_component(actor, f->registry, f->math, NULL, profile->tame, error) ==
          MC_ENTITY_OK);
    const double tx = 8.5 + points[profile->point][0], ty = 64.5 + points[profile->point][1],
                 tz = 8.5 + points[profile->point][2];
    CHECK(mc_entity_set_position(actor, 8.5, 64.5, 8.5) == MC_ENTITY_OK &&
          mc_entity_set_position(target, tx, ty, tz) == MC_ENTITY_OK &&
          mc_entity_set_velocity(actor, 0, .25) == MC_ENTITY_OK &&
          mc_entity_set_velocity(actor, 1, -.375) == MC_ENTITY_OK &&
          mc_entity_set_velocity(actor, 2, -.125) == MC_ENTITY_OK &&
          mc_entity_set_velocity(target, 0, -.5) == MC_ENTITY_OK &&
          mc_entity_set_velocity(target, 1, .75) == MC_ENTITY_OK &&
          mc_entity_set_velocity(target, 2, 1.25) == MC_ENTITY_OK);
    actor->base.aX = profile->ground;
    actor->mob->living.ae = profile->cooldown;
    target->falling.a = 12;
    target->falling.b = 7;
    target->base.be = target->base.bb = false;
    actor_box = actor->bounds;
    target_box = target->bounds;
    CHECK(actor_box != NULL && target_box != NULL);
    wolf_reference_set_box(actor_box, 8, 64, 8, 9, 65, 9);
    wolf_reference_set_box(target_box, tx - .5, ty - .5, tz - .5, tx + .5, ty + .5, tz + .5);
    if (profile->bounds_mode == 1)
        target->bounds = NULL;
    if (profile->bounds_mode == 2)
        actor->bounds = NULL;
    if (profile->bounds_mode == 3)
        target_box->max_y = 64;
    if (profile->bounds_mode == 4)
        target_box->min_y = 65;
    mc_java_random_reseed(actor->random, profile->seed);
    mc_java_random_reseed(target->random, 12345);
    const mc_java_random *actor_random_identity = actor->random;
    const mc_java_random *target_random_identity = target->random;
    const mc_java_random before_random = *target->random;
    const mc_entity_bounds before_box = *target_box;
    const mc_entity_bounds *visible_box = target->bounds;
    const size_t actor_holds = actor->holds, target_holds = target->holds;
    float distance;
    memcpy(&distance, &profile->distance, sizeof(distance));
    const mc_entity_status status =
        mt_wolf_attack_component(actor, f->registry, f->math, NULL,
                                 profile->target_mode == 0 ? target : NULL, distance, error);
    const bool npe = error->source_exception && error->exception_class != NULL &&
                     strcmp(error->exception_class, "java/lang/NullPointerException") == 0;
    CHECK((status == MC_ENTITY_OK && wolf_returned(error)) ||
          (status == MC_ENTITY_STATE_ERROR && npe && !error->state.returned));
    const bool unchanged =
        target->base.x == tx && target->base.y == ty && target->base.z == tz &&
        target->base.velocity_x == -.5 && target->base.velocity_y == .75 &&
        target->base.velocity_z == 1.25 && target->falling.a == 12 && target->falling.b == 7 &&
        !target->base.be && target->bounds == visible_box &&
        target_box->min_x == before_box.min_x && target_box->min_y == before_box.min_y &&
        target_box->min_z == before_box.min_z && target_box->max_x == before_box.max_x &&
        target_box->max_y == before_box.max_y && target_box->max_z == before_box.max_z &&
        target->random == target_random_identity &&
        mc_test_java_random_equal(target->random, &before_random);
    CHECK(unchanged && actor->random == actor_random_identity && actor->holds == actor_holds &&
          target->holds == target_holds && actor->base.world == NULL && target->base.world == NULL);
    fprintf(out,
            "{\"case\":%zu,\"profile\":[%" PRId64 ",\"%08" PRIx32 "\",%d,%u,%d,%u,%u,%" PRId32
            "],\"exception\":\"%s\",\"velocity\":",
            index, profile->seed, profile->distance, profile->ground ? 1 : 0, profile->point,
            profile->tame ? 1 : 0, profile->target_mode, profile->bounds_mode, profile->cooldown,
            npe ? "NPE" : "NONE");
    const double velocity[] = {actor->base.velocity_x, actor->base.velocity_y,
                               actor->base.velocity_z};
    wolf_reference_vector(out, velocity, COUNT(velocity));
    fprintf(out,
            ",\"cooldown\":%" PRId32 ",\"random\":\"%016" PRIx64
            "\",\"target_unchanged\":%s,\"target_attacked\":%s,\"actor_position\":",
            actor->mob->living.ae, actor->random->state, unchanged ? "true" : "false",
            target->base.bb ? "true" : "false");
    const double actor_position[] = {actor->base.x, actor->base.y, actor->base.z};
    const double target_position[] = {target->base.x, target->base.y, target->base.z};
    wolf_reference_vector(out, actor_position, COUNT(actor_position));
    fputs(",\"target_position\":", out);
    wolf_reference_vector(out, target_position, COUNT(target_position));
    fputs(",\"actor_bounds\":", out);
    wolf_reference_box(out, actor->bounds);
    fputs(",\"target_bounds\":", out);
    wolf_reference_box(out, target->bounds);
    fputs("}\n", out);
    CHECK(fflush(out) == 0);
    passed = true;
done:
    /* Restore only private allocation pointers after observing the actual
     * source failure. No result, source field or call prefix is repaired. */
    if (actor != NULL) {
        if (actor_box != NULL)
            actor->bounds = actor_box;
        if (mc_mob_tick_cleanup(actor, cleanup) != MC_ENTITY_OK)
            passed = false;
        if (mc_mob_retire(&actor, &construction) != MC_MOB_OK)
            passed = false;
    }
    if (target != NULL && target_box != NULL)
        target->bounds = target_box;
    mc_entity_release(target);
    return passed;
}

int main(int argc, char **argv) {
    if (argc != 3 && !(argc == 5 && strcmp(argv[3], "--reference") == 0))
        return 2;
    random_fixture *f = calloc(1, sizeof(*f));
    mc_entity *wolf = NULL, *sheep = NULL;
    mc_mob_tick_error *error = calloc(1, sizeof(*error)), *cleanup = calloc(1, sizeof(*cleanup));
    bool passed = f != NULL && error != NULL && cleanup != NULL && setup(f, argv[1], argv[2]);
    if (passed && argc == 5) {
        FILE *out = open_file(argv[4], "wb");
        passed = out != NULL;
        for (size_t i = 0; passed && i < COUNT(wolf_reference_profiles); ++i)
            passed = wolf_reference_one(f, out, i, error, cleanup);
        if (out != NULL && fclose(out) != 0)
            passed = false;
    } else if (passed)
        passed = wolf_actor(f, MC_ENTITY_CLASS_KIND_WOLF, &wolf) &&
                 wolf_actor(f, MC_ENTITY_CLASS_KIND_SHEEP, &sheep) &&
                 wolf_attack_paths(f, wolf, sheep, error) &&
                 wolf_attack_failures(f, wolf, sheep, error) &&
                 wolf_attack_bites(f, wolf, sheep, error);
    if (wolf != NULL && mc_mob_tick_cleanup(wolf, cleanup) != MC_ENTITY_OK)
        passed = false;
    mc_mob_error retired;
    if ((wolf != NULL && mc_mob_retire(&wolf, &retired) != MC_MOB_OK) ||
        (sheep != NULL && mc_mob_retire(&sheep, &retired) != MC_MOB_OK))
        passed = false;
    if (f != NULL && !teardown(f))
        passed = false;
    free(cleanup);
    free(error);
    free(f);
    if (passed)
        printf("PASS: %u Wolf attack source-component checks\n", checks);
    return passed ? 0 : 1;
}
