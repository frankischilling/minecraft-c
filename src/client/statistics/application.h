#ifndef MC_CLIENT_STATISTICS_APPLICATION_H
#define MC_CLIENT_STATISTICS_APPLICATION_H

#include "mc/client/gui/achievement_notice.h"
#include "mc/client/statistics/writer.h"
#include "mc/client/texture_effects/water.h"
#include "mc/entity/player/player.h"

typedef enum mc_client_statistics_owner_status {
    MC_CLIENT_STATISTICS_OWNER_OK,
    MC_CLIENT_STATISTICS_OWNER_INVALID_ARGUMENT,
    MC_CLIENT_STATISTICS_OWNER_INVALID_PHASE,
    MC_CLIENT_STATISTICS_OWNER_BUSY,
    MC_CLIENT_STATISTICS_OWNER_ALLOCATION_FAILED,
    MC_CLIENT_STATISTICS_OWNER_REGISTRY_FAILED,
    MC_CLIENT_STATISTICS_OWNER_SESSION_FAILED,
    MC_CLIENT_STATISTICS_OWNER_WRITER_FAILED,
    MC_CLIENT_STATISTICS_OWNER_NOTICE_FAILED,
    MC_CLIENT_STATISTICS_OWNER_WATER_FAILED,
    MC_CLIENT_STATISTICS_OWNER_UNIMPLEMENTED
} mc_client_statistics_owner_status;

/* Session reaches class/Registry/hold operations, not World or inventory
 * execution. Keep its actual used child channel compact instead of inheriting
 * the unrelated deep World diagnostics in general mc_player_error. */
typedef struct mc_client_statistics_session_error {
    mc_player_status status;
    const char *operation;
    mc_player_site site;
    mc_registry_error registry;
    mc_entity_status entity;
} mc_client_statistics_session_error;

typedef struct mc_client_statistics_owner_error {
    mc_client_statistics_owner_status status;
    const char *operation;
    mc_player_site site;
    mc_registry_error registry;
    mc_client_statistics_session_error session;
    mc_client_stats_error writer;
    mc_client_notice_error notice;
    mc_water_texture_error water;
    /* Native typed-File child snapshot; not embedded in the old Stats objects. */
    mc_client_stats_file_error source_writer;
} mc_client_statistics_owner_error;

/* Genuine earlier Minecraft119 vs/aw constructor, actual B active use and
 * ad122 publication. Keep a failed candidate privately retained. No catalog
 * prewarm substitutes for this owner. This portion follows actual u78. */
mc_client_statistics_owner_status
mc_client_application_construct_water_effect(client_application *, const mc_water_texture_options *,
                                             mc_client_statistics_owner_error *);
mc_client_statistics_owner_status
mc_client_application_retire_water_effect(client_application *, mc_client_statistics_owner_error *);

/* Actual jl.a()V at Minecraft.<init>186: class active use, then its literal
 * empty method body. Root wires this constructor portion before Session/xi.
 * It is not a complete Minecraft constructor or a late synthetic prewarm. */
mc_client_statistics_owner_status
mc_client_application_statistics_active_use(client_application *,
                                            mc_client_statistics_owner_error *);

/* Real gr class/constructor using supplied immutable String identities,
 * followed by actual application k publication. This declared native input
 * choice does not reproduce the outer launcher's time-derived fallback name.
 * Nullable Strings retain their genuine gr semantics. Current k is replaced
 * only after successful construction. Class failures remain owned. */
mc_client_statistics_owner_status
mc_client_application_create_session(client_application *, mc_player_string *username,
                                     mc_player_string *session_id,
                                     mc_client_statistics_owner_error *);
/* Native counted adoption of the actual same-namespace Session occurrence;
 * NULL clears k. Replacing k never changes a Session already retained in cj. */
mc_client_statistics_owner_status
mc_client_application_publish_session(client_application *, mc_player_session *,
                                      mc_client_statistics_owner_error *);

/* Diagnostic path profile: read current k/native diagnostic directory, construct xi
 * privately. Nonnull actual File Z reaches an explicit typed-adapter frontier. Publish
 * I only on success, then run the existing concrete kh installer. Constructor
 * failures stay privately retained; kh failure keeps already-published I.
 * Actual worker starts normally: no join/service is inserted before kh. One
 * startup construction attempt; failed constructor prefixes cannot retry. */
mc_client_statistics_owner_status
mc_client_application_construct_statistics(client_application *, const mc_client_stats_options *,
                                           mc_client_stats_report *,
                                           mc_client_statistics_owner_error *);

/* Genuine source File caller: MC new325, live k330 then live Z334, ctor337,
 * I340, existing ep/kh/ny354. classes is the SAME outer xi literal service and
 * files is the actual File namespace; neither is allocated here. Success ends
 * before next unexecuted MC358/x359. No worker join/service or logo is added.
 * Failed writer stays private; kh failure keeps the already-published I. */
mc_client_statistics_owner_status mc_client_application_construct_statistics_file(
    client_application *, mc_client_stats_classes *, mc_java_files *,
    const mc_client_stats_options *, mc_client_stats_report *, mc_client_statistics_owner_error *);

/* Native retirement only, called while the complete app/resources/Registry,
 * descriptor callbacks and diagnostic context remain live. Join/free writer
 * before Session/class. An unjoined writer remains owned and blocks dependent
 * retirement. No source save/service/acknowledgment is added. App must outlive
 * every borrowed Player/notice/kh identity. */
mc_client_statistics_owner_status
mc_client_application_retire_statistics(client_application *, mc_client_statistics_owner_error *);

#endif
