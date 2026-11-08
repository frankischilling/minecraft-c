/* Native component controls. Constructor clocks remain actual clocks; seeded
 * heading inputs below are explicit call inputs, not original observations. */
int mc_arrow_unused_statistics_main(int, char **);
#define main mc_arrow_unused_statistics_main
#include "../client/statistics/test_player_dispatch.c"
#undef main

#include "core/java/random_profile_internal.h"
#include "entity/action/action_internal.h"
#include "entity/arrow/internal.h"
#include "entity/arrow/references_internal.h"
#include "entity/construction_internal.h"
#include "entity/mob/internal.h"
#include "entity/position_internal.h"
#include "mc/entity/hit_result.h"
#include "mc/entity/mob/construction.h"
#include "world/runtime/runtime_internal.h"
#include <math.h>
#include <xmmintrin.h>

typedef struct arrow_fixture {
    fixture base;
    mc_math_classes *math;
    mc_entity *shooters[3];
} arrow_fixture;

/* Reused by the separately compiled native reference adapter. This creates
 * real owners without assigning a reference scenario's pose or Random input. */
static bool ac_setup(arrow_fixture *f, const char *resources, const char *directory) {
    CHECK(setup(&f->base, resources, directory, true) &&
          mc_math_classes_create(SIZE_MAX, &f->math, NULL) == MC_MATH_CLASS_OK &&
          f->base.app.profile->math != NULL);
    return true;
}

static bool ac_prepare_shooters(arrow_fixture *f) {
    static const mc_entity_class_kind kinds[] = {
        MC_ENTITY_CLASS_KIND_SKELETON, MC_ENTITY_CLASS_KIND_WOLF, MC_ENTITY_CLASS_KIND_GHAST};
    for (size_t i = 0; i < 3; ++i) {
        const mc_entity_class_literal *literal = NULL;
        mc_mob_error error;
        CHECK(f->shooters[i] == NULL &&
              mc_entity_class_literal_get(f->base.app.entities, kinds[i], &literal) ==
                  MC_ENTITY_OK &&
              mc_mob_allocate(f->base.app.entities, f->base.app.registry, literal, &f->shooters[i],
                              &error) == MC_MOB_OK &&
              mc_mob_construct(f->shooters[i], f->base.world, &error) == MC_MOB_OK);
    }
    return true;
}

static bool ac_finish(arrow_fixture *f) {
    for (size_t i = 0; i < 3; ++i) {
        mc_entity_release(f->shooters[i]);
        f->shooters[i] = NULL;
    }
    CHECK(mc_math_classes_release(&f->math, NULL) == MC_MATH_CLASS_OK && teardown(&f->base));
    return true;
}

