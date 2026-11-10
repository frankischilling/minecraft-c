#include "mc/world/runtime/bed_spawn.h"

#include "../../block/registry/registry_internal.h"
#include "../../block/registry/runtime_internal.h"
#include "../../item/render/internal.h"
#include "acquisition_internal.h"
#include "bed_spawn_execution_internal.h"
#include "block_queries_internal.h"
#include "chunk_provider_internal.h"
#include "construction_internal.h"
#include "execution_children_internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/core/java/java.h"
#include "runtime_internal.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

typedef struct bed_call {
    mc_world_runtime *world, *provider_world;
    mc_registry_classes *registry;
    mc_world_bed_spawn_error *error;
    mc_world_solid_support_detail *support_scratch;
    mc_world_bed_spawn_execution *owned;
    bool world_held, provider_held;
} bed_call;

void mc_world_bed_spawn_error_init(mc_world_bed_spawn_error *e) {
    if (e != NULL) {
        memset(e, 0, offsetof(mc_world_bed_spawn_error, child));
        e->native_policy = true;
        e->pass = -1;
    }
}

bool mc_world_bed_spawn_error_retire(mc_world_bed_spawn_error *e) {
    if (e == NULL || e->retained_world_count > 2)
        return false;
    for (size_t i = 0; i < e->retained_world_count; ++i) {
        mc_world_runtime *world = e->retained_worlds[i];
        size_t occurrences = 0;
        for (size_t j = 0; j < e->retained_world_count; ++j)
            if (e->retained_worlds[j] == world)
                ++occurrences;
        if (world == NULL || world->active_calls < occurrences)
            return false;
    }
    while (e->retained_world_count != 0) {
        const size_t index = --e->retained_world_count;
        mc_world_runtime_call_leave(e->retained_worlds[index]);
        e->retained_worlds[index] = NULL;
    }
    mc_world_bed_spawn_error_init(e);
    return true;
}

static void at(bed_call *c, bool helper, unsigned int pc) {
    mc_world_bed_spawn_error *e = c->error;
    e->source_class = helper ? "ve" : "gs";
    e->source_method = helper ? "f(Lfd;IIII)Lbr;" : "a(Lfd;Lbr;)Lbr;";
    e->pc = pc;
    if (helper)
        e->helper_pc = pc;
    else
        e->wrapper_pc = pc;
}

static bool fail(bed_call *c, mc_world_storage_status status, const char *operation) {
    c->error->status = status;
    c->error->operation = operation;
    c->error->cause = (mc_world_storage_error){status, operation, 0};
    return false;
}

static bool null(bed_call *c, const char *operation) {
    c->error->native_policy = false;
    c->error->source_exception = true;
    c->error->exception_class = "java/lang/NullPointerException";
    return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE, operation);
}

static bool native_world(bed_call *c) {
    if (c->world_held || c->world == NULL)
        return true;
    mc_world_runtime *w = c->world;
    if (w->construction.mode != MC_WORLD_RUNTIME_GAME ||
        w->construction.phase == MC_WORLD_RUNTIME_RETIRED || w->classes == NULL ||
        w->classes->side != MC_WORLD_INFO_CLIENT)
        return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Bed spawn actual client GAME World");
    const mc_world_storage_status status = mc_world_runtime_call_enter(w, &c->error->cause);
    if (status != MC_WORLD_STORAGE_OK) {
        c->error->status = status;
        c->error->operation = c->error->cause.operation;
        return false;
    }
    c->world_held = true;
    return true;
}

static mc_world_storage_status registry_status(mc_registry_status status) {
    return status == MC_REGISTRY_LIMIT               ? MC_WORLD_STORAGE_LIMIT
           : status == MC_REGISTRY_ALLOCATION_FAILED ? MC_WORLD_STORAGE_ALLOCATION_FAILED
           : status == MC_REGISTRY_BUSY              ? MC_WORLD_STORAGE_BUSY
                                                     : MC_WORLD_STORAGE_UNSUPPORTED_STATE;
}

static void registry_classification(bed_call *c, const mc_registry_error *error) {
    mc_item_render_throwable throwable;
    mc_item_render_registry_classification(c->registry, error, &throwable,
                                           &c->error->exception_class);
    c->error->source_exception = throwable == MC_ITEM_RENDER_SOURCE_EXCEPTION;
    c->error->source_error = throwable == MC_ITEM_RENDER_SOURCE_ERROR;
    c->error->native_policy = throwable == MC_ITEM_RENDER_NO_THROWABLE;
}

