/* Actual saved-world bootstrap, SDK events and returning mining/play frames. */
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include "client/runtime/menu_loop.h"
static bool mining_observed_frame(mc_client_menu_loop *);
#define mc_client_menu_loop_frame mining_observed_frame
int mc_mining_world_fixture_main(int, char **);
#define main mc_mining_world_fixture_main
#include "../startup/test_world_selection_loop.c"
#undef main
#undef mc_client_menu_loop_frame
#include "client/particles/effect_renderer_internal.h"
#include "client/world/entity_renderer/selection_internal.h"
#include "entity/particles/particle_internal.h"
#include "mc/client/world/first_person_hand.h"
#include "mc/entity/nbt/errors.h"
#include "mc/inventory/player_current.h"

typedef struct mining_work {
    const char *directory;
    size_t frames, partial_frames, qm_frames, qm_ticks, item_frames, held_frames;
    int32_t maximum_ticks;
    bool left_down, forward_down;
    reload_epoch epochs[3];
} mining_work;

static fixture *mining_fixture;
static int (*mining_extra_before_frame)(fixture *);
static void (*mining_extra_after_frame)(fixture *, bool);

static struct {
    fixture *fixture;
    mining_work *work;
    size_t epoch, samples, items, all_items;
    bool armed, selected;
} mining_saving;

static int mining_path(mining_work *work, const char *name, char path[8192]) {
    const int n = snprintf(path, 8192, "%s/%s", work->directory, name);
    CHECK(n > 0 && n < 8192);
    return 0;
}

static int mining_capture(fixture *f, mining_work *work, const char *name) {
    char path[8192];
    CHECK(mining_path(work, name, path) == 0 && capture(f, path) == 0);
    return 0;
}

static int mining_button(fixture *f, bool down) {
    SDL_Event event = {0};
    event.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
    event.button.windowID = SDL_GetWindowID(mc_client_display_window(f->app->display));
    event.button.button = SDL_BUTTON_LEFT;
    event.button.down = down;
    CHECK(SDL_PushEvent(&event));
    return transfer(f);
}

static int mining_statistic(fixture *f, int32_t expected) {
    mc_registry_statistic_view statistics;
    int32_t value = 0;
    CHECK(mc_registry_classes_inspect_statistics(f->app->registry, &statistics, NULL) ==
              MC_REGISTRY_OK &&
          statistics.C != NULL && statistics.C[3] != NULL &&
          mc_client_stats_writer_value(f->app->stats_writer, statistics.C[3], &value, NULL) ==
              MC_CLIENT_STATS_OK &&
          value == expected);
    return 0;
}

static int mining_current(mc_entity *player, int32_t *count) {
    mc_local_player_snapshot local;
    const mc_item_stack *stack = NULL;
    CHECK(mc_local_player_get_snapshot(player, &local) == MC_ENTITY_OK &&
          mc_inventory_player_current(local.player.c, &stack, NULL) == MC_INVENTORY_OK);
    *count = 0;
    if (stack != NULL) {
        mc_item_stack_fields fields;
        CHECK(mc_item_stack_fields_get(stack, &fields) == MC_ITEM_STACK_OK && fields.id == 3 &&
              fields.damage == 0 && fields.count == 1);
        *count = fields.count;
    }
    return 0;
}

static int mining_target(mc_world_runtime *world, int32_t *id) {
    CHECK(mc_world_runtime_block_get(world, 0, 64, 2, id, NULL) == MC_WORLD_STORAGE_OK);
    return 0;
}

static int mining_items(mc_world_runtime *world, size_t *count, mc_entity **one) {
    const mc_world_reference_list *list = world->fields.b;
    CHECK(list != NULL && list->count <= list->capacity && list->count <= 4096);
    *count = 0;
    *one = NULL;
    for (size_t i = 0; i < list->count; ++i) {
        mc_entity *entity = list->items[i];
        if (entity == NULL || !mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_ITEM) ||
            entity->base.be)
            continue;
        mc_item_stack_fields stack;
        mc_item_stack_phase phase;
        CHECK(entity->stack != NULL &&
              mc_item_stack_phase_get(entity->stack, &phase) == MC_ITEM_STACK_OK &&
              phase == MC_ITEM_STACK_READY &&
              mc_item_stack_fields_get(entity->stack, &stack) == MC_ITEM_STACK_OK && stack.id > 0 &&
              stack.count > 0 && stack.damage >= 0 && entity->base.world == world);
        if (stack.id != 3)
            continue;
        CHECK(stack.count == 1 && stack.damage == 0);
        ++*count;
        *one = entity;
    }
    CHECK(*count <= 1);
    return 0;
}

static int mining_semantic(mc_entity *entity, mining_work *work, const char *name) {
    mc_entity_nbt_error *error = calloc(1, sizeof(*error));
    mc_nbt_tree *tree = NULL;
    CHECK(error != NULL && mc_entity_build_nbt(entity, NULL, &tree, error) == MC_ENTITY_NBT_OK);
    mc_nbt_limits limits = mc_nbt_default_limits();
    limits.max_output_bytes = 1048576;
    size_t size = 0, written = 0;
    mc_nbt_error wire_error;
    CHECK(mc_nbt_measure(mc_nbt_tree_root(tree), &limits, &size, &wire_error) == MC_NBT_OK &&
          size <= 1048576);
    uint8_t *wire = malloc(size);
    char path[8192];
    CHECK(wire != NULL &&
          mc_nbt_write(mc_nbt_tree_root(tree), &limits, wire, size, &written, &wire_error) ==
              MC_NBT_OK &&
          written == size && mining_path(work, name, path) == 0 && SDL_SaveFile(path, wire, size));
    free(wire);
    mc_nbt_tree_free(tree);
    free(error);
    return 0;
}

