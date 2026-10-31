#include "boxes_internal.h"

#include "../../../block/registry/runtime_internal.h"
#include "../../../entity/bounds/bounds_internal.h"
#include "../../../entity/entity_internal.h"
#include "../chunk_provider_internal.h"
#include "../construction_internal.h"
#include "blocks_internal.h"
#include "mc/core/java/java.h"
#include "mc/core/math/math.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#if !defined(__SSE2__) && !defined(_M_X64) && !(defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#error "World collision arithmetic requires SSE2"
#endif
#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) ||       \
    (defined(_M_FP_FAST) && _M_FP_FAST)
#error "World collision arithmetic requires strict floating transformations"
#endif
#include <emmintrin.h>

void mc_world_collision_error_init(mc_world_collision_error *error) {
    static const mc_world_collision_error zero = {0};
    if (error == NULL)
        return;
    *error = zero;
    mc_world_nearby_error_init(&error->nearby);
}

mc_world_collision_error mc_world_collision_empty_error(void) {
    mc_world_collision_error error;
    mc_world_collision_error_init(&error);
    return error;
}

static mc_world_storage_status fail(mc_world_collision_error *frame, mc_world_storage_status status,
                                    const char *operation) {
    frame->nearby.query.status = status;
    frame->nearby.query.operation = operation;
    frame->nearby.query.cause = (mc_world_storage_error){status, operation, 0};
    return status;
}

