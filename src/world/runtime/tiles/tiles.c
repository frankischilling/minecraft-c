#include "mc/world/runtime/tiles/tiles.h"

#include "mc/storage/nbt/tree.h"

#include "../../../block/registry/registry_internal.h"
#include "../../../block/registry/runtime_internal.h"
#include "../../../platform/text/stdout.h"
#include "../../chunk/object_state_internal.h"
#include "../../storage/objects_internal.h"
#include "../construction_internal.h"
#include "../mutation/mutation_internal.h"
#include "../provider_internal.h"
#include "../runtime_internal.h"
#include "mc/world/runtime/blocks.h"
#include "mc/world/runtime/entities.h"

#include "mc/core/java/java.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#if !defined(__SSE2__) && !defined(_M_X64) && !(defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#error "Live Tile drop arithmetic requires SSE2"
#endif
#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) ||       \
    (defined(_M_FP_FAST) && _M_FP_FAST)
#error "Live Tile drop arithmetic requires strict floating transformations"
#endif
#include <emmintrin.h>

typedef enum live_class_id {
    LIVE_FURNACE,
    LIVE_CHEST,
    LIVE_RECORD,
    LIVE_TRAP,
    LIVE_SIGN,
    LIVE_SPAWNER,
    LIVE_MUSIC,
    LIVE_PISTON,
    LIVE_CLASS_COUNT
} live_class_id;

typedef struct live_class_entry {
    live_class_id kind;
    mc_tile_name *name;
} live_class_entry;

struct mc_tile_classes {
    mc_world_runtime_classes *parent;
    mc_registry_classes *registry;
    unsigned int phase, initializer_pc;
    mc_tile_status failure;
    mc_registry_error first_registry_failure;
    live_class_entry *by_name, *by_class;
    size_t name_count, class_count, live_tiles, allocation_bytes;
    mc_tile_name *literals;
    bool stdout_error;
};

struct tile_import_context;
static mc_registry_error live_empty_registry(void);
static mc_live_tile_error live_empty_error(void);
static mc_world_storage_status live_tile_error(mc_live_tile_error *, mc_tile_status, const char *);
static mc_tile_status live_classes_get(mc_world_runtime *world, mc_tile_classes **out);
static mc_tile_status live_literal(mc_tile_classes *classes, const char *ascii, mc_tile_name **out,
                                   mc_registry_error *attempt, const mc_tile *scope_tile,
                                   const struct tile_import_context *context);
static mc_tile_name *live_class_name(mc_tile_classes *classes, mc_tile_kind kind);
static bool live_graph_measure(const mc_world_chunk *chunk, mc_tile *extra, size_t ceiling,
                               size_t *out, mc_live_tile_error *error);

_Static_assert(sizeof(double) == sizeof(uint64_t), "Tile rotation snapshots require 64-bit double");

/* Concrete Tile/Stack/name layouts. Runtime and persistence joins require
 * the coordinated actual owners; no stand-in success is provided. */
/* Older MSVC diagnoses this standard C17 flexible array as extension C4200.
 * Keep the allocation layout and scope the compatibility guard to this type. */
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4200)
#endif
struct mc_tile_name {
    size_t holds;
    size_t allocation_bytes;
    bool immortal;
    mc_nbt_string text;
    const mc_registry_string *literal_identity;
    struct mc_tile_name *literal_next;
    uint16_t units[];
};
#ifdef _MSC_VER
#pragma warning(pop)
#endif

struct mc_tile {
    size_t holds;
    mc_tile_kind kind;
    mc_tile_phase phase;
    mc_tile_classes *classes;
    mc_world_runtime *world;
    int32_t x, y, z;
    bool invalid;
    mc_nbt_tree *source;
    union {
        struct {
            mc_item_stack **slots;
            size_t capacity;
        } chest;
        struct {
            int32_t delay;
            double rotation, previous_rotation;
            mc_tile_name *name;
        } spawner;
    } state;
};

typedef struct tile_loaded_record {
    mc_item_stack *stack;
    const mc_nbt_tag *record;
    size_t index;
} tile_loaded_record;

typedef struct tile_import_context {
    mc_nbt_limits limits;
    tile_loaded_record winners[27];
    /* When importing into a held Chunk, each allocation sees the complete
     * current Map plus the unpublished held Tile at the SAME full ceiling. */
    const mc_world_chunk *scope_chunk;
} tile_import_context;

static mc_tile_status tile_import_measure(const mc_tile *tile, const tile_import_context *context,
                                          size_t *out);
static bool live_graph_has_name(const mc_world_chunk *chunk, const mc_tile *extra,
                                const mc_tile_name *name);

static mc_registry_error live_empty_registry(void)
{
    return mc_tile_empty_error().registry;
}

static mc_live_tile_error live_empty_error(void)
{
    return mc_live_tiles_empty_error();
}

static mc_tile_status tile_inventory_result(mc_live_tile_error *error, mc_tile_status status,
                                            unsigned int pc)
{
    if (status != MC_TILE_OK && error != NULL) {
        if (error->status == MC_WORLD_STORAGE_OK)
            live_tile_error(error, status, "Chest native owner/slot/Stack operation");
        if (error->inventory_bytecode == 0)
            error->inventory_bytecode = pc;
    }
    return status;
}

static mc_tile_status tile_import_admit(const mc_tile *tile, const tile_import_context *context,
                                        size_t additional, mc_tile_error *error, const char *field,
                                        size_t index);

static const uint16_t tile_x_name[] = {'x'};
static const uint16_t tile_y_name[] = {'y'};
static const uint16_t tile_z_name[] = {'z'};
static const uint16_t tile_items_name[] = {'I', 't', 'e', 'm', 's'};
static const uint16_t tile_slot_name[] = {'S', 'l', 'o', 't'};
static const uint16_t tile_entity_name[] = {'E', 'n', 't', 'i', 't', 'y', 'I', 'd'};
static const uint16_t tile_delay_name[] = {'D', 'e', 'l', 'a', 'y'};
static const uint16_t tile_id_name[] = {'i', 'd'};

static mc_tile_status tile_status(mc_tile_error *error, mc_tile_status status, const char *field,
                                  size_t index)
{
    if (error != NULL) {
        *error = (mc_tile_error){
            .status = status,
            .field = field,
            .item_index = index,
            .item = {MC_ITEM_STACK_OK, NULL, {MC_NBT_OK, 0}},
            .nbt = {MC_NBT_OK, 0},
            .registry = live_empty_registry(),
        };
    }
    return status;
}

static mc_tile_status tile_item_error(mc_tile_error *error, mc_item_stack_status status,
                                      const mc_item_stack_error *item, size_t index)
{
    const mc_tile_status result = status == MC_ITEM_STACK_ALLOCATION_FAILED
                                      ? MC_TILE_ALLOCATION_FAILED
                                  : status == MC_ITEM_STACK_LIMIT         ? MC_TILE_LIMIT
                                  : status == MC_ITEM_STACK_INVALID_FIELD ? MC_TILE_INVALID_FIELD
                                                                          : MC_TILE_ITEM_FAILED;
    if (error != NULL) {
        *error = (mc_tile_error){
            .status = result,
            .field = item == NULL ? NULL : item->field,
            .item_index = index,
            .item = item == NULL ? (mc_item_stack_error){status, NULL, {MC_NBT_OK, 0}} : *item,
            .nbt = {MC_NBT_OK, 0},
            .registry = live_empty_registry(),
        };
    }
    return result;
}

static mc_tile_status tile_nbt_error(mc_tile_error *error, mc_nbt_status status, mc_nbt_error nbt,
                                     const char *field, size_t index)
{
    const mc_tile_status result = status == MC_NBT_LIMIT               ? MC_TILE_LIMIT
                                  : status == MC_NBT_ALLOCATION_FAILED ? MC_TILE_ALLOCATION_FAILED
                                                                       : MC_TILE_NBT_ERROR;
    tile_status(error, result, field, index);
    if (error != NULL) {
        error->nbt = nbt;
    }
    return result;
}

static mc_tile_status tile_ready(const mc_tile *tile)
{
    if (tile == NULL) {
        return MC_TILE_INVALID_ARGUMENT;
    }
    return tile->phase == MC_TILE_READY ? MC_TILE_OK : MC_TILE_INVALID_PHASE;
}

static mc_tile_status tile_kind_ready(const mc_tile *tile, mc_tile_kind kind)
{
    const mc_tile_status status = tile_ready(tile);
    if (status != MC_TILE_OK) {
        return status;
    }
    return tile->kind == kind ? MC_TILE_OK : MC_TILE_INVALID_KIND;
}

static mc_tile_status tile_chest_slot(const mc_tile *tile, int32_t slot)
{
    const mc_tile_status status = tile_kind_ready(tile, MC_TILE_CHEST);
    if (status != MC_TILE_OK) {
        return status;
    }
    return slot < 0 || (uint32_t)slot >= tile->state.chest.capacity ? MC_TILE_INVALID_SLOT
                                                                    : MC_TILE_OK;
}

static mc_tile_status tile_item_status(mc_item_stack_status status)
{
    if (status == MC_ITEM_STACK_OK) {
        return MC_TILE_OK;
    }
    if (status == MC_ITEM_STACK_ALLOCATION_FAILED) {
        return MC_TILE_ALLOCATION_FAILED;
    }
    if (status == MC_ITEM_STACK_LIMIT) {
        return MC_TILE_LIMIT;
    }
    return MC_TILE_ITEM_FAILED;
}

mc_tile_status mc_tile_name_create(const uint16_t *units, size_t length, mc_tile_name **out)
{
    if (out == NULL || (units == NULL && length != 0)) {
        return MC_TILE_INVALID_ARGUMENT;
    }
    if (length > (SIZE_MAX - sizeof(mc_tile_name)) / sizeof(uint16_t)) {
        return MC_TILE_LIMIT;
    }
    const size_t bytes = sizeof(mc_tile_name) + length * sizeof(uint16_t);
    mc_tile_name *name = malloc(bytes);
    if (name == NULL) {
        return MC_TILE_ALLOCATION_FAILED;
    }
    name->holds = 1;
    name->allocation_bytes = bytes;
    name->immortal = false;
    name->literal_identity = NULL;
    name->literal_next = NULL;
    if (length != 0) {
        memcpy(name->units, units, length * sizeof(uint16_t));
    }
    name->text = (mc_nbt_string){length == 0 ? NULL : name->units, length};
    *out = name;
    return MC_TILE_OK;
}

mc_tile_status mc_tile_name_retain(mc_tile_name *name)
{
    if (name == NULL || name->immortal) {
        return MC_TILE_OK;
    }
    if (name->holds == SIZE_MAX) {
        return MC_TILE_LIMIT;
    }
    ++name->holds;
    return MC_TILE_OK;
}

void mc_tile_name_release(mc_tile_name *name)
{
    if (name != NULL && !name->immortal && --name->holds == 0) {
        free(name);
    }
}

mc_tile_status mc_tile_name_get_view(const mc_tile_name *name, mc_tile_name_view *out)
{
    if (out == NULL) {
        return MC_TILE_INVALID_ARGUMENT;
    }
    *out = name == NULL ? (mc_tile_name_view){true, {NULL, 0}}
                        : (mc_tile_name_view){false, name->text};
    return MC_TILE_OK;
}

size_t mc_tile_name_native_bytes(const mc_tile_name *name)
{
    return name == NULL ? 0 : name->allocation_bytes;
}

mc_tile_status mc_tile_allocate(mc_world_runtime *world, mc_tile_kind kind, mc_tile **out)
{
    if (out == NULL) {
        return MC_TILE_INVALID_ARGUMENT;
    }
    if (kind != MC_TILE_CHEST && kind != MC_TILE_SPAWNER) {
        return MC_TILE_INVALID_KIND;
    }
    mc_tile_classes *classes = NULL;
    mc_tile_status active = live_classes_get(world, &classes);
    if (active != MC_TILE_OK)
        return active;
    if (classes->live_tiles == SIZE_MAX)
        return MC_TILE_LIMIT;
    mc_tile *tile = calloc(1, sizeof(*tile));
    if (tile == NULL) {
        return MC_TILE_ALLOCATION_FAILED;
    }
    tile->classes = classes;
    ++classes->live_tiles;
    tile->holds = 1;
    tile->kind = kind;
    tile->phase = MC_TILE_RAW;
    *out = tile;
    return MC_TILE_OK;
}

static mc_tile_status tile_construct(mc_tile *raw, const tile_import_context *context,
                                     mc_tile_error *error)
{
    if (raw == NULL) {
        return tile_status(error, MC_TILE_INVALID_ARGUMENT, NULL, SIZE_MAX);
    }
    if (raw->phase != MC_TILE_RAW) {
        return tile_status(error, MC_TILE_INVALID_PHASE, NULL, SIZE_MAX);
    }
    raw->phase = MC_TILE_FAILED_CONSTRUCTION;
    if (raw->kind == MC_TILE_CHEST) {
        const mc_tile_status admitted =
            tile_import_admit(raw, context, 36u * sizeof(mc_item_stack *), error, NULL, SIZE_MAX);
        if (admitted != MC_TILE_OK) {
            return admitted;
        }
        mc_item_stack **slots = calloc(36u, sizeof(*slots));
        if (slots == NULL) {
            return tile_status(error, MC_TILE_ALLOCATION_FAILED, NULL, SIZE_MAX);
        }
        raw->state.chest.slots = slots;
        raw->state.chest.capacity = 36;
    } else {
        raw->state.spawner.delay = -1;
        raw->state.spawner.previous_rotation = 0.0;
        mc_registry_error attempt = live_empty_registry();
        mc_tile_status named =
            live_literal(raw->classes, "Pig", &raw->state.spawner.name, &attempt, raw, context);
        if (named != MC_TILE_OK) {
            tile_status(error, named, "EntityId", SIZE_MAX);
            if (error != NULL) {
                error->registry = attempt;
                error->constructor_bytecode = 15;
            }
            return named;
        }
        raw->state.spawner.delay = 20;
    }
    raw->phase = MC_TILE_READY;
    return tile_status(error, MC_TILE_OK, NULL, SIZE_MAX);
}

mc_tile_status mc_tile_construct(mc_tile *raw)
{
    return tile_construct(raw, NULL, NULL);
}

mc_tile_status mc_tile_construct_detailed(mc_tile *raw, mc_tile_error *error)
{
    return tile_construct(raw, NULL, error);
}

mc_tile_status mc_tile_retain(mc_tile *tile)
{
    if (tile == NULL) {
        return MC_TILE_INVALID_ARGUMENT;
    }
    if (tile->holds == SIZE_MAX) {
        return MC_TILE_LIMIT;
    }
    ++tile->holds;
    return MC_TILE_OK;
}

static void tile_slots_release(mc_item_stack **slots, size_t capacity)
{
    for (size_t i = 0; i < capacity; ++i) {
        mc_item_stack_release(slots[i]);
    }
    free(slots);
}

void mc_tile_release(mc_tile *tile)
{
    if (tile == NULL || --tile->holds != 0) {
        return;
    }
    if (tile->kind == MC_TILE_CHEST) {
        tile_slots_release(tile->state.chest.slots, tile->state.chest.capacity);
    } else {
        mc_tile_name_release(tile->state.spawner.name);
    }
    mc_nbt_tree_free(tile->source);
    if (tile->world != NULL) {
        mc_world_runtime_binding_release(tile->world);
    }
    --tile->classes->live_tiles;
    free(tile);
}

mc_tile_status mc_tile_get_snapshot(const mc_tile *tile, mc_tile_snapshot *out)
{
    if (tile == NULL || out == NULL) {
        return MC_TILE_INVALID_ARGUMENT;
    }
    mc_tile_snapshot snapshot = {0};
    snapshot.kind = tile->kind;
    snapshot.phase = tile->phase;
    snapshot.world = tile->world;
    snapshot.x = tile->x;
    snapshot.y = tile->y;
    snapshot.z = tile->z;
    snapshot.invalid = tile->invalid;
    if (tile->kind == MC_TILE_CHEST) {
        snapshot.chest_capacity = tile->state.chest.capacity;
    } else {
        snapshot.spawner_delay = tile->state.spawner.delay;
        memcpy(&snapshot.spawner_rotation_bits, &tile->state.spawner.rotation,
               sizeof(snapshot.spawner_rotation_bits));
        memcpy(&snapshot.spawner_previous_rotation_bits, &tile->state.spawner.previous_rotation,
               sizeof(snapshot.spawner_previous_rotation_bits));
        snapshot.spawner_name = tile->state.spawner.name;
    }
    *out = snapshot;
    return MC_TILE_OK;
}

size_t mc_tile_native_bytes(const mc_tile *tile)
{
    if (tile == NULL) {
        return 0;
    }
    return sizeof(*tile) +
           (tile->kind == MC_TILE_CHEST ? tile->state.chest.capacity * sizeof(mc_item_stack *) : 0);
}

const mc_nbt_tag *mc_tile_source(const mc_tile *tile)
{
    return tile == NULL ? NULL : mc_nbt_tree_root(tile->source);
}

size_t mc_tile_source_bytes(const mc_tile *tile)
{
    return tile == NULL ? 0 : mc_nbt_tree_allocation_bytes(tile->source);
}

static bool tile_size_add(size_t *total, size_t amount)
{
    if (amount > SIZE_MAX - *total) {
        return false;
    }
    *total += amount;
    return true;
}

static bool tile_size_multiply(size_t left, size_t right, size_t *out)
{
    if (right != 0 && left > SIZE_MAX / right) {
        return false;
    }
    *out = left * right;
    return true;
}

mc_tile_status mc_tile_storage_bytes(const mc_tile *tile, size_t *out)
{
    if (tile == NULL || out == NULL) {
        return MC_TILE_INVALID_ARGUMENT;
    }
    size_t bytes = sizeof(*tile);
    if (!tile_size_add(&bytes, mc_nbt_tree_allocation_bytes(tile->source))) {
        return MC_TILE_LIMIT;
    }
    if (tile->kind == MC_TILE_CHEST) {
        const size_t capacity = tile->state.chest.capacity;
        if ((capacity != 0 && capacity != 27 && capacity != 36) ||
            (tile->state.chest.slots == NULL) != (capacity == 0) ||
            (tile->phase == MC_TILE_READY && capacity == 0)) {
            return MC_TILE_STATE_ERROR;
        }
        /* Capacity is one of 0/27/36, proving this product is bounded. */
        if (!tile_size_add(&bytes, capacity * sizeof(mc_item_stack *))) {
            return MC_TILE_LIMIT;
        }
        for (size_t i = 0; i < capacity; ++i) {
            const mc_item_stack *stack = tile->state.chest.slots[i];
            if (stack == NULL) {
                continue;
            }
            bool seen = false;
            for (size_t j = 0; j < i; ++j) {
                if (tile->state.chest.slots[j] == stack) {
                    seen = true;
                    break;
                }
            }
            if (seen) {
                continue;
            }
            size_t item_bytes;
            const mc_item_stack_status item = mc_item_stack_native_bytes(stack, &item_bytes);
            if (item != MC_ITEM_STACK_OK) {
                return tile_item_status(item);
            }
            if (!tile_size_add(&bytes, item_bytes)) {
                return MC_TILE_LIMIT;
            }
        }
    } else if (tile->kind == MC_TILE_SPAWNER) {
        if (!tile_size_add(&bytes, mc_tile_name_native_bytes(tile->state.spawner.name))) {
            return MC_TILE_LIMIT;
        }
    } else {
        return MC_TILE_STATE_ERROR;
    }
    *out = bytes;
    return MC_TILE_OK;
}

static bool tile_limits(const mc_nbt_limits *requested, mc_nbt_limits *out)
{
    *out = requested == NULL ? mc_nbt_default_limits() : *requested;
    return out->max_depth != 0 && out->max_depth <= 128;
}

static mc_tile_status tile_import_admit(const mc_tile *tile, const tile_import_context *context,
                                        size_t additional, mc_tile_error *error, const char *field,
                                        size_t index)
{
    if (context == NULL) {
        return MC_TILE_OK;
    }
    size_t live;
    const mc_tile_status measured = tile_import_measure(tile, context, &live);
    if (measured != MC_TILE_OK) {
        return tile_status(error, measured, field, index);
    }
    const size_t ceiling = context->limits.max_allocation_bytes;
    if (live > ceiling || additional > ceiling - live) {
        return tile_status(error, MC_TILE_LIMIT, field, index);
    }
    return MC_TILE_OK;
}

mc_tile_status mc_tile_bind_world(mc_tile *tile, mc_world_runtime *world)
{
    const mc_tile_status status = tile_ready(tile);
    if (status != MC_TILE_OK) {
        return status;
    }
    if (world != NULL && world->classes != tile->classes->parent) {
        return MC_TILE_INVALID_ARGUMENT;
    }
    if (tile->world == world) {
        return MC_TILE_OK;
    }
    if (world != NULL && !mc_world_runtime_binding_retain(world)) {
        return MC_TILE_WORLD_FAILED;
    }
    mc_world_runtime *previous = tile->world;
    tile->world = world;
    if (previous != NULL) {
        mc_world_runtime_binding_release(previous);
    }
    return MC_TILE_OK;
}

mc_tile_status mc_tile_set_coordinates(mc_tile *tile, int32_t x, int32_t y, int32_t z)
{
    const mc_tile_status status = tile_ready(tile);
    if (status == MC_TILE_OK) {
        tile->x = x;
        tile->y = y;
        tile->z = z;
    }
    return status;
}

mc_tile_status mc_tile_set_invalid(mc_tile *tile, bool invalid)
{
    const mc_tile_status status = tile_ready(tile);
    if (status == MC_TILE_OK) {
        tile->invalid = invalid;
    }
    return status;
}

