/* Reuse genuine constructors/region teardown, never its imported main. */
int dp_unused_damage_main(int, char **);
#define MC_DAMAGE_EXECUTION_MAIN dp_unused_damage_main
#include "test_damage_execution.c"
#undef MC_DAMAGE_EXECUTION_MAIN

#include "block/registry/runtime_internal.h"
#include "entity/arrow/references_internal.h"
#include "entity/mob/references_internal.h"
#include "entity/player/player_internal.h"
#include "entity/watcher_payload_internal.h"
#include "mc/entity/arrow/construction.h"
#include "world/runtime/execution_children_internal.h"

struct dp_fixture;

typedef struct dp_drift_context {
    struct dp_fixture *fixture;
    unsigned int index;
} dp_drift_context;

typedef struct dp_fixture {
    de_fixture base;
    bool answer, native_stop, projected;
    size_t saved_bindings, callbacks, invocations;
    mc_math_execution_failure independent_math;
    mc_math_class_status independent_status;
    mc_world_access *drift_listeners[2];
    dp_drift_context drift_contexts[2];
    mc_entity_tick_execution_world_token descendant;
    mc_world_storage_status push_status;
    size_t first_callbacks, second_callbacks;
    bool first_parent_armed;
    mc_entity_tick_execution_world_token bare_world;
    mc_world_storage_status bare_status;
} dp_fixture;

static bool dp_mob(dp_fixture *s, mc_entity_class_kind kind, mc_entity **out) {
    fixture *f = s->base.base.game;
    const mc_entity_class_literal *literal = NULL;
    mc_mob_error error = {0};
    DNC_CHECK(mc_entity_class_literal_get(f->entity_classes[0], kind, &literal) == MC_ENTITY_OK &&
              mc_mob_allocate(f->entity_classes[0], f->registry[0], literal, out, &error) ==
                  MC_MOB_OK &&
              mc_mob_construct(*out, f->worlds[0], &error) == MC_MOB_OK &&
              mc_entity_set_position(*out, 8.5, 64, 8.5) == MC_ENTITY_OK);
    (*out)->mob->living.Y = (*out)->mob->living.E = 20;
    (*out)->mob->living.au = (*out)->base.by = 0;
    return true;
}

static mc_entity_watch_record *dp_record(mc_entity *actor, int32_t key) {
    for (mc_entity_watch_node *node = actor->watcher->map->head; node != NULL; node = node->next)
        if (node->key == key)
            return node->record;
    return NULL;
}

static bool dp_call(dp_fixture *s, mc_entity *victim, mc_entity *source, int32_t amount) {
    fixture *f = s->base.base.game;
    ++s->invocations;
    mc_entity_damage_execution_init(&s->base.damage, &s->base.base.execution, f->classes[0],
                                    dnc_code, f->registry[0]);
    s->base.base.raw = MC_ENTITY_LIMIT;
    s->answer = false;
    return mc_entity_effect_damage_in_execution(&s->base.damage, victim, source, amount, &s->answer,
                                                &s->base.base.effect, &s->base.base.raw);
}

static bool dp_close(dp_fixture *s) {
    DNC_CHECK(mc_entity_damage_execution_native_close(&s->base.damage) == MC_WORLD_STORAGE_OK &&
              mc_entity_damage_execution_cleanup(&s->base.damage, &s->base.base.effect) ==
                  MC_ENTITY_OK &&
              s->base.damage.cleaned);
    mc_entity_effect_error_init(&s->base.base.effect);
    return true;
}

static bool dp_missing_math(dp_fixture *s) {
    /* Retire these unused initialized contexts, then construct NEW logical
     * contexts in the returned storage with declared IN=NULL. No live owner or
     * failure is overwritten and no Source Math object is replaced. */
    DNC_CHECK(mc_math_execution_retire(&s->base.base.math) == MC_MATH_CLASS_OK &&
              mc_entity_tick_execution_retire(&s->base.base.execution) == MC_WORLD_STORAGE_OK);
    mc_math_execution_init(&s->base.base.math, NULL);
    mc_entity_tick_execution_init(&s->base.base.execution, &s->base.base.math);
    return true;
}

static bool dp_context_done(dp_fixture *s) {
    mc_entity_effect_error_init(&s->base.base.effect);
    DNC_CHECK(mc_entity_tick_execution_release_receipts(&s->base.base.execution) ==
                  MC_WORLD_STORAGE_OK &&
              mc_math_execution_retire(&s->base.base.math) == MC_MATH_CLASS_OK &&
              mc_entity_tick_execution_retire(&s->base.base.execution) == MC_WORLD_STORAGE_OK &&
              mc_math_classes_release(&s->base.base.math_owner, NULL) == MC_MATH_CLASS_OK);
    return true;
}

