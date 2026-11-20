#ifndef MC_PLATFORM_FILES_URL_PATH_H
#define MC_PLATFORM_FILES_URL_PATH_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
bool mc_url_path_separator(char);
bool mc_url_path_absolute(const char *, size_t);
bool mc_url_path_next_utf8(const char *, size_t, size_t *, uint32_t *);
/* Pure old sound File URI kernel. Caller supplies admitted UTF8 normalized
 * absolute native path and >=3*length+6 UTF16 units; no IO/allocation. */
bool mc_url_path_encode(const char *, size_t, bool, uint16_t *, size_t, size_t *);
#endif
