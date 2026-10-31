#include "ge.h"
#include "internal.h"
#include "mc/core/java/java.h"
#include <stdlib.h>
#include <string.h>

static mc_client_graphics_site ge_site(const char *method, const char *descriptor, int32_t pc) {
    return (mc_client_graphics_site){"ge", method, descriptor, pc};
}

bool mc_client_ge_initialize(mc_client_render_engine_classes *c, mc_client_graphics_site caller,
                             mc_client_graphics_error *e) {
    *e = (mc_client_graphics_error){0};
    e->entry = caller;
    e->caller = caller;
    if (c == NULL) {
        mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_INVALID_ARGUMENT,
                                "native shared ge class owner", caller, true);
        return false;
    }
    if (c->ge_phase == MC_CLIENT_RENDER_ENGINE_READY)
        return true;
    if (c->ge_phase == MC_CLIENT_RENDER_ENGINE_FAILED) {
        e->class_cause = c->ge_failure;
        mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_CLASS_FAILED,
                                "previous actual ge class initialization failure", caller, false);
        return false;
    }
    if (c->ge_phase == MC_CLIENT_RENDER_ENGINE_CONSTRUCTING) {
        mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_BUSY, "ge initializer active", caller, true);
        return false;
    }
    c->ge_phase = MC_CLIENT_RENDER_ENGINE_CONSTRUCTING;
    const int32_t pcs[2] = {0, 10};
    mc_client_render_list **slots[2] = {&c->a, &c->b};
    for (size_t i = 0; i < 2; ++i) {
        const mc_client_graphics_site site = ge_site("<clinit>", "()V", pcs[i]);
        if (!mc_client_render_arena_check(&c->arena, sizeof(mc_client_render_list), site, e)) {
            e->operation = "ge List native constructor limit";
            goto failed;
        }
        mc_client_render_list *list = calloc(1, sizeof(*list));
        if (list == NULL) {
            mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_ALLOCATION_FAILED,
                                    "ge actual List allocation", site, true);
            goto failed;
        }
        mc_client_render_arena_commit_external(&c->arena, sizeof(*list));
        *slots[i] = list; /* actual a7/b17, only after each genuine constructor */
    }
    c->ge_phase = MC_CLIENT_RENDER_ENGINE_READY;
    return true;
failed:
    c->ge_phase = MC_CLIENT_RENDER_ENGINE_FAILED;
    c->ge_failure =
        (mc_client_graphics_cause){e->status, e->operation, e->caller, e->site, e->native_policy};
    /* The old inspection/error view remains meaningful for old ji callers;
     * a new nw/sj caller is not labelled with a fabricated ji source PC. */
    c->failure = (mc_client_render_engine_error){0};
    c->failure.status = e->status == MC_CLIENT_GRAPHICS_LIMIT
                            ? MC_CLIENT_RENDER_ENGINE_LIMIT
                            : MC_CLIENT_RENDER_ENGINE_ALLOCATION_FAILED;
    c->failure.operation = e->operation;
    c->failure.source_class = e->site.class_name;
    c->failure.source_pc = (unsigned)e->site.pc;
    c->failure.ge_method = e->site.method;
    c->failure.ge_pc = (unsigned)e->site.pc;
    if (caller.class_name != NULL && strcmp(caller.class_name, "ji") == 0)
        c->failure.ji_pc = (unsigned)caller.pc;
    return false;
}

static bool byte_buffer(mc_client_render_arena *arena, int32_t bytes,
                        mc_client_direct_byte_buffer **out, mc_client_graphics_error *e) {
    const mc_client_graphics_site site = ge_site("c", "(I)Ljava/nio/ByteBuffer;", 1);
    if (bytes < 0) {
        mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_SOURCE_ILLEGAL_ARGUMENT,
                                "actual allocateDirect negative capacity", site, false);
        return false;
    }
    mc_client_direct_byte_buffer *b =
        mc_client_render_arena_allocate(arena, sizeof(*b) + (size_t)bytes, site, e);
    if (b == NULL)
        return false;
    b->limit = b->capacity = (size_t)bytes;
    b->data = (uint8_t *)(b + 1);
    *out = b;
    return true;
}

bool mc_client_ge_c(mc_client_render_engine_classes *c, mc_client_render_arena *arena,
                    int32_t bytes, mc_client_graphics_site caller,
                    mc_client_direct_byte_buffer **out, mc_client_graphics_error *e) {
    if (!mc_client_ge_initialize(c, caller, e))
        return false;
    return byte_buffer(arena, bytes, out, e);
}

