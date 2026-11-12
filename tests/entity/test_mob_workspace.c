/* Native diagnostic storage controls over genuine World/App/Mob owners.
 * Source fields below are declared finite inputs, not loaded gameplay. */
int mc_workspace_unused_statistics_main(int, char **);
#define MC_PLAYER_STATISTICS_MAX_PROVIDER_CHUNKS 64
#define main mc_workspace_unused_statistics_main
#include "../client/statistics/test_player_dispatch.c"
#undef main

#include "entity/mob/references_internal.h"
#include "entity/mob/tick/internal.h"
#include "entity/player/damage_wake_internal.h"
#include "mc/entity/mob/construction.h"
#include "mc/world/runtime/entities.h"
#include "world/runtime/chunk_provider_internal.h"
#include "world/runtime/entity_tick_internal.h"
#include "world/storage/objects_internal.h"

typedef struct ws_fixture {
    fixture base;
    mc_math_classes *math;
    mc_chunk_provider_fields provider;
    mc_entity *actor, *other;
    mc_world_access *observer;
    mc_mob_tick_child_workspace *scratch, *nested_scratch;
    mc_player_action_error *saved_action;
    mc_mob_tick_error *working, *nested, *saved;
    mc_world_entity_tick_error *world_error, *nested_world;
    mc_world_entity_tick_context context;
    size_t sounds, reentries;
    bool reenter, callback_failed, inspect_scratch;
} ws_fixture;

static bool ws_chunks(fixture *f) {
    static const char *const names[] = {
        "xPos",       "zPos",      "LastUpdate",       "Blocks",   "Data",        "SkyLight",
        "BlockLight", "HeightMap", "TerrainPopulated", "Entities", "TileEntities"};
    static const mc_nbt_type types[] = {MC_NBT_INT,        MC_NBT_INT,        MC_NBT_LONG,
                                        MC_NBT_BYTE_ARRAY, MC_NBT_BYTE_ARRAY, MC_NBT_BYTE_ARRAY,
                                        MC_NBT_BYTE_ARRAY, MC_NBT_BYTE_ARRAY, MC_NBT_BYTE,
                                        MC_NBT_LIST,       MC_NBT_LIST};
    const size_t lengths[] = {MC_CHUNK_BLOCKS, MC_CHUNK_NIBBLE_BYTES, MC_CHUNK_NIBBLE_BYTES,
                              MC_CHUNK_NIBBLE_BYTES, MC_CHUNK_HEIGHTMAP_BYTES};
    uint8_t *arrays =
        calloc(1, MC_CHUNK_BLOCKS + 3u * MC_CHUNK_NIBBLE_BYTES + MC_CHUNK_HEIGHTMAP_BYTES);
    CHECK(arrays != NULL);
    mc_nbt_tag root = {.type = MC_NBT_COMPOUND}, level = {.type = MC_NBT_COMPOUND};
    mc_nbt_tag fields[11] = {0};
    const mc_nbt_tag *root_fields[] = {&level}, *level_fields[11];
    uint16_t encoded_names[12][20] = {0};
    const char *level_name = "Level";
    for (size_t i = 0; i < strlen(level_name); ++i)
        encoded_names[11][i] = (uint16_t)(unsigned char)level_name[i];
    level.name = (mc_nbt_string){encoded_names[11], strlen(level_name)};
    root.value.compound.items = root_fields;
    root.value.compound.length = 1;
    level.value.compound.items = level_fields;
    level.value.compound.length = 11;
    for (size_t i = 0; i < 11; ++i) {
        fields[i].type = types[i];
        for (size_t j = 0; j < strlen(names[i]); ++j)
            encoded_names[i][j] = (uint16_t)(unsigned char)names[i][j];
        fields[i].name = (mc_nbt_string){encoded_names[i], strlen(names[i])};
        level_fields[i] = &fields[i];
    }
    size_t offset = 0;
    for (size_t i = 0; i < 5; ++i) {
        fields[i + 3u].value.byte_array.data = arrays + offset;
        fields[i + 3u].value.byte_array.length = lengths[i];
        offset += lengths[i];
    }
    for (size_t i = 0; i < 256; ++i)
        arrays[i * 128u + 63u] = 1; /* Declared stone plane at y63. */
    memset(arrays + offset - MC_CHUNK_HEIGHTMAP_BYTES, 64, MC_CHUNK_HEIGHTMAP_BYTES);
    fields[8].value.byte_value = 1;
    fields[9].value.list.element_type = MC_NBT_COMPOUND;
    fields[10].value.list.element_type = MC_NBT_COMPOUND;
    size_t length = 0;
    CHECK(mc_nbt_measure(&root, NULL, &length, NULL) == MC_NBT_OK && length != 0);
    uint8_t *wire = malloc(length);
    CHECK(wire != NULL);
    for (int32_t x = -2; x <= 2; ++x)
        for (int32_t z = -2; z <= 2; ++z) {
            fields[0].value.int_value = x;
            fields[1].value.int_value = z;
            size_t written = 0;
            uint32_t growth = 0;
            CHECK(mc_nbt_write(&root, NULL, wire, length, &written, NULL) == MC_NBT_OK &&
                  written == length &&
                  mc_world_regions_write(f->regions, mc_world_store_root(f->store), x, z, wire,
                                         length, 73, &growth, NULL) == MC_WORLD_STORAGE_OK);
        }
    free(wire);
    free(arrays);
    for (int32_t x = -2; x <= 2; ++x)
        for (int32_t z = -2; z <= 2; ++z) {
            mc_chunk_state *chunk = NULL;
            CHECK(mc_world_runtime_get(f->world, x, z, &chunk) == MC_CHUNK_ACQUIRE_OK &&
                  chunk != NULL && !chunk->blank && chunk->active);
        }
    return true;
}

