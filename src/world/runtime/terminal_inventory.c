#include "terminal_inventory_internal.h"

#include "../../entity/entity_internal.h"
#include "../../entity/mob/internal.h"
#include "../../entity/mob/references_internal.h"
#include "construction_internal.h"
#include "mc/world/dimension/world_provider.h"
#include "runtime_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef enum terminal_record_kind {
    TERMINAL_ENTITY = 1,
    TERMINAL_TILE,
    TERMINAL_SLOT,
    TERMINAL_CHUNK,
    TERMINAL_PATH,
    TERMINAL_PATH_ARENA
} terminal_record_kind;

typedef struct terminal_record {
    const void *identity;
    terminal_record_kind kind;
    size_t holds, occurrences;
} terminal_record;

typedef struct terminal_collector {
    mc_world_runtime *world;
    mc_world_terminal_limits limits;
    mc_world_terminal_inventory inventory;
    terminal_record *records;
    size_t capacity, entries, bytes;
    mc_entity **entities;
    size_t entity_count, entity_capacity;
} terminal_collector;

static mc_world_storage_status result(mc_world_storage_error *error, mc_world_storage_status status,
                                      const char *operation) {
    if (error != NULL)
        *error = (mc_world_storage_error){status, operation, 0};
    return status;
}

mc_world_terminal_limits mc_world_runtime_terminal_default_limits(void) {
    /* Entries count all distinct object, owning-slot and Chunk records. Peak
     * bytes include both old and replacement tables during native growth. */
    return (mc_world_terminal_limits){65536u, 16u * 1048576u};
}

static size_t bucket(const void *identity, terminal_record_kind kind, size_t capacity) {
    uint64_t bits = (uint64_t)(uintptr_t)identity;
    bits ^= (uint64_t)kind * UINT64_C(0x9e3779b97f4a7c15);
    bits ^= bits >> 30;
    bits *= UINT64_C(0xbf58476d1ce4e5b9);
    bits ^= bits >> 27;
    bits *= UINT64_C(0x94d049bb133111eb);
    bits ^= bits >> 31;
    return (size_t)bits & (capacity - 1u);
}

static terminal_record *find(terminal_record *records, size_t capacity, const void *identity,
                             terminal_record_kind kind) {
    size_t index = bucket(identity, kind, capacity);
    while (records[index].identity != NULL &&
           (records[index].identity != identity || records[index].kind != kind))
        index = (index + 1u) & (capacity - 1u);
    return &records[index];
}