mc_tile_status mc_tile_dirty_detailed(mc_tile *tile, mc_live_tile_error *error)
{
    if (error != NULL)
        *error = live_empty_error();
    const mc_tile_status status = tile_ready(tile);
    if (status != MC_TILE_OK) {
        if (error != NULL)
            live_tile_error(error, status, "Tile dirty receiver phase");
        return status;
    }
    mc_world_runtime *world = tile->world;
    if (world == NULL) {
        return MC_TILE_OK;
    }
    const int32_t x = tile->x;
    const int32_t y = tile->y;
    const int32_t z = tile->z;
    const mc_world_storage_status dirty =
        mc_world_runtime_tile_dirty_detailed(world, x, y, z, tile, error);
    if (dirty != MC_WORLD_STORAGE_OK && error != NULL) {
        error->tile_bytecode = 24; /* ow.y_/jh.i -> actual World dirty invocation */
        error->tile.status = MC_TILE_WORLD_FAILED;
    }
    return dirty == MC_WORLD_STORAGE_OK ? MC_TILE_OK : MC_TILE_WORLD_FAILED;
}

mc_tile_status mc_tile_dirty(mc_tile *tile)
{
    return mc_tile_dirty_detailed(tile, NULL);
}

mc_tile_status mc_tile_chest_size(const mc_tile *tile, int32_t *out)
{
    if (out == NULL) {
        return MC_TILE_INVALID_ARGUMENT;
    }
    const mc_tile_status status = tile_kind_ready(tile, MC_TILE_CHEST);
    if (status == MC_TILE_OK) {
        *out = 27;
    }
    return status;
}

mc_tile_status mc_tile_chest_limit(const mc_tile *tile, int32_t *out)
{
    if (out == NULL) {
        return MC_TILE_INVALID_ARGUMENT;
    }
    const mc_tile_status status = tile_kind_ready(tile, MC_TILE_CHEST);
    if (status == MC_TILE_OK) {
        *out = 64;
    }
    return status;
}

mc_tile_status mc_tile_chest_capacity(const mc_tile *tile, size_t *out)
{
    if (out == NULL) {
        return MC_TILE_INVALID_ARGUMENT;
    }
    const mc_tile_status status = tile_kind_ready(tile, MC_TILE_CHEST);
    if (status == MC_TILE_OK) {
        *out = tile->state.chest.capacity;
    }
    return status;
}

mc_tile_status mc_tile_chest_get(mc_tile *tile, int32_t slot, mc_item_stack **out)
{
    if (out == NULL) {
        return MC_TILE_INVALID_ARGUMENT;
    }
    const mc_tile_status status = tile_chest_slot(tile, slot);
    if (status != MC_TILE_OK) {
        return status;
    }
    mc_item_stack *stack = tile->state.chest.slots[(size_t)slot];
    if (stack != NULL) {
        const mc_item_stack_status held = mc_item_stack_retain(stack);
        if (held != MC_ITEM_STACK_OK) {
            return tile_item_status(held);
        }
    }
    *out = stack;
    return MC_TILE_OK;
}

static mc_tile_status tile_inventory_item_result(mc_live_tile_error *error,
                                                 mc_item_stack_status item)
{
    if (error != NULL)
        error->tile.item.status = item;
    return tile_inventory_result(error, tile_item_status(item), 0);
}

mc_tile_status mc_tile_chest_set_detailed(mc_tile *tile, int32_t slot, mc_item_stack *stack,
                                          mc_live_tile_error *error)
{
    if (error != NULL)
        *error = live_empty_error();
    mc_tile_status status = tile_chest_slot(tile, slot);
    if (status != MC_TILE_OK) {
        return tile_inventory_result(error, status, 0);
    }
    if (stack != NULL) {
        mc_item_stack_phase phase;
        mc_item_stack_status item = mc_item_stack_phase_get(stack, &phase);
        if (item != MC_ITEM_STACK_OK || phase != MC_ITEM_STACK_READY) {
            return tile_inventory_result(error, MC_TILE_ITEM_FAILED, 0);
        }
        item = mc_item_stack_retain(stack);
        if (item != MC_ITEM_STACK_OK) {
            return tile_inventory_item_result(error, item);
        }
    }
    if (error != NULL)
        error->inventory_bytecode = 6;
    mc_item_stack *previous = tile->state.chest.slots[(size_t)slot];
    tile->state.chest.slots[(size_t)slot] = stack;
    mc_item_stack_release(previous);
    if (stack != NULL) {
        mc_item_stack_fields fields;
        if (error != NULL)
            error->inventory_bytecode = 12;
        mc_item_stack_status item = mc_item_stack_fields_get(stack, &fields);
        if (item != MC_ITEM_STACK_OK) {
            return tile_inventory_item_result(error, item);
        }
        int32_t limit;
        if (error != NULL)
            error->inventory_bytecode = 16;
        status = mc_tile_chest_limit(tile, &limit);
        if (status != MC_TILE_OK) {
            return tile_inventory_result(error, status, 0);
        }
        if (fields.count > limit) {
            if (error != NULL)
                error->inventory_bytecode = 24;
            status = mc_tile_chest_limit(tile, &limit);
            if (status != MC_TILE_OK) {
                return tile_inventory_result(error, status, 0);
            }
            if (error != NULL)
                error->inventory_bytecode = 27;
            item = mc_item_stack_count_set(stack, limit);
            if (item != MC_ITEM_STACK_OK) {
                return tile_inventory_item_result(error, item);
            }
        }
    }
    status = mc_tile_dirty_detailed(tile, error);
    if (status != MC_TILE_OK && error != NULL)
        error->inventory_bytecode = 31;
    return tile_inventory_result(error, status, 0);
}

mc_tile_status mc_tile_chest_set(mc_tile *tile, int32_t slot, mc_item_stack *stack)
{
    return mc_tile_chest_set_detailed(tile, slot, stack, NULL);
}

mc_tile_status mc_tile_chest_remove_detailed(mc_tile *tile, int32_t slot, int32_t amount,
                                             mc_item_stack **out, mc_live_tile_error *error)
{
    if (error != NULL)
        *error = live_empty_error();
    if (out == NULL) {
        return tile_inventory_result(error, MC_TILE_INVALID_ARGUMENT, 0);
    }
    mc_tile_status status = tile_chest_slot(tile, slot);
    if (status != MC_TILE_OK) {
        return tile_inventory_result(error, status, 0);
    }
    mc_item_stack *stack = tile->state.chest.slots[(size_t)slot];
    if (stack == NULL) {
        *out = NULL;
        return MC_TILE_OK;
    }
    mc_item_stack_fields fields;
    if (error != NULL)
        error->inventory_bytecode = 15;
    mc_item_stack_status item = mc_item_stack_fields_get(stack, &fields);
    if (item != MC_ITEM_STACK_OK) {
        return tile_inventory_item_result(error, item);
    }
    mc_item_stack *removed;
    const unsigned int dirty_pc = fields.count <= amount ? 37u : 73u;
    if (fields.count <= amount) {
        removed = stack;
        /* Transfer this physical slot hold into the private return local. */
        tile->state.chest.slots[(size_t)slot] = NULL;
    } else {
        if (error != NULL)
            error->inventory_bytecode = 49;
        item = mc_item_stack_split(stack, amount, &removed);
        if (item != MC_ITEM_STACK_OK) {
            return tile_inventory_item_result(error, item);
        }
        mc_item_stack *remaining = tile->state.chest.slots[(size_t)slot];
        if (error != NULL)
            error->inventory_bytecode = 59;
        item = mc_item_stack_fields_get(remaining, &fields);
        if (item != MC_ITEM_STACK_OK) {
            mc_item_stack_release(removed);
            return tile_inventory_item_result(error, item);
        }
        if (fields.count == 0) {
            tile->state.chest.slots[(size_t)slot] = NULL;
            mc_item_stack_release(remaining);
        }
    }
    status = mc_tile_dirty_detailed(tile, error);
    if (status != MC_TILE_OK) {
        if (error != NULL)
            error->inventory_bytecode = dirty_pc;
        mc_item_stack_release(removed);
        return tile_inventory_result(error, status, 0);
    }
    *out = removed;
    return MC_TILE_OK;
}

mc_tile_status mc_tile_chest_remove(mc_tile *tile, int32_t slot, int32_t amount,
                                    mc_item_stack **out)
{
    return mc_tile_chest_remove_detailed(tile, slot, amount, out, NULL);
}

mc_tile_status mc_tile_spawner_get_name(mc_tile *tile, mc_tile_name **out)
{
    if (out == NULL) {
        return MC_TILE_INVALID_ARGUMENT;
    }
    const mc_tile_status status = tile_kind_ready(tile, MC_TILE_SPAWNER);
    if (status != MC_TILE_OK) {
        return status;
    }
    mc_tile_name *name = tile->state.spawner.name;
    const mc_tile_status held = mc_tile_name_retain(name);
    if (held != MC_TILE_OK) {
        return held;
    }
    *out = name;
    return MC_TILE_OK;
}

mc_tile_status mc_tile_spawner_set_name(mc_tile *tile, mc_tile_name *name)
{
    const mc_tile_status status = tile_kind_ready(tile, MC_TILE_SPAWNER);
    if (status != MC_TILE_OK) {
        return status;
    }
    const mc_tile_status held = mc_tile_name_retain(name);
    if (held != MC_TILE_OK) {
        return held;
    }
    mc_tile_name *previous = tile->state.spawner.name;
    tile->state.spawner.name = name;
    mc_tile_name_release(previous);
    return MC_TILE_OK;
}

mc_tile_status mc_tile_read_base(mc_tile *tile, const mc_nbt_tag *compound, mc_tile_error *error)
{
    const mc_tile_status status = tile_ready(tile);
    if (status != MC_TILE_OK) {
        return tile_status(error, status, NULL, SIZE_MAX);
    }
    if (compound == NULL || compound->type != MC_NBT_COMPOUND) {
        return tile_status(error, MC_TILE_INVALID_ARGUMENT, "x", SIZE_MAX);
    }
    const mc_nbt_tag *tag = mc_nbt_compound_get(compound, tile_x_name, 1);
    if (tag != NULL && tag->type != MC_NBT_INT) {
        return tile_status(error, MC_TILE_INVALID_FIELD, "x", SIZE_MAX);
    }
    tile->x = tag == NULL ? 0 : tag->value.int_value;
    tag = mc_nbt_compound_get(compound, tile_y_name, 1);
    if (tag != NULL && tag->type != MC_NBT_INT) {
        return tile_status(error, MC_TILE_INVALID_FIELD, "y", SIZE_MAX);
    }
    tile->y = tag == NULL ? 0 : tag->value.int_value;
    tag = mc_nbt_compound_get(compound, tile_z_name, 1);
    if (tag != NULL && tag->type != MC_NBT_INT) {
        return tile_status(error, MC_TILE_INVALID_FIELD, "z", SIZE_MAX);
    }
    tile->z = tag == NULL ? 0 : tag->value.int_value;
    return tile_status(error, MC_TILE_OK, NULL, SIZE_MAX);
}

static mc_tile_status tile_chest_read(mc_tile *tile, const mc_nbt_tag *compound,
                                      tile_import_context *context, mc_tile_error *error)
{
    const mc_nbt_tag *items = mc_nbt_compound_get(compound, tile_items_name, 5);
    if (items != NULL && items->type != MC_NBT_LIST) {
        return tile_status(error, MC_TILE_INVALID_FIELD, "Items", SIZE_MAX);
    }
    const mc_tile_status admitted =
        tile_import_admit(tile, context, 27u * sizeof(mc_item_stack *), error, "Items", SIZE_MAX);
    if (admitted != MC_TILE_OK) {
        return admitted;
    }
    mc_item_stack **slots = calloc(27u, sizeof(*slots));
    if (slots == NULL) {
        return tile_status(error, MC_TILE_ALLOCATION_FAILED, "Items", SIZE_MAX);
    }
    mc_item_stack **previous = tile->state.chest.slots;
    const size_t previous_capacity = tile->state.chest.capacity;
    tile->state.chest.slots = slots;
    tile->state.chest.capacity = 27;
    tile_slots_release(previous, previous_capacity);
    const size_t length = items == NULL ? 0 : items->value.list.length;
    for (size_t i = 0; i < length; ++i) {
        const mc_nbt_tag *record = items->value.list.items[i];
        if (record == NULL || record->type != MC_NBT_COMPOUND) {
            return tile_status(error, MC_TILE_INVALID_FIELD, "Items[]", i);
        }
        const mc_nbt_tag *slot_tag = mc_nbt_compound_get(record, tile_slot_name, 4);
        if (slot_tag != NULL && slot_tag->type != MC_NBT_BYTE) {
            return tile_status(error, MC_TILE_INVALID_FIELD, "Slot", i);
        }
        const uint32_t slot =
            slot_tag == NULL ? 0 : (uint32_t)(int32_t)slot_tag->value.byte_value & UINT32_C(255);
        if (slot >= tile->state.chest.capacity) {
            continue;
        }
        const mc_tile_status item_admitted =
            tile_import_admit(tile, context, mc_item_stack_object_bytes(), error, "Items[]", i);
        if (item_admitted != MC_TILE_OK) {
            return item_admitted;
        }
        mc_item_stack *stack;
        mc_item_stack_status item = mc_item_stack_allocate(&stack);
        if (item != MC_ITEM_STACK_OK) {
            return tile_item_error(error, item, NULL, i);
        }
        mc_item_stack_error item_error;
        item = mc_item_stack_construct_nbt(stack, record, &item_error);
        if (item != MC_ITEM_STACK_OK) {
            mc_item_stack_release(stack);
            return tile_item_error(error, item, &item_error, i);
        }
        /* Constructor success precedes actual assignment. Do not call setter:
         * NBT load has no count clamp or dirty dispatch. */
        mc_item_stack *old = tile->state.chest.slots[slot];
        tile->state.chest.slots[slot] = stack;
        if (context != NULL) {
            context->winners[slot] = (tile_loaded_record){stack, record, i};
        }
        mc_item_stack_release(old);
    }
    return tile_status(error, MC_TILE_OK, NULL, SIZE_MAX);
}

static mc_tile_status tile_spawner_read(mc_tile *tile, const mc_nbt_tag *compound,
                                        tile_import_context *context, mc_tile_error *error)
{
    const mc_nbt_tag *tag = mc_nbt_compound_get(compound, tile_entity_name, 8);
    if (tag != NULL && tag->type != MC_NBT_STRING) {
        return tile_status(error, MC_TILE_INVALID_FIELD, "EntityId", SIZE_MAX);
    }
    mc_tile_name *name = NULL;
    if (tag != NULL) {
        if (context != NULL) {
            if (tag->value.string.units == NULL && tag->value.string.length != 0) {
                return tile_status(error, MC_TILE_INVALID_ARGUMENT, "EntityId", SIZE_MAX);
            }
            if (tag->value.string.length > (SIZE_MAX - sizeof(mc_tile_name)) / sizeof(uint16_t)) {
                return tile_status(error, MC_TILE_LIMIT, "EntityId", SIZE_MAX);
            }
            const size_t bytes = sizeof(mc_tile_name) + tag->value.string.length * sizeof(uint16_t);
            const mc_tile_status admitted =
                tile_import_admit(tile, context, bytes, error, "EntityId", SIZE_MAX);
            if (admitted != MC_TILE_OK) {
                return admitted;
            }
        }
        const mc_tile_status created =
            mc_tile_name_create(tag->value.string.units, tag->value.string.length, &name);
        if (created != MC_TILE_OK) {
            return tile_status(error, created, "EntityId", SIZE_MAX);
        }
    }
    if (tag == NULL) {
        mc_registry_error attempt = live_empty_registry();
        mc_tile_status empty = live_literal(tile->classes, "", &name, &attempt, tile, context);
        if (empty != MC_TILE_OK) {
            tile_status(error, empty, "EntityId", SIZE_MAX);
            if (error != NULL)
                error->registry = attempt;
            return empty;
        }
    }
    const mc_tile_status assigned = mc_tile_spawner_set_name(tile, name);
    mc_tile_name_release(name);
    if (assigned != MC_TILE_OK) {
        return tile_status(error, assigned, "EntityId", SIZE_MAX);
    }
    tag = mc_nbt_compound_get(compound, tile_delay_name, 5);
    if (tag != NULL && tag->type != MC_NBT_SHORT) {
        return tile_status(error, MC_TILE_INVALID_FIELD, "Delay", SIZE_MAX);
    }
    tile->state.spawner.delay = tag == NULL ? 0 : tag->value.short_value;
    return tile_status(error, MC_TILE_OK, NULL, SIZE_MAX);
}

static mc_tile_status tile_read_nbt(mc_tile *tile, const mc_nbt_tag *compound,
                                    tile_import_context *context, mc_tile_error *error)
{
    const mc_tile_status status = mc_tile_read_base(tile, compound, error);
    if (status != MC_TILE_OK) {
        return status;
    }
    return tile->kind == MC_TILE_CHEST ? tile_chest_read(tile, compound, context, error)
                                       : tile_spawner_read(tile, compound, context, error);
}

mc_tile_status mc_tile_read_nbt(mc_tile *tile, const mc_nbt_tag *compound, mc_tile_error *error)
{
    return tile_read_nbt(tile, compound, NULL, error);
}

static mc_tile_status tile_attach_import_sources(mc_tile *tile, const mc_nbt_tag *compound,
                                                 const tile_import_context *context,
                                                 mc_tile_error *error)
{
    if (tile->source != NULL || tile->world != NULL || tile->phase != MC_TILE_READY) {
        return tile_status(error, MC_TILE_STATE_ERROR, NULL, SIZE_MAX);
    }
    if (tile->kind == MC_TILE_CHEST) {
        if (tile->state.chest.capacity != 27 || tile->state.chest.slots == NULL) {
            return tile_status(error, MC_TILE_STATE_ERROR, "Items", SIZE_MAX);
        }
        const mc_nbt_tag *items = mc_nbt_compound_get(compound, tile_items_name, 5);
        for (size_t i = 0; i < 27; ++i) {
            const tile_loaded_record *winner = &context->winners[i];
            if (tile->state.chest.slots[i] != winner->stack) {
                return tile_status(error, MC_TILE_STATE_ERROR, "Items[]", winner->index);
            }
            if (winner->stack == NULL) {
                continue;
            }
            if (items == NULL || items->type != MC_NBT_LIST ||
                winner->index >= items->value.list.length ||
                items->value.list.items[winner->index] != winner->record ||
                mc_item_stack_source(winner->stack) != NULL) {
                return tile_status(error, MC_TILE_STATE_ERROR, "Items[]", winner->index);
            }
            /* Fresh import creates a new owner for every successful record.
             * Same-stack aliases are allowed later, but cannot arise here. */
            for (size_t j = 0; j < i; ++j) {
                if (context->winners[j].stack == winner->stack) {
                    return tile_status(error, MC_TILE_STATE_ERROR, "Items[]", winner->index);
                }
            }
        }
    }

    size_t live;
    mc_tile_status status = tile_import_measure(tile, context, &live);
    if (status != MC_TILE_OK) {
        return tile_status(error, status, NULL, SIZE_MAX);
    }
    mc_nbt_limits limits = context->limits;
    if (live > limits.max_allocation_bytes) {
        return tile_status(error, MC_TILE_LIMIT, NULL, SIZE_MAX);
    }
    limits.max_allocation_bytes -= live;
    mc_nbt_tree *source;
    mc_nbt_error nbt = {MC_NBT_OK, 0};
    const mc_nbt_status copied = mc_nbt_tree_copy_compound(compound, &limits, &source, &nbt);
    if (copied != MC_NBT_OK) {
        return tile_nbt_error(error, copied, nbt, NULL, SIZE_MAX);
    }
    tile->source = source;

    if (tile->kind == MC_TILE_CHEST) {
        const mc_nbt_tag *items = mc_nbt_compound_get(mc_nbt_tree_root(source), tile_items_name, 5);
        for (size_t i = 0; i < 27; ++i) {
            const tile_loaded_record *winner = &context->winners[i];
            if (winner->stack == NULL) {
                continue;
            }
            if (tile->state.chest.slots[i] != winner->stack || items == NULL ||
                items->type != MC_NBT_LIST || winner->index >= items->value.list.length) {
                return tile_status(error, MC_TILE_STATE_ERROR, "Items[]", winner->index);
            }
            const mc_nbt_tag *record = items->value.list.items[winner->index];
            if (record == NULL || record->type != MC_NBT_COMPOUND || record->name.units != NULL ||
                record->name.length != 0) {
                return tile_status(error, MC_TILE_STATE_ERROR, "Items[]", winner->index);
            }
            status = tile_import_measure(tile, context, &live);
            if (status != MC_TILE_OK) {
                return tile_status(error, status, "Items[]", winner->index);
            }
            size_t item_bytes;
            mc_item_stack_status item = mc_item_stack_native_bytes(winner->stack, &item_bytes);
            if (item != MC_ITEM_STACK_OK) {
                return tile_item_error(error, item, NULL, winner->index);
            }
            if (item_bytes > live) {
                return tile_status(error, MC_TILE_STATE_ERROR, "Items[]", winner->index);
            }
            if (live > context->limits.max_allocation_bytes) {
                return tile_status(error, MC_TILE_LIMIT, "Items[]", winner->index);
            }
            limits = context->limits;
            /* Existing preserve_source deducts I itself. Supply B-(A-I), so
             * its helper sees exactly B-A with all earlier copies still live. */
            limits.max_allocation_bytes -= live - item_bytes;
            mc_item_stack_error item_error;
            item = mc_item_stack_preserve_source(winner->stack, record, &limits, &item_error);
            if (item != MC_ITEM_STACK_OK) {
                return tile_item_error(error, item, &item_error, winner->index);
            }
        }
    }
    return tile_status(error, MC_TILE_OK, NULL, SIZE_MAX);
}

