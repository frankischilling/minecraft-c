/* Direct real leaf controls. Only constructor/region helpers are reused;
 * the imported old program never executes and no production C body is copied. */
int dnc_unused_publication_main(int, char **);
#define main dnc_unused_publication_main
#include "../test_entity_publication.c"
#undef main
#undef CHECK

#include "entity/mob/damage_execution_internal.h"
#include "entity/mob/internal.h"
#include "entity/player/ally_alert_execution_internal.h"

#include "entity/living/fields_internal.h"
#include "mc/entity/mob/construction.h"
#include "mc/entity/player/player.h"
#include "world/runtime/collision/class_entities_execution_internal.h"
#include "world/runtime/world_access_internal.h"

#define DNC_CHECK(x)                                                                               \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            fprintf(stderr, "damage-close line%d check%u: %s\n", __LINE__, checks, #x);            \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

typedef struct dnc_fixture {
    fixture *game;
    mc_math_classes *math_owner;
    mc_math_execution math;
    mc_entity_tick_execution execution;
    mc_world_runtime_execution_result owned;
    mc_living_damage_execution sound;
    mc_entity_effect_error effect;
    mc_player_ally_alert_call *ally;
    mc_player_ally_alert_state ally_state;
    mc_player_ally_alert_execution_view ally_view;
    mc_player_session_classes *sessions;
    mc_player_session *session;
    mc_player_string *username;
    mc_entity *player, *skeleton;
    mc_world_access *access;
    mc_world_access_receipt *receipt;
    mc_world_access_failure_lease *failure_node;
    mc_world_storage_error source_cause;
    mc_entity_tick_execution_error first_native;
    mc_java_random actor_after, world_before;
    unsigned char living_after[sizeof(mc_living_entity_fields)];
    mc_entity_status raw;
    size_t callbacks, saved_bindings, capture_steps, calls, primes;
    bool sound_mode, capture_fault, normal, unreached, projected, removed, callback_valid,
        continued;
} dnc_fixture;

static const mc_world_runtime_execution_code dnc_code = {{"lm", "e", "()V", 6},
                                                         {"lm", "e", "()V", 33}};
static const mc_entity_tick_execution_site dnc_reuse = {"native.damage.close", "reuse", "()V", 41};

static bool dnc_token(mc_entity_tick_execution_world_token a,
                      mc_entity_tick_execution_world_token b) {
    return a.owner == b.owner && a.slot == b.slot && a.generation == b.generation;
}

static bool dnc_error(mc_entity_tick_execution_error a, mc_entity_tick_execution_error b) {
    return a.status == b.status && a.operation == b.operation &&
           a.site.class_name == b.site.class_name && a.site.method == b.site.method &&
           a.site.descriptor == b.site.descriptor && a.site.pc == b.site.pc;
}

static bool dnc_cause(mc_world_storage_error a, mc_world_storage_error b) {
    return a.status == b.status && a.operation == b.operation && a.system_error == b.system_error;
}

static bool dnc_random(const mc_java_random *a, const mc_java_random *b) {
    uint64_t x, y;
    memcpy(&x, &a->next_next_gaussian, sizeof(x));
    memcpy(&y, &b->next_next_gaussian, sizeof(y));
    return a->state == b->state && a->have_next_next_gaussian == b->have_next_next_gaussian &&
           x == y;
}