static bool dp_victims(dp_fixture *s) {
    static const mc_entity_class_kind kinds[] = {
        MC_ENTITY_CLASS_KIND_SPIDER,  MC_ENTITY_CLASS_KIND_ZOMBIE,  MC_ENTITY_CLASS_KIND_SKELETON,
        MC_ENTITY_CLASS_KIND_CREEPER, MC_ENTITY_CLASS_KIND_SHEEP,   MC_ENTITY_CLASS_KIND_PIG,
        MC_ENTITY_CLASS_KIND_COW,     MC_ENTITY_CLASS_KIND_CHICKEN, MC_ENTITY_CLASS_KIND_SLIME,
        MC_ENTITY_CLASS_KIND_SQUID,   MC_ENTITY_CLASS_KIND_GHAST,   MC_ENTITY_CLASS_KIND_WOLF};
    DNC_CHECK(mc_entity_set_position(s->base.base.player, 11.5, 64, 12.5) == MC_ENTITY_OK);
    for (size_t i = 0; i < COUNT(kinds); ++i) {
        mc_entity *actor = NULL;
        DNC_CHECK(dp_mob(s, kinds[i], &actor));
        if (kinds[i] == MC_ENTITY_CLASS_KIND_WOLF)
            (void)mc_entity_watch_byte_replace(dp_record(actor, 16), 5);
        DNC_CHECK(dp_call(s, actor, s->base.base.player, 4) && s->answer &&
                  s->base.base.raw == MC_ENTITY_OK && actor->mob->living.Y == 16 &&
                  s->base.base.effect.living_damage.returned &&
                  s->base.base.effect.living_damage.knockback_returned &&
                  s->base.base.effect.living_damage.sound_returned &&
                  s->base.base.effect.living_damage.private_calls == 2 &&
                  s->base.damage.owned.ownership_status == MC_WORLD_STORAGE_OK &&
                  !s->base.base.execution.world_open && !mc_entity_effect_has_pending(actor) &&
                  dp_close(s));
        printf("DAMAGE_VICTIM kind=%d health=16 accepted=1\n", (int)kinds[i]);
        mc_entity_release(actor);
    }
    return true;
}

static bool dp_early(dp_fixture *s) {
    mc_entity *actor = s->base.base.skeleton;
    mc_living_entity_fields *living = &actor->mob->living;
    mc_world_runtime *world = s->base.base.game->worlds[0];
    for (unsigned int mode = 0; mode < 3; ++mode) {
        world->fields.B = mode == 0;
        living->Y = mode == 1 ? 0 : 20;
        living->E = 20;
        living->au = 4;
        actor->base.by = mode == 2 ? 20 : 0;
        DNC_CHECK(dp_call(s, actor, NULL, 4) && !s->answer && s->base.base.raw == MC_ENTITY_OK &&
                  !s->base.base.math.selected && s->base.base.math.owned == NULL &&
                  !s->base.base.effect.living_damage.health_stored &&
                  s->base.base.effect.living_damage.private_calls == 0 &&
                  !s->base.base.execution.world_open && dp_close(s));
    }
    return true;
}

static bool dp_math(dp_fixture *s) {
    mc_entity *actor = s->base.base.skeleton;
    DNC_CHECK(dp_missing_math(s) && mc_entity_set_position(actor, 8.5, 64, 8.5) == MC_ENTITY_OK &&
              mc_entity_set_position(s->base.base.player, 11.5, 64, 12.5) == MC_ENTITY_OK);
    actor->mob->living.Y = actor->mob->living.E = 20;
    actor->base.by = actor->mob->living.au = 0;
    const double x = actor->base.velocity_x, y = actor->base.velocity_y, z = actor->base.velocity_z;
    DNC_CHECK(
        !dp_call(s, actor, s->base.base.player, 4) && !s->answer &&
        s->base.base.raw == MC_ENTITY_OK && actor->mob->living.Y == 16 &&
        actor->base.velocity_x == x && actor->base.velocity_y == y && actor->base.velocity_z == z &&
        s->base.damage.math_failed && s->base.damage.child == MC_ENTITY_DAMAGE_CHILD_KNOCKBACK &&
        !s->base.base.effect.living_damage.returned &&
        !s->base.base.effect.living_damage.sound_returned && !s->base.base.execution.world_open &&
        s->base.damage.owned.ownership_status == MC_WORLD_STORAGE_OK);
    mc_entity_damage_failure failure;
    DNC_CHECK(mc_entity_damage_execution_source_failure(&s->base.damage, &failure) &&
              failure.kind == MC_ENTITY_DAMAGE_FAILURE_NATIVE && !failure.source_proven &&
              failure.math == &s->base.damage.math_failure && failure.math->caller.pc == 9 &&
              strcmp(failure.math->caller.class_name, "ls") == 0 && dp_close(s));
    return true;
}

static mc_world_storage_status dp_native_callback(void *context, mc_world_runtime *world,
                                                  const mc_registry_string *name, double x,
                                                  double y, double z, float volume, float pitch,
                                                  mc_world_storage_error *cause,
                                                  mc_world_access_failure *failure) {
    (void)name;
    (void)x;
    (void)y;
    (void)z;
    (void)volume;
    (void)pitch;
    (void)failure;
    dp_fixture *s = context;
    ++s->callbacks;
    bool removed = false;
    const mc_world_storage_status status =
        mc_world_runtime_remove_world_access(world, s->base.observer, &removed, cause);
    if (status != MC_WORLD_STORAGE_OK || !removed)
        return status == MC_WORLD_STORAGE_OK ? MC_WORLD_STORAGE_UNSUPPORTED_STATE : status;
    s->saved_bindings = world->persistent_bindings;
    world->persistent_bindings = 0; /* Native integrity control only, restored before inspection. */
    s->projected = true;
    return MC_WORLD_STORAGE_OK;
}

