/* Actual Arrow741 descendants on the genuine World/Chunk bootstrap. */
#define MC_ARROW_NATURAL_FLIGHT_MAIN arrow_handoff_unused_natural_main
#include "test_arrow_natural_flight.c"
#undef MC_ARROW_NATURAL_FLIGHT_MAIN

#include "entity/arrow/tick_internal.h"
#include "entity/mob/death_execution_internal.h"
#include "entity/mob/internal.h"

#define HH_REQUIRE(condition)                                                                      \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

typedef struct hh_fixture {
    nf_fixture flight;
    const char *mode;
    mc_world_access *observer;
    mc_world_access_receipt *receipt;
    mc_entity_tick_execution_world_token descendant;
    mc_entity *published;
    size_t sounds, impacts, additions, saved_bindings, saved_code_borrows;
    uint64_t observer_occurrence;
    bool valid, removed, descendant_open;
    mc_arrow_tick_limits limits;
} hh_fixture;

static const mc_entity_tick_execution_site hh_site = {"native.arrow.handoff", "run", "()V", 0};

static bool hh_is(const hh_fixture *s, const char *name) {
    return strcmp(s->mode, name) == 0;
}

static bool hh_text(const mc_registry_string *name, const char *expected) {
    mc_registry_string_view text = {0};
    if (!mc_registry_string_read(name, &text) || text.length != strlen(expected))
        return false;
    for (size_t i = 0; i < text.length; ++i)
        if (text.units[i] != (uint16_t)expected[i])
            return false;
    return true;
}

static mc_world_storage_status hh_source_fail(hh_fixture *s, mc_world_runtime *world,
                                              mc_world_storage_error *cause,
                                              mc_world_access_failure *failure) {
    bool removed = false;
    mc_world_storage_status status =
        mc_world_runtime_remove_world_access(world, s->observer, &removed, cause);
    if (status != MC_WORLD_STORAGE_OK || !removed)
        return status == MC_WORLD_STORAGE_OK ? MC_WORLD_STORAGE_UNSUPPORTED_STATE : status;
    s->removed = true;
    status = mc_world_access_receipt_create(s->observer, NULL, &s->receipt, failure, cause);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    s->receipt->exception_class =
        hh_is(s, "source-unknown") ? "authored/ArrowHurtError" : "java/lang/IllegalStateException";
    *cause = (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR,
                                      "Arrow actual hurt descendant Source failure", 57};
    return cause->status;
}