static mc_tile_status
tile_import_storage_scoped(mc_world_runtime *world, mc_tile_kind resolved_kind,
                           const mc_nbt_tag *compound, const mc_nbt_limits *requested_limits,
                           mc_tile **out, mc_tile_error *error, const mc_world_chunk *scope_chunk)
{
    if (out == NULL) {
        return tile_status(error, MC_TILE_INVALID_ARGUMENT, NULL, SIZE_MAX);
    }
    if (resolved_kind != MC_TILE_CHEST && resolved_kind != MC_TILE_SPAWNER) {
        return tile_status(error, MC_TILE_INVALID_KIND, NULL, SIZE_MAX);
    }
    tile_import_context context = {0};
    context.scope_chunk = scope_chunk;
    if (!tile_limits(requested_limits, &context.limits)) {
        return tile_status(error, MC_TILE_INVALID_ARGUMENT, NULL, SIZE_MAX);
    }
    if (sizeof(mc_tile) > context.limits.max_allocation_bytes) {
        return tile_status(error, MC_TILE_LIMIT, NULL, SIZE_MAX);
    }
    if (scope_chunk != NULL) {
        mc_live_tile_error graph_error = live_empty_error();
        size_t graph;
        if (!live_graph_measure(scope_chunk, NULL, context.limits.max_allocation_bytes, &graph,
                                &graph_error))
            return tile_status(error,
                               graph_error.status == MC_WORLD_STORAGE_LIMIT ? MC_TILE_LIMIT
                                                                            : MC_TILE_STATE_ERROR,
                               NULL, SIZE_MAX);
        if (sizeof(mc_tile) > context.limits.max_allocation_bytes - graph)
            return tile_status(error, MC_TILE_LIMIT, NULL, SIZE_MAX);
    }
    mc_tile *tile;
    mc_tile_status status = mc_tile_allocate(world, resolved_kind, &tile);
    if (status != MC_TILE_OK) {
        tile_status(error, status, NULL, SIZE_MAX);
        if (error != NULL && world != NULL && world->classes != NULL &&
            world->classes->tiles != NULL && world->classes->tiles->phase == 3) {
            error->registry = world->classes->tiles->first_registry_failure;
            error->initializer_bytecode = world->classes->tiles->initializer_pc;
        }
        return status;
    }
    unsigned int source_bytecode = 25;
    status = tile_construct(tile, &context, error);
    if (status == MC_TILE_OK) {
        source_bytecode = 46;
        status = tile_read_nbt(tile, compound, &context, error);
    }
    if (status == MC_TILE_OK) {
        status = tile_attach_import_sources(tile, compound, &context, error);
    }
    if (status != MC_TILE_OK) {
        if (error != NULL)
            error->source_bytecode = source_bytecode;
        mc_tile_release(tile);
        return status;
    }
    *out = tile;
    return tile_status(error, MC_TILE_OK, NULL, SIZE_MAX);
}

mc_tile_status mc_tile_import_storage(mc_world_runtime *world, mc_tile_kind resolved_kind,
                                      const mc_nbt_tag *compound,
                                      const mc_nbt_limits *requested_limits, mc_tile **out,
                                      mc_tile_error *error)
{
    return tile_import_storage_scoped(world, resolved_kind, compound, requested_limits, out, error,
                                      NULL);
}

static bool tile_names_equal(mc_nbt_string left, mc_nbt_string right)
{
    return left.length == right.length &&
           (left.length == 0 ||
            memcmp(left.units, right.units, left.length * sizeof(uint16_t)) == 0);
}

static mc_tile_status tile_overlay_pointer_count(const mc_nbt_tag *source, const mc_nbt_tag *fields,
                                                 size_t field_count, size_t *out)
{
    size_t count = source == NULL ? 0 : source->value.compound.length;
    for (size_t i = 0; i < field_count; ++i) {
        const mc_nbt_string name = fields[i].name;
        if (source == NULL || mc_nbt_compound_get(source, name.units, name.length) == NULL) {
            if (!tile_size_add(&count, 1)) {
                return MC_TILE_LIMIT;
            }
        }
    }
    if (count > SIZE_MAX / sizeof(const mc_nbt_tag *)) {
        return MC_TILE_LIMIT;
    }
    *out = count;
    return MC_TILE_OK;
}

static void tile_overlay(const mc_nbt_tag *source, const mc_nbt_tag *fields, size_t field_count,
                         const mc_nbt_tag **items)
{
    size_t used = 0;
    if (source != NULL) {
        for (size_t i = 0; i < source->value.compound.length; ++i) {
            const mc_nbt_tag *old = source->value.compound.items[i];
            const mc_nbt_tag *replacement = old;
            for (size_t j = 0; j < field_count; ++j) {
                if (tile_names_equal(old->name, fields[j].name)) {
                    replacement = &fields[j];
                    break;
                }
            }
            items[used++] = replacement;
        }
    }
    for (size_t i = 0; i < field_count; ++i) {
        const mc_nbt_string name = fields[i].name;
        if (source == NULL || mc_nbt_compound_get(source, name.units, name.length) == NULL) {
            items[used++] = &fields[i];
        }
    }
}

static mc_tile_status tile_build_fields(const mc_tile *tile, mc_nbt_tag fields[6],
                                        size_t *field_count, mc_tile_error *error)
{
    fields[0].type = MC_NBT_STRING;
    fields[0].name = (mc_nbt_string){tile_id_name, 2};
    mc_tile_name *registered = live_class_name(tile->classes, tile->kind);
    if (registered == NULL) {
        const mc_tile_status status = tile_status(error, MC_TILE_STATE_ERROR, "id", SIZE_MAX);
        if (error != NULL)
            error->base_save_bytecode = 7;
        return status;
    }
    fields[0].value.string = registered->text;
    fields[1].type = fields[2].type = fields[3].type = MC_NBT_INT;
    fields[1].name = (mc_nbt_string){tile_x_name, 1};
    fields[2].name = (mc_nbt_string){tile_y_name, 1};
    fields[3].name = (mc_nbt_string){tile_z_name, 1};
    fields[1].value.int_value = tile->x;
    fields[2].value.int_value = tile->y;
    fields[3].value.int_value = tile->z;
    if (tile->kind == MC_TILE_CHEST) {
        fields[4].type = MC_NBT_LIST;
        fields[4].name = (mc_nbt_string){tile_items_name, 5};
        fields[4].value.list.element_type = MC_NBT_COMPOUND;
        *field_count = 5;
    } else {
        /* The original String construction rejects null before reaching Delay.
         * This independent result has no caller-visible mutable base prefix. */
        if (tile->state.spawner.name == NULL) {
            const mc_tile_status status =
                tile_status(error, MC_TILE_INVALID_ARGUMENT, "EntityId", SIZE_MAX);
            if (error != NULL)
                error->save_bytecode = 12;
            return status;
        }
        fields[4].type = MC_NBT_STRING;
        fields[4].name = (mc_nbt_string){tile_entity_name, 8};
        fields[4].value.string = tile->state.spawner.name->text;
        fields[5].type = MC_NBT_SHORT;
        fields[5].name = (mc_nbt_string){tile_delay_name, 5};
        fields[5].value.short_value = mc_java_i16_narrow(tile->state.spawner.delay);
        *field_count = 6;
    }
    return MC_TILE_OK;
}

mc_tile_status mc_tile_build(const mc_tile *tile, const mc_nbt_limits *requested_limits,
                             mc_nbt_document **out, mc_tile_error *error)
{
    if (out == NULL) {
        return tile_status(error, MC_TILE_INVALID_ARGUMENT, NULL, SIZE_MAX);
    }
    mc_tile_status result = tile_ready(tile);
    if (result != MC_TILE_OK) {
        return tile_status(error, result, NULL, SIZE_MAX);
    }
    mc_nbt_limits limits;
    if (!tile_limits(requested_limits, &limits)) {
        return tile_status(error, MC_TILE_INVALID_ARGUMENT, NULL, SIZE_MAX);
    }
    size_t live;
    result = mc_tile_storage_bytes(tile, &live);
    if (result != MC_TILE_OK) {
        return tile_status(error, result, NULL, SIZE_MAX);
    }
    if (live > limits.max_allocation_bytes) {
        return tile_status(error, MC_TILE_LIMIT, NULL, SIZE_MAX);
    }
    limits.max_allocation_bytes -= live;

    mc_nbt_tag fields[6] = {0};
    size_t field_count;
    result = tile_build_fields(tile, fields, &field_count, error);
    if (result != MC_TILE_OK) {
        return result;
    }
    const mc_nbt_tag *source = mc_nbt_tree_root(tile->source);
    size_t root_count;
    result = tile_overlay_pointer_count(source, fields, field_count, &root_count);
    if (result != MC_TILE_OK || root_count > limits.max_elements) {
        return tile_status(error, MC_TILE_LIMIT, NULL, SIZE_MAX);
    }

    /* Bounded physical storage, validated by storage_bytes: one stable view and
     * Slot tag PER ENTRY, even when several entries share a stack identity. */
    mc_item_stack_record_view records[36];
    mc_nbt_tag slots[36] = {0};
    const mc_item_stack *stacks[36];
    size_t child_counts[36];
    size_t occupied = 0;
    size_t pointer_count = root_count;
    if (tile->kind == MC_TILE_CHEST) {
        for (size_t i = 0; i < tile->state.chest.capacity; ++i) {
            const mc_item_stack *stack = tile->state.chest.slots[i];
            if (stack == NULL) {
                continue;
            }
            slots[occupied].type = MC_NBT_BYTE;
            slots[occupied].name = (mc_nbt_string){tile_slot_name, 4};
            /* i is in 0..35, so its int32 conversion is exact. */
            slots[occupied].value.byte_value = mc_java_i8_narrow((int32_t)i);
            stacks[occupied] = stack;
            mc_item_stack_error item_error;
            const mc_item_stack_status item = mc_item_stack_record_pointer_count(
                stack, &slots[occupied], 1, &child_counts[occupied], &item_error);
            if (item != MC_ITEM_STACK_OK) {
                return tile_item_error(error, item, &item_error, occupied);
            }
            if (child_counts[occupied] > limits.max_elements ||
                !tile_size_add(&pointer_count, child_counts[occupied])) {
                return tile_status(error, MC_TILE_LIMIT, "Items[]", occupied);
            }
            ++occupied;
        }
        if (occupied > limits.max_elements || !tile_size_add(&pointer_count, occupied)) {
            return tile_status(error, MC_TILE_LIMIT, "Items", SIZE_MAX);
        }
    }
    if (pointer_count > SIZE_MAX / sizeof(const mc_nbt_tag *)) {
        return tile_status(error, MC_TILE_LIMIT, NULL, SIZE_MAX);
    }
    /* Every slice holds the same pointer type, so malloc alignment and pointer
     * multiples suffice; there is no tag/byte-buffer packing to misalign. */
    const size_t scratch_bytes = pointer_count * sizeof(const mc_nbt_tag *);
    if (scratch_bytes > limits.max_allocation_bytes) {
        return tile_status(error, MC_TILE_LIMIT, NULL, SIZE_MAX);
    }
    const mc_nbt_tag **scratch = malloc(scratch_bytes);
    if (scratch == NULL) {
        return tile_status(error, MC_TILE_ALLOCATION_FAILED, NULL, SIZE_MAX);
    }
    limits.max_allocation_bytes -= scratch_bytes;
    size_t child_offset = root_count + occupied;
    for (size_t i = 0; i < occupied; ++i) {
        mc_item_stack_error item_error;
        const mc_item_stack_status item =
            mc_item_stack_record_overlay(stacks[i], &slots[i], 1, scratch + child_offset,
                                         child_counts[i], &records[i], &item_error);
        if (item != MC_ITEM_STACK_OK) {
            free(scratch);
            return tile_item_error(error, item, &item_error, i);
        }
        scratch[root_count + i] = &records[i].root;
        child_offset += child_counts[i];
    }
    if (tile->kind == MC_TILE_CHEST) {
        fields[4].value.list.items = occupied == 0 ? NULL : scratch + root_count;
        fields[4].value.list.length = occupied;
    }
    tile_overlay(source, fields, field_count, scratch);
    mc_nbt_tag root = {0};
    root.type = MC_NBT_COMPOUND;
    if (source != NULL) {
        root.name = source->name;
    }
    root.value.compound.items = scratch;
    root.value.compound.length = root_count;

    mc_nbt_error nbt = {MC_NBT_OK, 0};
    size_t length = 0;
    mc_nbt_status status = mc_nbt_measure(&root, &limits, &length, &nbt);
    uint8_t *encoded = NULL;
    mc_nbt_document *document = NULL;
    if (status == MC_NBT_OK) {
        if (length > limits.max_allocation_bytes) {
            status = MC_NBT_LIMIT;
            nbt = (mc_nbt_error){status, 0};
        } else {
            encoded = malloc(length);
            if (encoded == NULL) {
                status = MC_NBT_ALLOCATION_FAILED;
                nbt = (mc_nbt_error){status, 0};
            }
        }
    }
    if (status == MC_NBT_OK) {
        limits.max_allocation_bytes -= length;
        size_t written = 0;
        status = mc_nbt_write(&root, &limits, encoded, length, &written, &nbt);
        if (status == MC_NBT_OK) {
            if (written != length) {
                result = MC_TILE_STATE_ERROR;
            } else {
                size_t consumed = 0;
                /* max_input_bytes applies here, never to compound import. */
                status =
                    mc_nbt_read_compound(encoded, written, &limits, &document, &consumed, &nbt);
                if (status == MC_NBT_OK && consumed != written) {
                    mc_nbt_document_free(document);
                    document = NULL;
                    result = MC_TILE_STATE_ERROR;
                }
            }
        }
    }
    free(encoded);
    free(scratch);
    if (result != MC_TILE_OK) {
        return tile_status(error, result, NULL, SIZE_MAX);
    }
    if (status != MC_NBT_OK) {
        return tile_nbt_error(error, status, nbt, NULL, SIZE_MAX);
    }
    *out = document;
    return tile_status(error, MC_TILE_OK, NULL, SIZE_MAX);
}

/* The common World class owns this lazy ow/jh class lifetime. There is one
 * registry, not one registry per Tile or a separately bound class graph. */
static mc_tile_status live_literal(mc_tile_classes *classes, const char *ascii, mc_tile_name **out,
                                   mc_registry_error *attempt, const mc_tile *scope_tile,
                                   const tile_import_context *context)
{
    *attempt = live_empty_registry();
    const mc_registry_string *identity = NULL;
    mc_registry_error cause = live_empty_registry();
    mc_registry_status selected =
        mc_registry_runtime_literal_ascii(classes->registry, ascii, &identity, &cause);
    if (selected != MC_REGISTRY_OK) {
        *attempt = cause;
        return selected == MC_REGISTRY_LIMIT               ? MC_TILE_LIMIT
               : selected == MC_REGISTRY_ALLOCATION_FAILED ? MC_TILE_ALLOCATION_FAILED
                                                           : MC_TILE_WORLD_FAILED;
    }
    for (mc_tile_name *name = classes->literals; name != NULL; name = name->literal_next) {
        if (name->literal_identity == identity) {
            if (context != NULL && scope_tile != NULL) {
                const bool already =
                    context->scope_chunk == NULL
                        ? scope_tile->state.spawner.name == name
                        : live_graph_has_name(context->scope_chunk, scope_tile, name);
                const mc_tile_status admitted = tile_import_admit(
                    scope_tile, context, already ? 0 : mc_tile_name_native_bytes(name), NULL,
                    "EntityId", SIZE_MAX);
                if (admitted != MC_TILE_OK)
                    return admitted;
            }
            mc_tile_status held = mc_tile_name_retain(name);
            if (held == MC_TILE_OK)
                *out = name;
            return held;
        }
    }
    const size_t ceiling = 1024u * 1024u;
    if (classes->allocation_bytes > ceiling ||
        sizeof(mc_tile_name) > ceiling - classes->allocation_bytes)
        return MC_TILE_LIMIT;
    if (context != NULL && scope_tile != NULL) {
        const mc_tile_status admitted = tile_import_admit(scope_tile, context, sizeof(mc_tile_name),
                                                          NULL, "EntityId", SIZE_MAX);
        if (admitted != MC_TILE_OK)
            return admitted;
    }
    mc_registry_string_view view;
    if (!mc_registry_string_read(identity, &view))
        return MC_TILE_STATE_ERROR;
    mc_tile_name *name = calloc(1, sizeof(*name));
    if (name == NULL)
        return MC_TILE_ALLOCATION_FAILED;
    name->holds = 2; /* Actual class-owned pool occurrence and returned hold. */
    name->allocation_bytes = sizeof(*name);
    name->text = (mc_nbt_string){view.units, view.length};
    name->literal_identity = identity;
    name->literal_next = classes->literals;
    classes->literals = name;
    classes->allocation_bytes += sizeof(*name);
    *out = name;
    return MC_TILE_OK;
}

static mc_tile_status live_classes_owner(mc_world_runtime *world, mc_tile_classes **out)
{
    if (world == NULL || world->classes == NULL ||
        world->construction.mode != MC_WORLD_RUNTIME_GAME ||
        world->construction.phase == MC_WORLD_RUNTIME_RETIRED)
        return MC_TILE_INVALID_ARGUMENT;
    mc_world_runtime_classes *parent = world->classes;
    if (parent->tiles == NULL) {
        mc_tile_classes *classes = calloc(1, sizeof(*classes));
        if (classes == NULL)
            return MC_TILE_ALLOCATION_FAILED;
        classes->parent = parent;
        classes->registry = parent->registry;
        classes->allocation_bytes = sizeof(*classes);
        parent->tiles = classes; /* Native owner publication, no Java field. */
    }
    mc_tile_classes *classes = parent->tiles;
    *out = classes;
    return MC_TILE_OK;
}

static mc_tile_status live_classes_get(mc_world_runtime *world, mc_tile_classes **out)
{
    mc_tile_classes *classes = NULL;
    mc_tile_status owned = live_classes_owner(world, &classes);
    if (owned != MC_TILE_OK)
        return owned;
    if (classes->phase == 2) {
        *out = classes;
        return MC_TILE_OK;
    }
    if (classes->phase == 3)
        return classes->failure;
    if (classes->phase == 1)
        return MC_TILE_STATE_ERROR; /* No callback/reentrant clinit is synthesized. */
    classes->phase = 1;
    mc_registry_error attempt = live_empty_registry();
    mc_tile_status status = MC_TILE_OK;
    classes->initializer_pc = 3;
    const size_t class_ceiling = 1024u * 1024u;
    if (classes->allocation_bytes > class_ceiling ||
        LIVE_CLASS_COUNT * sizeof(*classes->by_name) > class_ceiling - classes->allocation_bytes)
        status = MC_TILE_LIMIT;
    else
        classes->by_name = calloc(LIVE_CLASS_COUNT, sizeof(*classes->by_name));
    if (status == MC_TILE_OK && classes->by_name == NULL)
        status = MC_TILE_ALLOCATION_FAILED;
    else if (status == MC_TILE_OK)
        classes->allocation_bytes += LIVE_CLASS_COUNT * sizeof(*classes->by_name);
    if (status == MC_TILE_OK) {
        classes->initializer_pc = 13;
        if (classes->allocation_bytes > class_ceiling ||
            LIVE_CLASS_COUNT * sizeof(*classes->by_class) >
                class_ceiling - classes->allocation_bytes)
            status = MC_TILE_LIMIT;
        else
            classes->by_class = calloc(LIVE_CLASS_COUNT, sizeof(*classes->by_class));
        if (status == MC_TILE_OK && classes->by_class == NULL)
            status = MC_TILE_ALLOCATION_FAILED;
        else if (status == MC_TILE_OK)
            classes->allocation_bytes += LIVE_CLASS_COUNT * sizeof(*classes->by_class);
    }
    static const char *const names[LIVE_CLASS_COUNT] = {
        "Furnace", "Chest", "RecordPlayer", "Trap", "Sign", "MobSpawner", "Music", "Piston"};
    for (unsigned int i = 0; i < LIVE_CLASS_COUNT && status == MC_TILE_OK; ++i) {
        classes->initializer_pc = 22u + 7u * i; /* Reached ldc String. */
        mc_tile_name *name = NULL;
        status = live_literal(classes, names[i], &name, &attempt, NULL, NULL);
        if (status != MC_TILE_OK)
            break;
        classes->initializer_pc = 24u + 7u * i;
        for (size_t previous = 0; previous < classes->name_count; ++previous) {
            if (tile_names_equal(classes->by_name[previous].name->text, name->text)) {
                status = MC_TILE_STATE_ERROR;
                break;
            }
        }
        if (status == MC_TILE_OK) {
            /* Actual source ID->Class44 precedes Class->ID55. Both fixed
             * native arrays represent those distinct Maps, whose iteration is
             * not exposed by the original callers. Literal pool owns name. */
            classes->by_name[classes->name_count++] = (live_class_entry){(live_class_id)i, name};
            classes->by_class[classes->class_count++] = (live_class_entry){(live_class_id)i, name};
        }
        mc_tile_name_release(name);
    }
    if (status != MC_TILE_OK) {
        classes->failure = status;
        classes->first_registry_failure = attempt;
        classes->phase = 3;
        return status;
    }
    classes->initializer_pc = 76;
    classes->phase = 2;
    *out = classes;
    return MC_TILE_OK;
}

static mc_tile_name *live_class_name(mc_tile_classes *classes, mc_tile_kind kind)
{
    const live_class_id selected = kind == MC_TILE_CHEST ? LIVE_CHEST : LIVE_SPAWNER;
    if (classes == NULL || classes->phase != 2)
        return NULL;
    for (size_t i = 0; i < classes->class_count; ++i)
        if (classes->by_class[i].kind == selected)
            return classes->by_class[i].name;
    return NULL;
}

mc_world_storage_status mc_live_tiles_classes_retire(mc_world_runtime_classes *parent,
                                                     mc_world_storage_error *error)
{
    mc_world_storage_error cause = {0};
    if (parent != NULL && parent->tiles != NULL) {
        mc_tile_classes *classes = parent->tiles;
        if (parent->live_worlds != 0 || classes->live_tiles != 0)
            cause =
                (mc_world_storage_error){MC_WORLD_STORAGE_BUSY, "Tile class has live owners", 0};
        for (mc_tile_name *name = classes->literals;
             cause.status == MC_WORLD_STORAGE_OK && name != NULL; name = name->literal_next)
            if (name->holds != 1)
                cause = (mc_world_storage_error){MC_WORLD_STORAGE_BUSY,
                                                 "Tile class literal has external holds", 0};
        if (cause.status == MC_WORLD_STORAGE_OK) {
            mc_tile_name *name = classes->literals;
            while (name != NULL) {
                mc_tile_name *next = name->literal_next;
                mc_tile_name_release(name);
                name = next;
            }
            free(classes->by_name);
            free(classes->by_class);
            free(classes);
            parent->tiles = NULL;
        }
    }
    if (error != NULL)
        *error = cause;
    return cause.status;
}

