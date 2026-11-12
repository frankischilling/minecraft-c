#include "frame.h"
#include "../startup/application_startup.h"
#include "frame_platform.h"
#include <SDL3/SDL.h>
#include <stdlib.h>

struct mc_client_source_frame {
    SDL_Window *window, *canvas_peer;
    mc_client_source_canvas *canvas;
    mc_client_event_port *events;
    client_application *app;
    bool video, active, failed, close_dispatched;
    mc_client_frame_error failure;
};

static bool fail(mc_client_frame_error *e, mc_client_canvas_status status, const char *operation) {
    e->status = status;
    e->operation = operation;
    return false;
}

static bool native_event(void *context, const SDL_Event *event) {
    mc_client_source_frame *f = context;
    mc_client_frame_error *e = &f->failure;
    if (f->failed)
        return false;
    f->active = true;
    bool ok = true;
    if (event->type == SDL_EVENT_WINDOW_RESIZED) {
        /* Frame client size is the Center Canvas size. Preserve signed source
         * Component dimensions; only native layout rejects impossible sizes. */
        ok = mc_client_source_canvas_store_size(f->canvas, event->window.data1, event->window.data2,
                                                &e->canvas) == MC_CLIENT_CANVAS_OK;
        if (ok && event->window.data1 > 0 && event->window.data2 > 0)
            ok = SDL_SetWindowSize(f->canvas_peer, event->window.data1, event->window.data2);
    } else if (event->type == SDL_EVENT_WINDOW_CLOSE_REQUESTED || event->type == SDL_EVENT_QUIT) {
        if (!f->close_dispatched && f->app != NULL) {
            /* KJ.windowClosing invokes App.f before join/System.exit. Here the
             * same main-thread game finishes its current call, then the caller
             * performs normal checked terminal teardown. Never ungrab here. */
            e->source_stop_status = (int)mc_client_application_source_stop(f->app, NULL);
            ok = e->source_stop_status == MC_CLIENT_STARTUP_OK;
            if (ok)
                f->close_dispatched = true;
        }
    } else if (event->type == SDL_EVENT_WINDOW_FOCUS_GAINED) {
        const SDL_WindowFlags flags =
            SDL_GetWindowFlags(f->window) | SDL_GetWindowFlags(f->canvas_peer);
        if ((flags & (SDL_WINDOW_HIDDEN | SDL_WINDOW_MINIMIZED)) == 0)
            ok = mc_client_frame_native_focus(f->canvas_peer, e);
    }
    f->active = false;
    if (!ok) {
        f->failed = true;
        if (e->operation == NULL)
            fail(e, MC_CLIENT_CANVAS_NATIVE_PLATFORM, "Frame native event listener");
    }
    return ok;
}

bool mc_client_source_frame_open(int32_t width, int32_t height, mc_client_source_frame **out,
                                 mc_client_frame_error *e) {
    if (e == NULL)
        return false;
    *e = (mc_client_frame_error){0};
    if (out == NULL || *out != NULL || width <= 0 || height <= 0 || !SDL_IsMainThread())
        return fail(e, MC_CLIENT_CANVAS_ARGUMENT, "Frame main-thread empty output/extent");
    mc_client_source_frame *f = calloc(1, sizeof(*f));
    if (f == NULL)
        return fail(e, MC_CLIENT_CANVAS_ALLOCATION, "Frame allocation");
    *out = f;
    if (mc_client_source_canvas_allocate(&f->canvas, &e->canvas) != MC_CLIENT_CANVAS_OK)
        goto failed;
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO))
        goto failed;
    f->video = true;
    f->window =
        SDL_CreateWindow("Minecraft", width, height, SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE);
    if (f->window == NULL ||
        !SDL_SetWindowPosition(f->window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED))
        goto failed;
    /* Real peer prerequisite, not SDK class initialization. The Canvas peer
     * needs the default SDK-capable visual before Graphics or a GL context.
     * The source Display request remains separately reached during startup. */
    SDL_GL_ResetAttributes();

    const struct {
        SDL_GLAttr key;
        int value;
    } a[] = {{SDL_GL_CONTEXT_MAJOR_VERSION, 3},
             {SDL_GL_CONTEXT_MINOR_VERSION, 3},
             {SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE},
             {SDL_GL_RED_SIZE, 8},
             {SDL_GL_GREEN_SIZE, 8},
             {SDL_GL_BLUE_SIZE, 8},
             {SDL_GL_ALPHA_SIZE, 0},
             {SDL_GL_DEPTH_SIZE, 8},
             {SDL_GL_STENCIL_SIZE, 0},
             {SDL_GL_DOUBLEBUFFER, 1},
             {SDL_GL_MULTISAMPLEBUFFERS, 0},
             {SDL_GL_MULTISAMPLESAMPLES, 0},
             {SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, 0}};

    for (size_t i = 0; i < sizeof(a) / sizeof(*a); ++i)
        if (!SDL_GL_SetAttribute(a[i].key, a[i].value))
            goto failed;
    f->canvas_peer =
        SDL_CreateWindow("", width, height,
                         SDL_WINDOW_HIDDEN | SDL_WINDOW_OPENGL | SDL_WINDOW_BORDERLESS |
                             SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (f->canvas_peer == NULL || !mc_client_frame_native_embed(f->window, f->canvas_peer, e))
        goto failed;
    if (mc_client_source_canvas_bind_peer(f->canvas, f->canvas_peer, &e->canvas) !=
        MC_CLIENT_CANVAS_OK)
        goto failed;
    if (mc_client_events_attach(f->window, 0, &f->events, &e->events) != MC_CLIENT_EVENTS_OK ||
        mc_client_events_bind_window_dispatch(f->events, native_event, f, &e->events) !=
            MC_CLIENT_EVENTS_OK)
        goto failed;
    return true;
failed:
    f->failed = true;
    if (e->operation == NULL)
        fail(e, MC_CLIENT_CANVAS_NATIVE_PLATFORM, "Frame/Canvas real peer creation");
    f->failure = *e;
    return false;
}

