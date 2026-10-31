#include "mc/world/runtime/tile_entities/tile_entities.h"
#include "tick_internal.h"

#include "mc/storage/nbt/tree.h"

#include "../../../block/registry/registry_internal.h"
#include "../../../block/registry/runtime_internal.h"
#include "../../../platform/text/stdout.h"
#include "../../chunk/object_state_internal.h"
#include "../../storage/objects_internal.h"
#include "../chunk_provider_internal.h"
#include "../construction_internal.h"
#include "../mutation/mutation_internal.h"
#include "../runtime_internal.h"
#include "../terminal_inventory_internal.h"
#include "../world_access_internal.h"
#include "mc/world/runtime/blocks.h"
#include "mc/world/runtime/entities.h"
#include "piston_list_internal.h"
#include "piston_math_internal.h"
#include "tile_entities_internal.h"

#include "mc/core/java/java.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#if !defined(__SSE2__) && !defined(_M_X64) && !(defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#error "Live TileEntity drop arithmetic requires SSE2"
#endif
#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) ||       \
    (defined(_M_FP_FAST) && _M_FP_FAST)
#error "Live TileEntity drop arithmetic requires strict floating transformations"
#endif
#include <emmintrin.h>

typedef enum live_class_id {
    LIVE_FURNACE,
    LIVE_CHEST,
    LIVE_JUKEBOX,
    LIVE_DISPENSER,
    LIVE_SIGN,
    LIVE_SPAWNER,
    LIVE_NOTE_BLOCK,
    LIVE_PISTON,
    LIVE_CLASS_COUNT
} live_class_id;

typedef struct live_class_entry {
    live_class_id kind;
    mc_tile_entity_name *name;
} live_class_entry;

struct mc_tile_entity_class_literal {
    mc_tile_entity_classes *owner;
    mc_tile_entity_class_kind kind;
};

/* Unchanged ow/jh.<clinit> Class/String pairs at20..73, independently checked
 * against every referenced class's superclass metadata. The first row is the
 * base itself; these diagnostic names do not create source String identities.
 * Client archive af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c;
 * server archive 033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d. */
static const char *const class_literal_client_names[MC_TILE_ENTITY_CLASS_KIND_COUNT] = {
    "ow", "sk", "js", "eg", "az", "yk", "cy", "tn", "uk"};
static const char *const class_literal_server_names[MC_TILE_ENTITY_CLASS_KIND_COUNT] = {
    "jh", "ln", "gk", "cr", "ak", "pq", "bx", "mf", "mu"};

struct mc_tile_entity_classes {
    mc_world_runtime_classes *parent;
    mc_registry_classes *registry;
    mc_tile_entity_class_literal class_literals[MC_TILE_ENTITY_CLASS_KIND_COUNT];
    size_t class_literal_borrows;
    unsigned int phase, initializer_pc;
    mc_tile_entity_status failure;
    mc_registry_error first_registry_failure;
    live_class_entry *by_name, *by_class;
    size_t name_count, class_count, live_tile_entities, allocation_bytes;
    mc_tile_entity_name *literals;
    bool stdout_error;
    unsigned int piston_phase, piston_initializer_pc;
    mc_tile_entity_status piston_failure;
    mc_world_reference_list *piston_list;
    mc_world_reference_list *piston_list_identity;
    void **piston_items;
    mc_entity **piston_entities;
    size_t piston_count, piston_capacity;
    uint32_t piston_mod_count;
    size_t piston_allocation_bytes, piston_max_allocation_bytes;
    bool piston_configured;
};

struct tile_entity_import_context;
static mc_registry_error live_empty_registry(void);
static mc_world_storage_status live_tile_entity_error(mc_live_tile_entity_error *,
                                                      mc_tile_entity_status, const char *);
static mc_tile_entity_status live_classes_get(mc_world_runtime *world,
                                              mc_tile_entity_classes **out);
static mc_tile_entity_status live_classes_owner_in(mc_world_runtime_classes *parent,
                                                   mc_tile_entity_classes **out);
static mc_tile_entity_status live_classes_initialize(mc_tile_entity_classes *classes,
                                                     mc_tile_entity_classes **out);
static mc_tile_entity_status live_piston_classes_get(mc_tile_entity_classes *classes);
static void tile_entity_allocation_error(mc_world_runtime *world, mc_tile_entity_kind kind,
                                         mc_tile_entity_status status, mc_tile_entity_error *error);
static mc_tile_entity_status live_literal(mc_tile_entity_classes *classes, const char *ascii,
                                          mc_tile_entity_name **out, mc_registry_error *attempt,
                                          const mc_tile_entity *scope_tile_entity,
                                          const struct tile_entity_import_context *context);
static mc_tile_entity_name *live_class_name(mc_tile_entity_classes *classes,
                                            mc_tile_entity_kind kind);
static bool live_graph_measure(const mc_world_chunk *chunk, mc_tile_entity *extra, size_t ceiling,
                               size_t *out, mc_live_tile_entity_error *error);

_Static_assert(sizeof(double) == sizeof(uint64_t),
               "TileEntity rotation snapshots require 64-bit double");

/* Concrete TileEntity/Stack/name layouts. Runtime and persistence joins require
 * the coordinated actual owners; no stand-in success is provided. */
/* Older MSVC diagnoses this standard C17 flexible array as extension C4200.
 * Keep the allocation layout and scope the compatibility guard to this type. */
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4200)
#endif
struct mc_tile_entity_name {
    size_t holds;
    size_t allocation_bytes;
    bool immortal;
    mc_nbt_string text;
    const mc_registry_string *literal_identity;
    struct mc_tile_entity_name *literal_next;
    uint16_t units[];
};
#ifdef _MSC_VER
#pragma warning(pop)
#endif

typedef struct tile_entity_loaded_record {
    mc_item_stack *stack;
    const mc_nbt_tag *record;
    size_t index;
} tile_entity_loaded_record;

typedef struct tile_entity_import_context {
    mc_nbt_limits limits;
    tile_entity_loaded_record winners[27];
    /* When importing into a held Chunk, each allocation sees the complete
     * current Map plus the unpublished held TileEntity at the SAME full ceiling. */
    const mc_world_chunk *scope_chunk;
} tile_entity_import_context;

static mc_tile_entity_status tile_entity_import_measure(const mc_tile_entity *tile_entity,
                                                        const tile_entity_import_context *context,
                                                        size_t *out);
static bool live_graph_has_name(const mc_world_chunk *chunk, const mc_tile_entity *extra,
                                const mc_tile_entity_name *name);

static mc_registry_error live_empty_registry(void) {
    mc_tile_entity_error error;
    mc_tile_entity_error_init(&error);
    return error.registry;
}

static mc_world_storage_status tile_terminal_result(mc_world_storage_error *error,
                                                    mc_world_storage_status status,
                                                    const char *operation) {
    if (error != NULL)
        *error = (mc_world_storage_error){status, operation, 0};
    return status;
}

static mc_tile_entity_status tile_entity_inventory_result(mc_live_tile_entity_error *error,
                                                          mc_tile_entity_status status,
                                                          unsigned int pc) {
    if (status != MC_TILE_ENTITY_OK && error != NULL) {
        if (error->status == MC_WORLD_STORAGE_OK)
            live_tile_entity_error(error, status, "Chest native owner/slot/Stack operation");
        if (error->inventory_bytecode == 0)
            error->inventory_bytecode = pc;
    }
    return status;
}

static mc_tile_entity_status tile_entity_import_admit(const mc_tile_entity *tile_entity,
                                                      const tile_entity_import_context *context,
                                                      size_t additional,
                                                      mc_tile_entity_error *error,
                                                      const char *field, size_t index);

static const uint16_t tile_entity_x_name[] = {'x'};
static const uint16_t tile_entity_y_name[] = {'y'};
static const uint16_t tile_entity_z_name[] = {'z'};
static const uint16_t tile_entity_items_name[] = {'I', 't', 'e', 'm', 's'};
static const uint16_t tile_entity_slot_name[] = {'S', 'l', 'o', 't'};
static const uint16_t tile_entity_name[] = {'E', 'n', 't', 'i', 't', 'y', 'I', 'd'};
static const uint16_t tile_entity_delay_name[] = {'D', 'e', 'l', 'a', 'y'};
static const uint16_t tile_entity_id_name[] = {'i', 'd'};
static const uint16_t tile_entity_block_id_name[] = {'b', 'l', 'o', 'c', 'k', 'I', 'd'};
static const uint16_t tile_entity_block_data_name[] = {'b', 'l', 'o', 'c', 'k', 'D', 'a', 't', 'a'};
static const uint16_t tile_entity_facing_name[] = {'f', 'a', 'c', 'i', 'n', 'g'};
static const uint16_t tile_entity_progress_name[] = {'p', 'r', 'o', 'g', 'r', 'e', 's', 's'};
static const uint16_t tile_entity_extending_name[] = {'e', 'x', 't', 'e', 'n', 'd', 'i', 'n', 'g'};

