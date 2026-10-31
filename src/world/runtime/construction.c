#include "runtime_internal.h"

#include "../../block/registry/runtime_internal.h"
#include "../../entity/entity_internal.h"
#include "../storage/objects_internal.h"
#include "chunk_provider_internal.h"
#include "collision/boxes_internal.h"
#include "collision/entities_internal.h"
#include "construction_internal.h"
#include "dedicated/entity_index.h"
#include "entity_publication_internal.h"
#include "lighting_internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/core/java/java.h"
#include "mc/world/dimension/world_chunk_manager.h"
#include "mc/world/dimension/world_provider.h"
#include "mc/world/runtime/blocks.h"
#include "mc/world/runtime/entities.h"
#include "millis.h"
#include "source_construction_internal.h"
#include "terminal_inventory_internal.h"
#include "world_access_internal.h"

#include <limits.h>
#include <stdlib.h>

const mc_math_trig *mc_world_runtime_class_trig(const mc_world_runtime *runtime) {
    return runtime->classes->trig;
}

static mc_world_storage_status result(mc_world_storage_error *error, mc_world_storage_status status,
                                      const char *operation) {
    if (error != NULL)
        *error = (mc_world_storage_error){status, operation, 0};
    return status;
}

/* Native terminal admission of the constructor-owned coordinate Set. Entries
 * own no World/Chunk/Entity references. Keep their source contents and order
 * until final native free, just as for the concrete C/E/F owners. */
static bool active_chunks_owned_valid(const mc_world_runtime *world) {
    const mc_world_active_chunks *set = world->active_chunks;
    if (world->fields.P != set)
        return false;
    if (set == NULL)
        return true; /* Constructor has not reached P229. */
    if (set->count > set->capacity || set->capacity > 65536 ||
        ((set->capacity == 0) != (set->entries == NULL)) ||
        mc_java_int_set_size(set->order) != set->count)
        return false;
    for (size_t i = 0; i < set->count; ++i) {
        const mc_world_chunk_candidate *entry = &set->entries[i];
        const uint32_t hash = (entry->x < 0 ? UINT32_C(0x80000000) : 0) |
                              (((uint32_t)entry->x & UINT32_C(0x7fff)) << 16) |
                              (entry->z < 0 ? UINT32_C(0x8000) : 0) |
                              ((uint32_t)entry->z & UINT32_C(0x7fff));
        if ((uint32_t)entry->hash != hash || !mc_java_int_set_contains(set->order, entry->hash))
            return false;
        for (size_t j = 0; j < i; ++j)
            if (set->entries[j].hash == entry->hash)
                return false;
    }
    return true;
}

mc_world_runtime_construction_limits mc_world_runtime_default_construction_limits(void) {
    return (mc_world_runtime_construction_limits){
        .max_direct_allocation_bytes = 16u * 1024u * 1024u,
        .max_scheduled_entries = 262144u,
        .max_provider_chunks = 512u,
        .max_provider_allocation_bytes = 8u * 1024u * 1024u,
        .max_dedicated_entities = 65536u,
        .max_dedicated_allocation_bytes = 8u * 1024u * 1024u,
        .max_dedicated_peak_bytes = 16u * 1024u * 1024u,
        .saved_data = mc_world_saved_data_default_limits()};
}

mc_world_storage_status mc_world_runtime_classes_create(mc_world_info_side side,
                                                        mc_java_random_profile *profile,
                                                        const mc_math_trig *trig,
                                                        mc_world_runtime_classes **out,
                                                        mc_world_storage_error *error) {
    if ((side != MC_WORLD_INFO_CLIENT && side != MC_WORLD_INFO_SERVER) || profile == NULL ||
        trig == NULL || out == NULL)
        return result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "World class arguments");
    mc_world_runtime_classes *classes = calloc(1, sizeof(*classes));
    if (classes == NULL)
        return result(error, MC_WORLD_STORAGE_ALLOCATION_FAILED, "World class owner");
    classes->side = side;
    classes->profile = profile;
    classes->trig = trig;
    *out = classes;
    return result(error, MC_WORLD_STORAGE_OK, NULL);
}

mc_world_storage_status mc_world_runtime_classes_get(const mc_world_runtime_classes *classes,
                                                     mc_world_runtime_class_state *out) {
    if (classes == NULL || out == NULL)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    *out = (mc_world_runtime_class_state){classes->side, classes->initialized,
                                          classes->enqueue_depth, classes->live_worlds};
    return MC_WORLD_STORAGE_OK;
}

mc_world_storage_status mc_world_runtime_classes_free(mc_world_runtime_classes *classes,
                                                      mc_world_storage_error *error) {
    if (classes == NULL)
        return result(error, MC_WORLD_STORAGE_OK, NULL);
    if (classes->live_worlds != 0 || classes->native_borrows != 0)
        return result(error, MC_WORLD_STORAGE_BUSY, "World class has live instances");
    if (classes->registry != NULL) {
        mc_registry_error cause;
        mc_registry_status status =
            mc_registry_runtime_borrow_release_admit(classes->registry, &cause);
        if (status != MC_REGISTRY_OK)
            return result(error, mc_world_runtime_registry_status(status), cause.message);
    }
    if (classes->entities != NULL) {
        if (mc_entity_classes_runtime_release_admit(classes->entities) != MC_ENTITY_OK)
            return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                          "World Entity class binding release");
    }
    const mc_world_storage_status tile_entities =
        mc_live_tile_entities_classes_retire(classes, error);
    if (tile_entities != MC_WORLD_STORAGE_OK)
        return tile_entities;
    /* All dependent releases are admitted above under unchanged exclusive
     * owners. TileEntity retirement cannot alter either application binding. */
    if (classes->entities != NULL) {
        (void)mc_entity_classes_release(classes->entities);
        classes->entities = NULL;
    }
    if (classes->registry != NULL) {
        (void)mc_registry_classes_release(classes->registry, NULL);
        classes->registry = NULL;
    }
    mc_java_int_set_free(classes->spawning_chunks.order);
    free(classes->spawning_chunks.entries);
    free(classes);
    return result(error, MC_WORLD_STORAGE_OK, NULL);
}

