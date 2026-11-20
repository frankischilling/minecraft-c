/* Actual expired Spawner through ordinary Timer-selected SDK frames. */
int mc_spawner_tick_included_mining_main(int, char **);
#define MC_MINING_FIXTURE_MAIN mc_spawner_tick_included_mining_main
#include "gameplay/test_mining.c"
#undef MC_MINING_FIXTURE_MAIN

#include "mc/entity/mob/construction.h"
#include "mc/entity/nbt/storage.h"
#include "mc/world/runtime/world_access.h"
#include "world/runtime/tile_entities/tile_entities_internal.h"

enum { TICK_PIG_LIMIT = 128 };

static struct {
    fixture *fixture;
    mc_world_access *access;
    mc_world_runtime *world;
    uintptr_t preview, candidate;
    bool failure_profile, registered, armed, self_removed, failed;
    size_t post_smoke;
    size_t cell_calls, range_calls, sound_calls, dirty_calls, removed_calls, sky_calls,
        auxiliary_calls;
    bool capture_chunks;
    size_t chunk_count;

    struct {
        int32_t x, z;
    } chunks[TICK_PIG_LIMIT];
} tick_observer;

/* Passive native provenance only: this uses N's existing held occurrences,
 * without a World/Chunk getter that could create or register a missing Tile. */
static mc_tile_entity *tick_tile(const fixture *f) {
    if (f->app->g == NULL)
        return NULL;
    for (const mc_render_global_tile_occurrence *h = f->app->g->tile_occurrences; h != NULL;
         h = h->next)
        if (h->active && h->tile != NULL && h->tile->kind == MC_TILE_ENTITY_SPAWNER &&
            h->tile->world == f->app->active_world && h->tile->x == 0 && h->tile->y == 64 &&
            h->tile->z == 2)
            return h->tile;
    return NULL;
}

static int tick_pigs(fixture *f, size_t *out) {
    const mc_world_reference_list *list = NULL;
    int32_t count;
    CHECK(mc_world_runtime_render_entities_list(f->app->active_world, &list, NULL) ==
              MC_WORLD_STORAGE_OK &&
          mc_world_runtime_render_entity_list_count(f->app->active_world, list, &count, NULL) ==
              MC_WORLD_STORAGE_OK);
    size_t pigs = 0;
    for (int32_t i = 0; i < count; ++i) {
        mc_entity *entity = NULL;
        CHECK(mc_world_runtime_render_entity_list_get(f->app->active_world, list, i, &entity,
                                                      NULL) == MC_WORLD_STORAGE_OK);
        if (entity == NULL)
            continue;
        mc_entity_snapshot state;
        mc_entity_class_literal_view type;
        CHECK(mc_entity_get_snapshot(entity, &state) == MC_ENTITY_OK &&
              mc_entity_class_literal_inspect(state.class_literal, &type) == MC_ENTITY_OK);
        if (type.kind == MC_ENTITY_CLASS_KIND_PIG) {
            CHECK(state.base.world == f->app->active_world && state.base.bF &&
                  (uintptr_t)entity != tick_observer.preview);
            ++pigs;
        }
    }
    *out = pigs;
    return 0;
}

/* These are authored passive observer slots, alongside the real App N
 * receiver. They acknowledge only this observer's notification; they neither
 * replace a World leaf nor suppress any other registered receiver. */