static mc_tile_entity_status tile_entity_status(mc_tile_entity_error *error,
                                                mc_tile_entity_status status, const char *field,
                                                size_t index) {
    if (error != NULL) {
        *error = (mc_tile_entity_error){
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

static mc_tile_entity_status tile_entity_item_error(mc_tile_entity_error *error,
                                                    mc_item_stack_status status,
                                                    const mc_item_stack_error *item, size_t index) {
    const mc_tile_entity_status result =
        status == MC_ITEM_STACK_ALLOCATION_FAILED ? MC_TILE_ENTITY_ALLOCATION_FAILED
        : status == MC_ITEM_STACK_LIMIT           ? MC_TILE_ENTITY_LIMIT
        : status == MC_ITEM_STACK_INVALID_FIELD   ? MC_TILE_ENTITY_INVALID_FIELD
                                                  : MC_TILE_ENTITY_ITEM_FAILED;
    if (error != NULL) {
        *error = (mc_tile_entity_error){
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

static mc_tile_entity_status tile_entity_nbt_error(mc_tile_entity_error *error,
                                                   mc_nbt_status status, mc_nbt_error nbt,
                                                   const char *field, size_t index) {
    const mc_tile_entity_status result = status == MC_NBT_LIMIT ? MC_TILE_ENTITY_LIMIT
                                         : status == MC_NBT_ALLOCATION_FAILED
                                             ? MC_TILE_ENTITY_ALLOCATION_FAILED
                                             : MC_TILE_ENTITY_NBT_ERROR;
    tile_entity_status(error, result, field, index);
    if (error != NULL) {
        error->nbt = nbt;
    }
    return result;
}

static mc_tile_entity_status tile_entity_ready(const mc_tile_entity *tile_entity) {
    if (tile_entity == NULL) {
        return MC_TILE_ENTITY_INVALID_ARGUMENT;
    }
    return tile_entity->phase == MC_TILE_ENTITY_READY ? MC_TILE_ENTITY_OK
                                                      : MC_TILE_ENTITY_INVALID_PHASE;
}

static mc_tile_entity_status tile_entity_kind_ready(const mc_tile_entity *tile_entity,
                                                    mc_tile_entity_kind kind) {
    const mc_tile_entity_status status = tile_entity_ready(tile_entity);
    if (status != MC_TILE_ENTITY_OK) {
        return status;
    }
    return tile_entity->kind == kind ? MC_TILE_ENTITY_OK : MC_TILE_ENTITY_INVALID_KIND;
}

static mc_tile_entity_status tile_entity_chest_slot(const mc_tile_entity *tile_entity,
                                                    int32_t slot) {
    const mc_tile_entity_status status = tile_entity_kind_ready(tile_entity, MC_TILE_ENTITY_CHEST);
    if (status != MC_TILE_ENTITY_OK) {
        return status;
    }
    return slot < 0 || (uint32_t)slot >= tile_entity->state.chest.capacity
               ? MC_TILE_ENTITY_INVALID_SLOT
               : MC_TILE_ENTITY_OK;
}

static mc_tile_entity_status tile_entity_item_status(mc_item_stack_status status) {
    if (status == MC_ITEM_STACK_OK) {
        return MC_TILE_ENTITY_OK;
    }
    if (status == MC_ITEM_STACK_ALLOCATION_FAILED) {
        return MC_TILE_ENTITY_ALLOCATION_FAILED;
    }
    if (status == MC_ITEM_STACK_LIMIT) {
        return MC_TILE_ENTITY_LIMIT;
    }
    return MC_TILE_ENTITY_ITEM_FAILED;
}

mc_tile_entity_status mc_tile_entity_name_create(const uint16_t *units, size_t length,
                                                 mc_tile_entity_name **out) {
    if (out == NULL || (units == NULL && length != 0)) {
        return MC_TILE_ENTITY_INVALID_ARGUMENT;
    }
    if (length > (SIZE_MAX - sizeof(mc_tile_entity_name)) / sizeof(uint16_t)) {
        return MC_TILE_ENTITY_LIMIT;
    }
    const size_t bytes = sizeof(mc_tile_entity_name) + length * sizeof(uint16_t);
    mc_tile_entity_name *name = malloc(bytes);
    if (name == NULL) {
        return MC_TILE_ENTITY_ALLOCATION_FAILED;
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
    return MC_TILE_ENTITY_OK;
}

mc_tile_entity_status mc_tile_entity_name_retain(mc_tile_entity_name *name) {
    if (name == NULL || name->immortal) {
        return MC_TILE_ENTITY_OK;
    }
    if (name->holds == SIZE_MAX) {
        return MC_TILE_ENTITY_LIMIT;
    }
    ++name->holds;
    return MC_TILE_ENTITY_OK;
}

void mc_tile_entity_name_release(mc_tile_entity_name *name) {
    if (name != NULL && !name->immortal && --name->holds == 0) {
        free(name);
    }
}

mc_tile_entity_status mc_tile_entity_name_get_view(const mc_tile_entity_name *name,
                                                   mc_tile_entity_name_view *out) {
    if (out == NULL) {
        return MC_TILE_ENTITY_INVALID_ARGUMENT;
    }
    *out = name == NULL ? (mc_tile_entity_name_view){true, {NULL, 0}}
                        : (mc_tile_entity_name_view){false, name->text};
    return MC_TILE_ENTITY_OK;
}

size_t mc_tile_entity_name_native_bytes(const mc_tile_entity_name *name) {
    return name == NULL ? 0 : name->allocation_bytes;
}

static mc_tile_entity_status tile_entity_allocate_in_owner(mc_tile_entity_classes *classes,
                                                           mc_tile_entity_kind kind,
                                                           mc_tile_entity **out) {
    if (kind == MC_TILE_ENTITY_PISTON) {
        const mc_tile_entity_status active = live_piston_classes_get(classes);
        if (active != MC_TILE_ENTITY_OK)
            return active;
    }
    if (classes->live_tile_entities == SIZE_MAX)
        return MC_TILE_ENTITY_LIMIT;
    mc_tile_entity *tile_entity = calloc(1, sizeof(*tile_entity));
    if (tile_entity == NULL) {
        return MC_TILE_ENTITY_ALLOCATION_FAILED;
    }
    tile_entity->classes = classes;
    ++classes->live_tile_entities;
    tile_entity->holds = 1;
    tile_entity->kind = kind;
    tile_entity->phase = MC_TILE_ENTITY_RAW;
    *out = tile_entity;
    return MC_TILE_ENTITY_OK;
}

mc_tile_entity_status mc_tile_entity_allocate(mc_world_runtime *world, mc_tile_entity_kind kind,
                                              mc_tile_entity **out) {
    if (out == NULL)
        return MC_TILE_ENTITY_INVALID_ARGUMENT;
    if (kind != MC_TILE_ENTITY_CHEST && kind != MC_TILE_ENTITY_SPAWNER &&
        kind != MC_TILE_ENTITY_PISTON)
        return MC_TILE_ENTITY_INVALID_KIND;
    mc_tile_entity_classes *classes = NULL;
    const mc_tile_entity_status active = live_classes_get(world, &classes);
    return active == MC_TILE_ENTITY_OK ? tile_entity_allocate_in_owner(classes, kind, out) : active;
}

mc_tile_entity_status mc_tile_entity_allocate_in_classes(mc_world_runtime_classes *parent,
                                                         mc_tile_entity_kind kind,
                                                         mc_tile_entity **out) {
    if (out == NULL)
        return MC_TILE_ENTITY_INVALID_ARGUMENT;
    if (kind != MC_TILE_ENTITY_CHEST && kind != MC_TILE_ENTITY_SPAWNER &&
        kind != MC_TILE_ENTITY_PISTON)
        return MC_TILE_ENTITY_INVALID_KIND;
    mc_tile_entity_classes *classes = NULL;
    mc_tile_entity_status active = live_classes_owner_in(parent, &classes);
    if (active == MC_TILE_ENTITY_OK)
        active = live_classes_initialize(classes, &classes);
    return active == MC_TILE_ENTITY_OK ? tile_entity_allocate_in_owner(classes, kind, out) : active;
}

static mc_tile_entity_status tile_entity_construct(mc_tile_entity *raw,
                                                   const tile_entity_import_context *context,
                                                   mc_tile_entity_error *error) {
    if (raw == NULL) {
        return tile_entity_status(error, MC_TILE_ENTITY_INVALID_ARGUMENT, NULL, SIZE_MAX);
    }
    if (raw->phase != MC_TILE_ENTITY_RAW) {
        return tile_entity_status(error, MC_TILE_ENTITY_INVALID_PHASE, NULL, SIZE_MAX);
    }
    raw->phase = MC_TILE_ENTITY_FAILED_CONSTRUCTION;
    if (raw->kind == MC_TILE_ENTITY_CHEST) {
        const mc_tile_entity_status admitted = tile_entity_import_admit(
            raw, context, 36u * sizeof(mc_item_stack *), error, NULL, SIZE_MAX);
        if (admitted != MC_TILE_ENTITY_OK) {
            return admitted;
        }
        mc_item_stack **slots = calloc(36u, sizeof(*slots));
        if (slots == NULL) {
            return tile_entity_status(error, MC_TILE_ENTITY_ALLOCATION_FAILED, NULL, SIZE_MAX);
        }
        raw->state.chest.slots = slots;
        raw->state.chest.capacity = 36;
    } else if (raw->kind == MC_TILE_ENTITY_SPAWNER) {
        raw->state.spawner.delay = -1;
        raw->state.spawner.previous_rotation = 0.0;
        mc_registry_error attempt = live_empty_registry();
        mc_tile_entity_status named =
            live_literal(raw->classes, "Pig", &raw->state.spawner.name, &attempt, raw, context);
        if (named != MC_TILE_ENTITY_OK) {
            tile_entity_status(error, named, "EntityId", SIZE_MAX);
            if (error != NULL) {
                error->registry = attempt;
                error->constructor_bytecode = 15;
            }
            return named;
        }
        raw->state.spawner.delay = 20;
    } else if (raw->kind != MC_TILE_ENTITY_PISTON) {
        return tile_entity_status(error, MC_TILE_ENTITY_INVALID_KIND, NULL, SIZE_MAX);
    }
    raw->phase = MC_TILE_ENTITY_READY;
    return tile_entity_status(error, MC_TILE_ENTITY_OK, NULL, SIZE_MAX);
}

mc_tile_entity_status mc_tile_entity_construct(mc_tile_entity *raw) {
    return tile_entity_construct(raw, NULL, NULL);
}

mc_tile_entity_status mc_tile_entity_construct_detailed(mc_tile_entity *raw,
                                                        mc_tile_entity_error *error) {
    return tile_entity_construct(raw, NULL, error);
}

mc_tile_entity_status mc_tile_entity_piston_construct(mc_tile_entity *raw, int32_t block_id,
                                                      int32_t block_data, int32_t facing,
                                                      bool extending, bool render_head,
                                                      mc_tile_entity_error *error) {
    if (raw == NULL)
        return tile_entity_status(error, MC_TILE_ENTITY_INVALID_ARGUMENT, NULL, SIZE_MAX);
    if (raw->kind != MC_TILE_ENTITY_PISTON)
        return tile_entity_status(error, MC_TILE_ENTITY_INVALID_KIND, NULL, SIZE_MAX);
    if (raw->phase != MC_TILE_ENTITY_RAW)
        return tile_entity_status(error, MC_TILE_ENTITY_INVALID_PHASE, NULL, SIZE_MAX);
    raw->phase = MC_TILE_ENTITY_FAILED_CONSTRUCTION;
    /* Empty base constructor1, then actual uk/mu field stores6/11/16/22/28.
     * Neither constructor writes k/l or binds/installs the TileEntity in a World. */
    raw->state.piston.block_id = block_id;
    raw->state.piston.block_data = block_data;
    raw->state.piston.facing = facing;
    raw->state.piston.extending = extending;
    raw->state.piston.render_head = render_head;
    raw->phase = MC_TILE_ENTITY_READY;
    return tile_entity_status(error, MC_TILE_ENTITY_OK, NULL, SIZE_MAX);
}

mc_tile_entity_status mc_tile_entity_retain(mc_tile_entity *tile_entity) {
    if (tile_entity == NULL) {
        return MC_TILE_ENTITY_INVALID_ARGUMENT;
    }
    if (tile_entity->holds == SIZE_MAX) {
        return MC_TILE_ENTITY_LIMIT;
    }
    ++tile_entity->holds;
    return MC_TILE_ENTITY_OK;
}

mc_registry_classes *mc_tile_entity_piston_registry_held(const mc_tile_entity *tile_entity) {
    return tile_entity == NULL || tile_entity->classes == NULL ? NULL
                                                               : tile_entity->classes->registry;
}

mc_world_runtime_classes *
mc_tile_entity_piston_code_classes_held(const mc_tile_entity *tile_entity) {
    if (tile_entity == NULL || tile_entity->holds == 0 ||
        tile_entity->phase != MC_TILE_ENTITY_READY || tile_entity->kind != MC_TILE_ENTITY_PISTON ||
        tile_entity->classes == NULL)
        return NULL;
    const mc_tile_entity_classes *classes = tile_entity->classes;
    if (classes->phase != MC_TILE_ENTITY_CLASS_READY ||
        classes->piston_phase != MC_TILE_ENTITY_CLASS_READY || classes->parent == NULL ||
        classes->parent->tile_entities != classes)
        return NULL;
    return classes->parent;
}

mc_world_storage_status mc_tile_entity_terminal_read(const mc_tile_entity *tile_entity,
                                                     mc_tile_entity_terminal_view *out,
                                                     mc_world_storage_error *error) {
    mc_world_storage_error local = {0};
    if (error == NULL)
        error = &local;
    (void)tile_terminal_result(error, MC_WORLD_STORAGE_OK, NULL);
    if (tile_entity == NULL || out == NULL)
        return tile_terminal_result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                                    "terminal TileEntity view arguments");
    const mc_tile_entity_classes *classes = tile_entity->classes;
    if (tile_entity->holds == 0 || classes == NULL || classes->parent == NULL ||
        classes->parent->tile_entities != classes || classes->live_tile_entities == 0)
        return tile_terminal_result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                    "terminal TileEntity canonical owner invariant");
    /* RAW and failed instances own these same native occurrences. Neither
     * source construction nor a READY-only getter is required to inspect them. */
    *out = (mc_tile_entity_terminal_view){classes->parent, tile_entity->world, tile_entity->holds};
    return MC_WORLD_STORAGE_OK;
}

static void tile_entity_slots_release(mc_item_stack **slots, size_t capacity) {
    for (size_t i = 0; i < capacity; ++i) {
        mc_item_stack_release(slots[i]);
    }
    free(slots);
}

void mc_tile_entity_release(mc_tile_entity *tile_entity) {
    if (tile_entity == NULL || --tile_entity->holds != 0) {
        return;
    }
    if (tile_entity->kind == MC_TILE_ENTITY_CHEST) {
        tile_entity_slots_release(tile_entity->state.chest.slots,
                                  tile_entity->state.chest.capacity);
    } else if (tile_entity->kind == MC_TILE_ENTITY_SPAWNER) {
        mc_tile_entity_name_release(tile_entity->state.spawner.name);
    }
    mc_nbt_tree_free(tile_entity->source);
    if (tile_entity->world != NULL) {
        mc_world_runtime_binding_release(tile_entity->world);
    }
    --tile_entity->classes->live_tile_entities;
    free(tile_entity);
}

mc_tile_entity_status mc_tile_entity_get_snapshot(const mc_tile_entity *tile_entity,
                                                  mc_tile_entity_snapshot *out) {
    if (tile_entity == NULL || out == NULL) {
        return MC_TILE_ENTITY_INVALID_ARGUMENT;
    }
    mc_tile_entity_snapshot snapshot = {0};
    snapshot.kind = tile_entity->kind;
    snapshot.phase = tile_entity->phase;
    snapshot.world = tile_entity->world;
    snapshot.x = tile_entity->x;
    snapshot.y = tile_entity->y;
    snapshot.z = tile_entity->z;
    snapshot.invalid = tile_entity->invalid;
    if (tile_entity->kind == MC_TILE_ENTITY_CHEST) {
        snapshot.chest_capacity = tile_entity->state.chest.capacity;
    } else if (tile_entity->kind == MC_TILE_ENTITY_SPAWNER) {
        snapshot.spawner_delay = tile_entity->state.spawner.delay;
        memcpy(&snapshot.spawner_rotation_bits, &tile_entity->state.spawner.rotation,
               sizeof(snapshot.spawner_rotation_bits));
        memcpy(&snapshot.spawner_previous_rotation_bits,
               &tile_entity->state.spawner.previous_rotation,
               sizeof(snapshot.spawner_previous_rotation_bits));
        snapshot.spawner_name = tile_entity->state.spawner.name;
    } else if (tile_entity->kind == MC_TILE_ENTITY_PISTON) {
        snapshot.piston_block_id = tile_entity->state.piston.block_id;
        snapshot.piston_block_data = tile_entity->state.piston.block_data;
        snapshot.piston_facing = tile_entity->state.piston.facing;
        snapshot.piston_extending = tile_entity->state.piston.extending;
        snapshot.piston_render_head = tile_entity->state.piston.render_head;
        snapshot.piston_progress_bits = tile_entity->state.piston.progress_bits;
        snapshot.piston_previous_progress_bits = tile_entity->state.piston.previous_progress_bits;
    }
    *out = snapshot;
    return MC_TILE_ENTITY_OK;
}

size_t mc_tile_entity_native_bytes(const mc_tile_entity *tile_entity) {
    if (tile_entity == NULL) {
        return 0;
    }
    return sizeof(*tile_entity) + (tile_entity->kind == MC_TILE_ENTITY_CHEST
                                       ? tile_entity->state.chest.capacity * sizeof(mc_item_stack *)
                                       : 0);
}

const mc_nbt_tag *mc_tile_entity_source(const mc_tile_entity *tile_entity) {
    return tile_entity == NULL ? NULL : mc_nbt_tree_root(tile_entity->source);
}

size_t mc_tile_entity_source_bytes(const mc_tile_entity *tile_entity) {
    return tile_entity == NULL ? 0 : mc_nbt_tree_allocation_bytes(tile_entity->source);
}

static bool tile_entity_size_add(size_t *total, size_t amount) {
    if (amount > SIZE_MAX - *total) {
        return false;
    }
    *total += amount;
    return true;
}

static bool tile_entity_size_multiply(size_t left, size_t right, size_t *out) {
    if (right != 0 && left > SIZE_MAX / right) {
        return false;
    }
    *out = left * right;
    return true;
}

mc_tile_entity_status mc_tile_entity_storage_bytes(const mc_tile_entity *tile_entity, size_t *out) {
    if (tile_entity == NULL || out == NULL) {
        return MC_TILE_ENTITY_INVALID_ARGUMENT;
    }
    size_t bytes = sizeof(*tile_entity);
    if (!tile_entity_size_add(&bytes, mc_nbt_tree_allocation_bytes(tile_entity->source))) {
        return MC_TILE_ENTITY_LIMIT;
    }
    if (tile_entity->kind == MC_TILE_ENTITY_CHEST) {
        const size_t capacity = tile_entity->state.chest.capacity;
        if ((capacity != 0 && capacity != 27 && capacity != 36) ||
            (tile_entity->state.chest.slots == NULL) != (capacity == 0) ||
            (tile_entity->phase == MC_TILE_ENTITY_READY && capacity == 0)) {
            return MC_TILE_ENTITY_STATE_ERROR;
        }
        /* Capacity is one of 0/27/36, proving this product is bounded. */
        if (!tile_entity_size_add(&bytes, capacity * sizeof(mc_item_stack *))) {
            return MC_TILE_ENTITY_LIMIT;
        }
        for (size_t i = 0; i < capacity; ++i) {
            const mc_item_stack *stack = tile_entity->state.chest.slots[i];
            if (stack == NULL) {
                continue;
            }
            bool seen = false;
            for (size_t j = 0; j < i; ++j) {
                if (tile_entity->state.chest.slots[j] == stack) {
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
                return tile_entity_item_status(item);
            }
            if (!tile_entity_size_add(&bytes, item_bytes)) {
                return MC_TILE_ENTITY_LIMIT;
            }
        }
    } else if (tile_entity->kind == MC_TILE_ENTITY_SPAWNER) {
        if (!tile_entity_size_add(
                &bytes, mc_tile_entity_name_native_bytes(tile_entity->state.spawner.name))) {
            return MC_TILE_ENTITY_LIMIT;
        }
    } else if (tile_entity->kind != MC_TILE_ENTITY_PISTON) {
        return MC_TILE_ENTITY_STATE_ERROR;
    }
    *out = bytes;
    return MC_TILE_ENTITY_OK;
}

static bool tile_entity_limits(const mc_nbt_limits *requested, mc_nbt_limits *out) {
    *out = requested == NULL ? mc_nbt_default_limits() : *requested;
    return out->max_depth != 0 && out->max_depth <= 128;
}

static mc_tile_entity_status tile_entity_import_admit(const mc_tile_entity *tile_entity,
                                                      const tile_entity_import_context *context,
                                                      size_t additional,
                                                      mc_tile_entity_error *error,
                                                      const char *field, size_t index) {
    if (context == NULL) {
        return MC_TILE_ENTITY_OK;
    }
    size_t live;
    const mc_tile_entity_status measured = tile_entity_import_measure(tile_entity, context, &live);
    if (measured != MC_TILE_ENTITY_OK) {
        return tile_entity_status(error, measured, field, index);
    }
    const size_t ceiling = context->limits.max_allocation_bytes;
    if (live > ceiling || additional > ceiling - live) {
        return tile_entity_status(error, MC_TILE_ENTITY_LIMIT, field, index);
    }
    return MC_TILE_ENTITY_OK;
}

mc_tile_entity_status mc_tile_entity_bind_world(mc_tile_entity *tile_entity,
                                                mc_world_runtime *world) {
    const mc_tile_entity_status status = tile_entity_ready(tile_entity);
    if (status != MC_TILE_ENTITY_OK) {
        return status;
    }
    if (world != NULL && world->classes != tile_entity->classes->parent) {
        return MC_TILE_ENTITY_INVALID_ARGUMENT;
    }
    if (tile_entity->world == world) {
        return MC_TILE_ENTITY_OK;
    }
    if (world != NULL && !mc_world_runtime_binding_retain(world)) {
        return MC_TILE_ENTITY_WORLD_FAILED;
    }
    mc_world_runtime *previous = tile_entity->world;
    tile_entity->world = world;
    if (previous != NULL) {
        mc_world_runtime_binding_release(previous);
    }
    return MC_TILE_ENTITY_OK;
}

mc_tile_entity_status mc_tile_entity_set_coordinates(mc_tile_entity *tile_entity, int32_t x,
                                                     int32_t y, int32_t z) {
    const mc_tile_entity_status status = tile_entity_ready(tile_entity);
    if (status == MC_TILE_ENTITY_OK) {
        tile_entity->x = x;
        tile_entity->y = y;
        tile_entity->z = z;
    }
    return status;
}

mc_tile_entity_status mc_tile_entity_set_invalid(mc_tile_entity *tile_entity, bool invalid) {
    const mc_tile_entity_status status = tile_entity_ready(tile_entity);
    if (status == MC_TILE_ENTITY_OK) {
        tile_entity->invalid = invalid;
    }
    return status;
}

mc_tile_entity_status mc_tile_entity_dirty_detailed(mc_tile_entity *tile_entity,
                                                    mc_live_tile_entity_error *error) {
    if (error != NULL)
        mc_live_tile_entities_error_init(error);
    const mc_tile_entity_status status = tile_entity_ready(tile_entity);
    if (status != MC_TILE_ENTITY_OK) {
        if (error != NULL)
            live_tile_entity_error(error, status, "TileEntity dirty receiver phase");
        return status;
    }
    mc_world_runtime *world = tile_entity->world;
    if (world == NULL) {
        return MC_TILE_ENTITY_OK;
    }
    const int32_t x = tile_entity->x;
    const int32_t y = tile_entity->y;
    const int32_t z = tile_entity->z;
    const mc_world_storage_status dirty =
        mc_world_runtime_tile_entity_dirty_detailed(world, x, y, z, tile_entity, error);
    if (dirty != MC_WORLD_STORAGE_OK && error != NULL) {
        error->tile_entity_bytecode = 24; /* ow.y_/jh.i -> actual World dirty invocation */
        error->tile_entity.status = MC_TILE_ENTITY_WORLD_FAILED;
    }
    return dirty == MC_WORLD_STORAGE_OK ? MC_TILE_ENTITY_OK : MC_TILE_ENTITY_WORLD_FAILED;
}

mc_tile_entity_status mc_tile_entity_dirty(mc_tile_entity *tile_entity) {
    return mc_tile_entity_dirty_detailed(tile_entity, NULL);
}

mc_tile_entity_status mc_tile_entity_chest_size(const mc_tile_entity *tile_entity, int32_t *out) {
    if (out == NULL) {
        return MC_TILE_ENTITY_INVALID_ARGUMENT;
    }
    const mc_tile_entity_status status = tile_entity_kind_ready(tile_entity, MC_TILE_ENTITY_CHEST);
    if (status == MC_TILE_ENTITY_OK) {
        *out = 27;
    }
    return status;
}

mc_tile_entity_status mc_tile_entity_chest_limit(const mc_tile_entity *tile_entity, int32_t *out) {
    if (out == NULL) {
        return MC_TILE_ENTITY_INVALID_ARGUMENT;
    }
    const mc_tile_entity_status status = tile_entity_kind_ready(tile_entity, MC_TILE_ENTITY_CHEST);
    if (status == MC_TILE_ENTITY_OK) {
        *out = 64;
    }
    return status;
}

mc_tile_entity_status mc_tile_entity_chest_capacity(const mc_tile_entity *tile_entity,
                                                    size_t *out) {
    if (out == NULL) {
        return MC_TILE_ENTITY_INVALID_ARGUMENT;
    }
    const mc_tile_entity_status status = tile_entity_kind_ready(tile_entity, MC_TILE_ENTITY_CHEST);
    if (status == MC_TILE_ENTITY_OK) {
        *out = tile_entity->state.chest.capacity;
    }
    return status;
}

mc_tile_entity_status mc_tile_entity_chest_get(mc_tile_entity *tile_entity, int32_t slot,
                                               mc_item_stack **out) {
    if (out == NULL) {
        return MC_TILE_ENTITY_INVALID_ARGUMENT;
    }
    const mc_tile_entity_status status = tile_entity_chest_slot(tile_entity, slot);
    if (status != MC_TILE_ENTITY_OK) {
        return status;
    }
    mc_item_stack *stack = tile_entity->state.chest.slots[(size_t)slot];
    if (stack != NULL) {
        const mc_item_stack_status held = mc_item_stack_retain(stack);
        if (held != MC_ITEM_STACK_OK) {
            return tile_entity_item_status(held);
        }
    }
    *out = stack;
    return MC_TILE_ENTITY_OK;
}

static mc_tile_entity_status tile_entity_inventory_item_result(mc_live_tile_entity_error *error,
                                                               mc_item_stack_status item) {
    if (error != NULL)
        error->tile_entity.item.status = item;
    return tile_entity_inventory_result(error, tile_entity_item_status(item), 0);
}

mc_tile_entity_status mc_tile_entity_chest_set_detailed(mc_tile_entity *tile_entity, int32_t slot,
                                                        mc_item_stack *stack,
                                                        mc_live_tile_entity_error *error) {
    if (error != NULL)
        mc_live_tile_entities_error_init(error);
    mc_tile_entity_status status = tile_entity_chest_slot(tile_entity, slot);
    if (status != MC_TILE_ENTITY_OK) {
        return tile_entity_inventory_result(error, status, 0);
    }
    if (stack != NULL) {
        mc_item_stack_phase phase;
        mc_item_stack_status item = mc_item_stack_phase_get(stack, &phase);
        if (item != MC_ITEM_STACK_OK || phase != MC_ITEM_STACK_READY) {
            return tile_entity_inventory_result(error, MC_TILE_ENTITY_ITEM_FAILED, 0);
        }
        item = mc_item_stack_retain(stack);
        if (item != MC_ITEM_STACK_OK) {
            return tile_entity_inventory_item_result(error, item);
        }
    }
    if (error != NULL)
        error->inventory_bytecode = 6;
    mc_item_stack *previous = tile_entity->state.chest.slots[(size_t)slot];
    tile_entity->state.chest.slots[(size_t)slot] = stack;
    mc_item_stack_release(previous);
    if (stack != NULL) {
        mc_item_stack_fields fields;
        if (error != NULL)
            error->inventory_bytecode = 12;
        mc_item_stack_status item = mc_item_stack_fields_get(stack, &fields);
        if (item != MC_ITEM_STACK_OK) {
            return tile_entity_inventory_item_result(error, item);
        }
        int32_t limit;
        if (error != NULL)
            error->inventory_bytecode = 16;
        status = mc_tile_entity_chest_limit(tile_entity, &limit);
        if (status != MC_TILE_ENTITY_OK) {
            return tile_entity_inventory_result(error, status, 0);
        }
        if (fields.count > limit) {
            if (error != NULL)
                error->inventory_bytecode = 24;
            status = mc_tile_entity_chest_limit(tile_entity, &limit);
            if (status != MC_TILE_ENTITY_OK) {
                return tile_entity_inventory_result(error, status, 0);
            }
            if (error != NULL)
                error->inventory_bytecode = 27;
            item = mc_item_stack_count_set(stack, limit);
            if (item != MC_ITEM_STACK_OK) {
                return tile_entity_inventory_item_result(error, item);
            }
        }
    }
    status = mc_tile_entity_dirty_detailed(tile_entity, error);
    if (status != MC_TILE_ENTITY_OK && error != NULL)
        error->inventory_bytecode = 31;
    return tile_entity_inventory_result(error, status, 0);
}

mc_tile_entity_status mc_tile_entity_chest_set(mc_tile_entity *tile_entity, int32_t slot,
                                               mc_item_stack *stack) {
    return mc_tile_entity_chest_set_detailed(tile_entity, slot, stack, NULL);
}

mc_tile_entity_status mc_tile_entity_chest_remove_detailed(mc_tile_entity *tile_entity,
                                                           int32_t slot, int32_t amount,
                                                           mc_item_stack **out,
                                                           mc_live_tile_entity_error *error) {
    if (error != NULL)
        mc_live_tile_entities_error_init(error);
    if (out == NULL) {
        return tile_entity_inventory_result(error, MC_TILE_ENTITY_INVALID_ARGUMENT, 0);
    }
    mc_tile_entity_status status = tile_entity_chest_slot(tile_entity, slot);
    if (status != MC_TILE_ENTITY_OK) {
        return tile_entity_inventory_result(error, status, 0);
    }
    mc_item_stack *stack = tile_entity->state.chest.slots[(size_t)slot];
    if (stack == NULL) {
        *out = NULL;
        return MC_TILE_ENTITY_OK;
    }
    mc_item_stack_fields fields;
    if (error != NULL)
        error->inventory_bytecode = 15;
    mc_item_stack_status item = mc_item_stack_fields_get(stack, &fields);
    if (item != MC_ITEM_STACK_OK) {
        return tile_entity_inventory_item_result(error, item);
    }
    mc_item_stack *removed;
    const unsigned int dirty_pc = fields.count <= amount ? 37u : 73u;
    if (fields.count <= amount) {
        removed = stack;
        /* Transfer this physical slot hold into the private return local. */
        tile_entity->state.chest.slots[(size_t)slot] = NULL;
    } else {
        if (error != NULL)
            error->inventory_bytecode = 49;
        item = mc_item_stack_split(stack, amount, &removed);
        if (item != MC_ITEM_STACK_OK) {
            return tile_entity_inventory_item_result(error, item);
        }
        mc_item_stack *remaining = tile_entity->state.chest.slots[(size_t)slot];
        if (error != NULL)
            error->inventory_bytecode = 59;
        item = mc_item_stack_fields_get(remaining, &fields);
        if (item != MC_ITEM_STACK_OK) {
            mc_item_stack_release(removed);
            return tile_entity_inventory_item_result(error, item);
        }
        if (fields.count == 0) {
            tile_entity->state.chest.slots[(size_t)slot] = NULL;
            mc_item_stack_release(remaining);
        }
    }
    status = mc_tile_entity_dirty_detailed(tile_entity, error);
    if (status != MC_TILE_ENTITY_OK) {
        if (error != NULL)
            error->inventory_bytecode = dirty_pc;
        mc_item_stack_release(removed);
        return tile_entity_inventory_result(error, status, 0);
    }
    *out = removed;
    return MC_TILE_ENTITY_OK;
}

mc_tile_entity_status mc_tile_entity_chest_remove(mc_tile_entity *tile_entity, int32_t slot,
                                                  int32_t amount, mc_item_stack **out) {
    return mc_tile_entity_chest_remove_detailed(tile_entity, slot, amount, out, NULL);
}

mc_tile_entity_status mc_tile_entity_spawner_get_name(mc_tile_entity *tile_entity,
                                                      mc_tile_entity_name **out) {
    if (out == NULL) {
        return MC_TILE_ENTITY_INVALID_ARGUMENT;
    }
    const mc_tile_entity_status status =
        tile_entity_kind_ready(tile_entity, MC_TILE_ENTITY_SPAWNER);
    if (status != MC_TILE_ENTITY_OK) {
        return status;
    }
    mc_tile_entity_name *name = tile_entity->state.spawner.name;
    const mc_tile_entity_status held = mc_tile_entity_name_retain(name);
    if (held != MC_TILE_ENTITY_OK) {
        return held;
    }
    *out = name;
    return MC_TILE_ENTITY_OK;
}

mc_tile_entity_status mc_tile_entity_spawner_set_name(mc_tile_entity *tile_entity,
                                                      mc_tile_entity_name *name) {
    const mc_tile_entity_status status =
        tile_entity_kind_ready(tile_entity, MC_TILE_ENTITY_SPAWNER);
    if (status != MC_TILE_ENTITY_OK) {
        return status;
    }
    const mc_tile_entity_status held = mc_tile_entity_name_retain(name);
    if (held != MC_TILE_ENTITY_OK) {
        return held;
    }
    mc_tile_entity_name *previous = tile_entity->state.spawner.name;
    tile_entity->state.spawner.name = name;
    mc_tile_entity_name_release(previous);
    return MC_TILE_ENTITY_OK;
}

mc_tile_entity_status mc_tile_entity_piston_block_id(const mc_tile_entity *tile_entity,
                                                     int32_t *out) {
    if (out == NULL)
        return MC_TILE_ENTITY_INVALID_ARGUMENT;
    const mc_tile_entity_status status = tile_entity_kind_ready(tile_entity, MC_TILE_ENTITY_PISTON);
    if (status == MC_TILE_ENTITY_OK)
        *out = tile_entity->state.piston.block_id;
    return status;
}

mc_tile_entity_status mc_tile_entity_piston_block_data(const mc_tile_entity *tile_entity,
                                                       int32_t *out) {
    if (out == NULL)
        return MC_TILE_ENTITY_INVALID_ARGUMENT;
    const mc_tile_entity_status status = tile_entity_kind_ready(tile_entity, MC_TILE_ENTITY_PISTON);
    if (status == MC_TILE_ENTITY_OK)
        *out = tile_entity->state.piston.block_data;
    return status;
}

mc_tile_entity_status mc_tile_entity_piston_facing(const mc_tile_entity *tile_entity,
                                                   int32_t *out) {
    if (out == NULL)
        return MC_TILE_ENTITY_INVALID_ARGUMENT;
    const mc_tile_entity_status status = tile_entity_kind_ready(tile_entity, MC_TILE_ENTITY_PISTON);
    if (status == MC_TILE_ENTITY_OK)
        *out = tile_entity->state.piston.facing;
    return status;
}

mc_tile_entity_status mc_tile_entity_piston_extending(const mc_tile_entity *tile_entity,
                                                      bool *out) {
    if (out == NULL)
        return MC_TILE_ENTITY_INVALID_ARGUMENT;
    const mc_tile_entity_status status = tile_entity_kind_ready(tile_entity, MC_TILE_ENTITY_PISTON);
    if (status == MC_TILE_ENTITY_OK)
        *out = tile_entity->state.piston.extending;
    return status;
}

mc_tile_entity_status mc_tile_entity_piston_render_head(const mc_tile_entity *tile_entity,
                                                        bool *out) {
    if (out == NULL)
        return MC_TILE_ENTITY_INVALID_ARGUMENT;
    const mc_tile_entity_status status = tile_entity_kind_ready(tile_entity, MC_TILE_ENTITY_PISTON);
    if (status == MC_TILE_ENTITY_OK)
        *out = tile_entity->state.piston.render_head;
    return status;
}

mc_tile_entity_status mc_tile_entity_piston_progress_bits(const mc_tile_entity *tile_entity,
                                                          uint32_t *current, uint32_t *previous) {
    if (current == NULL || previous == NULL)
        return MC_TILE_ENTITY_INVALID_ARGUMENT;
    const mc_tile_entity_status status = tile_entity_kind_ready(tile_entity, MC_TILE_ENTITY_PISTON);
    if (status == MC_TILE_ENTITY_OK) {
        *current = tile_entity->state.piston.progress_bits;
        *previous = tile_entity->state.piston.previous_progress_bits;
    }
    return status;
}

mc_tile_entity_status mc_tile_entity_piston_progress(const mc_tile_entity *tile_entity,
                                                     uint32_t factor_bits, uint32_t *out_bits,
                                                     mc_tile_entity_error *error) {
    if (out_bits == NULL)
        return tile_entity_status(error, MC_TILE_ENTITY_INVALID_ARGUMENT, NULL, SIZE_MAX);
    const mc_tile_entity_status status = tile_entity_kind_ready(tile_entity, MC_TILE_ENTITY_PISTON);
    if (status != MC_TILE_ENTITY_OK)
        return tile_entity_status(error, status, NULL, SIZE_MAX);
    if (!mc_piston_math_admitted()) {
        tile_entity_status(error, MC_TILE_ENTITY_UNSUPPORTED_FP, NULL, SIZE_MAX);
        if (error != NULL) {
            error->operation = "Piston interpolation floating environment";
            error->getter_bytecode = 20;
        }
        return MC_TILE_ENTITY_UNSUPPORTED_FP;
    }
    *out_bits =
        mc_piston_interpolate_bits(tile_entity->state.piston.progress_bits,
                                   tile_entity->state.piston.previous_progress_bits, factor_bits);
    return tile_entity_status(error, MC_TILE_ENTITY_OK, NULL, SIZE_MAX);
}

static mc_tile_entity_status tile_entity_read_error(mc_tile_entity_error *error, const char *field,
                                                    unsigned int pc, bool base) {
    const mc_tile_entity_status status =
        tile_entity_status(error, MC_TILE_ENTITY_INVALID_FIELD, field, SIZE_MAX);
    if (error != NULL) {
        if (base)
            error->base_read_bytecode = pc;
        else
            error->read_bytecode = pc;
    }
    return status;
}

mc_tile_entity_status mc_tile_entity_read_base(mc_tile_entity *tile_entity,
                                               const mc_nbt_tag *compound,
                                               mc_tile_entity_error *error) {
    const mc_tile_entity_status status = tile_entity_ready(tile_entity);
    if (status != MC_TILE_ENTITY_OK) {
        return tile_entity_status(error, status, NULL, SIZE_MAX);
    }
    if (compound == NULL || compound->type != MC_NBT_COMPOUND) {
        return tile_entity_status(error, MC_TILE_ENTITY_INVALID_ARGUMENT, "x", SIZE_MAX);
    }
    const mc_nbt_tag *tag = mc_nbt_compound_get(compound, tile_entity_x_name, 1);
    if (tag != NULL && tag->type != MC_NBT_INT) {
        return tile_entity_read_error(error, "x", 4, true);
    }
    tile_entity->x = tag == NULL ? 0 : tag->value.int_value;
    tag = mc_nbt_compound_get(compound, tile_entity_y_name, 1);
    if (tag != NULL && tag->type != MC_NBT_INT) {
        return tile_entity_read_error(error, "y", 14, true);
    }
    tile_entity->y = tag == NULL ? 0 : tag->value.int_value;
    tag = mc_nbt_compound_get(compound, tile_entity_z_name, 1);
    if (tag != NULL && tag->type != MC_NBT_INT) {
        return tile_entity_read_error(error, "z", 24, true);
    }
    tile_entity->z = tag == NULL ? 0 : tag->value.int_value;
    return tile_entity_status(error, MC_TILE_ENTITY_OK, NULL, SIZE_MAX);
}

static mc_tile_entity_status tile_entity_chest_read(mc_tile_entity *tile_entity,
                                                    const mc_nbt_tag *compound,
                                                    tile_entity_import_context *context,
                                                    mc_tile_entity_error *error) {
    const mc_nbt_tag *items = mc_nbt_compound_get(compound, tile_entity_items_name, 5);
    if (items != NULL && items->type != MC_NBT_LIST) {
        return tile_entity_status(error, MC_TILE_ENTITY_INVALID_FIELD, "Items", SIZE_MAX);
    }
    const mc_tile_entity_status admitted = tile_entity_import_admit(
        tile_entity, context, 27u * sizeof(mc_item_stack *), error, "Items", SIZE_MAX);
    if (admitted != MC_TILE_ENTITY_OK) {
        return admitted;
    }
    mc_item_stack **slots = calloc(27u, sizeof(*slots));
    if (slots == NULL) {
        return tile_entity_status(error, MC_TILE_ENTITY_ALLOCATION_FAILED, "Items", SIZE_MAX);
    }
    mc_item_stack **previous = tile_entity->state.chest.slots;
    const size_t previous_capacity = tile_entity->state.chest.capacity;
    tile_entity->state.chest.slots = slots;
    tile_entity->state.chest.capacity = 27;
    tile_entity_slots_release(previous, previous_capacity);
    const size_t length = items == NULL ? 0 : items->value.list.length;
    for (size_t i = 0; i < length; ++i) {
        const mc_nbt_tag *record = items->value.list.items[i];
        if (record == NULL || record->type != MC_NBT_COMPOUND) {
            return tile_entity_status(error, MC_TILE_ENTITY_INVALID_FIELD, "Items[]", i);
        }
        const mc_nbt_tag *slot_tag = mc_nbt_compound_get(record, tile_entity_slot_name, 4);
        if (slot_tag != NULL && slot_tag->type != MC_NBT_BYTE) {
            return tile_entity_status(error, MC_TILE_ENTITY_INVALID_FIELD, "Slot", i);
        }
        const uint32_t slot =
            slot_tag == NULL ? 0 : (uint32_t)(int32_t)slot_tag->value.byte_value & UINT32_C(255);
        if (slot >= tile_entity->state.chest.capacity) {
            continue;
        }
        const mc_tile_entity_status item_admitted = tile_entity_import_admit(
            tile_entity, context, mc_item_stack_object_bytes(), error, "Items[]", i);
        if (item_admitted != MC_TILE_ENTITY_OK) {
            return item_admitted;
        }
        mc_item_stack *stack;
        mc_item_stack_status item = mc_item_stack_allocate(&stack);
        if (item != MC_ITEM_STACK_OK) {
            return tile_entity_item_error(error, item, NULL, i);
        }
        mc_item_stack_error item_error;
        item = mc_item_stack_construct_nbt(stack, record, &item_error);
        if (item != MC_ITEM_STACK_OK) {
            mc_item_stack_release(stack);
            return tile_entity_item_error(error, item, &item_error, i);
        }
        /* Constructor success precedes actual assignment. Do not call setter:
         * NBT load has no count clamp or dirty dispatch. */
        mc_item_stack *old = tile_entity->state.chest.slots[slot];
        tile_entity->state.chest.slots[slot] = stack;
        if (context != NULL) {
            context->winners[slot] = (tile_entity_loaded_record){stack, record, i};
        }
        mc_item_stack_release(old);
    }
    return tile_entity_status(error, MC_TILE_ENTITY_OK, NULL, SIZE_MAX);
}

static mc_tile_entity_status tile_entity_spawner_read(mc_tile_entity *tile_entity,
                                                      const mc_nbt_tag *compound,
                                                      tile_entity_import_context *context,
                                                      mc_tile_entity_error *error) {
    const mc_nbt_tag *tag = mc_nbt_compound_get(compound, tile_entity_name, 8);
    if (tag != NULL && tag->type != MC_NBT_STRING) {
        return tile_entity_status(error, MC_TILE_ENTITY_INVALID_FIELD, "EntityId", SIZE_MAX);
    }
    mc_tile_entity_name *name = NULL;
    if (tag != NULL) {
        if (context != NULL) {
            if (tag->value.string.units == NULL && tag->value.string.length != 0) {
                return tile_entity_status(error, MC_TILE_ENTITY_INVALID_ARGUMENT, "EntityId",
                                          SIZE_MAX);
            }
            if (tag->value.string.length >
                (SIZE_MAX - sizeof(mc_tile_entity_name)) / sizeof(uint16_t)) {
                return tile_entity_status(error, MC_TILE_ENTITY_LIMIT, "EntityId", SIZE_MAX);
            }
            const size_t bytes =
                sizeof(mc_tile_entity_name) + tag->value.string.length * sizeof(uint16_t);
            const mc_tile_entity_status admitted =
                tile_entity_import_admit(tile_entity, context, bytes, error, "EntityId", SIZE_MAX);
            if (admitted != MC_TILE_ENTITY_OK) {
                return admitted;
            }
        }
        const mc_tile_entity_status created =
            mc_tile_entity_name_create(tag->value.string.units, tag->value.string.length, &name);
        if (created != MC_TILE_ENTITY_OK) {
            return tile_entity_status(error, created, "EntityId", SIZE_MAX);
        }
    }
    if (tag == NULL) {
        mc_registry_error attempt = live_empty_registry();
        mc_tile_entity_status empty =
            live_literal(tile_entity->classes, "", &name, &attempt, tile_entity, context);
        if (empty != MC_TILE_ENTITY_OK) {
            tile_entity_status(error, empty, "EntityId", SIZE_MAX);
            if (error != NULL)
                error->registry = attempt;
            return empty;
        }
    }
    const mc_tile_entity_status assigned = mc_tile_entity_spawner_set_name(tile_entity, name);
    mc_tile_entity_name_release(name);
    if (assigned != MC_TILE_ENTITY_OK) {
        return tile_entity_status(error, assigned, "EntityId", SIZE_MAX);
    }
    tag = mc_nbt_compound_get(compound, tile_entity_delay_name, 5);
    if (tag != NULL && tag->type != MC_NBT_SHORT) {
        return tile_entity_status(error, MC_TILE_ENTITY_INVALID_FIELD, "Delay", SIZE_MAX);
    }
    tile_entity->state.spawner.delay = tag == NULL ? 0 : tag->value.short_value;
    return tile_entity_status(error, MC_TILE_ENTITY_OK, NULL, SIZE_MAX);
}

static mc_tile_entity_status tile_entity_piston_read(mc_tile_entity *tile_entity,
                                                     const mc_nbt_tag *compound,
                                                     mc_tile_entity_error *error) {
    const mc_nbt_tag *tag = mc_nbt_compound_get(compound, tile_entity_block_id_name, 7);
    if (tag != NULL && tag->type != MC_NBT_INT)
        return tile_entity_read_error(error, "blockId", 9, false);
    tile_entity->state.piston.block_id = tag == NULL ? 0 : tag->value.int_value;
    tag = mc_nbt_compound_get(compound, tile_entity_block_data_name, 9);
    if (tag != NULL && tag->type != MC_NBT_INT)
        return tile_entity_read_error(error, "blockData", 19, false);
    tile_entity->state.piston.block_data = tag == NULL ? 0 : tag->value.int_value;
    tag = mc_nbt_compound_get(compound, tile_entity_facing_name, 6);
    if (tag != NULL && tag->type != MC_NBT_INT)
        return tile_entity_read_error(error, "facing", 29, false);
    tile_entity->state.piston.facing = tag == NULL ? 0 : tag->value.int_value;
    tag = mc_nbt_compound_get(compound, tile_entity_progress_name, 8);
    if (tag != NULL && tag->type != MC_NBT_FLOAT)
        return tile_entity_read_error(error, "progress", 40, false);
    tile_entity->state.piston.progress_bits = tag == NULL ? 0 : tag->value.float_bits;
    tile_entity->state.piston.previous_progress_bits = tile_entity->state.piston.progress_bits;
    tag = mc_nbt_compound_get(compound, tile_entity_extending_name, 9);
    if (tag != NULL && tag->type != MC_NBT_BYTE)
        return tile_entity_read_error(error, "extending", 54, false);
    tile_entity->state.piston.extending = tag != NULL && tag->value.byte_value != 0;
    /* j is neither read nor reset: an existing full constructor flag survives. */
    return tile_entity_status(error, MC_TILE_ENTITY_OK, NULL, SIZE_MAX);
}

static mc_tile_entity_status tile_entity_read_nbt(mc_tile_entity *tile_entity,
                                                  const mc_nbt_tag *compound,
                                                  tile_entity_import_context *context,
                                                  mc_tile_entity_error *error) {
    const mc_tile_entity_status status = mc_tile_entity_read_base(tile_entity, compound, error);
    if (status != MC_TILE_ENTITY_OK) {
        if (error != NULL && tile_entity != NULL && tile_entity->kind == MC_TILE_ENTITY_PISTON &&
            error->base_read_bytecode != 0)
            error->read_bytecode = 2;
        return status;
    }
    if (tile_entity->kind == MC_TILE_ENTITY_CHEST)
        return tile_entity_chest_read(tile_entity, compound, context, error);
    if (tile_entity->kind == MC_TILE_ENTITY_SPAWNER)
        return tile_entity_spawner_read(tile_entity, compound, context, error);
    if (tile_entity->kind == MC_TILE_ENTITY_PISTON)
        return tile_entity_piston_read(tile_entity, compound, error);
    return tile_entity_status(error, MC_TILE_ENTITY_INVALID_KIND, NULL, SIZE_MAX);
}

mc_tile_entity_status mc_tile_entity_read_nbt(mc_tile_entity *tile_entity,
                                              const mc_nbt_tag *compound,
                                              mc_tile_entity_error *error) {
    return tile_entity_read_nbt(tile_entity, compound, NULL, error);
}

static mc_tile_entity_status
tile_entity_attach_import_sources(mc_tile_entity *tile_entity, const mc_nbt_tag *compound,
                                  const tile_entity_import_context *context,
                                  mc_tile_entity_error *error) {
    if (tile_entity->source != NULL || tile_entity->world != NULL ||
        tile_entity->phase != MC_TILE_ENTITY_READY) {
        return tile_entity_status(error, MC_TILE_ENTITY_STATE_ERROR, NULL, SIZE_MAX);
    }
    if (tile_entity->kind == MC_TILE_ENTITY_CHEST) {
        if (tile_entity->state.chest.capacity != 27 || tile_entity->state.chest.slots == NULL) {
            return tile_entity_status(error, MC_TILE_ENTITY_STATE_ERROR, "Items", SIZE_MAX);
        }
        const mc_nbt_tag *items = mc_nbt_compound_get(compound, tile_entity_items_name, 5);
        for (size_t i = 0; i < 27; ++i) {
            const tile_entity_loaded_record *winner = &context->winners[i];
            if (tile_entity->state.chest.slots[i] != winner->stack) {
                return tile_entity_status(error, MC_TILE_ENTITY_STATE_ERROR, "Items[]",
                                          winner->index);
            }
            if (winner->stack == NULL) {
                continue;
            }
            if (items == NULL || items->type != MC_NBT_LIST ||
                winner->index >= items->value.list.length ||
                items->value.list.items[winner->index] != winner->record ||
                mc_item_stack_source(winner->stack) != NULL) {
                return tile_entity_status(error, MC_TILE_ENTITY_STATE_ERROR, "Items[]",
                                          winner->index);
            }
            /* Fresh import creates a new owner for every successful record.
             * Same-stack aliases are allowed later, but cannot arise here. */
            for (size_t j = 0; j < i; ++j) {
                if (context->winners[j].stack == winner->stack) {
                    return tile_entity_status(error, MC_TILE_ENTITY_STATE_ERROR, "Items[]",
                                              winner->index);
                }
            }
        }
    }

    size_t live;
    mc_tile_entity_status status = tile_entity_import_measure(tile_entity, context, &live);
    if (status != MC_TILE_ENTITY_OK) {
        return tile_entity_status(error, status, NULL, SIZE_MAX);
    }
    mc_nbt_limits limits = context->limits;
    if (live > limits.max_allocation_bytes) {
        return tile_entity_status(error, MC_TILE_ENTITY_LIMIT, NULL, SIZE_MAX);
    }
    limits.max_allocation_bytes -= live;
    mc_nbt_tree *source;
    mc_nbt_error nbt = {MC_NBT_OK, 0};
    const mc_nbt_status copied = mc_nbt_tree_copy_compound(compound, &limits, &source, &nbt);
    if (copied != MC_NBT_OK) {
        return tile_entity_nbt_error(error, copied, nbt, NULL, SIZE_MAX);
    }
    tile_entity->source = source;

    if (tile_entity->kind == MC_TILE_ENTITY_CHEST) {
        const mc_nbt_tag *items =
            mc_nbt_compound_get(mc_nbt_tree_root(source), tile_entity_items_name, 5);
        for (size_t i = 0; i < 27; ++i) {
            const tile_entity_loaded_record *winner = &context->winners[i];
            if (winner->stack == NULL) {
                continue;
            }
            if (tile_entity->state.chest.slots[i] != winner->stack || items == NULL ||
                items->type != MC_NBT_LIST || winner->index >= items->value.list.length) {
                return tile_entity_status(error, MC_TILE_ENTITY_STATE_ERROR, "Items[]",
                                          winner->index);
            }
            const mc_nbt_tag *record = items->value.list.items[winner->index];
            if (record == NULL || record->type != MC_NBT_COMPOUND || record->name.units != NULL ||
                record->name.length != 0) {
                return tile_entity_status(error, MC_TILE_ENTITY_STATE_ERROR, "Items[]",
                                          winner->index);
            }
            status = tile_entity_import_measure(tile_entity, context, &live);
            if (status != MC_TILE_ENTITY_OK) {
                return tile_entity_status(error, status, "Items[]", winner->index);
            }
            size_t item_bytes;
            mc_item_stack_status item = mc_item_stack_native_bytes(winner->stack, &item_bytes);
            if (item != MC_ITEM_STACK_OK) {
                return tile_entity_item_error(error, item, NULL, winner->index);
            }
            if (item_bytes > live) {
                return tile_entity_status(error, MC_TILE_ENTITY_STATE_ERROR, "Items[]",
                                          winner->index);
            }
            if (live > context->limits.max_allocation_bytes) {
                return tile_entity_status(error, MC_TILE_ENTITY_LIMIT, "Items[]", winner->index);
            }
            limits = context->limits;
            /* Existing preserve_source deducts I itself. Supply B-(A-I), so
             * its helper sees exactly B-A with all earlier copies still live. */
            limits.max_allocation_bytes -= live - item_bytes;
            mc_item_stack_error item_error;
            item = mc_item_stack_preserve_source(winner->stack, record, &limits, &item_error);
            if (item != MC_ITEM_STACK_OK) {
                return tile_entity_item_error(error, item, &item_error, winner->index);
            }
        }
    }
    return tile_entity_status(error, MC_TILE_ENTITY_OK, NULL, SIZE_MAX);
}

static mc_tile_entity_status
tile_entity_import_storage_scoped(mc_world_runtime *world, mc_tile_entity_kind resolved_kind,
                                  const mc_nbt_tag *compound, const mc_nbt_limits *requested_limits,
                                  mc_tile_entity **out, mc_tile_entity_error *error,
                                  const mc_world_chunk *scope_chunk) {
    if (out == NULL) {
        return tile_entity_status(error, MC_TILE_ENTITY_INVALID_ARGUMENT, NULL, SIZE_MAX);
    }
    if (resolved_kind != MC_TILE_ENTITY_CHEST && resolved_kind != MC_TILE_ENTITY_SPAWNER &&
        resolved_kind != MC_TILE_ENTITY_PISTON) {
        return tile_entity_status(error, MC_TILE_ENTITY_INVALID_KIND, NULL, SIZE_MAX);
    }
    tile_entity_import_context context = {0};
    context.scope_chunk = scope_chunk;
    if (!tile_entity_limits(requested_limits, &context.limits)) {
        return tile_entity_status(error, MC_TILE_ENTITY_INVALID_ARGUMENT, NULL, SIZE_MAX);
    }
    if (sizeof(mc_tile_entity) > context.limits.max_allocation_bytes) {
        return tile_entity_status(error, MC_TILE_ENTITY_LIMIT, NULL, SIZE_MAX);
    }
    if (scope_chunk != NULL) {
        mc_live_tile_entity_error graph_error;
        mc_live_tile_entities_error_init(&graph_error);
        size_t graph;
        if (!live_graph_measure(scope_chunk, NULL, context.limits.max_allocation_bytes, &graph,
                                &graph_error))
            return tile_entity_status(error,
                                      graph_error.status == MC_WORLD_STORAGE_LIMIT
                                          ? MC_TILE_ENTITY_LIMIT
                                          : MC_TILE_ENTITY_STATE_ERROR,
                                      NULL, SIZE_MAX);
        if (sizeof(mc_tile_entity) > context.limits.max_allocation_bytes - graph)
            return tile_entity_status(error, MC_TILE_ENTITY_LIMIT, NULL, SIZE_MAX);
    }
    mc_tile_entity *tile_entity;
    mc_tile_entity_status status = mc_tile_entity_allocate(world, resolved_kind, &tile_entity);
    if (status != MC_TILE_ENTITY_OK) {
        tile_entity_allocation_error(world, resolved_kind, status, error);
        if (error != NULL)
            error->source_bytecode = 25;
        return status;
    }
    unsigned int source_bytecode = 25;
    status = tile_entity_construct(tile_entity, &context, error);
    if (status == MC_TILE_ENTITY_OK) {
        source_bytecode = 46;
        status = tile_entity_read_nbt(tile_entity, compound, &context, error);
    }
    if (status == MC_TILE_ENTITY_OK) {
        status = tile_entity_attach_import_sources(tile_entity, compound, &context, error);
    }
    if (status != MC_TILE_ENTITY_OK) {
        if (error != NULL)
            error->source_bytecode = source_bytecode;
        mc_tile_entity_release(tile_entity);
        return status;
    }
    *out = tile_entity;
    return tile_entity_status(error, MC_TILE_ENTITY_OK, NULL, SIZE_MAX);
}

mc_tile_entity_status
mc_tile_entity_import_storage(mc_world_runtime *world, mc_tile_entity_kind resolved_kind,
                              const mc_nbt_tag *compound, const mc_nbt_limits *requested_limits,
                              mc_tile_entity **out, mc_tile_entity_error *error) {
    return tile_entity_import_storage_scoped(world, resolved_kind, compound, requested_limits, out,
                                             error, NULL);
}

static bool tile_entity_names_equal(mc_nbt_string left, mc_nbt_string right) {
    return left.length == right.length &&
           (left.length == 0 ||
            memcmp(left.units, right.units, left.length * sizeof(uint16_t)) == 0);
}

static mc_tile_entity_status tile_entity_overlay_pointer_count(const mc_nbt_tag *source,
                                                               const mc_nbt_tag *fields,
                                                               size_t field_count, size_t *out) {
    size_t count = source == NULL ? 0 : source->value.compound.length;
    for (size_t i = 0; i < field_count; ++i) {
        const mc_nbt_string name = fields[i].name;
        if (source == NULL || mc_nbt_compound_get(source, name.units, name.length) == NULL) {
            if (!tile_entity_size_add(&count, 1)) {
                return MC_TILE_ENTITY_LIMIT;
            }
        }
    }
    if (count > SIZE_MAX / sizeof(const mc_nbt_tag *)) {
        return MC_TILE_ENTITY_LIMIT;
    }
    *out = count;
    return MC_TILE_ENTITY_OK;
}

static void tile_entity_overlay(const mc_nbt_tag *source, const mc_nbt_tag *fields,
                                size_t field_count, const mc_nbt_tag **items) {
    size_t used = 0;
    if (source != NULL) {
        for (size_t i = 0; i < source->value.compound.length; ++i) {
            const mc_nbt_tag *old = source->value.compound.items[i];
            const mc_nbt_tag *replacement = old;
            for (size_t j = 0; j < field_count; ++j) {
                if (tile_entity_names_equal(old->name, fields[j].name)) {
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

static mc_tile_entity_status tile_entity_build_fields(const mc_tile_entity *tile_entity,
                                                      mc_nbt_tag fields[9], size_t *field_count,
                                                      mc_tile_entity_error *error) {
    fields[0].type = MC_NBT_STRING;
    fields[0].name = (mc_nbt_string){tile_entity_id_name, 2};
    mc_tile_entity_name *registered = live_class_name(tile_entity->classes, tile_entity->kind);
    if (registered == NULL) {
        const mc_tile_entity_status status =
            tile_entity_status(error, MC_TILE_ENTITY_STATE_ERROR, "id", SIZE_MAX);
        if (error != NULL)
            error->base_save_bytecode = 7;
        return status;
    }
    fields[0].value.string = registered->text;
    fields[1].type = fields[2].type = fields[3].type = MC_NBT_INT;
    fields[1].name = (mc_nbt_string){tile_entity_x_name, 1};
    fields[2].name = (mc_nbt_string){tile_entity_y_name, 1};
    fields[3].name = (mc_nbt_string){tile_entity_z_name, 1};
    fields[1].value.int_value = tile_entity->x;
    fields[2].value.int_value = tile_entity->y;
    fields[3].value.int_value = tile_entity->z;
    if (tile_entity->kind == MC_TILE_ENTITY_CHEST) {
        fields[4].type = MC_NBT_LIST;
        fields[4].name = (mc_nbt_string){tile_entity_items_name, 5};
        fields[4].value.list.element_type = MC_NBT_COMPOUND;
        *field_count = 5;
    } else if (tile_entity->kind == MC_TILE_ENTITY_SPAWNER) {
        /* The original String construction rejects null before reaching Delay.
         * This independent result has no caller-visible mutable base prefix. */
        if (tile_entity->state.spawner.name == NULL) {
            const mc_tile_entity_status status =
                tile_entity_status(error, MC_TILE_ENTITY_INVALID_ARGUMENT, "EntityId", SIZE_MAX);
            if (error != NULL)
                error->save_bytecode = 12;
            return status;
        }
        fields[4].type = MC_NBT_STRING;
        fields[4].name = (mc_nbt_string){tile_entity_name, 8};
        fields[4].value.string = tile_entity->state.spawner.name->text;
        fields[5].type = MC_NBT_SHORT;
        fields[5].name = (mc_nbt_string){tile_entity_delay_name, 5};
        fields[5].value.short_value = mc_java_i16_narrow(tile_entity->state.spawner.delay);
        *field_count = 6;
    } else if (tile_entity->kind == MC_TILE_ENTITY_PISTON) {
        fields[4].type = fields[5].type = fields[6].type = MC_NBT_INT;
        fields[4].name = (mc_nbt_string){tile_entity_block_id_name, 7};
        fields[5].name = (mc_nbt_string){tile_entity_block_data_name, 9};
        fields[6].name = (mc_nbt_string){tile_entity_facing_name, 6};
        fields[4].value.int_value = tile_entity->state.piston.block_id;
        fields[5].value.int_value = tile_entity->state.piston.block_data;
        fields[6].value.int_value = tile_entity->state.piston.facing;
        fields[7].type = MC_NBT_FLOAT;
        fields[7].name = (mc_nbt_string){tile_entity_progress_name, 8};
        /* Actual b(Compound) serializes previous l, not current k. Encoding
         * canonicalizes NaN using the existing ordinary FloatTag codec. */
        fields[7].value.float_bits = tile_entity->state.piston.previous_progress_bits;
        fields[8].type = MC_NBT_BYTE;
        fields[8].name = (mc_nbt_string){tile_entity_extending_name, 9};
        fields[8].value.byte_value = tile_entity->state.piston.extending ? 1 : 0;
        *field_count = 9;
    } else {
        return tile_entity_status(error, MC_TILE_ENTITY_INVALID_KIND, NULL, SIZE_MAX);
    }
    return MC_TILE_ENTITY_OK;
}

mc_tile_entity_status mc_tile_entity_build(const mc_tile_entity *tile_entity,
                                           const mc_nbt_limits *requested_limits,
                                           mc_nbt_document **out, mc_tile_entity_error *error) {
    if (out == NULL) {
        return tile_entity_status(error, MC_TILE_ENTITY_INVALID_ARGUMENT, NULL, SIZE_MAX);
    }
    mc_tile_entity_status result = tile_entity_ready(tile_entity);
    if (result != MC_TILE_ENTITY_OK) {
        return tile_entity_status(error, result, NULL, SIZE_MAX);
    }
    mc_nbt_limits limits;
    if (!tile_entity_limits(requested_limits, &limits)) {
        return tile_entity_status(error, MC_TILE_ENTITY_INVALID_ARGUMENT, NULL, SIZE_MAX);
    }
    size_t live;
    result = mc_tile_entity_storage_bytes(tile_entity, &live);
    if (result != MC_TILE_ENTITY_OK) {
        return tile_entity_status(error, result, NULL, SIZE_MAX);
    }
    if (live > limits.max_allocation_bytes) {
        return tile_entity_status(error, MC_TILE_ENTITY_LIMIT, NULL, SIZE_MAX);
    }
    limits.max_allocation_bytes -= live;

    mc_nbt_tag fields[9] = {0};
    size_t field_count;
    result = tile_entity_build_fields(tile_entity, fields, &field_count, error);
    if (result != MC_TILE_ENTITY_OK) {
        return result;
    }
    const mc_nbt_tag *source = mc_nbt_tree_root(tile_entity->source);
    size_t root_count;
    result = tile_entity_overlay_pointer_count(source, fields, field_count, &root_count);
    if (result != MC_TILE_ENTITY_OK || root_count > limits.max_elements) {
        return tile_entity_status(error, MC_TILE_ENTITY_LIMIT, NULL, SIZE_MAX);
    }

    /* Bounded physical storage, validated by storage_bytes: one stable view and
     * Slot tag PER ENTRY, even when several entries share a stack identity. */
    mc_item_stack_record_view records[36];
    mc_nbt_tag slots[36] = {0};
    const mc_item_stack *stacks[36];
    size_t child_counts[36];
    size_t occupied = 0;
    size_t pointer_count = root_count;
    if (tile_entity->kind == MC_TILE_ENTITY_CHEST) {
        for (size_t i = 0; i < tile_entity->state.chest.capacity; ++i) {
            const mc_item_stack *stack = tile_entity->state.chest.slots[i];
            if (stack == NULL) {
                continue;
            }
            slots[occupied].type = MC_NBT_BYTE;
            slots[occupied].name = (mc_nbt_string){tile_entity_slot_name, 4};
            /* i is in 0..35, so its int32 conversion is exact. */
            slots[occupied].value.byte_value = mc_java_i8_narrow((int32_t)i);
            stacks[occupied] = stack;
            mc_item_stack_error item_error;
            const mc_item_stack_status item = mc_item_stack_record_pointer_count(
                stack, &slots[occupied], 1, &child_counts[occupied], &item_error);
            if (item != MC_ITEM_STACK_OK) {
                return tile_entity_item_error(error, item, &item_error, occupied);
            }
            if (child_counts[occupied] > limits.max_elements ||
                !tile_entity_size_add(&pointer_count, child_counts[occupied])) {
                return tile_entity_status(error, MC_TILE_ENTITY_LIMIT, "Items[]", occupied);
            }
            ++occupied;
        }
        if (occupied > limits.max_elements || !tile_entity_size_add(&pointer_count, occupied)) {
            return tile_entity_status(error, MC_TILE_ENTITY_LIMIT, "Items", SIZE_MAX);
        }
    }
    if (pointer_count > SIZE_MAX / sizeof(const mc_nbt_tag *)) {
        return tile_entity_status(error, MC_TILE_ENTITY_LIMIT, NULL, SIZE_MAX);
    }
    /* Every slice holds the same pointer type, so malloc alignment and pointer
     * multiples suffice; there is no tag/byte-buffer packing to misalign. */
    const size_t scratch_bytes = pointer_count * sizeof(const mc_nbt_tag *);
    if (scratch_bytes > limits.max_allocation_bytes) {
        return tile_entity_status(error, MC_TILE_ENTITY_LIMIT, NULL, SIZE_MAX);
    }
    const mc_nbt_tag **scratch = malloc(scratch_bytes);
    if (scratch == NULL) {
        return tile_entity_status(error, MC_TILE_ENTITY_ALLOCATION_FAILED, NULL, SIZE_MAX);
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
            return tile_entity_item_error(error, item, &item_error, i);
        }
        scratch[root_count + i] = &records[i].root;
        child_offset += child_counts[i];
    }
    if (tile_entity->kind == MC_TILE_ENTITY_CHEST) {
        fields[4].value.list.items = occupied == 0 ? NULL : scratch + root_count;
        fields[4].value.list.length = occupied;
    }
    tile_entity_overlay(source, fields, field_count, scratch);
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
                result = MC_TILE_ENTITY_STATE_ERROR;
            } else {
                size_t consumed = 0;
                /* max_input_bytes applies here, never to compound import. */
                status =
                    mc_nbt_read_compound(encoded, written, &limits, &document, &consumed, &nbt);
                if (status == MC_NBT_OK && consumed != written) {
                    mc_nbt_document_free(document);
                    document = NULL;
                    result = MC_TILE_ENTITY_STATE_ERROR;
                }
            }
        }
    }
    free(encoded);
    free(scratch);
    if (result != MC_TILE_ENTITY_OK) {
        return tile_entity_status(error, result, NULL, SIZE_MAX);
    }
    if (status != MC_NBT_OK) {
        return tile_entity_nbt_error(error, status, nbt, NULL, SIZE_MAX);
    }
    *out = document;
    return tile_entity_status(error, MC_TILE_ENTITY_OK, NULL, SIZE_MAX);
}

/* The common World class owns this lazy ow/jh class lifetime. There is one
 * registry, not one registry per TileEntity or a separately bound class graph. */
static mc_tile_entity_status live_literal(mc_tile_entity_classes *classes, const char *ascii,
                                          mc_tile_entity_name **out, mc_registry_error *attempt,
                                          const mc_tile_entity *scope_tile_entity,
                                          const tile_entity_import_context *context) {
    *attempt = live_empty_registry();
    const mc_registry_string *identity = NULL;
    mc_registry_error cause = live_empty_registry();
    mc_registry_status selected =
        mc_registry_runtime_literal_ascii(classes->registry, ascii, &identity, &cause);
    if (selected != MC_REGISTRY_OK) {
        *attempt = cause;
        return selected == MC_REGISTRY_LIMIT               ? MC_TILE_ENTITY_LIMIT
               : selected == MC_REGISTRY_ALLOCATION_FAILED ? MC_TILE_ENTITY_ALLOCATION_FAILED
                                                           : MC_TILE_ENTITY_WORLD_FAILED;
    }
    for (mc_tile_entity_name *name = classes->literals; name != NULL; name = name->literal_next) {
        if (name->literal_identity == identity) {
            if (context != NULL && scope_tile_entity != NULL) {
                const bool already =
                    context->scope_chunk == NULL
                        ? scope_tile_entity->kind == MC_TILE_ENTITY_SPAWNER &&
                              scope_tile_entity->state.spawner.name == name
                        : live_graph_has_name(context->scope_chunk, scope_tile_entity, name);
                const mc_tile_entity_status admitted =
                    tile_entity_import_admit(scope_tile_entity, context,
                                             already ? 0 : mc_tile_entity_name_native_bytes(name),
                                             NULL, "EntityId", SIZE_MAX);
                if (admitted != MC_TILE_ENTITY_OK)
                    return admitted;
            }
            mc_tile_entity_status held = mc_tile_entity_name_retain(name);
            if (held == MC_TILE_ENTITY_OK)
                *out = name;
            return held;
        }
    }
    const size_t ceiling = 1024u * 1024u;
    if (classes->allocation_bytes > ceiling ||
        sizeof(mc_tile_entity_name) > ceiling - classes->allocation_bytes)
        return MC_TILE_ENTITY_LIMIT;
    if (context != NULL && scope_tile_entity != NULL) {
        const mc_tile_entity_status admitted = tile_entity_import_admit(
            scope_tile_entity, context, sizeof(mc_tile_entity_name), NULL, "EntityId", SIZE_MAX);
        if (admitted != MC_TILE_ENTITY_OK)
            return admitted;
    }
    mc_registry_string_view view;
    if (!mc_registry_string_read(identity, &view))
        return MC_TILE_ENTITY_STATE_ERROR;
    mc_tile_entity_name *name = calloc(1, sizeof(*name));
    if (name == NULL)
        return MC_TILE_ENTITY_ALLOCATION_FAILED;
    name->holds = 2; /* Actual class-owned pool occurrence and returned hold. */
    name->allocation_bytes = sizeof(*name);
    name->text = (mc_nbt_string){view.units, view.length};
    name->literal_identity = identity;
    name->literal_next = classes->literals;
    classes->literals = name;
    classes->allocation_bytes += sizeof(*name);
    *out = name;
    return MC_TILE_ENTITY_OK;
}

static mc_tile_entity_status live_classes_owner_in(mc_world_runtime_classes *parent,
                                                   mc_tile_entity_classes **out) {
    if (parent == NULL)
        return MC_TILE_ENTITY_INVALID_ARGUMENT;
    if (parent->tile_entities == NULL) {
        mc_tile_entity_classes *classes = calloc(1, sizeof(*classes));
        if (classes == NULL)
            return MC_TILE_ENTITY_ALLOCATION_FAILED;
        classes->parent = parent;
        classes->registry = parent->registry;
        for (size_t i = 0; i < (size_t)MC_TILE_ENTITY_CLASS_KIND_COUNT; ++i)
            classes->class_literals[i] =
                (mc_tile_entity_class_literal){classes, (mc_tile_entity_class_kind)i};
        classes->allocation_bytes = sizeof(*classes);
        classes->piston_max_allocation_bytes = 1024u * 1024u;
        parent->tile_entities = classes; /* Native owner publication, no Java field. */
    }
    mc_tile_entity_classes *classes = parent->tile_entities;
    *out = classes;
    return MC_TILE_ENTITY_OK;
}

mc_tile_entity_status mc_tile_entity_classes_borrow(mc_world_runtime_classes *parent,
                                                    mc_tile_entity_classes **out) {
    if (parent == NULL || out == NULL)
        return MC_TILE_ENTITY_INVALID_ARGUMENT;
    mc_tile_entity_classes *classes = NULL;
    const mc_tile_entity_status status = live_classes_owner_in(parent, &classes);
    if (status != MC_TILE_ENTITY_OK)
        return status;
    if (classes->class_literal_borrows == SIZE_MAX)
        return MC_TILE_ENTITY_LIMIT;
    ++classes->class_literal_borrows;
    *out = classes;
    return MC_TILE_ENTITY_OK;
}

mc_tile_entity_status mc_tile_entity_classes_release(mc_tile_entity_classes **slot) {
    if (slot == NULL)
        return MC_TILE_ENTITY_INVALID_ARGUMENT;
    mc_tile_entity_classes *classes = *slot;
    if (classes == NULL)
        return MC_TILE_ENTITY_OK;
    if (classes->class_literal_borrows == 0)
        return MC_TILE_ENTITY_INVALID_ARGUMENT;
    --classes->class_literal_borrows;
    *slot = NULL;
    return MC_TILE_ENTITY_OK;
}

static bool class_literal_is_canonical(const mc_tile_entity_class_literal *literal) {
    return literal != NULL && literal->owner != NULL && literal->owner->parent != NULL &&
           literal->owner->parent->tile_entities == literal->owner &&
           (unsigned int)literal->kind < (unsigned int)MC_TILE_ENTITY_CLASS_KIND_COUNT &&
           literal == &literal->owner->class_literals[(size_t)literal->kind];
}

mc_tile_entity_status mc_tile_entity_class_literal_get(const mc_tile_entity_classes *classes,
                                                       mc_tile_entity_class_kind kind,
                                                       const mc_tile_entity_class_literal **out) {
    if (classes == NULL || out == NULL)
        return MC_TILE_ENTITY_INVALID_ARGUMENT;
    if ((unsigned int)kind >= (unsigned int)MC_TILE_ENTITY_CLASS_KIND_COUNT)
        return MC_TILE_ENTITY_INVALID_KIND;
    *out = &classes->class_literals[(size_t)kind];
    return MC_TILE_ENTITY_OK;
}

mc_tile_entity_status
mc_tile_entity_class_literal_inspect(const mc_tile_entity_class_literal *literal,
                                     mc_tile_entity_class_literal_view *out) {
    if (out == NULL || !class_literal_is_canonical(literal))
        return MC_TILE_ENTITY_INVALID_ARGUMENT;
    const size_t index = (size_t)literal->kind;
    *out = (mc_tile_entity_class_literal_view){
        .owner = literal->owner,
        .world_classes = literal->owner->parent,
        .side = literal->owner->parent->side,
        .kind = literal->kind,
        .client_name = class_literal_client_names[index],
        .server_name = class_literal_server_names[index],
        .superclass_name = literal->kind == MC_TILE_ENTITY_CLASS_KIND_TILE_ENTITY
                               ? "java.lang.Object"
                               : (literal->owner->parent->side == MC_WORLD_INFO_CLIENT
                                      ? class_literal_client_names[0]
                                      : class_literal_server_names[0])};
    return MC_TILE_ENTITY_OK;
}

mc_tile_entity_status
mc_tile_entity_class_literal_superclass(const mc_tile_entity_class_literal *literal,
                                        const mc_tile_entity_class_literal **out) {
    if (out == NULL || !class_literal_is_canonical(literal))
        return MC_TILE_ENTITY_INVALID_ARGUMENT;
    if (literal->kind == MC_TILE_ENTITY_CLASS_KIND_TILE_ENTITY)
        return MC_TILE_ENTITY_INVALID_KIND; /* Actual java/lang/Object is outside this namespace. */
    *out = &literal->owner->class_literals[MC_TILE_ENTITY_CLASS_KIND_TILE_ENTITY];
    return MC_TILE_ENTITY_OK;
}

mc_tile_entity_status mc_tile_entity_get_class(const mc_tile_entity *tile_entity,
                                               const mc_tile_entity_class_literal **out) {
    if (tile_entity == NULL || out == NULL || tile_entity->classes == NULL)
        return MC_TILE_ENTITY_INVALID_ARGUMENT;
    mc_tile_entity_class_kind kind;
    switch (tile_entity->kind) {
        case MC_TILE_ENTITY_CHEST:
            kind = MC_TILE_ENTITY_CLASS_KIND_CHEST;
            break;
        case MC_TILE_ENTITY_SPAWNER:
            kind = MC_TILE_ENTITY_CLASS_KIND_SPAWNER;
            break;
        case MC_TILE_ENTITY_PISTON:
            kind = MC_TILE_ENTITY_CLASS_KIND_PISTON;
            break;
        default:
            return MC_TILE_ENTITY_INVALID_KIND;
    }
    return mc_tile_entity_class_literal_get(tile_entity->classes, kind, out);
}

static mc_tile_entity_status live_classes_owner(mc_world_runtime *world,
                                                mc_tile_entity_classes **out) {
    if (world == NULL || world->classes == NULL ||
        world->construction.mode != MC_WORLD_RUNTIME_GAME ||
        world->construction.phase == MC_WORLD_RUNTIME_RETIRED)
        return MC_TILE_ENTITY_INVALID_ARGUMENT;
    return live_classes_owner_in(world->classes, out);
}

mc_tile_entity_status mc_tile_entity_piston_class_configure(mc_world_runtime *world,
                                                            size_t max_allocation_bytes) {
    mc_tile_entity_classes *classes = NULL;
    const mc_tile_entity_status status = live_classes_owner(world, &classes);
    if (status != MC_TILE_ENTITY_OK)
        return status;
    if (classes->piston_phase != MC_TILE_ENTITY_CLASS_COLD || classes->piston_configured)
        return MC_TILE_ENTITY_INVALID_PHASE;
    classes->piston_max_allocation_bytes = max_allocation_bytes;
    classes->piston_configured = true;
    return MC_TILE_ENTITY_OK;
}

mc_tile_entity_status
mc_tile_entity_piston_class_get_snapshot(const mc_world_runtime_classes *parent,
                                         mc_tile_entity_piston_class_snapshot *out) {
    if (parent == NULL || out == NULL)
        return MC_TILE_ENTITY_INVALID_ARGUMENT;
    mc_tile_entity_piston_class_snapshot view = {0};
    view.max_allocation_bytes = 1024u * 1024u;
    const mc_tile_entity_classes *classes = parent->tile_entities;
    if (classes != NULL) {
        view.base_phase = (mc_tile_entity_class_phase)classes->phase;
        view.piston_phase = (mc_tile_entity_class_phase)classes->piston_phase;
        view.base_initializer_bytecode = classes->initializer_pc;
        view.piston_initializer_bytecode = classes->piston_initializer_pc;
        view.base_failure = classes->failure;
        view.piston_failure = classes->piston_failure;
        view.base_name_count = classes->name_count;
        view.base_class_count = classes->class_count;
        view.list_identity = classes->piston_list;
        if (classes->piston_list != NULL) {
            view.list_count = classes->piston_list->count;
            view.list_capacity = classes->piston_list->capacity;
        }
        view.allocation_bytes = classes->piston_allocation_bytes;
        view.max_allocation_bytes = classes->piston_max_allocation_bytes;
        view.live_tile_entities = classes->live_tile_entities;
        view.list_mod_count_bits = classes->piston_mod_count;
        view.class_literal_borrows = classes->class_literal_borrows;
    }
    *out = view;
    return MC_TILE_ENTITY_OK;
}

static mc_tile_entity_status live_piston_classes_get(mc_tile_entity_classes *classes) {
    if (classes->piston_phase == MC_TILE_ENTITY_CLASS_READY)
        return MC_TILE_ENTITY_OK;
    if (classes->piston_phase == MC_TILE_ENTITY_CLASS_FAILED)
        return classes->piston_failure;
    if (classes->piston_phase != MC_TILE_ENTITY_CLASS_COLD ||
        classes->phase != MC_TILE_ENTITY_CLASS_READY)
        return MC_TILE_ENTITY_STATE_ERROR;
    classes->piston_phase = MC_TILE_ENTITY_CLASS_INITIALIZING;
    classes->piston_initializer_pc = 0; /* Actual new ArrayList, before publication7. */
    mc_tile_entity_status status = MC_TILE_ENTITY_OK;
    mc_world_reference_list *list = NULL;
    if (sizeof(*list) > classes->piston_max_allocation_bytes)
        status = MC_TILE_ENTITY_LIMIT;
    else {
        list = calloc(1, sizeof(*list));
        if (list == NULL)
            status = MC_TILE_ENTITY_ALLOCATION_FAILED;
    }
    if (status != MC_TILE_ENTITY_OK) {
        classes->piston_failure = status;
        classes->piston_phase = MC_TILE_ENTITY_CLASS_FAILED;
        return status;
    }
    classes->piston_allocation_bytes = sizeof(*list);
    classes->piston_initializer_pc = 4; /* Empty native ArrayList constructor. */
    classes->piston_initializer_pc = 7;
    classes->piston_list = list; /* Actual shared static m, distinct from every World List. */
    classes->piston_list_identity = list;
    classes->piston_initializer_pc = 10;
    classes->piston_phase = MC_TILE_ENTITY_CLASS_READY;
    return MC_TILE_ENTITY_OK;
}

static mc_world_storage_status piston_list_fail(mc_piston_list_error *error,
                                                mc_world_storage_status status,
                                                const char *operation, size_t index,
                                                mc_entity_status entity_status) {
    *error = (mc_piston_list_error){status, operation, entity_status, index};
    return status;
}

static mc_world_storage_status piston_list_finish(const mc_piston_list_error *frame,
                                                  mc_piston_list_error *error) {
    if (error != NULL)
        *error = *frame;
    return frame->status;
}

static mc_world_storage_status piston_entity_status(mc_entity_status status) {
    switch (status) {
        case MC_ENTITY_LIMIT:
            return MC_WORLD_STORAGE_LIMIT;
        case MC_ENTITY_ALLOCATION_FAILED:
            return MC_WORLD_STORAGE_ALLOCATION_FAILED;
        case MC_ENTITY_BUSY:
            return MC_WORLD_STORAGE_BUSY;
        case MC_ENTITY_INVALID_ARGUMENT:
            return MC_WORLD_STORAGE_INVALID_ARGUMENT;
        default:
            return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    }
}

/* The source descriptor never proves ownership of an arbitrary void* span.
 * Its identity, backing and metadata must agree with the private allocations.
 * Occurrences are intentionally not inspected by this constant-time check. */
static bool piston_list_shape(const mc_tile_entity_classes *classes, bool current_static) {
    if (classes == NULL || classes->phase != MC_TILE_ENTITY_CLASS_READY ||
        classes->piston_phase != MC_TILE_ENTITY_CLASS_READY || classes->parent == NULL ||
        classes->parent->tile_entities != classes || classes->piston_list_identity == NULL ||
        (current_static && classes->piston_list != classes->piston_list_identity))
        return false;
    const mc_world_reference_list *list = classes->piston_list_identity;
    if (list->items != classes->piston_items || list->count != classes->piston_count ||
        list->capacity != classes->piston_capacity || list->count > list->capacity ||
        list->capacity > (size_t)INT32_MAX ||
        ((list->capacity == 0) != (classes->piston_items == NULL)) ||
        ((list->capacity == 0) != (classes->piston_entities == NULL)) ||
        (classes->piston_items != NULL &&
         (void *)classes->piston_items == (void *)classes->piston_entities))
        return false;
    size_t bytes;
    if (!tile_entity_size_multiply(
            list->capacity, sizeof(*classes->piston_items) + sizeof(*classes->piston_entities),
            &bytes) ||
        !tile_entity_size_add(&bytes, sizeof(*list)))
        return false;
    return bytes == classes->piston_allocation_bytes &&
           bytes <= classes->piston_max_allocation_bytes;
}

static bool piston_list_occurrences(const mc_tile_entity_classes *classes, size_t *bad_index) {
    for (size_t i = 0; i < classes->piston_count; ++i)
        if (classes->piston_entities[i] == NULL ||
            classes->piston_items[i] != classes->piston_entities[i]) {
            if (bad_index != NULL)
                *bad_index = i;
            return false;
        }
    return true;
}

static bool piston_list_retire_valid(const mc_tile_entity_classes *classes) {
    if (classes->piston_list_identity != NULL)
        return piston_list_shape(classes, true) && piston_list_occurrences(classes, NULL);
    return (classes->piston_phase == MC_TILE_ENTITY_CLASS_COLD ||
            classes->piston_phase == MC_TILE_ENTITY_CLASS_FAILED) &&
           classes->piston_list == NULL && classes->piston_items == NULL &&
           classes->piston_entities == NULL && classes->piston_count == 0 &&
           classes->piston_capacity == 0 && classes->piston_mod_count == 0 &&
           classes->piston_allocation_bytes == 0;
}

mc_world_storage_status mc_tile_entity_piston_terminal_visit_entities(
    const mc_world_runtime_classes *parent, mc_world_terminal_entity_visitor visitor, void *context,
    mc_tile_entity_terminal_class_inventory *out, mc_world_storage_error *error) {
    mc_world_storage_error local = {0};
    if (error == NULL)
        error = &local;
    (void)tile_terminal_result(error, MC_WORLD_STORAGE_OK, NULL);
    if (parent == NULL || visitor == NULL || out == NULL)
        return tile_terminal_result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                                    "terminal Piston class inventory arguments");
    const mc_tile_entity_classes *classes = parent->tile_entities;
    mc_tile_entity_terminal_class_inventory inventory = {0};
    if (classes != NULL) {
        if (classes->parent != parent || !piston_list_retire_valid(classes))
            return tile_terminal_result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                        "terminal Piston class owning List invariant");
        inventory.live_tiles = classes->live_tile_entities;
        inventory.piston_occurrences = classes->piston_count;
        /* Only the typed ledger owns the Entity releases. Source items and
         * iterator aliases do not add another static-m occurrence. */
        for (size_t i = 0; i < classes->piston_count; ++i) {
            const mc_world_storage_status status =
                visitor(context, &classes->piston_entities[i], classes->piston_entities[i], error);
            if (status != MC_WORLD_STORAGE_OK)
                return status;
        }
    }
    /* An absent static-m backing contributes no Entity occurrences; an existing
     * namespace still reports its actual live Tile count. No class creation,
     * active use or source List clear occurs here. */
    *out = inventory;
    return MC_WORLD_STORAGE_OK;
}

static bool piston_list_owner(mc_tile_entity *tile_entity, mc_tile_entity_classes **out,
                              mc_piston_list_error *error) {
    if (mc_tile_entity_piston_code_classes_held(tile_entity) == NULL) {
        piston_list_fail(error,
                         tile_entity == NULL ? MC_WORLD_STORAGE_INVALID_ARGUMENT
                                             : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                         "Piston List held TileEntity code owner", SIZE_MAX, MC_ENTITY_OK);
        return false;
    }
    if (!piston_list_shape(tile_entity->classes, true)) {
        piston_list_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                         "Piston static List owning descriptor", SIZE_MAX, MC_ENTITY_OK);
        return false;
    }
    *out = tile_entity->classes;
    return true;
}

static bool piston_list_peak(const mc_tile_entity_classes *classes, size_t extra) {
    return classes->piston_allocation_bytes <= classes->piston_max_allocation_bytes &&
           extra <= classes->piston_max_allocation_bytes - classes->piston_allocation_bytes;
}

/* Exact captured JDK17 default-empty/grow/ArraysSupport integer selection.
 * Requested-byte admission below remains an independent native policy. */
static size_t piston_list_grown_capacity(size_t old, size_t minimum) {
    if (old == 0)
        return minimum < 10u ? 10u : minimum;
    const int32_t old_length = (int32_t)old;
    const int32_t min_growth = (int32_t)(minimum - old);
    const int32_t preferred = (int32_t)(old >> 1u);
    const int32_t growth = min_growth > preferred ? min_growth : preferred;
    const int32_t length = mc_java_i32_add(old_length, growth);
    const int32_t soft_maximum = INT32_MAX - 8;
    if (length > 0 && length <= soft_maximum)
        return (size_t)length;
    /* minimum was admitted <=INT32_MAX before entering growth. Thus the
     * hugeLength minLength sum is positive and cannot take its OOME branch. */
    return minimum <= (size_t)soft_maximum ? (size_t)soft_maximum : minimum;
}

mc_world_storage_status
mc_tile_entity_piston_entities_append(mc_tile_entity *tile_entity,
                                      const mc_world_entity_list_view *source,
                                      mc_piston_list_error *error) {
    mc_piston_list_error frame = {.index = SIZE_MAX};
    mc_tile_entity_classes *classes = NULL;
    if (!piston_list_owner(tile_entity, &classes, &frame))
        return piston_list_finish(&frame, error);
    size_t count;
    mc_world_storage_status status = mc_world_entity_list_view_count(source, &count);
    if (status != MC_WORLD_STORAGE_OK) {
        piston_list_fail(&frame, status, "Piston addAll source snapshot size", SIZE_MAX,
                         MC_ENTITY_OK);
        return piston_list_finish(&frame, error);
    }
    size_t snapshot_bytes;
    if (!tile_entity_size_multiply(count, sizeof(mc_entity *), &snapshot_bytes) ||
        !piston_list_peak(classes, snapshot_bytes)) {
        piston_list_fail(&frame, MC_WORLD_STORAGE_LIMIT, "Piston addAll source snapshot budget",
                         SIZE_MAX, MC_ENTITY_OK);
        return piston_list_finish(&frame, error);
    }
    mc_entity **snapshot = count == 0 ? NULL : malloc(snapshot_bytes);
    if (count != 0 && snapshot == NULL) {
        piston_list_fail(&frame, MC_WORLD_STORAGE_ALLOCATION_FAILED,
                         "Piston addAll source snapshot allocation", SIZE_MAX, MC_ENTITY_OK);
        return piston_list_finish(&frame, error);
    }
    classes->piston_allocation_bytes += snapshot_bytes;
    size_t prepared = 0;
    void **new_items = NULL;
    mc_entity **new_entities = NULL;
    size_t item_bytes = 0, entity_bytes = 0;
    for (; prepared < count; ++prepared) {
        mc_entity *entity = NULL;
        status = mc_world_entity_list_view_get(source, prepared, &entity);
        if (status != MC_WORLD_STORAGE_OK) {
            piston_list_fail(&frame, status, "Piston addAll source snapshot occurrence", prepared,
                             MC_ENTITY_OK);
            goto finish;
        }
        const mc_entity_status held = mc_entity_retain(entity);
        if (held != MC_ENTITY_OK) {
            piston_list_fail(&frame, piston_entity_status(held),
                             "Piston addAll source snapshot Entity hold", prepared, held);
            goto finish;
        }
        snapshot[prepared] = entity;
    }
    /* Actual addAll.toArray1 completed before modCount14, including empty
     * snapshots. Native snapshot/hold failures above leave modCount intact. */
    classes->piston_mod_count += UINT32_C(1);
    if (count == 0)
        goto finish;
    if (count > (size_t)INT32_MAX - classes->piston_count) {
        piston_list_fail(&frame, MC_WORLD_STORAGE_LIMIT, "Piston addAll requested List size",
                         SIZE_MAX, MC_ENTITY_OK);
        goto finish;
    }
    const size_t total = classes->piston_count + count;
    if (total > classes->piston_capacity) {
        const size_t capacity = piston_list_grown_capacity(classes->piston_capacity, total);
        size_t bytes;
        if (!tile_entity_size_multiply(capacity, sizeof(*new_items) + sizeof(*new_entities),
                                       &bytes) ||
            !piston_list_peak(classes, bytes)) {
            piston_list_fail(&frame, MC_WORLD_STORAGE_LIMIT, "Piston addAll growth peak budget",
                             SIZE_MAX, MC_ENTITY_OK);
            goto finish;
        }
        item_bytes = capacity * sizeof(*new_items);
        entity_bytes = capacity * sizeof(*new_entities);
        new_items = malloc(item_bytes);
        if (new_items == NULL) {
            piston_list_fail(&frame, MC_WORLD_STORAGE_ALLOCATION_FAILED,
                             "Piston addAll backing allocation", SIZE_MAX, MC_ENTITY_OK);
            goto finish;
        }
        classes->piston_allocation_bytes += item_bytes;
        new_entities = malloc(entity_bytes);
        if (new_entities == NULL) {
            piston_list_fail(&frame, MC_WORLD_STORAGE_ALLOCATION_FAILED,
                             "Piston addAll ownership ledger allocation", SIZE_MAX, MC_ENTITY_OK);
            goto finish;
        }
        classes->piston_allocation_bytes += entity_bytes;
        /* Java's newly allocated Object[] exposes NULL in every unused slot.
         * Initialize pointer values before publishing this native backing. */
        for (size_t i = 0; i < capacity; ++i) {
            new_items[i] = NULL;
            new_entities[i] = NULL;
        }
        if (classes->piston_count != 0) {
            memcpy(new_items, classes->piston_items, classes->piston_count * sizeof(*new_items));
            memcpy(new_entities, classes->piston_entities,
                   classes->piston_count * sizeof(*new_entities));
        }
        const size_t old_bytes =
            classes->piston_capacity * (sizeof(*new_items) + sizeof(*new_entities));
        free(classes->piston_items);
        free(classes->piston_entities);
        classes->piston_items = new_items;
        classes->piston_entities = new_entities;
        classes->piston_capacity = capacity;
        classes->piston_list_identity->items = new_items;
        classes->piston_list_identity->capacity = capacity;
        classes->piston_allocation_bytes -= old_bytes;
        new_items = NULL;
        new_entities = NULL;
    }
    for (size_t i = 0; i < count; ++i) {
        const size_t index = classes->piston_count + i;
        classes->piston_items[index] = snapshot[i];
        classes->piston_entities[index] = snapshot[i];
    }
    classes->piston_count = total;
    classes->piston_list_identity->count = total; /* Actual addAll size71. */
    prepared = 0; /* All snapshot holds now belong to their m occurrences. */
finish:
    if (new_items != NULL) {
        free(new_items);
        classes->piston_allocation_bytes -= item_bytes;
    }
    if (new_entities != NULL) {
        free(new_entities);
        classes->piston_allocation_bytes -= entity_bytes;
    }
    for (size_t i = 0; i < prepared; ++i)
        mc_entity_release(snapshot[i]);
    free(snapshot);
    classes->piston_allocation_bytes -= snapshot_bytes;
    return piston_list_finish(&frame, error);
}

mc_world_storage_status mc_tile_entity_piston_entities_iterator(mc_tile_entity *tile_entity,
                                                                mc_piston_entity_iterator *out,
                                                                mc_piston_list_error *error) {
    mc_piston_list_error frame = {.index = SIZE_MAX};
    mc_tile_entity_classes *classes = NULL;
    if (out == NULL)
        piston_list_fail(&frame, MC_WORLD_STORAGE_INVALID_ARGUMENT, "Piston List iterator output",
                         SIZE_MAX, MC_ENTITY_OK);
    else if (piston_list_owner(tile_entity, &classes, &frame))
        *out = (mc_piston_entity_iterator){classes, classes->piston_list_identity, 0,
                                           classes->piston_mod_count};
    return piston_list_finish(&frame, error);
}

static bool piston_iterator_owner(const mc_piston_entity_iterator *iterator,
                                  mc_piston_list_error *error) {
    if (iterator == NULL || iterator->owner == NULL) {
        piston_list_fail(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "Piston List iterator owner",
                         SIZE_MAX, MC_ENTITY_OK);
        return false;
    }
    /* Java Itr keeps its original List, ignoring a later static m replacement.
     * It reloads that owned descriptor's current storage, never an old buffer. */
    if (iterator->identity != iterator->owner->piston_list_identity ||
        !piston_list_shape(iterator->owner, false) || iterator->cursor > (size_t)INT32_MAX) {
        piston_list_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                         "Piston List captured iterator descriptor", iterator->cursor,
                         MC_ENTITY_OK);
        return false;
    }
    return true;
}

mc_world_storage_status
mc_tile_entity_piston_entities_has_next(const mc_piston_entity_iterator *iterator, bool *out,
                                        mc_piston_list_error *error) {
    mc_piston_list_error frame = {.index = SIZE_MAX};
    if (out == NULL)
        piston_list_fail(&frame, MC_WORLD_STORAGE_INVALID_ARGUMENT, "Piston List hasNext output",
                         SIZE_MAX, MC_ENTITY_OK);
    else if (piston_iterator_owner(iterator, &frame))
        *out = iterator->cursor != iterator->identity->count;
    return piston_list_finish(&frame, error);
}

mc_world_storage_status mc_tile_entity_piston_entities_next(mc_piston_entity_iterator *iterator,
                                                            mc_entity **out,
                                                            mc_piston_list_error *error) {
    mc_piston_list_error frame = {.index = SIZE_MAX};
    if (out == NULL) {
        piston_list_fail(&frame, MC_WORLD_STORAGE_INVALID_ARGUMENT, "Piston List next output",
                         SIZE_MAX, MC_ENTITY_OK);
    } else if (piston_iterator_owner(iterator, &frame)) {
        mc_tile_entity_classes *classes = iterator->owner;
        const size_t index = iterator->cursor;
        if (classes->piston_mod_count != iterator->expected_mod_count)
            piston_list_fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                             "Piston List iterator concurrent modification", index, MC_ENTITY_OK);
        else if (index >= iterator->identity->count)
            piston_list_fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                             "Piston List iterator no such element", index, MC_ENTITY_OK);
        else {
            /* Itr cursor54 precedes its actual element load64. An unsupported
             * reached occurrence or native local hold preserves that prefix. */
            iterator->cursor = index + 1u;
            mc_entity *entity = classes->piston_entities[index];
            if (entity == NULL || classes->piston_items[index] != entity)
                piston_list_fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                 "Piston List iterator selected occurrence", index, MC_ENTITY_OK);
            else {
                const mc_entity_status held = mc_entity_retain(entity);
                if (held != MC_ENTITY_OK)
                    piston_list_fail(&frame, piston_entity_status(held),
                                     "Piston List iterator local Entity hold", index, held);
                else
                    *out = entity;
            }
        }
    }
    return piston_list_finish(&frame, error);
}