static mc_world_storage_status dnc_callback(dnc_fixture *s, mc_world_runtime *world,
                                            mc_world_storage_error *cause,
                                            mc_world_access_failure *failure) {
    ++s->callbacks;
    s->failure_node = mc_entity_tick_execution_world_lease(&s->execution);
    s->callback_valid &= world == s->game->worlds[0] && s->callbacks == 1 &&
                         s->failure_node != NULL &&
                         s->failure_node->phase == MC_WORLD_ACCESS_FAILURE_LEASE_ARMED &&
                         s->access->active_holds == 1 && world->active_calls != 0;
    bool removed = false;
    mc_world_storage_status status =
        mc_world_runtime_remove_world_access(world, s->access, &removed, cause);
    s->removed = status == MC_WORLD_STORAGE_OK && removed;
    if (!s->removed)
        return status == MC_WORLD_STORAGE_OK ? MC_WORLD_STORAGE_UNSUPPORTED_STATE : status;
    if (s->capture_fault) {
        status = mc_world_access_receipt_create(s->access, NULL, &s->receipt, failure, cause);
        if (status != MC_WORLD_STORAGE_OK)
            return status;
        s->receipt->exception_class = "java/lang/IllegalStateException";
        *cause = (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR,
                                          "Damage-close real callback source failure", 117};
        s->source_cause = *cause;
    }
    s->saved_bindings = world->persistent_bindings;
    s->callback_valid &= s->saved_bindings != 0;
    /* Only native guard metadata is projected. Every actor occurrence stays
     * held, so no final actor release/World-binding drain occurs in this call. */
    world->persistent_bindings = 0;
    s->projected = true;
    return s->capture_fault ? cause->status : MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status dnc_sound(void *context, mc_world_runtime *world,
                                         const mc_registry_string *name, double x, double y,
                                         double z, float volume, float pitch,
                                         mc_world_storage_error *cause,
                                         mc_world_access_failure *failure) {
    dnc_fixture *s = context;
    mc_registry_string_view text = {0};
    s->callback_valid &= s->sound_mode && mc_registry_string_read(name, &text) &&
                         x == s->skeleton->base.x && z == s->skeleton->base.z &&
                         y == s->skeleton->base.y - (double)s->skeleton->base.offset &&
                         volume == 1.0f && pitch == s->effect.living_damage.pitch &&
                         s->effect.living_damage.private_calls == 2;
    if (s->callback_valid) {
        const char expected[] = "mob.skeletonhurt";
        s->callback_valid &= text.length == sizeof(expected) - 1;
        for (size_t i = 0; s->callback_valid && i < text.length; ++i)
            s->callback_valid &= text.units[i] == (uint16_t)(unsigned char)expected[i];
    }
    return dnc_callback(s, world, cause, failure);
}

static bool dnc_setup(dnc_fixture *s, const char *root) {
    fixture *f = s->game;
    DNC_CHECK(mc_math_trig_init(&trig) && mc_materials_create(&f->materials));
    DNC_CHECK(mc_java_random_profile_create(4u * 1024u * 1024u, &f->profile) ==
                  MC_JAVA_RANDOM_PROFILE_OK &&
              mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK);
    const uint16_t title[] = {'D', 'a', 'm', 'a', 'g', 'e', 'C', 'l', 'o', 's', 'e'};
    DNC_CHECK(mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
              MC_WORLD_INFO_OK);
    resources *r = &f->sources[0];
    r->streams[0] = (stream){.bytes = language, .length = sizeof(language) - 1};
    r->streams[1] = r->streams[2] = (stream){.bytes = language};
    r->api = (mc_registry_resources){r, open_resource, close_resource, ready_resource};
    const mc_registry_options options = {MC_REGISTRY_CLIENT, f->profile, f->materials, 0, &r->api};
    DNC_CHECK(mc_registry_classes_allocate(&options, &f->registry[0], NULL) == MC_REGISTRY_OK &&
              mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile, &trig,
                                              &f->classes[0], NULL) == MC_WORLD_STORAGE_OK &&
              mc_world_runtime_classes_bind_registry(f->classes[0], f->registry[0], NULL) ==
                  MC_REGISTRY_OK &&
              mc_entity_classes_create(MC_ENTITY_CLIENT, f->profile, NULL, &f->entity_classes[0]) ==
                  MC_ENTITY_OK &&
              mc_world_runtime_classes_bind_entities(f->classes[0], f->entity_classes[0]) ==
                  MC_ENTITY_OK &&
              mc_registry_classes_initialize_blocks(f->registry[0], NULL) == MC_REGISTRY_OK &&
              mc_registry_classes_initialize_statistics(f->registry[0], NULL) == MC_REGISTRY_OK);
    /* SAME genuine Store/World constructor sequence as publication fixture,
     * stopping before its convenience GET. No active/phase/owner field repair. */
    DNC_CHECK(open_store(f, 0, root) &&
              mc_world_provider_create(0, NULL, &f->dimensions[0], NULL) == MC_WORLD_PROVIDER_OK);
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_provider_chunks = 1;
    limits.max_scheduled_entries = 0;
    DNC_CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_LOAD_OR_CREATE, f->classes[0],
                                              &limits, &f->worlds[0], NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_runtime_world_arguments args = {
        .store = f->stores[0], .name = f->name, .seed = -99, .dimension = f->dimensions[0]};
    DNC_CHECK(mc_world_runtime_construct_world(f->worlds[0], &args, NULL) == MC_WORLD_STORAGE_OK &&
              mc_chunk_provider_bind_fields(f->worlds[0]->fields.v, &f->providers[0], NULL) ==
                  MC_WORLD_STORAGE_OK);
    f->bound[0] = true;
    DNC_CHECK(seed_record(f, 0));
    DNC_CHECK(mc_player_session_classes_create(f->entity_classes[0], f->registry[0], &s->sessions,
                                               NULL) == MC_PLAYER_OK &&
              mc_player_string_create(title, COUNT(title), &s->username) == MC_PLAYER_OK &&
              mc_player_session_create(s->sessions, s->username, NULL, &s->session, NULL) ==
                  MC_PLAYER_OK &&
              mc_local_player_allocate(f->entity_classes[0], &s->player, NULL) == MC_ENTITY_OK &&
              mc_local_player_construct(s->player, f->worlds[0], NULL, s->session, 0, NULL, NULL) ==
                  MC_PLAYER_OK &&
              mc_entity_set_position(s->player, 8.5, 64, 8.5) == MC_ENTITY_OK);
    const mc_entity_class_literal *literal = NULL;
    mc_mob_error mob = {0};
    DNC_CHECK(mc_entity_class_literal_get(f->entity_classes[0], MC_ENTITY_CLASS_KIND_SKELETON,
                                          &literal) == MC_ENTITY_OK &&
              mc_mob_allocate(f->entity_classes[0], f->registry[0], literal, &s->skeleton, &mob) ==
                  MC_MOB_OK &&
              mc_mob_construct(s->skeleton, f->worlds[0], &mob) == MC_MOB_OK);
    /* The one real setup GET reserves, loads, publishes and activates the
     * actual empty record. Preloading a separate Store owner would exhaust
     * the fixture's max_loaded_chunks=1 and would be a different identity. */
    DNC_CHECK(mc_world_store_chunk_count(f->stores[0]) == 0 && f->providers[0].cache->count == 0 &&
              f->providers[0].cache->insertion_count == 0);
    mc_chunk_state *published = NULL;
    ++s->primes;
    DNC_CHECK(mc_world_runtime_get(f->worlds[0], 0, 0, &published) == MC_CHUNK_ACQUIRE_OK &&
              published != NULL && published->active && !published->blank &&
              mc_world_store_owns_chunk(f->stores[0], published) &&
              f->providers[0].cache->count == 1 && f->providers[0].cache->insertion_count == 1 &&
              f->providers[0].cache->insertions[0] == published);
    f->states[0] = published; /* Record the actual returned identity, never repair it. */
    f->chunks[0] = mc_world_store_chunk_object(f->stores[0], published);
    DNC_CHECK(f->chunks[0] != NULL && mc_world_store_chunk_count(f->stores[0]) == 1);
    bool present = false;
    DNC_CHECK(mc_world_runtime_chunk_provider_contains(f->worlds[0], 0, 0, &present, NULL) ==
                  MC_WORLD_STORAGE_OK &&
              present && f->states[0]->active);
    char *path = NULL;
    const bool saved =
        region_path(f, 0, &path) &&
        mc_ws_read_file(path, 1024u * 1024u, &f->saved[0], NULL) == MC_WORLD_STORAGE_OK;
    free(path);
    DNC_CHECK(saved);
    DNC_CHECK(mc_math_classes_create(SIZE_MAX, &s->math_owner, NULL) == MC_MATH_CLASS_OK);
    mc_math_execution_init(&s->math, s->math_owner);
    mc_entity_tick_execution_init(&s->execution, &s->math);
    mc_entity_effect_error_init(&s->effect);
    s->world_before = *f->worlds[0]->fields.r;
    s->callback_valid = true;
    if (s->sound_mode) {
        const mc_world_access_source_callbacks callbacks = {.sound = dnc_sound};
        DNC_CHECK(mc_world_access_source_create(&callbacks, s, &s->access, NULL) ==
                      MC_WORLD_STORAGE_OK &&
                  mc_world_runtime_add_world_access(f->worlds[0], s->access, NULL) ==
                      MC_WORLD_STORAGE_OK);
    }
    return true;
}