static bool dp_native(dp_fixture *s) {
    fixture *f = s->base.base.game;
    mc_entity *actor = s->base.base.skeleton;
    actor->mob->living.Y = actor->mob->living.E = 20;
    actor->base.by = actor->mob->living.au = 0;
    const mc_world_access_source_callbacks callbacks = {.sound = dp_native_callback};
    DNC_CHECK(mc_world_access_source_create(&callbacks, s, &s->base.observer, NULL) ==
                  MC_WORLD_STORAGE_OK &&
              mc_world_runtime_add_world_access(f->worlds[0], s->base.observer, NULL) ==
                  MC_WORLD_STORAGE_OK);
    const bool completed = dp_call(s, actor, NULL, 4);
    if (s->projected) {
        f->worlds[0]->persistent_bindings = s->saved_bindings;
        s->projected = false;
    }
    DNC_CHECK(!completed && !s->answer && s->callbacks == 1 && s->base.base.raw == MC_ENTITY_OK &&
              actor->mob->living.Y == 16 && s->base.damage.child == MC_ENTITY_DAMAGE_CHILD_SOUND &&
              s->base.damage.sound.world_returned &&
              s->base.damage.sound.world_status == MC_WORLD_STORAGE_OK &&
              s->base.damage.owned.ownership_status != MC_WORLD_STORAGE_OK &&
              !s->base.base.effect.living_damage.returned && actor->mob->creature_target == NULL);
    mc_entity_damage_failure failure;
    DNC_CHECK(mc_entity_damage_execution_source_failure(&s->base.damage, &failure) &&
              failure.kind == MC_ENTITY_DAMAGE_FAILURE_NATIVE && !failure.source_proven &&
              failure.math == NULL && dp_close(s) &&
              mc_world_access_free(s->base.observer, NULL) == MC_WORLD_STORAGE_OK);
    s->base.observer = NULL;
    return true;
}

static mc_world_storage_status dp_older_math_callback(void *context, mc_world_runtime *world,
                                                      const mc_registry_string *name, double x,
                                                      double y, double z, float volume, float pitch,
                                                      mc_world_storage_error *cause,
                                                      mc_world_access_failure *failure) {
    (void)name;
    (void)x;
    (void)y;
    (void)z;
    (void)volume;
    (void)pitch;
    (void)failure;
    dp_fixture *s = context;
    ++s->callbacks;
    /* Independent Native callback diagnostic on the caller's explicit shared
     * context. It is NOT a require invoked by the later death body. The real
     * declared-NULL failure stays in that context through outer cleanup. */
    s->independent_status = mc_math_execution_require(
        &s->base.base.math, (mc_math_execution_site){"native.independent", "diagnostic", "()V", 37},
        "a", "(D)F");
    s->independent_math = s->base.base.math.current;
    mc_entity *victim = s->base.base.skeleton;
    mc_world_runtime *old = victim->base.world;
    victim->base.world = NULL;
    mc_world_runtime_binding_release(old); /* Real owning Source assignment; no later repair. */
    bool removed = false;
    const mc_world_storage_status status =
        mc_world_runtime_remove_world_access(world, s->base.observer, &removed, cause);
    return status != MC_WORLD_STORAGE_OK || !removed ? MC_WORLD_STORAGE_UNSUPPORTED_STATE
                                                     : MC_WORLD_STORAGE_OK;
}

static bool dp_older_math(dp_fixture *s) {
    fixture *f = s->base.base.game;
    mc_entity *actor = s->base.base.skeleton;
    actor->mob->living.Y = 4;
    actor->mob->living.E = 20;
    actor->base.by = actor->mob->living.au = 0;
    DNC_CHECK(dp_missing_math(s));
    const mc_world_access_source_callbacks callbacks = {.sound = dp_older_math_callback};
    DNC_CHECK(mc_world_access_source_create(&callbacks, s, &s->base.observer, NULL) ==
                  MC_WORLD_STORAGE_OK &&
              mc_world_runtime_add_world_access(f->worlds[0], s->base.observer, NULL) ==
                  MC_WORLD_STORAGE_OK);
    const bool completed = dp_call(s, actor, NULL, 4);
    mc_entity_damage_failure failure = {0};
    const bool inspected = mc_entity_damage_execution_source_failure(&s->base.damage, &failure);
    printf("DAMAGE_OLDER_MATH completed=%d raw=%d callbacks=%zu independent=%d "
           "shared_failed=%d attached=%d source_proven=%d kind=%d child=%d death_pc=%u\n",
           completed, (int)s->base.base.raw, s->callbacks, (int)s->independent_status,
           s->base.base.math.failed, failure.math != NULL, failure.source_proven, (int)failure.kind,
           (int)s->base.damage.child, s->base.base.effect.death.death_pc);
    fflush(stdout);
    DNC_CHECK(!completed && !s->answer && s->base.base.raw == MC_ENTITY_STATE_ERROR &&
              s->callbacks == 1 && s->independent_status == MC_MATH_CLASS_INVALID_ARGUMENT &&
              s->base.base.math.failed && s->base.base.effect.death.death_pc == 38 &&
              s->base.damage.child == MC_ENTITY_DAMAGE_CHILD_MOB_DEATH && inspected &&
              failure.kind == MC_ENTITY_DAMAGE_FAILURE_SOURCE_EXCEPTION && failure.source_proven &&
              strcmp(failure.exception_class, "java/lang/NullPointerException") == 0 &&
              failure.math == NULL && !s->base.damage.math_failed && dp_close(s) &&
              s->base.base.math.failed &&
              s->base.base.math.first.caller.class_name == s->independent_math.caller.class_name &&
              s->base.base.math.first.caller.pc == 37 &&
              mc_world_access_free(s->base.observer, NULL) == MC_WORLD_STORAGE_OK);
    s->base.observer = NULL;
    return true;
}

