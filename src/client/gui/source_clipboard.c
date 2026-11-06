#include "source_clipboard.h"
#include "source_screen_internal.h"

#include <SDL3/SDL.h>
#include <stdlib.h>
#include <string.h>

bool mc_client_screen_clipboard_read(void *context, mc_player_string **out,
                                     mc_client_screen_error *error) {
    (void)context;
    if (out == NULL || *out != NULL || error == NULL)
        return false;
    if (!SDL_IsMainThread()) {
        mc_screen_fail(error, MC_CLIENT_SCREEN_BUSY, "DA clipboard native video-thread admission",
                       "da", "d", "()Ljava/lang/String;", 0);
        return false;
    }
    /* DA.d returns NULL when no String data is available. RO substitutes its
     * own empty literal only after this nullable source call returns. */
    if (!SDL_HasClipboardText())
        return true;
    char *utf8 = SDL_GetClipboardText();
    if (utf8 == NULL)
        return true;
    const char *input = utf8;
    char *wide = SDL_iconv_string("UTF16LE", "UTF8", input, strlen(input) + 1);
    SDL_free(utf8);
    if (wide == NULL) {
        mc_screen_fail(error, MC_CLIENT_SCREEN_ALLOCATION_FAILED,
                       "DA clipboard native UTF16 conversion", "da", "d", "()Ljava/lang/String;",
                       0);
        return false;
    }
    const unsigned char *bytes = (const unsigned char *)wide;
    size_t length = 0;
    while (bytes[length * 2] != 0 || bytes[length * 2 + 1] != 0)
        ++length;
    uint16_t *units = length == 0 ? NULL : malloc(length * sizeof(*units));
    if (length != 0 && units == NULL) {
        SDL_free(wide);
        mc_screen_fail(error, MC_CLIENT_SCREEN_ALLOCATION_FAILED,
                       "DA clipboard native String argument", "da", "d", "()Ljava/lang/String;", 0);
        return false;
    }
    for (size_t i = 0; i < length; ++i)
        units[i] = (uint16_t)((uint16_t)bytes[2 * i] | (uint16_t)bytes[2 * i + 1] << 8);
    error->string = mc_player_string_create(units, length, out);
    free(units);
    SDL_free(wide);
    if (error->string != MC_PLAYER_OK) {
        mc_screen_fail(error, MC_CLIENT_SCREEN_STRING_FAILED,
                       "DA clipboard canonical String allocation", "da", "d",
                       "()Ljava/lang/String;", 0);
        return false;
    }
    return true;
}