static bool dnc_prepare_sound_close(dnc_fixture *s, mc_entity_tick_execution_world_token own) {
    mc_entity_tick_execution_world_token current;
    DNC_CHECK(mc_entity_tick_execution_current_world(&s->execution, &current) &&
              dnc_token(current, own));
    const mc_world_access_failure *held = NULL;
    mc_world_access_failure_lease *node = &s->execution.leases[own.slot];
    if (s->capture_fault) {
        DNC_CHECK(mc_world_access_failure_lease_inspect(node, &held) &&
                  mc_entity_tick_execution_capture_receipt(&s->execution, held) ==
                      MC_WORLD_STORAGE_OK &&
                  s->execution.receipt_count == 1);
        ++s->capture_steps;
    } else {
        DNC_CHECK(mc_world_access_failure_lease_release(node, NULL) == MC_WORLD_STORAGE_OK);
        /* A real additional Native borrow, not an invented token or Source
         * call, proves an unrelated current scope cannot authorize this close. */
        mc_entity_tick_execution_world_token descendant;
        DNC_CHECK(mc_entity_tick_execution_push_world(&s->execution, s->game->worlds[0], dnc_reuse,
                                                      &descendant) == MC_WORLD_STORAGE_OK &&
                  mc_living_damage_sound_native_close(&s->sound) == MC_WORLD_STORAGE_BUSY &&
                  !s->sound.native_close_attempted &&
                  mc_entity_tick_execution_current_world(&s->execution, &current) &&
                  dnc_token(current, descendant) &&
                  mc_entity_tick_execution_pop_world(&s->execution, &descendant, false) ==
                      MC_WORLD_STORAGE_OK);
    }
    return true;
}

