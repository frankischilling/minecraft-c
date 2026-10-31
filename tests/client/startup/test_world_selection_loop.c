/* Genuine native bootstrap/menu composition with authored SDL transport events. */
#include "../../support/java_random.h"
#include "../world/test_load_world_foreign_loading.h"
#include "../world/test_refresh_entity.h"
#include "../world/test_render_global_entities.h"
#include "../world/test_renderer_focus.h"
#include "client/application_state.h"
#include "client/display/sdk_input.h"
#include "client/display/source_surface.h"
#include "client/display/staged.h"
#include "client/gui/display_screen.h"
#include "client/gui/source_pause_screen.h"
#include "client/gui/source_screen_initialize_internal.h"
#include "client/gui/source_screen_transitions.h"
#include "client/gui/source_screen_transitions_terminal_internal.h"
#include "client/gui/source_world_list.h"
#include "client/gui/source_world_selection_drawing.h"
#include "client/gui/source_world_selection_internal.h"
#include "client/input/mouse_helper.h"
#include "client/render/manager/internal.h"
#include "client/render/tile_entity_renderer/internal.h"
#include "client/runtime/gameplay_input.h"
#include "client/runtime/menu_loop.h"
#include "client/runtime/world_tick.h"
#include "client/startup/application_startup.h"
#include "client/startup/application_startup_internal.h"
#include "client/world/load_world.h"
#include "client/world/prepare_world.h"
#include "client/world/render_global_access.h"
#include "client/world/render_global_internal.h"
#include "client/world/source_chunk_compile_internal.h"
#include "entity/entity_internal.h"
#include "mc/client/controller/singleplayer.h"
#include "mc/client/gui/gui_ingame_draw.h"
#include "mc/client/input/movement_input.h"
#include "mc/client/particles/effect_renderer.h"
#include "mc/client/statistics/writer.h"
#include "mc/client/world/chunk_cache.h"
#include "mc/client/world/held_item_draw.h"
#include "mc/client/world/picking.h"
#include "mc/client/world/world_frame.h"
#include "mc/core/java/java.h"
#include "mc/core/math/math.h"
#include "mc/entity/living/view.h"
#include "mc/entity/mob/construction.h"
#include "mc/entity/player/player.h"
#include "mc/world/runtime/blocks.h"
#include "mc/world/runtime/loading/player.h"
#include "mc/world/runtime/propagation.h"
#include "mc/world/runtime/random_tick.h"
#include "platform/time/millis.h"
#include "world/runtime/chunk_provider_internal.h"
#include "world/runtime/construction_internal.h"
#include "world/runtime/runtime_internal.h"
#include "world/runtime/source_construction_internal.h"
#include "world/storage/objects_internal.h"

#include <SDL3/SDL.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int checks;

static bool world_dependency_reached(const mc_world_tick_state *state,
                                     const mc_world_tick_error *error) {
    if (state->returned && state->source_pc == 198) {
        const mc_world_random_tick_state *random = state->random;
        return error->status == MC_WORLD_STORAGE_OK && state->weather_returned &&
               state->spawning_returned && state->provider_returned && state->time_stored &&
               state->scheduled_returned && state->random_returned &&
               state->random_chunk_count == 361 && random != NULL && random->returned &&
               random->source_pc == 994 && random->chunk_count == 361 &&
               random->chunks_started == 361 && random->chunks_returned == 361 &&
               random->samples_started == 28880 && random->samples_returned == 28880 &&
               random->error->status == MC_WORLD_STORAGE_OK;
    }
    mc_entity_snapshot entity;
    mc_mob_state mob;
    if (!state->first_constructor.captured || state->spawn_constructors == 0 ||
        state->spawn_constructors != state->spawn_predicates ||
        !state->spawn_eligibility_returned || !state->spawn_constructed ||
        !state->spawn_positioned || state->spawn_entity == NULL ||
        mc_entity_get_snapshot(state->spawn_entity, &entity) != MC_ENTITY_OK ||
        !mc_mob_inspect(state->spawn_entity, &mob) || entity.phase != MC_ENTITY_READY ||
        entity.base.world != state->world || !error->native_policy)
        return false;
    mc_entity_snapshot jockey;
    return state->source_pc == 80 && error->child == MC_WORLD_TICK_CHILD_SPAWNING &&
           error->child_bytecode == 845 && state->spawn_postspawn_pc == 56 &&
           state->spawn_eligible && state->spawn_publication_returned &&
           mob.kind == MC_ENTITY_CLASS_KIND_SPIDER && state->spawn_jockey != NULL &&
           mc_entity_get_snapshot(state->spawn_jockey, &jockey) == MC_ENTITY_OK &&
           jockey.phase == MC_ENTITY_READY && jockey.base.world == state->world;
}

static bool loaded_draw_reached(const mc_client_menu_loop_state *source) {
    if (source->world_draws == 0 || source->hud_draws != source->world_draws ||
        !source->render_clock_stored)
        return false;
    if (!source->failed)
        return source->render_returned;
    return source->source_class != NULL && strcmp(source->source_class, "oz") == 0 &&
           source->source_method != NULL && strcmp(source->source_method, "a") == 0 &&
           source->source_pc == 22;
}

/* Passive verification of an exact retained N/TH invocation and its canonical
 * Map entry. Returning Sheep frames use copied receipts below instead. */
static const char *mob_renderer_class(mc_entity_class_kind kind) {
    switch (kind) {
        case MC_ENTITY_CLASS_KIND_SPIDER:
            return "yx";
        case MC_ENTITY_CLASS_KIND_PIG:
            return "me";
        case MC_ENTITY_CLASS_KIND_SHEEP:
            return "xy";
        case MC_ENTITY_CLASS_KIND_COW:
            return "va";
        case MC_ENTITY_CLASS_KIND_WOLF:
            return "we";
        case MC_ENTITY_CLASS_KIND_CHICKEN:
            return "ip";
        case MC_ENTITY_CLASS_KIND_CREEPER:
            return "m";
        case MC_ENTITY_CLASS_KIND_SKELETON:
        case MC_ENTITY_CLASS_KIND_ZOMBIE:
            return "v";
        case MC_ENTITY_CLASS_KIND_SLIME:
            return "mj";
        case MC_ENTITY_CLASS_KIND_SQUID:
            return "es";
        default:
            return NULL;
    }
}

/* The returned empty-hand draw has run RA.a's real TH superclass lookup:
 * dc adds one canonical key at TH.a(Class)28 and shares the existing gs/DS
 * owner. Its 28 constructor keys and distinct renderer owners remain intact. */
static bool client_renderer_roster(const client_application *app,
                                   mc_client_render_manager *receiver,
                                   const mc_client_manager_state *manager) {
    static const struct {
        mc_entity_class_kind kind;
        const char *renderer;
    } constructed[] = {{MC_ENTITY_CLASS_KIND_SPIDER, "yx"},
                       {MC_ENTITY_CLASS_KIND_PIG, "me"},
                       {MC_ENTITY_CLASS_KIND_SHEEP, "xy"},
                       {MC_ENTITY_CLASS_KIND_COW, "va"},
                       {MC_ENTITY_CLASS_KIND_WOLF, "we"},
                       {MC_ENTITY_CLASS_KIND_CHICKEN, "ip"},
                       {MC_ENTITY_CLASS_KIND_CREEPER, "m"},
                       {MC_ENTITY_CLASS_KIND_SKELETON, "v"},
                       {MC_ENTITY_CLASS_KIND_ZOMBIE, "v"},
                       {MC_ENTITY_CLASS_KIND_SLIME, "mj"},
                       {MC_ENTITY_CLASS_KIND_PLAYER, "ds"},
                       {MC_ENTITY_CLASS_KIND_GIANT, "yg"},
                       {MC_ENTITY_CLASS_KIND_GHAST, "pq"},
                       {MC_ENTITY_CLASS_KIND_SQUID, "es"},
                       {MC_ENTITY_CLASS_KIND_LIVING_ENTITY, "gv"},
                       {MC_ENTITY_CLASS_KIND_ENTITY, "mb"},
                       {MC_ENTITY_CLASS_KIND_PAINTING, "dy"},
                       {MC_ENTITY_CLASS_KIND_ARROW, "mc"},
                       {MC_ENTITY_CLASS_KIND_SNOWBALL, "dg"},
                       {MC_ENTITY_CLASS_KIND_VV, "dg"},
                       {MC_ENTITY_CLASS_KIND_CF, "kl"},
                       {MC_ENTITY_CLASS_KIND_ITEM, "bb"},
                       {MC_ENTITY_CLASS_KIND_PRIMED_TNT, "on"},
                       {MC_ENTITY_CLASS_KIND_FALLING_SAND, "gn"},
                       {MC_ENTITY_CLASS_KIND_MINECART, "tb"},
                       {MC_ENTITY_CLASS_KIND_BOAT, "fe"},
                       {MC_ENTITY_CLASS_KIND_LX, "pl"},
                       {MC_ENTITY_CLASS_KIND_C, "pi"}};

    enum { constructor_count = sizeof(constructed) / sizeof(constructed[0]) };

    _Static_assert(constructor_count == 28, "actual TH constructor roster");
    mc_client_managed_renderer *owners[constructor_count] = {0};
    mc_client_managed_renderer *player = NULL, *cached = NULL;
    size_t cached_keys = 0;
    if (manager->entries != constructor_count + 1 || manager->completed_bw_stores != 28 ||
        manager->last_put_pc != 28 || !mc_entity_is_kind(app->h, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER))
        return false;
    for (size_t row = 0; row < manager->entries; ++row) {
        const mc_entity_class_literal *key = NULL, *canonical = NULL;
        mc_client_managed_renderer *renderer = NULL;
        mc_entity_class_literal_view literal;
        mc_client_managed_renderer_state state;
        if (mc_client_render_manager_entry(receiver, row, &key, &renderer, NULL) !=
                MC_CLIENT_MANAGER_OK ||
            mc_entity_class_literal_inspect(key, &literal) != MC_ENTITY_OK ||
            literal.owner != app->entities || literal.source != MC_ENTITY_CLIENT ||
            mc_entity_class_literal_get(app->entities, literal.kind, &canonical) != MC_ENTITY_OK ||
            key != canonical || renderer == NULL ||
            !mc_client_managed_renderer_inspect(renderer, &state) || state.identity == NULL ||
            state.phase != MC_CLIENT_MODEL_READY || state.manager != receiver ||
            state.source_class == NULL)
            return false;
        if (literal.kind == MC_ENTITY_CLASS_KIND_LOCAL_PLAYER) {
            if (++cached_keys != 1 || key != app->h->class_literal ||
                strcmp(state.source_class, "ds"))
                return false;
            cached = renderer;
            continue;
        }
        size_t index = 0;
        while (index < constructor_count && constructed[index].kind != literal.kind)
            ++index;
        if (index == constructor_count || owners[index] != NULL ||
            strcmp(state.source_class, constructed[index].renderer))
            return false;
        for (size_t prior = 0; prior < constructor_count; ++prior)
            if (owners[prior] == renderer)
                return false;
        owners[index] = renderer;
        if (literal.kind == MC_ENTITY_CLASS_KIND_PLAYER)
            player = renderer;
    }
    for (size_t index = 0; index < constructor_count; ++index)
        if (owners[index] == NULL)
            return false;
    return cached_keys == 1 && cached != NULL && cached == player;
}

static mc_entity *pending_mob_render_receiver(const mc_client_menu_loop_state *source,
                                              const client_application *app) {
    const mc_client_world_frame_state *draw = source->world_frame;
    if (app == NULL || app->active_world == NULL || source->source_class == NULL ||
        strcmp(source->source_class, "n") || source->source_method == NULL ||
        strcmp(source->source_method, "a") || source->source_pc != 516 || !source->failed ||
        draw == NULL || draw->active || draw->returned || draw->source_pc != 510 ||
        draw->app != app || draw->world != app->active_world || draw->owner != app->t ||
        draw->render_global != app->g || draw->camera != app->i || !draw->terrain_returned ||
        !draw->lighting_enabled || !draw->eye_position_returned || draw->entities_returned ||
        draw->entity_eye == NULL || draw->culler == NULL || draw->failure == NULL ||
        draw->failure->status != MC_CLIENT_WORLD_FRAME_DEPENDENCY ||
        !draw->failure->native_policy || draw->failure->site.class_name == NULL ||
        strcmp(draw->failure->site.class_name, "n") || draw->failure->site.method == NULL ||
        strcmp(draw->failure->site.method, "a") || draw->failure->site.pc != 516 ||
        source->hud != NULL)
        return NULL;
    const mc_client_render_global_entities_state *entities = draw->entities;
    if (entities == NULL || entities->active || entities->returned || entities->retiring ||
        entities->delayed || !entities->ll_returned || !entities->th_returned ||
        entities->source_pc != 516 || entities->last_store_pc != 507 ||
        !entities->counters_cleared || !entities->origins_stored ||
        entities->origins_written != 6 || entities->owner != app->g ||
        entities->eye != draw->entity_eye || entities->partial != draw->partial ||
        entities->frustum == NULL || entities->th == NULL || entities->entities_visited == 0 ||
        entities->distance_checks == 0 || entities->presence_checks == 0 || entities->index < 0 ||
        entities->failure == NULL || entities->failure != draw->failure->entities)
        return NULL;
    const mc_client_render_global_error *failure = entities->failure;
    if (failure->status != MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY ||
        !failure->native_policy || failure->source_class == NULL ||
        strcmp(failure->source_class, "n") || failure->source_method == NULL ||
        strcmp(failure->source_method, "a") || failure->source_descriptor == NULL ||
        strcmp(failure->source_descriptor, "(Lbt;Lyn;F)V") || failure->source_pc != 516 ||
        failure->child.class_name == NULL || strcmp(failure->child.class_name, "th") ||
        failure->child.method == NULL || strcmp(failure->child.method, "a") ||
        failure->child.descriptor == NULL || strcmp(failure->child.descriptor, "(Lsn;F)V") ||
        failure->child.pc != 0)
        return NULL;
    const mc_world_runtime *world = app->active_world;
    const mc_world_entity_list_ledger *ledger = &world->entity_membership;
    const size_t index = (size_t)entities->index;
    if (world->fields.b == NULL || world->fields.b != ledger->list ||
        entities->entities_list != ledger->list || world->fields.b->count != ledger->count ||
        index >= ledger->count || world->fields.b->items[index] != ledger->entries[index] ||
        entities->current != ledger->entries[index])
        return NULL;
    mc_entity *entity = entities->current;
    mc_mob_state mob;
    if (entity == NULL || !mc_mob_inspect(entity, &mob) || entity->phase != MC_ENTITY_READY ||
        entity->classes != world->classes->entities || entity->base.world != world ||
        entity->base.be || !mc_entity_is_kind(entity, mob.kind))
        return NULL;
    const char *expected = mob_renderer_class(mob.kind);
    mc_client_manager_state manager;
    if (expected == NULL ||
        !mc_client_render_manager_inspect(app->g->entity_manager_consumer.classes, &manager) ||
        manager.phase != MC_CLIENT_MANAGER_READY || manager.a != entities->th ||
        manager.fields.g != world || manager.living_occurrence != app->i ||
        !client_renderer_roster(app, entities->th, &manager))
        return NULL;
    for (size_t row = 0; row < manager.entries; ++row) {
        const mc_entity_class_literal *key = NULL;
        mc_client_managed_renderer *renderer = NULL;
        if (mc_client_render_manager_entry(entities->th, row, &key, &renderer, NULL) !=
            MC_CLIENT_MANAGER_OK)
            return NULL;
        if (key != entity->class_literal)
            continue;
        mc_client_managed_renderer_state state;
        return renderer != NULL && mc_client_managed_renderer_inspect(renderer, &state) &&
                       state.identity == renderer && state.phase == MC_CLIENT_MODEL_READY &&
                       state.manager == entities->th && state.source_class != NULL &&
                       !strcmp(state.source_class, expected)
                   ? entity
                   : NULL;
    }
    return NULL;
}

static bool unpaused_world_reached(const mc_client_menu_loop_state *source,
                                   const client_application *app) {
    if (!source->failed || source->source_class == NULL || source->source_method == NULL ||
        source->world_draws < 1 || source->hud_draws < 1 || source->world_tick == NULL ||
        !source->world_tick->renderer_returned)
        return false;
    const mc_client_world_tick *call = source->world_tick;
    if (!strcmp(source->source_class, "n") && !strcmp(source->source_method, "a") &&
        source->source_pc == 516) {
        const mc_world_random_display_state *display = &call->ambience_state;
        const mc_world_tick_state *scheduler = &call->scheduler_state;
        /* Native child retirement has ended the random call views. These
         * producer-owned scalar receipts were captured before that cleanup. */
        return call->returned && !call->failed && call->site.pc == 1328 &&
               call->render_global_returned && call->entities_returned && scheduler->returned &&
               scheduler->source_pc == 198 && scheduler->weather_returned &&
               scheduler->spawning_returned && scheduler->provider_returned &&
               scheduler->time_stored && scheduler->scheduled_returned &&
               scheduler->random_returned && scheduler->random_chunk_count == 361 &&
               call->scheduler_error.status == MC_WORLD_STORAGE_OK && call->scheduler == NULL &&
               scheduler->random == NULL && call->ambience == NULL &&
               call->ambience_error.status == MC_WORLD_STORAGE_OK && display->returned &&
               display->source_pc == 134 && display->samples_started == 1000 &&
               display->samples_returned == 1000 && display->world_random_calls == 6000 &&
               source->random_chunk_calls > 0 && source->last_random_chunk_count == 361 &&
               source->random_chunk_samples == source->random_chunk_calls * UINT64_C(28880) &&
               source->random_display_calls == source->random_chunk_calls &&
               source->random_display_samples == source->random_display_calls * UINT64_C(1000) &&
               source->random_display_random_calls ==
                   source->random_display_calls * UINT64_C(6000) &&
               pending_mob_render_receiver(source, app) != NULL;
    }
    return call->failed && call->entities_returned &&
           world_dependency_reached(&call->scheduler_state, &call->scheduler_error) &&
           strcmp(source->source_class, "fd") == 0 && strcmp(source->source_method, "l") == 0 &&
           source->source_pc == 80 && source->world_frame == NULL && source->hud == NULL;
}

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "World selection loop check%u at%d: %s\n", checks, __LINE__,           \
                    #condition);                                                                   \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

typedef struct fixture {
    mc_client_bootstrap *bootstrap;
    client_application *app;
    mc_client_menu_loop *loop;
    mc_client_menu_loop_state state;
    uintptr_t repeat_sheep_identity; /* Borrowed identity; World/Chunk own the live Entity. */
    uintptr_t animal_identity; /* Independent animals profile; same real publication ownership. */
} fixture;

/* This continuation uses an existing-player statistic read by the genuine
 * pre-bootstrap CJ constructor. The fresh absent-entry tutorial has its own
 * retained vu.a108 draw dependency and is not a Mob-rendering success. */
static int repeat_statistics_controls(fixture *f, bool initial) {
    mc_registry_achievement_view achievements;
    CHECK(f->app->stats_writer != NULL && f->app->achievement_notice != NULL &&
          mc_registry_classes_inspect_achievements(f->app->registry, &achievements, NULL) ==
              MC_REGISTRY_OK &&
          achievements.named != NULL && achievements.named[0] != NULL);
    const mc_statistic *inventory = achievements.named[0]; /* Actual ep.f. */
    const mc_statistic_fields *fields = mc_statistic_fields_get(inventory);
    CHECK(fields != NULL && fields->e == 5242880 &&
          mc_statistic_kind_get(inventory) == MC_STATISTIC_ACHIEVEMENT);
    bool present = false;
    int32_t value = 0;
    CHECK(mc_client_stats_writer_contains(f->app->stats_writer, inventory, &present, NULL) ==
              MC_CLIENT_STATS_OK &&
          present &&
          mc_client_stats_writer_value(f->app->stats_writer, inventory, &value, NULL) ==
              MC_CLIENT_STATS_OK &&
          value == 1);
    if (initial) {
        /* CJ loads the real unsent Map through xi.a(Map): its c store at7 sets
         * dirty before applying the entries to both actual Maps. */
        mc_client_stats_state state;
        CHECK(mc_client_stats_writer_read_state(f->app->stats_writer, &state) &&
              state.phase == MC_CLIENT_STATS_READY && state.total_count == 1 &&
              state.unsent_count == 1 && state.dirty);
    }
    const mc_client_achievement_notice_fields *notice =
        mc_client_achievement_notice_fields_get(f->app->achievement_notice);
    CHECK(notice != NULL && notice->a == f->app && notice->f == NULL && notice->g == 0 &&
          !notice->i && notice->d == NULL && notice->e == NULL);
    return 0;
}

/* Declared native caller input after a completely returned loaded frame. The
 * saved Chunks still contain no serialized Entities: Mob NBT loading remains
 * a separate dependency. Construct/publish a real Sheep without selecting its
 * private entropy, editing containers, or moving the actual camera/Player. */