static bool dp_null_list(dp_fixture *s) {
    fixture *f = s->base.base.game;
    mc_world_runtime *world = f->worlds[0];
    mc_entity *actor = s->base.base.skeleton;
    actor->mob->living.Y = actor->mob->living.E = 20;
    actor->base.by = actor->mob->living.au = 0;
    /* Current nullable Source u is independent of the constructor's retained
     * native ledger. Keep that real ledger live, and never restore Source u. */
    world->fields.u = NULL;
    const bool completed = dp_call(s, actor, NULL, 4);
    mc_entity_damage_failure failure = {0};
    const bool inspected = mc_entity_damage_execution_source_failure(&s->base.damage, &failure);
    printf("DAMAGE_NULL_LIST completed=%d raw=%d world_returned=%d world_raw=%d "
           "world_pc=%u access_pc=%u native=%d health=%d captures=%zu "
           "inspected=%d kind=%d source_proven=%d current_null=%d ledger_live=%d\n",
           completed, (int)s->base.base.raw, s->base.damage.sound.world_returned,
           (int)s->base.damage.sound.world_status, s->base.base.effect.world.bytecode,
           s->base.base.effect.world.world_access_bytecode,
           (int)s->base.damage.owned.ownership_status, actor->mob->living.Y,
           s->base.base.execution.receipt_count, inspected, (int)failure.kind,
           failure.source_proven, world->fields.u == NULL, world->world_accesses.list != NULL);
    fflush(stdout);
    DNC_CHECK(!completed && !s->answer && s->base.base.raw == MC_ENTITY_WORLD_FAILED &&
              s->base.damage.sound.entered && s->base.damage.sound.scope_created &&
              s->base.damage.sound.world_returned &&
              s->base.damage.sound.world_status == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
              s->base.base.effect.world.bytecode == 9 &&
              s->base.base.effect.world.world_access_bytecode == 9 &&
              s->base.base.effect.world.world_access_fault ==
                  MC_WORLD_ACCESS_SOURCE_FAULT_NULL_SIZE_LIST &&
              s->base.damage.owned.ownership_status == MC_WORLD_STORAGE_OK &&
              actor->mob->living.Y == 16 && s->base.base.execution.receipt_count == 0 &&
              inspected && failure.kind == MC_ENTITY_DAMAGE_FAILURE_SOURCE_EXCEPTION &&
              failure.source_proven && !failure.unresolved && failure.math == NULL &&
              strcmp(failure.exception_class, "java/lang/NullPointerException") == 0 &&
              strcmp(failure.site.class_name, "fd") == 0 && failure.site.pc == 9 &&
              world->fields.u == NULL && world->world_accesses.list != NULL &&
              mc_world_accesses_owned_valid(world) && dp_close(s) && world->fields.u == NULL);
    return true;
}

static mc_world_storage_status dp_scope_callback(void *context, mc_world_runtime *world,
                                                 const mc_registry_string *name, double x, double y,
                                                 double z, float volume, float pitch,
                                                 mc_world_storage_error *cause,
                                                 mc_world_access_failure *failure) {
    (void)name;
    (void)x;
    (void)y;
    (void)z;
    (void)volume;
    (void)pitch;
    (void)failure;
    dp_drift_context *input = context;
    dp_fixture *s = input->fixture;
    ++s->callbacks;
    if (input->index == 0) {
        ++s->first_callbacks;
        mc_world_access_failure_lease *parent =
            mc_entity_tick_execution_world_lease(&s->base.base.execution);
        s->first_parent_armed =
            parent != NULL && parent->phase == MC_WORLD_ACCESS_FAILURE_LEASE_ARMED;
        s->push_status = mc_entity_tick_execution_push_world(
            &s->base.base.execution, world,
            (mc_entity_tick_execution_site){"native.damage.scope", "callback", "()V", 71},
            &s->descendant);
        if (s->push_status != MC_WORLD_STORAGE_OK) {
            *cause = (mc_world_storage_error){s->push_status,
                                              "Actual sound callback descendant scope push", 0};
            return s->push_status;
        }
    } else
        ++s->second_callbacks;
    return MC_WORLD_STORAGE_OK;
}