static uint64_t ac_dbits(double value) {
    uint64_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static bool ac_random_equal(const mc_java_random *a, const mc_java_random *b) {
    return a->state == b->state &&
           ac_dbits(a->next_next_gaussian) == ac_dbits(b->next_next_gaussian) &&
           a->have_next_next_gaussian == b->have_next_next_gaussian;
}

static bool ac_gaussian_receipt(const mc_arrow_error *e, const mc_java_random *actual) {
    CHECK(e->random_captured && e->gaussian_calls == 3);
    mc_java_random expected = e->random_before;
    for (size_t i = 0; i < 3; ++i) {
        const double value = mc_java_random_next_gaussian(&expected);
        CHECK(ac_dbits(value) == ac_dbits(e->gaussian_values[i]));
    }
    CHECK(ac_random_equal(&expected, &e->random_after) &&
          ac_random_equal(actual, &e->random_after));
    return true;
}

static bool ac_fields(const mc_arrow_fields *v) {
    CHECK(!v->a && v->b == 0 && v->c == NULL && v->d == -1 && v->e == -1 && v->f == -1 &&
          v->g == 0 && v->h == 0 && !v->i && v->j == 0 && v->k == 0);
    return true;
}

static bool ac_allocate(arrow_fixture *f, mc_entity **out) {
    CHECK(mc_arrow_allocate(f->base.app.entities, out, NULL) == MC_ENTITY_OK && *out != NULL &&
          mc_entity_has_arrow(*out) && (*out)->phase == MC_ENTITY_RAW && (*out)->mob == NULL &&
          (*out)->local_player == NULL && (*out)->random == NULL && (*out)->bounds == NULL &&
          (*out)->watcher == NULL && (*out)->arrow->fields.c == NULL &&
          (*out)->arrow->shooter_hold == NULL);
    return true;
}

static bool ac_plain(arrow_fixture *f) {
    mc_entity_class_snapshot before, allocated;
    mc_math_class_state math;
    CHECK(mc_entity_classes_get_snapshot(f->base.app.entities, &before) == MC_ENTITY_OK);
    const uint64_t uniquifier = atomic_load(&f->base.app.profile->uniquifier);
    const uint64_t shared = atomic_load(&f->base.app.profile->math->state);
    mc_entity *a = NULL, *b = NULL;
    CHECK(ac_allocate(f, &a) &&
          mc_entity_classes_get_snapshot(f->base.app.entities, &allocated) == MC_ENTITY_OK &&
          allocated.next_id == before.next_id &&
          allocated.live_entities == before.live_entities + 1 &&
          atomic_load(&f->base.app.profile->uniquifier) == uniquifier);
    mc_arrow_error error;
    memset(&error, 0xa5, sizeof(error));
    CHECK(mc_arrow_construct_world(a, NULL, &error) == MC_ENTITY_OK && error.site.pc == 58 &&
          a->phase == MC_ENTITY_READY && a->arrow->constructor_pc == 58 &&
          !a->arrow->constructor_active && !error.heading_entered && error.gaussian_calls == 0 &&
          a->base.id == before.next_id && a->base.world == NULL &&
          atomic_load(&f->base.app.profile->uniquifier) ==
              uniquifier * UINT64_C(1181783497276652981) &&
          atomic_load(&f->base.app.profile->math->state) == shared && ac_fields(&a->arrow->fields));
    CHECK(a->base.width == .5f && a->base.height == .5f && a->base.offset == 0 &&
          a->bounds->min_x == -(double)(.6f / 2.f) && a->bounds->max_x == (double)(.6f / 2.f) &&
          a->bounds->min_y == 0 && a->bounds->max_y == (double)1.8f && a->base.aJ == 0 &&
          a->base.aK == 0 && a->base.aL == 0 && a->random != f->base.player->random &&
          a->random != f->base.world->fields.r && !a->random->have_next_next_gaussian &&
          ac_dbits(a->random->next_next_gaussian) == 0);
    mc_entity_watcher_snapshot watcher;
    CHECK(mc_entity_watcher_get_snapshot(a->watcher, &watcher) == MC_ENTITY_OK &&
          watcher.size == 1 && watcher.index == 0 && watcher.type == 0 && watcher.byte_value == 0);
    CHECK(ac_allocate(f, &b) &&
          mc_arrow_construct_position(b, f->base.world, 7, -3, 11, &error) == MC_ENTITY_OK &&
          error.site.pc == 72 && b->arrow->constructor_pc == 72 && b->random != a->random &&
          b->base.world == f->base.world && b->base.x == 7 && b->base.y == -3 && b->base.z == 11 &&
          b->bounds->min_x == 6.75 && b->bounds->max_y == -2.5 && b->bounds->max_z == 11.25 &&
          b->base.aJ == 0 && b->base.aK == 0 && b->base.aL == 0 && b->base.bl == 0 &&
          b->base.bm == 0 && b->base.bn == 0 && ac_fields(&b->arrow->fields));
    mc_arrow_state state;
    CHECK(mc_arrow_inspect(b, &state) && state.phase == MC_ENTITY_READY &&
          state.constructor_pc == 72 && !state.constructor_active && !state.heading_active &&
          mc_math_classes_state(f->math, &math, NULL) == MC_MATH_CLASS_OK &&
          math.phase == MC_MATH_CLASS_COLD && math.source_a == NULL &&
          mc_arrow_construct_world(b, NULL, &error) == MC_ENTITY_INVALID_PHASE &&
          b->phase == MC_ENTITY_READY && b->base.x == 7);
    mc_entity_release(a);
    mc_entity_release(b);
    CHECK(ac_allocate(f, &a) && mc_arrow_construct_world(a, NULL, NULL) == MC_ENTITY_OK);
    mc_entity_release(a);
    return true;
}

static bool ac_constructor_failures(arrow_fixture *f) {
    mc_entity *a = NULL;
    mc_arrow_error e;
    mc_math_class_state math;
    CHECK(ac_allocate(f, &a));
    const size_t player_holds = f->base.player->holds;
    CHECK(mc_arrow_construct_living(a, NULL, NULL, f->math, &e) == MC_ENTITY_STATE_ERROR &&
          e.source_exception && !e.native_policy && e.exception_class != NULL &&
          strcmp(e.exception_class, "java/lang/NullPointerException") == 0 && e.site.pc == 73 &&
          !e.heading_entered && e.gaussian_calls == 0 &&
          a->phase == MC_ENTITY_FAILED_CONSTRUCTION && a->arrow->constructor_pc == 68 &&
          a->base.width == .5f && a->base.height == .5f && ac_fields(&a->arrow->fields) &&
          f->base.player->holds == player_holds &&
          mc_math_classes_state(f->math, &math, NULL) == MC_MATH_CLASS_OK &&
          math.phase == MC_MATH_CLASS_COLD && math.source_a == NULL);
    const int32_t id = a->base.id;
    CHECK(mc_arrow_construct_world(a, NULL, &e) == MC_ENTITY_INVALID_PHASE && a->base.id == id);
    mc_entity_release(a);
    CHECK(ac_allocate(f, &a) &&
          mc_arrow_construct_living(a, NULL, NULL, NULL, NULL) == MC_ENTITY_STATE_ERROR &&
          a->phase == MC_ENTITY_FAILED_CONSTRUCTION && a->arrow->failure.site.pc == 73 &&
          a->arrow->failure.source_exception);
    mc_entity_release(a);

    CHECK(ac_allocate(f, &a));
    const size_t holds = f->base.player->holds;
    f->base.player->holds = SIZE_MAX; /* Reached native occurrence refusal, no Java exception. */
    const mc_entity_status refused =
        mc_arrow_construct_living(a, NULL, f->base.player, f->math, &e);
    f->base.player->holds = holds;
    CHECK(refused == MC_ENTITY_LIMIT && e.native_policy && !e.source_exception && e.site.pc == 52 &&
          a->phase == MC_ENTITY_FAILED_CONSTRUCTION && a->arrow->constructor_pc == 47 &&
          a->arrow->fields.c == NULL && a->arrow->shooter_hold == NULL && a->base.width == .6f &&
          a->base.height == 1.8f && a->random != NULL && a->watcher != NULL);
    mc_entity_release(a);
    return true;
}

static bool ac_living(arrow_fixture *f) {
    CHECK(ac_prepare_shooters(f));
    for (size_t i = 0; i < 4; ++i) {
        mc_entity *shooter = i == 0 ? f->base.player : f->shooters[i - 1];
        CHECK(mc_entity_set_position(shooter, 4, 9, -2) == MC_ENTITY_OK);
        shooter->base.yaw = 0;
        shooter->base.aT = 0;
        const float eye = i == 0 ? .12f : shooter->base.height * (i == 2 ? .8f : .85f);
        const double initial_y = 9 + (double)eye;
        const size_t held = shooter->holds;
        const mc_java_random donor_random = *shooter->random;
        const uint64_t shared = atomic_load(&f->base.app.profile->math->state);
        mc_entity *a = NULL;
        mc_arrow_error e;
        CHECK(ac_allocate(f, &a) &&
              mc_arrow_construct_living(a, NULL, shooter, f->math, &e) == MC_ENTITY_OK &&
              e.site.pc == 289 && e.heading_entered && e.heading_returned &&
              e.gaussian_calls == 3 && a->arrow->constructor_pc == 289 &&
              a->arrow->heading_pc == 198 && a->phase == MC_ENTITY_READY &&
              a->arrow->fields.c == shooter && a->arrow->shooter_hold == shooter &&
              a->arrow->fields.a == (i == 0) && shooter->holds == held + 1 &&
              a->random != shooter->random && ac_random_equal(shooter->random, &donor_random) &&
              atomic_load(&f->base.app.profile->math->state) == shared &&
              ac_gaussian_receipt(&e, a->random));
        CHECK(a->base.aJ == 4 && a->base.bl == 4 && a->base.aK == initial_y &&
              a->base.bm == initial_y && a->base.aL == -2 && a->base.bn == -2 &&
              a->base.x == 4 - (double).16f && a->base.y == initial_y - (double).1f &&
              a->base.z == -2 && a->base.width == .5f && a->base.height == .5f &&
              a->bounds->min_x == a->base.x - .25 && a->bounds->max_y == a->base.y + .5 &&
              a->bounds->max_z == a->base.z + .25 && a->base.yaw == a->base.aU &&
              a->base.aT == a->base.aV && a->random->have_next_next_gaussian &&
              a->arrow->fields.j == 0 && a->arrow->fields.k == 0);
        mc_entity_release(a);
        CHECK(shooter->holds == held);
    }
    /* Missing Math is unread until116: c/flag/size/pose are already committed. */
    mc_entity *a = NULL;
    mc_arrow_error e;
    const size_t holds = f->base.player->holds;
    CHECK(ac_allocate(f, &a) &&
          mc_arrow_construct_living(a, NULL, f->base.player, NULL, &e) != MC_ENTITY_OK &&
          e.native_policy && !e.source_exception && e.site.pc == 116 && e.gaussian_calls == 0 &&
          a->phase == MC_ENTITY_FAILED_CONSTRUCTION && a->arrow->constructor_pc == 98 &&
          a->arrow->fields.c == f->base.player && a->arrow->fields.a &&
          f->base.player->holds == holds + 1 && a->base.x == 4 && a->base.y == 9 + (double).12f &&
          a->base.aK == a->base.y && a->base.velocity_x == 0 &&
          !a->random->have_next_next_gaussian);
    mc_entity_release(a);
    CHECK(f->base.player->holds == holds);
    return true;
}

static bool ac_heading_inputs(arrow_fixture *f) {
    const struct heading_input {
        double x, y, z;
        float speed, spread;
    } inputs[] = {{3, 4, 0, 2, 0},        {0, 0, 0, 1, 0},        {1, 2, 3, 0, 0},
                  {1, -2, 3, -2, 12},     {INFINITY, 0, 1, 1, 1}, {1, NAN, 0, 1, 1},
                  {1, 0, 0, INFINITY, 0}, {1, 2, 3, 1, NAN}};

    mc_entity *a = NULL;
    CHECK(ac_allocate(f, &a) && mc_arrow_construct_world(a, NULL, NULL) == MC_ENTITY_OK);
    for (size_t i = 0; i < sizeof(inputs) / sizeof(*inputs); ++i) {
        const struct heading_input *v = &inputs[i];
        /* Declared seeded heading input. Alternate an existing cached Gaussian. */
        mc_java_random_seed(a->random, (int64_t)(918273 + i));
        if ((i & 1u) != 0)
            (void)mc_java_random_next_gaussian(a->random);
        mc_java_random expected = *a->random;
        for (unsigned int draw = 0; draw < 3; ++draw)
            (void)mc_java_random_next_gaussian(&expected);
        a->arrow->fields.j = 27;
        a->arrow->fields.k = 91;
        const mc_entity_bounds bounds = *a->bounds;
        const mc_java_random shooter = *f->base.player->random;
        const uint64_t shared = atomic_load(&f->base.app.profile->math->state);
        mc_arrow_error e;
        CHECK(mc_arrow_heading(a, f->math, v->x, v->y, v->z, v->speed, v->spread, &e) ==
                  MC_ENTITY_OK &&
              e.heading_entered && e.heading_returned && e.gaussian_calls == 3 &&
              e.site.pc == 198 && a->arrow->heading_pc == 198 && a->arrow->fields.j == 0 &&
              a->arrow->fields.k == 91 && ac_random_equal(a->random, &expected) &&
              ac_gaussian_receipt(&e, a->random) &&
              ac_random_equal(f->base.player->random, &shooter) &&
              atomic_load(&f->base.app.profile->math->state) == shared &&
              memcmp(a->bounds, &bounds, sizeof(bounds)) == 0 && !a->arrow->heading_active &&
              (a->base.yaw == a->base.aU || (isnan(a->base.yaw) && isnan(a->base.aU))) &&
              (a->base.aT == a->base.aV || (isnan(a->base.aT) && isnan(a->base.aV))));
        if (i == 0)
            CHECK(a->base.velocity_x == 1.2 && a->base.velocity_y == 1.6 &&
                  a->base.velocity_z == 0 && a->base.yaw == 90.f);
        if (i == 1)
            CHECK(isnan(a->base.velocity_x) && isnan(a->base.velocity_y) &&
                  isnan(a->base.velocity_z));
        if (i == 2)
            CHECK(a->base.velocity_x == 0 && a->base.velocity_y == 0 && a->base.velocity_z == 0);
    }
    CHECK(mc_arrow_heading(a, f->math, 1, 0, 0, 1, 0, NULL) == MC_ENTITY_OK);
    mc_entity_release(a);
    return true;
}

static bool ac_heading_failures(arrow_fixture *f) {
    mc_entity *a = NULL;
    CHECK(ac_allocate(f, &a) && mc_arrow_construct_world(a, NULL, NULL) == MC_ENTITY_OK);
    a->base.velocity_x = 7;
    a->base.velocity_y = 8;
    a->base.velocity_z = 9;
    a->base.yaw = 11;
    a->base.aT = 13;
    a->arrow->fields.j = 17;
    a->arrow->fields.k = 19;
    mc_java_random original = *a->random;
    mc_arrow_error e;
    CHECK(mc_arrow_heading(a, NULL, 1, 2, 3, 1, 1, &e) != MC_ENTITY_OK && e.native_policy &&
          e.gaussian_calls == 0 && a->base.velocity_x == 7 && a->base.velocity_y == 8 &&
          a->base.velocity_z == 9 && a->base.yaw == 11 && a->base.aT == 13 &&
          a->arrow->fields.j == 17 && a->arrow->fields.k == 19 &&
          ac_random_equal(a->random, &original));
    mc_java_random *random = a->random;
    a->random = NULL; /* Explicit actual source receiver fault, restored for disposal. */
    const mc_entity_status null_status = mc_arrow_heading(a, f->math, 1, 2, 3, 1, 1, &e);
    a->random = random;
    CHECK(null_status == MC_ENTITY_STATE_ERROR && e.source_exception && !e.native_policy &&
          e.gaussian_calls == 0 && e.exception_class != NULL &&
          strcmp(e.exception_class, "java/lang/NullPointerException") == 0 &&
          a->base.velocity_x == 7 && a->base.velocity_y == 8 && a->base.velocity_z == 9 &&
          a->base.yaw == 11 && a->base.aT == 13 && a->arrow->fields.j == 17 &&
          ac_random_equal(a->random, &original));
    mc_math_classes *limited = NULL;
    mc_math_class_state state;
    CHECK(mc_math_classes_create(1024, &limited, NULL) == MC_MATH_CLASS_OK &&
          mc_arrow_heading(a, limited, 1, 2, 3, 1, 1, &e) != MC_ENTITY_OK && e.native_policy &&
          e.gaussian_calls == 0 && ac_random_equal(a->random, &original) &&
          mc_math_classes_state(limited, &state, NULL) == MC_MATH_CLASS_OK &&
          state.phase == MC_MATH_CLASS_FAILED && state.source_a == NULL);
    CHECK(mc_arrow_heading(a, limited, 1, 2, 3, 1, 1, &e) != MC_ENTITY_OK &&
          e.gaussian_calls == 0 && ac_random_equal(a->random, &original) &&
          mc_math_classes_release(&limited, NULL) == MC_MATH_CLASS_OK);
    a->arrow->heading_active = true;
    CHECK(mc_arrow_heading(a, f->math, 1, 2, 3, 1, 1, &e) == MC_ENTITY_BUSY &&
          ac_random_equal(a->random, &original));
    a->arrow->heading_active = false;
    a->arrow->constructor_active = true;
    CHECK(mc_arrow_heading(a, f->math, 1, 2, 3, 1, 1, &e) == MC_ENTITY_BUSY &&
          ac_random_equal(a->random, &original));
    a->arrow->constructor_active = false;
    mc_entity_release(a);
    return true;
}

static bool ac_references(arrow_fixture *f) {
    mc_entity *a = NULL, *out = NULL;
    CHECK(ac_allocate(f, &a));
    mc_entity *first = f->shooters[0], *second = f->shooters[1];
    const size_t first_holds = first->holds, second_holds = second->holds;
    CHECK(mc_arrow_shooter_store(a, first, NULL) == MC_ENTITY_OK &&
          first->holds == first_holds + 1 && mc_arrow_shooter_get(a, &out, NULL) == MC_ENTITY_OK &&
          out == first && mc_arrow_shooter_store(a, first, NULL) == MC_ENTITY_OK &&
          first->holds == first_holds + 1 &&
          mc_arrow_shooter_store(a, a, NULL) == MC_ENTITY_INVALID_ARGUMENT &&
          a->arrow->fields.c == first && first->holds == first_holds + 1);
    second->holds = SIZE_MAX;
    const mc_entity_status refused = mc_arrow_shooter_store(a, second, NULL);
    second->holds = second_holds;
    CHECK(refused == MC_ENTITY_LIMIT && a->arrow->fields.c == first &&
          a->arrow->shooter_hold == first && first->holds == first_holds + 1);
    CHECK(mc_arrow_shooter_store(a, second, NULL) == MC_ENTITY_OK && first->holds == first_holds &&
          second->holds == second_holds + 1);
    a->arrow->fields.c = first; /* Provenance fault, never adopted as a new owner. */
    out = f->base.player;
    CHECK(mc_arrow_shooter_get(a, &out, NULL) == MC_ENTITY_STATE_ERROR && out == f->base.player &&
          mc_arrow_shooter_store(a, NULL, NULL) == MC_ENTITY_STATE_ERROR);
    a->arrow->fields.c = second;
    CHECK(mc_arrow_shooter_store(a, NULL, NULL) == MC_ENTITY_OK && second->holds == second_holds &&
          mc_arrow_shooter_get(a, &out, NULL) == MC_ENTITY_OK && out == NULL);
    f->base.player->local_player->terminal_inventory.sealed = true;
    const mc_entity_status busy = mc_arrow_shooter_store(a, f->base.player, NULL);
    f->base.player->local_player->terminal_inventory.sealed = false;
    CHECK(busy == MC_ENTITY_BUSY && a->arrow->fields.c == NULL);
    CHECK(mc_arrow_shooter_store(a, first, NULL) == MC_ENTITY_OK);
    mc_entity_release(a); /* RAW disposal must retire its source occurrence too. */
    CHECK(first->holds == first_holds);

    mc_entity_classes *foreign_classes = NULL;
    mc_entity *foreign = NULL;
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, f->base.app.profile, NULL, &foreign_classes) ==
              MC_ENTITY_OK &&
          mc_local_player_allocate(foreign_classes, &foreign, NULL) == MC_ENTITY_OK &&
          ac_allocate(f, &a) &&
          mc_arrow_shooter_store(a, foreign, NULL) == MC_ENTITY_INVALID_ARGUMENT &&
          a->arrow->fields.c == NULL);
    mc_entity_release(a);
    mc_entity_release(foreign);
    CHECK(mc_entity_classes_free(foreign_classes) == MC_ENTITY_OK);
    return true;
}