static void piston_list_clear_owned(mc_tile_entity_classes *classes) {
    classes->piston_mod_count += UINT32_C(1);
    const size_t count = classes->piston_count;
    classes->piston_list_identity->count = 0; /* Actual clear size23 before stores35. */
    classes->piston_count = 0;
    for (size_t i = 0; i < count; ++i) {
        mc_entity *entity = classes->piston_entities[i];
        classes->piston_items[i] = NULL;
        classes->piston_entities[i] = NULL;
        mc_entity_release(entity);
    }
}

mc_world_storage_status mc_tile_entity_piston_entities_clear(mc_tile_entity *tile_entity,
                                                             mc_piston_list_error *error) {
    mc_piston_list_error frame = {.index = SIZE_MAX};
    mc_tile_entity_classes *classes = NULL;
    if (!piston_list_owner(tile_entity, &classes, &frame))
        return piston_list_finish(&frame, error);
    size_t bad_index;
    if (!piston_list_occurrences(classes, &bad_index)) {
        piston_list_fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                         "Piston clear List occurrence ownership", bad_index, MC_ENTITY_OK);
        return piston_list_finish(&frame, error);
    }
    piston_list_clear_owned(classes);
    return piston_list_finish(&frame, error);
}

mc_world_storage_status
mc_tile_entity_piston_class_discard_entities(mc_world_runtime_classes *parent,
                                             mc_world_storage_error *error) {
    mc_world_storage_error frame = {0};
    if (parent == NULL)
        frame = (mc_world_storage_error){MC_WORLD_STORAGE_INVALID_ARGUMENT,
                                         "Piston class teardown owner", 0};
    else if (parent->tile_entities != NULL) {
        mc_tile_entity_classes *classes = parent->tile_entities;
        if (classes->live_tile_entities != 0)
            frame = (mc_world_storage_error){MC_WORLD_STORAGE_BUSY,
                                             "Piston class teardown has live TileEntities", 0};
        else if (classes->parent != parent || !piston_list_retire_valid(classes))
            frame = (mc_world_storage_error){MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                             "Piston class teardown owning List invariant", 0};
        else if (classes->piston_list_identity != NULL)
            piston_list_clear_owned(classes);
    }
    if (error != NULL)
        *error = frame;
    return frame.status;
}