static mc_world_storage_status entity_status(mc_entity_status status) {
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

/* Append/read admit the descriptor in constant time. Full occurrence/hold
 * validation belongs to source clear and terminal native retirement. */
static bool owned_shape(const mc_world_runtime *world) {
    const mc_world_collision_list_ledger *ledger = &world->collision_boxes;
    const mc_world_reference_list *list = ledger->list;
    if (list == NULL)
        return ledger->entries == NULL && ledger->count == 0 && ledger->capacity == 0;
    if (list->count != ledger->count || list->capacity != ledger->capacity ||
        list->count > list->capacity || list->capacity > (size_t)INT32_MAX ||
        list->capacity > SIZE_MAX / (sizeof(*list->items) + sizeof(*ledger->entries)) ||
        ((list->capacity == 0) != (list->items == NULL)) ||
        ((ledger->capacity == 0) != (ledger->entries == NULL)) ||
        (list->items != NULL && (void *)list->items == (void *)ledger->entries))
        return false;
    const size_t bytes = list->capacity * (sizeof(*list->items) + sizeof(*ledger->entries));
    if (bytes > world->construction.direct_allocation_bytes)
        return false;
    const mc_world_reference_list *others[] = {world->entity_membership.list,
                                               world->entity_deferral.list,
                                               world->weather_membership.list,
                                               world->nearby_entities.list,
                                               world->tile_entity_membership[0].identity,
                                               world->tile_entity_membership[1].identity};
    const void *backings[] = {
        world->entity_membership.entries,       world->entity_deferral.entries,
        world->weather_membership.entries,      world->nearby_entities.entries,
        world->tile_entity_membership[0].holds, world->tile_entity_membership[1].holds};
    for (size_t i = 0; i < sizeof(others) / sizeof(others[0]); ++i) {
        if (list == others[i])
            return false;
        const void *other_items = others[i] == NULL ? NULL : others[i]->items;
        if ((list->items != NULL &&
             ((void *)list->items == other_items || (void *)list->items == backings[i])) ||
            (ledger->entries != NULL &&
             ((void *)ledger->entries == other_items || (void *)ledger->entries == backings[i])))
            return false;
    }
    return true;
}

static size_t reference_holds(const mc_world_collision_reference *entry) {
    if (entry->bounds == NULL || ((entry->pool == NULL) == (entry->entity == NULL)))
        return 0;
    if (entry->pool != NULL)
        return mc_entity_bounds_native_holds(entry->pool, entry->bounds);
    if (entry->entity->bounds != entry->bounds)
        return 0;
    return entry->entity->holds;
}

/* Linear structural admission for ordinary source clear. The private append
 * path establishes one real hold per occurrence; source void* writes cannot
 * create trusted typed entries. Quiescent retirement additionally audits
 * actual pooled provenance and duplicate hold multiplicity below. */
static bool clear_admit(const mc_world_runtime *world) {
    if (!owned_shape(world))
        return false;
    const mc_world_collision_list_ledger *ledger = &world->collision_boxes;
    for (size_t i = 0; i < ledger->count; ++i) {
        const mc_world_collision_reference *entry = &ledger->entries[i];
        if (entry->bounds == NULL || ledger->list->items[i] != entry->bounds ||
            ((entry->pool == NULL) == (entry->entity == NULL)) ||
            (entry->entity != NULL && entry->entity->bounds != entry->bounds))
            return false;
    }
    return true;
}

bool mc_world_collision_owned_valid(const mc_world_runtime *world) {
    if (world == NULL || !owned_shape(world))
        return false;
    const mc_world_collision_list_ledger *ledger = &world->collision_boxes;
    for (size_t i = 0; i < ledger->count; ++i) {
        const mc_world_collision_reference *entry = &ledger->entries[i];
        if (ledger->list->items[i] != entry->bounds)
            return false;
        const size_t holds = reference_holds(entry);
        if (holds == 0)
            return false;
        /* Each duplicate owns its own hold. This terminal/clear check is
         * intentionally separate from the constant-time append path. */
        size_t occurrences = 0;
        for (size_t j = 0; j <= i; ++j)
            if (ledger->entries[j].bounds == entry->bounds &&
                ledger->entries[j].pool == entry->pool &&
                ledger->entries[j].entity == entry->entity)
                ++occurrences;
        if (occurrences > holds)
            return false;
    }
    return true;
}

static void clear_owned(mc_world_runtime *world) {
    mc_world_collision_list_ledger *ledger = &world->collision_boxes;
    const size_t count = ledger->count;
    for (size_t i = 0; i < count; ++i) {
        const mc_world_collision_reference entry = ledger->entries[i];
        ledger->entries[i] = (mc_world_collision_reference){0};
        ledger->list->items[i] = NULL;
        if (entry.pool != NULL)
            (void)mc_entity_bounds_release(entry.pool, entry.bounds);
        else
            mc_entity_release(entry.entity);
    }
    ledger->count = 0;
    if (ledger->list != NULL)
        ledger->list->count = 0;
}

void mc_world_collision_owned_release(mc_world_runtime *world) {
    mc_world_collision_list_ledger *ledger = &world->collision_boxes;
    clear_owned(world);
    if (ledger->list == NULL)
        return;
    const size_t bytes =
        ledger->capacity * (sizeof(*ledger->list->items) + sizeof(*ledger->entries));
    free(ledger->entries);
    free(ledger->list->items);
    ledger->entries = NULL;
    ledger->capacity = 0;
    ledger->list->items = NULL;
    ledger->list->capacity = 0;
    world->construction.direct_allocation_bytes -= bytes;
}

static mc_world_storage_status append(mc_world_runtime *world, mc_world_reference_list *list,
                                      mc_world_collision_reference entry,
                                      mc_world_collision_error *frame) {
    if (list == NULL || list != world->collision_boxes.list || !owned_shape(world))
        return fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Collision destination List owner");
    mc_world_collision_list_ledger *ledger = &world->collision_boxes;
    if (list->count == (size_t)INT32_MAX)
        return fail(frame, MC_WORLD_STORAGE_LIMIT, "Collision List size");
    void **items = list->items;
    mc_world_collision_reference *entries = ledger->entries;
    size_t capacity = list->capacity;
    size_t bytes = 0;
    if (list->count == capacity) {
        capacity = capacity == 0                       ? 4u
                   : capacity > (size_t)INT32_MAX / 2u ? (size_t)INT32_MAX
                                                       : capacity * 2u;
        if (capacity <= list->count || capacity > SIZE_MAX / (sizeof(*items) + sizeof(*entries)))
            return fail(frame, MC_WORLD_STORAGE_LIMIT, "Collision List capacity");
        bytes = capacity * (sizeof(*items) + sizeof(*entries));
        const size_t used = world->construction.direct_allocation_bytes;
        const size_t limit = world->construction_limits.max_direct_allocation_bytes;
        if (used > limit || bytes > limit - used)
            return fail(frame, MC_WORLD_STORAGE_LIMIT, "Collision List allocation budget");
        items = malloc(capacity * sizeof(*items));
        entries = malloc(capacity * sizeof(*entries));
        if (items == NULL || entries == NULL) {
            free(items);
            free(entries);
            return fail(frame, MC_WORLD_STORAGE_ALLOCATION_FAILED, "Collision List allocation");
        }
        world->construction.direct_allocation_bytes += bytes;
        if (list->count != 0) {
            memcpy(items, list->items, list->count * sizeof(*items));
            memcpy(entries, ledger->entries, list->count * sizeof(*entries));
        }
    }
    const mc_entity_status held = entry.pool != NULL
                                      ? mc_entity_bounds_retain(entry.pool, entry.bounds)
                                      : mc_entity_retain(entry.entity);
    if (held != MC_ENTITY_OK) {
        if (bytes != 0) {
            free(items);
            free(entries);
            world->construction.direct_allocation_bytes -= bytes;
        }
        frame->nearby.entity =
            (mc_entity_error){.status = held, .operation = "Collision occurrence ownership"};
        return fail(frame, entity_status(held), "Collision occurrence ownership");
    }
    items[list->count] = entry.bounds;
    entries[list->count] = entry;
    if (bytes != 0) {
        const size_t old_bytes = list->capacity * (sizeof(*items) + sizeof(*entries));
        free(list->items);
        free(ledger->entries);
        list->items = items;
        list->capacity = capacity;
        ledger->entries = entries;
        ledger->capacity = capacity;
        world->construction.direct_allocation_bytes -= old_bytes;
    }
    ++list->count;
    ++ledger->count;
    return MC_WORLD_STORAGE_OK;
}

mc_world_storage_status mc_world_collision_append_pool_held(mc_world_runtime *world,
                                                            mc_world_reference_list *destination,
                                                            mc_entity_bounds *bounds,
                                                            mc_world_collision_error *error) {
    mc_entity_classes *classes = mc_world_runtime_entities(world);
    if (classes == NULL || bounds == NULL)
        return fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Collision pooled bounds owner");
    return append(world, destination,
                  (mc_world_collision_reference){.bounds = bounds, .pool = classes}, error);
}

mc_world_storage_status mc_world_collision_append_entity_held(mc_world_runtime *world,
                                                              mc_world_reference_list *destination,
                                                              mc_entity *entity,
                                                              mc_entity_bounds *bounds,
                                                              mc_world_collision_error *error) {
    if (entity == NULL || bounds == NULL || entity->bounds != bounds)
        return fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Collision permanent bounds owner");
    return append(world, destination,
                  (mc_world_collision_reference){.bounds = bounds, .entity = entity}, error);
}

static bool registry_result(mc_world_runtime *world, mc_world_collision_error *frame,
                            mc_registry_status status, const mc_registry_error *child) {
    if (status == MC_REGISTRY_OK)
        return true;
    frame->nearby.query.registry = *child;
    frame->nearby.query.status = mc_world_runtime_registry_result(
        world, status, child, "Collision Block array", &frame->nearby.query.cause);
    frame->nearby.query.operation = frame->nearby.query.cause.operation;
    return false;
}

/* These concrete classes genuinely inherit both literal-null base bodies.
 * Minecart has different concrete virtuals and is not admitted here merely
 * because its constructors are available. */
static bool base_null_virtual(mc_entity *entity, mc_world_collision_error *frame) {
    if (entity == NULL) {
        fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Collision Entity receiver");
        return false;
    }
    if (!mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_ITEM) &&
        !mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_FALLING_SAND) &&
        !mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER) &&
        !mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_SPIDER) &&
        !mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_ZOMBIE) &&
        !mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_SKELETON) &&
        !mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_CREEPER) &&
        !mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_SLIME) &&
        !mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_PIG) &&
        !mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_SHEEP) &&
        !mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_COW) &&
        !mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_CHICKEN) &&
        !mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_WOLF) &&
        !mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_SQUID)) {
        fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Collision Entity concrete virtual");
        return false;
    }
    return true;
}