/* Actual inherited false virtuals on genuine Arrow allocation/constructor
 * owners. Source and caller admission are separate; no nearby query changes. */
static bool ac_inherited_predicates(arrow_fixture *f) {
    mc_entity *a = NULL, *foreign = NULL, *failed = NULL;
    mc_entity_classes *foreign_classes = NULL;
    mc_entity_hit_error hit;
    mc_player_action_error action;
    bool pickable = true, collidable = true;
    CHECK(ac_allocate(f, &a));
    mc_player_action_error_init(&action);
    action.source_class = "ls";
    action.source_method = "o()V";
    action.living_bytecode = 527;
    CHECK(mc_entity_pickable(a, &pickable, &hit) == MC_ENTITY_HIT_INVALID_PHASE && pickable &&
          hit.native_policy && !hit.site.entered && strcmp(hit.site.declaring_class, "sn") == 0 &&
          strcmp(hit.site.method, "h_") == 0 && !mc_action_collidable(a, &collidable, &action) &&
          collidable && action.status == MC_ENTITY_STATE_ERROR && action.living_bytecode == 527);
    CHECK(mc_arrow_construct_world(a, NULL, NULL) == MC_ENTITY_OK &&
          mc_arrow_shooter_store(a, f->base.player, NULL) == MC_ENTITY_OK);
    a->arrow->fields.j = 37;
    a->arrow->fields.k = 75;
    const mc_arrow_fields own = a->arrow->fields;
    const mc_java_random random = *a->random;
    const size_t holds = a->holds, shooter_holds = f->base.player->holds;
    const int32_t counter = a->classes->next_id;
    const size_t bytes = a->classes->allocated_bytes;
    for (unsigned int dead = 0; dead < 2; ++dead) {
        a->base.be = dead != 0;
        pickable = collidable = true;
        mc_player_action_error_init(&action);
        action.source_class = "ls";
        action.source_method = "o()V";
        action.living_bytecode = 527;
        CHECK(mc_entity_pickable(a, &pickable, &hit) == MC_ENTITY_HIT_OK && !pickable &&
              hit.status == MC_ENTITY_HIT_OK && !hit.site.selected &&
              mc_action_candidate(a, f->base.player, &action, 514) &&
              mc_action_collidable(a, &collidable, &action) && !collidable &&
              action.status == MC_ENTITY_OK && action.living_bytecode == 527 &&
              action.push_bytecode == 0 && strcmp(action.source_class, "ls") == 0 &&
              strcmp(action.source_method, "o()V") == 0);
    }
    /* These source methods do not dereference any of these reached-unread
     * child slots. Restore the genuine native occurrences before retirement. */
    mc_java_random *saved_random = a->random;
    mc_entity_bounds *saved_bounds = a->bounds;
    a->random = NULL;
    a->bounds = NULL;
    pickable = collidable = true;
    const mc_entity_hit_status unread_pick = mc_entity_pickable(a, &pickable, NULL);
    mc_player_action_error_init(&action);
    const bool unread_action = mc_action_collidable(a, &collidable, &action);
    a->random = saved_random;
    a->bounds = saved_bounds;
    CHECK(
        unread_pick == MC_ENTITY_HIT_OK && !pickable && unread_action && !collidable &&
        a->holds == holds && f->base.player->holds == shooter_holds &&
        a->classes->next_id == counter && a->classes->allocated_bytes == bytes &&
        ac_random_equal(a->random, &random) && a->arrow->fields.c == own.c &&
        a->arrow->shooter_hold == own.c && a->arrow->fields.j == own.j &&
        a->arrow->fields.k == own.k && a->arrow->fields.a == own.a && a->arrow->fields.b == own.b &&
        a->arrow->fields.d == own.d && a->arrow->fields.e == own.e && a->arrow->fields.f == own.f &&
        a->arrow->fields.g == own.g && a->arrow->fields.h == own.h && a->arrow->fields.i == own.i);
    mc_arrow_entity_state *saved_payload = a->arrow;
    a->arrow = NULL;
    pickable = collidable = true;
    const mc_entity_hit_status bad_payload = mc_entity_pickable(a, &pickable, &hit);
    mc_player_action_error_init(&action);
    const bool bad_action = mc_action_collidable(a, &collidable, &action);
    a->arrow = saved_payload;
    CHECK(bad_payload == MC_ENTITY_HIT_UNSUPPORTED_SOURCE && pickable && hit.native_policy &&
          !hit.site.selected && !bad_action && collidable &&
          action.status == MC_ENTITY_STATE_ERROR);
    for (size_t bad = 0; bad < 2; ++bad) {
        a->holds = bad == 0 ? 0 : SIZE_MAX;
        pickable = true;
        const mc_entity_hit_status limited = mc_entity_pickable(a, &pickable, &hit);
        bool action_rejected = true;
        if (bad == 0) {
            collidable = true;
            mc_player_action_error_init(&action);
            action_rejected = !mc_action_collidable(a, &collidable, &action) && collidable &&
                              action.status == MC_ENTITY_STATE_ERROR;
        }
        a->holds = holds;
        const mc_entity_status cause = bad == 0 ? MC_ENTITY_STATE_ERROR : MC_ENTITY_LIMIT;
        CHECK(limited == MC_ENTITY_HIT_NATIVE_FAILED && pickable && hit.native_policy &&
              hit.native.status == cause && action_rejected);
    }
    float border = 7;
    CHECK(mc_entity_pick_border_size(a, &border, &hit) == MC_ENTITY_HIT_UNSUPPORTED_SOURCE &&
          border == 7 && hit.native_policy);
    CHECK(mc_entity_pickable(a, NULL, &hit) == MC_ENTITY_HIT_INVALID_ARGUMENT &&
          hit.native_policy && a->holds == holds);
    pickable = true;
    CHECK(mc_entity_pickable(NULL, &pickable, &hit) == MC_ENTITY_HIT_SOURCE_NULL_REFERENCE &&
          pickable && !hit.native_policy && !hit.site.entered);
    mc_player_action_error_init(&action);
    action.source_class = "ls";
    action.source_method = "o()V";
    action.living_bytecode = 527;
    CHECK(!mc_action_collidable(NULL, &collidable, &action) &&
          action.status == MC_ENTITY_STATE_ERROR && action.living_bytecode == 527);
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, f->base.app.profile, NULL, &foreign_classes) ==
              MC_ENTITY_OK &&
          mc_arrow_allocate(foreign_classes, &foreign, NULL) == MC_ENTITY_OK &&
          mc_arrow_construct_world(foreign, NULL, NULL) == MC_ENTITY_OK);
    mc_player_action_error_init(&action);
    action.source_class = "ls";
    action.source_method = "o()V";
    CHECK(!mc_action_candidate(foreign, f->base.player, &action, 514) &&
          action.status == MC_ENTITY_STATE_ERROR && action.living_bytecode == 514);
    /* A different namespace's canonical literal is not this actual Class. */
    const mc_entity_class_literal *saved_class = a->class_literal;
    a->class_literal = foreign->class_literal;
    pickable = collidable = true;
    const mc_entity_hit_status wrong_class = mc_entity_pickable(a, &pickable, &hit);
    mc_player_action_error_init(&action);
    const bool wrong_action = mc_action_collidable(a, &collidable, &action);
    a->class_literal = saved_class;
    CHECK(wrong_class == MC_ENTITY_HIT_UNSUPPORTED_SOURCE && pickable && hit.native_policy &&
          !wrong_action && collidable && action.status == MC_ENTITY_STATE_ERROR);
    mc_arrow_error failed_call;
    CHECK(ac_allocate(f, &failed) &&
          mc_arrow_construct_living(failed, NULL, NULL, NULL, &failed_call) ==
              MC_ENTITY_STATE_ERROR &&
          failed->phase == MC_ENTITY_FAILED_CONSTRUCTION && failed_call.source_exception &&
          failed_call.site.pc == 73);
    pickable = collidable = true;
    mc_player_action_error_init(&action);
    CHECK(mc_entity_pickable(failed, &pickable, &hit) == MC_ENTITY_HIT_INVALID_PHASE && pickable &&
          hit.native_policy && !mc_action_collidable(failed, &collidable, &action) && collidable &&
          action.status == MC_ENTITY_STATE_ERROR);
    mc_entity_release(failed);
    mc_entity_release(foreign);
    CHECK(mc_entity_classes_free(foreign_classes) == MC_ENTITY_OK);
    mc_entity_release(a);
    CHECK(f->base.player->holds == shooter_holds - 1);
    return true;
}