bool mc_client_ge_int_view(mc_client_render_arena *arena, mc_client_direct_byte_buffer *bytes,
                           mc_client_graphics_site site, mc_client_direct_int_buffer **out,
                           mc_client_graphics_error *e) {
    if (bytes == NULL) {
        mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_NULL_REFERENCE,
                                "actual ByteBuffer.asIntBuffer receiver", site, false);
        return false;
    }
    if (bytes->position != 0) {
        mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_UNIMPLEMENTED_DEPENDENCY,
                                "nonzero ByteBuffer IntBuffer view offset", site, false);
        return false;
    }
    mc_client_direct_int_buffer *b = mc_client_render_arena_allocate(arena, sizeof(*b), site, e);
    if (b == NULL)
        return false;
    b->limit = b->capacity = (bytes->limit - bytes->position) / sizeof(int32_t);
    b->bytes = bytes;
    *out = b;
    return true;
}

bool mc_client_ge_float_view(mc_client_render_arena *arena, mc_client_direct_byte_buffer *bytes,
                             mc_client_graphics_site site, mc_client_direct_float_buffer **out,
                             mc_client_graphics_error *e) {
    if (bytes == NULL) {
        mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_NULL_REFERENCE,
                                "actual ByteBuffer.asFloatBuffer receiver", site, false);
        return false;
    }
    if (bytes->position != 0) {
        mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_UNIMPLEMENTED_DEPENDENCY,
                                "nonzero ByteBuffer FloatBuffer view offset", site, false);
        return false;
    }
    mc_client_direct_float_buffer *b = mc_client_render_arena_allocate(arena, sizeof(*b), site, e);
    if (b == NULL)
        return false;
    b->limit = b->capacity = (bytes->limit - bytes->position) / sizeof(float);
    b->bytes = bytes;
    *out = b;
    return true;
}

bool mc_client_ge_d(mc_client_render_engine_classes *c, mc_client_render_arena *arena,
                    int32_t count, mc_client_graphics_site caller,
                    mc_client_direct_int_buffer **out, mc_client_graphics_error *e) {
    if (!mc_client_ge_initialize(c, caller, e))
        return false;
    mc_client_direct_byte_buffer *bytes;
    if (!byte_buffer(arena, mc_java_i32_shl(count, 2), &bytes, e))
        return false;
    return mc_client_ge_int_view(arena, bytes, ge_site("d", "(I)Ljava/nio/IntBuffer;", 6), out, e);
}

bool mc_client_ge_e(mc_client_render_engine_classes *c, mc_client_render_arena *arena,
                    int32_t count, mc_client_graphics_site caller,
                    mc_client_direct_float_buffer **out, mc_client_graphics_error *e) {
    if (!mc_client_ge_initialize(c, caller, e))
        return false;
    mc_client_direct_byte_buffer *bytes;
    if (!byte_buffer(arena, mc_java_i32_shl(count, 2), &bytes, e))
        return false;
    return mc_client_ge_float_view(arena, bytes, ge_site("e", "(I)Ljava/nio/FloatBuffer;", 6), out,
                                   e);
}

bool mc_client_ge_int_clear(mc_client_direct_int_buffer *b, mc_client_graphics_site site,
                            mc_client_graphics_error *e) {
    if (b == NULL) {
        mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_NULL_REFERENCE,
                                "actual IntBuffer.clear receiver", site, false);
        return false;
    }
    b->position = 0;
    b->limit = b->capacity;
    return true;
}

bool mc_client_ge_int_flip(mc_client_direct_int_buffer *b, mc_client_graphics_site site,
                           mc_client_graphics_error *e) {
    if (b == NULL) {
        mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_NULL_REFERENCE,
                                "actual IntBuffer.flip receiver", site, false);
        return false;
    }
    b->limit = b->position;
    b->position = 0;
    return true;
}

bool mc_client_ge_int_put(mc_client_direct_int_buffer *b, int32_t value,
                          mc_client_graphics_site site, mc_client_graphics_error *e) {
    if (b == NULL) {
        mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_NULL_REFERENCE,
                                "actual IntBuffer.put receiver", site, false);
        return false;
    }
    if (b->position >= b->limit) {
        mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_BUFFER_OVERFLOW,
                                "actual IntBuffer.put overflow", site, false);
        return false;
    }
    memcpy(b->bytes->data + b->position * sizeof(value), &value, sizeof(value));
    ++b->position;
    return true;
}

