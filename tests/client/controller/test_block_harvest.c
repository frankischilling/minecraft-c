/* Real App/statistics/Player/World owners, without entering its test main. */
#define main mc_block_harvest_unused_statistics_main
#include "../statistics/test_player_dispatch.c"
#undef main

#include "../../../src/core/java/random_profile_internal.h"
#include "../../../src/world/runtime/runtime_internal.h"
#include "../../../src/world/storage/generated_internal.h"
#include "mc/client/controller/mining.h"
#include "mc/core/java/java.h"
#include "mc/item/mining/destroy.h"
#include "mc/world/runtime/blocks.h"
#include "mc/world/runtime/world_access.h"

#include <errno.h>
#include <inttypes.h>

typedef struct harvest_input {
    int32_t index, mode, block, metadata, remote, held, count, damage, below, fault;
    int64_t world_seed, math_seed;
} harvest_input;

typedef struct harvest_event {
    const char *kind;
    int32_t block, metadata, mined, used, broken, item_id, delay;
    size_t items;
    bool in_world, in_chunk;
    uint64_t world_random, math_random;
} harvest_event;

typedef struct harvest_observer {
    fixture *f;
    const harvest_input *input;
    mc_world_chunk *chunk;
    mc_statistic *mined;
    const mc_statistic *used, *broken;
    int32_t mined_before, used_before, broken_before;
    harvest_event events[32];
    size_t event_count;
} harvest_observer;