static void tile_entity_allocation_error(mc_world_runtime *world, mc_tile_entity_kind kind,
                                         mc_tile_entity_status status,
                                         mc_tile_entity_error *error) {
    tile_entity_status(error, status, NULL, SIZE_MAX);
    if (error == NULL || world == NULL || world->classes == NULL ||
        world->classes->tile_entities == NULL)
        return;
    const mc_tile_entity_classes *classes = world->classes->tile_entities;
    if (classes->phase == MC_TILE_ENTITY_CLASS_FAILED) {
        error->registry = classes->first_registry_failure;
        error->initializer_bytecode = classes->initializer_pc;
    } else if (kind == MC_TILE_ENTITY_PISTON &&
               classes->piston_phase == MC_TILE_ENTITY_CLASS_FAILED) {
        error->operation = "Piston class initializer";
        error->piston_initializer_bytecode = classes->piston_initializer_pc;
    }
}

static mc_tile_entity_status live_classes_get(mc_world_runtime *world,
                                              mc_tile_entity_classes **out) {
    mc_tile_entity_classes *classes = NULL;
    mc_tile_entity_status owned = live_classes_owner(world, &classes);
    if (owned != MC_TILE_ENTITY_OK)
        return owned;
    return live_classes_initialize(classes, out);
}

