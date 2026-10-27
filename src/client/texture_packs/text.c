#include "internal.h"
#include <stdlib.h>
#include <string.h>

bool texture_text_equal(mc_texture_pack_text a, mc_texture_pack_text b) {
    return a.is_null == b.is_null && a.length == b.length &&
           (a.length == 0 || memcmp(a.units, b.units, a.length * sizeof(uint16_t)) == 0);
}

void texture_string_free(mc_client_texture_string *text) {
    if (text != NULL && --text->holds == 0) {
        if (text->backing != NULL)
            mc_world_info_name_release(text->backing);
        else
            free(text->units);
        free(text);
    }
}

mc_texture_pack_status texture_string_retain(mc_client_texture_string *text) {
    if (text == NULL)
        return MC_TEXTURE_PACK_INVALID_ARGUMENT;
    if (text->holds == SIZE_MAX)
        return MC_TEXTURE_PACK_LIMIT;
    ++text->holds;
    return MC_TEXTURE_PACK_OK;
}

mc_client_texture_string *texture_string_copy(mc_texture_pack_text text, size_t limit,
                                              mc_texture_pack_error *error) {
    if (text.is_null)
        return NULL;
    if ((text.units == NULL && text.length != 0) || text.length > limit ||
        text.length > SIZE_MAX / sizeof(uint16_t)) {
        texture_pack_fail(error, MC_TEXTURE_PACK_LIMIT, "Texture pack String native bound", NULL,
                          NULL, NULL, 0, 0);
        return NULL;
    }
    mc_client_texture_string *owned = calloc(1, sizeof(*owned));
    if (owned != NULL) {
        owned->holds = 1;
        owned->units = malloc(text.length == 0 ? sizeof(uint16_t) : text.length * sizeof(uint16_t));
    }
    if (owned == NULL || owned->units == NULL) {
        texture_string_free(owned);
        texture_pack_fail(error, MC_TEXTURE_PACK_ALLOCATION_FAILED, "Texture pack String owner",
                          NULL, NULL, NULL, 0, 0);
        return NULL;
    }
    if (text.length != 0)
        memcpy(owned->units, text.units, text.length * sizeof(uint16_t));
    owned->length = text.length;
    return owned;
}

mc_client_texture_string *texture_string_utf8(const char *bytes, size_t limit,
                                              mc_texture_pack_error *error) {
    const size_t length = strlen(bytes);
    if ((limit <= SIZE_MAX / 3 && length > limit * 3) || length > SIZE_MAX / sizeof(uint16_t)) {
        texture_pack_fail(error, MC_TEXTURE_PACK_LIMIT, "Texture pack UTF8 text bound", NULL, NULL,
                          NULL, 0, 0);
        return NULL;
    }
    uint16_t *units = malloc((length == 0 ? 1 : length) * sizeof(uint16_t));
    if (units == NULL) {
        texture_pack_fail(error, MC_TEXTURE_PACK_ALLOCATION_FAILED, "Texture pack UTF8 text", NULL,
                          NULL, NULL, 0, 0);
        return NULL;
    }
    size_t count = 0, at = 0;
    while (at < length) {
        const uint8_t first = (uint8_t)bytes[at++];
        uint32_t value = first;
        unsigned int trailing = 0;
        if (first >= 0xc2 && first <= 0xdf) {
            value = first & 31u;
            trailing = 1;
        } else if (first >= 0xe0 && first <= 0xef) {
            value = first & 15u;
            trailing = 2;
        } else if (first >= 0xf0 && first <= 0xf4) {
            value = first & 7u;
            trailing = 3;
        } else if (first >= 0x80)
            goto unsupported;
        if (trailing > length - at)
            goto unsupported;
        for (unsigned int i = 0; i < trailing; ++i) {
            const uint8_t next = (uint8_t)bytes[at++];
            if ((next & 0xc0u) != 0x80u)
                goto unsupported;
            value = (value << 6) | (next & 63u);
        }
        if ((trailing == 1 && value < 0x80) || (trailing == 2 && value < 0x800) ||
            (trailing == 3 && value < 0x10000) || value > 0x10ffff ||
            (value >= 0xd800 && value <= 0xdfff))
            goto unsupported;
        if (value >= 0x10000) {
            value -= 0x10000;
            units[count++] = (uint16_t)(0xd800u + (value >> 10));
            units[count++] = (uint16_t)(0xdc00u + (value & 1023u));
        } else
            units[count++] = (uint16_t)value;
        if (count > limit) {
            free(units);
            texture_pack_fail(error, MC_TEXTURE_PACK_LIMIT, "Texture pack String native unit bound",
                              NULL, NULL, NULL, 0, 0);
            return NULL;
        }
    }
    mc_client_texture_string *owned = calloc(1, sizeof(*owned));
    if (owned == NULL) {
        free(units);
        texture_pack_fail(error, MC_TEXTURE_PACK_ALLOCATION_FAILED, "Texture pack String owner",
                          NULL, NULL, NULL, 0, 0);
        return NULL;
    }
    owned->units = units;
    owned->length = count;
    owned->holds = 1;
    return owned;
unsupported:
    free(units);
    texture_pack_fail(error, MC_TEXTURE_PACK_UNSUPPORTED_NATIVE,
                      "Texture pack native UTF8 encoding", NULL, NULL, NULL, 0, 0);
    return NULL;
}

