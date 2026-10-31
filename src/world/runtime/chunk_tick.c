#include "mc/world/runtime/chunk_tick.h"

#include "../chunk/object_state_internal.h"
#include "../storage/objects_internal.h"
#include "construction_internal.h"
#include "mc/core/java/java.h"
#include "runtime_internal.h"
#include <stdlib.h>

struct mc_world_chunk_tick_capture {
    mc_world_chunk_tick_state state;
    mc_world_chunk_tick_error failure;
    mc_world_chunk *object;
    mc_chunk_references references;
};

static mc_world_chunk_tick_site location(const char *cl, const char *method, const char *descriptor,
                                         int32_t pc) {
    return (mc_world_chunk_tick_site){cl, method, descriptor, pc};
}

static mc_world_storage_status fail(mc_world_chunk_tick_error *e, mc_world_storage_status status,
                                    const char *operation, const char *exception) {
    e->status = status;
    e->operation = operation;
    e->native_policy = exception == NULL;
    e->source_exception = exception != NULL;
    e->exception_class = exception;
    e->cause = (mc_world_storage_error){status, operation, 0};
    return status;
}

static mc_world_storage_status finish(mc_world_chunk_tick_capture *c,
                                      mc_world_chunk_tick_error *frame,
                                      mc_world_chunk_tick_error *out) {
    c->state.site = frame->site;
    c->state.child = frame->child;
    c->state.packed_index = frame->packed_index;
    c->state.array_index = frame->array_index;
    c->state.active = false;
    if (frame->status != MC_WORLD_STORAGE_OK) {
        c->failure = *frame;
        if (c->state.world_held)
            c->state.world->last_error = frame->cause;
    }
    if (out != NULL)
        *out = *frame;
    return frame->status;
}

mc_world_storage_status mc_world_chunk_tick_capture_begin(mc_world_runtime *world,
                                                          mc_chunk_state *captured,
                                                          mc_world_chunk_tick_capture **out,
                                                          mc_world_chunk_tick_error *error) {
    mc_world_chunk_tick_error frame = {.site = {"fd", "n", "()V", -1}};
    if (world == NULL || captured == NULL || out == NULL) {
        (void)fail(&frame, MC_WORLD_STORAGE_INVALID_ARGUMENT, "Chunk capture arguments", NULL);
        goto unowned;
    }
    if (world->construction.mode != MC_WORLD_RUNTIME_GAME ||
        world->construction.phase == MC_WORLD_RUNTIME_RETIRED || world->classes == NULL ||
        world->classes->side != MC_WORLD_INFO_CLIENT || !world->construction.contexts_bound ||
        world->options.store == NULL) {
        (void)fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Chunk capture client World context",
                   NULL);
        goto unowned;
    }
    const bool blank = captured == &world->fallback;
    mc_world_chunk *object = NULL;
    if (blank) {
        if (!captured->blank || world->acquisition.fallback != captured) {
            (void)fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                       "Chunk capture installed Blank identity", NULL);
            goto unowned;
        }
    } else {
        object = mc_world_store_chunk_object(world->options.store, captured);
        if (object == NULL || object->state != captured || captured->blank ||
            object->source_disabled) {
            (void)fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                       "Chunk capture concrete store identity", NULL);
            goto unowned;
        }
    }
    if (world->references.owner != world || world->references.retain == NULL ||
        world->references.release == NULL) {
        (void)fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                   "Chunk capture installed reference hooks", NULL);
        goto unowned;
    }
    mc_world_chunk_tick_capture *c = calloc(1, sizeof(*c));
    if (c == NULL) {
        (void)fail(&frame, MC_WORLD_STORAGE_ALLOCATION_FAILED, "Chunk capture native handle", NULL);
        goto unowned;
    }
    c->state = (mc_world_chunk_tick_state){.world = world,
                                           .captured = captured,
                                           .blank = blank,
                                           .active = true,
                                           .chunk_x = captured->x,
                                           .chunk_z = captured->z,
                                           .failure = &c->failure};
    c->object = object;
    c->references = world->references;
    *out = c;
    const mc_world_storage_status entered = mc_world_runtime_call_enter(world, &frame.cause);
    if (entered != MC_WORLD_STORAGE_OK) {
        const mc_world_storage_error cause = frame.cause;
        (void)fail(&frame, entered, cause.operation, NULL);
        frame.cause = cause;
        return finish(c, &frame, error);
    }
    c->state.world_held = true;
    if (!c->references.retain(c->references.owner, captured)) {
        const mc_world_storage_error cause = world->last_error;
        (void)fail(
            &frame,
            cause.status == MC_WORLD_STORAGE_OK ? MC_WORLD_STORAGE_UNSUPPORTED_STATE : cause.status,
            cause.operation == NULL ? "Chunk capture reference retain" : cause.operation, NULL);
        if (cause.status != MC_WORLD_STORAGE_OK)
            frame.cause = cause;
        return finish(c, &frame, error);
    }
    c->state.chunk_held = true;
    c->state.ready = true;
    return finish(c, &frame, error);