static mc_world_storage_status live_fail(mc_live_tile_error *error, mc_world_storage_status status,
                                         const char *operation)
{
    error->status = status;
    error->operation = operation;
    error->cause = (mc_world_storage_error){status, operation, 0};
    return status;
}

static mc_world_storage_status live_cause(mc_live_tile_error *error, mc_world_storage_error cause)
{
    error->status = cause.status;
    error->operation = cause.operation;
    error->cause = cause;
    return cause.status;
}

static bool live_registry_cause(mc_world_runtime *world, mc_live_tile_error *error,
                                const mc_registry_error *cause, const char *operation)
{
    error->registry = *cause;
    const mc_world_storage_status status =
        mc_world_runtime_registry_result(world, cause->status, cause, operation, &error->cause);
    error->status = status;
    error->operation = error->cause.operation;
    return false;
}

static mc_world_storage_status live_tile_error(mc_live_tile_error *error, mc_tile_status status,
                                               const char *operation)
{
    error->tile.status = status;
    if (error->tile.registry.status != MC_REGISTRY_OK)
        error->registry = error->tile.registry;
    if (error->tile.initializer_bytecode != 0)
        error->initializer_bytecode = error->tile.initializer_bytecode;
    const mc_world_storage_status mapped =
        status == MC_TILE_OK                  ? MC_WORLD_STORAGE_OK
        : status == MC_TILE_LIMIT             ? MC_WORLD_STORAGE_LIMIT
        : status == MC_TILE_ALLOCATION_FAILED ? MC_WORLD_STORAGE_ALLOCATION_FAILED
        : status == MC_TILE_INVALID_ARGUMENT  ? MC_WORLD_STORAGE_INVALID_ARGUMENT
                                              : MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    return live_fail(error, mapped, operation);
}

static mc_world_storage_status live_finish(mc_world_runtime *world, mc_live_tile_error frame,
                                           mc_live_tile_error *error)
{
    if (frame.status == MC_WORLD_STORAGE_OK)
        frame = live_empty_error();
    if (world != NULL)
        world->last_error = frame.cause;
    if (error != NULL)
        *error = frame;
    return frame.status;
}

static bool live_enter(mc_world_runtime *world, mc_live_tile_error *error)
{
    if (world == NULL || world->construction.mode != MC_WORLD_RUNTIME_GAME ||
        world->construction.phase == MC_WORLD_RUNTIME_RETIRED || world->tile_containers_retired) {
        live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Tile World native admission");
        return false;
    }
    mc_world_storage_error cause = {0};
    if (mc_world_runtime_call_enter(world, &cause) != MC_WORLD_STORAGE_OK) {
        live_cause(error, cause);
        return false;
    }
    return true;
}

mc_world_storage_status mc_live_tiles_new(mc_world_runtime *world, mc_tile_kind kind, mc_tile **out,
                                          mc_live_tile_error *error)
{
    mc_live_tile_error frame = live_empty_error();
    if (out == NULL) {
        live_fail(&frame, MC_WORLD_STORAGE_INVALID_ARGUMENT, "Tile constructor output");
        return live_finish(NULL, frame, error);
    }
    if (!live_enter(world, &frame))
        return live_finish(NULL, frame, error);
    mc_tile *tile = NULL;
    mc_tile_error detail = live_empty_error().tile;
    mc_tile_status status = mc_tile_allocate(world, kind, &tile);
    if (status == MC_TILE_OK)
        status = mc_tile_construct_detailed(tile, &detail);
    if (status != MC_TILE_OK) {
        frame.tile = detail;
        live_tile_error(&frame, status, "Tile constructor");
        if (world->classes->tiles != NULL) {
            if (world->classes->tiles->phase == 3) {
                frame.initializer_bytecode = world->classes->tiles->initializer_pc;
                frame.registry = world->classes->tiles->first_registry_failure;
            }
        }
        mc_tile_release(tile);
    } else
        *out = tile;
    mc_world_runtime_call_leave(world);
    return live_finish(world, frame, error);
}

static bool live_list_shape(const mc_world_reference_list *list)
{
    return list != NULL && list->count <= list->capacity && list->capacity <= (size_t)INT32_MAX &&
           list->capacity <= SIZE_MAX / sizeof(*list->items) &&
           ((list->capacity == 0) == (list->items == NULL));
}

void mc_live_tiles_capture_list(mc_world_runtime *world, bool deferred,
                                mc_world_reference_list *identity)
{
    /* Constructor-native hook: the same newly allocated empty descriptor. */
    mc_tile_list_ledger *ledger = &world->tile_membership[deferred ? 1u : 0u];
    ledger->identity = identity;
}

static bool live_list_valid(const mc_world_runtime *world, bool deferred)
{
    const mc_tile_list_ledger *ledger = &world->tile_membership[deferred ? 1u : 0u];
    const mc_world_reference_list *list = deferred ? world->fields.G : world->fields.c;
    if (list != ledger->identity)
        return false;
    if (list == NULL)
        return ledger->holds == NULL && ledger->count == 0 && ledger->capacity == 0;
    if (!live_list_shape(list) || ledger->count != list->count ||
        ledger->capacity != list->capacity ||
        ((ledger->capacity == 0) != (ledger->holds == NULL)) ||
        (ledger->holds != NULL && (void *)ledger->holds == (void *)list->items))
        return false;
    for (size_t i = 0; i < list->count; ++i)
        if (ledger->holds[i] == NULL || list->items[i] != ledger->holds[i])
            return false;
    return true;
}

bool mc_live_tiles_lists_valid(const mc_world_runtime *world)
{
    return world != NULL && live_list_valid(world, false) && live_list_valid(world, true);
}

static bool live_list_add(mc_world_runtime *world, bool deferred, mc_tile *tile,
                          mc_live_tile_error *error)
{
    mc_tile_list_ledger *ledger = &world->tile_membership[deferred ? 1u : 0u];
    mc_world_reference_list *list = deferred ? world->fields.G : world->fields.c;
    if (!live_list_valid(world, deferred) || list == NULL) {
        live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Tile List owning descriptor");
        return false;
    }
    if (list->count == (size_t)INT32_MAX) {
        live_fail(error, MC_WORLD_STORAGE_LIMIT, "Tile List size");
        return false;
    }
    void **items = list->items;
    mc_tile **holds = ledger->holds;
    size_t capacity = list->capacity, bytes = 0;
    if (list->count == capacity) {
        capacity = capacity == 0                       ? 4u
                   : capacity > (size_t)INT32_MAX / 2u ? (size_t)INT32_MAX
                                                       : capacity * 2u;
        if (capacity > SIZE_MAX / (sizeof(*items) + sizeof(*holds))) {
            live_fail(error, MC_WORLD_STORAGE_LIMIT, "Tile List byte overflow");
            return false;
        }
        bytes = capacity * (sizeof(*items) + sizeof(*holds));
        const size_t used = world->construction.direct_allocation_bytes;
        const size_t limit = world->construction_limits.max_direct_allocation_bytes;
        if (used > limit || bytes > limit - used) {
            live_fail(error, MC_WORLD_STORAGE_LIMIT, "Tile List peak allocation budget");
            return false;
        }
        items = malloc(capacity * sizeof(*items));
        holds = malloc(capacity * sizeof(*holds));
        if (items == NULL || holds == NULL) {
            free(items);
            free(holds);
            live_fail(error, MC_WORLD_STORAGE_ALLOCATION_FAILED, "Tile List allocation");
            return false;
        }
        if (list->count != 0) {
            memcpy(items, list->items, list->count * sizeof(*items));
            memcpy(holds, ledger->holds, list->count * sizeof(*holds));
        }
    }
    mc_tile_status held = mc_tile_retain(tile);
    if (held != MC_TILE_OK) {
        if (bytes != 0) {
            free(items);
            free(holds);
        }
        live_tile_error(error, held, "Tile List occurrence retain");
        return false;
    }
    if (bytes != 0) {
        const size_t old_bytes =
            list->capacity * sizeof(*list->items) + ledger->capacity * sizeof(*holds);
        free(list->items);
        free(ledger->holds);
        list->items = items;
        list->capacity = capacity;
        ledger->holds = holds;
        ledger->capacity = capacity;
        world->construction.direct_allocation_bytes += bytes;
        world->construction.direct_allocation_bytes -= old_bytes;
    }
    list->items[list->count++] = tile;
    ledger->holds[ledger->count++] = tile;
    ledger->mod_count = mc_java_i32_add(ledger->mod_count, 1);
    return true;
}

static bool live_list_remove_first(mc_world_runtime *world, mc_tile *tile,
                                   mc_live_tile_error *error)
{
    if (!live_list_valid(world, false) || world->fields.c == NULL) {
        live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Tile List remove descriptor");
        return false;
    }
    mc_world_reference_list *list = world->fields.c;
    mc_tile_list_ledger *ledger = &world->tile_membership[0];
    for (size_t i = 0; i < list->count; ++i) {
        if (ledger->holds[i] != tile)
            continue;
        memmove(list->items + i, list->items + i + 1,
                (list->count - i - 1u) * sizeof(*list->items));
        memmove(ledger->holds + i, ledger->holds + i + 1,
                (ledger->count - i - 1u) * sizeof(*ledger->holds));
        --list->count;
        --ledger->count;
        ledger->mod_count = mc_java_i32_add(ledger->mod_count, 1);
        mc_tile_release(tile);
        break;
    }
    return true;
}

void mc_live_tiles_lists_release(mc_world_runtime *world)
{
    world->tile_containers_retired = true;
    for (unsigned int selected = 0; selected < 2; ++selected) {
        mc_tile_list_ledger *ledger = &world->tile_membership[selected];
        mc_world_reference_list *list = ledger->identity;
        mc_tile **holds = ledger->holds;
        const size_t count = ledger->count;
        const size_t bytes = ledger->capacity * sizeof(*holds) +
                             (list == NULL ? 0 : list->capacity * sizeof(*list->items));
        if (list != NULL) {
            free(list->items);
            list->items = NULL;
            list->count = list->capacity = 0;
        }
        ledger->holds = NULL;
        ledger->count = ledger->capacity = 0;
        world->construction.direct_allocation_bytes -= bytes;
        for (size_t i = 0; i < count; ++i)
            mc_tile_release(holds[i]);
        free(holds);
    }
}
typedef struct live_map_node live_map_node;
struct live_map_node {
    int32_t x, y, z;
    uint32_t hash;
    mc_tile *value;
    live_map_node *next, *previous, *parent, *children[2];
    bool red, tree;
};

struct mc_tile_map_backing {
    mc_world_chunk *owner;
    live_map_node **buckets;
    size_t size, allocation_bytes;
    uint32_t capacity, threshold;
    bool journal_started, journal_complete;
    mc_nbt_tree *journal;
    bool *consumed;
    size_t record_count;
};

/* Rotations, balancing, chain-root movement and partitions below reuse the
 * existing concrete Integer HashSet mechanics. Tile keys differ: full triple
 * equality and non-Comparable full-hash ties. A tie requiring identityHashCode
 * is rejected before native map mutation. No coordinate/pointer ordering
 * pretends to be that missing profile. */
static bool live_map_before(uint32_t left, uint32_t right)
{
    return (left ^ UINT32_C(0x80000000)) < (right ^ UINT32_C(0x80000000));
}
static uint32_t live_map_threshold(uint32_t capacity)
{
    return capacity == UINT32_C(0x40000000) ? UINT32_C(0x7fffffff) : capacity - capacity / 4u;
}
static live_map_node *live_map_root(live_map_node *node)
{
    while (node->parent != NULL)
        node = node->parent;
    return node;
}

/* Promote one child while preserving the separate traversal chain. */
static live_map_node *live_map_rotate(live_map_node *root, live_map_node *node, unsigned side)
{
    live_map_node *pivot = node->children[side];
    live_map_node *parent = node->parent;
    node->children[side] = pivot->children[1u - side];
    if (node->children[side] != NULL)
        node->children[side]->parent = node;
    pivot->parent = parent;
    if (parent == NULL) {
        root = pivot;
        root->red = false;
    } else {
        parent->children[parent->children[1] == node ? 1 : 0] = pivot;
    }
    pivot->children[1u - side] = node;
    node->parent = pivot;
    return root;
}

static live_map_node *live_map_balance_add(live_map_node *root, live_map_node *node)
{
    node->red = true;
    for (;;) {
        live_map_node *parent = node->parent;
        if (parent == NULL) {
            node->red = false;
            return node;
        }
        live_map_node *grand = parent->parent;
        if (!parent->red || grand == NULL)
            return root;
        unsigned side = grand->children[1] == parent ? 1u : 0u;
        live_map_node *uncle = grand->children[1u - side];
        if (uncle != NULL && uncle->red) {
            parent->red = false;
            uncle->red = false;
            grand->red = true;
            node = grand;
            continue;
        }
        if (node == parent->children[1u - side]) {
            root = live_map_rotate(root, parent, 1u - side);
            node = parent;
            parent = node->parent;
            grand = parent->parent;
        }
        parent->red = false;
        grand->red = true;
        return live_map_rotate(root, grand, side);
    }
}

static bool live_map_red(const live_map_node *node)
{
    return node != NULL && node->red;
}

static live_map_node *live_map_balance_remove(live_map_node *root, live_map_node *node)
{
    while (node != NULL && node != root) {
        live_map_node *parent = node->parent;
        if (parent == NULL) {
            node->red = false;
            return node;
        }
        if (node->red) {
            node->red = false;
            return root;
        }
        unsigned side = parent->children[1] == node ? 1u : 0u;
        live_map_node *sibling = parent->children[1u - side];
        if (live_map_red(sibling)) {
            sibling->red = false;
            parent->red = true;
            root = live_map_rotate(root, parent, 1u - side);
            parent = node->parent;
            sibling = parent == NULL ? NULL : parent->children[1u - side];
        }
        if (sibling == NULL) {
            node = parent;
            continue;
        }
        live_map_node *near = sibling->children[side];
        live_map_node *far = sibling->children[1u - side];
        if (!live_map_red(near) && !live_map_red(far)) {
            sibling->red = true;
            node = parent;
            continue;
        }
        if (!live_map_red(far)) {
            if (near != NULL)
                near->red = false;
            sibling->red = true;
            root = live_map_rotate(root, sibling, side);
            parent = node->parent;
            sibling = parent == NULL ? NULL : parent->children[1u - side];
        }
        if (sibling != NULL) {
            sibling->red = parent != NULL && parent->red;
            far = sibling->children[1u - side];
            if (far != NULL)
                far->red = false;
        }
        if (parent != NULL) {
            parent->red = false;
            root = live_map_rotate(root, parent, 1u - side);
        }
        node = root;
    }
    return root;
}

static void live_map_front(live_map_node **buckets, uint32_t capacity, live_map_node *root)
{
    uint32_t index = root->hash & (capacity - 1u);
    live_map_node *head = buckets[index];
    if (head == root)
        return;
    if (root->next != NULL)
        root->next->previous = root->previous;
    if (root->previous != NULL)
        root->previous->next = root->next;
    root->previous = NULL;
    root->next = head;
    if (head != NULL)
        head->previous = root;
    buckets[index] = root;
}

static void live_map_treeify(live_map_node **buckets, uint32_t capacity, uint32_t index)
{
    live_map_node *root = NULL, *previous = NULL;
    for (live_map_node *node = buckets[index]; node != NULL; node = node->next) {
        node->previous = previous;
        previous = node;
        node->parent = node->children[0] = node->children[1] = NULL;
        node->red = false;
        node->tree = true;
        if (root == NULL) {
            root = node;
            continue;
        }
        live_map_node *parent = root;
        for (;;) {
            unsigned side = live_map_before(node->hash, parent->hash) ? 0u : 1u;
            if (parent->children[side] == NULL) {
                parent->children[side] = node;
                node->parent = parent;
                root = live_map_balance_add(root, node);
                break;
            }
            parent = parent->children[side];
        }
    }
    live_map_front(buckets, capacity, root);
}

static void live_map_plain(live_map_node *head)
{
    for (live_map_node *node = head; node != NULL; node = node->next) {
        node->tree = node->red = false;
        node->parent = node->children[0] = node->children[1] = NULL;
    }
}

static void live_map_tree_add(mc_tile_map_backing *set, live_map_node *head, live_map_node *node)
{
    live_map_node *root = live_map_root(head), *parent = root;
    for (;;) {
        unsigned side = live_map_before(node->hash, parent->hash) ? 0u : 1u;
        if (parent->children[side] == NULL) {
            parent->children[side] = node;
            node->tree = true;
            node->parent = node->previous = parent;
            node->next = parent->next;
            if (node->next != NULL)
                node->next->previous = node;
            parent->next = node;
            live_map_front(set->buckets, set->capacity, live_map_balance_add(root, node));
            return;
        }
        parent = parent->children[side];
    }
}

static void live_map_resize(mc_tile_map_backing *set, live_map_node **table, uint32_t capacity)
{
    uint32_t old_capacity = set->capacity;
    for (uint32_t index = 0; index < old_capacity; ++index) {
        live_map_node *head = set->buckets[index];
        if (head == NULL)
            continue;
        if (head->next == NULL) {
            table[head->hash & (capacity - 1u)] = head;
            continue;
        }
        bool tree = head->tree;
        live_map_node *heads[2] = {NULL, NULL}, *tails[2] = {NULL, NULL};
        size_t counts[2] = {0, 0};
        while (head != NULL) {
            live_map_node *next = head->next;
            unsigned side = (head->hash & old_capacity) == 0 ? 0u : 1u;
            head->next = NULL;
            head->previous = tails[side];
            if (tails[side] == NULL)
                heads[side] = head;
            else
                tails[side]->next = head;
            tails[side] = head;
            ++counts[side];
            head = next;
        }
        for (unsigned side = 0; side < 2u; ++side) {
            if (heads[side] == NULL)
                continue;
            uint32_t target = index + (side == 0 ? 0 : old_capacity);
            table[target] = heads[side];
            if (tree) {
                if (counts[side] <= 6u)
                    live_map_plain(heads[side]);
                else if (heads[1u - side] != NULL)
                    live_map_treeify(table, capacity, target);
            }
        }
    }
    free(set->buckets);
    set->owner->allocation_bytes -= (size_t)old_capacity * sizeof(*set->buckets);
    set->allocation_bytes -= (size_t)old_capacity * sizeof(*set->buckets);
    set->buckets = table;
    set->capacity = capacity;
    set->threshold = live_map_threshold(capacity);
}

static void live_map_tree_remove(mc_tile_map_backing *set, live_map_node *node)
{
    uint32_t index = node->hash & (set->capacity - 1u);
    live_map_node *root = live_map_root(set->buckets[index]);
    if (node->previous == NULL)
        set->buckets[index] = node->next;
    else
        node->previous->next = node->next;
    if (node->next != NULL)
        node->next->previous = node->previous;
    if (set->buckets[index] == NULL)
        return;
    if (root->children[1] == NULL || root->children[0] == NULL ||
        root->children[0]->children[0] == NULL) {
        live_map_plain(set->buckets[index]);
        return;
    }
    live_map_node *left = node->children[0], *right = node->children[1], *replacement;
    if (left != NULL && right != NULL) {
        live_map_node *successor = right;
        while (successor->children[0] != NULL)
            successor = successor->children[0];
        bool red = successor->red;
        successor->red = node->red;
        node->red = red;
        live_map_node *tail = successor->children[1], *parent = node->parent;
        if (successor == right) {
            node->parent = successor;
            successor->children[1] = node;
        } else {
            live_map_node *previous = successor->parent;
            node->parent = previous;
            previous->children[previous->children[1] == successor ? 1 : 0] = node;
            successor->children[1] = right;
            right->parent = successor;
        }
        node->children[0] = NULL;
        node->children[1] = tail;
        if (tail != NULL)
            tail->parent = node;
        successor->children[0] = left;
        left->parent = successor;
        successor->parent = parent;
        if (parent == NULL)
            root = successor;
        else
            parent->children[parent->children[1] == node ? 1 : 0] = successor;
        replacement = tail == NULL ? node : tail;
    } else {
        replacement = left != NULL ? left : right != NULL ? right : node;
    }
    if (replacement != node) {
        live_map_node *parent = node->parent;
        replacement->parent = parent;
        if (parent == NULL) {
            root = replacement;
            root->red = false;
        } else {
            parent->children[parent->children[1] == node ? 1 : 0] = replacement;
        }
        node->parent = node->children[0] = node->children[1] = NULL;
    }
    if (!node->red)
        root = live_map_balance_remove(root, replacement);
    if (replacement == node) {
        live_map_node *parent = node->parent;
        if (parent != NULL) {
            if (parent->children[0] == node)
                parent->children[0] = NULL;
            else if (parent->children[1] == node)
                parent->children[1] = NULL;
        }
        node->parent = NULL;
    }
    live_map_front(set->buckets, set->capacity, root);
}

static uint32_t live_key_hash(int32_t x, int32_t y, int32_t z)
{
    int32_t raw = mc_java_i32_add(
        mc_java_i32_add(mc_java_i32_mul(x, 8976890), mc_java_i32_mul(y, 981131)), z);
    uint32_t bits = (uint32_t)raw;
    return bits ^ (bits >> 16u);
}

static live_map_node *live_map_find(const mc_tile_map_backing *map, int32_t x, int32_t y, int32_t z,
                                    uint32_t hash)
{
    if (map == NULL || map->capacity == 0)
        return NULL;
    /* The concrete wf/oa equals body is pure full-int comparison. Searching
     * the same bucket chain returns the same identity without changing order. */
    for (live_map_node *node = map->buckets[hash & (map->capacity - 1u)]; node != NULL;
         node = node->next)
        if (node->hash == hash && node->x == x && node->y == y && node->z == z)
            return node;
    return NULL;
}