static int repeat_sheep_input(fixture *f) {
    mc_world_runtime *world = f->app->active_world;
    mc_entity *player = f->app->i;
    CHECK(world != NULL && player == f->app->h && f->app->r == NULL && !f->state.failed &&
          f->state.render_returned && f->state.world_frame != NULL &&
          f->state.world_frame->returned && f->repeat_sheep_identity == 0 &&
          mc_entity_is_kind(player, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER) &&
          player->base.world == world && player->classes == world->classes->entities &&
          world->fields.r != NULL);

    struct sheep_input_work {
        mc_entity_view_error view;
        mc_mob_error constructor;
        mc_world_light_error height;
        mc_world_entity_error publication;
    } *work = calloc(1, sizeof(*work));

    CHECK(work != NULL);
    const double player_x = player->base.x, player_y = player->base.y, player_z = player->base.z;
    const float player_yaw = player->base.yaw, player_pitch = player->base.aT;
    const mc_java_random world_random = *world->fields.r;
    mc_entity_vector *eye = NULL, *look = NULL;
    CHECK(mc_entity_living_position(player, 1.0f, &eye, &work->view) == MC_ENTITY_VIEW_OK &&
          mc_entity_living_look_current(player, mc_client_bootstrap_math(f->bootstrap), &look,
                                        &work->view) == MC_ENTITY_VIEW_OK &&
          eye != NULL && look != NULL && look->y > -.25 && look->y < .25);
    const double x = eye->x + look->x * 6.0, z = eye->z + look->z * 6.0;
    CHECK(mc_entity_vector_release(player->classes, look) == MC_ENTITY_OK &&
          mc_entity_vector_release(player->classes, eye) == MC_ENTITY_OK);
    const int32_t block_x = mc_math_floor_double(x), block_z = mc_math_floor_double(z);
    int32_t height = 0, air = -1;
    mc_world_storage_error error;
    CHECK(mc_world_runtime_height(world, block_x, block_z, &height, &work->height) ==
              MC_WORLD_STORAGE_OK &&
          height == 64 &&
          mc_world_runtime_block_get(world, block_x, height, block_z, &air, &error) ==
              MC_WORLD_STORAGE_OK &&
          air == 0 &&
          mc_world_runtime_block_get(world, block_x, height + 1, block_z, &air, &error) ==
              MC_WORLD_STORAGE_OK &&
          air == 0);
    const mc_entity_class_literal *literal = NULL;
    mc_entity *sheep = NULL;
    CHECK(mc_entity_class_literal_get(player->classes, MC_ENTITY_CLASS_KIND_SHEEP, &literal) ==
              MC_ENTITY_OK &&
          mc_mob_allocate(player->classes, f->app->registry, literal, &sheep, &work->constructor) ==
              MC_MOB_OK &&
          sheep != NULL && mc_mob_construct(sheep, world, &work->constructor) == MC_MOB_OK &&
          sheep->phase == MC_ENTITY_READY && sheep->classes == player->classes &&
          sheep->class_literal == literal && sheep->base.world == world && sheep->bounds != NULL);
    CHECK(mc_entity_set_position(sheep, x, (double)height, z) == MC_ENTITY_OK &&
          sheep->base.x == x && sheep->base.y == (double)height && sheep->base.z == z &&
          sheep->bounds->min_y == (double)height && sheep->bounds->max_y > sheep->bounds->min_y &&
          sheep->bounds->min_x < x && sheep->bounds->max_x > x && sheep->bounds->min_z < z &&
          sheep->bounds->max_z > z);
    const size_t before = world->entity_membership.count;
    bool inserted = false;
    CHECK(mc_world_runtime_insert_entity(world, sheep, &inserted, &work->publication) ==
              MC_WORLD_STORAGE_OK &&
          inserted && sheep->base.bF && world->fields.b == world->entity_membership.list &&
          world->fields.b->count == before + 1 && world->entity_membership.count == before + 1 &&
          world->fields.b->items[before] == sheep &&
          world->entity_membership.entries[before] == sheep);
    mc_chunk_state *chunk_state = NULL;
    CHECK(mc_world_runtime_get(world, sheep->base.bG, sheep->base.bI, &chunk_state) ==
              MC_CHUNK_ACQUIRE_OK &&
          chunk_state != NULL && !chunk_state->blank);
    mc_world_chunk *chunk = mc_world_store_chunk_object(world->fields.w, chunk_state);
    mc_world_chunk_fields chunk_fields;
    const mc_world_chunk_entity_list *slice = NULL;
    size_t count = 0, occurrences = 0;
    CHECK(chunk != NULL && mc_world_chunk_fields_get(chunk, &chunk_fields) == MC_WORLD_STORAGE_OK &&
          chunk_fields.d == world && chunk_fields.j == sheep->base.bG &&
          chunk_fields.k == sheep->base.bI && chunk_fields.q && sheep->base.bH == 4 &&
          mc_world_chunk_slice_at(chunk_fields.m, (size_t)sheep->base.bH, &slice) ==
              MC_WORLD_STORAGE_OK &&
          mc_world_chunk_entity_count(slice, &count) == MC_WORLD_STORAGE_OK);
    for (size_t index = 0; index < count; ++index) {
        mc_entity *entry = NULL;
        CHECK(mc_world_chunk_entity_at(slice, index, &entry) == MC_WORLD_STORAGE_OK);
        if (entry == sheep)
            ++occurrences;
    }
    CHECK(occurrences == 1 && sheep->holds >= 3 && player->base.x == player_x &&
          player->base.y == player_y && player->base.z == player_z &&
          player->base.yaw == player_yaw && player->base.aT == player_pitch &&
          mc_test_java_random_equal(&world_random, world->fields.r));
    f->repeat_sheep_identity = (uintptr_t)sheep;
    const size_t holds = sheep->holds;
    mc_entity_release(sheep); /* End only this genuine constructor occurrence. */
    sheep = NULL;
    mc_entity *published = world->entity_membership.entries[before];
    CHECK((uintptr_t)published == f->repeat_sheep_identity && published->holds == holds - 1 &&
          published->base.world == world &&
          mc_entity_is_kind(published, MC_ENTITY_CLASS_KIND_SHEEP));
    free(work);
    puts("WORLD_REPEAT_ENTITY input=native-constructed-sheep pose=actual-player-look "
         "distance=6 world-and-chunk=1 same-world-and-classes=1 constructor-returned=1 "
         "constructor-hold-released=1 player-unchanged=1 world-random-unchanged=1");
    return 0;
}

typedef struct repeat_render_control {
    uintptr_t renderer, primary, secondary, parts[12];
    int32_t lists[12];
    size_t frames, entity_frames, delayed_frames, undrawn_frames;
    size_t compile_parts, compile_faces, compile_vertices, reuse_parts;
    size_t shadow_quads;
    bool compiled, reused;
} repeat_render_control;

static mc_entity *repeat_live_sheep(const fixture *f) {
    mc_world_runtime *world = f->app->active_world;
    if (world == NULL || world->fields.b != world->entity_membership.list ||
        world->fields.b->count != world->entity_membership.count)
        return NULL;
    for (size_t i = 0; i < world->entity_membership.count; ++i) {
        mc_entity *entity = world->entity_membership.entries[i];
        if (world->fields.b->items[i] != entity)
            return NULL;
        if ((uintptr_t)entity == f->repeat_sheep_identity)
            return entity;
    }
    return NULL;
}

static int repeat_model_graph(fixture *f, repeat_render_control *control, bool compiled) {
    mc_entity *sheep = repeat_live_sheep(f);
    CHECK(sheep != NULL && sheep->phase == MC_ENTITY_READY && sheep->bounds != NULL &&
          sheep->base.world == f->app->active_world && sheep->classes == f->app->entities &&
          mc_entity_is_kind(sheep, MC_ENTITY_CLASS_KIND_SHEEP) && !sheep->base.be &&
          sheep->base.bF);
    mc_mob_state mob;
    CHECK(mc_mob_inspect(sheep, &mob) && mob.kind == MC_ENTITY_CLASS_KIND_SHEEP &&
          mob.constructor_pc == 19);
    mc_client_manager_state manager;
    CHECK(mc_client_render_manager_inspect(f->app->startup->manager_classes, &manager) &&
          manager.phase == MC_CLIENT_MANAGER_READY && manager.a != NULL &&
          client_renderer_roster(f->app, (mc_client_render_manager *)manager.a, &manager));
    mc_client_managed_renderer *renderer = NULL;
    for (size_t i = 0; i < manager.entries; ++i) {
        const mc_entity_class_literal *key = NULL;
        mc_client_managed_renderer *owner = NULL;
        CHECK(mc_client_render_manager_entry((mc_client_render_manager *)manager.a, i, &key, &owner,
                                             NULL) == MC_CLIENT_MANAGER_OK);
        if (key == sheep->class_literal)
            renderer = owner;
    }
    mc_client_managed_renderer_state state;
    CHECK(renderer != NULL && mc_client_managed_renderer_inspect(renderer, &state) &&
          state.phase == MC_CLIENT_MODEL_READY && state.identity == renderer &&
          state.manager == manager.a && state.source_class != NULL &&
          strcmp(state.source_class, "xy") == 0 && state.primary == renderer->primary &&
          state.secondary == renderer->secondary && renderer->primary != NULL &&
          renderer->secondary != NULL && renderer->primary != renderer->secondary);
    manager_model *models[] = {renderer->primary, renderer->secondary};
    CHECK(models[0]->phase == MC_CLIENT_MODEL_READY && !strcmp(models[0]->source_class, "mw") &&
          models[1]->phase == MC_CLIENT_MODEL_READY && !strcmp(models[1]->source_class, "ea"));
    if (control->renderer == 0) {
        control->renderer = (uintptr_t)renderer;
        control->primary = (uintptr_t)models[0];
        control->secondary = (uintptr_t)models[1];
    }
    CHECK(control->renderer == (uintptr_t)renderer && control->primary == (uintptr_t)models[0] &&
          control->secondary == (uintptr_t)models[1]);
    for (size_t m = 0; m < 2; ++m) {
        mc_client_model_part *parts[] = {models[m]->fields.d, models[m]->fields.e,
                                         models[m]->fields.f, models[m]->fields.g,
                                         models[m]->fields.h, models[m]->fields.i};
        for (size_t i = 0; i < 6; ++i) {
            const size_t index = m * 6u + i;
            mc_client_model_part *part = parts[i];
            CHECK(part != NULL && part->h && !part->i && part->n == compiled);
            if (control->parts[index] == 0)
                control->parts[index] = (uintptr_t)part;
            CHECK(control->parts[index] == (uintptr_t)part);
            for (size_t prior = 0; prior < index; ++prior)
                CHECK(control->parts[prior] != (uintptr_t)part);
            if (compiled) {
                mc_client_source_list_state list;
                CHECK(part->o > 0 &&
                      mc_client_display_list_inspect(f->app->display, part->o, &list) &&
                      list.defined && !list.building && list.draws == 6 && list.vertices == 36);
                /* TZ supplies four vertices per face. Actual NW.b=true copies
                 * two more and records six triangle vertices in each batch. */
                for (size_t face = 0; face < 6; ++face) {
                    mc_client_source_batch_view batch;
                    CHECK(mc_client_display_list_batch(f->app->display, part->o, face, &batch) &&
                          batch.mode == 4 && batch.vertices == 6 && batch.byte_count == 192);
                }
                if (control->lists[index] == 0)
                    control->lists[index] = part->o;
                CHECK(control->lists[index] == part->o);
                for (size_t prior = 0; prior < index; ++prior)
                    CHECK(control->lists[prior] != part->o);
            } else {
                CHECK(part->o == 0);
            }
        }
    }
    if (compiled) {
        CHECK(manager.fields.g == f->app->active_world && manager.living_occurrence == f->app->i &&
              manager.fields.k == f->app->settings && sheep->base.bt > 0);
    }
    return 0;
}

static bool repeat_world_work(const mc_client_menu_loop_state *source) {
    const mc_client_world_tick *call = source->world_tick;
    if (call == NULL)
        return false;
    const mc_world_tick_state *scheduler = &call->scheduler_state;
    const mc_world_random_display_state *display = &call->ambience_state;
    return call->returned && !call->failed && call->site.pc == 1328 && call->renderer_returned &&
           call->render_global_returned && call->entities_returned && scheduler->returned &&
           scheduler->source_pc == 198 && scheduler->weather_returned &&
           scheduler->spawning_returned && scheduler->provider_returned && scheduler->time_stored &&
           scheduler->scheduled_returned && scheduler->random_returned &&
           scheduler->random_chunk_count == 361 &&
           call->scheduler_error.status == MC_WORLD_STORAGE_OK && call->scheduler == NULL &&
           scheduler->random == NULL && call->ambience == NULL &&
           call->ambience_error.status == MC_WORLD_STORAGE_OK && display->returned &&
           display->source_pc == 134 && display->samples_started == 1000 &&
           display->samples_returned == 1000 && display->world_random_calls == 6000 &&
           source->world_tick_returns > 0 && source->random_chunk_calls > 0 &&
           source->last_random_chunk_count == 361 &&
           source->random_chunk_samples == source->random_chunk_calls * UINT64_C(28880) &&
           source->random_display_calls == source->random_chunk_calls &&
           source->random_display_samples == source->random_display_calls * UINT64_C(1000) &&
           source->random_display_random_calls == source->random_display_calls * UINT64_C(6000);
}

static int repeat_returned_frame(fixture *f, repeat_render_control *control) {
    const mc_client_menu_loop_state *source = &f->state;
    const mc_client_world_frame_state *frame = source->world_frame;
    const mc_client_gui_ingame_draw_state *hud = source->hud;
    CHECK(!source->failed && source->running && source->render_returned &&
          source->render_clock_stored && source->world_draws > 0 &&
          source->world_draws == source->hud_draws && frame != NULL && !frame->active &&
          frame->returned && frame->source_pc == 1013 && frame->app == f->app &&
          frame->world == f->app->active_world && frame->owner == f->app->t &&
          frame->camera == f->app->i && frame->render_global == f->app->g &&
          frame->failure != NULL && frame->failure->status == MC_CLIENT_WORLD_FRAME_OK &&
          frame->failure->entities == NULL && frame->failure->hand == NULL);
    CHECK(frame->picking_returned && frame->viewport_set && frame->fog_returned &&
          frame->camera_returned && frame->frustum_returned && frame->sky_returned &&
          frame->updates_returned && frame->terrain_returned && frame->lighting_enabled &&
          frame->eye_position_returned && frame->entities_returned &&
          frame->lit_particles_returned && frame->sprite_particles_returned &&
          frame->translucent_returned && frame->translucent_state_restored &&
          frame->precipitation_returned && frame->clouds_returned && frame->hand_returned &&
          frame->hand != NULL && frame->hand->returned && frame->hand->source_pc == 163 &&
          frame->hand->held_returned && frame->hand->overlays_returned &&
          frame->tail_anaglyph_read && !frame->tail_anaglyph);
    CHECK(hud != NULL && !hud->active && hud->returned && hud->source_pc == 2101 &&
          hud->projection_returned && hud->hotbar_returned && hud->crosshair_returned &&
          hud->stats_returned && hud->slots_returned && hud->empty_slots == 9 &&
          hud->chat_returned && hud->owner == NULL && hud->failure == NULL && hud->cleanup == NULL);
    const mc_client_render_global_entities_state *entities = frame->entities;
    CHECK(entities != NULL && !entities->active && entities->returned && !entities->retiring &&
          entities->draw == NULL && entities->owner == NULL && entities->current == NULL &&
          entities->entities_list == NULL && entities->eye == NULL && entities->frustum == NULL &&
          entities->ll == NULL && entities->th == NULL && entities->failure == NULL &&
          entities->cleanup == NULL);
    ++control->frames;
    if (entities->delayed) {
        CHECK(entities->source_pc == 17 && entities->draws_attempted == 0 &&
              !entities->first_entity_draw.valid && !control->compiled);
        ++control->delayed_frames;
        return 0;
    }
    CHECK(
        entities->source_pc == 569 && entities->ll_returned && entities->th_returned &&
        entities->counters_cleared && entities->origins_stored && entities->origins_written == 6 &&
        entities->entities_visited == f->app->active_world->entity_membership.count &&
        entities->entities_visited >= 2 && entities->distance_checks == entities->entities_visited);
    if (entities->draws_attempted == 0) {
        /* The initial real traversal can cull the unticked Sheep at its zero
         * interpolated pose. It consumes frame budget, never an Entity draw. */
        mc_entity *sheep = repeat_live_sheep(f);
        CHECK(sheep != NULL && sheep->base.bt == 0 && !control->compiled &&
              f->app->active_world->entity_membership.count == 2 &&
              entities->entities_visited == 2 && entities->distance_checks == 2 &&
              entities->frustum_checks == 2 && entities->camera_skips == 1 &&
              entities->presence_checks == 0 && entities->draws_returned == 0 &&
              entities->draws_retired == 0 && entities->entity_draws_returned == 0 &&
              entities->weather_draws_returned == 0 && !entities->first_entity_draw.valid &&
              !entities->last_draw.valid && entities->model_parts_compiled == 0 &&
              entities->model_faces == 0 && entities->model_vertices == 0 &&
              entities->shadow_quads == 0 && f->app->r == NULL && !atomic_load(&f->app->o));
        CHECK(repeat_model_graph(f, control, false) == 0);
        ++control->undrawn_frames;
        return 0;
    }
    CHECK(entities->presence_checks >= 1 && entities->draws_attempted == entities->draws_returned &&
          entities->draws_returned == entities->draws_retired &&
          entities->entity_draws_returned > 0);
    const mc_client_render_global_draw_receipt *draw = &entities->first_entity_draw;
    mc_entity *sheep = repeat_live_sheep(f);
    CHECK(sheep != NULL && repeat_world_work(source) && draw->valid && draw->returned &&
          draw->retired && !draw->post_only && draw->n_invoke_pc == 516 &&
          draw->th_source_pc == 115 && draw->living_source_pc == 769 &&
          draw->post_draw_source_pc == 100 && draw->actor_identity == f->repeat_sheep_identity &&
          draw->actor_class_identity == (uintptr_t)sheep->class_literal &&
          draw->actor_kind == MC_ENTITY_CLASS_KIND_SHEEP && draw->actor_class_name != NULL &&
          !strcmp(draw->actor_class_name, "dl") && draw->renderer_identity == control->renderer &&
          draw->renderer_class_name != NULL && !strcmp(draw->renderer_class_name, "xy") &&
          draw->primary_returned && draw->post_draw_returned && draw->primary_event_returned);
    CHECK(draw->primary_model.identity == control->primary &&
          draw->primary_model.class_name != NULL && !strcmp(draw->primary_model.class_name, "mw") &&
          draw->primary_model.returned && draw->primary_model.source_pc == 67 &&
          draw->primary_model.part_calls == 6 && draw->primary_model.part_returns == 6 &&
          draw->secondary_model.identity == control->secondary &&
          draw->secondary_model.class_name != NULL &&
          !strcmp(draw->secondary_model.class_name, "ea") && draw->secondary_model.returned &&
          draw->secondary_model.source_pc == 67 && draw->secondary_model.part_calls == 6 &&
          draw->secondary_model.part_returns == 6);
    for (size_t i = 0; i < 4; ++i)
        CHECK(draw->pass_attempted[i] && draw->pass_result[i] == (i == 0) &&
              draw->pass_render_returned[i] == (i == 0));
    CHECK(draw->shadow_quads > 0 && draw->shadow_vertices == draw->shadow_quads * 4 &&
          draw->shadow_block_queries > 0 && draw->shadow_light_queries > 0 &&
          draw->shadow_normal_queries >= draw->shadow_quads &&
          draw->shadow_brightness_queries >= draw->shadow_quads);
    if (!control->compiled) {
        CHECK(draw->model_parts_compiled == 12 && draw->primary_model.compiled_parts == 6 &&
              draw->secondary_model.compiled_parts == 6 && draw->model_faces == 72 &&
              draw->model_vertices == 288);
        control->compile_parts = draw->model_parts_compiled;
        control->compile_faces = draw->model_faces;
        control->compile_vertices = draw->model_vertices;
        control->compiled = true;
    } else {
        CHECK(draw->model_parts_compiled == 0 && draw->primary_model.compiled_parts == 0 &&
              draw->secondary_model.compiled_parts == 0 && draw->model_faces == 0 &&
              draw->model_vertices == 0);
        control->reuse_parts = draw->model_parts_compiled;
        control->reused = true;
    }
    CHECK(repeat_model_graph(f, control, true) == 0);
    ++control->entity_frames;
    control->shadow_quads += draw->shadow_quads;
    return 0;
}

static int frame(fixture *f) {
    const bool result = mc_client_menu_loop_frame(f->loop);
    CHECK(mc_client_menu_loop_inspect(f->loop, &f->state));
    if (!result)
        fprintf(stderr, "Reached %s.%s:%" PRId64 ": %s\n",
                f->state.source_class == NULL ? "native" : f->state.source_class,
                f->state.source_method == NULL ? "frame" : f->state.source_method,
                f->state.source_pc,
                f->state.operation == NULL ? "unknown operation" : f->state.operation);
    CHECK(result && !f->state.failed && f->state.running);
    SDL_Delay(1);
    return 0;
}

/* Native caller-input control, separate from ordinary UI load/repeat. Each
 * invocation starts after a completed frame with the declared pause flag. No
 * failed frame is resumed and the actual frame tail recomputes that flag. */
static int light_batch_controls(fixture *f) {
    mc_world_runtime *world = f->app->active_world;
    CHECK(world != NULL && f->app->r == NULL && !atomic_load(&f->app->o) && !f->state.failed &&
          f->state.render_returned && f->state.world_frame != NULL &&
          f->state.world_frame->returned && f->state.world_draws == 1 && f->state.hud_draws == 1 &&
          f->app->g != NULL && f->app->g->fields.k == world && world->fields.r != NULL &&
          world->fields.x != NULL && world->fields.u == world->world_accesses.list &&
          world->world_accesses.count == 1 && world->fields.u->count == 1 &&
          world->world_accesses.entries[0] != NULL &&
          world->fields.u->items[0] == world->world_accesses.entries[0]);
    mc_render_global_access_state access;
    CHECK(mc_render_global_access_inspect(f->app->g->access, &access) &&
          access.registrations == 1 && access.active_callbacks == 0 &&
          access.retained_failures == 0);
    mc_world_access *listener = world->world_accesses.entries[0];
    const uint64_t listener_identity = world->world_accesses.identities[0];
    mc_world_storage_error error;
    mc_world_light_list_view list;
    CHECK(mc_world_runtime_light_list_get(world, &list, &error) == MC_WORLD_STORAGE_OK &&
          list.identity != NULL && list.count == 0 && world->fields.M == 0);
    const mc_world_reference_list *queue = list.identity;
    const mc_world_light_type *type = NULL;
    CHECK(mc_world_runtime_light_type_get(world->classes, MC_LIGHT_BLOCK, &type, &error) ==
              MC_WORLD_STORAGE_OK &&
          type != NULL);

    struct light_work {
        mc_world_light_operation_error operation;
        mc_world_query_error query;
        mc_world_light_error submit;
    } *work = calloc(1, sizeof(*work));

    CHECK(work != NULL);
    int32_t block = -1, saved = -1;
    CHECK(mc_world_runtime_block_get(world, 8, 59, 8, &block, &error) == MC_WORLD_STORAGE_OK &&
          block == 1 &&
          mc_world_runtime_saved_light(world, type, 8, 59, 8, &saved, &work->query) ==
              MC_WORLD_STORAGE_OK &&
          saved == 0);
    CHECK(mc_world_runtime_saved_light_set(world, type, 8, 59, 8, 15, &work->operation) ==
              MC_WORLD_STORAGE_OK &&
          mc_world_runtime_saved_light(world, type, 8, 59, 8, &saved, &work->query) ==
              MC_WORLD_STORAGE_OK &&
          saved == 15);
    const mc_light_bounds bounds = {8, 59, 8, 8, 59, 8};
    const mc_world_light_region *records[500] = {0};
    for (size_t index = 0; index < 500; ++index) {
        mc_light_submit_result submitted = MC_LIGHT_SUBMIT_INVALID;
        CHECK(mc_world_runtime_light_submit(world, type, &bounds, false, &submitted,
                                            &work->submit) == MC_WORLD_STORAGE_OK &&
              submitted == MC_LIGHT_SUBMIT_APPENDED &&
              mc_world_runtime_light_at(world, index, &records[index], &error) ==
                  MC_WORLD_STORAGE_OK);
        const mc_world_light_region_fields *fields =
            mc_world_runtime_light_region_fields_get(records[index]);
        CHECK(fields != NULL && fields->a == type && fields->bounds.min_x == 8 &&
              fields->bounds.max_x == 8 && fields->bounds.min_y == 59 &&
              fields->bounds.max_y == 59 && fields->bounds.min_z == 8 && fields->bounds.max_z == 8);
        for (size_t earlier = 0; earlier < index; ++earlier)
            CHECK(records[earlier] != records[index]);
    }
    CHECK(mc_world_runtime_light_list_get(world, &list, &error) == MC_WORLD_STORAGE_OK &&
          list.identity == queue && list.count == 500);
    int64_t time;
    CHECK(mc_world_info_time_get(world->fields.x, &time) == MC_WORLD_INFO_OK);
    mc_java_random *random_owner = world->fields.r;
    const mc_java_random random = *random_owner;
    const uint64_t chunk_calls = f->state.random_chunk_calls;
    const uint64_t display_calls = f->state.random_display_calls;
    const uint64_t draws = f->state.world_draws;
    const uint64_t hud_draws = f->state.hud_draws;
    for (size_t invocation = 0; invocation < 2; ++invocation) {
        CHECK(f->app->r == NULL && !f->state.failed && f->state.render_returned &&
              f->state.world_frame != NULL && f->state.world_frame->returned &&
              !atomic_load(&f->app->o));
        atomic_store(&f->app->o, true); /* Declared input of this independent paused invocation. */
        CHECK(frame(f) == 0 && f->app->r == NULL && !atomic_load(&f->app->o));
        CHECK(mc_world_runtime_light_list_get(world, &list, &error) == MC_WORLD_STORAGE_OK &&
              list.identity == queue && list.count == (invocation == 0 ? 1u : 0u) &&
              world->fields.M == 0);
        if (invocation == 0) {
            const mc_world_light_region *remaining = NULL;
            CHECK(mc_world_runtime_light_at(world, 0, &remaining, &error) == MC_WORLD_STORAGE_OK &&
                  remaining == records[0]);
        }
        int64_t after_time;
        CHECK(mc_world_runtime_saved_light(world, type, 8, 59, 8, &saved, &work->query) ==
                  MC_WORLD_STORAGE_OK &&
              saved == 0 &&
              mc_world_info_time_get(world->fields.x, &after_time) == MC_WORLD_INFO_OK &&
              after_time == time && world->fields.r == random_owner &&
              mc_test_java_random_equal(&random, random_owner) &&
              f->state.random_chunk_calls == chunk_calls &&
              f->state.random_display_calls == display_calls &&
              f->state.world_draws == draws + invocation + 1 &&
              f->state.hud_draws == hud_draws + invocation + 1 &&
              world->fields.u == world->world_accesses.list && world->fields.u->count == 1 &&
              world->world_accesses.count == 1 && world->world_accesses.entries[0] == listener &&
              world->world_accesses.identities[0] == listener_identity &&
              mc_render_global_access_inspect(f->app->g->access, &access) &&
              access.registrations == 1 && access.active_callbacks == 0 &&
              access.retained_failures == 0);
    }
    free(work);
    puts("WORLD_LIGHT mode=load-light declared-paused-entries=2 completed-frames=2 "
         "queue=500,1,0 same-queue=1 oldest-survives=1 block-light=15,0,0 "
         "world-time-unchanged=1 world-random-unchanged=1 normal-world-access=1 source-batch=499");
    return 0;
}