mc_registry_status mc_world_runtime_classes_bind_registry(mc_world_runtime_classes *classes,
                                                          mc_registry_classes *registry,
                                                          mc_registry_error *error) {
    mc_registry_status status = MC_REGISTRY_INVALID_ARGUMENT;
    const char *message = "World registry binding arguments";
    if (classes != NULL && registry != NULL) {
        if (classes->registry == NULL && !classes->initialized && classes->live_worlds == 0) {
            status = mc_registry_classes_retain(
                registry,
                classes->side == MC_WORLD_INFO_CLIENT ? MC_REGISTRY_CLIENT : MC_REGISTRY_SERVER,
                classes->profile, error);
            if (status != MC_REGISTRY_OK)
                return status;
            if (classes->entities != NULL) {
                status = mc_registry_classes_bind_entities(registry, classes->entities, error);
                if (status != MC_REGISTRY_OK) {
                    (void)mc_registry_classes_release(registry, NULL);
                    return status;
                }
            }
            if (status == MC_REGISTRY_OK)
                classes->registry = registry;
            return status;
        }
        status = MC_REGISTRY_BUSY;
        message = "World registry already bound or class used for allocation";
    }
    if (error != NULL)
        *error = (mc_registry_error){.status = status,
                                     .message = message,
                                     .initializer_pc = -1,
                                     .block_id = -1,
                                     .constructor_pc = -1,
                                     .active_initializer_pc = -1,
                                     .item_argument = -1};
    return status;
}

mc_registry_classes *mc_world_runtime_registry(const mc_world_runtime *runtime) {
    return runtime == NULL || runtime->classes == NULL ? NULL : runtime->classes->registry;
}

mc_entity_status mc_world_runtime_classes_bind_entities(mc_world_runtime_classes *classes,
                                                        mc_entity_classes *entities) {
    if (classes == NULL || entities == NULL)
        return MC_ENTITY_INVALID_ARGUMENT;
    if (classes->entities != NULL || classes->initialized || classes->live_worlds != 0)
        return MC_ENTITY_BUSY;
    const mc_entity_status status = mc_entity_classes_retain(
        entities, classes->side == MC_WORLD_INFO_CLIENT ? MC_ENTITY_CLIENT : MC_ENTITY_SERVER,
        classes->profile);
    if (status != MC_ENTITY_OK)
        return status;
    if (classes->registry != NULL) {
        const mc_registry_status joined =
            mc_registry_classes_bind_entities(classes->registry, entities, NULL);
        if (joined != MC_REGISTRY_OK) {
            (void)mc_entity_classes_release(entities);
            if (joined == MC_REGISTRY_LIMIT)
                return MC_ENTITY_LIMIT;
            return joined == MC_REGISTRY_BUSY ? MC_ENTITY_BUSY : MC_ENTITY_INVALID_ARGUMENT;
        }
    }
    classes->entities = entities;
    return MC_ENTITY_OK;
}

mc_entity_classes *mc_world_runtime_entities(const mc_world_runtime *runtime) {
    return runtime == NULL || runtime->classes == NULL ? NULL : runtime->classes->entities;
}

mc_world_storage_status
mc_world_runtime_allocate_world(mc_world_runtime_entry entry, mc_world_runtime_classes *classes,
                                const mc_world_runtime_construction_limits *limits,
                                mc_world_runtime **out, mc_world_storage_error *error) {
    if (out == NULL || classes == NULL || entry < MC_WORLD_RUNTIME_CLIENT_LOAD_OR_CREATE ||
        entry > MC_WORLD_RUNTIME_SERVER_DEDICATED)
        return result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "World allocation arguments");
    mc_world_runtime_construction_limits selected =
        limits == NULL ? mc_world_runtime_default_construction_limits() : *limits;
    if (selected.max_scheduled_entries > INT32_MAX || selected.max_provider_chunks > INT32_MAX ||
        selected.max_dedicated_entities > INT32_MAX ||
        (entry >= MC_WORLD_RUNTIME_SERVER_LOAD_OR_CREATE) !=
            (classes->side == MC_WORLD_INFO_SERVER))
        return result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "World construction limits");
    if (sizeof(mc_world_runtime) > selected.max_direct_allocation_bytes)
        return result(error, MC_WORLD_STORAGE_LIMIT, "World raw owner limit");
    if (classes->live_worlds == SIZE_MAX)
        return result(error, MC_WORLD_STORAGE_LIMIT, "World class instance limit");
    if (!classes->initialized) {
        classes->enqueue_depth = 0;
        classes->initialized = true;
    }
    mc_world_runtime *runtime = calloc(1, sizeof(*runtime));
    if (runtime == NULL)
        return result(error, MC_WORLD_STORAGE_ALLOCATION_FAILED, "World raw owner");
    runtime->options.random_profile = classes->profile;
    runtime->classes = classes;
    runtime->construction_limits = selected;
    runtime->construction =
        (mc_world_runtime_construction){.mode = MC_WORLD_RUNTIME_GAME,
                                        .phase = MC_WORLD_RUNTIME_RAW,
                                        .entry = entry,
                                        .direct_allocation_bytes = sizeof(*runtime)};
    ++classes->live_worlds;
    *out = runtime;
    return result(error, MC_WORLD_STORAGE_OK, NULL);
}

mc_world_storage_status mc_world_runtime_construction_get(const mc_world_runtime *runtime,
                                                          mc_world_runtime_construction *out) {
    if (runtime == NULL || out == NULL)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    mc_world_runtime_construction state = runtime->construction;
    state.random_status = runtime->last_random_status;
    *out = state;
    return MC_WORLD_STORAGE_OK;
}

mc_world_storage_status mc_world_runtime_fields_get(const mc_world_runtime *runtime,
                                                    mc_world_runtime_fields *out) {
    if (runtime == NULL || out == NULL)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    if (runtime->construction.mode != MC_WORLD_RUNTIME_GAME)
        return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    *out = runtime->fields;
    return MC_WORLD_STORAGE_OK;
}