char *texture_text_utf8(mc_texture_pack_text text, size_t limit, bool allow_nul,
                        mc_texture_pack_error *error) {
    if (text.is_null || (text.units == NULL && text.length != 0) || text.length > limit ||
        text.length > (SIZE_MAX - 1) / 3) {
        texture_pack_fail(error, MC_TEXTURE_PACK_INVALID_ARGUMENT, "Texture pack native text view",
                          NULL, NULL, NULL, 0, 0);
        return NULL;
    }
    char *bytes = malloc(text.length * 3 + 1);
    if (bytes == NULL) {
        texture_pack_fail(error, MC_TEXTURE_PACK_ALLOCATION_FAILED, "Texture pack UTF8 storage",
                          NULL, NULL, NULL, 0, 0);
        return NULL;
    }
    size_t count = 0;
    for (size_t i = 0; i < text.length; ++i) {
        uint32_t value = text.units[i];
        if (value == 0 && !allow_nul)
            goto unsupported;
        if (value >= 0xd800 && value <= 0xdbff) {
            if (i + 1 >= text.length || text.units[i + 1] < 0xdc00 || text.units[i + 1] > 0xdfff)
                goto unsupported;
            value = 0x10000u + ((value - 0xd800u) << 10) + (text.units[++i] - 0xdc00u);
        } else if (value >= 0xdc00 && value <= 0xdfff)
            goto unsupported;
        if (value < 0x80)
            bytes[count++] = (char)value;
        else if (value < 0x800) {
            bytes[count++] = (char)(0xc0u | (value >> 6));
            bytes[count++] = (char)(0x80u | (value & 63u));
        } else if (value < 0x10000) {
            bytes[count++] = (char)(0xe0u | (value >> 12));
            bytes[count++] = (char)(0x80u | ((value >> 6) & 63u));
            bytes[count++] = (char)(0x80u | (value & 63u));
        } else {
            bytes[count++] = (char)(0xf0u | (value >> 18));
            bytes[count++] = (char)(0x80u | ((value >> 12) & 63u));
            bytes[count++] = (char)(0x80u | ((value >> 6) & 63u));
            bytes[count++] = (char)(0x80u | (value & 63u));
        }
    }
    bytes[count] = 0;
    return bytes;
unsupported:
    free(bytes);
    texture_pack_fail(error, MC_TEXTURE_PACK_UNSUPPORTED_NATIVE,
                      "Texture pack native filename/resource encoding", NULL, NULL, NULL, 0, 0);
    return NULL;
}

bool mc_client_texture_string_read(const mc_client_texture_string *text,
                                   mc_texture_pack_text *out) {
    if (text == NULL || out == NULL)
        return false;
    *out = (mc_texture_pack_text){text->units, text->length, false};
    return true;
}
