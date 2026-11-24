#ifndef MC_CLIENT_GUI_SOURCE_SIGN_SCREEN_H
#define MC_CLIENT_GUI_SOURCE_SIGN_SCREEN_H

#include "../display/sdk_input.h"
#include "font_mapping.h"
#include "mc/world/runtime/tile_entities/tile_entities.h"
#include "source_screen.h"

typedef struct mc_client_source_yc mc_client_source_yc;
typedef struct mc_client_display_screen_call mc_client_display_screen_call;

typedef struct mc_client_sign_screen_dependencies {
    mc_client_font_mapping *mapping;           /* Explicit defining FP namespace at NEW YC. */
    mc_client_sdk_input *input;                /* Optional READY ordinary SDK code capability. */
    mc_world_runtime_classes *literal_classes; /* Optional defining Tile/Registry namespace. */
} mc_client_sign_screen_dependencies;

typedef struct mc_client_sign_screen_error {
    mc_client_font_mapping_error mapping;
    mc_client_sdk_input_error input;
    mc_tile_entity_status tile;
    mc_live_tile_entity_error dirty;
    mc_registry_error literal;
    mc_client_screen_site child;
    bool dirty_returned, display_entered, display_returned;
    mc_client_sdk_input_error cleanup_input;
    mc_tile_entity_status cleanup_tile;
} mc_client_sign_screen_error;

typedef struct mc_client_sign_class_state {
    mc_client_screen_class_phase phase;
    const mc_client_font_mapping *mapping_identity; /* Actual held owner; NULL after class end. */
    const mc_player_string *accepted; /* Actual YC.m alias, retained until class final release. */
    unsigned int last_pc, last_store_pc;
    const mc_client_screen_error *failure;
    const mc_client_font_mapping_error *mapping_failure;
    bool mapping_held, admission_ended;
} mc_client_sign_class_state;

typedef struct mc_client_sign_screen_state {
    mc_client_screen_phase phase;
    size_t holds, source_calls, initialization_count;
    mc_player_string *title;
    mc_tile_entity *tile; /* Hidden once native terminal preparation ends its occurrence. */
    int32_t tick, row;
    bool tile_held, input_held, literal_classes_held, terminal_prepared;
    unsigned int last_pc, row_stores, line_stores, tick_stores;
    const char *method;
    const mc_client_screen_error *failure;
    const mc_client_sign_screen_error *detail;
    const mc_client_screen_error *cleanup_failure;
} mc_client_sign_screen_state;

/* Active use is independent of App/Font/World fields and precedes RAW allocation.
 * The existing FP namespace is a stable caller-owned binding during allocation;
 * its captured String is retained separately from the class native FP hold.
 * Native failed phases are retained and never silently retried/rebound. */
mc_client_screen_status mc_client_screen_sign_active_use(mc_client_screen_classes *,
                                                         mc_client_font_mapping *,
                                                         mc_client_screen_error *);
mc_client_screen_status mc_client_screen_sign_class_inspect(mc_client_screen_classes *,
                                                            mc_client_sign_class_state *,
                                                            mc_client_screen_error *);
/* End NEW/active-use admission and drop the FP native hold before resources
 * retire. Does not change captured Source m or retry initialization. Idempotent.
 * Existing YC instances keep their separately retained m/class dependency. */
mc_client_screen_status mc_client_screen_sign_class_terminal_prepare(mc_client_screen_classes *,
                                                                     mc_client_screen_error *);
mc_client_screen_status mc_client_screen_sign_allocate(mc_client_screen_classes *,
                                                       const mc_client_sign_screen_dependencies *,
                                                       const mc_client_screen_options *,
                                                       mc_client_source_yc **,
                                                       mc_client_screen_error *);
mc_client_screen_status mc_client_screen_sign_construct(mc_client_source_yc *,
                                                        mc_tile_entity *nullable_tile,
                                                        mc_client_screen_error *);
mc_client_source_da *mc_client_screen_sign_base(mc_client_source_yc *);
mc_client_screen_status mc_client_screen_sign_inspect(mc_client_source_yc *,
                                                      mc_client_sign_screen_state *,
                                                      mc_client_screen_error *);
/* Direct actual YC.b, YC.a(CI)V and YC.a()V component entries. DA initialization
 * and the real event pipeline dispatch to the same concrete method bodies.
 * Unsupported nonconstructor array backings remain an explicit native frontier.
 * No key dirties, normalizes UTF-16, changes edit-row or changes the read flag. */
mc_client_screen_status mc_client_screen_sign_initialize(mc_client_source_yc *,
                                                         mc_client_screen_error *);
mc_client_screen_status mc_client_screen_sign_key(mc_client_source_yc *, uint16_t character,
                                                  int32_t key, mc_client_screen_error *);
mc_client_screen_status mc_client_screen_sign_tick(mc_client_source_yc *, mc_client_screen_error *);
/* Actual button callback. out starts NULL; any reached Minecraft display child
 * is retained in this caller-owned slot even on failure. Retire that child
 * before this screen/App. This avoids a screen->display->old-screen cycle.
 * Disabled/nonzero buttons return without dirty/display and preserve out. */
mc_client_screen_status mc_client_screen_sign_button(mc_client_source_yc *, mc_client_source_ke *,
                                                     mc_client_display_screen_call **out,
                                                     mc_client_screen_error *);
/* One-way native admission ending, not Source h/dirty/display or repeat repair.
 * Requires no active/external screen occurrence. Releases the constructor Tile,
 * explicit input and Tile-class capabilities before storage/Display teardown;
 * preserves stored Source fields and copied failure/scalar receipts. */
mc_client_screen_status mc_client_screen_sign_terminal_prepare(mc_client_source_yc *,
                                                               mc_client_screen_error *);
mc_client_screen_status mc_client_screen_sign_release(mc_client_source_yc **,
                                                      mc_client_screen_error *);
mc_client_screen_status mc_client_screen_sign_retire(mc_client_source_yc **,
                                                     mc_client_screen_error *);

#endif
