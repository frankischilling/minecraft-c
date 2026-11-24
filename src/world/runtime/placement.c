#include "mc/world/runtime/placement.h"

#include "../../block/registry/registry_internal.h"
#include "../../block/registry/runtime_internal.h"
#include "../../entity/entity_internal.h"
#include "../../entity/living/fields_internal.h"
#include "../../entity/player/nbt_internal.h"
#include "../../item/render/internal.h"
#include "bed_occupancy_execution_internal.h"
#include "block_queries_internal.h"
#include "collision/blocks_internal.h"
#include "construction_internal.h"
#include "entity_publication_internal.h"
#include "mc/core/java/java.h"
#include "mc/entity/player/sleep.h"
#include "mc/inventory/chest.h"
#include "mc/inventory/dispenser.h"
#include "mc/inventory/furnace.h"
#include "mc/world/runtime/bed_state.h"
#include "mc/world/runtime/blocks.h"
#include "mc/world/runtime/collision/spawning.h"
#include "mc/world/runtime/mutations.h"
#include "mc/world/runtime/tile_entities/tile_entities.h"
#include "mc/world/storage/source_chunk_loader.h"
#include "mutation/fire/fire_internal.h"
#include "mutation/mutation_internal.h"
#include "mutation/piston_internal.h"
#include "plant_substrate_internal.h"
#include "runtime_internal.h"
#include "step_internal.h"
#include "tile_entities/source_detailed_internal.h"
#include "torch_support_internal.h"

#include <emmintrin.h>
#include <stdlib.h>
#include <string.h>
#include <xmmintrin.h>

enum { PLACEMENT_RECEIPTS = 16, PLACEMENT_SOURCES = 4, PLACEMENT_QUERIES = 8 };

typedef struct chest_placement_work {
    mc_world_chest_placement_detail detail;
    mc_world_runtime_acquisition_error *queries[24];
} chest_placement_work;

typedef struct furnace_activation_work {
    mc_live_tile_entity_error tile_error;
    mc_tile_entity *tile;
    mc_inventory_furnace *inventory;
    mc_inventory_error inventory_error;
} furnace_activation_work;

typedef struct dispenser_activation_work {
    mc_live_tile_entity_error tile_error;
    mc_tile_entity *tile;
    mc_tile_entity_classes *classes;
    mc_entity *player;
    mc_inventory_dispenser *inventory;
    mc_inventory_error inventory_error, cleanup;
} dispenser_activation_work;

struct mc_world_placement_work {
    mc_world_runtime *world;
    mc_registry_classes *registry;
    mc_world_runtime_classes *block_code_classes;
    mc_entity_classes *box_owner;
    mc_entity_bounds *box;
    mc_world_runtime_acquisition_error *queries[PLACEMENT_QUERIES];
    chest_placement_work *chest_placement;
    furnace_activation_work *furnace;
    dispenser_activation_work *dispenser;
    mc_math_classes *math;
    mc_entity *placed_by_living;
    mc_world_solid_support_detail *supports[5];
    mc_world_mutation_error *mutation;
    mc_world_step_error *step;
    mc_world_collision_error *collision;
    mc_world_spawn_collision_error *entities;
    mc_entity *bed_player;
    mc_entity_classes *bed_enum_owner;
    mc_player_sleep_error *sleep;
    mc_player_sleep_call *sleep_call;
    mc_player_sleep_cleanup sleep_cleanup;
    mc_player_error *bed_occupant;
    mc_player_sleep_result_error *sleep_result;
    mc_world_provider_error provider;
    mc_live_tile_entity_error *chest_tiles[5];
    mc_tile_entity *chest_receivers[5];
    mc_inventory_chest *chest_views[9];
    size_t chest_view_count;
    mc_inventory_error chest_error;
    mc_world_source_chunk_error *sources[PLACEMENT_SOURCES];
    const mc_world_source_chunk_error *source_identities[PLACEMENT_SOURCES];
    mc_world_access_failure receipts[PLACEMENT_RECEIPTS];
    size_t source_count, receipt_count;
    bool active, execution_children;
};

static mc_world_placement_site site(const char *owner, const char *method, const char *descriptor,
                                    unsigned int pc) {
    return (mc_world_placement_site){owner, method, descriptor, pc};
}

static mc_world_storage_status fail(mc_world_placement_error *e, mc_world_storage_status status,
                                    const char *operation, bool native) {
    e->status = status;
    e->operation = operation;
    e->native_policy = native;
    e->cause = (mc_world_storage_error){status, operation, 0};
    return status;
}

static mc_world_storage_status null_reference(mc_world_placement_error *e, const char *operation) {
    e->source_exception = true;
    e->exception_class = "java/lang/NullPointerException";
    return fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, operation, false);
}

static mc_world_storage_status registry_failed_in(mc_world_placement_error *e,
                                                  mc_registry_classes *classes) {
    const mc_registry_status status = e->registry.status;
    const mc_world_storage_status result = mc_world_runtime_registry_status(status);
    mc_item_render_throwable throwable = MC_ITEM_RENDER_NO_THROWABLE;
    mc_item_render_registry_classification(classes, &e->registry, &throwable, &e->exception_class);
    e->source_exception = throwable == MC_ITEM_RENDER_SOURCE_EXCEPTION;
    e->source_error = throwable == MC_ITEM_RENDER_SOURCE_ERROR;
    fail(e, result, e->registry.message, throwable == MC_ITEM_RENDER_NO_THROWABLE);
    return result;
}

static mc_world_storage_status registry_failed(mc_world_placement_error *e) {
    return registry_failed_in(e, e->work->registry);
}

static mc_world_storage_status registry_value_failed(mc_world_placement_error *e,
                                                     const mc_world_registry_failure *view,
                                                     mc_registry_classes *legacy_classes) {
    if (!mc_world_registry_failure_matches(view, &e->registry))
        return registry_failed_in(e, legacy_classes);
    e->exception_class = view->exception_class;
    e->source_exception = view->kind == MC_WORLD_REGISTRY_FAILURE_SOURCE_EXCEPTION;
    e->source_error = view->kind == MC_WORLD_REGISTRY_FAILURE_SOURCE_ERROR;
    return fail(e, mc_world_runtime_registry_status(e->registry.status), e->registry.message,
                view->kind == MC_WORLD_REGISTRY_FAILURE_NATIVE);
}

static bool allocate(mc_world_placement_error *e, mc_world_placement_site entry) {
    if (e == NULL)
        return false;
    /* A pending diagnostic is never reset or consumed by a second call. */
    if (e->work != NULL)
        return false;
    memset(e, 0, sizeof(*e));
    e->site = entry;
    e->registry = (mc_registry_error){.initializer_pc = -1,
                                      .block_id = -1,
                                      .constructor_pc = -1,
                                      .active_initializer_pc = -1,
                                      .item_argument = -1};
    e->work = calloc(1, sizeof(*e->work));
    if (e->work == NULL) {
        fail(e, MC_WORLD_STORAGE_ALLOCATION_FAILED, "Placement retained diagnostic allocation",
             true);
        return false;
    }
    e->work->active = true;
    return true;
}

static bool hold_world(mc_world_placement_error *e, mc_world_runtime *world) {
    mc_world_placement_work *w = e->work;
    if (w->world == world && world != NULL)
        return true;
    if (world == NULL) {
        null_reference(e, "Block placement actual World null receiver");
        return false;
    }
    if (w->world != NULL || world->construction.mode != MC_WORLD_RUNTIME_GAME ||
        world->construction.phase == MC_WORLD_RUNTIME_RETIRED || world->classes == NULL ||
        world->classes->side != MC_WORLD_INFO_CLIENT) {
        fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Placement actual client GAME World", true);
        return false;
    }
    if (!mc_world_runtime_binding_retain(world)) {
        fail(e, MC_WORLD_STORAGE_BUSY, "Placement actual World occurrence", true);
        return false;
    }
    w->world = world;
    return true;
}

