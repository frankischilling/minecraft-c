#include "mc/resources/image/image.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    if (argc != 2)
        return 2;
    FILE *input = fopen(argv[1], "rb");
    if (input == NULL)
        return 2;
    uint8_t *encoded = NULL;
    mc_image *image = NULL;
    int result = 2;
    if (fseek(input, 0, SEEK_END) != 0)
        goto cleanup;
    const long length = ftell(input);
    if (length < 0 || length > 16L * 1024L * 1024L || fseek(input, 0, SEEK_SET) != 0)
        goto cleanup;
    encoded = malloc(length == 0 ? 1u : (size_t)length);
    if (encoded == NULL || fread(encoded, 1, (size_t)length, input) != (size_t)length)
        goto cleanup;
    const mc_image_status status = mc_image_decode_png(encoded, (size_t)length, NULL, &image);
    if (status != MC_IMAGE_OK) {
        fprintf(stderr, "%s\n", mc_image_status_string(status));
        result = 1;
        goto cleanup;
    }
    mc_image_view view = {0};
    if (!mc_image_view_get(image, &view))
        goto cleanup;
    if (printf("MC_IMAGE_RGBA8_V1 %u %u %zu\n", view.width, view.height, view.stride) < 0)
        goto cleanup;
    for (size_t i = 0; i < view.stride * view.height; ++i)
        if (printf("%02x", (unsigned int)view.pixels[i]) < 0)
            goto cleanup;
    if (putchar('\n') == EOF || fflush(stdout) != 0)
        goto cleanup;
    result = 0;
cleanup:
    if (fclose(input) != 0)
        result = 2;
    free(encoded);
    mc_image_free(image);
    return result;
}