static bool registry_result(bed_call *c, mc_registry_status status) {
    if (status == MC_REGISTRY_OK) {
        c->error->child_kind = MC_WORLD_BED_SPAWN_CHILD_NONE;
        return true;
    }
    registry_classification(c, &c->error->child.registry);
    return fail(c, registry_status(status), c->error->child.registry.message);
}

static bool registry_owner(bed_call *c) {
    if (c->registry == NULL || c->registry->options.side != MC_REGISTRY_CLIENT ||
        (c->owned == NULL && c->world_held && c->world->classes->registry != c->registry))
        return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                    "Bed spawn actual code Registry namespace");
    return true;
}

static mc_world_bed_spawn_event *event(bed_call *c, mc_world_bed_spawn_event_kind kind,
                                       unsigned int pc, int32_t x, int32_t y, int32_t z) {
    mc_world_bed_spawn_error *e = c->error;
    if (e->event_count != SIZE_MAX)
        ++e->event_count;
    if (e->retained_events == MC_WORLD_BED_SPAWN_RETAINED_EVENTS) {
        e->event_overflow = true;
        return NULL;
    }
    mc_world_bed_spawn_event *record = &e->events[e->retained_events++];
    *record = (mc_world_bed_spawn_event){.kind = kind, .pc = pc, .x = x, .y = y, .z = z};
    return record;
}

static void completed(mc_world_bed_spawn_event *record, int32_t result) {
    if (record != NULL) {
        record->completed = true;
        record->result = result;
    }
}

static bool world_result(bed_call *c, mc_world_storage_status status) {
    if (status == MC_WORLD_STORAGE_OK) {
        c->error->child_kind = MC_WORLD_BED_SPAWN_CHILD_NONE;
        return true;
    }
    c->error->status = status;
    c->error->operation = c->error->cause.operation;
    return false;
}

static bool bed_quiet(const mc_world_runtime_execution_result *r) {
    if (r->ownership_status != MC_WORLD_STORAGE_OK || r->selection_ready || r->root_pending)
        return false;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_LOADERS; ++i)
        if (r->pending[i].used)
            return false;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_WORLDS; ++i)
        if (r->closes[i].used)
            return false;
    return true;
}

static bool bed_next(bed_call *c) {
    if (c->owned == NULL || bed_quiet(c->owned->owned))
        return true;
    c->owned->native_stopped = true;
    mc_world_runtime_execution_result_note(c->owned->owned, MC_WORLD_STORAGE_BUSY,
                                           "Bed lower query result is not reusable",
                                           c->owned->receiver_site);
    return false;
}

static bool query(bed_call *c, mc_world_bed_spawn_event_kind kind, unsigned int pc, int32_t x,
                  int32_t y, int32_t z, int32_t *out) {
    mc_world_bed_spawn_event *record = event(c, kind, pc, x, y, z);
    if (c->world == NULL)
        return null(c, "Bed spawn null World query receiver");
    c->error->child_kind = MC_WORLD_BED_SPAWN_CHILD_ACQUISITION;
    mc_world_storage_status status;
    if (c->owned != NULL) {
        if (!bed_next(c))
            return false;
        mc_world_block_execution *frame = &c->owned->scalar;
        mc_world_block_execution_init(frame, c->owned->execution, c->owned->code,
                                      c->owned->receiver_site, c->owned->owned);
        status = MC_WORLD_STORAGE_OK;
        const bool continued = kind == MC_WORLD_BED_SPAWN_METADATA
                                   ? mc_world_runtime_metadata_get_in_execution(
                                         frame, c->world, x, y, z, out, &c->error->cause, &status)
                                   : mc_world_runtime_block_get_in_execution(
                                         frame, c->world, x, y, z, out, &c->error->cause, &status);
        if (c->owned->owned->ownership_status != MC_WORLD_STORAGE_OK) {
            c->owned->native_stopped = true;
            c->error->status = status; /* Actual lower C status, not Source classification. */
            return false;
        }
        if (!continued) {
            if (frame->get_returned && frame->get_status != MC_CHUNK_ACQUIRE_OK)
                c->error->child.acquisition = frame->acquisition;
            else
                c->error->child_kind = MC_WORLD_BED_SPAWN_CHILD_NONE;
            if (frame->source.source_exception) {
                c->error->source_exception = true;
                c->error->native_policy = false;
                c->error->exception_class = frame->source.exception_class;
            }
            (void)world_result(c, status);
            return false;
        }
    } else {
        status = kind == MC_WORLD_BED_SPAWN_METADATA
                     ? mc_world_runtime_metadata_get_detailed(
                           c->world, x, y, z, out, &c->error->cause, &c->error->child.acquisition)
                     : mc_world_runtime_block_get_detailed(c->world, x, y, z, out, &c->error->cause,
                                                           &c->error->child.acquisition);
    }
    if (!world_result(c, status))
        return false;
    if (kind == MC_WORLD_BED_SPAWN_AIR)
        *out = *out == 0;
    completed(record, *out);
    return true;
}

