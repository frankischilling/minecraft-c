#include "bounds/bounds_internal.h"
#include "classes/memory_internal.h"
#include "construction_internal.h"
#include "entity_internal.h"
#include "minecart/classes_internal.h"
#include "minecart/entity_internal.h"

#include <float.h>
#include <limits.h>
#include <stdlib.h>

#if !defined(_M_X64) && !defined(_M_AMD64) && !defined(__x86_64__)
#error "Entity numeric construction requires x86-64/SSE2"
#endif
#if !defined(_MSC_VER) && !defined(__SSE2__)
#error "Entity numeric construction requires SSE2"
#endif
#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) ||       \
    (defined(_M_FP_FAST) && _M_FP_FAST)
#error "Entity numeric construction requires strict floating transformations"
#endif
#include <emmintrin.h>

_Static_assert(CHAR_BIT == 8 && sizeof(float) == sizeof(uint32_t) &&
                   sizeof(double) == sizeof(uint64_t),
               "binary32/binary64 storage required");
_Static_assert(FLT_RADIX == 2 && FLT_MANT_DIG == 24 && FLT_MAX_EXP == 128 && DBL_MANT_DIG == 53 &&
                   DBL_MAX_EXP == 1024 && FLT_EVAL_METHOD == 0,
               "strict binary32/binary64 evaluation required");

static void *allocate(mc_entity_classes *classes, mc_entity *entity, size_t bytes,
                      mc_entity_error *error, const char *operation, uint16_t bytecode) {
    void *storage = mc_entity_class_memory_allocate(classes, bytes, error, operation, bytecode);
    if (storage != NULL && entity != NULL)
        entity->direct_bytes += bytes;
    return storage;
}

static void retire(mc_entity_classes *classes, mc_entity *entity, void *storage, size_t bytes) {
    if (storage != NULL) {
        mc_entity_class_memory_retire(classes, storage, bytes);
        if (entity != NULL)
            entity->direct_bytes -= bytes;
    }
}

void *mc_entity_construction_allocate(mc_entity *entity, size_t bytes, mc_entity_error *error,
                                      const char *operation, uint16_t bytecode) {
    return allocate(entity->classes, entity, bytes, error, operation, bytecode);
}

void mc_entity_construction_retire(mc_entity *entity, void *storage, size_t bytes) {
    retire(entity->classes, entity, storage, bytes);
}

static mc_entity_status ensure_watcher_class(mc_entity_classes *classes, mc_entity_error *error) {
    static const uint16_t put_bytecodes[] = {19, 32, 45, 58, 71, 84, 98};
    if (classes->watcher_phase == MC_ENTITY_CLASS_READY)
        return MC_ENTITY_OK;
    if (classes->watcher_phase == MC_ENTITY_CLASS_FAILED)
        return mc_entity_error_set(error, MC_ENTITY_CLASS_FAILED_STATUS,
                                   "watcher class initialization", 0);
    if (classes->watcher_phase != MC_ENTITY_CLASS_COLD)
        return mc_entity_error_set(error, MC_ENTITY_STATE_ERROR, "watcher class initialization", 0);
    classes->watcher_phase = MC_ENTITY_CLASS_INITIALIZING;
    mc_entity_type_map *map =
        allocate(classes, NULL, sizeof(*map), error, "watcher class initialization", 0);
    if (map == NULL) {
        classes->watcher_phase = MC_ENTITY_CLASS_FAILED;
        return error->status;
    }
    *map = (mc_entity_type_map){0};
    classes->types = map; /* ud/mo.<clinit> PC7, before any of the seven puts. */
    for (int32_t kind = 0; kind < 7; ++kind) {
        mc_entity_type_entry *entry = allocate(classes, NULL, sizeof(*entry), error,
                                               "watcher class initialization", put_bytecodes[kind]);
        if (entry == NULL) {
            classes->watcher_phase = MC_ENTITY_CLASS_FAILED;
            return error->status;
        }
        /* Byte, Short, Integer, Float, String, ItemStack, br/ay class tokens.
         * Native typed lookup does not expose a generic Java Class/box map. */
        *entry = (mc_entity_type_entry){.kind = kind, .value = kind};
        if (map->tail == NULL)
            map->head = entry;
        else
            map->tail->next = entry;
        map->tail = entry;
        ++map->size;
    }
    classes->watcher_phase = MC_ENTITY_CLASS_READY;
    return MC_ENTITY_OK;
}