static bool hold_registry(mc_world_placement_error *e, mc_registry_classes *registry) {
    if (e->work->registry == registry && registry != NULL)
        return true;
    if (e->work->registry != NULL) {
        fail(e, MC_WORLD_STORAGE_INVALID_ARGUMENT, "Placement code Registry occurrence changed",
             true);
        return false;
    }
    if (registry == NULL ||
        mc_registry_classes_retain(registry, MC_REGISTRY_CLIENT,
                                   registry == NULL ? NULL : registry->options.random_profile,
                                   &e->registry) != MC_REGISTRY_OK) {
        if (registry == NULL)
            fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Placement actual code Registry", true);
        else
            registry_failed(e);
        return false;
    }
    e->work->registry = registry;
    return true;
}

static bool copy_source(mc_world_placement_error *e, const mc_world_source_chunk_error **slot) {
    const mc_world_source_chunk_error *source = *slot;
    if (source == NULL)
        return true;
    mc_world_placement_work *w = e->work;
    for (size_t i = 0; i < w->source_count; ++i)
        if (source == w->sources[i] || source == w->source_identities[i]) {
            *slot = w->sources[i];
            return true;
        }
    if (w->source_count == PLACEMENT_SOURCES) {
        e->cleanup = (mc_world_storage_error){MC_WORLD_STORAGE_LIMIT,
                                              "Placement loader diagnostic extent", 0};
        return false;
    }
    mc_world_source_chunk_error *copy = malloc(sizeof(*copy));
    if (copy == NULL) {
        e->cleanup = (mc_world_storage_error){MC_WORLD_STORAGE_ALLOCATION_FAILED,
                                              "Placement loader diagnostic copy", 0};
        return false;
    }
    *copy = *source;
    const size_t i = w->source_count++;
    w->sources[i] = copy;
    w->source_identities[i] = source;
    *slot = copy;
    return true;
}

static bool receipt(mc_world_placement_error *e, const mc_world_access_failure *v) {
    if (v->receipt == NULL) {
        /* The held child already owns a value-only null-sound failure. It
         * needs classification, but no producer hold or retained slot. */
        if (e->status != MC_WORLD_STORAGE_OK &&
            mc_world_access_failure_source_exception(v, &e->exception_class)) {
            e->source_exception = true;
            e->source_error = false;
            e->native_policy = false;
        }
        return true;
    }
    mc_world_placement_work *w = e->work;
    for (size_t i = 0; i < w->receipt_count; ++i)
        if (w->receipts[i].receipt == v->receipt)
            return true;
    if (w->receipt_count == PLACEMENT_RECEIPTS) {
        e->cleanup = (mc_world_storage_error){MC_WORLD_STORAGE_LIMIT,
                                              "Placement callback diagnostic extent", 0};
        return false;
    }
    const mc_world_storage_status status =
        mc_world_access_failure_retain(v, &w->receipts[w->receipt_count]);
    if (status != MC_WORLD_STORAGE_OK) {
        e->cleanup = (mc_world_storage_error){status, "Placement callback producer occurrence", 0};
        return false;
    }
    e->retained_producers = ++w->receipt_count;
    return true;
}

static bool publication_receipts(mc_world_placement_error *e, mc_world_entity_batch_error *batch,
                                 mc_live_tile_entity_error *tile) {
    return receipt(e, &batch->publication.world_access_failure) &&
           receipt(e, &tile->world_access_failure) &&
           receipt(e, &tile->publication.world_access_failure) &&
           receipt(e, &tile->batch.publication.world_access_failure);
}

static bool mutation_receipts(mc_world_placement_error *e, mc_world_mutation_error *m) {
    if (m == NULL)
        return true;
    if (e->status != MC_WORLD_STORAGE_OK && (m->piston_source_null || m->piston_class_cast)) {
        e->exception_class = m->piston_source_null ? "java/lang/NullPointerException"
                                                   : "java/lang/ClassCastException";
        e->source_exception = true;
        e->source_error = false;
        e->native_policy = false;
    }
    if (e->status != MC_WORLD_STORAGE_OK &&
        m->world_access_source_fault != MC_WORLD_ACCESS_SOURCE_FAULT_NONE) {
        e->exception_class = "java/lang/NullPointerException";
        e->source_exception = true;
        e->source_error = false;
        e->native_policy = false;
    }
    return receipt(e, &m->world_access_failure) &&
           receipt(e, &m->publication.world_access_failure) &&
           publication_receipts(e, &m->batch, &m->tile_entity) &&
           publication_receipts(e, &m->nearby.batch, &m->nearby.tile_entity) &&
           publication_receipts(e, &m->power.access.batch, &m->power.access.tile_entity) &&
           publication_receipts(e, &m->rail_access.nearby.batch,
                                &m->rail_access.nearby.tile_entity) &&
           copy_source(e, &m->query.source_chunk) &&
           copy_source(e, &m->nearby.query.source_chunk) &&
           copy_source(e, &m->plant.query.source_chunk) &&
           copy_source(e, &m->power.access.query.source_chunk) &&
           copy_source(e, &m->rail_access.nearby.query.source_chunk);
}

static bool capture(mc_world_placement_error *e) {
    mc_world_placement_work *w = e->work;
    for (size_t i = 0; i < PLACEMENT_QUERIES; ++i)
        if (w->queries[i] != NULL &&
            (!copy_source(e, &w->queries[i]->source_chunk) ||
             !publication_receipts(e, &w->queries[i]->batch, &w->queries[i]->tile_entity)))
            return false;
    if (w->chest_placement != NULL)
        for (size_t i = 0; i < w->chest_placement->detail.query_count; ++i) {
            mc_world_runtime_acquisition_error *query = w->chest_placement->queries[i];
            if (!copy_source(e, &query->source_chunk) ||
                !publication_receipts(e, &query->batch, &query->tile_entity))
                return false;
        }
    for (size_t i = 0; i < e->chest_tile_count; ++i)
        if (!publication_receipts(e, &w->chest_tiles[i]->batch, w->chest_tiles[i]))
            return false;
    if (w->furnace != NULL &&
        !publication_receipts(e, &w->furnace->tile_error.batch, &w->furnace->tile_error))
        return false;
    if (w->dispenser != NULL &&
        !publication_receipts(e, &w->dispenser->tile_error.batch, &w->dispenser->tile_error))
        return false;
    mc_world_nearby_error *nearby[] = {w->collision == NULL ? NULL : &w->collision->nearby,
                                       w->entities == NULL ? NULL : &w->entities->nearby,
                                       w->step == NULL ? NULL : &w->step->block_read.nearby};
    for (size_t i = 0; i < sizeof nearby / sizeof *nearby; ++i)
        if (nearby[i] != NULL &&
            (!copy_source(e, &nearby[i]->query.source_chunk) ||
             !publication_receipts(e, &nearby[i]->batch, &nearby[i]->tile_entity)))
            return false;
    for (size_t i = 0; i < e->support_query_count; ++i) {
        mc_world_nearby_error *child = &w->supports[i]->access;
        if (!copy_source(e, &child->query.source_chunk) ||
            !publication_receipts(e, &child->batch, &child->tile_entity))
            return false;
    }
    return mutation_receipts(e, w->mutation) &&
           mutation_receipts(e, w->step == NULL ? NULL : &w->step->mutation);
}

/* Concrete producer receipts supplement the value-only invocation classified
 * during capture. Native adapter refusals and limits carry no Throwable. */
