#include "canvas.h"
#include "canvas_platform.h"
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_video.h>

#include <limits.h>
#include <stdlib.h>

struct mc_client_source_canvas {
    size_t holds, graphics;
    int32_t width, height;
    SDL_Window *window;
    SDL_WindowID window_id;
    bool active;
    mc_client_canvas_resize_listener resize;
    void *resize_context;
    struct mc_client_source_canvas *live_next;
};

/* Externally serialized cold/native owner domain. Compare foreign identities
 * without dereference before admitting their reached source operation. */
static mc_client_source_canvas *live_canvases;

static bool represented(const mc_client_source_canvas *c) {
    for (const mc_client_source_canvas *p = live_canvases; p != NULL; p = p->live_next)
        if (p == c)
            return true;
    return false;
}

struct mc_client_canvas_graphics {
    mc_client_source_canvas *canvas;
    void *native_context;
    bool black, active;
};

static mc_client_canvas_status result(mc_client_canvas_error *out, mc_client_canvas_status status,
                                      const char *operation) {
    if (out != NULL)
        *out = (mc_client_canvas_error){status, operation, 0,
                                        status != MC_CLIENT_CANVAS_OK &&
                                            status != MC_CLIENT_CANVAS_NULL_REFERENCE};
    return status;
}

static bool live_peer(const mc_client_source_canvas *c) {
    /* Lookup by retained ID before any SDL operation on the borrowed pointer.
     * The host still owes the ordinary exclusive lifetime contract. */
    return c->window != NULL && c->window_id != 0 && SDL_GetWindowFromID(c->window_id) == c->window;
}

mc_client_canvas_status mc_client_source_canvas_allocate(mc_client_source_canvas **out,
                                                         mc_client_canvas_error *error) {
    if (out == NULL || *out != NULL)
        return result(error, MC_CLIENT_CANVAS_ARGUMENT, "Canvas native empty output");
    mc_client_source_canvas *c = calloc(1, sizeof(*c));
    if (c == NULL)
        return result(error, MC_CLIENT_CANVAS_ALLOCATION, "Canvas native allocation");
    c->holds = 1;
    c->live_next = live_canvases;
    live_canvases = c;
    *out = c;
    return result(error, MC_CLIENT_CANVAS_OK, NULL);
}

mc_client_canvas_status mc_client_source_canvas_retain(mc_client_source_canvas *c,
                                                       mc_client_canvas_error *error) {
    if (c != NULL && !represented(c))
        return result(error, MC_CLIENT_CANVAS_NATIVE_STATE,
                      "foreign Canvas occurrence; no source dereference");
    if (c == NULL)
        return result(error, MC_CLIENT_CANVAS_ARGUMENT, "Canvas native retain receiver");
    if (c->active)
        return result(error, MC_CLIENT_CANVAS_BUSY, "Canvas active occurrence");
    if (c->holds == SIZE_MAX)
        return result(error, MC_CLIENT_CANVAS_LIMIT, "Canvas native occurrence limit");
    ++c->holds;
    return result(error, MC_CLIENT_CANVAS_OK, NULL);
}

mc_client_canvas_status mc_client_source_canvas_release(mc_client_source_canvas **slot,
                                                        mc_client_canvas_error *error) {
    if (slot == NULL)
        return result(error, MC_CLIENT_CANVAS_ARGUMENT, "Canvas native release slot");
    mc_client_source_canvas *c = *slot;
    if (c != NULL && !represented(c))
        return result(error, MC_CLIENT_CANVAS_NATIVE_STATE, "foreign Canvas release occurrence");
    if (c == NULL)
        return result(error, MC_CLIENT_CANVAS_OK, NULL);
    if (c->active || c->holds <= c->graphics || (c->window != NULL && c->holds - c->graphics == 1))
        return result(error, MC_CLIENT_CANVAS_BUSY, "Canvas active Graphics or bound peer");
    if (c->holds == 0)
        return result(error, MC_CLIENT_CANVAS_NATIVE_STATE, "Canvas native occurrence state");
    if (--c->holds == 0) {
        mc_client_source_canvas **link = &live_canvases;
        while (*link != c)
            link = &(*link)->live_next;
        *link = c->live_next;
        free(c);
    }
    *slot = NULL;
    return result(error, MC_CLIENT_CANVAS_OK, NULL);
}