mc_world_storage_status
mc_world_runtime_dedicated_get(const mc_world_runtime *runtime,
                               mc_world_runtime_dedicated_fields *fields,
                               mc_world_runtime_dedicated_construction *construction) {
    if (runtime == NULL || (fields == NULL && construction == NULL))
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    if (runtime->construction.mode != MC_WORLD_RUNTIME_GAME ||
        runtime->construction.entry != MC_WORLD_RUNTIME_SERVER_DEDICATED)
        return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    if (fields != NULL)
        *fields = runtime->dedicated;
    if (construction != NULL)
        *construction = runtime->dedicated_construction;
    return MC_WORLD_STORAGE_OK;
}

static void *world_allocate(mc_world_runtime *runtime, size_t bytes, unsigned int call_pc,
                            const char *operation) {
    size_t used = runtime->construction.direct_allocation_bytes;
    if (bytes > runtime->construction_limits.max_direct_allocation_bytes - used) {
        runtime->construction.failed_call_pc = call_pc;
        result(&runtime->last_error, MC_WORLD_STORAGE_LIMIT, operation);
        return NULL;
    }
    void *allocation = calloc(1, bytes);
    if (allocation == NULL) {
        runtime->construction.failed_call_pc = call_pc;
        result(&runtime->last_error, MC_WORLD_STORAGE_ALLOCATION_FAILED, operation);
        return NULL;
    }
    runtime->construction.direct_allocation_bytes += bytes;
    return allocation;
}

static bool world_list(mc_world_runtime *runtime, mc_world_reference_list **field,
                       unsigned int call_pc, unsigned int store_pc) {
    mc_world_reference_list *list =
        world_allocate(runtime, sizeof(*list), call_pc, "World ArrayList");
    if (list == NULL)
        return false;
    *field = list;
    if (field == &runtime->fields.b)
        runtime->entity_membership.list = list;
    else if (field == &runtime->fields.D)
        runtime->entity_deferral.list = list;
    else if (field == &runtime->fields.d)
        runtime->player_membership.list = list;
    else if (field == &runtime->fields.e)
        runtime->weather_membership.list = list;
    else if (field == &runtime->fields.R)
        runtime->nearby_entities.list = list;
    else if (field == &runtime->fields.K)
        runtime->collision_boxes.list = list;
    else if (field == &runtime->fields.u)
        runtime->world_accesses.list = list;
    else if (field == &runtime->fields.c)
        mc_live_tile_entities_capture_list(runtime, false, list);
    else if (field == &runtime->fields.G)
        mc_live_tile_entities_capture_list(runtime, true, list);
    runtime->construction.last_store_pc = store_pc;
    return true;
}

static bool world_random(mc_world_runtime *runtime, mc_java_random **out,
                         unsigned int allocation_pc, unsigned int call_pc) {
    mc_java_random *random =
        world_allocate(runtime, sizeof(*random), allocation_pc, "World Random owner");
    if (random == NULL)
        return false;
    mc_java_random_profile_status status =
        mc_java_random_construct_default(runtime->options.random_profile, random);
    runtime->last_random_status = status;
    if (status != MC_JAVA_RANDOM_PROFILE_OK) {
        runtime->construction.failed_call_pc = call_pc;
        mc_world_storage_status mapped =
            status == MC_JAVA_RANDOM_PROFILE_INVALID_ARGUMENT ? MC_WORLD_STORAGE_INVALID_ARGUMENT
            : status == MC_JAVA_RANDOM_PROFILE_LIMIT          ? MC_WORLD_STORAGE_LIMIT
            : status == MC_JAVA_RANDOM_PROFILE_ALLOCATION_FAILED
                ? MC_WORLD_STORAGE_ALLOCATION_FAILED
                : MC_WORLD_STORAGE_UNSUPPORTED_STATE;
        result(&runtime->last_error, mapped, "World default Random construction");
        runtime->construction.direct_allocation_bytes -= sizeof(*random);
        free(random);
        return false;
    }
    *out = random;
    return true;
}