static void classify_receipts(mc_world_placement_error *e) {
    for (size_t i = 0; i < e->work->receipt_count; ++i)
        if (mc_world_access_failure_source_exception(&e->work->receipts[i], &e->exception_class)) {
            e->source_exception = true;
            e->source_error = false;
            e->native_policy = false;
            return;
        }
}

static bool query_value(mc_world_placement_error *e, mc_world_runtime *world,
                        mc_world_runtime_acquisition_error *query, int32_t x, int32_t y, int32_t z,
                        bool metadata, int32_t *out) {
    e->acquisition = query;
    mc_world_storage_error cause = {0};
    const mc_world_storage_status status =
        metadata ? mc_world_runtime_metadata_get_detailed(world, x, y, z, out, &cause, query)
                 : mc_world_runtime_block_get_detailed(world, x, y, z, out, &cause, query);
    if (status != MC_WORLD_STORAGE_OK) {
        fail(e, status, cause.operation, true);
        e->cause = cause;
    }
    if (!capture(e)) {
        if (status == MC_WORLD_STORAGE_OK)
            fail(e, e->cleanup.status, e->cleanup.operation, true);
        return false;
    }
    if (status != MC_WORLD_STORAGE_OK)
        classify_receipts(e);
    return status == MC_WORLD_STORAGE_OK;
}

static bool value_get(mc_world_placement_error *e, mc_world_runtime *world, size_t index, int32_t x,
                      int32_t y, int32_t z, bool metadata, int32_t *out) {
    if (!hold_world(e, world))
        return false;
    mc_world_placement_work *w = e->work;
    if (index >= PLACEMENT_QUERIES || w->queries[index] != NULL) {
        fail(e, MC_WORLD_STORAGE_LIMIT, "Placement reached query diagnostic extent", true);
        return false;
    }
    w->queries[index] = calloc(1, sizeof(*w->queries[index]));
    if (w->queries[index] == NULL) {
        fail(e, MC_WORLD_STORAGE_ALLOCATION_FAILED, "Placement reached World query diagnostic",
             true);
        return false;
    }
    e->queries[index] = w->queries[index];
    ++e->query_count;
    return query_value(e, world, w->queries[index], x, y, z, metadata, out);
}

static bool block_get(mc_world_placement_error *e, mc_world_runtime *world, size_t index, int32_t x,
                      int32_t y, int32_t z, int32_t *out) {
    return value_get(e, world, index, x, y, z, false, out);
}

static bool torch_solid(mc_world_placement_error *e, mc_world_runtime *world, int32_t x, int32_t y,
                        int32_t z, bool *out) {
    if (!hold_world(e, world))
        return false;
    const size_t index = e->support_query_count;
    if (index == 5) {
        fail(e, MC_WORLD_STORAGE_LIMIT, "Placement reached support diagnostic extent", true);
        return false;
    }
    mc_world_solid_support_detail *child = calloc(1, sizeof(*child));
    if (child == NULL) {
        fail(e, MC_WORLD_STORAGE_ALLOCATION_FAILED, "Placement reached support diagnostic", true);
        return false;
    }
    e->work->supports[index] = child;
    e->support_queries[index] = child;
    ++e->support_query_count;
    mc_world_storage_error cause = {0};
    mc_world_storage_status status = mc_world_runtime_call_enter(world, &cause);
    if (status == MC_WORLD_STORAGE_OK) {
        status = mc_world_solid_support_detailed_held(world, x, y, z, out, child);
        mc_world_runtime_call_leave(world);
        cause = child->error.cause;
    }
    if (status != MC_WORLD_STORAGE_OK) {
        fail(e, status, cause.operation, true);
        e->cause = cause;
        e->registry = child->error.registry;
        if (e->registry.status != MC_REGISTRY_OK) {
            mc_item_render_throwable throwable = MC_ITEM_RENDER_NO_THROWABLE;
            mc_item_render_registry_classification(mc_world_runtime_registry(world), &e->registry,
                                                   &throwable, &e->exception_class);
            e->source_exception = throwable == MC_ITEM_RENDER_SOURCE_EXCEPTION;
            e->source_error = throwable == MC_ITEM_RENDER_SOURCE_ERROR;
            e->native_policy = throwable == MC_ITEM_RENDER_NO_THROWABLE;
        }
    }
    if (!capture(e)) {
        if (status == MC_WORLD_STORAGE_OK)
            fail(e, e->cleanup.status, e->cleanup.operation, true);
        return false;
    }
    if (status != MC_WORLD_STORAGE_OK)
        classify_receipts(e);
    return status == MC_WORLD_STORAGE_OK;
}

static bool torch_floor(mc_world_placement_error *e, mc_world_runtime *world, int32_t x, int32_t y,
                        int32_t z, size_t query_index, bool *out) {
    e->site = e->floor = site("vm", "h", "(Lfd;III)Z", 5);
    bool value;
    if (!torch_solid(e, world, x, y, z, &value))
        return false;
    if (!value) {
        e->site.pc = e->floor.pc = 16;
        int32_t id;
        if (!block_get(e, world, query_index, x, y, z, &id))
            return false;
        e->below_read = true;
        e->below_id = id;
        mc_world_torch_fence_error child;
        const mc_registry_status status =
            mc_world_torch_floor_fence(e->work->registry, id, &value, &child);
        e->site.pc = e->floor.pc = child.pc;
        if (status != MC_REGISTRY_OK) {
            e->registry = child.registry;
            registry_failed(e);
            return false;
        }
    }
    e->site.pc = e->floor.pc = 33;
    *out = value;
    return true;
}

static bool torch_predicate(mc_world_placement_error *e, mc_world_runtime *world, int32_t x,
                            int32_t y, int32_t z, size_t query_index, bool *out) {
    const unsigned int pcs[] = {7, 22, 37, 52};
    const int32_t xs[] = {mc_java_i32_sub(x, 1), mc_java_i32_add(x, 1), x, x};
    const int32_t zs[] = {z, z, mc_java_i32_sub(z, 1), mc_java_i32_add(z, 1)};
    bool value = false;
    unsigned int return_pc = 77;
    for (size_t i = 0; i < 4; ++i) {
        e->site = e->base;
        e->site.pc = e->base.pc = pcs[i];
        if (!torch_solid(e, world, xs[i], y, zs[i], &value))
            return false;
        if (value) {
            return_pc = pcs[i] + 7;
            break;
        }
    }
    if (!value) {
        e->base.pc = 68;
        if (!torch_floor(e, world, x, mc_java_i32_sub(y, 1), z, query_index, &value))
            return false;
        if (value)
            return_pc = 75;
    }
    e->site = e->base;
    e->site.pc = e->base.pc = return_pc;
    e->base_returned = e->predicate_returned = true;
    e->base_value = value;
    *out = value;
    return true;
}

#include "placement/bed.inc"
#include "placement/chest.inc"
#include "placement/chest_predicate.inc"
#include "placement/dispenser.inc"
#include "placement/furnace.inc"
#include "placement/lever.inc"

static const char *activation_class(mc_block_kind kind) {
    switch (kind) {
        case MC_BLOCK_TNT:
            return "ah";
        case MC_BLOCK_REDSTONE_ORE:
            return "bs";
        case MC_BLOCK_CHEST:
            return "e";
        case MC_BLOCK_CRAFTING_TABLE:
            return "fi";
        case MC_BLOCK_JUKEBOX:
            return "fo";
        case MC_BLOCK_PISTON_BASE:
            return "jq";
        case MC_BLOCK_DOOR:
            return "le";
        case MC_BLOCK_BUTTON:
            return "oi";
        case MC_BLOCK_TRAPDOOR:
            return "oq";
        case MC_BLOCK_NOTE_BLOCK:
            return "pt";
        case MC_BLOCK_STAIRS:
            return "ss";
        case MC_BLOCK_FURNACE:
            return "tc";
        case MC_BLOCK_CAKE:
            return "um";
        case MC_BLOCK_MOVING_PISTON:
            return "ut";
        case MC_BLOCK_BED:
            return "ve";
        case MC_BLOCK_REPEATER:
            return "wo";
        case MC_BLOCK_DISPENSER:
            return "xq";
        case MC_BLOCK_LEVER:
            return "xr";
        default:
            return "uu";
    }
}

