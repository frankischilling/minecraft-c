/* Saved-World bootstrap, actual picking and ordinary SDK left-click attack. */
int mc_attack_included_mining_main(int, char **);
#define MC_MINING_FIXTURE_MAIN mc_attack_included_mining_main
#include "test_mining.c"
#undef MC_MINING_FIXTURE_MAIN
#include "entity/mob/internal.h"
#include "entity/player/player_internal.h"
#include "mc/client/world/entity_renderer.h"
#include "mc/world/runtime/world_access.h"
#include "test_attack_autosave.inc"

typedef struct attack_profile {
    const char *name;
    int32_t id, count, damage, power;
    bool breaks;
} attack_profile;

static const attack_profile attack_profiles[] = {{"unarmed", 0, 0, 0, 1, false},
                                                 {"held-dirt", 3, 7, 0, 1, false},
                                                 {"iron-sword-break", 267, 1, 250, 8, true}};
static const attack_profile *attack_selected_profile = &attack_profiles[0];
static mc_item_stack *attack_initial_stack;
static bool attack_clear_observed, attack_clear_receipt;

static struct {
    mc_entity *entity;
    mc_item_stack *source_stack;
    size_t publications, pop_sounds, other_publications;
    bool removed, bad;
    const char *first_bad;
} attack_wool;

static void attack_wool_diagnostics(fixture *f, const char *stage) {
    mc_item_stack_fields source = {0};
    const bool same =
        attack_wool.entity != NULL && attack_wool.entity->stack == attack_wool.source_stack;
    const mc_item_stack_status status =
        same ? mc_item_stack_fields_get(attack_wool.source_stack, &source)
             : MC_ITEM_STACK_INVALID_ARGUMENT;
    fprintf(stderr,
            "[attack-wool-diagnostic] stage=%s profile=%s bad=%d first-bad=%s publications=%zu "
            "pops=%zu other-publications=%zu entity=%" PRIxPTR " captured-stack=%" PRIxPTR
            " current-stack=%" PRIxPTR " same-stack=%d stack-status=%d id=%d count=%d damage=%d "
            "dead=%d removed=%d\n",
            stage, attack_selected_profile->name, (int)attack_wool.bad,
            attack_wool.first_bad == NULL ? "none" : attack_wool.first_bad,
            attack_wool.publications, attack_wool.pop_sounds, attack_wool.other_publications,
            (uintptr_t)attack_wool.entity, (uintptr_t)attack_wool.source_stack,
            attack_wool.entity == NULL ? 0 : (uintptr_t)attack_wool.entity->stack, (int)same,
            (int)status, source.id, source.count, source.damage,
            attack_wool.entity == NULL ? -1 : (int)attack_wool.entity->base.be,
            (int)attack_wool.removed);
    if (f == NULL || f->app == NULL || f->app->h == NULL)
        return;
    mc_local_player_snapshot player;
    mc_inventory_player_state inventory = {0};
    const bool readable = mc_local_player_get_snapshot(f->app->h, &player) == MC_ENTITY_OK &&
                          mc_inventory_player_read_state(player.player.c, &inventory);
    fprintf(stderr,
            "[attack-wool-inventory] readable=%d selected=%d main=%" PRIxPTR " armor=%" PRIxPTR
            "\n",
            (int)readable, readable ? inventory.selected : -1,
            readable ? (uintptr_t)inventory.main_slots : 0,
            readable ? (uintptr_t)inventory.armor_slots : 0);
    for (size_t i = 0; readable && i < 40; ++i) {
        mc_item_stack *const *array = i < 36 ? inventory.main_slots : inventory.armor_slots;
        const size_t index = i < 36 ? i : i - 36;
        if (array == NULL || array[index] == NULL)
            continue;
        mc_item_stack_fields fields = {0};
        const mc_item_stack_status read = mc_item_stack_fields_get(array[index], &fields);
        fprintf(stderr,
                "[attack-wool-inventory] slot=%zu stack=%" PRIxPTR
                " status=%d id=%d count=%d damage=%d animation=%d\n",
                i < 36 ? i : i - 36 + 100, (uintptr_t)array[index], (int)read, fields.id,
                fields.count, fields.damage, fields.animation);
    }
    const mc_world_reference_list *entities =
        f->app->active_world == NULL ? NULL : f->app->active_world->fields.b;
    size_t items = 0, live = 0;
    const bool list_readable =
        entities != NULL && entities->count <= entities->capacity && entities->count <= 4096;
    for (size_t i = 0; list_readable && i < entities->count; ++i) {
        mc_entity *entity = entities->items[i];
        if (entity == NULL || !mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_ITEM))
            continue;
        ++items;
        live += !entity->base.be;
        if (items > 16)
            continue;
        mc_item_stack_fields fields = {0};
        const mc_item_stack_status read = mc_item_stack_fields_get(entity->stack, &fields);
        fprintf(stderr,
                "[attack-wool-world] index=%zu entity=%" PRIxPTR " tracked=%d stack=%" PRIxPTR
                " status=%d id=%d count=%d damage=%d dead=%d x=%.17g y=%.17g z=%.17g\n",
                i, (uintptr_t)entity, (int)(entity == attack_wool.entity), (uintptr_t)entity->stack,
                (int)read, fields.id, fields.count, fields.damage, (int)entity->base.be,
                entity->base.x, entity->base.y, entity->base.z);
    }
    fprintf(stderr, "[attack-wool-world] readable=%d items=%zu live=%zu\n", (int)list_readable,
            items, live);
}