static mc_world_storage_status tick_cell(void *context, mc_world_runtime *world, int32_t x,
                                         int32_t y, int32_t z, mc_world_storage_error *error) {
    (void)context;
    (void)world;
    (void)x;
    (void)y;
    (void)z;
    (void)error;
    ++tick_observer.cell_calls;
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status tick_range(void *context, mc_world_runtime *world, int32_t min_x,
                                          int32_t min_y, int32_t min_z, int32_t max_x,
                                          int32_t max_y, int32_t max_z,
                                          mc_world_storage_error *error) {
    (void)context;
    (void)world;
    (void)min_x;
    (void)min_y;
    (void)min_z;
    (void)max_x;
    (void)max_y;
    (void)max_z;
    (void)error;
    ++tick_observer.range_calls;
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status tick_sound(void *context, mc_world_runtime *world,
                                          const mc_registry_string *name, double x, double y,
                                          double z, float volume, float pitch,
                                          mc_world_storage_error *error) {
    (void)context;
    (void)world;
    (void)name;
    (void)x;
    (void)y;
    (void)z;
    (void)volume;
    (void)pitch;
    (void)error;
    ++tick_observer.sound_calls;
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status tick_dirty(void *context, mc_world_runtime *world, int32_t x,
                                          int32_t y, int32_t z, mc_tile_entity *tile,
                                          mc_world_storage_error *error) {
    (void)context;
    (void)world;
    (void)x;
    (void)y;
    (void)z;
    (void)tile;
    (void)error;
    ++tick_observer.dirty_calls;
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status tick_removed(void *context, mc_world_runtime *world,
                                            mc_entity *entity, mc_world_storage_error *error) {
    (void)context;
    (void)world;
    (void)entity;
    (void)error;
    ++tick_observer.removed_calls;
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status tick_sky(void *context, mc_world_runtime *world,
                                        mc_world_storage_error *error) {
    (void)context;
    (void)world;
    (void)error;
    ++tick_observer.sky_calls;
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status tick_auxiliary(void *context, mc_world_runtime *world,
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
    (void)error;
    ++tick_observer.auxiliary_calls;
    return MC_WORLD_STORAGE_OK;
}

/* Last pre-frame World.b membership while the genuine save/title helper is
 * running. The actual leave frame has no intervening World tick before save.
 * Retain scalar coordinates only, never a released old-World Entity borrow. */
static int tick_capture_chunks(fixture *f) {
    const mc_world_reference_list *list = NULL;
    int32_t count;
    CHECK(mc_world_runtime_render_entities_list(f->app->active_world, &list, NULL) ==
              MC_WORLD_STORAGE_OK &&
          mc_world_runtime_render_entity_list_count(f->app->active_world, list, &count, NULL) ==
              MC_WORLD_STORAGE_OK);
    tick_observer.chunk_count = 0;
    for (int32_t i = 0; i < count; ++i) {
        mc_entity *entity = NULL;
        CHECK(mc_world_runtime_render_entity_list_get(f->app->active_world, list, i, &entity,
                                                      NULL) == MC_WORLD_STORAGE_OK);
        if (entity == NULL)
            continue;
        mc_entity_snapshot state;
        mc_entity_class_literal_view type;
        CHECK(mc_entity_get_snapshot(entity, &state) == MC_ENTITY_OK &&
              mc_entity_class_literal_inspect(state.class_literal, &type) == MC_ENTITY_OK);
        if (type.kind != MC_ENTITY_CLASS_KIND_PIG)
            continue;
        CHECK(state.base.world == f->app->active_world && state.base.bF);
        size_t j = 0;
        while (j < tick_observer.chunk_count && (tick_observer.chunks[j].x != state.base.bG ||
                                                 tick_observer.chunks[j].z != state.base.bI))
            ++j;
        if (j == tick_observer.chunk_count) {
            CHECK(j < TICK_PIG_LIMIT);
            tick_observer.chunks[j].x = state.base.bG;
            tick_observer.chunks[j].z = state.base.bI;
            ++tick_observer.chunk_count;
        }
    }
    return 0;
}

typedef struct tick_pig_readback {
    uint64_t pos[3], motion[3];
    uint32_t rotation[2];
    int32_t health, hurt, death, attack, fire, air;
    bool grounded, saddle;
} tick_pig_readback;

static const mc_nbt_tag *tick_nbt_field(const mc_nbt_tag *root, const char *name) {
    uint16_t units[16];
    const size_t length = strlen(name);
    if (length > sizeof(units) / sizeof(units[0]))
        return NULL;
    for (size_t i = 0; i < length; ++i)
        units[i] = (uint8_t)name[i];
    return mc_nbt_compound_get(root, units, length);
}

static int tick_pig_input(const mc_entity *entity, tick_pig_readback *out) {
    /* Existing immutable import provenance, not a new read, semantic writer,
     * source attachment or pose reset. It survives ordinary post-load ticks. */
    const mc_nbt_tag *root = mc_entity_storage_source(entity);
    CHECK(root != NULL && root->type == MC_NBT_COMPOUND);
    const mc_nbt_tag *id = tick_nbt_field(root, "id");
    static const uint16_t pig[] = {'P', 'i', 'g'};
    CHECK(id != NULL && id->type == MC_NBT_STRING && id->value.string.length == 3 &&
          memcmp(id->value.string.units, pig, sizeof(pig)) == 0);
    static const char *vectors[] = {"Pos", "Motion", "Rotation"};
    for (size_t i = 0; i < 3; ++i) {
        const mc_nbt_tag *list = tick_nbt_field(root, vectors[i]);
        const size_t count = i == 2 ? 2 : 3;
        const mc_nbt_type type = i == 2 ? MC_NBT_FLOAT : MC_NBT_DOUBLE;
        CHECK(list != NULL && list->type == MC_NBT_LIST && list->value.list.length == count &&
              list->value.list.element_type == type && list->value.list.items != NULL);
        for (size_t j = 0; j < count; ++j) {
            const mc_nbt_tag *value = list->value.list.items[j];
            CHECK(value != NULL && value->type == type);
            if (i == 2)
                out->rotation[j] = value->value.float_bits;
            else if (i == 1)
                out->motion[j] = value->value.double_bits;
            else
                out->pos[j] = value->value.double_bits;
        }
    }
    static const char *scalars[] = {"Health", "HurtTime", "DeathTime", "AttackTime", "Fire", "Air"};
    int32_t *destinations[] = {&out->health, &out->hurt, &out->death,
                               &out->attack, &out->fire, &out->air};
    for (size_t i = 0; i < sizeof(scalars) / sizeof(scalars[0]); ++i) {
        const mc_nbt_tag *tag = tick_nbt_field(root, scalars[i]);
        CHECK(tag != NULL && tag->type == MC_NBT_SHORT);
        *destinations[i] = tag->value.short_value;
    }
    const mc_nbt_tag *grounded = tick_nbt_field(root, "OnGround");
    const mc_nbt_tag *saddle = tick_nbt_field(root, "Saddle");
    CHECK(grounded != NULL && grounded->type == MC_NBT_BYTE && saddle != NULL &&
          saddle->type == MC_NBT_BYTE);
    out->grounded = grounded->value.byte_value != 0;
    out->saddle = saddle->value.byte_value != 0;
    return 0;
}

static void tick_pig_print(const char *phase, size_t index, int32_t chunk_x, int32_t chunk_z,
                           const tick_pig_readback *row) {
    printf("SPAWNER_TICK_PIG phase=%s index=%zu chunk-x=%" PRId32 " chunk-z=%" PRId32
           " pos=%016" PRIx64 ",%016" PRIx64 ",%016" PRIx64 " motion=%016" PRIx64 ",%016" PRIx64
           ",%016" PRIx64 " rotation=%08" PRIx32 ",%08" PRIx32 " health=%" PRId32 " hurt=%" PRId32
           " death=%" PRId32 " attack=%" PRId32 " fire=%" PRId32 " air=%" PRId32
           " grounded=%d saddle=%d\n",
           phase, index, chunk_x, chunk_z, row->pos[0], row->pos[1], row->pos[2], row->motion[0],
           row->motion[1], row->motion[2], row->rotation[0], row->rotation[1], row->health,
           row->hurt, row->death, row->attack, row->fire, row->air, (int)row->grounded,
           (int)row->saddle);
}

static int tick_reload_readback(fixture *f, size_t *out) {
    CHECK(tick_observer.chunk_count > 0);
    for (size_t i = 0; i < tick_observer.chunk_count; ++i) {
        const int32_t x = tick_observer.chunks[i].x, z = tick_observer.chunks[i].z;
        bool present = false;
        CHECK(x >= INT32_MIN / 16 && x <= INT32_MAX / 16 && z >= INT32_MIN / 16 &&
              z <= INT32_MAX / 16 &&
              mc_world_runtime_render_block_present(f->app->active_world, x * 16, 64, z * 16,
                                                    &present, NULL) == MC_WORLD_STORAGE_OK);
        printf("SPAWNER_TICK_CHUNK index=%zu x=%" PRId32 " z=%" PRId32 " loaded=%d nonloading=1\n",
               i, x, z, (int)present);
    }
    const mc_world_reference_list *list = NULL;
    int32_t count;
    CHECK(mc_world_runtime_render_entities_list(f->app->active_world, &list, NULL) ==
              MC_WORLD_STORAGE_OK &&
          mc_world_runtime_render_entity_list_count(f->app->active_world, list, &count, NULL) ==
              MC_WORLD_STORAGE_OK);
    size_t pigs = 0;
    for (int32_t i = 0; i < count; ++i) {
        mc_entity *entity = NULL;
        CHECK(mc_world_runtime_render_entity_list_get(f->app->active_world, list, i, &entity,
                                                      NULL) == MC_WORLD_STORAGE_OK);
        if (entity == NULL)
            continue;
        mc_entity_snapshot state;
        mc_entity_class_literal_view type;
        CHECK(mc_entity_get_snapshot(entity, &state) == MC_ENTITY_OK &&
              mc_entity_class_literal_inspect(state.class_literal, &type) == MC_ENTITY_OK);
        if (type.kind != MC_ENTITY_CLASS_KIND_PIG)
            continue;
        mc_mob_state mob;
        CHECK(pigs < TICK_PIG_LIMIT && mc_mob_inspect(entity, &mob) &&
              state.base.world == f->app->active_world && state.base.bF &&
              (uintptr_t)entity != tick_observer.preview && mob.watcher_16_present &&
              mob.watcher_16_type == 0);
        tick_pig_readback current = {0}, input = {0};
        const double pos[] = {state.base.x, state.base.y, state.base.z};
        const double motion[] = {state.base.velocity_x, state.base.velocity_y,
                                 state.base.velocity_z};
        const float rotation[] = {state.base.yaw, state.base.aT};
        memcpy(current.pos, pos, sizeof(pos));
        memcpy(current.motion, motion, sizeof(motion));
        memcpy(current.rotation, rotation, sizeof(rotation));
        current.health = mob.living.Y;
        current.hurt = mob.living.aa;
        current.death = mob.living.ad;
        current.attack = mob.living.ae;
        current.fire = state.base.bv;
        current.air = state.base.bz;
        current.grounded = state.base.aX;
        current.saddle = (mob.watcher_16 & 1) != 0;
        CHECK(tick_pig_input(entity, &input) == 0);
        tick_pig_print("runtime", pigs, state.base.bG, state.base.bI, &current);
        tick_pig_print("input", pigs, state.base.bG, state.base.bI, &input);
        ++pigs;
    }
    *out = pigs;
    return 0;
}

static mc_world_storage_status tick_added(void *context, mc_world_runtime *world, mc_entity *entity,
                                          mc_world_storage_error *error) {
    (void)context;
    (void)error;
    mc_entity_snapshot state;
    mc_entity_class_literal_view type;
    if (world->fields.L && mc_entity_get_snapshot(entity, &state) == MC_ENTITY_OK &&
        mc_entity_class_literal_inspect(state.class_literal, &type) == MC_ENTITY_OK &&
        type.kind == MC_ENTITY_CLASS_KIND_PIG && state.base.world == world && state.base.x > -4 &&
        state.base.x < 4 && state.base.y >= 63 && state.base.y <= 65 && state.base.z > -2 &&
        state.base.z < 6) {
        tick_observer.armed = true;
        tick_observer.candidate = (uintptr_t)entity;
    }
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status tick_particle(void *context, mc_world_runtime *world,
                                             const mc_registry_string *name, double x, double y,
                                             double z, double vx, double vy, double vz,
                                             mc_world_storage_error *error) {
    (void)context;
    (void)x;
    (void)y;
    (void)z;
    (void)vx;
    (void)vy;
    (void)vz;
    static const uint16_t smoke[] = {'s', 'm', 'o', 'k', 'e'};
    mc_registry_string_view text;
    if (!tick_observer.armed || name == NULL || !mc_registry_string_read(name, &text) ||
        text.length != sizeof(smoke) / sizeof(smoke[0]) ||
        memcmp(text.units, smoke, sizeof(smoke)) != 0)
        return MC_WORLD_STORAGE_OK;
    ++tick_observer.post_smoke;
    bool removed = false;
    const mc_world_storage_status status =
        mc_world_runtime_remove_world_access(world, tick_observer.access, &removed, error);
    if (status != MC_WORLD_STORAGE_OK || !removed)
        return status == MC_WORLD_STORAGE_OK ? MC_WORLD_STORAGE_UNSUPPORTED_STATE : status;
    tick_observer.registered = false;
    tick_observer.self_removed = true;
    tick_observer.failed = true;
    *error = (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR,
                                      "authored SDK Spawner post-publication smoke failure", 73};
    return MC_WORLD_STORAGE_IO_ERROR;
}

static int tick_before_frame(fixture *f) {
    if (tick_observer.capture_chunks && f != NULL && f->app->active_world != NULL)
        CHECK(tick_capture_chunks(f) == 0);
    if (!tick_observer.failure_profile || tick_observer.access != NULL || f == NULL ||
        f->app->active_world == NULL)
        return 0;
    const mc_world_access_callbacks callbacks = {.cell = tick_cell,
                                                 .range = tick_range,
                                                 .sound = tick_sound,
                                                 .particle = tick_particle,
                                                 .entity_added = tick_added,
                                                 .tile_entity_dirty = tick_dirty,
                                                 .entity_removed = tick_removed,
                                                 .sky_changed = tick_sky,
                                                 .auxiliary = tick_auxiliary};
    CHECK(mc_world_access_create(&callbacks, NULL, &tick_observer.access, NULL) ==
              MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(f->app->active_world, tick_observer.access, NULL) ==
              MC_WORLD_STORAGE_OK);
    tick_observer.world = f->app->active_world;
    tick_observer.registered = true;
    return 0;
}

static void tick_after_frame(fixture *f, bool returned) {
    if (!returned || f == NULL || !mc_client_menu_loop_inspect(f->loop, &f->state))
        return;
    const mc_client_world_frame_state *frame_state = f->state.world_frame;
    if (frame_state != NULL && frame_state->returned && frame_state->entities != NULL &&
        frame_state->entities->last_tile_draw.valid &&
        frame_state->entities->last_tile_draw.preview_identity != 0)
        tick_observer.preview = frame_state->entities->last_tile_draw.preview_identity;
}

static int tick_wait(fixture *f, const char *profile) {
    const uint64_t deadline = SDL_GetTicks() + 120000;
    mc_client_source_da *recovered[4] = {0};
    size_t pauses = 0;
    while (SDL_GetTicks() < deadline) {
        CHECK(save_title_focus(f) == 0);
        /* Declared input wait only. The actual Timer chooses ticks; no Timer,
         * World, pose, Random, delay or registration outcome is rewritten. */
        SDL_Delay(60);
        const bool returned = mining_observed_frame(f->loop);
        CHECK(mc_client_menu_loop_inspect(f->loop, &f->state));
        if (!returned) {
            if (tick_observer.failure_profile && tick_observer.failed)
                return 0;
            fprintf(stderr, "Spawner SDK reached %s.%s:%" PRId64 ": %s\n",
                    f->state.source_class == NULL ? "native" : f->state.source_class,
                    f->state.source_method == NULL ? "frame" : f->state.source_method,
                    f->state.source_pc, f->state.operation == NULL ? "(none)" : f->state.operation);
            return 1;
        }
        if (f->app->active_world == NULL)
            continue;
        if (f->app->r != NULL) {
            size_t eligible;
            int64_t time;
            CHECK(save_title_resume(f, f->app->active_world, NULL, recovered, &pauses, &eligible,
                                    &time) == 0);
            continue;
        }
        mc_tile_entity *tile = tick_tile(f);
        if (tile == NULL || f->state.entity_tick_calls == 0 || f->state.spawner.smoke_calls == 0)
            continue;
        if (f->state.world_frame == NULL || !f->state.world_frame->returned ||
            f->state.world_frame->world != f->app->active_world ||
            !f->state.world_frame->entities_returned || f->state.hud == NULL ||
            !f->state.hud->returned)
            continue;
        CHECK(!f->state.spawner_saturated && tile->world == f->app->active_world && !tile->invalid);
        bool registered = false;
        for (size_t i = 0; i < tile->world->fields.c->count; ++i)
            registered = registered || tile->world->fields.c->items[i] == tile;
        CHECK(registered);
        if (strcmp(profile, "countdown") == 0) {
            if (tile->state.spawner.delay < 32767)
                return 0;
        } else if (!tick_observer.failure_profile && f->state.spawner.insertion_true > 0 &&
                   f->state.spawner.explode_calls >= 20 && f->state.spawner.delay_resets > 0) {
            size_t pigs = 0;
            CHECK(tick_pigs(f, &pigs) == 0);
            if (pigs > 0 && tick_observer.preview != 0)
                return 0;
        }
    }
    fprintf(stderr, "Spawner SDK timed out waiting for actual %s World.g branch\n", profile);
    return 1;
}

static int tick_stop(fixture *f, uint32_t frame_id) {
    SDL_Event event = {0};
    event.type = SDL_EVENT_WINDOW_CLOSE_REQUESTED;
    event.window.windowID = frame_id;
    CHECK(SDL_PushEvent(&event));
    mc_client_sdk_update_error update = {0};
    for (size_t i = 0; i < 100 && atomic_load(&f->app->J); ++i) {
        CHECK(
            mc_client_sdk_display_process_messages(mc_client_display_update_owner(f->app->display),
                                                   &update) == MC_CLIENT_SDK_UPDATE_OK);
        SDL_Delay(2);
    }
    mc_client_frame_state host;
    CHECK(!atomic_load(&f->app->J) &&
          mc_client_source_frame_inspect(mc_client_bootstrap_frame(f->bootstrap), &host) &&
          host.close_dispatched);
    puts("SPAWNER_TICK_STOP actual-frame-close=1 source-retry=0");
    return 0;
}

typedef struct tick_snapshot {
    mc_tile_entity_snapshot tile;
    mc_entity_base_fields candidate;
    mc_entity_bounds bounds;
    mc_java_random random;
    const void *world, *player, *screen, *entities, *tiles, *pending;
    size_t entities_count, tiles_count, pending_count;
    uint64_t ticks, world_ticks, entity_calls;
    int64_t time;
    bool tile_phase;
} tick_snapshot;

static int tick_snapshot_read(fixture *f, mc_tile_entity *tile, mc_entity *candidate,
                              tick_snapshot *out) {
    memset(out, 0, sizeof(*out));
    CHECK(mc_client_menu_loop_inspect(f->loop, &f->state) &&
          mc_tile_entity_get_snapshot(tile, &out->tile) == MC_TILE_ENTITY_OK && candidate != NULL &&
          candidate->bounds != NULL && candidate->random != NULL);
    out->candidate = candidate->base;
    out->bounds = *candidate->bounds;
    out->random = *candidate->random;
    mc_world_runtime *world = f->app->active_world;
    out->world = world;
    out->player = f->app->h;
    out->screen = f->app->r;
    out->entities = world->fields.b;
    out->tiles = world->fields.c;
    out->pending = world->fields.G;
    out->entities_count = world->fields.b->count;
    out->tiles_count = world->fields.c->count;
    out->pending_count = world->fields.G->count;
    out->tile_phase = world->fields.L;
    out->ticks = f->state.ticks;
    out->world_ticks = f->state.world_tick_returns;
    out->entity_calls = f->state.entity_tick_calls;
    CHECK(mc_world_info_time_get(world->fields.x, &out->time) == MC_WORLD_INFO_OK);
    return 0;
}

static int tick_failure(fixture *f, uint32_t frame_id) {
    const mc_client_world_tick *world_tick = f->state.world_tick;
    CHECK(f->state.failed && world_tick != NULL && world_tick->failed &&
          world_tick->entities.child == MC_WORLD_ENTITY_TICK_CHILD_SPAWNER &&
          world_tick->entities.status == MC_WORLD_STORAGE_IO_ERROR &&
          world_tick->entities.loop_bytecode == 390 &&
          world_tick->entities.spawner_bytecode == 571);
    const mc_world_spawner_tick_error *error = &world_tick->entities.child_error.spawner;
    mc_entity *candidate = error->candidate;
    mc_tile_entity *tile = tick_tile(f);
    size_t pigs = 0;
    CHECK(tile != NULL && candidate != NULL && candidate->base.world == f->app->active_world &&
          candidate->base.bF && (uintptr_t)candidate == tick_observer.candidate &&
          tick_observer.preview != 0 && (uintptr_t)candidate != tick_observer.preview &&
          tick_observer.self_removed && !tick_observer.registered &&
          tick_observer.post_smoke == 1 && tick_pigs(f, &pigs) == 0 && pigs > 0 &&
          tile->state.spawner.delay == 0 && tile->world->fields.L &&
          world_tick->entities.spawner.delay_resets == 0);
    puts("SPAWNER_TICK_FAILURE world-pc=390 cy-pc=571 published-pig=1 preview-distinct=1 "
         "observer-self-removed=1 delay-reset=0 owned-child=1");
    CHECK(tick_stop(f, frame_id) == 0);
    tick_snapshot before, after;
    CHECK(tick_snapshot_read(f, tile, candidate, &before) == 0 &&
          mc_client_menu_loop_terminal_prepare(f->loop) &&
          tick_snapshot_read(f, tile, candidate, &after) == 0 &&
          memcmp(&before, &after, sizeof(before)) == 0 &&
          f->state.world_tick->entities.child == MC_WORLD_ENTITY_TICK_CHILD_NONE);
    puts("SPAWNER_TICK_PREPARE source-unchanged=1 world-time-unchanged=1 ticks-unchanged=1 "
         "candidate-still-published=1 owned-child-ended=1");
    return 0;
}

static int tick_run(fixture *f, mining_work *work, const char *profile, uint32_t *frame_id,
                    uint32_t *canvas_id) {
    tick_observer.fixture = f;
    tick_observer.failure_profile = strcmp(profile, "failure") == 0;
    mining_extra_before_frame = tick_before_frame;
    mining_extra_after_frame = tick_after_frame;
    CHECK(save_title_host(f, "open", true, frame_id, canvas_id) == 0 &&
          mc_client_menu_loop_create(f->bootstrap, &f->loop) &&
          mc_client_menu_loop_inspect(f->loop, &f->state) &&
          repeat_statistics_controls(f, true) == 0 && save_title_focus(f) == 0 &&
          ticks(f, 2) == 0 && mining_choose(f, work, NULL, &work->epochs[0]) == 0);
    if (tick_observer.failure_profile)
        CHECK(mining_capture(f, work, "spawner-tick-before-failure.png") == 0);
    CHECK(tick_wait(f, profile) == 0);
    if (tick_observer.failure_profile) {
        CHECK(save_title_host(f, "after-spawner-tick", false, frame_id, canvas_id) == 0);
        return tick_failure(f, *frame_id);
    }
    mc_tile_entity *tile = tick_tile(f);
    size_t pigs = 0;
    int64_t time;
    CHECK(tile != NULL && tick_pigs(f, &pigs) == 0 &&
          mc_world_info_time_get(f->app->active_world->fields.x, &time) == MC_WORLD_INFO_OK &&
          mining_capture(f, work, "spawner-tick.png") == 0);
    const mc_world_spawner_tick_observation *o = &f->state.spawner;
    printf("SPAWNER_TICK_ACTIVE profile=%s entity-calls=%" PRIu64 " attempts=%zu factory=%zu "
           "predicates=%zu accepted=%zu inserted=%zu smoke=%zu flame=%zu explode=%zu resets=%zu "
           "pigs=%zu delay=%" PRId32 " time=%" PRId64 " active-tile=1 real-timer=1\n",
           profile, f->state.entity_tick_calls, o->attempts, o->factory_calls, o->predicate_calls,
           o->accepted, o->insertion_true, o->smoke_calls, o->flame_calls, o->explode_calls,
           o->delay_resets, pigs, tile->state.spawner.delay, time);
    CHECK(fflush(stdout) == 0);
    if (strcmp(profile, "success") == 0) {
        char title[8192];
        tick_observer.capture_chunks = true;
        CHECK(pigs > 0 && mining_path(work, "spawner-tick-title.png", title) == 0 &&
              save_title_run(f, NULL, title, false, 1, 0) == 0 &&
              reload_snapshot("spawner-tick-saved") == 0);
        tick_observer.capture_chunks = false;
        const size_t before_reload = f->state.spawner.insertion_true;
        CHECK(mining_choose(f, work, &work->epochs[0], &work->epochs[1]) == 0 &&
              tick_reload_readback(f, &pigs) == 0 && pigs > 0 &&
              f->state.spawner.insertion_true == before_reload &&
              mining_capture(f, work, "spawner-tick-reloaded.png") == 0);
        printf("SPAWNER_TICK_RELOAD pigs=%zu new-world=1 preview-distinct=1 "
               "ordinary-save-reselect=1 fresh-insertions=0\n",
               pigs);
    } else
        CHECK(pigs == 0 && o->factory_calls == 0 && tile->state.spawner.delay < 32767);
    CHECK(save_title_host(f, "after-spawner-tick", false, frame_id, canvas_id) == 0 &&
          tick_stop(f, *frame_id) == 0);
    return 0;
}

int main(int argc, char **argv) {
    CHECK(argc == 4 && (strcmp(argv[3], "countdown") == 0 || strcmp(argv[3], "success") == 0 ||
                        strcmp(argv[3], "failure") == 0));
    fixture *f = calloc(1, sizeof(*f));
    CHECK(f != NULL);
    mining_fixture = f;
    mining_work work = {.directory = argv[2]};
    const mc_client_application_options options = {.assets = argv[1],
                                                   .username = "Selection control",
                                                   .width = 854,
                                                   .height = 480,
                                                   .hidden = false};
    int result = 1;
    uint32_t frame_id = 0, canvas_id = 0;
    if (mc_client_bootstrap_open_standalone(&options, &f->bootstrap) == MC_CLIENT_BOOTSTRAP_OK) {
        f->app = mc_client_bootstrap_app(f->bootstrap);
        result = tick_run(f, &work, argv[3], &frame_id, &canvas_id);
    }
    /* Authored observer teardown, including unexpected fixture failures. This
     * removes only our own real World.u occurrence before App terminal work;
     * no Source field or failed call result is repaired or replayed. */
    if (tick_observer.registered) {
        bool removed = false;
        if (mc_world_runtime_remove_world_access(tick_observer.world, tick_observer.access,
                                                 &removed, NULL) != MC_WORLD_STORAGE_OK ||
            !removed) {
            fprintf(stderr, "Spawner tick observer teardown could not remove registration\n");
            result = 1;
        } else
            tick_observer.registered = false;
    }
    if (f->loop != NULL && !mc_client_menu_loop_terminal_retire(&f->loop)) {
        if (mc_client_menu_loop_inspect(f->loop, &f->state))
            fprintf(stderr, "Spawner tick native cleanup refused: %s\n",
                    f->state.retirement_operation);
        result = 1;
    }
    if (tick_observer.access != NULL &&
        mc_world_access_free(tick_observer.access, NULL) != MC_WORLD_STORAGE_OK)
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
        printf("SPAWNER_TICK_FINISH profile=%s cleanup=1 windows-absent=1 checks=%u "
               "source-field-repair=0\n",
               argv[3], checks);
    free(f);
    return result;
}
