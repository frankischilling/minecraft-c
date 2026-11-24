#ifndef MC_CLIENT_IMAGES_CLASS_URL_H
#define MC_CLIENT_IMAGES_CLASS_URL_H
#include "class_stream.h"
#include "mc/resources/files/files.h"
typedef struct mc_client_class_url mc_client_class_url;

typedef enum mc_client_class_url_status {
    MC_CLIENT_CLASS_URL_OK,
    MC_CLIENT_CLASS_URL_INVALID_ARGUMENT,
    MC_CLIENT_CLASS_URL_BUSY,
    MC_CLIENT_CLASS_URL_ALLOCATION_FAILED,
    MC_CLIENT_CLASS_URL_LIMIT,
    MC_CLIENT_CLASS_URL_SOURCE_ILLEGAL_ARGUMENT,
    MC_CLIENT_CLASS_URL_SOURCE_IO,
    MC_CLIENT_CLASS_URL_PROVIDER_FAILED,
    MC_CLIENT_CLASS_URL_IMAGE_FAILED,
    MC_CLIENT_CLASS_URL_UNIMPLEMENTED
} mc_client_class_url_status;

typedef struct mc_client_class_url_error {
    mc_client_class_url_status status;
    const char *operation;
    mc_registry_status locate;
    mc_player_status string;
    mc_client_class_stream_error stream;
    mc_client_image_io_error image;
    bool url_found, read_entered, source_close_reached, source_returned;
} mc_client_class_url_error;

/* Real imported-directory URL namespace, distinct from InputStream lookup.
 * resources must be this files owner's canonical source. Missing succeeds
 * with NULL URL; no source stream is opened until read_image. No Pack object.
 * Files/provider remain borrowed and live through every retained prefix. */
mc_client_class_url_status mc_client_class_url_create(mc_resource_files *,
                                                      const mc_registry_resources *,
                                                      const char *absolute_name,
                                                      mc_client_class_url **,
                                                      mc_client_class_url_error *);
/* One actual ImageIO(URL) attempt. Reached Java return/IOException includes
 * the URL overload's actual close-finally, unlike ImageIO(InputStream).
 * Native decoder/provider admission failures keep their reached stream for
 * explicit native cleanup and never fabricate a Java exception. */
mc_client_class_url_status mc_client_class_url_read_image(mc_client_class_url *,
                                                          const mc_client_image_io_limits *,
                                                          mc_client_argb_image **,
                                                          mc_client_class_url_error *);
mc_client_class_url_status mc_client_class_url_release(mc_client_class_url **,
                                                       mc_client_class_url_error *);
#endif
