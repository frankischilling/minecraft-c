#include "sdk_cursor_internal.h"
#include "sdk_display_internal.h"
#include "sdk_input_internal.h"
#include "source_surface.h"

#include "../render_engine/internal.h"
#include "millis.h"

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_surface.h>
#include <SDL3/SDL_video.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CURSOR_CLASS "org/lwjgl/input/Cursor"
#define CURSOR_CTOR "(IIIIILjava/nio/IntBuffer;Ljava/nio/IntBuffer;)V"
#define CREATE_CURSORS                                                                             \
    "(IIIIILjava/nio/IntBuffer;Ljava/nio/IntBuffer;)[Lorg/lwjgl/input/Cursor$CursorElement;"

typedef struct cursor_element {
    SDL_Cursor *handle;
    int64_t delay, timeout;
} cursor_element;

struct mc_client_sdk_cursor {
    mc_client_display *display;
    mc_client_display_context_lease *lease;
    mc_client_render_arena arena;
    mc_client_sdk_cursor_phase phase;
    size_t holds;
    bool active, cursors_stored, source_destroyed, native_created, native_retired;
    int32_t source_index;
    int32_t last_store, native_hotspot_x, native_hotspot_y;
    mc_client_sdk_cursor_platform platform;
    mc_client_direct_int_buffer *flipped;
    cursor_element **array;
    cursor_element *element;
    SDL_Surface *surface;
    SDL_Cursor *native;
    uint8_t *rgba;
    mc_client_sdk_cursor_error failure;
};

static mc_client_sdk_cursor_status fail(mc_client_sdk_cursor_error *e,
                                        mc_client_sdk_cursor_status status, const char *operation,
                                        const char *method, int32_t pc, bool native) {
    e->status = status;
    e->operation = operation;
    e->site = (mc_client_graphics_site){
        CURSOR_CLASS, method, strcmp(method, "<init>") == 0 ? CURSOR_CTOR : CREATE_CURSORS, pc};
    e->native_policy = native;
    return status;
}

static mc_client_sdk_cursor_status publish(mc_client_sdk_cursor_error *e,
                                           mc_client_sdk_cursor_error *out) {
    if (out != NULL)
        *out = *e;
    return e->status;
}

mc_client_sdk_cursor_limits mc_client_sdk_cursor_default_limits(void) {
    return (mc_client_sdk_cursor_limits){65536u, 32u};
}

mc_client_sdk_cursor_status mc_client_sdk_cursor_allocate(mc_client_display *display,
                                                          const mc_client_sdk_cursor_limits *limits,
                                                          mc_client_sdk_cursor **out,
                                                          mc_client_sdk_cursor_error *error) {
    mc_client_sdk_cursor_error e = {0};
    const mc_client_sdk_cursor_limits bounds =
        limits == NULL ? mc_client_sdk_cursor_default_limits() : *limits;
    if (display == NULL || out == NULL || !SDL_IsMainThread() ||
        !mc_client_display_sdk_live(display) ||
        SDL_GL_GetCurrentWindow() != mc_client_display_window(display) ||
        SDL_GL_GetCurrentContext() == NULL) {
        fail(&e, MC_CLIENT_SDK_CURSOR_INVALID_ARGUMENT, "native cursor actual current SDK Display",
             "<init>", -1, true);
    } else if (bounds.max_bytes < sizeof(mc_client_sdk_cursor) || bounds.max_allocations == 0) {
        fail(&e, MC_CLIENT_SDK_CURSOR_LIMIT, "native cursor owner bound", "<init>", -1, true);
    } else {
        mc_client_display_context_lease *lease = NULL;
        if (!mc_client_display_context_acquire(display, &lease, &e.display)) {
            fail(&e, MC_CLIENT_SDK_CURSOR_DISPLAY_FAILED, "native cursor Display lifetime lease",
                 "<init>", -1, true);
        } else {
            mc_client_sdk_cursor *o = calloc(1, sizeof(*o));
            if (o == NULL) {
                (void)mc_client_display_context_release(&lease, NULL);
                fail(&e, MC_CLIENT_SDK_CURSOR_ALLOCATION_FAILED, "native cursor RAW allocation",
                     "<init>", -1, true);
            } else {
                o->display = display;
                o->lease = lease;
                o->holds = 1;
                o->last_store = -1;
                o->arena.options =
                    (mc_client_render_engine_options){bounds.max_bytes, bounds.max_allocations};
                mc_client_render_arena_commit_external(&o->arena, sizeof(*o));
                *out = o;
            }
        }
    }
    return publish(&e, error);
}