static bool live_owned_map_valid(const mc_world_chunk *chunk)
{
    if (chunk == NULL)
        return false;
    const mc_world_chunk_tile_map *descriptor = &chunk->tile_map;
    const mc_tile_map_backing *map = descriptor->backing;
    if (map == NULL)
        return descriptor->count == 0 &&
               ((descriptor->release_backing == NULL && descriptor->backing_bytes == NULL) ||
                (descriptor->release_backing == mc_live_tiles_map_release &&
                 descriptor->backing_bytes == mc_live_tiles_owned_map_bytes));
    if (descriptor->release_backing != mc_live_tiles_map_release ||
        descriptor->backing_bytes != mc_live_tiles_owned_map_bytes || map->owner != chunk ||
        map->capacity > UINT32_C(0x40000000) ||
        (map->capacity != 0 && (map->capacity & (map->capacity - 1u)) != 0) ||
        ((map->capacity == 0) != (map->buckets == NULL)) ||
        (map->capacity == 0 && map->size != 0) || map->size != descriptor->count ||
        map->size > (size_t)INT32_MAX || map->record_count > SIZE_MAX / sizeof(*map->consumed) ||
        ((map->record_count == 0) != (map->consumed == NULL)))
        return false;
    size_t visited = 0;
    for (uint32_t bucket = 0; bucket < map->capacity; ++bucket) {
        live_map_node *previous = NULL;
        for (const live_map_node *node = map->buckets[bucket]; node != NULL; node = node->next) {
            if (visited == map->size || node->previous != previous || node->value == NULL ||
                (node->hash & (map->capacity - 1u)) != bucket)
                return false;
            previous = (live_map_node *)node;
            ++visited;
        }
    }
    size_t expected = sizeof(*map);
    if (map->size > (SIZE_MAX - expected) / sizeof(live_map_node))
        return false;
    expected += map->size * sizeof(live_map_node);
    if (!tile_size_add(&expected, (size_t)map->capacity * sizeof(*map->buckets)) ||
        !tile_size_add(&expected, mc_nbt_tree_allocation_bytes(map->journal)) ||
        !tile_size_add(&expected, map->record_count * sizeof(*map->consumed)))
        return false;
    return visited == map->size && expected == map->allocation_bytes &&
           expected <= chunk->allocation_bytes;
}

bool mc_live_tiles_owned_map_bytes(const mc_world_chunk *chunk, size_t *out)
{
    /* This lower native inspector never evaluates source l or recursively
     * validates the common Chunk ledger that called it. */
    if (out == NULL || !live_owned_map_valid(chunk))
        return false;
    *out = chunk->tile_map.backing == NULL ? 0 : chunk->tile_map.backing->allocation_bytes;
    return true;
}

bool mc_live_tiles_map_valid(const mc_world_chunk *chunk)
{
    return chunk != NULL && chunk->l == &chunk->tile_map && live_owned_map_valid(chunk);
}

static bool live_map_put(mc_world_chunk *chunk, int32_t x, int32_t y, int32_t z, mc_tile *tile,
                         size_t requested_ceiling, mc_live_tile_error *error)
{
    mc_world_chunk_tile_map *descriptor = chunk->l;
    if (!mc_live_tiles_map_valid(chunk) ||
        (descriptor->backing == NULL && descriptor->release_backing != NULL)) {
        live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Tile Map native ownership");
        return false;
    }
    const size_t ceiling = requested_ceiling != 0 && requested_ceiling < chunk->max_allocation_bytes
                               ? requested_ceiling
                               : chunk->max_allocation_bytes;
    size_t graph;
    if (!live_graph_measure(chunk, tile, ceiling, &graph, error))
        return false;
    mc_tile_map_backing *map = descriptor->backing;
    const uint32_t hash = live_key_hash(x, y, z);
    live_map_node *existing = live_map_find(map, x, y, z, hash);
    if (existing != NULL) {
        mc_tile_status held = mc_tile_retain(tile);
        if (held != MC_TILE_OK) {
            live_tile_error(error, held, "Tile Map replacement retain");
            return false;
        }
        mc_tile *old = existing->value;
        existing->value = tile; /* Existing node/key/order retained. */
        mc_tile_release(old);
        return true;
    }
    const size_t size = map == NULL ? 0 : map->size;
    if (size == (size_t)INT32_MAX) {
        live_fail(error, MC_WORLD_STORAGE_LIMIT, "Tile Map size");
        return false;
    }
    uint32_t old_capacity = map == NULL ? 0 : map->capacity;
    live_map_node *head = old_capacity == 0 ? NULL : map->buckets[hash & (old_capacity - 1u)];
    size_t chain = 0;
    bool equal_full_hash = false;
    for (live_map_node *node = head; node != NULL; node = node->next) {
        ++chain;
        equal_full_hash |= node->hash == hash;
    }
    if (old_capacity >= 64u && head != NULL && !head->tree && chain >= 8u) {
        for (live_map_node *node = head; node != NULL; node = node->next)
            for (live_map_node *other = head; other != node; other = other->next)
                equal_full_hash |= other->hash == node->hash;
    }
    if (old_capacity >= 64u && (head != NULL && (head->tree || chain >= 8u)) && equal_full_hash) {
        live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                  "Tile Map non-Comparable identityHashCode tree profile");
        return false;
    }
    uint32_t capacity = old_capacity == 0 ? 16u : old_capacity;
    bool collision = head != NULL && !head->tree && chain >= 8u && capacity < 64u;
    uint32_t capacities[2];
    size_t table_count = 0;
    if (old_capacity == 0)
        capacities[table_count++] = capacity;
    if (collision) {
        capacity *= 2u;
        capacities[table_count++] = capacity;
    }
    const bool growth = size + 1u > live_map_threshold(capacity) && capacity < UINT32_C(0x40000000);
    if (growth) {
        capacity *= 2u;
        capacities[table_count++] = capacity;
    }
    size_t additional = sizeof(live_map_node) + (map == NULL ? sizeof(*map) : 0);
    size_t table_bytes[2];
    for (size_t i = 0; i < table_count; ++i) {
        if (!tile_size_multiply((size_t)capacities[i], sizeof(live_map_node *), &table_bytes[i])) {
            live_fail(error, MC_WORLD_STORAGE_LIMIT, "Tile Map table byte overflow");
            return false;
        }
        if (!tile_size_add(&additional, table_bytes[i])) {
            live_fail(error, MC_WORLD_STORAGE_LIMIT, "Tile Map peak byte overflow");
            return false;
        }
    }
    if (graph > ceiling || additional > ceiling - graph) {
        live_fail(error, MC_WORLD_STORAGE_LIMIT, "Tile Map peak allocation budget");
        return false;
    }
    mc_tile_map_backing *fresh_map = map == NULL ? calloc(1, sizeof(*map)) : NULL;
    live_map_node *node = calloc(1, sizeof(*node));
    live_map_node **tables[2] = {NULL, NULL};
    bool allocated = node != NULL && (map != NULL || fresh_map != NULL);
    for (size_t i = 0; i < table_count && allocated; ++i) {
        tables[i] = calloc(capacities[i], sizeof(*tables[i]));
        allocated = tables[i] != NULL;
    }
    mc_tile_status held = allocated ? mc_tile_retain(tile) : MC_TILE_ALLOCATION_FAILED;
    if (!allocated || held != MC_TILE_OK) {
        free(fresh_map);
        free(node);
        for (size_t i = 0; i < table_count; ++i)
            free(tables[i]);
        live_tile_error(error, held, "Tile Map allocation/occurrence retain");
        return false;
    }
    chunk->allocation_bytes += additional;
    if (map == NULL) {
        map = fresh_map;
        map->owner = chunk;
        descriptor->backing = map;
        descriptor->release_backing = mc_live_tiles_map_release;
        descriptor->backing_bytes = mc_live_tiles_owned_map_bytes;
    }
    map->allocation_bytes += additional;
    *node = (live_map_node){.x = x, .y = y, .z = z, .hash = hash, .value = tile};
    size_t table_index = 0;
    if (old_capacity == 0)
        live_map_resize(map, tables[table_index++], capacities[0]);
    const uint32_t index = hash & (map->capacity - 1u);
    head = map->buckets[index];
    if (head == NULL)
        map->buckets[index] = node;
    else if (head->tree)
        live_map_tree_add(map, head, node);
    else {
        live_map_node *tail = head;
        while (tail->next != NULL)
            tail = tail->next;
        tail->next = node;
        node->previous = tail;
        if (chain >= 8u) {
            if (collision) {
                live_map_resize(map, tables[table_index], capacities[table_index]);
                ++table_index;
            } else
                live_map_treeify(map->buckets, map->capacity, index);
        }
    }
    ++map->size;
    ++descriptor->count;
    if (growth)
        live_map_resize(map, tables[table_index], capacities[table_index]);
    return true;
}

static mc_tile *live_map_remove(mc_world_chunk *chunk, int32_t x, int32_t y, int32_t z)
{
    mc_tile_map_backing *map = chunk->l->backing;
    live_map_node *node = live_map_find(map, x, y, z, live_key_hash(x, y, z));
    if (node == NULL)
        return NULL;
    if (node->tree)
        live_map_tree_remove(map, node);
    else {
        if (node->previous == NULL)
            map->buckets[node->hash & (map->capacity - 1u)] = node->next;
        else
            node->previous->next = node->next;
        if (node->next != NULL)
            node->next->previous = node->previous;
    }
    mc_tile *value = node->value; /* Transfer this actual map occurrence hold. */
    free(node);
    --map->size;
    --chunk->l->count;
    chunk->allocation_bytes -= sizeof(*node);
    map->allocation_bytes -= sizeof(*node);
    return value;
}

void mc_live_tiles_map_release(mc_world_chunk_tile_map *descriptor)
{
    mc_tile_map_backing *map = descriptor->backing;
    descriptor->backing = NULL;
    descriptor->count = 0;
    descriptor->release_backing = mc_live_tiles_map_release; /* Terminal native marker. */
    descriptor->backing_bytes = mc_live_tiles_owned_map_bytes;
    if (map == NULL)
        return;
    mc_world_chunk *owner = map->owner;
    for (uint32_t i = 0; i < map->capacity; ++i) {
        live_map_node *node = map->buckets[i];
        while (node != NULL) {
            live_map_node *next = node->next;
            mc_tile *value = node->value;
            free(node);
            owner->allocation_bytes -= sizeof(*node);
            mc_tile_release(value);
            node = next;
        }
    }
    owner->allocation_bytes -= sizeof(*map) + (size_t)map->capacity * sizeof(*map->buckets);
    owner->allocation_bytes -=
        mc_nbt_tree_allocation_bytes(map->journal) + map->record_count * sizeof(*map->consumed);
    mc_nbt_tree_free(map->journal);
    free(map->consumed);
    free(map->buckets);
    free(map);
}

mc_world_storage_status mc_live_tiles_map_at(const mc_world_chunk *chunk, size_t ordinal,
                                             mc_tile **out)
{
    if (out == NULL || !mc_live_tiles_map_valid(chunk) || ordinal >= chunk->l->count)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    const mc_tile_map_backing *map = chunk->l->backing;
    size_t index = 0;
    for (uint32_t i = 0; i < map->capacity; ++i) {
        for (live_map_node *node = map->buckets[i]; node != NULL; node = node->next) {
            if (index++ == ordinal) {
                *out = node->value; /* Borrowed same owner for actual values traversal. */
                return MC_WORLD_STORAGE_OK;
            }
        }
    }
    return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
}
static bool live_tile_block_kind(const mc_block *block)
{
    if (block == NULL)
        return false;
    switch (block->kind) {
        case MC_BLOCK_DD_BZ:
        case MC_BLOCK_E_E:
        case MC_BLOCK_FO_DT:
        case MC_BLOCK_PT_JR:
        case MC_BLOCK_TC_LZ:
        case MC_BLOCK_UJ_MT:
        case MC_BLOCK_UT_MZ:
        case MC_BLOCK_XQ_PA:
            return true;
        default:
            return false;
    }
}

static bool live_chunk_block(mc_world_chunk *chunk, int32_t x, int32_t y, int32_t z, int32_t *out,
                             mc_live_tile_error *error)
{
    const int32_t cell = mc_java_i32_shl(x, 11) | mc_java_i32_shl(z, 7) | y;
    mc_world_chunk_fields fields;
    if (mc_world_chunk_fields_get(chunk, &fields) != MC_WORLD_STORAGE_OK || fields.b == NULL ||
        cell < 0 || (uint32_t)cell >= MC_CHUNK_BLOCKS) {
        live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Tile Chunk block element");
        return false;
    }
    *out = fields.b[(size_t)cell];
    return true;
}

static bool live_acquire(mc_world_runtime *world, int32_t x, int32_t z, mc_chunk_state **out,
                         mc_live_tile_error *error)
{
    /* Actual World.c reads v1 before provider.b6, including cached hits. */
    if (world->fields.v == NULL) {
        error->provider_bytecode = 6;
        live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World Tile provider receiver");
        return false;
    }
    mc_world_runtime_acquisition_error acquired;
    mc_chunk_acquire_status status = mc_world_runtime_get_detailed(world, x, z, out, &acquired);
    if (status == MC_CHUNK_ACQUIRE_OK)
        return true;
    if (acquired.tile.status != MC_WORLD_STORAGE_OK) {
        const mc_live_tile_call_frame caller = {.bytecode = error->bytecode,
                                                .chunk_bytecode = error->chunk_bytecode,
                                                .observer_bytecode = error->observer_bytecode,
                                                .initializer_bytecode = error->initializer_bytecode,
                                                .caller_bytecode = error->caller_bytecode,
                                                .hook_bytecode = error->hook_bytecode,
                                                .remove_bytecode = error->remove_bytecode,
                                                .provider_bytecode = error->provider_bytecode,
                                                .tile_bytecode = error->tile_bytecode,
                                                .inventory_bytecode = error->inventory_bytecode};
        *error = acquired.tile;
        error->acquisition_caller_bytecode = caller.bytecode;
        error->acquisition_parent = caller;
    } else if (acquired.cause.status != MC_WORLD_STORAGE_OK)
        live_cause(error, acquired.cause);
    else if (status == MC_CHUNK_ACQUIRE_LIMIT || status == MC_CHUNK_ACQUIRE_PARTIAL_LIMIT)
        live_fail(error, MC_WORLD_STORAGE_LIMIT, "Tile Chunk acquisition limit");
    else
        live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Tile Chunk acquisition");
    if (acquired.registry.status != MC_REGISTRY_OK)
        error->registry = acquired.registry;
    error->batch = acquired.batch;
    error->activation_bytecode = acquired.activation_bytecode;
    error->activation_world_bytecode = acquired.activation_world_bytecode;
    error->load_bytecode = acquired.load_bytecode;
    error->load_builder_bytecode = acquired.load_builder_bytecode;
    return false;
}

static bool live_lease(mc_world_runtime *world, mc_chunk_state *state, mc_world_chunk **out,
                       mc_live_tile_error *error)
{
    mc_world_chunk *chunk = mc_world_store_chunk_object(world->fields.w, state);
    if (!mc_world_chunk_source_enabled(chunk)) {
        live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Tile actual Store Chunk owner");
        return false;
    }
    mc_world_storage_error cause = {0};
    if (mc_world_store_lease_chunk(world->fields.w, state, &cause) != MC_WORLD_STORAGE_OK) {
        live_cause(error, cause);
        return false;
    }
    *out = chunk;
    return true;
}

static bool live_chunk_install_held(mc_world_chunk *chunk, int32_t x, int32_t y, int32_t z,
                                    mc_tile *tile, size_t storage_ceiling,
                                    mc_live_tile_error *error)
{
    /* Native map admission precedes the source receiver fields. */
    if (!mc_live_tiles_map_valid(chunk) ||
        (chunk->l->backing == NULL && chunk->l->release_backing != NULL)) {
        live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Tile install Map ownership");
        return false;
    }
    error->chunk_bytecode = 18;
    if (tile == NULL) {
        live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Tile install World receiver");
        return false;
    }
    mc_tile_status status = mc_tile_bind_world(tile, chunk->d);
    if (status != MC_TILE_OK) {
        live_tile_error(error, status, "Tile World binding");
        return false;
    }
    tile->x = mc_java_i32_add(mc_java_i32_mul(chunk->constructor_x, 16), x); /*32*/
    tile->y = y;                                                             /*38*/
    tile->z = mc_java_i32_add(mc_java_i32_mul(chunk->constructor_z, 16), z); /*52*/
    int32_t block_id;
    error->chunk_bytecode = 59;
    if (!live_chunk_block(chunk, x, y, z, &block_id, error))
        return false;
    bool accepts = false;
    if (block_id != 0) {
        error->chunk_bytecode = 65;
        mc_registry_block_m_view array;
        mc_registry_error cause = live_empty_registry();
        if (mc_registry_block_m_get(mc_world_runtime_registry(chunk->d), &array, &cause) !=
            MC_REGISTRY_OK) {
            return live_registry_cause(chunk->d, error, &cause, "Tile install Block.m active use");
        }
        error->chunk_bytecode = 72;
        if (!live_chunk_block(chunk, x, y, z, &block_id, error))
            return false;
        error->chunk_bytecode = 75;
        const mc_block *block = NULL;
        if (mc_registry_block_m_at(&array, block_id, &block, &cause) != MC_REGISTRY_OK) {
            return live_registry_cause(chunk->d, error, &cause,
                                       "Tile install captured Block.m element");
        }
        error->chunk_bytecode = 76;
        accepts = live_tile_block_kind(block); /* instanceof accepts NULL. */
    }
    if (!accepts) {
        error->chunk_bytecode = 87;
        static const char diagnostic[] =
            "Attempted to place a tile entity where there was no entity tile!";
        mc_text_stdout stream;
        bool io_failed = false;
        mc_text_stdout_status printed = mc_text_stdout_capture(&stream);
        if (printed == MC_TEXT_STDOUT_OK)
            printed =
                mc_text_stdout_println(&stream, diagnostic, sizeof(diagnostic) - 1u, &io_failed);
        if (printed != MC_TEXT_STDOUT_OK) {
            live_fail(error, MC_WORLD_STORAGE_IO_ERROR, "Tile install diagnostic native output");
            return false;
        }
        tile->classes->stdout_error |= io_failed; /* PrintStream suppresses underlying I/O. */
        return true; /* Actual return90, with earlier fields retained. */
    }
    error->chunk_bytecode = 93;
    tile->invalid = false;
    error->chunk_bytecode = 104;
    return live_map_put(chunk, x, y, z, tile, storage_ceiling, error);
}

static bool live_world_set_held(mc_world_runtime *world, int32_t x, int32_t y, int32_t z,
                                mc_tile *tile, mc_live_tile_error *error)
{
    error->bytecode = 2;
    if (tile == NULL) {
        live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Tile World invalid receiver");
        return false;
    }
    if (tile->invalid)
        return true; /* Ordinary return93. */
    if (world->fields.L) {
        tile->x = x; /*18*/
        tile->y = y; /*24*/
        tile->z = z; /*30*/
        error->bytecode = 39;
        return live_list_add(world, true, tile, error);
    }
    error->bytecode = 54;
    if (!live_list_add(world, false, tile, error))
        return false;
    error->bytecode = 67;
    mc_chunk_state *state = NULL;
    if (!live_acquire(world, mc_java_i32_shr(x, 4), mc_java_i32_shr(z, 4), &state, error))
        return false;
    if (state == NULL || state->blank)
        return true; /* NULL skips; blank's local install is actual return0. */
    error->bytecode = 90;
    mc_world_chunk *chunk;
    if (!live_lease(world, state, &chunk, error))
        return false;
    const bool completed = live_chunk_install_held(chunk, x & 15, y, z & 15, tile, 0, error);
    (void)mc_world_store_release_chunk(world->fields.w, state, NULL);
    return completed;
}

static bool live_block_added_held(mc_world_runtime *world, const mc_block *block, int32_t x,
                                  int32_t y, int32_t z, mc_live_tile_error *error)
{
    error->bytecode = 6; /* Proved uu/na.c base body returns. */
    if (block == NULL) {
        live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Tile Block added receiver");
        return false;
    }
    error->bytecode = 15;
    mc_tile_kind kind;
    if (block->kind == MC_BLOCK_E_E)
        kind = MC_TILE_CHEST;
    else if (block->kind == MC_BLOCK_DD_BZ)
        kind = MC_TILE_SPAWNER;
    else {
        live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Tile concrete Block factory");
        return false;
    }
    mc_tile *tile = NULL;
    mc_tile_error detail = live_empty_error().tile;
    mc_tile_status status = mc_tile_allocate(world, kind, &tile);
    if (status == MC_TILE_OK)
        status = mc_tile_construct_detailed(tile, &detail);
    if (status != MC_TILE_OK) {
        error->tile = detail;
        live_tile_error(error, status, "Tile Block concrete constructor");
        if (world->classes->tiles != NULL && world->classes->tiles->phase == 3) {
            error->initializer_bytecode = world->classes->tiles->initializer_pc;
            error->registry = world->classes->tiles->first_registry_failure;
        }
        mc_tile_release(tile);
        return false;
    }
    error->bytecode = 18;
    const bool completed = live_world_set_held(world, x, y, z, tile, error);
    if (!completed)
        error->caller_bytecode = 18;
    mc_tile_release(tile); /* Only factory's private temporary. */
    return completed;
}

