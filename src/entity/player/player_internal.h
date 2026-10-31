#ifndef MC_ENTITY_PLAYER_INTERNAL_H
#define MC_ENTITY_PLAYER_INTERNAL_H

#include "../entity_internal.h"
#include "mc/entity/player/player.h"

struct mc_player_string {
    size_t holds, length;
    const uint16_t *units;
};

struct mc_player_session {
    mc_player_session_classes *classes;
    mc_player_string *username, *session_id;
    size_t holds;
};

struct mc_player_nbt_coordinate;

typedef struct mc_local_player_state {
    mc_local_player_snapshot fields;
    struct mc_player_nbt_coordinate *nbt_coordinates;
} mc_local_player_state;

typedef struct mc_local_player_object {
    mc_entity entity;
    mc_local_player_state state;
} mc_local_player_object;

mc_entity_status mc_local_player_watcher_initialize(mc_entity *entity, mc_entity_error *error);
void mc_local_player_dispose(mc_entity *entity);
const mc_player_string *mc_player_texture_literal(void);
const mc_player_string *mc_player_model_literal(void);
mc_player_status mc_player_skin_string(const mc_player_string *name, mc_player_string **out);
mc_player_status mc_player_cloak_string(const mc_player_string *name, mc_player_string **out);
bool mc_player_session_matches(const mc_player_session *session, const mc_entity_classes *entities,
                               const mc_registry_classes *registry);
bool mc_player_session_registry_matches(const mc_player_session *session,
                                        const mc_registry_classes *registry);
float mc_local_player_aj_from_draw(double value);
float mc_local_player_g_from_draw(double value);
float mc_local_player_f_from_draw(double value);
float mc_local_player_yaw_from_draw(double value);
void mc_player_error_clear(mc_player_error *error);
mc_player_status mc_player_fail(mc_player_error *error, mc_player_status status,
                                const char *operation, const char *class_name, const char *method,
                                const char *descriptor, unsigned int pc);

#endif