static mc_world_storage_status grow(terminal_collector *c, mc_world_storage_error *error) {
    if (c->capacity > SIZE_MAX / 2u)
        return result(error, MC_WORLD_STORAGE_LIMIT, "World terminal scratch capacity overflow");
    const size_t capacity = c->capacity == 0 ? 16u : c->capacity * 2u;
    if (capacity > SIZE_MAX / sizeof(*c->records))
        return result(error, MC_WORLD_STORAGE_LIMIT, "World terminal scratch table overflow");
    const size_t bytes = capacity * sizeof(*c->records);
    if (c->bytes > c->limits.max_bytes || bytes > c->limits.max_bytes - c->bytes)
        return result(error, MC_WORLD_STORAGE_LIMIT, "World terminal scratch peak byte bound");
    terminal_record *records = calloc(capacity, sizeof(*records));
    if (records == NULL)
        return result(error, MC_WORLD_STORAGE_ALLOCATION_FAILED,
                      "World terminal scratch allocation");
    for (size_t i = 0; i < c->capacity; ++i)
        if (c->records[i].identity != NULL)
            *find(records, capacity, c->records[i].identity, c->records[i].kind) = c->records[i];
    const size_t old_bytes = c->capacity * sizeof(*c->records);
    free(c->records);
    c->records = records;
    c->capacity = capacity;
    c->bytes = c->bytes - old_bytes + bytes;
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status queue_entity(terminal_collector *c, mc_entity *entity,
                                            mc_world_storage_error *error) {
    if (c->entity_count == c->entity_capacity) {
        if (c->entity_capacity > SIZE_MAX / 2u)
            return result(error, MC_WORLD_STORAGE_LIMIT, "World terminal Entity queue overflow");
        const size_t capacity = c->entity_capacity == 0 ? 16u : c->entity_capacity * 2u;
        if (capacity > SIZE_MAX / sizeof(*c->entities))
            return result(error, MC_WORLD_STORAGE_LIMIT, "World terminal Entity queue span");
        const size_t bytes = capacity * sizeof(*c->entities);
        if (c->bytes > c->limits.max_bytes || bytes > c->limits.max_bytes - c->bytes)
            return result(error, MC_WORLD_STORAGE_LIMIT,
                          "World terminal Entity queue peak byte bound");
        mc_entity **entities = malloc(bytes);
        if (entities == NULL)
            return result(error, MC_WORLD_STORAGE_ALLOCATION_FAILED,
                          "World terminal Entity queue allocation");
        if (c->entity_count != 0)
            memcpy(entities, c->entities, c->entity_count * sizeof(*entities));
        c->bytes = c->bytes - c->entity_capacity * sizeof(*c->entities) + bytes;
        free(c->entities);
        c->entities = entities;
        c->entity_capacity = capacity;
    }
    c->entities[c->entity_count++] = entity;
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status record(terminal_collector *c, const void *identity,
                                      terminal_record_kind kind, terminal_record **out,
                                      bool *created, mc_world_storage_error *error) {
    if (identity == NULL)
        return result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                      "World terminal owning identity required");
    if (c->capacity != 0) {
        terminal_record *existing = find(c->records, c->capacity, identity, kind);
        if (existing->identity != NULL) {
            *out = existing;
            *created = false;
            return MC_WORLD_STORAGE_OK;
        }
    }
    if (c->entries == c->limits.max_entries || c->entries == SIZE_MAX)
        return result(error, MC_WORLD_STORAGE_LIMIT, "World terminal distinct record bound");
    if (c->capacity == 0 || c->entries + 1u > c->capacity / 2u) {
        const mc_world_storage_status status = grow(c, error);
        if (status != MC_WORLD_STORAGE_OK)
            return status;
    }
    terminal_record *entry = find(c->records, c->capacity, identity, kind);
    entry->identity = identity;
    entry->kind = kind;
    ++c->entries;
    *out = entry;
    *created = true;
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status owning_slot(terminal_collector *c, const void *identity,
                                           mc_world_storage_error *error) {
    terminal_record *entry;
    bool created;
    const mc_world_storage_status status =
        record(c, identity, TERMINAL_SLOT, &entry, &created, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    if (!created)
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "World terminal repeated owning slot");
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status increment(size_t *value, mc_world_storage_error *error) {
    if (*value == SIZE_MAX)
        return result(error, MC_WORLD_STORAGE_LIMIT, "World terminal occurrence overflow");
    ++*value;
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status collect_entity(void *context, const void *slot, mc_entity *value,
                                              mc_world_storage_error *error) {
    terminal_collector *c = context;
    mc_world_storage_status status = owning_slot(c, slot, error);
    if (status != MC_WORLD_STORAGE_OK || value == NULL)
        return status;
    terminal_record *entry;
    bool created;
    status = record(c, value, TERMINAL_ENTITY, &entry, &created, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    if (created) {
        if (value->classes != c->world->classes->entities || value->holds == 0 ||
            (unsigned int)value->phase > (unsigned int)MC_ENTITY_FAILED_CONSTRUCTION)
            return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                          "World terminal canonical Entity owner");
        if (value->phase == MC_ENTITY_CONSTRUCTING)
            return result(error, MC_WORLD_STORAGE_BUSY,
                          "World terminal Entity constructor still entered");
        mc_entity_class_literal_view view;
        const mc_entity_class_literal *canonical = NULL;
        if (mc_entity_class_literal_inspect(value->class_literal, &view) != MC_ENTITY_OK ||
            view.owner != value->classes || view.source != MC_ENTITY_CLIENT ||
            mc_entity_class_literal_get(value->classes, view.kind, &canonical) != MC_ENTITY_OK ||
            canonical != value->class_literal)
            return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                          "World terminal canonical Entity Class identity");
        if (value->particle != NULL || value->base.aG != NULL || value->base.aH != NULL)
            return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                          "World terminal unqualified Entity child or relationship graph");
        if (value->mob != NULL && value->mob->tick_active)
            return result(error, MC_WORLD_STORAGE_BUSY, "World terminal Mob tick still entered");
        if (value->mob != NULL && value->mob->pending_ray != NULL)
            return result(error, MC_WORLD_STORAGE_BUSY, "World terminal Mob ray cleanup pending");
        if (value->mob != NULL && value->mob->pending_path != NULL)
            return result(error, MC_WORLD_STORAGE_BUSY, "World terminal Mob path cleanup pending");
        if (value->base.world != NULL && value->base.world != c->world)
            return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                          "World terminal Entity bound outside admitted World");
        entry->holds = value->holds;
        status = increment(&c->inventory.entities, error);
        if (status != MC_WORLD_STORAGE_OK)
            return status;
        if (value->base.world != NULL) {
            status = increment(&c->inventory.expected_world_bindings, error);
            if (status != MC_WORLD_STORAGE_OK)
                return status;
        }
        status = queue_entity(c, value, error);
        if (status != MC_WORLD_STORAGE_OK)
            return status;
    }
    status = increment(&entry->occurrences, error);
    return status == MC_WORLD_STORAGE_OK ? increment(&c->inventory.entity_occurrences, error)
                                         : status;
}

static mc_world_storage_status collect_path(terminal_collector *c, const void *slot, mc_path *path,
                                            mc_world_storage_error *error) {
    mc_world_storage_status status = owning_slot(c, slot, error);
    if (status != MC_WORLD_STORAGE_OK || path == NULL)
        return status;
    terminal_record *entry;
    bool created;
    status = record(c, path, TERMINAL_PATH, &entry, &created, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    if (created) {
        mc_path_state state;
        if (!mc_path_inspect(path, &state) || state.classes != c->world->classes->entities ||
            state.arena_identity == NULL || state.arena_holds == 0)
            return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                          "World terminal canonical Path arena owner");
        entry->holds = state.holds;
        status = increment(&c->inventory.paths, error);
        if (status != MC_WORLD_STORAGE_OK)
            return status;
        terminal_record *arena;
        bool arena_created;
        status =
            record(c, state.arena_identity, TERMINAL_PATH_ARENA, &arena, &arena_created, error);
        if (status != MC_WORLD_STORAGE_OK)
            return status;
        if (arena_created) {
            arena->holds = state.arena_holds;
            status = increment(&c->inventory.path_arenas, error);
            if (status != MC_WORLD_STORAGE_OK)
                return status;
        } else if (arena->holds != state.arena_holds)
            return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                          "World terminal Path arena hold metadata");
        status = increment(&arena->occurrences, error);
        if (status != MC_WORLD_STORAGE_OK)
            return status;
        /* Adding the arena record may have grown the identity table. */
        entry = find(c->records, c->capacity, path, TERMINAL_PATH);
    }
    status = increment(&entry->occurrences, error);
    return status == MC_WORLD_STORAGE_OK ? increment(&c->inventory.path_occurrences, error)
                                         : status;
}

