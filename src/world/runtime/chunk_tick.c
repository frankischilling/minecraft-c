#include "mc/world/runtime/chunk_tick.h"

#include "chunk_tick_internal.h"

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

static mc_world_storage_status read_admit(mc_world_chunk_tick_capture *c,
                                          mc_world_chunk_tick_operation operation,
                                          const int32_t *out, const char **message) {
    if (c == NULL || out == NULL) {
        *message = "Captured Chunk read arguments";
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    }
    if (c->state.active) {
        *message = "Captured Chunk active read";
        return MC_WORLD_STORAGE_BUSY;
    }
    if (!c->state.ready || !c->state.world_held || !c->state.chunk_held) {
        *message = "Captured Chunk incomplete ownership";
        return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    }
    if (c->state.reads == SIZE_MAX) {
        *message = "Captured Chunk native read counter";
        return MC_WORLD_STORAGE_LIMIT;
    }
    c->state.active = true;
    c->state.operation = operation;
    ++c->state.reads;
    return MC_WORLD_STORAGE_OK;
}

static bool enter(mc_world_chunk_tick_capture *c, mc_world_chunk_tick_operation operation,
                  const int32_t *out, mc_world_chunk_tick_error *e) {
    *e = (mc_world_chunk_tick_error){.site = {"fd", "n", "()V", -1}};
    const char *message = NULL;
    const mc_world_storage_status status = read_admit(c, operation, out, &message);
    if (status != MC_WORLD_STORAGE_OK)
        (void)fail(e, status, message, NULL);
    return status == MC_WORLD_STORAGE_OK;
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

typedef enum byte_read_status {
    BYTE_READ_OK,
    BYTE_READ_DATA,
    BYTE_READ_NULL,
    BYTE_READ_INDEX
} byte_read_status;

/* One actual data/array/index/read body for virtual and raw callers. */
static byte_read_status read_bytes(mc_world_chunk_tick_capture *c, int32_t x, int32_t y, int32_t z,
                                   int32_t *value, int32_t *index) {
    mc_chunk_data *data = c->state.blank ? c->state.captured->data : c->object->data;
    if (data == NULL)
        return BYTE_READ_DATA;
    /* Read the actual b reference before computing the source packed index.
     * A Java NULL array fails at baload, after that index computation. */
    const uint8_t *array = data->blocks;
    *index = packed(x, y, z);
    if (array == NULL)
        return BYTE_READ_NULL;
    if (*index < 0 || (size_t)*index >= data->blocks_capacity)
        return BYTE_READ_INDEX;
    *value = (int32_t)array[(size_t)*index];
    return BYTE_READ_OK;
}

static bool bytes(mc_world_chunk_tick_capture *c, int32_t x, int32_t y, int32_t z, int32_t *value,
                  bool raw, mc_world_chunk_tick_error *e) {
    int32_t index = 0;
    const byte_read_status status = read_bytes(c, x, y, z, value, &index);
    e->site = raw ? location("fd", "n", "()V", 927) : location("lm", "a", "(III)I", 1);
    if (status == BYTE_READ_DATA) {
        (void)fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Captured Chunk native data owner", NULL);
        return false;
    }
    e->packed_index = e->array_index = index;
    e->site.pc = raw ? 944 : 15;
    if (status == BYTE_READ_NULL) {
        (void)fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Captured Chunk null block byte array",
                   "java/lang/NullPointerException");
        return false;
    }
    if (status == BYTE_READ_INDEX) {
        (void)fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Captured Chunk block byte index",
                   "java/lang/ArrayIndexOutOfBoundsException");
        return false;
    }
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

#if defined(_MSC_VER)
#define RAW_NOINLINE __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
#define RAW_NOINLINE __attribute__((noinline))
#else
#define RAW_NOINLINE
#endif

/* Complete diagnostic construction stays outside the internal success frame. */
static RAW_NOINLINE mc_world_storage_status
raw_diagnostic(mc_world_chunk_tick_capture *c, bool entered, mc_world_storage_status status,
               const char *message, const char *exception, int32_t pc, int32_t index,
               mc_world_chunk_tick_error *error) {
    mc_world_chunk_tick_error frame = {
        .site = {"fd", "n", "()V", pc}, .packed_index = index, .array_index = index};
    if (status != MC_WORLD_STORAGE_OK)
        (void)fail(&frame, status, message, exception);
    return entered ? finish(c, &frame, error) : rejected(&frame, error);
}

static mc_world_storage_status raw_read(mc_world_chunk_tick_capture *c, int32_t x, int32_t y,
                                        int32_t z, int32_t *out, mc_world_chunk_tick_error *error,
                                        bool success_error) {
    const char *message = NULL;
    const mc_world_storage_status admission =
        read_admit(c, MC_WORLD_CHUNK_TICK_RAW_BLOCK, out, &message);
    if (admission != MC_WORLD_STORAGE_OK)
        return raw_diagnostic(c, false, admission, message, NULL, -1, 0, error);
    int32_t value, index = 0;
    const byte_read_status status = read_bytes(c, x, y, z, &value, &index);
    if (status == BYTE_READ_DATA)
        return raw_diagnostic(c, true, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                              "Captured Chunk native data owner", NULL, 927, 0, error);
    if (status == BYTE_READ_NULL)
        return raw_diagnostic(c, true, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                              "Captured Chunk null block byte array",
                              "java/lang/NullPointerException", 944, index, error);
    if (status == BYTE_READ_INDEX)
        return raw_diagnostic(c, true, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                              "Captured Chunk block byte index",
                              "java/lang/ArrayIndexOutOfBoundsException", 944, index, error);
    *out = value;
    if (success_error)
        return raw_diagnostic(c, true, MC_WORLD_STORAGE_OK, NULL, NULL, 948, index, error);
    c->state.site = location("fd", "n", "()V", 948);
    c->state.child = (mc_world_chunk_tick_site){0};
    c->state.packed_index = c->state.array_index = index;
    c->state.active = false;
    return MC_WORLD_STORAGE_OK;
}

mc_world_storage_status mc_world_chunk_tick_raw_block(mc_world_chunk_tick_capture *c, int32_t x,
                                                      int32_t y, int32_t z, int32_t *out,
                                                      mc_world_chunk_tick_error *error) {
    return raw_read(c, x, y, z, out, error, true);
}

mc_world_storage_status mc_world_chunk_tick_raw_block_failure(mc_world_chunk_tick_capture *c,
                                                              int32_t x, int32_t y, int32_t z,
                                                              int32_t *out,
                                                              mc_world_chunk_tick_error *error) {
    return raw_read(c, x, y, z, out, error, false);
}

#undef RAW_NOINLINE

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
