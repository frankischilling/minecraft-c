#ifndef MC_CLIENT_APPLICATION_ACCESS_H
#define MC_CLIENT_APPLICATION_ACCESS_H
#include "mc/client/gui/achievement_notice.h"
#include "mc/client/settings/game_settings.h"
typedef struct mc_client_stats_writer mc_client_stats_writer;
/* Typed access to the existing complete application owner. The concrete body
 * reads its live z/settings field on every kh invocation. */
mc_game_settings *mc_client_application_live_settings(const client_application *);
mc_client_stats_writer *mc_client_application_live_stats_writer(const client_application *);
mc_client_achievement_notice *
mc_client_application_live_achievement_notice(const client_application *);
mc_client_notice_status mc_client_application_install_inventory_callback(client_application *,
                                                                         mc_client_notice_error *);
#endif