static bool buffer_allocate(mc_client_sdk_cursor *o, mc_client_sdk_cursor_error *e) {
    /* BufferUtils.createIntBuffer3 -> createByteBuffer1, then asIntBuffer6.
     * The generic native view kernel takes the real supplied source site;
     * this does not pretend the source invoked ge.d for this temporary. */
    const mc_client_graphics_site site = {"org/lwjgl/BufferUtils", "createByteBuffer",
                                          "(I)Ljava/nio/ByteBuffer;", 1};
    mc_client_direct_byte_buffer *bytes =
        mc_client_render_arena_allocate(&o->arena, sizeof(*bytes) + 4096u, site, &e->buffer);
    if (bytes == NULL)
        return false;
    bytes->position = 0;
    bytes->limit = bytes->capacity = 4096;
    bytes->data = (uint8_t *)(bytes + 1);
    return mc_client_ge_int_view(&o->arena, bytes,
                                 (mc_client_graphics_site){"org/lwjgl/BufferUtils",
                                                           "createIntBuffer",
                                                           "(I)Ljava/nio/IntBuffer;", 6},
                                 &o->flipped, &e->buffer);
}

static bool flip_images(mc_client_sdk_cursor *o, mc_client_direct_int_buffer *images,
                        mc_client_sdk_cursor_error *e) {
    for (int32_t y = 0; y < 16; ++y) {
        for (int32_t x = 0; x < 32; ++x) {
            const int32_t upper = y * 32 + x, lower = (31 - y) * 32 + x;
            int32_t top = 0, bottom = 0;
            const mc_client_graphics_site first = {
                CURSOR_CLASS, "flipImage", "(IIILjava/nio/IntBuffer;Ljava/nio/IntBuffer;)V", 62};
            const mc_client_graphics_site second = {
                CURSOR_CLASS, "flipImage", "(IIILjava/nio/IntBuffer;Ljava/nio/IntBuffer;)V", 79};
            if (!mc_client_ge_int_get(images, upper, &top, first, &e->buffer) ||
                !mc_client_ge_int_get(images, lower, &bottom, second, &e->buffer))
                return false;
            memcpy(o->flipped->bytes->data + (size_t)upper * 4u, &bottom, sizeof(bottom));
            memcpy(o->flipped->bytes->data + (size_t)lower * 4u, &top, sizeof(top));
        }
    }
    return true;
}

static void *allocate(mc_client_sdk_cursor *o, size_t bytes, int32_t pc,
                      mc_client_sdk_cursor_error *e) {
    return mc_client_render_arena_allocate(
        &o->arena, bytes,
        (mc_client_graphics_site){CURSOR_CLASS, "createCursors", CREATE_CURSORS, pc}, &e->buffer);
}

static bool native_cursor(mc_client_sdk_cursor *o, int32_t pc, mc_client_sdk_cursor_error *e) {
    o->rgba = allocate(o, 4096, pc, e);
    if (o->rgba == NULL)
        return false;
    for (size_t i = 0; i < 1024; ++i) {
        uint32_t argb;
        memcpy(&argb, o->flipped->bytes->data + i * 4u, sizeof(argb));
        o->rgba[i * 4u] = (uint8_t)(argb >> 16);
        o->rgba[i * 4u + 1u] = (uint8_t)(argb >> 8);
        o->rgba[i * 4u + 2u] = (uint8_t)argb;
        o->rgba[i * 4u + 3u] = (uint8_t)(argb >> 24);
    }
    o->surface = SDL_CreateSurfaceFrom(32, 32, SDL_PIXELFORMAT_RGBA32, o->rgba, 32 * 4);
    if (o->surface != NULL)
        o->native = SDL_CreateColorCursor(o->surface, o->native_hotspot_x, o->native_hotspot_y);
    if (o->surface == NULL || o->native == NULL) {
        (void)snprintf(e->detail, sizeof(e->detail), "%s", SDL_GetError());
        fail(e, MC_CLIENT_SDK_CURSOR_PLATFORM_FAILED, "real native color cursor creation",
             "createCursors", pc, true);
        return false;
    }
    o->native_created = true;
    SDL_Surface *surface = o->surface;
    o->surface = NULL;
    SDL_DestroySurface(surface); /* Pixel storage remains live in this owned arena. */
    return true;
}