static bool ac_geometry_admission(arrow_fixture *f) {
    mc_entity *a = NULL;
    mc_arrow_error e;
    CHECK(ac_allocate(f, &a) &&
          mc_arrow_heading(a, f->math, 1, 0, 0, 1, 0, &e) == MC_ENTITY_INVALID_PHASE &&
          a->phase == MC_ENTITY_RAW && mc_arrow_construct_world(a, NULL, &e) == MC_ENTITY_OK);
    mc_entity_bounds *bounds = a->bounds;
    a->bounds = NULL;
    mc_entity_position_rotation_error pose;
    const mc_entity_status status = mc_entity_source_position_rotation(a, 5, 6, 7, 15, 25, &pose);
    a->bounds = bounds;
    CHECK(status == MC_ENTITY_STATE_ERROR && pose.source_null && !pose.native_policy &&
          pose.bytecode == 77 && pose.position_bytecode == 86 && a->base.x == 5 &&
          a->base.aJ == 5 && a->base.bl == 5 && a->base.y == 6 && a->base.aK == 6 &&
          a->base.bm == 6 && a->base.z == 7 && a->base.aL == 7 && a->base.bn == 7 &&
          a->base.yaw == 15 && a->base.aT == 25);
    const unsigned int saved = _mm_getcsr();
    _mm_setcsr((saved & ~0x6000u) | 0x2000u);
    const mc_entity_status fp = mc_arrow_heading(a, f->math, 1, 2, 3, 1, 1, &e);
    _mm_setcsr(saved);
    CHECK(fp == MC_ENTITY_UNSUPPORTED_FP && e.native_policy && e.gaussian_calls == 0);
    mc_entity_release(a);
    mc_entity_classes *server = NULL;
    a = NULL;
    CHECK(mc_entity_classes_create(MC_ENTITY_SERVER, f->base.app.profile, NULL, &server) ==
              MC_ENTITY_OK &&
          mc_arrow_allocate(server, &a, NULL) == MC_ENTITY_INVALID_ARGUMENT && a == NULL &&
          mc_entity_classes_free(server) == MC_ENTITY_OK);
    return ac_inherited_predicates(f);
}