static bool dnc_reuse_control(dnc_fixture *s, mc_entity_tick_execution_world_token old) {
    mc_entity_tick_execution_world_token newer, current;
    DNC_CHECK(mc_entity_tick_execution_push_world(&s->execution, s->game->worlds[0], dnc_reuse,
                                                  &newer) == MC_WORLD_STORAGE_OK);
    DNC_CHECK(!dnc_token(newer, old) && newer.slot == old.slot);
    const size_t bindings = s->game->worlds[0]->persistent_bindings;
    DNC_CHECK((s->sound_mode ? mc_living_damage_sound_native_close(&s->sound)
                             : mc_player_ally_alert_native_close(s->ally)) == MC_WORLD_STORAGE_OK &&
              mc_entity_tick_execution_current_world(&s->execution, &current) &&
              dnc_token(current, newer) && s->game->worlds[0]->persistent_bindings == bindings &&
              mc_entity_tick_execution_pop_world(&s->execution, &newer, false) ==
                  MC_WORLD_STORAGE_OK &&
              !s->execution.world_open);
    return true;
}

static bool dnc_run(dnc_fixture *s) {
    mc_world_runtime *world = s->game->worlds[0];
    const size_t live = s->game->entity_classes[0]->live_entities;
    mc_java_random expected_random = *s->skeleton->random;
    if (s->sound_mode) {
        (void)mc_java_random_next_float(&expected_random);
        (void)mc_java_random_next_float(&expected_random);
        mc_living_damage_execution_init(&s->sound, &s->execution, s->game->classes[0], &s->owned);
        ++s->calls;
        s->continued = mc_living_damage_sound_in_execution(s->skeleton, s->game->registry[0], false,
                                                           &s->sound, &s->effect);
    } else {
        ++s->calls;
        s->continued = mc_player_ally_alert_begin_in_execution(
            &s->execution, s->game->classes[0], dnc_code, s->player,
            s->unreached ? s->player : s->skeleton, false, &s->ally, &s->raw, &s->owned);
    }
    /* Restore only the projected Native guard before any checked cleanup.
     * Every actor is independently retained; verify no final actor died. */
    if (s->projected) {
        world->persistent_bindings = s->saved_bindings;
        s->projected = false;
    }
    DNC_CHECK(s->game->entity_classes[0]->live_entities == live && s->calls == 1 &&
              world->active_calls == 0 && s->callback_valid &&
              s->callbacks == (s->normal ? 0u : 1u) && (s->normal || s->removed) &&
              s->continued == s->normal && dnc_random(world->fields.r, &s->world_before));
    s->actor_after = *s->skeleton->random;
    memcpy(s->living_after, mc_entity_living_fields(s->skeleton), sizeof(s->living_after));
    mc_entity_tick_execution_world_token own = {0};
    if (s->sound_mode) {
        own = s->sound.world;
        DNC_CHECK(s->sound.entered && s->sound.scope_created && s->sound.close_attempted &&
                  s->sound.close_locals_drained && s->sound.scope_open &&
                  !s->sound.close_consumed && !s->sound.completed &&
                  !s->sound.native_close_attempted && s->effect.status == MC_ENTITY_OK &&
                  !s->effect.living_damage.sound_returned && s->sound.close_failed &&
                  s->sound.initial_close_status == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
                  s->sound.world_returned && s->effect.world.bytecode == 53 &&
                  s->effect.world.world_access_bytecode == 53 &&
                  s->sound.world_status ==
                      (s->capture_fault ? MC_WORLD_STORAGE_IO_ERROR : MC_WORLD_STORAGE_OK) &&
                  s->effect.living_damage.private_calls == 2 &&
                  dnc_random(s->skeleton->random, &expected_random));
    } else {
        DNC_CHECK(mc_player_ally_alert_inspect(s->ally, &s->ally_state) &&
                  mc_player_ally_alert_execution_inspect(s->ally, &s->ally_view));
        own = s->ally_view.world;
        DNC_CHECK(s->raw == MC_ENTITY_OK && s->ally_view.completed && s->ally_state.returned);
        if (s->unreached) {
            DNC_CHECK(!s->ally_view.scope_created && !s->ally_view.close_attempted &&
                      !s->ally_view.close_locals_drained && !s->ally_view.query_returned &&
                      !s->execution.world_open && s->ally_state.wolves == NULL &&
                      s->ally_state.error->site.pc == 63 &&
                      mc_player_ally_alert_native_close(s->ally) ==
                          MC_WORLD_STORAGE_INVALID_ARGUMENT);
        } else {
            size_t count = SIZE_MAX;
            DNC_CHECK(s->ally_view.scope_created && s->ally_view.close_attempted &&
                      s->ally_view.close_locals_drained && !s->ally_view.close_failed &&
                      s->ally_view.initial_close_status == MC_WORLD_STORAGE_OK &&
                      s->ally_view.query_returned && !s->ally_view.scope_open &&
                      s->ally_view.close_consumed && !s->ally_view.native_close_attempted &&
                      s->ally_state.source_pc == 214 && s->ally_state.wolves != NULL &&
                      mc_world_class_entity_list_count(s->ally_state.wolves, &count) ==
                          MC_WORLD_STORAGE_OK &&
                      count == 0);
        }
    }
    if (s->unreached) {
        mc_math_class_state unused;
        DNC_CHECK(s->math.owned == NULL && !s->math.failed &&
                  mc_math_classes_state(s->math_owner, &unused, NULL) == MC_MATH_CLASS_OK &&
                  unused.phase == MC_MATH_CLASS_COLD && unused.holds == 1 &&
                  unused.source_a == NULL);
    }
    s->first_native = s->owned.ownership_error;
    if (!s->normal) {
        DNC_CHECK(s->owned.ownership_status == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
                  s->first_native.status == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
                  s->first_native.site.pc == 327 && s->first_native.site.class_name != NULL &&
                  s->first_native.site.method != NULL && s->first_native.site.descriptor != NULL &&
                  strcmp(s->first_native.site.class_name, "ls") == 0 &&
                  strcmp(s->first_native.site.method, "a") == 0 &&
                  strcmp(s->first_native.site.descriptor, "(Lsn;I)Z") == 0 &&
                  s->execution.world_slots[own.slot].generation == own.generation &&
                  s->execution.world_slots[own.slot].phase == MC_ENTITY_TICK_EXECUTION_WORLD_OPEN);
        if (s->capture_fault) {
            const mc_world_access_failure *held = NULL;
            DNC_CHECK(
                !s->execution.capture_pending && s->execution.first.status == MC_WORLD_STORAGE_OK &&
                s->execution.capture.status == MC_WORLD_STORAGE_OK &&
                s->execution.receipt_count == 0 && s->failure_node != NULL &&
                mc_world_access_failure_lease_inspect(s->failure_node, &held) &&
                dnc_cause(s->failure_node->callback_error, s->source_cause) &&
                held->receiver == s->access && held->receipt == s->receipt &&
                s->receipt->exception_class != NULL &&
                strcmp(s->receipt->exception_class, "java/lang/IllegalStateException") == 0 &&
                s->first_native.operation != NULL &&
                strcmp(s->first_native.operation, "World Entity sound owned receipt capture") == 0);
        } else
            DNC_CHECK(
                s->first_native.operation != NULL &&
                strcmp(s->first_native.operation, "World Entity sound checked current scope") == 0);
        DNC_CHECK(dnc_prepare_sound_close(s, own));
    }
    if (!s->unreached)
        DNC_CHECK((s->sound_mode
                       ? mc_living_damage_sound_native_close(&s->sound)
                       : mc_player_ally_alert_native_close(s->ally)) == MC_WORLD_STORAGE_OK);
    if (s->sound_mode) {
        DNC_CHECK(s->sound.close_consumed && !s->sound.scope_open && !s->sound.completed &&
                  s->sound.initial_close_status == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
                  s->sound.native_close_attempted &&
                  s->sound.native_close_status == MC_WORLD_STORAGE_OK);
    } else if (!s->unreached) {
        DNC_CHECK(mc_player_ally_alert_execution_inspect(s->ally, &s->ally_view) &&
                  s->ally_view.close_consumed && !s->ally_view.scope_open &&
                  s->ally_view.completed == s->normal);
    }
    if (!s->unreached)
        DNC_CHECK(s->execution.world_slots[own.slot].generation == own.generation &&
                  s->execution.world_slots[own.slot].phase ==
                      (s->sound_mode ? MC_ENTITY_TICK_EXECUTION_WORLD_RETAINED
                                     : MC_ENTITY_TICK_EXECUTION_WORLD_FREE));
    DNC_CHECK(!s->execution.world_open && dnc_error(s->owned.ownership_error, s->first_native));
    if (s->capture_fault) {
        const mc_world_access_failure *held = NULL;
        const char *exception = NULL;
        DNC_CHECK(mc_world_access_failure_lease_inspect(s->failure_node, &held) &&
                  mc_world_access_failure_source_exception(held, &exception) && exception != NULL &&
                  strcmp(exception, "java/lang/IllegalStateException") == 0 &&
                  dnc_cause(s->failure_node->callback_error, s->source_cause) &&
                  dnc_cause(s->effect.world.cause, s->source_cause) &&
                  s->effect.world.bytecode == 53 && s->effect.world.world_access_bytecode == 53 &&
                  s->effect.world.world_access_failure.receiver == s->access &&
                  s->effect.world.world_access_failure.receipt == s->receipt &&
                  mc_world_access_receipt_retire(&s->receipt, NULL) == MC_WORLD_STORAGE_BUSY);
    }
    if (!s->sound_mode && !s->unreached)
        DNC_CHECK(dnc_reuse_control(s, own));
    if (s->sound_mode) {
        const size_t bindings = world->persistent_bindings, slots = s->execution.world_count,
                     leases = s->execution.lease_count, receipts = s->execution.receipt_count;
        mc_entity_tick_execution_world_token refused = {0};
        DNC_CHECK(
            s->execution.phase == MC_ENTITY_TICK_EXECUTION_FAILED &&
            mc_entity_tick_execution_push_world(&s->execution, world, dnc_reuse, &refused) ==
                MC_WORLD_STORAGE_BUSY &&
            refused.owner == NULL && refused.slot == 0 && refused.generation == 0 &&
            world->persistent_bindings == bindings && s->execution.world_count == slots &&
            s->execution.lease_count == leases && s->execution.receipt_count == receipts &&
            s->execution.world_slots[own.slot].generation == own.generation &&
            s->execution.world_slots[own.slot].phase == MC_ENTITY_TICK_EXECUTION_WORLD_RETAINED &&
            mc_living_damage_sound_native_close(&s->sound) == MC_WORLD_STORAGE_OK &&
            world->persistent_bindings == bindings && s->execution.world_count == slots &&
            s->execution.lease_count == leases && s->execution.receipt_count == receipts &&
            s->execution.world_slots[own.slot].generation == own.generation &&
            s->execution.world_slots[own.slot].phase == MC_ENTITY_TICK_EXECUTION_WORLD_RETAINED &&
            !s->execution.world_open);
    }
    DNC_CHECK(s->callbacks == (s->normal ? 0u : 1u) && s->calls == 1 &&
              dnc_random(s->skeleton->random, &s->actor_after) &&
              memcmp(mc_entity_living_fields(s->skeleton), &s->living_after,
                     sizeof(s->living_after)) == 0 &&
              dnc_random(world->fields.r, &s->world_before) &&
              dnc_error(s->owned.ownership_error, s->first_native));
    if (s->sound_mode)
        DNC_CHECK(!s->sound.completed && s->sound.close_failed &&
                  s->sound.initial_close_status == MC_WORLD_STORAGE_UNSUPPORTED_STATE);
    else {
        DNC_CHECK(mc_player_ally_alert_execution_inspect(s->ally, &s->ally_view) &&
                  s->ally_view.completed && !s->ally_view.native_close_attempted &&
                  s->ally_state.error->status == MC_ENTITY_OK);
    }
    /* Explicit enclosing-caller order: actual child closes before retained
     * ally call, borrowed views expire before receipts, Math before context. */
    if (s->ally != NULL) {
        s->ally_state = (mc_player_ally_alert_state){0};
        s->ally_view = (mc_player_ally_alert_execution_view){0};
        DNC_CHECK(mc_player_ally_alert_retire(&s->ally, NULL) == MC_ENTITY_OK && s->ally == NULL);
    }
    mc_entity_effect_error_init(&s->effect);
    s->failure_node = NULL;
    DNC_CHECK(mc_entity_tick_execution_release_receipts(&s->execution) == MC_WORLD_STORAGE_OK &&
              mc_math_execution_retire(&s->math) == MC_MATH_CLASS_OK &&
              mc_entity_tick_execution_retire(&s->execution) == MC_WORLD_STORAGE_OK &&
              s->execution.world_count == 0 && s->execution.lease_count == 0 &&
              s->execution.receipt_count == 0 &&
              (!s->sound_mode || s->execution.world_slots[own.slot].generation == own.generation) &&
              s->execution.world_slots[own.slot].phase == MC_ENTITY_TICK_EXECUTION_WORLD_FREE &&
              mc_math_classes_release(&s->math_owner, NULL) == MC_MATH_CLASS_OK &&
              mc_world_access_receipt_retire(&s->receipt, NULL) == MC_WORLD_STORAGE_OK &&
              mc_world_access_free(s->access, NULL) == MC_WORLD_STORAGE_OK &&
              dnc_error(s->owned.ownership_error, s->first_native));
    s->access = NULL;
    return true;
}