static bool ws_blank(const mc_mob_tick_error *error, mc_entity_class_kind kind) {
    mc_mob_tick_state expected;
    memset(&expected, 0, sizeof(expected));
    expected.kind = kind;
    CHECK(memcmp(&error->state, &expected, sizeof(expected)) == 0 &&
          error->child == MC_MOB_TICK_CHILD_NONE);
    return true;
}

static bool ws_mob(ws_fixture *f, mc_entity_class_kind kind, double x, mc_entity **out) {
    const mc_entity_class_literal *literal = NULL;
    mc_mob_error construction;
    CHECK(mc_entity_class_literal_get(f->base.app.entities, kind, &literal) == MC_ENTITY_OK &&
          mc_mob_allocate(f->base.app.entities, f->base.app.registry, literal, out,
                          &construction) == MC_MOB_OK &&
          mc_mob_construct(*out, f->base.world, &construction) == MC_MOB_OK &&
          mc_entity_set_position(*out, x, 64, 8.5) == MC_ENTITY_OK);
    (*out)->mob->living.private_a = -10000; /* Suppress only ambient sound input. */
    bool inserted = false;
    CHECK(mc_world_runtime_insert_entity(f->base.world, *out, &inserted, NULL) ==
              MC_WORLD_STORAGE_OK &&
          inserted && (*out)->base.bF);
    return true;
}