const char *mc_entity_status_string(mc_entity_status status) {
    switch (status) {
        case MC_ENTITY_OK:
            return "ok";
        case MC_ENTITY_INVALID_ARGUMENT:
            return "invalid argument";
        case MC_ENTITY_INVALID_PHASE:
            return "invalid phase";
        case MC_ENTITY_LIMIT:
            return "resource limit";
        case MC_ENTITY_ALLOCATION_FAILED:
            return "allocation failed";
        case MC_ENTITY_PROFILE_FAILED:
            return "Random profile failed";
        case MC_ENTITY_WORLD_FAILED:
            return "world binding failed";
        case MC_ENTITY_ITEM_FAILED:
            return "ItemStack hold failed";
        case MC_ENTITY_CLASS_FAILED_STATUS:
            return "class initialization failed";
        case MC_ENTITY_BUSY:
            return "owner in use";
        case MC_ENTITY_STATE_ERROR:
            return "native state error";
        case MC_ENTITY_UNSUPPORTED_FP:
            return "unsupported floating environment";
    }
    return "unknown Entity status";
}

bool mc_entity_is_kind(const mc_entity *entity, mc_entity_class_kind kind) {
    return entity != NULL && entity->classes != NULL &&
           (unsigned int)kind < (unsigned int)MC_ENTITY_CLASS_KIND_COUNT &&
           entity->class_literal == &entity->classes->literals[(size_t)kind];
}

mc_entity_status mc_entity_item_pickup_delay_set(mc_entity *entity, int32_t delay) {
    if (!mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_ITEM))
        return MC_ENTITY_INVALID_ARGUMENT;
    entity->item.c = delay;
    return MC_ENTITY_OK;
}

mc_entity_base_fields *mc_entity_runtime_fields(mc_entity *entity) {
    return entity == NULL ? NULL : &entity->base;
}

mc_entity_status mc_entity_allocate_canonical(mc_entity_classes *classes, mc_entity_class_kind kind,
                                              const char *operation, mc_entity **out,
                                              mc_entity_error *error) {
    mc_entity_error local_error;
    if (error == NULL)
        error = &local_error;
    if (classes == NULL || out == NULL)
        return mc_entity_error_set(error, MC_ENTITY_INVALID_ARGUMENT, operation, 0);
    size_t bytes;
    mc_entity_status status;
    switch (kind) {
        case MC_ENTITY_CLASS_KIND_ITEM:
        case MC_ENTITY_CLASS_KIND_FALLING_SAND:
            bytes = sizeof(mc_entity);
            status = mc_entity_base_class_initialize(classes, error);
            break;
        case MC_ENTITY_CLASS_KIND_MINECART:
            bytes = sizeof(mc_minecart_entity_object);
            status = mc_minecart_class_initialize(classes, error);
            break;
        default:
            return mc_entity_error_set(error, MC_ENTITY_INVALID_ARGUMENT, operation, 0);
    }
    if (status != MC_ENTITY_OK)
        return status;
    if (classes->live_entities == SIZE_MAX ||
        classes->live_entities >= classes->limits.max_live_entities)
        return mc_entity_error_set(error, MC_ENTITY_LIMIT, operation, 0);
    void *storage = allocate(classes, NULL, bytes, error, operation, 0);
    if (storage == NULL)
        return error->status;
    mc_entity *entity;
    if (kind == MC_ENTITY_CLASS_KIND_MINECART) {
        mc_minecart_entity_object *object = storage;
        *object = (mc_minecart_entity_object){0};
        entity = &object->entity;
        entity->minecart = &object->state;
    } else {
        entity = storage;
        *entity = (mc_entity){0};
    }
    entity->classes = classes;
    entity->class_literal = &classes->literals[(size_t)kind];
    entity->holds = 1;
    entity->direct_bytes = bytes;
    ++classes->live_entities;
    *out = entity;
    return mc_entity_error_set(error, MC_ENTITY_OK, NULL, 0);
}

