#ifndef MC_CLIENT_TEXTURE_PACKS_IMAGE_IO_H
#define MC_CLIENT_TEXTURE_PACKS_IMAGE_IO_H
#include "mc/client/texture_packs/texture_packs.h"
/* Actual existing shared-core Pack adapter. It does not close the source stream.
 * Actual i/vp/ji wrappers own their different source close/exception ordering. */
mc_texture_pack_status texture_pack_image_read(mc_client_texture_stream *, mc_client_argb_image **,
                                               mc_texture_pack_error *);
#endif