static bool ws_setup(ws_fixture *f, const char *resources, unsigned int id) {
    char directory[64];
    CHECK(snprintf(directory, sizeof(directory), "mob-workspace-%u", id) > 0 &&
          setup(&f->base, resources, directory, true) && ws_chunks(&f->base) &&
          mc_math_classes_create(SIZE_MAX, &f->math, NULL) == MC_MATH_CLASS_OK &&
          mc_math_classes_ensure(f->math, NULL) == MC_MATH_CLASS_OK &&
          mc_chunk_provider_bind_fields(f->base.world->fields.v, &f->provider, NULL) ==
              MC_WORLD_STORAGE_OK &&
          ws_mob(f, MC_ENTITY_CLASS_KIND_ZOMBIE, 9.5, &f->actor) &&
          ws_mob(f, MC_ENTITY_CLASS_KIND_PIG, 13.5, &f->other) &&
          mc_entity_set_position(f->base.player, 8.5, 64 + (double)1.62f, 8.5) == MC_ENTITY_OK &&
          mc_world_info_time_set(f->base.world->fields.x, 13000) == MC_WORLD_INFO_OK);
    /* The real Player stays unpublished; FD.g genuinely ticks the two Mobs.
     * Targeted calls explicitly store its real independent occurrence below. */
    CHECK(f->base.world->fields.d->count == 0);
    f->base.world->fields.B = false;
    f->base.world->fields.q = 2;
    f->base.world->fields.f = 4;
    f->context.player.travel.code_registry = f->base.app.registry;
    f->context.player.travel.trig = mc_math_classes_table(f->math);
    f->context.player.travel.math = f->math;
    f->context.math = f->math;
    f->scratch = malloc(sizeof(*f->scratch));
    f->nested_scratch = malloc(sizeof(*f->nested_scratch));
    f->saved_action = malloc(sizeof(*f->saved_action));
    f->working = calloc(1, sizeof(*f->working));
    f->nested = calloc(1, sizeof(*f->nested));
    f->saved = calloc(1, sizeof(*f->saved));
    f->world_error = calloc(1, sizeof(*f->world_error));
    f->nested_world = calloc(1, sizeof(*f->nested_world));
    CHECK(f->scratch && f->nested_scratch && f->saved_action && f->working && f->nested &&
          f->saved && f->world_error && f->nested_world);
    return true;
}

static bool ws_finish(ws_fixture *f) {
    CHECK(mc_mob_tick_error_retire(f->working) && mc_mob_tick_error_retire(f->nested) &&
          mc_world_entity_tick_error_retire(f->world_error) &&
          mc_world_entity_tick_error_retire(f->nested_world) &&
          !mc_entity_effect_has_pending(f->base.player));
    if (f->observer != NULL) {
        bool removed = false;
        CHECK(mc_world_runtime_remove_world_access(f->base.world, f->observer, &removed, NULL) ==
                  MC_WORLD_STORAGE_OK &&
              removed && mc_world_access_free(f->observer, NULL) == MC_WORLD_STORAGE_OK);
    }
    mc_entity_release(f->actor);
    mc_entity_release(f->other);
    f->actor = f->other = NULL;
    mc_chunk_state *retiring[64];
    const size_t count = f->provider.cache->insertion_count;
    CHECK(count <= sizeof(retiring) / sizeof(*retiring));
    if (count != 0)
        memcpy(retiring, f->provider.cache->insertions, count * sizeof(*retiring));
    CHECK(mc_world_runtime_retire_entity_containers(f->base.world, NULL) == MC_WORLD_STORAGE_OK &&
          mc_chunk_pending_clear(f->provider.pending) == MC_CHUNK_ACQUIRE_OK &&
          mc_chunk_cache_init(f->provider.cache, f->provider.cache->entries,
                              f->provider.cache->capacity, f->provider.cache->insertions,
                              f->provider.cache->insertion_capacity) &&
          mc_world_runtime_retire_world(f->base.world, NULL) == MC_WORLD_STORAGE_OK);
    for (size_t i = 0; i < count; ++i) {
        bool duplicate = false;
        for (size_t earlier = 0; earlier < i; ++earlier)
            duplicate = duplicate || retiring[earlier] == retiring[i];
        if (!duplicate)
            CHECK(mc_world_store_retire_chunk(f->base.store, retiring[i], NULL) ==
                  MC_WORLD_STORAGE_OK);
    }
    CHECK(mc_math_classes_release(&f->math, NULL) == MC_MATH_CLASS_OK && teardown(&f->base));
    free(f->scratch);
    free(f->nested_scratch);
    free(f->saved_action);
    free(f->working);
    free(f->nested);
    free(f->saved);
    free(f->world_error);
    free(f->nested_world);
    return true;
}

