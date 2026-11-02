/* Actual complete Player/App/statistics/World construction, shared with the
 * statistics contract. Its earlier executable entry point is never run. */
#define main mc_attack_unused_statistics_fixture_main
#include "../../client/statistics/test_player_dispatch.c"
#undef main

#include "entity/mob/internal.h"
#include "entity/mob/references_internal.h"
#include "inventory/internal.h"
#include "mc/entity/hit_result.h"
#include "mc/entity/player/attack.h"
#include "mc/world/runtime/world_access.h"
#include "world/runtime/runtime_internal.h"
#include "world/storage/generated_internal.h"
#include "world/storage/objects_internal.h"

#include <limits.h>
#include <xmmintrin.h>

typedef struct attack_fixture {
    fixture base;
    mc_math_classes *math;
    mc_entity *victim, *wolves[4];
    size_t wolf_count;
    mc_chunk_state *chunk;
    mc_world_access *access;
    mc_player_attack_call *call;
    size_t sounds, added, active_refusals;
    bool fail_added, install_stack;
    mc_item_stack *late_stack;
} attack_fixture;

static mc_world_storage_status attack_sound(void *context, mc_world_runtime *world,
                                            const mc_registry_string *name, double x, double y,
                                            double z, float volume, float pitch,
                                            mc_world_storage_error *error) {
    attack_fixture *f = context;
    (void)name;
    (void)x;
    (void)y;
    (void)z;
    (void)volume;
    (void)pitch;
    ++f->sounds;
    if (world == f->base.world && mc_local_player_attack_retire(&f->call, NULL) == MC_ENTITY_BUSY &&
        f->call != NULL)
        ++f->active_refusals;
    if (f->install_stack && mc_inventory_player_set(f->base.player->local_player->fields.player.c,
                                                    0, f->late_stack, NULL) != MC_INVENTORY_OK) {
        *error = (mc_world_storage_error){MC_WORLD_STORAGE_LIMIT, "authored late Stack failure", 0};
        return MC_WORLD_STORAGE_LIMIT;
    }
    *error = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status attack_added(void *context, mc_world_runtime *world, mc_entity *item,
                                            mc_world_storage_error *error) {
    attack_fixture *f = context;
    ++f->added;
    if (world == f->base.world && item != NULL &&
        mc_local_player_attack_retire(&f->call, NULL) == MC_ENTITY_BUSY && f->call != NULL)
        ++f->active_refusals;
    if (f->fail_added) {
        *error = (mc_world_storage_error){MC_WORLD_STORAGE_LIMIT,
                                          "authored failure after actual Item insertion", 0};
        return MC_WORLD_STORAGE_LIMIT;
    }
    *error = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

static mc_entity_watch_record *record(mc_entity *entity, int32_t key) {
    for (mc_entity_watch_node *n = entity->watcher->map->head; n != NULL; n = n->next)
        if (n->key == key)
            return n->record;
    return NULL;
}

static bool mob(attack_fixture *f, mc_entity_class_kind kind, double x, mc_entity **out) {
    const mc_entity_class_literal *literal = NULL;
    CHECK(mc_entity_class_literal_get(f->base.app.entities, kind, &literal) == MC_ENTITY_OK &&
          mc_mob_allocate(f->base.app.entities, f->base.app.registry, literal, out, NULL) ==
              MC_MOB_OK &&
          mc_mob_construct(*out, f->base.world, NULL) == MC_MOB_OK &&
          mc_entity_set_position(*out, x, 64.0, .5) == MC_ENTITY_OK);
    return true;
}

static bool prepare(attack_fixture *f, const char *resources, const char *directory, bool sheep) {
    memset(f, 0, sizeof(*f));
    CHECK(setup(&f->base, resources, directory, true));
    CHECK(mc_math_classes_create(SIZE_MAX, &f->math, NULL) == MC_MATH_CLASS_OK);
    uint8_t *blocks = calloc(MC_CHUNK_BLOCKS, 1);
    mc_world_chunk *chunk = NULL;
    CHECK(blocks != NULL &&
          mc_world_store_create_generated(f->base.store, f->base.world, 0, 0, 0, &blocks,
                                          MC_CHUNK_BLOCKS, &chunk, NULL) == MC_WORLD_STORAGE_OK &&
          blocks == NULL);
    f->chunk = mc_world_chunk_state(chunk);
    const mc_chunk_acquire_context *acquisition = mc_world_runtime_acquisition(f->base.world);
    CHECK(acquisition != NULL && f->chunk != NULL &&
          mc_chunk_cache_publish(acquisition->cache, 0, 0, f->chunk));
    CHECK(mc_entity_set_position(f->base.player, .5, 64.0, .5) == MC_ENTITY_OK &&
          mob(f, sheep ? MC_ENTITY_CLASS_KIND_SHEEP : MC_ENTITY_CLASS_KIND_COW, 1.5, &f->victim));
    f->base.player->base.velocity_y = 0.0;
    f->base.player->local_player->fields.player.g = 3;
    f->victim->base.velocity_x = f->victim->base.velocity_y = f->victim->base.velocity_z = 0.0;
    f->victim->base.yaw = 0.0f;
    f->victim->base.by = 0;
    f->victim->mob->living.Y = 10;
    f->victim->mob->living.Z = 10;
    f->victim->mob->living.E = 20;
    f->victim->mob->living.aa = f->victim->mob->living.ab = 0;
    f->victim->mob->living.au = f->victim->mob->living.av = 0;
    f->victim->mob->living.ah = false;
    bool pickable = false;
    float border = 0.0f;
    CHECK(mc_entity_pickable(f->victim, &pickable, NULL) == MC_ENTITY_HIT_OK && pickable &&
          mc_entity_pick_border_size(f->victim, &border, NULL) == MC_ENTITY_HIT_OK &&
          border == .1f);
    f->victim->base.be = true;
    CHECK(mc_entity_pickable(f->victim, &pickable, NULL) == MC_ENTITY_HIT_OK && !pickable);
    f->victim->base.be = false;
    if (sheep) {
        f->victim->mob->living.Y = 1;
        f->victim->mob->living.T = 7;
        CHECK(mc_mob_sheep_set_color(f->victim, 0, NULL) == MC_MOB_OK);
    }
    const mc_world_access_callbacks callbacks = {.sound = attack_sound,
                                                 .entity_added = attack_added};
    CHECK(mc_world_access_create(&callbacks, f, &f->access, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(f->base.world, f->access, NULL) == MC_WORLD_STORAGE_OK);
    return true;
}

static bool cleanup(attack_fixture *f) {
    CHECK(mc_local_player_attack_retire(&f->call, NULL) == MC_ENTITY_OK && f->call == NULL);
    bool removed = false;
    CHECK(mc_world_runtime_remove_world_access(f->base.world, f->access, &removed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          removed && mc_world_access_free(f->access, NULL) == MC_WORLD_STORAGE_OK);
    for (size_t i = 0; i < f->wolf_count; ++i) {
        CHECK(mc_mob_creature_target_store(f->wolves[i], NULL, NULL) == MC_ENTITY_OK);
        mc_entity_release(f->wolves[i]);
    }
    mc_entity_release(f->victim);
    f->victim = NULL;
    mc_item_stack_release(f->late_stack);
    const mc_chunk_acquire_context *acquisition = mc_world_runtime_acquisition(f->base.world);
    CHECK(acquisition != NULL &&
          mc_world_runtime_retire_entity_containers(f->base.world, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_chunk_cache_init(acquisition->cache, acquisition->cache->entries,
                              acquisition->cache->capacity, acquisition->cache->insertions,
                              acquisition->cache->insertion_capacity) &&
          mc_world_store_retire_chunk(f->base.store, f->chunk, NULL) == MC_WORLD_STORAGE_OK &&
          mc_math_classes_release(&f->math, NULL) == MC_MATH_CLASS_OK && teardown(&f->base));
    return true;
}

static bool stat(attack_fixture *f, size_t index, int32_t *out) {
    mc_registry_statistic_view view;
    CHECK(mc_registry_classes_inspect_statistics(f->base.app.registry, &view, NULL) ==
              MC_REGISTRY_OK &&
          mc_client_stats_writer_value(f->base.app.stats_writer, view.named[index], out, NULL) ==
              MC_CLIENT_STATS_OK);
    return true;
}

static bool wolf(attack_fixture *f, int8_t flags, bool same, bool busy) {
    CHECK(f->wolf_count < 4);
    mc_entity *value = NULL;
    CHECK(mob(f, MC_ENTITY_CLASS_KIND_WOLF, 2.5 + (double)f->wolf_count, &value));
    f->wolves[f->wolf_count++] = value;
    mc_entity_watch_record *byte = record(value, 16), *name = record(value, 17);
    CHECK(byte != NULL && name != NULL);
    byte->byte_value = flags;
    if (same)
        name->string_value = f->base.player->local_player->fields.player.l;
    if (busy)
        CHECK(mc_mob_creature_target_store(value, f->base.player, NULL) == MC_ENTITY_OK);
    bool inserted = false;
    CHECK(mc_world_runtime_insert_entity(f->base.world, value, &inserted, NULL) ==
              MC_WORLD_STORAGE_OK &&
          inserted);
    f->added = 0; /* Fixture publication precedes the observed attack. */
    f->active_refusals = 0;
    return true;
}

static bool observe(attack_fixture *f, const char *profile) {
    int32_t kills, damage;
    CHECK(stat(f, 20, &kills) && stat(f, 17, &damage));
    const mc_living_entity_fields *v = &f->victim->mob->living;
    const int flags = f->wolf_count == 0 ? -1 : record(f->wolves[0], 16)->byte_value;
    const int targeting = f->wolf_count == 0 ? -1 : f->wolves[0]->mob->creature_target == f->victim;
    printf("PLAYER_ATTACK profile=%s health=%d previous-health=%d hurt=%d hurt-time=%d "
           "max-hurt=%d last-damage=%d idle=%d dying=%d score=%d mob-kills=%d damage-dealt=%d "
           "drops=%zu wolf-flags=%d wolf-target=%d\n",
           profile, v->Y, v->Z, f->victim->base.by, v->aa, v->ab, v->au, v->av, v->ah,
           f->base.player->local_player->fields.player.g, kills, damage, f->added, flags,
           targeting);
    return true;
}

static bool actual_item_publication(attack_fixture *f, uintptr_t identity) {
    CHECK(f->base.world->entity_membership.count == 1 &&
          (uintptr_t)f->base.world->entity_membership.entries[0] == identity);
    mc_entity *item = f->base.world->entity_membership.entries[0];
    CHECK(mc_entity_is_kind(item, MC_ENTITY_CLASS_KIND_ITEM) && item->base.bF &&
          item->base.bG == 0 && item->base.bH == 4 && item->base.bI == 0);
    mc_world_chunk *chunk = mc_world_store_chunk_object(f->base.store, f->chunk);
    mc_world_chunk_fields fields;
    const mc_world_chunk_entity_list *slice = NULL;
    size_t count;
    mc_entity *stored = NULL;
    CHECK(chunk != NULL && mc_world_chunk_fields_get(chunk, &fields) == MC_WORLD_STORAGE_OK &&
          fields.d == f->base.world && fields.j == 0 && fields.k == 0 && fields.q &&
          mc_world_chunk_slice_at(fields.m, 4, &slice) == MC_WORLD_STORAGE_OK &&
          mc_world_chunk_entity_count(slice, &count) == MC_WORLD_STORAGE_OK && count == 1 &&
          mc_world_chunk_entity_at(slice, 0, &stored) == MC_WORLD_STORAGE_OK && stored == item);
    return true;
}

static bool reference_profiles(const char *resources) {
    static const char *const profiles[] = {"cow-normal", "cow-falling", "cow-resisted",
                                           "sheep-fatal", "cow-with-owned-wolf"};
    attack_fixture *f = calloc(1, sizeof(*f));
    CHECK(f != NULL);
    for (size_t i = 0; i < sizeof(profiles) / sizeof(*profiles); ++i) {
        CHECK(prepare(f, resources, profiles[i], i == 3));
        if (i == 1)
            f->base.player->base.velocity_y = -.1;
        if (i == 2) {
            f->victim->base.by = 20;
            f->victim->mob->living.au = 1;
        }
        if (i == 4)
            CHECK(wolf(f, 4, true, false));
        CHECK(mc_local_player_attack_begin(f->base.app.registry, f->math, f->base.player, f->victim,
                                           &f->call, NULL) == MC_ENTITY_OK);
        mc_player_attack_state state;
        CHECK(mc_local_player_attack_inspect(f->call, &state) && state.returned &&
              state.damage_returned && state.fresh_stack_read && state.statistic_returned &&
              state.damage == (i == 1 ? 2 : 1) && state.damage_result == (i != 2) &&
              f->victim->mob->living.Y == (i == 1   ? 8
                                           : i == 2 ? 10
                                           : i == 3 ? 0
                                                    : 9) &&
              state.alert_entered == (i != 3) && state.alert_returned == (i != 3) &&
              state.wolves_alerted == (i == 4 ? 1u : 0u) &&
              f->active_refusals == (i == 2   ? 0u
                                     : i == 3 ? 2u
                                              : 1u));
        CHECK(observe(f, profiles[i]) && cleanup(f));
    }
    free(f);
    return true;
}

static bool pickability_controls(attack_fixture *f) {
    static const mc_entity_class_kind kinds[] = {
        MC_ENTITY_CLASS_KIND_SPIDER,  MC_ENTITY_CLASS_KIND_ZOMBIE, MC_ENTITY_CLASS_KIND_SKELETON,
        MC_ENTITY_CLASS_KIND_CREEPER, MC_ENTITY_CLASS_KIND_SLIME,  MC_ENTITY_CLASS_KIND_PIG,
        MC_ENTITY_CLASS_KIND_SHEEP,   MC_ENTITY_CLASS_KIND_COW,    MC_ENTITY_CLASS_KIND_CHICKEN,
        MC_ENTITY_CLASS_KIND_WOLF,    MC_ENTITY_CLASS_KIND_SQUID};
    for (size_t i = 0; i < sizeof(kinds) / sizeof(*kinds); ++i) {
        const mc_entity_class_literal *literal = NULL;
        mc_entity *entity = NULL;
        CHECK(mc_entity_class_literal_get(f->base.app.entities, kinds[i], &literal) ==
                  MC_ENTITY_OK &&
              mc_mob_allocate(f->base.app.entities, f->base.app.registry, literal, &entity, NULL) ==
                  MC_MOB_OK &&
              entity->phase == MC_ENTITY_RAW && entity->holds == 1 && entity->random == NULL &&
              entity->mob->living.Y == 0);
        bool pickable = false;
        float border = 0.0f;
        const mc_entity_base_fields before = entity->base;
        /* The real LS body reads only be, even with zero health or RAW source
         * fields. Neither getter adds a READY, World or health predicate. */
        CHECK(mc_entity_pickable(entity, &pickable, NULL) == MC_ENTITY_HIT_OK && pickable &&
              mc_entity_pick_border_size(entity, &border, NULL) == MC_ENTITY_HIT_OK &&
              border == .1f && entity->holds == 1 && entity->phase == MC_ENTITY_RAW &&
              entity->random == NULL && memcmp(&before, &entity->base, sizeof(before)) == 0);
        entity->base.be = true;
        CHECK(mc_entity_pickable(entity, &pickable, NULL) == MC_ENTITY_HIT_OK && !pickable &&
              mc_entity_pick_border_size(entity, &border, NULL) == MC_ENTITY_HIT_OK &&
              border == .1f && entity->holds == 1 && entity->random == NULL);
        mc_entity_release(entity);
    }
    return true;
}

static bool controls(const char *resources) {
    attack_fixture *f = calloc(1, sizeof(*f));
    CHECK(f != NULL && prepare(f, resources, "controls", false) && pickability_controls(f));
    mc_player_attack_state state;
    CHECK(mc_local_player_attack_begin(NULL, NULL, NULL, NULL, &f->call, NULL) ==
              MC_ENTITY_STATE_ERROR &&
          mc_local_player_attack_inspect(f->call, &state) && state.error->source_exception &&
          state.error->site.pc == 1 &&
          mc_local_player_attack_retire(&f->call, NULL) == MC_ENTITY_OK);
    mc_inventory_player *inventory = f->base.player->local_player->fields.player.c;
    for (unsigned int mode = 0; mode < 4; ++mode) {
        const int32_t selected = inventory->selected;
        mc_item_stack **main = inventory->main_slots, **armor = inventory->armor_slots;
        if (mode == 0)
            inventory->selected = -1;
        if (mode == 1)
            inventory->main_slots = NULL;
        if (mode >= 2) {
            inventory->selected = 36;
            if (mode == 2)
                inventory->armor_slots = NULL;
        }
        const mc_entity_status status = mc_local_player_attack_begin(
            f->base.app.registry, f->math, f->base.player, NULL, &f->call, NULL);
        inventory->selected = selected;
        inventory->main_slots = main;
        inventory->armor_slots = armor;
        CHECK(status == MC_ENTITY_STATE_ERROR && mc_local_player_attack_inspect(f->call, &state) &&
              state.error->source_exception && !state.damage_returned &&
              state.error->site.pc == (mode == 3 ? 28u : 5u) && f->victim->mob->living.Y == 10 &&
              mc_local_player_attack_retire(&f->call, NULL) == MC_ENTITY_OK);
    }
    mc_item_stack *held = NULL;
    CHECK(mc_item_stack_allocate(&held) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(held, 3, 1, 0) == MC_ITEM_STACK_OK &&
          mc_inventory_player_set(inventory, 0, held, NULL) == MC_INVENTORY_OK);
    CHECK(mc_local_player_attack_begin(f->base.app.registry, f->math, f->base.player, f->victim,
                                       &f->call, NULL) == MC_ENTITY_STATE_ERROR &&
          mc_local_player_attack_inspect(f->call, &state) && !state.damage_returned &&
          !state.fresh_stack_read && state.first_stack_identity == (uintptr_t)held &&
          state.error->site.pc == 5 && strcmp(state.error->child.class_name, "iz") == 0 &&
          strcmp(state.error->child.descriptor, "(Lsn;)I") == 0 && state.error->child.pc == 9 &&
          f->victim->mob->living.Y == 10 && f->sounds == 0 && f->added == 0 &&
          mc_local_player_attack_retire(&f->call, NULL) == MC_ENTITY_OK &&
          mc_inventory_player_set(inventory, 0, NULL, NULL) == MC_INVENTORY_OK);
    int32_t held_damage;
    CHECK(stat(f, 17, &held_damage) && held_damage == 0);
    mc_item_stack_release(held);
    CHECK(wolf(f, 4, true, false) && wolf(f, 5, true, false) && wolf(f, 4, false, false) &&
          wolf(f, 4, true, true));
    CHECK(mc_local_player_attack_begin(f->base.app.registry, f->math, f->base.player, f->victim,
                                       &f->call, NULL) == MC_ENTITY_OK &&
          mc_local_player_attack_inspect(f->call, &state) && state.wolves_examined == 4 &&
          state.wolves_alerted == 1 && f->wolves[0]->mob->creature_target == f->victim &&
          f->wolves[1]->mob->creature_target == NULL &&
          f->wolves[2]->mob->creature_target == NULL &&
          f->wolves[3]->mob->creature_target == f->base.player &&
          mc_local_player_attack_retire(&f->call, NULL) == MC_ENTITY_OK);
    CHECK(cleanup(f));

    CHECK(prepare(f, resources, "fresh-stack", false));
    CHECK(mc_item_stack_allocate(&f->late_stack) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(f->late_stack, 3, 1, 0) == MC_ITEM_STACK_OK);
    f->install_stack = true;
    CHECK(mc_local_player_attack_begin(f->base.app.registry, f->math, f->base.player, f->victim,
                                       &f->call, NULL) == MC_ENTITY_STATE_ERROR &&
          mc_local_player_attack_inspect(f->call, &state) && state.damage_returned &&
          state.first_stack_identity == 0 &&
          state.fresh_stack_identity == (uintptr_t)f->late_stack && state.error->site.pc == 54 &&
          f->victim->mob->living.Y == 9);
    int32_t value;
    CHECK(stat(f, 17, &value) && value == 0 && cleanup(f));

    CHECK(prepare(f, resources, "fatal-publication", true));
    f->fail_added = true;
    const mc_entity_status publication_status = mc_local_player_attack_begin(
        f->base.app.registry, f->math, f->base.player, f->victim, &f->call, NULL);
    CHECK(mc_local_player_attack_inspect(f->call, &state));
    if (publication_status != MC_ENTITY_LIMIT || !state.error->effect.death.pending ||
        state.error->effect.death.inserted[0] || f->added != 1)
        fprintf(stderr,
                "[attack-publication] status=%d returned=%d pending=%d inserted=%d health=%d "
                "dying=%d added=%zu membership=%zu score=%d op=%s\n",
                (int)publication_status, state.returned, state.error->effect.death.pending,
                state.error->effect.death.inserted[0], f->victim->mob->living.Y,
                f->victim->mob->living.ah, f->added, f->base.world->entity_membership.count,
                f->base.player->local_player->fields.player.g,
                state.error->operation == NULL ? "none" : state.error->operation);
    CHECK(publication_status == MC_ENTITY_LIMIT &&
          mc_local_player_attack_inspect(f->call, &state) && !state.returned &&
          state.error->effect.death.pending && !state.error->effect.death.inserted[0] &&
          f->victim->mob->living.Y == 0 && f->victim->mob->living.ah && f->added == 1 &&
          actual_item_publication(f, state.error->effect.death.items[0]) &&
          f->base.player->local_player->fields.player.g == 10 && stat(f, 20, &value) &&
          value == 1 && stat(f, 17, &value) && value == 0);
    mc_player_attack_call *rejected = NULL;
    CHECK(mc_local_player_attack_begin(f->base.app.registry, f->math, f->base.player, f->victim,
                                       &rejected, NULL) == MC_ENTITY_BUSY &&
          rejected != NULL && mc_local_player_attack_retire(&rejected, NULL) == MC_ENTITY_OK &&
          rejected == NULL && mc_entity_effect_has_pending(f->victim) &&
          mc_local_player_attack_inspect(f->call, &state) && state.error->effect.death.pending &&
          f->base.world->entity_membership.count == 1);
    CHECK(mc_local_player_attack_retire(&f->call, NULL) == MC_ENTITY_OK &&
          !mc_entity_effect_has_pending(f->victim) && f->base.world->entity_membership.count == 1 &&
          cleanup(f));

    CHECK(prepare(f, resources, "fatal-statistic", true));
    mc_client_stats_writer *writer = f->base.app.stats_writer;
    f->base.app.stats_writer = NULL;
    const mc_entity_status failed = mc_local_player_attack_begin(
        f->base.app.registry, f->math, f->base.player, f->victim, &f->call, NULL);
    f->base.app.stats_writer = writer;
    CHECK(failed == MC_ENTITY_STATE_ERROR && mc_local_player_attack_inspect(f->call, &state) &&
          state.error->source_exception && state.error->effect.death.pending &&
          state.error->effect.death.score_callback_entered &&
          !state.error->effect.death.score_callback_returned && f->added == 0 &&
          !f->victim->mob->living.ah && f->victim->mob->living.Y == 0 &&
          f->base.player->local_player->fields.player.g == 10 && stat(f, 20, &value) &&
          value == 0 && cleanup(f));
    free(f);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3 || CD(argv[2]) != 0 || !mc_math_trig_init(&trig))
        return 2;
    if (!reference_profiles(argv[1]) || !controls(argv[1]))
        return 1;
    printf("PASS: %zu Player attack source-order/ownership checks\n", checks);
    return 0;
}
