#ifndef MC_CLIENT_DISPLAY_SOURCE_TEXTURE_UPDATE_H
#define MC_CLIENT_DISPLAY_SOURCE_TEXTURE_UPDATE_H
#include "display.h"

/* Reached source GL calls, without an added glGetError or binding restore.
 * Numeric binds require an actual live generated resource in this Display.
 * Subimage reads the supplied direct-buffer remaining bytes without advancing
 * its cursor. Native unpack/PBO admission prevents an unsafe client-memory
 * interpretation; texture dimensions and offsets remain actual GL inputs. */
bool mc_client_display_source_texture_bind(mc_client_display *, mc_client_texture,
                                           mc_client_display_error *);
bool mc_client_display_source_texture_sub_image(mc_client_display *, int32_t level, int32_t x,
                                                int32_t y, int32_t width, int32_t height,
                                                const uint8_t *bytes, size_t remaining,
                                                mc_client_display_error *);
#endif