static mc_world_storage_status collect_entity_references(terminal_collector *c,
                                                         mc_world_storage_error *error) {
    /* Appending new identities keeps this traversal linear and avoids native
     * recursion for aliases and cycles. Hash-table growth cannot move queued
     * Entity identities or the actual owning slots supplied to collect_entity. */
    for (size_t i = 0; i < c->entity_count; ++i) {
        mc_entity *entity = c->entities[i];
        if (entity->mob == NULL)
            continue;
        mc_entity *value;
        mc_entity_error child;
        if (mc_mob_look_target_get(entity, &value, &child) != MC_ENTITY_OK)
            return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, child.operation);
        mc_world_storage_status status =
            collect_entity(c, &entity->mob->look_target_hold, value, error);
        if (status != MC_WORLD_STORAGE_OK)
            return status;
        if (mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_SLIME)) {
            if (entity->mob->creature_target != NULL || entity->mob->creature_target_hold != NULL ||
                entity->mob->creature_path != NULL || entity->mob->creature_path_hold != NULL)
                return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                              "World terminal Slime has no Creature reference fields");
            continue;
        }
        if (mc_mob_creature_target_get(entity, &value, &child) != MC_ENTITY_OK)
            return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, child.operation);
        status = collect_entity(c, &entity->mob->creature_target_hold, value, error);
        if (status != MC_WORLD_STORAGE_OK)
            return status;
        mc_path *path;
        mc_path_error path_error;
        if (mc_mob_path_get(entity, &path, &path_error) != MC_ENTITY_OK)
            return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, path_error.operation);
        status = collect_path(c, &entity->mob->creature_path_hold, path, error);
        if (status != MC_WORLD_STORAGE_OK)
            return status;
    }
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status collect_tile(void *context, const void *slot, mc_tile_entity *value,
                                            mc_world_storage_error *error) {
    terminal_collector *c = context;
    mc_world_storage_status status = owning_slot(c, slot, error);
    if (status != MC_WORLD_STORAGE_OK || value == NULL)
        return status;
    terminal_record *entry;
    bool created;
    status = record(c, value, TERMINAL_TILE, &entry, &created, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    if (created) {
        mc_tile_entity_terminal_view view;
        status = mc_tile_entity_terminal_read(value, &view, error);
        if (status != MC_WORLD_STORAGE_OK)
            return status;
        if (view.parent != c->world->classes || view.holds == 0 ||
            (view.world != NULL && view.world != c->world))
            return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                          "World terminal canonical TileEntity owner and World");
        entry->holds = view.holds;
        status = increment(&c->inventory.tile_entities, error);
        if (status != MC_WORLD_STORAGE_OK)
            return status;
        if (view.world != NULL) {
            status = increment(&c->inventory.expected_world_bindings, error);
            if (status != MC_WORLD_STORAGE_OK)
                return status;
        }
    }
    status = increment(&entry->occurrences, error);
    return status == MC_WORLD_STORAGE_OK ? increment(&c->inventory.tile_entity_occurrences, error)
                                         : status;
}

