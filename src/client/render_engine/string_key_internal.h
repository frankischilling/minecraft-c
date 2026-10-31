#ifndef MC_CLIENT_RENDER_ENGINE_STRING_KEY_INTERNAL_H
#define MC_CLIENT_RENDER_ENGINE_STRING_KEY_INTERNAL_H
#include "mc/client/texture_packs/texture_packs.h"

typedef enum mc_client_resource_key_status {
    MC_CLIENT_RESOURCE_KEY_OK,
    MC_CLIENT_RESOURCE_KEY_INVALID_ARGUMENT,
    MC_CLIENT_RESOURCE_KEY_LIMIT,
    MC_CLIENT_RESOURCE_KEY_ALLOCATION_FAILED
} mc_client_resource_key_status;

bool mc_client_resource_key_shape(mc_texture_pack_text);
bool mc_client_resource_key_equal(const uint16_t *, size_t, bool, mc_texture_pack_text);
bool mc_client_resource_key_starts(mc_texture_pack_text, const char *ascii_prefix, size_t length);
/* Shared immutable key-copy primitive. The actual b/c tables own separate
 * occurrences and publication sites. No detached key cache or String owner. */
mc_client_resource_key_status mc_client_resource_key_copy(mc_texture_pack_text, size_t max_units,
                                                          uint16_t **empty_slot);
#endif
