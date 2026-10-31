#define main mc_world_access_publication_fixture_main
#include "../test_entity_publication.c"
#undef main

#include "../../../../src/world/runtime/mutation/mutation_internal.h"
#include "../../../../src/world/runtime/world_access_internal.h"
#include "mc/world/runtime/movement.h"
#include "mc/world/runtime/presentation.h"
#include "mc/world/runtime/tile_entities/tile_entities.h"
#include "mc/world/runtime/world_access.h"

/* Real constructed Worlds and live source methods exercise the public
 * registration seam. These callbacks are native contract receivers; no
 * renderer, audio output, tracker or original callback trace is claimed. */
enum { CELL, RANGE, SOUND, PARTICLE, ENTITY_ADDED, TILE_DIRTY, EVENT_KINDS };

typedef struct world_access_log {
    unsigned int ids[4096], kinds[4096];
    mc_world_runtime *worlds[4096];
    size_t count;
    bool bad;
} world_access_log;

typedef struct receiver {
    world_access_log *log;
    unsigned int id;
    size_t calls[EVENT_KINDS];
    int32_t coordinates[6];
    double values[6];
    float volume, pitch;
    const mc_registry_string *name;
    mc_entity *entity, *move_entity;
    mc_tile_entity *tile_entity;
    mc_world_access *self, *append;
    mc_world_runtime *peer;
    mc_world_reference_list *replace_list;
    bool remove_self, move_once, reenter, reentered, saw_outer_suppression;
    mc_world_storage_status free_while_active, retire_while_active;
    unsigned int fail_kind;
} receiver;

