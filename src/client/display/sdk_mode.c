#include "sdk_mode_internal.h"

#include <stdlib.h>

static bool failure(mc_client_display_stage_error *e, mc_client_display_failure_kind kind,
                    mc_client_display_status status, const char *operation) {
    if (e != NULL) {
        *e = (mc_client_display_stage_error){0};
        e->kind = kind;
        e->operation = operation;
        e->display.status = status;
        e->display.operation = operation;
        e->site = (mc_client_display_site){"org/lwjgl/opengl/DisplayMode", "<init>", "(II)V", -1};
    }
    return false;
}

bool mc_client_display_mode_allocate(mc_client_display_mode **out,
                                     mc_client_display_stage_error *e) {
    if (e != NULL)
        *e = (mc_client_display_stage_error){0};
    if (out == NULL)
        return failure(e, MC_CLIENT_DISPLAY_NATIVE_POLICY, MC_CLIENT_DISPLAY_INVALID_ARGUMENT,
                       "Mode native allocation output");
    mc_client_display_mode *mode = calloc(1, sizeof(*mode));
    if (mode == NULL)
        return failure(e, MC_CLIENT_DISPLAY_NATIVE_ALLOCATION, MC_CLIENT_DISPLAY_ALLOCATION_FAILED,
                       "Mode actual immutable owner allocation");
    mode->holds = 1;
    mode->last_store_pc = -1;
    *out = mode;
    return true;
}

bool mc_client_display_mode_construct(mc_client_display_mode *mode, int32_t width, int32_t height,
                                      mc_client_display_stage_error *e) {
    if (e != NULL)
        *e = (mc_client_display_stage_error){0};
    if (mode == NULL || mode->constructed)
        return failure(e, MC_CLIENT_DISPLAY_NATIVE_POLICY, MC_CLIENT_DISPLAY_INVALID_ARGUMENT,
                       "Mode RAW once-only constructor admission");
    /* Exact primitive stores of (IIIIZ), reached by (II). No geometry, native
     * window bound, validation, clamp or source Display active use occurs. */
    mode->width = width;
    mode->last_store_pc = 6;
    mode->height = height;
    mode->last_store_pc = 11;
    mode->bits_per_pixel = 0;
    mode->last_store_pc = 16;
    mode->frequency = 0;
    mode->last_store_pc = 22;
    mode->fullscreen_capable = false;
    mode->last_store_pc = 28;
    mode->constructed = true;
    return true;
}

bool mc_client_display_mode_retain(mc_client_display_mode *mode) {
    if (mode == NULL || mode->holds == SIZE_MAX)
        return false;
    ++mode->holds;
    return true;
}

void mc_client_display_mode_release(mc_client_display_mode **slot) {
    if (slot != NULL && *slot != NULL) {
        mc_client_display_mode *mode = *slot;
        if (mode->holds == 1)
            free(mode);
        else
            --mode->holds;
        *slot = NULL;
    }
}

bool mc_client_display_mode_inspect(const mc_client_display_mode *mode,
                                    mc_client_display_mode_state *out) {
    if (mode == NULL || out == NULL)
        return false;
    *out = (mc_client_display_mode_state){
        mode->holds,  mode->constructed,    mode->fullscreen_capable, mode->width,
        mode->height, mode->bits_per_pixel, mode->frequency,          mode->last_store_pc};
    return true;
}

bool mc_client_display_mode_platform(int32_t width, int32_t height, int32_t bpp, int32_t frequency,
                                     mc_client_display_mode **out,
                                     mc_client_display_stage_error *e) {
    mc_client_display_mode *mode = NULL;
    if (!mc_client_display_mode_allocate(&mode, e))
        return false;
    mode->width = width;
    mode->last_store_pc = 6;
    mode->height = height;
    mode->last_store_pc = 11;
    mode->bits_per_pixel = bpp;
    mode->last_store_pc = 16;
    mode->frequency = frequency;
    mode->last_store_pc = 22;
    mode->fullscreen_capable = true;
    mode->last_store_pc = 28;
    mode->constructed = true;
    *out = mode;
    return true;
}
