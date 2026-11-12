/* Actual cached GAME World/Chunk constructors. The imported main never runs.
 * This exercises direct UW.K only, independently of natural LS tick removal. */
int slime_owned_unused_world_main(int, char **);
#define MC_WORLD_ENTITY_TICK_MAIN slime_owned_unused_world_main
#include "test_entity_tick.c"
#undef MC_WORLD_ENTITY_TICK_MAIN

#include "core/math/execution_internal.h"
#include "entity/mob/internal.h"
#include "entity/mob/removal_execution_internal.h"
#include "world/runtime/terminal_inventory_internal.h"
#include "world/runtime/world_access_internal.h"

#define SE_REQUIRE(condition)                                                                      \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "Slime owned line%d: %s\n", __LINE__, #condition);                     \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

typedef struct slime_owned_fixture {
    fixture *world;
    mc_math_classes *math_owner;
    mc_math_execution math;
    mc_entity_tick_execution execution;
    mc_entity_tick_execution_world_token outer, descendant;
    mc_world_runtime_execution_result owned;
    mc_entity *parent, *unrelated, *children[4];
    mc_world_access *observer;
    uint64_t occurrence;
    mc_entity_effect_error damage_error, error;
    mc_entity_status raw;
    mc_java_random expected;
    float yaw[4];
    const char *mode;
    size_t callbacks, saved_bindings, published;
    size_t saved_entity_limit;
    unsigned int native_status;
    unsigned int legacy_pc, legacy_draws;
    bool valid, completed, invariants, dead;
    bool legacy_pending;
    bool entity_limit_lowered;
    bool execution_initialized, math_initialized, outer_open, descendant_open;
} slime_owned_fixture;

static const mc_entity_tick_execution_site se_site = {"native.slime.removal", "run", "()V", 0};
static const mc_world_runtime_execution_code se_code = {{"lm", "e", "()V", 6},
                                                        {"lm", "e", "()V", 33}};

static bool se_is(const slime_owned_fixture *s, const char *mode) {
    return strcmp(s->mode, mode) == 0;
}

static bool se_token_same(mc_entity_tick_execution_world_token a,
                          mc_entity_tick_execution_world_token b) {
    return a.owner == b.owner && a.slot == b.slot && a.generation == b.generation;
}

static bool se_result_quiet(const mc_world_runtime_execution_result *r) {
    if (r->root_pending || r->selection_ready)
        return false;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_LOADERS; ++i)
        if (r->pending[i].used)
            return false;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_WORLDS; ++i)
        if (r->closes[i].used)
            return false;
    return true;
}

static bool se_make(slime_owned_fixture *s, int32_t size, double x, mc_entity **out) {
    fixture *f = s->world;
    const mc_entity_class_literal *literal = NULL;
    mc_mob_error mob = {0};
    bool inserted = false;
    SE_REQUIRE(
        mc_entity_class_literal_get(f->entity_classes[0], MC_ENTITY_CLASS_KIND_SLIME, &literal) ==
            MC_ENTITY_OK &&
        mc_mob_allocate(f->entity_classes[0], f->registry[0], literal, out, &mob) == MC_MOB_OK &&
        mc_mob_construct(*out, f->worlds[0], &mob) == MC_MOB_OK &&
        mc_mob_slime_set_size(*out, size, &mob) == MC_MOB_OK &&
        mc_entity_set_position(*out, x, 64, 8.5) == MC_ENTITY_OK &&
        mc_world_runtime_insert_entity(f->worlds[0], *out, &inserted, NULL) ==
            MC_WORLD_STORAGE_OK &&
        inserted);
    return true;
}

