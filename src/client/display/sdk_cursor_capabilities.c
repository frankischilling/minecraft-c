#include <SDL3/SDL_init.h>
#include <SDL3/SDL_video.h>
/* Native bridge for actual SDK cursor capabilities. It loads only
 * the platform Xcursor library, never game code or the supplied SDK native. */
#include "sdk_input_internal.h"
#include <SDL3/SDL_loadso.h>
#include <SDL3/SDL_properties.h>
#ifdef __linux__
#include <X11/Xlib.h>
#endif

bool mc_client_sdk_native_cursor_capabilities(mc_client_sdk_input *o, int32_t *out,
                                              mc_client_sdk_input_error *error) {
    if (o == NULL || out == NULL || !o->active || !SDL_IsMainThread()) {
        mc_client_sdk_input_fail(error, MC_CLIENT_SDK_INPUT_INVALID_ARGUMENT,
                                 "native cursor-capability call domain", "org/lwjgl/input/Cursor",
                                 "getCapabilities", "()I", -1, true);
        return false;
    }
    if (o->platform.lwjgl_platform == 3) {
        *out = 1; /* Actual WindowsDisplay.getNativeCursorCapabilities0/1. */
        return true;
    }
#ifdef __linux__
    if (o->platform.lwjgl_platform == 1) {
        typedef Bool (*query_fn)(Display *);
        const SDL_PropertiesID properties = SDL_GetWindowProperties(o->window);
        Display *display =
            SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, NULL);
        if (display == NULL) {
            mc_client_sdk_input_fail(error, MC_CLIENT_SDK_INPUT_PLATFORM_FAILED,
                                     "actual native cursor-capability X11 Display",
                                     "org/lwjgl/input/Cursor", "getCapabilities", "()I", -1, true);
            return false;
        }
        SDL_SharedObject *library = SDL_LoadObject("libXcursor.so.1");
        query_fn argb =
            library == NULL ? NULL : (query_fn)SDL_LoadFunction(library, "XcursorSupportsARGB");
        query_fn animation =
            library == NULL ? NULL : (query_fn)SDL_LoadFunction(library, "XcursorSupportsAnim");
        if (argb == NULL || animation == NULL) {
            if (library != NULL)
                SDL_UnloadObject(library);
            mc_client_sdk_input_fail(error, MC_CLIENT_SDK_INPUT_UNSUPPORTED_PROFILE,
                                     "actual native Xcursor capability functions",
                                     "org/lwjgl/input/Cursor", "getCapabilities", "()I", -1, true);
            return false;
        }
        /* Verified 2.9.0 liblwjgl64.so416c4/416ce: both queries execute.
         * ARGB selects bits0/1 together; Anim independently adds bit2.
         * One-bit support is not fabricated for every X11 host. */
        const Bool supports_argb = argb(display);
        const Bool supports_animation = animation(display);
        *out = (supports_argb ? 3 : 0) | (supports_animation ? 4 : 0);
        SDL_UnloadObject(library);
        return true;
    }
#endif
    mc_client_sdk_input_fail(error, MC_CLIENT_SDK_INPUT_UNSUPPORTED_PROFILE,
                             "actual cursor-capability platform profile", "org/lwjgl/input/Cursor",
                             "getCapabilities", "()I", -1, true);
    return false;
}