/* Actual candidate f() has a separate dispatch from moving a(candidate).
 * Canonical yl.f() is literally aconst_null/areturn. Its a(sn), in contrast,
 * returns the argument permanent bounds and remains an explicit dependency. */
static bool candidate_null_virtual(mc_entity *entity, mc_world_collision_error *frame) {
    if (entity != NULL && mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_MINECART))
        return true;
    return base_null_virtual(entity, frame);
}

static bool query(mc_world_runtime *world, mc_entity *moving, const mc_entity_bounds *bounds,
                  mc_world_collision_list_view *value, mc_world_collision_error *frame) {
    frame->world_bytecode = 4;
    if (world->fields.K == NULL || world->fields.K != world->collision_boxes.list ||
        !clear_admit(world)) {
        fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Collision clear List owner");
        return false;
    }
    clear_owned(world);
    frame->world_bytecode = 8;
    if (bounds == NULL) {
        fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Collision query bounds");
        return false;
    }
    frame->world_bytecode = 11;
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u) {
        fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Collision coordinate FP admission");
        return false;
    }
    const int32_t low_x = mc_math_floor_double(bounds->min_x);
    const int32_t high_x = mc_math_floor_double(bounds->max_x + 1.0);
    const int32_t low_y = mc_math_floor_double(bounds->min_y);
    const int32_t high_y = mc_math_floor_double(bounds->max_y + 1.0);
    const int32_t low_z = mc_math_floor_double(bounds->min_z);
    const int32_t high_z = mc_math_floor_double(bounds->max_z + 1.0);
    for (int32_t x = low_x; x < high_x; x = mc_java_i32_add(x, 1)) {
        for (int32_t z = low_z; z < high_z; z = mc_java_i32_add(z, 1)) {
            frame->world_bytecode = 94;
            frame->x = x;
            frame->y = 64;
            frame->z = z;
            mc_world_nearby_error_init(&frame->nearby);
            frame->nearby.query.bytecode = 20; /* column's private presence call */
            frame->nearby.query.provider_bytecode = 6;
            bool present;
            mc_world_storage_error child = {0};
            if (mc_world_runtime_chunk_provider_contains(world, mc_java_i32_shr(x, 4),
                                                         mc_java_i32_shr(z, 4), &present,
                                                         &child) != MC_WORLD_STORAGE_OK) {
                frame->nearby.query.status = child.status;
                frame->nearby.query.operation = child.operation;
                frame->nearby.query.cause = child;
                return false;
            }
            mc_world_nearby_error_init(&frame->nearby);
            if (!present)
                continue;
            for (int32_t y = mc_java_i32_sub(low_y, 1); y < high_y; y = mc_java_i32_add(y, 1)) {
                frame->y = y;
                frame->world_bytecode = 113;
                mc_registry_block_m_view array;
                mc_registry_error registry = frame->nearby.query.registry;
                if (!registry_result(world, frame,
                                     mc_registry_block_m_get(mc_world_runtime_registry(world),
                                                             &array, &registry),
                                     &registry))
                    return false;
                frame->world_bytecode = 123;
                int32_t id;
                if (mc_world_collision_block_value_held(world, x, y, z, false, &id, frame) !=
                    MC_WORLD_STORAGE_OK)
                    return false;
                frame->world_bytecode = 126;
                const mc_block *block;
                const mc_registry_status selected =
                    mc_registry_block_m_at(&array, id, &block, &registry);
                if (!registry_result(world, frame, selected, &registry))
                    return false;
                if (block == NULL)
                    continue;
                frame->world_bytecode = 148;
                if (mc_world_collision_block_contribute_held(world, block, x, y, z, bounds,
                                                             world->fields.K,
                                                             frame) != MC_WORLD_STORAGE_OK)
                    return false;
                frame->block_class = NULL;
                frame->block_bytecode = 0;
            }
        }
    }
    frame->world_bytecode = 183;
    mc_world_nearby_error_init(&frame->nearby);
    mc_entity_bounds *expanded;
    const mc_entity_status expansion = mc_entity_bounds_expand(
        mc_world_runtime_entities(world), bounds, 0.25, 0.25, 0.25, &expanded, &frame->bounds);
    if (expansion != MC_ENTITY_OK) {
        fail(frame, entity_status(expansion), "Collision nearby expansion");
        return false;
    }
    frame->world_bytecode = 186;
    mc_world_entity_list_view nearby = {0};
    const mc_world_storage_status gathered =
        mc_world_runtime_nearby_entities(world, moving, expanded, &nearby, &frame->nearby);
    (void)mc_entity_bounds_release(mc_world_runtime_entities(world), expanded);
    if (gathered != MC_WORLD_STORAGE_OK)
        return false;
    bool success = true;
    for (size_t i = 0;; ++i) {
        frame->world_bytecode = 198;
        size_t count;
        mc_world_storage_status status = mc_world_entity_list_view_count(&nearby, &count);
        if (status != MC_WORLD_STORAGE_OK) {
            fail(frame, status, "Collision nearby List size");
            success = false;
            break;
        }
        if (i >= count)
            break;
        frame->world_bytecode = 210;
        mc_entity *candidate;
        status = mc_world_entity_list_view_get(&nearby, i, &candidate);
        if (status != MC_WORLD_STORAGE_OK) {
            fail(frame, status, "Collision nearby first get");
            success = false;
            break;
        }
        frame->world_bytecode = 218;
        if (!candidate_null_virtual(candidate, frame)) {
            success = false;
            break;
        }
        /* Actual inherited candidate box returned null. The second get is
         * still a separate reached List operation, before the moving call. */
        frame->world_bytecode = 252;
        status = mc_world_entity_list_view_get(&nearby, i, &candidate);
        if (status != MC_WORLD_STORAGE_OK) {
            fail(frame, status, "Collision nearby second get");
            success = false;
            break;
        }
        frame->world_bytecode = 260;
        if (!base_null_virtual(moving, frame)) {
            success = false;
            break;
        }
    }
    mc_world_entity_list_view_release(&nearby);
    if (!success)
        return false;
    frame->world_bytecode = 299;
    const mc_world_reference_list *result = world->fields.K;
    if (result != NULL && (result != world->collision_boxes.list || !owned_shape(world))) {
        fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Collision returned List owner");
        return false;
    }
    if (world->local_leases == SIZE_MAX) {
        fail(frame, MC_WORLD_STORAGE_LIMIT, "Collision returned List lease");
        return false;
    }
    ++world->local_leases;
    *value = (mc_world_collision_list_view){world, result};
    return true;
}