static bool support(bed_call *c, int32_t x, int32_t y, int32_t z, bool *out) {
    mc_world_bed_spawn_event *record = event(c, MC_WORLD_BED_SPAWN_SUPPORT, 101, x, y, z);
    if (c->world == NULL)
        return null(c, "Bed spawn null World support receiver");
    if (c->owned != NULL) {
        if (!bed_next(c))
            return false;
        mc_world_solid_support_execution *frame = &c->owned->support;
        mc_world_solid_support_execution_init(frame, c->owned->execution, c->owned->code_classes,
                                              c->owned->code, c->owned->receiver_site,
                                              c->owned->owned);
        mc_world_storage_status status = MC_WORLD_STORAGE_OK;
        const bool continued = mc_world_solid_support_detailed_held_in_execution(
            frame, c->world, x, y, z, out, &status);
        if (c->owned->owned->ownership_status != MC_WORLD_STORAGE_OK) {
            c->owned->native_stopped = true;
            c->error->status = status; /* Inspect owned before interpreting this number. */
            return false;
        }
        if (!continued) {
            c->error->child_kind = MC_WORLD_BED_SPAWN_CHILD_SUPPORT;
            c->error->child.support.error = frame->detail.error;
            c->error->child.support.access = frame->detail.access;
            c->error->cause = frame->detail.error.cause;
            if (frame->detail.error.registry.status != MC_REGISTRY_OK) {
                mc_item_render_throwable throwable;
                mc_registry_classes *classes = c->owned->code_classes->registry;
                mc_item_render_registry_classification(classes, &frame->detail.error.registry,
                                                       &throwable, &c->error->exception_class);
                c->error->source_exception = throwable == MC_ITEM_RENDER_SOURCE_EXCEPTION;
                c->error->source_error = throwable == MC_ITEM_RENDER_SOURCE_ERROR;
                c->error->native_policy = throwable == MC_ITEM_RENDER_NO_THROWABLE;
            }
            (void)world_result(c, status);
            return false;
        }
        c->error->child_kind = MC_WORLD_BED_SPAWN_CHILD_NONE;
        completed(record, *out);
        return true;
    }
    if (c->support_scratch == NULL)
        c->support_scratch = malloc(sizeof(*c->support_scratch));
    mc_world_solid_support_detail *detail = c->support_scratch;
    if (detail == NULL)
        return fail(c, MC_WORLD_STORAGE_ALLOCATION_FAILED, "Bed support native child scratch");
    const mc_world_storage_status status =
        mc_world_solid_support_detailed_held(c->world, x, y, z, out, detail);
    if (status != MC_WORLD_STORAGE_OK) {
        c->error->child_kind = MC_WORLD_BED_SPAWN_CHILD_SUPPORT;
        c->error->child.support.error = detail->error;
        c->error->child.support.access = detail->access;
        c->error->cause = detail->error.cause;
        if (detail->error.registry.status != MC_REGISTRY_OK)
            registry_classification(c, &detail->error.registry);
    }
    if (!world_result(c, status))
        return false;
    completed(record, *out);
    return true;
}