static mc_world_storage_status collect_chunk(void *context, const mc_world_chunk *chunk,
                                             mc_world_storage_error *error) {
    terminal_collector *c = context;
    terminal_record *entry;
    bool created;
    mc_world_storage_status status = record(c, chunk, TERMINAL_CHUNK, &entry, &created, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    if (!created)
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "World terminal repeated Store Chunk owner");
    status = mc_world_chunk_terminal_visit_entities(chunk, collect_entity, c, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    return mc_live_tile_entities_terminal_visit_owned_map(chunk, collect_tile, c, error);
}

static mc_world_storage_status collect_world_lists(terminal_collector *c,
                                                   mc_world_storage_error *error) {
    const mc_world_entity_list_ledger *lists[] = {
        &c->world->entity_membership, &c->world->entity_deferral, &c->world->player_membership,
        &c->world->weather_membership, &c->world->nearby_entities};
    for (size_t list = 0; list < sizeof(lists) / sizeof(lists[0]); ++list)
        for (size_t i = 0; i < lists[list]->count; ++i) {
            const mc_world_storage_status status =
                collect_entity(c, &lists[list]->entries[i], lists[list]->entries[i], error);
            if (status != MC_WORLD_STORAGE_OK)
                return status;
        }
    const mc_world_collision_list_ledger *boxes = &c->world->collision_boxes;
    for (size_t i = 0; i < boxes->count; ++i)
        if (boxes->entries[i].entity != NULL) {
            const mc_world_storage_status status =
                collect_entity(c, &boxes->entries[i].entity, boxes->entries[i].entity, error);
            if (status != MC_WORLD_STORAGE_OK)
                return status;
        }
    return mc_live_tile_entities_terminal_visit_lists(c->world, collect_tile, c, error);
}

static mc_world_storage_status collect_anchors(terminal_collector *c,
                                               const mc_world_terminal_anchors *anchors,
                                               mc_world_storage_error *error) {
    if (anchors == NULL)
        return MC_WORLD_STORAGE_OK;
    if ((anchors->entity_count != 0 && anchors->entities == NULL) ||
        (anchors->world_count != 0 && anchors->worlds == NULL))
        return result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                      "World terminal typed anchor arrays");
    if (anchors->entity_count > SIZE_MAX / sizeof(*anchors->entities) ||
        anchors->world_count > SIZE_MAX / sizeof(*anchors->worlds))
        return result(error, MC_WORLD_STORAGE_LIMIT, "World terminal anchor span overflow");
    for (size_t i = 0; i < anchors->entity_count; ++i) {
        mc_entity *const *slot = anchors->entities[i].slot;
        if (slot == NULL)
            return result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                          "World terminal typed Entity anchor slot");
        const mc_world_storage_status status = collect_entity(c, slot, *slot, error);
        if (status != MC_WORLD_STORAGE_OK)
            return status;
    }
    for (size_t i = 0; i < anchors->world_count; ++i) {
        mc_world_runtime *const *slot = anchors->worlds[i].slot;
        if (slot == NULL)
            return result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                          "World terminal typed World anchor slot");
        mc_world_storage_status status = owning_slot(c, slot, error);
        if (status != MC_WORLD_STORAGE_OK)
            return status;
        if (*slot != NULL) {
            if (*slot != c->world)
                return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                              "World terminal anchor outside admitted World");
            status = increment(&c->inventory.expected_world_bindings, error);
            if (status != MC_WORLD_STORAGE_OK)
                return status;
        }
    }
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status scope(terminal_collector *c, mc_world_storage_error *error) {
    mc_world_runtime *world = c->world;
    if (world->construction.mode != MC_WORLD_RUNTIME_GAME ||
        world->construction.phase != MC_WORLD_RUNTIME_READY || world->entity_containers_retired ||
        world->tile_entity_containers_retired || world->classes == NULL ||
        world->classes->side != MC_WORLD_INFO_CLIENT || world->classes->live_worlds == 0 ||
        world->classes->registry == NULL || world->classes->entities == NULL ||
        world->classes->profile == NULL || world->classes->entities->source != MC_ENTITY_CLIENT ||
        world->classes->entities->profile != world->classes->profile || world->fields.w == NULL)
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "World terminal READY CLIENT GAME namespace scope");
    mc_world_storage_status status = mc_world_store_terminal_validate(
        world->fields.w, world, world->options.cache, &world->acquisition.fallback, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    status = mc_world_runtime_entity_containers_validate(world, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    if (world->world_accesses.count != 0)
        return result(error, MC_WORLD_STORAGE_BUSY,
                      "World terminal WorldAccess registrations remain");
    if (world->dedicated.G != NULL)
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "World terminal dedicated Entity index outside first scope");
    if (world->fields.t == NULL || mc_world_provider_world(world->fields.t) != world)
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "World terminal canonical bound WorldProvider");
    const size_t holds = mc_world_provider_hold_count(world->fields.t);
    if (holds == 0)
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "World terminal WorldProvider ownership metadata");
    if (holds != 1)
        return result(error, MC_WORLD_STORAGE_BUSY, "World terminal external WorldProvider hold");
    c->inventory.expected_world_bindings = 1; /* Actual sole provider.a occurrence. */
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status audit(terminal_collector *c,
                                     const mc_world_terminal_anchors *anchors,
                                     mc_world_storage_error *error) {
    mc_world_storage_status status = scope(c, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    status = collect_world_lists(c, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    status =
        mc_world_store_terminal_visit_chunks(c->world->fields.w, c->world, collect_chunk, c, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    status = collect_anchors(c, anchors, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    const size_t before_piston = c->inventory.entity_occurrences;
    mc_tile_entity_terminal_class_inventory classes = {0};
    status = mc_tile_entity_piston_terminal_visit_entities(c->world->classes, collect_entity, c,
                                                           &classes, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    if (c->inventory.entity_occurrences - before_piston != classes.piston_occurrences)
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "World terminal Piston occurrence metadata");
    if (classes.live_tiles < c->inventory.tile_entities)
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "World terminal TileEntity class ownership metadata");
    if (classes.piston_occurrences != 0) {
        if (c->world->classes->live_worlds != 1)
            return result(error, MC_WORLD_STORAGE_BUSY,
                          "World terminal Piston proof requires the sole live World");
        if (classes.live_tiles != c->inventory.tile_entities)
            return result(error, MC_WORLD_STORAGE_BUSY,
                          "World terminal unaccounted class TileEntity owner");
        c->inventory.piston_discard_required = true;
    }
    status = collect_entity_references(c, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    for (size_t i = 0; i < c->capacity; ++i) {
        const terminal_record *entry = &c->records[i];
        if (entry->identity == NULL ||
            (entry->kind != TERMINAL_ENTITY && entry->kind != TERMINAL_TILE &&
             entry->kind != TERMINAL_PATH && entry->kind != TERMINAL_PATH_ARENA))
            continue;
        if (entry->holds < entry->occurrences)
            return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                          "World terminal owned occurrences exceed actual holds");
        if (entry->holds != entry->occurrences)
            return result(error, MC_WORLD_STORAGE_BUSY,
                          entry->kind == TERMINAL_PATH ? "World terminal external Path hold"
                          : entry->kind == TERMINAL_PATH_ARENA
                              ? "World terminal external Path arena hold"
                              : "World terminal external Entity or TileEntity hold");
    }
    if (c->world->persistent_bindings < c->inventory.expected_world_bindings)
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "World terminal bindings below exact owned inventory");
    if (c->world->persistent_bindings != c->inventory.expected_world_bindings)
        return result(error, MC_WORLD_STORAGE_BUSY,
                      "World terminal unexplained direct World binding");
    return result(error, MC_WORLD_STORAGE_OK, NULL);
}

mc_world_storage_status mc_world_runtime_terminal_admit_owned(
    mc_world_runtime *world, const mc_world_terminal_anchors *anchors,
    const mc_world_terminal_limits *limits, mc_world_terminal_inventory *out,
    mc_world_storage_error *error) {
    mc_world_storage_error child = {0};
    mc_world_storage_status status;
    if (world == NULL || out == NULL)
        status = result(&child, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                        "World terminal owner and inventory output required");
    else {
        terminal_collector collector = {
            .world = world,
            .limits = limits == NULL ? mc_world_runtime_terminal_default_limits() : *limits};
        status = audit(&collector, anchors, &child);
        if (status == MC_WORLD_STORAGE_OK)
            *out = collector.inventory;
        free(collector.records);
        free(collector.entities);
    }
    if (error != NULL)
        *error = child;
    return status;
}

mc_world_storage_status mc_world_runtime_terminal_admit(mc_world_runtime *world,
                                                        const mc_world_terminal_limits *limits,
                                                        mc_world_terminal_inventory *out,
                                                        mc_world_storage_error *error) {
    return mc_world_runtime_terminal_admit_owned(world, NULL, limits, out, error);
}

struct mc_world_terminal_mob_graph {
    terminal_collector collector;
};

typedef struct mob_graph_scan {
    bool needed;
} mob_graph_scan;

static mc_world_storage_status scan_mob(void *context, const void *slot, mc_entity *entity,
                                        mc_world_storage_error *error) {
    (void)slot;
    (void)error;
    mob_graph_scan *scan = context;
    if (entity != NULL && entity->mob != NULL &&
        (entity->mob->tick_active || entity->mob->pending_ray != NULL ||
         entity->mob->pending_path != NULL || entity->mob->living.private_b != NULL ||
         entity->mob->creature_target != NULL || entity->mob->look_target_hold != NULL ||
         entity->mob->creature_target_hold != NULL))
        scan->needed = true;
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status scan_mob_chunk(void *context, const mc_world_chunk *chunk,
                                              mc_world_storage_error *error) {
    return mc_world_chunk_terminal_visit_entities(chunk, scan_mob, context, error);
}

static mc_world_storage_status mob_graph_needed(mc_world_runtime *world, bool *out,
                                                mc_world_storage_error *error) {
    mob_graph_scan scan = {0};
    const mc_world_entity_list_ledger *lists[] = {
        &world->entity_membership, &world->entity_deferral, &world->player_membership,
        &world->weather_membership, &world->nearby_entities};
    for (size_t list = 0; list < sizeof(lists) / sizeof(lists[0]); ++list)
        for (size_t i = 0; i < lists[list]->count; ++i)
            (void)scan_mob(&scan, &lists[list]->entries[i], lists[list]->entries[i], NULL);
    for (size_t i = 0; i < world->collision_boxes.count; ++i)
        (void)scan_mob(&scan, &world->collision_boxes.entries[i].entity,
                       world->collision_boxes.entries[i].entity, NULL);
    if (world->fields.w != NULL) {
        const mc_world_storage_status status = mc_world_store_entity_containers_visit_chunks(
            world->fields.w, world, scan_mob_chunk, &scan, error);
        if (status != MC_WORLD_STORAGE_OK)
            return status;
    }
    mc_tile_entity_terminal_class_inventory classes = {0};
    if (world->classes != NULL) {
        const mc_world_storage_status status = mc_tile_entity_piston_terminal_visit_entities(
            world->classes, scan_mob, &scan, &classes, error);
        if (status != MC_WORLD_STORAGE_OK)
            return status;
    }
    *out = scan.needed;
    return MC_WORLD_STORAGE_OK;
}

mc_world_storage_status mc_world_runtime_terminal_mob_graph_prepare(
    mc_world_runtime *world, mc_world_terminal_mob_graph **out, mc_world_storage_error *error) {
    if (world == NULL || out == NULL || *out != NULL)
        return result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "World terminal Mob graph output");
    bool needed;
    mc_world_storage_status status = mob_graph_needed(world, &needed, error);
    if (status != MC_WORLD_STORAGE_OK || !needed)
        return status;
    if (world->classes == NULL)
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "World terminal Mob graph class namespace");
    mc_world_terminal_mob_graph *graph = calloc(1, sizeof(*graph));
    if (graph == NULL)
        return result(error, MC_WORLD_STORAGE_ALLOCATION_FAILED, "World terminal Mob graph");
    terminal_collector *c = &graph->collector;
    c->world = world;
    c->limits = mc_world_runtime_terminal_default_limits();
    status = collect_world_lists(c, error);
    if (status == MC_WORLD_STORAGE_OK && world->fields.w != NULL)
        status =
            mc_world_store_terminal_visit_chunks(world->fields.w, world, collect_chunk, c, error);
    mc_tile_entity_terminal_class_inventory classes = {0};
    if (status == MC_WORLD_STORAGE_OK)
        status = mc_tile_entity_piston_terminal_visit_entities(world->classes, collect_entity, c,
                                                               &classes, error);
    if (status == MC_WORLD_STORAGE_OK && classes.piston_occurrences != 0 &&
        world->classes->live_worlds != 1)
        status = result(error, MC_WORLD_STORAGE_BUSY,
                        "World terminal Piston Mob graph requires the sole live World");
    if (status == MC_WORLD_STORAGE_OK && classes.piston_occurrences != 0 &&
        classes.live_tiles != c->inventory.tile_entities)
        status = result(error, MC_WORLD_STORAGE_BUSY,
                        "World terminal unaccounted Piston Mob graph TileEntity owner");
    if (status == MC_WORLD_STORAGE_OK && classes.piston_occurrences != 0) {
        for (size_t i = 0; i < c->capacity; ++i) {
            const terminal_record *entry = &c->records[i];
            if (entry->identity == NULL || entry->kind != TERMINAL_TILE)
                continue;
            if (entry->holds < entry->occurrences) {
                status = result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                "World terminal Piston Mob graph TileEntity occurrence holds");
                break;
            }
            if (entry->holds != entry->occurrences) {
                status = result(error, MC_WORLD_STORAGE_BUSY,
                                "World terminal external Piston Mob graph TileEntity hold");
                break;
            }
        }
    }
    if (status == MC_WORLD_STORAGE_OK)
        status = collect_entity_references(c, error);
    for (size_t i = 0; status == MC_WORLD_STORAGE_OK && i < c->capacity; ++i) {
        const terminal_record *entry = &c->records[i];
        if (entry->identity != NULL &&
            (entry->kind == TERMINAL_PATH || entry->kind == TERMINAL_PATH_ARENA) &&
            entry->holds < entry->occurrences)
            status = result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                            "World terminal Path holds below owned occurrences");
    }
    for (size_t i = 0; status == MC_WORLD_STORAGE_OK && i < c->entity_count; ++i) {
        mc_entity *entity = c->entities[i];
        const terminal_record *entry = find(c->records, c->capacity, entity, TERMINAL_ENTITY);
        if (entry->holds < entry->occurrences)
            status = result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                            "World terminal Entity reference holds below occurrences");
        /* A surviving external owner may still inspect its source targets.
         * Never clear those fields to make shutdown pass. Ordinary externally
         * held leaf Entities retain the older container-retirement contract. */
        else if (entry->holds != entry->occurrences && entity->mob != NULL &&
                 (entity->mob->look_target_hold != NULL ||
                  entity->mob->creature_target_hold != NULL))
            status = result(error, MC_WORLD_STORAGE_BUSY,
                            "World terminal external owner of Mob reference fields");
        else if (entity->holds == SIZE_MAX)
            status = result(error, MC_WORLD_STORAGE_LIMIT, "World terminal Mob graph pin ceiling");
    }
    if (status != MC_WORLD_STORAGE_OK) {
        free(c->records);
        free(c->entities);
        free(graph);
        return status;
    }
    /* All source/native validation has completed. These temporary occurrences
     * keep target-only nodes and cycles alive until the caller's joint commit
     * has consumed every source reference field and container root. */
    for (size_t i = 0; i < c->entity_count; ++i)
        (void)mc_entity_retain(c->entities[i]);
    *out = graph;
    return result(error, MC_WORLD_STORAGE_OK, NULL);
}

void mc_world_runtime_terminal_mob_graph_clear(mc_world_terminal_mob_graph *graph) {
    if (graph == NULL)
        return;
    for (size_t i = 0; i < graph->collector.entity_count; ++i)
        if (graph->collector.entities[i]->mob != NULL)
            mc_mob_references_dispose(graph->collector.entities[i]);
}

void mc_world_runtime_terminal_mob_graph_release(mc_world_terminal_mob_graph **slot) {
    mc_world_terminal_mob_graph *graph = *slot;
    if (graph == NULL)
        return;
    for (size_t i = 0; i < graph->collector.entity_count; ++i)
        mc_entity_release(graph->collector.entities[i]);
    free(graph->collector.records);
    free(graph->collector.entities);
    free(graph);
    *slot = NULL;
}