static bool dp_scope_drift(dp_fixture *s) {
    fixture *f = s->base.base.game;
    mc_entity *actor = s->base.base.skeleton;
    actor->mob->living.Y = actor->mob->living.E = 20;
    actor->base.by = actor->mob->living.au = 0;
    const mc_world_access_source_callbacks callbacks = {.sound = dp_scope_callback};
    for (size_t i = 0; i < COUNT(s->drift_listeners); ++i) {
        s->drift_contexts[i] = (dp_drift_context){s, (unsigned int)i};
        DNC_CHECK(mc_world_access_source_create(&callbacks, &s->drift_contexts[i],
                                                &s->drift_listeners[i],
                                                NULL) == MC_WORLD_STORAGE_OK &&
                  mc_world_runtime_add_world_access(f->worlds[0], s->drift_listeners[i], NULL) ==
                      MC_WORLD_STORAGE_OK);
    }
    const bool completed = dp_call(s, actor, NULL, 4);
    mc_entity_tick_execution_world_token current = {0};
    const bool current_read =
        mc_entity_tick_execution_current_world(&s->base.base.execution, &current);
    const bool current_descendant = current_read && dnc_token(current, s->descendant);
    printf("DAMAGE_SCOPE_DRIFT completed=%d raw=%d world_returned=%d world_raw=%d native=%d "
           "callbacks=%zu first=%zu second=%zu push=%d parent_armed=%d "
           "world_pc=%u access_pc=%u current_descendant=%d parent_open=%d health=%d\n",
           completed, (int)s->base.base.raw, s->base.damage.sound.world_returned,
           (int)s->base.damage.sound.world_status, (int)s->base.damage.owned.ownership_status,
           s->callbacks, s->first_callbacks, s->second_callbacks, (int)s->push_status,
           s->first_parent_armed, s->base.base.effect.world.bytecode,
           s->base.base.effect.world.world_access_bytecode, current_descendant,
           s->base.damage.sound.scope_open, actor->mob->living.Y);
    fflush(stdout);
    DNC_CHECK(!completed && !s->answer && s->base.base.raw == MC_ENTITY_OK &&
              s->base.damage.sound.world_returned &&
              s->base.damage.sound.world_status == MC_WORLD_STORAGE_OK &&
              s->base.damage.owned.ownership_status != MC_WORLD_STORAGE_OK && s->callbacks == 1 &&
              s->first_callbacks == 1 && s->second_callbacks == 0 &&
              s->push_status == MC_WORLD_STORAGE_OK && s->first_parent_armed &&
              s->base.base.effect.world.world_access_bytecode == 53 && current_descendant &&
              s->base.damage.sound.scope_open && actor->mob->living.Y == 16 &&
              !s->base.base.effect.living_damage.returned);
    DNC_CHECK(mc_entity_damage_execution_native_close(&s->base.damage) == MC_WORLD_STORAGE_BUSY &&
              mc_entity_tick_execution_current_world(&s->base.base.execution, &current) &&
              dnc_token(current, s->descendant) && s->callbacks == 1 &&
              mc_entity_tick_execution_pop_world(&s->base.base.execution, &s->descendant, false) ==
                  MC_WORLD_STORAGE_OK &&
              dp_close(s) && s->callbacks == 1);
    for (size_t i = 0; i < COUNT(s->drift_listeners); ++i) {
        bool removed = false;
        DNC_CHECK(mc_world_runtime_remove_world_access(f->worlds[0], s->drift_listeners[i],
                                                       &removed, NULL) == MC_WORLD_STORAGE_OK &&
                  removed &&
                  mc_world_access_free(s->drift_listeners[i], NULL) == MC_WORLD_STORAGE_OK);
        s->drift_listeners[i] = NULL;
    }
    return true;
}

static bool dp_scope_drift_bare(dp_fixture *s) {
    fixture *f = s->base.base.game;
    const mc_world_access_source_callbacks callbacks = {.sound = dp_scope_callback};
    for (size_t i = 0; i < COUNT(s->drift_listeners); ++i) {
        s->drift_contexts[i] = (dp_drift_context){s, (unsigned int)i};
        DNC_CHECK(mc_world_access_source_create(&callbacks, &s->drift_contexts[i],
                                                &s->drift_listeners[i],
                                                NULL) == MC_WORLD_STORAGE_OK &&
                  mc_world_runtime_add_world_access(f->worlds[0], s->drift_listeners[i], NULL) ==
                      MC_WORLD_STORAGE_OK);
    }
    DNC_CHECK(mc_entity_tick_execution_push_world(
                  &s->base.base.execution, f->worlds[0],
                  (mc_entity_tick_execution_site){"native.damage.scope", "bare", "()V", 11},
                  &s->bare_world) == MC_WORLD_STORAGE_OK);
    const mc_registry_string *name = NULL;
    mc_registry_error registry = {0};
    DNC_CHECK(mc_registry_runtime_literal_ascii(f->registry[0], "mob.skeletonhurt", &name,
                                                &registry) == MC_REGISTRY_OK);
    s->bare_status = mc_world_runtime_entity_sound_in_execution(
        &s->base.base.execution, f->worlds[0], s->base.base.skeleton, name, 1.0f, 1.0f,
        &s->base.base.effect.world);
    mc_entity_tick_execution_world_token current = {0};
    const bool current_read =
        mc_entity_tick_execution_current_world(&s->base.base.execution, &current);
    printf("DAMAGE_SCOPE_BARE raw=%d error=%d callbacks=%zu first=%zu second=%zu push=%d "
           "parent_armed=%d world_pc=%u access_pc=%u current_descendant=%d\n",
           (int)s->bare_status, (int)s->base.base.effect.world.status, s->callbacks,
           s->first_callbacks, s->second_callbacks, (int)s->push_status, s->first_parent_armed,
           s->base.base.effect.world.bytecode, s->base.base.effect.world.world_access_bytecode,
           current_read && dnc_token(current, s->descendant));
    fflush(stdout);
    DNC_CHECK(s->bare_status == MC_WORLD_STORAGE_BUSY &&
              s->base.base.effect.world.status == MC_WORLD_STORAGE_BUSY && s->callbacks == 1 &&
              s->first_callbacks == 1 && s->second_callbacks == 0 &&
              s->push_status == MC_WORLD_STORAGE_OK && s->first_parent_armed &&
              s->base.base.effect.world.world_access_bytecode == 53 && current_read &&
              dnc_token(current, s->descendant) &&
              s->base.base.effect.world.world_access_fault == MC_WORLD_ACCESS_SOURCE_FAULT_NONE &&
              mc_entity_tick_execution_pop_world(&s->base.base.execution, &s->descendant, false) ==
                  MC_WORLD_STORAGE_OK &&
              mc_entity_tick_execution_pop_world(&s->base.base.execution, &s->bare_world, true) ==
                  MC_WORLD_STORAGE_OK &&
              s->callbacks == 1);
    for (size_t i = 0; i < COUNT(s->drift_listeners); ++i) {
        bool removed = false;
        DNC_CHECK(mc_world_runtime_remove_world_access(f->worlds[0], s->drift_listeners[i],
                                                       &removed, NULL) == MC_WORLD_STORAGE_OK &&
                  removed &&
                  mc_world_access_free(s->drift_listeners[i], NULL) == MC_WORLD_STORAGE_OK);
        s->drift_listeners[i] = NULL;
    }
    return true;
}