static mc_tile_entity_status live_classes_initialize(mc_tile_entity_classes *classes,
                                                     mc_tile_entity_classes **out) {
    if (classes->phase == 2) {
        *out = classes;
        return MC_TILE_ENTITY_OK;
    }
    if (classes->phase == 3)
        return classes->failure;
    if (classes->phase == 1)
        return MC_TILE_ENTITY_STATE_ERROR; /* No callback/reentrant clinit is synthesized. */
    /* Passive literal users may have created this native owner before the
     * one immutable application Registry binding. Refresh that dependency
     * before first active use; never replace it in READY/FAILED phases. */
    classes->registry = classes->parent->registry;
    classes->phase = 1;
    mc_registry_error attempt = live_empty_registry();
    mc_tile_entity_status status = MC_TILE_ENTITY_OK;
    classes->initializer_pc = 3;
    const size_t class_ceiling = 1024u * 1024u;
    if (classes->allocation_bytes > class_ceiling ||
        LIVE_CLASS_COUNT * sizeof(*classes->by_name) > class_ceiling - classes->allocation_bytes)
        status = MC_TILE_ENTITY_LIMIT;
    else
        classes->by_name = calloc(LIVE_CLASS_COUNT, sizeof(*classes->by_name));
    if (status == MC_TILE_ENTITY_OK && classes->by_name == NULL)
        status = MC_TILE_ENTITY_ALLOCATION_FAILED;
    else if (status == MC_TILE_ENTITY_OK)
        classes->allocation_bytes += LIVE_CLASS_COUNT * sizeof(*classes->by_name);
    if (status == MC_TILE_ENTITY_OK) {
        classes->initializer_pc = 13;
        if (classes->allocation_bytes > class_ceiling ||
            LIVE_CLASS_COUNT * sizeof(*classes->by_class) >
                class_ceiling - classes->allocation_bytes)
            status = MC_TILE_ENTITY_LIMIT;
        else
            classes->by_class = calloc(LIVE_CLASS_COUNT, sizeof(*classes->by_class));
        if (status == MC_TILE_ENTITY_OK && classes->by_class == NULL)
            status = MC_TILE_ENTITY_ALLOCATION_FAILED;
        else if (status == MC_TILE_ENTITY_OK)
            classes->allocation_bytes += LIVE_CLASS_COUNT * sizeof(*classes->by_class);
    }
    static const char *const names[LIVE_CLASS_COUNT] = {
        "Furnace", "Chest", "RecordPlayer", "Trap", "Sign", "MobSpawner", "Music", "Piston"};
    for (unsigned int i = 0; i < LIVE_CLASS_COUNT && status == MC_TILE_ENTITY_OK; ++i) {
        classes->initializer_pc = 22u + 7u * i; /* Reached ldc String. */
        mc_tile_entity_name *name = NULL;
        status = live_literal(classes, names[i], &name, &attempt, NULL, NULL);
        if (status != MC_TILE_ENTITY_OK)
            break;
        classes->initializer_pc = 24u + 7u * i;
        for (size_t previous = 0; previous < classes->name_count; ++previous) {
            if (tile_entity_names_equal(classes->by_name[previous].name->text, name->text)) {
                status = MC_TILE_ENTITY_STATE_ERROR;
                break;
            }
        }
        if (status == MC_TILE_ENTITY_OK) {
            /* Actual source ID->Class44 precedes Class->ID55. Both fixed
             * native arrays represent those distinct Maps, whose iteration is
             * not exposed by the original callers. Literal pool owns name. */
            classes->by_name[classes->name_count++] = (live_class_entry){(live_class_id)i, name};
            classes->by_class[classes->class_count++] = (live_class_entry){(live_class_id)i, name};
        }
        mc_tile_entity_name_release(name);
    }
    if (status != MC_TILE_ENTITY_OK) {
        classes->failure = status;
        classes->first_registry_failure = attempt;
        classes->phase = 3;
        return status;
    }
    classes->initializer_pc = 76;
    classes->phase = 2;
    *out = classes;
    return MC_TILE_ENTITY_OK;
}

static mc_tile_entity_name *live_class_name(mc_tile_entity_classes *classes,
                                            mc_tile_entity_kind kind) {
    live_class_id selected;
    switch (kind) {
        case MC_TILE_ENTITY_CHEST:
            selected = LIVE_CHEST;
            break;
        case MC_TILE_ENTITY_SPAWNER:
            selected = LIVE_SPAWNER;
            break;
        case MC_TILE_ENTITY_PISTON:
            selected = LIVE_PISTON;
            break;
        default:
            return NULL;
    }
    if (classes == NULL || classes->phase != 2)
        return NULL;
    for (size_t i = 0; i < classes->class_count; ++i)
        if (classes->by_class[i].kind == selected)
            return classes->by_class[i].name;
    return NULL;
}

mc_world_storage_status mc_live_tile_entities_classes_retire(mc_world_runtime_classes *parent,
                                                             mc_world_storage_error *error) {
    mc_world_storage_error cause = {0};
    if (parent != NULL && parent->tile_entities != NULL) {
        mc_tile_entity_classes *classes = parent->tile_entities;
        if (parent->live_worlds != 0 || classes->live_tile_entities != 0 ||
            classes->class_literal_borrows != 0)
            cause = (mc_world_storage_error){MC_WORLD_STORAGE_BUSY,
                                             "TileEntity class has live owners", 0};
        for (mc_tile_entity_name *name = classes->literals;
             cause.status == MC_WORLD_STORAGE_OK && name != NULL; name = name->literal_next)
            if (name->holds != 1)
                cause = (mc_world_storage_error){MC_WORLD_STORAGE_BUSY,
                                                 "TileEntity class literal has external holds", 0};
        if (cause.status == MC_WORLD_STORAGE_OK && !piston_list_retire_valid(classes))
            cause = (mc_world_storage_error){MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                             "Piston static List native invariant", 0};
        if (cause.status == MC_WORLD_STORAGE_OK) {
            /* Quiescent native retirement admits the complete owning graph
             * before releasing anything. A failed push may leave m nonempty;
             * its genuine occurrences keep independent holds until here.
             * Runtime Entity-class borrow release is separately admitted by
             * the caller before this step, and remains held during releases. */
            for (size_t i = 0; i < classes->piston_count; ++i)
                mc_entity_release(classes->piston_entities[i]);
            free(classes->piston_items);
            free(classes->piston_entities);
            free(classes->piston_list_identity);
            mc_tile_entity_name *name = classes->literals;
            while (name != NULL) {
                mc_tile_entity_name *next = name->literal_next;
                mc_tile_entity_name_release(name);
                name = next;
            }
            free(classes->by_name);
            free(classes->by_class);
            free(classes);
            parent->tile_entities = NULL;
        }
    }
    if (error != NULL)
        *error = cause;
    return cause.status;
}

static mc_world_storage_status live_fail(mc_live_tile_entity_error *error,
                                         mc_world_storage_status status, const char *operation) {
    error->status = status;
    error->operation = operation;
    error->cause = (mc_world_storage_error){status, operation, 0};
    return status;
}

static mc_world_storage_status live_cause(mc_live_tile_entity_error *error,
                                          mc_world_storage_error cause) {
    error->status = cause.status;
    error->operation = cause.operation;
    error->cause = cause;
    return cause.status;
}

static bool live_registry_cause(mc_world_runtime *world, mc_live_tile_entity_error *error,
                                const mc_registry_error *cause, const char *operation) {
    error->registry = *cause;
    const mc_world_storage_status status =
        mc_world_runtime_registry_result(world, cause->status, cause, operation, &error->cause);
    error->status = status;
    error->operation = error->cause.operation;
    return false;
}

static mc_world_storage_status live_tile_entity_error(mc_live_tile_entity_error *error,
                                                      mc_tile_entity_status status,
                                                      const char *operation) {
    error->tile_entity.status = status;
    if (error->tile_entity.registry.status != MC_REGISTRY_OK)
        error->registry = error->tile_entity.registry;
    if (error->tile_entity.initializer_bytecode != 0)
        error->initializer_bytecode = error->tile_entity.initializer_bytecode;
    const mc_world_storage_status mapped =
        status == MC_TILE_ENTITY_OK                  ? MC_WORLD_STORAGE_OK
        : status == MC_TILE_ENTITY_LIMIT             ? MC_WORLD_STORAGE_LIMIT
        : status == MC_TILE_ENTITY_ALLOCATION_FAILED ? MC_WORLD_STORAGE_ALLOCATION_FAILED
        : status == MC_TILE_ENTITY_INVALID_ARGUMENT  ? MC_WORLD_STORAGE_INVALID_ARGUMENT
                                                     : MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    return live_fail(error, mapped, operation);
}

static mc_world_storage_status live_finish(mc_world_runtime *world, mc_live_tile_entity_error frame,
                                           mc_live_tile_entity_error *error) {
    if (frame.status == MC_WORLD_STORAGE_OK)
        mc_live_tile_entities_error_init(&frame);
    if (world != NULL)
        world->last_error = frame.cause;
    if (error != NULL)
        *error = frame;
    return frame.status;
}

static bool live_enter(mc_world_runtime *world, mc_live_tile_entity_error *error) {
    if (world == NULL || world->construction.mode != MC_WORLD_RUNTIME_GAME ||
        world->construction.phase == MC_WORLD_RUNTIME_RETIRED ||
        world->tile_entity_containers_retired) {
        live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "TileEntity World native admission");
        return false;
    }
    mc_world_storage_error cause = {0};
    if (mc_world_runtime_call_enter(world, &cause) != MC_WORLD_STORAGE_OK) {
        live_cause(error, cause);
        return false;
    }
    return true;
}

mc_world_storage_status mc_live_tile_entities_new(mc_world_runtime *world, mc_tile_entity_kind kind,
                                                  mc_tile_entity **out,
                                                  mc_live_tile_entity_error *error) {
    mc_live_tile_entity_error frame;
    mc_live_tile_entities_error_init(&frame);
    if (out == NULL) {
        live_fail(&frame, MC_WORLD_STORAGE_INVALID_ARGUMENT, "TileEntity constructor output");
        return live_finish(NULL, frame, error);
    }
    if (!live_enter(world, &frame))
        return live_finish(NULL, frame, error);
    mc_tile_entity *tile_entity = NULL;
    mc_tile_entity_error detail;
    mc_tile_entity_error_init(&detail);
    mc_tile_entity_status status = mc_tile_entity_allocate(world, kind, &tile_entity);
    if (status == MC_TILE_ENTITY_OK)
        status = mc_tile_entity_construct_detailed(tile_entity, &detail);
    else
        tile_entity_allocation_error(world, kind, status, &detail);
    if (status != MC_TILE_ENTITY_OK) {
        frame.tile_entity = detail;
        live_tile_entity_error(&frame, status, "TileEntity constructor");
        if (world->classes->tile_entities != NULL) {
            if (world->classes->tile_entities->phase == 3) {
                frame.initializer_bytecode = world->classes->tile_entities->initializer_pc;
                frame.registry = world->classes->tile_entities->first_registry_failure;
            }
        }
        mc_tile_entity_release(tile_entity);
    } else
        *out = tile_entity;
    mc_world_runtime_call_leave(world);
    return live_finish(world, frame, error);
}

static bool live_list_shape(const mc_world_reference_list *list) {
    return list != NULL && list->count <= list->capacity && list->capacity <= (size_t)INT32_MAX &&
           list->capacity <= SIZE_MAX / sizeof(*list->items) &&
           ((list->capacity == 0) == (list->items == NULL));
}

void mc_live_tile_entities_capture_list(mc_world_runtime *world, bool deferred,
                                        mc_world_reference_list *identity) {
    /* Constructor-native hook: the same newly allocated empty descriptor. */
    mc_tile_entity_list_ledger *ledger = &world->tile_entity_membership[deferred ? 1u : 0u];
    ledger->identity = identity;
}

static bool live_list_valid(const mc_world_runtime *world, bool deferred) {
    const mc_tile_entity_list_ledger *ledger = &world->tile_entity_membership[deferred ? 1u : 0u];
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

bool mc_live_tile_entities_lists_valid(const mc_world_runtime *world) {
    return world != NULL && live_list_valid(world, false) && live_list_valid(world, true);
}

mc_world_storage_status
mc_live_tile_entities_terminal_visit_lists(const mc_world_runtime *world,
                                           mc_world_terminal_tile_visitor visitor, void *context,
                                           mc_world_storage_error *error) {
    mc_world_storage_error local = {0};
    if (error == NULL)
        error = &local;
    (void)tile_terminal_result(error, MC_WORLD_STORAGE_OK, NULL);
    if (world == NULL || visitor == NULL)
        return tile_terminal_result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                                    "terminal TileEntity World List arguments");
    if (!mc_live_tile_entities_lists_valid(world))
        return tile_terminal_result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                    "terminal TileEntity World List owning invariant");
    for (size_t selected = 0; selected < 2; ++selected) {
        const mc_tile_entity_list_ledger *ledger = &world->tile_entity_membership[selected];
        for (size_t i = 0; i < ledger->count; ++i) {
            const mc_world_storage_status status =
                visitor(context, &ledger->holds[i], ledger->holds[i], error);
            if (status != MC_WORLD_STORAGE_OK)
                return status;
        }
    }
    return MC_WORLD_STORAGE_OK;
}

static bool live_list_add(mc_world_runtime *world, bool deferred, mc_tile_entity *tile_entity,
                          mc_live_tile_entity_error *error) {
    mc_tile_entity_list_ledger *ledger = &world->tile_entity_membership[deferred ? 1u : 0u];
    mc_world_reference_list *list = deferred ? world->fields.G : world->fields.c;
    if (!live_list_valid(world, deferred) || list == NULL) {
        live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "TileEntity List owning descriptor");
        return false;
    }
    if (list->count == (size_t)INT32_MAX) {
        live_fail(error, MC_WORLD_STORAGE_LIMIT, "TileEntity List size");
        return false;
    }
    void **items = list->items;
    mc_tile_entity **holds = ledger->holds;
    size_t capacity = list->capacity, bytes = 0;
    if (list->count == capacity) {
        capacity = capacity == 0                       ? 4u
                   : capacity > (size_t)INT32_MAX / 2u ? (size_t)INT32_MAX
                                                       : capacity * 2u;
        if (capacity > SIZE_MAX / (sizeof(*items) + sizeof(*holds))) {
            live_fail(error, MC_WORLD_STORAGE_LIMIT, "TileEntity List byte overflow");
            return false;
        }
        bytes = capacity * (sizeof(*items) + sizeof(*holds));
        const size_t used = world->construction.direct_allocation_bytes;
        const size_t limit = world->construction_limits.max_direct_allocation_bytes;
        if (used > limit || bytes > limit - used) {
            live_fail(error, MC_WORLD_STORAGE_LIMIT, "TileEntity List peak allocation budget");
            return false;
        }
        items = malloc(capacity * sizeof(*items));
        holds = malloc(capacity * sizeof(*holds));
        if (items == NULL || holds == NULL) {
            free(items);
            free(holds);
            live_fail(error, MC_WORLD_STORAGE_ALLOCATION_FAILED, "TileEntity List allocation");
            return false;
        }
        if (list->count != 0) {
            memcpy(items, list->items, list->count * sizeof(*items));
            memcpy(holds, ledger->holds, list->count * sizeof(*holds));
        }
    }
    mc_tile_entity_status held = mc_tile_entity_retain(tile_entity);
    if (held != MC_TILE_ENTITY_OK) {
        if (bytes != 0) {
            free(items);
            free(holds);
        }
        live_tile_entity_error(error, held, "TileEntity List occurrence retain");
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
    list->items[list->count++] = tile_entity;
    ledger->holds[ledger->count++] = tile_entity;
    ledger->mod_count = mc_java_i32_add(ledger->mod_count, 1);
    return true;
}

static bool live_list_remove_first(mc_world_runtime *world, mc_tile_entity *tile_entity,
                                   mc_live_tile_entity_error *error) {
    if (!live_list_valid(world, false) || world->fields.c == NULL) {
        live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "TileEntity List remove descriptor");
        return false;
    }
    mc_world_reference_list *list = world->fields.c;
    mc_tile_entity_list_ledger *ledger = &world->tile_entity_membership[0];
    for (size_t i = 0; i < list->count; ++i) {
        if (ledger->holds[i] != tile_entity)
            continue;
        memmove(list->items + i, list->items + i + 1,
                (list->count - i - 1u) * sizeof(*list->items));
        memmove(ledger->holds + i, ledger->holds + i + 1,
                (ledger->count - i - 1u) * sizeof(*ledger->holds));
        --list->count;
        --ledger->count;
        ledger->mod_count = mc_java_i32_add(ledger->mod_count, 1);
        mc_tile_entity_release(tile_entity);
        break;
    }
    return true;
}

void mc_live_tile_entities_lists_release(mc_world_runtime *world) {
    world->tile_entity_containers_retired = true;
    for (unsigned int selected = 0; selected < 2; ++selected) {
        mc_tile_entity_list_ledger *ledger = &world->tile_entity_membership[selected];
        mc_world_reference_list *list = ledger->identity;
        mc_tile_entity **holds = ledger->holds;
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
            mc_tile_entity_release(holds[i]);
        free(holds);
    }
}
typedef struct live_map_node live_map_node;

struct live_map_node {
    int32_t x, y, z;
    uint32_t hash;
    mc_tile_entity *value;
    live_map_node *next, *previous, *parent, *children[2];
    bool red, tree;
};

