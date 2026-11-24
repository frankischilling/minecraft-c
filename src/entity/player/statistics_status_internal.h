#ifndef MC_ENTITY_PLAYER_STATISTICS_STATUS_INTERNAL_H
#define MC_ENTITY_PLAYER_STATISTICS_STATUS_INTERNAL_H

#include "mc/entity/player/statistics.h"

/* Native failure channel translation only; source sites stay in the child. */
static inline mc_entity_status
mc_player_statistic_entity_status(const mc_player_statistic_error *error) {
    switch (error->status) {
        case MC_PLAYER_STATISTIC_OK:
            return MC_ENTITY_OK;
        case MC_PLAYER_STATISTIC_INVALID_ARGUMENT:
            return MC_ENTITY_INVALID_ARGUMENT;
        case MC_PLAYER_STATISTIC_INVALID_PHASE:
            return MC_ENTITY_INVALID_PHASE;
        case MC_PLAYER_STATISTIC_ENTITY_FAILED:
            return error->entity;
        default:
            break;
    }
    if (error->status == MC_PLAYER_STATISTIC_WRITER_FAILED) {
        if (error->writer.status == MC_CLIENT_STATS_LIMIT)
            return MC_ENTITY_LIMIT;
        if (error->writer.status == MC_CLIENT_STATS_ALLOCATION_FAILED)
            return MC_ENTITY_ALLOCATION_FAILED;
        if (error->writer.status == MC_CLIENT_STATS_BUSY)
            return MC_ENTITY_BUSY;
    } else if (error->status == MC_PLAYER_STATISTIC_NOTICE_FAILED) {
        if (error->notice.status == MC_CLIENT_NOTICE_ALLOCATION_FAILED)
            return MC_ENTITY_ALLOCATION_FAILED;
        if (error->notice.status == MC_CLIENT_NOTICE_BUSY)
            return MC_ENTITY_BUSY;
    }
    return MC_ENTITY_STATE_ERROR;
}

#endif
