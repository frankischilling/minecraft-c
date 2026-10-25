#include "mc/world/runtime/propagation.h"

#include "../../block/registry/runtime_internal.h"
#include "../../platform/text/stdout.h"
#include "../chunk/object_state_internal.h"
#include "../storage/objects_internal.h"
#include "chunk_provider_internal.h"
#include "construction_internal.h"
#include "lighting_internal.h"
#include "mc/core/java/java.h"
#include "runtime_internal.h"
#include "world_access_internal.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

typedef struct light_chunk {
    mc_chunk_state *state;
    mc_world_chunk *object;
    mc_world_store *store;
    bool leased;
} light_chunk;

static mc_registry_error empty_registry(void) {
    return (mc_registry_error){.initializer_pc = -1,
                               .block_id = -1,
                               .constructor_pc = -1,
                               .active_initializer_pc = -1,
                               .item_argument = -1};
}

static void init_error(mc_world_light_operation_error *frame) {
    memset(frame, 0, sizeof(*frame));
    frame->registry = empty_registry();
    frame->query.registry = empty_registry();
    frame->acquisition.registry = empty_registry();
}

static bool failed(mc_world_light_operation_error *frame, mc_world_storage_status status,
                   const char *operation) {
    frame->status = status;
    frame->operation = operation;
    frame->cause = (mc_world_storage_error){status, operation, 0};
    return false;
}

static bool storage_failed(mc_world_light_operation_error *frame, mc_world_storage_error cause) {
    frame->status = cause.status;
    frame->operation = cause.operation;
    frame->cause = cause;
    return false;
}

static bool registry_failed(mc_world_runtime *world, mc_world_light_operation_error *frame,
                            mc_registry_error cause, const char *operation) {
    frame->registry = cause;
    frame->status =
        mc_world_runtime_registry_result(world, cause.status, &cause, operation, &frame->cause);
    frame->operation = frame->cause.operation;
    return false;
}

static bool same_class(mc_world_runtime *world, const mc_world_light_type *type) {
    return type == NULL ||
           (world->classes != NULL && world->classes->light_types_initialized &&
            (type == &world->classes->light_types[0] || type == &world->classes->light_types[1]));
}