unowned:
    if (error != NULL)
        *error = frame;
    return frame.status;
}

bool mc_world_chunk_tick_capture_inspect(const mc_world_chunk_tick_capture *c,
                                         mc_world_chunk_tick_state *out) {
    if (c == NULL || out == NULL)
        return false;
    *out = c->state;
    return true;
}

static bool enter(mc_world_chunk_tick_capture *c, mc_world_chunk_tick_operation operation,
                  const int32_t *out, mc_world_chunk_tick_error *e) {
    *e = (mc_world_chunk_tick_error){.site = {"fd", "n", "()V", -1}};
    if (c == NULL || out == NULL) {
        (void)fail(e, MC_WORLD_STORAGE_INVALID_ARGUMENT, "Captured Chunk read arguments", NULL);
        return false;
    }
    if (c->state.active) {
        (void)fail(e, MC_WORLD_STORAGE_BUSY, "Captured Chunk active read", NULL);
        return false;
    }
    if (!c->state.ready || !c->state.world_held || !c->state.chunk_held) {
        (void)fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Captured Chunk incomplete ownership",
                   NULL);
        return false;
    }
    if (c->state.reads == SIZE_MAX) {
        (void)fail(e, MC_WORLD_STORAGE_LIMIT, "Captured Chunk native read counter", NULL);
        return false;
    }
    c->state.active = true;
    c->state.operation = operation;
    ++c->state.reads;
    return true;
}

static mc_world_storage_status rejected(const mc_world_chunk_tick_error *frame,
                                        mc_world_chunk_tick_error *out) {
    if (out != NULL)
        *out = *frame;
    return frame->status;
}

static int32_t packed(int32_t x, int32_t y, int32_t z) {
    return mc_java_i32_shl(x, 11) | mc_java_i32_shl(z, 7) | y;
}

static bool bytes(mc_world_chunk_tick_capture *c, int32_t x, int32_t y, int32_t z, int32_t *value,
                  bool raw, mc_world_chunk_tick_error *e) {
    mc_chunk_data *data = c->state.blank ? c->state.captured->data : c->object->data;
    e->site = raw ? location("fd", "n", "()V", 927) : location("lm", "a", "(III)I", 1);
    if (data == NULL) {
        (void)fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Captured Chunk native data owner", NULL);
        return false;
    }
    /* Read the actual b reference before computing the source packed index.
     * A Java NULL array fails at baload, after that index computation. */
    const uint8_t *array = data->blocks;
    e->packed_index = packed(x, y, z);
    e->array_index = e->packed_index;
    e->site.pc = raw ? 944 : 15;
    if (array == NULL) {
        (void)fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Captured Chunk null block byte array",
                   "java/lang/NullPointerException");
        return false;
    }
    if (e->array_index < 0 || (size_t)e->array_index >= data->blocks_capacity) {
        (void)fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Captured Chunk block byte index",
                   "java/lang/ArrayIndexOutOfBoundsException");
        return false;
    }
    *value = (int32_t)array[(size_t)e->array_index];
    e->site.pc = raw ? 948 : 20;
    return true;
}

static bool nibble(const mc_world_chunk_nibble *n, int32_t x, int32_t y, int32_t z, int32_t *value,
                   mc_world_chunk_tick_error *e) {
    if (n == NULL) {
        (void)fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Captured Chunk null Nibble receiver",
                   "java/lang/NullPointerException");
        return false;
    }
    e->packed_index = packed(x, y, z);
    e->array_index = mc_java_i32_shr(e->packed_index, 1);
    const bool odd = ((uint32_t)e->packed_index & 1u) != 0;
    e->child = location("wi", "a", "(III)I", odd ? 47 : 36);
    const uint8_t *array = n->a;
    if (array == NULL) {
        (void)fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Captured Nibble null byte array",
                   "java/lang/NullPointerException");
        return false;
    }
    if (e->array_index < 0 || (size_t)e->array_index >= n->length) {
        (void)fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Captured Nibble byte index",
                   "java/lang/ArrayIndexOutOfBoundsException");
        return false;
    }
    *value = ((int32_t)array[(size_t)e->array_index] >> (odd ? 4u : 0u)) & 15;
    e->child.pc = odd ? 53 : 40;
    return true;
}

mc_world_storage_status mc_world_chunk_tick_block(mc_world_chunk_tick_capture *c, int32_t x,
                                                  int32_t y, int32_t z, int32_t *out,
                                                  mc_world_chunk_tick_error *error) {
    mc_world_chunk_tick_error frame;
    if (!enter(c, MC_WORLD_CHUNK_TICK_BLOCK, out, &frame))
        return rejected(&frame, error);
    int32_t value = 0;
    if (c->state.blank)
        frame.site = location("li", "a", "(III)I", 1);
    else if (!bytes(c, x, y, z, &value, false, &frame))
        return finish(c, &frame, error);
    *out = value;
    return finish(c, &frame, error);
}

