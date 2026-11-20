#include "../../world/storage/java_file_native.h"
#include "internal.h"
#include "native_open.h"
#include <stdlib.h>

FILE *texture_pack_file_open(mc_client_texture_pack *pack, mc_texture_pack_error *error) {
    if (pack->source_file == NULL)
        return texture_file_open(pack->file, error);
    mc_file_native_char *path = NULL;
    mc_java_file_status status =
        mc_java_file_native_path_copy(pack->source_file, &path, &pack->file_error);
    if (status != MC_JAVA_FILE_OK) {
        texture_source_file_error(status, &pack->file_error, error, 0);
        error->source_class = NULL;
        error->source_method = NULL;
        error->source_descriptor = NULL; /* Actual vp caller supplies its reached source frame. */
        return NULL;
    }
    /* Path is ephemeral native transport, never vp.h or an extra source File.
     * SAME pack->source_file stays retained through actual ZIP retirement. */
    FILE *file = texture_file_open_native(path, error);
    free(path);
    return file;
}
