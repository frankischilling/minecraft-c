/* Genuine protected FR189 controls, not a scheduled Skeleton encounter.
 * Constructor entropy stays actual and independent; callback mutations below
 * are explicit inputs and are restored only after their observed assertions. */
#define MC_ARROW_CONSTRUCTION_MAIN mc_skeleton_fire_unused_arrow_main
#include "test_arrow_construction.c"
#undef MC_ARROW_CONSTRUCTION_MAIN

#include "entity/mob/tick/internal.h"
#include "mc/world/runtime/world_access.h"
#include "world/runtime/chunk_provider_internal.h"
#include "world/storage/objects_internal.h"

typedef struct sf_fixture {
    arrow_fixture construction;
    mc_chunk_provider_fields provider;
    mc_chunk_state *loaded;
} sf_fixture;

static mc_entity *sf_actor(sf_fixture *f) {
    return f->construction.shooters[0];
}

static bool sf_new_mob(sf_fixture *f, mc_entity_class_kind kind, mc_entity **out) {
    fixture *base = &f->construction.base;
    const mc_entity_class_literal *literal = NULL;
    mc_mob_error error;
    CHECK(mc_entity_class_literal_get(base->app.entities, kind, &literal) == MC_ENTITY_OK &&
          mc_mob_allocate(base->app.entities, base->app.registry, literal, out, &error) ==
              MC_MOB_OK &&
          mc_mob_construct(*out, base->world, &error) == MC_MOB_OK);
    return true;
}

/* Reused by the independent reference emitter. No hidden callback or reseed. */
static bool sf_reset(sf_fixture *f, mc_entity *target) {
    mc_entity *actor = sf_actor(f);
    CHECK(mc_entity_source_position_rotation(actor, 8.5, 64, 8.5, 30.0f, 10.0f, NULL) ==
          MC_ENTITY_OK);
    if (target != NULL)
        CHECK(mc_entity_source_position_rotation(target, 15.5, 66.25, 11.25, 0, 0, NULL) ==
              MC_ENTITY_OK);
    actor->mob->living.ae = 0;
    actor->mob->creature_attacking = false;
    f->construction.base.world->fields.B = false;
    f->construction.base.world->fields.q = 2;
    return true;
}

static bool sf_setup(sf_fixture *f, const char *resources, const char *directory, bool load_chunk) {
    CHECK(ac_setup(&f->construction, resources, directory) &&
          mc_chunk_provider_bind_fields(f->construction.base.world->fields.v, &f->provider, NULL) ==
              MC_WORLD_STORAGE_OK &&
          sf_new_mob(f, MC_ENTITY_CLASS_KIND_SKELETON, &f->construction.shooters[0]) &&
          sf_new_mob(f, MC_ENTITY_CLASS_KIND_WOLF, &f->construction.shooters[1]) &&
          sf_reset(f, f->construction.base.player));
    if (load_chunk)
        CHECK(mc_world_runtime_get(f->construction.base.world, 0, 0, &f->loaded) ==
                  MC_CHUNK_ACQUIRE_OK &&
              f->loaded != NULL && !f->loaded->blank && f->loaded->active);
    return true;
}