static int mining_save_entry(void) {
    fixture *f = mining_saving.fixture;
    mining_work *work = mining_saving.work;
    const size_t epoch = mining_saving.epoch;
    int32_t target, carried;
    size_t items;
    mc_entity *item = NULL;
    CHECK(epoch >= 1 && epoch <= 2 && f->app->active_world == work->epochs[epoch - 1].world &&
          f->app->h == work->epochs[epoch - 1].player &&
          mining_target(f->app->active_world, &target) == 0 && target == 0 &&
          mining_current(f->app->h, &carried) == 0 && carried == (epoch == 1 ? 0 : 1) &&
          mining_items(f->app->active_world, &items, &item) == 0 &&
          items == (epoch == 1 ? 1u : 0u) &&
          mining_semantic(f->app->h, work,
                          epoch == 1 ? "saved-player-1.nbt" : "saved-player-2.nbt") == 0);
    if (epoch == 1)
        CHECK(mining_semantic(item, work, "saved-item-1.nbt") == 0);
    size_t all_items = 0;
    const mc_world_reference_list *list = f->app->active_world->fields.b;
    for (size_t i = 0; i < list->count; ++i) {
        mc_entity *entity = list->items[i];
        if (entity == NULL || !mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_ITEM) ||
            entity->base.be)
            continue;
        char name[64];
        const int n = snprintf(name, sizeof(name), "saved-items-%zu-%04zu.nbt", epoch, all_items);
        CHECK(all_items < 128 && n > 0 && (size_t)n < sizeof(name) &&
              mining_semantic(entity, work, name) == 0);
        ++all_items;
    }
    char name[64], path[8192], count[32];
    const int named = snprintf(name, sizeof(name), "saved-items-%zu.count", epoch);
    const int counted = snprintf(count, sizeof(count), "%zu\n", all_items);
    CHECK(named > 0 && (size_t)named < sizeof(name) && counted > 0 &&
          (size_t)counted < sizeof(count) && mining_path(work, name, path) == 0 &&
          SDL_SaveFile(path, count, (size_t)counted));
    mining_saving.items = items;
    mining_saving.all_items = all_items;
    ++mining_saving.samples;
    return 0;
}

static void mining_mob_diagnostics(const fixture *f) {
    const mc_client_world_tick *tick = f->state.world_tick;
    if (tick == NULL || tick->entities.child != MC_WORLD_ENTITY_TICK_CHILD_MOB)
        return;
    const mc_world_entity_tick_error *world = &tick->entities;
    const mc_mob_tick_error *mob = &world->child_error.mob;
    creature_tick_diagnostics(f);
    fprintf(stderr,
            "[mining-mob-diagnostic] kind=%d source-pc=%u living-pc=%u creature-pc=%u "
            "monster-pc=%u concrete-pc=%u loaded-index=%" PRId32 "\n",
            (int)mob->state.kind, mob->state.source_pc, mob->state.living_pc,
            mob->state.creature_pc, mob->state.monster_pc, mob->state.concrete_pc,
            world->loaded_index);
    /* The retained failure owns its actual World graph. Identify the current
     * live List occurrence at the recorded index as a candidate, rather than
     * infer an actor pointer from the obfuscated virtual declaring class. */
    const mc_world_reference_list *list =
        f->app->active_world == NULL ? NULL : f->app->active_world->fields.b;
    if (list != NULL && list->count <= list->capacity && world->loaded_index >= 0 &&
        (size_t)world->loaded_index < list->count) {
        const mc_entity *actor = list->items[(size_t)world->loaded_index];
        if (actor != NULL && actor->class_literal != NULL)
            fprintf(stderr,
                    "[mining-mob-diagnostic] loaded-actor-candidate=%" PRIuPTR
                    " kind=%d matches-failed-kind=%d class=%s id=%" PRId32
                    " x=%.17g y=%.17g z=%.17g dead=%d phase=%d\n",
                    (uintptr_t)actor, (int)actor->class_literal->kind,
                    (int)(actor->class_literal->kind == mob->state.kind),
                    creature_diagnostic_text(actor->class_literal->client_name), actor->base.id,
                    actor->base.x, actor->base.y, actor->base.z, (int)actor->base.be,
                    (int)actor->phase);
    }
    if (mob->child == MC_MOB_TICK_CHILD_QUERY) {
        const mc_mob_tick_query_error *query = &mob->detail.query;
        fprintf(stderr,
                "[mining-mob-diagnostic] query status=%d operation=%s world-cause=%s "
                "source-pc=%u provider-pc=%u chunk-pc=%u source-operation=%s\n",
                (int)query->cause.status, creature_diagnostic_text(query->cause.operation),
                creature_diagnostic_text(query->source.cause.operation), query->source.bytecode,
                query->source.provider_bytecode, query->source.chunk_bytecode,
                creature_diagnostic_text(query->source.operation));
    } else if (mob->child == MC_MOB_TICK_CHILD_PATH) {
        const mc_mob_tick_path_error *path = &mob->detail.path;
        fprintf(stderr,
                "[mining-mob-diagnostic] path kind=%d status=%d site=%s.%s%s:%" PRId32
                " child=%s.%s%s:%" PRId32 " operation=%s world-cause=%s\n",
                (int)path->kind, (int)path->cause.status,
                creature_diagnostic_text(path->cause.site.class_name),
                creature_diagnostic_text(path->cause.site.method),
                creature_diagnostic_text(path->cause.site.descriptor), path->cause.site.pc,
                creature_diagnostic_text(path->cause.child.class_name),
                creature_diagnostic_text(path->cause.child.method),
                creature_diagnostic_text(path->cause.child.descriptor), path->cause.child.pc,
                creature_diagnostic_text(path->cause.operation),
                path->has_world ? creature_diagnostic_text(path->world.operation) : "unreached");
    } else if (mob->child == MC_MOB_TICK_CHILD_BRIGHTNESS) {
        const mc_world_brightness_error *brightness = &mob->detail.brightness;
        fprintf(stderr,
                "[mining-mob-diagnostic] brightness status=%d pc=%u operation=%s "
                "query-pc=%u query-operation=%s\n",
                (int)brightness->status, brightness->bytecode,
                creature_diagnostic_text(brightness->operation), brightness->query.bytecode,
                creature_diagnostic_text(brightness->query.operation));
    } else if (mob->child == MC_MOB_TICK_CHILD_ACTION &&
               mob->detail.action.child_kind == MC_PLAYER_ACTION_CHILD_TRAVEL) {
        const mc_player_travel_error *travel = &mob->detail.action.travel;
        fprintf(stderr,
                "[mining-mob-diagnostic] travel status=%d cause=%d site=%s.%s living-pc=%u "
                "operation=%s move-status=%d move-pc=%u move-operation=%s "
                "world-operation=%s contact-operation=%s\n",
                (int)travel->status, (int)travel->cause,
                creature_diagnostic_text(travel->source_class),
                creature_diagnostic_text(travel->source_method), travel->living_bytecode,
                creature_diagnostic_text(travel->operation), (int)travel->movement.status,
                travel->movement.bytecode, creature_diagnostic_text(travel->movement.operation),
                creature_diagnostic_text(travel->movement.world.operation),
                creature_diagnostic_text(travel->movement.contact.operation));
    } else if (mob->child == MC_MOB_TICK_CHILD_ENVIRONMENT) {
        const mc_entity_tick_error *environment = &mob->detail.environment;
        fprintf(stderr,
                "[mining-mob-diagnostic] environment-child operation=%s class=%s pc=%u "
                "world-operation=%s world-pc=%u effect-operation=%s\n",
                creature_diagnostic_text(environment->environment.operation),
                creature_diagnostic_text(environment->environment.source_class),
                environment->environment.bytecode,
                creature_diagnostic_text(environment->world.operation), environment->world.bytecode,
                creature_diagnostic_text(environment->effect.operation));
    } else if (mob->child == MC_MOB_TICK_CHILD_MATERIAL) {
        fprintf(stderr, "[mining-mob-diagnostic] material class=%s pc=%u operation=%s\n",
                creature_diagnostic_text(mob->detail.material.source_class),
                mob->detail.material.bytecode,
                creature_diagnostic_text(mob->detail.material.operation));
    } else if (mob->child == MC_MOB_TICK_CHILD_SOUND) {
        const mc_world_movement_error *sound = &mob->detail.sound;
        fprintf(stderr,
                "[mining-mob-diagnostic] sound status=%d pc=%u operation=%s "
                "world-access-pc=%u presentation-operation=%s\n",
                (int)sound->status, sound->bytecode, creature_diagnostic_text(sound->operation),
                sound->world_access_bytecode,
                creature_diagnostic_text(sound->presentation.operation));
    } else if (mob->child == MC_MOB_TICK_CHILD_EFFECT) {
        const mc_entity_effect_error *effect = &mob->detail.effect;
        fprintf(stderr,
                "[mining-mob-diagnostic] effect status=%d cause=%d class=%s pc=%u damage-pc=%u "
                "living-pc=%u operation=%s world-operation=%s\n",
                (int)effect->status, (int)effect->cause,
                creature_diagnostic_text(effect->source_class),
                (unsigned int)effect->source_bytecode, (unsigned int)effect->damage_bytecode,
                (unsigned int)effect->living_bytecode, creature_diagnostic_text(effect->operation),
                creature_diagnostic_text(effect->world.operation));
    } else if (mob->child == MC_MOB_TICK_CHILD_RAY) {
        const mc_world_raytrace_error *ray = &mob->detail.ray.cause;
        fprintf(stderr,
                "[mining-mob-diagnostic] ray status=%d site=%s.%s%s:%d child=%s.%s%s:%d "
                "operation=%s\n",
                (int)ray->status, creature_diagnostic_text(ray->site.class_name),
                creature_diagnostic_text(ray->site.method),
                creature_diagnostic_text(ray->site.descriptor), ray->site.pc,
                creature_diagnostic_text(ray->child.class_name),
                creature_diagnostic_text(ray->child.method),
                creature_diagnostic_text(ray->child.descriptor), ray->child.pc,
                creature_diagnostic_text(ray->operation));
    } else if (mob->child == MC_MOB_TICK_CHILD_CLASS_LIST) {
        fprintf(stderr, "[mining-mob-diagnostic] class-list operation=%s query-operation=%s\n",
                creature_diagnostic_text(mob->detail.class_list.query.cause.operation),
                creature_diagnostic_text(mob->detail.class_list.query.operation));
    } else if (mob->child == MC_MOB_TICK_CHILD_INVENTORY) {
        const mc_inventory_current_error *inventory = &mob->detail.inventory;
        fprintf(stderr, "[mining-mob-diagnostic] inventory site=%s.%s%s:%" PRId32 " operation=%s\n",
                creature_diagnostic_text(inventory->site.class_name),
                creature_diagnostic_text(inventory->site.method),
                creature_diagnostic_text(inventory->site.descriptor), inventory->site.pc,
                creature_diagnostic_text(inventory->operation));
    }
    (void)fflush(stderr);
}

