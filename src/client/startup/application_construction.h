#ifndef MC_CLIENT_STARTUP_APPLICATION_CONSTRUCTION_H
#define MC_CLIENT_STARTUP_APPLICATION_CONSTRUCTION_H
#include "../gui/loading_screen.h"
#include "../statistics/application.h"
#include "../texture_effects/lava_construction.h"
#include "../timer/construction.h"
#include "application_owner.h"
#include "mc/client/render/biped.h"
#include "mc/client/sound/backend.h"
#include "mc/client/sound/construction.h"

typedef struct mc_client_source_canvas mc_client_source_canvas;
typedef struct mc_client_source_applet mc_client_source_applet;

typedef enum mc_client_application_construction_status {
    MC_CLIENT_APP_OK = 0,
    MC_CLIENT_APP_INVALID_ARGUMENT,
    MC_CLIENT_APP_INVALID_PHASE,
    MC_CLIENT_APP_LIMIT,
    MC_CLIENT_APP_ALLOCATION_FAILED,
    MC_CLIENT_APP_CHILD_FAILED,
    MC_CLIENT_APP_UNIMPLEMENTED,
    MC_CLIENT_APP_BUSY
} mc_client_application_construction_status;

typedef enum mc_client_application_child {
    MC_CLIENT_APP_CHILD_NONE,
    MC_CLIENT_APP_CHILD_CLASS,
    MC_CLIENT_APP_CHILD_TIMER,
    MC_CLIENT_APP_CHILD_LOADING,
    MC_CLIENT_APP_CHILD_NOTICE,
    MC_CLIENT_APP_CHILD_BIPED,
    MC_CLIENT_APP_CHILD_SOUND,
    MC_CLIENT_APP_CHILD_WATER,
    MC_CLIENT_APP_CHILD_LAVA,
    MC_CLIENT_APP_CHILD_CLOCK,
    MC_CLIENT_APP_CHILD_STATISTICS,
    MC_CLIENT_APP_CHILD_WORKER,
    MC_CLIENT_APP_CHILD_STRING,
    MC_CLIENT_APP_CHILD_REGISTRY,
    MC_CLIENT_APP_CHILD_ENTITY,
    MC_CLIENT_APP_CHILD_RENDER_CLASSES,
    MC_CLIENT_APP_CHILD_PLATFORM
} mc_client_application_child;

/* Parent MC allocation/invoke/store remains distinct from the complete actual
 * child's own site/cause. These records may be large: allocate output on heap,
 * disjoint from owner/inputs. No compressed successful descendant is invented. */
typedef struct mc_client_application_frontier {
    const char *class_name, *method, *descriptor;
    int32_t pc; /* -1 if no concrete lower body has been implemented/pinned. */
} mc_client_application_frontier;

typedef struct mc_client_application_construction_error {
    mc_client_application_construction_status status;
    mc_client_application_child child;
    const char *operation;
    int32_t minecraft_pc; /* -1 for native allocation/admission/retirement. */
    mc_client_application_frontier frontier;

    union {
        mc_client_application_owner_error owner;
        mc_timer_construction_error timer;
        mc_client_loading_error loading;
        mc_client_notice_error notice;
        mc_client_biped_error biped;
        mc_sound_manager_constructor_error sound;
        mc_sound_error sound_raw;
        mc_registry_error registry;
        mc_entity_status entity_status;
        mc_client_model_status model_status;
        mc_water_texture_error water;
        mc_lava_texture_error lava;
        mc_platform_millis_status clock;
        mc_client_statistics_owner_error statistics;
        mc_player_status string;
    } cause;
} mc_client_application_construction_error;

typedef struct mc_client_application_constructor_services {
    mc_client_application_classes *application_classes;
    mc_client_loading_classes *gui_classes;
    mc_registry_classes *registry;
    mc_entity_classes *entities;
    mc_client_render_classes *render_classes;
    mc_java_random_profile *profile;
    mc_sound_manager_shared *sound_shared;
    /* shared must be made from this actual cold backend's manager interface.
     * Its real backend/context, profile/resource provider remain externally
     * owned and live through every retained failed/ready child. No callback
     * provider/device/Settings setup is invoked by this constructor. */
    mc_sound_backend *sound_backend;
} mc_client_application_constructor_services;

typedef struct mc_client_application_constructor_options {
    size_t maximum_native_bytes; /* zero: actual App plus separate heap-diagnostic storage */
    size_t timer_bytes, biped_bytes, sound_pool_bytes;
    mc_water_texture_options water;
    mc_lava_texture_options lava;
} mc_client_application_constructor_options;

typedef struct mc_client_application_constructor_arguments {
    const void *component;           /* actual argument1 is unused */
    mc_client_source_canvas *canvas; /* real borrowed identity, store219 only */
    mc_client_source_applet *applet; /* real borrowed identity; nonnull reaches249 */
    int32_t width, height;
    bool fullscreen;
} mc_client_application_constructor_arguments;

typedef struct mc_client_application_constructor_state {
    mc_client_application_constructor_phase phase;
    const client_application *fields; /* SAME complete actual storage; NULL RETIRING */
    size_t source_stores;
    int32_t last_source_store;
    const int32_t *source_store_pcs; /* borrowed native trace, not source state */
    const mc_client_application_construction_error *failure, *retirement;
    const mc_client_timer_hack *worker;
    bool namespace_borrowed;
    size_t static_holds;
} mc_client_application_constructor_state;

/* One actual zero/default-valued App. Real MC class activation precedes native
 * source instance storage; failed class prefix stays in its class owner. Native
 * dependencies/quotas are explicit, cold and compatible, no child initializer,
 * Random, clock, trig or GL activity is performed to prepare a successful App.
 * Profile/provider/backend/shared lifetimes are borrowed/exclusive as declared;
 * explicit Registry/Entity/render/GUI namespace occurrences are held. Native
 * services and their external occurrences remain quiescent/exclusively owned
 * during allocation: balancing a newly gained cold occurrence cannot enter
 * final retirement. All output/error storage is valid, aligned and disjoint
 * from App/classes, service/options/argument storage and ownership slots. */
mc_client_application_construction_status
mc_client_application_constructor_allocate(const mc_client_application_constructor_services *,
                                           const mc_client_application_constructor_options *,
                                           client_application **,
                                           mc_client_application_construction_error *);
/* Exactly one actual Minecraft(Component,Canvas,Applet,int,int,boolean) body.
 * No component initialize(), Session, xi, World or run/startup success is supplied.
 * Failure retains every prior field/candidate/error; no rollback or retry.
 * Nonnull Applet getParameter is an explicit reached platform frontier249. */
mc_client_application_construction_status
mc_client_application_constructor_construct(client_application *,
                                            const mc_client_application_constructor_arguments *,
                                            mc_client_application_construction_error *);
bool mc_client_application_constructor_inspect(const client_application *,
                                               mc_client_application_constructor_state *);
/* Native source stop/cleanup. RETIRING rejects source APIs/views. Stop/join kg
 * using its native cancel, NEVER source J=false; release each actual occurrence
 * once, preserving BUSY and progress. Keep an externally retained mk's App borrow
 * alive (hold App's mk occurrence until final releasable). Detach MC namespace
 * only after class-dependent cleanup; then wait for disappearing static hold.
 * Caller terminal class release/retry completes teardown with no source a=NULL.
 * Successful final release consumes/nulls this unique App. No allocation occurs. */
mc_client_application_construction_status
mc_client_application_constructor_retire(client_application **,
                                         mc_client_application_construction_error *);
#endif