static bool live_chunk_get_held(mc_world_chunk *chunk, int32_t x, int32_t y, int32_t z,
                                mc_tile **out, mc_live_tile_error *error)
{
    error->chunk_bytecode = 18;
    if (!mc_live_tiles_map_valid(chunk) ||
        (chunk->l->backing == NULL && chunk->l->release_backing != NULL)) {
        live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Tile lookup Map ownership");
        return false;
    }
    live_map_node *node = live_map_find(chunk->l->backing, x, y, z, live_key_hash(x, y, z));
    mc_tile *tile = node == NULL ? NULL : node->value;
    if (tile == NULL) {
        int32_t id;
        error->chunk_bytecode = 37;
        if (!live_chunk_block(chunk, x, y, z, &id, error))
            return false;
        error->chunk_bytecode = 42;
        mc_registry_block_p_view flags;
        mc_registry_error cause = live_empty_registry();
        if (mc_registry_block_p_get(mc_world_runtime_registry(chunk->d), &flags, &cause) !=
            MC_REGISTRY_OK) {
            return live_registry_cause(chunk->d, error, &cause, "Tile lookup Block.p active use");
        }
        error->chunk_bytecode = 47;
        bool has_tile;
        if (mc_registry_block_p_at(&flags, id, &has_tile, &cause) != MC_REGISTRY_OK) {
            return live_registry_cause(chunk->d, error, &cause,
                                       "Tile lookup captured Block.p element");
        }
        if (!has_tile) {
            *out = NULL;
            return true;
        }
        error->chunk_bytecode = 53;
        mc_registry_block_m_view array;
        if (mc_registry_block_m_get(mc_world_runtime_registry(chunk->d), &array, &cause) !=
            MC_REGISTRY_OK) {
            return live_registry_cause(chunk->d, error, &cause, "Tile lookup Block.m active use");
        }
        error->chunk_bytecode = 58;
        const mc_block *block = NULL;
        if (mc_registry_block_m_at(&array, id, &block, &cause) != MC_REGISTRY_OK) {
            return live_registry_cause(chunk->d, error, &cause,
                                       "Tile lookup captured Block.m element");
        }
        error->chunk_bytecode = 59;
        if (block != NULL && !live_tile_block_kind(block)) {
            live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "Tile lookup Block tile checkcast");
            return false;
        }
        error->chunk_bytecode = 89;
        const int32_t wx = mc_java_i32_add(mc_java_i32_mul(chunk->constructor_x, 16), x);
        const int32_t wz = mc_java_i32_add(mc_java_i32_mul(chunk->constructor_z, 16), z);
        if (block == NULL) {
            live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "Tile lookup tile-block invocation receiver");
            return false; /* NULL passed checkcast59; callee base6 is NOT entered. */
        }
        if (!live_block_added_held(chunk->d, block, wx, y, wz, error))
            return false;
        error->chunk_bytecode = 98;
        if (!mc_live_tiles_map_valid(chunk)) {
            live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "Tile lookup reread Map ownership");
            return false;
        }
        node = live_map_find(chunk->l->backing, x, y, z, live_key_hash(x, y, z));
        tile = node == NULL ? NULL : node->value;
    }
    error->chunk_bytecode = 115;
    if (tile != NULL && tile->invalid) {
        error->chunk_bytecode = 127;
        mc_tile_release(live_map_remove(chunk, x, y, z));
        *out = NULL;
        return true;
    }
    if (tile != NULL) {
        mc_tile_status held = mc_tile_retain(tile);
        if (held != MC_TILE_OK) {
            live_tile_error(error, held, "Tile lookup result retain");
            return false;
        }
    }
    *out = tile;
    return true;
}

static bool live_world_get_held(mc_world_runtime *world, int32_t x, int32_t y, int32_t z,
                                mc_tile **out, mc_live_tile_error *error)
{
    error->bytecode = 7;
    mc_chunk_state *state = NULL;
    if (!live_acquire(world, mc_java_i32_shr(x, 4), mc_java_i32_shr(z, 4), &state, error))
        return false;
    if (state == NULL || state->blank) {
        *out = NULL;
        return true;
    }
    error->bytecode = 28;
    mc_world_chunk *chunk;
    if (!live_lease(world, state, &chunk, error))
        return false;
    const bool completed = live_chunk_get_held(chunk, x & 15, y, z & 15, out, error);
    (void)mc_world_store_release_chunk(world->fields.w, state, NULL);
    return completed;
}

static bool live_chunk_remove_held(mc_world_chunk *chunk, int32_t x, int32_t y, int32_t z,
                                   mc_live_tile_error *error)
{
    error->chunk_bytecode = 16;
    if (!mc_live_tiles_map_valid(chunk)) {
        live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Tile removal Map ownership");
        return false;
    }
    if (!chunk->state->active)
        return true;
    error->chunk_bytecode = 25;
    mc_tile *tile = live_map_remove(chunk, x, y, z);
    if (tile != NULL) {
        error->chunk_bytecode = 42;
        tile->invalid = true;
        mc_tile_release(tile);
    }
    return true;
}

mc_world_storage_status mc_live_tiles_get(mc_world_runtime *world, int32_t x, int32_t y, int32_t z,
                                          mc_tile **out, mc_live_tile_error *error)
{
    mc_live_tile_error frame = live_empty_error();
    if (out == NULL) {
        live_fail(&frame, MC_WORLD_STORAGE_INVALID_ARGUMENT, "Tile lookup output");
        return live_finish(NULL, frame, error);
    }
    if (!live_enter(world, &frame))
        return live_finish(NULL, frame, error);
    mc_tile *tile = NULL;
    if (live_world_get_held(world, x, y, z, &tile, &frame))
        *out = tile;
    mc_world_runtime_call_leave(world);
    return live_finish(world, frame, error);
}

mc_world_storage_status mc_live_tiles_set(mc_world_runtime *world, int32_t x, int32_t y, int32_t z,
                                          mc_tile *tile, mc_live_tile_error *error)
{
    mc_live_tile_error frame = live_empty_error();
    if (!live_enter(world, &frame))
        return live_finish(NULL, frame, error);
    if (tile != NULL && tile->classes->parent != world->classes) {
        live_fail(&frame, MC_WORLD_STORAGE_INVALID_ARGUMENT, "Tile application class owner");
        mc_world_runtime_call_leave(world);
        return live_finish(NULL, frame, error);
    }
    bool retained = false;
    if (tile != NULL) {
        mc_tile_status held = mc_tile_retain(tile);
        if (held != MC_TILE_OK)
            live_tile_error(&frame, held, "Tile set temporary retain");
        else
            retained = true;
    }
    if (frame.status == MC_WORLD_STORAGE_OK)
        (void)live_world_set_held(world, x, y, z, tile, &frame);
    if (retained)
        mc_tile_release(tile);
    mc_world_runtime_call_leave(world);
    return live_finish(world, frame, error);
}

mc_world_storage_status mc_live_tiles_remove(mc_world_runtime *world, int32_t x, int32_t y,
                                             int32_t z, mc_live_tile_error *error)
{
    mc_live_tile_error frame = live_empty_error();
    if (!live_enter(world, &frame))
        return live_finish(NULL, frame, error);
    mc_tile *tile = NULL;
    frame.bytecode = 4;
    frame.remove_bytecode = 4;
    if (live_world_get_held(world, x, y, z, &tile, &frame)) {
        if (tile != NULL && world->fields.L) {
            frame.bytecode = 23;
            frame.remove_bytecode = 23;
            tile->invalid = true;
        } else {
            frame.bytecode = 40;
            frame.remove_bytecode = 40;
            bool removed = tile == NULL || live_list_remove_first(world, tile, &frame);
            if (removed) {
                frame.bytecode = 53;
                frame.remove_bytecode = 53;
                mc_chunk_state *state = NULL;
                if (live_acquire(world, mc_java_i32_shr(x, 4), mc_java_i32_shr(z, 4), &state,
                                 &frame) &&
                    state != NULL && !state->blank) {
                    frame.bytecode = 74;
                    frame.remove_bytecode = 74;
                    mc_world_chunk *chunk;
                    if (live_lease(world, state, &chunk, &frame)) {
                        (void)live_chunk_remove_held(chunk, x & 15, y, z & 15, &frame);
                        (void)mc_world_store_release_chunk(world->fields.w, state, NULL);
                    }
                }
            }
        }
    }
    mc_tile_release(tile);
    mc_world_runtime_call_leave(world);
    return live_finish(world, frame, error);
}

mc_world_storage_status mc_live_tiles_block_added(mc_world_runtime *world, const mc_block *block,
                                                  int32_t x, int32_t y, int32_t z,
                                                  mc_live_tile_error *error)
{
    mc_live_tile_error frame = live_empty_error();
    if (!live_enter(world, &frame))
        return live_finish(NULL, frame, error);
    (void)live_block_added_held(world, block, x, y, z, &frame);
    mc_world_runtime_call_leave(world);
    return live_finish(world, frame, error);
}
static bool live_list_add_map(mc_world_runtime *world, bool deferred, mc_world_chunk *chunk,
                              mc_live_tile_error *error)
{
    if (!live_list_valid(world, deferred) || !mc_live_tiles_map_valid(chunk)) {
        live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                  "Tile addAll actual collection owners");
        return false;
    }
    mc_world_reference_list *list = deferred ? world->fields.G : world->fields.c;
    mc_tile_list_ledger *ledger = &world->tile_membership[deferred ? 1u : 0u];
    if (list == NULL) {
        live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Tile addAll List receiver");
        return false;
    }
    const size_t incoming = chunk->l->count;
    if (incoming > (size_t)INT32_MAX - list->count) {
        live_fail(error, MC_WORLD_STORAGE_LIMIT, "Tile addAll size");
        return false;
    }
    if (incoming == 0) {
        ledger->mod_count = mc_java_i32_add(ledger->mod_count, 1);
        return true;
    }
    const size_t required = list->count + incoming;
    size_t capacity = list->capacity;
    if (capacity < required) {
        capacity = capacity == 0                       ? 4u
                   : capacity > (size_t)INT32_MAX / 2u ? (size_t)INT32_MAX
                                                       : capacity * 2u;
        if (capacity < required)
            capacity = required;
    }
    if (capacity > SIZE_MAX / (sizeof(void *) + sizeof(mc_tile *))) {
        live_fail(error, MC_WORLD_STORAGE_LIMIT, "Tile addAll byte overflow");
        return false;
    }
    /* Native separate buffers admit all source references before publication.
     * Charge old plus both complete new spans. No partial repeated add replaces
     * this actual single ArrayList.addAll operation. */
    const size_t bytes = capacity * (sizeof(void *) + sizeof(mc_tile *));
    const size_t used = world->construction.direct_allocation_bytes;
    const size_t limit = world->construction_limits.max_direct_allocation_bytes;
    if (used > limit || bytes > limit - used) {
        live_fail(error, MC_WORLD_STORAGE_LIMIT, "Tile addAll peak allocation budget");
        return false;
    }
    void **items = malloc(capacity * sizeof(*items));
    mc_tile **holds = malloc(capacity * sizeof(*holds));
    if (items == NULL || holds == NULL) {
        free(items);
        free(holds);
        live_fail(error, MC_WORLD_STORAGE_ALLOCATION_FAILED, "Tile addAll storage");
        return false;
    }
    if (list->count != 0) {
        memcpy(items, list->items, list->count * sizeof(*items));
        memcpy(holds, ledger->holds, list->count * sizeof(*holds));
    }
    size_t copied = 0;
    const mc_tile_map_backing *map = chunk->l->backing;
    for (uint32_t bucket = 0; bucket < map->capacity; ++bucket) {
        for (live_map_node *node = map->buckets[bucket]; node != NULL; node = node->next) {
            mc_tile_status held = mc_tile_retain(node->value);
            if (held != MC_TILE_OK) {
                for (size_t i = 0; i < copied; ++i)
                    mc_tile_release(holds[list->count + i]);
                free(items);
                free(holds);
                live_tile_error(error, held, "Tile addAll occurrence retain");
                return false;
            }
            items[list->count + copied] = node->value;
            holds[list->count + copied] = node->value;
            ++copied;
        }
    }
    const size_t old_bytes =
        list->capacity * sizeof(*list->items) + ledger->capacity * sizeof(*ledger->holds);
    free(list->items);
    free(ledger->holds);
    list->items = items;
    list->capacity = capacity;
    list->count = required;
    ledger->holds = holds;
    ledger->capacity = capacity;
    ledger->count = required;
    ledger->mod_count = mc_java_i32_add(ledger->mod_count, 1);
    world->construction.direct_allocation_bytes += bytes;
    world->construction.direct_allocation_bytes -= old_bytes;
    return true;
}

static bool live_chunk_entry(mc_world_chunk *chunk, mc_world_runtime **world,
                             mc_live_tile_error *error)
{
    if (chunk == NULL) {
        live_fail(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "Tile Chunk receiver");
        return false;
    }
    if (!mc_world_chunk_source_enabled(chunk)) {
        live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Tile Chunk source is disabled");
        return false;
    }
    *world = chunk->d;
    if (!live_enter(*world, error))
        return false;
    mc_world_chunk *leased;
    if (mc_world_store_chunk_object((*world)->fields.w, chunk->state) != chunk ||
        !live_lease(*world, chunk->state, &leased, error)) {
        if (error->status == MC_WORLD_STORAGE_OK)
            live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Tile Chunk Store identity");
        mc_world_runtime_call_leave(*world);
        return false;
    }
    return true;
}

static mc_world_storage_status live_chunk_finish(mc_world_chunk *chunk, mc_live_tile_error frame,
                                                 mc_live_tile_error *error)
{
    mc_world_runtime *world = chunk->d;
    (void)mc_world_store_release_chunk(world->fields.w, chunk->state, NULL);
    mc_world_runtime_call_leave(world);
    return live_finish(world, frame, error);
}

mc_world_storage_status mc_live_tiles_chunk_import(mc_world_chunk *chunk, mc_tile *tile,
                                                   mc_live_tile_error *error)
{
    mc_live_tile_error frame = live_empty_error();
    mc_world_runtime *world;
    if (!live_chunk_entry(chunk, &world, &frame))
        return live_finish(NULL, frame, error);
    if (tile == NULL)
        live_fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Tile import coordinate receiver");
    else {
        mc_tile_status held = mc_tile_retain(tile);
        if (held != MC_TILE_OK)
            live_tile_error(&frame, held, "Tile import temporary retain");
        else {
            const int32_t x = mc_java_i32_sub(tile->x, mc_java_i32_mul(chunk->constructor_x, 16));
            const int32_t y = tile->y;
            const int32_t z = mc_java_i32_sub(tile->z, mc_java_i32_mul(chunk->constructor_z, 16));
            frame.bytecode = 38;
            if (live_chunk_install_held(chunk, x, y, z, tile, 0, &frame) && chunk->state->active) {
                frame.bytecode = 56; /* Direct c append; deliberately no L branch. */
                (void)live_list_add(world, false, tile, &frame);
            }
            mc_tile_release(tile);
        }
    }
    return live_chunk_finish(chunk, frame, error);
}

mc_world_storage_status mc_live_tiles_chunk_register(mc_world_chunk *chunk,
                                                     mc_live_tile_error *error)
{
    mc_live_tile_error frame = live_empty_error();
    mc_world_runtime *world;
    if (!live_chunk_entry(chunk, &world, &frame))
        return live_finish(NULL, frame, error);
    frame.bytecode = 18;
    frame.chunk_bytecode = world->fields.L ? 12u : 26u;
    (void)live_list_add_map(world, world->fields.L, chunk, &frame);
    return live_chunk_finish(chunk, frame, error);
}

mc_world_storage_status mc_live_tiles_chunk_invalidate(mc_world_chunk *chunk,
                                                       mc_live_tile_error *error)
{
    mc_live_tile_error frame = live_empty_error();
    /* The caller holds the actual Chunk through this callback-free traversal.
     * Its source body never dereferences d, even when the Map is nonempty. */
    if (!mc_world_chunk_source_enabled(chunk)) {
        live_fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Tile detach native admission");
        return live_finish(NULL, frame, error);
    }
    frame.bytecode = 21;
    if (!mc_live_tiles_map_valid(chunk))
        live_fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Tile detach Map iterator");
    else if (chunk->l->backing != NULL) {
        mc_tile_map_backing *map = chunk->l->backing;
        for (uint32_t bucket = 0; bucket < map->capacity; ++bucket)
            for (live_map_node *node = map->buckets[bucket]; node != NULL; node = node->next) {
                frame.bytecode = 40;
                node->value->invalid = true; /* No clear, detach, or World release. */
            }
    }
    return live_finish(NULL, frame, error);
}

mc_world_storage_status mc_world_runtime_tile_dirty_detailed(mc_world_runtime *world, int32_t x,
                                                             int32_t y, int32_t z, mc_tile *tile,
                                                             mc_live_tile_error *error)
{
    mc_live_tile_error frame = live_empty_error();
    if (!live_enter(world, &frame))
        return live_finish(NULL, frame, error);
    bool present = false;
    frame.bytecode = 4;
    if (y >= 0 && y < 128) {
        mc_world_storage_error cause = {0};
        if (mc_world_runtime_provider_contains(world, mc_java_i32_shr(x, 4), mc_java_i32_shr(z, 4),
                                               &present, &cause) != MC_WORLD_STORAGE_OK)
            live_cause(&frame, cause);
    }
    if (frame.status == MC_WORLD_STORAGE_OK && present) {
        frame.bytecode = 13;
        mc_chunk_state *state = NULL;
        if (live_acquire(world, mc_java_i32_shr(x, 4), mc_java_i32_shr(z, 4), &state, &frame)) {
            frame.bytecode = 16;
            if (state == NULL)
                live_fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Tile dirty Chunk receiver");
            else if (!state->blank)
                state->dirty = true;
        }
    }
    /* The real World owner has no typed pm/jp observer registration yet.
     * Empty reaches size28/return66. A nonempty list preserves get42/cast47/
     * invoke55; no invented successful renderer/server observer is installed. */
    if (frame.status == MC_WORLD_STORAGE_OK) {
        frame.bytecode = 28;
        const mc_world_reference_list *list = world->fields.u;
        if (!live_list_shape(list))
            live_fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Tile dirty observer size");
        else if (list->count != 0) {
            frame.observer_bytecode = 42;
            list = world->fields.u;
            if (!live_list_shape(list) || list->count == 0)
                live_fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Tile dirty observer get");
            else if (list->items[0] == NULL) {
                frame.observer_bytecode = 55;
                live_fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                          "Tile dirty observer invocation receiver");
            } else {
                frame.observer_bytecode = 47;
                live_fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                          "Tile dirty concrete observer owner/cast");
            }
        }
    }
    (void)tile; /* Same owner argument reaches invoke55 only with actual observer support. */
    mc_world_runtime_call_leave(world);
    return live_finish(world, frame, error);
}

bool mc_world_runtime_tile_dirty(mc_world_runtime *world, int32_t x, int32_t y, int32_t z,
                                 mc_tile *tile)
{
    return mc_world_runtime_tile_dirty_detailed(world, x, y, z, tile, NULL) == MC_WORLD_STORAGE_OK;
}

