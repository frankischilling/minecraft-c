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
/* Compass/watch keep their actual App storage alive through the final effect
 * occurrence. Registered Apps use a native borrow, without counting an
 * App->Engine->App ownership cycle. A RAW prepared App remains explicitly
 * caller-owned stable storage. NULL preserves the actual nullable source field.
 * App registration/retirement and these operations require exclusive App use. */
bool mc_client_application_effect_borrow(client_application *, bool *counted);
void mc_client_application_effect_release(client_application *, bool counted);
bool mc_client_application_effect_borrows_pending(const client_application *);
/* A distinct native cz storage borrow. Acquire at the actual b11 store, then
 * release only after worker join and every fallible retained-prefix cleanup.
 * NULL is an uncounted source value; RAW prepared storage remains caller-owned.
 * Registered Apps count this borrow without an App-to-cz ownership cycle.
 * Registration/retirement/acquisition require exclusive App use. Native thread
 * creation publishes the already-acquired borrow; release follows its join.
 * Failure leaves counted=false. No source field is stored by these functions. */
bool mc_client_application_resource_thread_borrow(client_application *, bool *counted);
void mc_client_application_resource_thread_release(client_application *, bool counted);
bool mc_client_application_resource_thread_borrows_pending(const client_application *);
/* A separate screen-domain borrow for each da initialization record and each
 * independently retained du. Source replacement of App.r/da.b does not release
 * those earlier records. Drop a borrow only after its final fallible cleanup.
 * Same NULL/RAW/registered and exclusive-acquisition policy as above; never
 * reuse the worker/effect counters. A counted release remains allowed during
 * registered RETIRING cleanup. An unmatched/underflow/invalid counted release
 * returns false without mutation; an uncounted release always succeeds. */
bool mc_client_application_screen_borrow(client_application *, bool *counted);
bool mc_client_application_screen_release(client_application *, bool counted);
bool mc_client_application_screen_borrows_pending(const client_application *);
#endif