static mc_world_storage_status hh_sound(void *context, mc_world_runtime *world,
                                        const mc_registry_string *name, double x, double y,
                                        double z, float volume, float pitch,
                                        mc_world_storage_error *cause,
                                        mc_world_access_failure *failure) {
    hh_fixture *s = context;
    nf_fixture *n = &s->flight;
    mc_arrow_tick_call *c = n->arrow->arrow->tick_call;
    const bool impact = hh_text(name, "random.drr");
    s->valid &= world == n->world->worlds[0] && c != NULL && c->state.active && pitch > 0.f &&
                !n->arrow->base.be && c->hurt_source == NULL &&
                c->hurt_target == (impact ? NULL : n->victim);
    if (impact) {
        ++s->impacts;
        s->valid &= volume == 1.f && x == n->arrow->base.x && y == n->arrow->base.y &&
                    z == n->arrow->base.z && c->hurt == NULL && c->hurt_bytes == 0 &&
                    c->state.hurt_returned && c->state.hurt_continued && c->state.hurt_result;
    } else {
        s->valid &= c->hurt != NULL && c->hurt_bytes == sizeof(*c->hurt) && c->state.hurt_invoked &&
                    !c->state.hurt_returned && c->hurt->execution.victim == n->victim &&
                    x == n->victim->base.x &&
                    y == n->victim->base.y - (double)n->victim->base.offset &&
                    z == n->victim->base.z;
    }
    ++s->sounds;
    if (!s->valid) {
        *cause = (mc_world_storage_error){MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                          "Arrow actual hurt callback inputs", 0};
        return cause->status;
    }
    if (!impact && (hh_is(s, "source-sound") || hh_is(s, "source-unknown") ||
                    hh_is(s, "source-exact-budget") || hh_is(s, "retire-provenance")))
        return hh_source_fail(s, world, cause, failure);
    if (!impact && hh_is(s, "native-close")) {
        s->saved_bindings = world->persistent_bindings;
        world->persistent_bindings = 0; /* Declared downward native-counter fault. */
    }
    if (!impact && hh_is(s, "scope-drift")) {
        const mc_world_storage_status status =
            mc_entity_tick_execution_push_world(&c->execution, world, hh_site, &s->descendant);
        s->descendant_open = status == MC_WORLD_STORAGE_OK;
        return status;
    }
    *cause = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status hh_added(void *context, mc_world_runtime *world, mc_entity *item,
                                        mc_world_storage_error *cause,
                                        mc_world_access_failure *failure) {
    hh_fixture *s = context;
    nf_fixture *n = &s->flight;
    mc_arrow_tick_call *c = n->arrow->arrow->tick_call;
    ++s->additions;
    s->published = item;
    mc_mob_death_execution_view death = {0};
    s->valid &= c != NULL && c->hurt != NULL && c->hurt->returned == false &&
                mc_mob_death_execution_inspect(n->victim, &death) && death.has_child &&
                death.execution == &c->execution && death.child.result != &c->query_result &&
                death.child.result != &c->hurt->execution.owned && item != NULL &&
                item->phase == MC_ENTITY_READY && item->base.bF && world == n->world->worlds[0] &&
                mc_entity_living_fields(n->victim)->Y == 0 &&
                mc_entity_living_fields(n->victim)->ah && s->additions == 1;
    if (!s->valid) {
        *cause = (mc_world_storage_error){MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                          "Arrow actual fatal Item publication inputs", 0};
        return cause->status;
    }
    return hh_source_fail(s, world, cause, failure);
}

static bool hh_victim(hh_fixture *s) {
    nf_fixture *n = &s->flight;
    const mc_entity_class_literal *kind = NULL;
    mc_mob_error error;
    bool inserted = false;
    HH_REQUIRE(mc_entity_class_literal_get(n->world->entity_classes[0],
                                           hh_is(s, "wolf") ? MC_ENTITY_CLASS_KIND_WOLF
                                                            : MC_ENTITY_CLASS_KIND_PIG,
                                           &kind) == MC_ENTITY_OK &&
               mc_mob_allocate(n->world->entity_classes[0], n->world->registry[0], kind, &n->victim,
                               &error) == MC_MOB_OK &&
               mc_mob_construct(n->victim, n->world->worlds[0], &error) == MC_MOB_OK &&
               mc_entity_set_position(n->victim, 9.6, 65.75, 8.5) == MC_ENTITY_OK &&
               mc_world_runtime_insert_entity(n->world->worlds[0], n->victim, &inserted, NULL) ==
                   MC_WORLD_STORAGE_OK &&
               inserted);
    if (hh_is(s, "reject"))
        mc_entity_living_fields(n->victim)->Y = 0; /* Authored actual Source input. */
    if (hh_is(s, "impact-null-list")) {
        /* Real stronger repeat-hit inputs. LS accepts delta(4-1) without its
         * full-hit sound, so only the later Arrow impact reads Source u. */
        n->victim->base.by = 20;
        mc_entity_living_fields(n->victim)->au = 1;
    }
    if (hh_is(s, "fatal-publication")) {
        mc_entity_living_fields(n->victim)->Y = 4;
        /* Select an actual Java seed whose fatal-sound prefix leaves a real
         * nonzero ordinary LS.q quantity. No returned Source result is patched. */
        int32_t quantity = 0;
        int64_t seed = 0;
        do {
            mc_java_random expected;
            mc_java_random_seed(&expected, seed++);
            (void)mc_java_random_next_float(&expected);
            (void)mc_java_random_next_float(&expected);
            HH_REQUIRE(mc_java_random_next_bounded(&expected, 3, &quantity));
        } while (quantity == 0 && seed < 100);
        HH_REQUIRE(quantity != 0);
        mc_java_random_seed(n->victim->random, seed - 1);
    }
    return true;
}

static bool hh_drain(hh_fixture *s, mc_arrow_tick_call *c) {
    nf_fixture *n = &s->flight;
    const int32_t health = mc_entity_living_fields(n->victim)->Y;
    const mc_java_random victim_random = *n->victim->random, arrow_random = *n->arrow->random;
    const uint64_t x = ad_bits(n->arrow->base.x), vx = ad_bits(n->arrow->base.velocity_x);
    const size_t sounds = s->sounds, additions = s->additions;
    const int32_t published_id = s->published == NULL ? 0 : s->published->base.id;
    mc_arrow_tick_state state = {0};
    HH_REQUIRE(mc_arrow_tick_inspect(c, &state));
    const mc_arrow_tick_status first_status = state.failure->status;
    const mc_arrow_tick_failure_kind first_kind = state.failure->classification;
    const char *const old_name = state.failure->exception_class;
    const bool named = old_name != NULL;
    if (named) {
        bool quiet = true;
        HH_REQUIRE(
            mc_arrow_tick_native_unwind_step(c, &quiet) == MC_ARROW_TICK_OK && !quiet &&
            mc_arrow_tick_inspect(c, &state) && state.diagnostic_bytes == strlen(old_name) + 1 &&
            state.failure->exception_class != old_name &&
            strcmp(state.failure->exception_class, old_name) == 0 &&
            state.hurt_bytes == sizeof(mc_arrow_tick_hurt_work) &&
            sizeof(*c) + state.hurt_bytes + state.diagnostic_bytes <= c->limits.max_parent_bytes);
        if (hh_is(s, "source-exact-budget"))
            HH_REQUIRE(sizeof(*c) + state.hurt_bytes + state.diagnostic_bytes ==
                       c->limits.max_parent_bytes);
    }
    if (hh_is(s, "native-close") || hh_is(s, "scope-drift")) {
        bool quiet = true;
        mc_arrow_tick_call *const identity = c;
        HH_REQUIRE(mc_arrow_tick_native_unwind_step(c, &quiet) != MC_ARROW_TICK_OK && quiet &&
                   n->arrow->arrow->tick_call == identity && c->hurt != NULL &&
                   !c->hurt->cleanup_done && c->error.effect == &c->hurt->error);
        HH_REQUIRE(!mc_world_entity_tick_error_retire(tick_error) &&
                   tick_error->child_error.arrow.call == identity &&
                   n->arrow->arrow->tick_call == identity);
        if (hh_is(s, "native-close")) {
            HH_REQUIRE(s->saved_bindings != 0);
            n->world->worlds[0]->persistent_bindings = s->saved_bindings;
            s->saved_bindings = 0;
        } else {
            HH_REQUIRE(s->descendant_open &&
                       mc_entity_tick_execution_pop_world(&c->execution, &s->descendant, false) ==
                           MC_WORLD_STORAGE_OK);
            s->descendant_open = false;
        }
    }
    bool quiet = false;
    size_t actions = 0;
    while (!quiet && actions++ < 48)
        HH_REQUIRE(mc_arrow_tick_native_unwind_step(c, &quiet) == MC_ARROW_TICK_OK);
    HH_REQUIRE(quiet && actions < 48 && mc_arrow_tick_inspect(c, &state) &&
               state.failure->status == first_status &&
               state.failure->classification == first_kind && state.failure->effect == NULL &&
               state.hurt_bytes == sizeof(mc_arrow_tick_hurt_work) && c->hurt != NULL &&
               c->hurt->cleanup_done && c->hurt->execution.cleaned &&
               mc_entity_living_fields(n->victim)->Y == health &&
               n->victim->random->state == victim_random.state &&
               n->arrow->random->state == arrow_random.state && s->sounds == sounds &&
               s->additions == additions && ad_bits(n->arrow->base.x) == x &&
               ad_bits(n->arrow->base.velocity_x) == vx && !n->arrow->base.be);
    if (s->published != NULL)
        HH_REQUIRE(s->published->base.id == published_id && s->published->base.bF &&
                   s->published->phase == MC_ENTITY_READY && n->victim->mob->pending_death == NULL);
    if (s->receipt != NULL)
        HH_REQUIRE(mc_world_access_receipt_retire(&s->receipt, NULL) == MC_WORLD_STORAGE_BUSY);
    if (hh_is(s, "retire-provenance")) {
        /* A downward Native defining-owner counter fault AFTER canonical
         * hurt cleanup. The parent must preserve its inspectable diagnostic
         * even when hurt and the actual producer can now be released. */
        s->saved_code_borrows = n->world->classes[0]->native_borrows;
        HH_REQUIRE(s->saved_code_borrows != 0 && state.diagnostic_bytes != 0);
        n->world->classes[0]->native_borrows = 0;
        HH_REQUIRE(
            !mc_world_entity_tick_error_retire(tick_error) &&
            tick_error->child_error.arrow.call == c && n->arrow->arrow->tick_call == c &&
            mc_arrow_tick_inspect(c, &state) && !state.active && !state.retiring &&
            state.hurt_bytes == 0 && c->hurt == NULL && state.failure->effect == NULL &&
            state.failure->query == NULL &&
            (state.failure->registry == NULL || state.failure->registry == &c->registry_error) &&
            state.failure->status == first_status && state.failure->classification == first_kind &&
            strcmp(state.failure->exception_class, "java/lang/IllegalStateException") == 0 &&
            state.failure->exception_class == c->exception_storage &&
            state.cleanup->status == MC_ARROW_TICK_CLEANUP_FAILED);
        HH_REQUIRE(mc_world_access_receipt_retire(&s->receipt, NULL) == MC_WORLD_STORAGE_OK &&
                   mc_world_access_free(s->observer, NULL) == MC_WORLD_STORAGE_OK);
        s->observer = NULL;
        HH_REQUIRE(mc_arrow_tick_inspect(c, &state) &&
                   strcmp(state.failure->exception_class, "java/lang/IllegalStateException") == 0 &&
                   state.failure->effect == NULL && state.failure->query == NULL &&
                   mc_entity_living_fields(n->victim)->Y == health && s->sounds == sounds &&
                   s->additions == additions && !n->arrow->base.be &&
                   ad_bits(n->arrow->base.x) == x);
        n->world->classes[0]->native_borrows = s->saved_code_borrows;
        s->saved_code_borrows = 0;
    }
    HH_REQUIRE(mc_world_entity_tick_error_retire(tick_error) && n->arrow->arrow->tick_call == NULL);
    if (s->receipt != NULL)
        HH_REQUIRE(mc_world_access_receipt_retire(&s->receipt, NULL) == MC_WORLD_STORAGE_OK);
    return true;
}

static bool hh_run(hh_fixture *s) {
    nf_fixture *n = &s->flight;
    HH_REQUIRE(hh_victim(s));
    const mc_world_access_source_callbacks callbacks = {.sound = hh_sound,
                                                        .entity_added = hh_added};
    HH_REQUIRE(mc_world_access_source_create(&callbacks, s, &s->observer, NULL) ==
                   MC_WORLD_STORAGE_OK &&
               mc_world_access_add_receipted(n->world->worlds[0], s->observer,
                                             &s->observer_occurrence, NULL) == MC_WORLD_STORAGE_OK);
    s->limits = mc_arrow_tick_default_limits();
    if (hh_is(s, "workspace-limit"))
        s->limits.max_parent_bytes =
            sizeof(mc_arrow_tick_call) + sizeof(mc_arrow_tick_hurt_work) - 1;
    if (hh_is(s, "source-exact-budget"))
        s->limits.max_parent_bytes = sizeof(mc_arrow_tick_call) + sizeof(mc_arrow_tick_hurt_work) +
                                     sizeof("java/lang/IllegalStateException");
    if (hh_is(s, "impact-null-list"))
        n->world->worlds[0]->fields.u = NULL; /* Authored Source value; never repaired. */
    tick_context.arrow_limits = &s->limits;
    const mc_world_storage_status status = mc_world_runtime_tick_entity(
        n->world->worlds[0], n->arrow, &tick_context, NULL, tick_error);
    if (hh_is(s, "wolf") || hh_is(s, "reject")) {
        const bool wolf = hh_is(s, "wolf");
        HH_REQUIRE(status == MC_WORLD_STORAGE_OK && s->valid &&
                   mc_entity_living_fields(n->victim)->Y == (wolf ? 4 : 0) &&
                   s->sounds == (wolf ? 2u : 0u) && s->impacts == (wolf ? 1u : 0u) &&
                   n->arrow->base.be == wolf && n->arrow->arrow->tick_call == NULL &&
                   n->arrow->base.bt == 1 && n->arrow->arrow->fields.k == (wolf ? 1 : 0) &&
                   n->victim->mob->pending_death == NULL);
        if (!wolf) {
            /* Literal SL789/801/813 reflect by -0.10000000149011612;
             * common tail adds velocity, then applies .99f/.03f. Its live
             * yaw normalization changes reflected history270 to-90. */
            HH_REQUIRE(ad_bits(n->arrow->base.x) == UINT64_C(0x4020e147adf851ec) &&
                       ad_bits(n->arrow->base.velocity_x) == UINT64_C(0xbfae69ad638ef357) &&
                       ad_bits(n->arrow->base.velocity_y) == UINT64_C(0xbf9eb851e0000000) &&
                       ad_bits(n->arrow->base.velocity_z) == UINT64_C(0x8000000000000000) &&
                       n->arrow->base.yaw == -90.f && n->arrow->base.aU == -90.f);
        }
        return true;
    }
    HH_REQUIRE(status != MC_WORLD_STORAGE_OK &&
               tick_error->child == MC_WORLD_ENTITY_TICK_CHILD_ARROW &&
               tick_error->child_error.arrow.call != NULL);
    mc_arrow_tick_call *c = tick_error->child_error.arrow.call;
    mc_arrow_tick_state state = {0};
    if (hh_is(s, "impact-null-list")) {
        HH_REQUIRE(mc_arrow_tick_inspect(c, &state) && !state.active && !state.source_returned &&
                   state.source_pc == 771 && state.hurt_invoked && state.hurt_returned &&
                   state.hurt_continued && state.hurt_result && state.hurt_bytes == 0 &&
                   state.failure->classification == MC_ARROW_TICK_FAILURE_SOURCE_EXCEPTION &&
                   state.failure->source_exception && !state.failure->native_policy &&
                   strcmp(state.failure->exception_class, "java/lang/NullPointerException") == 0 &&
                   strcmp(state.failure->child_site.class_name, "fd") == 0 &&
                   state.failure->child_site.pc == 9 && state.failure->sound != NULL &&
                   state.failure->sound->world_access_fault ==
                       MC_WORLD_ACCESS_SOURCE_FAULT_NULL_SIZE_LIST &&
                   state.failure->sound->world_access_bytecode == 9 &&
                   state.failure->effect == NULL && c->hurt == NULL &&
                   mc_entity_living_fields(n->victim)->Y == 7 && s->sounds == 0 && s->valid &&
                   s->impacts == 0 && !n->arrow->base.be && n->world->worlds[0]->fields.u == NULL &&
                   c->execution.receipt_count == 0 &&
                   ad_bits(n->arrow->base.x) == UINT64_C(0x4021000000000000));
        HH_REQUIRE(mc_world_entity_tick_error_retire(tick_error) &&
                   n->arrow->arrow->tick_call == NULL && n->world->worlds[0]->fields.u == NULL &&
                   s->sounds == 0 && mc_entity_living_fields(n->victim)->Y == 7);
        return true;
    }
    HH_REQUIRE(mc_arrow_tick_inspect(c, &state) && !state.active && !state.source_returned &&
               state.source_pc == 741 && state.hurt_target == n->victim &&
               state.hurt_source == NULL && !n->arrow->base.be &&
               ad_bits(n->arrow->base.x) == UINT64_C(0x4021000000000000) &&
               n->arrow->arrow->fields.k == 1 && s->impacts == 0);
    if (hh_is(s, "workspace-limit")) {
        HH_REQUIRE(!state.hurt_invoked && !state.hurt_returned && !state.hurt_continued &&
                   state.hurt_bytes == 0 && c->hurt == NULL && s->sounds == 0 &&
                   mc_entity_living_fields(n->victim)->Y == 10 &&
                   state.failure->status == MC_ARROW_TICK_LIMIT &&
                   state.failure->classification == MC_ARROW_TICK_FAILURE_NATIVE &&
                   mc_world_entity_tick_error_retire(tick_error));
        return true;
    }
    HH_REQUIRE(s->valid && state.hurt_invoked && state.hurt_returned && !state.hurt_continued &&
               !state.hurt_result && state.hurt_bytes == sizeof(mc_arrow_tick_hurt_work) &&
               state.failure->effect == &c->hurt->error &&
               mc_entity_living_fields(n->victim)->Y == (hh_is(s, "fatal-publication") ? 0 : 6));
    if (s->receipt != NULL)
        HH_REQUIRE(state.failure->classification ==
                       (hh_is(s, "source-unknown") ? MC_ARROW_TICK_FAILURE_UNRESOLVED
                                                   : MC_ARROW_TICK_FAILURE_SOURCE_EXCEPTION) &&
                   state.failure->exception_class == s->receipt->exception_class &&
                   c->hurt->status == MC_ENTITY_WORLD_FAILED && s->observer->active_holds == 1 &&
                   mc_world_access_free(s->observer, NULL) == MC_WORLD_STORAGE_BUSY);
    else
        HH_REQUIRE(c->hurt->status == MC_ENTITY_OK &&
                   state.failure->classification == MC_ARROW_TICK_FAILURE_NATIVE);
    if (hh_is(s, "fatal-publication")) {
        mc_entity_damage_execution_view view = {0};
        mc_entity_drop_state drop = {0};
        HH_REQUIRE(state.hurt_cleanup_acquired && s->published != NULL && s->additions == 1 &&
                   mc_entity_damage_execution_inspect(&c->hurt->execution, &view) &&
                   view.mob_drop != NULL && view.result != &c->query_result &&
                   view.result != &c->hurt->execution.owned &&
                   mc_entity_drop_inspect(view.mob_drop, &drop) && drop.item == s->published &&
                   !drop.inserted && !drop.publication_returned && drop.publication_entered);
    }
    return hh_drain(s, c);
}

int main(int argc, char **argv) {
    hh_fixture s = {0};
    char root[8192];
    int result = 1;
    if (argc != 3 || !decode_path(argv[2], root, sizeof(root)))
        return 2;
    s.mode = argv[1];
    s.valid = true;
    if (!hh_is(&s, "source-sound") && !hh_is(&s, "source-unknown") &&
        !hh_is(&s, "source-exact-budget") && !hh_is(&s, "native-close") &&
        !hh_is(&s, "scope-drift") && !hh_is(&s, "workspace-limit") &&
        !hh_is(&s, "fatal-publication") && !hh_is(&s, "reject") && !hh_is(&s, "wolf") &&
        !hh_is(&s, "impact-null-list") && !hh_is(&s, "retire-provenance"))
        return 2;
    printf("ARROW_HURT_HANDOFF_BEGIN mode=%s\n", s.mode);
    fflush(stdout);
    CHECK(nf_setup(&s.flight, root, NULL) && nf_construct(&s.flight) && hh_run(&s));
    result = 0;
cleanup:
    if (s.saved_bindings != 0)
        s.flight.world->worlds[0]->persistent_bindings = s.saved_bindings;
    if (s.saved_code_borrows != 0)
        s.flight.world->classes[0]->native_borrows = s.saved_code_borrows;
    if (s.descendant_open) {
        mc_arrow_tick_call *c = s.flight.arrow->arrow->tick_call;
        if (c == NULL || mc_entity_tick_execution_pop_world(&c->execution, &s.descendant, false) !=
                             MC_WORLD_STORAGE_OK)
            return 1;
    }
    if (tick_error != NULL && !mc_world_entity_tick_error_retire(tick_error))
        return 1;
    if (s.receipt != NULL &&
        mc_world_access_receipt_retire(&s.receipt, NULL) != MC_WORLD_STORAGE_OK)
        return 1;
    if (s.observer != NULL) {
        if ((!s.removed && mc_world_access_detach_occurrence(s.flight.world->worlds[0], s.observer,
                                                             s.observer_occurrence,
                                                             NULL) != MC_WORLD_STORAGE_OK) ||
            mc_world_access_free(s.observer, NULL) != MC_WORLD_STORAGE_OK)
            return 1;
    }
    if (!nf_cleanup(&s.flight))
        return 1;
    if (result == 0)
        printf("PASS: %u actual Arrow hurt handoff checks;mode=%s\n", checks, s.mode);
    return result;
}