static bool direction(bed_call *c, int32_t axis, int32_t *out) {
    const unsigned int get_pc = axis == 0 ? 26u : 43u;
    const unsigned int row_pc = axis == 0 ? 31u : 48u;
    const unsigned int value_pc = axis == 0 ? 33u : 50u;
    mc_registry_bed_table_view table;
    mc_registry_bed_row_view row;
    at(c, true, get_pc);
    c->error->child_kind = MC_WORLD_BED_SPAWN_CHILD_REGISTRY;
    if (!registry_result(c,
                         mc_registry_bed_table_get(c->registry, &table, &c->error->child.registry)))
        return false;
    c->error->table_identity = (uintptr_t)table.identity;
    at(c, true, row_pc);
    c->error->child_kind = MC_WORLD_BED_SPAWN_CHILD_REGISTRY;
    if (!registry_result(c, mc_registry_bed_table_at(&table, c->error->orientation, &row,
                                                     &c->error->child.registry)))
        return false;
    c->error->row_identity = (uintptr_t)row.identity;
    at(c, true, value_pc);
    c->error->child_kind = MC_WORLD_BED_SPAWN_CHILD_REGISTRY;
    return registry_result(c, mc_registry_bed_row_at(&row, axis, out, &c->error->child.registry));
}

static bool work(bed_call *c, unsigned int pc) {
    at(c, true, pc);
    if (c->error->loop_work == c->error->max_loop_work)
        return fail(c, MC_WORLD_STORAGE_LIMIT, "Bed search pending source work ceiling");
    ++c->error->loop_work;
    return true;
}

static bool exit_body(bed_call *c, int32_t x, int32_t y, int32_t z, int32_t skip,
                      mc_player_coordinates **out) {
    at(c, true, 0);
    if (!registry_owner(c))
        return false;
    c->error->child_kind = MC_WORLD_BED_SPAWN_CHILD_REGISTRY;
    if (!registry_result(c, mc_registry_bed_active_use(c->registry, &c->error->child.registry)))
        return false;
    c->error->helper_entered = true;
    if (!native_world(c))
        return false;
    if (!registry_owner(c))
        return false;
    at(c, true, 4);
    if (!query(c, MC_WORLD_BED_SPAWN_METADATA, 4, x, y, z, &c->error->metadata))
        return false;
    c->error->orientation = (int32_t)((uint32_t)c->error->metadata & 3u);
    c->error->remaining_skip = skip;
    for (int32_t pass = 0; pass <= 1; pass = mc_java_i32_add(pass, 1)) {
        c->error->pass = pass;
        int32_t dx, dz;
        if (!direction(c, 0, &dx))
            return false;
        const int32_t min_x = mc_java_i32_sub(mc_java_i32_sub(x, mc_java_i32_mul(dx, pass)), 1);
        if (!direction(c, 1, &dz))
            return false;
        const int32_t min_z = mc_java_i32_sub(mc_java_i32_sub(z, mc_java_i32_mul(dz, pass)), 1);
        const int32_t max_x = mc_java_i32_add(min_x, 2), max_z = mc_java_i32_add(min_z, 2);
        for (int32_t cx = min_x; cx <= max_x; cx = mc_java_i32_add(cx, 1)) {
            c->error->candidate_x = cx;
            if (!work(c, 75))
                return false;
            for (int32_t cz = min_z; cz <= max_z; cz = mc_java_i32_add(cz, 1)) {
                c->error->candidate_z = cz;
                if (!work(c, 93))
                    return false;
                ++c->error->candidate_visits;
                bool solid;
                at(c, true, 101);
                if (!support(c, cx, mc_java_i32_sub(y, 1), cz, &solid))
                    return false;
                if (!solid)
                    continue;
                int32_t air;
                at(c, true, 113);
                if (!query(c, MC_WORLD_BED_SPAWN_AIR, 113, cx, y, cz, &air))
                    return false;
                if (!air)
                    continue;
                at(c, true, 127);
                if (!query(c, MC_WORLD_BED_SPAWN_AIR, 127, cx, mc_java_i32_add(y, 1), cz, &air))
                    return false;
                if (!air)
                    continue;
                if (c->error->remaining_skip > 0) {
                    c->error->remaining_skip = mc_java_i32_sub(c->error->remaining_skip, 1);
                    continue;
                }
                at(c, true, 144);
                mc_player_coordinates *position = malloc(sizeof(*position));
                if (position == NULL)
                    return fail(c, MC_WORLD_STORAGE_ALLOCATION_FAILED, "Bed exit NEW br owner");
                *position = (mc_player_coordinates){cx, y, cz};
                *out = position;
                at(c, true, 156);
                c->error->result_present = true;
                return true;
            }
        }
    }
    at(c, true, 176);
    *out = NULL;
    return true;
}

