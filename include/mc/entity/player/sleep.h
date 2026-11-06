#ifndef MC_ENTITY_PLAYER_SLEEP_H
#define MC_ENTITY_PLAYER_SLEEP_H

#include "mc/entity/player/player.h"
#include "mc/world/dimension/world_provider.h"

typedef struct mc_player_sleep_result mc_player_sleep_result;
typedef struct mc_player_sleep_name mc_player_sleep_name;

typedef enum mc_player_sleep_result_status {
    MC_PLAYER_SLEEP_RESULT_OK,
    MC_PLAYER_SLEEP_RESULT_ARGUMENT,
    MC_PLAYER_SLEEP_RESULT_BUSY,
    MC_PLAYER_SLEEP_RESULT_LIMIT,
    MC_PLAYER_SLEEP_RESULT_ALLOCATION_FAILED,
    MC_PLAYER_SLEEP_RESULT_CLASS_FAILED
} mc_player_sleep_result_status;

typedef struct mc_player_sleep_result_error {
    mc_player_sleep_result_status status;
    const char *operation;
    mc_player_site site;
    bool native_policy;
    mc_entity_error native;
} mc_player_sleep_result_error;

typedef struct mc_player_sleep_result_view {
    const mc_entity_classes *classes;
    mc_entity_class_phase phase;
    const mc_player_sleep_name *name;
    const uint16_t *name_units;
    size_t name_length;
    int32_t ordinal;
} mc_player_sleep_result_view;

typedef struct mc_player_sleep_result_class_view {
    mc_entity_class_phase phase;
    const mc_player_sleep_result *a, *b, *c, *d, *e;
    const mc_player_sleep_result *candidates[5];
    const void *values_identity;
    size_t values_length;
    const mc_player_sleep_result *values[5];
    unsigned int last_pc, last_store_pc;
    const mc_player_sleep_result_error *failure;
} mc_player_sleep_result_class_view;

/* Reached client CW getstatic, including lazy complete initialization and
 * actual nullable/replaced static cells. Native status is not a source result.
 * The caller owns its enclosing GS/VE site and keeps this exact namespace
 * live/exclusive until every returned borrowed enum/name/array view expires.
 * Cold/failed prefixes are retained by the namespace; no source retry. */
mc_player_sleep_result_status mc_player_sleep_result_static_get(mc_entity_classes *,
                                                                const char *field,
                                                                const mc_player_sleep_result **out,
                                                                mc_player_sleep_result_error *);
bool mc_player_sleep_result_inspect(const mc_player_sleep_result *, mc_player_sleep_result_view *);
bool mc_player_sleep_result_classes_inspect(const mc_entity_classes *,
                                            mc_player_sleep_result_class_view *);

typedef enum mc_player_sleep_status {
    MC_PLAYER_SLEEP_OK,
    MC_PLAYER_SLEEP_ARGUMENT,
    MC_PLAYER_SLEEP_BUSY,
    MC_PLAYER_SLEEP_LIMIT,
    MC_PLAYER_SLEEP_ALLOCATION_FAILED,
    MC_PLAYER_SLEEP_SOURCE_NULL,
    MC_PLAYER_SLEEP_CHILD_FAILED,
    MC_PLAYER_SLEEP_CLASS_FAILED,
    MC_PLAYER_SLEEP_UNIMPLEMENTED
} mc_player_sleep_status;

typedef struct mc_player_sleep_error {
    mc_player_sleep_status status;
    const char *operation, *exception_class;
    mc_player_site site, child;
    bool native_policy, source_exception, source_error, returned;
    mc_entity_error entity;
    mc_world_storage_error world;
    mc_world_provider_error provider;
    mc_player_error sleeping;
    mc_player_sleep_result_error result;
} mc_player_sleep_error;

/* Actual client GS.b(III):CW on a canonical LocalPlayer, through the ordinary
 * sleeping/alive/provider/daytime/distance rejection returns. The actual Player
 * World is independent of a caller's Block World. A nullable CW cell is a valid
 * successful return. Every failure preserves out. Night/remote success paths
 * stop immediately BEFORE resize119, without sleep/position/Q stores or eager
 * CW initialization. No GUI, Registry activation or Math context is required.
 * These synchronous prefixes own no retained child call: caller keeps Player,
 * World and defining namespace live/exclusive through use of all borrowed views. */
mc_player_sleep_status mc_player_try_sleep(mc_entity *, int32_t x, int32_t y, int32_t z,
                                           const mc_player_sleep_result **out,
                                           mc_player_sleep_error *);

#endif
