#ifndef MC_ENTITY_PLAYER_STATISTICS_H
#define MC_ENTITY_PLAYER_STATISTICS_H

#include "mc/client/gui/achievement_notice.h"
#include "mc/client/statistics/writer.h"
#include "mc/entity/player/player.h"

typedef enum mc_player_statistic_status {
    MC_PLAYER_STATISTIC_OK,
    MC_PLAYER_STATISTIC_INVALID_ARGUMENT,
    MC_PLAYER_STATISTIC_INVALID_PHASE,
    MC_PLAYER_STATISTIC_ENTITY_FAILED,
    MC_PLAYER_STATISTIC_NULL_REFERENCE,
    MC_PLAYER_STATISTIC_WRITER_FAILED,
    MC_PLAYER_STATISTIC_NOTICE_FAILED
} mc_player_statistic_status;

typedef struct mc_player_statistic_error {
    mc_player_statistic_status status;
    const char *operation;
    mc_player_site site; /* Actual dc.a(Lvr;I)V read/invocation site. */
    mc_entity_status entity;
    mc_client_stats_error writer;
    mc_client_notice_error notice;
} mc_player_statistic_error;

/* Genuine client dc.a(vr,int), on a held READY concrete LocalPlayer. The
 * native receiver/phase guard is separate from source execution. Null stat
 * returns before any client/World/Session/catalog access. Concrete kind,
 * parent and child containsKey reads, real notice completion and the final
 * increment follow source order, reloading actual client/I/u at each site.
 * Zero/negative amounts are not filtered. No save/service/tick is implied.
 * The actual borrowed application and descriptor namespace must stay live;
 * access is exclusive. Errors/arguments must be disjoint from owners. */
mc_player_statistic_status mc_local_player_add_statistic(mc_entity *player,
                                                         const mc_statistic *statistic,
                                                         int32_t amount,
                                                         mc_player_statistic_error *error);

#endif
