/* Native Frame/Canvas focus and polled pointer; no World or prepared SDL state. */
#include "client/display/events.h"
#include "client/window/frame.h"
#include "client/window/frame_platform.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <windows.h>

static bool geometry(SDL_Window *window) {
    int x, y, width, height, wx, wy, ww, wh;
    const char *bounds = SDL_getenv("SDL_DISPLAY_USABLE_BOUNDS");
    return bounds != NULL && SDL_sscanf(bounds, "%d,%d,%d,%d", &x, &y, &width, &height) == 4 &&
           SDL_GetWindowPosition(window, &wx, &wy) && SDL_GetWindowSize(window, &ww, &wh) &&
           wx >= x && wy >= y && wx + ww <= x + width && wy + wh <= y + height;
}

static bool focus(mc_client_event_port *port, SDL_Window *frame, SDL_Window *canvas, size_t epoch) {
    mc_client_events_error events = {0};
    HWND hwnd = SDL_GetPointerProperty(SDL_GetWindowProperties(canvas),
                                       SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
    HWND frame_hwnd = SDL_GetPointerProperty(SDL_GetWindowProperties(frame),
                                             SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
    if (!SDL_RaiseWindow(frame))
        return false;
    SetFocus(frame_hwnd);
    if (GetFocus() != frame_hwnd || GetForegroundWindow() != frame_hwnd ||
        SDL_GetKeyboardFocus() != frame) {
        fprintf(stderr, "Embedded Frame activation failed at epoch=%zu\n", epoch);
        return false;
    }
    if (mc_client_events_pump(port, &events) != MC_CLIENT_EVENTS_OK)
        return false;
    SDL_WarpMouseInWindow(canvas, 426, 120);
    const Uint64 deadline = SDL_GetTicks() + 2000;
    do {
        if (mc_client_events_pump(port, &events) != MC_CLIENT_EVENTS_OK)
            return false;
        float x, y;
        SDL_GetMouseState(&x, &y);
        if (GetFocus() == hwnd && GetForegroundWindow() == GetAncestor(hwnd, GA_ROOT) &&
            SDL_GetKeyboardFocus() == canvas && SDL_GetMouseFocus() == canvas && x == 426 &&
            y == 120)
            return true;
        SDL_Delay(1);
    } while (SDL_GetTicks() < deadline);
    float x, y, gx, gy;
    SDL_GetMouseState(&x, &y);
    SDL_GetGlobalMouseState(&gx, &gy);
    DWORD foreground_pid = 0;
    HWND foreground = GetForegroundWindow();
    GetWindowThreadProcessId(foreground, &foreground_pid);
    fprintf(stderr,
            "Embedded focus epoch=%zu native-canvas=%d foreground-root=%d foreground-owned=%d "
            "keyboard=%u mouse=%u canvas=%u "
            "local=%g,%g global=%g,%g\n",
            epoch, GetFocus() == hwnd, foreground == GetAncestor(hwnd, GA_ROOT),
            foreground_pid == GetCurrentProcessId(), SDL_GetWindowID(SDL_GetKeyboardFocus()),
            SDL_GetWindowID(SDL_GetMouseFocus()), SDL_GetWindowID(canvas), (double)x, (double)y,
            (double)gx, (double)gy);
    return false;
}

static bool peer_focus(mc_client_event_port *port, SDL_Window *frame, SDL_Window *peer) {
    HWND hwnd = SDL_GetPointerProperty(SDL_GetWindowProperties(peer),
                                       SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
    HWND frame_hwnd = SDL_GetPointerProperty(SDL_GetWindowProperties(frame),
                                             SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
    mc_client_events_error events = {0};
    Uint64 deadline = SDL_GetTicks() + 2000;
    bool activated_before_pump = false;
    do {
        /* Queue a genuine Frame gain, then give the peer focus before the
         * owner retains and dispatches that batch. No synthetic focus event. */
        if (!SDL_RaiseWindow(frame))
            return false;
        SetFocus(frame_hwnd);
        if (GetFocus() != frame_hwnd || SDL_GetKeyboardFocus() != frame) {
            fprintf(stderr, "Embedded stale Frame setup failed\n");
            return false;
        }
        if (!SDL_RaiseWindow(peer))
            return false;
        SetFocus(hwnd);
        activated_before_pump =
            GetFocus() == hwnd && GetForegroundWindow() == hwnd && SDL_GetKeyboardFocus() == peer;
        if (mc_client_events_pump(port, &events) != MC_CLIENT_EVENTS_OK)
            return false;
        if (GetFocus() == hwnd && GetForegroundWindow() == hwnd && SDL_GetKeyboardFocus() == peer)
            return true;
        if (activated_before_pump)
            break; /* Do not hide a witnessed focus theft behind a retry. */
        SDL_Delay(1);
    } while (SDL_GetTicks() < deadline);
    DWORD pid = 0;
    HWND foreground = GetForegroundWindow();
    GetWindowThreadProcessId(foreground, &pid);
    fprintf(stderr,
            "Embedded focus peer activation failed native-peer=%d foreground-peer=%d "
            "foreground-owned=%d activated-before-pump=%d keyboard=%u peer=%u\n",
            GetFocus() == hwnd, foreground == hwnd, pid == GetCurrentProcessId(),
            activated_before_pump, SDL_GetWindowID(SDL_GetKeyboardFocus()), SDL_GetWindowID(peer));
    return false;
}

int main(void) {
    mc_client_source_frame *frame = NULL;
    mc_client_frame_error error = {0};
    mc_client_event_port *port = NULL;
    mc_client_events_error events = {0};
    SDL_Window *peer = NULL;
    SDL_GLContext gl = NULL;
    SDL_WindowID frame_id = 0, canvas_id = 0, peer_id = 0;
    bool passed = false;
    if (!SDL_Init(SDL_INIT_VIDEO) || !mc_client_source_frame_open(854, 480, &frame, &error))
        goto done;
    mc_client_frame_state host;
    if (!mc_client_source_frame_inspect(frame, &host) ||
        mc_client_events_attach(host.canvas_window, 0, &port, &events) != MC_CLIENT_EVENTS_OK ||
        !mc_client_source_frame_show(frame, false, &error))
        goto done;
    gl = SDL_GL_CreateContext(host.canvas_window);
    if (gl == NULL || !SDL_GL_MakeCurrent(host.canvas_window, gl))
        goto done;
    peer = SDL_CreateWindow("Focus peer", 180, 120, 0);
    if (peer == NULL || !geometry(peer) || !geometry(host.frame_window) ||
        !geometry(host.canvas_window))
        goto done;
    frame_id = SDL_GetWindowID(host.frame_window);
    canvas_id = SDL_GetWindowID(host.canvas_window);
    peer_id = SDL_GetWindowID(peer);
    for (size_t epoch = 0; epoch < 200; ++epoch) {
        if (!peer_focus(port, host.frame_window, peer) ||
            !focus(port, host.frame_window, host.canvas_window, epoch))
            goto done;
    }
    passed = true;
done:
    if (peer != NULL)
        SDL_DestroyWindow(peer);
    if (gl != NULL && !SDL_GL_DestroyContext(gl))
        passed = false;
    if (mc_client_events_retire(&port, &events) != MC_CLIENT_EVENTS_OK ||
        !mc_client_source_frame_retire(&frame, &error))
        passed = false;
    if (passed && (frame != NULL || SDL_GetWindowFromID(frame_id) != NULL ||
                   SDL_GetWindowFromID(canvas_id) != NULL || SDL_GetWindowFromID(peer_id) != NULL))
        passed = false;
    SDL_Quit();
    if (passed)
        puts("Embedded focus: 200 native focus/pointer transitions; cleanup complete");
    else
        fprintf(stderr, "Embedded focus failed: %s; Frame operation=%s\n", SDL_GetError(),
                error.operation == NULL ? "none" : error.operation);
    return passed ? 0 : 1;
}
