#ifndef MC_WORLD_RUNTIME_BED_SPAWN_H
#define MC_WORLD_RUNTIME_BED_SPAWN_H

#include "mc/entity/player/player.h"
#include "mc/world/runtime/blocks.h"
#include "mc/world/runtime/collision/entities.h"

#define MC_WORLD_BED_SPAWN_DEFAULT_LOOP_WORK ((size_t)1048576)
#define MC_WORLD_BED_SPAWN_RETAINED_EVENTS 64u

typedef struct mc_world_bed_spawn_limits {
    size_t max_loop_work;
} mc_world_bed_spawn_limits;

typedef enum mc_world_bed_spawn_event_kind {
    MC_WORLD_BED_SPAWN_CAPTURED_ACQUIRE,
    MC_WORLD_BED_SPAWN_BLOCK,
    MC_WORLD_BED_SPAWN_METADATA,
    MC_WORLD_BED_SPAWN_SUPPORT,
    MC_WORLD_BED_SPAWN_AIR
} mc_world_bed_spawn_event_kind;

typedef struct mc_world_bed_spawn_event {
    mc_world_bed_spawn_event_kind kind;
    unsigned int pc;
    int32_t x, y, z, result;
    bool completed;
} mc_world_bed_spawn_event;

typedef enum mc_world_bed_spawn_child {
    MC_WORLD_BED_SPAWN_CHILD_NONE,
    MC_WORLD_BED_SPAWN_CHILD_ACQUISITION,
    MC_WORLD_BED_SPAWN_CHILD_REGISTRY,
    MC_WORLD_BED_SPAWN_CHILD_SUPPORT
} mc_world_bed_spawn_child;

typedef struct mc_world_bed_spawn_error {
    mc_world_storage_status status;
    const char *operation, *source_class, *source_method;
    unsigned int pc, wrapper_pc, helper_pc;
    bool native_policy, source_exception, source_error;
    const char *exception_class;
    bool wrapper_entered, helper_entered, returned, result_present;
    uintptr_t provider_identity, table_identity, row_identity;
    size_t provider_calls, candidate_visits, loop_work, max_loop_work;
    int32_t metadata, orientation, pass, candidate_x, candidate_z, remaining_skip;
    mc_world_storage_error cause;
    /* Failed returned diagnostics retain their actual source and captured-
     * provider World call holds, including two occurrences of one identity.
     * No World/provider/source-loader producer can retire before checked
     * error retirement. These native holds add no gameplay reference field. */
    mc_world_runtime *retained_worlds[2];
    size_t retained_world_count;
    size_t event_count, retained_events;
    bool event_overflow;
    /* Passive bounded journal. Overflow never changes source work or success;
     * completed=false retains a reached invocation's failed operand prefix. */
    mc_world_bed_spawn_event events[MC_WORLD_BED_SPAWN_RETAINED_EVENTS];
    mc_world_bed_spawn_child child_kind;

    /* Only the tagged child is active. Complete child views borrow the actual
     * producers protected by this error's retained World occurrences. Pending
     * load/activation work itself still belongs to that exact World/loader. */
    union {
        mc_world_runtime_acquisition_error acquisition;
        mc_registry_error registry;

        struct {
            mc_world_solid_support_error error;
            mc_world_nearby_error access;
        } support;
    } child;
} mc_world_bed_spawn_error;

void mc_world_bed_spawn_error_init(mc_world_bed_spawn_error *);
/* Consume failed diagnostic World holds before reusing/destroying this error
 * or retiring its graph. Failure preserves every hold; success performs no
 * Source work or child retry/rollback. Errors owning holds are noncopyable.
 * The caller keeps each supplying Registry/dependency live and excludes later
 * mutation of the borrowed failure producer until this retirement. */
bool mc_world_bed_spawn_error_retire(mc_world_bed_spawn_error *);

/* Actual client VE.f(World,x,y,z,skip): initialize the declaring Bed class,
 * query metadata, then search both ordered 3x3 passes using the actual Bed
 * facing table and exact support/air bodies. Java int wrapping and overlapping
 * candidates are preserved. A native work limit refuses before pending work;
 * it never changes incomplete search into successful null. No bed-pair,
 * collision-volume, Entity, liquid, occupancy or Random check is added. */
mc_world_storage_status mc_world_bed_spawn_exit(mc_world_runtime *, mc_registry_classes *,
                                                int32_t x, int32_t y, int32_t z, int32_t skip,
                                                const mc_world_bed_spawn_limits *,
                                                mc_player_coordinates **out,
                                                mc_world_bed_spawn_error *);

/* Actual client GS.a(World,br): capture World.w once, invoke that same
 * provider's c four times (including repeated coordinates/cache hits), then
 * read the current Block and late actual Bed.T ID before invoking VE.f(skip0).
 * Each source coordinate field read remains live at its argument position.
 * A nonnull input is a caller-pinned source identity: keep it and its owning
 * storage live through callbacks, including source field replacement. const
 * is a readable view, not a promise that callbacks freeze its scalar fields. */
mc_world_storage_status mc_world_bed_spawn_validate(mc_world_runtime *, mc_registry_classes *,
                                                    const mc_player_coordinates *,
                                                    const mc_world_bed_spawn_limits *,
                                                    mc_player_coordinates **out,
                                                    mc_world_bed_spawn_error *);

/* Each successful nonnull result is one newly allocated source br identity.
 * NULL is ordinary success. out is required and unchanged on failure; release
 * a previous result before successful replacement. Results never alias input
 * or retain Worlds. Keep all actual World/provider/Registry dependencies and
 * error's borrowed child producers live/exclusive. Retire failed errors before
 * reuse; an initialized/retired error owns no occurrences. Output/error/limits must
 * be disjoint from owners and inputs. Error is optional; use heap storage for
 * these complete nested diagnostics. No JVM or original asset is required. */
void mc_world_bed_spawn_position_free(mc_player_coordinates *);

#endif