/* The included ordinary save/title helper calls this only in the fixture.
 * Capture owned wire bytes before each real paused frame; the frame that
 * actually enters leave selects its preceding capture. No actor alias is
 * inspected after that source call releases or replaces its graph. */
static bool mining_observed_frame(mc_client_menu_loop *loop) {
    fixture *f = mining_fixture;
    const bool capture = mining_saving.armed && !mining_saving.selected && f != NULL &&
                         f->loop == loop && f->app->active_world != NULL && f->app->r != NULL &&
                         f->app->r->kind == MC_CLIENT_SCREEN_PAUSE && !f->app->N &&
                         atomic_load(&f->app->o);
    if (capture && mining_save_entry() != 0)
        return false;
    if (mining_extra_before_frame != NULL && mining_extra_before_frame(f) != 0)
        return false;
    const bool returned = mc_client_menu_loop_frame(loop);
    if (mining_extra_after_frame != NULL)
        mining_extra_after_frame(f, returned);
    if (!returned && f != NULL && f->loop == loop && mc_client_menu_loop_inspect(loop, &f->state))
        mining_mob_diagnostics(f);
    if (capture && returned && f->app->active_world == NULL && f->app->h == NULL &&
        f->app->r != NULL && f->app->r->kind == MC_CLIENT_SCREEN_MAIN_MENU)
        mining_saving.selected = true;
    return returned;
}

static bool mining_dirt_draw(const mc_client_render_global_draw_receipt *draw,
                             const mc_entity *dirt) {
    return dirt != NULL && draw->valid && draw->returned && draw->retired &&
           draw->actor_identity == (uintptr_t)dirt &&
           draw->actor_kind == MC_ENTITY_CLASS_KIND_ITEM && draw->primary_returned &&
           draw->post_draw_returned && draw->living_source_pc == 796 &&
           draw->renderer_class_name != NULL && strcmp(draw->renderer_class_name, "bb") == 0;
}