static bool admit(mc_world_runtime *world, const mc_world_light_type *type, bool has_out,
                  mc_world_light_operation_error *frame) {
    if (world == NULL || !has_out)
        return failed(frame, MC_WORLD_STORAGE_INVALID_ARGUMENT, "World propagation arguments");
    if (world->construction.mode != MC_WORLD_RUNTIME_GAME ||
        world->construction.phase == MC_WORLD_RUNTIME_RETIRED)
        return failed(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World propagation phase");
    if (!same_class(world, type))
        return failed(frame, MC_WORLD_STORAGE_INVALID_ARGUMENT, "World propagation Light class");
    mc_world_storage_error cause = {0};
    if (mc_world_runtime_call_enter(world, &cause) != MC_WORLD_STORAGE_OK)
        return storage_failed(frame, cause);
    return true;
}

static mc_world_storage_status finish(mc_world_runtime *world,
                                      mc_world_light_operation_error *frame,
                                      mc_world_light_operation_error *error) {
    if (frame->status == MC_WORLD_STORAGE_OK)
        init_error(frame);
    else
        world->last_error = frame->cause;
    mc_world_runtime_call_leave(world);
    if (error != NULL)
        *error = *frame;
    return frame->status;
}

static void release(light_chunk *chunk) {
    if (chunk->leased)
        (void)mc_world_store_release_chunk(chunk->store, chunk->state, NULL);
    *chunk = (light_chunk){0};
}

/* Capture the actual separate getter result and protect a normal Chunk across
 * reached Registry initialization/callback reentry. Do not preflight payloads. */
static bool acquire(mc_world_runtime *world, int32_t chunk_x, int32_t chunk_z, light_chunk *out,
                    mc_world_light_operation_error *frame) {
    frame->provider_bytecode = 6;
    if (world->fields.v == NULL)
        return failed(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "World propagation getter provider");
    mc_chunk_provider_fields provider;
    mc_world_storage_error cause = {0};
    if (mc_chunk_provider_bind_fields(world->fields.v, &provider, &cause) != MC_WORLD_STORAGE_OK)
        return storage_failed(frame, cause);
    if (provider.world != world || provider.cache != world->acquisition.cache ||
        provider.pending != world->acquisition.pending || provider.store != world->options.store)
        return failed(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "World propagation replacement provider binding");
    mc_world_runtime_acquisition_error detail;
    mc_chunk_acquire_status status =
        mc_world_runtime_get_detailed(world, chunk_x, chunk_z, &out->state, &detail);
    frame->acquisition = (mc_world_light_acquisition_error){
        .status = detail.status,
        .cause = detail.cause,
        .registry = detail.registry,
        .tile_entity = detail.tile_entity,
        .batch = detail.batch,
        .activation_bytecode = detail.activation_bytecode,
        .activation_world_bytecode = detail.activation_world_bytecode,
        .load_bytecode = detail.load_bytecode,
        .load_builder_bytecode = detail.load_builder_bytecode,
        .load_entity = detail.load_entity,
        .population = detail.population};
    if (status != MC_CHUNK_ACQUIRE_OK) {
        if (frame->acquisition.cause.status != MC_WORLD_STORAGE_OK)
            return storage_failed(frame, frame->acquisition.cause);
        return failed(frame,
                      status == MC_CHUNK_ACQUIRE_LIMIT || status == MC_CHUNK_ACQUIRE_PARTIAL_LIMIT
                          ? MC_WORLD_STORAGE_LIMIT
                          : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "World propagation getter acquisition");
    }
    frame->provider_bytecode = 0;
    if (out->state == NULL)
        return failed(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "World propagation Chunk receiver");
    if (out->state->blank)
        return true;
    out->store = world->fields.w;
    out->object = mc_world_store_chunk_object(out->store, out->state);
    if (out->object == NULL)
        return failed(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "World propagation concrete Chunk");
    if (mc_world_store_lease_chunk(out->store, out->state, &cause) != MC_WORLD_STORAGE_OK)
        return storage_failed(frame, cause);
    out->leased = true;
    return true;
}

static bool contains(mc_world_runtime *world, int32_t chunk_x, int32_t chunk_z, bool *out,
                     mc_world_light_operation_error *frame) {
    frame->provider_bytecode = 6;
    mc_world_storage_error cause = {0};
    if (mc_world_runtime_chunk_provider_contains(world, chunk_x, chunk_z, out, &cause) !=
        MC_WORLD_STORAGE_OK)
        return storage_failed(frame, cause);
    frame->provider_bytecode = 0;
    return true;
}

static bool outside(int32_t x, int32_t z) {
    return x < -32000000 || x >= 32000000 || z < -32000000 || z > 32000000;
}

static bool type_get(mc_world_runtime *world, mc_light_kind kind, const mc_world_light_type **out,
                     mc_world_light_operation_error *frame) {
    mc_world_storage_error cause = {0};
    if (mc_world_runtime_light_type_get(world->classes, kind, out, &cause) != MC_WORLD_STORAGE_OK)
        return storage_failed(frame, cause);
    return true;
}

static bool sky_exposed(mc_world_runtime *world, int32_t x, int32_t y, int32_t z, bool *out,
                        mc_world_light_operation_error *frame) {
    if (outside(x, z) || y < 0) {
        *out = false;
        return true;
    }
    if (y >= 128) {
        *out = true;
        return true;
    }
    bool present;
    frame->sky_bytecode = 48;
    if (!contains(world, mc_java_i32_shr(x, 4), mc_java_i32_shr(z, 4), &present, frame))
        return false;
    if (!present) {
        *out = false;
        frame->sky_bytecode = 0;
        return true;
    }
    light_chunk chunk = {0};
    frame->sky_bytecode = 63;
    bool ok = acquire(world, mc_java_i32_shr(x, 4), mc_java_i32_shr(z, 4), &chunk, frame);
    bool value = false;
    if (ok && !chunk.state->blank) {
        frame->sky_bytecode = 83;
        frame->chunk_bytecode = 10;
        if (chunk.object->h == NULL)
            ok = failed(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                        "Chunk propagation sky exposure height backing");
        else {
            size_t index = ((size_t)((uint32_t)z & 15u) << 4u) | ((uint32_t)x & 15u);
            value = y >= (int32_t)chunk.object->h[index];
        }
    }
    release(&chunk);
    if (ok) {
        *out = value;
        frame->sky_bytecode = frame->chunk_bytecode = 0;
    }
    return ok;
}

static bool nibble_set(mc_world_chunk_nibble *owner, int32_t x, int32_t y, int32_t z, int32_t value,
                       mc_world_light_operation_error *frame) {
    if (owner == NULL || owner->a == NULL)
        return failed(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "Chunk propagation Nibble backing");
    const int32_t cell = mc_java_i32_shl(x, 11) | mc_java_i32_shl(z, 7) | y;
    const int32_t index = mc_java_i32_shr(cell, 1);
    if (index < 0 || (size_t)index >= owner->length)
        return failed(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Chunk propagation Nibble index");
    const unsigned int shift = ((uint32_t)cell & 1u) * 4u;
    owner->a[(size_t)index] = (uint8_t)(((uint32_t)owner->a[(size_t)index] & ~(15u << shift)) |
                                        (((uint32_t)value & 15u) << shift));
    return true;
}

static bool saved_set(mc_world_runtime *world, const mc_world_light_type *type, int32_t x,
                      int32_t y, int32_t z, int32_t value, mc_world_light_operation_error *frame) {
    if (outside(x, z) || y < 0 || y >= 128)
        return true;
    bool present;
    frame->setter_bytecode = 48;
    if (!contains(world, mc_java_i32_shr(x, 4), mc_java_i32_shr(z, 4), &present, frame))
        return false;
    if (!present) {
        frame->setter_bytecode = 0;
        return true;
    }
    light_chunk chunk = {0};
    frame->setter_bytecode = 63;
    bool ok = acquire(world, mc_java_i32_shr(x, 4), mc_java_i32_shr(z, 4), &chunk, frame);
    if (!ok) {
        release(&chunk);
        return false;
    }
    frame->setter_bytecode = 83;
    if (!chunk.state->blank) {
        chunk.state->dirty = true; /* lm.o2 precedes Light getstatic6. */
        const mc_world_light_type *selected;
        frame->chunk_bytecode = 6;
        ok = type_get(world, MC_LIGHT_SKY, &selected, frame);
        if (ok && type == selected) {
            frame->chunk_bytecode = 22;
            ok = nibble_set(chunk.object->f, (int32_t)((uint32_t)x & 15u), y,
                            (int32_t)((uint32_t)z & 15u), value, frame);
        } else if (ok) {
            frame->chunk_bytecode = 29;
            ok = type_get(world, MC_LIGHT_BLOCK, &selected, frame);
            if (ok && type == selected) {
                frame->chunk_bytecode = 45;
                ok = nibble_set(chunk.object->g, (int32_t)((uint32_t)x & 15u), y,
                                (int32_t)((uint32_t)z & 15u), value, frame);
            }
        }
    }
    if (ok) {
        frame->chunk_bytecode = 0;
        for (size_t index = 0;; ++index) {
            mc_world_access *access = NULL;
            bool has = false;
            mc_world_storage_error cause = {0};
            frame->setter_bytecode = 95;
            if (!mc_world_access_next(world, index, 95, 109, 114, &access, &has,
                                      &frame->world_access_bytecode, &cause)) {
                frame->setter_bytecode = frame->world_access_bytecode;
                ok = storage_failed(frame, cause);
                break;
            }
            if (!has)
                break;
            frame->setter_bytecode = frame->world_access_bytecode = 121;
            mc_world_storage_status status = MC_WORLD_STORAGE_UNSUPPORTED_STATE;
            if (access != NULL && access->callbacks.cell != NULL)
                status = access->callbacks.cell(access->context, world, x, y, z, &cause);
            else
                cause.operation = "World light WorldAccess invocation";
            mc_world_access_release_held(access);
            if (status != MC_WORLD_STORAGE_OK) {
                ok = storage_failed(frame, mc_world_access_callback_error(status, cause));
                break;
            }
        }
    }
    release(&chunk);
    if (ok)
        frame->setter_bytecode = frame->world_access_bytecode = 0;
    return ok;
}

static bool saved(mc_world_runtime *world, const mc_world_light_type *type, int32_t x, int32_t y,
                  int32_t z, int32_t *out, mc_world_light_operation_error *frame) {
    if (mc_world_runtime_saved_light(world, type, x, y, z, out, &frame->query) !=
        MC_WORLD_STORAGE_OK)
        return storage_failed(frame, frame->query.cause);
    return true;
}

static bool emission(mc_world_runtime *world, int32_t id, int32_t *out, unsigned int get_pc,
                     unsigned int at_pc, bool region, mc_world_light_operation_error *frame) {
    unsigned int *pc = region ? &frame->region_bytecode : &frame->point_bytecode;
    *pc = get_pc;
    mc_registry_classes *classes = mc_world_runtime_registry(world);
    mc_registry_error cause = empty_registry();
    if (classes == NULL) {
        cause.status = MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY;
        cause.message = "World light Block.s Registry binding";
        return registry_failed(world, frame, cause, cause.message);
    }
    mc_registry_block_s_view array;
    if (mc_registry_block_s_get(classes, &array, &cause) != MC_REGISTRY_OK)
        return registry_failed(world, frame, cause, "World light Block.s active use");
    *pc = at_pc;
    if (mc_registry_block_s_at(&array, id, out, &cause) != MC_REGISTRY_OK)
        return registry_failed(world, frame, cause, "World light captured Block.s element");
    return true;
}

static bool recheck(mc_world_runtime *world, const mc_world_light_type *type, int32_t x, int32_t y,
                    int32_t z, int32_t hint, mc_world_light_operation_error *frame) {
    frame->point_bytecode = 4;
    mc_world_provider_fields dimension;
    if (mc_world_provider_fields_get(world->fields.t, &dimension) != MC_WORLD_PROVIDER_OK)
        return failed(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "World light point Dimension receiver");
    const mc_world_light_type *selected;
    if (dimension.e) {
        frame->point_bytecode = 11;
        if (!type_get(world, MC_LIGHT_SKY, &selected, frame))
            return false;
        if (type == selected) {
            frame->point_bytecode = 0;
            return true;
        }
    }
    frame->point_bytecode = 23;
    bool present = false;
    if (y >= 0 && y < 128) {
        frame->presence_bytecode = 20;
        if (!contains(world, mc_java_i32_shr(x, 4), mc_java_i32_shr(z, 4), &present, frame))
            return false;
        frame->presence_bytecode = 0;
    }
    if (!present) {
        frame->point_bytecode = 0;
        return true;
    }
    frame->point_bytecode = 31;
    if (!type_get(world, MC_LIGHT_SKY, &selected, frame))
        return false;
    if (type == selected) {
        bool exposed;
        frame->point_bytecode = 42;
        if (!sky_exposed(world, x, y, z, &exposed, frame))
            return false;
        if (exposed)
            hint = 15;
    } else {
        frame->point_bytecode = 56;
        if (!type_get(world, MC_LIGHT_BLOCK, &selected, frame))
            return false;
        if (type == selected) {
            frame->point_bytecode = 67;
            int32_t id;
            mc_world_storage_error cause = {0};
            if (mc_world_runtime_block_get(world, x, y, z, &id, &cause) != MC_WORLD_STORAGE_OK)
                return storage_failed(frame, cause);
            int32_t intrinsic;
            if (!emission(world, id, &intrinsic, 72, 77, false, frame))
                return false;
            if (intrinsic > hint) {
                /* The second getstatic/iaload is source-visible; do not reuse. */
                if (!emission(world, id, &hint, 83, 88, false, frame))
                    return false;
            }
        }
    }
    frame->point_bytecode = 97;
    int32_t current;
    if (!saved(world, type, x, y, z, &current, frame))
        return false;
    if (current != hint) {
        frame->point_bytecode = 115;
        mc_light_bounds bounds = {x, y, z, x, y, z};
        mc_light_submit_result submitted;
        if (mc_world_runtime_light_submit(world, type, &bounds, true, &submitted, &frame->submit) !=
            MC_WORLD_STORAGE_OK)
            return storage_failed(frame, frame->submit.cause);
    }
    frame->point_bytecode = 0;
    return true;
}

static bool step(size_t *remaining, mc_world_light_operation_error *frame) {
    if (*remaining == 0)
        return failed(frame, MC_WORLD_STORAGE_LIMIT, "World light propagation native work budget");
    --*remaining;
    return true;
}

static bool area_present(mc_world_runtime *world, int32_t x, int32_t z, bool *out,
                         mc_world_light_operation_error *frame) {
    const int32_t min_x = mc_java_i32_shr(mc_java_i32_sub(x, 1), 4);
    const int32_t max_x = mc_java_i32_shr(mc_java_i32_add(x, 1), 4);
    const int32_t min_z = mc_java_i32_shr(mc_java_i32_sub(z, 1), 4);
    const int32_t max_z = mc_java_i32_shr(mc_java_i32_add(z, 1), 4);
    /* Y=-1..1 passes the actual area's Y gate. Chunk coordinates from a
     * signed >>4 cannot overflow this inclusive loop's increment. */
    for (int32_t cx = min_x; cx <= max_x; ++cx)
        for (int32_t cz = min_z; cz <= max_z; ++cz) {
            bool present;
            frame->presence_bytecode = 69;
            if (!contains(world, cx, cz, &present, frame))
                return false;
            if (!present) {
                *out = false;
                frame->presence_bytecode = 0;
                return true;
            }
        }
    *out = true;
    frame->presence_bytecode = 0;
    return true;
}

static bool propagate(mc_world_runtime *world, mc_world_light_region *region, size_t *remaining,
                      mc_world_light_operation_error *frame) {
    mc_light_bounds *bounds = &region->fields.bounds;
    int32_t width = mc_java_i32_add(mc_java_i32_sub(bounds->max_x, bounds->min_x), 1);
    int32_t height = mc_java_i32_add(mc_java_i32_sub(bounds->max_y, bounds->min_y), 1);
    int32_t depth = mc_java_i32_add(mc_java_i32_sub(bounds->max_z, bounds->min_z), 1);
    int32_t volume = mc_java_i32_mul(mc_java_i32_mul(width, height), depth);
    if (volume > 32768) {
        frame->region_bytecode = 57;
        static const char diagnostic[] = "Light too large, skipping!";
        mc_text_stdout output;
        bool io_failed = false;
        mc_text_stdout_status status = mc_text_stdout_capture(&output);
        if (status == MC_TEXT_STDOUT_OK)
            status =
                mc_text_stdout_println(&output, diagnostic, sizeof(diagnostic) - 1u, &io_failed);
        if (status != MC_TEXT_STDOUT_OK || io_failed)
            return failed(frame, MC_WORLD_STORAGE_IO_ERROR, "World light region diagnostic");
        frame->region_bytecode = 0;
        return true;
    }
    /* The original reuse flag stays false. Repeat radius presence and the
     * separate getter on every X/Z column, including repeated same chunks. */
    for (int32_t x = bounds->min_x; x <= bounds->max_x; x = mc_java_i32_add(x, 1)) {
        frame->region_bytecode = 79;
        if (!step(remaining, frame))
            return false;
        for (int32_t z = bounds->min_z; z <= bounds->max_z; z = mc_java_i32_add(z, 1)) {
            frame->region_bytecode = 151;
            if (!step(remaining, frame))
                return false;
            bool present;
            if (!area_present(world, x, z, &present, frame))
                return false;
            if (!present)
                continue;
            frame->region_bytecode = 170;
            light_chunk column = {0};
            bool ok = acquire(world, mc_java_i32_shr(x, 4), mc_java_i32_shr(z, 4), &column, frame);
            if (!ok) {
                release(&column);
                return false;
            }
            frame->region_bytecode = 177;
            const bool blank = column.state->blank;
            release(&column);
            if (blank)
                continue;
            if (bounds->min_y < 0)
                bounds->min_y = 0;
            if (bounds->max_y >= 128)
                bounds->max_y = 127;
            for (int32_t y = bounds->min_y; y <= bounds->max_y; y = mc_java_i32_add(y, 1)) {
                frame->region_bytecode = 257;
                if (!step(remaining, frame))
                    return false;
                const mc_world_light_type *type = region->fields.a;
                int32_t current;
                if (!saved(world, type, x, y, z, &current, frame))
                    return false;
                frame->region_bytecode = 272;
                int32_t id;
                mc_world_storage_error cause = {0};
                if (mc_world_runtime_block_get(world, x, y, z, &id, &cause) != MC_WORLD_STORAGE_OK)
                    return storage_failed(frame, cause);
                frame->region_bytecode = 277;
                mc_registry_classes *classes = mc_world_runtime_registry(world);
                mc_registry_error registry = empty_registry();
                if (classes == NULL) {
                    registry.status = MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY;
                    registry.message = "World light Block.q Registry binding";
                    return registry_failed(world, frame, registry, registry.message);
                }
                mc_registry_block_q_view array;
                if (mc_registry_block_q_get(classes, &array, &registry) != MC_REGISTRY_OK)
                    return registry_failed(world, frame, registry,
                                           "World light Block.q active use");
                frame->region_bytecode = 282;
                int32_t opacity;
                if (mc_registry_block_q_at(&array, id, &opacity, &registry) != MC_REGISTRY_OK)
                    return registry_failed(world, frame, registry,
                                           "World light captured Block.q element");
                if (opacity == 0)
                    opacity = 1;
                int32_t intrinsic = 0;
                const mc_world_light_type *selected;
                frame->region_bytecode = 300;
                if (!type_get(world, MC_LIGHT_SKY, &selected, frame))
                    return false;
                if (type == selected) {
                    frame->region_bytecode = 313;
                    bool exposed;
                    if (!sky_exposed(world, x, y, z, &exposed, frame))
                        return false;
                    if (exposed)
                        intrinsic = 15;
                } else {
                    frame->region_bytecode = 330;
                    if (!type_get(world, MC_LIGHT_BLOCK, &selected, frame))
                        return false;
                    if (type == selected && !emission(world, id, &intrinsic, 336, 341, true, frame))
                        return false;
                }
                int32_t target = 0;
                if (opacity < 15 || intrinsic != 0) {
                    const int32_t coordinates[6][3] = {
                        {mc_java_i32_sub(x, 1), y, z}, {mc_java_i32_add(x, 1), y, z},
                        {x, mc_java_i32_sub(y, 1), z}, {x, mc_java_i32_add(y, 1), z},
                        {x, y, mc_java_i32_sub(z, 1)}, {x, y, mc_java_i32_add(z, 1)}};
                    static const unsigned int pcs[6] = {375, 393, 411, 429, 447, 465};
                    for (size_t i = 0; i < 6; ++i) {
                        int32_t sample;
                        frame->region_bytecode = pcs[i];
                        if (!saved(world, type, coordinates[i][0], coordinates[i][1],
                                   coordinates[i][2], &sample, frame))
                            return false;
                        if (i == 0 || sample > target)
                            target = sample;
                    }
                    target = mc_java_i32_sub(target, opacity);
                    if (target < 0)
                        target = 0;
                    if (intrinsic > target)
                        target = intrinsic;
                }
                if (current == target)
                    continue;
                frame->region_bytecode = 575;
                if (!saved_set(world, type, x, y, z, target, frame))
                    return false;
                int32_t hint = mc_java_i32_sub(target, 1);
                if (hint < 0)
                    hint = 0;
                frame->region_bytecode = 607;
                if (!recheck(world, type, mc_java_i32_sub(x, 1), y, z, hint, frame))
                    return false;
                frame->region_bytecode = 625;
                if (!recheck(world, type, x, mc_java_i32_sub(y, 1), z, hint, frame))
                    return false;
                frame->region_bytecode = 643;
                if (!recheck(world, type, x, y, mc_java_i32_sub(z, 1), hint, frame))
                    return false;
                if (mc_java_i32_add(x, 1) >= bounds->max_x) {
                    frame->region_bytecode = 672;
                    if (!recheck(world, type, mc_java_i32_add(x, 1), y, z, hint, frame))
                        return false;
                }
                if (mc_java_i32_add(y, 1) >= bounds->max_y) {
                    frame->region_bytecode = 701;
                    if (!recheck(world, type, x, mc_java_i32_add(y, 1), z, hint, frame))
                        return false;
                }
                if (mc_java_i32_add(z, 1) >= bounds->max_z) {
                    frame->region_bytecode = 730;
                    if (!recheck(world, type, x, y, mc_java_i32_add(z, 1), hint, frame))
                        return false;
                }
            }
        }
    }
    frame->region_bytecode = 0;
    return true;
}

mc_world_storage_status mc_world_runtime_sky_exposed(mc_world_runtime *world, int32_t x, int32_t y,
                                                     int32_t z, bool *out,
                                                     mc_world_light_operation_error *error) {
    mc_world_light_operation_error frame;
    init_error(&frame);
    if (!admit(world, NULL, out != NULL, &frame)) {
        if (error != NULL)
            *error = frame;
        return frame.status;
    }
    bool value;
    if (sky_exposed(world, x, y, z, &value, &frame))
        *out = value;
    return finish(world, &frame, error);
}

mc_world_storage_status mc_world_runtime_saved_light_set(mc_world_runtime *world,
                                                         const mc_world_light_type *type, int32_t x,
                                                         int32_t y, int32_t z, int32_t value,
                                                         mc_world_light_operation_error *error) {
    mc_world_light_operation_error frame;
    init_error(&frame);
    if (!admit(world, type, true, &frame)) {
        if (error != NULL)
            *error = frame;
        return frame.status;
    }
    (void)saved_set(world, type, x, y, z, value, &frame);
    return finish(world, &frame, error);
}

mc_world_storage_status mc_world_runtime_light_recheck(mc_world_runtime *world,
                                                       const mc_world_light_type *type, int32_t x,
                                                       int32_t y, int32_t z, int32_t hint,
                                                       mc_world_light_operation_error *error) {
    mc_world_light_operation_error frame;
    init_error(&frame);
    if (!admit(world, type, true, &frame)) {
        if (error != NULL)
            *error = frame;
        return frame.status;
    }
    (void)recheck(world, type, x, y, z, hint, &frame);
    return finish(world, &frame, error);
}

static bool list(mc_world_runtime *world, mc_world_reference_list **out,
                 mc_world_light_operation_error *frame) {
    mc_world_reference_list *current = world->fields.C;
    if (current == NULL)
        return failed(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World drain C receiver is null");
    if (current->count > current->capacity || current->capacity > (size_t)INT32_MAX ||
        current->capacity > SIZE_MAX / sizeof(*current->items) ||
        (current->capacity != 0 && current->items == NULL))
        return failed(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World drain C metadata");
    *out = current;
    return true;
}

mc_world_storage_status mc_world_runtime_light_drain(mc_world_runtime *world, size_t max_steps,
                                                     bool *out,
                                                     mc_world_light_operation_error *error) {
    mc_world_light_operation_error frame;
    init_error(&frame);
    if (!admit(world, NULL, out != NULL, &frame)) {
        if (error != NULL)
            *error = frame;
        return frame.status;
    }
    if (world->fields.M >= 50) {
        *out = false;
        return finish(world, &frame, error);
    }
    world->fields.M = mc_java_i32_add(world->fields.M, 1);
    bool value = false;
    int32_t budget = 500;
    for (;;) {
        mc_world_reference_list *current;
        frame.drain_bytecode = 29;
        if (!list(world, &current, &frame))
            break;
        if (current->count == 0)
            break;
        budget = mc_java_i32_sub(budget, 1);
        if (budget <= 0) {
            value = true;
            break;
        }
        /* C58 captures remove's receiver before C62/size67. Neither has
         * callbacks, but keep the distinct reached validation and source PCs. */
        mc_world_reference_list *receiver = world->fields.C;
        frame.drain_bytecode = 67;
        if (!list(world, &current, &frame))
            break;
        frame.drain_bytecode = 73;
        const size_t index = current->count - 1u;
        if (receiver == NULL || index >= receiver->count) {
            (void)failed(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World drain C remove index");
            break;
        }
        mc_world_light_region *region = receiver->items[index];
        --receiver->count;
        receiver->items[receiver->count] = NULL;
        frame.drain_bytecode = 82;
        bool ok;
        if (region == NULL)
            ok = failed(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World drain region receiver");
        else
            ok = propagate(world, region, &max_steps, &frame);
        /* The popped record is no longer queue-owned; callback clear/growth
         * only affects remaining records, so this local retires exactly once. */
        if (region != NULL) {
            free(region);
            world->construction.direct_allocation_bytes -= sizeof(*region);
        }
        if (!ok)
            break;
    }
    world->fields.M = mc_java_i32_sub(world->fields.M, 1);
    if (frame.status == MC_WORLD_STORAGE_OK)
        *out = value;
    return finish(world, &frame, error);
}