static const char *face_predicate_class(mc_block_kind kind) {
    switch (kind) {
        case MC_BLOCK_PISTON_HEAD:
            return "h";
        case MC_BLOCK_BUTTON:
            return "oi";
        case MC_BLOCK_TRAPDOOR:
            return "oq";
        case MC_BLOCK_MOVING_PISTON:
            return "ut";
        case MC_BLOCK_LEVER:
            return "xr";
        default:
            return "uu";
    }
}

static const char *predicate_class(mc_block_kind kind) {
    switch (kind) {
        case MC_BLOCK_PRESSURE_PLATE:
            return "bv";
        case MC_BLOCK_LADDER:
            return "dp";
        case MC_BLOCK_CHEST:
            return "e";
        case MC_BLOCK_PUMPKIN:
            return "fc";
        case MC_BLOCK_PISTON_HEAD:
            return "h";
        case MC_BLOCK_SNOW_LAYER:
            return "jr";
        case MC_BLOCK_FENCE:
            return "jw";
        case MC_BLOCK_LOCKED_CHEST:
            return "ks";
        case MC_BLOCK_DOOR:
            return "le";
        case MC_BLOCK_BUTTON:
            return "oi";
        case MC_BLOCK_CACTUS:
            return "or";
        case MC_BLOCK_DETECTOR_RAIL:
        case MC_BLOCK_RAIL:
            return "pc";
        case MC_BLOCK_SUGAR_CANE:
            return "ri";
        case MC_BLOCK_REDSTONE:
            return "sm";
        case MC_BLOCK_STAIRS:
            return "ss";
        case MC_BLOCK_CAKE:
            return "um";
        case MC_BLOCK_MOVING_PISTON:
            return "ut";
        case MC_BLOCK_REDSTONE_TORCH:
        case MC_BLOCK_TORCH:
            return "vm";
        case MC_BLOCK_CROPS:
        case MC_BLOCK_DEAD_BUSH:
        case MC_BLOCK_FLOWER:
        case MC_BLOCK_MUSHROOM:
        case MC_BLOCK_SAPLING:
        case MC_BLOCK_TALL_GRASS:
            return "wb";
        case MC_BLOCK_REPEATER:
            return "wo";
        case MC_BLOCK_LEVER:
            return "xr";
        case MC_BLOCK_FIRE:
            return "yq";
        default:
            return "uu";
    }
}

static const char *placed_face_class(mc_block_kind kind) {
    switch (kind) {
        case MC_BLOCK_LADDER:
            return "dp";
        case MC_BLOCK_BUTTON:
            return "oi";
        case MC_BLOCK_TRAPDOOR:
            return "oq";
        case MC_BLOCK_REDSTONE_TORCH:
        case MC_BLOCK_TORCH:
            return "vm";
        case MC_BLOCK_LEVER:
            return "xr";
        default:
            return "uu";
    }
}

static const char *placed_by_class(mc_block_kind kind) {
    switch (kind) {
        case MC_BLOCK_PUMPKIN:
            return "fc";
        case MC_BLOCK_PISTON_BASE:
            return "jq";
        case MC_BLOCK_STAIRS:
            return "ss";
        case MC_BLOCK_FURNACE:
            return "tc";
        case MC_BLOCK_REPEATER:
            return "wo";
        case MC_BLOCK_DISPENSER:
            return "xq";
        default:
            return "uu";
    }
}

static bool receiver(mc_world_placement_error *e, mc_registry_classes *registry,
                     const mc_block *block) {
    if (block == NULL) {
        null_reference(e, "Placement actual Block null invocation receiver");
        return false;
    }
    if (!hold_registry(e, registry))
        return false;
    if (block->classes != registry) {
        fail(e, MC_WORLD_STORAGE_INVALID_ARGUMENT, "Placement Block code Registry identity", true);
        return false;
    }
    return true;
}

static bool inherited(mc_world_placement_error *e, const char *declaring) {
    e->site.class_name = declaring;
    if (strcmp(declaring, "uu") == 0)
        return true;
    e->site.pc = 0;
    fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Placement distinct Block virtual body", true);
    return false;
}

/* This is also WB's invokespecial body: its receiver does not select another
 * virtual can-place method here. The World ID read precedes Block active use. */
static bool uu_predicate(mc_world_placement_error *e, mc_world_runtime *world, int32_t x, int32_t y,
                         int32_t z, size_t query_index, bool *out) {
    e->site = site("uu", "a", "(Lfd;III)Z", 5);
    int32_t id;
    if (!block_get(e, world, query_index, x, y, z, &id))
        return false;
    e->fresh_id = id;
    bool value = true;
    if (id != 0) {
        mc_registry_block_m_view array;
        e->site.pc = 15;
        if (mc_registry_block_m_get(e->work->registry, &array, &e->registry) != MC_REGISTRY_OK) {
            registry_failed(e);
            return false;
        }
        const mc_block *existing = NULL;
        e->site.pc = 20;
        if (mc_registry_block_m_at(&array, id, &existing, &e->registry) != MC_REGISTRY_OK) {
            registry_failed(e);
            return false;
        }
        e->site.pc = 21;
        if (existing == NULL) {
            null_reference(e, "Placement actual fresh Block Material field receiver");
            return false;
        }
        const mc_material *material = existing->fields.bA;
        e->site.pc = 24;
        if (material == NULL) {
            null_reference(e, "Placement actual Material.g null receiver");
            return false;
        }
        value = mc_material_g(material);
    }
    e->site.pc = 35;
    *out = value;
    return true;
}