mc_world_storage_status mc_world_runtime_collision_boxes(mc_world_runtime *world, mc_entity *moving,
                                                         const mc_entity_bounds *bounds,
                                                         mc_world_collision_list_view *out,
                                                         mc_world_collision_error *error) {
    mc_world_collision_error frame;
    mc_world_collision_error_init(&frame);
    if (world == NULL || out == NULL)
        fail(&frame, MC_WORLD_STORAGE_INVALID_ARGUMENT, "Collision arguments");
    else if (world->construction.mode != MC_WORLD_RUNTIME_GAME ||
             world->construction.phase == MC_WORLD_RUNTIME_RETIRED)
        fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Collision World phase");
    else {
        mc_world_storage_error child = {0};
        if (mc_world_runtime_call_enter(world, &child) != MC_WORLD_STORAGE_OK) {
            frame.nearby.query.status = child.status;
            frame.nearby.query.operation = child.operation;
            frame.nearby.query.cause = child;
        } else {
            const mc_entity_status held = moving == NULL ? MC_ENTITY_OK : mc_entity_retain(moving);
            if (held != MC_ENTITY_OK) {
                frame.nearby.entity =
                    (mc_entity_error){.status = held, .operation = "Collision moving Entity hold"};
                fail(&frame, entity_status(held), "Collision moving Entity hold");
            } else {
                mc_world_collision_list_view value = {0};
                if (query(world, moving, bounds, &value, &frame)) {
                    *out = value;
                    mc_world_collision_error_init(&frame);
                }
                mc_entity_release(moving);
            }
            if (frame.nearby.query.status != MC_WORLD_STORAGE_OK)
                world->last_error = frame.nearby.query.cause;
            mc_world_runtime_call_leave(world);
        }
    }
    if (error != NULL)
        *error = frame;
    return frame.nearby.query.status;
}

mc_world_storage_status mc_world_collision_list_view_count(const mc_world_collision_list_view *view,
                                                           size_t *out) {
    if (view == NULL || view->owner == NULL || out == NULL)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    if (view->identity == NULL || view->identity != view->owner->collision_boxes.list ||
        !owned_shape(view->owner))
        return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    *out = view->identity->count;
    return MC_WORLD_STORAGE_OK;
}

mc_world_storage_status mc_world_collision_list_view_get(const mc_world_collision_list_view *view,
                                                         size_t index,
                                                         const mc_entity_bounds **out) {
    if (out == NULL)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    size_t count;
    const mc_world_storage_status status = mc_world_collision_list_view_count(view, &count);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    if (index >= count)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    const mc_world_collision_reference *entry = &view->owner->collision_boxes.entries[index];
    if (view->identity->items[index] != entry->bounds || reference_holds(entry) == 0)
        return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    *out = entry->bounds;
    return MC_WORLD_STORAGE_OK;
}

void mc_world_collision_list_view_release(mc_world_collision_list_view *view) {
    if (view == NULL || view->owner == NULL)
        return;
    --view->owner->local_leases;
    *view = (mc_world_collision_list_view){0};
}
