#ifndef MC_CLIENT_APPLICATION_ACCESS_H
#define MC_CLIENT_APPLICATION_ACCESS_H
#include "mc/client/gui/achievement_notice.h"
#include "mc/client/settings/game_settings.h"
typedef struct mc_client_stats_writer mc_client_stats_writer;
typedef struct mc_world_runtime mc_world_runtime;
typedef struct mc_client_effect_renderer mc_client_effect_renderer;
/* Typed access to the existing complete application owner. The concrete body
 * reads its live z/settings field on every kh invocation. */
mc_game_settings *mc_client_application_live_settings(const client_application *);
mc_client_stats_writer *mc_client_application_live_stats_writer(const client_application *);
mc_world_runtime *mc_client_application_live_world(const client_application *);
mc_client_effect_renderer *mc_client_application_live_effect_renderer(const client_application *);
mc_client_achievement_notice *
mc_client_application_live_achievement_notice(const client_application *);
mc_client_notice_status mc_client_application_install_inventory_callback(client_application *,
                                                                         mc_client_notice_error *);
/* Concrete native owned kh, with allocation before actual App borrow acquisition.
 * App/Registry stay exclusive; failed create preserves output and reached RAW kh. */
mc_client_notice_status mc_client_application_inventory_callback_create_owned(
    client_application *, const mc_achievement_description_callback **, mc_client_notice_error *);
bool mc_client_application_callback_dispatch_admitted(const client_application *);
#endif