mc_world_storage_status mc_live_tiles_drain_deferred(mc_world_runtime *world,
                                                     mc_live_tile_error *error)
{
    mc_live_tile_error frame = live_empty_error();
    if (!live_enter(world, &frame))
        return live_finish(NULL, frame, error);
    frame.bytecode = 461;
    if (!live_list_valid(world, true) || world->fields.G == NULL)
        live_fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Tile deferred isEmpty receiver");
    else if (world->fields.G->count != 0) {
        const int32_t expected = world->tile_membership[1].mod_count;
        size_t cursor = 0;
        while (frame.status == MC_WORLD_STORAGE_OK) {
            mc_world_reference_list *pending = world->fields.G;
            if (!live_list_valid(world, true) || pending == NULL) {
                live_fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                          "Tile deferred iterator owner");
                break;
            }
            if (cursor == pending->count)
                break;
            frame.bytecode = 489;
            if (world->tile_membership[1].mod_count != expected || cursor > pending->count) {
                live_fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                          "Tile deferred iterator concurrent modification");
                break;
            }
            mc_tile *tile = world->tile_membership[1].holds[cursor++];
            mc_tile_status held = mc_tile_retain(tile);
            if (held != MC_TILE_OK) {
                live_tile_error(&frame, held, "Tile deferred temporary retain");
                break;
            }
            frame.bytecode = 499;
            if (!tile->invalid) {
                frame.bytecode = 510;
                if (!live_list_valid(world, false) || world->fields.c == NULL)
                    live_fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                              "Tile deferred active contains receiver");
                else {
                    bool contains = false;
                    for (size_t i = 0; i < world->fields.c->count; ++i)
                        contains |= world->tile_membership[0].holds[i] == tile;
                    frame.bytecode = 523;
                    bool admitted = contains || live_list_add(world, false, tile, &frame);
                    if (admitted) {
                        frame.bytecode = 542;
                        mc_chunk_state *state = NULL;
                        if (live_acquire(world, mc_java_i32_shr(tile->x, 4),
                                         mc_java_i32_shr(tile->z, 4), &state, &frame)) {
                            if (state != NULL && !state->blank) {
                                frame.bytecode = 573;
                                mc_world_chunk *chunk;
                                if (live_lease(world, state, &chunk, &frame)) {
                                    (void)live_chunk_install_held(chunk, tile->x & 15, tile->y,
                                                                  tile->z & 15, tile, 0, &frame);
                                    (void)mc_world_store_release_chunk(world->fields.w, state,
                                                                       NULL);
                                }
                            }
                            if (frame.status == MC_WORLD_STORAGE_OK) {
                                frame.bytecode = 589;
                                mc_world_mutation_error observed = mc_world_mutation_empty_error();
                                if (!mc_world_mutation_observe_cell(world, tile->x, tile->y,
                                                                    tile->z, &observed)) {
                                    frame.observer_bytecode = observed.observer_bytecode;
                                    live_cause(&frame, observed.cause);
                                }
                            }
                        }
                    }
                }
            }
            mc_tile_release(tile);
        }
        if (frame.status == MC_WORLD_STORAGE_OK) {
            frame.bytecode = 599;
            mc_world_reference_list *pending = world->fields.G;
            mc_tile_list_ledger *ledger = &world->tile_membership[1];
            const size_t count = ledger->count;
            pending->count = ledger->count = 0;
            ledger->mod_count = mc_java_i32_add(ledger->mod_count, 1);
            for (size_t i = 0; i < count; ++i) {
                mc_tile *tile = ledger->holds[i];
                ledger->holds[i] = NULL;
                pending->items[i] = NULL;
                mc_tile_release(tile);
            }
        }
    }
    mc_world_runtime_call_leave(world);
    return live_finish(world, frame, error);
}
static mc_world_storage_status live_tiles_factory_scoped(mc_world_runtime *world,
                                                         const mc_nbt_tag *compound,
                                                         const mc_nbt_limits *limits, bool preserve,
                                                         mc_tile **out, mc_live_tile_error *error,
                                                         const mc_world_chunk *scope_chunk)
{
    mc_live_tile_error frame = live_empty_error();
    if (out == NULL) {
        live_fail(&frame, MC_WORLD_STORAGE_INVALID_ARGUMENT, "Tile factory output");
        return live_finish(NULL, frame, error);
    }
    if (!live_enter(world, &frame))
        return live_finish(NULL, frame, error);
    mc_tile_classes *classes = NULL;
    mc_tile_status active = live_classes_get(world, &classes);
    if (active != MC_TILE_OK) {
        live_tile_error(&frame, active, "Tile factory class active use");
        if (world->classes->tiles != NULL) {
            if (world->classes->tiles->phase == 3) {
                frame.initializer_bytecode = world->classes->tiles->initializer_pc;
                frame.registry = world->classes->tiles->first_registry_failure;
            }
        }
    } else {
        frame.bytecode = 8;
        const mc_nbt_tag *id = compound == NULL || compound->type != MC_NBT_COMPOUND
                                   ? NULL
                                   : mc_nbt_compound_get(compound, tile_id_name, 2);
        if (compound == NULL || compound->type != MC_NBT_COMPOUND ||
            (id != NULL && id->type != MC_NBT_STRING)) {
            /* The original catches this Java getter exception, prints its
             * stack at36, then re-reads ID70 on the skip path. Native failures
             * are not fabricated Java Exception objects or silently skipped. */
            frame.bytecode = 36;
            frame.tile.field = "id";
            frame.tile.status = MC_TILE_INVALID_FIELD;
            live_fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "Tile factory caught-ID exception stack diagnostic");
        } else {
            mc_nbt_string name = id == NULL ? (mc_nbt_string){NULL, 0} : id->value.string;
            const live_class_entry *entry = NULL;
            frame.bytecode = 11;
            for (size_t i = 0; i < classes->name_count; ++i)
                if (tile_names_equal(name, classes->by_name[i].name->text)) {
                    entry = &classes->by_name[i];
                    break;
                }
            if (entry == NULL) {
                /* Actual archives pin the TRAILING SPACE, which javap elides:
                 * UTF8 client#145/server#137, String#10, bytes28. */
                static const char prefix[] = "Skipping TileEntity with id ";
                char diagnostic[64];
                mc_text_stdout stream;
                frame.bytecode = 52;
                mc_text_stdout_status captured = mc_text_stdout_capture(&stream);
                bool representable = name.length <= sizeof(diagnostic) - (sizeof(prefix) - 1u);
                if (representable) {
                    memcpy(diagnostic, prefix, sizeof(prefix) - 1u);
                    for (size_t i = 0; i < name.length; ++i) {
                        if (name.units[i] > 0x7fu) {
                            representable = false;
                            break;
                        }
                        diagnostic[sizeof(prefix) - 1u + i] = (char)name.units[i];
                    }
                }
                frame.bytecode = 79;
                if (!representable)
                    live_fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                              "Tile factory extended PrintStream encoding/length");
                else if (captured != MC_TEXT_STDOUT_OK)
                    live_fail(&frame, MC_WORLD_STORAGE_IO_ERROR, "Tile factory stdout capture");
                else {
                    bool io_failed = false;
                    mc_text_stdout_status printed = mc_text_stdout_println(
                        &stream, diagnostic, sizeof(prefix) - 1u + name.length, &io_failed);
                    if (printed != MC_TEXT_STDOUT_OK)
                        live_fail(&frame, MC_WORLD_STORAGE_IO_ERROR,
                                  "Tile factory diagnostic native output");
                    else {
                        classes->stdout_error |= io_failed;
                        *out = NULL; /* Ordinary unknown-ID return83. */
                    }
                }
            } else if (entry->kind != LIVE_CHEST && entry->kind != LIVE_SPAWNER) {
                frame.bytecode = 25;
                live_fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                          "Tile registered concrete constructor");
            } else {
                const mc_tile_kind kind =
                    entry->kind == LIVE_CHEST ? MC_TILE_CHEST : MC_TILE_SPAWNER;
                mc_tile *tile = NULL;
                mc_tile_error detail = live_empty_error().tile;
                mc_tile_status status;
                frame.bytecode = 25;
                if (preserve) {
                    /* Shared ordered semantic body captures real winning slots,
                     * then adds native independent provenance before publication.
                     * It does not rerun semantics or use a guessed association. */
                    status = tile_import_storage_scoped(world, kind, compound, limits, &tile,
                                                        &detail, scope_chunk);
                } else {
                    status = mc_tile_allocate(world, kind, &tile);
                    if (status == MC_TILE_OK)
                        status = mc_tile_construct_detailed(tile, &detail);
                    if (status == MC_TILE_OK) {
                        frame.bytecode = 46; /* Outside factory Exception catch2..32. */
                        status = mc_tile_read_nbt(tile, compound, &detail);
                    }
                }
                if (status != MC_TILE_OK) {
                    frame.tile = detail;
                    if (detail.source_bytecode != 0)
                        frame.bytecode = detail.source_bytecode;
                    live_tile_error(&frame, status, "Tile factory constructor/semantic read");
                    mc_tile_release(tile);
                } else
                    *out = tile;
            }
        }
    }
    mc_world_runtime_call_leave(world);
    return live_finish(world, frame, error);
}
mc_world_storage_status mc_live_tiles_factory(mc_world_runtime *world, const mc_nbt_tag *compound,
                                              const mc_nbt_limits *limits, bool preserve,
                                              mc_tile **out, mc_live_tile_error *error)
{
    return live_tiles_factory_scoped(world, compound, limits, preserve, out, error, NULL);
}
static bool live_drop_entity_error(mc_live_tile_error *error, mc_entity_status status,
                                   const char *operation)
{
    if (error->entity.status != status)
        error->entity = (mc_entity_error){.status = status, .operation = operation};
    live_fail(error,
              status == MC_ENTITY_LIMIT               ? MC_WORLD_STORAGE_LIMIT
              : status == MC_ENTITY_ALLOCATION_FAILED ? MC_WORLD_STORAGE_ALLOCATION_FAILED
                                                      : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
              operation);
    return false;
}

static bool live_chest_drops_held(mc_world_runtime *world, const mc_block *block, int32_t x,
                                  int32_t y, int32_t z, mc_live_tile_error *error)
{
    mc_tile *chest = NULL;
    error->bytecode = 5;
    if (!live_world_get_held(world, x, y, z, &chest, error))
        return false;
    error->bytecode = 8;
    if (chest != NULL && chest->kind != MC_TILE_CHEST) {
        mc_tile_release(chest);
        live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Chest removal Tile checkcast");
        return false;
    }
    bool completed = true;
    for (int32_t slot = 0; completed; slot = mc_java_i32_add(slot, 1)) {
        error->bytecode = 20;
        int32_t size;
        mc_tile_status tile_status_code = mc_tile_chest_size(chest, &size);
        if (tile_status_code != MC_TILE_OK) {
            live_tile_error(error, tile_status_code, "Chest removal size receiver");
            completed = false;
            break;
        }
        if (slot >= size)
            break;
        error->bytecode = 32;
        mc_item_stack *stack = NULL;
        tile_status_code = mc_tile_chest_get(chest, slot, &stack);
        if (tile_status_code != MC_TILE_OK) {
            live_tile_error(error, tile_status_code, "Chest removal slot getter");
            completed = false;
            break;
        }
        if (stack == NULL)
            continue;
        error->bytecode = 48;
        mc_java_random *random = block->subtype.e.a;
        if (random == NULL || (_mm_getcsr() & 0xffc0u) != 0x1f80u) {
            live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "Chest private Random/strict float call");
            mc_item_stack_release(stack);
            completed = false;
            break;
        }
        volatile float offset_x = mc_java_random_next_float(random) * 0.8f + 0.1f;
        error->bytecode = 63;
        random = block->subtype.e.a;
        if (random == NULL) {
            live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Chest second private Random");
            mc_item_stack_release(stack);
            completed = false;
            break;
        }
        volatile float offset_y = mc_java_random_next_float(random) * 0.8f + 0.1f;
        error->bytecode = 78;
        random = block->subtype.e.a;
        if (random == NULL) {
            live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Chest third private Random");
            mc_item_stack_release(stack);
            completed = false;
            break;
        }
        volatile float offset_z = mc_java_random_next_float(random) * 0.8f + 0.1f;
        while (completed) {
            error->bytecode = 91;
            mc_item_stack_fields fields;
            mc_item_stack_status item = mc_item_stack_fields_get(stack, &fields);
            if (item != MC_ITEM_STACK_OK) {
                live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Chest Stack count field");
                completed = false;
                break;
            }
            if (fields.count <= 0)
                break;
            error->bytecode = 103;
            random = block->subtype.e.a;
            int32_t count;
            if (random == NULL || !mc_java_random_next_bounded(random, 21, &count)) {
                live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Chest private count Random");
                completed = false;
                break;
            }
            count = mc_java_i32_add(count, 10);
            item = mc_item_stack_fields_get(stack, &fields);
            if (item != MC_ITEM_STACK_OK) {
                live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Chest cap count field");
                completed = false;
                break;
            }
            if (count > fields.count)
                count = fields.count;
            error->bytecode = 137;
            item = mc_item_stack_count_set(stack, mc_java_i32_sub(fields.count, count));
            if (item != MC_ITEM_STACK_OK) {
                live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Chest drop count decrement");
                completed = false;
                break;
            }
            error->bytecode = 140;
            mc_entity *entity = NULL;
            mc_entity_status entity_status =
                mc_item_entity_allocate(mc_world_runtime_entities(world), &entity, &error->entity);
            if (entity_status != MC_ENTITY_OK) {
                completed = live_drop_entity_error(error, entity_status, "Chest raw ItemEntity");
                break;
            }
            /* Source i2f and fadd precede widening. The ItemEntity allocation
             * already happened before these constructor argument operations. */
            volatile float px = (float)x + offset_x;
            volatile float py = (float)y + offset_y;
            volatile float pz = (float)z + offset_z;
            error->bytecode = 164;
            mc_item_stack *drop = NULL;
            item = mc_item_stack_allocate(&drop);
            if (item == MC_ITEM_STACK_OK) {
                error->bytecode = 170;
                item = mc_item_stack_fields_get(stack, &fields);
                if (item == MC_ITEM_STACK_OK) {
                    error->bytecode = 180;
                    item = mc_item_stack_construct(drop, fields.id, count, fields.damage);
                }
            }
            if (item != MC_ITEM_STACK_OK) {
                error->tile.item.status = item;
                live_fail(error,
                          item == MC_ITEM_STACK_LIMIT ? MC_WORLD_STORAGE_LIMIT
                          : item == MC_ITEM_STACK_ALLOCATION_FAILED
                              ? MC_WORLD_STORAGE_ALLOCATION_FAILED
                              : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                          "Chest numeric drop Stack");
                mc_item_stack_release(drop);
                mc_entity_release(entity);
                completed = false;
                break;
            }
            error->bytecode = 183;
            entity_status = mc_item_entity_construct(entity, world, (double)px, (double)py,
                                                     (double)pz, drop, &error->entity);
            mc_item_stack_release(drop);
            if (entity_status != MC_ENTITY_OK) {
                completed =
                    live_drop_entity_error(error, entity_status, "Chest ItemEntity constructor");
                mc_entity_release(entity);
                break;
            }
            static const unsigned int gaussian_pc[3] = {198, 215, 235};
            static const unsigned int store_pc[3] = {206, 226, 243};
            for (unsigned int axis = 0; axis < 3; ++axis) {
                error->bytecode = gaussian_pc[axis];
                random = block->subtype.e.a;
                if (random == NULL || (_mm_getcsr() & 0xffc0u) != 0x1f80u) {
                    live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                              "Chest private Gaussian/strict double call");
                    completed = false;
                    break;
                }
                volatile float velocity = (float)mc_java_random_next_gaussian(random) * 0.05f;
                if (axis == 1u)
                    velocity = velocity + 0.2f;
                error->bytecode = store_pc[axis];
                entity_status = mc_entity_set_velocity(entity, axis, (double)velocity);
                if (entity_status != MC_ENTITY_OK) {
                    completed =
                        live_drop_entity_error(error, entity_status, "Chest velocity store");
                    break;
                }
            }
            if (completed) {
                error->bytecode = 249;
                bool ordinary;
                if (mc_world_runtime_insert_entity(world, entity, &ordinary, &error->publication) !=
                    MC_WORLD_STORAGE_OK) {
                    live_cause(error, error->publication.cause);
                    completed = false;
                }
                /* Source pop252 discards both ordinary false and true. */
            }
            mc_entity_release(entity);
        }
        mc_item_stack_release(stack);
    }
    mc_tile_release(chest);
    return completed;
}

mc_world_storage_status mc_live_tiles_block_removed(mc_world_runtime *world, const mc_block *block,
                                                    int32_t x, int32_t y, int32_t z,
                                                    mc_live_tile_error *error)
{
    mc_live_tile_error frame = live_empty_error();
    if (!live_enter(world, &frame))
        return live_finish(NULL, frame, error);
    bool base = false;
    if (block == NULL)
        live_fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Tile removed Block receiver");
    else if (block->kind == MC_BLOCK_DD_BZ)
        base = true; /* Actual inherited rw/lb removed hook. */
    else if (block->kind == MC_BLOCK_E_E) {
        if (live_chest_drops_held(world, block, x, y, z, &frame)) {
            frame.bytecode = 268;
            base = true;
        }
    } else
        live_fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Tile concrete removed override");
    if (base) {
        /* rw/lb.b: proved base block return6, actual World remove14. This
         * nested wrapper retains its own World and exact fetched Tile alias. */
        frame.bytecode = 14;
        mc_live_tile_error nested = live_empty_error();
        if (mc_live_tiles_remove(world, x, y, z, &nested) != MC_WORLD_STORAGE_OK) {
            /* Preserve the COMPLETE child, including Tile/Registry/initializer/
             * Entity/publication data, plus the two actual enclosing calls. */
            frame = nested;
            frame.caller_bytecode = 14; /* rw/lb removed -> World remove */
            frame.hook_bytecode = block->kind == MC_BLOCK_E_E ? 268u : 0u;
        }
    }
    mc_world_runtime_call_leave(world);
    return live_finish(world, frame, error);
}

static const uint16_t live_records_name[] = {'T', 'i', 'l', 'e', 'E', 'n',
                                             't', 'i', 't', 'i', 'e', 's'};

static bool live_map_ensure_header(mc_world_chunk *chunk, size_t ceiling, mc_live_tile_error *error)
{
    if (!mc_live_tiles_map_valid(chunk) ||
        (chunk->l->backing == NULL && chunk->l->release_backing != NULL)) {
        live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Tile storage Map owner");
        return false;
    }
    if (chunk->l->backing != NULL)
        return true;
    size_t graph;
    if (!live_graph_measure(chunk, NULL, ceiling, &graph, error))
        return false;
    if (sizeof(mc_tile_map_backing) > ceiling - graph) {
        live_fail(error, MC_WORLD_STORAGE_LIMIT, "Tile storage Map header budget");
        return false;
    }
    mc_tile_map_backing *map = calloc(1, sizeof(*map));
    if (map == NULL) {
        live_fail(error, MC_WORLD_STORAGE_ALLOCATION_FAILED, "Tile storage Map header");
        return false;
    }
    map->owner = chunk;
    map->allocation_bytes = sizeof(*map);
    chunk->l->backing = map;
    chunk->l->release_backing = mc_live_tiles_map_release;
    chunk->l->backing_bytes = mc_live_tiles_owned_map_bytes;
    chunk->allocation_bytes += sizeof(*map);
    return true;
}