static mc_world_storage_status observed(receiver *r, mc_world_runtime *world, unsigned int kind,
                                        mc_world_storage_error *error) {
    if (kind >= EVENT_KINDS || r->log->count == COUNT(r->log->ids)) {
        r->log->bad = true;
        *error = (mc_world_storage_error){MC_WORLD_STORAGE_LIMIT, "world_access test log", 0};
        return error->status;
    }
    const size_t at = r->log->count++;
    r->log->ids[at] = r->id;
    r->log->kinds[at] = kind;
    r->log->worlds[at] = world;
    ++r->calls[kind];
    if (kind == SOUND && r->append != NULL) {
        mc_world_access *append = r->append;
        r->append = NULL;
        if (mc_world_runtime_add_world_access(world, append, error) != MC_WORLD_STORAGE_OK)
            return error->status;
    }
    if (kind == SOUND && r->remove_self) {
        bool removed = false;
        r->remove_self = false;
        if (mc_world_runtime_remove_world_access(world, r->self, &removed, error) !=
                MC_WORLD_STORAGE_OK ||
            !removed)
            return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
        r->free_while_active = mc_world_access_free(r->self, NULL);
        r->retire_while_active = mc_world_runtime_retire_entity_containers(world, NULL);
    }
    if (kind == SOUND && r->replace_list != NULL) {
        world->fields.u = r->replace_list;
        r->replace_list = NULL;
    }
    if (kind == SOUND && r->move_once) {
        r->move_once = false;
        if (mc_entity_set_position(r->move_entity, 9.5, 71.0, 10.5) != MC_ENTITY_OK)
            return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    }
    if (kind == RANGE && r->reenter && !r->reentered && world != r->peer) {
        r->reentered = true;
        r->saw_outer_suppression = world->fields.o;
        mc_world_mutation_error *child = malloc(sizeof(*child));
        if (child == NULL)
            return MC_WORLD_STORAGE_ALLOCATION_FAILED;
        const mc_world_storage_status status =
            mc_world_runtime_metadata_set(r->peer, 6, 64, 8, 1, child);
        *error = child->cause;
        free(child);
        if (status != MC_WORLD_STORAGE_OK)
            return status;
    }
    if (r->fail_kind == kind) {
        *error = (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR, "receiver failure", 73};
        return error->status;
    }
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status cell_callback(void *context, mc_world_runtime *world, int32_t x,
                                             int32_t y, int32_t z, mc_world_storage_error *error) {
    receiver *r = context;
    r->coordinates[0] = x;
    r->coordinates[1] = y;
    r->coordinates[2] = z;
    return observed(r, world, CELL, error);
}

static mc_world_storage_status range_callback(void *context, mc_world_runtime *world, int32_t min_x,
                                              int32_t min_y, int32_t min_z, int32_t max_x,
                                              int32_t max_y, int32_t max_z,
                                              mc_world_storage_error *error) {
    receiver *r = context;
    const int32_t coordinates[] = {min_x, min_y, min_z, max_x, max_y, max_z};
    memcpy(r->coordinates, coordinates, sizeof(coordinates));
    return observed(r, world, RANGE, error);
}

static mc_world_storage_status sound_callback(void *context, mc_world_runtime *world,
                                              const mc_registry_string *name, double x, double y,
                                              double z, float volume, float pitch,
                                              mc_world_storage_error *error) {
    receiver *r = context;
    r->name = name;
    r->values[0] = x;
    r->values[1] = y;
    r->values[2] = z;
    r->volume = volume;
    r->pitch = pitch;
    return observed(r, world, SOUND, error);
}

static mc_world_storage_status particle_callback(void *context, mc_world_runtime *world,
                                                 const mc_registry_string *name, double x, double y,
                                                 double z, double vx, double vy, double vz,
                                                 mc_world_storage_error *error) {
    receiver *r = context;
    const double values[] = {x, y, z, vx, vy, vz};
    r->name = name;
    memcpy(r->values, values, sizeof(values));
    return observed(r, world, PARTICLE, error);
}

static mc_world_storage_status entity_callback(void *context, mc_world_runtime *world,
                                               mc_entity *entity, mc_world_storage_error *error) {
    receiver *r = context;
    r->entity = entity;
    bool found = false;
    for (size_t i = 0; i < world->fields.b->count; ++i)
        found = found || world->fields.b->items[i] == entity;
    if (!found)
        r->log->bad = true;
    return observed(r, world, ENTITY_ADDED, error);
}

static mc_world_storage_status tile_entity_callback(void *context, mc_world_runtime *world,
                                                    int32_t x, int32_t y, int32_t z,
                                                    mc_tile_entity *tile_entity,
                                                    mc_world_storage_error *error) {
    receiver *r = context;
    r->coordinates[0] = x;
    r->coordinates[1] = y;
    r->coordinates[2] = z;
    r->tile_entity = tile_entity;
    return observed(r, world, TILE_DIRTY, error);
}

static const mc_world_access_callbacks callbacks = {.cell = cell_callback,
                                                    .range = range_callback,
                                                    .sound = sound_callback,
                                                    .particle = particle_callback,
                                                    .entity_added = entity_callback,
                                                    .tile_entity_dirty = tile_entity_callback,
                                                    .entity_removed = NULL};

static void reset_log(world_access_log *log) {
    log->count = 0;
    log->bad = false;
}

static bool ids_are(const world_access_log *log, const unsigned int *expected, size_t count) {
    return !log->bad && log->count == count &&
           memcmp(log->ids, expected, count * sizeof(*expected)) == 0;
}

static bool remove_receiver(mc_world_runtime *world, mc_world_access *world_access) {
    bool removed = false;
    return mc_world_runtime_remove_world_access(world, world_access, &removed, NULL) ==
               MC_WORLD_STORAGE_OK &&
           removed;
}

static bool arguments(fixture *f, size_t side, receiver *r) {
    bool ok = false, entered = false;
    mc_world_runtime *world = f->worlds[side];
    mc_world_mutation_error *error = malloc(sizeof(*error));
    mc_tile_entity *tile_entity = NULL;
    mc_entity *entity = NULL;
    CHECK(error != NULL);
    const uint64_t nan_bits = UINT64_C(0x7ff8000000001234);
    double nan;
    memcpy(&nan, &nan_bits, sizeof(nan));
    const double positions[] = {-0.0, nan, 1.25};
    CHECK(mc_world_runtime_sound(world, positions[0], positions[1], positions[2], NULL, -0.0f, 1.5f,
                                 error) == MC_WORLD_STORAGE_OK);
    CHECK(r->name == NULL && memcmp(r->values, positions, sizeof(positions)) == 0 &&
          memcmp(&r->volume, &(float){-0.0f}, sizeof(float)) == 0 && r->pitch == 1.5f);
    const double particles[] = {-4.5, 9.25, -0.0, nan, 0.0, 1.0};
    CHECK(mc_world_runtime_particle(world, NULL, particles[0], particles[1], particles[2],
                                    particles[3], particles[4], particles[5],
                                    error) == MC_WORLD_STORAGE_OK);
    CHECK(memcmp(r->values, particles, sizeof(particles)) == 0);
    CHECK(mc_world_runtime_call_enter(world, NULL) == MC_WORLD_STORAGE_OK);
    entered = true;
    mc_world_mutation_error_init(error);
    CHECK(mc_world_mutation_observe_cell(world, INT32_MIN, -50, INT32_MAX, error));
    CHECK(r->coordinates[0] == INT32_MIN && r->coordinates[1] == -50 &&
          r->coordinates[2] == INT32_MAX && error->world_access_bytecode == 0);
    CHECK(mc_world_mutation_observe_range(world, 9, 80, -7, -2, -12, 18, error));
    const int32_t bounds[] = {9, 80, -7, -2, -12, 18};
    CHECK(memcmp(r->coordinates, bounds, sizeof(bounds)) == 0);
    CHECK(mc_world_mutation_observe_column(world, -17, 31, 90, 4, error));
    const int32_t column[] = {-17, 4, 31, -17, 90, 31};
    CHECK(memcmp(r->coordinates, column, sizeof(column)) == 0);
    mc_world_runtime_call_leave(world);
    entered = false;
    CHECK(make_entity(f, side, world, 8.5, 64.0, 8.5, &entity));
    bool inserted = false;
    mc_world_entity_error publication_error;
    CHECK(mc_world_runtime_insert_entity(world, entity, &inserted, &publication_error) ==
              MC_WORLD_STORAGE_OK &&
          inserted && r->entity == entity && !r->log->bad);
    CHECK(mc_tile_entity_allocate(world, MC_TILE_ENTITY_CHEST, &tile_entity) == MC_TILE_ENTITY_OK &&
          mc_tile_entity_construct(tile_entity) == MC_TILE_ENTITY_OK);
    mc_live_tile_entity_error tile_entity_error;
    CHECK(mc_world_runtime_tile_entity_dirty_detailed(world, 9, 65, 10, tile_entity,
                                                      &tile_entity_error) == MC_WORLD_STORAGE_OK &&
          r->tile_entity == tile_entity && f->states[side]->dirty);
    /* No loaded area at this coordinate: dispatch still follows the false
     * presence result, and carries the nullable TileEntity unchanged. */
    CHECK(mc_world_runtime_tile_entity_dirty_detailed(world, 999, -1, -999, NULL,
                                                      &tile_entity_error) == MC_WORLD_STORAGE_OK &&
          r->tile_entity == NULL && r->coordinates[0] == 999 && r->coordinates[1] == -1 &&
          r->coordinates[2] == -999);
    for (size_t i = 0; i < EVENT_KINDS; ++i)
        CHECK(r->calls[i] != 0);
    CHECK(world->active_calls == 0 && world->local_leases == 0);
    ok = true;
cleanup:
    if (entered)
        mc_world_runtime_call_leave(world);
    mc_tile_entity_release(tile_entity);
    free(error);
    return ok;
}

static bool live_list(fixture *f, receiver *a, receiver *b, receiver *c) {
    bool ok = false;
    mc_world_runtime *world = f->worlds[0];
    mc_world_mutation_error *error = malloc(sizeof(*error));
    world_access_log *log = a->log;
    CHECK(error != NULL &&
          mc_world_runtime_add_world_access(world, b->self, NULL) == MC_WORLD_STORAGE_OK);
    reset_log(log);
    a->append = c->self;
    CHECK(mc_world_runtime_sound(world, 1, 2, 3, NULL, 4, 5, error) == MC_WORLD_STORAGE_OK);
    const unsigned int appended[] = {a->id, b->id, c->id};
    CHECK(ids_are(log, appended, COUNT(appended)));
    reset_log(log);
    a->remove_self = true;
    CHECK(mc_world_runtime_sound(world, 1, 2, 3, NULL, 4, 5, error) == MC_WORLD_STORAGE_OK);
    const unsigned int removed[] = {a->id, c->id};
    CHECK(ids_are(log, removed, COUNT(removed)) && a->free_while_active == MC_WORLD_STORAGE_BUSY &&
          a->retire_while_active == MC_WORLD_STORAGE_BUSY);
    CHECK(remove_receiver(world, b->self) && remove_receiver(world, c->self));
    CHECK(mc_world_runtime_add_world_access(world, b->self, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(world, b->self, NULL) == MC_WORLD_STORAGE_OK);
    reset_log(log);
    CHECK(mc_world_runtime_sound(world, 0, 0, 0, NULL, 0, 0, error) == MC_WORLD_STORAGE_OK);
    const unsigned int duplicate[] = {b->id, b->id};
    CHECK(ids_are(log, duplicate, COUNT(duplicate)) && remove_receiver(world, b->self));
    reset_log(log);
    CHECK(mc_world_runtime_sound(world, 0, 0, 0, NULL, 0, 0, error) == MC_WORLD_STORAGE_OK);
    CHECK(ids_are(log, duplicate, 1) && remove_receiver(world, b->self));
    CHECK(mc_world_runtime_add_world_access(world, NULL, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_sound(world, 0, 0, 0, NULL, 0, 0, error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          error->world_access_bytecode == 41);
    CHECK(remove_receiver(world, NULL));
    bool absent = true;
    CHECK(mc_world_runtime_remove_world_access(world, b->self, &absent, NULL) ==
              MC_WORLD_STORAGE_OK &&
          !absent);
    CHECK(mc_world_runtime_add_world_access(world, a->self, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(world, b->self, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(world, c->self, NULL) == MC_WORLD_STORAGE_OK);
    reset_log(log);
    b->fail_kind = SOUND;
    const mc_world_storage_status failed =
        mc_world_runtime_sound(world, 0, 0, 0, NULL, 0, 0, error);
    b->fail_kind = EVENT_KINDS;
    const unsigned int prefix[] = {a->id, b->id};
    CHECK(failed == MC_WORLD_STORAGE_IO_ERROR && ids_are(log, prefix, COUNT(prefix)) &&
          error->world_access_bytecode == 41 && error->cause.system_error == 73 &&
          strcmp(error->cause.operation, "receiver failure") == 0);
    CHECK(remove_receiver(world, b->self) && remove_receiver(world, c->self));
    /* A returned callback can replace u with a different empty descriptor.
     * The loop must reread that descriptor instead of the owned list. */
    mc_world_reference_list empty = {0};
    mc_world_reference_list *owned = world->fields.u;
    a->replace_list = &empty;
    reset_log(log);
    const mc_world_storage_status replaced =
        mc_world_runtime_sound(world, 0, 0, 0, NULL, 0, 0, error);
    world->fields.u = owned;
    CHECK(replaced == MC_WORLD_STORAGE_OK && ids_are(log, prefix, 1));
    CHECK(world->active_calls == 0 && world->local_leases == 0);
    ok = true;
cleanup:
    free(error);
    return ok;
}

static bool untyped(fixture *f, size_t side) {
    bool ok = false;
    mc_world_runtime *world = f->worlds[side];
    mc_world_reference_list *owned = world->fields.u;
    mc_world_mutation_error *error = malloc(sizeof(*error));
    CHECK(error != NULL);
    void *items[] = {world}; /* Valid storage, deliberately not an world_access. */
    mc_world_reference_list foreign = {items, 1, 1};
    world->fields.u = &foreign;
    mc_world_storage_status status = mc_world_runtime_sound(world, 0, 0, 0, NULL, 0, 0, error);
    world->fields.u = owned;
    CHECK(status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && error->world_access_bytecode == 28);
    items[0] = NULL;
    world->fields.u = &foreign;
    status = mc_world_runtime_particle(world, NULL, 0, 0, 0, 0, 0, 0, error);
    world->fields.u = owned;
    CHECK(status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && error->world_access_bytecode == 43);
    foreign.count = 0;
    world->fields.u = &foreign;
    status = mc_world_runtime_sound(world, 0, 0, 0, NULL, 0, 0, error);
    world->fields.u = owned;
    CHECK(status == MC_WORLD_STORAGE_OK);
    foreign.count = 2;
    world->fields.u = &foreign;
    status = mc_world_runtime_sound(world, 0, 0, 0, NULL, 0, 0, error);
    world->fields.u = owned;
    CHECK(status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && error->world_access_bytecode == 9);
    CHECK(world->active_calls == 0 && world->local_leases == 0);
    ok = true;
cleanup:
    world->fields.u = owned;
    free(error);
    return ok;
}

static bool entity_sound(fixture *f, receiver *a, receiver *b) {
    bool ok = false;
    mc_world_runtime *world = f->worlds[0];
    mc_entity *entity = NULL;
    mc_world_movement_error *error = malloc(sizeof(*error));
    CHECK(error != NULL && make_entity(f, 0, world, 8.5, 64, 8.5, &entity));
    CHECK(mc_world_runtime_add_world_access(world, b->self, NULL) == MC_WORLD_STORAGE_OK);
    a->move_entity = entity;
    a->move_once = true;
    const double old_y =
        mc_entity_runtime_fields(entity)->y - (double)mc_entity_runtime_fields(entity)->offset;
    CHECK(mc_world_runtime_entity_sound(world, entity, NULL, 0.5f, 0.75f, error) ==
          MC_WORLD_STORAGE_OK);
    CHECK(a->values[0] == 8.5 && a->values[1] == old_y && a->values[2] == 8.5);
    CHECK(b->values[0] == 9.5 &&
          b->values[1] == 71.0 - (double)mc_entity_runtime_fields(entity)->offset &&
          b->values[2] == 10.5);
    CHECK(remove_receiver(world, b->self));
    CHECK(mc_world_runtime_entity_sound(world, NULL, NULL, 0, 0, error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          error->bytecode == 33);
    CHECK(world->active_calls == 0 && world->local_leases == 0);
    ok = true;
cleanup:
    free(error);
    return ok;
}

static bool registration_controls(fixture *f, receiver *a, mc_world_access **retained) {
    bool ok = false;
    mc_world_runtime *world = f->worlds[0], *empty_world = f->worlds[2];
    mc_world_access *missing = NULL;
    const mc_world_access_callbacks absent = {0};
    mc_world_mutation_error *error = malloc(sizeof(*error));
    mc_world_movement_error *movement = malloc(sizeof(*movement));
    const size_t limit = empty_world->construction_limits.max_direct_allocation_bytes;
    CHECK(error != NULL && movement != NULL &&
          mc_world_access_create(&absent, NULL, &missing, NULL) == MC_WORLD_STORAGE_OK);
    *retained = missing;
    const size_t allocated = empty_world->construction.direct_allocation_bytes;
    empty_world->construction_limits.max_direct_allocation_bytes = allocated;
    const mc_world_storage_status limited =
        mc_world_runtime_add_world_access(empty_world, missing, NULL);
    empty_world->construction_limits.max_direct_allocation_bytes = limit;
    CHECK(limited == MC_WORLD_STORAGE_LIMIT && empty_world->fields.u->count == 0 &&
          empty_world->construction.direct_allocation_bytes == allocated);
    CHECK(mc_world_runtime_entity_sound(empty_world, NULL, NULL, 0, 0, movement) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_add_world_access(world, missing, NULL) == MC_WORLD_STORAGE_OK);
    reset_log(a->log);
    CHECK(mc_world_runtime_sound(world, 0, 0, 0, NULL, 0, 0, error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          error->world_access_bytecode == 41 && a->log->count == 1 && a->log->ids[0] == a->id);
    CHECK(remove_receiver(world, missing));
    CHECK(mc_world_runtime_add_world_access(empty_world, missing, NULL) == MC_WORLD_STORAGE_OK);
    /* The absent sound slot is checked after the actual Entity operand reads;
     * NULL Entity therefore fails at33, before the missing invoke53. */
    CHECK(mc_world_runtime_entity_sound(empty_world, NULL, NULL, 0, 0, movement) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          movement->bytecode == 33);
    CHECK(remove_receiver(empty_world, missing));
    /* Foreign replacement storage must never become owned registration data. */
    mc_world_reference_list *owned = empty_world->fields.u;
    mc_world_reference_list foreign = {0};
    empty_world->fields.u = &foreign;
    const mc_world_storage_status replacement =
        mc_world_runtime_add_world_access(empty_world, missing, NULL);
    empty_world->fields.u = owned;
    CHECK(replacement == MC_WORLD_STORAGE_UNSUPPORTED_STATE && foreign.count == 0);
    CHECK(mc_world_access_free(missing, NULL) == MC_WORLD_STORAGE_OK);
    missing = NULL;
    *retained = NULL;
    CHECK(world->active_calls == 0 && empty_world->active_calls == 0 &&
          mc_world_access_free(NULL, NULL) == MC_WORLD_STORAGE_OK);
    ok = true;
cleanup:
    empty_world->construction_limits.max_direct_allocation_bytes = limit;
    free(movement);
    free(error);
    return ok;
}

static bool set_cell(fixture *f, size_t slot, int32_t x, int32_t y, uint8_t id) {
    f->states[slot]->data->blocks[((size_t)x << 11u) | (8u << 7u) | (size_t)y] = id;
    bool ignored = false;
    return mc_world_runtime_metadata_set_quiet(f->worlds[slot], x, y, 8, 0, &ignored, NULL) ==
           MC_WORLD_STORAGE_OK;
}

static bool reentry(fixture *f, size_t side, receiver *r) {
    bool ok = false;
    mc_world_mutation_error *error = malloc(sizeof(*error));
    CHECK(error != NULL);
    const size_t slots[] = {side, side + 2u};
    mc_java_random before[2];
    for (size_t i = 0; i < COUNT(slots); ++i) {
        const size_t slot = slots[i];
        memset(f->states[slot]->data->blocks, 0, MC_CHUNK_BLOCKS);
        memset(f->states[slot]->data->metadata, 0, MC_CHUNK_NIBBLE_BYTES);
        for (int32_t x = 6; x <= 9; ++x)
            CHECK(set_cell(f, slot, x, 63, 1) && set_cell(f, slot, x, 64, x == 6 ? 70 : 55));
        before[i] = *f->worlds[slot]->fields.r;
    }
    CHECK(mc_world_runtime_add_world_access(f->worlds[side + 2u], r->self, NULL) ==
          MC_WORLD_STORAGE_OK);
    r->peer = f->worlds[side + 2u];
    r->reenter = true;
    r->reentered = false;
    CHECK(mc_world_runtime_metadata_set(f->worlds[side], 6, 64, 8, 1, error) ==
          MC_WORLD_STORAGE_OK);
    r->reenter = false;
    CHECK(r->reentered && r->saw_outer_suppression);
    for (size_t i = 0; i < COUNT(slots); ++i) {
        mc_world_runtime *world = f->worlds[slots[i]];
        for (int32_t x = 7; x <= 9; ++x) {
            int32_t metadata = -1;
            CHECK(mc_world_runtime_metadata_get(world, x, 64, 8, &metadata, NULL) ==
                      MC_WORLD_STORAGE_OK &&
                  metadata == 22 - x);
        }
        CHECK(!world->fields.o && world->active_calls == 0 && world->local_leases == 0 &&
              mc_test_java_random_equal(&before[i], world->fields.r));
    }
    mc_registry_redstone_set_view view;
    CHECK(f->registry[side]->blocks.m[55]->subtype.sm.a &&
          f->registry[side]->redstone_snapshot_count == 0 &&
          mc_registry_redstone_set_inspect(f->registry[side]->blocks.m[55]->subtype.sm.b, &view) ==
              MC_REGISTRY_OK &&
          view.size == 0);
    ok = true;
cleanup:
    free(error);
    return ok;
}

static bool terminal_controls(fixture *f, size_t side, receiver *r) {
    bool ok = false;
    const size_t slot = side + 2u;
    mc_world_runtime *world = f->worlds[slot];
    mc_world_reference_list *owned = world->fields.u;
    const size_t bytes = world->construction.direct_allocation_bytes;
    const size_t memberships = r->self->memberships;
    void *items[] = {world};
    mc_world_reference_list foreign = {items, 1, 1};
    world->fields.u = &foreign;
    const mc_world_storage_status rejected = mc_world_runtime_retire_entity_containers(world, NULL);
    world->fields.u = owned;
    CHECK(rejected == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          world->construction.phase == MC_WORLD_RUNTIME_READY &&
          world->construction.direct_allocation_bytes == bytes &&
          r->self->memberships == memberships);
    foreign.count = 0;
    const size_t calls = r->log->count;
    world->fields.u = &foreign;
    const mc_world_storage_status retired = mc_world_runtime_retire_entity_containers(world, NULL);
    world->fields.u = owned;
    if (retired == MC_WORLD_STORAGE_OK)
        f->terminal[slot] = true;
    CHECK(retired == MC_WORLD_STORAGE_OK && world->entity_containers_retired &&
          world->construction.phase == MC_WORLD_RUNTIME_RETIRED && owned->count == 0 &&
          r->self->memberships == memberships - 1u && r->log->count == calls &&
          foreign.items == items && foreign.capacity == 1 &&
          mc_world_access_free(r->self, NULL) == MC_WORLD_STORAGE_BUSY);
    ok = true;
cleanup:
    world->fields.u = owned;
    return ok;
}

int main(int argc, char **argv) {
    int result = 1;
    fixture *f = NULL;
    world_access_log *log = NULL;
    mc_world_access *control = NULL;
    receiver receivers[4] = {0};
    char root[8192];
    const uint16_t title[] = {'O', 'b', 's', 'e', 'r', 'v', 'e', 'r'};
    if (argc != 3 || strcmp(argv[1], "world-access") != 0 ||
        !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 1;
    f = calloc(1, sizeof(*f));
    log = calloc(1, sizeof(*log));
    CHECK(f != NULL && log != NULL &&
          mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_materials_create(&f->materials));
    CHECK(mc_java_random_profile_create(4u * 1024u * 1024u, &f->profile) ==
          MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
          MC_WORLD_INFO_OK);
    CHECK(mc_item_stack_allocate(&f->stack) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(f->stack, 262, 5, 7) == MC_ITEM_STACK_OK);
    for (size_t side = 0; side < 2; ++side) {
        resources *r = &f->sources[side];
        r->streams[0] = (stream){.bytes = language, .length = sizeof(language) - 1u};
        r->streams[1] = r->streams[2] = (stream){.bytes = language};
        r->api = (mc_registry_resources){r, open_resource, close_resource, ready_resource};
        const mc_registry_options options = {side == 0 ? MC_REGISTRY_CLIENT : MC_REGISTRY_SERVER,
                                             f->profile, f->materials, 0, &r->api};
        CHECK(mc_registry_classes_allocate(&options, &f->registry[side], NULL) == MC_REGISTRY_OK);
        CHECK(mc_world_runtime_classes_create(
                  side == 0 ? MC_WORLD_INFO_CLIENT : MC_WORLD_INFO_SERVER, f->profile, &trig,
                  &f->classes[side], NULL) == MC_WORLD_STORAGE_OK);
        CHECK(mc_world_runtime_classes_bind_registry(f->classes[side], f->registry[side], NULL) ==
              MC_REGISTRY_OK);
        CHECK(mc_entity_classes_create(side == 0 ? MC_ENTITY_CLIENT : MC_ENTITY_SERVER, f->profile,
                                       NULL, &f->entity_classes[side]) == MC_ENTITY_OK &&
              mc_world_runtime_classes_bind_entities(f->classes[side], f->entity_classes[side]) ==
                  MC_ENTITY_OK);
        CHECK(mc_registry_classes_initialize_blocks(f->registry[side], NULL) == MC_REGISTRY_OK);
    }
    for (size_t slot = 0; slot < 4; ++slot)
        CHECK(open_store(f, slot, root) && create_world(f, slot));
    for (size_t i = 0; i < COUNT(receivers); ++i) {
        receivers[i].id = (unsigned int)i + 1u;
        receivers[i].log = log;
        receivers[i].fail_kind = EVENT_KINDS;
        CHECK(mc_world_access_create(&callbacks, &receivers[i], &receivers[i].self, NULL) ==
              MC_WORLD_STORAGE_OK);
    }
    CHECK(mc_world_runtime_add_world_access(f->worlds[0], receivers[0].self, NULL) ==
              MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(f->worlds[1], receivers[1].self, NULL) ==
              MC_WORLD_STORAGE_OK);
    CHECK(mc_world_access_free(receivers[0].self, NULL) == MC_WORLD_STORAGE_BUSY);
    bool untouched = true;
    CHECK(mc_world_runtime_remove_world_access(f->worlds[1], receivers[1].self, &untouched, NULL) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          untouched);
    CHECK(arguments(f, 0, &receivers[0]) && arguments(f, 1, &receivers[1]));
    CHECK(live_list(f, &receivers[0], &receivers[2], &receivers[3]));
    CHECK(untyped(f, 0) && untyped(f, 1));
    CHECK(entity_sound(f, &receivers[0], &receivers[2]));
    CHECK(registration_controls(f, &receivers[0], &control));
    CHECK(reentry(f, 0, &receivers[0]) && reentry(f, 1, &receivers[1]));
    CHECK(terminal_controls(f, 0, &receivers[0]) && terminal_controls(f, 1, &receivers[1]));
    CHECK(!log->bad);
    result = 0;
cleanup:
    if (!cleanup_fixture(f)) {
        fputs("world_access cleanup refused; surviving owners retained\n", stderr);
        return 1;
    }
    for (size_t i = 0; i < COUNT(receivers); ++i)
        if (mc_world_access_free(receivers[i].self, NULL) != MC_WORLD_STORAGE_OK)
            result = 1;
    if (mc_world_access_free(control, NULL) != MC_WORLD_STORAGE_OK)
        result = 1;
    free(log);
    if (result == 0)
        printf("PASS: %u live WorldAccess checks\n", checks);
    return result;
}