static int mining_observe(fixture *f, mining_work *work) {
    CHECK(!f->state.failed && f->state.render_returned && f->state.clocks.frame_returned &&
          f->state.clocks.timer_ticks >= 0 && f->state.clocks.timer_ticks <= 10 &&
          f->state.clocks.tick_returns == f->state.clocks.timer_ticks);
    if (f->state.clocks.timer_ticks > work->maximum_ticks)
        work->maximum_ticks = f->state.clocks.timer_ticks;
    if (f->app->active_world == NULL)
        return 0;
    const mc_client_world_frame_state *world = f->state.world_frame;
    CHECK(world != NULL && world->returned && world->source_pc == 1013 &&
          world->world == f->app->active_world && f->state.hud != NULL && f->state.hud->returned &&
          f->state.hud->source_pc == 2101);
    bool cracked = false;
    for (size_t i = 0; i < world->selection_calls; ++i) {
        const mc_client_selection_state *selection = world->selections[i];
        if (selection != NULL && selection->progress > 0.f && selection->progress < 1.f &&
            selection->crack_cv_returned && selection->crack_draw_returned &&
            selection->crack_state_restored && selection->crack_texture >= 240 &&
            selection->crack_texture <= 249)
            cracked = true;
    }
    if (cracked) {
        ++work->partial_frames;
        if (work->partial_frames == 1)
            CHECK(mining_capture(f, work, "partial.png") == 0);
    }
    mc_effect_renderer_snapshot effects;
    CHECK(mc_client_effect_renderer_read(f->app->effect_renderer, &effects));
    size_t qm = 0, ticked = 0;
    for (size_t i = 0; i < effects.sizes[1]; ++i) {
        mc_entity *particle = mc_client_effect_renderer_particle(f->app->effect_renderer, 1, i);
        mc_particle_snapshot state;
        CHECK(mc_entity_is_kind(particle, MC_ENTITY_CLASS_KIND_DIGGING_PARTICLE) &&
              mc_particle_read(particle, &state) && state.phase == MC_ENTITY_READY &&
              state.item_pc == 80 && state.digging_block != NULL && !state.tick_active);
        ++qm;
        if (state.particle.e > 0)
            ++ticked;
    }
    if (qm != 0 && world->sprite_particles != NULL && world->sprite_particles->returned &&
        world->sprite_particles->visited >= qm && world->sprite_particles->item_particle_returned &&
        world->sprite_particles->item_vertices_returned == 4)
        ++work->qm_frames;
    if (ticked != 0)
        ++work->qm_ticks;
    size_t dirt_items;
    mc_entity *dirt = NULL;
    CHECK(mining_items(f->app->active_world, &dirt_items, &dirt) == 0);
    const mc_client_render_global_entities_state *entities = world->entities;
    if (entities != NULL && entities->returned && entities->item_draws_returned > 0 &&
        (mining_dirt_draw(&entities->first_item_draw, dirt) ||
         mining_dirt_draw(&entities->first_entity_draw, dirt) ||
         mining_dirt_draw(&entities->last_draw, dirt))) {
        ++work->item_frames;
        if (work->item_frames == 1)
            CHECK(mining_capture(f, work, "dropped-item.png") == 0);
        if (work->item_frames == 3)
            CHECK(mining_capture(f, work, "dropped-item-settled.png") == 0);
    }
    int32_t carried = 0;
    CHECK(mining_current(f->app->h, &carried) == 0);
    const mc_client_first_person_hand_state *hand = world->hand;
    if (carried == 1 && f->state.hud->slots_returned && f->state.hud->empty_slots == 8 &&
        hand != NULL && hand->returned && hand->held_returned && hand->held_item != NULL &&
        hand->held_item->returned && hand->held_item->stack_captured &&
        !hand->held_item->empty_hand && hand->held_item->faces == 6 &&
        hand->held_item->vertices == 24) {
        ++work->held_frames;
        if (work->held_frames == 1)
            CHECK(mining_capture(f, work, "held-dirt.png") == 0);
    }
    return 0;
}

static int mining_frame(fixture *f, mining_work *work) {
    CHECK(save_title_focus(f) == 0 && frame(f) == 0);
    ++work->frames;
    return mining_observe(f, work);
}

static int mining_ready(fixture *f, mining_work *work, bool require_hit) {
    const uint64_t deadline = SDL_GetTicks() + 180000;
    mc_client_source_da *recovered[4] = {0};
    size_t focus_pauses = 0;
    for (size_t i = 0; i < 180 && SDL_GetTicks() < deadline; ++i) {
        CHECK(mining_frame(f, work) == 0);
        if (f->app->active_world == NULL)
            continue;
        if (f->app->r != NULL) {
            mc_chunk_provider_fields provider;
            size_t eligible;
            int64_t time;
            CHECK(mc_chunk_provider_bind_fields(f->app->active_world->fields.v, &provider, NULL) ==
                      MC_WORLD_STORAGE_OK &&
                  save_title_resume(f, f->app->active_world, &provider, recovered, &focus_pauses,
                                    &eligible, &time) == 0);
            continue;
        }
        CHECK(f->app->N && !atomic_load(&f->app->o));
        if (!require_hit)
            return 0;
        mc_entity_hit_result_view hit;
        if (f->app->y != NULL &&
            mc_entity_hit_result_inspect(f->app->y, &hit) == MC_ENTITY_HIT_OK &&
            hit.fields.b == 0 && hit.fields.c == 64 && hit.fields.d == 2 &&
            f->state.world_frame->selection_calls != 0)
            return 0;
    }
    fprintf(stderr, "Mining SDK fixture did not reach genuine focused target\n");
    return 1;
}