mc_world_storage_status mc_live_tiles_load_records(mc_world_chunk *chunk, const mc_nbt_tag *records,
                                                   const mc_nbt_limits *requested,
                                                   mc_live_tile_error *error)
{
    mc_live_tile_error frame = live_empty_error();
    mc_world_runtime *world;
    if (!live_chunk_entry(chunk, &world, &frame))
        return live_finish(NULL, frame, error);
    mc_nbt_limits limits;
    if (!tile_limits(requested, &limits))
        live_fail(&frame, MC_WORLD_STORAGE_INVALID_ARGUMENT, "Tile storage import limits");
    else if (!live_map_ensure_header(chunk,
                                     limits.max_allocation_bytes < chunk->max_allocation_bytes
                                         ? limits.max_allocation_bytes
                                         : chunk->max_allocation_bytes,
                                     &frame)) {
        /* Exact native cause already retained. */
    } else {
        mc_tile_map_backing *map = chunk->l->backing;
        if (map->journal_started)
            live_fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "Tile storage import cannot retry same Chunk");
        else {
            map->journal_started = true; /* Incomplete source attempt blocks writes. */
            frame.bytecode = 301;
            if (records != NULL && records->type != MC_NBT_LIST)
                live_fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                          "TileEntities list getter type");
            else {
                const size_t count = records == NULL ? 0 : records->value.list.length;
                const size_t ceiling = limits.max_allocation_bytes < chunk->max_allocation_bytes
                                           ? limits.max_allocation_bytes
                                           : chunk->max_allocation_bytes;
                size_t graph = 0;
                if (count > limits.max_elements || count > (size_t)INT32_MAX ||
                    count > SIZE_MAX / sizeof(bool))
                    live_fail(&frame, MC_WORLD_STORAGE_LIMIT, "Tile import journal allocation");
                else if (!live_graph_measure(chunk, NULL, ceiling, &graph, &frame)) {
                    /* Complete old graph measured with no scratch request. */
                } else if (count * sizeof(bool) > ceiling - graph)
                    live_fail(&frame, MC_WORLD_STORAGE_LIMIT, "Tile import complete journal peak");
                else {
                    bool *consumed = count == 0 ? NULL : calloc(count, sizeof(*consumed));
                    if (count != 0 && consumed == NULL)
                        live_fail(&frame, MC_WORLD_STORAGE_ALLOCATION_FAILED,
                                  "Tile import journal allocation");
                    else {
                        map->consumed = consumed;
                        map->record_count = count;
                        chunk->allocation_bytes += count * sizeof(*consumed);
                        map->allocation_bytes += count * sizeof(*consumed);
                        for (size_t i = 0; i < count && frame.status == MC_WORLD_STORAGE_OK; ++i) {
                            frame.bytecode = 331;
                            const mc_nbt_tag *record = records->value.list.items[i];
                            if (record == NULL || record->type != MC_NBT_COMPOUND) {
                                live_fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                          "TileEntities compound checkcast");
                                break;
                            }
                            frame.bytecode = 338;
                            mc_tile *tile = NULL;
                            mc_live_tile_error nested = live_empty_error();
                            size_t previous_graph;
                            if (!live_graph_measure(chunk, NULL, ceiling, &previous_graph, &frame))
                                break;
                            mc_nbt_limits constructor_limits = limits;
                            constructor_limits.max_allocation_bytes = ceiling;
                            if (live_tiles_factory_scoped(world, record, &constructor_limits, true,
                                                          &tile, &nested,
                                                          chunk) != MC_WORLD_STORAGE_OK) {
                                frame.tile = nested.tile;
                                frame.registry = nested.registry;
                                frame.initializer_bytecode = nested.initializer_bytecode;
                                frame.chunk_bytecode = nested.bytecode;
                                live_cause(&frame, nested.cause);
                                break;
                            }
                            if (tile == NULL)
                                continue; /* Actual unmapped-ID skip; retain opaque record. */
                            const int32_t lx =
                                mc_java_i32_sub(tile->x, mc_java_i32_mul(chunk->constructor_x, 16));
                            const int32_t ly = tile->y;
                            const int32_t lz =
                                mc_java_i32_sub(tile->z, mc_java_i32_mul(chunk->constructor_z, 16));
                            frame.bytecode = 352;
                            bool installed_ok =
                                live_chunk_install_held(chunk, lx, ly, lz, tile, ceiling, &frame);
                            if (installed_ok && chunk->state->active) {
                                frame.chunk_bytecode = 56;
                                installed_ok = live_list_add(world, false, tile, &frame);
                            }
                            if (installed_ok) {
                                live_map_node *installed =
                                    live_map_find(map, lx, ly, lz, live_key_hash(lx, ly, lz));
                                /* Witness at this exact publication, never a later
                                 * search by matching coordinates/type/payload. */
                                consumed[i] = installed != NULL && installed->value == tile;
                            }
                            mc_tile_release(tile);
                        }
                        if (frame.status == MC_WORLD_STORAGE_OK) {
                            mc_nbt_tag list = {0}, root = {0};
                            if (records != NULL)
                                list = *records;
                            else {
                                list.type = MC_NBT_LIST;
                                list.value.list.element_type = MC_NBT_COMPOUND;
                            }
                            list.name = (mc_nbt_string){live_records_name, 12};
                            const mc_nbt_tag *child = &list;
                            root.type = MC_NBT_COMPOUND;
                            root.value.compound.items = &child;
                            root.value.compound.length = 1;
                            mc_nbt_limits budget = limits;
                            size_t current_graph;
                            if (live_graph_measure(chunk, NULL, ceiling, &current_graph, &frame)) {
                                budget.max_allocation_bytes = ceiling - current_graph;
                                mc_nbt_error nbt = {0};
                                mc_nbt_tree *journal = NULL;
                                mc_nbt_status copied =
                                    mc_nbt_tree_copy_compound(&root, &budget, &journal, &nbt);
                                if (copied != MC_NBT_OK) {
                                    frame.tile.nbt = nbt;
                                    live_fail(&frame,
                                              copied == MC_NBT_LIMIT ? MC_WORLD_STORAGE_LIMIT
                                              : copied == MC_NBT_ALLOCATION_FAILED
                                                  ? MC_WORLD_STORAGE_ALLOCATION_FAILED
                                                  : MC_WORLD_STORAGE_INVALID_DATA,
                                              "Tile import provenance copy");
                                } else {
                                    const size_t bytes = mc_nbt_tree_allocation_bytes(journal);
                                    if (chunk->allocation_bytes > chunk->max_allocation_bytes ||
                                        bytes >
                                            chunk->max_allocation_bytes - chunk->allocation_bytes) {
                                        mc_nbt_tree_free(journal);
                                        live_fail(&frame, MC_WORLD_STORAGE_LIMIT,
                                                  "Tile import retained provenance budget");
                                    } else {
                                        map->journal = journal;
                                        chunk->allocation_bytes += bytes;
                                        map->allocation_bytes += bytes;
                                        map->journal_complete = true;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return live_chunk_finish(chunk, frame, error);
}

typedef struct live_record_view {
    mc_tile *tile;
    mc_nbt_tag root, fields[6], slots[36];
    mc_item_stack_record_view items[36];
    const mc_item_stack *stacks[36];
    size_t counts[36], occupied, root_count, pointer_count, field_count;
} live_record_view;

static mc_tile_status live_record_plan(mc_tile *tile, live_record_view *view,
                                       const mc_nbt_limits *limits, mc_tile_error *error)
{
    mc_tile_status status = tile_ready(tile);
    if (status != MC_TILE_OK)
        return tile_status(error, status, NULL, SIZE_MAX);
    size_t checked;
    status = mc_tile_storage_bytes(tile, &checked);
    if (status != MC_TILE_OK)
        return tile_status(error, status, NULL, SIZE_MAX);
    view->tile = tile;
    status = tile_build_fields(tile, view->fields, &view->field_count, error);
    if (status != MC_TILE_OK)
        return status;
    status = tile_overlay_pointer_count(mc_tile_source(tile), view->fields, view->field_count,
                                        &view->root_count);
    if (status != MC_TILE_OK || view->root_count > limits->max_elements)
        return tile_status(error, MC_TILE_LIMIT, NULL, SIZE_MAX);
    view->pointer_count = view->root_count;
    if (tile->kind == MC_TILE_CHEST) {
        for (size_t slot = 0; slot < tile->state.chest.capacity; ++slot) {
            mc_item_stack *stack = tile->state.chest.slots[slot];
            if (stack == NULL)
                continue;
            const size_t n = view->occupied;
            view->slots[n].type = MC_NBT_BYTE;
            view->slots[n].name = (mc_nbt_string){tile_slot_name, 4};
            view->slots[n].value.byte_value = mc_java_i8_narrow((int32_t)slot);
            view->stacks[n] = stack;
            mc_item_stack_error item_error = {0};
            mc_item_stack_status item = mc_item_stack_record_pointer_count(
                stack, &view->slots[n], 1, &view->counts[n], &item_error);
            if (item != MC_ITEM_STACK_OK) {
                const mc_tile_status failure = tile_item_error(error, item, &item_error, n);
                if (error != NULL)
                    error->save_bytecode = 59;
                return failure;
            }
            if (view->counts[n] > limits->max_elements ||
                !tile_size_add(&view->pointer_count, view->counts[n]))
                return tile_status(error, MC_TILE_LIMIT, "Items[]", n);
            ++view->occupied;
        }
        if (!tile_size_add(&view->pointer_count, view->occupied))
            return tile_status(error, MC_TILE_LIMIT, "Items", SIZE_MAX);
    }
    return MC_TILE_OK;
}

static mc_tile_status live_record_prepare(live_record_view *view, const mc_nbt_tag **scratch,
                                          mc_tile_error *error)
{
    size_t offset = view->root_count + view->occupied;
    for (size_t i = 0; i < view->occupied; ++i) {
        mc_item_stack_error item_error = {0};
        mc_item_stack_status item =
            mc_item_stack_record_overlay(view->stacks[i], &view->slots[i], 1, scratch + offset,
                                         view->counts[i], &view->items[i], &item_error);
        if (item != MC_ITEM_STACK_OK) {
            const mc_tile_status failure = tile_item_error(error, item, &item_error, i);
            if (error != NULL)
                error->save_bytecode = 59;
            return failure;
        }
        scratch[view->root_count + i] = &view->items[i].root;
        offset += view->counts[i];
    }
    if (view->tile->kind == MC_TILE_CHEST) {
        view->fields[4].value.list.items = view->occupied == 0 ? NULL : scratch + view->root_count;
        view->fields[4].value.list.length = view->occupied;
    }
    tile_overlay(mc_tile_source(view->tile), view->fields, view->field_count, scratch);
    view->root.type = MC_NBT_COMPOUND;
    const mc_nbt_tag *source = mc_tile_source(view->tile);
    if (source != NULL)
        view->root.name = source->name;
    view->root.value.compound.items = scratch;
    view->root.value.compound.length = view->root_count;
    return MC_TILE_OK;
}

/* Native accounting only: address hashing never selects source Map traversal. */
static bool live_seen_insert(const void **seen, size_t capacity, const void *pointer)
{
    if (pointer == NULL)
        return false;
    uintptr_t bits = (uintptr_t)pointer;
    bits ^= bits >> 17u;
    size_t index = (size_t)bits & (capacity - 1u);
    while (seen[index] != NULL) {
        if (seen[index] == pointer)
            return false;
        index = (index + 1u) & (capacity - 1u);
    }
    seen[index] = pointer;
    return true;
}

mc_world_storage_status mc_live_tiles_build_records(const mc_world_chunk *chunk,
                                                    const mc_nbt_limits *requested,
                                                    mc_nbt_tree **out, mc_live_tile_error *error)
{
    mc_live_tile_error frame = live_empty_error();
    mc_nbt_limits limits;
    if (out == NULL || chunk == NULL || !tile_limits(requested, &limits) ||
        !mc_live_tiles_map_valid(chunk)) {
        live_fail(&frame, MC_WORLD_STORAGE_INVALID_ARGUMENT, "Tile records build arguments");
        return live_finish(NULL, frame, error);
    }
    const mc_tile_map_backing *map = chunk->l->backing;
    if ((map == NULL && chunk->l->release_backing != NULL) ||
        (map != NULL && map->journal_started && !map->journal_complete)) {
        live_fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                  "Tile records incomplete/retired owner");
        return live_finish(NULL, frame, error);
    }
    const size_t count = chunk->l->count;
    if (limits.max_allocation_bytes > chunk->max_allocation_bytes)
        limits.max_allocation_bytes = chunk->max_allocation_bytes;
    const mc_nbt_tag *retained =
        map == NULL || map->journal == NULL
            ? NULL
            : mc_nbt_compound_get(mc_nbt_tree_root(map->journal), live_records_name, 12);
    const size_t old_count = retained == NULL ? 0 : retained->value.list.length;
    if (map != NULL && map->journal_complete &&
        (retained == NULL || retained->type != MC_NBT_LIST || old_count != map->record_count ||
         (old_count != 0 && map->consumed == NULL))) {
        live_fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Tile record provenance witness");
        return live_finish(NULL, frame, error);
    }
    size_t opaque = 0;
    for (size_t i = 0; i < old_count; ++i)
        if (!map->consumed[i])
            ++opaque;
    size_t output_count = count;
    if (!tile_size_add(&output_count, opaque) || output_count > limits.max_elements ||
        count > SIZE_MAX / sizeof(live_record_view) || count > (SIZE_MAX / 2u) / 38u) {
        live_fail(&frame, MC_WORLD_STORAGE_LIMIT, "Tile records output/scratch bounds");
        return live_finish(NULL, frame, error);
    }
    size_t seen_capacity = 1;
    const size_t seen_required = count * 38u * 2u;
    while (seen_capacity < seen_required) {
        if (seen_capacity > SIZE_MAX / 2u) {
            live_fail(&frame, MC_WORLD_STORAGE_LIMIT, "Tile records accounting capacity");
            return live_finish(NULL, frame, error);
        }
        seen_capacity *= 2u;
    }
    if (seen_capacity > SIZE_MAX / sizeof(void *)) {
        live_fail(&frame, MC_WORLD_STORAGE_LIMIT, "Tile records accounting bytes");
        return live_finish(NULL, frame, error);
    }
    const size_t seen_bytes = seen_capacity * sizeof(void *);
    const size_t view_bytes = count * sizeof(live_record_view);
    size_t complete_graph;
    if (!live_graph_measure(chunk, NULL, limits.max_allocation_bytes, &complete_graph, &frame))
        return live_finish(NULL, frame, error);
    size_t admitted_peak = complete_graph;
    if (!tile_size_add(&admitted_peak, seen_bytes) || !tile_size_add(&admitted_peak, view_bytes) ||
        admitted_peak > limits.max_allocation_bytes) {
        live_fail(&frame, MC_WORLD_STORAGE_LIMIT, "Tile records complete planning peak budget");
        return live_finish(NULL, frame, error);
    }
    size_t live = chunk->allocation_bytes;
    if (!tile_size_add(&live, seen_bytes) || !tile_size_add(&live, view_bytes) ||
        live > limits.max_allocation_bytes) {
        live_fail(&frame, MC_WORLD_STORAGE_LIMIT, "Tile records initial live budget");
        return live_finish(NULL, frame, error);
    }
    const void **seen = calloc(seen_capacity, sizeof(*seen));
    live_record_view *views = count == 0 ? NULL : calloc(count, sizeof(*views));
    if (seen == NULL || (count != 0 && views == NULL)) {
        free(seen);
        free(views);
        live_fail(&frame, MC_WORLD_STORAGE_ALLOCATION_FAILED, "Tile records planning allocation");
        return live_finish(NULL, frame, error);
    }
    size_t pointer_count = output_count;
    for (size_t i = 0; i < count && frame.status == MC_WORLD_STORAGE_OK; ++i) {
        mc_tile *tile = NULL;
        if (mc_live_tiles_map_at(chunk, i, &tile) != MC_WORLD_STORAGE_OK) {
            live_fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Tile records Map iteration");
            break;
        }
        mc_tile_status planned = live_record_plan(tile, &views[i], &limits, &frame.tile);
        if (planned != MC_TILE_OK) {
            live_tile_error(&frame, planned, "Tile record semantic plan");
            if (frame.tile.base_save_bytecode != 0 || frame.tile.save_bytecode != 0)
                frame.bytecode = 271;
            break;
        }
        if (!tile_size_add(&pointer_count, views[i].pointer_count)) {
            live_fail(&frame, MC_WORLD_STORAGE_LIMIT, "Tile records pointer overflow");
            break;
        }
        if (live_seen_insert(seen, seen_capacity, tile)) {
            size_t direct = mc_tile_native_bytes(tile);
            if (!tile_size_add(&direct, mc_tile_source_bytes(tile)) ||
                !tile_size_add(&live, direct)) {
                live_fail(&frame, MC_WORLD_STORAGE_LIMIT, "Tile records owner graph");
                break;
            }
            if (tile->kind == MC_TILE_CHEST) {
                for (size_t slot = 0; slot < tile->state.chest.capacity; ++slot) {
                    mc_item_stack *stack = tile->state.chest.slots[slot];
                    if (live_seen_insert(seen, seen_capacity, stack)) {
                        size_t bytes;
                        mc_item_stack_status item = mc_item_stack_native_bytes(stack, &bytes);
                        if (item != MC_ITEM_STACK_OK || !tile_size_add(&live, bytes)) {
                            live_fail(&frame, MC_WORLD_STORAGE_LIMIT, "Tile records Stack graph");
                            break;
                        }
                    }
                }
            } else if (live_seen_insert(seen, seen_capacity, tile->state.spawner.name)) {
                if (!tile_size_add(&live, mc_tile_name_native_bytes(tile->state.spawner.name)))
                    live_fail(&frame, MC_WORLD_STORAGE_LIMIT, "Tile records name graph");
            }
        }
    }
    const mc_nbt_tag **scratch = NULL;
    if (frame.status == MC_WORLD_STORAGE_OK) {
        if (pointer_count > SIZE_MAX / sizeof(*scratch) ||
            !tile_size_add(&live, pointer_count * sizeof(*scratch)) ||
            live > limits.max_allocation_bytes)
            live_fail(&frame, MC_WORLD_STORAGE_LIMIT, "Tile records complete peak budget");
        else {
            scratch = pointer_count == 0 ? NULL : malloc(pointer_count * sizeof(*scratch));
            if (pointer_count != 0 && scratch == NULL)
                live_fail(&frame, MC_WORLD_STORAGE_ALLOCATION_FAILED, "Tile records pointer arena");
        }
    }
    mc_nbt_tree *built = NULL;
    if (frame.status == MC_WORLD_STORAGE_OK) {
        size_t offset = output_count;
        for (size_t i = 0; i < count; ++i) {
            mc_tile_status prepared = live_record_prepare(&views[i], scratch + offset, &frame.tile);
            if (prepared != MC_TILE_OK) {
                live_tile_error(&frame, prepared, "Tile records borrowed overlay");
                if (frame.tile.save_bytecode != 0)
                    frame.bytecode = 271;
                break;
            }
            offset += views[i].pointer_count;
        }
        if (frame.status == MC_WORLD_STORAGE_OK) {
            size_t next = 0, written = 0;
            for (size_t i = 0; i < old_count; ++i) {
                if (!map->consumed[i])
                    scratch[written++] = retained->value.list.items[i];
                else if (next < count)
                    scratch[written++] = &views[next++].root;
            }
            while (next < count)
                scratch[written++] = &views[next++].root;
            if (written != output_count)
                live_fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Tile records merge witness");
            else {
                mc_nbt_tag list = {0}, root = {0};
                list.type = MC_NBT_LIST;
                list.name = (mc_nbt_string){live_records_name, 12};
                list.value.list.element_type = MC_NBT_COMPOUND;
                list.value.list.items = output_count == 0 ? NULL : scratch;
                list.value.list.length = output_count;
                const mc_nbt_tag *child = &list;
                root.type = MC_NBT_COMPOUND;
                root.value.compound.items = &child;
                root.value.compound.length = 1;
                mc_nbt_limits budget = limits;
                budget.max_allocation_bytes -= live;
                mc_nbt_error nbt = {0};
                mc_nbt_status copied = mc_nbt_tree_copy_compound(&root, &budget, &built, &nbt);
                if (copied != MC_NBT_OK) {
                    frame.tile.nbt = nbt;
                    live_fail(&frame,
                              copied == MC_NBT_LIMIT ? MC_WORLD_STORAGE_LIMIT
                              : copied == MC_NBT_ALLOCATION_FAILED
                                  ? MC_WORLD_STORAGE_ALLOCATION_FAILED
                                  : MC_WORLD_STORAGE_INVALID_DATA,
                              "Tile records independent output tree");
                }
            }
        }
    }
    free(scratch);
    free(views);
    free(seen);
    if (frame.status == MC_WORLD_STORAGE_OK)
        *out = built;
    else
        mc_nbt_tree_free(built);
    return live_finish(NULL, frame, error);
}

static bool live_graph_measure(const mc_world_chunk *chunk, mc_tile *extra, size_t ceiling,
                               size_t *out, mc_live_tile_error *error)
{
    const size_t count = chunk->l->count;
    if (!mc_live_tiles_map_valid(chunk) || count == SIZE_MAX) {
        live_fail(error, MC_WORLD_STORAGE_LIMIT, "Tile graph accounting count");
        return false;
    }
    /* Nonallocating deduplication precedes EVERY measuring/planning request.
     * The actual Chunk ledger already includes all Map/backing/table/consumed/
     * journal (including opaque records) storage. Add each reachable Tile,
     * Stack+its source, name and Tile source exactly once, plus an explicitly
     * held unpublished/replacement Tile. Repeated scans are native accounting,
     * never source iteration selection or a source work clamp. */
    size_t live = chunk->allocation_bytes;
    if (live > ceiling) {
        live_fail(error, MC_WORLD_STORAGE_LIMIT, "Tile graph accounting peak budget");
        return false;
    }
    const size_t total = count + (extra == NULL ? 0u : 1u);
    for (size_t i = 0; i < total && error->status == MC_WORLD_STORAGE_OK; ++i) {
        mc_tile *tile = extra;
        if (i < count && mc_live_tiles_map_at(chunk, i, &tile) != MC_WORLD_STORAGE_OK) {
            live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Tile graph actual Map iteration");
            break;
        }
        bool seen_tile = false;
        for (size_t j = 0; j < i; ++j) {
            mc_tile *previous = NULL;
            if (mc_live_tiles_map_at(chunk, j, &previous) != MC_WORLD_STORAGE_OK) {
                live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                          "Tile graph prior Map iteration");
                return false;
            }
            if (previous == tile) {
                seen_tile = true;
                break;
            }
        }
        if (seen_tile)
            continue;
        size_t validated;
        mc_tile_status status = mc_tile_storage_bytes(tile, &validated);
        if (status != MC_TILE_OK) {
            live_tile_error(error, status, "Tile graph native owner invariant");
            break;
        }
        size_t bytes = mc_tile_native_bytes(tile);
        if (!tile_size_add(&bytes, mc_tile_source_bytes(tile)) || !tile_size_add(&live, bytes)) {
            live_fail(error, MC_WORLD_STORAGE_LIMIT, "Tile graph owner bytes");
            break;
        }
        if (tile->kind == MC_TILE_CHEST) {
            for (size_t slot = 0; slot < tile->state.chest.capacity; ++slot) {
                mc_item_stack *stack = tile->state.chest.slots[slot];
                bool seen_stack = stack == NULL;
                for (size_t j = 0; j <= i && !seen_stack; ++j) {
                    mc_tile *previous = tile;
                    if (j < i && mc_live_tiles_map_at(chunk, j, &previous) != MC_WORLD_STORAGE_OK) {
                        live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                  "Tile graph Stack Map iteration");
                        return false;
                    }
                    if (previous->kind != MC_TILE_CHEST)
                        continue;
                    const size_t end = j == i ? slot : previous->state.chest.capacity;
                    for (size_t k = 0; k < end; ++k)
                        if (previous->state.chest.slots[k] == stack) {
                            seen_stack = true;
                            break;
                        }
                }
                if (!seen_stack) {
                    mc_item_stack_status item = mc_item_stack_native_bytes(stack, &bytes);
                    if (item != MC_ITEM_STACK_OK || !tile_size_add(&live, bytes)) {
                        live_fail(error, MC_WORLD_STORAGE_LIMIT, "Tile graph Stack bytes");
                        break;
                    }
                }
            }
        } else {
            mc_tile_name *name = tile->state.spawner.name;
            bool seen_name = name == NULL;
            for (size_t j = 0; j < i && !seen_name; ++j) {
                mc_tile *previous = NULL;
                if (mc_live_tiles_map_at(chunk, j, &previous) != MC_WORLD_STORAGE_OK) {
                    live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                              "Tile graph name Map iteration");
                    return false;
                }
                seen_name =
                    previous->kind == MC_TILE_SPAWNER && previous->state.spawner.name == name;
            }
            if (!seen_name && !tile_size_add(&live, mc_tile_name_native_bytes(name)))
                live_fail(error, MC_WORLD_STORAGE_LIMIT, "Tile graph name bytes");
        }
        if (live > ceiling)
            live_fail(error, MC_WORLD_STORAGE_LIMIT, "Tile graph complete peak budget");
    }
    if (error->status != MC_WORLD_STORAGE_OK)
        return false;
    *out = live;
    return true;
}

static bool live_graph_has_name(const mc_world_chunk *chunk, const mc_tile *extra,
                                const mc_tile_name *name)
{
    if (name == NULL)
        return true;
    if (extra != NULL && extra->kind == MC_TILE_SPAWNER && extra->state.spawner.name == name)
        return true;
    for (size_t i = 0; i < chunk->l->count; ++i) {
        mc_tile *tile = NULL;
        if (mc_live_tiles_map_at(chunk, i, &tile) != MC_WORLD_STORAGE_OK)
            return false; /* The following full admission still rejects invalid Map state. */
        if (tile->kind == MC_TILE_SPAWNER && tile->state.spawner.name == name)
            return true;
    }
    return false;
}

static mc_tile_status tile_import_measure(const mc_tile *tile, const tile_import_context *context,
                                          size_t *out)
{
    if (context == NULL || context->scope_chunk == NULL)
        return mc_tile_storage_bytes(tile, out);
    mc_live_tile_error frame = live_empty_error();
    if (live_graph_measure(context->scope_chunk, (mc_tile *)tile,
                           context->limits.max_allocation_bytes, out, &frame))
        return MC_TILE_OK;
    return frame.status == MC_WORLD_STORAGE_LIMIT ? MC_TILE_LIMIT : MC_TILE_STATE_ERROR;
}

const void *mc_live_tiles_name_identity(const mc_tile_name *name)
{
    return name == NULL                     ? NULL
           : name->literal_identity != NULL ? (const void *)name->literal_identity
                                            : (const void *)name;
}

mc_world_storage_status mc_live_tiles_literal(mc_world_runtime *world, const char *ascii,
                                              mc_tile_name **out, mc_live_tile_error *error)
{
    mc_live_tile_error frame = live_empty_error();
    if (ascii == NULL || out == NULL) {
        live_fail(&frame, MC_WORLD_STORAGE_INVALID_ARGUMENT, "Tile code literal arguments");
        return live_finish(NULL, frame, error);
    }
    if (!live_enter(world, &frame))
        return live_finish(NULL, frame, error);
    mc_tile_classes *classes = NULL;
    mc_tile_status status = live_classes_owner(world, &classes);
    mc_tile_name *name = NULL;
    if (status == MC_TILE_OK)
        status = live_literal(classes, ascii, &name, &frame.registry, NULL, NULL);
    if (status != MC_TILE_OK) {
        live_tile_error(&frame, status, "Tile reached shared literal");
    } else
        *out = name;
    mc_world_runtime_call_leave(world);
    return live_finish(world, frame, error);
}