struct mc_tile_entity_map_backing {
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
 * existing concrete Integer HashSet mechanics. TileEntity keys differ: full triple
 * equality and non-Comparable full-hash ties. A tie requiring identityHashCode
 * is rejected before native map mutation. No coordinate/pointer ordering
 * pretends to be that missing profile. */
static bool live_map_before(uint32_t left, uint32_t right) {
    return (left ^ UINT32_C(0x80000000)) < (right ^ UINT32_C(0x80000000));
}

static uint32_t live_map_threshold(uint32_t capacity) {
    return capacity == UINT32_C(0x40000000) ? UINT32_C(0x7fffffff) : capacity - capacity / 4u;
}

static live_map_node *live_map_root(live_map_node *node) {
    while (node->parent != NULL)
        node = node->parent;
    return node;
}

/* Promote one child while preserving the separate traversal chain. */
static live_map_node *live_map_rotate(live_map_node *root, live_map_node *node, unsigned side) {
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

static live_map_node *live_map_balance_add(live_map_node *root, live_map_node *node) {
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

static bool live_map_red(const live_map_node *node) {
    return node != NULL && node->red;
}

static live_map_node *live_map_balance_remove(live_map_node *root, live_map_node *node) {
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

static void live_map_front(live_map_node **buckets, uint32_t capacity, live_map_node *root) {
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

static void live_map_treeify(live_map_node **buckets, uint32_t capacity, uint32_t index) {
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

static void live_map_plain(live_map_node *head) {
    for (live_map_node *node = head; node != NULL; node = node->next) {
        node->tree = node->red = false;
        node->parent = node->children[0] = node->children[1] = NULL;
    }
}

static void live_map_tree_add(mc_tile_entity_map_backing *set, live_map_node *head,
                              live_map_node *node) {
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

static void live_map_resize(mc_tile_entity_map_backing *set, live_map_node **table,
                            uint32_t capacity) {
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

static void live_map_tree_remove(mc_tile_entity_map_backing *set, live_map_node *node) {
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

static uint32_t live_key_hash(int32_t x, int32_t y, int32_t z) {
    int32_t raw = mc_java_i32_add(
        mc_java_i32_add(mc_java_i32_mul(x, 8976890), mc_java_i32_mul(y, 981131)), z);
    uint32_t bits = (uint32_t)raw;
    return bits ^ (bits >> 16u);
}

static live_map_node *live_map_find(const mc_tile_entity_map_backing *map, int32_t x, int32_t y,
                                    int32_t z, uint32_t hash) {
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

static bool live_owned_map_valid(const mc_world_chunk *chunk) {
    if (chunk == NULL)
        return false;
    const mc_world_chunk_tile_entity_map *descriptor = &chunk->tile_entity_map;
    const mc_tile_entity_map_backing *map = descriptor->backing;
    if (map == NULL)
        return descriptor->count == 0 &&
               ((descriptor->release_backing == NULL && descriptor->backing_bytes == NULL) ||
                (descriptor->release_backing == mc_live_tile_entities_map_release &&
                 descriptor->backing_bytes == mc_live_tile_entities_owned_map_bytes));
    if (descriptor->release_backing != mc_live_tile_entities_map_release ||
        descriptor->backing_bytes != mc_live_tile_entities_owned_map_bytes || map->owner != chunk ||
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
    if (!tile_entity_size_add(&expected, (size_t)map->capacity * sizeof(*map->buckets)) ||
        !tile_entity_size_add(&expected, mc_nbt_tree_allocation_bytes(map->journal)) ||
        !tile_entity_size_add(&expected, map->record_count * sizeof(*map->consumed)))
        return false;
    return visited == map->size && expected == map->allocation_bytes &&
           expected <= chunk->allocation_bytes;
}

mc_world_storage_status
mc_live_tile_entities_terminal_visit_owned_map(const mc_world_chunk *chunk,
                                               mc_world_terminal_tile_visitor visitor,
                                               void *context, mc_world_storage_error *error) {
    mc_world_storage_error local = {0};
    if (error == NULL)
        error = &local;
    (void)tile_terminal_result(error, MC_WORLD_STORAGE_OK, NULL);
    if (chunk == NULL || visitor == NULL)
        return tile_terminal_result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                                    "terminal TileEntity Chunk Map arguments");
    if (!live_owned_map_valid(chunk))
        return tile_terminal_result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                    "terminal TileEntity Chunk Map owning invariant");
    const mc_tile_entity_map_backing *map = chunk->tile_entity_map.backing;
    if (map == NULL)
        return MC_WORLD_STORAGE_OK;
    size_t visited = 0;
    for (uint32_t bucket = 0; bucket < map->capacity; ++bucket)
        for (const live_map_node *node = map->buckets[bucket]; node != NULL; node = node->next) {
            if (visited == map->size)
                return tile_terminal_result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                            "terminal TileEntity Chunk Map chain extent");
            const mc_world_storage_status status =
                visitor(context, &node->value, node->value, error);
            if (status != MC_WORLD_STORAGE_OK)
                return status;
            ++visited;
        }
    if (visited != map->size)
        return tile_terminal_result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                    "terminal TileEntity Chunk Map visited size");
    /* Source chunk.l may be replaced. The embedded Map, validated above,
     * still owns these exact node values and their eventual releases. */
    return MC_WORLD_STORAGE_OK;
}

bool mc_live_tile_entities_owned_map_bytes(const mc_world_chunk *chunk, size_t *out) {
    /* This lower native inspector never evaluates source l or recursively
     * validates the common Chunk ledger that called it. */
    if (out == NULL || !live_owned_map_valid(chunk))
        return false;
    *out = chunk->tile_entity_map.backing == NULL
               ? 0
               : chunk->tile_entity_map.backing->allocation_bytes;
    return true;
}

bool mc_live_tile_entities_map_valid(const mc_world_chunk *chunk) {
    return chunk != NULL && chunk->l == &chunk->tile_entity_map && live_owned_map_valid(chunk);
}

static bool live_map_put(mc_world_chunk *chunk, int32_t x, int32_t y, int32_t z,
                         mc_tile_entity *tile_entity, size_t requested_ceiling,
                         mc_live_tile_entity_error *error) {
    mc_world_chunk_tile_entity_map *descriptor = chunk->l;
    if (!mc_live_tile_entities_map_valid(chunk) ||
        (descriptor->backing == NULL && descriptor->release_backing != NULL)) {
        live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "TileEntity Map native ownership");
        return false;
    }
    const size_t ceiling = requested_ceiling != 0 && requested_ceiling < chunk->max_allocation_bytes
                               ? requested_ceiling
                               : chunk->max_allocation_bytes;
    size_t graph;
    if (!live_graph_measure(chunk, tile_entity, ceiling, &graph, error))
        return false;
    mc_tile_entity_map_backing *map = descriptor->backing;
    const uint32_t hash = live_key_hash(x, y, z);
    live_map_node *existing = live_map_find(map, x, y, z, hash);
    if (existing != NULL) {
        mc_tile_entity_status held = mc_tile_entity_retain(tile_entity);
        if (held != MC_TILE_ENTITY_OK) {
            live_tile_entity_error(error, held, "TileEntity Map replacement retain");
            return false;
        }
        mc_tile_entity *old = existing->value;
        existing->value = tile_entity; /* Existing node/key/order retained. */
        mc_tile_entity_release(old);
        return true;
    }
    const size_t size = map == NULL ? 0 : map->size;
    if (size == (size_t)INT32_MAX) {
        live_fail(error, MC_WORLD_STORAGE_LIMIT, "TileEntity Map size");
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
                  "TileEntity Map non-Comparable identityHashCode tree profile");
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
        if (!tile_entity_size_multiply((size_t)capacities[i], sizeof(live_map_node *),
                                       &table_bytes[i])) {
            live_fail(error, MC_WORLD_STORAGE_LIMIT, "TileEntity Map table byte overflow");
            return false;
        }
        if (!tile_entity_size_add(&additional, table_bytes[i])) {
            live_fail(error, MC_WORLD_STORAGE_LIMIT, "TileEntity Map peak byte overflow");
            return false;
        }
    }
    if (graph > ceiling || additional > ceiling - graph) {
        live_fail(error, MC_WORLD_STORAGE_LIMIT, "TileEntity Map peak allocation budget");
        return false;
    }
    mc_tile_entity_map_backing *fresh_map = map == NULL ? calloc(1, sizeof(*map)) : NULL;
    live_map_node *node = calloc(1, sizeof(*node));
    live_map_node **tables[2] = {NULL, NULL};
    bool allocated = node != NULL && (map != NULL || fresh_map != NULL);
    for (size_t i = 0; i < table_count && allocated; ++i) {
        tables[i] = calloc(capacities[i], sizeof(*tables[i]));
        allocated = tables[i] != NULL;
    }
    mc_tile_entity_status held =
        allocated ? mc_tile_entity_retain(tile_entity) : MC_TILE_ENTITY_ALLOCATION_FAILED;
    if (!allocated || held != MC_TILE_ENTITY_OK) {
        free(fresh_map);
        free(node);
        for (size_t i = 0; i < table_count; ++i)
            free(tables[i]);
        live_tile_entity_error(error, held, "TileEntity Map allocation/occurrence retain");
        return false;
    }
    chunk->allocation_bytes += additional;
    if (map == NULL) {
        map = fresh_map;
        map->owner = chunk;
        descriptor->backing = map;
        descriptor->release_backing = mc_live_tile_entities_map_release;
        descriptor->backing_bytes = mc_live_tile_entities_owned_map_bytes;
    }
    map->allocation_bytes += additional;
    *node = (live_map_node){.x = x, .y = y, .z = z, .hash = hash, .value = tile_entity};
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

static mc_tile_entity *live_map_remove(mc_world_chunk *chunk, int32_t x, int32_t y, int32_t z) {
    mc_tile_entity_map_backing *map = chunk->l->backing;
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
    mc_tile_entity *value = node->value; /* Transfer this actual map occurrence hold. */
    free(node);
    --map->size;
    --chunk->l->count;
    chunk->allocation_bytes -= sizeof(*node);
    map->allocation_bytes -= sizeof(*node);
    return value;
}

void mc_live_tile_entities_map_release(mc_world_chunk_tile_entity_map *descriptor) {
    mc_tile_entity_map_backing *map = descriptor->backing;
    descriptor->backing = NULL;
    descriptor->count = 0;
    descriptor->release_backing = mc_live_tile_entities_map_release; /* Terminal native marker. */
    descriptor->backing_bytes = mc_live_tile_entities_owned_map_bytes;
    if (map == NULL)
        return;
    mc_world_chunk *owner = map->owner;
    for (uint32_t i = 0; i < map->capacity; ++i) {
        live_map_node *node = map->buckets[i];
        while (node != NULL) {
            live_map_node *next = node->next;
            mc_tile_entity *value = node->value;
            free(node);
            owner->allocation_bytes -= sizeof(*node);
            mc_tile_entity_release(value);
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

mc_world_storage_status mc_live_tile_entities_map_entry_at(const mc_world_chunk *chunk,
                                                           size_t ordinal,
                                                           mc_live_tile_entity_map_entry *out) {
    if (out == NULL || !mc_live_tile_entities_map_valid(chunk) || ordinal >= chunk->l->count)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    const mc_tile_entity_map_backing *map = chunk->l->backing;
    size_t index = 0;
    for (uint32_t i = 0; i < map->capacity; ++i) {
        for (live_map_node *node = map->buckets[i]; node != NULL; node = node->next) {
            if (index++ == ordinal) {
                *out = (mc_live_tile_entity_map_entry){node->x, node->y, node->z, node->value};
                return MC_WORLD_STORAGE_OK;
            }
        }
    }
    return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
}

mc_world_storage_status mc_live_tile_entities_map_at(const mc_world_chunk *chunk, size_t ordinal,
                                                     mc_tile_entity **out) {
    if (out == NULL)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    mc_live_tile_entity_map_entry entry;
    const mc_world_storage_status status =
        mc_live_tile_entities_map_entry_at(chunk, ordinal, &entry);
    if (status == MC_WORLD_STORAGE_OK)
        *out = entry.tile_entity;
    return status;
}

static bool live_tile_entity_block_kind(const mc_block *block) {
    if (block == NULL)
        return false;
    switch (block->kind) {
        case MC_BLOCK_MOB_SPAWNER:
        case MC_BLOCK_CHEST:
        case MC_BLOCK_JUKEBOX:
        case MC_BLOCK_NOTE_BLOCK:
        case MC_BLOCK_FURNACE:
        case MC_BLOCK_SIGN:
        case MC_BLOCK_MOVING_PISTON:
        case MC_BLOCK_DISPENSER:
            return true;
        default:
            return false;
    }
}

static bool live_chunk_block(mc_world_chunk *chunk, int32_t x, int32_t y, int32_t z, int32_t *out,
                             mc_live_tile_entity_error *error) {
    const int32_t cell = mc_java_i32_shl(x, 11) | mc_java_i32_shl(z, 7) | y;
    mc_world_chunk_fields fields;
    if (mc_world_chunk_fields_get(chunk, &fields) != MC_WORLD_STORAGE_OK || fields.b == NULL ||
        cell < 0 || (uint32_t)cell >= MC_CHUNK_BLOCKS) {
        live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "TileEntity Chunk block element");
        return false;
    }
    *out = fields.b[(size_t)cell];
    return true;
}

static bool live_acquire(mc_world_runtime *world, int32_t x, int32_t z, mc_chunk_state **out,
                         mc_live_tile_entity_error *error) {
    /* Actual World.c reads v1 before provider.b6, including cached hits. */
    if (world->fields.v == NULL) {
        error->provider_bytecode = 6;
        live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World TileEntity provider receiver");
        return false;
    }
    mc_chunk_provider_fields provider;
    mc_world_storage_error binding = {0};
    if (mc_chunk_provider_bind_fields(world->fields.v, &provider, &binding) !=
        MC_WORLD_STORAGE_OK) {
        error->provider_bytecode = 6;
        live_cause(error, binding);
        return false;
    }
    if (provider.world != world || provider.cache != world->acquisition.cache ||
        provider.pending != world->acquisition.pending || provider.store != world->options.store) {
        error->provider_bytecode = 6;
        live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                  "World TileEntity replacement provider binding");
        return false;
    }
    mc_world_runtime_acquisition_error acquired;
    mc_chunk_acquire_status status = mc_world_runtime_get_detailed(world, x, z, out, &acquired);
    if (status == MC_CHUNK_ACQUIRE_OK)
        return true;
    if (acquired.tile_entity.status != MC_WORLD_STORAGE_OK) {
        const mc_live_tile_entity_call_frame caller = {
            .bytecode = error->bytecode,
            .chunk_bytecode = error->chunk_bytecode,
            .world_access_bytecode = error->world_access_bytecode,
            .initializer_bytecode = error->initializer_bytecode,
            .caller_bytecode = error->caller_bytecode,
            .hook_bytecode = error->hook_bytecode,
            .remove_bytecode = error->remove_bytecode,
            .provider_bytecode = error->provider_bytecode,
            .tile_entity_bytecode = error->tile_entity_bytecode,
            .inventory_bytecode = error->inventory_bytecode};
        *error = acquired.tile_entity;
        error->acquisition_caller_bytecode = caller.bytecode;
        error->acquisition_parent = caller;
    } else if (acquired.cause.status != MC_WORLD_STORAGE_OK)
        live_cause(error, acquired.cause);
    else if (status == MC_CHUNK_ACQUIRE_LIMIT || status == MC_CHUNK_ACQUIRE_PARTIAL_LIMIT)
        live_fail(error, MC_WORLD_STORAGE_LIMIT, "TileEntity Chunk acquisition limit");
    else
        live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "TileEntity Chunk acquisition");
    if (acquired.registry.status != MC_REGISTRY_OK)
        error->registry = acquired.registry;
    error->batch = acquired.batch;
    error->activation_bytecode = acquired.activation_bytecode;
    error->activation_world_bytecode = acquired.activation_world_bytecode;
    error->load_bytecode = acquired.load_bytecode;
    error->load_builder_bytecode = acquired.load_builder_bytecode;
    error->load_entity = acquired.load_entity;
    return false;
}

static bool live_lease(mc_world_runtime *world, mc_chunk_state *state, mc_world_chunk **out,
                       mc_live_tile_entity_error *error) {
    mc_world_chunk *chunk = mc_world_store_chunk_object(world->fields.w, state);
    if (!mc_world_chunk_source_enabled(chunk)) {
        live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "TileEntity actual Store Chunk owner");
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
                                    mc_tile_entity *tile_entity, size_t storage_ceiling,
                                    mc_live_tile_entity_error *error) {
    /* Native map admission precedes the source receiver fields. */
    if (!mc_live_tile_entities_map_valid(chunk) ||
        (chunk->l->backing == NULL && chunk->l->release_backing != NULL)) {
        live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "TileEntity install Map ownership");
        return false;
    }
    error->chunk_bytecode = 18;
    if (tile_entity == NULL) {
        live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "TileEntity install World receiver");
        return false;
    }
    mc_tile_entity_status status = mc_tile_entity_bind_world(tile_entity, chunk->d);
    if (status != MC_TILE_ENTITY_OK) {
        live_tile_entity_error(error, status, "TileEntity World binding");
        return false;
    }
    tile_entity->x = mc_java_i32_add(mc_java_i32_mul(chunk->constructor_x, 16), x); /*32*/
    tile_entity->y = y;                                                             /*38*/
    tile_entity->z = mc_java_i32_add(mc_java_i32_mul(chunk->constructor_z, 16), z); /*52*/
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
            return live_registry_cause(chunk->d, error, &cause,
                                       "TileEntity install Block.m active use");
        }
        error->chunk_bytecode = 72;
        if (!live_chunk_block(chunk, x, y, z, &block_id, error))
            return false;
        error->chunk_bytecode = 75;
        const mc_block *block = NULL;
        if (mc_registry_block_m_at(&array, block_id, &block, &cause) != MC_REGISTRY_OK) {
            return live_registry_cause(chunk->d, error, &cause,
                                       "TileEntity install captured Block.m element");
        }
        error->chunk_bytecode = 76;
        accepts = live_tile_entity_block_kind(block); /* instanceof accepts NULL. */
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
            live_fail(error, MC_WORLD_STORAGE_IO_ERROR,
                      "TileEntity install diagnostic native output");
            return false;
        }
        tile_entity->classes->stdout_error |=
            io_failed; /* PrintStream suppresses underlying I/O. */
        return true;   /* Actual return90, with earlier fields retained. */
    }
    error->chunk_bytecode = 93;
    tile_entity->invalid = false;
    error->chunk_bytecode = 104;
    return live_map_put(chunk, x, y, z, tile_entity, storage_ceiling, error);
}

static bool live_world_set_held(mc_world_runtime *world, int32_t x, int32_t y, int32_t z,
                                mc_tile_entity *tile_entity, mc_live_tile_entity_error *error) {
    error->bytecode = 2;
    if (tile_entity == NULL) {
        live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "TileEntity World invalid receiver");
        return false;
    }
    if (tile_entity->invalid)
        return true; /* Ordinary return93. */
    if (world->fields.L) {
        tile_entity->x = x; /*18*/
        tile_entity->y = y; /*24*/
        tile_entity->z = z; /*30*/
        error->bytecode = 39;
        return live_list_add(world, true, tile_entity, error);
    }
    error->bytecode = 54;
    if (!live_list_add(world, false, tile_entity, error))
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
    const bool completed = live_chunk_install_held(chunk, x & 15, y, z & 15, tile_entity, 0, error);
    (void)mc_world_store_release_chunk(world->fields.w, state, NULL);
    return completed;
}

static bool live_block_added_held(mc_world_runtime *world, const mc_block *block, int32_t x,
                                  int32_t y, int32_t z, mc_live_tile_entity_error *error) {
    error->bytecode = 6; /* Proved uu/na.c base body returns. */
    if (block == NULL) {
        live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "TileEntity Block added receiver");
        return false;
    }
    if (block->kind == MC_BLOCK_MOVING_PISTON) {
        error->bytecode = 0; /* Actual ut/mz.c(World,III) is literal return0. */
        return true;
    }
    error->bytecode = 15;
    mc_tile_entity_kind kind;
    if (block->kind == MC_BLOCK_CHEST)
        kind = MC_TILE_ENTITY_CHEST;
    else if (block->kind == MC_BLOCK_MOB_SPAWNER)
        kind = MC_TILE_ENTITY_SPAWNER;
    else {
        live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "TileEntity concrete Block factory");
        return false;
    }
    mc_tile_entity *tile_entity = NULL;
    mc_tile_entity_error detail;
    mc_tile_entity_error_init(&detail);
    mc_tile_entity_status status = mc_tile_entity_allocate(world, kind, &tile_entity);
    if (status == MC_TILE_ENTITY_OK)
        status = mc_tile_entity_construct_detailed(tile_entity, &detail);
    if (status != MC_TILE_ENTITY_OK) {
        error->tile_entity = detail;
        live_tile_entity_error(error, status, "TileEntity Block concrete constructor");
        if (world->classes->tile_entities != NULL && world->classes->tile_entities->phase == 3) {
            error->initializer_bytecode = world->classes->tile_entities->initializer_pc;
            error->registry = world->classes->tile_entities->first_registry_failure;
        }
        mc_tile_entity_release(tile_entity);
        return false;
    }
    error->bytecode = 18;
    const bool completed = live_world_set_held(world, x, y, z, tile_entity, error);
    if (!completed)
        error->caller_bytecode = 18;
    mc_tile_entity_release(tile_entity); /* Only factory's private temporary. */
    return completed;
}

static bool live_chunk_get_held(mc_world_chunk *chunk, int32_t x, int32_t y, int32_t z,
                                mc_tile_entity **out, mc_live_tile_entity_error *error) {
    error->chunk_bytecode = 18;
    if (!mc_live_tile_entities_map_valid(chunk) ||
        (chunk->l->backing == NULL && chunk->l->release_backing != NULL)) {
        live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "TileEntity lookup Map ownership");
        return false;
    }
    live_map_node *node = live_map_find(chunk->l->backing, x, y, z, live_key_hash(x, y, z));
    mc_tile_entity *tile_entity = node == NULL ? NULL : node->value;
    if (tile_entity == NULL) {
        int32_t id;
        error->chunk_bytecode = 37;
        if (!live_chunk_block(chunk, x, y, z, &id, error))
            return false;
        error->chunk_bytecode = 42;
        mc_registry_block_p_view flags;
        mc_registry_error cause = live_empty_registry();
        if (mc_registry_block_p_get(mc_world_runtime_registry(chunk->d), &flags, &cause) !=
            MC_REGISTRY_OK) {
            return live_registry_cause(chunk->d, error, &cause,
                                       "TileEntity lookup Block.p active use");
        }
        error->chunk_bytecode = 47;
        bool has_tile_entity;
        if (mc_registry_block_p_at(&flags, id, &has_tile_entity, &cause) != MC_REGISTRY_OK) {
            return live_registry_cause(chunk->d, error, &cause,
                                       "TileEntity lookup captured Block.p element");
        }
        if (!has_tile_entity) {
            *out = NULL;
            return true;
        }
        error->chunk_bytecode = 53;
        mc_registry_block_m_view array;
        if (mc_registry_block_m_get(mc_world_runtime_registry(chunk->d), &array, &cause) !=
            MC_REGISTRY_OK) {
            return live_registry_cause(chunk->d, error, &cause,
                                       "TileEntity lookup Block.m active use");
        }
        error->chunk_bytecode = 58;
        const mc_block *block = NULL;
        if (mc_registry_block_m_at(&array, id, &block, &cause) != MC_REGISTRY_OK) {
            return live_registry_cause(chunk->d, error, &cause,
                                       "TileEntity lookup captured Block.m element");
        }
        error->chunk_bytecode = 59;
        if (block != NULL && !live_tile_entity_block_kind(block)) {
            live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "TileEntity lookup Block TileEntity checkcast");
            return false;
        }
        error->chunk_bytecode = 89;
        const int32_t wx = mc_java_i32_add(mc_java_i32_mul(chunk->constructor_x, 16), x);
        const int32_t wz = mc_java_i32_add(mc_java_i32_mul(chunk->constructor_z, 16), z);
        if (block == NULL) {
            live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "TileEntity lookup TileEntity-block invocation receiver");
            return false; /* NULL passed checkcast59; callee base6 is NOT entered. */
        }
        if (!live_block_added_held(chunk->d, block, wx, y, wz, error))
            return false;
        error->chunk_bytecode = 98;
        if (!mc_live_tile_entities_map_valid(chunk)) {
            live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "TileEntity lookup reread Map ownership");
            return false;
        }
        node = live_map_find(chunk->l->backing, x, y, z, live_key_hash(x, y, z));
        tile_entity = node == NULL ? NULL : node->value;
    }
    error->chunk_bytecode = 115;
    if (tile_entity != NULL && tile_entity->invalid) {
        error->chunk_bytecode = 127;
        mc_tile_entity_release(live_map_remove(chunk, x, y, z));
        *out = NULL;
        return true;
    }
    if (tile_entity != NULL) {
        mc_tile_entity_status held = mc_tile_entity_retain(tile_entity);
        if (held != MC_TILE_ENTITY_OK) {
            live_tile_entity_error(error, held, "TileEntity lookup result retain");
            return false;
        }
    }
    *out = tile_entity;
    return true;
}

static bool live_world_get_held(mc_world_runtime *world, int32_t x, int32_t y, int32_t z,
                                mc_tile_entity **out, mc_live_tile_entity_error *error) {
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
                                   mc_live_tile_entity_error *error) {
    error->chunk_bytecode = 16;
    if (!chunk->state->active)
        return true;
    error->chunk_bytecode = 25;
    if (!mc_live_tile_entities_map_valid(chunk)) {
        live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "TileEntity removal Map ownership");
        return false;
    }
    mc_tile_entity *tile_entity = live_map_remove(chunk, x, y, z);
    if (tile_entity != NULL) {
        error->chunk_bytecode = 42;
        tile_entity->invalid = true;
        mc_tile_entity_release(tile_entity);
    }
    return true;
}

mc_world_storage_status mc_live_tile_entities_get(mc_world_runtime *world, int32_t x, int32_t y,
                                                  int32_t z, mc_tile_entity **out,
                                                  mc_live_tile_entity_error *error) {
    mc_live_tile_entity_error frame;
    mc_live_tile_entities_error_init(&frame);
    if (out == NULL) {
        live_fail(&frame, MC_WORLD_STORAGE_INVALID_ARGUMENT, "TileEntity lookup output");
        return live_finish(NULL, frame, error);
    }
    if (!live_enter(world, &frame))
        return live_finish(NULL, frame, error);
    mc_tile_entity *tile_entity = NULL;
    if (live_world_get_held(world, x, y, z, &tile_entity, &frame))
        *out = tile_entity;
    mc_world_runtime_call_leave(world);
    return live_finish(world, frame, error);
}

mc_world_storage_status mc_live_tile_entities_set(mc_world_runtime *world, int32_t x, int32_t y,
                                                  int32_t z, mc_tile_entity *tile_entity,
                                                  mc_live_tile_entity_error *error) {
    mc_live_tile_entity_error frame;
    mc_live_tile_entities_error_init(&frame);
    if (!live_enter(world, &frame))
        return live_finish(NULL, frame, error);
    if (tile_entity != NULL && tile_entity->classes->parent != world->classes) {
        live_fail(&frame, MC_WORLD_STORAGE_INVALID_ARGUMENT, "TileEntity application class owner");
        mc_world_runtime_call_leave(world);
        return live_finish(NULL, frame, error);
    }
    bool retained = false;
    if (tile_entity != NULL) {
        mc_tile_entity_status held = mc_tile_entity_retain(tile_entity);
        if (held != MC_TILE_ENTITY_OK)
            live_tile_entity_error(&frame, held, "TileEntity set temporary retain");
        else
            retained = true;
    }
    if (frame.status == MC_WORLD_STORAGE_OK)
        (void)live_world_set_held(world, x, y, z, tile_entity, &frame);
    if (retained)
        mc_tile_entity_release(tile_entity);
    mc_world_runtime_call_leave(world);
    return live_finish(world, frame, error);
}