static int mining_admit_click(fixture *f, mining_work *work) {
    const uint64_t clicks = f->state.gameplay_left_clicks;
    const uint64_t swings = f->state.gameplay_swing_calls;
    const uint64_t stores = f->state.gameplay_click_timer_stores;
    const uint64_t starts = f->state.gameplay_mining_starts;
    const uint64_t progress = f->state.gameplay_mining_progress_calls;
    const uint64_t mined = f->state.gameplay_mined_blocks;
    const uint64_t deadline = SDL_GetTicks() + 60000;
    size_t frames = 0;
    for (size_t attempt = 0; attempt < 12 && frames < 64 && SDL_GetTicks() < deadline; ++attempt) {
        CHECK(f->app->r == NULL && save_title_focus(f) == 0);
        int64_t now_before, now_after;
        CHECK(mc_platform_current_time_millis(&now_before) == MC_PLATFORM_MILLIS_OK);
        const int64_t previous_tick = f->app->P;
        const bool focused_before = f->app->N;
        const uint64_t ticks_before = f->state.ticks;
        const size_t events_before = f->state.mouse_events;
        /* Both real native records enter the same unpolled SDK batch. Its
         * final button state is released, so a stale/focus-only tap cannot
         * accidentally advance the held mining path while we qualify it. */
        CHECK(mining_button(f, true) == 0);
        work->left_down = true;
        CHECK(mining_button(f, false) == 0);
        work->left_down = false;
        bool drained = false;
        mc_client_sdk_input_state input = {0};
        mc_client_sdk_bytes_state read = {0}, queue = {0}, buttons = {0};
        do {
            CHECK(mining_frame(f, work) == 0);
            ++frames;
            int32_t target;
            CHECK(f->app->r == NULL && mining_target(f->app->active_world, &target) == 0 &&
                  target == 3 && f->state.gameplay_mining_progress_calls == progress &&
                  f->state.gameplay_mined_blocks == mined &&
                  mc_client_sdk_input_inspect(mc_client_display_inputs(f->app->display), &input) &&
                  mc_client_sdk_bytes_inspect(input.mouse_read, &read) &&
                  mc_client_sdk_queue_inspect(input.mouse_events, &queue) &&
                  mc_client_sdk_bytes_inspect(input.buttons, &buttons) && buttons.limit > 0 &&
                  buttons.data[0] == 0);
            /* A focus click may clear the unread release through the actual
             * Mouse.setGrabbed/resetMouse body. Either way no prior tap may
             * remain in the source read buffer or its backend queue at retry. */
            drained = f->state.ticks > ticks_before && f->state.mouse_events > events_before &&
                      read.position == read.limit && queue.position == 0 &&
                      input.pending_native_event == NULL;
        } while (!drained && frames < 64 && SDL_GetTicks() < deadline);
        CHECK(mc_platform_current_time_millis(&now_after) == MC_PLATFORM_MILLIS_OK);
        const mc_client_gameplay_input *last = f->state.gameplay_input;
        const bool admitted = f->state.gameplay_left_clicks > clicks &&
                              f->state.gameplay_swing_calls > swings &&
                              f->state.gameplay_click_timer_stores > stores &&
                              f->state.gameplay_mining_starts > starts;
        printf(
            "MINING_CLICK_ADMISSION attempt=%zu frames=%zu drained=%d admitted=%d"
            " now-before=%" PRId64 " P-before=%" PRId64 " age-before=%" PRId64
            " N-before=%d now-after=%" PRId64 " P-after=%" PRId64 " age-after=%" PRId64
            " N-after=%d ticks=%" PRIu64 " mouse-events=%zu stale-last-tick=%zu"
            " read-remaining=%zu queued-bytes=%zu clicks=%" PRIu64 " swings=%" PRIu64
            " click-stores=%" PRIu64 " starts=%" PRIu64 " progress=%" PRIu64 " mined=%" PRIu64
            " render-nanos=%" PRId64 "\n",
            attempt + 1, frames, (int)drained, (int)admitted, now_before, previous_tick,
            mc_java_i64_sub(now_before, previous_tick), (int)focused_before, now_after, f->app->P,
            mc_java_i64_sub(now_after, f->app->P), (int)f->app->N, f->state.ticks - ticks_before,
            f->state.mouse_events - events_before, last == NULL ? 0 : last->stale_mouse_events,
            read.limit - read.position, queue.position, f->state.gameplay_left_clicks - clicks,
            f->state.gameplay_swing_calls - swings, f->state.gameplay_click_timer_stores - stores,
            f->state.gameplay_mining_starts - starts,
            f->state.gameplay_mining_progress_calls - progress,
            f->state.gameplay_mined_blocks - mined,
            mc_java_i64_sub(f->state.clocks.run_387, f->state.clocks.run_203));
        CHECK(fflush(stdout) == 0 && drained);
        if (admitted)
            return 0;
        /* Retry only a tap with no click-side effects: source-stale events or
         * the source focus-only branch. A partial action is a test failure. */
        CHECK(f->state.gameplay_left_clicks == clicks && f->state.gameplay_swing_calls == swings &&
              f->state.gameplay_click_timer_stores == stores &&
              f->state.gameplay_mining_starts == starts);
    }
    fprintf(stderr,
            "Mining SDK fixture did not admit a fresh real click within 12 taps/64 frames/60s\n");
    return 1;
}

static int mining_choose(fixture *f, mining_work *work, const reload_epoch *previous,
                         reload_epoch *current) {
    mc_client_source_rq *rq = NULL;
    CHECK(f->app->r != NULL && f->app->r->kind == MC_CLIENT_SCREEN_MAIN_MENU &&
          open_selection(f, f->app->r, &rq) == 0 && release_mouse(f, rq) == 0);
    int32_t index = -1;
    const size_t count = mc_client_save_list_size(rq->fields.n);
    for (size_t i = 0; i < count; ++i)
        if (filename_is(mc_client_save_list_at(rq->fields.n, i), "alpha")) {
            CHECK(index == -1);
            index = (int32_t)i;
        }
    CHECK(index >= 0);
    const mc_client_world_list_fields *list = &rq->fields.o->fields;
    const int32_t y = list->h + 4 - mc_java_float_to_i32(list->m) + index * 36 + 8;
    CHECK(push_mouse(f, &rq->base, rq->base.fields.c / 2, y, true) == 0 && transfer(f) == 0);
    const uint64_t deadline = SDL_GetTicks() + 5000;
    while (rq->fields.m != index && SDL_GetTicks() < deadline)
        CHECK(frame(f) == 0);
    CHECK(rq->fields.m == index && release_mouse(f, rq) == 0 &&
          push_button(f, &rq->base, 1, true) == 0 && push_button(f, &rq->base, 1, false) == 0 &&
          transfer(f) == 0 && mining_ready(f, work, false) == 0 &&
          reload_capture_epoch(f, current) == 0);
    if (previous != NULL)
        CHECK(current->world != previous->world && current->player != previous->player &&
              current->handler != previous->handler &&
              current->controller != previous->controller && current->call != previous->call);
    return 0;
}