static bool dp_wolf_query(dp_fixture *s, bool missing) {
    fixture *f = s->base.base.game;
    mc_entity *wolf = NULL, *cow = NULL;
    DNC_CHECK(dp_mob(s, MC_ENTITY_CLASS_KIND_WOLF, &wolf) &&
              dp_mob(s, MC_ENTITY_CLASS_KIND_COW, &cow) &&
              mc_entity_set_position(cow, 11.5, 64, 12.5) == MC_ENTITY_OK);
    (void)mc_entity_watch_byte_replace(dp_record(wolf, 16), 1);
    if (missing) {
        wolf->base.by = 20;
        wolf->mob->living.au = 0;
        DNC_CHECK(dp_missing_math(s));
    } else {
        bool inserted = false;
        mc_world_entity_error error = {0};
        DNC_CHECK(mc_world_runtime_insert_entity(f->worlds[0], wolf, &inserted, &error) ==
                      MC_WORLD_STORAGE_OK &&
                  inserted);
    }
    const bool completed = dp_call(s, wolf, cow, 4);
    DNC_CHECK(wolf->mob->living.Y == 18 &&
              s->base.damage.child == MC_ENTITY_DAMAGE_CHILD_WOLF_QUERY);
    if (missing) {
        mc_entity_damage_execution_view view;
        mc_entity_damage_failure failure;
        DNC_CHECK(!completed && !s->answer && s->base.base.raw == MC_ENTITY_OK &&
                  mc_entity_effect_has_pending(wolf) && s->base.damage.math_failed &&
                  mc_entity_damage_execution_inspect(&s->base.damage, &view) &&
                  view.acquired_work == wolf->mob->pending_wolf_damage && view.query_returned &&
                  view.query_status == MC_WORLD_STORAGE_OK &&
                  view.result != &s->base.damage.owned &&
                  mc_entity_damage_execution_source_failure(&s->base.damage, &failure) &&
                  failure.kind == MC_ENTITY_DAMAGE_FAILURE_NATIVE && failure.math != NULL &&
                  strcmp(failure.math->caller.class_name, "fd") == 0 && dp_close(s) &&
                  !mc_entity_effect_has_pending(wolf));
    } else
        DNC_CHECK(completed && s->answer && s->base.base.raw == MC_ENTITY_OK &&
                  s->base.base.effect.wolf_damage.query_returned &&
                  s->base.base.effect.wolf_damage.wolves_examined == 1 &&
                  s->base.base.effect.wolf_damage.wolves_alerted == 1 &&
                  wolf->mob->creature_target == cow && !mc_entity_effect_has_pending(wolf) &&
                  !s->base.base.execution.world_open && dp_close(s));
    mc_entity_release(wolf);
    mc_entity_release(cow);
    return true;
}

static bool dp_wolf_owner(dp_fixture *s) {
    mc_entity *wolf = NULL;
    DNC_CHECK(dp_mob(s, MC_ENTITY_CLASS_KIND_WOLF, &wolf) &&
              mc_entity_set_position(s->base.base.player, 11.5, 64, 12.5) == MC_ENTITY_OK);
    const uint16_t name[] = {0xd801, 0xdc00, 0x212a, 0x0130, 0xd800};
    const uint16_t owner[] = {0xd801, 0xdc28, 'k', 'i', 0xd800};
    mc_player_string *new_name = NULL, *new_owner = NULL;
    DNC_CHECK(mc_player_string_create(name, COUNT(name), &new_name) == MC_PLAYER_OK &&
              mc_player_string_create(owner, COUNT(owner), &new_owner) == MC_PLAYER_OK);
    mc_player_string_release(
        (mc_player_string *)s->base.base.player->local_player->fields.player.l);
    s->base.base.player->local_player->fields.player.l = new_name; /* Transfer actual field hold. */
    mc_entity_watch_record *record = dp_record(wolf, 17);
    record->payload_kind = MC_ENTITY_WATCH_PAYLOAD_STRING;
    record->string_value = new_owner; /* Caller keeps this borrowed String live. */
    (void)mc_entity_watch_byte_replace(dp_record(wolf, 16), 5);
    DNC_CHECK(dp_call(s, wolf, s->base.base.player, 4) && s->answer && wolf->mob->living.Y == 16 &&
              dp_record(wolf, 16)->byte_value == 4 && wolf->mob->creature_target == NULL &&
              s->base.base.effect.living_damage.wrapper_pc == 275 && dp_close(s));
    mc_entity_release(wolf);
    mc_player_string_release(new_owner);
    return true;
}