static mc_world_storage_status se_added(void *context, mc_world_runtime *world, mc_entity *child,
                                        mc_world_storage_error *cause,
                                        mc_world_access_failure *failure) {
    slime_owned_fixture *s = context;
    (void)failure;
    const size_t i = s->callbacks++;
    mc_slime_removal_state state;
    mc_slime_removal_execution_view view;
    mc_entity_tick_execution_world_token current;
    mc_mob_state mob = {0};
    const double x = 8.5 + (i % 2 == 0 ? -.25 : .25);
    const double z = 8.5 + (i / 2 == 0 ? -.25 : .25);
    s->valid &=
        i < 4 && world == s->world->worlds[0] && child != NULL && child != s->parent &&
        child != s->unrelated && child->classes == s->parent->classes &&
        child->phase == MC_ENTITY_READY && child->base.world == world && child->base.bF &&
        child->base.bH == 4 && !child->base.be && !s->parent->base.be && child->base.x == x &&
        child->base.y == 64.5 && child->base.z == z && child->base.aJ == x &&
        child->base.aK == 64.5 && child->base.aL == z && child->base.yaw == s->yaw[i < 4 ? i : 0] &&
        child->base.aT == 0.f && mc_mob_inspect(child, &mob) && mob.watcher_16_present &&
        mob.watcher_16 == 1 && mob.living.Y == 1 && child->base.width == .6f &&
        child->base.height == .6f && mc_slime_removal_inspect(s->parent, &state) && state.active &&
        state.pending && state.size == 2 && state.health == 0 && state.index == (int32_t)i &&
        state.parent_draws == i + 1 && state.children_completed == i &&
        mc_slime_removal_execution_inspect(s->parent, &view) && view.execution == &s->execution &&
        view.current_child == child && view.constructor_value == world &&
        view.publication_value == world && view.constructor_world.consumed &&
        view.publication_world.open &&
        mc_entity_tick_execution_current_world(&s->execution, &current) &&
        se_token_same(current, view.publication_world.world) &&
        s->execution.leases[current.slot].phase == MC_WORLD_ACCESS_FAILURE_LEASE_ARMED &&
        world->fields.b->count == i + 3 && world->entity_membership.entries[i + 2] == child &&
        s->world->chunks[0]->entities[4].items[i + 2] == child &&
        mc_slime_removal_cleanup(s->parent, NULL) == MC_ENTITY_BUSY;
    for (size_t j = 0; j < i && j < 4; ++j)
        s->valid &= s->children[j] != child && s->children[j]->base.id != child->base.id;
    if (i < 4)
        s->children[i] = child; /* Borrow the actual World/Chunk occurrences. */
    if (!s->valid) {
        *cause = (mc_world_storage_error){MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                          "Slime owned actual publication input", 0};
        return cause->status;
    }
    if (i == 1 && se_is(s, "scope-drift")) {
        const mc_world_storage_status pushed =
            mc_entity_tick_execution_push_world(&s->execution, world, se_site, &s->descendant);
        s->descendant_open = pushed == MC_WORLD_STORAGE_OK;
        return pushed;
    }
    if (i == 1 && se_is(s, "native-close")) {
        s->saved_bindings = world->persistent_bindings;
        world->persistent_bindings = 0; /* Existing genuine downward native-counter fault. */
    }
    if ((i == 1 && se_is(s, "owned-release")) || (i == 3 && se_is(s, "owned-final-release"))) {
        s->saved_bindings = world->persistent_bindings;
        /* Two actual execution scopes share this World. Leave their two
         * ledger bindings, or three for pop plus both ordinary occurrences. */
        world->persistent_bindings = se_is(s, "owned-release") ? 2u : 3u;
    }
    if (i == 0 && se_is(s, "child-allocation")) {
        s->saved_entity_limit = s->parent->classes->limits.max_live_entities;
        s->parent->classes->limits.max_live_entities = s->parent->classes->live_entities;
        s->entity_limit_lowered = true; /* Native allocation ceiling, not Source state. */
    }
    *cause = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status se_legacy_added(void *context, mc_world_runtime *world,
                                               mc_entity *child, mc_world_storage_error *cause) {
    slime_owned_fixture *s = context;
    const size_t i = s->callbacks++;
    mc_slime_removal_state state;
    mc_mob_state mob = {0};
    s->valid &= i < 2 && world == s->world->worlds[0] && child != NULL && child != s->parent &&
                child != s->unrelated && mc_mob_inspect(child, &mob) &&
                mob.kind == MC_ENTITY_CLASS_KIND_SLIME && mob.watcher_16_present &&
                mob.watcher_16 == 1 && mob.living.Y == 1 &&
                child->base.x == 8.5 + (i == 0 ? -.25 : .25) && child->base.y == 64.5 &&
                child->base.z == 8.25 && child->base.yaw == s->yaw[i < 2 ? i : 0] &&
                !child->base.be && !s->parent->base.be &&
                mc_slime_removal_inspect(s->parent, &state) && state.active && state.pending &&
                state.size == 2 && state.health == 0 && state.index == (int32_t)i &&
                state.parent_draws == i + 1 && world->fields.b->count == i + 3 &&
                world->fields.b->items[i + 2] == child &&
                s->world->chunks[0]->entities[4].items[i + 2] == child &&
                mc_slime_removal_cleanup(s->parent, NULL) == MC_ENTITY_BUSY;
    if (i < 2)
        s->children[i] = child; /* Actual publication borrows; no extra occurrence. */
    if (!s->valid) {
        *cause = (mc_world_storage_error){MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                          "Slime legacy actual publication input", 0};
        return cause->status;
    }
    if (i == 1) {
        s->saved_bindings = world->persistent_bindings;
        world->persistent_bindings = 0; /* Native release guard fault, no Source field repair. */
    }
    *cause = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

static bool se_run_legacy(slime_owned_fixture *s) {
    fixture *f = s->world;
    mc_world_runtime *world = f->worlds[0];
    SE_REQUIRE(se_make(s, 2, 8.5, &s->parent) && se_make(s, 1, 5.5, &s->unrelated));
    bool accepted = false;
    SE_REQUIRE(mc_entity_effect_damage_with_math(s->parent, f->registry[0], s->math_owner, NULL, 4,
                                                 &accepted, &s->damage_error) == MC_ENTITY_OK &&
               accepted && s->parent->mob->living.Y == 0 && !s->parent->base.be &&
               !mc_entity_effect_has_pending(s->parent));
    s->expected = *s->parent->random;
    for (size_t i = 0; i < 2; ++i) {
        const float draw = mc_java_random_next_float(&s->expected);
        volatile float yaw = draw * 360.f;
        s->yaw[i] = yaw;
    }
    const mc_world_access_callbacks callbacks = {.entity_added = se_legacy_added};
    SE_REQUIRE(mc_world_access_create(&callbacks, s, &s->observer, NULL) == MC_WORLD_STORAGE_OK &&
               mc_world_access_add_receipted(world, s->observer, &s->occurrence, NULL) ==
                   MC_WORLD_STORAGE_OK);
    /* Public legacy entry, with no execution object or borrowed owned result. */
    s->raw =
        mc_entity_effect_dead_with_context(s->parent, f->registry[0], s->math_owner, &s->error);
    if (s->saved_bindings != 0) {
        world->persistent_bindings = s->saved_bindings;
        s->saved_bindings = 0;
    }
    mc_slime_removal_state state;
    SE_REQUIRE(mc_slime_removal_inspect(s->parent, &state));
    s->dead = s->parent->base.be;
    s->published = world->fields.b->count - 2;
    s->legacy_pending = state.pending;
    s->legacy_pc = state.removal_pc;
    s->legacy_draws = state.parent_draws;
    s->native_status = (unsigned int)state.cleanup_status;
    SE_REQUIRE(
        s->raw != MC_ENTITY_OK && s->error.status == s->raw && s->valid && state.pending &&
        !state.active && !state.returned && !state.be_stored &&
        state.cleanup_status != MC_ENTITY_OK && state.index == 1 && state.parent_draws == 2 &&
        state.children_attempted == 2 && state.children_completed == 2 &&
        state.children_released == 1 && s->callbacks == 2 && s->published == 2 && !s->dead &&
        s->parent->random->state == s->expected.state && world->fields.b->count == 4 &&
        f->chunks[0]->entities[4].count == 4 && world->fields.b->items[0] == s->parent &&
        world->fields.b->items[1] == s->unrelated && world->fields.b->items[2] == s->children[0] &&
        world->fields.b->items[3] == s->children[1] && s->unrelated->mob->living.Y == 1 &&
        !s->unrelated->base.be && s->unrelated->base.x == 5.5 && !s->execution_initialized &&
        !s->math_initialized);
    s->invariants = true;
    return true;
}

static bool se_pending(slime_owned_fixture *s) {
    mc_world_runtime *world = s->world->worlds[0];
    mc_slime_removal_state state;
    mc_slime_removal_execution_view view;
    SE_REQUIRE(mc_slime_removal_inspect(s->parent, &state) && state.pending && !state.active &&
               !state.returned && !state.be_stored && state.receiver == s->parent &&
               state.receiver_held && state.size == 2 && state.health == 0 &&
               mc_slime_removal_execution_inspect(s->parent, &view) &&
               view.execution == &s->execution && view.result != &s->owned &&
               view.current_child != NULL && view.publication != NULL && !view.completed &&
               view.publication_returned && se_result_quiet(view.result));
    if (se_is(s, "sticky-math")) {
        SE_REQUIRE(state.parent_draws == 1 && state.index == 0 && state.children_attempted == 1 &&
                   state.constructors_returned == 1 && state.poses_returned == 1 &&
                   state.children_completed == 0 && !state.publication_continued &&
                   state.publication_status == MC_WORLD_STORAGE_OK &&
                   view.publication->body_entered && !view.publication->get_returned &&
                   view.result->math_failed && view.result->math_owner == &s->math &&
                   view.result->math_failure.caller.pc == 8 &&
                   strcmp(view.result->math_failure.caller.class_name, "fd") == 0 &&
                   strcmp(view.result->math_failure.caller.method, "b") == 0 &&
                   view.result->math_failure.cause.status == s->math.first.cause.status &&
                   s->owned.math_failed && s->owned.math_owner == &s->math &&
                   s->owned.math_failure.caller.pc == 8 && s->callbacks == 0);
        return true;
    }
    SE_REQUIRE(state.parent_draws == 2 && state.index == 1 && state.children_attempted == 2 &&
               state.constructors_returned == 2 && state.children_completed == 1 &&
               state.children_released == 1 && s->callbacks == 2 && view.publication_world.open &&
               !view.publication_world.consumed && view.publication_world.close_attempted &&
               view.publication_world.locals_drained && !view.result->math_failed &&
               view.result->ownership_status != MC_WORLD_STORAGE_OK);
    mc_entity_tick_execution_world_token current;
    const void *const pending = s->parent->mob->pending_slime_removal;
    const size_t count = world->fields.b->count;
    const uint64_t random = s->parent->random->state;
    mc_entity_effect_error cleanup;
    if (se_is(s, "scope-drift")) {
        SE_REQUIRE(s->descendant_open &&
                   mc_entity_tick_execution_current_world(&s->execution, &current) &&
                   se_token_same(current, s->descendant) &&
                   mc_slime_removal_native_close(s->parent) == MC_WORLD_STORAGE_BUSY &&
                   mc_entity_effect_cleanup(s->parent, &cleanup) != MC_ENTITY_OK &&
                   s->parent->mob->pending_slime_removal == pending &&
                   mc_entity_tick_execution_current_world(&s->execution, &current) &&
                   se_token_same(current, s->descendant));
        SE_REQUIRE(mc_entity_tick_execution_pop_world(&s->execution, &s->descendant, true) ==
                   MC_WORLD_STORAGE_OK);
        s->descendant_open = false;
    } else {
        const size_t bindings = world->persistent_bindings;
        world->persistent_bindings =
            0; /* Native-only cleanup failure, restored before assertion. */
        const mc_world_storage_status close = mc_slime_removal_native_close(s->parent);
        world->persistent_bindings = bindings;
        SE_REQUIRE(close != MC_WORLD_STORAGE_OK &&
                   s->parent->mob->pending_slime_removal == pending &&
                   mc_slime_removal_execution_inspect(s->parent, &view) &&
                   view.publication_world.open && !view.publication_world.consumed);
    }
    SE_REQUIRE(world->fields.b->count == count && s->callbacks == 2 &&
               s->parent->random->state == random && !s->parent->base.be);
    return true;
}

static bool se_admission(slime_owned_fixture *s) {
    mc_world_runtime *world = s->world->worlds[0];
    mc_entity_tick_execution_world_token before, after;
    SE_REQUIRE(mc_entity_tick_execution_current_world(&s->execution, &before));
    const size_t count = world->fields.b->count, bindings = world->persistent_bindings;
    const size_t holds = s->parent->holds, worlds = s->execution.world_count;
    const size_t leases = s->execution.lease_count, live = s->parent->classes->live_entities;
    const uint64_t random = s->parent->random->state;
    const int32_t next_id = s->parent->classes->next_id;
    for (unsigned int mode = 0; mode < 7; ++mode) {
        mc_entity_effect_error error;
        mc_entity_effect_error_init(&error);
        error.status = MC_ENTITY_BUSY;
        error.operation = "preserved owned removal admission";
        error.close_bytecode = 203;
        const mc_entity_effect_error saved = error;
        mc_entity_status raw = MC_ENTITY_BUSY;
        mc_world_runtime_execution_result result;
        mc_world_runtime_execution_result_init(&result);
        const mc_entity_tick_execution *identity = s->execution.identity;
        mc_math_execution *const math = s->execution.math;
        if (mode == 1)
            s->execution.identity = NULL; /* Native descriptor fault only. */
        if (mode == 2)
            s->execution.math = NULL;
        const bool continued = mc_slime_removal_entered_in_execution(
            mode == 0 ? NULL : &s->execution, mode == 3 ? NULL : s->world->classes[0], se_code,
            s->parent, mode == 4 ? NULL : &raw, mode == 5 ? NULL : &error,
            mode == 6 ? NULL : &result);
        s->execution.identity = identity;
        s->execution.math = math;
        SE_REQUIRE(!continued && raw == MC_ENTITY_BUSY &&
                   memcmp(&error, &saved, sizeof(error)) == 0 &&
                   (mode == 6 || result.ownership_status != MC_WORLD_STORAGE_OK) &&
                   s->parent->random->state == random && s->parent->mob->living.Y == 0 &&
                   !s->parent->base.be && !mc_entity_effect_has_pending(s->parent) &&
                   s->parent->holds == holds && s->parent->classes->next_id == next_id &&
                   s->parent->classes->live_entities == live && world->fields.b->count == count &&
                   world->persistent_bindings == bindings && s->execution.world_count == worlds &&
                   s->execution.lease_count == leases &&
                   mc_entity_tick_execution_current_world(&s->execution, &after) &&
                   se_token_same(before, after) && s->callbacks == 0);
    }
    return true;
}

static bool se_restore_owned_release(slime_owned_fixture *s) {
    const bool final = se_is(s, "owned-final-release");
    const size_t consumed = final ? 3u : 1u;
    mc_world_runtime *world = s->world->worlds[0];
    mc_slime_removal_execution_view view;
    mc_slime_removal_state state;
    /* These are the actual saved per-invocation consumption witnesses. The
     * native counter is restored only after proving which releases occurred. */
    const bool observed =
        mc_slime_removal_execution_inspect(s->parent, &view) &&
        mc_slime_removal_inspect(s->parent, &state) && s->saved_bindings >= consumed &&
        world->persistent_bindings == (final ? 0u : 1u) && view.constructor_world.created &&
        view.constructor_world.consumed && !view.constructor_world.open &&
        view.publication_world.created && view.publication_world.consumed &&
        !view.publication_world.open && view.publication_world.close_attempted &&
        view.publication_world.initial_close_status == MC_WORLD_STORAGE_OK &&
        !view.publication_world.failed && view.gate_held &&
        (final ? (!view.constructor_held && !view.publication_held && view.current_child == NULL &&
                  state.children_released == 4)
               : (view.constructor_held && view.publication_held && view.current_child != NULL &&
                  state.children_released == 1));
    /* The subtraction is justified by that actual pop/release proof; no Source
     * field or completed release is restored. The assertion follows restoration. */
    if (observed) {
        world->persistent_bindings = s->saved_bindings - consumed;
        s->saved_bindings = 0;
    }
    SE_REQUIRE(observed);
    return true;
}

static bool se_released_pending(slime_owned_fixture *s) {
    const bool final = se_is(s, "owned-final-release");
    mc_slime_removal_state state;
    mc_slime_removal_execution_view view;
    SE_REQUIRE(
        mc_slime_removal_inspect(s->parent, &state) && state.pending && !state.active &&
        state.receiver_held && state.receiver == s->parent && state.size == 2 &&
        state.health == 0 && state.removal_pc == (final ? 143u : 132u) &&
        state.index == (final ? 4 : 1) && state.parent_draws == (final ? 4 : 2) &&
        state.children_completed == (final ? 4 : 2) && state.children_released == (final ? 4 : 1) &&
        state.returned == final && state.be_stored == final && s->dead == final &&
        state.cleanup_status != MC_ENTITY_OK &&
        mc_slime_removal_execution_inspect(s->parent, &view) && view.result != &s->owned &&
        se_result_quiet(view.result) && view.result->ownership_status != MC_WORLD_STORAGE_OK &&
        s->owned.ownership_status != MC_WORLD_STORAGE_OK && !view.result->math_failed &&
        !s->owned.math_failed && view.constructor_world.consumed &&
        view.publication_world.consumed && !view.constructor_world.open &&
        !view.publication_world.open);
    return true;
}

static bool se_allocation_pending(slime_owned_fixture *s) {
    mc_slime_removal_state state;
    mc_slime_removal_execution_view view;
    SE_REQUIRE(mc_slime_removal_inspect(s->parent, &state) && state.pending && !state.active &&
               !state.returned && !state.be_stored && state.index == 1 && state.removal_pc == 63 &&
               state.status == MC_ENTITY_LIMIT && state.parent_draws == 1 &&
               state.children_attempted == 2 && state.constructors_returned == 1 &&
               state.children_completed == 1 && state.children_released == 1 &&
               !state.child_allocated && !state.constructor_invoked && !state.publication_entered &&
               state.current_child == NULL && state.publication == NULL &&
               mc_slime_removal_execution_inspect(s->parent, &view) && view.current_child == NULL &&
               view.publication == NULL && view.constructor_value == NULL &&
               view.publication_value == NULL && !view.constructor_world.created &&
               !view.publication_world.created && !view.constructor_held &&
               !view.publication_held && view.gate_held && se_result_quiet(view.result) &&
               view.result->ownership_status == MC_WORLD_STORAGE_OK && !view.result->math_failed &&
               s->owned.ownership_status == MC_WORLD_STORAGE_OK && !s->owned.math_failed &&
               s->callbacks == 1 && s->published == 1 &&
               s->world->worlds[0]->fields.b->items[2] == s->children[0]);
    return true;
}

static bool se_run(slime_owned_fixture *s) {
    fixture *f = s->world;
    mc_world_runtime *world = f->worlds[0];
    SE_REQUIRE(se_make(s, 2, 8.5, &s->parent) && se_make(s, 1, 5.5, &s->unrelated));
    bool accepted = false;
    SE_REQUIRE(mc_entity_effect_damage_with_math(s->parent, f->registry[0], s->math_owner, NULL, 4,
                                                 &accepted, &s->damage_error) == MC_ENTITY_OK &&
               accepted && s->parent->mob->living.Y == 0 && !s->parent->base.be &&
               !mc_entity_effect_has_pending(s->parent));
    s->expected = *s->parent->random;
    mc_java_random preview = s->expected;
    for (size_t i = 0; i < 4; ++i) {
        const float draw = mc_java_random_next_float(&preview);
        volatile float yaw = draw * 360.f;
        s->yaw[i] = yaw;
    }
    mc_math_execution_init(&s->math, se_is(s, "sticky-math") ? NULL : s->math_owner);
    s->math_initialized = true;
    if (se_is(s, "sticky-math"))
        SE_REQUIRE(mc_math_execution_require(
                       &s->math, (mc_math_execution_site){"native.old", "failed", "()V", 77}, "b",
                       "(D)I") != MC_MATH_CLASS_OK &&
                   s->math.failed && s->math.first.caller.pc == 77);
    mc_entity_tick_execution_init(&s->execution, &s->math);
    s->execution_initialized = true;
    SE_REQUIRE(mc_entity_tick_execution_push_world(&s->execution, world, se_site, &s->outer) ==
               MC_WORLD_STORAGE_OK);
    s->outer_open = true;
    const mc_world_access_source_callbacks callbacks = {.entity_added = se_added};
    SE_REQUIRE(mc_world_access_source_create(&callbacks, s, &s->observer, NULL) ==
                   MC_WORLD_STORAGE_OK &&
               mc_world_access_add_receipted(world, s->observer, &s->occurrence, NULL) ==
                   MC_WORLD_STORAGE_OK);
    SE_REQUIRE(se_admission(s));
    mc_world_runtime_execution_result_init(&s->owned);
    s->completed = mc_slime_removal_entered_in_execution(&s->execution, f->classes[0], se_code,
                                                         s->parent, &s->raw, &s->error, &s->owned);
    if (s->entity_limit_lowered) {
        s->parent->classes->limits.max_live_entities = s->saved_entity_limit;
        s->entity_limit_lowered = false;
    }
    if (se_is(s, "owned-release") || se_is(s, "owned-final-release")) {
        SE_REQUIRE(se_restore_owned_release(s));
    } else if (s->saved_bindings != 0) {
        world->persistent_bindings =
            s->saved_bindings; /* Restore native fault, never Source fields. */
        s->saved_bindings = 0;
    }
    s->native_status = (unsigned int)s->owned.ownership_status;
    s->dead = s->parent->base.be;
    s->published = world->fields.b->count - 2;
    const bool final_release = se_is(s, "owned-final-release");
    const bool released = final_release || se_is(s, "owned-release");
    const bool allocation = se_is(s, "child-allocation");
    const size_t draws = (se_is(s, "ready") || final_release)      ? 4u
                         : (se_is(s, "sticky-math") || allocation) ? 1u
                                                                   : 2u;
    for (size_t i = 0; i < draws; ++i)
        (void)mc_java_random_next_float(&s->expected);
    SE_REQUIRE(s->raw == (allocation ? MC_ENTITY_LIMIT : MC_ENTITY_OK) &&
               (final_release || s->error.status == s->raw) && s->valid &&
               s->parent->random->state == s->expected.state && s->parent->mob->living.Y == 0 &&
               s->unrelated->mob->living.Y == 1 && !s->unrelated->base.be &&
               s->unrelated->base.x == 5.5 && world->fields.b->items[0] == s->parent &&
               world->fields.b->items[1] == s->unrelated &&
               world->fields.b->count == s->published + 2 &&
               f->chunks[0]->entities[4].count == s->published + 2);
    if (se_is(s, "ready")) {
        SE_REQUIRE(s->completed && s->dead && s->callbacks == 4 && s->published == 4 &&
                   !mc_entity_effect_has_pending(s->parent) && s->error.slime_removal.returned &&
                   s->error.slime_removal.be_stored &&
                   s->error.slime_removal.children_completed == 4 &&
                   s->error.slime_removal.children_released == 4 &&
                   s->error.slime_removal.parent_draws == 4 &&
                   s->owned.ownership_status == MC_WORLD_STORAGE_OK && !s->owned.math_failed);
    } else {
        SE_REQUIRE(!s->completed && s->dead == final_release &&
                   mc_entity_effect_has_pending(s->parent) &&
                   (released     ? se_released_pending(s)
                    : allocation ? se_allocation_pending(s)
                                 : se_pending(s)));
        const void *const pending = s->parent->mob->pending_slime_removal;
        const size_t saved_callbacks = s->callbacks, saved_count = world->fields.b->count;
        const uint64_t saved_random = s->parent->random->state;
        mc_entity_status retry_raw = MC_ENTITY_OK;
        mc_entity_effect_error retry_error;
        mc_entity_effect_error_init(&retry_error);
        mc_world_runtime_execution_result retry_result;
        mc_world_runtime_execution_result_init(&retry_result);
        SE_REQUIRE(
            !mc_slime_removal_entered_in_execution(&s->execution, f->classes[0], se_code, s->parent,
                                                   &retry_raw, &retry_error, &retry_result) &&
            retry_raw == MC_ENTITY_BUSY && s->parent->mob->pending_slime_removal == pending &&
            s->callbacks == saved_callbacks && world->fields.b->count == saved_count &&
            s->parent->random->state == saved_random && s->parent->base.be == final_release);
        /* This rejected invocation acquired no removal work. Its ordinary
         * error retirement must leave the earlier producer live for its owner. */
        const bool retry_retired = mc_entity_effect_error_retire(&retry_error);
        const bool original_live = s->parent->mob->pending_slime_removal == pending;
        if (!retry_retired || !original_live)
            fprintf(stderr, "Slime rejected receipt retirement: returned=%u original-live=%u\n",
                    (unsigned int)retry_retired, (unsigned int)original_live);
        SE_REQUIRE(retry_retired && original_live && s->callbacks == saved_callbacks &&
                   world->fields.b->count == saved_count &&
                   s->parent->random->state == saved_random && s->parent->base.be == s->dead);
        mc_entity_effect_error public_retry;
        SE_REQUIRE(mc_entity_effect_dead_with_context(s->parent, f->registry[0], s->math_owner,
                                                      &public_retry) == MC_ENTITY_BUSY &&
                   mc_entity_effect_error_retire(&public_retry) &&
                   s->parent->mob->pending_slime_removal == pending &&
                   s->callbacks == saved_callbacks && world->fields.b->count == saved_count &&
                   s->parent->random->state == saved_random && s->parent->base.be == s->dead);
        /* The nonmutating terminal graph must refuse before any container release. */
        mc_world_terminal_mob_graph *graph = NULL;
        mc_world_storage_error terminal = {0};
        const size_t count = world->fields.b->count, holds = s->parent->holds;
        SE_REQUIRE(mc_world_runtime_terminal_mob_graph_prepare(world, &graph, &terminal) ==
                       MC_WORLD_STORAGE_BUSY &&
                   graph == NULL && world->fields.b->count == count && s->parent->holds == holds &&
                   !world->entity_containers_retired && s->unrelated->mob->living.Y == 1 &&
                   !s->unrelated->base.be);
    }
    s->invariants = true;
    return true;
}

static bool se_cleanup(slime_owned_fixture *s) {
    if (s->world == NULL)
        return false;
    mc_world_runtime *world = s->world->worlds[0];
    if (s->entity_limit_lowered) {
        s->parent->classes->limits.max_live_entities = s->saved_entity_limit;
        s->entity_limit_lowered = false;
    }
    if (s->saved_bindings != 0) {
        world->persistent_bindings = s->saved_bindings;
        s->saved_bindings = 0;
    }
    if (s->descendant_open) {
        SE_REQUIRE(mc_entity_tick_execution_pop_world(&s->execution, &s->descendant, true) ==
                   MC_WORLD_STORAGE_OK);
        s->descendant_open = false;
    }
    const size_t callbacks = s->callbacks;
    const uint64_t random = s->parent == NULL ? 0 : s->parent->random->state;
    const size_t published_count = world->fields.b->count;
    const bool dead_before = s->parent != NULL && s->parent->base.be;
    SE_REQUIRE((se_is(s, "legacy-release") ||
                mc_slime_removal_native_close(s->parent) == MC_WORLD_STORAGE_OK) &&
               mc_entity_effect_error_retire(&s->error) &&
               mc_entity_effect_error_retire(&s->damage_error) &&
               !mc_entity_effect_has_pending(s->parent) && s->callbacks == callbacks &&
               (s->parent == NULL || s->parent->random->state == random));
    SE_REQUIRE(world->fields.b->count == published_count &&
               (s->parent == NULL || s->parent->base.be == dead_before) && s->unrelated != NULL &&
               s->unrelated->mob->living.Y == 1 && !s->unrelated->base.be &&
               s->unrelated->base.x == 5.5);
    if (se_is(s, "legacy-release"))
        SE_REQUIRE(world->fields.b->count == published_count && published_count == 4 &&
                   s->world->chunks[0]->entities[4].count == 4 &&
                   world->fields.b->items[2] == s->children[0] &&
                   world->fields.b->items[3] == s->children[1] && !s->parent->base.be &&
                   s->unrelated->mob->living.Y == 1 && !s->unrelated->base.be &&
                   s->unrelated->base.x == 5.5);
    s->error = s->damage_error = (mc_entity_effect_error){0};
    s->owned = (mc_world_runtime_execution_result){0}; /* Expire passive views before owners. */
    if (s->execution_initialized && s->outer_open) {
        /* An idempotent close may not consume this independent current scope. */
        mc_entity_tick_execution_world_token other, current;
        const size_t worlds = s->execution.world_count, leases = s->execution.lease_count;
        if (s->execution.phase == MC_ENTITY_TICK_EXECUTION_OPEN) {
            SE_REQUIRE(mc_entity_tick_execution_push_world(&s->execution, NULL, se_site, &other) ==
                           MC_WORLD_STORAGE_OK &&
                       mc_slime_removal_native_close(s->parent) == MC_WORLD_STORAGE_OK &&
                       mc_entity_tick_execution_current_world(&s->execution, &current) &&
                       se_token_same(current, other) &&
                       mc_entity_tick_execution_pop_world(&s->execution, &other, false) ==
                           MC_WORLD_STORAGE_OK);
        } else {
            /* A failed execution cannot acquire a fresh scope to bypass its
             * retained failure. Empty actor cleanup preserves that refusal. */
            SE_REQUIRE(s->execution.phase == MC_ENTITY_TICK_EXECUTION_FAILED &&
                       mc_entity_tick_execution_push_world(&s->execution, NULL, se_site, &other) ==
                           MC_WORLD_STORAGE_BUSY &&
                       mc_slime_removal_native_close(s->parent) == MC_WORLD_STORAGE_OK &&
                       s->execution.world_count == worlds && s->execution.lease_count == leases &&
                       mc_entity_tick_execution_current_world(&s->execution, &current) &&
                       se_token_same(current, s->outer));
        }
        SE_REQUIRE(mc_entity_tick_execution_pop_world(&s->execution, &s->outer, !s->completed) ==
                   MC_WORLD_STORAGE_OK);
        s->outer_open = false;
    }
    SE_REQUIRE((!s->execution_initialized ||
                mc_entity_tick_execution_retire(&s->execution) == MC_WORLD_STORAGE_OK) &&
               (!s->math_initialized || mc_math_execution_retire(&s->math) == MC_MATH_CLASS_OK));
    SE_REQUIRE((!s->execution_initialized ||
                (s->execution.phase == MC_ENTITY_TICK_EXECUTION_RETIRED &&
                 s->execution.world_count == 0 && s->execution.lease_count == 0 &&
                 s->execution.loader_count == 0 && s->execution.receipt_count == 0)) &&
               (!s->math_initialized || (s->math.phase == MC_MATH_EXECUTION_RETIRED &&
                                         s->math.owned == NULL && s->math.declared == NULL)));
    if (s->observer != NULL) {
        SE_REQUIRE(mc_world_access_detach_occurrence(world, s->observer, s->occurrence, NULL) ==
                       MC_WORLD_STORAGE_OK &&
                   mc_world_access_free(s->observer, NULL) == MC_WORLD_STORAGE_OK);
        s->observer = NULL;
    }
    for (size_t i = 0; i < 4; ++i)
        s->children[i] = NULL;
    mc_entity_release(s->parent);
    mc_entity_release(s->unrelated);
    s->parent = s->unrelated = NULL;
    SE_REQUIRE(tick_cleanup(s->world));
    s->world = NULL;
    SE_REQUIRE(mc_math_classes_release(&s->math_owner, NULL) == MC_MATH_CLASS_OK);
    return true;
}

int main(int argc, char **argv) {
    char root[8192];
    if (argc != 3 ||
        (strcmp(argv[1], "ready") != 0 && strcmp(argv[1], "sticky-math") != 0 &&
         strcmp(argv[1], "scope-drift") != 0 && strcmp(argv[1], "native-close") != 0 &&
         strcmp(argv[1], "legacy-release") != 0 && strcmp(argv[1], "owned-release") != 0 &&
         strcmp(argv[1], "owned-final-release") != 0 && strcmp(argv[1], "child-allocation") != 0) ||
        !decode_path(argv[2], root, sizeof(root)))
        return 2;
    slime_owned_fixture *s = calloc(1, sizeof(*s));
    if (s == NULL)
        return 1;
    s->world = calloc(1, sizeof(*s->world));
    s->mode = argv[1];
    s->valid = true;
    mc_entity_effect_error_init(&s->error);
    mc_entity_effect_error_init(&s->damage_error);
    const bool legacy = se_is(s, "legacy-release");
    printf("SLIME_REMOVAL_%s_BEGIN mode=%s\n", legacy ? "LEGACY" : "EXECUTION", s->mode);
    fflush(stdout);
    const bool prepared =
        s->world != NULL &&
        mc_math_classes_create(SIZE_MAX, &s->math_owner, NULL) == MC_MATH_CLASS_OK &&
        mc_math_classes_ensure(s->math_owner, NULL) == MC_MATH_CLASS_OK &&
        tick_setup_configured(s->world, root, 4, mc_math_classes_table(s->math_owner)) &&
        (legacy ? se_run_legacy(s) : se_run(s));
    const bool math_failed = s->owned.math_failed;
    const bool cleaned = se_cleanup(s);
    if (legacy)
        printf("SLIME_REMOVAL_LEGACY_OBSERVATION mode=%s prepared=%u raw=%u cleanup_status=%u "
               "pc=%u pending=%u draws=%u callbacks=%zu children=%zu dead=%u invariants=%u "
               "cleanup=%u\n",
               s->mode, (unsigned int)prepared, (unsigned int)s->raw, s->native_status,
               s->legacy_pc, (unsigned int)s->legacy_pending, s->legacy_draws, s->callbacks,
               s->published, (unsigned int)s->dead, (unsigned int)s->invariants,
               (unsigned int)cleaned);
    else
        printf("SLIME_REMOVAL_EXECUTION_OBSERVATION mode=%s prepared=%u completed=%u raw=%u "
               "native=%u math_failed=%u callbacks=%zu children=%zu dead=%u invariants=%u "
               "cleanup=%u\n",
               s->mode, (unsigned int)prepared, (unsigned int)s->completed, (unsigned int)s->raw,
               s->native_status, (unsigned int)math_failed, s->callbacks, s->published,
               (unsigned int)s->dead, (unsigned int)s->invariants, (unsigned int)cleaned);
    const bool passed = prepared && cleaned && s->invariants;
    free(s);
    if (!passed)
        return 1;
    printf("PASS: %u %s Slime removal checks;mode=%s\n", checks,
           legacy ? "legacy-release" : "execution-owned", argv[1]);
    return 0;
}