static mc_mob_tick_limits ws_limits(void) {
    const mc_mob_tick_limits result = {
        .max_work = MC_MOB_TICK_DEFAULT_WORK,
        .environment = {.max_particle_work = MC_ENTITY_TICK_DEFAULT_PARTICLE_WORK,
                        .material = {MC_WORLD_MATERIAL_MOVEMENT_DEFAULT_CELL_WORK}},
        .action = {MC_PLAYER_ACTION_DEFAULT_LOOP_WORK},
        .movement = {MC_ENTITY_MOVE_DEFAULT_LOOP_WORK},
        .path = mc_world_path_default_limits()};
    return result;
}

static bool ws_reset_failures(ws_fixture *f) {
    const mc_entity_class_kind initial = (mc_entity_class_kind)0;
    memset(f->working, 0xa5, sizeof(*f->working));
    CHECK(mt_tick_in_place(NULL, f->base.app.registry, f->math, NULL, f->working, f->scratch) ==
              MC_ENTITY_STATE_ERROR &&
          ws_blank(f->working, initial));
    memset(f->working, 0xa5, sizeof(*f->working));
    CHECK(mt_tick_in_place(f->base.player, f->base.app.registry, f->math, NULL, f->working,
                           f->scratch) == MC_ENTITY_STATE_ERROR &&
          ws_blank(f->working, initial));
    mc_entity *raw = NULL;
    const mc_entity_class_literal *literal = NULL;
    mc_mob_error construction;
    CHECK(mc_entity_class_literal_get(f->base.app.entities, MC_ENTITY_CLASS_KIND_PIG, &literal) ==
              MC_ENTITY_OK &&
          mc_mob_allocate(f->base.app.entities, f->base.app.registry, literal, &raw,
                          &construction) == MC_MOB_OK);
    memset(f->working, 0xa5, sizeof(*f->working));
    CHECK(mt_tick_in_place(raw, f->base.app.registry, f->math, NULL, f->working, f->scratch) ==
              MC_ENTITY_INVALID_PHASE &&
          ws_blank(f->working, initial));
    mc_entity_release(raw);

    /* Genuine environment failure: copy before scratch reuse, no source repair
     * credited as a successful call or cleanup. */
    f->actor->base.world = NULL;
    memset(f->working, 0xa5, sizeof(*f->working));
    CHECK(mt_tick_in_place(f->actor, f->base.app.registry, f->math, NULL, f->working, f->scratch) !=
              MC_ENTITY_OK &&
          f->working->child == MC_MOB_TICK_CHILD_ENVIRONMENT &&
          !f->working->state.environment_returned && f->actor->base.world == NULL &&
          memcmp(&f->working->detail.environment, &f->scratch->environment,
                 sizeof(f->scratch->environment)) == 0);
    memcpy(f->saved, f->working, sizeof(*f->saved));
    memset(f->scratch, 0x5a, sizeof(*f->scratch));
    CHECK(memcmp(f->saved, f->working, sizeof(*f->saved)) == 0 &&
          mc_mob_tick_error_retire(f->working) && f->actor->base.world == NULL);
    f->actor->base.world = f->base.world; /* Next declared input / terminal owner. */

    /* Suffocation writes the parent Math failure while the environment child
     * itself remains OK. The wrapper must preserve that selected parent. */
    memset(f->working, 0xa5, sizeof(*f->working));
    CHECK(mt_tick_in_place(f->actor, f->base.app.registry, NULL, NULL, f->working, f->scratch) ==
              MC_ENTITY_STATE_ERROR &&
          f->working->native_policy && f->working->operation != NULL &&
          strcmp(f->working->operation, "Mob tick actual Math namespace") == 0 &&
          f->working->child != MC_MOB_TICK_CHILD_ENVIRONMENT &&
          !f->working->state.environment_returned &&
          f->scratch->environment.status == MC_ENTITY_OK && mc_mob_tick_error_retire(f->working));

    mc_mob_tick_limits limits = ws_limits();
    limits.action.max_loop_work = 0;
    f->actor->mob->living.an = 1;
    f->actor->mob->living.ao = f->actor->base.x;
    f->actor->mob->living.ap = f->actor->base.y;
    f->actor->mob->living.aq = f->actor->base.z;
    f->actor->mob->living.ar = (double)f->actor->base.yaw + 360;
    f->actor->mob->living.as = f->actor->base.aT;
    memset(f->working, 0xa5, sizeof(*f->working));
    CHECK(mt_tick_in_place(f->actor, f->base.app.registry, f->math, &limits, f->working,
                           f->scratch) == MC_ENTITY_LIMIT &&
          f->working->child == MC_MOB_TICK_CHILD_ACTION && f->working->state.environment_returned &&
          !f->working->state.action_returned && f->working->detail.action.max_loop_work == 0 &&
          memcmp(&f->working->detail.action, &f->scratch->action, sizeof(f->scratch->action)) == 0);
    memcpy(f->saved, f->working, sizeof(*f->saved));
    memset(f->scratch, 0x39, sizeof(*f->scratch));
    CHECK(memcmp(f->saved, f->working, sizeof(*f->saved)) == 0 &&
          mc_mob_tick_error_retire(f->working));
    f->actor->mob->living.an = 0; /* New declared call input. */

    /* A literal native allocation budget refuses a reached real path. It is
     * not a claim that malloc itself failed. Fresh hurt state stays empty. */
    limits = ws_limits();
    limits.path.max_native_bytes = 0;
    CHECK(mc_mob_creature_target_store(f->other, f->base.player, NULL) == MC_ENTITY_OK);
    memset(f->working, 0xa5, sizeof(*f->working));
    CHECK(mt_tick_in_place(f->other, f->base.app.registry, f->math, &limits, f->working,
                           f->scratch) != MC_ENTITY_OK &&
          f->working->child == MC_MOB_TICK_CHILD_PATH && f->working->native_policy &&
          f->working->state.path_calls == 1 && !f->working->state.attack.hurt_invoked &&
          f->working->state.environment_returned &&
          f->working->detail.path.cause.status == MC_WORLD_PATH_LIMIT &&
          mc_mob_tick_error_retire(f->working));
    CHECK(mc_mob_creature_target_store(f->other, NULL, NULL) == MC_ENTITY_OK);

    /* Both public and private NULL-output routes execute independent work.
     * The supplied scratch must remain untouched on the private NULL route. */
    memcpy(f->nested_scratch, f->scratch, sizeof(*f->scratch));
    uint64_t random = f->other->random->state;
    CHECK(mt_tick_in_place(f->other, f->base.app.registry, f->math, NULL, NULL, f->scratch) ==
              MC_ENTITY_OK &&
          f->other->random->state != random &&
          memcmp(f->nested_scratch, f->scratch, sizeof(*f->scratch)) == 0);
    random = f->actor->random->state;
    CHECK(mc_mob_tick(f->actor, f->base.app.registry, f->math, NULL, NULL) == MC_ENTITY_OK &&
          f->actor->random->state != random &&
          mc_world_runtime_tick_entities(f->base.world, &f->context, NULL, NULL) ==
              MC_WORLD_STORAGE_OK &&
          f->base.world->active_calls == 0 &&
          f->base.world->last_error.status == MC_WORLD_STORAGE_OK);
    return true;
}