static int ticks(fixture *f, uint64_t count) {
    const uint64_t target = f->state.ticks + count, deadline = SDL_GetTicks() + 3000;
    while (f->state.ticks < target && SDL_GetTicks() < deadline)
        CHECK(frame(f) == 0);
    CHECK(f->state.ticks >= target);
    return 0;
}

static int display_focus(fixture *f, bool expected) {
    SDL_Window *window = mc_client_display_window(f->app->display);
    if (expected)
        CHECK(SDL_ShowWindow(window) && SDL_RaiseWindow(window));
    const uint64_t deadline = SDL_GetTicks() + 1500;
    do {
        SDL_PumpEvents();
        bool active;
        mc_client_sdk_update_error update;
        CHECK(mc_client_sdk_display_active(mc_client_display_update_owner(f->app->display), &active,
                                           &update) == MC_CLIENT_SDK_UPDATE_OK);
        if (active == expected && (SDL_GetKeyboardFocus() == window) == expected)
            return 0;
        if (expected)
            CHECK(SDL_RaiseWindow(window));
        SDL_Delay(1);
    } while (SDL_GetTicks() < deadline);
    fprintf(stderr, "Focused repeat fixture could not establish actual display focus=%d\n",
            expected);
    return 1;
}

static int transfer(fixture *f) {
    mc_client_events_error e = {0};
    mc_client_events_state before, after;
    mc_client_event_port *events = mc_client_display_event_port(f->app->display);
    CHECK(mc_client_events_inspect(events, &before));
    CHECK(mc_client_events_retain_pending(events, &e) == MC_CLIENT_EVENTS_OK);
    CHECK(mc_client_events_inspect(events, &after));
    CHECK(after.pump_generation == before.pump_generation && after.records > before.records);
    return 0;
}

static int push_mouse(fixture *f, mc_client_source_da *screen, int32_t x, int32_t y, bool down) {
    int width, height;
    SDL_Window *window = mc_client_display_window(f->app->display);
    CHECK(SDL_GetWindowSize(window, &width, &height));
    CHECK(screen->fields.c > 0 && screen->fields.d > 0);
    const float native_x = (float)((int64_t)x * width / screen->fields.c),
                native_y = (float)((int64_t)y * height / screen->fields.d);
    /* LG reads polled Mouse coordinates during drawing. Pushed event records
     * alone do not change SDL's device state; establish the real local pointer
     * position as well as the authored event/button occurrences below. */
    SDL_WarpMouseInWindow(window, native_x, native_y);
    SDL_Event motion = {0};
    motion.type = SDL_EVENT_MOUSE_MOTION;
    motion.motion.windowID = SDL_GetWindowID(window);
    motion.motion.x = native_x;
    motion.motion.y = native_y;
    CHECK(SDL_PushEvent(&motion));
    SDL_Event button = {0};
    button.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
    button.button.windowID = SDL_GetWindowID(window);
    button.button.button = SDL_BUTTON_LEFT;
    button.button.down = down;
    button.button.x = native_x;
    button.button.y = native_y;
    CHECK(SDL_PushEvent(&button));
    return 0;
}

/* Keep the real button state held while moving the native pointer. No
 * additional DOWN/UP record is used to stand in for a scrollbar drag. */
static int move_mouse(fixture *f, mc_client_source_da *screen, int32_t x, int32_t y) {
    int width, height;
    SDL_Window *window = mc_client_display_window(f->app->display);
    CHECK(SDL_GetWindowSize(window, &width, &height));
    CHECK(screen->fields.c > 0 && screen->fields.d > 0);
    const float native_x = (float)((int64_t)x * width / screen->fields.c),
                native_y = (float)((int64_t)y * height / screen->fields.d);
    SDL_WarpMouseInWindow(window, native_x, native_y);
    SDL_Event motion = {0};
    motion.type = SDL_EVENT_MOUSE_MOTION;
    motion.motion.windowID = SDL_GetWindowID(window);
    motion.motion.x = native_x;
    motion.motion.y = native_y;
    CHECK(SDL_PushEvent(&motion));
    return 0;
}

static int push_escape(fixture *f, bool down) {
    SDL_Event key = {0};
    key.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    key.key.windowID = SDL_GetWindowID(mc_client_display_window(f->app->display));
    key.key.scancode = SDL_SCANCODE_ESCAPE;
    key.key.key = SDLK_ESCAPE;
    key.key.down = down;
    CHECK(SDL_PushEvent(&key));
    return 0;
}

static mc_client_source_ke *button(mc_client_source_da *screen, int32_t id) {
    size_t count;
    if (!mc_client_screen_list_size(screen->fields.e, &count))
        return NULL;
    for (size_t i = 0; i < count; ++i) {
        const mc_client_source_ke *candidate = NULL;
        mc_client_screen_ke_state state;
        if (!mc_client_screen_list_button(screen->fields.e, i, &candidate) ||
            mc_client_screen_button_inspect((mc_client_source_ke *)candidate, &state, NULL) !=
                MC_CLIENT_SCREEN_OK)
            return NULL;
        if (state.fields.f == id)
            return (mc_client_source_ke *)candidate;
    }
    return NULL;
}

static int push_button(fixture *f, mc_client_source_da *screen, int32_t id, bool down) {
    mc_client_source_ke *b = button(screen, id);
    CHECK(b != NULL && b->fields.g && b->fields.h);
    return push_mouse(f, screen, mc_java_i32_add(b->fields.c, b->fields.a / 2),
                      mc_java_i32_add(b->fields.d, b->fields.b / 2), down);
}

static int open_selection(fixture *f, mc_client_source_da *initial, mc_client_source_rq **out) {
    const size_t keyboard_before = f->state.keyboard_events;
    CHECK(push_button(f, initial, 1, true) == 0);
    /* FU.a(CI) is empty; RQ's inherited Escape would enter its explicit close
     * dependency. These trailing real keyboard events must drain on FU even
     * after its Singleplayer call synchronously publishes RQ. */
    CHECK(push_escape(f, true) == 0 && push_escape(f, false) == 0 && transfer(f) == 0);
    const uint64_t deadline = SDL_GetTicks() + 3000;
    while ((f->app->r == initial || f->state.keyboard_events < keyboard_before + 2) &&
           SDL_GetTicks() < deadline)
        CHECK(frame(f) == 0);
    CHECK(f->app->r != NULL && f->app->r != initial &&
          f->app->r->kind == MC_CLIENT_SCREEN_WORLD_SELECTION &&
          f->state.keyboard_events >= keyboard_before + 2);
    mc_client_source_rq *rq = (mc_client_source_rq *)f->app->r;
    CHECK(rq->fields.a == initial && rq->fields.m == -1 && rq->fields.o != NULL &&
          rq->fields.o->fields.k == -2.0f);
    bool down = false;
    mc_client_sdk_input_error input;
    CHECK(mc_client_sdk_mouse_button_down(mc_client_display_inputs(f->app->display), 0, &down,
                                          &input) == MC_CLIENT_SDK_INPUT_OK &&
          down);
    CHECK(!rq->fields.t->fields.g && !rq->fields.s->fields.g && !rq->fields.u->fields.g);
    CHECK(ticks(f, 2) == 0 && rq->fields.m == -1 && rq->fields.o->fields.k == -2.0f);
    CHECK(initial->fields.a != NULL && initial->fields.a->fields.f == 1);
    *out = rq;
    return 0;
}

static int release_mouse(fixture *f, mc_client_source_rq *rq) {
    CHECK(push_mouse(f, &rq->base, rq->base.fields.c / 2, 40, false) == 0 && transfer(f) == 0);
    const uint64_t deadline = SDL_GetTicks() + 3000;
    while (rq->fields.o->fields.k != -1.0f && SDL_GetTicks() < deadline)
        CHECK(frame(f) == 0);
    CHECK(rq->fields.o->fields.k == -1.0f && ticks(f, 1) == 0);
    return 0;
}

static int drawing_monitor_recovery(mc_client_source_rq *rq) {
    mc_client_world_selection_draw_error *pending = calloc(1, sizeof(*pending));
    CHECK(pending != NULL);
    CHECK(mc_client_source_world_selection_draw_retire(NULL) == MC_CLIENT_SCREEN_INVALID_ARGUMENT);
    CHECK(mc_client_source_world_selection_draw_retire(pending) == MC_CLIENT_SCREEN_OK);
    /* Prepared native failure receipt: no live drawing call runs here. Avoid
     * destroying a mutex to force an undefined host failure. This control
     * checks the same retained occurrence used by final draw reacquisition. */
    CHECK(mtx_lock(&rq->base.classes->mutex) == thrd_success);
    CHECK(rq->base.active_calls == 0);
    ++rq->base.active_calls;
    pending->pending_receiver = &rq->base;
    CHECK(mtx_unlock(&rq->base.classes->mutex) == thrd_success);
    CHECK(mc_client_source_world_selection_draw_retire(pending) == MC_CLIENT_SCREEN_BUSY &&
          pending->pending_receiver == &rq->base && rq->base.active_calls == 1);
    mc_client_source_da *screen = &rq->base;
    CHECK(mc_client_screen_retire(&screen, NULL) == MC_CLIENT_SCREEN_BUSY && screen == &rq->base);
    const int32_t selection = rq->fields.m;
    const uint64_t stores = rq->store_count;
    pending->monitor_lost = true;
    CHECK(mc_client_source_world_selection_draw_retire(pending) == MC_CLIENT_SCREEN_OK &&
          pending->pending_receiver == NULL && !pending->monitor_lost &&
          rq->base.active_calls == 0 && rq->fields.m == selection && rq->store_count == stores);
    CHECK(mc_client_source_world_selection_draw_retire(pending) == MC_CLIENT_SCREEN_OK);
    free(pending);
    return 0;
}

static int row_press(fixture *f, mc_client_source_rq *rq, bool populated) {
    const mc_client_world_list_fields *list = &rq->fields.o->fields;
    const int32_t y = mc_java_i32_add(
        mc_java_i32_sub(mc_java_i32_add(list->h, 4), mc_java_float_to_i32(list->m)), 8);
    CHECK(y >= list->h && y <= list->i);
    CHECK(push_mouse(f, &rq->base, rq->base.fields.c / 2, y, true) == 0 && transfer(f) == 0);
    CHECK(frame(f) == 0);
    if (populated) {
        const uint64_t deadline = SDL_GetTicks() + 3000;
        while (rq->fields.m != 0 && SDL_GetTicks() < deadline)
            CHECK(frame(f) == 0);
        if (rq->fields.m != 0 || rq->fields.o->fields.n != 0) {
            int32_t sx = 0, sy = 0;
            mc_client_sdk_input_error input_error;
            (void)mc_client_sdk_mouse_x(mc_client_display_inputs(f->app->display), &sx,
                                        &input_error);
            (void)mc_client_sdk_mouse_y(mc_client_display_inputs(f->app->display), &sy,
                                        &input_error);
            fprintf(stderr,
                    "Row state selected=%d previous=%d drag=%g scroll=%g target=%d,%d SDK=%d,%d "
                    "screen=%d,%d display=%d,%d\n",
                    rq->fields.m, rq->fields.o->fields.n, (double)rq->fields.o->fields.k,
                    (double)rq->fields.o->fields.m, rq->base.fields.c / 2, y, sx, sy,
                    rq->base.fields.c, rq->base.fields.d, f->app->d, f->app->e);
        }
        CHECK(rq->fields.m == 0 && rq->fields.o->fields.n == 0);
        CHECK(rq->fields.t->fields.g && rq->fields.s->fields.g && rq->fields.u->fields.g);
    } else {
        CHECK(ticks(f, 1) == 0 && rq->fields.m == -1);
        CHECK(!rq->fields.t->fields.g && !rq->fields.s->fields.g && !rq->fields.u->fields.g);
    }
    CHECK(f->app->active_world == NULL && f->app->h == NULL && f->app->i == NULL &&
          f->app->controller == NULL && !f->app->N);
    return 0;
}

static int capture(fixture *f, const char *path) {
    mc_client_capture pixels = {0};
    mc_client_display_error e;
    CHECK(mc_client_display_capture_rgba(f->app->display, &pixels, &e));
    CHECK(pixels.width == 854 && pixels.height == 480 && pixels.pixels != NULL);
    /* PNG presentation alpha only; the retained native RGB pixels are unchanged. */
    for (size_t y = 0; y < (size_t)pixels.height; ++y)
        for (size_t x = 0; x < (size_t)pixels.width; ++x)
            pixels.pixels[y * pixels.stride + x * 4 + 3] = 255;
    SDL_Surface *surface = SDL_CreateSurfaceFrom(
        pixels.width, pixels.height, SDL_PIXELFORMAT_RGBA32, pixels.pixels, (int)pixels.stride);
    CHECK(surface != NULL);
    const bool saved = SDL_SavePNG(surface, path);
    SDL_DestroySurface(surface);
    mc_client_capture_free(&pixels);
    CHECK(saved);
    return 0;
}

static int world_display_controls(fixture *f, mc_client_screen_classes *classes) {
    mc_client_display_screen_call *call = NULL;
    const bool had_focus = f->app->N;
    CHECK(mc_client_display_screen_run(f->app, classes, NULL, &call) ==
          MC_CLIENT_DISPLAY_SCREEN_OK);
    CHECK(call != NULL && call->source_returned && call->site.pc == 168 && call->health_read &&
          call->player_health > 0 && call->r_stored && call->target == NULL &&
          call->stats_c_returned && call->focus_returned && !call->initialized &&
          f->app->r == NULL);
    if (had_focus && call->display_active)
        CHECK(call->focus_pc == 14 && !call->focus_stored && !call->mouse_grabbed &&
              call->focus_display == NULL);
    else if (!call->display_active)
        CHECK(call->focus_pc == 6 && !call->focus_stored && !call->mouse_grabbed &&
              call->focus_display == NULL);
    else
        CHECK(call->focus_pc == 51 && call->focus_stored && call->mouse_grabbed &&
              call->focus_display != NULL && call->focus_display->source_returned &&
              call->focus_display->health_read && call->focus_display->focus_pc == 14 &&
              call->focus_display->focus_display == NULL && f->app->W == 10000 &&
              f->app->ag == mc_java_i32_add(f->app->V, 10000));
    CHECK(mc_client_display_screen_retire(&call) == MC_CLIENT_DISPLAY_SCREEN_OK && call == NULL);
    mc_entity *player = f->app->h;
    const bool focus = f->app->N;
    f->app->h = NULL;
    const mc_client_display_screen_status status =
        mc_client_display_screen_run(f->app, classes, NULL, &call);
    f->app->h = player;
    CHECK(status == MC_CLIENT_DISPLAY_SCREEN_NULL_REFERENCE && call != NULL &&
          call->site.pc == 76 && call->stats_c_returned && !call->health_read && !call->r_stored &&
          !call->focus_returned && !call->source_returned && f->app->N == focus &&
          f->app->r == NULL && f->app->i == player);
    CHECK(mc_client_display_screen_retire(&call) == MC_CLIENT_DISPLAY_SCREEN_OK && call == NULL);
    return 0;
}

static int gameplay_key(fixture *f, SDL_Scancode code, bool down) {
    SDL_Event event = {0};
    event.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    event.key.windowID = SDL_GetWindowID(mc_client_display_window(f->app->display));
    event.key.scancode = code;
    event.key.key = SDL_GetKeyFromScancode(code, SDL_KMOD_NONE, true);
    event.key.down = down;
    CHECK(SDL_PushEvent(&event));
    return 0;
}

static int gameplay_poll(fixture *f) {
    mc_client_sdk_update_error error;
    CHECK(mc_client_sdk_display_process_messages(mc_client_display_update_owner(f->app->display),
                                                 &error) == MC_CLIENT_SDK_UPDATE_OK);
    return 0;
}

static int gameplay_wheel(fixture *f, float wheel) {
    SDL_Event event = {0};
    event.type = SDL_EVENT_MOUSE_WHEEL;
    event.wheel.windowID = SDL_GetWindowID(mc_client_display_window(f->app->display));
    event.wheel.y = wheel;
    CHECK(SDL_PushEvent(&event));
    return 0;
}

static int gameplay_controls(fixture *f) {
    mc_client_gameplay_input *call = calloc(1, sizeof(*call));
    CHECK(call != NULL);
    mc_local_player_snapshot player;
    mc_movement_input_state movement;
    mc_inventory_player_state slots;
    mc_game_settings_state before, after;
    CHECK(mc_local_player_get_snapshot(f->app->h, &player) == MC_ENTITY_OK &&
          mc_game_settings_read_state(f->app->settings, &before));
    mc_client_sdk_input *input = mc_client_display_inputs(f->app->display);
    mc_client_sdk_input_error input_error;
    bool down;
    CHECK(mc_client_sdk_mouse_button_down(input, 0, &down, &input_error) ==
              MC_CLIENT_SDK_INPUT_OK &&
          !down);
    CHECK(mc_client_sdk_mouse_button_down(input, 1, &down, &input_error) ==
              MC_CLIENT_SDK_INPUT_OK &&
          !down);
    CHECK(gameplay_key(f, SDL_SCANCODE_W, true) == 0 &&
          gameplay_key(f, SDL_SCANCODE_9, true) == 0 &&
          gameplay_key(f, SDL_SCANCODE_9, false) == 0 &&
          gameplay_key(f, SDL_SCANCODE_F1, true) == 0 &&
          gameplay_key(f, SDL_SCANCODE_F1, false) == 0 &&
          gameplay_key(f, SDL_SCANCODE_F3, true) == 0 &&
          gameplay_key(f, SDL_SCANCODE_F3, false) == 0 &&
          gameplay_key(f, SDL_SCANCODE_F5, true) == 0 &&
          gameplay_key(f, SDL_SCANCODE_F5, false) == 0 &&
          gameplay_key(f, SDL_SCANCODE_F8, true) == 0 &&
          gameplay_key(f, SDL_SCANCODE_F8, false) == 0 && gameplay_poll(f) == 0);
    f->app->W = 17;
    CHECK(mc_client_gameplay_input_run(f->app, f->app->screen_transitions, call) &&
          call->returned && !call->failed && call->source_pc == 1073 &&
          call->keyboard_events == 11 && call->hotbar_stores == 1 && f->app->W == 0);
    CHECK(mc_movement_input_read_state(player.input, &movement) && movement.keys[0] &&
          movement.fields.forward == 0.0f); /* Key dispatch does not run lr.a(Player). */
    CHECK(mc_inventory_player_read_state(player.player.c, &slots) && slots.selected == 8);
    CHECK(mc_game_settings_read_state(f->app->settings, &after) &&
          after.values.hide_gui != before.values.hide_gui &&
          after.values.show_debug_info != before.values.show_debug_info &&
          after.values.third_person_view != before.values.third_person_view &&
          after.values.smooth_camera != before.values.smooth_camera);
    CHECK(gameplay_key(f, SDL_SCANCODE_W, false) == 0 && gameplay_poll(f) == 0);
    CHECK(mc_client_gameplay_input_run(f->app, f->app->screen_transitions, call) &&
          call->keyboard_events == 1 && mc_movement_input_read_state(player.input, &movement) &&
          !movement.keys[0]);
    CHECK(gameplay_wheel(f, 1.0f) == 0 && gameplay_poll(f) == 0);
    CHECK(mc_platform_current_time_millis(&f->app->P) == MC_PLATFORM_MILLIS_OK);
    CHECK(mc_client_gameplay_input_run(f->app, f->app->screen_transitions, call) &&
          call->mouse_events >= 1 && call->stale_mouse_events == 0 &&
          mc_inventory_player_read_state(player.player.c, &slots) && slots.selected == 7);
    CHECK(gameplay_wheel(f, -1.0f) == 0 && gameplay_poll(f) == 0);
    f->app->P = 0; /* Actual elapsed-wall-time branch discards the entire event. */
    CHECK(mc_client_gameplay_input_run(f->app, f->app->screen_transitions, call) &&
          call->stale_mouse_events >= 1 &&
          mc_inventory_player_read_state(player.player.c, &slots) && slots.selected == 7);
    CHECK(gameplay_key(f, SDL_SCANCODE_E, true) == 0 &&
          gameplay_key(f, SDL_SCANCODE_3, true) == 0 &&
          gameplay_key(f, SDL_SCANCODE_3, false) == 0 &&
          gameplay_key(f, SDL_SCANCODE_E, false) == 0 && gameplay_poll(f) == 0);
    CHECK(!mc_client_gameplay_input_run(f->app, f->app->screen_transitions, call) && call->failed &&
          call->keyboard_events == 1 && call->site.pc == 778 &&
          mc_inventory_player_read_state(player.player.c, &slots) && slots.selected == 7);
    const char *operation = call->operation;
    CHECK(mc_client_gameplay_input_retire(call) && call->failed && call->site.pc == 778 &&
          call->operation == operation); /* Preserve diagnostics through cleanup. */
    memset(call, 0, sizeof(*call));
    CHECK(mc_client_gameplay_input_run(f->app, f->app->screen_transitions, call) &&
          call->keyboard_events == 3 && call->hotbar_stores == 1 &&
          mc_inventory_player_read_state(player.player.c, &slots) && slots.selected == 2);
    CHECK(mc_client_gameplay_input_retire(call));
    free(call);
    printf("GAMEPLAY_INPUT sdk=1 movement-press-release=1 hotbar=1 toggles=4 wheel=1 "
           "stale-wheel=1 failure-preserves-tail=1 reset=1 returned=1073\n");
    return 0;
}