mc_entity_status mc_entity_constructor_begin(mc_entity *entity, mc_entity_class_kind kind,
                                             mc_world_runtime *world, const char *operation,
                                             const char *profile_operation,
                                             mc_entity_error *error) {
    if (!mc_entity_is_kind(entity, kind))
        return mc_entity_error_set(error, MC_ENTITY_INVALID_ARGUMENT, operation, 0);
    if (entity->phase != MC_ENTITY_RAW)
        return mc_entity_error_set(error, MC_ENTITY_INVALID_PHASE, operation, 0);
    if (world != NULL && mc_world_runtime_random_profile(world) != entity->classes->profile)
        return mc_entity_error_set(error, MC_ENTITY_INVALID_ARGUMENT, profile_operation, 0);
    entity->phase = MC_ENTITY_CONSTRUCTING;
    return MC_ENTITY_OK;
}

mc_entity_status mc_entity_constructor_finish(mc_entity *entity, mc_entity_status status,
                                              mc_entity_error *error) {
    if (status != MC_ENTITY_OK) {
        entity->phase = MC_ENTITY_FAILED_CONSTRUCTION;
        return status;
    }
    entity->phase = MC_ENTITY_READY;
    return mc_entity_error_set(error, MC_ENTITY_OK, NULL, 0);
}

mc_entity_status mc_item_entity_allocate(mc_entity_classes *classes, mc_entity **out,
                                         mc_entity_error *error) {
    return mc_entity_allocate_canonical(classes, MC_ENTITY_CLASS_KIND_ITEM, "item allocation", out,
                                        error);
}

mc_entity_status mc_falling_entity_allocate(mc_entity_classes *classes, mc_entity **out,
                                            mc_entity_error *error) {
    return mc_entity_allocate_canonical(classes, MC_ENTITY_CLASS_KIND_FALLING_SAND,
                                        "falling allocation", out, error);
}

void mc_entity_source_position(mc_entity *entity, double x, double y, double z) {
    entity->base.x = x;
    entity->base.y = y;
    entity->base.z = z;
    const float half_width = entity->base.width / 2.0f;
    const float height = entity->base.height;
    const double minimum_y_before_size = y - (double)entity->base.offset;
    const double minimum_y = minimum_y_before_size + (double)entity->base.y_size;
    /* Repeat the original maximum-Y association instead of deriving from a
     * reordered height/offset sum. Each intermediate has binary64 precision. */
    const double maximum_y_before_size = y - (double)entity->base.offset;
    const double maximum_y_before_height = maximum_y_before_size + (double)entity->base.y_size;
    const double maximum_y = maximum_y_before_height + (double)height;
    *entity->bounds = (mc_entity_bounds){x - (double)half_width, minimum_y, z - (double)half_width,
                                         x + (double)half_width, maximum_y, z + (double)half_width};
}

static mc_entity_status make_watcher(mc_entity *entity, mc_entity_error *error) {
    mc_entity_classes *classes = entity->classes;
    mc_entity_status status = ensure_watcher_class(classes, error);
    if (status != MC_ENTITY_OK)
        return status;
    mc_entity_watcher *watcher =
        allocate(classes, entity, sizeof(*watcher), error, "entity constructor", 183);
    if (watcher == NULL)
        return error->status;
    *watcher = (mc_entity_watcher){0};
    mc_entity_watch_map *map =
        allocate(classes, entity, sizeof(*map), error, "watcher constructor", 5);
    if (map == NULL) {
        retire(classes, entity, watcher, sizeof(*watcher));
        return error->status;
    }
    *map = (mc_entity_watch_map){0};
    watcher->map = map;
    entity->watcher = watcher;
    return MC_ENTITY_OK;
}