mc_world_storage_status mc_live_tile_entities_remove(mc_world_runtime *world, int32_t x, int32_t y,
                                                     int32_t z, mc_live_tile_entity_error *error) {
    mc_live_tile_entity_error frame;
    mc_live_tile_entities_error_init(&frame);
    if (!live_enter(world, &frame))
        return live_finish(NULL, frame, error);
    mc_tile_entity *tile_entity = NULL;
    frame.bytecode = 4;
    frame.remove_bytecode = 4;
    if (live_world_get_held(world, x, y, z, &tile_entity, &frame)) {
        if (tile_entity != NULL && world->fields.L) {
            frame.bytecode = 23;
            frame.remove_bytecode = 23;
            tile_entity->invalid = true;
        } else {
            frame.bytecode = 40;
            frame.remove_bytecode = 40;
            bool removed =
                tile_entity == NULL || live_list_remove_first(world, tile_entity, &frame);
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
    mc_tile_entity_release(tile_entity);
    mc_world_runtime_call_leave(world);
    return live_finish(world, frame, error);
}

mc_world_storage_status mc_live_tile_entities_block_added(mc_world_runtime *world,
                                                          const mc_block *block, int32_t x,
                                                          int32_t y, int32_t z,
                                                          mc_live_tile_entity_error *error) {
    mc_live_tile_entity_error frame;
    mc_live_tile_entities_error_init(&frame);
    if (!live_enter(world, &frame))
        return live_finish(NULL, frame, error);
    (void)live_block_added_held(world, block, x, y, z, &frame);
    mc_world_runtime_call_leave(world);
    return live_finish(world, frame, error);
}

static bool live_list_add_map(mc_world_runtime *world, bool deferred, mc_world_chunk *chunk,
                              mc_live_tile_entity_error *error) {
    if (!live_list_valid(world, deferred) || !mc_live_tile_entities_map_valid(chunk)) {
        live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                  "TileEntity addAll actual collection owners");
        return false;
    }
    mc_world_reference_list *list = deferred ? world->fields.G : world->fields.c;
    mc_tile_entity_list_ledger *ledger = &world->tile_entity_membership[deferred ? 1u : 0u];
    if (list == NULL) {
        live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "TileEntity addAll List receiver");
        return false;
    }
    const size_t incoming = chunk->l->count;
    if (incoming > (size_t)INT32_MAX - list->count) {
        live_fail(error, MC_WORLD_STORAGE_LIMIT, "TileEntity addAll size");
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
    if (capacity > SIZE_MAX / (sizeof(void *) + sizeof(mc_tile_entity *))) {
        live_fail(error, MC_WORLD_STORAGE_LIMIT, "TileEntity addAll byte overflow");
        return false;
    }
    /* Native separate buffers admit all source references before publication.
     * Charge old plus both complete new spans. No partial repeated add replaces
     * this actual single ArrayList.addAll operation. */
    const size_t bytes = capacity * (sizeof(void *) + sizeof(mc_tile_entity *));
    const size_t used = world->construction.direct_allocation_bytes;
    const size_t limit = world->construction_limits.max_direct_allocation_bytes;
    if (used > limit || bytes > limit - used) {
        live_fail(error, MC_WORLD_STORAGE_LIMIT, "TileEntity addAll peak allocation budget");
        return false;
    }
    void **items = malloc(capacity * sizeof(*items));
    mc_tile_entity **holds = malloc(capacity * sizeof(*holds));
    if (items == NULL || holds == NULL) {
        free(items);
        free(holds);
        live_fail(error, MC_WORLD_STORAGE_ALLOCATION_FAILED, "TileEntity addAll storage");
        return false;
    }
    if (list->count != 0) {
        memcpy(items, list->items, list->count * sizeof(*items));
        memcpy(holds, ledger->holds, list->count * sizeof(*holds));
    }
    size_t copied = 0;
    const mc_tile_entity_map_backing *map = chunk->l->backing;
    for (uint32_t bucket = 0; bucket < map->capacity; ++bucket) {
        for (live_map_node *node = map->buckets[bucket]; node != NULL; node = node->next) {
            mc_tile_entity_status held = mc_tile_entity_retain(node->value);
            if (held != MC_TILE_ENTITY_OK) {
                for (size_t i = 0; i < copied; ++i)
                    mc_tile_entity_release(holds[list->count + i]);
                free(items);
                free(holds);
                live_tile_entity_error(error, held, "TileEntity addAll occurrence retain");
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
                             mc_live_tile_entity_error *error) {
    if (chunk == NULL) {
        live_fail(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "TileEntity Chunk receiver");
        return false;
    }
    if (!mc_world_chunk_source_enabled(chunk)) {
        live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "TileEntity Chunk source is disabled");
        return false;
    }
    *world = chunk->d;
    if (!live_enter(*world, error))
        return false;
    mc_world_chunk *leased;
    if (mc_world_store_chunk_object((*world)->fields.w, chunk->state) != chunk ||
        !live_lease(*world, chunk->state, &leased, error)) {
        if (error->status == MC_WORLD_STORAGE_OK)
            live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "TileEntity Chunk Store identity");
        mc_world_runtime_call_leave(*world);
        return false;
    }
    return true;
}

static mc_world_storage_status live_chunk_finish(mc_world_chunk *chunk,
                                                 mc_live_tile_entity_error frame,
                                                 mc_live_tile_entity_error *error) {
    mc_world_runtime *world = chunk->d;
    (void)mc_world_store_release_chunk(world->fields.w, chunk->state, NULL);
    mc_world_runtime_call_leave(world);
    return live_finish(world, frame, error);
}

mc_world_storage_status mc_live_tile_entities_chunk_get(mc_world_chunk *chunk, int32_t x, int32_t y,
                                                        int32_t z, mc_tile_entity **out,
                                                        mc_live_tile_entity_error *error) {
    mc_live_tile_entity_error frame;
    mc_live_tile_entities_error_init(&frame);
    if (out == NULL) {
        live_fail(&frame, MC_WORLD_STORAGE_INVALID_ARGUMENT, "TileEntity Chunk lookup output");
        return live_finish(NULL, frame, error);
    }
    mc_world_runtime *world;
    if (!live_chunk_entry(chunk, &world, &frame))
        return live_finish(NULL, frame, error);
    mc_tile_entity *value = NULL;
    if (live_chunk_get_held(chunk, x, y, z, &value, &frame))
        *out = value;
    return live_chunk_finish(chunk, frame, error);
}

mc_world_storage_status mc_live_tile_entities_chunk_import(mc_world_chunk *chunk,
                                                           mc_tile_entity *tile_entity,
                                                           mc_live_tile_entity_error *error) {
    mc_live_tile_entity_error frame;
    mc_live_tile_entities_error_init(&frame);
    mc_world_runtime *world;
    if (!live_chunk_entry(chunk, &world, &frame))
        return live_finish(NULL, frame, error);
    if (tile_entity == NULL)
        live_fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                  "TileEntity import coordinate receiver");
    else {
        mc_tile_entity_status held = mc_tile_entity_retain(tile_entity);
        if (held != MC_TILE_ENTITY_OK)
            live_tile_entity_error(&frame, held, "TileEntity import temporary retain");
        else {
            const int32_t x =
                mc_java_i32_sub(tile_entity->x, mc_java_i32_mul(chunk->constructor_x, 16));
            const int32_t y = tile_entity->y;
            const int32_t z =
                mc_java_i32_sub(tile_entity->z, mc_java_i32_mul(chunk->constructor_z, 16));
            frame.bytecode = 38;
            if (live_chunk_install_held(chunk, x, y, z, tile_entity, 0, &frame) &&
                chunk->state->active) {
                frame.bytecode = 56; /* Direct c append; deliberately no L branch. */
                (void)live_list_add(world, false, tile_entity, &frame);
            }
            mc_tile_entity_release(tile_entity);
        }
    }
    return live_chunk_finish(chunk, frame, error);
}

mc_world_storage_status mc_live_tile_entities_chunk_register(mc_world_chunk *chunk,
                                                             mc_live_tile_entity_error *error) {
    mc_live_tile_entity_error frame;
    mc_live_tile_entities_error_init(&frame);
    mc_world_runtime *world;
    if (!live_chunk_entry(chunk, &world, &frame))
        return live_finish(NULL, frame, error);
    frame.bytecode = 18;
    frame.chunk_bytecode = world->fields.L ? 12u : 26u;
    (void)live_list_add_map(world, world->fields.L, chunk, &frame);
    return live_chunk_finish(chunk, frame, error);
}

mc_world_storage_status mc_live_tile_entities_chunk_invalidate(mc_world_chunk *chunk,
                                                               mc_live_tile_entity_error *error) {
    mc_live_tile_entity_error frame;
    mc_live_tile_entities_error_init(&frame);
    /* The caller holds the actual Chunk through this callback-free traversal.
     * Its source body never dereferences d, even when the Map is nonempty. */
    if (!mc_world_chunk_source_enabled(chunk)) {
        live_fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "TileEntity detach native admission");
        return live_finish(NULL, frame, error);
    }
    frame.bytecode = 21;
    if (!mc_live_tile_entities_map_valid(chunk))
        live_fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "TileEntity detach Map iterator");
    else if (chunk->l->backing != NULL) {
        mc_tile_entity_map_backing *map = chunk->l->backing;
        for (uint32_t bucket = 0; bucket < map->capacity; ++bucket)
            for (live_map_node *node = map->buckets[bucket]; node != NULL; node = node->next) {
                frame.bytecode = 40;
                node->value->invalid = true; /* No clear, detach, or World release. */
            }
    }
    return live_finish(NULL, frame, error);
}

mc_world_storage_status
mc_world_runtime_tile_entity_dirty_detailed(mc_world_runtime *world, int32_t x, int32_t y,
                                            int32_t z, mc_tile_entity *tile_entity,
                                            mc_live_tile_entity_error *error) {
    mc_live_tile_entity_error frame;
    mc_live_tile_entities_error_init(&frame);
    if (!live_enter(world, &frame))
        return live_finish(NULL, frame, error);
    bool present = false;
    frame.bytecode = 4;
    if (y >= 0 && y < 128) {
        mc_world_storage_error cause = {0};
        if (mc_world_runtime_chunk_provider_contains(world, mc_java_i32_shr(x, 4),
                                                     mc_java_i32_shr(z, 4), &present,
                                                     &cause) != MC_WORLD_STORAGE_OK)
            live_cause(&frame, cause);
    }
    if (frame.status == MC_WORLD_STORAGE_OK && present) {
        frame.bytecode = 13;
        mc_chunk_state *state = NULL;
        if (live_acquire(world, mc_java_i32_shr(x, 4), mc_java_i32_shr(z, 4), &state, &frame)) {
            frame.bytecode = 16;
            if (state == NULL)
                live_fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                          "TileEntity dirty Chunk receiver");
            else if (!state->blank)
                state->dirty = true;
        }
    }
    /* Presence only guards the Chunk dirty prefix. The live WorldAccess loop is
     * still reached when presence was false, with the same nullable TileEntity. */
    for (size_t index = 0; frame.status == MC_WORLD_STORAGE_OK; ++index) {
        mc_world_access *world_access = NULL;
        bool has = false;
        mc_world_storage_error cause = {0};
        if (!mc_world_access_next(world, index, 28, 42, 47, &world_access, &has,
                                  &frame.world_access_bytecode, &cause)) {
            frame.bytecode = frame.world_access_bytecode;
            live_cause(&frame, cause);
            break;
        }
        if (!has)
            break; /* return66, without preflighting the TileEntity argument */
        frame.bytecode = frame.world_access_bytecode = 55;
        if (world_access == NULL || (world_access->callbacks.tile_entity_dirty == NULL &&
                                     world_access->source_callbacks.tile_entity_dirty == NULL)) {
            mc_world_access_release_held(world_access);
            live_fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "TileEntity dirty WorldAccess invocation");
            break;
        }
        if (tile_entity != NULL) {
            const mc_tile_entity_status held = mc_tile_entity_retain(tile_entity);
            if (held != MC_TILE_ENTITY_OK) {
                mc_world_access_release_held(world_access);
                live_tile_entity_error(&frame, held, "TileEntity dirty callback TileEntity hold");
                break;
            }
        }
        const mc_world_storage_status status = mc_world_access_call_tile_entity_dirty(
            world_access, world, x, y, z, tile_entity, &cause, &frame.world_access_failure);
        mc_tile_entity_release(tile_entity);
        mc_world_access_release_held(world_access);
        if (status != MC_WORLD_STORAGE_OK)
            live_cause(&frame, mc_world_access_callback_error(status, cause));
        /* iinc60 occurs only after callback success. Reentry may change the
         * next reached current-u size/get without changing this argument. */
    }
    mc_world_runtime_call_leave(world);
    return live_finish(world, frame, error);
}

bool mc_world_runtime_tile_entity_dirty(mc_world_runtime *world, int32_t x, int32_t y, int32_t z,
                                        mc_tile_entity *tile_entity) {
    return mc_world_runtime_tile_entity_dirty_detailed(world, x, y, z, tile_entity, NULL) ==
           MC_WORLD_STORAGE_OK;
}

mc_world_storage_status mc_live_tile_entities_drain_deferred(mc_world_runtime *world,
                                                             mc_live_tile_entity_error *error) {
    mc_live_tile_entity_error frame;
    mc_live_tile_entities_error_init(&frame);
    if (!live_enter(world, &frame))
        return live_finish(NULL, frame, error);
    frame.bytecode = 461;
    if (!live_list_valid(world, true) || world->fields.G == NULL)
        live_fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                  "TileEntity deferred isEmpty receiver");
    else if (world->fields.G->count != 0) {
        const int32_t expected = world->tile_entity_membership[1].mod_count;
        size_t cursor = 0;
        while (frame.status == MC_WORLD_STORAGE_OK) {
            mc_world_reference_list *pending = world->fields.G;
            if (!live_list_valid(world, true) || pending == NULL) {
                live_fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                          "TileEntity deferred iterator owner");
                break;
            }
            if (cursor == pending->count)
                break;
            frame.bytecode = 489;
            if (world->tile_entity_membership[1].mod_count != expected || cursor > pending->count) {
                live_fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                          "TileEntity deferred iterator concurrent modification");
                break;
            }
            mc_tile_entity *tile_entity = world->tile_entity_membership[1].holds[cursor++];
            mc_tile_entity_status held = mc_tile_entity_retain(tile_entity);
            if (held != MC_TILE_ENTITY_OK) {
                live_tile_entity_error(&frame, held, "TileEntity deferred temporary retain");
                break;
            }
            frame.bytecode = 499;
            if (!tile_entity->invalid) {
                frame.bytecode = 510;
                if (!live_list_valid(world, false) || world->fields.c == NULL)
                    live_fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                              "TileEntity deferred active contains receiver");
                else {
                    bool contains = false;
                    for (size_t i = 0; i < world->fields.c->count; ++i)
                        contains |= world->tile_entity_membership[0].holds[i] == tile_entity;
                    frame.bytecode = 523;
                    bool admitted = contains || live_list_add(world, false, tile_entity, &frame);
                    if (admitted) {
                        frame.bytecode = 542;
                        mc_chunk_state *state = NULL;
                        if (live_acquire(world, mc_java_i32_shr(tile_entity->x, 4),
                                         mc_java_i32_shr(tile_entity->z, 4), &state, &frame)) {
                            if (state != NULL && !state->blank) {
                                frame.bytecode = 573;
                                mc_world_chunk *chunk;
                                if (live_lease(world, state, &chunk, &frame)) {
                                    (void)live_chunk_install_held(
                                        chunk, tile_entity->x & 15, tile_entity->y,
                                        tile_entity->z & 15, tile_entity, 0, &frame);
                                    (void)mc_world_store_release_chunk(world->fields.w, state,
                                                                       NULL);
                                }
                            }
                            if (frame.status == MC_WORLD_STORAGE_OK) {
                                frame.bytecode = 589;
                                mc_world_mutation_error observed;
                                mc_world_mutation_error_init(&observed);
                                if (!mc_world_mutation_observe_cell(world, tile_entity->x,
                                                                    tile_entity->y, tile_entity->z,
                                                                    &observed)) {
                                    frame.world_access_bytecode = observed.world_access_bytecode;
                                    live_cause(&frame, observed.cause);
                                }
                            }
                        }
                    }
                }
            }
            mc_tile_entity_release(tile_entity);
        }
        if (frame.status == MC_WORLD_STORAGE_OK) {
            frame.bytecode = 599;
            mc_world_reference_list *pending = world->fields.G;
            mc_tile_entity_list_ledger *ledger = &world->tile_entity_membership[1];
            const size_t count = ledger->count;
            pending->count = ledger->count = 0;
            ledger->mod_count = mc_java_i32_add(ledger->mod_count, 1);
            for (size_t i = 0; i < count; ++i) {
                mc_tile_entity *tile_entity = ledger->holds[i];
                ledger->holds[i] = NULL;
                pending->items[i] = NULL;
                mc_tile_entity_release(tile_entity);
            }
        }
    }
    mc_world_runtime_call_leave(world);
    return live_finish(world, frame, error);
}

static mc_world_storage_status live_tile_entities_factory_scoped(
    mc_world_runtime *world, const mc_nbt_tag *compound, const mc_nbt_limits *limits, bool preserve,
    mc_tile_entity **out, mc_live_tile_entity_error *error, const mc_world_chunk *scope_chunk) {
    mc_live_tile_entity_error frame;
    mc_live_tile_entities_error_init(&frame);
    if (out == NULL) {
        live_fail(&frame, MC_WORLD_STORAGE_INVALID_ARGUMENT, "TileEntity factory output");
        return live_finish(NULL, frame, error);
    }
    if (!live_enter(world, &frame))
        return live_finish(NULL, frame, error);
    mc_tile_entity_classes *classes = NULL;
    mc_tile_entity_status active = live_classes_get(world, &classes);
    if (active != MC_TILE_ENTITY_OK) {
        live_tile_entity_error(&frame, active, "TileEntity factory class active use");
        if (world->classes->tile_entities != NULL) {
            if (world->classes->tile_entities->phase == 3) {
                frame.initializer_bytecode = world->classes->tile_entities->initializer_pc;
                frame.registry = world->classes->tile_entities->first_registry_failure;
            }
        }
    } else {
        frame.bytecode = 8;
        const mc_nbt_tag *id = compound == NULL || compound->type != MC_NBT_COMPOUND
                                   ? NULL
                                   : mc_nbt_compound_get(compound, tile_entity_id_name, 2);
        if (compound == NULL || compound->type != MC_NBT_COMPOUND ||
            (id != NULL && id->type != MC_NBT_STRING)) {
            /* The original catches this Java getter exception, prints its
             * stack at36, then re-reads ID70 on the skip path. Native failures
             * are not fabricated Java Exception objects or silently skipped. */
            frame.bytecode = 36;
            frame.tile_entity.field = "id";
            frame.tile_entity.status = MC_TILE_ENTITY_INVALID_FIELD;
            live_fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "TileEntity factory caught-ID exception stack diagnostic");
        } else {
            mc_nbt_string name = id == NULL ? (mc_nbt_string){NULL, 0} : id->value.string;
            const live_class_entry *entry = NULL;
            frame.bytecode = 11;
            for (size_t i = 0; i < classes->name_count; ++i)
                if (tile_entity_names_equal(name, classes->by_name[i].name->text)) {
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
                              "TileEntity factory extended PrintStream encoding/length");
                else if (captured != MC_TEXT_STDOUT_OK)
                    live_fail(&frame, MC_WORLD_STORAGE_IO_ERROR,
                              "TileEntity factory stdout capture");
                else {
                    bool io_failed = false;
                    mc_text_stdout_status printed = mc_text_stdout_println(
                        &stream, diagnostic, sizeof(prefix) - 1u + name.length, &io_failed);
                    if (printed != MC_TEXT_STDOUT_OK)
                        live_fail(&frame, MC_WORLD_STORAGE_IO_ERROR,
                                  "TileEntity factory diagnostic native output");
                    else {
                        classes->stdout_error |= io_failed;
                        *out = NULL; /* Ordinary unknown-ID return83. */
                    }
                }
            } else if (entry->kind != LIVE_CHEST && entry->kind != LIVE_SPAWNER &&
                       entry->kind != LIVE_PISTON) {
                frame.bytecode = 25;
                live_fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                          "TileEntity registered concrete constructor");
            } else {
                const mc_tile_entity_kind kind = entry->kind == LIVE_CHEST ? MC_TILE_ENTITY_CHEST
                                                 : entry->kind == LIVE_SPAWNER
                                                     ? MC_TILE_ENTITY_SPAWNER
                                                     : MC_TILE_ENTITY_PISTON;
                mc_tile_entity *tile_entity = NULL;
                mc_tile_entity_error detail;
                mc_tile_entity_error_init(&detail);
                mc_tile_entity_status status;
                frame.bytecode = 25;
                if (preserve) {
                    /* Shared ordered semantic body captures real winning slots,
                     * then adds native independent provenance before publication.
                     * It does not rerun semantics or use a guessed association. */
                    status = tile_entity_import_storage_scoped(world, kind, compound, limits,
                                                               &tile_entity, &detail, scope_chunk);
                } else {
                    status = mc_tile_entity_allocate(world, kind, &tile_entity);
                    if (status == MC_TILE_ENTITY_OK)
                        status = mc_tile_entity_construct_detailed(tile_entity, &detail);
                    else
                        tile_entity_allocation_error(world, kind, status, &detail);
                    if (status == MC_TILE_ENTITY_OK) {
                        frame.bytecode = 46; /* Outside factory Exception catch2..32. */
                        status = mc_tile_entity_read_nbt(tile_entity, compound, &detail);
                    }
                }
                if (status != MC_TILE_ENTITY_OK) {
                    frame.tile_entity = detail;
                    if (detail.source_bytecode != 0)
                        frame.bytecode = detail.source_bytecode;
                    live_tile_entity_error(&frame, status,
                                           "TileEntity factory constructor/semantic read");
                    mc_tile_entity_release(tile_entity);
                } else
                    *out = tile_entity;
            }
        }
    }
    mc_world_runtime_call_leave(world);
    return live_finish(world, frame, error);
}

mc_world_storage_status mc_live_tile_entities_factory(mc_world_runtime *world,
                                                      const mc_nbt_tag *compound,
                                                      const mc_nbt_limits *limits, bool preserve,
                                                      mc_tile_entity **out,
                                                      mc_live_tile_entity_error *error) {
    return live_tile_entities_factory_scoped(world, compound, limits, preserve, out, error, NULL);
}

static bool live_drop_entity_error(mc_live_tile_entity_error *error, mc_entity_status status,
                                   const char *operation) {
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
                                  int32_t y, int32_t z, mc_live_tile_entity_error *error) {
    mc_tile_entity *chest = NULL;
    error->bytecode = 5;
    if (!live_world_get_held(world, x, y, z, &chest, error))
        return false;
    error->bytecode = 8;
    if (chest != NULL && chest->kind != MC_TILE_ENTITY_CHEST) {
        mc_tile_entity_release(chest);
        live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Chest removal TileEntity checkcast");
        return false;
    }
    bool completed = true;
    for (int32_t slot = 0; completed; slot = mc_java_i32_add(slot, 1)) {
        error->bytecode = 20;
        int32_t size;
        mc_tile_entity_status tile_entity_status_code = mc_tile_entity_chest_size(chest, &size);
        if (tile_entity_status_code != MC_TILE_ENTITY_OK) {
            live_tile_entity_error(error, tile_entity_status_code, "Chest removal size receiver");
            completed = false;
            break;
        }
        if (slot >= size)
            break;
        error->bytecode = 32;
        mc_item_stack *stack = NULL;
        tile_entity_status_code = mc_tile_entity_chest_get(chest, slot, &stack);
        if (tile_entity_status_code != MC_TILE_ENTITY_OK) {
            live_tile_entity_error(error, tile_entity_status_code, "Chest removal slot getter");
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
                error->tile_entity.item.status = item;
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
    mc_tile_entity_release(chest);
    return completed;
}

mc_world_storage_status mc_live_tile_entities_block_removed(mc_world_runtime *world,
                                                            const mc_block *block, int32_t x,
                                                            int32_t y, int32_t z,
                                                            mc_live_tile_entity_error *error) {
    mc_live_tile_entity_error frame;
    mc_live_tile_entities_error_init(&frame);
    if (!live_enter(world, &frame))
        return live_finish(NULL, frame, error);
    bool base = false;
    if (block == NULL)
        live_fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "TileEntity removed Block receiver");
    else if (block->kind == MC_BLOCK_MOB_SPAWNER)
        base = true; /* Actual inherited rw/lb removed hook. */
    else if (block->kind == MC_BLOCK_CHEST) {
        if (live_chest_drops_held(world, block, x, y, z, &frame)) {
            frame.bytecode = 268;
            base = true;
        }
    } else
        live_fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                  "TileEntity concrete removed override");
    if (base) {
        /* rw/lb.b: proved base block return6, actual World remove14. This
         * nested wrapper retains its own World and exact fetched TileEntity alias. */
        frame.bytecode = 14;
        mc_live_tile_entity_error nested;
        mc_live_tile_entities_error_init(&nested);
        if (mc_live_tile_entities_remove(world, x, y, z, &nested) != MC_WORLD_STORAGE_OK) {
            /* Preserve the COMPLETE child, including TileEntity/Registry/initializer/
             * Entity/publication data, plus the two actual enclosing calls. */
            frame = nested;
            frame.caller_bytecode = 14; /* rw/lb removed -> World remove */
            frame.hook_bytecode = block->kind == MC_BLOCK_CHEST ? 268u : 0u;
        }
    }
    mc_world_runtime_call_leave(world);
    return live_finish(world, frame, error);
}

static const uint16_t live_records_name[] = {'T', 'i', 'l', 'e', 'E', 'n',
                                             't', 'i', 't', 'i', 'e', 's'};

static bool live_map_ensure_header(mc_world_chunk *chunk, size_t ceiling,
                                   mc_live_tile_entity_error *error) {
    if (!mc_live_tile_entities_map_valid(chunk) ||
        (chunk->l->backing == NULL && chunk->l->release_backing != NULL)) {
        live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "TileEntity storage Map owner");
        return false;
    }
    if (chunk->l->backing != NULL)
        return true;
    size_t graph;
    if (!live_graph_measure(chunk, NULL, ceiling, &graph, error))
        return false;
    if (sizeof(mc_tile_entity_map_backing) > ceiling - graph) {
        live_fail(error, MC_WORLD_STORAGE_LIMIT, "TileEntity storage Map header budget");
        return false;
    }
    mc_tile_entity_map_backing *map = calloc(1, sizeof(*map));
    if (map == NULL) {
        live_fail(error, MC_WORLD_STORAGE_ALLOCATION_FAILED, "TileEntity storage Map header");
        return false;
    }
    map->owner = chunk;
    map->allocation_bytes = sizeof(*map);
    chunk->l->backing = map;
    chunk->l->release_backing = mc_live_tile_entities_map_release;
    chunk->l->backing_bytes = mc_live_tile_entities_owned_map_bytes;
    chunk->allocation_bytes += sizeof(*map);
    return true;
}