static bool ws_reenter(ws_fixture *f, mc_world_runtime *world) {
    CHECK(f->actor->mob->tick_active && world == f->base.world && world->active_calls > 0);
    if (f->inspect_scratch) {
        CHECK(f->working->state.environment_returned && !f->working->state.action_returned &&
              f->working->child == MC_MOB_TICK_CHILD_EFFECT &&
              f->scratch->action.source_class != NULL &&
              strcmp(f->scratch->action.source_class, "ls") == 0 &&
              f->scratch->action.status == MC_ENTITY_OK);
        memcpy(f->saved, f->working, sizeof(*f->saved));
        memcpy(f->saved_action, &f->scratch->action, sizeof(*f->saved_action));
    }
    const size_t calls = world->active_calls;
    memset(f->nested, 0xa5, sizeof(*f->nested));
    CHECK(mc_mob_tick(f->actor, f->base.app.registry, f->math, NULL, f->nested) == MC_ENTITY_BUSY &&
          ws_blank(f->nested, (mc_entity_class_kind)0));
    memset(f->nested, 0xa5, sizeof(*f->nested));
    CHECK(mt_tick_in_place(f->actor, f->base.app.registry, f->math, NULL, f->nested,
                           f->nested_scratch) == MC_ENTITY_BUSY &&
          ws_blank(f->nested, (mc_entity_class_kind)0));
    CHECK(mc_mob_tick(f->other, f->base.app.registry, f->math, NULL, f->nested) == MC_ENTITY_OK &&
          f->nested->state.returned && f->nested->state.environment_returned &&
          f->nested->state.action_returned && !f->nested->state.attack.hurt_invoked);
    CHECK(mc_world_runtime_tick_entity(world, f->other, &f->context, NULL, f->nested_world) ==
              MC_WORLD_STORAGE_OK &&
          world->active_calls == calls && !f->nested_world->mob_hurt.last.present);

    /* Enter a real nested World action with a different budget while the
     * outer action is live. A World-global scratch union would overwrite its
     * positive budget with zero before its later nearby-Entity loop. */
    mc_mob_tick_limits limits = ws_limits();
    limits.action.max_loop_work = 0;
    mc_world_entity_tick_context limited = f->context;
    limited.mob_limits = &limits;
    mc_living_entity_fields *interpolation = &f->other->mob->living;
    interpolation->an = 1;
    interpolation->ao = f->other->base.x;
    interpolation->ap = f->other->base.y;
    interpolation->aq = f->other->base.z;
    interpolation->ar = (double)f->other->base.yaw + 360;
    interpolation->as = f->other->base.aT;
    CHECK(mc_world_runtime_tick_entity(world, f->other, &limited, NULL, f->nested_world) ==
              MC_WORLD_STORAGE_LIMIT &&
          world->active_calls == calls &&
          f->nested_world->child == MC_WORLD_ENTITY_TICK_CHILD_MOB &&
          f->nested_world->child_error.mob.status == MC_ENTITY_LIMIT &&
          f->nested_world->child_error.mob.child == MC_MOB_TICK_CHILD_ACTION &&
          f->nested_world->child_error.mob.state.environment_returned &&
          !f->nested_world->child_error.mob.state.action_returned &&
          !f->nested_world->child_error.mob.state.attack.hurt_invoked &&
          f->nested_world->child_error.mob.detail.action.living_bytecode == 117 &&
          f->nested_world->child_error.mob.detail.action.max_loop_work == 0 &&
          f->nested_world->child_error.mob.detail.action.loop_work == 0 &&
          !f->nested_world->mob_hurt.last.present && interpolation->an == 1 &&
          !f->other->mob->tick_active);
    CHECK(mc_world_entity_tick_error_retire(f->nested_world) && interpolation->an == 1);
    interpolation->an = 0; /* Next declared fixture input; no failed-call replay. */
    CHECK(mc_mob_tick_cleanup(f->actor, f->nested) == MC_ENTITY_BUSY &&
          mc_world_runtime_retire_world(world, NULL) == MC_WORLD_STORAGE_BUSY &&
          f->actor->mob->tick_active && !f->other->mob->tick_active);
    if (f->inspect_scratch)
        CHECK(memcmp(f->saved, f->working, sizeof(*f->saved)) == 0 &&
              memcmp(f->saved_action, &f->scratch->action, sizeof(*f->saved_action)) == 0);
    ++f->reentries;
    return true;
}