mc_client_canvas_status mc_client_source_canvas_store_size(mc_client_source_canvas *c,
                                                           int32_t width, int32_t height,
                                                           mc_client_canvas_error *error) {
    if (c != NULL && !represented(c))
        return result(error, MC_CLIENT_CANVAS_NATIVE_STATE,
                      "foreign Canvas occurrence; no source dereference");
    if (c == NULL)
        return result(error, MC_CLIENT_CANVAS_ARGUMENT, "Canvas host-layout receiver");
    if (c->active)
        return result(error, MC_CLIENT_CANVAS_BUSY, "Canvas active host layout");
    const bool changed = c->width != width || c->height != height;
    c->width = width;
    c->height = height;
    if (changed && c->resize != NULL)
        c->resize(c->resize_context);
    return result(error, MC_CLIENT_CANVAS_OK, NULL);
}

mc_client_canvas_status mc_client_source_canvas_bind_peer(mc_client_source_canvas *c,
                                                          SDL_Window *window,
                                                          mc_client_canvas_error *error) {
    if (c != NULL && !represented(c))
        return result(error, MC_CLIENT_CANVAS_NATIVE_STATE,
                      "foreign Canvas occurrence; no source dereference");
    if (c == NULL || window == NULL || !SDL_IsMainThread())
        return result(error, MC_CLIENT_CANVAS_ARGUMENT, "Canvas real peer/main-thread binding");
    if (c->active || c->window != NULL || c->graphics != 0)
        return result(error, MC_CLIENT_CANVAS_BUSY, "Canvas existing/active peer");
    c->active = true;
    int width = 0, height = 0;
    const SDL_WindowID id = SDL_GetWindowID(window);
    const bool ok = id != 0 && SDL_GetWindowSize(window, &width, &height);
    if (ok) {
        c->window = window;
        c->window_id = id;
        c->width = width;
        c->height = height;
    }
    c->active = false;
    return result(error, ok ? MC_CLIENT_CANVAS_OK : MC_CLIENT_CANVAS_NATIVE_PLATFORM,
                  ok ? NULL : "Canvas actual peer extent");
}

mc_client_canvas_status mc_client_source_canvas_detach_peer(mc_client_source_canvas *c,
                                                            mc_client_canvas_error *error) {
    if (c != NULL && !represented(c))
        return result(error, MC_CLIENT_CANVAS_NATIVE_STATE,
                      "foreign Canvas occurrence; no source dereference");
    if (c == NULL || !SDL_IsMainThread())
        return result(error, MC_CLIENT_CANVAS_ARGUMENT, "Canvas detach receiver/main thread");
    if (c->active || c->graphics != 0 || c->holds != 1 || c->resize != NULL)
        return result(error, MC_CLIENT_CANVAS_BUSY, "Canvas retained or active peer");
    c->window = NULL;
    c->window_id = 0;
    return result(error, MC_CLIENT_CANVAS_OK, NULL);
}

mc_client_canvas_status mc_client_source_canvas_resize_event(mc_client_source_canvas *c,
                                                             const SDL_Event *event,
                                                             mc_client_canvas_error *error) {
    if (c != NULL && !represented(c))
        return result(error, MC_CLIENT_CANVAS_NATIVE_STATE,
                      "foreign Canvas occurrence; no source dereference");
    if (c == NULL || event == NULL || !SDL_IsMainThread())
        return result(error, MC_CLIENT_CANVAS_ARGUMENT, "Canvas retained resize argument");
    if (c->active)
        return result(error, MC_CLIENT_CANVAS_BUSY, "Canvas active resize");
    if (!live_peer(c) || event->type != SDL_EVENT_WINDOW_RESIZED ||
        event->window.windowID != c->window_id)
        return result(error, MC_CLIENT_CANVAS_NATIVE_STATE, "Canvas actual logical resize record");
    return mc_client_source_canvas_store_size(c, event->window.data1, event->window.data2, error);
}