static int attack_inventory_wool(fixture *f, bool after, size_t *wool) {
    mc_local_player_snapshot player;
    mc_inventory_player_state inventory;
    CHECK(mc_local_player_get_snapshot(f->app->h, &player) == MC_ENTITY_OK &&
          mc_inventory_player_read_state(player.player.c, &inventory) && inventory.selected == 0 &&
          inventory.main_slots != NULL && inventory.armor_slots != NULL);
    *wool = 0;
    for (size_t i = 0; i < 36; ++i) {
        if (i == 0 && attack_selected_profile->id != 0 &&
            !(after && attack_selected_profile->breaks))
            continue;
        if (inventory.main_slots[i] == NULL)
            continue;
        mc_item_stack_fields fields, source;
        const bool valid =
            after && i == (attack_selected_profile->id == 3 ? 1u : 0u) && !attack_wool.bad &&
            attack_wool.publications == 1 && attack_wool.pop_sounds == 1 &&
            attack_wool.source_stack != NULL &&
            mc_item_stack_fields_get(attack_wool.source_stack, &source) == MC_ITEM_STACK_OK &&
            source.id == 35 && source.count == 0 && source.damage == 0 &&
            inventory.main_slots[i] != attack_wool.source_stack &&
            mc_item_stack_fields_get(inventory.main_slots[i], &fields) == MC_ITEM_STACK_OK &&
            fields.id == 35 && fields.count == 1 && fields.damage == 0;
        if (!valid)
            attack_wool_diagnostics(f, "inventory-predicate");
        CHECK(valid);
        ++*wool;
    }
    for (size_t i = 0; i < 4; ++i)
        CHECK(inventory.armor_slots[i] == NULL);
    CHECK(*wool <= 1);
    return 0;
}

/* All attack profiles observe their declared Inventory and the one actual
 * Wool drop, with ordinary frames and the same autosave/focus observers. */
static int attack_frame(fixture *f, mining_work *work) {
    CHECK(save_title_focus(f) == 0 && repeat_focus_state(f, "attack-before-frame") == 0 &&
          frame(f) == 0 && repeat_focus_state(f, "attack-returned-frame") == 0 &&
          !f->state.failed && f->state.render_returned && f->state.clocks.frame_returned &&
          f->state.clocks.timer_ticks >= 0 && f->state.clocks.timer_ticks <= 10 &&
          f->state.clocks.tick_returns == f->state.clocks.timer_ticks &&
          f->state.world_frame != NULL && f->state.world_frame->returned &&
          f->state.world_frame->source_pc == 1013 &&
          f->state.world_frame->world == f->app->active_world && f->state.hud != NULL &&
          f->state.hud->returned && f->state.hud->source_pc == 2101);
    ++work->frames;
    return 0;
}

static int attack_choose(fixture *f, mining_work *work) {
    mc_client_source_rq *rq = NULL;
    CHECK(f->app->r != NULL && f->app->r->kind == MC_CLIENT_SCREEN_MAIN_MENU &&
          open_selection(f, f->app->r, &rq) == 0 && release_mouse(f, rq) == 0);
    int32_t index = -1;
    for (size_t i = 0; i < mc_client_save_list_size(rq->fields.n); ++i)
        if (filename_is(mc_client_save_list_at(rq->fields.n, i), "alpha")) {
            CHECK(index == -1);
            index = (int32_t)i;
        }
    CHECK(index >= 0);
    const mc_client_world_list_fields *list = &rq->fields.o->fields;
    const int32_t y = list->h + 4 - mc_java_float_to_i32(list->m) + index * 36 + 8;
    CHECK(push_mouse(f, &rq->base, rq->base.fields.c / 2, y, true) == 0 && transfer(f) == 0);
    const uint64_t selected_deadline = SDL_GetTicks() + 5000;
    while (rq->fields.m != index && SDL_GetTicks() < selected_deadline)
        CHECK(frame(f) == 0);
    CHECK(rq->fields.m == index && release_mouse(f, rq) == 0 &&
          push_button(f, &rq->base, 1, true) == 0 && push_button(f, &rq->base, 1, false) == 0 &&
          transfer(f) == 0);
    const uint64_t deadline = SDL_GetTicks() + 180000;
    for (size_t frames = 0; frames < 180 && SDL_GetTicks() < deadline; ++frames) {
        CHECK(save_title_focus(f) == 0 && frame(f) == 0);
        ++work->frames;
        if (f->app->active_world == NULL)
            continue;
        CHECK(immutable_profile_focus(f) == 0 && reload_capture_epoch(f, &work->epochs[0]) == 0);
        return 0;
    }
    return 1;
}

static int attack_selected_stack(fixture *f, bool after) {
    mc_local_player_snapshot player;
    mc_inventory_player_state inventory;
    const mc_item_stack *stack = NULL;
    CHECK(mc_local_player_get_snapshot(f->app->h, &player) == MC_ENTITY_OK &&
          mc_inventory_player_read_state(player.player.c, &inventory) && inventory.selected == 0 &&
          inventory.main_slots != NULL &&
          mc_inventory_player_current(player.player.c, &stack, NULL) == MC_INVENTORY_OK);
    if (attack_selected_profile->id == 0) {
        size_t wool;
        CHECK(attack_initial_stack == NULL && stack == inventory.main_slots[0] &&
              attack_inventory_wool(f, after, &wool) == 0 && (stack == NULL) == (wool == 0));
        return 0;
    }
    if (after && attack_selected_profile->breaks) {
        mc_item_stack_fields fields;
        size_t wool;
        CHECK(attack_clear_observed && attack_inventory_wool(f, after, &wool) == 0 &&
              stack == inventory.main_slots[0] && (stack == NULL) == (wool == 0) &&
              attack_initial_stack != NULL &&
              mc_item_stack_fields_get(attack_initial_stack, &fields) == MC_ITEM_STACK_OK &&
              fields.id == attack_selected_profile->id && fields.count == 0 && fields.damage == 0);
    } else {
        mc_item_stack_fields fields;
        CHECK(stack != NULL && inventory.main_slots[0] == stack &&
              mc_item_stack_fields_get(stack, &fields) == MC_ITEM_STACK_OK &&
              fields.id == attack_selected_profile->id &&
              fields.count == attack_selected_profile->count &&
              fields.damage == attack_selected_profile->damage);
        if (attack_initial_stack == NULL) {
            CHECK(!after && mc_item_stack_retain((mc_item_stack *)stack) == MC_ITEM_STACK_OK);
            attack_initial_stack = (mc_item_stack *)stack;
        }
        CHECK(stack == attack_initial_stack);
    }
    size_t wool;
    CHECK(attack_inventory_wool(f, after, &wool) == 0);
    return 0;
}