static mc_entity_status register_byte_zero(mc_entity *entity, mc_entity_error *error) {
    const mc_entity_type_entry *type = entity->classes->types->head;
    while (type != NULL && type->kind != 0)
        type = type->next;
    if (type == NULL || type->value != 0 || entity->watcher->map->size != 0)
        return mc_entity_error_set(error, MC_ENTITY_STATE_ERROR, "watcher registration", 7);
    mc_entity_watch_record *record =
        allocate(entity->classes, entity, sizeof(*record), error, "watcher registration", 144);
    if (record == NULL)
        return error->status;
    /* ma/ht constructor store order. This is typed Byte0, not a new box. */
    record->index = 0;
    record->byte_value = 0;
    record->type = type->value;
    record->dirty = true;
    mc_entity_watch_node *node =
        allocate(entity->classes, entity, sizeof(*node), error, "watcher registration", 169);
    if (node == NULL) {
        retire(entity->classes, entity, record, sizeof(*record));
        return error->status;
    }
    *node = (mc_entity_watch_node){.key = 0, .record = record};
    entity->watcher->map->head = node;
    entity->watcher->map->size = 1;
    return MC_ENTITY_OK;
}

mc_entity_status mc_entity_construct_base(mc_entity *entity, mc_world_runtime *world,
                                          mc_entity_error *error) {
    mc_entity_classes *classes = entity->classes;
    const int32_t id = classes->next_id;
    classes->next_id = mc_java_i32_add(id, 1);
    entity->base.id = id;
    entity->base_bytecode = 14;
    entity->base.aE = 1.0;
    entity->base.aF = false;
    entity->base_bytecode = 24;
    mc_entity_status status = mc_entity_bounds_initialize(classes, error);
    if (status != MC_ENTITY_OK)
        return status;
    mc_entity_bounds *bounds =
        allocate(classes, entity, sizeof(*bounds), error, "bounds factory", 0);
    if (bounds == NULL)
        return error->status;
    *bounds = (mc_entity_bounds){0};
    entity->bounds = bounds;
    entity->base_bytecode = 37;
    entity->base.aX = false;
    entity->base.ba = false;
    entity->base.bb = false;
    entity->base.bd = true;
    entity->base.be = false;
    entity->base.offset = 0.0f;
    entity->base.width = 0.6f;
    entity->base.height = 1.8f;
    entity->base.bi = 0.0f;
    entity->base.bj = 0.0f;
    entity->base.bk = 0.0f;
    entity->base.private_b = 1;
    entity->base.y_size = 0.0f;
    entity->base.bp = 0.0f;
    entity->base.bq = false;
    entity->base.br = 0.0f;
    entity->base_bytecode = 119;
    mc_java_random *random =
        allocate(classes, entity, sizeof(*random), error, "entity constructor", 123);
    if (random == NULL)
        return error->status;
    const mc_java_random_profile_status profile_status =
        world == NULL ? mc_java_random_construct_default(classes->profile, random)
                      : mc_world_runtime_construct_default_random(world, random);
    if (profile_status != MC_JAVA_RANDOM_PROFILE_OK) {
        retire(classes, entity, random, sizeof(*random));
        mc_entity_error_set(error, MC_ENTITY_PROFILE_FAILED, "entity constructor", 127);
        error->profile = profile_status;
        return error->status;
    }
    entity->random = random;
    entity->base_bytecode = 130;
    entity->base.bt = 0;
    entity->base.bu = 1;
    entity->base.bv = 0;
    entity->base.bw = 300;
    entity->base.bx = false;
    entity->base.by = 0;
    entity->base.bz = 300;
    entity->base.private_c = true;
    entity->base.bC = false;
    entity->base_bytecode = 179;
    status = make_watcher(entity, error);
    if (status != MC_ENTITY_OK)
        return status;
    entity->base_bytecode = 190;
    entity->base.bE = 0.0f;
    entity->base.bF = false;
    entity->base_bytecode = 200;
    if (world != NULL && !mc_world_runtime_binding_retain(world)) {
        mc_entity_error_set(error, MC_ENTITY_WORLD_FAILED, "entity constructor", 205);
        error->world = mc_world_runtime_last_error(world)->status;
        return error->status;
    }
    entity->base.world = world;
    entity->base_bytecode = 205;
    mc_entity_source_position(entity, 0.0, 0.0, 0.0);
    entity->base_bytecode = 212;
    /* The exposed watcher holds a typed Byte value; no per-entity Byte box. */
    entity->base_bytecode = 221;
    status = register_byte_zero(entity, error);
    if (status != MC_ENTITY_OK)
        return status;
    entity->base_bytecode = 224;
    /* Actual hl/ez, ju/gl and yl/pr b()V hooks are empty returns. */
    entity->base_bytecode = 231;
    return MC_ENTITY_OK;
}

