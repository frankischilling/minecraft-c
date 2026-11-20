#include "../../../reference/world/spawner_tick/native-invariants.h"
#include "fixture.inc"

#include <math.h>

typedef struct spawner_observer {
    size_t smoke, flame, explode, added;
    const char *fail_particle;
    size_t fail_ordinal;
    bool fail_added;
    uintptr_t last_added;
    mc_java_random added_random;
} spawner_observer;

static bool literal_is(const mc_registry_string *name, const char *text) {
    if (name == NULL || name->length != strlen(text))
        return false;
    for (size_t i = 0; i < name->length; ++i)
        if (name->units[i] != (unsigned char)text[i])
            return false;
    return true;
}

static mc_world_storage_status observe_particle(void *context, mc_world_runtime *world,
                                                const mc_registry_string *name, double x, double y,
                                                double z, double vx, double vy, double vz,
                                                mc_world_storage_error *error) {
    spawner_observer *o = context;
    (void)world;
    if (!isfinite(x) || !isfinite(y) || !isfinite(z) || !isfinite(vx) || !isfinite(vy) ||
        !isfinite(vz)) {
        *error = (mc_world_storage_error){MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                          "unexpected nonfinite authored Spawner particle", 0};
        return error->status;
    }
    size_t ordinal = 0;
    if (literal_is(name, "smoke"))
        ordinal = ++o->smoke;
    else if (literal_is(name, "flame"))
        ordinal = ++o->flame;
    else if (literal_is(name, "explode"))
        ordinal = ++o->explode;
    else {
        *error = (mc_world_storage_error){MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                          "unexpected Spawner particle literal", 0};
        return error->status;
    }
    if (o->fail_particle != NULL && literal_is(name, o->fail_particle) &&
        ordinal == o->fail_ordinal) {
        *error = (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR,
                                          "authored Spawner WorldAccess particle failure", 91};
        return error->status;
    }
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status observe_added(void *context, mc_world_runtime *world,
                                             mc_entity *entity, mc_world_storage_error *error) {
    spawner_observer *o = context;
    (void)world;
    ++o->added;
    o->last_added = (uintptr_t)entity;
    o->added_random = *entity->random;
    if (o->fail_added) {
        *error = (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR,
                                          "authored Spawner WorldAccess publication failure", 92};
        return error->status;
    }
    return MC_WORLD_STORAGE_OK;
}

static bool initial_pigs(spawner_fixture *s, size_t count) {
    bool ok = false;
    static const double positions[6][2] = {{1, 1}, {1, 14}, {14, 1}, {14, 14}, {2, 1}, {1, 2}};
    const uint16_t name[] = {'P', 'i', 'g'};
    mc_entity *actor = NULL;
    CHECK(count <= COUNT(positions));
    for (size_t i = 0; i < count; ++i) {
        mc_entity_factory_error factory;
        CHECK(mc_entity_factory_named(s->base->entity_classes[0], s->base->registry[0],
                                      (mc_nbt_string){name, COUNT(name)}, s->base->worlds[0],
                                      &actor, &factory) == MC_ENTITY_FACTORY_OK &&
              actor != NULL);
        mc_entity_position_rotation_error pose;
        bool inserted = false;
        CHECK(mc_entity_source_position_rotation(actor, positions[i][0], 64, positions[i][1], 0, 0,
                                                 &pose) == MC_ENTITY_OK &&
              mc_world_runtime_insert_entity(s->base->worlds[0], actor, &inserted, NULL) ==
                  MC_WORLD_STORAGE_OK &&
              inserted);
        mc_entity_release(actor);
        actor = NULL;
    }
    ok = true;
cleanup:
    mc_entity_release(actor);
    return ok;
}

/* Independent explicit Random-call schedule, never an expected Entity/World
 * result fed back into Source. Seed224 gives four y64, nonoverlapping poses. */
static void expected_pose(mc_java_random *random) {
    (void)mc_java_random_next_double(random);
    (void)mc_java_random_next_double(random);
    int32_t y;
    (void)mc_java_random_next_bounded(random, 3, &y);
    (void)mc_java_random_next_double(random);
    (void)mc_java_random_next_double(random);
    (void)mc_java_random_next_float(random);
}

static int32_t expected_reset(mc_java_random *random) {
    int32_t delay = 0;
    (void)mc_java_random_next_bounded(random, 600, &delay);
    return 200 + delay;
}

static void expected_pair_points(mc_java_random *random, size_t points) {
    for (size_t i = 0; i < points * 3; ++i)
        (void)mc_java_random_next_float(random);
}

static bool random_same(const mc_java_random *a, const mc_java_random *b) {
    return a->state == b->state && a->have_next_next_gaussian == b->have_next_next_gaussian &&
           memcmp(&a->next_next_gaussian, &b->next_next_gaussian, sizeof(a->next_next_gaussian)) ==
               0;
}

static bool exception_names_equal(const char *native, const char *original) {
    if (native == NULL || original == NULL)
        return native == original;
    while (*native != '\0' && *original != '\0') {
        if ((*native == '/' ? '.' : *native) != *original)
            return false;
        ++native;
        ++original;
    }
    return *native == *original;
}

enum spawner_case {
    INACTIVE,
    COUNTDOWN,
    UNKNOWN,
    NONLIVING,
    REJECTED,
    CAP6,
    FOUR_SUCCESS,
    GROWTH_CAP,
    PUBLICATION_FAIL,
    SMOKE_FAIL,
    FLAME_FAIL,
    EXPLODE_FAIL,
    NULL_OUTPUT,
    FACTORY_LIMIT,
    LATER_FACTORY_LIMIT,
    PREDICATE_MATH_FAIL
};

static const char *const case_names[] = {
    "inactive",           "countdown",     "unknown",
    "nonliving",          "rejected",      "cap6",
    "four-success",       "growth-cap",    "publication-fail",
    "smoke-fail",         "flame-fail",    "explode-fail",
    "null-output",        "factory-limit", "later-factory-limit",
    "predicate-math-fail"};

static bool spawner_publication_present(spawner_fixture *s, mc_entity *entity) {
    size_t world_rows = 0, chunk_rows = 0;
    mc_world_runtime *world = s->base->worlds[0];
    for (size_t i = 0; i < world->fields.b->count; ++i)
        if (world->fields.b->items[i] == entity)
            ++world_rows;
    mc_world_chunk *chunk = s->base->chunks[0];
    for (size_t slice = 0; slice < chunk->m->length; ++slice) {
        const mc_world_chunk_entity_list *list = chunk->m->items[slice];
        for (size_t i = 0; i < list->count; ++i)
            if (list->items[i] == entity)
                ++chunk_rows;
    }
    return world_rows == 1 && chunk_rows == 1 && entity->base.bF && entity->base.world == world;
}

static bool spawner_case(const char *root, enum spawner_case which) {
    bool ok = false, registered = false;
    spawner_fixture s = {0};
    mc_world_access *access = NULL;
    mc_world_entity_tick_error *error = calloc(1, sizeof(*error));
    spawner_observer observer = {0};
    mc_world_spawner_tick_observation observed = {0};
    const bool failing = which == NONLIVING ||
                         (which >= PUBLICATION_FAIL && which <= EXPLODE_FAIL) ||
                         which == NULL_OUTPUT || which >= FACTORY_LIMIT;
    const int16_t input_delay = which == COUNTDOWN ? 300 : 0;
    const char *name = which == UNKNOWN ? "Unknown" : which == NONLIVING ? "Arrow" : "Pig";
    const mc_entity_limits entity_limits = {MC_ENTITY_DEFAULT_ALLOCATION_BYTES,
                                            which == FACTORY_LIMIT ? 1 : 2};
    CHECK(error != NULL);
    if (which == FACTORY_LIMIT || which == LATER_FACTORY_LIMIT)
        CHECK(
            spawner_open_with_entity_limits(&s, root, name, input_delay, 2, true, &entity_limits));
    else
        CHECK(spawner_open(&s, root, name, input_delay, which == REJECTED ? 1 : 2,
                           which != INACTIVE));
    fixture *f = s.base;
    mc_world_runtime *world = f->worlds[0];
    if (which == CAP6 || which == GROWTH_CAP)
        CHECK(initial_pigs(&s, which == CAP6 ? 6 : 4));
    const size_t initial_count = world->fields.b->count;
    const int32_t initial_id = f->entity_classes[0]->next_id;
    const int32_t initial_ticks = s.player->base.bt;
    const size_t initial_bindings = world->persistent_bindings;
    const size_t initial_live = f->entity_classes[0]->live_entities;
    mc_entity_class_snapshot pool_before;
    CHECK(mc_entity_classes_get_snapshot(f->entity_classes[0], &pool_before) == MC_ENTITY_OK);
    const size_t bounds_holds_before = f->entity_classes[0]->bounds_holds;
    observer.fail_added = which == PUBLICATION_FAIL;
    observer.fail_particle = which == SMOKE_FAIL     ? "smoke"
                             : which == FLAME_FAIL   ? "flame"
                             : which == EXPLODE_FAIL ? "explode"
                                                     : NULL;
    observer.fail_ordinal = which == EXPLODE_FAIL ? 1 : 2;
    const mc_world_access_callbacks callbacks = {.particle = observe_particle,
                                                 .entity_added = observe_added};
    CHECK(mc_world_access_create(&callbacks, &observer, &access, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(world, access, NULL) == MC_WORLD_STORAGE_OK);
    registered = true;
    /* Declared native-only initial Random input, before the sole actual
     * World.g call. This is not original constructor-entropy comparison. */
    mc_java_random_reseed(world->fields.r, 224);
    mc_java_random expected;
    mc_java_random_seed(&expected, 224);
    if (which != INACTIVE)
        expected_pair_points(&expected, 1);
    /* Missing Math is an explicit native dependency input to the reached
     * predicate, not a replacement for its return or a repaired source field. */
    const mc_world_entity_tick_context context = {.math =
                                                      which == PREDICATE_MATH_FAIL ? NULL : s.math};
    const mc_world_storage_status status =
        mc_world_runtime_tick_entities(world, &context, NULL, which == NULL_OUTPUT ? NULL : error);
    if (status != MC_WORLD_STORAGE_OK && !failing)
        fprintf(stderr, "Spawner case=%s status=%d operation=%s pc=%u loop=%u child=%d\n",
                case_names[which], status, error->operation == NULL ? "(none)" : error->operation,
                error->spawner_bytecode, error->loop_bytecode, error->child);
    CHECK((status == MC_WORLD_STORAGE_OK) == !failing);
    CHECK(s.player->base.bt == initial_ticks && world->fields.d->count == 1 &&
          world->fields.d->items[0] == s.player && !s.tile->invalid &&
          world->fields.c->count == 1 && world->fields.c->items[0] == s.tile &&
          world->fields.G->count == 0);
    if (which != NULL_OUTPUT)
        observed = error->spawner;
    CHECK(world->fields.L == failing);
    int32_t expected_delay = input_delay;
    if (which == COUNTDOWN)
        expected_delay = 299;
    if (which == CAP6) {
        expected_delay = expected_reset(&expected);
        CHECK(observed.attempts == 1 && observed.factory_calls == 1 && observed.queries == 1 &&
              observed.predicate_calls == 0 && observed.cap_returns == 1 &&
              observed.delay_resets == 1 && observed.completed == 0);
        mc_entity_class_snapshot pool_after;
        CHECK(mc_entity_classes_get_snapshot(f->entity_classes[0], &pool_after) == MC_ENTITY_OK &&
              pool_after.bounds_pool_cursor == pool_before.bounds_pool_cursor + 2 &&
              f->entity_classes[0]->bounds_holds == bounds_holds_before);
    } else if (which == REJECTED) {
        for (size_t i = 0; i < 4; ++i)
            expected_pose(&expected);
        CHECK(observed.attempts == 4 && observed.factory_calls == 4 && observed.queries == 4 &&
              observed.predicate_calls == 4 && observed.accepted == 0 &&
              observed.insertion_calls == 0 && observed.delay_resets == 0 &&
              observed.completed == 1);
    } else if (which == FOUR_SUCCESS || which == GROWTH_CAP) {
        const size_t successes = which == FOUR_SUCCESS ? 4 : 2;
        for (size_t i = 0; i < successes; ++i) {
            expected_pose(&expected);
            expected_pair_points(&expected, 20);
            expected_delay = expected_reset(&expected);
        }
        if (which == GROWTH_CAP)
            expected_delay = expected_reset(&expected);
        CHECK(observed.attempts == (which == FOUR_SUCCESS ? 4u : 3u) &&
              observed.factory_calls == observed.attempts &&
              observed.queries == observed.attempts && observed.predicate_calls == successes &&
              observed.accepted == successes && observed.insertion_calls == successes &&
              observed.insertion_true == successes && observed.insertion_false == 0 &&
              observed.delay_resets == (which == FOUR_SUCCESS ? 4u : 3u) &&
              observed.cap_returns == (which == GROWTH_CAP ? 1u : 0u) &&
              observed.completed == (which == FOUR_SUCCESS ? 1u : 0u) &&
              observed.smoke_calls == 1 + successes * mc_spawner_tick_effect_counts[0] &&
              observed.flame_calls == 1 + successes * mc_spawner_tick_effect_counts[1] &&
              observed.explode_calls == successes * mc_spawner_tick_effect_counts[2] &&
              observer.added == successes && world->fields.b->count == initial_count + successes);
    } else if (which == UNKNOWN) {
        CHECK(observed.attempts == 1 && observed.factory_calls == 1 && observed.null_returns == 1 &&
              observed.queries == 0 && observed.completed == 0);
    } else if (which == NONLIVING) {
        CHECK(error->child == MC_WORLD_ENTITY_TICK_CHILD_SPAWNER &&
              error->spawner_bytecode == 226 && error->child_error.spawner.source_exception &&
              strcmp(error->child_error.spawner.exception_class, "java/lang/ClassCastException") ==
                  0 &&
              error->child_error.spawner.candidate != NULL && observed.attempts == 1 &&
              observed.factory_calls == 1 && observed.queries == 0 &&
              mc_entity_has_arrow(error->child_error.spawner.candidate) &&
              error->child_error.spawner.candidate->phase == MC_ENTITY_READY);
    } else if (which == FACTORY_LIMIT || which == LATER_FACTORY_LIMIT) {
        const bool later = which == LATER_FACTORY_LIMIT;
        if (later) {
            expected_pose(&expected);
            expected_pair_points(&expected, 20);
            expected_delay = expected_reset(&expected);
        }
        const mc_world_spawner_tick_error *child = &error->child_error.spawner;
        const mc_entity_factory_error *factory = &child->detail.factory;
        CHECK(status == MC_WORLD_STORAGE_LIMIT &&
              error->child == MC_WORLD_ENTITY_TICK_CHILD_SPAWNER &&
              error->spawner_bytecode == 223 && error->loop_bytecode == 390 &&
              child->child == MC_WORLD_SPAWNER_TICK_CHILD_FACTORY && child->candidate == NULL &&
              child->evaluated_world == NULL && !child->source_exception &&
              child->exception_class == NULL && factory->status == MC_ENTITY_FACTORY_LIMIT &&
              factory->bytecode == 40 && factory->native_policy && !factory->source_exception &&
              factory->mob_selected && factory->construction.status == MC_ENTITY_LIMIT &&
              factory->mob_call.status == MC_MOB_LIMIT && factory->mob_call.native_policy &&
              !factory->mob_call.source_exception && factory->mob_call.site.pc == -1);
        CHECK(observed.attempts == (later ? 2u : 1u) &&
              observed.factory_calls == observed.attempts &&
              observed.queries == (later ? 1u : 0u) &&
              observed.predicate_calls == (later ? 1u : 0u) &&
              observed.accepted == (later ? 1u : 0u) &&
              observed.insertion_true == (later ? 1u : 0u) &&
              observed.insertion_calls == (later ? 1u : 0u) && observed.insertion_false == 0 &&
              observed.delay_resets == (later ? 1u : 0u) && observed.cap_returns == 0 &&
              observed.null_returns == 0 && observed.completed == 0 &&
              observed.smoke_calls == (later ? 21u : 1u) &&
              observed.flame_calls == (later ? 21u : 1u) &&
              observed.explode_calls == (later ? 20u : 0u) && observer.added == (later ? 1u : 0u) &&
              observer.smoke == observed.smoke_calls && observer.flame == observed.flame_calls &&
              observer.explode == observed.explode_calls &&
              world->fields.b->count == initial_count + (later ? 1u : 0u));
        CHECK(f->entity_classes[0]->limits.max_live_entities == entity_limits.max_live_entities &&
              f->entity_classes[0]->live_entities == initial_live + (later ? 1u : 0u) &&
              world->persistent_bindings == initial_bindings + (later ? 1u : 0u));
        if (later) {
            mc_entity *published = world->fields.b->items[initial_count];
            CHECK((uintptr_t)published == observer.last_added && published->holds == 2 &&
                  mc_entity_is_kind(published, MC_ENTITY_CLASS_KIND_PIG) &&
                  spawner_publication_present(&s, published));
            mc_java_random own = observer.added_random;
            for (size_t iteration = 0; iteration < 20; ++iteration) {
                for (size_t i = 0; i < 3; ++i)
                    (void)mc_java_random_next_gaussian(&own);
                for (size_t i = 0; i < 3; ++i)
                    (void)mc_java_random_next_float(&own);
            }
            CHECK(random_same(&own, published->random));
        }
    } else if (which == PREDICATE_MATH_FAIL) {
        expected_pose(&expected);
        const mc_world_spawner_tick_error *child = &error->child_error.spawner;
        const mc_mob_spawn_error *predicate = &child->detail.predicate;
        mc_entity *candidate = child->candidate;
        CHECK(status == MC_WORLD_STORAGE_INVALID_ARGUMENT &&
              error->child == MC_WORLD_ENTITY_TICK_CHILD_SPAWNER &&
              error->spawner_bytecode == 441 && error->loop_bytecode == 390 &&
              child->child == MC_WORLD_SPAWNER_TICK_CHILD_PREDICATE && !child->source_exception &&
              child->exception_class == NULL &&
              predicate->status == MC_WORLD_STORAGE_INVALID_ARGUMENT && predicate->native_policy &&
              !predicate->source_exception && strcmp(predicate->site.class_name, "bg") == 0 &&
              strcmp(predicate->site.method, "d") == 0 && predicate->site.pc == 4 &&
              predicate->active_child == MC_MOB_SPAWN_CHILD_NONE);
        CHECK(candidate != NULL && candidate->phase == MC_ENTITY_READY && candidate->holds == 1 &&
              mc_entity_is_kind(candidate, MC_ENTITY_CLASS_KIND_PIG) &&
              candidate->base.world == world && !candidate->base.bF &&
              child->evaluated_world == world &&
              world->persistent_bindings == initial_bindings + 2 &&
              f->entity_classes[0]->live_entities == initial_live + 1 && candidate->base.x != 0 &&
              candidate->base.y == 64 && candidate->base.z != 0 &&
              candidate->base.aJ == candidate->base.x && candidate->base.bl == candidate->base.x &&
              candidate->base.aK == candidate->base.y && candidate->base.bm == candidate->base.y &&
              candidate->base.aL == candidate->base.z && candidate->base.bn == candidate->base.z &&
              candidate->base.yaw > 0 && candidate->base.yaw < 360);
        CHECK(observed.attempts == 1 && observed.factory_calls == 1 && observed.queries == 1 &&
              observed.predicate_calls == 1 && observed.accepted == 0 &&
              observed.insertion_calls == 0 && observed.delay_resets == 0 &&
              observed.completed == 0 && observed.smoke_calls == 1 && observed.flame_calls == 1 &&
              observed.explode_calls == 0 && observer.added == 0);
    } else if (which >= PUBLICATION_FAIL && which <= EXPLODE_FAIL) {
        expected_pose(&expected);
        const unsigned int failed_pc = which == PUBLICATION_FAIL ? 453
                                       : which == SMOKE_FAIL     ? 571
                                       : which == FLAME_FAIL     ? 587
                                                                 : 598;
        if (which == SMOKE_FAIL || which == FLAME_FAIL)
            expected_pair_points(&expected, 1);
        if (which == EXPLODE_FAIL)
            expected_pair_points(&expected, 20);
        CHECK(status == MC_WORLD_STORAGE_IO_ERROR &&
              error->child == MC_WORLD_ENTITY_TICK_CHILD_SPAWNER &&
              error->spawner_bytecode == failed_pc && error->loop_bytecode == 390 &&
              error->child_error.spawner.candidate != NULL &&
              (uintptr_t)error->child_error.spawner.candidate == observer.last_added &&
              observed.attempts == 1 && observed.factory_calls == 1 && observed.queries == 1 &&
              observed.predicate_calls == 1 && observed.accepted == 1 &&
              observed.insertion_calls == 1 && observed.delay_resets == 0 &&
              observed.completed == 0 && observer.added == 1 &&
              world->fields.b->count == initial_count + 1 &&
              error->child_error.spawner.candidate->base.bF);
        CHECK(observed.insertion_true == (which == PUBLICATION_FAIL ? 0u : 1u));
        if (which == EXPLODE_FAIL) {
            CHECK(error->child_error.spawner.child == MC_WORLD_SPAWNER_TICK_CHILD_EFFECT &&
                  error->child_error.spawner.detail.effect.site.pc == 149 &&
                  observer.explode == 1 && observed.smoke_calls == 21 &&
                  observed.flame_calls == 21 && observed.explode_calls == 0);
            mc_java_random own = observer.added_random;
            for (size_t i = 0; i < 3; ++i)
                (void)mc_java_random_next_gaussian(&own);
            for (size_t i = 0; i < 3; ++i)
                (void)mc_java_random_next_float(&own);
            CHECK(random_same(&own, error->child_error.spawner.candidate->random));
        } else {
            CHECK(
                random_same(&observer.added_random, error->child_error.spawner.candidate->random));
        }
    } else {
        CHECK(observed.attempts == 0 && observed.factory_calls == 0 && observed.delay_resets == 0);
    }
    CHECK(s.tile->state.spawner.delay == expected_delay && random_same(world->fields.r, &expected));
    if (which != FOUR_SUCCESS && which != GROWTH_CAP && which != LATER_FACTORY_LIMIT &&
        !(which >= PUBLICATION_FAIL && which <= EXPLODE_FAIL))
        CHECK(world->fields.b->count == initial_count && observer.added == 0);
    const int32_t constructions = which == INACTIVE || which == COUNTDOWN || which == UNKNOWN ||
                                          which == NULL_OUTPUT || which == FACTORY_LIMIT
                                      ? 0
                                  : which == REJECTED || which == FOUR_SUCCESS ? 4
                                  : which == GROWTH_CAP                        ? 3
                                                                               : 1;
    CHECK(f->entity_classes[0]->next_id == mc_java_i32_add(initial_id, constructions));
    /* Only common finite original projections are compared here. Constructor
     * entropy, random positions/delays and positive success counts are separate
     * authored native inputs, never copied from either original VM. */
    const mc_spawner_tick_reference *reference =
        which == UNKNOWN     ? &mc_spawner_tick_references[0]
        : which == NONLIVING ? &mc_spawner_tick_references[1]
        : which == CAP6      ? &mc_spawner_tick_references[2]
        : which == REJECTED  ? &mc_spawner_tick_references[3]
                             : NULL;
    if (reference != NULL) {
        uint64_t rotation;
        memcpy(&rotation, &s.tile->state.spawner.rotation, sizeof(rotation));
        CHECK(mc_java_i32_sub(f->entity_classes[0]->next_id, initial_id) ==
              reference->counter_delta);
        CHECK((status == MC_WORLD_STORAGE_OK) == reference->returned);
        CHECK(rotation == reference->rotation_bits);
        CHECK(observed.smoke_calls + observed.flame_calls == reference->initial_particles);
        if (which == NONLIVING)
            CHECK(error->child_error.spawner.source_exception &&
                  exception_names_equal(error->child_error.spawner.exception_class,
                                        reference->exception_class));
    }
    if (which != NULL_OUTPUT) {
        const bool source_l = world->fields.L;
        const size_t count = world->fields.b->count;
        const mc_java_random source_random = *world->fields.r;
        const int32_t source_id = f->entity_classes[0]->next_id;
        CHECK(mc_world_entity_tick_error_retire(error) && world->fields.L == source_l &&
              world->fields.b->count == count && s.tile->state.spawner.delay == expected_delay &&
              random_same(world->fields.r, &source_random) &&
              f->entity_classes[0]->next_id == source_id);
        CHECK(mc_world_entity_tick_error_retire(error));
        if (which >= FACTORY_LIMIT) {
            const bool later = which == LATER_FACTORY_LIMIT;
            CHECK(f->entity_classes[0]->live_entities == initial_live + (later ? 1u : 0u) &&
                  world->persistent_bindings == initial_bindings + (later ? 1u : 0u));
            if (later) {
                mc_entity *published = world->fields.b->items[initial_count];
                CHECK((uintptr_t)published == observer.last_added && published->holds == 2 &&
                      spawner_publication_present(&s, published));
            }
        }
    }
    ok = true;
cleanup:
    if (error != NULL && !mc_world_entity_tick_error_retire(error))
        ok = false;
    if (registered) {
        bool removed = false;
        if (mc_world_runtime_remove_world_access(s.base->worlds[0], access, &removed, NULL) !=
                MC_WORLD_STORAGE_OK ||
            !removed)
            ok = false;
    }
    if (access != NULL && mc_world_access_free(access, NULL) != MC_WORLD_STORAGE_OK)
        ok = false;
    if (!spawner_close(&s))
        ok = false;
    free(error);
    if (ok)
        printf("SPAWNER_TICK case=%s attempts=%zu factory=%zu accepted=%zu inserted=%zu caps=%zu "
               "nulls=%zu failed=%d cleanup=1\n",
               case_names[which], observed.attempts, observed.factory_calls, observed.accepted,
               observed.insertion_true, observed.cap_returns, observed.null_returns, (int)failing);
    return ok;
}

int main(int argc, char **argv) {
    char root[8192], path[8192];
    if (argc != 3 || strcmp(argv[1], "world-spawner-tick") != 0 ||
        !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 1;
    for (size_t i = 0; i < COUNT(case_names); ++i) {
        const int written = snprintf(path, sizeof(path), "%s/case%zu", root, i);
        if (written <= 0 || (size_t)written >= sizeof(path) ||
            !spawner_case(path, (enum spawner_case)i))
            return 1;
    }
    printf("PASS: Spawner tick; cases=%zu; checks=%u; cleanup=1; source-field-repair=0; "
           "original-entropy=0\n",
           COUNT(case_names), checks);
    return 0;
}