static int pause_controls(fixture *f) {
    mc_client_screen_error error;
    mc_client_screen_transition_state before, after;
    mc_client_source_da *shown = f->app->r;
    if (shown != NULL) {
        CHECK(shown->kind == MC_CLIENT_SCREEN_PAUSE);
        CHECK(mc_client_screen_transitions_inspect(f->app->screen_transitions, &before) &&
              mc_client_screen_transitions_pause(f->app->screen_transitions, &error) ==
                  MC_CLIENT_SCREEN_OK &&
              mc_client_screen_transitions_inspect(f->app->screen_transitions, &after) &&
              before.screens == after.screens && before.calls == after.calls && f->app->r == shown);
        CHECK(mc_client_screen_transitions_close(f->app->screen_transitions, shown, &error) ==
                  MC_CLIENT_SCREEN_OK &&
              f->app->r == NULL);
    }
    const bool focused = f->app->N;
    f->app->N = false; /* Prepared input segment; the enclosing frame is terminal. */
    mc_client_gameplay_input *input = calloc(1, sizeof(*input));
    CHECK(input != NULL && gameplay_key(f, SDL_SCANCODE_ESCAPE, true) == 0 &&
          gameplay_key(f, SDL_SCANCODE_ESCAPE, false) == 0 && gameplay_poll(f) == 0 &&
          mc_client_gameplay_input_run(f->app, f->app->screen_transitions, input) &&
          input->returned && input->keyboard_events == 2);
    mc_client_source_da *pause = f->app->r;
    CHECK(pause != NULL && pause->kind == MC_CLIENT_SCREEN_PAUSE);
    size_t count;
    CHECK(mc_client_screen_list_size(pause->fields.e, &count) && count == 5);
    mc_client_pause_screen_state state;
    CHECK(mc_client_screen_pause_inspect((mc_client_source_oz *)pause, &state, &error) ==
              MC_CLIENT_SCREEN_OK &&
          state.last_initialization_returned && state.save_pass == 0 && state.ticks == 0);
    CHECK(mc_client_screen_transitions_inspect(f->app->screen_transitions, &before) &&
          mc_client_screen_transitions_pause(f->app->screen_transitions, &error) ==
              MC_CLIENT_SCREEN_OK &&
          mc_client_screen_transitions_inspect(f->app->screen_transitions, &after) &&
          before.screens == after.screens && before.calls == after.calls && f->app->r == pause);
    mc_client_screen_event_error tick;
    CHECK(mc_client_screen_tick(pause, &tick) == MC_CLIENT_SCREEN_OK &&
          mc_client_screen_pause_inspect((mc_client_source_oz *)pause, &state, &error) ==
              MC_CLIENT_SCREEN_OK &&
          state.ticks == 1);
    CHECK(mc_client_screen_transitions_button(f->app->screen_transitions, pause, button(pause, 4),
                                              &error) == MC_CLIENT_SCREEN_OK &&
          f->app->r == NULL);
    mc_client_display_screen_call *release = NULL;
    mc_local_player_snapshot player;
    CHECK(mc_local_player_get_snapshot(f->app->h, &player) == MC_ENTITY_OK &&
          mc_movement_input_retain(player.input, NULL) == MC_MOVEMENT_INPUT_OK);
    CHECK(mc_local_player_set_movement_input(f->app->h, NULL, NULL) == MC_PLAYER_OK);
    f->app->N = true;
    CHECK(mc_client_display_screen_run(f->app, pause->classes, pause, &release) ==
              MC_CLIENT_DISPLAY_SCREEN_NULL_REFERENCE &&
          release != NULL && release->r_stored && !release->player_keys_released &&
          release->child.pc == 4 && strcmp(release->child.class_name, "dc") == 0 && f->app->N);
    CHECK(mc_client_display_screen_retire(&release) == MC_CLIENT_DISPLAY_SCREEN_OK &&
          mc_local_player_set_movement_input(f->app->h, player.input, NULL) == MC_PLAYER_OK);
    mc_movement_input *held_input = (mc_movement_input *)player.input;
    CHECK(mc_movement_input_retire(&held_input, NULL) == MC_MOVEMENT_INPUT_OK);
    CHECK(mc_local_player_input_key(f->app->h, 17, true, NULL) == MC_PLAYER_OK);
    CHECK(mc_client_display_screen_run(f->app, pause->classes, pause, &release) ==
              MC_CLIENT_DISPLAY_SCREEN_NULL_REFERENCE &&
          release != NULL && release->r_stored && release->player_keys_released && !f->app->N &&
          release->mouse.site.pc == 4 && !release->mouse.native_policy);
    mc_movement_input_state movement;
    CHECK(mc_movement_input_read_state(player.input, &movement) && !movement.keys[0]);
    CHECK(mc_client_display_screen_retire(&release) == MC_CLIENT_DISPLAY_SCREEN_OK &&
          mc_client_screen_transitions_close(f->app->screen_transitions, pause, &error) ==
              MC_CLIENT_SCREEN_OK &&
          f->app->r == NULL);
    CHECK(mc_client_gameplay_input_retire(input));
    free(input);
    f->app->N = focused;
    puts("PAUSE_SCREEN escape=1 init-buttons=5 nonempty-entry=1 tick=1 back-to-game=1 "
         "null-input-prefix=1 key-reset-before-null-Canvas=1");
    return 0;
}

static int world_tick_controls(fixture *f) {
    mc_client_world_tick *call = calloc(1, sizeof(*call));
    CHECK(call != NULL);
    const bool paused = atomic_load(&f->app->o);
    const int32_t counter = f->app->ah;
    atomic_store(&f->app->o, true);
    f->app->ah = 29;
    CHECK(mc_client_world_tick_run(f->app, mc_client_bootstrap_math(f->bootstrap), call) &&
          call->returned && call->counter_stored && call->counter_reset && f->app->ah == 0 &&
          call->refresh_state.returned && call->refresh_state.chunk_returns == 25 &&
          call->refresh_state.contains && !call->refresh_state.appended &&
          !call->renderer_returned && !call->entities_returned);
    CHECK(mc_client_world_tick_cleanup(call));
    atomic_store(&f->app->o, false);
    memset(call, 0, sizeof(*call));
    const bool returned =
        mc_client_world_tick_run(f->app, mc_client_bootstrap_math(f->bootstrap), call);
    if (!returned && call->site.pc != 80) {
        fprintf(stderr, "World segment reached %s.%s:%d: %s; renderer=%d entities=%d\n",
                call->site.class_name, call->site.method, call->site.pc, call->operation,
                call->renderer_returned, call->entities_returned);
        if (call->site.pc == 1216)
            fprintf(stderr, "Entity child %s.%s:%u kind=%d op=%s\n", call->entities.source_class,
                    call->entities.source_method, call->entities.bytecode,
                    (int)call->entities.child, call->entities.operation);
    }
    const bool ambience = call->scheduler_state.returned;
    CHECK((ambience ? returned && call->returned && !call->failed &&
                          strcmp(call->site.class_name, "net/minecraft/client/Minecraft") == 0 &&
                          strcmp(call->site.method, "k") == 0 && call->site.pc == 1328 &&
                          call->ambience_arguments == 3 && call->ambience != NULL &&
                          call->ambience_state.returned && call->ambience_state.source_pc == 134 &&
                          call->ambience_state.local_random_constructed &&
                          call->ambience_state.samples_started == 1000 &&
                          call->ambience_state.samples_returned == 1000 &&
                          call->ambience_state.world_random_calls == 6000 &&
                          call->ambience_error.status == MC_WORLD_STORAGE_OK &&
                          call->effects.status == MC_EFFECT_RENDERER_OK
                    : !returned && call->failed && strcmp(call->site.class_name, "fd") == 0 &&
                          call->site.pc == 80) &&
          call->counter_stored && !call->counter_reset && call->difficulty_stored &&
          call->renderer_returned && call->render_global_returned && call->entities_returned &&
          call->spawn_flags_stored && call->renderer_state.source_returned &&
          call->renderer_state.source_pc == 186 && call->renderer_state.interpolation_stores == 6 &&
          call->renderer_state.brightness_returned && call->renderer_state.held_item_returned &&
          call->renderer_state.rain_returned && call->renderer_state.rain->returned);
    if (!world_dependency_reached(&call->scheduler_state, &call->scheduler_error))
        fprintf(stderr, "Spawn child=%u descendant=%u operation=%s\n",
                call->scheduler_error.child_bytecode, call->scheduler_error.descendant_bytecode,
                call->scheduler_error.operation);
    CHECK(call->scheduler_state.weather_returned &&
          call->scheduler_state.spawn_chunk_count == 289 &&
          world_dependency_reached(&call->scheduler_state, &call->scheduler_error) &&
          call->scheduler_state.spawn_pick.entry != NULL &&
          call->scheduler_state.spawn_candidates_visited > 0 &&
          call->scheduler_state.spawn_random_calls >= 10 &&
          call->scheduler_state.spawn_pick.total_weight > 0);
    const unsigned int spawn_pc = call->scheduler_error.child_bytecode;
    const unsigned int world_pc = call->scheduler_state.source_pc;
    const int32_t next_pc = call->site.pc;
    const size_t samples =
        call->scheduler_state.random == NULL ? 0 : call->scheduler_state.random->samples_returned;
    const size_t display_samples = call->ambience_state.samples_returned;
    const unsigned int display_pc = call->ambience_state.source_pc;
    CHECK(mc_client_world_tick_cleanup(call) && call->failed == !ambience &&
          call->scheduler_error.mob == NULL && call->scheduler_state.spawn_entity == NULL &&
          call->ambience == NULL && call->ambience_state.source_random == NULL &&
          call->ambience_state.world == NULL && call->ambience_error.block == NULL);
    atomic_store(&f->app->o, paused);
    f->app->ah = counter;
    free(call);
    printf("WORLD_TICK paused-refresh25=1 unpaused-renderer186=1 render-global=1 "
           "entities=1 spawn-flags=1 weather=1 spawn-candidates=289 scheduler=%u samples=%zu "
           "display=%u display-samples=%zu next=%d child=%u\n",
           world_pc, samples, display_pc, display_samples, next_pc, spawn_pc);
    return 0;
}

static int large_drag(fixture *f, mc_client_source_rq *rq, const char *capture_path) {
    mc_client_world_list_fields *list = &rq->fields.o->fields;
    CHECK(mc_client_save_list_size(rq->fields.n) == 16 && list->m == 0.0f && list->k == -1.0f);
    const int32_t x = list->b / 2 + 127, upper = list->h + 8, lower = list->i - 8;
    const int32_t maximum = 16 * 36 - (list->i - list->h - 4);
    CHECK(maximum > 0 && lower > upper);
    CHECK(push_mouse(f, &rq->base, x, upper, true) == 0 && transfer(f) == 0);
    const uint64_t start_deadline = SDL_GetTicks() + 3000;
    while (list->k < 0 && SDL_GetTicks() < start_deadline)
        CHECK(frame(f) == 0);
    CHECK(list->k >= 0 && list->l < 0.0f && list->m == 0.0f);
    bool down = false;
    mc_client_sdk_input_error input;
    CHECK(mc_client_sdk_mouse_button_down(mc_client_display_inputs(f->app->display), 0, &down,
                                          &input) == MC_CLIENT_SDK_INPUT_OK &&
          down);
    CHECK(move_mouse(f, &rq->base, x, lower) == 0 && transfer(f) == 0);
    const uint64_t down_deadline = SDL_GetTicks() + 3000;
    while (list->m != (float)maximum && SDL_GetTicks() < down_deadline)
        CHECK(frame(f) == 0);
    CHECK(list->m == (float)maximum && list->k >= 0 && rq->fields.m == -1);
    CHECK(list->n == -1 && list->o == 0 && !rq->fields.t->fields.g && !rq->fields.s->fields.g &&
          !rq->fields.u->fields.g);
    CHECK(capture(f, capture_path) == 0);
    CHECK(mc_client_sdk_mouse_button_down(mc_client_display_inputs(f->app->display), 0, &down,
                                          &input) == MC_CLIENT_SDK_INPUT_OK &&
          down);
    CHECK(move_mouse(f, &rq->base, x, upper) == 0 && transfer(f) == 0);
    const uint64_t up_deadline = SDL_GetTicks() + 3000;
    while (list->m != 0.0f && SDL_GetTicks() < up_deadline)
        CHECK(frame(f) == 0);
    CHECK(list->m == 0.0f && list->k >= 0 && rq->fields.m == -1 && list->n == -1 && list->o == 0);
    CHECK(release_mouse(f, rq) == 0 && list->k == -1.0f);
    printf("WORLD_SELECTION drag-count=16 bottom=%d restored=0 button-held=1\n", maximum);
    return 0;
}

static int release_for_double(fixture *f, mc_client_source_rq *rq) {
    mc_client_world_list_fields *list = &rq->fields.o->fields;
    const int32_t y = list->h + 4 - mc_java_float_to_i32(list->m) + 8;
    CHECK(push_mouse(f, &rq->base, rq->base.fields.c / 2, y, false) == 0 && transfer(f) == 0);
    const uint64_t deadline = SDL_GetTicks() + 3000;
    while (list->k != -1.0f && SDL_GetTicks() < deadline)
        CHECK(frame(f) == 0);
    CHECK(list->k == -1.0f);
    return 0;
}

static bool filename_is(const mc_client_save_selection *selection, const char *ascii) {
    mc_client_save_selection_fields fields;
    if (mc_client_save_selection_read(selection, &fields, NULL) != MC_CLIENT_SAVE_OK ||
        fields.filename == NULL)
        return false;
    mc_nbt_string view = mc_world_info_name_text(fields.filename);
    if (view.length != strlen(ascii))
        return false;
    for (size_t i = 0; i < view.length; ++i)
        if (view.units[i] != (uint8_t)ascii[i])
            return false;
    return true;
}

/* Only the non-loading modes use this prefix. A prepared menu still owns its
 * real screens/App/services; it has not admitted full loaded-World shutdown. */
static int terminal_prepare_controls(fixture *f, mc_client_source_da *external) {
    mc_client_menu_loop_state before;
    mc_client_screen_transition_state transitions_before;
    mc_client_screen_state retained_before, shown_before;
    client_application *app = f->app;
    mc_client_source_da *shown = app->r;
    mc_client_display *display = app->display;
    SDL_Window *window = mc_client_display_window(display);
    mc_client_screen_transitions *transitions = app->screen_transitions;
    mc_client_application_startup *startup = app->startup;
    mc_client_render_engine *engine = app->render_engine;
    mc_client_bitmap_font *font = app->font_renderer;
    mc_client_loading_screen *loading = app->loading_screen;
    mc_game_settings *settings = app->settings;
    mc_sound_manager *sound = app->B;
    mc_client_render_global *renderer = app->g;
    mc_client_source_px *entity_renderer = app->t;
    mc_client_singleplayer_controller *controller = app->controller;
    mc_client_render_classes *models = app->render_classes;
    mc_registry_classes *registry = app->registry;
    mc_entity_classes *entities = app->entities;
    mc_java_random_profile *profile = app->profile;
    const mc_registry_resources *resources = mc_client_bootstrap_resources(f->bootstrap);
    mc_resource_files *files = mc_client_bootstrap_class_files(f->bootstrap);
    mc_math_classes *math = mc_client_bootstrap_math(f->bootstrap);
    mc_client_render_engine_classes *graphics = mc_client_bootstrap_graphics(f->bootstrap);
    const bool source_running = atomic_load(&app->J), source_paused = atomic_load(&app->o);
    CHECK(display != NULL && window != NULL && shown != NULL && external != NULL &&
          startup != NULL && engine != NULL && font != NULL && loading != NULL &&
          settings != NULL && resources != NULL && files != NULL && math != NULL &&
          graphics != NULL);
    mc_client_tile_renderer_classes *absent_tiles = NULL;
    CHECK(mc_client_tile_entity_renderer_classes_existing(app->render_classes, app->registry,
                                                          app->world_classes, &absent_tiles,
                                                          NULL) == MC_CLIENT_TILE_RENDERER_OK &&
          absent_tiles == NULL);
    const SDL_WindowID window_id = SDL_GetWindowID(window);
    int width, height;
    CHECK(window_id != 0 && SDL_GetWindowSize(window, &width, &height) && width > 0 && height > 0);
    CHECK(mc_client_menu_loop_inspect(f->loop, &before) && !before.terminal_prepare_selected &&
          !before.terminal_prepared);
    CHECK(mc_client_screen_transitions_inspect(transitions, &transitions_before) &&
          !transitions_before.active && !transitions_before.retiring &&
          !transitions_before.terminal_prepare_selected && !transitions_before.terminal_prepared);
    CHECK(mc_client_screen_inspect(external, &retained_before, NULL) == MC_CLIENT_SCREEN_OK &&
          retained_before.phase == MC_CLIENT_SCREEN_READY && retained_before.holds >= 2 &&
          mc_client_screen_inspect(shown, &shown_before, NULL) == MC_CLIENT_SCREEN_OK &&
          shown_before.phase == MC_CLIENT_SCREEN_READY);
    mc_client_world_load_state load_before = {0};
    mc_client_world_load_status source_failure = MC_CLIENT_WORLD_LOAD_OK;
    int source_failure_pc = 0;
    if (transitions_before.world_load != NULL) {
        CHECK(mc_client_world_load_inspect(transitions_before.world_load, &load_before) &&
              !load_before.active && !load_before.retiring && !load_before.source_returned &&
              !load_before.terminal_prepare_selected && !load_before.terminal_prepared &&
              load_before.needs_conversion && load_before.conversion_checked &&
              !load_before.handler_opened && load_before.handler == NULL &&
              load_before.world == NULL && load_before.player == NULL &&
              load_before.input == NULL && load_before.failure != NULL &&
              load_before.failure->status == MC_CLIENT_WORLD_LOAD_UNIMPLEMENTED);
        source_failure = load_before.failure->status;
        source_failure_pc = load_before.failure->site.pc;
    }
    CHECK(!mc_client_menu_loop_terminal_prepare(NULL));
    for (unsigned int attempt = 0; attempt < 2; ++attempt) {
        CHECK(mc_client_menu_loop_terminal_prepare(f->loop));
        mc_client_screen_transition_state prepared;
        mc_client_screen_state retained, current;
        CHECK(mc_client_menu_loop_inspect(f->loop, &f->state) &&
              f->state.terminal_prepare_selected && f->state.terminal_prepared &&
              f->state.failed == before.failed && f->state.running == before.running &&
              f->state.source_class == before.source_class &&
              f->state.source_method == before.source_method &&
              f->state.source_pc == before.source_pc);
        CHECK(mc_client_screen_transitions_inspect(transitions, &prepared) && !prepared.active &&
              prepared.retiring && prepared.terminal_prepare_selected &&
              prepared.terminal_prepared && prepared.screens == transitions_before.screens &&
              prepared.calls == transitions_before.calls &&
              prepared.initial_screen == transitions_before.initial_screen &&
              prepared.slot_handed_off == transitions_before.slot_handed_off &&
              prepared.world_load == transitions_before.world_load);
        CHECK(mc_client_bootstrap_app(f->bootstrap) == app && f->app == app &&
              app->native_lifetime_phase == MC_CLIENT_APPLICATION_LIFETIME_REGISTERED &&
              app->constructor_phase == MC_CLIENT_APP_READY && app->startup == startup &&
              app->screen_transitions == transitions && app->r == shown &&
              app->display == display && app->render_engine == engine &&
              app->font_renderer == font && app->loading_screen == loading &&
              app->settings == settings && app->B == sound && app->g == renderer &&
              app->t == entity_renderer && app->controller == controller &&
              app->render_classes == models && app->registry == registry &&
              app->entities == entities && app->profile == profile &&
              mc_client_bootstrap_resources(f->bootstrap) == resources &&
              mc_client_bootstrap_class_files(f->bootstrap) == files &&
              mc_client_bootstrap_math(f->bootstrap) == math &&
              mc_client_bootstrap_graphics(f->bootstrap) == graphics &&
              atomic_load(&app->J) == source_running && atomic_load(&app->o) == source_paused &&
              app->active_world == NULL && app->h == NULL && app->i == NULL);
        CHECK(mc_client_display_window(display) == window && SDL_GetWindowID(window) == window_id &&
              SDL_GetWindowSize(window, &width, &height) && width > 0 && height > 0);
        CHECK(mc_client_screen_inspect(external, &retained, NULL) == MC_CLIENT_SCREEN_OK &&
              retained.phase == retained_before.phase && retained.kind == retained_before.kind &&
              retained.holds >= 2 &&
              mc_client_screen_inspect(shown, &current, NULL) == MC_CLIENT_SCREEN_OK &&
              current.phase == shown_before.phase && current.kind == shown_before.kind);
        if (prepared.world_load != NULL) {
            /* Inspect fresh after preparation; retired child views may expire.
             * The failed conversion call itself and its source outcome remain. */
            mc_client_world_load_state load;
            CHECK(mc_client_world_load_inspect(prepared.world_load, &load) && !load.active &&
                  load.retiring && load.terminal_prepare_selected && load.terminal_prepared &&
                  !load.source_returned && load.needs_conversion && load.conversion_checked &&
                  !load.handler_opened && !load.world_constructed && !load.world_published &&
                  !load.player_published && !load.input_published && load.handler == NULL &&
                  load.world == NULL && load.player == NULL && load.input == NULL &&
                  load.app == app && load.folder == load_before.folder &&
                  load.display == load_before.display && load.seed == load_before.seed &&
                  load.gc_requests == load_before.gc_requests &&
                  load.last_outer_pc == load_before.last_outer_pc &&
                  load.last_change_pc == load_before.last_change_pc && load.failure != NULL &&
                  load.failure->status == source_failure &&
                  load.failure->site.pc == source_failure_pc && load.cleanup != NULL &&
                  load.cleanup->status == MC_CLIENT_WORLD_LOAD_OK);
        }
        CHECK(!mc_client_menu_loop_frame(f->loop));
        CHECK(mc_client_menu_loop_inspect(f->loop, &f->state) &&
              f->state.terminal_prepare_selected && f->state.terminal_prepared &&
              f->state.frames == before.frames && f->state.ticks == before.ticks &&
              f->state.texture_ticks == before.texture_ticks &&
              f->state.mouse_events == before.mouse_events &&
              f->state.keyboard_events == before.keyboard_events &&
              f->state.source_pc == before.source_pc);
    }
    return 0;
}

