#include "application.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool integer(const char *value, int64_t minimum, int64_t maximum, int64_t *out) {
    if (value == NULL || value[0] == '\0')
        return false;
    char *end = NULL;
    errno = 0;
    const long long parsed = strtoll(value, &end, 10);
    if (errno != 0 || end == value || *end != '\0' || parsed < minimum || parsed > maximum)
        return false;
    *out = (int64_t)parsed;
    return true;
}

static void usage(void) {
    puts("minecraft-client --assets IMPORT_DIRECTORY [--username NAME]\n"
         "  --width N --height N --gui-scale N\n"
         "  --frames N --capture OUTPUT.png --hidden --time-millis N");
}

int main(int argc, char **argv) {
    mc_client_application_options options = {.width = 854, .height = 480};
    for (int i = 1; i < argc; ++i) {
        const char *option = argv[i];
        if (strcmp(option, "--help") == 0) {
            usage();
            return 0;
        }
        if (strcmp(option, "--hidden") == 0) {
            options.hidden = true;
            continue;
        }
        if (i + 1 >= argc) {
            usage();
            return 2;
        }
        const char *value = argv[++i];
        int64_t parsed = 0;
        if (strcmp(option, "--assets") == 0)
            options.assets = value;
        else if (strcmp(option, "--username") == 0)
            options.username = value;
        else if (strcmp(option, "--capture") == 0)
            options.capture = value;
        else if (strcmp(option, "--width") == 0 && integer(value, 1, 16384, &parsed))
            options.width = (int32_t)parsed;
        else if (strcmp(option, "--height") == 0 && integer(value, 1, 16384, &parsed))
            options.height = (int32_t)parsed;
        else if (strcmp(option, "--gui-scale") == 0 &&
                 integer(value, INT32_MIN, INT32_MAX, &parsed))
            options.gui_scale = (int32_t)parsed;
        else if (strcmp(option, "--frames") == 0 && integer(value, 1, UINT32_MAX, &parsed))
            options.frame_limit = (uint32_t)parsed;
        else if (strcmp(option, "--time-millis") == 0 &&
                 integer(value, INT64_MIN, INT64_MAX, &parsed)) {
            options.time_millis = parsed;
            options.fixed_time = true;
        } else {
            fprintf(stderr, "Invalid client option: %s\n", option);
            return 2;
        }
    }
    if (options.assets == NULL || options.assets[0] == '\0' ||
        (options.hidden && options.frame_limit == 0)) {
        usage();
        return 2;
    }
    return mc_client_application_run(&options);
}