mc_client_sdk_cursor_status mc_client_sdk_cursor_construct32(mc_client_sdk_cursor *o,
                                                             mc_client_sdk_input *input,
                                                             mc_client_direct_int_buffer *images,
                                                             mc_client_direct_int_buffer *delays,
                                                             mc_client_sdk_cursor_error *error) {
    mc_client_sdk_cursor_error e = {0};
    if (o == NULL || o->active || o->phase != MC_CLIENT_SDK_CURSOR_RAW) {
        fail(&e,
             o != NULL && o->active ? MC_CLIENT_SDK_CURSOR_BUSY
                                    : MC_CLIENT_SDK_CURSOR_INVALID_PHASE,
             "native Cursor one-shot constructor", "<init>", -1, true);
        return publish(&e, error);
    }
    /* This finite caller supplies direct views with exactly the reached vy
     * extents. Invalid native pointer/view domains are not Java exceptions. */
    if (input == NULL || mc_client_display_inputs(o->display) != input || images == NULL ||
        delays == NULL || images->position != 0 || delays->position != 0 ||
        mc_client_ge_int_remaining(images) < 1024 || mc_client_ge_int_remaining(delays) < 1) {
        fail(&e, MC_CLIENT_SDK_CURSOR_INVALID_ARGUMENT,
             "native Cursor actual direct-buffer/input domain", "<init>", -1, true);
        return publish(&e, error);
    }
    if (!mc_client_sdk_input_enter(input, CURSOR_CLASS, "<init>", CURSOR_CTOR, &e.input)) {
        fail(&e,
             e.input.status == MC_CLIENT_SDK_INPUT_BUSY ? MC_CLIENT_SDK_CURSOR_BUSY
                                                        : MC_CLIENT_SDK_CURSOR_INVALID_PHASE,
             "native Cursor actual input admission", "<init>", -1, true);
        return publish(&e, error);
    }
    o->active = true;
    o->phase = MC_CLIENT_SDK_CURSOR_CONSTRUCTING;
    int32_t buffer_invoke = 5;
    if (!mc_client_sdk_cursor_platform_query(mc_client_display_window(o->display), &o->platform,
                                             &e)) {
        if (e.status == MC_CLIENT_SDK_CURSOR_OK)
            e.status = MC_CLIENT_SDK_CURSOR_PLATFORM_FAILED;
        fail(&e, e.status, "actual current native cursor capabilities", "<init>", 11, true);
        goto done;
    }
    if (!o->platform.one_bit_supported) {
        /* A failed native capability admission is not evidence that the
         * original getter returned0 and constructed LWJGLException25. */
        fail(&e, MC_CLIENT_SDK_CURSOR_UNSUPPORTED_PROFILE,
             "native cursor capability admission is outside the positive profile", "<init>", 11,
             true);
        goto done;
    }
    if (o->platform.best_width < 32 || o->platform.best_height < 32) {
        fail(&e, MC_CLIENT_SDK_CURSOR_UNSUPPORTED_PROFILE,
             "native provider cannot admit32 cursor extent", "<init>", 11, true);
        goto done;
    }
    /* Both already-direct wrapBuffer calls37/54 have no copying effect for
     * these sufficient native views. The actual Mouse.isCreated follows. */
    if (!input->mouse_created) {
        fail(&e, MC_CLIENT_SDK_CURSOR_SOURCE_EXCEPTION, "actual Cursor Mouse.isCreated guard",
             "<init>", 71, false);
        e.exception_class = "java/lang/IllegalStateException";
        goto done;
    }
    /* Sys.initialize136 is already satisfied by the actual initialized native
     * window/input owner. The reached source height-1-y conversion is144. */
    o->native_hotspot_x = 16;
    o->native_hotspot_y = 32 - 1 - 16;
    e.caller = (mc_client_graphics_site){CURSOR_CLASS, "<init>", CURSOR_CTOR, 159};
    if (!buffer_allocate(o, &e))
        goto buffer_failed;
    buffer_invoke = 18;
    if (!flip_images(o, images, &e))
        goto buffer_failed;
    const bool windows = o->platform.platform == 3;
    if (windows) {
        buffer_invoke = 159;
        o->array = allocate(o, sizeof(*o->array), 159, &e);
        if (o->array == NULL)
            goto buffer_failed;
        for (size_t i = 0; i < 1024; ++i) {
            uint32_t pixel;
            memcpy(&pixel, o->flipped->bytes->data + i * 4u, sizeof(pixel));
            if ((pixel >> 24) != 255u) {
                pixel = 0;
                memcpy(o->flipped->bytes->data + i * 4u, &pixel, sizeof(pixel));
            }
        }
    }
    const int32_t create_pc = windows ? 249 : 334;
    buffer_invoke = create_pc;
    if (!native_cursor(o, create_pc, &e)) {
        if (e.status == MC_CLIENT_SDK_CURSOR_OK)
            goto buffer_failed;
        goto done;
    }
    int64_t delay = -1, timeout = -1;
    if (windows) {
        int32_t value = 0;
        buffer_invoke = 265;
        if (!mc_client_ge_int_get(
                delays, 0, &value,
                (mc_client_graphics_site){CURSOR_CLASS, "createCursors", CREATE_CURSORS, 265},
                &e.buffer))
            goto buffer_failed;
        delay = value;
        const mc_platform_millis_status clock = mc_platform_current_time_millis(&timeout);
        if (clock != MC_PLATFORM_MILLIS_OK) {
            fail(&e, MC_CLIENT_SDK_CURSOR_CLOCK_FAILED, "actual CursorElement currentTimeMillis",
                 "createCursors", 275, true);
            e.platform_clock_status = (int)clock;
            goto done;
        }
    }
    buffer_invoke = windows ? 284 : 341;
    o->element = allocate(o, sizeof(*o->element), windows ? 284 : 341, &e);
    if (o->element == NULL)
        goto buffer_failed;
    /* Actual CursorElement stores handle6, delay11, timeout17. */
    o->element->handle = o->native;
    o->element->delay = delay;
    o->element->timeout = timeout;
    if (!windows) {
        buffer_invoke = 359;
        o->array = allocate(o, sizeof(*o->array), 359, &e);
        if (o->array == NULL)
            goto buffer_failed;
    }
    o->array[0] = o->element;
    if (windows)
        o->flipped->position = 1024; /* Actual position308, after array store297. */
    o->cursors_stored = true;
    o->last_store = 162;
    goto done;
buffer_failed:
    fail(&e,
         e.buffer.status == MC_CLIENT_GRAPHICS_LIMIT ? MC_CLIENT_SDK_CURSOR_LIMIT
         : e.buffer.status == MC_CLIENT_GRAPHICS_ALLOCATION_FAILED
             ? MC_CLIENT_SDK_CURSOR_ALLOCATION_FAILED
             : MC_CLIENT_SDK_CURSOR_BUFFER_FAILED,
         "actual Cursor direct-buffer/array prefix", "createCursors", buffer_invoke, true);
done:
    /* Cursor failure never poisons an otherwise unchanged Mouse/Keyboard owner. */
    input->active = false;
    o->active = false;
    o->phase = e.status == MC_CLIENT_SDK_CURSOR_OK ? MC_CLIENT_SDK_CURSOR_READY
                                                   : MC_CLIENT_SDK_CURSOR_FAILED;
    e.native_created = o->native_created;
    e.source_cursors_stored = o->cursors_stored;
    if (e.status != MC_CLIENT_SDK_CURSOR_OK)
        o->failure = e;
    return publish(&e, error);
}