float mc_entity_item_phase_from_draw(double draw) {
    const double radians = draw * 3.141592653589793;
    const double doubled = radians * 2.0;
    return (float)doubled;
}

float mc_entity_item_yaw_from_draw(double draw) {
    const double degrees = draw * 360.0;
    return (float)degrees;
}

double mc_entity_item_velocity_from_draw(double draw) {
    const double scaled = draw * 0.20000000298023224;
    const double centered = scaled - 0.10000000149011612;
    const float narrowed = (float)centered;
    return (double)narrowed;
}

static mc_entity_status math_draw(mc_entity *entity, double *out, mc_entity_error *error,
                                  uint16_t bytecode) {
    const mc_java_random_profile_status status = mc_java_math_random(entity->classes->profile, out);
    if (status == MC_JAVA_RANDOM_PROFILE_OK)
        return MC_ENTITY_OK;
    mc_entity_error_set(error, MC_ENTITY_PROFILE_FAILED, "item constructor", bytecode);
    error->profile = status;
    return error->status;
}

static mc_entity_status item_prefix(mc_entity *entity, mc_entity_error *error) {
    entity->item.b = 0;
    entity->item.f = 5;
    entity->item_bytecode = 12;
    double draw;
    mc_entity_status status = math_draw(entity, &draw, error, 16);
    if (status != MC_ENTITY_OK)
        return status;
    entity->item.d = mc_entity_item_phase_from_draw(draw);
    entity->item_bytecode = 28;
    entity->base.width = 0.25f;
    entity->base.height = 0.25f;
    entity->item_bytecode = 36;
    entity->base.offset = entity->base.height / 2.0f;
    entity->item_bytecode = 46;
    return MC_ENTITY_OK;
}

static mc_entity_status construct(mc_entity *entity, mc_world_runtime *world, double x, double y,
                                  double z, mc_item_stack *stack, bool full,
                                  mc_entity_error *error) {
    mc_entity_error local_error;
    if (error == NULL)
        error = &local_error;
    mc_entity_status admitted = mc_entity_constructor_begin(
        entity, MC_ENTITY_CLASS_KIND_ITEM, world, "item constructor", "item world profile", error);
    if (admitted != MC_ENTITY_OK)
        return admitted;
    mc_entity_status status = mc_entity_construct_base(entity, world, error);
    if (status == MC_ENTITY_OK)
        status = item_prefix(entity, error);
    if (status == MC_ENTITY_OK && full) {
        mc_entity_source_position(entity, x, y, z);
        entity->item_bytecode = 55;
        if (stack != NULL) {
            const mc_item_stack_status item_status = mc_item_stack_retain(stack);
            if (item_status != MC_ITEM_STACK_OK) {
                status = mc_entity_error_set(error, MC_ENTITY_ITEM_FAILED, "item constructor", 61);
                error->item = item_status;
            }
        }
        if (status == MC_ENTITY_OK) {
            entity->stack = stack;
            entity->item_bytecode = 61;
            double draw;
            status = math_draw(entity, &draw, error, 65);
            if (status == MC_ENTITY_OK) {
                entity->base.yaw = mc_entity_item_yaw_from_draw(draw);
                entity->item_bytecode = 73;
                status = math_draw(entity, &draw, error, 77);
            }
            if (status == MC_ENTITY_OK) {
                entity->base.velocity_x = mc_entity_item_velocity_from_draw(draw);
                entity->item_bytecode = 90;
                entity->base.velocity_y = 0.20000000298023224;
                entity->item_bytecode = 97;
                status = math_draw(entity, &draw, error, 101);
            }
            if (status == MC_ENTITY_OK) {
                entity->base.velocity_z = mc_entity_item_velocity_from_draw(draw);
                entity->item_bytecode = 114;
            }
        }
    }
    if (status == MC_ENTITY_OK)
        entity->item_bytecode = full ? 117 : 49;
    return mc_entity_constructor_finish(entity, status, error);
}