static bool ac_failed_disposal(arrow_fixture *f) {
    mc_entity_class_snapshot before, after;
    CHECK(mc_entity_classes_get_snapshot(f->base.app.entities, &before) == MC_ENTITY_OK);
    mc_entity *a = NULL;
    CHECK(ac_allocate(f, &a));
    const size_t limit = f->base.app.entities->limits.max_allocation_bytes;
    f->base.app.entities->limits.max_allocation_bytes = f->base.app.entities->allocated_bytes;
    mc_arrow_error e;
    const mc_entity_status status = mc_arrow_construct_world(a, NULL, &e);
    f->base.app.entities->limits.max_allocation_bytes = limit;
    CHECK(status == MC_ENTITY_LIMIT && e.native_policy &&
          a->phase == MC_ENTITY_FAILED_CONSTRUCTION && a->base.id == before.next_id &&
          a->random == NULL && a->watcher == NULL);
    mc_entity_release(a);
    CHECK(mc_entity_classes_get_snapshot(f->base.app.entities, &after) == MC_ENTITY_OK &&
          after.live_entities == before.live_entities &&
          after.allocation_bytes == before.allocation_bytes);
    const mc_entity_class_literal *literal = NULL;
    mc_mob_error construction;
    mc_entity *raw = NULL;
    CHECK(mc_entity_class_literal_get(f->base.app.entities, MC_ENTITY_CLASS_KIND_PIG, &literal) ==
              MC_ENTITY_OK &&
          mc_mob_allocate(f->base.app.entities, f->base.app.registry, literal, &raw,
                          &construction) == MC_MOB_OK &&
          ac_allocate(f, &a) && mc_arrow_shooter_store(a, raw, NULL) == MC_ENTITY_OK &&
          raw->holds == 2);
    mc_entity_release(raw); /* Arrow alone now owns this genuine RAW Living allocation. */
    mc_entity_release(a);
    CHECK(mc_entity_classes_get_snapshot(f->base.app.entities, &after) == MC_ENTITY_OK &&
          after.live_entities == before.live_entities);
    return true;
}