static bool base_predicate(mc_world_placement_error *e, const mc_block *block,
                           mc_world_runtime *world, int32_t x, int32_t y, int32_t z,
                           size_t query_index, bool *out) {
    const char *declaring = predicate_class(block->kind);
    const bool flower = strcmp(declaring, "wb") == 0;
    e->site = site(declaring, "a", "(Lfd;III)Z", 0);
    e->base = e->site;
    e->predicate_entered = e->base_entered = true;
    if (block->kind == MC_BLOCK_CHEST)
        return chest_predicate(e, block, world, x, y, z, out);
    if (block->kind == MC_BLOCK_FIRE) {
        if (!hold_world(e, world))
            return false;
        if (e->work->mutation == NULL) {
            e->work->mutation = malloc(sizeof(*e->work->mutation));
            if (e->work->mutation == NULL) {
                fail(e, MC_WORLD_STORAGE_ALLOCATION_FAILED, "Fire placement diagnostic allocation",
                     true);
                return false;
            }
            e->mutation = e->work->mutation;
        }
        mc_world_mutation_error *child = e->work->mutation;
        mc_world_mutation_error_init(child);
        mc_world_storage_error admission = {0};
        const mc_world_storage_status status = mc_world_runtime_call_enter(world, &admission);
        if (status != MC_WORLD_STORAGE_OK) {
            fail(e, status, admission.operation, true);
            e->cause = admission;
            return false;
        }
        bool value;
        const bool complete =
            mc_world_mutation_fire_can_place(world, block, x, y, z, &value, child);
        mc_world_runtime_call_leave(world);
        if (!complete) {
            fail(e, child->status, child->operation, true);
            e->cause = child->cause;
            e->registry = child->registry;
            if (child->registry.status != MC_REGISTRY_OK)
                registry_failed_in(e, e->work->registry);
            if (child->fire_source_null || child->fire_array_index) {
                e->source_exception = true;
                e->exception_class = child->fire_source_null
                                         ? "java/lang/NullPointerException"
                                         : "java/lang/ArrayIndexOutOfBoundsException";
                e->native_policy = false;
            }
            if (!capture(e))
                return false;
            return false;
        }
        e->site.pc = e->base.pc = 30;
        e->base_returned = e->predicate_returned = true;
        e->base_value = *out = value;
        return true;
    }
    if (strcmp(declaring, "vm") == 0)
        return torch_predicate(e, world, x, y, z, query_index, out);
    if (block->kind == MC_BLOCK_LEVER)
        return lever_base_predicate(e, world, x, y, z, out);
    if (strcmp(declaring, "sm") == 0) {
        e->site.pc = e->base.pc = 7;
        bool value;
        if (!torch_solid(e, world, x, mc_java_i32_sub(y, 1), z, &value))
            return false;
        e->site.pc = e->base.pc = 10;
        e->base_returned = e->predicate_returned = true;
        e->base_value = value;
        *out = value;
        return true;
    }
    if (!flower && !inherited(e, declaring))
        return false;
    if (flower)
        e->base.pc = 6;
    bool value;
    if (!uu_predicate(e, world, x, y, z, query_index, &value))
        return false;
    if (flower && value) {
        e->base.pc = 20;
        e->site = e->base;
        int32_t below;
        if (!block_get(e, world, query_index + 1, x, mc_java_i32_sub(y, 1), z, &below))
            return false;
        e->below_id = below;
        e->below_read = true;
        e->base.pc = 23;
        e->substrate_entered = true;
        mc_world_plant_substrate_error child;
        const mc_registry_status status = mc_world_plant_substrate(block, below, &value, &child);
        e->site = site(child.class_name, child.method, "(I)Z", child.pc);
        e->substrate = e->site;
        if (status != MC_REGISTRY_OK) {
            e->registry = child.registry;
            registry_failed(e);
            return false;
        }
        e->substrate_returned = true;
    }
    e->base.pc = flower ? 34u : 35u;
    e->site = e->base;
    e->base_returned = e->predicate_returned = true;
    e->base_value = value;
    *out = value;
    return true;
}

static bool predicate(mc_world_placement_error *e, const mc_block *block, mc_world_runtime *world,
                      int32_t x, int32_t y, int32_t z, int32_t face, size_t query_index,
                      bool *out) {
    if (block->kind == MC_BLOCK_LEVER)
        return lever_face_predicate(e, world, x, y, z, face, out);
    const mc_world_placement_site wrapper = e->site;
    if (!inherited(e, face_predicate_class(block->kind)))
        return false;
    e->predicate_entered = true;
    e->site.pc = 6; /* UU face wrapper invokes its independently virtual base. */
    e->wrapper = e->site;
    if (e->caller.class_name == NULL)
        e->caller = e->site;
    bool value;
    if (!base_predicate(e, block, world, x, y, z, query_index, &value))
        return false;
    e->site = wrapper;
    e->site.pc = 9;
    *out = value;
    return true;
}

static mc_world_storage_status finish(mc_world_placement_error *e, bool returned, bool value,
                                      bool *out) {
    e->work->active = false;
    if (returned) {
        e->returned = true;
        e->value = value;
        if (out != NULL)
            *out = value;
    }
    return e->status;
}

static mc_world_storage_status bed_occupancy(mc_registry_classes *registry, mc_world_runtime *world,
                                             int32_t x, int32_t y, int32_t z, bool occupied,
                                             mc_world_placement_error *e,
                                             mc_world_bed_occupancy_execution *owned) {
    if (e == NULL)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    if (e->work != NULL)
        return MC_WORLD_STORAGE_BUSY;
    if (!allocate(e, site("ve", "a", "(Lfd;IIIZ)V", 0)))
        return e->status;
    e->work->execution_children = owned != NULL;
    if (!hold_registry(e, registry))
        return finish(e, false, false, NULL);
    return finish(e, bed_set_occupied(e, world, x, y, z, occupied, owned), false, NULL);
}

mc_world_storage_status mc_world_bed_set_occupied(mc_registry_classes *registry,
                                                  mc_world_runtime *world, int32_t x, int32_t y,
                                                  int32_t z, bool occupied,
                                                  mc_world_placement_error *e) {
    return bed_occupancy(registry, world, x, y, z, occupied, e, NULL);
}

bool mc_world_bed_set_occupied_in_execution(mc_registry_classes *registry, mc_world_runtime *world,
                                            int32_t x, int32_t y, int32_t z, bool occupied,
                                            mc_world_placement_error *e,
                                            mc_world_bed_occupancy_execution *owned,
                                            mc_world_storage_status *raw) {
    if (e == NULL || raw == NULL || owned == NULL || owned->context == NULL ||
        owned->scalar == NULL || owned->mutation == NULL || *owned->mutation != NULL)
        return false;
    *raw = bed_occupancy(registry, world, x, y, z, occupied, e, owned);
    return *raw == MC_WORLD_STORAGE_OK && e->returned;
}