static bool sf_finish(sf_fixture *f) {
    /* Parent-owned returned Arrows and any extra targets/WorldAccesses have
     * already been released. Published c edges are retired by the real graph. */
    for (size_t i = 0; i < 3; ++i) {
        mc_entity_release(f->construction.shooters[i]);
        f->construction.shooters[i] = NULL;
    }
    mc_chunk_state *retiring[8];
    const size_t count = f->provider.cache->insertion_count;
    CHECK(count <= sizeof(retiring) / sizeof(*retiring));
    if (count != 0)
        memcpy(retiring, f->provider.cache->insertions, count * sizeof(*retiring));
    CHECK(mc_world_runtime_retire_entity_containers(f->construction.base.world, NULL) ==
              MC_WORLD_STORAGE_OK &&
          mc_chunk_pending_clear(f->provider.pending) == MC_CHUNK_ACQUIRE_OK &&
          mc_chunk_cache_init(f->provider.cache, f->provider.cache->entries,
                              f->provider.cache->capacity, f->provider.cache->insertions,
                              f->provider.cache->insertion_capacity) &&
          mc_world_runtime_retire_world(f->construction.base.world, NULL) == MC_WORLD_STORAGE_OK);
    for (size_t i = 0; i < count; ++i) {
        bool duplicate = false;
        for (size_t j = 0; j < i; ++j)
            duplicate = duplicate || retiring[j] == retiring[i];
        if (!duplicate)
            CHECK(mc_world_store_retire_chunk(f->construction.base.store, retiring[i], NULL) ==
                  MC_WORLD_STORAGE_OK);
    }
    f->loaded = NULL;
    CHECK(ac_finish(&f->construction));
    return true;
}

static mc_entity_status sf_call(sf_fixture *f, mc_entity *target, float distance, mc_entity **arrow,
                                mt_skeleton_fire_observation *capture, mc_mob_tick_error *error) {
    return mt_skeleton_component(sf_actor(f), f->construction.base.app.registry,
                                 f->construction.math, NULL, target, distance, arrow, capture,
                                 error);
}

static uint32_t sf_fbits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static bool sf_receipts(sf_fixture *f, mc_entity *arrow, const mt_skeleton_fire_observation *c) {
    CHECK(c->entered && c->returned && c->allocated && c->constructor_invoked &&
          c->constructor_returned && c->constructor.heading_entered &&
          c->constructor.heading_returned && c->constructor.gaussian_calls == 3 &&
          c->constructor_arguments.speed == 1.5f && c->constructor_arguments.spread == 1.0f &&
          c->final_heading.heading_entered && c->final_heading.heading_returned &&
          c->final_heading.gaussian_calls == 3 && c->final_arguments.speed == .6f &&
          c->final_arguments.spread == 12.0f && c->final_arguments.x == 7.0 &&
          c->final_arguments.z == 2.75 && c->constructor_j == 0 &&
          sf_fbits(c->constructor_angles[0]) == sf_fbits(c->constructor_angles[2]) &&
          sf_fbits(c->constructor_angles[1]) == sf_fbits(c->constructor_angles[3]) &&
          (c->constructor_motion[0] != 0 || c->constructor_motion[1] != 0 ||
           c->constructor_motion[2] != 0) &&
          ac_random_equal(&c->constructor.random_after, &c->final_heading.random_before) &&
          ac_gaussian_receipt(&c->constructor, &c->constructor.random_after) &&
          ac_gaussian_receipt(&c->final_heading, arrow->random));
    mc_java_random expected = c->actor_random_before;
    const float draw = mc_java_random_next_float(&expected);
    CHECK(c->actor_random_captured && c->actor_float_returned &&
          c->actor_random_identity == (uintptr_t)sf_actor(f)->random &&
          c->actor_random_identity != (uintptr_t)arrow->random &&
          sf_fbits(draw) == sf_fbits(c->actor_float) &&
          ac_random_equal(&expected, &c->actor_random_after) &&
          ac_random_equal(sf_actor(f)->random, &c->actor_random_after) && c->cooldown_stored &&
          c->facing_stored && c->attacking_stored && sf_actor(f)->mob->living.ae == 30 &&
          sf_actor(f)->mob->creature_attacking && arrow->arrow->fields.c == sf_actor(f) &&
          arrow->arrow->shooter_hold == sf_actor(f));
    return true;
}