static uint64_t double_bits(double value) {
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static uint32_t float_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static bool statistic_value(fixture *f, const mc_statistic *statistic, int32_t *out) {
    *out = 0;
    return statistic == NULL ||
           mc_client_stats_writer_value(f->app.stats_writer_retained, statistic, out, NULL) ==
               MC_CLIENT_STATS_OK;
}

static size_t item_count(mc_world_runtime *world) {
    size_t count = 0;
    for (size_t i = 0; i < world->entity_membership.count; ++i)
        if (mc_entity_is_kind(world->entity_membership.entries[i], MC_ENTITY_CLASS_KIND_ITEM))
            ++count;
    return count;
}

static bool item_memberships(harvest_observer *o, mc_entity *entity, bool *in_world,
                             bool *in_chunk) {
    size_t world_count = 0, chunk_count = 0;
    for (size_t i = 0; i < o->f->world->entity_membership.count; ++i)
        if (o->f->world->entity_membership.entries[i] == entity)
            ++world_count;
    mc_world_chunk_fields fields;
    CHECK(mc_world_chunk_fields_get(o->chunk, &fields) == MC_WORLD_STORAGE_OK);
    for (size_t i = 0; i < 8; ++i) {
        const mc_world_chunk_entity_list *slice;
        size_t count;
        CHECK(mc_world_chunk_slice_at(fields.m, i, &slice) == MC_WORLD_STORAGE_OK &&
              mc_world_chunk_entity_count(slice, &count) == MC_WORLD_STORAGE_OK);
        for (size_t j = 0; j < count; ++j) {
            mc_entity *member;
            CHECK(mc_world_chunk_entity_at(slice, j, &member) == MC_WORLD_STORAGE_OK);
            if (member == entity)
                ++chunk_count;
        }
    }
    CHECK(world_count <= 1 && chunk_count <= 1);
    *in_world = world_count == 1;
    *in_chunk = chunk_count == 1;
    return true;
}

static bool event_record(harvest_observer *o, const char *kind, mc_entity *entity) {
    CHECK(o->event_count < sizeof(o->events) / sizeof(*o->events));
    harvest_event *event = &o->events[o->event_count++];
    event->kind = kind;
    event->item_id = -1;
    event->delay = -1;
    CHECK(mc_world_runtime_block_get(o->f->world, 3, 64, 5, &event->block, NULL) ==
              MC_WORLD_STORAGE_OK &&
          mc_world_runtime_metadata_get(o->f->world, 3, 64, 5, &event->metadata, NULL) ==
              MC_WORLD_STORAGE_OK &&
          statistic_value(o->f, o->mined, &event->mined) &&
          statistic_value(o->f, o->used, &event->used) &&
          statistic_value(o->f, o->broken, &event->broken));
    event->mined = mc_java_i32_sub(event->mined, o->mined_before);
    event->used = mc_java_i32_sub(event->used, o->used_before);
    event->broken = mc_java_i32_sub(event->broken, o->broken_before);
    event->items = item_count(o->f->world);
    event->world_random = o->f->world->fields.r->state;
    event->math_random = atomic_load(&o->f->app.profile->math->state);
    if (entity != NULL) {
        mc_item_stack_fields stack;
        CHECK(mc_item_stack_fields_get(entity->stack, &stack) == MC_ITEM_STACK_OK &&
              item_memberships(o, entity, &event->in_world, &event->in_chunk));
        event->item_id = stack.id;
        event->delay = entity->item.c;
    }
    return true;
}

static mc_world_storage_status observed_cell(void *context, mc_world_runtime *world, int32_t x,
                                             int32_t y, int32_t z, mc_world_storage_error *error) {
    (void)world;
    (void)error;
    harvest_observer *o = context;
    return (x != 3 || y != 64 || z != 5 || event_record(o, "CHANGE", NULL))
               ? MC_WORLD_STORAGE_OK
               : MC_WORLD_STORAGE_UNSUPPORTED_STATE;
}

static mc_world_storage_status observed_range(void *context, mc_world_runtime *world, int32_t x,
                                              int32_t y, int32_t z, int32_t max_x, int32_t max_y,
                                              int32_t max_z, mc_world_storage_error *error) {
    (void)context;
    (void)world;
    (void)x;
    (void)y;
    (void)z;
    (void)max_x;
    (void)max_y;
    (void)max_z;
    (void)error;
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status observed_auxiliary(void *context, mc_world_runtime *world,
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
    return MC_WORLD_STORAGE_OK;
}

/* The original pm observer returns normally from these slots too. They do
 * not replace the Water/Lava body: its actual entropy and argument evaluation
 * still execute, and the final Random/Math states are compared with Java. */
static mc_world_storage_status observed_sound(void *context, mc_world_runtime *world,
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
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status observed_particle(void *context, mc_world_runtime *world,
                                                 const mc_registry_string *name, double x, double y,
                                                 double z, double vx, double vy, double vz,
                                                 mc_world_storage_error *error) {
    (void)context;
    (void)world;
    (void)name;
    (void)x;
    (void)y;
    (void)z;
    (void)vx;
    (void)vy;
    (void)vz;
    (void)error;
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status observed_item(void *context, mc_world_runtime *world,
                                             mc_entity *entity, mc_world_storage_error *error) {
    (void)world;
    harvest_observer *o = context;
    if (!mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_ITEM))
        return MC_WORLD_STORAGE_OK;
    if (!event_record(o, "INSERT", entity))
        return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    if (o->input->fault == 6) {
        *error = (mc_world_storage_error){MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                          "authored Item publication callback failure", 0};
        return error->status;
    }
    if (o->input->fault == 7)
        o->f->app.stats_writer = NULL; /* Actual next statistic receiver, after publication. */
    return MC_WORLD_STORAGE_OK;
}

static bool null_failure(const mc_singleplayer_mining_error *error) {
    return error->status == MC_SINGLEPLAYER_MINING_SOURCE_NULL ||
           (error->statistic != NULL &&
            error->statistic->status == MC_PLAYER_STATISTIC_NULL_REFERENCE) ||
           (error->tool != NULL &&
            error->tool->statistic.status == MC_PLAYER_STATISTIC_NULL_REFERENCE);
}

static bool exercise(fixture *f, const harvest_input *p) {
    CHECK(p->mode == 1 && (p->block == 18 || p->block == 78 || p->block == 79));
    uint8_t *blocks = calloc(MC_CHUNK_BLOCKS, 1);
    CHECK(blocks != NULL);
    const size_t column = ((size_t)3 * 16u + 5u) * 128u;
    blocks[column + 64] = (uint8_t)p->block;
    blocks[column + 63] = (uint8_t)p->below;
    if (p->below == 78)
        blocks[column + 62] = 1; /* Same actual Snow support as original scene. */
    mc_world_chunk *chunk = NULL;
    CHECK(mc_world_store_create_generated(f->store, f->world, 0, 0, 0, &blocks, MC_CHUNK_BLOCKS,
                                          &chunk, NULL) == MC_WORLD_STORAGE_OK &&
          blocks == NULL);
    mc_chunk_state *state = mc_world_chunk_state(chunk);
    const mc_chunk_acquire_context *acquisition = mc_world_runtime_acquisition(f->world);
    CHECK(state != NULL && acquisition != NULL &&
          mc_chunk_cache_publish(acquisition->cache, 0, 0, state));
    state->terrain_populated = true; /* Same declared LM.n input as original. */
    bool changed;
    CHECK(mc_world_runtime_metadata_set_quiet(f->world, 3, 64, 5, p->metadata, &changed, NULL) ==
          MC_WORLD_STORAGE_OK);
    f->app.h = f->player;
    f->app.active_world = f->world;
    CHECK(mc_entity_set_position(f->player, 0.5, 64.0, 0.5) == MC_ENTITY_OK);
    f->player->base.aX = true;
    mc_item_stack *captured = NULL;
    if (p->held >= 0)
        CHECK(mc_item_stack_allocate(&captured) == MC_ITEM_STACK_OK &&
              mc_item_stack_construct(captured, p->held, p->count, p->damage) == MC_ITEM_STACK_OK &&
              mc_inventory_player_set(f->player->local_player->fields.player.c, 0, captured,
                                      NULL) == MC_INVENTORY_OK);
    harvest_observer observer = {
        .f = f,
        .input = p,
        .chunk = chunk,
        .mined = f->app.registry->statistics.C[p->block],
        .used = p->held < 0 ? NULL : f->app.registry->statistics.E[p->held],
        .broken = p->held < 0 ? NULL : f->app.registry->statistics.F[p->held]};
    CHECK(statistic_value(f, observer.mined, &observer.mined_before) &&
          statistic_value(f, observer.used, &observer.used_before) &&
          statistic_value(f, observer.broken, &observer.broken_before));
    mc_world_access *access = NULL;
    const mc_world_access_callbacks callbacks = {.cell = observed_cell,
                                                 .range = observed_range,
                                                 .auxiliary = observed_auxiliary,
                                                 .sound = observed_sound,
                                                 .particle = observed_particle,
                                                 .entity_added = observed_item};
    CHECK(mc_world_access_create(&callbacks, &observer, &access, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(f->world, access, NULL) == MC_WORLD_STORAGE_OK);
    mc_client_singleplayer_controller *controller = NULL;
    CHECK(mc_client_singleplayer_controller_allocate(f->app.entities, f->app.profile, 0,
                                                     &controller,
                                                     NULL) == MC_SINGLEPLAYER_CONTROLLER_OK &&
          mc_client_singleplayer_controller_construct(controller, &f->app, NULL) ==
              MC_SINGLEPLAYER_CONTROLLER_OK);
    /* Declared input preparation, outside the call: same actual seeded JDK
     * World Random and initialized Math.Random owner as the original probe.
     * No constructor/Random result is supplied to the body under test. */
    double unused;
    CHECK(mc_java_math_random(f->app.profile, &unused) == MC_JAVA_RANDOM_PROFILE_OK);
    mc_java_random math_input;
    mc_java_random_seed(&math_input, p->math_seed);
    atomic_store(&f->app.profile->math->state, math_input.state);
    f->app.profile->math->next_next_gaussian = 0.0;
    f->app.profile->math->have_next_next_gaussian = false;
    mc_java_random_reseed(f->world->fields.r, p->world_seed);
    const uint64_t world_before = f->world->fields.r->state;
    const uint64_t math_before = atomic_load(&f->app.profile->math->state);
    f->world->fields.B = p->remote != 0;
    if (p->fault == 1)
        f->app.registry->statistics.C[p->block] = NULL;
    if (p->fault == 2)
        f->app.stats_writer = NULL;
    mc_block *support = p->below == 0 ? NULL : f->app.registry->blocks.m[p->below];
    mc_material *support_material = support == NULL ? NULL : support->fields.bA;
    if (p->fault == 4 && support != NULL)
        support->fields.bA = NULL;
    mc_block *water = f->app.registry->blocks.named[7]; /* Actual UU.B cell. */
    if (p->fault == 5)
        f->app.registry->blocks.named[7] = NULL;
    mc_singleplayer_mining_call *call = NULL;
    mc_singleplayer_mining_error error;
    mc_singleplayer_mining_state result;
    const mc_singleplayer_mining_status status = mc_client_singleplayer_mining_begin(
        controller, MC_SINGLEPLAYER_MINING_DESTROY, 3, 64, 5, 1, &call, &error);
    CHECK(mc_client_singleplayer_mining_inspect(call, &result));
    if (status != MC_SINGLEPLAYER_MINING_OK && !null_failure(&error) && p->fault != 6)
        fprintf(stderr, "block harvest: %s at %s.%s:%u\n", error.operation, error.site.class_name,
                error.site.method, error.site.pc);
    CHECK(status == MC_SINGLEPLAYER_MINING_OK || null_failure(&error) ||
          (p->fault == 6 && status == MC_SINGLEPLAYER_MINING_WORLD));
    CHECK(result.removal_returned && result.removed && !result.active &&
          result.returned == (status == MC_SINGLEPLAYER_MINING_OK));
    int32_t target_id, target_metadata, mined, used, broken;
    CHECK(mc_world_runtime_block_get(f->world, 3, 64, 5, &target_id, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_metadata_get(f->world, 3, 64, 5, &target_metadata, NULL) ==
              MC_WORLD_STORAGE_OK &&
          statistic_value(f, observer.mined, &mined) && statistic_value(f, observer.used, &used) &&
          statistic_value(f, observer.broken, &broken));
    mined = mc_java_i32_sub(mined, observer.mined_before);
    used = mc_java_i32_sub(used, observer.used_before);
    broken = mc_java_i32_sub(broken, observer.broken_before);
    const mc_item_stack *current = NULL;
    mc_item_stack_fields current_fields = {.id = -1}, captured_fields = {.id = -1};
    CHECK(mc_inventory_player_borrow(f->player->local_player->fields.player.c, 0, &current, NULL) ==
          MC_INVENTORY_OK);
    if (current != NULL)
        CHECK(mc_item_stack_fields_get(current, &current_fields) == MC_ITEM_STACK_OK);
    if (captured != NULL)
        CHECK(mc_item_stack_fields_get(captured, &captured_fields) == MC_ITEM_STACK_OK);
    const char *outcome = status == MC_SINGLEPLAYER_MINING_OK ? "OK"
                          : null_failure(&error)              ? "NULL"
                                                              : "NATIVE";
    printf("RESULT\t%d\t%s\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%" PRIu64
           "\t%" PRIu64 "\t%" PRIu64 "\t%" PRIu64 "\t%zu\t%zu\n",
           p->index, outcome, status == MC_SINGLEPLAYER_MINING_OK ? (int)result.removed : -1,
           current != NULL, current_fields.id, current_fields.count, current_fields.damage,
           captured != NULL, captured_fields.id, captured_fields.count, captured_fields.damage,
           target_id, target_metadata, mined, used, broken, world_before, f->world->fields.r->state,
           math_before, atomic_load(&f->app.profile->math->state), item_count(f->world),
           observer.event_count);
    size_t item_index = 0;
    for (size_t i = 0; i < f->world->entity_membership.count; ++i) {
        mc_entity *entity = f->world->entity_membership.entries[i];
        if (!mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_ITEM))
            continue;
        mc_item_stack_fields payload;
        bool in_world, in_chunk;
        CHECK(mc_item_stack_fields_get(entity->stack, &payload) == MC_ITEM_STACK_OK &&
              item_memberships(&observer, entity, &in_world, &in_chunk) && in_world && in_chunk &&
              entity->item.c == 10 && entity->item.b == 0 && entity->phase == MC_ENTITY_READY);
        printf("ITEM\t%d\t%zu\t%d\t%d\t%d\t%d\t%d\t%016" PRIx64 "\t%016" PRIx64 "\t%016" PRIx64
               "\t%016" PRIx64 "\t%016" PRIx64 "\t%016" PRIx64 "\t%08" PRIx32 "\t%08" PRIx32
               "\t%d\t%d\n",
               p->index, item_index++, payload.id, payload.count, payload.damage, entity->item.c,
               entity->item.b, double_bits(entity->base.x), double_bits(entity->base.y),
               double_bits(entity->base.z), double_bits(entity->base.velocity_x),
               double_bits(entity->base.velocity_y), double_bits(entity->base.velocity_z),
               float_bits(entity->base.yaw), float_bits(entity->item.d), (int)in_world,
               (int)in_chunk);
    }
    for (size_t i = 0; i < observer.event_count; ++i) {
        const harvest_event *e = &observer.events[i];
        printf("EVENT\t%d\t%zu\t%s\t%d\t%d\t%d\t%d\t%d\t%zu\t%d\t%d\t%d\t%d\t%" PRIu64 "\t%" PRIu64
               "\n",
               p->index, i, e->kind, e->block, e->metadata, e->mined, e->used, e->broken, e->items,
               e->item_id, e->delay, (int)e->in_world, (int)e->in_chunk, e->world_random,
               e->math_random);
    }
    if (p->fault == 4 || p->fault == 5)
        CHECK(status == MC_SINGLEPLAYER_MINING_SOURCE_NULL && !result.returned &&
              strcmp(error.site.class_name, "nk") == 0 &&
              error.site.pc == (p->fault == 4 ? 28u : 51u) && result.statistic_returned &&
              result.drop_returned && mined == 1 && target_id == 0);
    if (p->fault == 6 || p->fault == 7)
        CHECK(!result.returned && strcmp(error.site.class_name, "jr") == 0 &&
              error.site.pc == (p->fault == 6 ? 129u : 154u) && !result.statistic_returned &&
              result.drop_returned == (p->fault == 7) && mined == 0 && item_count(f->world) == 1 &&
              observer.event_count == 2 && strcmp(observer.events[1].kind, "INSERT") == 0);
    f->app.registry->statistics.C[p->block] = observer.mined;
    f->app.registry->blocks.named[7] = water;
    if (support != NULL)
        support->fields.bA = support_material;
    f->app.stats_writer = f->app.stats_writer_retained;
    CHECK(mc_client_singleplayer_mining_retire(&call, NULL) == MC_SINGLEPLAYER_MINING_OK &&
          mc_client_singleplayer_controller_free(&controller, NULL) ==
              MC_SINGLEPLAYER_CONTROLLER_OK);
    bool removed;
    CHECK(mc_world_runtime_remove_world_access(f->world, access, &removed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          removed && mc_world_access_free(access, NULL) == MC_WORLD_STORAGE_OK);
    mc_item_stack_release(captured);
    f->app.h = NULL;
    f->app.active_world = NULL;
    mc_entity_release(f->player);
    f->player = NULL;
    CHECK(mc_world_runtime_retire_entity_containers(f->world, NULL) == MC_WORLD_STORAGE_OK &&
          mc_chunk_cache_init(acquisition->cache, acquisition->cache->entries,
                              acquisition->cache->capacity, acquisition->cache->insertions,
                              acquisition->cache->insertion_capacity) &&
          mc_world_store_retire_chunk(f->store, state, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_store_chunk_count(f->store) == 0);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 15 || CD(argv[2]) != 0 || !mc_math_trig_init(&trig))
        return 2;
    int64_t values[12];
    for (size_t i = 0; i < 12; ++i) {
        char *end;
        errno = 0;
        values[i] = strtoll(argv[i + 3], &end, 10);
        if (errno != 0 || *end != '\0' ||
            ((i != 9 && i != 10) && (values[i] < INT32_MIN || values[i] > INT32_MAX)))
            return 2;
    }
    const harvest_input input = {(int32_t)values[0],  (int32_t)values[1], (int32_t)values[2],
                                 (int32_t)values[3],  (int32_t)values[4], (int32_t)values[5],
                                 (int32_t)values[6],  (int32_t)values[7], (int32_t)values[8],
                                 (int32_t)values[11], values[9],          values[10]};
    fixture *f = calloc(1, sizeof(*f));
    if (f == NULL || !setup(f, argv[1], "block-harvest", true) || !exercise(f, &input) ||
        !teardown(f))
        return 1;
    free(f);
    printf("PASS: %zu concrete Block harvest checks\n", checks);
    return 0;
}