mc_world_storage_status mc_world_runtime_can_place(mc_world_runtime *world, int32_t id, int32_t x,
                                                   int32_t y, int32_t z, bool ignore, int32_t face,
                                                   bool *out, mc_world_placement_error *e) {
    if (e == NULL || out == NULL)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    if (e->work != NULL)
        return MC_WORLD_STORAGE_BUSY;
    if (!allocate(e, site("fd", "a", "(IIIIZI)Z", 5)))
        return e->status;
    int32_t existing_id;
    if (!block_get(e, world, 0, x, y, z, &existing_id))
        return finish(e, false, false, out);
    e->existing_id = existing_id;
    e->existing_read = true;
    e->site.pc = 10;
    if (!hold_registry(e, mc_world_runtime_registry(world)))
        return finish(e, false, false, out);
    mc_registry_block_m_view array;
    if (mc_registry_block_m_get(e->work->registry, &array, &e->registry) != MC_REGISTRY_OK) {
        registry_failed(e);
        return finish(e, false, false, out);
    }
    e->site.pc = 15;
    if (mc_registry_block_m_at(&array, existing_id, &e->existing, &e->registry) != MC_REGISTRY_OK) {
        registry_failed(e);
        return finish(e, false, false, out);
    }
    e->site.pc = 18;
    if (mc_registry_block_m_get(e->work->registry, &array, &e->registry) != MC_REGISTRY_OK) {
        registry_failed(e);
        return finish(e, false, false, out);
    }
    e->site.pc = 22;
    if (mc_registry_block_m_at(&array, id, &e->candidate, &e->registry) != MC_REGISTRY_OK) {
        registry_failed(e);
        return finish(e, false, false, out);
    }
    e->candidate_read = true;
    e->site.pc = 32;
    e->work->collision = calloc(1, sizeof(*e->work->collision));
    if (e->work->collision == NULL) {
        fail(e, MC_WORLD_STORAGE_ALLOCATION_FAILED, "Placement collision getter diagnostic", true);
        return finish(e, false, false, out);
    }
    e->collision = e->work->collision;
    mc_world_collision_error_init(e->work->collision);
    if (e->candidate == NULL) {
        null_reference(e, "Placement actual candidate collision getter null receiver");
        return finish(e, false, false, out);
    }
    mc_world_storage_error admission = {0};
    if (mc_world_runtime_call_enter(world, &admission) != MC_WORLD_STORAGE_OK) {
        fail(e, admission.status, admission.operation, true);
        return finish(e, false, false, out);
    }
    e->work->box_owner = mc_world_runtime_entities(world);
    const mc_world_storage_status box_status = mc_world_collision_block_box_held(
        world, e->candidate, x, y, z, &e->work->box, e->work->collision);
    mc_world_runtime_call_leave(world);
    e->box = e->work->box;
    e->bounds_status = e->collision->bounds.status;
    if (box_status != MC_WORLD_STORAGE_OK) {
        fail(e, box_status, e->collision->nearby.query.operation, true);
        e->cause = e->collision->nearby.query.cause;
    }
    const bool captured = capture(e);
    if (!captured && box_status == MC_WORLD_STORAGE_OK)
        fail(e, e->cleanup.status, e->cleanup.operation, true);
    if (captured && box_status != MC_WORLD_STORAGE_OK)
        classify_receipts(e);
    if (box_status != MC_WORLD_STORAGE_OK || !captured)
        return finish(e, false, false, out);
    e->box_returned = true;
    e->box_ignored = ignore;
    if (e->work->box != NULL && !ignore) {
        e->site.pc = 53;
        e->work->entities = calloc(1, sizeof(*e->work->entities));
        if (e->work->entities == NULL) {
            fail(e, MC_WORLD_STORAGE_ALLOCATION_FAILED, "Placement Entity collision diagnostic",
                 true);
            return finish(e, false, false, out);
        }
        e->entity_collision = e->work->entities;
        bool clear;
        const mc_world_storage_status status =
            mc_world_runtime_no_entity_collision(world, e->work->box, &clear, e->work->entities);
        if (status != MC_WORLD_STORAGE_OK) {
            fail(e, status, e->entity_collision->operation, e->entity_collision->native_policy);
            e->source_exception = e->entity_collision->source_exception;
            e->exception_class = e->entity_collision->exception_class;
        }
        const bool held = capture(e);
        if (!held && status == MC_WORLD_STORAGE_OK)
            fail(e, e->cleanup.status, e->cleanup.operation, true);
        if (held && status != MC_WORLD_STORAGE_OK)
            classify_receipts(e);
        if (status != MC_WORLD_STORAGE_OK || !held)
            return finish(e, false, false, out);
        e->collision_checked = true;
        if (!clear) {
            e->site.pc = 60;
            return finish(e, true, false, out);
        }
    }
    static const char *const statics[] = {"B", "C", "D", "E", "as", "aT"};
    static const unsigned int pcs[] = {63, 71, 79, 87, 95, 103};
    const mc_block *existing = e->existing;
    for (size_t i = 0; i < sizeof statics / sizeof *statics; ++i) {
        e->site.pc = pcs[i];
        const mc_block *special = NULL;
        if (mc_registry_block_static_get(e->work->registry, statics[i], &special, &e->registry) !=
            MC_REGISTRY_OK) {
            registry_failed(e);
            return finish(e, false, false, out);
        }
        if (existing == special) {
            existing = NULL;
            e->existing_replaced = true;
            break;
        }
    }
    if (id > 0 && existing == NULL) {
        e->site.pc = 130;
        const mc_world_placement_site caller = e->site;
        e->caller = caller;
        e->site = site("uu", "a", "(Lfd;IIII)Z", 0);
        bool allowed;
        if (!predicate(e, e->candidate, world, x, y, z, face, 1, &allowed))
            return finish(e, false, false, out);
        e->caller = caller;
        e->site = site("fd", "a", "(IIIIZI)Z", allowed ? 137u : 139u);
        return finish(e, true, allowed, out);
    }
    e->site.pc = 139;
    return finish(e, true, false, out);
}

static bool block_entry(mc_world_placement_error *e, mc_registry_classes *registry,
                        const mc_block *block, const char *method, const char *descriptor) {
    if (!allocate(e, site("uu", method, descriptor, 0)))
        return false;
    if (!receiver(e, registry, block)) {
        e->work->active = false;
        return false;
    }
    return true;
}

mc_world_storage_status mc_world_block_can_place(mc_registry_classes *registry,
                                                 const mc_block *block, mc_world_runtime *world,
                                                 int32_t x, int32_t y, int32_t z, int32_t face,
                                                 bool *out, mc_world_placement_error *e) {
    if (e == NULL || out == NULL)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    if (e->work != NULL)
        return MC_WORLD_STORAGE_BUSY;
    if (!block_entry(e, registry, block, "a", "(Lfd;IIII)Z"))
        return e->status;
    bool value;
    const bool returned = predicate(e, block, world, x, y, z, face, 0, &value);
    return finish(e, returned, returned && value, out);
}

mc_world_storage_status mc_world_block_can_place_base(mc_registry_classes *registry,
                                                      const mc_block *block,
                                                      mc_world_runtime *world, int32_t x, int32_t y,
                                                      int32_t z, bool *out,
                                                      mc_world_placement_error *e) {
    if (e == NULL || out == NULL)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    if (e->work != NULL)
        return MC_WORLD_STORAGE_BUSY;
    if (!block_entry(e, registry, block, "a", "(Lfd;III)Z"))
        return e->status;
    bool value;
    const bool returned = base_predicate(e, block, world, x, y, z, 0, &value);
    return finish(e, returned, returned && value, out);
}

static bool ore_activate(mc_world_placement_error *e, const mc_block *block,
                         mc_world_runtime *world, int32_t x, int32_t y, int32_t z) {
    e->site.class_name = "bs";
    e->site.pc = 6;
    e->wrapper = e->site;
    /* The actual first World dereference belongs to BS.i:1. A nonnull World
     * is retained here, while NULL reaches that exact child operation. */
    if (world != NULL && !hold_world(e, world))
        return false;
    mc_world_step_error *child = calloc(1, sizeof(*child));
    if (child == NULL) {
        fail(e, MC_WORLD_STORAGE_ALLOCATION_FAILED, "Ore activation child diagnostic", true);
        return false;
    }
    e->work->step = child;
    e->step = child;
    mc_world_step_error_init(child);
    child->block = block;
    child->block_class = "bs";
    child->bytecode = 6;
    const bool returned =
        mc_world_step_redstone_activate(world, block, x, y, z, child, &child->source);
    if (!returned) {
        fail(e, child->status, child->operation, true);
        e->registry = child->registry;
        e->caller = site("bs", "h", "(Lfd;III)V", child->activation_bytecode);
        switch (child->source.kind) {
            case MC_WORLD_STEP_SOURCE_NULL_WORLD:
            case MC_WORLD_STEP_SOURCE_NULL_RANDOM:
            case MC_WORLD_STEP_SOURCE_NULL_STATIC:
                null_reference(e, child->operation);
                break;
            case MC_WORLD_STEP_SOURCE_REGISTRY:
                (void)registry_failed_in(e, block->classes);
                break;
            case MC_WORLD_STEP_SOURCE_QUERY:
                e->registry = child->block_read.nearby.query.registry;
                if (child->block_read.nearby.query.source_null)
                    null_reference(e, child->operation);
                else if (e->registry.status != MC_REGISTRY_OK)
                    (void)registry_failed_in(e, block->classes);
                e->cause = child->block_read.nearby.query.cause;
                break;
            case MC_WORLD_STEP_SOURCE_PRESENTATION:
            case MC_WORLD_STEP_SOURCE_MUTATION:
                e->mutation = &child->mutation;
                e->cause = child->mutation.cause;
                break;
            case MC_WORLD_STEP_SOURCE_NONE:
                if (child->cause.status != MC_WORLD_STORAGE_OK)
                    e->cause = child->cause;
                break;
        }
    }
    if (!capture(e)) {
        if (returned)
            fail(e, e->cleanup.status, e->cleanup.operation, true);
        return false;
    }
    if (!returned)
        classify_receipts(e);
    return returned;
}