mc_world_storage_status mc_world_chunk_tick_raw_block(mc_world_chunk_tick_capture *c, int32_t x,
                                                      int32_t y, int32_t z, int32_t *out,
                                                      mc_world_chunk_tick_error *error) {
    mc_world_chunk_tick_error frame;
    if (!enter(c, MC_WORLD_CHUNK_TICK_RAW_BLOCK, out, &frame))
        return rejected(&frame, error);
    int32_t value;
    if (bytes(c, x, y, z, &value, true, &frame))
        *out = value;
    return finish(c, &frame, error);
}

mc_world_storage_status mc_world_chunk_tick_metadata(mc_world_chunk_tick_capture *c, int32_t x,
                                                     int32_t y, int32_t z, int32_t *out,
                                                     mc_world_chunk_tick_error *error) {
    mc_world_chunk_tick_error frame;
    if (!enter(c, MC_WORLD_CHUNK_TICK_METADATA, out, &frame))
        return rejected(&frame, error);
    int32_t value = 0;
    if (c->state.blank)
        frame.site = location("li", "b", "(III)I", 1);
    else {
        frame.site = location("lm", "b", "(III)I", 7);
        if (!nibble(c->object->e, x, y, z, &value, &frame))
            return finish(c, &frame, error);
        frame.site.pc = 10;
    }
    *out = value;
    return finish(c, &frame, error);
}

mc_world_storage_status mc_world_chunk_tick_saved_light(mc_world_chunk_tick_capture *c,
                                                        const mc_world_light_type *type, int32_t x,
                                                        int32_t y, int32_t z, int32_t *out,
                                                        mc_world_chunk_tick_error *error) {
    mc_world_chunk_tick_error frame;
    if (!enter(c, MC_WORLD_CHUNK_TICK_SAVED_LIGHT, out, &frame))
        return rejected(&frame, error);
    mc_world_runtime_classes *classes = c->state.world->classes;
    if (type != NULL && (!classes->light_types_initialized ||
                         (type != &classes->light_types[0] && type != &classes->light_types[1]))) {
        (void)fail(&frame, MC_WORLD_STORAGE_INVALID_ARGUMENT, "Captured Chunk Light namespace",
                   NULL);
        return finish(c, &frame, error);
    }
    int32_t value = 0;
    if (c->state.blank)
        frame.site = location("li", "a", "(Leb;III)I", 1);
    else {
        frame.site = location("lm", "a", "(Leb;III)I", 1);
        const mc_world_light_type *current = NULL;
        mc_world_storage_status status =
            mc_world_runtime_light_type_get(classes, MC_LIGHT_SKY, &current, &frame.cause);
        if (status != MC_WORLD_STORAGE_OK) {
            const mc_world_storage_error cause = frame.cause;
            (void)fail(&frame, status, cause.operation, NULL);
            frame.cause = cause;
            return finish(c, &frame, error);
        }
        if (type == current) {
            frame.site.pc = 15;
            if (!nibble(c->object->f, x, y, z, &value, &frame))
                return finish(c, &frame, error);
            frame.site.pc = 18;
        } else {
            frame.site.pc = 20;
            status =
                mc_world_runtime_light_type_get(classes, MC_LIGHT_BLOCK, &current, &frame.cause);
            if (status != MC_WORLD_STORAGE_OK) {
                const mc_world_storage_error cause = frame.cause;
                (void)fail(&frame, status, cause.operation, NULL);
                frame.cause = cause;
                return finish(c, &frame, error);
            }
            if (type == current) {
                frame.site.pc = 34;
                if (!nibble(c->object->g, x, y, z, &value, &frame))
                    return finish(c, &frame, error);
                frame.site.pc = 37;
            } else
                frame.site.pc = 39;
        }
    }
    *out = value;
    return finish(c, &frame, error);
}

mc_world_storage_status mc_world_chunk_tick_capture_retire(mc_world_chunk_tick_capture **slot,
                                                           mc_world_chunk_tick_error *error) {
    mc_world_chunk_tick_error frame = {.site = {"fd", "n", "()V", -1}};
    if (slot == NULL)
        (void)fail(&frame, MC_WORLD_STORAGE_INVALID_ARGUMENT, "Chunk capture retirement slot",
                   NULL);
    else if (*slot != NULL) {
        mc_world_chunk_tick_capture *c = *slot;
        if (c->state.active)
            (void)fail(&frame, MC_WORLD_STORAGE_BUSY, "Chunk capture active retirement", NULL);
        else {
            if (c->state.chunk_held) {
                c->references.release(c->references.owner, c->state.captured);
                c->state.chunk_held = false;
            }
            if (c->state.world_held) {
                mc_world_runtime_call_leave(c->state.world);
                c->state.world_held = false;
            }
            free(c);
            *slot = NULL;
        }
    }
    if (error != NULL)
        *error = frame;
    return frame.status;
}