mc_client_sdk_cursor_status mc_client_sdk_cursor_retain(mc_client_sdk_cursor *o,
                                                        mc_client_sdk_cursor_error *error) {
    mc_client_sdk_cursor_error e = {0};
    if (o == NULL || !SDL_IsMainThread())
        fail(&e, MC_CLIENT_SDK_CURSOR_INVALID_ARGUMENT, "native Cursor retain", "<init>", -1, true);
    else if (o->active || o->phase == MC_CLIENT_SDK_CURSOR_RETIRING)
        fail(&e, MC_CLIENT_SDK_CURSOR_BUSY, "native Cursor active/retiring hold", "<init>", -1,
             true);
    else if (o->holds == SIZE_MAX)
        fail(&e, MC_CLIENT_SDK_CURSOR_LIMIT, "native Cursor holds", "<init>", -1, true);
    else
        ++o->holds;
    if (o != NULL) {
        e.native_created = o->native_created;
        e.source_cursors_stored = o->cursors_stored;
    }
    return publish(&e, error);
}

mc_client_sdk_cursor_status mc_client_sdk_cursor_release(mc_client_sdk_cursor **slot,
                                                         mc_client_sdk_cursor_error *error) {
    mc_client_sdk_cursor_error e = {0};
    if (slot == NULL || (*slot != NULL && !SDL_IsMainThread())) {
        fail(&e, MC_CLIENT_SDK_CURSOR_INVALID_ARGUMENT, "native Cursor release slot/thread",
             "<init>", -1, true);
    } else if (*slot != NULL) {
        mc_client_sdk_cursor *o = *slot;
        e.native_created = o->native_created;
        e.source_cursors_stored = o->cursors_stored;
        if (o->active) {
            fail(&e, MC_CLIENT_SDK_CURSOR_BUSY, "native Cursor active release", "<init>", -1, true);
        } else if (o->holds > 1) {
            --o->holds;
            *slot = NULL;
        } else if (mc_client_display_sdk_external_busy(o->display)) {
            fail(&e, MC_CLIENT_SDK_CURSOR_BUSY, "native Cursor Display/input operation active",
                 "<init>", -1, true);
        } else if (o->native != NULL && SDL_GetCursor() == o->native) {
            fail(&e, MC_CLIENT_SDK_CURSOR_BUSY, "native Cursor is externally selected", "<init>",
                 -1, true);
        } else {
            /* Protect the shared physical input/Display during real native
             * destruction too. A FAILED input can still be cleaned; this is
             * native ownership, not a new source Input operation. */
            mc_client_sdk_input *input = mc_client_display_inputs(o->display);
            if (input != NULL)
                input->active = true;
            o->phase = MC_CLIENT_SDK_CURSOR_RETIRING;
            o->active = true;
            SDL_Cursor *native = o->native;
            o->native = NULL;
            if (native != NULL) {
                o->native_retired = true;
                SDL_DestroyCursor(native);
            }
            SDL_Surface *surface = o->surface;
            o->surface = NULL;
            SDL_DestroySurface(surface);
            if (!mc_client_display_context_release(&o->lease, &e.display)) {
                fail(&e, MC_CLIENT_SDK_CURSOR_DISPLAY_FAILED, "native Cursor context lease cleanup",
                     "<init>", -1, true);
                o->failure = e;
                o->active = false;
                if (input != NULL)
                    input->active = false;
            } else {
                if (input != NULL)
                    input->active = false;
                mc_client_render_arena_free(&o->arena);
                free(o);
                *slot = NULL;
            }
        }
    }
    return publish(&e, error);
}