mc_client_source_canvas *mc_client_source_frame_canvas(mc_client_source_frame *f) {
    return f == NULL ? NULL : f->canvas;
}

bool mc_client_source_frame_bind_app(mc_client_source_frame *f, client_application *app,
                                     mc_client_frame_error *e) {
    if (f == NULL || f->active || !SDL_IsMainThread())
        return fail(e, MC_CLIENT_CANVAS_BUSY, "Frame App borrow binding");
    f->app = app;
    return true;
}

bool mc_client_source_frame_show(mc_client_source_frame *f, bool hidden, mc_client_frame_error *e) {
    if (f == NULL || f->active || f->failed || !SDL_IsMainThread())
        return fail(e, MC_CLIENT_CANVAS_BUSY, "Frame show live owner");
    if (hidden)
        return true;
    if (!SDL_ShowWindow(f->window) || !SDL_ShowWindow(f->canvas_peer))
        return fail(e, MC_CLIENT_CANVAS_NATIVE_PLATFORM, "Frame/Canvas show");
    return true;
}

bool mc_client_source_frame_retire(mc_client_source_frame **slot, mc_client_frame_error *e) {
    if (e == NULL || slot == NULL || !SDL_IsMainThread())
        return false;
    *e = (mc_client_frame_error){0};
    mc_client_source_frame *f = *slot;
    if (f == NULL)
        return true;
    if (f->active || f->app != NULL)
        return fail(e, MC_CLIENT_CANVAS_BUSY, "Frame active/App borrowed");
    mc_client_canvas_state state = {0};
    if (f->canvas != NULL && (!mc_client_source_canvas_inspect(f->canvas, &state) ||
                              state.holds != 1 || state.graphics != 0 || state.active))
        return fail(e, MC_CLIENT_CANVAS_BUSY, "Frame Canvas has live borrowers/Graphics");
    if (f->events != NULL && mc_client_events_retire(&f->events, &e->events) != MC_CLIENT_EVENTS_OK)
        return fail(e, MC_CLIENT_CANVAS_BUSY, "Frame retained event port");
    if (f->canvas != NULL && state.peer_bound &&
        mc_client_source_canvas_detach_peer(f->canvas, &e->canvas) != MC_CLIENT_CANVAS_OK)
        return false;
    if (f->canvas_peer != NULL) {
        SDL_DestroyWindow(f->canvas_peer);
        f->canvas_peer = NULL;
    }
    if (f->canvas != NULL &&
        mc_client_source_canvas_release(&f->canvas, &e->canvas) != MC_CLIENT_CANVAS_OK)
        return false;
    if (f->window != NULL)
        SDL_DestroyWindow(f->window);
    if (f->video)
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
    free(f);
    *slot = NULL;
    return true;
}

bool mc_client_source_frame_inspect(const mc_client_source_frame *f, mc_client_frame_state *out) {
    if (f == NULL || out == NULL)
        return false;
    *out = (mc_client_frame_state){f->window, f->canvas_peer, f->canvas,           f->app,
                                   f->active, f->failed,      f->close_dispatched, &f->failure};
    return true;
}