static bool world_prefix(mc_world_runtime *runtime) {
    mc_world_runtime_fields *f = &runtime->fields;
    mc_world_runtime_construction *state = &runtime->construction;
    f->a = false;
    state->last_store_pc = 6;
    if (!world_list(runtime, &f->C, 14, 17) || !world_list(runtime, &f->b, 25, 28) ||
        !world_list(runtime, &f->D, 36, 39))
        return false;
    size_t remaining =
        runtime->construction_limits.max_direct_allocation_bytes - state->direct_allocation_bytes;
    mc_scheduled_ordered_status ordered = mc_scheduled_ordered_create(
        runtime->construction_limits.max_scheduled_entries, remaining, &f->E);
    if (ordered != MC_SCHEDULED_ORDERED_OK) {
        state->failed_call_pc = 47;
        result(&runtime->last_error,
               ordered == MC_SCHEDULED_ORDERED_ALLOCATION_FAILED
                   ? MC_WORLD_STORAGE_ALLOCATION_FAILED
                   : MC_WORLD_STORAGE_LIMIT,
               "World scheduled ordered owner");
        return false;
    }
    size_t bytes = 0;
    (void)mc_scheduled_ordered_native_bytes(f->E, &bytes);
    state->direct_allocation_bytes += bytes;
    state->last_store_pc = 50;
    remaining =
        runtime->construction_limits.max_direct_allocation_bytes - state->direct_allocation_bytes;
    mc_scheduled_membership_status membership = mc_scheduled_membership_create(
        runtime->construction_limits.max_scheduled_entries, remaining, &f->F);
    if (membership != MC_SCHEDULED_MEMBERSHIP_OK) {
        state->failed_call_pc = 58;
        result(&runtime->last_error,
               membership == MC_SCHEDULED_MEMBERSHIP_ALLOCATION_FAILED
                   ? MC_WORLD_STORAGE_ALLOCATION_FAILED
                   : MC_WORLD_STORAGE_LIMIT,
               "World scheduled membership owner");
        return false;
    }
    (void)mc_scheduled_membership_native_bytes(f->F, &bytes);
    state->direct_allocation_bytes += bytes;
    state->last_store_pc = 61;
    if (!world_list(runtime, &f->c, 69, 72) || !world_list(runtime, &f->G, 80, 83) ||
        !world_list(runtime, &f->d, 91, 94) || !world_list(runtime, &f->e, 102, 105))
        return false;
    f->H = INT64_C(16777215);
    state->last_store_pc = 112;
    f->f = 0;
    state->last_store_pc = 117;
    mc_java_random *temporary = NULL;
    if (!world_random(runtime, &temporary, 121, 125))
        return false;
    f->g = mc_java_random_next_i32(temporary);
    state->last_store_pc = 131;
    state->direct_allocation_bytes -= sizeof(*temporary);
    free(temporary);
    f->h = INT32_C(1013904223);
    state->last_store_pc = 137;
    f->m = 0;
    state->last_store_pc = 142;
    f->n = 0;
    state->last_store_pc = 147;
    f->o = false;
    state->last_store_pc = 152;
    int64_t millis = 0;
    mc_platform_millis_status clock = mc_platform_current_time_millis(&millis);
    state->clock_status = (int32_t)clock;
    if (clock != MC_PLATFORM_MILLIS_OK) {
        state->failed_call_pc = 156;
        result(&runtime->last_error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World wall clock");
        return false;
    }
    f->I = millis;
    state->last_store_pc = 159;
    f->p = 40;
    state->last_store_pc = 165;
    if (!world_random(runtime, &f->r, 169, 173))
        return false;
    state->last_store_pc = 176;
    f->s = false;
    state->last_store_pc = 181;
    if (!world_list(runtime, &f->u, 189, 192) || !world_list(runtime, &f->K, 200, 203))
        return false;
    f->M = 0;
    state->last_store_pc = 208;
    f->N = true;
    state->last_store_pc = 213;
    f->O = true;
    state->last_store_pc = 218;
    f->P = world_allocate(runtime, sizeof(*f->P), 226, "World active chunk set");
    if (f->P == NULL)
        return false;
    runtime->active_chunks = f->P;
    state->last_store_pc = 229;
    (void)mc_java_random_next_bounded(f->r, 12000, &f->Q);
    state->last_store_pc = 243;
    if (!world_list(runtime, &f->R, 251, 254))
        return false;
    f->B = false;
    state->last_store_pc = 259;
    return true;
}

static bool world_info_result(mc_world_runtime *runtime, mc_world_info_status status,
                              unsigned int call_pc) {
    if (status == MC_WORLD_INFO_OK)
        return true;
    runtime->construction.failed_call_pc = call_pc;
    mc_world_storage_status mapped =
        status == MC_WORLD_INFO_ALLOCATION_FAILED  ? MC_WORLD_STORAGE_ALLOCATION_FAILED
        : status == MC_WORLD_INFO_LIMIT            ? MC_WORLD_STORAGE_LIMIT
        : status == MC_WORLD_INFO_INVALID_ARGUMENT ? MC_WORLD_STORAGE_INVALID_ARGUMENT
                                                   : MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    result(&runtime->last_error, mapped, mc_world_info_status_string(status));
    return false;
}

static void world_bind_info_cells(mc_world_runtime *runtime) {
    const mc_world_info_live *cells = mc_world_info_live_get(runtime->fields.x);
    runtime->live = (mc_world_runtime_state){
        .time = cells->time,
        .spawn_x = cells->spawn_x,
        .spawn_y = cells->spawn_y,
        .spawn_z = cells->spawn_z,
        .world_force_load = &runtime->fields.y,
        .provider_force_load = &runtime->provider_force_load,
        .saving_disabled = runtime->construction.entry == MC_WORLD_RUNTIME_SERVER_DEDICATED
                               ? &runtime->dedicated.E
                               : &runtime->saving_disabled};
}

static bool world_new_info(mc_world_runtime *runtime,
                           const mc_world_runtime_world_arguments *arguments,
                           const mc_world_info *copy, bool copying, unsigned int call_pc,
                           unsigned int store_pc) {
    mc_world_info *info = NULL;
    mc_world_info_status status = mc_world_info_allocate(runtime->classes->side, &info);
    if (status == MC_WORLD_INFO_OK)
        status = copying ? mc_world_info_construct_copy(info, copy)
                         : mc_world_info_construct(info, arguments->seed, arguments->name);
    if (!world_info_result(runtime, status, call_pc)) {
        mc_world_info_release(info);
        return false;
    }
    runtime->fields.x = info;
    runtime->construction.last_store_pc = store_pc;
    world_bind_info_cells(runtime);
    return true;
}

static bool world_saved_data(mc_world_runtime *runtime, unsigned int call_pc,
                             unsigned int store_pc) {
    mc_world_saved_data *saved = NULL;
    const bool source = mc_world_store_is_source(runtime->fields.w);
    mc_world_storage_status status =
        source ? mc_world_runtime_source_saved_data(runtime, &runtime->last_error)
               : mc_world_saved_data_create_for_side(runtime->fields.w, runtime->classes->side,
                                                     &runtime->construction_limits.saved_data,
                                                     &saved, &runtime->last_error);
    if (status != MC_WORLD_STORAGE_OK) {
        runtime->construction.failed_call_pc = call_pc;
        return false;
    }
    if (!source)
        runtime->fields.z = saved;
    runtime->construction.last_store_pc = store_pc;
    return true;
}

static bool world_dimension_result(mc_world_runtime *runtime, mc_world_provider_status status,
                                   unsigned int call_pc) {
    if (status == MC_WORLD_PROVIDER_OK)
        return true;
    runtime->construction.failed_call_pc = call_pc;
    mc_world_storage_status mapped =
        status == MC_WORLD_PROVIDER_ALLOCATION_FAILED  ? MC_WORLD_STORAGE_ALLOCATION_FAILED
        : status == MC_WORLD_PROVIDER_LIMIT            ? MC_WORLD_STORAGE_LIMIT
        : status == MC_WORLD_PROVIDER_INVALID_ARGUMENT ? MC_WORLD_STORAGE_INVALID_ARGUMENT
                                                       : MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    result(&runtime->last_error, mapped, mc_world_provider_status_string(status));
    return false;
}

static bool world_set_dimension(mc_world_runtime *runtime, mc_world_provider *dimension,
                                unsigned int store_pc) {
    if (dimension != NULL &&
        !world_dimension_result(runtime, mc_world_provider_retain(dimension), store_pc))
        return false;
    runtime->fields.t = dimension;
    runtime->construction.last_store_pc = store_pc;
    return true;
}

static bool world_metadata_tail(mc_world_runtime *runtime,
                                const mc_world_runtime_world_arguments *arguments,
                                bool *new_world) {
    mc_world_runtime_fields *f = &runtime->fields;
    mc_world_runtime_construction *state = &runtime->construction;
    *new_world = false;
    if (state->entry == MC_WORLD_RUNTIME_CLIENT_COPY) {
        const mc_world_runtime *parent = arguments->parent;
        if (parent == NULL || parent->construction.mode != MC_WORLD_RUNTIME_GAME ||
            parent->options.random_profile != runtime->options.random_profile) {
            state->failed_call_pc = 264;
            result(&runtime->last_error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "World copy parent");
            return false;
        }
        f->I = parent->fields.I;
        state->last_store_pc = 267;
        if (mc_world_runtime_source_bind_store(runtime, parent->fields.w, &runtime->last_error) !=
            MC_WORLD_STORAGE_OK) {
            state->failed_call_pc = 275;
            return false;
        }
        f->w = parent->fields.w;
        runtime->options.store = f->w;
        state->last_store_pc = 275;
        if (!world_new_info(runtime, arguments, parent->fields.x, true, 287, 290) ||
            !world_saved_data(runtime, 302, 305))
            return false;
        return world_set_dimension(runtime, arguments->dimension, 310);
    }
    if (mc_world_runtime_source_bind_store(runtime, arguments->store, &runtime->last_error) !=
        MC_WORLD_STORAGE_OK) {
        state->failed_call_pc = 264;
        return false;
    }
    f->w = arguments->store;
    runtime->options.store = f->w;
    state->last_store_pc = 264;
    if (state->entry == MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA) {
        if (!world_new_info(runtime, arguments, NULL, false, 275, 278) ||
            !world_set_dimension(runtime, arguments->dimension, 283) ||
            !world_saved_data(runtime, 292, 295))
            return false;
        return true;
    }
    if (!world_saved_data(runtime, 273, 276))
        return false;
    mc_world_info *loaded = NULL;
    mc_world_storage_status status =
        mc_world_store_is_source(f->w)
            ? mc_world_runtime_source_read_info(runtime, &loaded, &runtime->last_error)
            : mc_world_store_load_world_info(f->w, runtime->classes->side, &loaded,
                                             &runtime->last_error);
    if (status != MC_WORLD_STORAGE_OK && status != MC_WORLD_STORAGE_MISSING) {
        state->failed_call_pc = 281;
        return false;
    }
    f->x = loaded;
    state->last_store_pc = 286;
    f->s = loaded == NULL;
    state->last_store_pc = 302;
    if (arguments->dimension != NULL) {
        if (!world_set_dimension(runtime, arguments->dimension, 313))
            return false;
    } else {
        int32_t selected = 0;
        if (loaded != NULL) {
            mc_world_info_fields fields;
            (void)mc_world_info_fields_get(loaded, &fields);
            if (fields.value.player_dimension == -1)
                selected = -1;
        }
        mc_world_provider *dimension = NULL;
        mc_world_provider_status created =
            mc_world_provider_create(selected, NULL, &dimension, NULL);
        if (!world_dimension_result(runtime, created, selected == -1 ? 339u : 350u))
            return false;
        f->t = dimension;
        state->last_store_pc = selected == -1 ? 342u : 353u;
    }
    if (loaded == NULL) {
        if (!world_new_info(runtime, arguments, NULL, false, 373, 376))
            return false;
        *new_world = true;
    } else {
        if (!world_info_result(runtime, mc_world_info_name_set(loaded, arguments->name), 390))
            return false;
        world_bind_info_cells(runtime);
    }
    return true;
}

static bool world_chunk_provider(mc_world_runtime *runtime, unsigned int call_pc) {
    runtime->construction.failed_call_pc = call_pc;
    if (runtime->fields.w == NULL) {
        result(&runtime->last_error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "World loader storage");
        return false;
    }
    /* The concrete normal store loader is a stable borrowed store/dimension
     * binding. Its region files are opened by the actual first chunk request.
     * Other selected provider/loader kinds remain explicit missing descendants. */
    mc_world_provider_state dimension;
    if (mc_world_provider_state_get(runtime->fields.t, &dimension) != MC_WORLD_PROVIDER_OK) {
        result(&runtime->last_error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World loader Dimension");
        return false;
    }
    /* Actual fd.b() calls wt.a(Dimension)8 BEFORE Dimension.generator24 and
     * ok27. This creates the genuine ld/File owner without opening regions. */
    if (mc_world_store_is_source(runtime->fields.w) &&
        mc_world_runtime_source_create_loader(runtime, &runtime->last_error) != MC_WORLD_STORAGE_OK)
        return false;
    runtime->loader =
        (mc_world_store_loader){.store = runtime->fields.w,
                                .dimension = dimension.kind == MC_WORLD_PROVIDER_NETHER ? -1 : 0};
    mc_chunk_provider *provider = NULL;
    mc_java_random_profile_status cause = runtime->last_random_status;
    mc_world_storage_status status = mc_chunk_provider_create(
        runtime, runtime->fields.t, runtime->classes->trig, runtime->classes->profile,
        runtime->construction_limits.max_provider_chunks,
        runtime->construction_limits.max_provider_allocation_bytes, &provider, &cause,
        &runtime->last_error);
    runtime->last_random_status = cause;
    if (status != MC_WORLD_STORAGE_OK)
        return false;
    if (runtime->construction.entry == MC_WORLD_RUNTIME_SERVER_DEDICATED) {
        runtime->dedicated.C = provider; /* dp.b() store31 precedes dj.v406. */
        runtime->dedicated_construction.last_store_pc = 31;
    }
    runtime->fields.v = provider;
    runtime->construction.last_store_pc = call_pc + 3u;
    runtime->construction.failed_call_pc = 0;
    mc_chunk_provider_fields fields;
    status = mc_chunk_provider_bind_fields(provider, &fields, &runtime->last_error);
    if (status != MC_WORLD_STORAGE_OK)
        return false;
    runtime->options.cache = fields.cache;
    runtime->options.pending = fields.pending;
    runtime->options.dimension = 0;
    runtime->options.policy = fields.policy;
    runtime->live.provider_force_load = fields.force_load;
    runtime->options.generator = fields.generator;
    runtime->options.provider = provider;
    status = mc_world_runtime_bind_contexts(runtime, &runtime->last_error);
    if (status != MC_WORLD_STORAGE_OK) {
        runtime->construction.failed_call_pc = call_pc;
        return false;
    }
    return true;
}

static bool world_spawn_draw(mc_world_runtime *runtime, int32_t *out) {
    if (runtime->fields.r == NULL || !mc_java_random_next_bounded(runtime->fields.r, 64, out)) {
        result(&runtime->last_error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
               "World spawn Random receiver");
        return false;
    }
    return true;
}

static bool world_initial_spawn(mc_world_runtime *runtime) {
    runtime->fields.y = true;
    int32_t x = 0, z = 0;
    for (;;) {
        bool accepted;
        mc_world_provider_error cause;
        mc_world_provider_status status =
            mc_world_provider_can_spawn(runtime->fields.t, x, z, &accepted, &cause);
        if (status != MC_WORLD_PROVIDER_OK) {
            if (status == MC_WORLD_PROVIDER_REGISTRY_FAILED)
                (void)mc_world_runtime_registry_result(runtime, cause.registry.status,
                                                       &cause.registry, "World spawn registry",
                                                       &runtime->last_error);
            else if (status == MC_WORLD_PROVIDER_WORLD_FAILED)
                runtime->last_error = cause.world;
            else
                result(&runtime->last_error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                       "World spawn Dimension");
            return false;
        }
        if (accepted)
            break;
        /* Reload the actual World.r separately at every original draw. x is
         * completed before the z pair. The source has no rejection cap. */
        int32_t first, second;
        if (!world_spawn_draw(runtime, &first))
            return false;
        if (!world_spawn_draw(runtime, &second))
            return false;
        x = mc_java_i32_add(x, mc_java_i32_sub(first, second));
        if (!world_spawn_draw(runtime, &first))
            return false;
        if (!world_spawn_draw(runtime, &second))
            return false;
        z = mc_java_i32_add(z, mc_java_i32_sub(first, second));
    }
    if (!world_info_result(runtime, mc_world_info_spawn_set(runtime->fields.x, x, 64, z), 415))
        return false;
    runtime->fields.y = false;
    return true;
}

static bool dedicated_tail(mc_world_runtime *runtime,
                           const mc_world_runtime_world_arguments *arguments) {
    mc_world_runtime_dedicated_fields *f = &runtime->dedicated;
    mc_world_runtime_dedicated_construction *state = &runtime->dedicated_construction;
    f->D = false;
    state->last_store_pc = 15;
    state->failed_call_pc = 23;
    const mc_world_entity_index_limits limits = {
        runtime->construction_limits.max_dedicated_entities,
        runtime->construction_limits.max_dedicated_allocation_bytes,
        runtime->construction_limits.max_dedicated_peak_bytes};
    mc_world_entity_index_error cause;
    if (mc_world_entity_index_create(&limits, &f->G, &cause) != MC_WORLD_STORAGE_OK) {
        runtime->last_error = cause.cause;
        return false;
    }
    state->last_store_pc = 26;
    state->failed_call_pc = 31;
    if (arguments->server != NULL) {
        const mc_server_status held = mc_server_runtime_retain(arguments->server);
        if (held != MC_SERVER_OK) {
            result(&runtime->last_error,
                   held == MC_SERVER_LIMIT ? MC_WORLD_STORAGE_LIMIT
                                           : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                   "Dedicated World server ownership");
            return false;
        }
    }
    f->F = arguments->server;
    state->last_store_pc = 31;
    state->failed_call_pc = 0;
    return true;
}

mc_world_storage_status
mc_world_runtime_construct_world(mc_world_runtime *runtime,
                                 const mc_world_runtime_world_arguments *arguments,
                                 mc_world_storage_error *error) {
    if (runtime == NULL || arguments == NULL)
        return result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "World constructor arguments");
    if (runtime->construction.mode != MC_WORLD_RUNTIME_GAME ||
        runtime->construction.phase != MC_WORLD_RUNTIME_RAW)
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World constructor phase");
    if (runtime->active_calls == SIZE_MAX)
        return result(error, MC_WORLD_STORAGE_LIMIT, "World constructor call limit");
    ++runtime->active_calls;
    runtime->construction.phase = MC_WORLD_RUNTIME_CONSTRUCTING;
    bool completed = false;
    const bool dedicated = runtime->construction.entry == MC_WORLD_RUNTIME_SERVER_DEDICATED;
    mc_world_runtime_world_arguments selected = *arguments;
    mc_world_provider *factory_dimension = NULL;
    bool prefix = true;
    if (dedicated) {
        runtime->dedicated_construction.failed_call_pc = 7;
        prefix = world_dimension_result(
            runtime,
            mc_world_provider_create(arguments->dimension_id, NULL, &factory_dimension, NULL), 7);
        selected.dimension = factory_dimension;
        if (prefix)
            runtime->dedicated_construction.failed_call_pc = 10;
    }
    if (prefix)
        prefix = world_prefix(runtime);
    if (prefix) {
        bool new_world = false;
        if (world_metadata_tail(runtime, &selected, &new_world)) {
            unsigned int bind_pc =
                runtime->construction.entry == MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA ? 300u
                : runtime->construction.entry == MC_WORLD_RUNTIME_CLIENT_COPY          ? 315u
                                                                                       : 398u;
            mc_world_provider_error dimension_cause = {0};
            mc_world_provider_status bound;
            if (runtime->classes->registry != NULL) {
                const mc_world_provider_registry_binding binding = {
                    .classes = runtime->classes->registry,
                    .side = runtime->classes->side == MC_WORLD_INFO_CLIENT ? MC_REGISTRY_CLIENT
                                                                           : MC_REGISTRY_SERVER,
                    .profile = runtime->classes->profile};
                bound = mc_world_provider_bind_game(runtime->fields.t, runtime, &binding,
                                                    &dimension_cause);
            } else {
                bound = mc_world_provider_bind(runtime->fields.t, runtime, &dimension_cause);
            }
            bool dimension_ready = false;
            if (bound != MC_WORLD_PROVIDER_OK &&
                dimension_cause.registry.status != MC_REGISTRY_OK) {
                runtime->construction.failed_call_pc = bind_pc;
                (void)mc_world_runtime_registry_result(runtime, dimension_cause.registry.status,
                                                       &dimension_cause.registry,
                                                       "World Dimension Biome registry", NULL);
            } else {
                dimension_ready = world_dimension_result(runtime, bound, bind_pc);
            }
            if (dimension_ready) {
                if (world_chunk_provider(runtime, bind_pc + 5u)) {
                    bool spawn_ready = !new_world || world_initial_spawn(runtime);
                    if (!spawn_ready)
                        runtime->construction.failed_call_pc = 415;
                    if (spawn_ready) {
                        unsigned int light_pc = bind_pc == 398u ? 419u : bind_pc + 12u;
                        if (mc_world_runtime_initialize_environment(
                                runtime, &runtime->last_error) == MC_WORLD_STORAGE_OK)
                            completed = true;
                        else
                            runtime->construction.failed_call_pc = light_pc;
                    }
                }
            }
        }
    }
    if (completed && dedicated)
        completed = dedicated_tail(runtime, arguments);
    mc_world_provider_release(factory_dimension);
    runtime->construction.phase =
        completed ? MC_WORLD_RUNTIME_READY : MC_WORLD_RUNTIME_FAILED_CONSTRUCTION;
    --runtime->active_calls;
    if (error != NULL)
        *error = runtime->last_error;
    return runtime->last_error.status;
}

mc_world_storage_status mc_world_runtime_game_free(mc_world_runtime *runtime,
                                                   mc_world_storage_error *error) {
    if (runtime->construction.contexts_bound || runtime->construction.cache_registered)
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World context retirement");
    if (!active_chunks_owned_valid(runtime))
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "World active Chunk Set ownership metadata");
    if (!mc_world_runtime_entity_list_valid(runtime) ||
        (!mc_world_runtime_player_list_retirement_valid(runtime) ||
         !mc_world_runtime_weather_list_retirement_valid(runtime)))
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World Entity ownership metadata");
    if (runtime->entity_membership.count != 0 || runtime->entity_deferral.count != 0 ||
        runtime->player_membership.count != 0 || runtime->weather_membership.count != 0)
        return result(error, MC_WORLD_STORAGE_BUSY, "World Entity occurrences remain");
    if (!mc_live_tile_entities_lists_valid(runtime))
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "World TileEntity ownership metadata");
    if (runtime->tile_entity_membership[0].count != 0 ||
        runtime->tile_entity_membership[1].count != 0)
        return result(error, MC_WORLD_STORAGE_BUSY, "World TileEntity occurrences remain");
    if (!mc_world_nearby_owned_valid(runtime))
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World nearby ownership metadata");
    if (runtime->nearby_entities.count != 0)
        return result(error, MC_WORLD_STORAGE_BUSY, "World nearby occurrences remain");
    if (!mc_world_collision_owned_valid(runtime))
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "World collision ownership metadata");
    if (runtime->collision_boxes.count != 0)
        return result(error, MC_WORLD_STORAGE_BUSY, "World collision occurrences remain");
    if (!mc_world_accesses_retirement_valid(runtime))
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "WorldAccess ownership metadata");
    /* Every existing active/registration/container guard above has passed.
     * Checked source child cleanup may consume earlier native slots on failure;
     * preserve this SAME World and all remaining owners for retry. */
    const mc_world_storage_status source_cleanup =
        mc_world_runtime_source_prepare_free(runtime, error);
    if (source_cleanup != MC_WORLD_STORAGE_OK)
        return source_cleanup;
    if (runtime->dedicated.F != NULL) {
        if (mc_server_runtime_release(runtime->dedicated.F) != MC_SERVER_OK)
            return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                          "Dedicated World server release");
        runtime->dedicated.F = NULL;
    }
    mc_world_entity_index_free(runtime->dedicated.G);
    runtime->dedicated.G = NULL;
    mc_world_runtime_fields *f = &runtime->fields;
    mc_world_runtime_entity_list_release(runtime);
    mc_live_tile_entities_lists_release(runtime);
    mc_world_nearby_owned_release(runtime);
    mc_world_collision_owned_release(runtime);
    mc_world_accesses_owned_release(runtime);
    mc_world_runtime_lighting_free(runtime);
    mc_world_reference_list *lists[] = {f->C,
                                        runtime->entity_membership.list,
                                        runtime->entity_deferral.list,
                                        f->c,
                                        f->G,
                                        runtime->player_membership.list,
                                        runtime->weather_membership.list,
                                        runtime->world_accesses.list,
                                        runtime->collision_boxes.list,
                                        runtime->nearby_entities.list};
    for (size_t i = 0; i < sizeof(lists) / sizeof(lists[0]); ++i) {
        if (lists[i] != NULL) {
            free(lists[i]->items);
            free(lists[i]);
        }
    }
    mc_scheduled_ordered_destroy(f->E);
    mc_scheduled_membership_destroy(f->F);
    free(f->r);
    if (runtime->active_chunks != NULL) {
        mc_java_int_set_free(runtime->active_chunks->order);
        free(runtime->active_chunks->entries);
        free(runtime->active_chunks);
    }
    mc_chunk_provider_free(f->v);
    mc_world_info_release(f->x);
    mc_world_saved_data_free(f->z);
    mc_world_provider_release(f->t);
    mc_world_runtime_source_free(runtime);
    --runtime->classes->live_worlds;
    free(runtime);
    return result(error, MC_WORLD_STORAGE_OK, NULL);
}

