#ifndef MC_CLIENT_DISPLAY_SDK_CURSOR_INTERNAL_H
#define MC_CLIENT_DISPLAY_SDK_CURSOR_INTERNAL_H

#include "sdk_cursor.h"

typedef struct mc_client_sdk_cursor_platform {
    int32_t platform;
    bool one_bit_supported;
    unsigned best_width, best_height;
} mc_client_sdk_cursor_platform;

bool mc_client_sdk_cursor_platform_query(SDL_Window *, mc_client_sdk_cursor_platform *,
                                         mc_client_sdk_cursor_error *);

#endif