mc_entity_status mc_item_entity_construct(mc_entity *raw, mc_world_runtime *world, double x,
                                          double y, double z, mc_item_stack *stack,
                                          mc_entity_error *error) {
    return construct(raw, world, x, y, z, stack, true, error);
}

mc_entity_status mc_item_entity_construct_world(mc_entity *raw, mc_world_runtime *world,
                                                mc_entity_error *error) {
    return construct(raw, world, 0.0, 0.0, 0.0, NULL, false, error);
}

static mc_entity_status construct_falling(mc_entity *entity, mc_world_runtime *world, double x,
                                          double y, double z, int32_t id, bool full,
                                          mc_entity_error *error) {
    mc_entity_error local_error;
    if (error == NULL)
        error = &local_error;
    mc_entity_status admitted =
        mc_entity_constructor_begin(entity, MC_ENTITY_CLASS_KIND_FALLING_SAND, world,
                                    "falling constructor", "falling world profile", error);
    if (admitted != MC_ENTITY_OK)
        return admitted;
    const mc_entity_status status = mc_entity_construct_base(entity, world, error);
    if (status != MC_ENTITY_OK)
        return mc_entity_constructor_finish(entity, status, error);
    entity->falling.b = 0;
    entity->falling_bytecode = 7;
    if (full) {
        entity->falling.a = id;
        entity->falling_bytecode = 13;
        entity->base.aF = true;
        entity->falling_bytecode = 18;
        /* Shared b(FF) only stores size; it leaves the existing bounds alone. */
        entity->base.width = 0.98f;
        entity->base.height = 0.98f;
        entity->falling_bytecode = 26;
        entity->base.offset = entity->base.height / 2.0f;
        entity->falling_bytecode = 36;
        mc_entity_source_position(entity, x, y, z);
        entity->falling_bytecode = 45;
        entity->base.velocity_x = 0.0;
        entity->falling_bytecode = 50;
        entity->base.velocity_y = 0.0;
        entity->falling_bytecode = 55;
        entity->base.velocity_z = 0.0;
        entity->falling_bytecode = 60;
        entity->base.aJ = x;
        entity->falling_bytecode = 65;
        entity->base.aK = y;
        entity->falling_bytecode = 71;
        entity->base.aL = z;
        entity->falling_bytecode = 77;
    }
    entity->falling_bytecode = full ? 80 : 10;
    return mc_entity_constructor_finish(entity, MC_ENTITY_OK, error);
}

mc_entity_status mc_falling_entity_construct_world(mc_entity *raw, mc_world_runtime *world,
                                                   mc_entity_error *error) {
    return construct_falling(raw, world, 0.0, 0.0, 0.0, 0, false, error);
}

mc_entity_status mc_falling_entity_construct(mc_entity *raw, mc_world_runtime *world, double x,
                                             double y, double z, int32_t id,
                                             mc_entity_error *error) {
    return construct_falling(raw, world, x, y, z, id, true, error);
}

mc_entity_status mc_entity_retain(mc_entity *entity) {
    if (entity == NULL)
        return MC_ENTITY_INVALID_ARGUMENT;
    if (entity->holds == SIZE_MAX)
        return MC_ENTITY_LIMIT;
    ++entity->holds;
    return MC_ENTITY_OK;
}

void mc_entity_release(mc_entity *entity) {
    if (entity == NULL)
        return;
    if (--entity->holds != 0)
        return;
    mc_entity_classes *classes = entity->classes;
    const size_t bytes = mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_MINECART)
                             ? sizeof(mc_minecart_entity_object)
                             : sizeof(*entity);
    if (mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_MINECART))
        mc_minecart_entity_dispose(entity);
    mc_nbt_tree_free(entity->storage_source);
    mc_item_stack_release(entity->stack);
    mc_world_runtime_binding_release(entity->base.world);
    if (entity->watcher != NULL) {
        mc_entity_watch_node *node = entity->watcher->map->head;
        while (node != NULL) {
            mc_entity_watch_node *next = node->next;
            retire(classes, entity, node->record, sizeof(*node->record));
            retire(classes, entity, node, sizeof(*node));
            node = next;
        }
        retire(classes, entity, entity->watcher->map, sizeof(*entity->watcher->map));
        retire(classes, entity, entity->watcher, sizeof(*entity->watcher));
    }
    retire(classes, entity, entity->random, sizeof(*entity->random));
    retire(classes, entity, entity->bounds, sizeof(*entity->bounds));
    --classes->live_entities;
    retire(classes, NULL, entity, bytes);
}