static int loaded_admission_attempt(fixture *f, mc_world_storage_status expected,
                                    const char *operation, mc_world_terminal_inventory *success) {
    client_application *app = f->app;
    mc_world_runtime *world = app->active_world;
    mc_entity *player = app->h;
    mc_world_source_state handler;
    mc_world_source_chunk_loader_state loader;
    mc_world_saved_data_source_state saved_data;
    CHECK(world->source_construction != NULL &&
          mc_world_store_source_inspect(world->fields.w, &handler) &&
          mc_world_source_chunk_loader_inspect(world->source_construction->loader, &loader) &&
          mc_world_saved_data_source_inspect(world->fields.z, &saved_data));

    struct owner_copy {
        const void *owner;
        size_t bytes;
        unsigned char *before;
    } copies[] = {{app, sizeof(*app), NULL},
                  {world, sizeof(*world), NULL},
                  {player, sizeof(*player), NULL},
                  {world->source_construction, sizeof(*world->source_construction), NULL},
                  {handler.failure, sizeof(*handler.failure), NULL},
                  {handler.cleanup, sizeof(*handler.cleanup), NULL},
                  {loader.failure, sizeof(*loader.failure), NULL},
                  {loader.cleanup, sizeof(*loader.cleanup), NULL},
                  {saved_data.failure, sizeof(*saved_data.failure), NULL},
                  {saved_data.cleanup, sizeof(*saved_data.cleanup), NULL}};

    for (size_t i = 0; i < sizeof(copies) / sizeof(copies[0]); ++i) {
        CHECK(copies[i].owner != NULL);
        copies[i].before = malloc(copies[i].bytes);
        CHECK(copies[i].before != NULL);
        memcpy(copies[i].before, copies[i].owner, copies[i].bytes);
    }
    mc_world_terminal_inventory inventory = {.entities = 101,
                                             .tile_entities = 102,
                                             .entity_occurrences = 103,
                                             .tile_entity_occurrences = 104,
                                             .expected_world_bindings = 105,
                                             .piston_discard_required = true};
    unsigned char sentinel[sizeof(inventory)];
    memcpy(sentinel, &inventory, sizeof(inventory));
    mc_world_storage_error error = {0};
    const mc_world_storage_status admitted = mc_client_screen_transitions_terminal_admit(
        app->screen_transitions, NULL, &inventory, &error);
    if (admitted != expected)
        fprintf(stderr, "Terminal admission got%d expected%d: %s\n", (int)admitted, (int)expected,
                error.operation == NULL ? "none" : error.operation);
    CHECK(admitted == expected && error.status == expected);
    for (size_t i = 0; i < sizeof(copies) / sizeof(copies[0]); ++i)
        CHECK(memcmp(copies[i].before, copies[i].owner, copies[i].bytes) == 0);
    if (expected != MC_WORLD_STORAGE_OK)
        CHECK(memcmp(sentinel, &inventory, sizeof(inventory)) == 0);
    if (operation != NULL && (error.operation == NULL || strcmp(error.operation, operation) != 0))
        fprintf(stderr, "Terminal admission operation %s expected %s\n",
                error.operation == NULL ? "none" : error.operation, operation);
    if (operation != NULL)
        CHECK(error.operation != NULL && strcmp(error.operation, operation) == 0);
    if (expected == MC_WORLD_STORAGE_OK && success != NULL)
        *success = inventory;
    for (size_t i = 0; i < sizeof(copies) / sizeof(copies[0]); ++i)
        free(copies[i].before);
    return 0;
}

static int terminal_source_diagnostic_same(fixture *f, const mc_client_menu_loop_state *before) {
    CHECK(mc_client_menu_loop_inspect(f->loop, &f->state) && f->state.failed == before->failed &&
          f->state.running == before->running && f->state.operation == before->operation &&
          f->state.source_class == before->source_class &&
          f->state.source_method == before->source_method &&
          f->state.source_pc == before->source_pc && f->state.frames == before->frames &&
          f->state.ticks == before->ticks && f->state.texture_ticks == before->texture_ticks &&
          f->state.account_checks_omitted == before->account_checks_omitted &&
          f->state.font_measurements == before->font_measurements &&
          f->state.mouse_events == before->mouse_events &&
          f->state.keyboard_events == before->keyboard_events);
    return 0;
}

