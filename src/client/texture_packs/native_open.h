#ifndef MC_CLIENT_TEXTURE_PACKS_NATIVE_OPEN_H
#define MC_CLIENT_TEXTURE_PACKS_NATIVE_OPEN_H
#include "../../platform/files/directory.h"
#include "mc/client/texture_packs/texture_packs.h"
#include <stdio.h>
/* Shared actual open/unbuffered kernel. Path is borrowed transport only. */
FILE *texture_file_open_native(const mc_file_native_char *, mc_texture_pack_error *);
#endif