static mc_world_storage_status ws_sound(void *context, mc_world_runtime *world,
                                        const mc_registry_string *name, double x, double y,
                                        double z, float volume, float pitch,
                                        mc_world_storage_error *error) {
    ws_fixture *f = context;
    (void)x;
    (void)y;
    (void)z;
    (void)volume;
    (void)pitch;
    ++f->sounds;
    mc_registry_string_view text = {0};
    const char expected[] = "random.hurt";
    bool hurt = mc_registry_string_read(name, &text) && text.length == strlen(expected);
    for (size_t i = 0; hurt && i < text.length; ++i)
        hurt = text.units[i] == (uint16_t)(unsigned char)expected[i];
    if (f->reenter && f->reentries == 0 && hurt && !ws_reenter(f, world)) {
        f->callback_failed = true;
        *error = (mc_world_storage_error){MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                          "Mob workspace reentry control failure", 0};
        return error->status;
    }
    return MC_WORLD_STORAGE_OK;
}

static bool ws_target(ws_fixture *f) {
    f->actor->mob->living.ae = 0;
    CHECK(mc_mob_creature_target_store(f->actor, f->base.player, NULL) == MC_ENTITY_OK);
    return true;
}

static bool ws_observation(ws_fixture *f, const mc_world_mob_hurt_observation *value) {
    CHECK(value->reached_calls == 1 && value->returned_calls == 1 && value->true_returns == 1 &&
          value->last.present && value->last.returned && value->last.parent_returned &&
          value->last.actor_identity == (uintptr_t)f->actor &&
          value->last.target_identity == (uintptr_t)f->base.player && value->last.caller_pc == 61 &&
          value->last.return_pc == 65 && value->last.damage == 5 && value->last.health_stored &&
          value->last.health_before == 20 && value->last.health_after == 15 &&
          value->last.statistic_returned && value->last.statistic_amount == 5);
    return true;
}

