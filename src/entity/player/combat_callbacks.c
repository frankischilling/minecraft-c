#include "combat_callbacks_internal.h"

#include "../../block/registry/registry_internal.h"
#include "../../item/render/internal.h"
#include "../mob/internal.h"
#include "mc/core/java/java.h"
#include "mc/entity/player/statistics.h"
#include "player_internal.h"
#include "statistics_status_internal.h"

static bool fail(mc_entity_effect_error *e, mc_entity_status status, mc_entity_effect_cause cause,
                 const char *operation) {
    e->status = status;
    e->cause = cause;
    e->operation = operation;
    return false;
}

bool mc_player_death_callback(mc_entity *p, mc_registry_classes *registry, mc_entity *victim,
                              int32_t score, bool scoring, mc_player_site *site,
                              mc_entity_effect_error *e) {
    *site = (mc_player_site){"gs", scoring ? "c" : "a", scoring ? "(Lsn;I)V" : "(Lls;)V", 0};
    if (!mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER) || p->local_player == NULL)
        return fail(e, MC_ENTITY_STATE_ERROR, MC_ENTITY_EFFECT_CAUSE_OWNER,
                    "Player death callback actual LocalPlayer storage");
    if (!scoring) {
        /* Animals are not GZ. Their callback returns14 before any EP use.
         * The Monster branch retains its exact achievement dependency. */
        const mc_entity_class_literal *monster = NULL;
        bool match = false;
        if (mc_entity_class_literal_get(p->classes, MC_ENTITY_CLASS_KIND_MONSTER, &monster) !=
                MC_ENTITY_OK ||
            (victim != NULL && mc_entity_class_literal_is_assignable_from(
                                   monster, victim->class_literal, &match) != MC_ENTITY_OK))
            return fail(e, MC_ENTITY_STATE_ERROR, MC_ENTITY_EFFECT_CAUSE_OWNER,
                        "Player kill actual Monster Class identity");
        if (match) {
            site->pc = 7;
            return fail(e, MC_ENTITY_STATE_ERROR, MC_ENTITY_EFFECT_CAUSE_UNSUPPORTED_VIRTUAL,
                        "Player kill actual Monster achievement getstatic");
        }
        site->pc = 14;
        return true;
    }
    p->local_player->fields.player.g = mc_java_i32_add(p->local_player->fields.player.g, score);
    site->pc = 7;
    const bool player =
        victim != NULL && mc_entity_is_kind(victim, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER);
    site->pc = player ? 18u : 29u;
    e->player_bytecode = (uint16_t)site->pc;
    if (registry == NULL || registry->entities != p->classes)
        return fail(e, MC_ENTITY_STATE_ERROR, MC_ENTITY_EFFECT_CAUSE_OWNER,
                    "Player score actual statistic Registry namespace");
    if (mc_registry_classes_initialize_statistics(registry, &e->registry) != MC_REGISTRY_OK) {
        mc_item_render_throwable throwable;
        mc_item_render_registry_classification(registry, &e->registry, &throwable,
                                               &e->living_damage.exception_class);
        e->living_damage.source_exception = throwable != MC_ITEM_RENDER_NO_THROWABLE;
        e->living_damage.native_policy = !e->living_damage.source_exception;
        return fail(e, MC_ENTITY_STATE_ERROR, MC_ENTITY_EFFECT_CAUSE_REGISTRY,
                    "Player score actual statistic getstatic");
    }
    e->statistic = registry->statistics.named[player ? 21 : 20];
    e->statistic_amount = 1;
    site->pc = player ? 22u : 33u;
    e->player_bytecode = (uint16_t)site->pc;
    const mc_player_statistic_status status =
        mc_local_player_add_statistic(p, e->statistic, 1, &e->statistic_error);
    e->statistic_bytecode = (uint16_t)e->statistic_error.site.pc;
    if (status != MC_PLAYER_STATISTIC_OK) {
        const bool null = status == MC_PLAYER_STATISTIC_NULL_REFERENCE ||
                          (status == MC_PLAYER_STATISTIC_WRITER_FAILED &&
                           e->statistic_error.writer.status == MC_CLIENT_STATS_SOURCE_EXCEPTION &&
                           e->statistic_error.writer.exception == MC_CLIENT_STATS_NULL_REFERENCE);
        e->living_damage.source_exception = null;
        e->living_damage.native_policy = !null;
        e->living_damage.exception_class = null ? "java/lang/NullPointerException" : NULL;
        return fail(e, mc_player_statistic_entity_status(&e->statistic_error),
                    null ? MC_ENTITY_EFFECT_CAUSE_NULL_REFERENCE : MC_ENTITY_EFFECT_CAUSE_STATISTIC,
                    "Player score actual statistic child");
    }
    site->pc = 36;
    return true;
}
