#ifndef MC_CLIENT_ACHIEVEMENTS_DESCRIPTION_REGISTRY_H
#define MC_CLIENT_ACHIEVEMENTS_DESCRIPTION_REGISTRY_H
#include "mc/client/gui/achievement_notice.h"
/* Registry-only storage/admission seam. No SDL, client dispatch or linked
 * application function is used by its implementation. */
mc_registry_status mc_client_achievement_admit(mc_registry_classes *, const mc_statistic *,
                                               mc_registry_error *);
mc_registry_status mc_client_inventory_callback_client(mc_registry_classes *,
                                                       const mc_achievement_description_callback *,
                                                       client_application **, mc_registry_error *);
mc_registry_status mc_client_description_format(mc_registry_classes *, const mc_registry_string *,
                                                const char *key_name,
                                                mc_client_description_result *,
                                                mc_registry_error *);
mc_registry_status mc_client_achievement_get_title(mc_registry_classes *,
                                                   const mc_registry_string **,
                                                   mc_registry_error *);
mc_registry_status mc_client_notice_registry_retire_admit(const mc_registry_classes *,
                                                          mc_registry_error *);
#endif