bool mc_client_sdk_cursor_inspect(const mc_client_sdk_cursor *o, mc_client_sdk_cursor_state *out) {
    if (o == NULL || out == NULL)
        return false;
    *out = (mc_client_sdk_cursor_state){
        .phase = o->phase,
        .holds = o->holds,
        .native_bytes = o->arena.bytes,
        .native_allocations = o->arena.allocations,
        .active = o->active,
        .source_cursors_stored = o->cursors_stored,
        .source_destroyed = o->source_destroyed,
        .native_created = o->native_created,
        .native_retired = o->native_retired,
        .source_index = o->source_index,
        .last_store_pc = o->last_store,
        .platform = o->platform.platform,
        .native_hotspot_x = o->native_hotspot_x,
        .native_hotspot_y = o->native_hotspot_y,
        .best_width = o->platform.best_width,
        .best_height = o->platform.best_height,
        .one_bit_supported = o->platform.one_bit_supported,
        .element_delay = o->element == NULL ? 0 : o->element->delay,
        .element_timeout = o->element == NULL ? 0 : o->element->timeout,
        .flipped_position = o->flipped == NULL ? 0 : o->flipped->position,
        .flipped_limit = o->flipped == NULL ? 0 : o->flipped->limit,
        .native_handle = o->native,
        .failure = &o->failure};
    return true;
}