static bool ws_reentry_case(ws_fixture *f, unsigned int mode) {
    /* Keep a genuine nearby candidate for the outer action's later loop. */
    CHECK(mc_entity_set_position(f->other, 10, 64, 8.5) == MC_ENTITY_OK && ws_target(f));
    const mc_world_access_callbacks callbacks = {.sound = ws_sound};
    CHECK(mc_world_access_create(&callbacks, f, &f->observer, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(f->base.world, f->observer, NULL) ==
              MC_WORLD_STORAGE_OK);
    f->reenter = true;
    f->inspect_scratch = mode == 0;
    if (mode == 0) {
        memset(f->working, 0xa5, sizeof(*f->working));
        CHECK(mt_tick_in_place(f->actor, f->base.app.registry, f->math, NULL, f->working,
                               f->scratch) == MC_ENTITY_OK &&
              f->working->state.returned && f->working->state.environment_returned &&
              f->working->state.action_returned && f->working->state.attack.hurt_returned &&
              f->working->state.attack.hurt_result && f->working->state.attack.hurt_damage == 5 &&
              f->scratch->action.loop_work > 0 &&
              f->scratch->action.max_loop_work == MC_PLAYER_ACTION_DEFAULT_LOOP_WORK &&
              f->scratch->action.nearby_index != SIZE_MAX);
    } else {
        memset(f->world_error, 0xa5, sizeof(*f->world_error));
        const mc_world_storage_status status =
            mode == 1
                ? mc_world_runtime_tick_entities(f->base.world, &f->context, NULL, f->world_error)
                : wt_tick_entities_without_mob_workspace(f->base.world, &f->context, NULL,
                                                         f->world_error);
        CHECK(status == MC_WORLD_STORAGE_OK && ws_observation(f, &f->world_error->mob_hurt) &&
              f->world_error->child == MC_WORLD_ENTITY_TICK_CHILD_NONE);
    }
    CHECK(!f->callback_failed && f->reentries == 1 &&
          f->base.player->local_player->fields.living.Y == 15 && !f->actor->mob->tick_active &&
          !f->other->mob->tick_active && !mc_entity_effect_has_pending(f->base.player));
    return true;
}

static bool ws_failure_case(ws_fixture *f, bool fallback) {
    CHECK(ws_target(f));
    mc_world_reference_list *players = f->base.world->fields.d;
    f->base.player->local_player->fields.player.u = true;
    f->base.world->fields.d = NULL; /* Actual source wake List fault. */
    memset(f->world_error, 0xa5, sizeof(*f->world_error));
    const mc_world_storage_status status =
        fallback ? wt_tick_entities_without_mob_workspace(f->base.world, &f->context, NULL,
                                                          f->world_error)
                 : mc_world_runtime_tick_entities(f->base.world, &f->context, NULL, f->world_error);
    mc_world_entity_tick_error *e = f->world_error;
    CHECK(
        status != MC_WORLD_STORAGE_OK && e->child == MC_WORLD_ENTITY_TICK_CHILD_MOB &&
        e->child_error.mob.child == MC_MOB_TICK_CHILD_EFFECT &&
        e->child_error.mob.state.environment_returned &&
        !e->child_error.mob.state.action_returned && e->child_error.mob.state.attack.hurt_invoked &&
        !e->child_error.mob.state.attack.hurt_returned && e->child_error.mob.source_exception &&
        !e->child_error.mob.native_policy && e->child_error.mob.detail.effect.player_wake.pending &&
        mc_entity_effect_has_pending(f->base.player) &&
        f->base.player->local_player->fields.living.Y == 20 && e->mob_hurt.reached_calls == 1 &&
        e->mob_hurt.returned_calls == 0);
    const mc_world_storage_status first = e->status;
    const char *operation = e->operation;
    struct mc_player_damage_wake_work *pending = f->base.player->local_player->pending_damage_wake;
    const size_t held = f->base.player->holds;
    f->base.player->holds = SIZE_MAX; /* Native cleanup-admission fault only. */
    const bool retired = mc_world_entity_tick_error_retire(e);
    f->base.player->holds = held;
    CHECK(!retired && e->status == first && e->operation == operation &&
          e->child == MC_WORLD_ENTITY_TICK_CHILD_MOB &&
          e->child_error.mob.detail.effect.player_wake.cleanup_status == MC_ENTITY_LIMIT &&
          f->base.player->local_player->pending_damage_wake == pending &&
          f->base.world->fields.d == NULL);
    CHECK(mc_world_entity_tick_error_retire(e) && !mc_entity_effect_has_pending(f->base.player) &&
          f->base.world->fields.d == NULL && f->base.player->local_player->fields.living.Y == 20);
    f->base.world->fields.d = players; /* Terminal graph disposal only; no replay. */
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3 || CD(argv[2]) != 0 || !mc_math_trig_init(&trig))
        return 2;
    for (unsigned int id = 0; id < 6; ++id) {
        ws_fixture *f = calloc(1, sizeof(*f));
        if (f == NULL || !ws_setup(f, argv[1], id))
            return 1;
        const bool done = id == 0   ? ws_reset_failures(f)
                          : id <= 3 ? ws_reentry_case(f, id - 1)
                                    : ws_failure_case(f, id == 5);
        if (!done || !ws_finish(f))
            return 1;
        free(f);
        if (printf("MOB_WORKSPACE_CASE id=%u\n", id) < 0 || fflush(stdout) != 0)
            return 1;
    }
    if (printf("Mob workspace: %zu checks;6 actual owner controls\n", checks) < 0 ||
        fflush(stdout) != 0)
        return 1;
    return 0;
}
