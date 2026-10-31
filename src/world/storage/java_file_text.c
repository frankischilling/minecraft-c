#include "java_file_text.h"
#include "java_file_internal.h"
#include <stdlib.h>
#include <string.h>

mc_java_file_status mc_file_decode_utf8(const char *input, size_t maximum_bytes,
                                        mc_world_info_name **out, mc_java_file_error *e) {
    size_t n = strlen(input);
    if (n > SIZE_MAX / sizeof(uint16_t) || n * sizeof(uint16_t) > maximum_bytes)
        return mc_file_result(e, MC_JAVA_FILE_LIMIT, "File native name decoding bound", 0);
    uint16_t *u = malloc((n == 0 ? 1u : n) * sizeof(*u));
    if (u == NULL)
        return mc_file_result(e, MC_JAVA_FILE_ALLOCATION_FAILED, "File native name decoding", 0);
    size_t count = 0;
    for (size_t i = 0; i < n;) {
        uint32_t c = (unsigned char)input[i++];
        unsigned need = 0;
        uint32_t minimum = 0;
        if (c < 0x80u) {
        } else if (c >= 0xc2u && c <= 0xdfu) {
            c &= 31u;
            need = 1;
            minimum = 0x80u;
        } else if (c >= 0xe0u && c <= 0xefu) {
            c &= 15u;
            need = 2;
            minimum = 0x800u;
        } else if (c >= 0xf0u && c <= 0xf4u) {
            c &= 7u;
            need = 3;
            minimum = 0x10000u;
        } else
            goto unknown;
        if (need > n - i)
            goto unknown;
        for (unsigned j = 0; j < need; ++j) {
            unsigned ch = (unsigned char)input[i++];
            if ((ch & 0xc0u) != 0x80u)
                goto unknown;
            c = (c << 6u) | (ch & 63u);
        }
        if (c < minimum || c > 0x10ffffu || (c >= 0xd800u && c <= 0xdfffu))
            goto unknown;
        if (c < 0x10000u)
            u[count++] = (uint16_t)c;
        else {
            c -= 0x10000u;
            u[count++] = (uint16_t)(0xd800u + (c >> 10u));
            u[count++] = (uint16_t)(0xdc00u + (c & 1023u));
        }
    }
    mc_world_info_status copied =
        mc_world_info_name_copy((mc_nbt_string){u, count}, maximum_bytes, out);
    free(u);
    return copied == MC_WORLD_INFO_OK
               ? MC_JAVA_FILE_OK
               : mc_file_result(e,
                                copied == MC_WORLD_INFO_LIMIT ? MC_JAVA_FILE_LIMIT
                                : copied == MC_WORLD_INFO_ALLOCATION_FAILED
                                    ? MC_JAVA_FILE_ALLOCATION_FAILED
                                    : MC_JAVA_FILE_STATE_ERROR,
                                "File native name String", 0);
unknown:
    free(u);
    return mc_file_result(e, MC_JAVA_FILE_UNSUPPORTED_PROVIDER,
                          "File nonUTF8 native filename decoding", 0);
}