mc_world_storage_status mc_world_block_clicked(mc_registry_classes *registry, const mc_block *block,
                                               mc_world_runtime *world, int32_t x, int32_t y,
                                               int32_t z, mc_entity *player,
                                               mc_world_placement_error *e) {
    (void)player;
    if (e == NULL)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    if (e->work != NULL)
        return MC_WORLD_STORAGE_BUSY;
    if (!block_entry(e, registry, block, "b", "(Lfd;IIILgs;)V"))
        return e->status;
    if (block->kind == MC_BLOCK_REDSTONE_ORE && registry->options.side == MC_REGISTRY_CLIENT) {
        const bool returned = ore_activate(e, block, world, x, y, z);
        if (returned) {
            e->caller = site("bs", "b", "(Lfd;IIILgs;)V", 17);
            e->site.pc = 20;
        }
        return finish(e, returned, false, NULL);
    }
    const char *declaring = "uu";
    switch (block->kind) {
        case MC_BLOCK_TNT:
            declaring = "ah";
            break;
        case MC_BLOCK_REDSTONE_ORE:
            declaring = "bs";
            break;
        case MC_BLOCK_DOOR:
            declaring = "le";
            break;
        case MC_BLOCK_BUTTON:
            declaring = "oi";
            break;
        case MC_BLOCK_TRAPDOOR:
            declaring = "oq";
            break;
        case MC_BLOCK_NOTE_BLOCK:
            declaring = "pt";
            break;
        case MC_BLOCK_CAKE:
            declaring = "um";
            break;
        case MC_BLOCK_LEVER:
            declaring = "xr";
            break;
        case MC_BLOCK_STAIRS:
            declaring = "ss";
            break;
        default:
            break;
    }
    return finish(e, inherited(e, declaring), false, NULL);
}

mc_world_storage_status mc_world_block_activate_with_context(
    mc_registry_classes *registry, const mc_block *block, mc_world_runtime *world, int32_t x,
    int32_t y, int32_t z, mc_entity *player, const mc_world_block_activation_context *context,
    bool *out, mc_world_placement_error *e) {
    if (e == NULL || out == NULL)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    if (e->work != NULL)
        return MC_WORLD_STORAGE_BUSY;
    if (!block_entry(e, registry, block, "a", "(Lfd;IIILgs;)Z"))
        return e->status;
    if (block->kind == MC_BLOCK_REDSTONE_ORE && registry->options.side == MC_REGISTRY_CLIENT) {
        const bool returned = ore_activate(e, block, world, x, y, z);
        if (returned) {
            e->caller = site("bs", "a", "(Lfd;IIILgs;)Z", 17);
            e->site.pc = 20;
        }
        return finish(e, returned, false, out);
    }
    if (block->kind == MC_BLOCK_LEVER) {
        e->work->block_code_classes = context == NULL ? NULL : context->block_code_classes;
        const bool returned = lever_activate(e, block, world, x, y, z);
        return finish(e, returned, returned, out);
    }
    if (block->kind == MC_BLOCK_BED) {
        const bool returned = bed_activate(e, block, world, x, y, z, player, context);
        return finish(e, returned, returned, out);
    }
    if (block->kind == MC_BLOCK_CHEST) {
        const bool returned = chest_activate(e, block, world, x, y, z, player, context);
        return finish(e, returned, returned, out);
    }
    if (block->kind == MC_BLOCK_FURNACE) {
        const bool returned = furnace_activate(e, world, x, y, z, player, context);
        return finish(e, returned, returned, out);
    }
    if (block->kind == MC_BLOCK_DISPENSER && registry->options.side == MC_REGISTRY_CLIENT) {
        const bool returned = dispenser_activate(e, world, x, y, z, player, context);
        return finish(e, returned, returned, out);
    }
    if (block->kind == MC_BLOCK_CRAFTING_TABLE) {
        e->site = site("fi", "a", "(Lfd;IIILgs;)Z", 1);
        if (!hold_world(e, world))
            return finish(e, false, false, out);
        e->remote_read = true;
        e->remote = world->fields.B;
        if (e->remote) {
            e->site.pc = 8;
            return finish(e, true, true, out);
        }
        e->site.pc = 15;
        if (player == NULL) {
            null_reference(e, "FI actual Player Workbench invocation null receiver");
            return finish(e, false, false, out);
        }
        if (context == NULL || context->workbench == NULL) {
            fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "FI reached Player Workbench display child",
                 true);
            return finish(e, false, false, out);
        }
        e->caller = e->site;
        e->workbench_entered = true;
        const mc_world_storage_status child =
            context->workbench(context->context, player, x, y, z, e);
        if (child != MC_WORLD_STORAGE_OK) {
            e->status = child;
            return finish(e, false, false, out);
        }
        e->workbench_returned = true;
        e->site = site("fi", "a", "(Lfd;IIILgs;)Z", 19);
        return finish(e, true, true, out);
    }
    const bool returned = inherited(e, activation_class(block->kind));
    if (returned)
        e->site.pc = 1;
    return finish(e, returned, false, out);
}

mc_world_storage_status mc_world_block_activate(mc_registry_classes *registry,
                                                const mc_block *block, mc_world_runtime *world,
                                                int32_t x, int32_t y, int32_t z, mc_entity *player,
                                                bool *out, mc_world_placement_error *e) {
    return mc_world_block_activate_with_context(registry, block, world, x, y, z, player, NULL, out,
                                                e);
}

mc_world_storage_status mc_world_block_placed_face(mc_registry_classes *registry,
                                                   const mc_block *block, mc_world_runtime *world,
                                                   int32_t x, int32_t y, int32_t z, int32_t face,
                                                   mc_world_placement_error *e) {
    if (e == NULL)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    if (e->work != NULL)
        return MC_WORLD_STORAGE_BUSY;
    if (!block_entry(e, registry, block, "e", "(Lfd;IIII)V"))
        return e->status;
    if (block->kind == MC_BLOCK_LEVER)
        return finish(e, lever_placed_face(e, block, world, x, y, z, face), false, NULL);
    if (strcmp(placed_face_class(block->kind), "vm") == 0) {
        e->site = site("vm", "e", "(Lfd;IIII)V", 5);
        int32_t metadata;
        if (!value_get(e, world, 0, x, y, z, true, &metadata))
            return finish(e, false, false, NULL);
        bool supported = false;
        if (face == 1) {
            e->caller = e->site;
            e->caller.pc = 24;
            if (!torch_floor(e, world, x, mc_java_i32_sub(y, 1), z, 1, &supported))
                return finish(e, false, false, NULL);
            if (supported)
                metadata = 5;
        } else if (face >= 2 && face <= 5) {
            const unsigned int pcs[] = {46, 68, 90, 112};
            const int32_t xs[] = {x, x, mc_java_i32_add(x, 1), mc_java_i32_sub(x, 1)};
            const int32_t zs[] = {mc_java_i32_add(z, 1), mc_java_i32_sub(z, 1), z, z};
            const size_t index = (size_t)(face - 2);
            e->site.pc = pcs[index];
            if (!torch_solid(e, world, xs[index], y, zs[index], &supported))
                return finish(e, false, false, NULL);
            if (supported)
                metadata = 6 - face;
        }
        e->site = site("vm", "e", "(Lfd;IIII)V", 128);
        e->work->mutation = malloc(sizeof(*e->work->mutation));
        if (e->work->mutation == NULL) {
            fail(e, MC_WORLD_STORAGE_ALLOCATION_FAILED, "Torch notifying metadata diagnostic",
                 true);
            return finish(e, false, false, NULL);
        }
        e->mutation = e->work->mutation;
        const mc_world_storage_status status =
            mc_world_runtime_metadata_set(world, x, y, z, metadata, e->work->mutation);
        if (status != MC_WORLD_STORAGE_OK) {
            fail(e, status, e->mutation->operation, true);
            e->cause = e->mutation->cause;
            e->registry = e->mutation->registry;
            if (e->registry.status != MC_REGISTRY_OK)
                registry_failed_in(e, mc_world_runtime_registry(world));
        }
        if (!capture(e)) {
            if (status == MC_WORLD_STORAGE_OK)
                fail(e, e->cleanup.status, e->cleanup.operation, true);
            return finish(e, false, false, NULL);
        }
        if (status != MC_WORLD_STORAGE_OK) {
            classify_receipts(e);
            return finish(e, false, false, NULL);
        }
        e->site.pc = 131;
        return finish(e, true, false, NULL);
    }
    return finish(e, inherited(e, placed_face_class(block->kind)), false, NULL);
}