static mc_client_canvas_status extent(const mc_client_source_canvas *c, int32_t *out, bool width,
                                      mc_client_canvas_error *error) {
    if (c != NULL && !represented(c))
        return result(error, MC_CLIENT_CANVAS_NATIVE_STATE,
                      "foreign Component occurrence; no source dereference");
    if (c == NULL)
        return result(error, MC_CLIENT_CANVAS_NULL_REFERENCE,
                      width ? "Component.getWidth null receiver"
                            : "Component.getHeight null receiver");
    if (out == NULL)
        return result(error, MC_CLIENT_CANVAS_ARGUMENT, "Canvas native extent output");
    *out = width ? c->width : c->height;
    return result(error, MC_CLIENT_CANVAS_OK, NULL);
}

mc_client_canvas_status mc_client_source_canvas_width(const mc_client_source_canvas *c,
                                                      int32_t *out, mc_client_canvas_error *error) {
    return extent(c, out, true, error);
}

mc_client_canvas_status mc_client_source_canvas_height(const mc_client_source_canvas *c,
                                                       int32_t *out,
                                                       mc_client_canvas_error *error) {
    return extent(c, out, false, error);
}

mc_client_canvas_status mc_client_source_canvas_displayable(const mc_client_source_canvas *c,
                                                            bool *out,
                                                            mc_client_canvas_error *error) {
    if (c != NULL && !represented(c))
        return result(error, MC_CLIENT_CANVAS_NATIVE_STATE,
                      "foreign Canvas occurrence; no source dereference");
    if (c == NULL)
        return result(error, MC_CLIENT_CANVAS_NULL_REFERENCE, "Canvas.isDisplayable null receiver");
    if (out == NULL || (c->window != NULL && !SDL_IsMainThread()))
        return result(error, MC_CLIENT_CANVAS_ARGUMENT, "Canvas displayability output/main thread");
    if (c->window != NULL && !live_peer(c))
        return result(error, MC_CLIENT_CANVAS_NATIVE_STATE, "Canvas bound peer lifetime violation");
    *out = live_peer(c);
    return result(error, MC_CLIENT_CANVAS_OK, NULL);
}

bool mc_client_source_canvas_inspect(const mc_client_source_canvas *c,
                                     mc_client_canvas_state *out) {
    if (!represented(c) || out == NULL)
        return false;
    *out = (mc_client_canvas_state){c->holds,     c->graphics, c->width,         c->height,
                                    c->window_id, c->active,   c->window != NULL};
    return true;
}

SDL_Window *mc_client_source_canvas_window(const mc_client_source_canvas *c) {
    return represented(c) && SDL_IsMainThread() && live_peer(c) ? c->window : NULL;
}

mc_client_canvas_status mc_client_source_canvas_get_graphics(mc_client_source_canvas *c,
                                                             mc_client_canvas_graphics **out,
                                                             mc_client_canvas_error *error) {
    if (c != NULL && !represented(c))
        return result(error, MC_CLIENT_CANVAS_NATIVE_STATE,
                      "foreign Canvas occurrence; no source dereference");
    if (c == NULL)
        return result(error, MC_CLIENT_CANVAS_NULL_REFERENCE, "Canvas.getGraphics null receiver");
    if (out == NULL || *out != NULL || (c->window != NULL && !SDL_IsMainThread()))
        return result(error, MC_CLIENT_CANVAS_ARGUMENT, "Canvas Graphics output/main thread");
    if (c->active)
        return result(error, MC_CLIENT_CANVAS_BUSY, "Canvas active Graphics acquisition");
    if (c->window == NULL)
        return result(error, MC_CLIENT_CANVAS_OK, NULL); /* Actual no-peer NULL result. */
    if (!live_peer(c))
        return result(error, MC_CLIENT_CANVAS_NATIVE_STATE, "Canvas bound Graphics peer lifetime");
    if (c->holds == SIZE_MAX || c->graphics == SIZE_MAX)
        return result(error, MC_CLIENT_CANVAS_LIMIT, "Canvas Graphics occurrence limit");
    mc_client_canvas_graphics *g = calloc(1, sizeof(*g));
    if (g == NULL)
        return result(error, MC_CLIENT_CANVAS_ALLOCATION, "Canvas Graphics native allocation");
    c->active = true;
    const mc_client_canvas_status acquired =
        mc_client_canvas_peer_graphics_acquire(c->window, &g->native_context, error);
    c->active = false;
    if (acquired != MC_CLIENT_CANVAS_OK) {
        free(g);
        return acquired;
    }
    ++c->holds;
    ++c->graphics;
    g->canvas = c;
    *out = g;
    return result(error, MC_CLIENT_CANVAS_OK, NULL);
}