mc_world_storage_status mc_world_runtime_retire_world(mc_world_runtime *runtime,
                                                      mc_world_storage_error *error) {
    if (runtime == NULL || runtime->construction.mode != MC_WORLD_RUNTIME_GAME)
        return result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "World retirement arguments");
    if (runtime->active_calls != 0 || runtime->local_leases != 0)
        return result(error, MC_WORLD_STORAGE_BUSY, "World call or lease during retirement");
    if (!mc_world_runtime_entity_list_valid(runtime) ||
        (!mc_world_runtime_player_list_retirement_valid(runtime) ||
         !mc_world_runtime_weather_list_retirement_valid(runtime)))
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World Entity ownership metadata");
    if (!mc_world_nearby_owned_valid(runtime))
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World nearby ownership metadata");
    if (!mc_world_collision_owned_valid(runtime))
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "World collision ownership metadata");
    if (!mc_world_accesses_retirement_valid(runtime))
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "WorldAccess ownership metadata");
    if (runtime->construction.phase == MC_WORLD_RUNTIME_RETIRED &&
        !runtime->construction.contexts_bound && !runtime->construction.cache_registered &&
        mc_world_provider_world(runtime->fields.t) != runtime)
        return result(error, MC_WORLD_STORAGE_OK, NULL);
    mc_world_runtime_fields *f = &runtime->fields;
    mc_world_reference_list *lists[] = {f->C,
                                        runtime->entity_membership.list,
                                        runtime->entity_deferral.list,
                                        f->c,
                                        f->G,
                                        runtime->player_membership.list,
                                        runtime->weather_membership.list,
                                        runtime->collision_boxes.list,
                                        runtime->nearby_entities.list};
    /* C contains typed World-owned records, retired at final native free.
     * Valid typed u occurrences release only at the terminal commit below.
     * Other unimplemented collection descendants still require emptiness. */
    for (size_t i = 1; i < sizeof(lists) / sizeof(lists[0]); ++i) {
        if (lists[i] != NULL && lists[i]->count != 0)
            return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                          "World owned collection retirement");
    }
    /* E/F remain owned and inspectable until final native free, like C.
     * A failed E insertion can leave a legitimate F-only prefix; retirement
     * neither repairs that state nor executes/discards its game work here. */
    if (!active_chunks_owned_valid(runtime))
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "World active Chunk Set ownership metadata");
    if (runtime->construction.contexts_bound || runtime->construction.cache_registered) {
        if (!mc_chunk_cache_validate(runtime->options.cache) ||
            !mc_chunk_pending_validate(runtime->options.pending))
            return result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                          "World retirement cache bindings");
        if (runtime->options.cache->count != 0 || runtime->options.cache->insertion_count != 0 ||
            runtime->options.pending->count != 0)
            return result(error, MC_WORLD_STORAGE_BUSY, "World retirement cache nonempty");
        if (runtime->construction.cache_registered) {
            mc_world_storage_status status = mc_world_store_unregister_cache(
                runtime->options.store, runtime->options.cache, error);
            if (status != MC_WORLD_STORAGE_OK)
                return status;
            runtime->construction.cache_registered = false;
        }
        runtime->construction.contexts_bound = false;
    }
    if (mc_world_provider_world(f->t) == runtime)
        mc_world_provider_unbind(f->t);
    mc_world_entity_index_free(runtime->dedicated.G);
    runtime->dedicated.G = NULL;
    mc_world_accesses_owned_release(runtime);
    runtime->construction.phase = MC_WORLD_RUNTIME_RETIRED;
    return result(error, MC_WORLD_STORAGE_OK, NULL);
}