static int mining_save(fixture *f, mining_work *work, size_t epoch, const char *snapshot) {
    char path[8192];
    CHECK(!mining_saving.armed && epoch >= 1 && epoch <= 2 &&
          mining_path(work, "save-title.png", path) == 0);
    memset(&mining_saving, 0, sizeof(mining_saving));
    mining_saving.fixture = f;
    mining_saving.work = work;
    mining_saving.epoch = epoch;
    mining_saving.armed = true;
    const int saved = save_title_run(f, NULL, path, false, epoch, 0);
    mining_saving.armed = false;
    CHECK(saved == 0 && mining_saving.selected && mining_saving.samples > 0 &&
          reload_snapshot(snapshot) == 0);
    printf("MINING_SAVE_ENTRY epoch=%zu samples=%zu items=%zu all-items=%zu paused=1 owned-nbt=1 "
           "selected-before-leave-frame=1\n",
           epoch, mining_saving.samples, mining_saving.items, mining_saving.all_items);
    printf("MINING_SAVE epoch=%zu air=1 items=%zu ordinary-save-title=1\n", epoch,
           mining_saving.items);
    return 0;
}

static void mining_pickup_diagnostics(const fixture *f, const mining_work *work, int32_t count,
                                      double before_z) {
    const mc_entity *player = f->app->h;
    const mc_world_reference_list *list =
        f->app->active_world == NULL ? NULL : f->app->active_world->fields.b;
    const bool valid_list = list != NULL && list->count <= list->capacity && list->count <= 4096;
    size_t dirt_count = 0, invalid_items = 0;
    const mc_entity *dirt = NULL;
    if (valid_list)
        for (size_t i = 0; i < list->count; ++i) {
            const mc_entity *entity = list->items[i];
            if (entity == NULL || !mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_ITEM))
                continue;
            mc_item_stack_fields stack;
            if (entity->stack == NULL ||
                mc_item_stack_fields_get(entity->stack, &stack) != MC_ITEM_STACK_OK) {
                ++invalid_items;
                continue;
            }
            if (stack.id == 3) {
                if (dirt == NULL)
                    dirt = entity;
                if (!entity->base.be)
                    ++dirt_count;
            }
        }
    if (!work->forward_down && count == 1 && player != NULL && player->base.z > before_z &&
        valid_list && dirt_count == 0 && invalid_items == 0)
        return;
    const mc_client_source_da *screen = f->app->r;
    const char *screen_class = screen == NULL                                     ? "none"
                               : screen->kind == MC_CLIENT_SCREEN_PAUSE           ? "oz"
                               : screen->kind == MC_CLIENT_SCREEN_MAIN_MENU       ? "fu"
                               : screen->kind == MC_CLIENT_SCREEN_WORLD_SELECTION ? "rq"
                                                                                  : "da";
    fprintf(stderr,
            "[mining-pickup-diagnostic] forward-down=%d count=%" PRId32
            " before-z=%.17g N=%d paused=%d screen=%" PRIuPTR " screen-class=%s "
            "frames=%zu ticks=%" PRIu64 " tick-returns=%" PRId32 " timer-ticks=%" PRId32
            " world-tick-returns=%" PRIu64 " mouse-events=%zu keyboard-events=%zu "
            "list-valid=%d list=%" PRIuPTR " list-count=%zu live-dirt=%zu invalid-items=%zu\n",
            (int)work->forward_down, count, before_z, (int)f->app->N, (int)atomic_load(&f->app->o),
            (uintptr_t)screen, screen_class, work->frames, f->state.ticks,
            f->state.clocks.tick_returns, f->state.clocks.timer_ticks, f->state.world_tick_returns,
            f->state.mouse_events, f->state.keyboard_events, (int)valid_list, (uintptr_t)list,
            list == NULL ? 0 : list->count, dirt_count, invalid_items);
    if (player != NULL) {
        fprintf(stderr,
                "[mining-pickup-diagnostic] player=%" PRIuPTR " id=%" PRId32
                " pos=%.17g,%.17g,%.17g motion=%.17g,%.17g,%.17g yaw=%.9g pitch=%.9g "
                "dead=%d world-match=%d\n",
                (uintptr_t)player, player->base.id, player->base.x, player->base.y, player->base.z,
                player->base.velocity_x, player->base.velocity_y, player->base.velocity_z,
                (double)player->base.yaw, (double)player->base.aT, (int)player->base.be,
                (int)(player->base.world == f->app->active_world));
        mc_local_player_snapshot local;
        mc_movement_input_state input;
        if (mc_local_player_get_snapshot(player, &local) == MC_ENTITY_OK &&
            mc_movement_input_read_state(local.input, &input))
            fprintf(stderr,
                    "[mining-pickup-diagnostic] input=%" PRIuPTR " kind=%d forward=%.9g "
                    "strafe=%.9g jump=%d sneak=%d key-count=%zu "
                    "keys=%d%d%d%d%d%d%d%d%d%d living-af=%.9g living-ag=%.9g\n",
                    (uintptr_t)local.input, (int)input.kind, (double)input.fields.forward,
                    (double)input.fields.strafe, (int)input.fields.jump, (int)input.fields.sneak,
                    input.key_count, (int)input.keys[0], (int)input.keys[1], (int)input.keys[2],
                    (int)input.keys[3], (int)input.keys[4], (int)input.keys[5], (int)input.keys[6],
                    (int)input.keys[7], (int)input.keys[8], (int)input.keys[9],
                    (double)local.living.af, (double)local.living.ag);
    }
    mc_client_sdk_input_state sdk;
    mc_client_sdk_bytes_state keys, read, queue;
    if (mc_client_sdk_input_inspect(mc_client_display_inputs(f->app->display), &sdk) &&
        mc_client_sdk_bytes_inspect(sdk.key_down, &keys) &&
        mc_client_sdk_bytes_inspect(sdk.keyboard_read, &read) &&
        mc_client_sdk_queue_inspect(sdk.keyboard_events, &queue))
        fprintf(stderr,
                "[mining-pickup-diagnostic] sdk-W17=%d sdk-S31=%d read-remaining=%zu "
                "queued-bytes=%zu last-key=%" PRId32
                " pressed=%d repeat=%d pending-native=%" PRIuPTR "\n",
                keys.limit > 17 ? (int)keys.data[17] : -1,
                keys.limit > 31 ? (int)keys.data[31] : -1, read.limit - read.position,
                queue.position, sdk.current_key.key, (int)sdk.current_key.pressed,
                (int)sdk.current_key.repeat, (uintptr_t)sdk.pending_native_event);
    if (dirt != NULL) {
        mc_item_stack_fields stack = {0};
        const mc_item_stack_status status = mc_item_stack_fields_get(dirt->stack, &stack);
        fprintf(stderr,
                "[mining-pickup-diagnostic] dirt=%" PRIuPTR " id=%" PRId32
                " in-world-list=1 pos=%.17g,%.17g,%.17g motion=%.17g,%.17g,%.17g dead=%d "
                "world-match=%d stack=%" PRIuPTR " stack-status=%d stack-id=%" PRId32
                " stack-count=%" PRId32 " stack-damage=%" PRId32 "\n",
                (uintptr_t)dirt, dirt->base.id, dirt->base.x, dirt->base.y, dirt->base.z,
                dirt->base.velocity_x, dirt->base.velocity_y, dirt->base.velocity_z,
                (int)dirt->base.be, (int)(dirt->base.world == f->app->active_world),
                (uintptr_t)dirt->stack, (int)status, stack.id, stack.count, stack.damage);
    }
    (void)fflush(stderr);
}

