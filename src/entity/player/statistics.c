#include "mc/entity/player/statistics.h"
#include "../../client/application_access.h"
#include "player_internal.h"

static mc_player_statistic_status fail(mc_player_statistic_error *error,
                                       mc_player_statistic_status status, const char *operation,
                                       unsigned int pc) {
    error->status = status;
    error->operation = operation;
    error->site = (mc_player_site){pc == 0 ? NULL : "dc", pc == 0 ? NULL : "a",
                                   pc == 0 ? NULL : "(Lvr;I)V", pc};
    return status;
}

static bool load_writer(mc_entity *player, unsigned int field_pc, mc_client_stats_writer **out,
                        mc_player_statistic_error *error) {
    /* This is deliberately re-evaluated for every source client/I load. */
    client_application *app = player->local_player->fields.minecraft;
    if (app == NULL) {
        fail(error, MC_PLAYER_STATISTIC_NULL_REFERENCE, "local Player statistic null Minecraft",
             field_pc);
        return false;
    }
    *out = mc_client_application_live_stats_writer(app);
    return true;
}

static bool contains(mc_client_stats_writer *writer, const mc_statistic *achievement,
                     unsigned int pc, bool *out, mc_player_statistic_error *error) {
    /* Java evaluates the argument (including ny.c32) before checking the
     * nullable invocation receiver loaded from Minecraft.I. */
    if (writer == NULL) {
        fail(error, MC_PLAYER_STATISTIC_NULL_REFERENCE, "local Player statistic null I", pc);
        return false;
    }
    if (mc_client_stats_writer_contains(writer, achievement, out, &error->writer) ==
        MC_CLIENT_STATS_OK)
        return true;
    fail(error, MC_PLAYER_STATISTIC_WRITER_FAILED, "local Player statistic presence child", pc);
    return false;
}

static void increment(mc_entity *player, const mc_statistic *statistic, int32_t amount,
                      unsigned int field_pc, unsigned int invoke_pc,
                      mc_player_statistic_error *error) {
    mc_client_stats_writer *writer = NULL;
    if (!load_writer(player, field_pc, &writer, error))
        return;
    if (writer == NULL) {
        fail(error, MC_PLAYER_STATISTIC_NULL_REFERENCE, "local Player statistic null I", invoke_pc);
        return;
    }
    if (mc_client_stats_writer_increment(writer, statistic, amount, &error->writer) !=
        MC_CLIENT_STATS_OK)
        fail(error, MC_PLAYER_STATISTIC_WRITER_FAILED, "local Player statistic increment child",
             invoke_pc);
}

mc_player_statistic_status mc_local_player_add_statistic(mc_entity *player,
                                                         const mc_statistic *statistic,
                                                         int32_t amount,
                                                         mc_player_statistic_error *out) {
    mc_player_statistic_error error = {.writer = {.index = SIZE_MAX, .text_offset = SIZE_MAX}};
    if (player == NULL || !mc_entity_is_kind(player, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER))
        fail(&error, MC_PLAYER_STATISTIC_INVALID_ARGUMENT,
             "local Player statistic concrete receiver", 0);
    else if (player->phase != MC_ENTITY_READY)
        fail(&error, MC_PLAYER_STATISTIC_INVALID_PHASE, "local Player statistic READY receiver", 0);
    else {
        error.entity = mc_entity_retain(player);
        if (error.entity != MC_ENTITY_OK)
            fail(&error, MC_PLAYER_STATISTIC_ENTITY_FAILED,
                 "local Player statistic native Entity hold", 0);
        else {
            if (statistic == NULL)
                goto finished; /* dc4 before all client/class/map reads. */
            const mc_statistic_kind kind = mc_statistic_kind_get(statistic); /* vr.d6. */
            if ((unsigned int)kind >= MC_STATISTIC_KIND_COUNT) {
                fail(&error, MC_PLAYER_STATISTIC_INVALID_ARGUMENT,
                     "local Player statistic concrete kind", 6);
                goto finished;
            }
            if (kind != MC_STATISTIC_ACHIEVEMENT) {
                increment(player, statistic, amount, 85, 90, &error);
                goto finished;
            }
            const mc_achievement_fields *fields = mc_statistic_achievement_fields_get(statistic);
            if (fields == NULL) {
                fail(&error, MC_PLAYER_STATISTIC_INVALID_ARGUMENT, "local Player actual ny storage",
                     13);
                goto finished;
            }
            mc_client_stats_writer *writer = NULL;
            bool present;
            if (fields->c != NULL) { /* ny.c18 null branch; argument is reloaded below. */
                if (!load_writer(player, 28, &writer, &error) ||
                    !contains(writer, mc_statistic_achievement_fields_get(statistic)->c, 35,
                              &present, &error) ||
                    !present)
                    goto finished;
            }
            if (!load_writer(player, 45, &writer, &error) ||
                !contains(writer, statistic, 49, &present, &error))
                goto finished;
            if (!present) {
                client_application *app = player->local_player->fields.minecraft; /* b56 reload. */
                if (app == NULL) {
                    fail(&error, MC_PLAYER_STATISTIC_NULL_REFERENCE,
                         "local Player statistic null Minecraft notice load", 59);
                    goto finished;
                }
                mc_client_achievement_notice *notice =
                    mc_client_application_live_achievement_notice(app);
                if (notice == NULL) {
                    fail(&error, MC_PLAYER_STATISTIC_NULL_REFERENCE,
                         "local Player statistic null u", 63);
                    goto finished;
                }
                if (mc_client_achievement_notice_complete(notice, statistic, &error.notice) !=
                    MC_CLIENT_NOTICE_OK) {
                    fail(&error, MC_PLAYER_STATISTIC_NOTICE_FAILED,
                         "local Player actual achievement completion", 63);
                    goto finished;
                }
            }
            increment(player, statistic, amount, 70, 75, &error);
        finished:
            mc_entity_release(player);
        }
    }
    if (out != NULL)
        *out = error;
    return error.status;
}