static bool dp_player(dp_fixture *s, const char *mode) {
    fixture *f = s->base.base.game;
    mc_entity *player = s->base.base.player, *source = NULL;
    mc_living_entity_fields *living = mc_entity_living_fields(player);
    const bool wake = strcmp(mode, "player-wake") == 0;
    const bool fatal = strcmp(mode, "player-fatal") == 0 || strcmp(mode, "player-fatal-math") == 0;
    const bool missing = strcmp(mode, "player-fatal-math") == 0;
    const bool statistic = strcmp(mode, "player-statistic-null") == 0;
    living->Y = fatal ? 4 : 20;
    living->E = 20;
    living->au = player->base.by = 0;
    living->av = 71;
    player->local_player->fields.player.u = wake;
    f->worlds[0]->fields.B = false;
    f->worlds[0]->fields.q = 2;
    /* Authored nullable actual getstatic values. DC's null-stat return4 is
     * exercised; no invented application/StatsWriter is supplied. */
    if (!statistic)
        f->registry[0]->statistics.named[18] = NULL;
    if (fatal)
        f->registry[0]->statistics.named[19] = NULL;
    if (missing) {
        DNC_CHECK(dp_mob(s, MC_ENTITY_CLASS_KIND_CREEPER, &source) && dp_missing_math(s));
        player->base.by = 20; /* Partial hit skips knockback/sound before real GS death IN91. */
    }
    const bool completed = dp_call(s, player, source, 4);
    DNC_CHECK(living->av == 0);
    if (statistic) {
        mc_entity_damage_failure failure;
        DNC_CHECK(!completed && !s->answer && s->base.base.raw == MC_ENTITY_STATE_ERROR &&
                  living->Y == 20 && !s->base.base.effect.living_damage.entered &&
                  s->base.base.effect.statistic_error.status ==
                      MC_PLAYER_STATISTIC_NULL_REFERENCE &&
                  mc_entity_damage_execution_source_failure(&s->base.damage, &failure) &&
                  failure.kind == MC_ENTITY_DAMAGE_FAILURE_SOURCE_EXCEPTION &&
                  failure.source_proven && !failure.unresolved &&
                  strcmp(failure.exception_class, "java/lang/NullPointerException") == 0 &&
                  !s->base.base.math.selected);
    } else if (wake) {
        mc_entity_damage_failure failure;
        DNC_CHECK(!completed && !s->answer && s->base.base.raw == MC_ENTITY_STATE_ERROR &&
                  living->Y == 20 && player->local_player->fields.player.u &&
                  s->base.damage.child == MC_ENTITY_DAMAGE_CHILD_WAKE_DEPENDENCY &&
                  !s->base.base.effect.player_wake.entered &&
                  player->local_player->pending_damage_wake == NULL &&
                  !s->base.base.math.selected &&
                  mc_entity_damage_execution_source_failure(&s->base.damage, &failure) &&
                  failure.kind == MC_ENTITY_DAMAGE_FAILURE_NATIVE && !failure.source_proven &&
                  failure.site.pc == 35);
    } else if (missing) {
        mc_entity_damage_execution_view view;
        mc_entity_damage_failure failure;
        DNC_CHECK(!completed && !s->answer && s->base.base.raw == MC_ENTITY_OK && living->Y == 0 &&
                  s->base.damage.child == MC_ENTITY_DAMAGE_CHILD_PLAYER_DEATH &&
                  mc_entity_damage_execution_inspect(&s->base.damage, &view) &&
                  view.acquired_work == player->local_player->pending_death &&
                  view.player_drop == NULL && s->base.damage.math_failed &&
                  mc_entity_damage_execution_source_failure(&s->base.damage, &failure) &&
                  failure.kind == MC_ENTITY_DAMAGE_FAILURE_NATIVE && failure.math != NULL &&
                  failure.math->caller.pc == 91 &&
                  strcmp(failure.math->caller.class_name, "gs") == 0);
    } else
        DNC_CHECK(completed && s->answer && s->base.base.raw == MC_ENTITY_OK &&
                  living->Y == (fatal ? 0 : 16) && s->base.base.effect.damage_statistic_returned &&
                  s->base.base.effect.living_damage.player_health.returned &&
                  s->base.base.effect.living_damage.returned &&
                  (!fatal || s->base.base.effect.player_death.returned) &&
                  !mc_entity_effect_has_pending(player));
    DNC_CHECK(dp_close(s));
    mc_entity_release(source);
    return true;
}

static bool dp_fatal(dp_fixture *s) {
    mc_entity *actor = s->base.base.skeleton;
    actor->mob->living.Y = 4;
    actor->mob->living.E = 20;
    actor->base.by = actor->mob->living.au = 0;
    mc_java_random_reseed(actor->random, 38);
    DNC_CHECK(dp_call(s, actor, NULL, 4) && s->answer && s->base.base.raw == MC_ENTITY_OK &&
              actor->mob->living.Y == 0 && actor->mob->living.ah &&
              s->base.base.effect.living_damage.death_invoke_reached &&
              s->base.base.effect.death.returned &&
              s->base.damage.child == MC_ENTITY_DAMAGE_CHILD_MOB_DEATH &&
              !mc_entity_effect_has_pending(actor) && !s->base.base.execution.world_open &&
              dp_close(s));
    return true;
}

