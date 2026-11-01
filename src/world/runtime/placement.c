#include "mc/world/runtime/placement.h"

#include "../../block/registry/registry_internal.h"
#include "../../block/registry/runtime_internal.h"
#include "../../item/render/internal.h"
#include "collision/blocks_internal.h"
#include "construction_internal.h"
#include "mc/world/runtime/blocks.h"
#include "mc/world/runtime/collision/spawning.h"
#include "mc/world/storage/source_chunk_loader.h"
#include "runtime_internal.h"

#include <stdlib.h>
#include <string.h>

enum { PLACEMENT_RECEIPTS = 16, PLACEMENT_SOURCES = 4 };

struct mc_world_placement_work {
    mc_world_runtime *world;
    mc_registry_classes *registry;
    mc_entity_classes *box_owner;
    mc_entity_bounds *box;
    mc_world_runtime_acquisition_error *queries[2];
    mc_world_collision_error *collision;
    mc_world_spawn_collision_error *entities;
    mc_world_source_chunk_error *sources[PLACEMENT_SOURCES];
    const mc_world_source_chunk_error *source_identities[PLACEMENT_SOURCES];
    mc_world_access_failure receipts[PLACEMENT_RECEIPTS];
    size_t source_count, receipt_count;
    bool active;
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

static mc_world_storage_status registry_failed(mc_world_placement_error *e) {
    const mc_registry_status status = e->registry.status;
    const mc_world_storage_status result = mc_world_runtime_registry_status(status);
    mc_item_render_throwable throwable = MC_ITEM_RENDER_NO_THROWABLE;
    mc_item_render_registry_classification(e->work->registry, &e->registry, &throwable,
                                           &e->exception_class);
    e->source_exception = throwable == MC_ITEM_RENDER_SOURCE_EXCEPTION;
    e->source_error = throwable == MC_ITEM_RENDER_SOURCE_ERROR;
    fail(e, result, e->registry.message, throwable == MC_ITEM_RENDER_NO_THROWABLE);
    return result;
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
    if (v->receipt == NULL)
        return true;
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

static bool capture(mc_world_placement_error *e) {
    mc_world_placement_work *w = e->work;
    for (size_t i = 0; i < 2; ++i)
        if (w->queries[i] != NULL &&
            (!copy_source(e, &w->queries[i]->source_chunk) ||
             !publication_receipts(e, &w->queries[i]->batch, &w->queries[i]->tile_entity)))
            return false;
    mc_world_nearby_error *nearby[] = {w->collision == NULL ? NULL : &w->collision->nearby,
                                       w->entities == NULL ? NULL : &w->entities->nearby};
    for (size_t i = 0; i < sizeof nearby / sizeof *nearby; ++i)
        if (nearby[i] != NULL &&
            (!copy_source(e, &nearby[i]->query.source_chunk) ||
             !publication_receipts(e, &nearby[i]->batch, &nearby[i]->tile_entity)))
            return false;
    return true;
}

/* Only exact invocation receipts classify source Exceptions. A native adapter
 * refusal, allocation limit or unavailable body has no such evidence. */
static void classify_receipts(mc_world_placement_error *e) {
    for (size_t i = 0; i < e->work->receipt_count; ++i)
        if (mc_world_access_failure_source_exception(&e->work->receipts[i], &e->exception_class)) {
            e->source_exception = true;
            e->source_error = false;
            e->native_policy = false;
            return;
        }
}

static bool block_get(mc_world_placement_error *e, mc_world_runtime *world, size_t index, int32_t x,
                      int32_t y, int32_t z, int32_t *out) {
    if (!hold_world(e, world))
        return false;
    mc_world_placement_work *w = e->work;
    w->queries[index] = calloc(1, sizeof(*w->queries[index]));
    if (w->queries[index] == NULL) {
        fail(e, MC_WORLD_STORAGE_ALLOCATION_FAILED, "Placement reached World query diagnostic",
             true);
        return false;
    }
    e->acquisition = w->queries[index];
    mc_world_storage_error cause = {0};
    const mc_world_storage_status status =
        mc_world_runtime_block_get_detailed(world, x, y, z, out, &cause, w->queries[index]);
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

static bool predicate(mc_world_placement_error *e, const mc_block *block, mc_world_runtime *world,
                      int32_t x, int32_t y, int32_t z, size_t query_index, bool *out) {
    const mc_world_placement_site wrapper = e->site;
    if (!inherited(e, face_predicate_class(block->kind)))
        return false;
    e->predicate_entered = true;
    e->site.pc = 6; /* UU face wrapper invokes its independently virtual base. */
    e->wrapper = e->site;
    if (e->caller.class_name == NULL)
        e->caller = e->site;
    e->site = site(predicate_class(block->kind), "a", "(Lfd;III)Z", 0);
    if (!inherited(e, e->site.class_name))
        return false;
    int32_t id;
    e->site.pc = 5;
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
    e->site = wrapper;
    e->site.pc = 9;
    e->predicate_returned = true;
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

mc_world_storage_status mc_world_runtime_can_place(mc_world_runtime *world, int32_t id, int32_t x,
                                                   int32_t y, int32_t z, bool ignore, int32_t face,
                                                   bool *out, mc_world_placement_error *e) {
    (void)face; /* UU face wrapper deliberately does not read this source argument. */
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
        if (!predicate(e, e->candidate, world, x, y, z, 1, &allowed))
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
    (void)face;
    if (e == NULL || out == NULL)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    if (e->work != NULL)
        return MC_WORLD_STORAGE_BUSY;
    if (!block_entry(e, registry, block, "a", "(Lfd;IIII)Z"))
        return e->status;
    bool value;
    const bool returned = predicate(e, block, world, x, y, z, 0, &value);
    return finish(e, returned, returned && value, out);
}

mc_world_storage_status mc_world_block_activate(mc_registry_classes *registry,
                                                const mc_block *block, mc_world_runtime *world,
                                                int32_t x, int32_t y, int32_t z, mc_entity *player,
                                                bool *out, mc_world_placement_error *e) {
    (void)world;
    (void)x;
    (void)y;
    (void)z;
    (void)player;
    if (e == NULL || out == NULL)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    if (e->work != NULL)
        return MC_WORLD_STORAGE_BUSY;
    if (!block_entry(e, registry, block, "a", "(Lfd;IIILgs;)Z"))
        return e->status;
    const bool returned = inherited(e, activation_class(block->kind));
    if (returned)
        e->site.pc = 1;
    return finish(e, returned, false, out);
}

mc_world_storage_status mc_world_block_placed_face(mc_registry_classes *registry,
                                                   const mc_block *block, mc_world_runtime *world,
                                                   int32_t x, int32_t y, int32_t z, int32_t face,
                                                   mc_world_placement_error *e) {
    (void)world;
    (void)x;
    (void)y;
    (void)z;
    (void)face;
    if (e == NULL)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    if (e->work != NULL)
        return MC_WORLD_STORAGE_BUSY;
    if (!block_entry(e, registry, block, "e", "(Lfd;IIII)V"))
        return e->status;
    return finish(e, inherited(e, placed_face_class(block->kind)), false, NULL);
}

mc_world_storage_status mc_world_block_placed_by(mc_registry_classes *registry,
                                                 const mc_block *block, mc_world_runtime *world,
                                                 int32_t x, int32_t y, int32_t z, mc_entity *living,
                                                 mc_world_placement_error *e) {
    (void)world;
    (void)x;
    (void)y;
    (void)z;
    (void)living;
    if (e == NULL)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    if (e->work != NULL)
        return MC_WORLD_STORAGE_BUSY;
    if (!block_entry(e, registry, block, "a", "(Lfd;IIILls;)V"))
        return e->status;
    return finish(e, inherited(e, placed_by_class(block->kind)), false, NULL);
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
    if (!capture(e))
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
    for (size_t i = 0; i < w->receipt_count; ++i)
        mc_world_access_failure_release(&w->receipts[i]);
    for (size_t i = 0; i < w->source_count; ++i)
        free(w->sources[i]);
    for (size_t i = 0; i < 2; ++i)
        free(w->queries[i]);
    free(w->collision);
    free(w->entities);
    if (w->world != NULL)
        mc_world_runtime_binding_release(w->world);
    free(w);
    memset(e, 0, sizeof(*e));
    return true;
}
