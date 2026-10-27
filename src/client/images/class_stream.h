#ifndef MC_CLIENT_IMAGES_CLASS_STREAM_H
#define MC_CLIENT_IMAGES_CLASS_STREAM_H
#include "image_io.h"
#include "mc/entity/player/player.h"
#include "mc/resources/translation/translation.h"

typedef struct mc_client_class_stream mc_client_class_stream;

typedef enum mc_client_class_stream_status {
    MC_CLIENT_CLASS_STREAM_OK,
    MC_CLIENT_CLASS_STREAM_INVALID_ARGUMENT,
    MC_CLIENT_CLASS_STREAM_LIMIT,
    MC_CLIENT_CLASS_STREAM_ALLOCATION_FAILED,
    MC_CLIENT_CLASS_STREAM_SOURCE_NULL_NAME,
    MC_CLIENT_CLASS_STREAM_PROVIDER_FAILED,
    MC_CLIENT_CLASS_STREAM_UNIMPLEMENTED,
    MC_CLIENT_CLASS_STREAM_BUSY,
    MC_CLIENT_CLASS_STREAM_NATIVE_CLOSE_FAILED
} mc_client_class_stream_status;

typedef struct mc_client_class_stream_error {
    mc_client_class_stream_status status;
    const char *operation;
    mc_registry_status resource, close;
    mc_java_properties_status read;
    size_t bytes_read;
    bool missing_stream, native_close_attempted;
} mc_client_class_stream_error;

/* Actual ji.class namespace (default Java package). Absolute/relative ASCII
 * names resolve to the same actual class-loader resource source; no Pack/i/ik
 * object or decoded-image callback is created. Nullable name reaches source
 * Class.getResourceAsStream NPE; absent resource succeeds with NULL stream.
 * Other native open/provider faults remain typed, outside fabricated catches.
 * The provider/stream contexts remain borrowed through every retained prefix.
 * Name is borrowed only during open; this owner keeps its own resolved path. */
mc_client_class_stream_status mc_client_class_stream_open(const mc_registry_resources *,
                                                          const mc_player_string *nullable_name,
                                                          size_t max_name, size_t max_read_bytes,
                                                          mc_client_class_stream **out,
                                                          mc_client_class_stream_error *);
mc_client_image_io_input mc_client_class_stream_image_input(mc_client_class_stream *);
bool mc_client_class_stream_inspect(const mc_client_class_stream *, mc_client_class_stream_error *);
/* Native file retirement, distinctly not source InputStream.close. ImageIO
 * itself leaves input open. The actual close callback runs under this owner's
 * active guard: nested read/retire returns BUSY without another provider call
 * or slot consumption. Read-only inspection remains valid. Failed close keeps
 * this owner and slot for explicit retry after the guard has been released.
 * Callbacks must not replace the slot or retire the borrowed resource source. */
mc_client_class_stream_status mc_client_class_stream_retire(mc_client_class_stream **,
                                                            mc_client_class_stream_error *);
#endif
