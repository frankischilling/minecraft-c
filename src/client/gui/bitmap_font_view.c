#include "bitmap_font_internal.h"

#include <stdlib.h>

mc_client_bitmap_font_status mc_client_bitmap_font_view_enter(mc_client_bitmap_font *font,
                                                              const mc_player_string_view *text,
                                                              const char *method,
                                                              const char *descriptor,
                                                              mc_client_bitmap_font_error **scratch,
                                                              mc_client_bitmap_font_error *error) {
    mc_client_bitmap_font_status status =
        mc_client_bitmap_font_admit(font, method, descriptor, error);
    if (status != MC_CLIENT_BITMAP_FONT_OK)
        return status;
    if (text == NULL || (text->length != 0 && text->units == NULL) || scratch == NULL)
        status = MC_CLIENT_BITMAP_FONT_INVALID_ARGUMENT;
    else if (text->length > 65536u || font->holds == SIZE_MAX)
        status = MC_CLIENT_BITMAP_FONT_LIMIT;
    if (status != MC_CLIENT_BITMAP_FONT_OK) {
        if (error != NULL)
            mc_client_bitmap_font_fail(error, status, "sj native held String view bound", method,
                                       descriptor, -1, true);
        return status;
    }
    mc_client_bitmap_font_error *e = calloc(1, sizeof(*e));
    if (e == NULL) {
        if (error != NULL)
            mc_client_bitmap_font_fail(error, MC_CLIENT_BITMAP_FONT_ALLOCATION_FAILED,
                                       "sj native call diagnostics", method, descriptor, -1, true);
        return MC_CLIENT_BITMAP_FONT_ALLOCATION_FAILED;
    }
    ++font->holds;
    font->active = true;
    *scratch = e;
    return MC_CLIENT_BITMAP_FONT_OK;
}

mc_client_bitmap_font_status mc_client_bitmap_font_view_leave(mc_client_bitmap_font *font,
                                                              mc_client_bitmap_font_error *e,
                                                              mc_client_bitmap_font_error *error) {
    font->active = false;
    --font->holds;
    const mc_client_bitmap_font_status status = e->status;
    if (error != NULL)
        *error = *e;
    free(e);
    return status;
}