static bool entity_retirement_other_collections_valid(const mc_world_runtime *world) {
    /* C/E/F/P are concrete typed owners retained until final native free.
     * b/D/d/c/G/R/K/u are admitted separately. No source clear or gameplay
     * callback is performed during this passive ownership check. */
    if (!mc_world_runtime_weather_list_retirement_valid(world))
        return false;
    return active_chunks_owned_valid(world);
}

mc_world_storage_status mc_world_runtime_entity_containers_validate(mc_world_runtime *world,
                                                                    mc_world_storage_error *error) {
    if (world == NULL || world->construction.mode != MC_WORLD_RUNTIME_GAME)
        return result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                      "World entity retirement arguments");
    if (world->active_calls != 0 || world->local_leases != 0)
        return result(error, MC_WORLD_STORAGE_BUSY,
                      "World call or local lease during entity retirement");
    if (!mc_world_runtime_entity_list_valid(world))
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "World entity retirement ownership metadata");
    if (!mc_live_tile_entities_lists_valid(world))
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "World TileEntity retirement ownership metadata");
    if (!mc_world_nearby_owned_valid(world))
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "World nearby retirement ownership metadata");
    if (!mc_world_collision_owned_valid(world))
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "World collision retirement ownership metadata");
    if (!mc_world_accesses_retirement_valid(world))
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "WorldAccess retirement ownership metadata");
    if (!mc_world_runtime_player_list_retirement_valid(world) ||
        !entity_retirement_other_collections_valid(world))
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "World entity retirement untyped collections");
    mc_world_store *store = world->fields.w;
    if (store != NULL) {
        const mc_world_storage_status status =
            mc_world_store_entity_containers_validate(store, world, error);
        if (status != MC_WORLD_STORAGE_OK)
            return status;
    }
    return result(error, MC_WORLD_STORAGE_OK, NULL);
}

