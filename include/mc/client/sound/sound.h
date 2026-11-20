#ifndef MC_CLIENT_SOUND_SOUND_H
#define MC_CLIENT_SOUND_SOUND_H

#include "mc/core/java/random_profile.h"

#include <stddef.h>

typedef struct mc_sound_text {
    const uint16_t *units;
    size_t length;
    bool is_null;
} mc_sound_text;

/* A borrowed, actually resolved file identity. URL.path retains its counted
 * UTF-16 units, including escapes; it is not inferred from the native path.
 * The resolver owns these views. Owners retaining an identity copy both. */
typedef struct mc_sound_file_identity {
    const char *native_utf8_path;
    mc_sound_text url_path;
} mc_sound_file_identity;

typedef enum mc_sound_status {
    MC_SOUND_OK = 0,
    MC_SOUND_INVALID_ARGUMENT,
    MC_SOUND_LIMIT,
    MC_SOUND_ALLOCATION_FAILED,
    MC_SOUND_IO_FAILED,
    MC_SOUND_UNSUPPORTED_PATH,
    MC_SOUND_SOURCE_EXCEPTION,
    MC_SOUND_BUSY,
    MC_SOUND_RANDOM_FAILED
} mc_sound_status;

typedef enum mc_sound_source_exception {
    MC_SOUND_EXCEPTION_NONE = 0,
    MC_SOUND_EXCEPTION_NULL_NAME,
    MC_SOUND_EXCEPTION_NULL_FILE,
    MC_SOUND_EXCEPTION_STRING_INDEX,
    MC_SOUND_EXCEPTION_RANDOM_BOUND,
    MC_SOUND_EXCEPTION_MALFORMED_URL
} mc_sound_source_exception;

/* source_pc is -1 for native admission failures without a reached source PC.
 * A native error is not an invented Java exception. Operation strings are
 * static borrowed storage; successful calls report status OK. */
typedef struct mc_sound_error {
    mc_sound_status status;
    const char *operation;
    int system_error;
    int32_t source_pc;
    mc_sound_source_exception exception;
    mc_java_random_profile_status random_status;
} mc_sound_error;

#endif