static int loaded_terminal_admission_controls(fixture *f) {
    client_application *app = f->app;
    mc_world_runtime *world = app->active_world;
    mc_entity *player = app->h;
    mc_client_screen_transition_state transitions;
    mc_client_world_load_state load;
    mc_client_menu_loop_state source;
    CHECK(mc_client_menu_loop_inspect(f->loop, &source) && loaded_draw_reached(&source) &&
          !source.terminal_tail_entered);
    CHECK(mc_client_screen_transitions_inspect(app->screen_transitions, &transitions) &&
          transitions.world_load != NULL &&
          mc_client_world_load_inspect(transitions.world_load, &load) && load.source_returned &&
          load.world == world && load.player == player && load.input != NULL);
    mc_client_tile_renderer_classes *present_tiles = NULL;
    mc_client_tile_renderer_state tiles_state;
    CHECK(mc_client_tile_entity_renderer_classes_existing(app->render_classes, app->registry,
                                                          app->world_classes, &present_tiles,
                                                          NULL) == MC_CLIENT_TILE_RENDERER_OK &&
          present_tiles != NULL && mc_client_tile_renderer_inspect(present_tiles, &tiles_state) &&
          tiles_state.source_views_available && tiles_state.fields.f == world &&
          tiles_state.fields.g == player && tiles_state.native_consumers == 0);
    CHECK(loaded_admission_attempt(f, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                   "screen terminal admission prepared selected-load scope",
                                   NULL) == 0);
    CHECK(mc_client_menu_loop_terminal_prepare(f->loop));
    CHECK(loaded_admission_attempt(f, MC_WORLD_STORAGE_BUSY,
                                   "World terminal external Entity or TileEntity hold", NULL) == 0);

    /* Four derived Handler occurrences: actual Load, World sidecar, HC and ld
     * slots. Retaining ld itself adds no fifth Handler occurrence. */
    mc_world_source_state handler;
    CHECK(mc_world_store_source_inspect(load.handler, &handler) && handler.holds == 4);
    CHECK(mc_world_store_source_retain(load.handler, NULL) == MC_WORLD_SOURCE_OK &&
          mc_world_store_source_inspect(load.handler, &handler) && handler.holds == 5);
    mc_world_store *extra_handler = load.handler;
    CHECK(loaded_admission_attempt(f, MC_WORLD_STORAGE_BUSY,
                                   "Store terminal source external Handler occurrence", NULL) == 0);
    CHECK(mc_world_store_source_release(&extra_handler, NULL) == MC_WORLD_SOURCE_OK &&
          extra_handler == NULL);
    mc_world_source_chunk_loader *owned_loader = mc_world_runtime_source_loader(world);
    CHECK(owned_loader != NULL &&
          mc_world_source_chunk_loader_retain(owned_loader, NULL) == MC_WORLD_SOURCE_OK &&
          mc_world_store_source_inspect(load.handler, &handler) && handler.holds == 4);
    mc_world_source_chunk_loader *extra_loader_hold = owned_loader;
    CHECK(loaded_admission_attempt(f, MC_WORLD_STORAGE_BUSY,
                                   "World terminal external loader occurrence", NULL) == 0);
    CHECK(mc_world_source_chunk_loader_release(&extra_loader_hold, NULL) == MC_WORLD_SOURCE_OK &&
          extra_loader_hold == NULL);
    mc_world_saved_data *extra_saved_data = NULL;
    CHECK(mc_world_saved_data_source_allocate(MC_WORLD_INFO_CLIENT, NULL, &extra_saved_data,
                                              NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_saved_data_source_construct(extra_saved_data, load.handler, NULL) ==
              MC_WORLD_STORAGE_OK &&
          mc_world_store_source_inspect(load.handler, &handler) && handler.holds == 5);
    CHECK(loaded_admission_attempt(f, MC_WORLD_STORAGE_BUSY,
                                   "Store terminal source external Handler occurrence", NULL) == 0);
    CHECK(mc_world_saved_data_retire(&extra_saved_data, NULL) == MC_WORLD_STORAGE_OK &&
          extra_saved_data == NULL);
    mc_world_source_chunk_loader *extra_loader = NULL;
    CHECK(mc_world_source_chunk_loader_allocate(load.handler,
                                                mc_world_store_source_regions(load.handler),
                                                &extra_loader, NULL) == MC_WORLD_SOURCE_OK &&
          mc_world_store_source_inspect(load.handler, &handler) && handler.holds == 5);
    CHECK(loaded_admission_attempt(f, MC_WORLD_STORAGE_BUSY,
                                   "Store terminal source external Handler occurrence", NULL) == 0);
    CHECK(mc_world_source_chunk_loader_retire(&extra_loader, NULL) == MC_WORLD_SOURCE_OK &&
          extra_loader == NULL && mc_world_store_source_inspect(load.handler, &handler) &&
          handler.holds == 4);
    CHECK(loaded_admission_attempt(f, MC_WORLD_STORAGE_BUSY,
                                   "World terminal external Entity or TileEntity hold", NULL) == 0);

    CHECK(mc_movement_input_retain(load.input, NULL) == MC_MOVEMENT_INPUT_OK);
    mc_movement_input *extra_input = load.input;
    CHECK(loaded_admission_attempt(f, MC_WORLD_STORAGE_BUSY,
                                   "Load terminal external Input occurrence", NULL) == 0);
    CHECK(mc_movement_input_retire(&extra_input, NULL) == MC_MOVEMENT_INPUT_OK &&
          extra_input == NULL);
    CHECK(mc_client_singleplayer_controller_retain(app->controller) ==
          MC_SINGLEPLAYER_CONTROLLER_OK);
    CHECK(loaded_admission_attempt(f, MC_WORLD_STORAGE_BUSY,
                                   "Load terminal external controller occurrence", NULL) == 0);
    CHECK(mc_client_singleplayer_controller_release(app->controller) ==
          MC_SINGLEPLAYER_CONTROLLER_OK);
    CHECK(mc_client_effect_renderer_retain(app->effect_renderer) == MC_EFFECT_RENDERER_OK);
    CHECK(loaded_admission_attempt(f, MC_WORLD_STORAGE_BUSY,
                                   "Load terminal external effect occurrence", NULL) == 0);
    CHECK(mc_client_effect_renderer_release(app->effect_renderer) == MC_EFFECT_RENDERER_OK);

    /* One App plus a genuine independent native consumer, not a second App. */
    mc_client_manager_consumer foreign = {0};
    CHECK(mc_client_render_manager_consumer_attach(app->startup->manager_classes, &foreign, NULL) ==
          MC_CLIENT_MANAGER_OK);
    CHECK(mc_client_bootstrap_terminal_dispatchers(f->bootstrap) == MC_CLIENT_BOOTSTRAP_BUSY &&
          foreign.attached && app->active_world == world && app->h == player && app->i == player &&
          world->construction.phase == MC_WORLD_RUNTIME_READY &&
          !world->entity_containers_retired && !world->tile_entity_containers_retired);
    mc_client_menu_loop *same_loop = f->loop;
    CHECK(!mc_client_menu_loop_terminal_retire(&f->loop) && f->loop == same_loop &&
          terminal_source_diagnostic_same(f, &source) == 0 && foreign.attached &&
          f->state.terminal_prepared && !f->state.terminal_tail_entered &&
          f->state.retirement_bootstrap == MC_CLIENT_BOOTSTRAP_BUSY &&
          f->state.retirement_world == NULL && f->state.retirement_operation != NULL &&
          app->active_world == world && app->h == player && app->i == player &&
          world->construction.phase == MC_WORLD_RUNTIME_READY &&
          !world->entity_containers_retired && !world->tile_entity_containers_retired);
    /* Close may end borrowed getters while the transition lease keeps the
     * exact App/startup alive. The coordinator must still resume this owner. */
    mc_client_bootstrap *same_bootstrap = f->bootstrap;
    CHECK(mc_client_bootstrap_close(&f->bootstrap) == MC_CLIENT_BOOTSTRAP_BUSY &&
          f->bootstrap == same_bootstrap && mc_client_bootstrap_app(f->bootstrap) == NULL &&
          app->screen_transitions != NULL && app->active_world == world && app->h == player &&
          app->i == player && terminal_source_diagnostic_same(f, &source) == 0);
    CHECK(!mc_client_menu_loop_terminal_retire(&f->loop) && f->loop == same_loop &&
          terminal_source_diagnostic_same(f, &source) == 0 && foreign.attached &&
          !f->state.terminal_tail_entered &&
          f->state.retirement_bootstrap == MC_CLIENT_BOOTSTRAP_BUSY &&
          f->state.retirement_world == NULL && app->active_world == world &&
          world->construction.phase == MC_WORLD_RUNTIME_READY &&
          !world->entity_containers_retired && !world->tile_entity_containers_retired);
    CHECK(mc_client_render_manager_consumer_detach(&foreign, NULL) == MC_CLIENT_MANAGER_OK);
    CHECK(mc_client_bootstrap_terminal_dispatchers(f->bootstrap) == MC_CLIENT_BOOTSTRAP_OK &&
          mc_client_bootstrap_terminal_dispatchers(f->bootstrap) == MC_CLIENT_BOOTSTRAP_OK &&
          app->active_world == world && app->h == player && app->i == player &&
          world->construction.phase == MC_WORLD_RUNTIME_READY &&
          !world->entity_containers_retired && !world->tile_entity_containers_retired);
    /* Dispatcher-owned Player occurrences have ended. An independent actual
     * retain must now be the sole reason selected-load admission refuses. */
    CHECK(mc_entity_retain(player) == MC_ENTITY_OK);
    CHECK(loaded_admission_attempt(f, MC_WORLD_STORAGE_BUSY,
                                   "World terminal external Entity or TileEntity hold", NULL) == 0);
    mc_entity_release(player); /* End only the actual extra test occurrence. */
    mc_world_terminal_inventory inventory;
    CHECK(mc_world_runtime_terminal_admit(world, NULL, &inventory, NULL) == MC_WORLD_STORAGE_BUSY);
    CHECK(loaded_admission_attempt(f, MC_WORLD_STORAGE_OK, NULL, &inventory) == 0 &&
          inventory.entities == 1 && inventory.entity_occurrences == player->holds &&
          inventory.tile_entities == 0 && inventory.tile_entity_occurrences == 0 &&
          inventory.expected_world_bindings == 4 && !inventory.piston_discard_required);
    return 0;
}

static int normal_terminal_retirement_controls(fixture *f, mc_client_source_da **external,
                                               bool selected_load) {
    mc_client_menu_loop_state source;
    client_application *app = f->app;
    mc_client_menu_loop *same_loop = f->loop;
    mc_client_screen_transitions *transitions = app->screen_transitions;
    mc_client_singleplayer_controller *controller = app->controller;
    mc_client_display *display = app->display;
    SDL_Window *window = mc_client_display_window(display);
    CHECK(external != NULL && *external != NULL && transitions != NULL && window != NULL &&
          mc_client_menu_loop_inspect(f->loop, &source) && !source.terminal_tail_entered);
    if (!selected_load) {
        mc_client_terminal_storage_scope scope;
        mc_world_storage_error error;
        CHECK(mc_client_screen_transitions_terminal_classify(transitions, &scope, &error) ==
                  MC_WORLD_STORAGE_OK &&
              scope == MC_CLIENT_TERMINAL_EMPTY_STORAGE && error.status == MC_WORLD_STORAGE_OK);
    } else {
        CHECK((loaded_draw_reached(&source) || unpaused_world_reached(&source, app)) &&
              controller != NULL);
    }
    CHECK(!mc_client_menu_loop_terminal_retire(&f->loop) && f->loop == same_loop &&
          terminal_source_diagnostic_same(f, &source) == 0 && f->state.terminal_prepare_selected &&
          f->state.terminal_prepared && f->state.terminal_tail_entered &&
          f->state.retirement_operation != NULL &&
          f->state.retirement_bootstrap == MC_CLIENT_BOOTSTRAP_OK &&
          f->state.retirement_world == NULL);
    /* The first retained screen occurs AFTER actual Load/World destruction.
     * Inspect surviving owners only; the former World/Player views expired. */
    mc_client_screen_transition_state retained;
    mc_client_screen_state screen;
    CHECK(app->screen_transitions == transitions &&
          mc_client_screen_transitions_inspect(transitions, &retained) && !retained.active &&
          retained.retiring && retained.terminal_prepared && retained.world_load == NULL &&
          app->active_world == NULL && app->h == NULL && app->i == NULL &&
          app->view_occurrence == NULL && app->controller == controller &&
          app->native_lifetime_phase == MC_CLIENT_APPLICATION_LIFETIME_REGISTERED &&
          app->display == display && mc_client_display_window(display) == window &&
          SDL_GetWindowID(window) != 0 &&
          mc_client_screen_inspect(*external, &screen, NULL) == MC_CLIENT_SCREEN_OK &&
          screen.phase == MC_CLIENT_SCREEN_READY && screen.holds >= 2);
    if (selected_load) {
        mc_singleplayer_controller_snapshot current;
        CHECK(mc_client_singleplayer_controller_read(controller, &current) &&
              current.phase == MC_SINGLEPLAYER_CONTROLLER_READY && !current.entered &&
              current.native_holds == 1);
        CHECK(mc_client_singleplayer_controller_retain(controller) ==
              MC_SINGLEPLAYER_CONTROLLER_OK);
        CHECK(mc_client_screen_release(external, NULL) == MC_CLIENT_SCREEN_OK && *external == NULL);
        CHECK(!mc_client_menu_loop_terminal_retire(&f->loop) && f->loop == same_loop &&
              terminal_source_diagnostic_same(f, &source) == 0 && f->state.terminal_tail_entered &&
              f->state.retirement_world == NULL &&
              f->state.retirement_bootstrap == MC_CLIENT_BOOTSTRAP_OK &&
              f->state.retirement_operation != NULL &&
              strcmp(f->state.retirement_operation, "actual controller native retirement") == 0 &&
              app->screen_transitions == transitions && app->controller == controller &&
              app->active_world == NULL && app->h == NULL && app->i == NULL &&
              mc_client_screen_transitions_inspect(transitions, &retained) &&
              retained.world_load == NULL && !retained.active &&
              mc_client_singleplayer_controller_read(controller, &current) && !current.entered &&
              current.native_holds == 2);
        CHECK(mc_client_singleplayer_controller_release(controller) ==
              MC_SINGLEPLAYER_CONTROLLER_OK);
    } else {
        CHECK(mc_client_screen_release(external, NULL) == MC_CLIENT_SCREEN_OK && *external == NULL);
    }
    CHECK(mc_client_menu_loop_terminal_retire(&f->loop) && f->loop == NULL &&
          mc_client_menu_loop_terminal_retire(&f->loop));
    CHECK(app->screen_transitions == NULL && app->controller == NULL && app->active_world == NULL &&
          app->h == NULL && app->i == NULL && app->view_occurrence == NULL &&
          app->native_lifetime_phase == MC_CLIENT_APPLICATION_LIFETIME_REGISTERED &&
          app->display == display && mc_client_display_window(display) == window);
    return 0;
}

/* Authored malformed Player NBT reaches the real source exception after a
 * nonnull World/Player/Input graph was acquired. This is a refusal control,
 * not a normal-shutdown fallback for that unsupported partial graph. */
static int partial_world_terminal_controls(fixture *f, mc_client_source_da **external,
                                           const mc_client_world_load_state *failed_load) {
    client_application *app = f->app;
    mc_world_runtime *world = failed_load->world;
    mc_entity *player = failed_load->player;
    mc_client_menu_loop_state source;
    CHECK(mc_client_menu_loop_inspect(f->loop, &source) && source.failed &&
          source.source_class != NULL && strcmp(source.source_class, "fd") == 0 &&
          source.source_method != NULL && strcmp(source.source_method, "a") == 0 &&
          source.source_pc == 88 && !source.terminal_tail_entered);
    CHECK(world != NULL && player != NULL && failed_load->handler != NULL &&
          failed_load->input != NULL && failed_load->failure != NULL &&
          failed_load->failure->status == MC_CLIENT_WORLD_LOAD_CHILD_FAILED &&
          failed_load->last_change_pc == 324 && !failed_load->source_returned &&
          !failed_load->player_load_returned && !failed_load->view_published &&
          failed_load->handler_opened && failed_load->world_constructed &&
          failed_load->world_published && failed_load->player_published &&
          failed_load->input_published && failed_load->renderer_returned &&
          failed_load->effects_returned && !failed_load->needs_conversion &&
          failed_load->failure->child.class_name != NULL &&
          strcmp(failed_load->failure->child.class_name, "fd") == 0 &&
          strcmp(failed_load->failure->child.method, "a") == 0 &&
          strcmp(failed_load->failure->child.descriptor, "(Lgs;)V") == 0 &&
          failed_load->failure->child.pc == 88);
    const mc_world_player_load_error *cause = failed_load->failure->player_load;
    CHECK(cause != NULL && cause->saved_data_present && cause->print_stack_trace_pending &&
          cause->catch_bytecode == 88 && cause->source_exception && cause->caught_exception &&
          !cause->source_returned && !cause->nbt_returned && !cause->info_cleared &&
          !cause->publication_returned && cause->nbt.status == MC_ENTITY_NBT_INDEX_OUT_OF_BOUNDS &&
          cause->nbt.base_bytecode == 129 && cause->nbt.index == 0 && cause->nbt.field != NULL &&
          strcmp(cause->nbt.field, "Pos") == 0);
    CHECK(app->active_world == world && app->h == player && app->i == NULL &&
          app->view_occurrence == NULL && world->construction.phase == MC_WORLD_RUNTIME_READY &&
          !world->entity_containers_retired && !world->tile_entity_containers_retired &&
          player->base.velocity_x == 0.25 && player->base.velocity_y == -0.5 &&
          player->base.velocity_z == 0.75);
    mc_entity_base_fields base;
    memcpy(&base, &player->base, sizeof(base));
    mc_client_menu_loop *same_loop = f->loop;
    CHECK(!mc_client_menu_loop_terminal_retire(&f->loop) && f->loop == same_loop &&
          terminal_source_diagnostic_same(f, &source) == 0 && f->state.terminal_prepared &&
          !f->state.terminal_tail_entered &&
          f->state.retirement_bootstrap == MC_CLIENT_BOOTSTRAP_OK &&
          f->state.retirement_world != NULL &&
          f->state.retirement_world->status == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          app->active_world == world && app->h == player && app->i == NULL &&
          app->view_occurrence == NULL && world->construction.phase == MC_WORLD_RUNTIME_READY &&
          !world->entity_containers_retired && !world->tile_entity_containers_retired &&
          memcmp(&base, &player->base, sizeof(base)) == 0);

    /* Native prefix/dispatcher ending has completed. A repeated refusal must
     * preserve these actual source owners, diagnostics and cache backing. */
    mc_world_source_state handler;
    mc_world_source_chunk_loader_state loader;
    mc_world_saved_data_source_state saved_data;
    mc_chunk_provider_fields provider;
    mc_movement_input_state input_before, input_after;
    CHECK(mc_world_store_source_inspect(failed_load->handler, &handler) &&
          mc_world_source_chunk_loader_inspect(world->source_construction->loader, &loader) &&
          mc_world_saved_data_source_inspect(world->fields.z, &saved_data) &&
          mc_chunk_provider_bind_fields(world->fields.v, &provider, NULL) == MC_WORLD_STORAGE_OK &&
          provider.cache != NULL && provider.pending != NULL &&
          mc_movement_input_read_state(failed_load->input, &input_before));

    struct partial_copy {
        const void *owner;
        size_t bytes;
        unsigned char *before;
    } copies[] = {{app, sizeof(*app), NULL},
                  {world, sizeof(*world), NULL},
                  {player, sizeof(*player), NULL},
                  {world->source_construction, sizeof(*world->source_construction), NULL},
                  {world->classes, sizeof(*world->classes), NULL},
                  {failed_load->failure, sizeof(*failed_load->failure), NULL},
                  {failed_load->cleanup, sizeof(*failed_load->cleanup), NULL},
                  {cause, sizeof(*cause), NULL},
                  {handler.failure, sizeof(*handler.failure), NULL},
                  {handler.cleanup, sizeof(*handler.cleanup), NULL},
                  {loader.failure, sizeof(*loader.failure), NULL},
                  {loader.cleanup, sizeof(*loader.cleanup), NULL},
                  {saved_data.failure, sizeof(*saved_data.failure), NULL},
                  {saved_data.cleanup, sizeof(*saved_data.cleanup), NULL},
                  {provider.cache, sizeof(*provider.cache), NULL},
                  {provider.cache->entries,
                   provider.cache->capacity * sizeof(*provider.cache->entries), NULL},
                  {provider.cache->insertions,
                   provider.cache->insertion_capacity * sizeof(*provider.cache->insertions), NULL},
                  {provider.pending, sizeof(*provider.pending), NULL},
                  {provider.pending->keys,
                   provider.pending->capacity * sizeof(*provider.pending->keys), NULL}};

    for (size_t i = 0; i < sizeof(copies) / sizeof(copies[0]); ++i) {
        CHECK(copies[i].owner != NULL && copies[i].bytes != 0);
        copies[i].before = malloc(copies[i].bytes);
        CHECK(copies[i].before != NULL);
        memcpy(copies[i].before, copies[i].owner, copies[i].bytes);
    }
    CHECK(!mc_client_menu_loop_terminal_retire(&f->loop) && f->loop == same_loop &&
          terminal_source_diagnostic_same(f, &source) == 0 && !f->state.terminal_tail_entered &&
          f->state.retirement_world != NULL &&
          f->state.retirement_world->status == MC_WORLD_STORAGE_UNSUPPORTED_STATE);
    for (size_t i = 0; i < sizeof(copies) / sizeof(copies[0]); ++i) {
        CHECK(memcmp(copies[i].before, copies[i].owner, copies[i].bytes) == 0);
        free(copies[i].before);
    }
    CHECK(mc_movement_input_read_state(failed_load->input, &input_after) &&
          input_after.kind == input_before.kind &&
          input_after.native_holds == input_before.native_holds &&
          input_after.settings == input_before.settings &&
          input_after.keys_identity == input_before.keys_identity &&
          input_after.key_count == input_before.key_count &&
          memcmp(input_after.keys, input_before.keys, sizeof(input_after.keys)) == 0 &&
          memcmp(&input_after.fields.strafe, &input_before.fields.strafe, sizeof(float)) == 0 &&
          memcmp(&input_after.fields.forward, &input_before.fields.forward, sizeof(float)) == 0 &&
          input_after.fields.source_c == input_before.fields.source_c &&
          input_after.fields.jump == input_before.fields.jump &&
          input_after.fields.sneak == input_before.fields.sneak);

    /* Deliberate fixture-owned low-level cleanup AFTER the normal coordinator
     * refused. The production normal runner must never choose this fallback. */
    CHECK(external != NULL && *external != NULL && !mc_client_menu_loop_retire(&f->loop) &&
          f->loop == same_loop && terminal_source_diagnostic_same(f, &source) == 0 &&
          !f->state.terminal_tail_entered && app->active_world == NULL && app->h == NULL &&
          app->i == NULL);
    CHECK(mc_client_screen_release(external, NULL) == MC_CLIENT_SCREEN_OK && *external == NULL);
    CHECK(mc_client_menu_loop_retire(&f->loop) && f->loop == NULL);
    puts("WORLD_LOAD_PARTIAL world-ready=1 player-nbt=Pos129:0 load-change=324 child=fd.a:88 "
         "coordinator-refused=1 explicit-legacy-cleanup=1");
    return 0;
}

/* Three independent loaded invocations keep every declared actor observable
 * in the existing first-Entity receipt. They add no production tracing. */
typedef struct animal_profile {
    const char *mode, *name, *source_class, *renderer_class, *model_class;
    mc_entity_class_kind kind;
    unsigned int constructor_pc, model_pc;
    size_t rendered_parts, graph_parts;
} animal_profile;

static const animal_profile animal_profiles[] = {
    {"animals-pig", "pig", "wh", "me", "eh", MC_ENTITY_CLASS_KIND_PIG, 19, 67, 6, 12},
    {"animals-cow", "cow", "bx", "va", "hh", MC_ENTITY_CLASS_KIND_COW, 19, 40, 9, 9},
    {"animals-chicken", "chicken", "ww", "ip", "td", MC_ENTITY_CLASS_KIND_CHICKEN, 62, 85, 8, 8}};

static const animal_profile *animal_profile_get(const char *mode) {
    for (size_t i = 0; i < sizeof(animal_profiles) / sizeof(*animal_profiles); ++i)
        if (!strcmp(mode, animal_profiles[i].mode))
            return &animal_profiles[i];
    return NULL;
}

static int animal_input(fixture *f, const animal_profile *profile) {
    mc_world_runtime *world = f->app->active_world;
    mc_entity *player = f->app->i;
    CHECK(world != NULL && player == f->app->h && f->app->r == NULL && !f->state.failed &&
          f->state.render_returned && f->state.world_frame != NULL &&
          f->state.world_frame->returned && f->animal_identity == 0 &&
          mc_entity_is_kind(player, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER) &&
          player->base.world == world && player->classes == world->classes->entities &&
          world->fields.r != NULL);

    struct animal_input_work {
        mc_entity_view_error view;
        mc_mob_error constructor;
        mc_world_light_error height;
        mc_world_entity_error publication;
    } *work = calloc(1, sizeof(*work));

    CHECK(work != NULL);
    const double player_x = player->base.x, player_y = player->base.y, player_z = player->base.z;
    const float player_yaw = player->base.yaw, player_pitch = player->base.aT;
    const mc_java_random world_random = *world->fields.r;
    mc_entity_vector *eye = NULL, *look = NULL;
    CHECK(mc_entity_living_position(player, 1.0f, &eye, &work->view) == MC_ENTITY_VIEW_OK &&
          mc_entity_living_look_current(player, mc_client_bootstrap_math(f->bootstrap), &look,
                                        &work->view) == MC_ENTITY_VIEW_OK &&
          eye != NULL && look != NULL && look->y > -.25 && look->y < .25);
    const double x = eye->x + look->x * 6.0, z = eye->z + look->z * 6.0;
    CHECK(mc_entity_vector_release(player->classes, look) == MC_ENTITY_OK &&
          mc_entity_vector_release(player->classes, eye) == MC_ENTITY_OK);
    const int32_t block_x = mc_math_floor_double(x), block_z = mc_math_floor_double(z);
    int32_t height = 0, air = -1;
    mc_world_storage_error error;
    CHECK(mc_world_runtime_height(world, block_x, block_z, &height, &work->height) ==
              MC_WORLD_STORAGE_OK &&
          height == 64 &&
          mc_world_runtime_block_get(world, block_x, height, block_z, &air, &error) ==
              MC_WORLD_STORAGE_OK &&
          air == 0 &&
          mc_world_runtime_block_get(world, block_x, height + 1, block_z, &air, &error) ==
              MC_WORLD_STORAGE_OK &&
          air == 0);
    const mc_entity_class_literal *literal = NULL;
    mc_entity *animal = NULL;
    CHECK(mc_entity_class_literal_get(player->classes, profile->kind, &literal) == MC_ENTITY_OK &&
          mc_mob_allocate(player->classes, f->app->registry, literal, &animal,
                          &work->constructor) == MC_MOB_OK &&
          animal != NULL && mc_mob_construct(animal, world, &work->constructor) == MC_MOB_OK &&
          animal->phase == MC_ENTITY_READY && animal->classes == player->classes &&
          animal->class_literal == literal && animal->base.world == world &&
          animal->bounds != NULL);
    CHECK(mc_entity_set_position(animal, x, (double)height, z) == MC_ENTITY_OK &&
          animal->base.x == x && animal->base.y == (double)height && animal->base.z == z &&
          animal->bounds->min_y == (double)height &&
          animal->bounds->max_y > animal->bounds->min_y && animal->bounds->min_x < x &&
          animal->bounds->max_x > x && animal->bounds->min_z < z && animal->bounds->max_z > z);
    const size_t before = world->entity_membership.count;
    CHECK(before == 1); /* Entity-empty save plus the actual published Player. */
    bool inserted = false;
    CHECK(mc_world_runtime_insert_entity(world, animal, &inserted, &work->publication) ==
              MC_WORLD_STORAGE_OK &&
          inserted && animal->base.bF && world->fields.b == world->entity_membership.list &&
          world->fields.b->count == before + 1 && world->entity_membership.count == before + 1 &&
          world->fields.b->items[before] == animal &&
          world->entity_membership.entries[before] == animal);
    mc_chunk_state *chunk_state = NULL;
    CHECK(mc_world_runtime_get(world, animal->base.bG, animal->base.bI, &chunk_state) ==
              MC_CHUNK_ACQUIRE_OK &&
          chunk_state != NULL && !chunk_state->blank);
    mc_world_chunk *chunk = mc_world_store_chunk_object(world->fields.w, chunk_state);
    mc_world_chunk_fields chunk_fields;
    const mc_world_chunk_entity_list *slice = NULL;
    size_t count = 0, occurrences = 0;
    CHECK(chunk != NULL && mc_world_chunk_fields_get(chunk, &chunk_fields) == MC_WORLD_STORAGE_OK &&
          chunk_fields.d == world && chunk_fields.j == animal->base.bG &&
          chunk_fields.k == animal->base.bI && chunk_fields.q && animal->base.bH == 4 &&
          mc_world_chunk_slice_at(chunk_fields.m, (size_t)animal->base.bH, &slice) ==
              MC_WORLD_STORAGE_OK &&
          mc_world_chunk_entity_count(slice, &count) == MC_WORLD_STORAGE_OK);
    for (size_t index = 0; index < count; ++index) {
        mc_entity *entry = NULL;
        CHECK(mc_world_chunk_entity_at(slice, index, &entry) == MC_WORLD_STORAGE_OK);
        if (entry == animal)
            ++occurrences;
    }
    CHECK(occurrences == 1 && animal->holds >= 3 && player->base.x == player_x &&
          player->base.y == player_y && player->base.z == player_z &&
          player->base.yaw == player_yaw && player->base.aT == player_pitch &&
          mc_test_java_random_equal(&world_random, world->fields.r));
    f->animal_identity = (uintptr_t)animal;
    const size_t holds = animal->holds;
    mc_entity_release(animal); /* End only this genuine constructor occurrence. */
    animal = NULL;
    mc_entity *published = world->entity_membership.entries[before];
    CHECK((uintptr_t)published == f->animal_identity && published->holds == holds - 1 &&
          published->base.world == world && mc_entity_is_kind(published, profile->kind));
    free(work);
    printf("WORLD_ANIMAL_ENTITY profile=%s input=native-constructed pose=actual-player-look "
           "distance=6 world-and-chunk=1 same-world-and-classes=1 constructor-returned=1 "
           "constructor-hold-released=1 player-unchanged=1 world-random-unchanged=1\n",
           profile->name);
    return 0;
}

static mc_entity *animal_live(const fixture *f) {
    mc_world_runtime *world = f->app->active_world;
    if (world == NULL || world->fields.b != world->entity_membership.list ||
        world->fields.b->count != world->entity_membership.count)
        return NULL;
    for (size_t i = 0; i < world->entity_membership.count; ++i) {
        mc_entity *entity = world->entity_membership.entries[i];
        if (world->fields.b->items[i] != entity)
            return NULL;
        if ((uintptr_t)entity == f->animal_identity)
            return entity;
    }
    return NULL;
}

static int animal_model_graph(fixture *f, const animal_profile *profile,
                              repeat_render_control *control, bool compiled) {
    mc_entity *animal = animal_live(f);
    CHECK(animal != NULL && animal->phase == MC_ENTITY_READY && animal->bounds != NULL &&
          animal->base.world == f->app->active_world && animal->classes == f->app->entities &&
          mc_entity_is_kind(animal, profile->kind) && !animal->base.be && animal->base.bF);
    mc_mob_state mob;
    CHECK(mc_mob_inspect(animal, &mob) && mob.kind == profile->kind &&
          mob.constructor_pc == profile->constructor_pc);
    if (profile->kind == MC_ENTITY_CLASS_KIND_PIG)
        CHECK(mob.watcher_16_present && mob.watcher_16_type == 0 && (mob.watcher_16 & 1) == 0);
    mc_client_manager_state manager;
    CHECK(mc_client_render_manager_inspect(f->app->startup->manager_classes, &manager) &&
          manager.phase == MC_CLIENT_MANAGER_READY && manager.a != NULL &&
          client_renderer_roster(f->app, (mc_client_render_manager *)manager.a, &manager));
    mc_client_managed_renderer *renderer = NULL;
    for (size_t i = 0; i < manager.entries; ++i) {
        const mc_entity_class_literal *key = NULL;
        mc_client_managed_renderer *owner = NULL;
        CHECK(mc_client_render_manager_entry((mc_client_render_manager *)manager.a, i, &key, &owner,
                                             NULL) == MC_CLIENT_MANAGER_OK);
        if (key == animal->class_literal)
            renderer = owner;
    }
    mc_client_managed_renderer_state state;
    CHECK(renderer != NULL && mc_client_managed_renderer_inspect(renderer, &state) &&
          state.phase == MC_CLIENT_MODEL_READY && state.identity == renderer &&
          state.manager == manager.a && state.source_class != NULL &&
          !strcmp(state.source_class, profile->renderer_class) &&
          state.primary == renderer->primary && state.secondary == renderer->secondary &&
          renderer->primary != NULL && renderer->primary->phase == MC_CLIENT_MODEL_READY &&
          !strcmp(renderer->primary->source_class, profile->model_class));
    if (profile->kind == MC_ENTITY_CLASS_KIND_PIG)
        CHECK(renderer->secondary != NULL && renderer->secondary != renderer->primary &&
              renderer->secondary->phase == MC_CLIENT_MODEL_READY &&
              !strcmp(renderer->secondary->source_class, "eh"));
    else
        CHECK(renderer->secondary == NULL);
    if (control->renderer == 0) {
        control->renderer = (uintptr_t)renderer;
        control->primary = (uintptr_t)renderer->primary;
        control->secondary = (uintptr_t)renderer->secondary;
    }
    CHECK(control->renderer == (uintptr_t)renderer &&
          control->primary == (uintptr_t)renderer->primary &&
          control->secondary == (uintptr_t)renderer->secondary);
    manager_model *model = renderer->primary;
    mc_client_model_part *parts[12] = {0};
    if (profile->kind == MC_ENTITY_CLASS_KIND_CHICKEN) {
        mc_client_model_part *selected[] = {model->fields.a, model->fields.g, model->fields.h,
                                            model->fields.b, model->fields.c, model->fields.d,
                                            model->fields.e, model->fields.f};
        memcpy(parts, selected, sizeof(selected));
    } else {
        mc_client_model_part *selected[] = {model->fields.d, model->fields.e, model->fields.f,
                                            model->fields.g, model->fields.h, model->fields.i};
        memcpy(parts, selected, sizeof(selected));
        if (profile->kind == MC_ENTITY_CLASS_KIND_COW) {
            parts[6] = model->fields.b;
            parts[7] = model->fields.c;
            parts[8] = model->fields.a;
        } else {
            model = renderer->secondary;
            mc_client_model_part *secondary[] = {model->fields.d, model->fields.e, model->fields.f,
                                                 model->fields.g, model->fields.h, model->fields.i};
            memcpy(parts + 6, secondary, sizeof(secondary));
        }
    }
    for (size_t i = 0; i < profile->graph_parts; ++i) {
        mc_client_model_part *part = parts[i];
        const bool rendered = compiled && i < profile->rendered_parts;
        CHECK(part != NULL && part->h && !part->i && part->n == rendered);
        if (control->parts[i] == 0)
            control->parts[i] = (uintptr_t)part;
        CHECK(control->parts[i] == (uintptr_t)part);
        for (size_t j = 0; j < i; ++j)
            CHECK(control->parts[j] != (uintptr_t)part);
        if (rendered) {
            mc_client_source_list_state list;
            CHECK(part->o > 0 && mc_client_display_list_inspect(f->app->display, part->o, &list) &&
                  list.defined && !list.building && list.draws == 6 && list.vertices == 36);
            for (size_t face = 0; face < 6; ++face) {
                mc_client_source_batch_view batch;
                CHECK(mc_client_display_list_batch(f->app->display, part->o, face, &batch) &&
                      batch.mode == 4 && batch.vertices == 6 && batch.byte_count == 192);
            }
            if (control->lists[i] == 0)
                control->lists[i] = part->o;
            CHECK(control->lists[i] == part->o);
            for (size_t j = 0; j < i; ++j)
                CHECK(control->lists[j] != part->o);
        } else {
            CHECK(part->o == 0);
        }
    }
    if (compiled)
        CHECK(manager.fields.g == f->app->active_world && manager.living_occurrence == f->app->i &&
              manager.fields.k == f->app->settings && animal->base.bt > 0);
    return 0;
}

static int animal_returned_frame(fixture *f, const animal_profile *profile,
                                 repeat_render_control *control) {
    const mc_client_menu_loop_state *source = &f->state;
    const mc_client_world_frame_state *frame = source->world_frame;
    const mc_client_gui_ingame_draw_state *hud = source->hud;
    CHECK(!source->failed && source->running && source->render_returned &&
          source->render_clock_stored && source->world_draws > 0 &&
          source->world_draws == source->hud_draws && frame != NULL && !frame->active &&
          frame->returned && frame->source_pc == 1013 && frame->app == f->app &&
          frame->world == f->app->active_world && frame->owner == f->app->t &&
          frame->camera == f->app->i && frame->render_global == f->app->g &&
          frame->failure != NULL && frame->failure->status == MC_CLIENT_WORLD_FRAME_OK &&
          frame->failure->entities == NULL && frame->failure->hand == NULL);
    CHECK(frame->picking_returned && frame->viewport_set && frame->fog_returned &&
          frame->camera_returned && frame->frustum_returned && frame->sky_returned &&
          frame->updates_returned && frame->terrain_returned && frame->lighting_enabled &&
          frame->eye_position_returned && frame->entities_returned &&
          frame->lit_particles_returned && frame->sprite_particles_returned &&
          frame->translucent_returned && frame->translucent_state_restored &&
          frame->precipitation_returned && frame->clouds_returned && frame->hand_returned &&
          frame->hand != NULL && frame->hand->returned && frame->hand->source_pc == 163 &&
          frame->hand->held_returned && frame->hand->overlays_returned &&
          frame->tail_anaglyph_read && !frame->tail_anaglyph);
    CHECK(hud != NULL && !hud->active && hud->returned && hud->source_pc == 2101 &&
          hud->projection_returned && hud->hotbar_returned && hud->crosshair_returned &&
          hud->stats_returned && hud->slots_returned && hud->empty_slots == 9 &&
          hud->chat_returned && hud->owner == NULL && hud->failure == NULL && hud->cleanup == NULL);
    const mc_client_render_global_entities_state *entities = frame->entities;
    CHECK(entities != NULL && !entities->active && entities->returned && !entities->retiring &&
          entities->draw == NULL && entities->owner == NULL && entities->current == NULL &&
          entities->entities_list == NULL && entities->eye == NULL && entities->frustum == NULL &&
          entities->ll == NULL && entities->th == NULL && entities->failure == NULL &&
          entities->cleanup == NULL);
    ++control->frames;
    if (entities->delayed) {
        CHECK(entities->source_pc == 17 && entities->draws_attempted == 0 &&
              !entities->first_entity_draw.valid && !control->compiled);
        ++control->delayed_frames;
        return 0;
    }
    CHECK(
        entities->source_pc == 569 && entities->ll_returned && entities->th_returned &&
        entities->counters_cleared && entities->origins_stored && entities->origins_written == 6 &&
        entities->entities_visited == f->app->active_world->entity_membership.count &&
        entities->entities_visited >= 2 && entities->distance_checks == entities->entities_visited);
    mc_entity *animal = animal_live(f);
    if (entities->draws_attempted == 0) {
        CHECK(animal != NULL && animal->base.bt == 0 && !control->compiled &&
              f->app->active_world->entity_membership.count == 2 &&
              entities->entities_visited == 2 && entities->distance_checks == 2 &&
              entities->frustum_checks == 2 && entities->camera_skips == 1 &&
              entities->presence_checks == 0 && entities->draws_returned == 0 &&
              entities->draws_retired == 0 && entities->entity_draws_returned == 0 &&
              entities->weather_draws_returned == 0 && !entities->first_entity_draw.valid &&
              !entities->last_draw.valid && entities->model_parts_compiled == 0 &&
              entities->model_faces == 0 && entities->model_vertices == 0 &&
              entities->shadow_quads == 0 && f->app->r == NULL && !atomic_load(&f->app->o));
        CHECK(animal_model_graph(f, profile, control, false) == 0);
        ++control->undrawn_frames;
        return 0;
    }
    CHECK(entities->presence_checks >= 1 && entities->draws_attempted == entities->draws_returned &&
          entities->draws_returned == entities->draws_retired &&
          entities->entity_draws_returned > 0);
    const mc_client_render_global_draw_receipt *draw = &entities->first_entity_draw;
    CHECK(animal != NULL && repeat_world_work(source) && draw->valid && draw->returned &&
          draw->retired && !draw->post_only && draw->n_invoke_pc == 516 &&
          draw->th_source_pc == 115 && draw->living_source_pc == 769 &&
          draw->post_draw_source_pc == 100 && draw->actor_identity == f->animal_identity &&
          draw->actor_class_identity == (uintptr_t)animal->class_literal &&
          draw->actor_kind == (int32_t)profile->kind && draw->actor_class_name != NULL &&
          !strcmp(draw->actor_class_name, profile->source_class) &&
          draw->renderer_identity == control->renderer && draw->renderer_class_name != NULL &&
          !strcmp(draw->renderer_class_name, profile->renderer_class) && draw->primary_returned &&
          draw->post_draw_returned && draw->primary_event_returned);
    CHECK(draw->primary_model.identity == control->primary &&
          draw->primary_model.class_name != NULL &&
          !strcmp(draw->primary_model.class_name, profile->model_class) &&
          draw->primary_model.returned && draw->primary_model.source_pc == profile->model_pc &&
          draw->primary_model.part_calls == profile->rendered_parts &&
          draw->primary_model.part_returns == profile->rendered_parts &&
          draw->secondary_model.identity == 0 && !draw->secondary_model.returned);
    for (size_t i = 0; i < 4; ++i)
        CHECK(draw->pass_attempted[i] && !draw->pass_result[i] && !draw->pass_render_returned[i]);
    CHECK(draw->shadow_quads > 0 && draw->shadow_vertices == draw->shadow_quads * 4 &&
          draw->shadow_block_queries > 0 && draw->shadow_light_queries > 0 &&
          draw->shadow_normal_queries >= draw->shadow_quads &&
          draw->shadow_brightness_queries >= draw->shadow_quads);
    if (!control->compiled) {
        CHECK(draw->model_parts_compiled == profile->rendered_parts &&
              draw->primary_model.compiled_parts == profile->rendered_parts &&
              draw->model_faces == profile->rendered_parts * 6 &&
              draw->model_vertices == profile->rendered_parts * 24);
        control->compile_parts = draw->model_parts_compiled;
        control->compile_faces = draw->model_faces;
        control->compile_vertices = draw->model_vertices;
        control->compiled = true;
    } else {
        CHECK(draw->model_parts_compiled == 0 && draw->primary_model.compiled_parts == 0 &&
              draw->model_faces == 0 && draw->model_vertices == 0);
        control->reuse_parts = draw->model_parts_compiled;
        control->reused = true;
    }
    CHECK(animal_model_graph(f, profile, control, true) == 0);
    ++control->entity_frames;
    control->shadow_quads += draw->shadow_quads;
    return 0;
}

static int animal_controls(fixture *f, const animal_profile *profile, const char *capture_path) {
    CHECK(repeat_statistics_controls(f, false) == 0 && f->app->r == NULL &&
          !atomic_load(&f->app->o) && display_focus(f, true) == 0);
    printf("WORLD_ANIMAL_PROFILE profile=%s scopes=independent-loaded "
           "statistics-input=pre-bootstrap-unsent-file open-inventory=1 "
           "canonical=ep.f notice-inactive=1 native-integration-only=1\n",
           profile->name);
    CHECK(animal_input(f, profile) == 0);
    SDL_Delay(100); /* The real Timer decides the number of source ticks. */
    repeat_render_control control = {0};
    CHECK(animal_model_graph(f, profile, &control, false) == 0);
    const uint64_t ticks_before = f->state.world_tick_returns;
    const uint64_t draws_before = f->state.world_draws;
    const uint64_t started = SDL_GetTicks(), deadline = started + 60000;
    while (!control.reused && control.frames < 8 && SDL_GetTicks() < deadline) {
        CHECK(display_focus(f, true) == 0);
        const bool returned = mc_client_menu_loop_frame(f->loop);
        CHECK(mc_client_menu_loop_inspect(f->loop, &f->state));
        if (!returned)
            fprintf(stderr, "Animal %s retained %s.%s:%" PRId64 " %s\n", profile->name,
                    f->state.source_class == NULL ? "native" : f->state.source_class,
                    f->state.source_method == NULL ? "frame" : f->state.source_method,
                    f->state.source_pc, f->state.operation == NULL ? "none" : f->state.operation);
        CHECK(returned && animal_returned_frame(f, profile, &control) == 0);
        if (!control.reused)
            SDL_Delay(100);
    }
    if (!control.reused) {
        mc_entity *animal = animal_live(f);
        fprintf(stderr,
                "Animal %s deadline elapsed-seconds=%" PRIu64
                " limit-seconds=60 frames=%zu delayed=%zu undrawn=%zu entity-frames=%zu "
                "paused=%d screen=%d actor-ticks=%d\n",
                profile->name, (SDL_GetTicks() - started) / UINT64_C(1000), control.frames,
                control.delayed_frames, control.undrawn_frames, control.entity_frames,
                (int)atomic_load(&f->app->o), f->app->r == NULL ? -1 : (int)f->app->r->kind,
                animal == NULL ? -1 : animal->base.bt);
    }
    CHECK(control.compiled && control.reused && control.entity_frames >= 2 && control.frames >= 2 &&
          control.frames <= 8 &&
          control.frames ==
              control.entity_frames + control.delayed_frames + control.undrawn_frames &&
          f->state.world_tick_returns > ticks_before &&
          f->state.world_draws == draws_before + control.frames &&
          repeat_statistics_controls(f, false) == 0);
    printf(
        "WORLD_ANIMAL_FRAME profile=%s frames=%zu entity-frames=%zu delayed-frames=%zu "
        "undrawn-frames=%zu source=px.a:1013 entities=n.a:569 hud=uq.a:2101 "
        "mob=%s renderer=%s primary=%s exact-declared-actor=1 "
        "renderer-map=29 constructor-owners=28 local-player-alias=gs same-world-and-classes=1 "
        "compiled-parts=%zu reused-parts=%zu compiled-faces=%zu compiled-vertices=%zu "
        "part-lists=%zu unused-secondary-parts=%zu part-source-vertices=24 part-list-vertices=36 "
        "stable-list-ids=1 shadow-quads=%zu checked-draw-retirement=1 copied-receipts=1 "
        "tick-return=1328 scheduler=198 completed-random-calls=%" PRIu64
        " completed-chunks=%zu completed-samples=%" PRIu64 " display-calls=%" PRIu64
        " display-samples=%" PRIu64 " display-random=%" PRIu64 "\n",
        profile->name, control.frames, control.entity_frames, control.delayed_frames,
        control.undrawn_frames, profile->source_class, profile->renderer_class,
        profile->model_class, control.compile_parts, control.reuse_parts, control.compile_faces,
        control.compile_vertices, profile->rendered_parts,
        profile->graph_parts - profile->rendered_parts, control.shadow_quads,
        f->state.random_chunk_calls, f->state.last_random_chunk_count,
        f->state.random_chunk_samples, f->state.random_display_calls,
        f->state.random_display_samples, f->state.random_display_random_calls);
    CHECK(capture(f, capture_path) == 0);
    return 0;
}

int main(int argc, char **argv) {
    CHECK(argc == 4 && (strcmp(argv[2], "empty") == 0 || strcmp(argv[2], "populated") == 0 ||
                        strcmp(argv[2], "large") == 0 || strcmp(argv[2], "double") == 0 ||
                        strcmp(argv[2], "load") == 0 || strcmp(argv[2], "generate") == 0 ||
                        strcmp(argv[2], "load-terminal") == 0 ||
                        strcmp(argv[2], "load-partial") == 0 || strcmp(argv[2], "repeat") == 0 ||
                        strcmp(argv[2], "load-light") == 0 || animal_profile_get(argv[2]) != NULL));
    const animal_profile *animal = animal_profile_get(argv[2]);
    const bool animals_load = animal != NULL;
    const bool double_click = strcmp(argv[2], "double") == 0;
    const bool terminal_admission = strcmp(argv[2], "load-terminal") == 0;
    const bool partial_load = strcmp(argv[2], "load-partial") == 0;
    const bool repeat_load = strcmp(argv[2], "repeat") == 0;
    const bool light_control = strcmp(argv[2], "load-light") == 0;
    const bool focused_load =
        strcmp(argv[2], "load") == 0 || repeat_load || light_control || animals_load;
    const bool loading_world = strcmp(argv[2], "load") == 0 || strcmp(argv[2], "generate") == 0 ||
                               terminal_admission || repeat_load || light_control || animals_load;
    const bool populated =
        strcmp(argv[2], "populated") == 0 || double_click || loading_world || partial_load;
    const bool large = strcmp(argv[2], "large") == 0;
    fixture f = {0};
    mc_client_bootstrap_status closed;
    uint64_t close_deadline;
    const mc_client_application_options options = {.assets = argv[1],
                                                   .username = "Selection control",
                                                   .width = 854,
                                                   .height = 480,
                                                   .hidden = !focused_load};
    CHECK(mc_client_bootstrap_open(&options, &f.bootstrap) == MC_CLIENT_BOOTSTRAP_OK);
    f.app = mc_client_bootstrap_app(f.bootstrap);
    if (repeat_load || animals_load)
        CHECK(repeat_statistics_controls(&f, true) == 0);
    if (focused_load)
        CHECK(SDL_RaiseWindow(mc_client_display_window(f.app->display)));
    CHECK(f.app != NULL && f.app->constructor_phase == MC_CLIENT_APP_READY && f.app->r != NULL &&
          f.app->r->kind == MC_CLIENT_SCREEN_MAIN_MENU);
    CHECK(f.app->active_world == NULL && f.app->h == NULL && f.app->i == NULL &&
          f.app->controller == NULL && !f.app->N);
    mc_client_source_da *initial = f.app->r;
    const size_t initializations = initial->initialization_count;
    mc_client_source_ke *original_press = button(initial, 1);
    CHECK(original_press != NULL);
    CHECK(mc_client_menu_loop_create(f.bootstrap, &f.loop));
    mc_client_application_startup *startup_attempt = f.app->startup;
    mc_client_startup_error *blocked = calloc(1, sizeof(*blocked));
    CHECK(blocked != NULL && f.app->screen_transitions != NULL);
    CHECK(mc_client_application_startup_retire(&startup_attempt, blocked) ==
              MC_CLIENT_STARTUP_BUSY &&
          startup_attempt == f.app->startup && f.app->display != NULL && f.app->r == initial &&
          f.app->constructor_phase == MC_CLIENT_APP_READY);
    free(blocked);
    CHECK(mc_client_menu_loop_inspect(f.loop, &f.state));
    CHECK(ticks(&f, 2) == 0);
    if (!populated && !large)
        CHECK(mc_test_world_load_foreign_loading(f.app) == 0);
    mc_client_source_rq *rq = NULL;
    CHECK(open_selection(&f, initial, &rq) == 0);
    CHECK(drawing_monitor_recovery(rq) == 0);
    CHECK(rq->fields.n != NULL && mc_client_save_list_size(rq->fields.n) == (large       ? 16u
                                                                             : populated ? 3u
                                                                                         : 0u));
    if (large) {
        CHECK(filename_is(mc_client_save_list_at(rq->fields.n, 0), "world00"));
        CHECK(filename_is(mc_client_save_list_at(rq->fields.n, 15), "world15"));
    } else {
        const int32_t count = populated ? 3 : 0;
        const mc_client_world_list_fields *list = &rq->fields.o->fields;
        const int32_t negative_limit = (count * 36 - (list->i - list->h - 4)) / 2;
        CHECK(negative_limit < 0 && list->m == (float)negative_limit);
    }
    if (populated) {
        CHECK(filename_is(mc_client_save_list_at(rq->fields.n, 0), "alpha"));
        CHECK(filename_is(mc_client_save_list_at(rq->fields.n, 1), "bravo"));
        CHECK(filename_is(mc_client_save_list_at(rq->fields.n, 2), "older"));
    }
    mc_client_source_da *external = &rq->base;
    CHECK(mc_client_screen_retain(external, NULL) == MC_CLIENT_SCREEN_OK);
    if (large) {
        CHECK(release_mouse(&f, rq) == 0 && large_drag(&f, rq, argv[3]) == 0 &&
              row_press(&f, rq, true) == 0);
    } else {
        CHECK(release_mouse(&f, rq) == 0 && row_press(&f, rq, populated) == 0);
        CHECK(capture(&f, argv[3]) == 0);
    }
    CHECK(release_mouse(&f, rq) == 0);
    CHECK(push_button(&f, &rq->base, 0, true) == 0 && push_button(&f, &rq->base, 0, false) == 0 &&
          transfer(&f) == 0);
    const uint64_t cancel_deadline = SDL_GetTicks() + 3000;
    while (f.app->r != initial && SDL_GetTicks() < cancel_deadline)
        CHECK(frame(&f) == 0);
    CHECK(f.app->r == initial && initial->initialization_count == initializations + 1);
    CHECK(rq->fields.a == initial && rq->base.fields.a == NULL &&
          initial->fields.a == original_press); /* Trailing mouse-up ran on original RQ. */
    CHECK(initial->last_initialization != NULL &&
          initial->last_initialization->phase == MC_CLIENT_SCREEN_INIT_RETURNED);
    CHECK(f.app->active_world == NULL && f.app->h == NULL && f.app->i == NULL && !f.app->N);
    if (populated) {
        mc_client_source_rq *next = NULL;
        CHECK(open_selection(&f, initial, &next) == 0 && next != rq);
        int32_t previous_index = -1;
        int64_t previous_click = 0, double_started = 0;
        if (double_click) {
            CHECK(release_mouse(&f, next) == 0 && row_press(&f, next, true) == 0);
            previous_index = next->fields.o->fields.n;
            previous_click = next->fields.o->fields.o;
            CHECK(previous_index == 0 && previous_click != 0);
            CHECK(release_for_double(&f, next) == 0);
            CHECK(next->fields.o->fields.n == previous_index &&
                  next->fields.o->fields.o == previous_click);
            CHECK(mc_platform_current_time_millis(&double_started) == MC_PLATFORM_MILLIS_OK &&
                  mc_java_i64_sub(double_started, previous_click) < 250);
            const mc_client_world_list_fields *list = &next->fields.o->fields;
            const int32_t y = list->h + 4 - mc_java_float_to_i32(list->m) + 8;
            CHECK(push_mouse(&f, &next->base, next->base.fields.c / 2, y, true) == 0 &&
                  transfer(&f) == 0);
        } else {
            CHECK(release_mouse(&f, next) == 0 && row_press(&f, next, true) == 0 &&
                  release_mouse(&f, next) == 0);
            CHECK(push_button(&f, &next->base, 1, true) == 0 &&
                  push_button(&f, &next->base, 1, false) == 0 && transfer(&f) == 0);
        }
        bool stopped = false, loaded_frame = false;
        const uint64_t select_deadline = SDL_GetTicks() + 3000;
        while (!stopped && !loaded_frame && SDL_GetTicks() < select_deadline) {
            stopped = !mc_client_menu_loop_frame(f.loop);
            CHECK(mc_client_menu_loop_inspect(f.loop, &f.state));
            loaded_frame = loading_world && loaded_draw_reached(&f.state);
            SDL_Delay(1);
        }
        CHECK((stopped && f.state.failed && f.state.source_class != NULL) ||
              (loaded_frame && !f.state.failed));
        mc_client_screen_transition_state transitions;
        mc_client_world_load_state load;
        CHECK(mc_client_screen_transitions_inspect(f.app->screen_transitions, &transitions) &&
              transitions.world_load != NULL &&
              mc_client_world_load_inspect(transitions.world_load, &load));
        if (partial_load) {
            CHECK(partial_world_terminal_controls(&f, &external, &load) == 0);
            goto close_bootstrap;
        }
        if (loading_world &&
            (load.failure == NULL || load.failure->status != MC_CLIENT_WORLD_LOAD_OK ||
             !load.source_returned || !load.player_published)) {
            fprintf(stderr,
                    "World load reached status=%d %s.%s:%d child=%s:%d operation=%s terrain=%zu\n",
                    load.failure == NULL ? -1 : (int)load.failure->status,
                    load.failure == NULL || load.failure->site.class_name == NULL
                        ? "native"
                        : load.failure->site.class_name,
                    load.failure == NULL || load.failure->site.method == NULL
                        ? "none"
                        : load.failure->site.method,
                    load.failure == NULL ? -1 : load.failure->site.pc,
                    load.failure == NULL || load.failure->child.class_name == NULL
                        ? "none"
                        : load.failure->child.class_name,
                    load.failure == NULL ? -1 : load.failure->child.pc,
                    load.failure == NULL || load.failure->operation == NULL
                        ? "none"
                        : load.failure->operation,
                    load.terrain_samples);
            if (load.failure != NULL && load.failure->terrain != NULL &&
                load.failure->terrain->loading != NULL) {
                const mc_client_loading_error *detail = load.failure->terrain->loading;
                fprintf(stderr, "Loading failure status=%d %s.%s:%d child=%s:%d op=%s display=%s\n",
                        (int)detail->status,
                        detail->site.class_name == NULL ? "native" : detail->site.class_name,
                        detail->site.method == NULL ? "none" : detail->site.method, detail->site.pc,
                        detail->child.class_name == NULL ? "none" : detail->child.class_name,
                        detail->child.pc, detail->operation == NULL ? "none" : detail->operation,
                        detail->display.operation == NULL ? "none" : detail->display.operation);
                fprintf(stderr, "Graphics status=%d op=%s display=%d:%s detail=%s\n",
                        (int)detail->graphics.status,
                        detail->graphics.operation == NULL ? "none" : detail->graphics.operation,
                        (int)detail->graphics.display.status,
                        detail->graphics.display.operation == NULL
                            ? "none"
                            : detail->graphics.display.operation,
                        detail->graphics.display.detail);
            }
        }
        CHECK(load.failure != NULL);
        if (loading_world)
            CHECK(load.failure->status == MC_CLIENT_WORLD_LOAD_OK && load.source_returned &&
                  load.last_outer_pc == 130 && load.last_change_pc == 366 &&
                  load.renderer_returned && load.effects_returned && load.player_load_returned &&
                  load.view_published);
        else
            CHECK(load.failure->status == MC_CLIENT_WORLD_LOAD_UNIMPLEMENTED &&
                  strcmp(load.failure->site.class_name, "net/minecraft/client/Minecraft") == 0 &&
                  strcmp(load.failure->site.method, "b") == 0 && load.failure->site.pc == 52);
        CHECK(load.initial_detach_returned && load.initial_i_cleared && load.initial_h_cleared &&
              load.initial_P_stored && load.gc_requests == (loading_world ? 3u : 2u) &&
              load.conversion_checked);
        if (loading_world) {
            CHECK(!load.needs_conversion && load.handler_opened && load.world_constructed &&
                  load.world_published && load.player_published && load.input_published &&
                  load.world == f.app->active_world && load.player == f.app->h &&
                  f.app->i == f.app->h && load.input != NULL && load.terrain_samples == 289);
            mc_world_runtime_construction constructed;
            mc_client_world_prepare_state prepared;
            mc_local_player_snapshot player;
            CHECK(mc_world_runtime_construction_get(load.world, &constructed) ==
                      MC_WORLD_STORAGE_OK &&
                  constructed.mode == MC_WORLD_RUNTIME_GAME &&
                  constructed.phase == MC_WORLD_RUNTIME_READY &&
                  constructed.entry == MC_WORLD_RUNTIME_CLIENT_LOAD_OR_CREATE);
            CHECK(mc_client_world_prepare_inspect(load.prepare, &prepared) &&
                  prepared.source_returned && prepared.count == 289 && prepared.total == 289 &&
                  prepared.block_samples == 289 && prepared.progress_calls == 289 &&
                  prepared.progress_returns == 289 && prepared.last_progress == 99 &&
                  prepared.unused_simulation_count == 2000 && prepared.spawn_constructed &&
                  !prepared.player_x_stored && !prepared.player_z_stored);
            for (size_t sample = 0; sample < 289; ++sample) {
                const mc_client_world_prepare_sample *actual = &prepared.samples[sample];
                CHECK(actual->dx == -128 + (int32_t)(sample / 17) * 16 &&
                      actual->dz == -128 + (int32_t)(sample % 17) * 16 &&
                      actual->progress == (int32_t)(sample * 100 / 289) &&
                      actual->progress_returned && actual->block_returned &&
                      actual->block_world == load.world && actual->light_calls != 0);
            }
            CHECK(mc_local_player_get_snapshot(load.player, &player) == MC_ENTITY_OK &&
                  player.minecraft == f.app && player.input == load.input);
            int32_t marker = 0;
            mc_world_storage_error stored;
            CHECK(mc_world_runtime_block_get(load.world, 0, 127, 0, &marker, &stored) ==
                      MC_WORLD_STORAGE_OK &&
                  marker == 57);
            mc_client_sdk_input_state input;
            CHECK((f.app->r == NULL || f.app->r->kind == MC_CLIENT_SCREEN_PAUSE) &&
                  mc_client_sdk_input_inspect(mc_client_display_inputs(f.app->display), &input) &&
                  input.grabbed == f.app->N && player.living.Y > 0);
            bool display_active;
            mc_client_sdk_update_error update;
            CHECK(mc_client_sdk_display_active(mc_client_display_update_owner(f.app->display),
                                               &display_active,
                                               &update) == MC_CLIENT_SDK_UPDATE_OK);
            if (!loaded_frame)
                fprintf(stderr, "Loaded World frame reached %s.%s:%" PRId64 "\n",
                        f.state.source_class, f.state.source_method, f.state.source_pc);
            mc_client_entity_renderer_state renderer;
            CHECK(mc_client_entity_renderer_inspect(f.app->t, &renderer));
            CHECK(loaded_frame && loaded_draw_reached(&f.state) && f.state.render_clock_stored &&
                  f.state.render_clock == renderer.fields.I && f.state.world_frame != NULL &&
                  f.state.world_frame->picking_returned && f.state.world_frame->viewport_set &&
                  f.state.world_frame->fog_blend_computed && f.state.world_frame->fog_returned &&
                  f.state.world_frame->camera_returned && f.state.world_frame->frustum_returned &&
                  f.state.world_frame->sky_fog_returned && f.state.world_frame->sky_returned &&
                  f.state.world_frame->sky != NULL && f.state.world_frame->sky->cleanup == NULL &&
                  f.state.world_frame->terrain_fog_returned &&
                  f.state.world_frame->frustum_constructed &&
                  f.state.world_frame->frustum_recentered && f.state.world_frame->culling_returned);
            if (focused_load)
                CHECK(display_active && !f.state.failed && f.state.render_returned);
            CHECK(f.state.hud != NULL && f.state.hud->returned && f.state.hud->source_pc == 2101 &&
                  f.state.hud->projection_returned && f.state.hud->hotbar_returned &&
                  f.state.hud->crosshair_returned && f.state.hud->stats_returned &&
                  f.state.hud->slots_returned && f.state.hud->empty_slots == 9 &&
                  f.state.hud->chat_returned && f.state.hud->owner == NULL &&
                  f.state.hud->failure == NULL && f.state.hud->cleanup == NULL);
            const mc_client_world_frame_state *frame = f.state.world_frame;
            CHECK(frame->source_pc == 1013 && frame->returned && frame->update_calls >= 1 &&
                  frame->updates_returned && frame->draw_fog_returned &&
                  frame->terrain_texture_bound && frame->lighting_disabled &&
                  frame->update != NULL && !frame->update->active && frame->update->returned &&
                  frame->update->failure == NULL && frame->failure->update == NULL &&
                  frame->update->compilation == NULL && frame->update->near_chunks == NULL &&
                  frame->update->compile_order == NULL &&
                  frame->update->compile_attempts == frame->update->completed_compiles &&
                  frame->terrain_returned && frame->submission != NULL &&
                  frame->submission->returned && frame->submission->source_pc == 896 &&
                  frame->submission->result == frame->terrain_count &&
                  frame->submission->dirty_polls == 10 && frame->submission->subsets >= 1 &&
                  frame->submission->frames == NULL && frame->submission->rebuild == NULL &&
                  frame->submission->failure == NULL && frame->submission->cleanup == NULL &&
                  frame->submission->owner == NULL && frame->submission->camera == NULL &&
                  frame->submission->batch_camera == NULL && frame->submission->math == NULL &&
                  !frame->submission->matrix_pushed && !frame->submission->query_active &&
                  frame->lighting_enabled && frame->eye_position_returned &&
                  frame->entity_eye != NULL && frame->standard_lighting != NULL &&
                  frame->failure->lighting_enable == NULL && frame->failure->view == NULL &&
                  frame->entities_returned && frame->entities != NULL && !frame->entities->active &&
                  frame->entities->returned && frame->entities->delayed &&
                  frame->entities->source_pc == 17 && frame->entities->initial_delay == 2 &&
                  frame->entities->remaining_delay == 1 && frame->entities->owner == NULL &&
                  frame->entities->eye == NULL && frame->entities->frustum == NULL &&
                  frame->entities->entities_list == NULL && frame->entities->current == NULL &&
                  frame->entities->failure == NULL && frame->entities->cleanup == NULL &&
                  frame->entities->ll == NULL && frame->entities->th == NULL &&
                  frame->failure->entities == NULL &&
                  frame->failure->status == MC_CLIENT_WORLD_FRAME_OK &&
                  frame->failure->site.class_name == NULL &&
                  frame->failure->child.class_name == NULL);
            CHECK(frame->effects_captured && frame->effects == f.app->effect_renderer &&
                  frame->camera == load.player && frame->render_global == f.app->g &&
                  frame->lit_particles_returned && frame->particle_lighting_disabled &&
                  frame->particle_fog_returned && frame->particle_fog != NULL &&
                  frame->particle_fog != frame->draw_fog && frame->sprite_particles_returned &&
                  frame->lit_particles != NULL && frame->lit_particles->returned &&
                  frame->lit_particles->lit && frame->lit_particles->empty &&
                  frame->lit_particles->source_pc == 16 && frame->lit_particles->size_reads == 1 &&
                  frame->lit_particles->visited == 0 &&
                  frame->lit_particles->orientation_values == 0 &&
                  frame->lit_particles->origins_stored == 0 && !frame->lit_particles->nw_captured &&
                  frame->lit_particles->owner == NULL && frame->lit_particles->camera == NULL &&
                  frame->lit_particles->current == NULL && frame->lit_particles->nw == NULL &&
                  frame->lit_particles->failure == NULL && frame->lit_particles->cleanup == NULL &&
                  frame->sprite_particles != NULL && frame->sprite_particles->returned &&
                  !frame->sprite_particles->lit && frame->sprite_particles->empty &&
                  frame->sprite_particles->source_pc == 311 &&
                  frame->sprite_particles->orientation_values == 5 &&
                  frame->sprite_particles->origins_stored == 3 &&
                  frame->sprite_particles->size_reads == 3 &&
                  frame->sprite_particles->visited == 0 && !frame->sprite_particles->nw_captured &&
                  frame->sprite_particles->owner == NULL &&
                  frame->sprite_particles->camera == NULL &&
                  frame->sprite_particles->current == NULL && frame->sprite_particles->nw == NULL &&
                  frame->sprite_particles->failure == NULL &&
                  frame->sprite_particles->cleanup == NULL && frame->failure->particles == NULL &&
                  frame->first_hit_read && frame->translucent_fog_returned &&
                  frame->translucent_fog != NULL && frame->translucent_fog != frame->particle_fog &&
                  frame->translucent_texture_bound && frame->fancy_graphics_read &&
                  frame->translucent_returned && frame->translucent_submission != NULL &&
                  frame->translucent_submission->returned &&
                  frame->translucent_submission->source_pc == 896 &&
                  frame->translucent_submission->pass == 1 &&
                  frame->translucent_submission->result == frame->translucent_count &&
                  frame->translucent_submission->dirty_polls == 10 &&
                  frame->translucent_submission->frames == NULL &&
                  frame->translucent_submission->rebuild == NULL &&
                  frame->translucent_submission->owner == NULL &&
                  frame->translucent_submission->camera == NULL &&
                  frame->translucent_submission->batch_camera == NULL &&
                  frame->translucent_submission->math == NULL &&
                  frame->translucent_submission->failure == NULL &&
                  frame->translucent_submission->cleanup == NULL &&
                  frame->translucent_state_restored && frame->selection_zoom_read &&
                  frame->selection_player_read && frame->selection_player &&
                  frame->selection_hit_read && !frame->selection_hit_present &&
                  !frame->selection_water_read && !frame->selection_alpha_disabled &&
                  !frame->selection_current_returned && frame->selection_hit == NULL &&
                  frame->selection_stack == NULL);
            CHECK(frame->precipitation_returned && frame->precipitation != NULL &&
                  frame->precipitation->returned && frame->precipitation->source_pc == 18 &&
                  frame->precipitation->strength_read && !frame->precipitation->camera_captured &&
                  frame->weather_fog_disabled && frame->cloud_fog_returned &&
                  frame->cloud_fog_enabled && frame->cloud_fog != NULL &&
                  frame->cloud_fog->returned);
            CHECK(frame->clouds_returned && frame->clouds != NULL && frame->clouds->returned &&
                  frame->clouds->fancy && frame->clouds->passes == 2 &&
                  frame->clouds->draws == 72 && frame->post_cloud_fog_disabled &&
                  frame->post_cloud_fog_returned && frame->post_cloud_fog != NULL &&
                  frame->post_cloud_fog->returned && frame->hand_zoom_read &&
                  frame->hand_depth_cleared && frame->hand_returned && frame->hand != NULL &&
                  frame->hand->returned && frame->hand->source_pc == 163 &&
                  frame->hand->matrix_pushed && frame->hand->matrix_popped &&
                  frame->hand->held_returned && frame->hand->overlays_returned &&
                  frame->hand->held_item != NULL && frame->hand->overlays != NULL &&
                  frame->hand->held_item->arm_returned && frame->hand->held_item->skin_returned &&
                  frame->hand->held_item->renderer_returned &&
                  frame->hand->held_item->pose_returned && frame->hand->held_item->faces == 6 &&
                  frame->hand->held_item->vertices == 24 && frame->hand->held_item->owner == NULL &&
                  frame->hand->held_item->failure == NULL && frame->hand->overlays->returned &&
                  frame->hand->overlays->alpha_restored && frame->tail_anaglyph_read &&
                  !frame->tail_anaglyph && frame->failure->hand == NULL);
            const bool replay_expected = frame->fancy_graphics && frame->translucent_count > 0;
            CHECK(frame->translucent_replay_returned == replay_expected);
            if (replay_expected) {
                CHECK(frame->translucent_replay != NULL && frame->translucent_replay->returned &&
                      frame->translucent_replay->source_pc == 29 &&
                      frame->translucent_replay->pass == 1 &&
                      frame->translucent_replay->dirty_polls == 0 &&
                      frame->translucent_replay->subsets == 0 &&
                      frame->translucent_replay->configured_batches == 0 &&
                      frame->translucent_replay->selected_chunks == 0 &&
                      frame->translucent_replay->owner == NULL &&
                      frame->translucent_replay->camera == NULL &&
                      frame->translucent_replay->batch_camera == NULL &&
                      frame->translucent_replay->math == NULL &&
                      frame->translucent_replay->frames == NULL &&
                      frame->translucent_replay->rebuild == NULL &&
                      frame->translucent_replay->failure == NULL &&
                      frame->translucent_replay->cleanup == NULL);
            } else {
                CHECK(frame->translucent_replay == NULL);
            }
            /* Source updates finish and retire their borrowed cache/call views.
             * The following submission may recenter those SAME DKs, which
             * invalidates their earlier geometry until a later source update. */
            for (const mc_client_source_dk *dk = frame->render_global->chunk_owners; dk != NULL;
                 dk = dk->next) {
                if (!dk->fields.E || dk->fields.u || dk->fields.p == NULL || dk->fields.p[0])
                    continue;
                mc_client_source_list_state compiled;
                CHECK(mc_client_display_list_inspect(f.app->display, dk->fields.C, &compiled) &&
                      compiled.defined && !compiled.building && compiled.draws > 0 &&
                      compiled.vertices > 0);
            }
            /* Observe the returned World draw's back buffer before
             * separate component controls change its inherited GL state. */
            const size_t prefix_length = strlen(argv[3]);
            static const char prefix_suffix[] = ".world-prefix.png";
            CHECK(prefix_length <= SIZE_MAX - sizeof(prefix_suffix));
            char *prefix_path = malloc(prefix_length + sizeof(prefix_suffix));
            CHECK(prefix_path != NULL);
            memcpy(prefix_path, argv[3], prefix_length);
            memcpy(prefix_path + prefix_length, prefix_suffix, sizeof(prefix_suffix));
            CHECK(capture(&f, prefix_path) == 0);
            free(prefix_path);
            if (light_control) {
                CHECK(light_batch_controls(&f) == 0);
                goto loaded_draw_controls_done;
            }
            if (animals_load) {
                CHECK(animal_controls(&f, animal, argv[3]) == 0);
                goto loaded_draw_controls_done;
            }
            if (repeat_load) {
                CHECK(repeat_statistics_controls(&f, false) == 0);
                puts("WORLD_REPEAT profile=existing-inventory-player "
                     "statistics-input=pre-bootstrap-unsent-file open-inventory=1 "
                     "canonical=ep.f notice-inactive=1");
                mc_client_source_vy *mouse_owner = f.app->C;
                mc_client_mouse_helper_state mouse_before, mouse_after;
                mc_client_sdk_input_state input_before, input_after;
                CHECK(mc_client_mouse_helper_inspect(mouse_owner, &mouse_before) &&
                      mc_client_sdk_input_inspect(mc_client_display_inputs(f.app->display),
                                                  &input_before));
                const bool source_grab = f.app->N;
                /* This profile qualifies focused continuation. Exercise real
                 * focus loss/recovery in the transport before entering source
                 * work; source grab, Canvas and PX timing fields stay untouched. */
                CHECK(SDL_HideWindow(mc_client_display_window(f.app->display)) &&
                      display_focus(&f, false) == 0 && display_focus(&f, true) == 0);
                CHECK(f.app->C == mouse_owner &&
                      mc_client_mouse_helper_inspect(mouse_owner, &mouse_after) &&
                      mc_client_sdk_input_inspect(mc_client_display_inputs(f.app->display),
                                                  &input_after) &&
                      mouse_after.source_c == mouse_before.source_c && f.app->N == source_grab &&
                      input_before.grabbed == input_after.grabbed);
                puts("WORLD_FOCUS hidden-inactive=1 restored-active=1 component-preserved=1 "
                     "source-grab-preserved=1");
                CHECK(repeat_sheep_input(&f) == 0);
                /* Real elapsed time lets the existing Timer schedule source
                 * simulation before the guaranteed visible Entity reaches N.
                 * No Timer field, tick callback or render branch is replaced. */
                SDL_Delay(100);
                repeat_render_control render_control = {0};
                CHECK(repeat_model_graph(&f, &render_control, false) == 0);
                const uint64_t tick_returns_before = f.state.world_tick_returns;
                const uint64_t world_draws_before = f.state.world_draws;
                const uint64_t repeat_started = SDL_GetTicks();
                const uint64_t repeat_deadline = repeat_started + 60000;
                while (!render_control.reused && render_control.frames < 8 &&
                       SDL_GetTicks() < repeat_deadline) {
                    CHECK(display_focus(&f, true) == 0);
                    const bool returned = mc_client_menu_loop_frame(f.loop);
                    CHECK(mc_client_menu_loop_inspect(f.loop, &f.state));
                    if (!returned)
                        fprintf(stderr, "Repeated loaded frame retained %s.%s:%" PRId64 " %s\n",
                                f.state.source_class == NULL ? "native" : f.state.source_class,
                                f.state.source_method == NULL ? "frame" : f.state.source_method,
                                f.state.source_pc,
                                f.state.operation == NULL ? "none" : f.state.operation);
                    CHECK(returned);
                    CHECK(repeat_returned_frame(&f, &render_control) == 0);
                    if (!render_control.reused)
                        SDL_Delay(100); /* Actual Timer chooses ticks from real wall time. */
                }
                if (!render_control.reused) {
                    mc_entity *sheep = repeat_live_sheep(&f);
                    fprintf(
                        stderr,
                        "Repeated Sheep visibility deadline: elapsed-seconds=%" PRIu64
                        " limit-seconds=60 "
                        "frames=%zu delayed=%zu undrawn=%zu "
                        "entity-frames=%zu paused=%d screen=%d player=(%.17g,%.17g,%.17g) "
                        "player-prev=(%.17g,%.17g,%.17g) yaw=%.9g pitch=%.9g "
                        "sheep-ticks=%d sheep=(%.17g,%.17g,%.17g) "
                        "sheep-prev=(%.17g,%.17g,%.17g)\n",
                        (SDL_GetTicks() - repeat_started) / UINT64_C(1000), render_control.frames,
                        render_control.delayed_frames, render_control.undrawn_frames,
                        render_control.entity_frames, (int)atomic_load(&f.app->o),
                        f.app->r == NULL ? -1 : (int)f.app->r->kind, f.app->i->base.x,
                        f.app->i->base.y, f.app->i->base.z, f.app->i->base.bl, f.app->i->base.bm,
                        f.app->i->base.bn, (double)f.app->i->base.yaw, (double)f.app->i->base.aT,
                        sheep == NULL ? -1 : sheep->base.bt, sheep == NULL ? 0.0 : sheep->base.x,
                        sheep == NULL ? 0.0 : sheep->base.y, sheep == NULL ? 0.0 : sheep->base.z,
                        sheep == NULL ? 0.0 : sheep->base.bl, sheep == NULL ? 0.0 : sheep->base.bm,
                        sheep == NULL ? 0.0 : sheep->base.bn);
                }
                CHECK(render_control.compiled && render_control.reused &&
                      render_control.entity_frames >= 2 && render_control.frames >= 2 &&
                      render_control.frames <= 8 &&
                      render_control.frames == render_control.entity_frames +
                                                   render_control.delayed_frames +
                                                   render_control.undrawn_frames &&
                      f.state.world_tick_returns > tick_returns_before &&
                      f.state.world_draws == world_draws_before + render_control.frames &&
                      repeat_statistics_controls(&f, false) == 0);
                printf("WORLD_FRAME first-returned=1 continuation=returned-sheep "
                       "frames=%zu entity-frames=%zu delayed-frames=%zu undrawn-frames=%zu "
                       "source=px.a:1013 entities=n.a:569 hud=uq.a:2101 "
                       "mob=dl renderer=xy primary=mw secondary=ea exact-declared-actor=1 "
                       "renderer-map=29 constructor-owners=28 local-player-alias=gs "
                       "same-world-and-classes=1 compiled-parts=%zu reused-parts=%zu "
                       "compiled-faces=%zu compiled-vertices=%zu part-lists=12 "
                       "part-source-vertices=24 part-list-vertices=36 stable-list-ids=1 "
                       "shadow-quads=%zu checked-draw-retirement=1 copied-receipts=1 "
                       "tick-return=1328 scheduler=198 completed-random-calls=%" PRIu64
                       " completed-chunks=%zu completed-samples=%" PRIu64 " display-calls=%" PRIu64
                       " display-samples=%" PRIu64 " display-random=%" PRIu64 "\n",
                       render_control.frames, render_control.entity_frames,
                       render_control.delayed_frames, render_control.undrawn_frames,
                       render_control.compile_parts, render_control.reuse_parts,
                       render_control.compile_faces, render_control.compile_vertices,
                       render_control.shadow_quads, f.state.random_chunk_calls,
                       f.state.last_random_chunk_count, f.state.random_chunk_samples,
                       f.state.random_display_calls, f.state.random_display_samples,
                       f.state.random_display_random_calls);
                CHECK(capture(&f, argv[3]) == 0);
            }
            if (repeat_load)
                goto loaded_draw_controls_done;
            if (!display_active)
                CHECK(f.app->r != NULL && f.app->r->kind == MC_CLIENT_SCREEN_PAUSE);
            CHECK(f.state.gameplay_input != NULL && f.state.gameplay_input->returned &&
                  f.state.gameplay_input->source_pc == 1073 && f.state.world_tick != NULL &&
                  f.state.world_tick->returned && !f.state.world_tick->failed);
            CHECK(pause_controls(&f) == 0);
            CHECK(world_display_controls(&f, initial->classes) == 0);
            CHECK(mc_test_renderer_focus(f.app) == 0);
            CHECK(gameplay_controls(&f) == 0);
            CHECK(mc_test_world_refresh_entity(f.app) == 0);
            mc_entity_vector_snapshot before_pick, after_pick;
            CHECK(mc_entity_vector_get_snapshot(f.app->entities, &before_pick) == MC_ENTITY_OK);
            mc_client_entity_renderer_pick_call *pick = NULL;
            CHECK(mc_client_entity_renderer_pick_begin(f.app->t,
                                                       mc_client_bootstrap_math(f.bootstrap), 1.0f,
                                                       &pick) == MC_CLIENT_PICK_OK &&
                  pick != NULL);
            mc_client_entity_renderer_pick_state picked;
            CHECK(mc_client_entity_renderer_pick_inspect(pick, &picked) && picked.source_returned &&
                  picked.source_pc == 452 && picked.block_published && picked.target_cleared &&
                  picked.query_returned && picked.block_reach == 4.0 &&
                  picked.entity_reach <= 3.0 && picked.owner == f.app->t && picked.app == f.app &&
                  picked.origin != NULL && picked.look != NULL && picked.endpoint != NULL);
            CHECK(mc_entity_vector_get_snapshot(f.app->entities, &after_pick) == MC_ENTITY_OK &&
                  after_pick.cursor > before_pick.cursor);
            CHECK(mc_client_entity_renderer_pick_retire(&pick) == MC_CLIENT_PICK_OK &&
                  pick == NULL);
            if (terminal_admission)
                CHECK(mc_test_render_global_entities(f.app,
                                                     mc_client_bootstrap_math(f.bootstrap)) == 0);
            /* These component controls above use their declared Player-only
             * graph. The separate unpaused control now publishes real Mobs;
             * the dedicated terminal-admission profile keeps its one-Player
             * input, while load/repeat qualify the populated continuation. */
            if (!terminal_admission)
                CHECK(world_tick_controls(&f) == 0);
            printf("WORLD_LOAD ready=1 terrain=289 stored-marker=57 player=1 input=1 "
                   "renderer=1 effects=1 publication=1 view=1 returned=130 picker=452 "
                   "input=1073 paused-tick=1328 world-frame=1013 hud=2101 frame-returned=%d\n",
                   f.state.render_returned ? 1 : 0);
        loaded_draw_controls_done:;

        } else {
            CHECK(load.needs_conversion && !load.handler_opened && load.world == NULL &&
                  load.player == NULL && !load.world_published);
        }
        if (!loading_world) {
            const mc_client_world_load_site *reached =
                load.failure->child.class_name == NULL ? &load.failure->site : &load.failure->child;
            CHECK(strcmp(f.state.source_class, reached->class_name) == 0 &&
                  strcmp(f.state.source_method, reached->method) == 0 &&
                  f.state.source_pc == reached->pc);
        }
        if (double_click) {
            /* MG85 failed before LG's n187/o194 and k400 stores. Both clicks
             * came through the real SDK and the formatter/clock is unmodified. */
            CHECK(next->fields.o->fields.n == previous_index &&
                  next->fields.o->fields.o == previous_click && next->fields.o->fields.k == -1.0f);
            printf("WORLD_SELECTION double-clock-gap=%" PRId64 " failed-callback-n-o-preserved=1\n",
                   mc_java_i64_sub(double_started, previous_click));
        }
        CHECK(next->fields.l && next->fields.m == 0 && f.app->controller != NULL);
        if (!loading_world)
            CHECK(f.app->r != NULL && f.app->r != &next->base &&
                  f.app->r->kind == MC_CLIENT_SCREEN_MAIN_MENU);
        mc_singleplayer_controller_snapshot controller;
        CHECK(mc_client_singleplayer_controller_read(
            (const mc_client_singleplayer_controller *)f.app->controller, &controller));
        CHECK(controller.phase == MC_SINGLEPLAYER_CONTROLLER_READY && controller.a == f.app &&
              controller.constructor_pc == 40 && controller.c == -1 && controller.d == -1 &&
              controller.e == -1 && controller.failed_player == NULL);
        CHECK(loading_world ||
              (f.app->active_world == NULL && f.app->h == NULL && f.app->i == NULL && !f.app->N));
    } else {
        CHECK(push_button(&f, initial, 4, true) == 0 && push_button(&f, initial, 4, false) == 0 &&
              transfer(&f) == 0);
        const uint64_t quit_deadline = SDL_GetTicks() + 3000;
        while (f.state.running && SDL_GetTicks() < quit_deadline) {
            CHECK(mc_client_menu_loop_frame(f.loop));
            CHECK(mc_client_menu_loop_inspect(f.loop, &f.state));
            SDL_Delay(1);
        }
        CHECK(!f.state.running && !f.state.failed && !atomic_load(&f.app->J));
    }
    printf("WORLD_SELECTION mode=%s frames=%" PRIu64 " ticks=%" PRIu64
           " mouse=%zu keyboard=%zu held-entry=1 row=1 cancel-same-parent=1 select-frontier=%d\n",
           argv[2], f.state.frames, f.state.ticks, f.state.mouse_events, f.state.keyboard_events,
           populated ? 1 : 0);
    if (!loading_world)
        CHECK(terminal_prepare_controls(&f, external) == 0);
    if (terminal_admission)
        CHECK(loaded_terminal_admission_controls(&f) == 0);
    if (loading_world && !terminal_admission && !repeat_load && !light_control && !animals_load) {
        mc_world_runtime *world = f.app->active_world;
        mc_entity *player = f.app->h;
        mc_client_render_global *renderer = f.app->g;
        mc_client_render_global_error *error = calloc(1, sizeof(*error));
        CHECK(error != NULL &&
              mc_client_render_global_retain(renderer, error) == MC_CLIENT_RENDER_GLOBAL_OK);
        CHECK(!mc_client_menu_loop_retire(&f.loop) && f.loop != NULL);
        mc_client_screen_transition_state transitions;
        mc_client_world_load_state load;
        CHECK(mc_client_screen_transitions_inspect(f.app->screen_transitions, &transitions) &&
              transitions.world_load != NULL &&
              mc_client_world_load_inspect(transitions.world_load, &load) && load.source_returned &&
              load.cleanup->status == MC_CLIENT_WORLD_LOAD_BUSY && load.cleanup->renderer == NULL &&
              load.cleanup->render_global != NULL &&
              load.cleanup->render_global->status == MC_CLIENT_RENDER_GLOBAL_BUSY &&
              renderer->fields.k == world && f.app->active_world == world && f.app->h == player &&
              f.app->i == player);
        CHECK(mc_client_render_global_release(&renderer, error) == MC_CLIENT_RENDER_GLOBAL_OK);
        free(error);
        mc_client_effect_renderer *effects = f.app->effect_renderer;
        CHECK(mc_client_effect_renderer_retain(effects) == MC_EFFECT_RENDERER_OK);
        CHECK(!mc_client_menu_loop_retire(&f.loop) && f.loop != NULL);
        mc_effect_renderer_snapshot state;
        CHECK(mc_client_effect_renderer_read(effects, &state) && state.world == world &&
              f.app->active_world == world && f.app->h == player && f.app->i == player);
        CHECK(mc_client_effect_renderer_release(effects) == MC_EFFECT_RENDERER_OK);
    }
    if (!loading_world || terminal_admission || repeat_load || light_control || animals_load) {
        CHECK(normal_terminal_retirement_controls(&f, &external, loading_world) == 0);
    } else {
        CHECK(!mc_client_menu_loop_retire(&f.loop) && f.loop != NULL && external != NULL);
        CHECK(mc_client_bootstrap_app(f.bootstrap) == f.app &&
              f.app->native_lifetime_phase == MC_CLIENT_APPLICATION_LIFETIME_REGISTERED);
        CHECK(mc_client_screen_release(&external, NULL) == MC_CLIENT_SCREEN_OK && external == NULL);
        CHECK(mc_client_menu_loop_retire(&f.loop) && f.loop == NULL);
    }
close_bootstrap:
    close_deadline = SDL_GetTicks() + 35000;
    do {
        closed = mc_client_bootstrap_close(&f.bootstrap);
        if (closed == MC_CLIENT_BOOTSTRAP_BUSY)
            SDL_Delay(1);
    } while (closed == MC_CLIENT_BOOTSTRAP_BUSY && SDL_GetTicks() < close_deadline);
    CHECK(closed == MC_CLIENT_BOOTSTRAP_OK && f.bootstrap == NULL);
    printf("WORLD_SELECTION mode=%s cleanup=1 checks=%u\n", argv[2], checks);
    return 0;
}