static bool sf_guards(sf_fixture *f) {
    mc_entity *actor = sf_actor(f), *arrow = NULL;
    mc_mob_tick_error *error = calloc(1, sizeof(*error));
    mt_skeleton_fire_observation c;
    CHECK(error != NULL);
    const float ranges[] = {10.0f, INFINITY, NAN};
    const int32_t counter = actor->classes->next_id;
    const mc_java_random random = *actor->random;
    for (size_t i = 0; i < sizeof(ranges) / sizeof(*ranges); ++i) {
        memset(&c, 0xa5, sizeof(c));
        CHECK(sf_call(f, NULL, ranges[i], &arrow, &c, error) == MC_ENTITY_OK && arrow == NULL &&
              c.returned && !c.allocated && !c.actor_float_returned && error->site.pc == 188 &&
              !actor->mob->creature_attacking && actor->classes->next_id == counter &&
              ac_random_equal(actor->random, &random));
    }
    for (int32_t cooldown = -1; cooldown <= 1; cooldown += 2) {
        CHECK(sf_reset(f, f->construction.base.player));
        actor->mob->living.ae = cooldown;
        CHECK(sf_call(f, f->construction.base.player, 5.0f, &arrow, &c, error) == MC_ENTITY_OK &&
              arrow == NULL && c.returned && !c.allocated && c.facing_stored &&
              c.attacking_stored && actor->mob->living.ae == cooldown &&
              actor->mob->creature_attacking && actor->classes->next_id == counter &&
              ac_random_equal(actor->random, &random));
        CHECK(sf_call(f, NULL, 5.0f, &arrow, &c, error) == MC_ENTITY_STATE_ERROR && arrow == NULL &&
              !c.returned && !c.allocated && error->source_exception && !error->native_policy &&
              error->site.pc == 8 && actor->mob->living.ae == cooldown);
    }
    free(error);
    return true;
}

static bool sf_false(sf_fixture *f) {
    mc_entity *arrow = NULL;
    mc_mob_tick_error *error = calloc(1, sizeof(*error));
    mt_skeleton_fire_observation c;
    CHECK(error != NULL && f->provider.cache->insertion_count == 0 &&
          sf_call(f, f->construction.base.player, 5.0f, &arrow, &c, error) == MC_ENTITY_OK &&
          arrow != NULL && arrow->phase == MC_ENTITY_READY && arrow->holds == 1 &&
          c.sound_returned && c.publication_returned && !c.publication_result &&
          c.heading_returned && error->state.returned && error->site.pc == 188 && !arrow->base.bF &&
          f->construction.base.world->fields.b->count == 0 && sf_receipts(f, arrow, &c));
    CHECK(arrow->base.y != arrow->bounds->min_y &&
          ac_dbits(arrow->base.y) == ac_dbits(at_add(arrow->bounds->min_y, 1.399999976158142)));
    mc_entity_release(arrow);
    free(error);
    return true;
}

static bool sf_true(sf_fixture *f) {
    mc_entity *arrow = NULL, *target_arrow = NULL;
    mc_mob_tick_error *error = calloc(1, sizeof(*error));
    mt_skeleton_fire_observation c;
    CHECK(error != NULL && ac_allocate(&f->construction, &target_arrow) &&
          mc_arrow_construct_world(target_arrow, f->construction.base.world, NULL) == MC_ENTITY_OK);
    mc_entity *targets[] = {f->construction.base.player, f->construction.shooters[1], target_arrow};
    for (size_t i = 0; i < 3; ++i) {
        CHECK(sf_reset(f, targets[i]));
        f->construction.base.world->fields.B = i == 2;
        CHECK(sf_call(f, targets[i], 5.0f, &arrow, &c, error) == MC_ENTITY_OK &&
              c.publication_returned && c.publication_result && c.heading_returned &&
              arrow != NULL && arrow->base.bF && arrow->holds >= 3 &&
              f->construction.base.world->fields.b->count == i + 1 &&
              error->state.attack.returned && sf_receipts(f, arrow, &c));
        CHECK(c.eye_height == (i == 0   ? .12f
                               : i == 2 ? 0.0f
                                        : at_fmul(targets[i]->base.height, .8f)));
        mc_entity_release(arrow);
        arrow = NULL;
    }
    mc_entity_release(target_arrow);
    free(error);
    return true;
}