mc_client_canvas_status mc_client_canvas_graphics_black(mc_client_canvas_graphics *g,
                                                        mc_client_canvas_error *error) {
    if (g == NULL)
        return result(error, MC_CLIENT_CANVAS_NULL_REFERENCE, "Graphics.setColor null receiver");
    if (g->active || g->canvas->active)
        return result(error, MC_CLIENT_CANVAS_BUSY, "Graphics active color store");
    g->black = true;
    return result(error, MC_CLIENT_CANVAS_OK, NULL);
}

mc_client_canvas_status mc_client_canvas_graphics_fill_rect(mc_client_canvas_graphics *g, int32_t x,
                                                            int32_t y, int32_t width,
                                                            int32_t height,
                                                            mc_client_canvas_error *error) {
    if (g == NULL)
        return result(error, MC_CLIENT_CANVAS_NULL_REFERENCE, "Graphics.fillRect null receiver");
    mc_client_source_canvas *c = g->canvas;
    if (!SDL_IsMainThread() || !live_peer(c) || !g->black)
        return result(error, MC_CLIENT_CANVAS_NATIVE_STATE, "Graphics actual black peer state");
    if (g->active || c->active)
        return result(error, MC_CLIENT_CANVAS_BUSY, "Graphics active fill");
    if (width <= 0 || height <= 0 || c->width <= 0 || c->height <= 0)
        return result(error, MC_CLIENT_CANVAS_OK, NULL);
    int64_t left = x, top = y, right = left + width, bottom = top + height;
    if (left < 0)
        left = 0;
    if (top < 0)
        top = 0;
    if (right > c->width)
        right = c->width;
    if (bottom > c->height)
        bottom = c->height;
    if (left >= right || top >= bottom)
        return result(error, MC_CLIENT_CANVAS_OK, NULL);
    c->active = g->active = true;
    const mc_client_canvas_status status = mc_client_canvas_peer_graphics_black(
        g->native_context, (int32_t)left, (int32_t)top, (int32_t)(right - left),
        (int32_t)(bottom - top), error);
    c->active = g->active = false;
    return status;
}

mc_client_canvas_status mc_client_canvas_graphics_dispose(mc_client_canvas_graphics **slot,
                                                          mc_client_canvas_error *error) {
    if (slot == NULL)
        return result(error, MC_CLIENT_CANVAS_ARGUMENT, "Graphics native dispose slot");
    mc_client_canvas_graphics *g = *slot;
    if (g == NULL)
        return result(error, MC_CLIENT_CANVAS_OK, NULL);
    mc_client_source_canvas *c = g->canvas;
    if (g->active || c->active)
        return result(error, MC_CLIENT_CANVAS_BUSY, "Graphics active dispose");
    if (c->graphics == 0 || c->holds < 2)
        return result(error, MC_CLIENT_CANVAS_NATIVE_STATE, "Graphics native occurrence state");
    c->active = g->active = true;
    const mc_client_canvas_status released =
        mc_client_canvas_peer_graphics_release(&g->native_context, error);
    c->active = g->active = false;
    if (released != MC_CLIENT_CANVAS_OK)
        return released;
    --c->graphics;
    --c->holds;
    free(g);
    *slot = NULL;
    return result(error, MC_CLIENT_CANVAS_OK, NULL);
}

mc_client_canvas_status
mc_client_source_canvas_resize_listener(mc_client_source_canvas *c,
                                        mc_client_canvas_resize_listener listener, void *context,
                                        mc_client_canvas_error *e) {
    if (c != NULL && !represented(c))
        return result(e, MC_CLIENT_CANVAS_NATIVE_STATE,
                      "foreign Canvas occurrence; no source dereference");
    if (c == NULL || c->active || !SDL_IsMainThread())
        return result(e, MC_CLIENT_CANVAS_BUSY, "Canvas active listener binding");
    if (listener != NULL && c->resize != NULL)
        return result(e, MC_CLIENT_CANVAS_BUSY, "Canvas existing Display listener");
    c->resize = listener;
    c->resize_context = context;
    return result(e, MC_CLIENT_CANVAS_OK, NULL);
}