static int mining_run(fixture *f, mining_work *work) {
    CHECK(mc_client_menu_loop_create(f->bootstrap, &f->loop) &&
          mc_client_menu_loop_inspect(f->loop, &f->state) &&
          repeat_statistics_controls(f, true) == 0 && save_title_focus(f) == 0 &&
          ticks(f, 2) == 0 && mining_choose(f, work, NULL, &work->epochs[0]) == 0 &&
          mining_ready(f, work, true) == 0);
    int32_t target = 0, count = 0;
    CHECK(mining_statistic(f, 0) == 0);
    CHECK(mining_target(f->app->active_world, &target) == 0 && target == 3 &&
          mining_current(f->app->h, &count) == 0 && count == 0 &&
          mining_capture(f, work, "aimed.png") == 0);
    const uint64_t starts = f->state.gameplay_mining_starts, clicks = f->state.gameplay_left_clicks;
    CHECK(mining_admit_click(f, work) == 0);
    CHECK(mining_button(f, true) == 0);
    work->left_down = true;
    const uint64_t deadline = SDL_GetTicks() + 180000;
    for (size_t i = 0; i < 240 && SDL_GetTicks() < deadline; ++i) {
        CHECK(mining_frame(f, work) == 0 && mining_target(f->app->active_world, &target) == 0);
        if (target == 0) {
            CHECK(mining_button(f, false) == 0);
            work->left_down = false;
            break;
        }
        CHECK(target == 3);
    }
    const mc_effect_draw_state *sprites = f->state.world_frame->sprite_particles;
    printf("MINING_INPUT_COUNTS left-down=%d target=%" PRId32
           " frames=%zu partial=%zu qm-frames=%zu qm-ticks=%zu max-ticks=%" PRId32
           " clicks-before=%" PRIu64 " clicks=%" PRIu64 " swings=%" PRIu64 " click-stores=%" PRIu64
           " starts-before=%" PRIu64 " starts=%" PRIu64 " progress=%" PRIu64 " mined=%" PRIu64
           " hit-constructed=%" PRIu64 " hit-appended=%" PRIu64
           " sprite-present=%d sprite-returned=%d sprite-visited=%zu"
           " sprite-item-returned=%d sprite-vertices=%u sprite-particle-pc=%u\n",
           (int)work->left_down, target, work->frames, work->partial_frames, work->qm_frames,
           work->qm_ticks, work->maximum_ticks, clicks, f->state.gameplay_left_clicks,
           f->state.gameplay_swing_calls, f->state.gameplay_click_timer_stores, starts,
           f->state.gameplay_mining_starts, f->state.gameplay_mining_progress_calls,
           f->state.gameplay_mined_blocks, f->state.gameplay_hit_particles_constructed,
           f->state.gameplay_hit_particles_appended, (int)(sprites != NULL),
           (int)(sprites != NULL && sprites->returned), sprites == NULL ? 0 : sprites->visited,
           (int)(sprites != NULL && sprites->item_particle_returned),
           sprites == NULL ? 0u : sprites->item_vertices_returned,
           sprites == NULL ? 0u : sprites->particle_pc);
    CHECK(fflush(stdout) == 0);
    CHECK(!work->left_down && target == 0 && work->partial_frames > 0 && work->qm_frames > 0 &&
          work->qm_ticks > 0 && f->state.gameplay_left_clicks > clicks &&
          f->state.gameplay_swing_calls > 0 && f->state.gameplay_click_timer_stores > 0 &&
          f->state.gameplay_mining_starts > starts && f->state.gameplay_mining_progress_calls > 0 &&
          f->state.gameplay_mined_blocks == 1 && f->state.gameplay_hit_particles_constructed > 0 &&
          f->state.gameplay_hit_particles_appended > 0);
    mc_render_global_auxiliary_observation auxiliary;
    CHECK(mc_render_global_access_auxiliary_observe(f->app->g, &auxiliary) &&
          !auxiliary.overflowed && auxiliary.event2001_entered == 1 &&
          auxiliary.event2001_returned == 1 && auxiliary.break_sound_returns == 1 &&
          auxiliary.destruction_calls == 1 && auxiliary.destruction_returns == 1 &&
          auxiliary.destruction_constructed == 64 && auxiliary.destruction_appended == 64 &&
          auxiliary.last.x == 0 && auxiliary.last.y == 64 && auxiliary.last.z == 2 &&
          auxiliary.last.block_id == 3 && auxiliary.last.metadata == 0 &&
          auxiliary.last.sound_returned && auxiliary.last.destruction_returned &&
          auxiliary.last.sound_before_destruction && auxiliary.last.source_returned &&
          auxiliary.last.native_cleanup_returned && auxiliary.last.constructed == 64 &&
          auxiliary.last.appended == 64);
    puts("MINING_DESTRUCTION event2001=1 sound=1 qm-constructed=64 qm-appended=64 "
         "sound-before-particles=1 before-air=1");
    CHECK(mining_statistic(f, 1) == 0 && mining_capture(f, work, "air.png") == 0);
    const uint64_t item_deadline = SDL_GetTicks() + 60000;
    while (work->item_frames < 3 && SDL_GetTicks() < item_deadline)
        CHECK(mining_frame(f, work) == 0);
    CHECK(work->item_frames >= 3 && mining_current(f->app->h, &count) == 0 && count == 0);
    printf("MINING_BREAK frames=%zu partial=%zu qm-frames=%zu qm-ticks=%zu item-frames=%zu "
           "max-ticks=%" PRId32 " clicks=%" PRIu64 " swings=%" PRIu64 " click-stores=%" PRIu64
           " starts=%" PRIu64 " progress=%" PRIu64 " mined=%" PRIu64 " hit-constructed=%" PRIu64
           " hit-appended=%" PRIu64 "\n",
           work->frames, work->partial_frames, work->qm_frames, work->qm_ticks, work->item_frames,
           work->maximum_ticks, f->state.gameplay_left_clicks, f->state.gameplay_swing_calls,
           f->state.gameplay_click_timer_stores, f->state.gameplay_mining_starts,
           f->state.gameplay_mining_progress_calls, f->state.gameplay_mined_blocks,
           f->state.gameplay_hit_particles_constructed, f->state.gameplay_hit_particles_appended);
    CHECK(mining_save(f, work, 1, "saved-1") == 0 &&
          mining_choose(f, work, &work->epochs[0], &work->epochs[1]) == 0 &&
          mining_target(f->app->active_world, &target) == 0 && target == 0 &&
          mining_semantic(f->app->h, work, "reloaded-player-1.nbt") == 0);
    size_t items;
    mc_entity *item = NULL;
    CHECK(mining_items(f->app->active_world, &items, &item) == 0 && items == 1 &&
          mining_semantic(item, work, "reloaded-item-1.nbt") == 0);
    const size_t item_before = work->item_frames;
    const uint64_t reload_deadline = SDL_GetTicks() + 60000;
    while (work->item_frames < item_before + 2 && SDL_GetTicks() < reload_deadline)
        CHECK(mining_frame(f, work) == 0);
    CHECK(work->item_frames >= item_before + 2 && gameplay_key(f, SDL_SCANCODE_W, true) == 0 &&
          transfer(f) == 0);
    work->forward_down = true;
    const double before_z = f->app->h->base.z;
    const uint64_t pickup_deadline = SDL_GetTicks() + 60000;
    for (size_t i = 0; i < 180 && SDL_GetTicks() < pickup_deadline; ++i) {
        CHECK(mining_frame(f, work) == 0 && mining_current(f->app->h, &count) == 0);
        if (count == 1) {
            CHECK(gameplay_key(f, SDL_SCANCODE_W, false) == 0 && transfer(f) == 0);
            work->forward_down = false;
            break;
        }
    }
    mining_pickup_diagnostics(f, work, count, before_z);
    CHECK(!work->forward_down && count == 1 && f->app->h->base.z > before_z &&
          mining_items(f->app->active_world, &items, &item) == 0 && items == 0);
    const uint64_t carried_deadline = SDL_GetTicks() + 60000;
    while (work->held_frames < 3 && SDL_GetTicks() < carried_deadline)
        CHECK(mining_frame(f, work) == 0);
    CHECK(work->held_frames >= 3);
    printf("MINING_PICKUP sdk-forward=1 actual-movement=1 inventory-dirt=1 count=1 live-items=0 "
           "held-frames=%zu\n",
           work->held_frames);
    CHECK(mining_save(f, work, 2, "saved-2") == 0 &&
          mining_choose(f, work, &work->epochs[1], &work->epochs[2]) == 0 &&
          mining_target(f->app->active_world, &target) == 0 && target == 0 &&
          mining_current(f->app->h, &count) == 0 && count == 1 &&
          mining_items(f->app->active_world, &items, &item) == 0 && items == 0 &&
          mining_semantic(f->app->h, work, "reloaded-player-2.nbt") == 0 &&
          mining_capture(f, work, "reloaded.png") == 0);
    CHECK(mining_statistic(f, 1) == 0);
    puts("MINING_RELOAD epochs=3 saved-item=1 reloaded-item=1 picked-item-absent=1 air=1 "
         "inventory-dirt=1");
    return 0;
}