bool mc_client_ge_int_get(const mc_client_direct_int_buffer *b, int32_t index, int32_t *out,
                          mc_client_graphics_site site, mc_client_graphics_error *e) {
    if (b == NULL) {
        mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_NULL_REFERENCE,
                                "actual IntBuffer.get receiver", site, false);
        return false;
    }
    if (index < 0 || (size_t)index >= b->limit) {
        e->actual_index = index;
        mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_ARRAY_INDEX,
                                "actual IntBuffer absolute get index", site, false);
        return false;
    }
    memcpy(out, b->bytes->data + (size_t)index * sizeof(*out), sizeof(*out));
    return true;
}

size_t mc_client_ge_int_remaining(const mc_client_direct_int_buffer *b) {
    return b->limit - b->position;
}

bool mc_client_ge_int_position(mc_client_direct_int_buffer *b, int32_t value,
                               mc_client_graphics_site site, mc_client_graphics_error *e) {
    if (b == NULL) {
        mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_NULL_REFERENCE,
                                "actual IntBuffer.position receiver", site, false);
        return false;
    }
    if (value < 0 || (size_t)value > b->limit) {
        e->actual_index = value;
        mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_SOURCE_ILLEGAL_ARGUMENT,
                                "actual IntBuffer.position argument", site, false);
        return false;
    }
    b->position = (size_t)value;
    return true;
}

bool mc_client_ge_int_limit(mc_client_direct_int_buffer *b, int32_t value,
                            mc_client_graphics_site site, mc_client_graphics_error *e) {
    if (b == NULL) {
        mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_NULL_REFERENCE,
                                "actual IntBuffer.limit receiver", site, false);
        return false;
    }
    if (value < 0 || (size_t)value > b->capacity) {
        e->actual_index = value;
        mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_SOURCE_ILLEGAL_ARGUMENT,
                                "actual IntBuffer.limit argument", site, false);
        return false;
    }
    b->limit = (size_t)value;
    if (b->position > b->limit)
        b->position = b->limit;
    return true;
}

bool mc_client_ge_int_writable_remaining(mc_client_direct_int_buffer *b, void **storage,
                                         size_t *remaining, mc_client_graphics_site site,
                                         mc_client_graphics_error *e) {
    if (storage == NULL || remaining == NULL) {
        mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_INVALID_ARGUMENT,
                                "native direct IntBuffer span output", site, true);
        return false;
    }
    if (b == NULL) {
        mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_NULL_REFERENCE,
                                "actual native IntBuffer address receiver", site, false);
        return false;
    }
    if (b->bytes == NULL || b->bytes->data == NULL || b->position > b->limit ||
        b->limit > b->capacity || b->capacity > b->bytes->capacity / sizeof(int32_t)) {
        mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_INVALID_ARGUMENT,
                                "native actual direct IntBuffer storage invariant", site, true);
        return false;
    }
    *storage = b->bytes->data + b->position * sizeof(int32_t);
    *remaining = b->limit - b->position;
    return true;
}

bool mc_client_ge_append(mc_client_render_list *list, int32_t value, size_t limit,
                         mc_client_graphics_site site, mc_client_graphics_error *e) {
    if (list == NULL) {
        mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_NULL_REFERENCE,
                                "actual shared ge List.add receiver", site, false);
        return false;
    }
    if (list->count >= limit) {
        mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_LIMIT, "ge List native entry bound", site,
                                true);
        return false;
    }
    if (list->count == list->capacity) {
        size_t capacity = list->capacity == 0 ? 8 : list->capacity;
        if (list->capacity != 0)
            capacity = capacity > SIZE_MAX / 2 ? limit : capacity * 2;
        if (capacity > limit)
            capacity = limit;
        if (capacity > SIZE_MAX / sizeof(int32_t)) {
            mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_LIMIT, "ge List native byte bound", site,
                                    true);
            return false;
        }
        void *entries = realloc(list->entries, capacity * sizeof(int32_t));
        if (entries == NULL) {
            mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_ALLOCATION_FAILED,
                                    "ge List native entry growth", site, true);
            return false;
        }
        list->entries = (void **)(void *)entries;
        list->capacity = capacity;
    }
    memcpy((uint8_t *)(void *)list->entries + list->count * sizeof(value), &value, sizeof(value));
    ++list->count;
    return true;
}