static bool dp_pending(dp_fixture *s) {
    fixture *f = s->base.base.game;
    mc_entity *actor = s->base.base.skeleton;
    mc_world_runtime *old = actor->base.world;
    actor->base.world = NULL;
    mc_world_runtime_binding_release(old);
    mc_entity_effect_error_init(&s->base.unrelated_error);
    mc_entity_status raw = MC_ENTITY_OK;
    DNC_CHECK(!mc_mob_death_entered_in_execution(
                  &s->base.base.execution, f->classes[0], dnc_code, actor, f->registry[0], NULL,
                  &raw, &s->base.unrelated_error, &s->base.unrelated_result) &&
              raw == MC_ENTITY_STATE_ERROR && actor->mob->pending_death != NULL);
    const void *work = actor->mob->pending_death;
    DNC_CHECK(!dp_call(s, actor, NULL, 4) && !s->answer && s->base.base.raw == MC_ENTITY_BUSY &&
              s->base.damage.prior_pending && s->base.damage.acquired_work == NULL &&
              s->base.damage.death_work == NULL && actor->mob->pending_death == work &&
              dp_close(s) && actor->mob->pending_death == work &&
              mc_entity_effect_error_retire(&s->base.unrelated_error) &&
              actor->mob->pending_death == NULL);
    mc_entity_effect_error_init(&s->base.unrelated_error);
    return true;
}

static bool dp_admission(dp_fixture *s) {
    fixture *f = s->base.base.game;
    mc_entity_effect_error *error = &s->base.base.effect;
    for (unsigned int mode = 0; mode < 3; ++mode) {
        mc_entity_damage_execution_init(&s->base.damage, mode == 0 ? NULL : &s->base.base.execution,
                                        mode == 1 ? NULL : f->classes[0], dnc_code, f->registry[0]);
        error->status = MC_ENTITY_PROFILE_FAILED;
        error->operation = "untouched Native damage preflight";
        s->base.base.raw = MC_ENTITY_LIMIT;
        s->answer = true;
        DNC_CHECK(!mc_entity_effect_damage_in_execution(&s->base.damage, s->base.base.skeleton,
                                                        NULL, 4, mode == 2 ? NULL : &s->answer,
                                                        error, &s->base.base.raw) &&
                  s->answer && s->base.base.raw == MC_ENTITY_LIMIT &&
                  error->status == MC_ENTITY_PROFILE_FAILED && !s->base.damage.entered &&
                  s->base.damage.owned.ownership_status == MC_WORLD_STORAGE_INVALID_ARGUMENT &&
                  mc_entity_damage_execution_cleanup(&s->base.damage, error) == MC_ENTITY_OK);
    }
    return true;
}

int main(int argc, char **argv) {
    char root[8192];
    if (argc != 3 || !decode_path(argv[2], root, sizeof(root)))
        return 2;
    dp_fixture *s = calloc(1, sizeof(*s));
    if (s == NULL)
        return 1;
    s->base.base.game = calloc(1, sizeof(*s->base.base.game));
    printf("DAMAGE_BEGIN schema=1 mode=%s\n", argv[1]);
    fflush(stdout);
    if (s->base.base.game == NULL || !dnc_setup(&s->base.base, root))
        return 1;
    const bool passed = strcmp(argv[1], "victims") == 0                  ? dp_victims(s)
                        : strcmp(argv[1], "early") == 0                  ? dp_early(s)
                        : strcmp(argv[1], "math-missing") == 0           ? dp_math(s)
                        : strcmp(argv[1], "native-stop") == 0            ? dp_native(s)
                        : strcmp(argv[1], "older-math-no-in") == 0       ? dp_older_math(s)
                        : strcmp(argv[1], "sound-null-list") == 0        ? dp_null_list(s)
                        : strcmp(argv[1], "sound-scope-drift") == 0      ? dp_scope_drift(s)
                        : strcmp(argv[1], "sound-scope-drift-bare") == 0 ? dp_scope_drift_bare(s)
                        : strcmp(argv[1], "wolf-query") == 0             ? dp_wolf_query(s, false)
                        : strcmp(argv[1], "wolf-query-math") == 0        ? dp_wolf_query(s, true)
                        : strcmp(argv[1], "wolf-owner") == 0             ? dp_wolf_owner(s)
                        : strncmp(argv[1], "player-", 7) == 0            ? dp_player(s, argv[1])
                        : strcmp(argv[1], "mob-fatal") == 0              ? dp_fatal(s)
                        : strcmp(argv[1], "pending") == 0                ? dp_pending(s)
                        : strcmp(argv[1], "admission") == 0              ? dp_admission(s)
                                                                         : false;
    if (!passed || !dp_context_done(s) || !dnc_cleanup(&s->base.base))
        return 1;
    printf("DAMAGE_PROFILES calls=%zu callbacks=%zu\n", s->invocations, s->callbacks);
    printf("PASS: %u execution-owned damage checks;mode=%s\n", checks, argv[1]);
    free(s);
    return 0;
}