#ifndef MC_MINING_FIXTURE_MAIN
#define MC_MINING_FIXTURE_MAIN main
#endif
int MC_MINING_FIXTURE_MAIN(int argc, char **argv) {
    CHECK(argc == 3);
    fixture *f = calloc(1, sizeof(*f));
    mining_work *work = calloc(1, sizeof(*work));
    CHECK(f != NULL && work != NULL);
    mining_fixture = f;
    work->directory = argv[2];
    const mc_client_application_options options = {.assets = argv[1],
                                                   .username = "Selection control",
                                                   .width = 854,
                                                   .height = 480,
                                                   .hidden = false};
    int result = 1;
    uint32_t frame_id = 0, canvas_id = 0;
    if (mc_client_bootstrap_open_standalone(&options, &f->bootstrap) == MC_CLIENT_BOOTSTRAP_OK) {
        f->app = mc_client_bootstrap_app(f->bootstrap);
        mc_client_frame_state host;
        if (f->app != NULL &&
            mc_client_source_frame_inspect(mc_client_bootstrap_frame(f->bootstrap), &host) &&
            host.frame_window != NULL && host.canvas_window != NULL) {
            frame_id = SDL_GetWindowID(host.frame_window);
            canvas_id = SDL_GetWindowID(host.canvas_window);
            result = mining_run(f, work);
        }
    }
    if (f->app != NULL && work->left_down)
        (void)mining_button(f, false);
    if (f->app != NULL && work->forward_down) {
        (void)gameplay_key(f, SDL_SCANCODE_W, false);
        (void)transfer(f);
    }
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
        printf("MINING_FINISH cleanup=1 windows-absent=1 checks=%u\n", checks);
    free(work);
    free(f);
    return result;
}