mc_world_storage_status mc_live_tile_entities_load_records(mc_world_chunk *chunk,
                                                           const mc_nbt_tag *records,
                                                           const mc_nbt_limits *requested,
                                                           mc_live_tile_entity_error *error) {
    mc_live_tile_entity_error frame;
    mc_live_tile_entities_error_init(&frame);
    mc_world_runtime *world;
    if (!live_chunk_entry(chunk, &world, &frame))
        return live_finish(NULL, frame, error);
    mc_nbt_limits limits;
    if (!tile_entity_limits(requested, &limits))
        live_fail(&frame, MC_WORLD_STORAGE_INVALID_ARGUMENT, "TileEntity storage import limits");
    else if (!live_map_ensure_header(chunk,
                                     limits.max_allocation_bytes < chunk->max_allocation_bytes
                                         ? limits.max_allocation_bytes
                                         : chunk->max_allocation_bytes,
                                     &frame)) {
        /* Exact native cause already retained. */
    } else {
        mc_tile_entity_map_backing *map = chunk->l->backing;
        if (map->journal_started)
            live_fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "TileEntity storage import cannot retry same Chunk");
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
                    live_fail(&frame, MC_WORLD_STORAGE_LIMIT,
                              "TileEntity import journal allocation");
                else if (!live_graph_measure(chunk, NULL, ceiling, &graph, &frame)) {
                    /* Complete old graph measured with no scratch request. */
                } else if (count * sizeof(bool) > ceiling - graph)
                    live_fail(&frame, MC_WORLD_STORAGE_LIMIT,
                              "TileEntity import complete journal peak");
                else {
                    bool *consumed = count == 0 ? NULL : calloc(count, sizeof(*consumed));
                    if (count != 0 && consumed == NULL)
                        live_fail(&frame, MC_WORLD_STORAGE_ALLOCATION_FAILED,
                                  "TileEntity import journal allocation");
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
                            mc_tile_entity *tile_entity = NULL;
                            mc_live_tile_entity_error nested;
                            mc_live_tile_entities_error_init(&nested);
                            size_t previous_graph;
                            if (!live_graph_measure(chunk, NULL, ceiling, &previous_graph, &frame))
                                break;
                            mc_nbt_limits constructor_limits = limits;
                            constructor_limits.max_allocation_bytes = ceiling;
                            if (live_tile_entities_factory_scoped(
                                    world, record, &constructor_limits, true, &tile_entity, &nested,
                                    chunk) != MC_WORLD_STORAGE_OK) {
                                frame.tile_entity = nested.tile_entity;
                                frame.registry = nested.registry;
                                frame.initializer_bytecode = nested.initializer_bytecode;
                                frame.chunk_bytecode = nested.bytecode;
                                live_cause(&frame, nested.cause);
                                break;
                            }
                            if (tile_entity == NULL)
                                continue; /* Actual unmapped-ID skip; retain opaque record. */
                            const int32_t lx = mc_java_i32_sub(
                                tile_entity->x, mc_java_i32_mul(chunk->constructor_x, 16));
                            const int32_t ly = tile_entity->y;
                            const int32_t lz = mc_java_i32_sub(
                                tile_entity->z, mc_java_i32_mul(chunk->constructor_z, 16));
                            frame.bytecode = 352;
                            bool installed_ok = live_chunk_install_held(
                                chunk, lx, ly, lz, tile_entity, ceiling, &frame);
                            if (installed_ok && chunk->state->active) {
                                frame.chunk_bytecode = 56;
                                installed_ok = live_list_add(world, false, tile_entity, &frame);
                            }
                            if (installed_ok) {
                                live_map_node *installed =
                                    live_map_find(map, lx, ly, lz, live_key_hash(lx, ly, lz));
                                /* Witness at this exact publication, never a later
                                 * search by matching coordinates/type/payload. */
                                consumed[i] = installed != NULL && installed->value == tile_entity;
                            }
                            mc_tile_entity_release(tile_entity);
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
                                    frame.tile_entity.nbt = nbt;
                                    live_fail(&frame,
                                              copied == MC_NBT_LIMIT ? MC_WORLD_STORAGE_LIMIT
                                              : copied == MC_NBT_ALLOCATION_FAILED
                                                  ? MC_WORLD_STORAGE_ALLOCATION_FAILED
                                                  : MC_WORLD_STORAGE_INVALID_DATA,
                                              "TileEntity import provenance copy");
                                } else {
                                    const size_t bytes = mc_nbt_tree_allocation_bytes(journal);
                                    if (chunk->allocation_bytes > chunk->max_allocation_bytes ||
                                        bytes >
                                            chunk->max_allocation_bytes - chunk->allocation_bytes) {
                                        mc_nbt_tree_free(journal);
                                        live_fail(&frame, MC_WORLD_STORAGE_LIMIT,
                                                  "TileEntity import retained provenance budget");
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
    mc_tile_entity *tile_entity;
    mc_nbt_tag root, fields[9], slots[36];
    mc_item_stack_record_view items[36];
    const mc_item_stack *stacks[36];
    size_t counts[36], occupied, root_count, pointer_count, field_count;
} live_record_view;

static mc_tile_entity_status live_record_plan(mc_tile_entity *tile_entity, live_record_view *view,
                                              const mc_nbt_limits *limits,
                                              mc_tile_entity_error *error) {
    mc_tile_entity_status status = tile_entity_ready(tile_entity);
    if (status != MC_TILE_ENTITY_OK)
        return tile_entity_status(error, status, NULL, SIZE_MAX);
    size_t checked;
    status = mc_tile_entity_storage_bytes(tile_entity, &checked);
    if (status != MC_TILE_ENTITY_OK)
        return tile_entity_status(error, status, NULL, SIZE_MAX);
    view->tile_entity = tile_entity;
    status = tile_entity_build_fields(tile_entity, view->fields, &view->field_count, error);
    if (status != MC_TILE_ENTITY_OK)
        return status;
    status = tile_entity_overlay_pointer_count(mc_tile_entity_source(tile_entity), view->fields,
                                               view->field_count, &view->root_count);
    if (status != MC_TILE_ENTITY_OK || view->root_count > limits->max_elements)
        return tile_entity_status(error, MC_TILE_ENTITY_LIMIT, NULL, SIZE_MAX);
    view->pointer_count = view->root_count;
    if (tile_entity->kind == MC_TILE_ENTITY_CHEST) {
        for (size_t slot = 0; slot < tile_entity->state.chest.capacity; ++slot) {
            mc_item_stack *stack = tile_entity->state.chest.slots[slot];
            if (stack == NULL)
                continue;
            const size_t n = view->occupied;
            view->slots[n].type = MC_NBT_BYTE;
            view->slots[n].name = (mc_nbt_string){tile_entity_slot_name, 4};
            view->slots[n].value.byte_value = mc_java_i8_narrow((int32_t)slot);
            view->stacks[n] = stack;
            mc_item_stack_error item_error = {0};
            mc_item_stack_status item = mc_item_stack_record_pointer_count(
                stack, &view->slots[n], 1, &view->counts[n], &item_error);
            if (item != MC_ITEM_STACK_OK) {
                const mc_tile_entity_status failure =
                    tile_entity_item_error(error, item, &item_error, n);
                if (error != NULL)
                    error->save_bytecode = 59;
                return failure;
            }
            if (view->counts[n] > limits->max_elements ||
                !tile_entity_size_add(&view->pointer_count, view->counts[n]))
                return tile_entity_status(error, MC_TILE_ENTITY_LIMIT, "Items[]", n);
            ++view->occupied;
        }
        if (!tile_entity_size_add(&view->pointer_count, view->occupied))
            return tile_entity_status(error, MC_TILE_ENTITY_LIMIT, "Items", SIZE_MAX);
    }
    return MC_TILE_ENTITY_OK;
}

static mc_tile_entity_status live_record_prepare(live_record_view *view, const mc_nbt_tag **scratch,
                                                 mc_tile_entity_error *error) {
    size_t offset = view->root_count + view->occupied;
    for (size_t i = 0; i < view->occupied; ++i) {
        mc_item_stack_error item_error = {0};
        mc_item_stack_status item =
            mc_item_stack_record_overlay(view->stacks[i], &view->slots[i], 1, scratch + offset,
                                         view->counts[i], &view->items[i], &item_error);
        if (item != MC_ITEM_STACK_OK) {
            const mc_tile_entity_status failure =
                tile_entity_item_error(error, item, &item_error, i);
            if (error != NULL)
                error->save_bytecode = 59;
            return failure;
        }
        scratch[view->root_count + i] = &view->items[i].root;
        offset += view->counts[i];
    }
    if (view->tile_entity->kind == MC_TILE_ENTITY_CHEST) {
        view->fields[4].value.list.items = view->occupied == 0 ? NULL : scratch + view->root_count;
        view->fields[4].value.list.length = view->occupied;
    }
    tile_entity_overlay(mc_tile_entity_source(view->tile_entity), view->fields, view->field_count,
                        scratch);
    view->root.type = MC_NBT_COMPOUND;
    const mc_nbt_tag *source = mc_tile_entity_source(view->tile_entity);
    if (source != NULL)
        view->root.name = source->name;
    view->root.value.compound.items = scratch;
    view->root.value.compound.length = view->root_count;
    return MC_TILE_ENTITY_OK;
}

/* Native accounting only: address hashing never selects source Map traversal. */
static bool live_seen_insert(const void **seen, size_t capacity, const void *pointer) {
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

mc_world_storage_status mc_live_tile_entities_build_records(const mc_world_chunk *chunk,
                                                            const mc_nbt_limits *requested,
                                                            mc_nbt_tree **out,
                                                            mc_live_tile_entity_error *error) {
    mc_live_tile_entity_error frame;
    mc_live_tile_entities_error_init(&frame);
    mc_nbt_limits limits;
    if (out == NULL || chunk == NULL || !tile_entity_limits(requested, &limits) ||
        !mc_live_tile_entities_map_valid(chunk)) {
        live_fail(&frame, MC_WORLD_STORAGE_INVALID_ARGUMENT, "TileEntity records build arguments");
        return live_finish(NULL, frame, error);
    }
    const mc_tile_entity_map_backing *map = chunk->l->backing;
    if ((map == NULL && chunk->l->release_backing != NULL) ||
        (map != NULL && map->journal_started && !map->journal_complete)) {
        live_fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                  "TileEntity records incomplete/retired owner");
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
        live_fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                  "TileEntity record provenance witness");
        return live_finish(NULL, frame, error);
    }
    size_t opaque = 0;
    for (size_t i = 0; i < old_count; ++i)
        if (!map->consumed[i])
            ++opaque;
    size_t output_count = count;
    if (!tile_entity_size_add(&output_count, opaque) || output_count > limits.max_elements ||
        count > SIZE_MAX / sizeof(live_record_view) || count > (SIZE_MAX / 2u) / 38u) {
        live_fail(&frame, MC_WORLD_STORAGE_LIMIT, "TileEntity records output/scratch bounds");
        return live_finish(NULL, frame, error);
    }
    size_t seen_capacity = 1;
    const size_t seen_required = count * 38u * 2u;
    while (seen_capacity < seen_required) {
        if (seen_capacity > SIZE_MAX / 2u) {
            live_fail(&frame, MC_WORLD_STORAGE_LIMIT, "TileEntity records accounting capacity");
            return live_finish(NULL, frame, error);
        }
        seen_capacity *= 2u;
    }
    if (seen_capacity > SIZE_MAX / sizeof(void *)) {
        live_fail(&frame, MC_WORLD_STORAGE_LIMIT, "TileEntity records accounting bytes");
        return live_finish(NULL, frame, error);
    }
    const size_t seen_bytes = seen_capacity * sizeof(void *);
    const size_t view_bytes = count * sizeof(live_record_view);
    size_t complete_graph;
    if (!live_graph_measure(chunk, NULL, limits.max_allocation_bytes, &complete_graph, &frame))
        return live_finish(NULL, frame, error);
    size_t admitted_peak = complete_graph;
    if (!tile_entity_size_add(&admitted_peak, seen_bytes) ||
        !tile_entity_size_add(&admitted_peak, view_bytes) ||
        admitted_peak > limits.max_allocation_bytes) {
        live_fail(&frame, MC_WORLD_STORAGE_LIMIT,
                  "TileEntity records complete planning peak budget");
        return live_finish(NULL, frame, error);
    }
    size_t live = chunk->allocation_bytes;
    if (!tile_entity_size_add(&live, seen_bytes) || !tile_entity_size_add(&live, view_bytes) ||
        live > limits.max_allocation_bytes) {
        live_fail(&frame, MC_WORLD_STORAGE_LIMIT, "TileEntity records initial live budget");
        return live_finish(NULL, frame, error);
    }
    const void **seen = calloc(seen_capacity, sizeof(*seen));
    live_record_view *views = count == 0 ? NULL : calloc(count, sizeof(*views));
    if (seen == NULL || (count != 0 && views == NULL)) {
        free(seen);
        free(views);
        live_fail(&frame, MC_WORLD_STORAGE_ALLOCATION_FAILED,
                  "TileEntity records planning allocation");
        return live_finish(NULL, frame, error);
    }
    size_t pointer_count = output_count;
    for (size_t i = 0; i < count && frame.status == MC_WORLD_STORAGE_OK; ++i) {
        mc_tile_entity *tile_entity = NULL;
        if (mc_live_tile_entities_map_at(chunk, i, &tile_entity) != MC_WORLD_STORAGE_OK) {
            live_fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "TileEntity records Map iteration");
            break;
        }
        mc_tile_entity_status planned =
            live_record_plan(tile_entity, &views[i], &limits, &frame.tile_entity);
        if (planned != MC_TILE_ENTITY_OK) {
            live_tile_entity_error(&frame, planned, "TileEntity record semantic plan");
            if (frame.tile_entity.base_save_bytecode != 0 || frame.tile_entity.save_bytecode != 0)
                frame.bytecode = 271;
            break;
        }
        if (!tile_entity_size_add(&pointer_count, views[i].pointer_count)) {
            live_fail(&frame, MC_WORLD_STORAGE_LIMIT, "TileEntity records pointer overflow");
            break;
        }
        if (live_seen_insert(seen, seen_capacity, tile_entity)) {
            size_t direct = mc_tile_entity_native_bytes(tile_entity);
            if (!tile_entity_size_add(&direct, mc_tile_entity_source_bytes(tile_entity)) ||
                !tile_entity_size_add(&live, direct)) {
                live_fail(&frame, MC_WORLD_STORAGE_LIMIT, "TileEntity records owner graph");
                break;
            }
            if (tile_entity->kind == MC_TILE_ENTITY_CHEST) {
                for (size_t slot = 0; slot < tile_entity->state.chest.capacity; ++slot) {
                    mc_item_stack *stack = tile_entity->state.chest.slots[slot];
                    if (live_seen_insert(seen, seen_capacity, stack)) {
                        size_t bytes;
                        mc_item_stack_status item = mc_item_stack_native_bytes(stack, &bytes);
                        if (item != MC_ITEM_STACK_OK || !tile_entity_size_add(&live, bytes)) {
                            live_fail(&frame, MC_WORLD_STORAGE_LIMIT,
                                      "TileEntity records Stack graph");
                            break;
                        }
                    }
                }
            } else if (tile_entity->kind == MC_TILE_ENTITY_SPAWNER &&
                       live_seen_insert(seen, seen_capacity, tile_entity->state.spawner.name)) {
                if (!tile_entity_size_add(
                        &live, mc_tile_entity_name_native_bytes(tile_entity->state.spawner.name)))
                    live_fail(&frame, MC_WORLD_STORAGE_LIMIT, "TileEntity records name graph");
            }
        }
    }
    const mc_nbt_tag **scratch = NULL;
    if (frame.status == MC_WORLD_STORAGE_OK) {
        if (pointer_count > SIZE_MAX / sizeof(*scratch) ||
            !tile_entity_size_add(&live, pointer_count * sizeof(*scratch)) ||
            live > limits.max_allocation_bytes)
            live_fail(&frame, MC_WORLD_STORAGE_LIMIT, "TileEntity records complete peak budget");
        else {
            scratch = pointer_count == 0 ? NULL : malloc(pointer_count * sizeof(*scratch));
            if (pointer_count != 0 && scratch == NULL)
                live_fail(&frame, MC_WORLD_STORAGE_ALLOCATION_FAILED,
                          "TileEntity records pointer arena");
        }
    }
    mc_nbt_tree *built = NULL;
    if (frame.status == MC_WORLD_STORAGE_OK) {
        size_t offset = output_count;
        for (size_t i = 0; i < count; ++i) {
            mc_tile_entity_status prepared =
                live_record_prepare(&views[i], scratch + offset, &frame.tile_entity);
            if (prepared != MC_TILE_ENTITY_OK) {
                live_tile_entity_error(&frame, prepared, "TileEntity records borrowed overlay");
                if (frame.tile_entity.save_bytecode != 0)
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
                live_fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                          "TileEntity records merge witness");
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
                    frame.tile_entity.nbt = nbt;
                    live_fail(&frame,
                              copied == MC_NBT_LIMIT ? MC_WORLD_STORAGE_LIMIT
                              : copied == MC_NBT_ALLOCATION_FAILED
                                  ? MC_WORLD_STORAGE_ALLOCATION_FAILED
                                  : MC_WORLD_STORAGE_INVALID_DATA,
                              "TileEntity records independent output tree");
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

static bool live_graph_measure(const mc_world_chunk *chunk, mc_tile_entity *extra, size_t ceiling,
                               size_t *out, mc_live_tile_entity_error *error) {
    const size_t count = chunk->l->count;
    if (!mc_live_tile_entities_map_valid(chunk) || count == SIZE_MAX) {
        live_fail(error, MC_WORLD_STORAGE_LIMIT, "TileEntity graph accounting count");
        return false;
    }
    /* Nonallocating deduplication precedes EVERY measuring/planning request.
     * The actual Chunk ledger already includes all Map/backing/table/consumed/
     * journal (including opaque records) storage. Add each reachable TileEntity,
     * Stack+its source, name and TileEntity source exactly once, plus an explicitly
     * held unpublished/replacement TileEntity. Repeated scans are native accounting,
     * never source iteration selection or a source work clamp. */
    size_t live = chunk->allocation_bytes;
    if (live > ceiling) {
        live_fail(error, MC_WORLD_STORAGE_LIMIT, "TileEntity graph accounting peak budget");
        return false;
    }
    const size_t total = count + (extra == NULL ? 0u : 1u);
    for (size_t i = 0; i < total && error->status == MC_WORLD_STORAGE_OK; ++i) {
        mc_tile_entity *tile_entity = extra;
        if (i < count &&
            mc_live_tile_entities_map_at(chunk, i, &tile_entity) != MC_WORLD_STORAGE_OK) {
            live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "TileEntity graph actual Map iteration");
            break;
        }
        bool seen_tile_entity = false;
        for (size_t j = 0; j < i; ++j) {
            mc_tile_entity *previous = NULL;
            if (mc_live_tile_entities_map_at(chunk, j, &previous) != MC_WORLD_STORAGE_OK) {
                live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                          "TileEntity graph prior Map iteration");
                return false;
            }
            if (previous == tile_entity) {
                seen_tile_entity = true;
                break;
            }
        }
        if (seen_tile_entity)
            continue;
        size_t validated;
        mc_tile_entity_status status = mc_tile_entity_storage_bytes(tile_entity, &validated);
        if (status != MC_TILE_ENTITY_OK) {
            live_tile_entity_error(error, status, "TileEntity graph native owner invariant");
            break;
        }
        size_t bytes = mc_tile_entity_native_bytes(tile_entity);
        if (!tile_entity_size_add(&bytes, mc_tile_entity_source_bytes(tile_entity)) ||
            !tile_entity_size_add(&live, bytes)) {
            live_fail(error, MC_WORLD_STORAGE_LIMIT, "TileEntity graph owner bytes");
            break;
        }
        if (tile_entity->kind == MC_TILE_ENTITY_CHEST) {
            for (size_t slot = 0; slot < tile_entity->state.chest.capacity; ++slot) {
                mc_item_stack *stack = tile_entity->state.chest.slots[slot];
                bool seen_stack = stack == NULL;
                for (size_t j = 0; j <= i && !seen_stack; ++j) {
                    mc_tile_entity *previous = tile_entity;
                    if (j < i &&
                        mc_live_tile_entities_map_at(chunk, j, &previous) != MC_WORLD_STORAGE_OK) {
                        live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                  "TileEntity graph Stack Map iteration");
                        return false;
                    }
                    if (previous->kind != MC_TILE_ENTITY_CHEST)
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
                    if (item != MC_ITEM_STACK_OK || !tile_entity_size_add(&live, bytes)) {
                        live_fail(error, MC_WORLD_STORAGE_LIMIT, "TileEntity graph Stack bytes");
                        break;
                    }
                }
            }
        } else if (tile_entity->kind == MC_TILE_ENTITY_SPAWNER) {
            mc_tile_entity_name *name = tile_entity->state.spawner.name;
            bool seen_name = name == NULL;
            for (size_t j = 0; j < i && !seen_name; ++j) {
                mc_tile_entity *previous = NULL;
                if (mc_live_tile_entities_map_at(chunk, j, &previous) != MC_WORLD_STORAGE_OK) {
                    live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                              "TileEntity graph name Map iteration");
                    return false;
                }
                seen_name = previous->kind == MC_TILE_ENTITY_SPAWNER &&
                            previous->state.spawner.name == name;
            }
            if (!seen_name && !tile_entity_size_add(&live, mc_tile_entity_name_native_bytes(name)))
                live_fail(error, MC_WORLD_STORAGE_LIMIT, "TileEntity graph name bytes");
        }
        if (live > ceiling)
            live_fail(error, MC_WORLD_STORAGE_LIMIT, "TileEntity graph complete peak budget");
    }
    if (error->status != MC_WORLD_STORAGE_OK)
        return false;
    *out = live;
    return true;
}

static bool live_graph_has_name(const mc_world_chunk *chunk, const mc_tile_entity *extra,
                                const mc_tile_entity_name *name) {
    if (name == NULL)
        return true;
    if (extra != NULL && extra->kind == MC_TILE_ENTITY_SPAWNER && extra->state.spawner.name == name)
        return true;
    for (size_t i = 0; i < chunk->l->count; ++i) {
        mc_tile_entity *tile_entity = NULL;
        if (mc_live_tile_entities_map_at(chunk, i, &tile_entity) != MC_WORLD_STORAGE_OK)
            return false; /* The following full admission still rejects invalid Map state. */
        if (tile_entity->kind == MC_TILE_ENTITY_SPAWNER && tile_entity->state.spawner.name == name)
            return true;
    }
    return false;
}

static mc_tile_entity_status tile_entity_import_measure(const mc_tile_entity *tile_entity,
                                                        const tile_entity_import_context *context,
                                                        size_t *out) {
    if (context == NULL || context->scope_chunk == NULL)
        return mc_tile_entity_storage_bytes(tile_entity, out);
    mc_live_tile_entity_error frame;
    mc_live_tile_entities_error_init(&frame);
    if (live_graph_measure(context->scope_chunk, (mc_tile_entity *)tile_entity,
                           context->limits.max_allocation_bytes, out, &frame))
        return MC_TILE_ENTITY_OK;
    return frame.status == MC_WORLD_STORAGE_LIMIT ? MC_TILE_ENTITY_LIMIT
                                                  : MC_TILE_ENTITY_STATE_ERROR;
}

const void *mc_live_tile_entities_name_identity(const mc_tile_entity_name *name) {
    return name == NULL                     ? NULL
           : name->literal_identity != NULL ? (const void *)name->literal_identity
                                            : (const void *)name;
}

mc_world_storage_status mc_live_tile_entities_literal(mc_world_runtime *world, const char *ascii,
                                                      mc_tile_entity_name **out,
                                                      mc_live_tile_entity_error *error) {
    mc_live_tile_entity_error frame;
    mc_live_tile_entities_error_init(&frame);
    if (ascii == NULL || out == NULL) {
        live_fail(&frame, MC_WORLD_STORAGE_INVALID_ARGUMENT, "TileEntity code literal arguments");
        return live_finish(NULL, frame, error);
    }
    if (!live_enter(world, &frame))
        return live_finish(NULL, frame, error);
    mc_tile_entity_classes *classes = NULL;
    mc_tile_entity_status status = live_classes_owner(world, &classes);
    mc_tile_entity_name *name = NULL;
    if (status == MC_TILE_ENTITY_OK)
        status = live_literal(classes, ascii, &name, &frame.registry, NULL, NULL);
    if (status != MC_TILE_ENTITY_OK) {
        live_tile_entity_error(&frame, status, "TileEntity reached shared literal");
    } else
        *out = name;
    mc_world_runtime_call_leave(world);
    return live_finish(world, frame, error);
}

/* Captured active iterator kernels. Native ownership is checked against the
 * actual captured ledger, independently of replaceable current source c/G. */
static mc_tile_entity_list_ledger *tick_iterator_owner(mc_world_runtime *world,
                                                       mc_world_reference_list *identity,
                                                       mc_live_tile_entity_error *error,
                                                       unsigned int pc) {
    for (size_t k = 0; identity != NULL && k < 2; ++k) {
        mc_tile_entity_list_ledger *ledger = &world->tile_entity_membership[k];
        if (identity != ledger->identity)
            continue;
        if (!live_list_shape(identity) || identity->count != ledger->count ||
            identity->capacity != ledger->capacity ||
            ((ledger->capacity == 0) != (ledger->holds == NULL)) ||
            (identity->items != NULL && (void *)identity->items == (void *)ledger->holds))
            break;
        return ledger;
    }
    error->bytecode = pc;
    live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "TileEntity captured iterator owner");
    return NULL;
}

bool mc_live_tile_iterator_begin_held(mc_world_runtime *world, mc_world_reference_list *identity,
                                      mc_live_tile_iterator *out,
                                      mc_live_tile_entity_error *error) {
    mc_live_tile_entities_error_init(error);
    mc_tile_entity_list_ledger *ledger = tick_iterator_owner(world, identity, error, 357);
    if (ledger == NULL)
        return false;
    *out = (mc_live_tile_iterator){
        .identity = identity, .last_returned = SIZE_MAX, .expected_mod_count = ledger->mod_count};
    return true;
}

bool mc_live_tile_iterator_has_next_held(mc_world_runtime *world, mc_live_tile_iterator *iterator,
                                         bool *out, mc_live_tile_entity_error *error) {
    mc_live_tile_entities_error_init(error);
    if (tick_iterator_owner(world, iterator->identity, error, 364) == NULL)
        return false;
    *out = iterator->cursor != iterator->identity->count;
    return true;
}

bool mc_live_tile_iterator_next_held(mc_world_runtime *world, mc_live_tile_iterator *iterator,
                                     mc_tile_entity **out, mc_live_tile_entity_error *error) {
    mc_live_tile_entities_error_init(error);
    mc_tile_entity_list_ledger *ledger = tick_iterator_owner(world, iterator->identity, error, 373);
    if (ledger == NULL)
        return false;
    error->bytecode = 373;
    if (ledger->mod_count != iterator->expected_mod_count ||
        iterator->cursor >= iterator->identity->count) {
        live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                  "TileEntity actual iterator next concurrent modification/extent");
        return false;
    }
    const size_t index = iterator->cursor;
    if (iterator->identity->items[index] != ledger->holds[index]) {
        error->bytecode = 378;
        live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                  "TileEntity iterator reached cast provenance");
        return false;
    }
    mc_tile_entity *tile = ledger->holds[index];
    if (tile != NULL) {
        const mc_tile_entity_status held = mc_tile_entity_retain(tile);
        if (held != MC_TILE_ENTITY_OK) {
            live_tile_entity_error(error, held, "TileEntity iterator local hold");
            return false;
        }
    }
    iterator->cursor = index + 1;
    iterator->last_returned = index;
    *out = tile;
    return true;
}

bool mc_live_tile_iterator_remove_held(mc_world_runtime *world, mc_live_tile_iterator *iterator,
                                       mc_live_tile_entity_error *error) {
    mc_live_tile_entities_error_init(error);
    mc_tile_entity_list_ledger *ledger = tick_iterator_owner(world, iterator->identity, error, 401);
    if (ledger == NULL)
        return false;
    const size_t index = iterator->last_returned;
    error->bytecode = 401;
    if (index == SIZE_MAX || ledger->mod_count != iterator->expected_mod_count ||
        index >= iterator->identity->count ||
        iterator->identity->items[index] != ledger->holds[index]) {
        live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                  "TileEntity actual iterator remove state/concurrent modification");
        return false;
    }
    mc_tile_entity *victim = ledger->holds[index];
    memmove(iterator->identity->items + index, iterator->identity->items + index + 1,
            (ledger->count - index - 1) * sizeof(*iterator->identity->items));
    memmove(ledger->holds + index, ledger->holds + index + 1,
            (ledger->count - index - 1) * sizeof(*ledger->holds));
    --ledger->count;
    iterator->identity->count = ledger->count;
    iterator->identity->items[ledger->count] = NULL;
    ledger->holds[ledger->count] = NULL;
    ledger->mod_count = mc_java_i32_add(ledger->mod_count, 1);
    iterator->expected_mod_count = ledger->mod_count;
    iterator->cursor = index;
    iterator->last_returned = SIZE_MAX;
    mc_tile_entity_release(victim);
    return true;
}

bool mc_live_tile_chunk_remove_held(mc_world_chunk *chunk, int32_t x, int32_t y, int32_t z,
                                    mc_live_tile_entity_error *error) {
    mc_live_tile_entities_error_init(error);
    return live_chunk_remove_held(chunk, x, y, z, error);
}

mc_registry_classes *mc_live_tile_tick_code_registry(const mc_tile_entity *tile) {
    return tile == NULL || tile->classes == NULL ? NULL : tile->classes->registry;
}