static int attack_item_statistics(fixture *f, bool after) {
    if (attack_selected_profile->id == 0)
        return 0;
    mc_registry_statistic_view statistics;
    CHECK(mc_registry_classes_inspect_statistics(f->app->registry, &statistics, NULL) ==
              MC_REGISTRY_OK &&
          statistics.E != NULL && statistics.F != NULL);
    const int32_t expected = after && attack_selected_profile->breaks ? 1 : 0;
    int32_t used = -1, broken = 0;
    const mc_statistic *use = statistics.E[attack_selected_profile->id];
    const mc_statistic *breaking = statistics.F[attack_selected_profile->id];
    CHECK(use != NULL &&
          mc_client_stats_writer_value(f->app->stats_writer, use, &used, NULL) ==
              MC_CLIENT_STATS_OK &&
          used == expected);
    if (breaking != NULL)
        CHECK(mc_client_stats_writer_value(f->app->stats_writer, breaking, &broken, NULL) ==
                  MC_CLIENT_STATS_OK &&
              broken == expected);
    else
        CHECK(!attack_selected_profile->breaks && expected == 0);
    if (after)
        printf("ATTACK_ITEM profile=%s id=%d used=%d broken=%d broken-entry=%d selected-clear=%d "
               "final-count=%d final-damage=0 clear-receipt=%d\n",
               attack_selected_profile->name, attack_selected_profile->id, used, broken,
               (int)(breaking != NULL), (int)attack_selected_profile->breaks,
               attack_selected_profile->breaks ? 0 : attack_selected_profile->count,
               (int)attack_clear_receipt);
    return 0;
}

static int attack_hand_frames(fixture *f, mining_work *work, bool after) {
    size_t frames = 0;
    const mc_item_stack *previous_stack = NULL;
    size_t previous_wool = 0;
    bool previous = false;
    const uint64_t deadline = SDL_GetTicks() + 60000;
    for (size_t attempt = 0; attempt < 2048 && frames < 3 && SDL_GetTicks() < deadline; ++attempt) {
        CHECK(attack_frame(f, work) == 0 && attack_selected_stack(f, after) == 0);
        mc_local_player_snapshot player;
        const mc_item_stack *stack = NULL;
        size_t wool;
        mc_item_stack_fields fields = {0};
        CHECK(mc_local_player_get_snapshot(f->app->h, &player) == MC_ENTITY_OK &&
              mc_inventory_player_current(player.player.c, &stack, NULL) == MC_INVENTORY_OK &&
              attack_inventory_wool(f, after, &wool) == 0 &&
              (stack == NULL || mc_item_stack_fields_get(stack, &fields) == MC_ITEM_STACK_OK));
        const bool empty = stack == NULL;
        const mc_client_world_frame_state *world = f->state.world_frame;
        const mc_client_first_person_hand_state *hand = world == NULL ? NULL : world->hand;
        const mc_client_held_item_draw_state *held = hand == NULL ? NULL : hand->held_item;
        if (hand == NULL || !hand->returned || !hand->held_returned || held == NULL ||
            !held->returned || held->empty_hand != empty || !(held->equipped >= .99f))
            continue;
        /* The completed draw is retired before overlays, clearing its borrowed
         * Stack pointer. Inspect the retained source RA.b for actual identity. */
        mc_client_entity_renderer_state renderer;
        CHECK(hand->overlays_returned && held->stack_captured && held->source_pc == 1377 &&
              held->stack == NULL && world->owner == f->app->t && hand->owner == world->owner &&
              mc_client_entity_renderer_inspect(world->owner, &renderer) &&
              renderer.ra_occurrence != NULL && renderer.pending_ra == renderer.fields.c &&
              renderer.ra_app == f->app && renderer.ra_b == stack && f->state.hud != NULL &&
              f->state.hud->slots_returned &&
              f->state.hud->empty_slots == 9u -
                                               (attack_selected_profile->id != 0 &&
                                                        !(after && attack_selected_profile->breaks)
                                                    ? 1u
                                                    : 0u) -
                                               wool);
        if (!empty) {
            CHECK(held->item_color_returned && held->item_rotate_returned);
            if (fields.id == 3 || fields.id == 35)
                CHECK(held->block_item && held->block_inventory_returned && held->faces == 6 &&
                      held->vertices == 24);
            else
                CHECK(!held->block_item && !held->block_inventory_returned && held->flat_item &&
                      held->item_icon_returned && held->item_geometry_returned &&
                      held->item_icon == 66 && held->faces == 66 && held->vertices == 264);
        } else
            CHECK(held->arm_returned);
        if (previous && (previous_stack != stack || previous_wool != wool))
            frames = 0;
        previous = true;
        previous_stack = stack;
        previous_wool = wool;
        ++frames;
        if (frames == 3)
            printf("ATTACK_HAND stage=%s profile=%s frames=3 empty=%d item=%d count=%d damage=%d "
                   "block=%d flat=%d icon=%d geometry=%d faces=%u vertices=%u source=1377 "
                   "wool-slots=%zu\n",
                   after ? "after" : "before", attack_selected_profile->name, (int)empty, fields.id,
                   fields.count, fields.damage, (int)held->block_item, (int)held->flat_item,
                   held->item_icon, (int)held->item_geometry_returned, held->faces, held->vertices,
                   wool);
    }
    CHECK(frames == 3);
    return mining_capture(f, work, after ? "attack-hand-after.png" : "attack-hand-before.png");
}

static struct {
    fixture *fixture;
    mc_entity *victim;
    mc_world_runtime *world;
    mc_world_access *access;
    size_t samples;
    double velocity_y;
    int32_t damage;
    bool falling_bonus, actual_call;
    size_t death_particles;
    uintptr_t particles[20];
    bool removed, bad_particle;
} attack_probe;