mc_world_storage_status mc_world_runtime_retire_entity_containers(mc_world_runtime *world,
                                                                  mc_world_storage_error *error) {
    const mc_world_storage_status admitted =
        mc_world_runtime_entity_containers_validate(world, error);
    if (admitted != MC_WORLD_STORAGE_OK)
        return admitted;
    mc_world_store *store = world->fields.w;
    /* COMMIT: no allocation, callback, fallible admission or source gameplay
     * call may be introduced below. A later full-World/cache admission belongs
     * to a separate API; this operation must not clear then report BUSY.
     * The latch is terminal: central source entry/acquire/lifecycle, constructor,
     * default Random creation/new bindings and context getters must reject it.
     * Do not save or resume a World after discarding Entity or TileEntity state. */
    world->entity_containers_retired = true;
    world->tile_entity_containers_retired = true;
    world->construction.phase = MC_WORLD_RUNTIME_RETIRED;
    if (store != NULL)
        mc_world_store_entity_containers_release(store, world);
    mc_world_runtime_entity_list_release(world);
    mc_live_tile_entities_lists_release(world);
    mc_world_nearby_owned_release(world);
    mc_world_collision_owned_release(world);
    mc_world_accesses_owned_release(world);
    mc_world_entity_index_free(world->dedicated.G);
    world->dedicated.G = NULL;
    return result(error, MC_WORLD_STORAGE_OK, NULL);
}