static bool dnc_cleanup(dnc_fixture *s) {
    DNC_CHECK(files_unchanged(s->game, 0));
    mc_entity_release(s->player);
    s->player = NULL;
    mc_entity_release(s->skeleton);
    s->skeleton = NULL;
    mc_player_session_retire(&s->session);
    mc_player_string_release(s->username);
    s->username = NULL;
    DNC_CHECK(mc_player_session_classes_free(&s->sessions, NULL) == MC_PLAYER_OK);
    DNC_CHECK(cleanup_fixture(s->game));
    s->game = NULL;
    return true;
}

#ifndef MC_DAMAGE_CLOSE_MAIN
#define MC_DAMAGE_CLOSE_MAIN main
#endif
int MC_DAMAGE_CLOSE_MAIN(int argc, char **argv) {
    char root[8192];
    if (argc != 3 || !decode_path(argv[2], root, sizeof(root)))
        return 2;
    const bool sound = strcmp(argv[1], "sound-pop") == 0 || strcmp(argv[1], "sound-capture") == 0;
    const bool unreached = strcmp(argv[1], "ally-unreached") == 0;
    const bool normal = strcmp(argv[1], "ally-success") == 0 || unreached;
    const bool ally = normal;
    if (!sound && !ally)
        return 2;
    printf("DAMAGE_CLOSE_BEGIN schema=1 mode=%s\n", argv[1]);
    fflush(stdout);
    dnc_fixture *s = calloc(1, sizeof(*s));
    if (s == NULL)
        return 1;
    s->game = calloc(1, sizeof(*s->game));
    s->sound_mode = sound;
    s->normal = normal;
    s->unreached = unreached;
    s->capture_fault = strstr(argv[1], "capture") != NULL;
    if (s->game == NULL || !dnc_setup(s, root) || !dnc_run(s) || !dnc_cleanup(s)) {
        /* No Source retry, owner repair or guessed teardown after a red check.
         * The process/report keeps its first failed assertion as the primary cause. */
        fputs("DAMAGE_CLOSE_FAILED real-owner cleanup deferred; inspect first assertion\n", stderr);
        return 1;
    }
    printf("DAMAGE_CLOSE_END mode=%s entries=%zu primes=%zu callbacks=%zu captures=%zu\n", argv[1],
           s->calls, s->primes, s->callbacks, s->capture_steps);
    puts("DAMAGE_CLOSE_OPEN "
         "ally-pop-fault/ally-capture-fault/consumed-nonOK/full-damage741/Source-parity/FPS");
    printf("PASS: %u genuine direct damage-close checks; mode=%s\n", checks, argv[1]);
    free(s);
    return 0;
}