static bool sf_nulls(sf_fixture *f) {
    mc_entity *actor = sf_actor(f), *arrow = NULL;
    mc_mob_tick_error *error = calloc(1, sizeof(*error));
    mt_skeleton_fire_observation c;
    CHECK(error != NULL);
    mc_world_runtime *world = actor->base.world;
    actor->base.world = NULL;
    const mc_entity_status null_world =
        sf_call(f, f->construction.base.player, 5, &arrow, &c, error);
    actor->base.world = world;
    CHECK(null_world == MC_ENTITY_STATE_ERROR && arrow != NULL && arrow->phase == MC_ENTITY_READY &&
          error->site.pc == 124 && error->source_exception && !error->native_policy &&
          c.constructor_returned && c.constructor.gaussian_calls == 3 && c.actor_float_returned &&
          c.sound_invoked && !c.sound_returned && !c.publication_invoked && !c.heading_invoked &&
          arrow->base.world == NULL && actor->mob->living.ae == 0);
    mc_entity_release(arrow);
    arrow = NULL;
    mc_java_random *random = actor->random;
    actor->random = NULL;
    const mc_entity_status null_random =
        sf_call(f, f->construction.base.player, 5, &arrow, &c, error);
    actor->random = random;
    CHECK(null_random == MC_ENTITY_STATE_ERROR && error->site.pc == 114 &&
          error->source_exception && c.constructor_returned && c.constructor.gaussian_calls == 3 &&
          !c.actor_float_returned && !c.sound_invoked && !c.heading_invoked && arrow != NULL &&
          arrow->random != NULL);
    mc_entity_release(arrow);
    free(error);
    return true;
}