static bool captured_acquire(bed_call *c, mc_chunk_provider *provider, int32_t x, int32_t z,
                             unsigned int pc) {
    at(c, false, pc);
    mc_world_bed_spawn_event *record = event(c, MC_WORLD_BED_SPAWN_CAPTURED_ACQUIRE, pc, x, 0, z);
    if (provider == NULL)
        return null(c, "Bed validation null captured provider receiver");
    if (!c->provider_held) {
        const mc_world_storage_status status =
            mc_chunk_provider_acquire_owner(provider, &c->provider_world, &c->error->cause);
        if (!world_result(c, status))
            return false;
        if (!world_result(c, mc_world_runtime_call_enter(c->provider_world, &c->error->cause)))
            return false;
        c->provider_held = true;
    }
    ++c->error->provider_calls;
    c->error->child_kind = MC_WORLD_BED_SPAWN_CHILD_ACQUISITION;
    mc_chunk_state *ignored = NULL;
    const mc_chunk_acquire_status status = mc_world_runtime_acquire_captured_provider(
        provider, x, z, &ignored, &c->error->child.acquisition);
    if (status != MC_CHUNK_ACQUIRE_OK) {
        c->error->cause = c->error->child.acquisition.cause;
        if (c->error->cause.status != MC_WORLD_STORAGE_OK)
            return world_result(c, c->error->cause.status);
        if (status == MC_CHUNK_ACQUIRE_LIMIT || status == MC_CHUNK_ACQUIRE_PARTIAL_LIMIT)
            return fail(c, MC_WORLD_STORAGE_LIMIT, "Bed captured provider acquisition limit");
        return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Bed captured provider acquisition");
    }
    c->error->child_kind = MC_WORLD_BED_SPAWN_CHILD_NONE;
    completed(record, 0);
    return true;
}

static bool validate_body(bed_call *c, const mc_player_coordinates *bed,
                          mc_player_coordinates **out) {
    c->error->wrapper_entered = true;
    at(c, false, 1);
    if (c->world == NULL)
        return null(c, "Bed validation null World.w receiver");
    mc_chunk_provider *provider = c->world->fields.v;
    c->error->provider_identity = (uintptr_t)provider;
    if (bed == NULL) {
        at(c, false, 7);
        return null(c, "Bed validation null br.x receiver");
    }
    static const unsigned int pcs[] = {22, 45, 68, 91};
    for (size_t i = 0; i < 4; ++i) {
        const int32_t cx = mc_java_i32_shr(
            (i & 1u) == 0 ? mc_java_i32_sub(bed->x, 3) : mc_java_i32_add(bed->x, 3), 4);
        const int32_t cz =
            mc_java_i32_shr(i < 2 ? mc_java_i32_sub(bed->z, 3) : mc_java_i32_add(bed->z, 3), 4);
        if (!captured_acquire(c, provider, cx, cz, pcs[i]))
            return false;
    }
    int32_t id;
    at(c, false, 110);
    if (!query(c, MC_WORLD_BED_SPAWN_BLOCK, 110, bed->x, bed->y, bed->z, &id))
        return false;
    at(c, false, 113);
    if (!registry_owner(c))
        return false;
    c->error->child_kind = MC_WORLD_BED_SPAWN_CHILD_REGISTRY;
    const mc_block *block = NULL;
    if (!registry_result(
            c, mc_registry_block_static_get(c->registry, "T", &block, &c->error->child.registry)))
        return false;
    at(c, false, 116);
    if (block == NULL)
        return null(c, "Bed validation null Block.T ID receiver");
    if (id != block->fields.bn) {
        at(c, false, 123);
        *out = NULL;
        return true;
    }
    at(c, false, 138);
    if (!exit_body(c, bed->x, bed->y, bed->z, 0, out))
        return false;
    at(c, false, 143);
    return true;
}

