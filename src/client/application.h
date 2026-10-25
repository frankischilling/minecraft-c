#ifndef MC_CLIENT_APPLICATION_H
#define MC_CLIENT_APPLICATION_H

#include <stdbool.h>
#include <stdint.h>

typedef struct mc_client_application_options {
    const char *assets;
    const char *game_directory;
    const char *username;
    const char *capture;
    int32_t width, height, gui_scale;
    uint32_t frame_limit;
    bool hidden, fixed_time, gui_scale_override;
    int64_t time_millis;
} mc_client_application_options;

int mc_client_application_run(const mc_client_application_options *options);

#endif