static bool sf_failures(sf_fixture *f) {
    fixture *base = &f->construction.base;
    mc_entity *actor = sf_actor(f), *arrow = NULL;
    mc_mob_tick_error *error = calloc(1, sizeof(*error));
    mt_skeleton_fire_observation c;
    CHECK(error != NULL &&
          mt_skeleton_component(actor, base->app.registry, NULL, NULL, base->player, 5, &arrow, &c,
                                error) == MC_ENTITY_STATE_ERROR &&
          arrow != NULL && arrow->phase == MC_ENTITY_FAILED_CONSTRUCTION && c.allocated &&
          !c.constructor_returned && !c.actor_float_returned && error->native_policy &&
          error->site.pc == 44 && error->child == MC_MOB_TICK_CHILD_ARROW &&
          arrow->arrow->fields.c == actor && arrow->arrow->constructor_pc == 98);
    mc_entity_release(arrow);
    arrow = NULL;
    const size_t limit = actor->classes->limits.max_allocation_bytes;
    actor->classes->limits.max_allocation_bytes = actor->classes->allocated_bytes;
    const mc_entity_status limited = sf_call(f, base->player, 5, &arrow, &c, error);
    actor->classes->limits.max_allocation_bytes = limit;
    CHECK(limited == MC_ENTITY_LIMIT && arrow == NULL && !c.allocated && !c.constructor_invoked &&
          error->site.pc == 35 && error->native_policy && !error->source_exception);
    CHECK(ac_allocate(&f->construction, &arrow));
    mc_entity *occupied = arrow;
    memset(&c, 0xa5, sizeof(c));
    CHECK(sf_call(f, base->player, 5, &arrow, &c, error) == MC_ENTITY_INVALID_ARGUMENT &&
          arrow == occupied && !c.entered && !c.returned && error->native_policy);
    mc_arrow_error observed;
    mc_arrow_heading_arguments arguments = {1, 2, 3, 4, 5};
    CHECK(mc_arrow_heading_observed(arrow, f->construction.math, 7, 8, 9, .6f, 12, &arguments,
                                    &observed) == MC_ENTITY_INVALID_PHASE &&
          !observed.heading_entered && arguments.x == 0 && arguments.y == 0 && arguments.z == 0 &&
          arguments.speed == 0 && arguments.spread == 0 && arrow->phase == MC_ENTITY_RAW);
    arguments = (mc_arrow_heading_arguments){1, 2, 3, 4, 5};
    CHECK(mc_arrow_construct_living_observed(arrow, base->world, NULL, f->construction.math,
                                             &arguments, &observed) == MC_ENTITY_STATE_ERROR &&
          observed.source_exception && observed.site.pc == 73 && !observed.heading_entered &&
          arguments.x == 0 && arguments.y == 0 && arguments.z == 0 && arguments.speed == 0 &&
          arguments.spread == 0 && arrow->phase == MC_ENTITY_FAILED_CONSTRUCTION);
    mc_entity_release(arrow);
    arrow = NULL;
    actor->mob->tick_active = true;
    const mc_entity_status busy = sf_call(f, base->player, 5, &arrow, &c, error);
    actor->mob->tick_active = false;
    CHECK(busy == MC_ENTITY_BUSY && !c.entered && !c.allocated && arrow == NULL &&
          error->native_policy);
    CHECK(mt_skeleton_component(NULL, base->app.registry, f->construction.math, NULL, NULL, 10,
                                &arrow, &c, error) == MC_ENTITY_STATE_ERROR &&
          !c.entered &&
          mt_skeleton_component(f->construction.shooters[1], base->app.registry,
                                f->construction.math, NULL, base->player, 5, &arrow, &c,
                                error) == MC_ENTITY_STATE_ERROR &&
          !c.entered && arrow == NULL);
    mc_entity *raw = NULL;
    const mc_entity_class_literal *literal = NULL;
    mc_mob_error construction;
    CHECK(mc_entity_class_literal_get(base->app.entities, MC_ENTITY_CLASS_KIND_SKELETON,
                                      &literal) == MC_ENTITY_OK &&
          mc_mob_allocate(base->app.entities, base->app.registry, literal, &raw, &construction) ==
              MC_MOB_OK &&
          mt_skeleton_component(raw, base->app.registry, f->construction.math, NULL, base->player,
                                5, &arrow, &c, error) == MC_ENTITY_INVALID_PHASE &&
          !c.entered && arrow == NULL);
    mc_entity_release(raw);
    const size_t actor_holds = actor->holds, target_holds = base->player->holds;
    actor->holds = SIZE_MAX;
    const mc_entity_status actor_limit = sf_call(f, base->player, 5, &arrow, &c, error);
    actor->holds = actor_holds;
    CHECK(actor_limit == MC_ENTITY_LIMIT && !c.entered && arrow == NULL);
    base->player->holds = 0;
    const mc_entity_status target_empty = sf_call(f, base->player, 5, &arrow, &c, error);
    base->player->holds = target_holds;
    CHECK(target_empty == MC_ENTITY_STATE_ERROR && !c.entered && arrow == NULL &&
          actor->holds == actor_holds);
    free(error);
    return true;
}

typedef enum sf_mode {
    SF_SOUND_FAIL,
    SF_PUBLICATION_FAIL,
    SF_CLEAR,
    SF_WOLF,
    SF_RESEED,
    SF_NULL_RANDOM,
    SF_NULL_WORLD,
    SF_REENTER
} sf_mode;

typedef struct sf_probe {
    sf_fixture *fixture;
    sf_mode mode;
    size_t sounds, added;
    bool valid;
    mc_entity *seen, *other;
    mc_java_random *removed_random;
    mc_mob_tick_error *nested;
} sf_probe;