#ifndef MC_ARROW_CONSTRUCTION_MAIN
#define MC_ARROW_CONSTRUCTION_MAIN main
#endif
int MC_ARROW_CONSTRUCTION_MAIN(int argc, char **argv);

int MC_ARROW_CONSTRUCTION_MAIN(int argc, char **argv) {
    if (argc != 3 || CD(argv[2]) != 0 || !mc_math_trig_init(&trig))
        return 2;
    arrow_fixture *f = calloc(1, sizeof(*f));
    if (f == NULL || !ac_setup(f, argv[1], "arrow-construction"))
        return 1;
    static bool (*const cases[])(arrow_fixture *) = {ac_plain,
                                                     ac_constructor_failures,
                                                     ac_living,
                                                     ac_heading_inputs,
                                                     ac_heading_failures,
                                                     ac_references,
                                                     ac_geometry_admission,
                                                     ac_failed_disposal};
    for (size_t i = 0; i < sizeof(cases) / sizeof(*cases); ++i) {
        if (!cases[i](f) || printf("ARROW_CONSTRUCTION_CASE id=%zu\n", i) < 0 ||
            fflush(stdout) != 0)
            return 1;
    }
    if (!ac_finish(f))
        return 1;
    free(f);
    if (printf("Arrow construction: %zu checks;8 native component controls\n", checks) < 0 ||
        fflush(stdout) != 0)
        return 1;
    return 0;
}