mc_world_storage_status mc_world_block_placed_by_with_math(
    mc_registry_classes *registry, const mc_block *block, mc_world_runtime *world, int32_t x,
    int32_t y, int32_t z, mc_entity *living, mc_math_classes *math, mc_world_placement_error *e) {
    if (e == NULL)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    if (e->work != NULL)
        return MC_WORLD_STORAGE_BUSY;
    if (!block_entry(e, registry, block, "a", "(Lfd;IIILls;)V"))
        return e->status;
    if (block->kind == MC_BLOCK_FURNACE)
        return finish(e, furnace_placed_by(e, world, x, y, z, living, math), false, NULL);
    if (block->kind == MC_BLOCK_DISPENSER)
        return finish(e, dispenser_placed_by(e, world, x, y, z, living, math), false, NULL);
    return finish(e, inherited(e, placed_by_class(block->kind)), false, NULL);
}

mc_world_storage_status mc_world_block_placed_by(mc_registry_classes *registry,
                                                 const mc_block *block, mc_world_runtime *world,
                                                 int32_t x, int32_t y, int32_t z, mc_entity *living,
                                                 mc_world_placement_error *e) {
    return mc_world_block_placed_by_with_math(registry, block, world, x, y, z, living, NULL, e);
}

bool mc_world_placement_error_retire(mc_world_placement_error *e) {
    if (e == NULL)
        return false;
    mc_world_placement_work *w = e->work;
    if (w == NULL) {
        memset(e, 0, sizeof(*e));
        return true;
    }
    if (w->active) {
        e->cleanup = (mc_world_storage_error){MC_WORLD_STORAGE_BUSY,
                                              "Placement active native retirement", 0};
        return false;
    }
    /* Owned Bed children borrow the execution's retained diagnostics. Their
     * caller drains that canonical work before retiring this placement frame.
     * Legacy capture would add a new producer hold during native cleanup. */
    if (!w->execution_children && !capture(e))
        return false;
    if (w->box != NULL) {
        e->cleanup_bounds = mc_entity_bounds_release(w->box_owner, w->box);
        if (e->cleanup_bounds != MC_ENTITY_OK) {
            e->cleanup = (mc_world_storage_error){MC_WORLD_STORAGE_BUSY,
                                                  "Placement canonical pooled box retirement", 0};
            return false;
        }
        w->box = NULL;
        e->box = NULL;
    }
    if (w->bed_enum_owner != NULL) {
        e->cleanup_enum_classes = mc_entity_classes_release(w->bed_enum_owner);
        if (e->cleanup_enum_classes != MC_ENTITY_OK) {
            e->cleanup = (mc_world_storage_error){MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                                  "Bed caller CW namespace retirement", 0};
            return false;
        }
        w->bed_enum_owner = NULL;
    }
    if (w->sleep_call != NULL) {
        const mc_player_sleep_status status =
            mc_player_sleep_retire(&w->sleep_call, &w->sleep_cleanup);
        if (status != MC_PLAYER_SLEEP_OK) {
            e->cleanup_sleep = &w->sleep_cleanup;
            e->cleanup = (mc_world_storage_error){status == MC_PLAYER_SLEEP_BUSY
                                                      ? MC_WORLD_STORAGE_BUSY
                                                      : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                                  w->sleep_cleanup.operation, 0};
            return false;
        }
        e->cleanup_sleep = NULL;
        /* A later independent caller-Registry retirement may still refuse.
         * Keep the copied first scalar failure, but end views into the child
         * whose storage has now been consumed. */
        if (w->sleep != NULL) {
            w->sleep->acquisition = NULL;
            w->sleep->occupancy = NULL;
            w->sleep->bed = NULL;
            w->sleep->world_sleep = NULL;
            w->sleep->position = NULL;
        }
    }
    if (w->registry != NULL) {
        const mc_registry_status status =
            mc_registry_classes_release(w->registry, &e->cleanup_registry);
        if (status != MC_REGISTRY_OK) {
            e->cleanup = (mc_world_storage_error){mc_world_runtime_registry_status(status),
                                                  "Placement code Registry retirement", 0};
            return false;
        }
        w->registry = NULL;
        e->existing = e->candidate = NULL;
    }
    if (w->math != NULL &&
        mc_math_classes_release(&w->math, &e->cleanup_math) != MC_MATH_CLASS_OK) {
        e->cleanup = (mc_world_storage_error){MC_WORLD_STORAGE_BUSY, e->cleanup_math.operation, 0};
        return false;
    }
    if (w->dispenser != NULL) {
        dispenser_activation_work *f = w->dispenser;
        e->cleanup_dispenser = &f->cleanup;
        const mc_inventory_status status =
            mc_inventory_dispenser_retire(&f->inventory, &f->cleanup);
        if (status != MC_INVENTORY_OK) {
            e->cleanup = (mc_world_storage_error){
                status == MC_INVENTORY_LIMIT ? MC_WORLD_STORAGE_LIMIT : MC_WORLD_STORAGE_BUSY,
                f->cleanup.operation, 0};
            return false;
        }
        e->dispenser_inventory = NULL;
        if (mc_tile_entity_classes_release(&f->classes) != MC_TILE_ENTITY_OK) {
            e->cleanup = (mc_world_storage_error){MC_WORLD_STORAGE_BUSY,
                                                  "XQ target Class native retirement", 0};
            return false;
        }
    }
    for (size_t i = 0; i < w->receipt_count; ++i)
        mc_world_access_failure_release(&w->receipts[i]);
    for (size_t i = 0; i < w->source_count; ++i)
        free(w->sources[i]);
    for (size_t i = 0; i < PLACEMENT_QUERIES; ++i)
        free(w->queries[i]);
    if (w->chest_placement != NULL) {
        for (size_t i = 0; i < w->chest_placement->detail.query_count; ++i)
            free(w->chest_placement->queries[i]);
        free(w->chest_placement);
    }
    for (size_t i = 0; i < e->support_query_count; ++i)
        free(w->supports[i]);
    for (size_t i = w->chest_view_count; i != 0; --i)
        mc_inventory_chest_release(w->chest_views[i - 1]);
    for (size_t i = 0; i < e->chest_tile_count; ++i) {
        mc_tile_entity_release(w->chest_receivers[i]);
        free(w->chest_tiles[i]);
    }
    if (w->furnace != NULL) {
        mc_inventory_furnace_release(w->furnace->inventory);
        mc_tile_entity_release(w->furnace->tile);
        free(w->furnace);
    }
    if (w->dispenser != NULL) {
        mc_entity_release(w->dispenser->player);
        mc_tile_entity_release(w->dispenser->tile);
        free(w->dispenser);
    }
    free(w->mutation);
    free(w->step);
    free(w->collision);
    free(w->entities);
    free(w->sleep);
    free(w->bed_occupant);
    free(w->sleep_result);
    mc_entity_release(w->bed_player);
    mc_entity_release(w->placed_by_living);
    if (w->world != NULL)
        mc_world_runtime_binding_release(w->world);
    free(w);
    memset(e, 0, sizeof(*e));
    return true;
}