static mc_world_storage_status sf_sound(void *context, mc_world_runtime *world,
                                        const mc_registry_string *name, double x, double y,
                                        double z, float volume, float pitch,
                                        mc_world_storage_error *error) {
    sf_probe *probe = context;
    sf_fixture *f = probe->fixture;
    mc_entity *actor = sf_actor(f);
    ++probe->sounds;
    probe->valid = probe->valid && world == f->construction.base.world && name != NULL &&
                   x == actor->base.x && y == at_sub(actor->base.y, at_widen(actor->base.offset)) &&
                   z == actor->base.z && volume == 1.0f && pitch > 0.0f;
    if (probe->mode == SF_NULL_WORLD)
        actor->base.world = NULL;
    if (probe->mode == SF_REENTER) {
        mt_skeleton_fire_observation nested;
        mc_entity *arrow = NULL;
        probe->valid = probe->valid &&
                       sf_call(f, f->construction.base.player, 5, &arrow, &nested, probe->nested) ==
                           MC_ENTITY_BUSY &&
                       arrow == NULL && !nested.entered;
        probe->other->mob->living.ae = 1;
        probe->valid =
            probe->valid &&
            mt_skeleton_component(probe->other, f->construction.base.app.registry,
                                  f->construction.math, NULL, f->construction.base.player, 5,
                                  &arrow, &nested, probe->nested) == MC_ENTITY_OK &&
            arrow == NULL && nested.returned && !nested.allocated;
    }
    if (probe->mode == SF_SOUND_FAIL) {
        *error = (mc_world_storage_error){MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                          "Authored native bow sound failure", 0};
        return error->status;
    }
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status sf_added(void *context, mc_world_runtime *world, mc_entity *arrow,
                                        mc_world_storage_error *error) {
    sf_probe *probe = context;
    ++probe->added;
    probe->seen = arrow;
    probe->valid = probe->valid && world == probe->fixture->construction.base.world &&
                   mc_entity_has_arrow(arrow) && arrow->phase == MC_ENTITY_READY &&
                   arrow->base.bF && arrow->arrow->heading_pc == 198 &&
                   arrow->base.y != arrow->bounds->min_y;
    if (probe->mode == SF_CLEAR || probe->mode == SF_WOLF) {
        mc_entity *value =
            probe->mode == SF_CLEAR ? NULL : probe->fixture->construction.shooters[1];
        probe->valid = probe->valid && mc_arrow_shooter_store(arrow, value, NULL) == MC_ENTITY_OK;
    } else if (probe->mode == SF_RESEED)
        mc_java_random_reseed(arrow->random, 271828);
    else if (probe->mode == SF_NULL_RANDOM) {
        probe->removed_random = arrow->random;
        arrow->random = NULL;
    }
    if (probe->mode == SF_PUBLICATION_FAIL) {
        *error = (mc_world_storage_error){MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                          "Authored native Arrow added failure", 0};
        return error->status;
    }
    return MC_WORLD_STORAGE_OK;
}

static bool sf_attach(sf_fixture *f, sf_probe *probe, mc_world_access **access) {
    const mc_world_access_callbacks callbacks = {.sound = sf_sound, .entity_added = sf_added};
    CHECK(mc_world_access_create(&callbacks, probe, access, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(f->construction.base.world, *access, NULL) ==
              MC_WORLD_STORAGE_OK);
    return true;
}

static bool sf_detach(sf_fixture *f, mc_world_access *access) {
    bool removed = false;
    CHECK(mc_world_runtime_remove_world_access(f->construction.base.world, access, &removed,
                                               NULL) == MC_WORLD_STORAGE_OK &&
          removed && mc_world_access_free(access, NULL) == MC_WORLD_STORAGE_OK);
    return true;
}

static bool sf_sound_failures(sf_fixture *f) {
    mc_entity *arrow = NULL;
    mc_mob_tick_error *error = calloc(1, sizeof(*error));
    mt_skeleton_fire_observation c;
    sf_probe probe = {.fixture = f, .mode = SF_SOUND_FAIL, .valid = true};
    mc_world_access *access = NULL;
    CHECK(error != NULL && sf_attach(f, &probe, &access));
    CHECK(sf_call(f, f->construction.base.player, 5, &arrow, &c, error) == MC_ENTITY_WORLD_FAILED &&
          c.constructor_returned && c.actor_float_returned && !c.sound_returned &&
          !c.heading_invoked && error->site.pc == 124 && error->native_policy &&
          !error->source_exception && error->child == MC_MOB_TICK_CHILD_SOUND &&
          probe.sounds == 1 && probe.added == 0 && probe.valid && arrow != NULL &&
          arrow->holds == 1 && sf_actor(f)->mob->living.ae == 0);
    mc_entity_release(arrow);
    arrow = NULL;
    CHECK(sf_detach(f, access) &&
          mc_world_runtime_add_world_access(f->construction.base.world, NULL, NULL) ==
              MC_WORLD_STORAGE_OK);
    CHECK(sf_call(f, f->construction.base.player, 5, &arrow, &c, error) == MC_ENTITY_WORLD_FAILED &&
          error->site.pc == 124 && error->source_exception && !error->native_policy &&
          strcmp(error->exception_class, "java/lang/NullPointerException") == 0 &&
          error->detail.sound.world_access_bytecode == 53 &&
          error->detail.sound.world_access_failure.source_null_receiver && !c.sound_returned &&
          !c.heading_invoked);
    mc_entity_release(arrow);
    bool removed = false;
    CHECK(mc_world_runtime_remove_world_access(f->construction.base.world, NULL, &removed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          removed);
    free(error);
    return true;
}

static bool sf_mutations(sf_fixture *f) {
    mc_mob_tick_error *error = calloc(1, sizeof(*error));
    mt_skeleton_fire_observation c;
    sf_probe probe = {.fixture = f, .mode = SF_PUBLICATION_FAIL, .valid = true};
    mc_world_access *access = NULL;
    CHECK(error != NULL && sf_attach(f, &probe, &access));
    const sf_mode modes[] = {SF_PUBLICATION_FAIL, SF_CLEAR,       SF_WOLF,
                             SF_RESEED,           SF_NULL_RANDOM, SF_NULL_WORLD};
    for (size_t i = 0; i < sizeof(modes) / sizeof(*modes); ++i) {
        CHECK(sf_reset(f, f->construction.base.player));
        probe.mode = modes[i];
        probe.sounds = probe.added = 0;
        probe.seen = NULL;
        mc_entity *arrow = NULL;
        const mc_entity_status status =
            sf_call(f, f->construction.base.player, 5, &arrow, &c, error);
        CHECK(probe.valid && probe.sounds == 1 && arrow != NULL && c.constructor_returned &&
              c.actor_float_returned);
        if (modes[i] == SF_PUBLICATION_FAIL) {
            CHECK(status == MC_ENTITY_WORLD_FAILED && error->site.pc == 133 &&
                  error->native_policy && error->child == MC_MOB_TICK_CHILD_ENTITY_PUBLICATION &&
                  !c.publication_returned && !c.heading_invoked && probe.added == 1 &&
                  probe.seen == arrow && arrow->base.bF && arrow->holds >= 3 &&
                  sf_actor(f)->mob->living.ae == 0 && !sf_actor(f)->mob->creature_attacking);
        } else if (modes[i] == SF_NULL_RANDOM) {
            CHECK(status == MC_ENTITY_STATE_ERROR && error->site.pc == 152 &&
                  error->source_exception && c.final_heading.heading_entered &&
                  c.final_heading.site.pc == 43 && c.final_heading.gaussian_calls == 0 &&
                  !c.heading_returned && c.publication_returned &&
                  sf_actor(f)->mob->living.ae == 0 && arrow->random == NULL);
            arrow->random =
                probe.removed_random; /* Only after the actual null prefix assertions. */
            probe.removed_random = NULL;
        } else if (modes[i] == SF_NULL_WORLD) {
            CHECK(status == MC_ENTITY_STATE_ERROR && error->site.pc == 133 &&
                  error->source_exception && c.sound_returned && !c.publication_returned &&
                  !c.heading_invoked && probe.added == 0 &&
                  c.sound_world_identity == (uintptr_t)f->construction.base.world &&
                  c.publication_world_identity == 0 && sf_actor(f)->base.world == NULL);
        } else {
            CHECK(status == MC_ENTITY_OK && c.returned && c.publication_result &&
                  c.heading_returned && c.final_heading.gaussian_calls == 3 &&
                  ac_gaussian_receipt(&c.final_heading, arrow->random));
            if (modes[i] == SF_CLEAR)
                CHECK(arrow->arrow->fields.c == NULL && arrow->arrow->shooter_hold == NULL &&
                      !arrow->arrow->fields.a);
            if (modes[i] == SF_WOLF)
                CHECK(arrow->arrow->fields.c == f->construction.shooters[1] &&
                      !arrow->arrow->fields.a);
            if (modes[i] == SF_RESEED) {
                mc_java_random seeded;
                mc_java_random_seed(&seeded, 271828);
                CHECK(c.final_heading.random_before.state == seeded.state &&
                      !c.final_heading.random_before.have_next_next_gaussian);
            }
        }
        sf_actor(f)->base.world = f->construction.base.world; /* After prefix assertions. */
        mc_entity_release(arrow);
    }
    CHECK(sf_detach(f, access));
    free(error);
    return true;
}

static bool sf_reentry(sf_fixture *f) {
    mc_entity *other = NULL, *arrow = NULL;
    mc_mob_tick_error *error = calloc(1, sizeof(*error)), *nested = calloc(1, sizeof(*nested));
    mt_skeleton_fire_observation c;
    sf_probe probe = {.fixture = f, .mode = SF_REENTER, .valid = true, .nested = nested};
    mc_world_access *access = NULL;
    CHECK(error != NULL && nested != NULL && sf_new_mob(f, MC_ENTITY_CLASS_KIND_SKELETON, &other));
    probe.other = other;
    const size_t target_holds = f->construction.base.player->holds;
    CHECK(sf_attach(f, &probe, &access) &&
          sf_call(f, f->construction.base.player, 5, &arrow, &c, error) == MC_ENTITY_OK &&
          probe.valid && probe.sounds == 1 && probe.added == 1 && c.returned &&
          sf_receipts(f, arrow, &c) && f->construction.base.player->holds == target_holds &&
          !other->mob->tick_active && other->mob->living.ae == 1 && other->mob->creature_attacking);
    mc_entity_release(arrow);
    mc_entity_release(other);
    CHECK(sf_detach(f, access) && sf_reset(f, f->construction.base.player));
    const size_t live = f->construction.base.app.entities->live_entities;
    CHECK(sf_call(f, f->construction.base.player, 5, NULL, NULL, NULL) == MC_ENTITY_OK &&
          sf_actor(f)->mob->living.ae == 30 &&
          f->construction.base.app.entities->live_entities == live + 1);
    free(nested);
    free(error);
    return true;
}

#ifndef MC_SKELETON_FIRE_MAIN
#define MC_SKELETON_FIRE_MAIN main
#endif
int MC_SKELETON_FIRE_MAIN(int argc, char **argv) {
    if (argc != 3 || CD(argv[2]) != 0 || !mc_math_trig_init(&trig))
        return 2;
    static bool (*const cases[])(sf_fixture *) = {sf_guards,    sf_false,    sf_true,
                                                  sf_nulls,     sf_failures, sf_sound_failures,
                                                  sf_mutations, sf_reentry};
    for (size_t i = 0; i < sizeof(cases) / sizeof(*cases); ++i) {
        sf_fixture *f = calloc(1, sizeof(*f));
        char directory[40];
        if (f == NULL || snprintf(directory, sizeof(directory), "skeleton-fire-%zu", i) <= 0 ||
            !sf_setup(f, argv[1], directory, i >= 2 && i != 3 && i != 4) || !cases[i](f) ||
            !sf_finish(f))
            return 1;
        free(f);
        if (printf("SKELETON_FIRE_CASE id=%zu\n", i) < 0 || fflush(stdout) != 0)
            return 1;
    }
    if (printf("Skeleton firing: %zu checks;8 native component controls\n", checks) < 0 ||
        fflush(stdout) != 0)
        return 1;
    return 0;
}