static mc_world_storage_status attack_probe_sound(void *context, mc_world_runtime *world,
                                                  const mc_registry_string *name, double x,
                                                  double y, double z, float volume, float pitch,
                                                  mc_world_storage_error *error) {
    (void)context;
    (void)x;
    (void)y;
    (void)z;
    (void)pitch;
    mc_registry_string_view sound;
    static const uint16_t pop[] = {'r', 'a', 'n', 'd', 'o', 'm', '.', 'p', 'o', 'p'};
    /* FD's Entity-sound call forwards that captured Entity's x/y-offset/z.
     * Other ordinary World Item pickups are outside this authored drop. */
    const mc_entity *wool = attack_wool.entity;
    const bool wool_position = wool != NULL && x == wool->base.x && z == wool->base.z &&
                               y == wool->base.y - (double)wool->base.offset;
    if (world == attack_probe.world && wool_position && !wool->base.be &&
        mc_registry_string_read(name, &sound) && sound.length == 10 &&
        memcmp(sound.units, pop, sizeof(pop)) == 0) {
        mc_item_stack_fields source = {0};
        ++attack_wool.pop_sounds;
        const mc_item_stack_status read =
            mc_item_stack_fields_get(attack_wool.source_stack, &source);
        const char *reason = attack_wool.publications != 1      ? "pop-publications"
                             : attack_wool.pop_sounds != 1      ? "pop-count"
                             : volume != .2f                    ? "pop-volume"
                             : attack_wool.source_stack == NULL ? "pop-null-source-stack"
                             : read != MC_ITEM_STACK_OK         ? "pop-source-read"
                             : source.id != 35                  ? "pop-source-id"
                             : source.count != 0                ? "pop-source-count"
                             : source.damage != 0               ? "pop-source-metadata"
                                                                : NULL;
        if (reason != NULL) {
            attack_wool.bad = true;
            if (attack_wool.first_bad == NULL) {
                attack_wool.first_bad = reason;
                uint32_t volume_bits, pitch_bits;
                memcpy(&volume_bits, &volume, sizeof(volume_bits));
                memcpy(&pitch_bits, &pitch, sizeof(pitch_bits));
                fprintf(stderr,
                        "[attack-wool-pop] first-bad=%s volume=%08" PRIx32 " pitch=%08" PRIx32
                        " source-status=%d id=%d count=%d damage=%d\n",
                        reason, volume_bits, pitch_bits, (int)read, source.id, source.count,
                        source.damage);
                attack_wool_diagnostics(attack_probe.fixture, "pop-callback");
            }
        }
    }
    if (world == attack_probe.world && attack_probe.victim->mob->living.Y <= 0 &&
        !attack_probe.victim->mob->living.ah) {
        ++attack_probe.samples;
        attack_probe.velocity_y = attack_probe.fixture->app->h->base.velocity_y;
        const mc_client_gameplay_input *input = attack_probe.fixture->state.gameplay_input;
        mc_player_attack_state state;
        if (input != NULL && mc_local_player_attack_inspect(input->attack_call, &state) &&
            state.active && state.target == attack_probe.victim &&
            state.player == attack_probe.fixture->app->h) {
            attack_probe.actual_call = true;
            attack_probe.damage = state.damage;
            attack_probe.falling_bonus = state.falling_bonus;
        }
    }
    *error = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status attack_probe_added(void *context, mc_world_runtime *world,
                                                  mc_entity *entity,
                                                  mc_world_storage_error *error) {
    (void)context;
    if (world == attack_probe.world && entity != NULL &&
        mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_ITEM)) {
        const mc_client_gameplay_input *input = attack_probe.fixture->state.gameplay_input;
        mc_player_attack_state producer;
        if (input == NULL || !mc_local_player_attack_inspect(input->attack_call, &producer) ||
            !producer.active || producer.target != attack_probe.victim ||
            producer.player != attack_probe.fixture->app->h ||
            attack_probe.victim->mob->living.Y > 0) {
            ++attack_wool.other_publications;
            *error = (mc_world_storage_error){0};
            return MC_WORLD_STORAGE_OK;
        }
        mc_item_stack_fields source = {0};
        ++attack_wool.publications;
        const mc_item_stack_status read = mc_item_stack_fields_get(entity->stack, &source);
        const char *reason = attack_wool.publications != 1 ? "publication-count"
                             : entity->stack == NULL       ? "publication-null-stack"
                             : read != MC_ITEM_STACK_OK    ? "publication-source-read"
                             : source.id != 35             ? "publication-source-id"
                             : source.count != 1           ? "publication-source-count"
                             : source.damage != 0          ? "publication-source-metadata"
                                                           : NULL;
        const bool retain_attempted = reason == NULL;
        const mc_entity_status retained =
            retain_attempted ? mc_entity_retain(entity) : MC_ENTITY_OK;
        if (retained != MC_ENTITY_OK)
            reason = "publication-native-hold";
        if (reason != NULL) {
            attack_wool.bad = true;
            if (attack_wool.first_bad == NULL) {
                attack_wool.first_bad = reason;
                fprintf(stderr,
                        "[attack-wool-publication] first-bad=%s observed-entity=%" PRIxPTR
                        " observed-stack=%" PRIxPTR " read-status=%d id=%d count=%d damage=%d "
                        "retain-attempted=%d retain-status=%d\n",
                        reason, (uintptr_t)entity, (uintptr_t)entity->stack, (int)read, source.id,
                        source.count, source.damage, (int)retain_attempted, (int)retained);
                attack_wool_diagnostics(attack_probe.fixture, "publication-callback");
            }
        } else {
            attack_wool.entity = entity;
            attack_wool.source_stack = entity->stack;
        }
    }
    *error = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status attack_probe_removed(void *context, mc_world_runtime *world,
                                                    mc_entity *entity,
                                                    mc_world_storage_error *error) {
    (void)context;
    if (world == attack_probe.world && entity == attack_probe.victim)
        attack_probe.removed = true;
    if (world == attack_probe.world && entity == attack_wool.entity)
        attack_wool.removed = true;
    *error = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

/* This authored observer's other notifications are literal returns. The real
 * render, sound and particle listeners remain registered and run normally. */
static mc_world_storage_status attack_probe_cell(void *context, mc_world_runtime *world, int32_t x,
                                                 int32_t y, int32_t z,
                                                 mc_world_storage_error *error) {
    (void)context;
    (void)world;
    (void)x;
    (void)y;
    (void)z;
    *error = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status attack_probe_range(void *context, mc_world_runtime *world,
                                                  int32_t x0, int32_t y0, int32_t z0, int32_t x1,
                                                  int32_t y1, int32_t z1,
                                                  mc_world_storage_error *error) {
    (void)context;
    (void)world;
    (void)x0;
    (void)y0;
    (void)z0;
    (void)x1;
    (void)y1;
    (void)z1;
    *error = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status attack_probe_particle(void *context, mc_world_runtime *world,
                                                     const mc_registry_string *name, double x,
                                                     double y, double z, double vx, double vy,
                                                     double vz, mc_world_storage_error *error) {
    (void)context;
    (void)x;
    (void)y;
    (void)z;
    (void)vx;
    (void)vy;
    (void)vz;
    /* A World update runs one actual Mob body at a time. Observe this victim's
     * entered death loop, after the production RenderGlobal listener returns. */
    if (world == attack_probe.world && attack_probe.victim->mob->tick_active &&
        attack_probe.victim->mob->living.ad > 20 && attack_probe.victim->base.be) {
        mc_registry_string_view text;
        static const uint16_t expected[] = {'e', 'x', 'p', 'l', 'o', 'd', 'e'};
        mc_effect_renderer_snapshot effects;
        if (!mc_registry_string_read(name, &text) || text.length != 7 ||
            memcmp(text.units, expected, sizeof(expected)) != 0 ||
            !mc_client_effect_renderer_read(attack_probe.fixture->app->effect_renderer, &effects) ||
            effects.sizes[0] == 0 || attack_probe.death_particles == 20) {
            attack_probe.bad_particle = true;
        } else {
            mc_entity *particle = mc_client_effect_renderer_particle(
                attack_probe.fixture->app->effect_renderer, 0, effects.sizes[0] - 1);
            if (particle == NULL ||
                !mc_entity_is_kind(particle, MC_ENTITY_CLASS_KIND_EXPLOSION_PARTICLE) ||
                particle->phase != MC_ENTITY_READY || particle->base.world != world)
                attack_probe.bad_particle = true;
            const uintptr_t identity = (uintptr_t)particle;
            for (size_t i = 0; i < attack_probe.death_particles; ++i)
                if (attack_probe.particles[i] == identity)
                    attack_probe.bad_particle = true;
            attack_probe.particles[attack_probe.death_particles++] = identity;
        }
    }
    *error = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status attack_probe_tile(void *context, mc_world_runtime *world, int32_t x,
                                                 int32_t y, int32_t z, mc_tile_entity *tile,
                                                 mc_world_storage_error *error) {
    (void)context;
    (void)world;
    (void)x;
    (void)y;
    (void)z;
    (void)tile;
    *error = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status attack_probe_sky(void *context, mc_world_runtime *world,
                                                mc_world_storage_error *error) {
    (void)context;
    (void)world;
    *error = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status attack_probe_auxiliary(void *context, mc_world_runtime *world,
                                                      mc_entity *player, int32_t effect, int32_t x,
                                                      int32_t y, int32_t z, int32_t data,
                                                      mc_world_storage_error *error) {
    (void)context;
    (void)world;
    (void)player;
    (void)effect;
    (void)x;
    (void)y;
    (void)z;
    (void)data;
    *error = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

static int attack_probe_remove(void) {
    if (attack_probe.access == NULL)
        return 0;
    bool removed = false;
    CHECK(mc_world_runtime_remove_world_access(attack_probe.world, attack_probe.access, &removed,
                                               NULL) == MC_WORLD_STORAGE_OK &&
          removed && mc_world_access_free(attack_probe.access, NULL) == MC_WORLD_STORAGE_OK);
    attack_probe.access = NULL;
    return 0;
}

static int attack_value(fixture *f, size_t index, int32_t expected) {
    mc_registry_statistic_view statistics;
    int32_t value;
    CHECK(mc_registry_classes_inspect_statistics(f->app->registry, &statistics, NULL) ==
              MC_REGISTRY_OK &&
          statistics.named[index] != NULL &&
          mc_client_stats_writer_value(f->app->stats_writer, statistics.named[index], &value,
                                       NULL) == MC_CLIENT_STATS_OK &&
          value == expected);
    return 0;
}

static void attack_click_diagnostics(fixture *f, mc_entity *victim, size_t attempt,
                                     const mc_entity_hit_result_view *before,
                                     mc_entity_hit_status before_status, uint64_t swings,
                                     size_t events) {
    mc_entity_hit_result_view now = {0};
    const mc_entity_hit_status now_status = mc_entity_hit_result_inspect(f->app->y, &now);
    mc_registry_statistic_view statistics;
    int32_t dealt = -1, kills = -1;
    const bool stat_read = mc_registry_classes_inspect_statistics(f->app->registry, &statistics,
                                                                  NULL) == MC_REGISTRY_OK &&
                           statistics.named[17] != NULL && statistics.named[20] != NULL &&
                           mc_client_stats_writer_value(f->app->stats_writer, statistics.named[17],
                                                        &dealt, NULL) == MC_CLIENT_STATS_OK &&
                           mc_client_stats_writer_value(f->app->stats_writer, statistics.named[20],
                                                        &kills, NULL) == MC_CLIENT_STATS_OK;
    fprintf(stderr,
            "[attack-click-diagnostic] profile=%s attempt=%zu health=%d dying=%d dead=%d "
            "swing-before=%" PRIu64 " swing-now=%" PRIu64 " events-before=%zu events-now=%zu "
            "stats-readable=%d dealt=%d kills=%d ticks=%" PRIu64 " frame-ticks=%d click-timer=%d "
            "pre-hit-status=%d pre-hit-kind=%" PRIxPTR " pre-hit-entity=%" PRIxPTR
            " pre-hit-victim=%d pre-hit-block=%d,%d,%d,%d current-hit-status=%d "
            "current-hit-kind=%" PRIxPTR " current-hit-entity=%" PRIxPTR
            " current-hit-victim=%d current-hit-block=%d,%d,%d,%d\n",
            attack_selected_profile->name, attempt, victim->mob->living.Y,
            (int)victim->mob->living.ah, (int)victim->base.be, swings,
            f->state.gameplay_swing_calls, events, f->state.mouse_events, (int)stat_read, dealt,
            kills, f->state.ticks, f->state.clocks.timer_ticks, f->app->W, (int)before_status,
            (uintptr_t)before->fields.a, (uintptr_t)before->fields.g,
            (int)(before->fields.g == victim), before->fields.b, before->fields.c, before->fields.d,
            before->fields.e, (int)now_status, (uintptr_t)now.fields.a, (uintptr_t)now.fields.g,
            (int)(now.fields.g == victim), now.fields.b, now.fields.c, now.fields.d, now.fields.e);
    const mc_client_gameplay_input *input = f->state.gameplay_input;
    if (input != NULL)
        fprintf(stderr,
                "[attack-click-last-input] returned=%d source-pc=%u mouse-events=%zu stale=%zu "
                "left-clicks=%zu swings=%zu attacks=%zu hits=%zu attack-returned=%d "
                "attack-source=%u damage=%d damage-returned=%d damage-result=%d\n",
                (int)input->returned, input->source_pc, input->mouse_events,
                input->stale_mouse_events, input->left_clicks, input->swing_calls,
                input->attack_calls, input->attack_hits, (int)input->attack_state.returned,
                input->attack_state.source_pc, input->attack_state.damage,
                (int)input->attack_state.damage_returned, (int)input->attack_state.damage_result);
    fprintf(stderr,
            "[attack-click-position] player=%.17g,%.17g,%.17g yaw=%.9g source-aT=%.9g "
            "victim=%.17g,%.17g,%.17g\n",
            f->app->h->base.x, f->app->h->base.y, f->app->h->base.z, (double)f->app->h->base.yaw,
            (double)f->app->h->base.aT, victim->base.x, victim->base.y, victim->base.z);
}

static int attack_target(fixture *f, mining_work *work, mc_entity **out) {
    mc_entity_view_error *error = calloc(1, sizeof(*error));
    mc_entity_vector *eye = NULL, *look = NULL;
    mc_entity *player = f->app->h;
    CHECK(error != NULL &&
          mc_entity_living_position(player, 1.0f, &eye, error) == MC_ENTITY_VIEW_OK &&
          mc_entity_living_look_current(player, mc_client_bootstrap_math(f->bootstrap), &look,
                                        error) == MC_ENTITY_VIEW_OK &&
          eye != NULL && look != NULL);
    const double x = eye->x + look->x * 2.0, z = eye->z + look->z * 2.0;
    CHECK(mc_entity_vector_release(player->classes, look) == MC_ENTITY_OK &&
          mc_entity_vector_release(player->classes, eye) == MC_ENTITY_OK);
    free(error);
    const mc_entity_class_literal *kind = NULL;
    CHECK(mc_entity_class_literal_get(player->classes, MC_ENTITY_CLASS_KIND_SHEEP, &kind) ==
              MC_ENTITY_OK &&
          mc_mob_allocate(player->classes, f->app->registry, kind, out, NULL) == MC_MOB_OK &&
          mc_mob_construct(*out, f->app->active_world, NULL) == MC_MOB_OK &&
          mc_entity_set_position(*out, x, 64.0, z) == MC_ENTITY_OK);
    /* Explicit component input before real World publication. The Player pose,
     * HitResult, simulation clock and subsequent victim state are untouched. */
    (*out)->mob->living.Y = 1;
    (*out)->mob->living.T = 7;
    CHECK(mc_mob_sheep_set_color(*out, 0, NULL) == MC_MOB_OK);
    bool inserted = false;
    CHECK(mc_world_runtime_insert_entity(f->app->active_world, *out, &inserted, NULL) ==
              MC_WORLD_STORAGE_OK &&
          inserted);
    for (size_t i = 0; i < 64; ++i) {
        CHECK(attack_frame(f, work) == 0);
        mc_entity_hit_result_view hit;
        if (f->app->y != NULL &&
            mc_entity_hit_result_inspect(f->app->y, &hit) == MC_ENTITY_HIT_OK &&
            hit.fields.g == *out) {
            puts("ATTACK_PICK actual-source-ray=1 same-published-sheep=1 hitresult-untouched=1");
            return mining_capture(f, work, "attack-target.png");
        }
    }
    fprintf(stderr, "Actual source picking did not reach the published Sheep\n");
    return 1;
}

static int attack_wool_state(fixture *f, bool print) {
    mc_item_stack_fields source = {0};
    size_t inventory = 0;
    const bool valid =
        !attack_wool.bad && attack_wool.publications == 1 && attack_wool.pop_sounds <= 1 &&
        attack_wool.entity != NULL && attack_wool.entity->stack == attack_wool.source_stack &&
        mc_item_stack_fields_get(attack_wool.source_stack, &source) == MC_ITEM_STACK_OK &&
        source.id == 35 && source.damage == 0 &&
        source.count == (attack_wool.pop_sounds == 0 ? 1 : 0) &&
        attack_wool.entity->base.be == (attack_wool.pop_sounds == 1) &&
        (!attack_wool.removed || attack_wool.pop_sounds == 1) &&
        attack_selected_stack(f, true) == 0 && attack_inventory_wool(f, true, &inventory) == 0 &&
        inventory == attack_wool.pop_sounds;
    if (!valid)
        attack_wool_diagnostics(f, print ? "final-predicate" : "post-hand-predicate");
    CHECK(valid);
    size_t live = 0;
    const mc_world_reference_list *entities = f->app->active_world->fields.b;
    for (size_t i = 0; i < entities->count; ++i) {
        mc_entity *entity = entities->items[i];
        if (entity == attack_wool.entity && !entity->base.be) {
            mc_item_stack_fields fields;
            CHECK(mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_ITEM) &&
                  mc_item_stack_fields_get(entity->stack, &fields) == MC_ITEM_STACK_OK &&
                  fields.id == 35 && fields.count == 1 && fields.damage == 0);
            ++live;
        }
    }
    CHECK(live + inventory == 1);
    if (print)
        printf("ATTACK_WOOL published=1 id=35 count=1 damage=0 pop-sounds=%zu inventory-slot=%d "
               "inventory-count=%zu source-count=%d removed=%d\n",
               attack_wool.pop_sounds,
               inventory == 0                     ? -1
               : attack_selected_profile->id == 3 ? 1
                                                  : 0,
               inventory, source.count, (int)attack_wool.removed);
    return 0;
}

static int attack_run(fixture *f, mining_work *work, mc_entity **victim) {
    CHECK(mc_client_menu_loop_create(f->bootstrap, &f->loop) &&
          mc_client_menu_loop_inspect(f->loop, &f->state) &&
          repeat_statistics_controls(f, true) == 0 && save_title_focus(f) == 0 &&
          ticks(f, 2) == 0 && attack_choose(f, work) == 0 && attack_value(f, 17, 0) == 0 &&
          attack_value(f, 20, 0) == 0);
    attack_autosave.focused_frames = true;
    CHECK(repeat_focus_state(f, "attack-ready") == 0 && attack_selected_stack(f, false) == 0 &&
          attack_item_statistics(f, false) == 0 && attack_hand_frames(f, work, false) == 0 &&
          attack_target(f, work, victim) == 0);
    /* A declared read-only WorldAccess observes the actual death-sound call.
     * It samples the Player before the ordinary later World/Player tick. */
    attack_probe.fixture = f;
    attack_probe.victim = *victim;
    attack_probe.world = f->app->active_world;
    const mc_world_access_callbacks observer = {.cell = attack_probe_cell,
                                                .range = attack_probe_range,
                                                .sound = attack_probe_sound,
                                                .particle = attack_probe_particle,
                                                .entity_added = attack_probe_added,
                                                .tile_entity_dirty = attack_probe_tile,
                                                .entity_removed = attack_probe_removed,
                                                .sky_changed = attack_probe_sky,
                                                .auxiliary = attack_probe_auxiliary};
    CHECK(mc_world_access_create(&observer, NULL, &attack_probe.access, NULL) ==
              MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(attack_probe.world, attack_probe.access, NULL) ==
              MC_WORLD_STORAGE_OK);
    const uint64_t swings = f->state.gameplay_swing_calls;
    const size_t events = f->state.mouse_events;
    const int32_t score = f->app->h->local_player->fields.player.g;
    const uint64_t deadline = SDL_GetTicks() + 60000;
    bool killed = false;
    size_t attempts = 0;
    for (; attempts < 12 && SDL_GetTicks() < deadline && !killed; ++attempts) {
        mc_entity_hit_result_view before = {0};
        const mc_entity_hit_status before_status = mc_entity_hit_result_inspect(f->app->y, &before);
        CHECK(f->app->r == NULL && f->app->N && save_title_focus(f) == 0 &&
              mining_button(f, true) == 0);
        work->left_down = true;
        CHECK(mining_button(f, false) == 0);
        work->left_down = false;
        const uint64_t ticks_before = f->state.ticks;
        for (size_t frames = 0; frames < 64 && SDL_GetTicks() < deadline; ++frames) {
            CHECK(attack_frame(f, work) == 0);
            killed = (*victim)->mob->living.Y <= 0;
            if (killed || f->state.ticks > ticks_before)
                break;
        }
        if (!killed) {
            if ((*victim)->mob->living.Y != 1 || f->state.gameplay_swing_calls != swings)
                attack_click_diagnostics(f, *victim, attempts + 1, &before, before_status, swings,
                                         events);
            CHECK((*victim)->mob->living.Y == 1 && f->state.gameplay_swing_calls == swings &&
                  attack_value(f, 17, 0) == 0);
        }
    }
    const int32_t damage = attack_selected_profile->power + (attack_probe.velocity_y < 0.0 ? 1 : 0);
    uint64_t velocity_bits;
    memcpy(&velocity_bits, &attack_probe.velocity_y, sizeof(velocity_bits));
    printf("ATTACK_OPERAND y-velocity-bits=%016" PRIx64 " expected-damage=%d actual-damage=%d "
           "falling-bonus=%d sample-count=%zu active-call=%d\n",
           velocity_bits, damage, attack_probe.damage, (int)attack_probe.falling_bonus,
           attack_probe.samples, (int)attack_probe.actual_call);
    CHECK(attack_probe.samples == 1 && attack_probe.actual_call && attack_probe.damage == damage &&
          attack_probe.falling_bonus == (attack_probe.velocity_y < 0.0) && killed &&
          (*victim)->mob->living.Y == 1 - damage && (*victim)->mob->living.ah &&
          f->state.gameplay_swing_calls == swings + 1 && f->state.mouse_events > events &&
          f->app->h->local_player->fields.player.g == mc_java_i32_add(score, 7) &&
          attack_value(f, 17, damage) == 0 && attack_value(f, 20, 1) == 0);
    if (attack_selected_profile->id != 0) {
        CHECK(f->state.gameplay_input != NULL);
        const mc_player_attack_state *attack = &f->state.gameplay_input->attack_state;
        /* A later ordinary tick in the same frame can replace this receipt.
         * When retained, audit its actual GS.d/GS.H identities and flags. */
        if (attack->returned && attack->first_stack_identity == (uintptr_t)attack_initial_stack) {
            CHECK(!attack->active && attack->fresh_stack_read &&
                  attack->fresh_stack_identity == (uintptr_t)attack_initial_stack &&
                  attack->item_hit_returned &&
                  attack->item_hit_result == attack_selected_profile->breaks &&
                  attack->item_used_returned == attack_selected_profile->breaks &&
                  attack->exhausted_stack == attack_selected_profile->breaks &&
                  attack->clear_returned == attack_selected_profile->breaks);
            attack_clear_receipt = attack->clear_returned;
            if (attack_clear_receipt)
                CHECK(attack->destroy_returned && attack->cleared_slot == 0 &&
                      attack->clear_inventory_identity ==
                          (uintptr_t)f->app->h->local_player->fields.player.c);
        }
        if (attack_selected_profile->breaks) {
            const mc_item_stack *current;
            mc_item_stack_fields old;
            CHECK(mc_inventory_player_current(f->app->h->local_player->fields.player.c, &current,
                                              NULL) == MC_INVENTORY_OK &&
                  current != attack_initial_stack &&
                  mc_item_stack_fields_get(attack_initial_stack, &old) == MC_ITEM_STACK_OK &&
                  old.id == 267 && old.count == 0 && old.damage == 0);
            attack_clear_observed = true;
        }
    }
    CHECK(attack_selected_stack(f, true) == 0 && attack_item_statistics(f, true) == 0 &&
          attack_hand_frames(f, work, true) == 0);
    CHECK(attack_wool_state(f, false) == 0);
    /* Let the ordinary Timer reach the complete LS.U death loop and the next
     * actual World removal. No age, clock, Random or source field is repaired. */
    for (size_t frames = 0; frames < 2048 && SDL_GetTicks() < deadline &&
                            (attack_probe.death_particles < 20 || !attack_probe.removed);
         ++frames)
        CHECK(attack_frame(f, work) == 0);
    CHECK(!attack_probe.bad_particle && attack_probe.death_particles == 20 &&
          attack_probe.removed && (*victim)->base.be && (*victim)->mob->living.ad > 20);
    printf("ATTACK_DEATH age=%d particles=20 distinct-gy=20 actual-world-remove=1 normal-timer=1\n",
           (*victim)->mob->living.ad);
    const size_t autosaves_after_death = attack_autosave.returns;
    /* Reach the next actual periodic save after removal. Earlier saves remain
     * audited. Only ordinary Timer-driven frames advance the World. */
    for (size_t frames = 0; frames < 2048 && SDL_GetTicks() < deadline &&
                            attack_autosave.returns == autosaves_after_death;
         ++frames)
        CHECK(attack_frame(f, work) == 0);
    CHECK(!attack_autosave.failed && !attack_autosave.active &&
          attack_autosave.returns > autosaves_after_death &&
          attack_autosave.entries == attack_autosave.returns);
    printf("ATTACK_AUTOSAVE_POST_DEATH event=%zu world-remove=1 player-score=%d\n",
           attack_autosave.returns - 1, f->app->h->local_player->fields.player.g);
    for (size_t i = 0; i < 3; ++i)
        CHECK(attack_frame(f, work) == 0);
    CHECK(mining_capture(f, work, "attack-returned.png") == 0 && !attack_autosave.failed &&
          !attack_autosave.active && attack_autosave.entries == attack_autosave.returns);
    CHECK(attack_wool_state(f, true) == 0 && attack_probe_remove() == 0);
    printf("ATTACK_INPUT sdk-left=1 attempts=%zu mouse-progress=1 swing=1 health-nonpositive=1 "
           "dying=1 "
           "score-added=7 mob-kills=1 damage-dealt=%d wool-items=1 returned-world-hud=3\n",
           attempts, damage);
    return 0;
}

int main(int argc, char **argv) {
    CHECK(argc == 3 || argc == 4);
    if (argc == 4) {
        const attack_profile *selected = NULL;
        for (size_t i = 0; i < sizeof(attack_profiles) / sizeof(attack_profiles[0]); ++i)
            if (strcmp(argv[3], attack_profiles[i].name) == 0)
                selected = &attack_profiles[i];
        CHECK(selected != NULL);
        attack_selected_profile = selected;
    }
    fixture *f = calloc(1, sizeof(*f));
    mining_work *work = calloc(1, sizeof(*work));
    CHECK(f != NULL && work != NULL);
    mining_fixture = f;
    attack_autosave.fixture = f;
    attack_autosave.work = work;
    mining_extra_before_frame = attack_autosave_frame_before;
    mining_extra_after_frame = attack_autosave_frame_after;
    work->directory = argv[2];
    const mc_client_application_options options = {.assets = argv[1],
                                                   .username = "Selection control",
                                                   .width = 854,
                                                   .height = 480,
                                                   .hidden = false};
    int result = 1;
    uint32_t frame_id = 0, canvas_id = 0;
    mc_entity *victim = NULL;
    if (mc_client_bootstrap_open_standalone(&options, &f->bootstrap) == MC_CLIENT_BOOTSTRAP_OK) {
        f->app = mc_client_bootstrap_app(f->bootstrap);
        mc_client_frame_state host;
        if (f->app != NULL &&
            mc_client_source_frame_inspect(mc_client_bootstrap_frame(f->bootstrap), &host) &&
            host.frame_window != NULL && host.canvas_window != NULL) {
            frame_id = SDL_GetWindowID(host.frame_window);
            canvas_id = SDL_GetWindowID(host.canvas_window);
            result = attack_run(f, work, &victim);
        }
    }
    if (f->app != NULL && work->left_down)
        (void)mining_button(f, false);
    if (attack_probe.access != NULL && attack_probe_remove() != 0)
        result = 1;
    mc_item_stack_release(attack_initial_stack);
    attack_initial_stack = NULL;
    mc_entity_release(attack_wool.entity);
    attack_wool.entity = NULL;
    attack_wool.source_stack = NULL;
    mc_entity_release(victim);
    if (f->loop != NULL && !mc_client_menu_loop_terminal_retire(&f->loop))
        result = 1;
    mc_client_bootstrap_status closed = MC_CLIENT_BOOTSTRAP_BUSY;
    const uint64_t deadline = SDL_GetTicks() + 35000;
    do {
        closed = mc_client_bootstrap_close(&f->bootstrap);
        if (closed == MC_CLIENT_BOOTSTRAP_BUSY)
            SDL_Delay(1);
    } while (closed == MC_CLIENT_BOOTSTRAP_BUSY && SDL_GetTicks() < deadline);
    if (closed != MC_CLIENT_BOOTSTRAP_OK || f->bootstrap != NULL || f->loop != NULL ||
        frame_id == 0 || canvas_id == 0 || SDL_GetWindowFromID(frame_id) != NULL ||
        SDL_GetWindowFromID(canvas_id) != NULL)
        result = 1;
    if (result == 0)
        printf("ATTACK_AUTOSAVE_SUMMARY entries=%zu returns=%zu passive-observer=1 normal-timer=1\n"
               "ATTACK_FINISH cleanup=1 windows-absent=1 checks=%u\n",
               attack_autosave.entries, attack_autosave.returns, checks);
    free(work);
    free(f);
    return result;
}
