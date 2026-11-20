/* Separately labelled native fault/source-order probe. Compile the unchanged
 * production notice body with only its platform clock leaf redirected. No
 * production callback/interface, replacement notice or fake Player is added. */
#include "../../../src/client/application_state.h"
#include "../../../src/platform/time/millis.h"

int mc_player_statistics_clock_mode;
client_application *mc_player_statistics_clock_app;
mc_client_stats_writer *mc_player_statistics_clock_writer;

mc_platform_millis_status mc_player_statistics_test_clock(int64_t *out) {
    if (mc_player_statistics_clock_mode == 1)
        return MC_PLATFORM_MILLIS_PLATFORM_FAILURE;
    if (mc_player_statistics_clock_mode == 2)
        mc_player_statistics_clock_app->stats_writer = NULL;
    if (mc_player_statistics_clock_mode == 3)
        mc_player_statistics_clock_app->stats_writer = mc_player_statistics_clock_writer;
    return mc_platform_current_time_millis(out);
}

#define mc_platform_current_time_millis mc_player_statistics_test_clock
#include "../../../src/client/gui/achievement_notice.c"
#undef mc_platform_current_time_millis