mc_entity_status mc_entity_get_snapshot(const mc_entity *entity, mc_entity_snapshot *out) {
    if (entity == NULL || out == NULL)
        return MC_ENTITY_INVALID_ARGUMENT;
    *out = (mc_entity_snapshot){.source = entity->classes->source,
                                .class_literal = entity->class_literal,
                                .phase = entity->phase,
                                .base_bytecode = entity->base_bytecode,
                                .item_bytecode = entity->item_bytecode,
                                .falling_bytecode = entity->falling_bytecode,
                                .base = entity->base,
                                .item = entity->item,
                                .falling = entity->falling,
                                .bounds = entity->bounds,
                                .random = entity->random,
                                .watcher = entity->watcher,
                                .stack = entity->stack};
    return MC_ENTITY_OK;
}

mc_entity_status mc_entity_watcher_get_snapshot(const mc_entity_watcher *watcher,
                                                mc_entity_watcher_snapshot *out) {
    if (watcher == NULL || out == NULL)
        return MC_ENTITY_INVALID_ARGUMENT;
    mc_entity_watcher_snapshot snapshot = {.dirty = watcher->dirty, .size = watcher->map->size};
    const mc_entity_watch_node *node = watcher->map->head;
    while (node != NULL && node->key != 0)
        node = node->next;
    if (node != NULL) {
        snapshot.record = node->record;
        snapshot.index = node->record->index;
        snapshot.type = node->record->type;
        snapshot.byte_value = node->record->byte_value;
        snapshot.record_dirty = node->record->dirty;
    }
    *out = snapshot;
    return MC_ENTITY_OK;
}

mc_entity_status mc_item_entity_stack_get(mc_entity *entity, mc_item_stack **out) {
    if (!mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_ITEM) || out == NULL)
        return MC_ENTITY_INVALID_ARGUMENT;
    if (entity->stack != NULL && mc_item_stack_retain(entity->stack) != MC_ITEM_STACK_OK)
        return MC_ENTITY_ITEM_FAILED;
    *out = entity->stack;
    return MC_ENTITY_OK;
}

size_t mc_entity_native_bytes(const mc_entity *entity) {
    return entity == NULL ? 0 : entity->direct_bytes;
}

mc_entity_status mc_entity_set_velocity(mc_entity *entity, unsigned int axis, double value) {
    if (entity == NULL || axis > 2)
        return MC_ENTITY_INVALID_ARGUMENT;
    if (entity->phase != MC_ENTITY_READY)
        return MC_ENTITY_INVALID_PHASE;
    if (axis == 0)
        entity->base.velocity_x = value;
    else if (axis == 1)
        entity->base.velocity_y = value;
    else
        entity->base.velocity_z = value;
    return MC_ENTITY_OK;
}

mc_entity_status mc_entity_set_position(mc_entity *entity, double x, double y, double z) {
    if (entity == NULL)
        return MC_ENTITY_INVALID_ARGUMENT;
    if (entity->phase != MC_ENTITY_READY)
        return MC_ENTITY_INVALID_PHASE;
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
        return MC_ENTITY_UNSUPPORTED_FP;
    mc_entity_source_position(entity, x, y, z);
    return MC_ENTITY_OK;
}

mc_entity_status mc_entity_set_size(mc_entity *entity, float width, float height) {
    if (entity == NULL)
        return MC_ENTITY_INVALID_ARGUMENT;
    if (entity->phase != MC_ENTITY_READY)
        return MC_ENTITY_INVALID_PHASE;
    entity->base.width = width;
    entity->base.height = height;
    return MC_ENTITY_OK;
}

bool mc_entity_equals(const mc_entity *left, const mc_entity *right) {
    return right != NULL && left->classes == right->classes && left->base.id == right->base.id;
}

int32_t mc_entity_hash_code(const mc_entity *entity) {
    return entity->base.id;
}
