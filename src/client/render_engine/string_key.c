#include "string_key_internal.h"
#include <stdlib.h>
#include <string.h>

bool mc_client_resource_key_shape(mc_texture_pack_text key) {
    return key.is_null ? key.units == NULL && key.length == 0
                       : key.length == 0 || key.units != NULL;
}

bool mc_client_resource_key_equal(const uint16_t *units, size_t length, bool is_null,
                                  mc_texture_pack_text key) {
    return is_null == key.is_null && length == key.length &&
           (length == 0 || memcmp(units, key.units, length * sizeof(*units)) == 0);
}

bool mc_client_resource_key_starts(mc_texture_pack_text key, const char *prefix, size_t length) {
    if (key.length < length)
        return false;
    for (size_t i = 0; i < length; ++i)
        if (key.units[i] != (uint16_t)(unsigned char)prefix[i])
            return false;
    return true;
}

mc_client_resource_key_status mc_client_resource_key_copy(mc_texture_pack_text key,
                                                          size_t max_units, uint16_t **slot) {
    if (slot == NULL || *slot != NULL || !mc_client_resource_key_shape(key))
        return MC_CLIENT_RESOURCE_KEY_INVALID_ARGUMENT;
    if (key.length > max_units || key.length > SIZE_MAX / sizeof(uint16_t))
        return MC_CLIENT_RESOURCE_KEY_LIMIT;
    if (key.length == 0)
        return MC_CLIENT_RESOURCE_KEY_OK;
    uint16_t *units = malloc(key.length * sizeof(*units));
    if (units == NULL)
        return MC_CLIENT_RESOURCE_KEY_ALLOCATION_FAILED;
    memcpy(units, key.units, key.length * sizeof(*units));
    *slot = units;
    return MC_CLIENT_RESOURCE_KEY_OK;
}