static mc_world_storage_status run(mc_world_runtime *world, mc_registry_classes *registry,
                                   const mc_player_coordinates *bed, bool wrapper, int32_t x,
                                   int32_t y, int32_t z, int32_t skip,
                                   const mc_world_bed_spawn_limits *limits,
                                   mc_player_coordinates **out, mc_world_bed_spawn_error *output,
                                   mc_world_bed_spawn_execution *owned) {
    mc_world_bed_spawn_error *e = output == NULL ? malloc(sizeof(*e)) : output;
    if (e == NULL)
        return MC_WORLD_STORAGE_ALLOCATION_FAILED;
    mc_world_bed_spawn_error_init(e);
    e->max_loop_work =
        limits == NULL ? MC_WORLD_BED_SPAWN_DEFAULT_LOOP_WORK : limits->max_loop_work;
    bed_call c = {.world = world, .registry = registry, .error = e, .owned = owned};
    bool done = false;
    if (out == NULL)
        (void)fail(&c, MC_WORLD_STORAGE_INVALID_ARGUMENT, "Bed spawn output slot");
    else if (!wrapper)
        done = exit_body(&c, x, y, z, skip, out);
    else if (native_world(&c))
        done = validate_body(&c, bed, out);
    if (done)
        e->returned = true;
    if (output != NULL && (e->status != MC_WORLD_STORAGE_OK || (owned != NULL && !done))) {
        if (c.provider_held)
            e->retained_worlds[e->retained_world_count++] = c.provider_world;
        if (c.world_held)
            e->retained_worlds[e->retained_world_count++] = c.world;
    } else {
        if (c.provider_held)
            mc_world_runtime_call_leave(c.provider_world);
        if (c.world_held)
            mc_world_runtime_call_leave(c.world);
    }
    if (owned != NULL)
        owned->completed = done && e->status == MC_WORLD_STORAGE_OK && !owned->native_stopped &&
                           bed_quiet(owned->owned);
    free(c.support_scratch);
    const mc_world_storage_status status = e->status;
    if (output == NULL)
        free(e);
    return status;
}

mc_world_storage_status
mc_world_bed_spawn_exit(mc_world_runtime *world, mc_registry_classes *registry, int32_t x,
                        int32_t y, int32_t z, int32_t skip, const mc_world_bed_spawn_limits *limits,
                        mc_player_coordinates **out, mc_world_bed_spawn_error *error) {
    return run(world, registry, NULL, false, x, y, z, skip, limits, out, error, NULL);
}

mc_world_storage_status mc_world_bed_spawn_validate(mc_world_runtime *world,
                                                    mc_registry_classes *registry,
                                                    const mc_player_coordinates *bed,
                                                    const mc_world_bed_spawn_limits *limits,
                                                    mc_player_coordinates **out,
                                                    mc_world_bed_spawn_error *error) {
    return run(world, registry, bed, true, 0, 0, 0, 0, limits, out, error, NULL);
}

void mc_world_bed_spawn_execution_init(mc_world_bed_spawn_execution *frame,
                                       mc_entity_tick_execution *execution,
                                       mc_world_runtime_classes *code_classes,
                                       mc_world_runtime_execution_code code,
                                       mc_entity_tick_execution_site site,
                                       mc_world_runtime_execution_result *owned) {
    if (frame != NULL)
        *frame = (mc_world_bed_spawn_execution){.execution = execution,
                                                .code_classes = code_classes,
                                                .code = code,
                                                .receiver_site = site,
                                                .owned = owned};
    mc_world_runtime_execution_result_init(owned);
}

bool mc_world_bed_spawn_exit_in_execution(
    mc_world_bed_spawn_execution *frame, mc_world_runtime *world, mc_registry_classes *registry,
    int32_t x, int32_t y, int32_t z, int32_t skip, const mc_world_bed_spawn_limits *limits,
    mc_player_coordinates **out, mc_world_bed_spawn_error *error, mc_world_storage_status *status) {
    if (frame == NULL)
        return false;
    if (frame->execution == NULL || frame->execution->identity != frame->execution ||
        frame->owned == NULL || out == NULL || error == NULL || status == NULL || frame->entered) {
        mc_world_runtime_execution_result_note(frame->owned, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                                               "Bed execution/fresh outputs required",
                                               frame->receiver_site);
        return false;
    }
    if (!bed_quiet(frame->owned)) {
        mc_world_runtime_execution_result_note(frame->owned, MC_WORLD_STORAGE_BUSY,
                                               "Bed fresh result required", frame->receiver_site);
        return false;
    }
    mc_world_access_failure_lease *unused = NULL;
    const mc_world_storage_status admitted =
        mc_world_execution_child_lease(frame->execution, world, &unused);
    if (admitted != MC_WORLD_STORAGE_OK) {
        mc_world_runtime_execution_result_note(
            frame->owned, admitted, "Bed evaluated World ownership", frame->receiver_site);
        return false;
    }
    frame->entered = true;
    *status = run(world, registry, NULL, false, x, y, z, skip, limits, out, error, frame);
    return frame->completed;
}

void mc_world_bed_spawn_position_free(mc_player_coordinates *position) {
    free(position);
}
