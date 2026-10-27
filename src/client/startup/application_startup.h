#ifndef MC_CLIENT_APPLICATION_STARTUP_H
#define MC_CLIENT_APPLICATION_STARTUP_H

#include "../display/staged.h"
#include "../gui/bitmap_font.h"
#include "../gui/loading_screen.h"
#include "../render_engine/color_lookup.h"
#include "../render_engine/colorizer.h"
#include "application_owner.h"
#include "directory_inputs.h"
#include "mc/client/saves/save_format.h"
#include "mc/client/settings/construction.h"
#include "mc/client/texture_packs/construction.h"

typedef struct mc_client_application_startup mc_client_application_startup;

typedef enum mc_client_startup_status {
    MC_CLIENT_STARTUP_OK,
    MC_CLIENT_STARTUP_INVALID_ARGUMENT,
    MC_CLIENT_STARTUP_INVALID_PHASE,
    MC_CLIENT_STARTUP_BUSY,
    MC_CLIENT_STARTUP_ALLOCATION_FAILED,
    MC_CLIENT_STARTUP_LIMIT,
    MC_CLIENT_STARTUP_OWNER_FAILED,
    MC_CLIENT_STARTUP_DISPLAY_FAILED,
    MC_CLIENT_STARTUP_DIRECTORY_FAILED,
    MC_CLIENT_STARTUP_STRING_FAILED,
    MC_CLIENT_STARTUP_FILE_FAILED,
    MC_CLIENT_STARTUP_SAVE_FAILED,
    MC_CLIENT_STARTUP_UNIMPLEMENTED,
    MC_CLIENT_STARTUP_SETTINGS_FAILED,
    MC_CLIENT_STARTUP_PACK_FAILED,
    MC_CLIENT_STARTUP_RENDER_ENGINE_FAILED,
    MC_CLIENT_STARTUP_BITMAP_FONT_FAILED,
    MC_CLIENT_STARTUP_COLORS_FAILED
} mc_client_startup_status;

typedef enum mc_client_startup_phase {
    MC_CLIENT_STARTUP_RAW,
    MC_CLIENT_STARTUP_ACTIVE,
    MC_CLIENT_STARTUP_STOPPED,
    MC_CLIENT_STARTUP_RETIRING
} mc_client_startup_phase;

/* Native contexts are source-location metadata, not fabricated Java stack
 * frames. A native failure/frontier never enters run's Exception handler.
 * Returned/inspected UTF16 error views are borrowed through the next mutating
 * startup-record operation; re-inspect after cleanup. The record retains its
 * Settings diagnostic line before consuming Settings, through partial cleanup
 * failure and until final record retirement. Color diagnostics contain only
 * pinned static text and by-value causes; array/lookup inspection pointers are
 * borrowed only while the inspected owner remains live, never copied as error
 * ownership. color_results traces clear when Engine is consumed; re-inspect
 * after cleanup. Capture failure keeps the actual
 * Settings owner live; copied prior views expire at the retry operation. */
typedef struct mc_client_startup_error {
    mc_client_startup_status status;
    const char *operation;
    mc_client_application_owner_site caller, child;
    mc_client_application_owner_error owner;
    mc_client_display_stage_error display;
    mc_client_game_directory_error directory;
    mc_client_game_directory_state directory_prefix;
    mc_world_info_status string;
    mc_player_status player_string;
    mc_java_file_error file;
    mc_client_save_error save;
    mc_game_settings_source_error settings;
    mc_client_sdk_icons_error icons;
    mc_texture_pack_source_error pack;
    mc_client_render_engine_error engine;
    mc_client_font_status default_font;
    mc_client_bitmap_font_error bitmap_font;
    mc_client_loading_error gui;
    mc_client_color_lookup_error colors;
    mc_client_colorizer_error colorizer;
    mc_client_color_array_error color_argument;
} mc_client_startup_error;

typedef struct mc_client_startup_services {
    mc_java_files *files;
    mc_client_save_formats *save_formats;
    mc_client_directory_inputs *directory_inputs;
    mc_game_settings_classes *settings_classes;
    mc_client_sdk_icons *icons; /* Native imported prerequisite; NULL keeps its reached failure. */
    /* Borrowed real class resource provider; must outlive startup and every
     * retained Pack/stream. Read only at actual reached Pack operations. */
    const mc_registry_resources *texture_resources;
} mc_client_startup_services;

typedef struct mc_client_startup_options {
    bool hidden; /* native window visibility; never a Minecraft field */
    size_t maximum_builder_units, maximum_string_bytes;
    size_t maximum_default_font_bytes; /* native provider storage, no eager activation */
    mc_client_render_engine_options render_engine;
    mc_client_render_engine_options bitmap_font; /* Separate native sj constructor quota. */
    mc_client_color_lookup_options color_lookup;
    mc_client_colorizer_options colorizers;
} mc_client_startup_options;

typedef struct mc_client_startup_state {
    mc_client_startup_phase phase;
    const client_application *app;
    const mc_client_display *display;
    const mc_client_display_mode *mode;
    const mc_java_file *directory_candidate, *saves_candidate;
    const mc_client_save_format *format_candidate;
    const mc_game_settings *settings_candidate;
    const mc_client_texture_pack_list *pack_candidate;
    bool D_stored, p_stored, ji_active_use_completed;
    const mc_client_render_engine *engine_candidate;
    const mc_client_render_engine_classes *engine_classes;
    const mc_client_default_font *default_font;
    const mc_client_bitmap_font *font_candidate;
    const mc_client_loading_classes *font_classes;
    const mc_player_string *font_resource;
    bool q_stored;
    const mc_client_color_array *color_argument;
    const mc_client_color_array *color_results[MC_CLIENT_COLORIZER_COUNT];
    const mc_player_string *color_literals[MC_CLIENT_COLORIZER_COUNT];
    /* Successful native call completions; source return can precede cleanup refusal. */
    unsigned int color_lookups_completed, color_setters_completed;
    unsigned int last_color_lookup_pc, last_color_setter_pc;
    bool color_lookup_source_returned; /* Actual last lookup source progress, distinct native
                                          success. */
    bool colors_entered, color_metadata_retired;
    const mc_world_info_name *home, *appdata, *directory_child;
    const mc_client_startup_error *failure, *retirement;
    bool active, run_J_store_completed, display_created;
    bool Z_stored, aa_stored, z_stored, appdata_present;
    int32_t last_startup_store, last_directory_pc;
} mc_client_startup_state;

mc_client_startup_options mc_client_application_startup_default_options(void);
/* Native record/admission only. Requires the genuine constructor-READY App,
 * registered actual storage, no existing startup record or later owners.
 * It retains the supplied concrete services. No SDL/System/File IO or source
 * field store occurs here. App storage is borrowed until successful retire. */
mc_client_startup_status mc_client_application_startup_allocate(client_application *,
                                                                const mc_client_startup_services *,
                                                                const mc_client_startup_options *,
                                                                mc_client_application_startup **out,
                                                                mc_client_startup_error *);
/* One shot: actual run J=true2, then invokes startup a6. The implemented
 * null-Canvas/windowed prefix performs actual Display methods and the real
 * directory/SaveFormat/actual Settings, Pack, RenderEngine and bitmap FontRenderer calls. Its
 * finite later frontier is NOT a return from a749 or run32, and is never a whole-client/playability
 * claim. */
mc_client_startup_status mc_client_application_run_prefix(mc_client_application_startup *,
                                                          mc_client_startup_error *);
/* Actual f()V body: only J=false2. Requires live registered actual storage;
 * does not cancel/retire native owners or require startup to have completed. */
mc_client_startup_status mc_client_application_source_stop(client_application *,
                                                           mc_client_startup_error *);
bool mc_client_application_startup_inspect(const mc_client_application_startup *,
                                           mc_client_startup_state *);
/* Native terminal cleanup has no source J/o store. BUSY/error retains pointer
 * and all still-owned children. External operations are exclusive; source f
 * may be called serially while the retained stopped attempt remains live.
 * Strict font retirement precedes Engine/Pack/Settings/Display. Once that font
 * releases its leases, actual class-owned graphics/context resources can still
 * return BUSY, preserving those dependencies. The actual outer namespace owner
 * must explicitly coordinate retire_resources with Display alive; this App
 * never infers authority from occurrence counts or invokes global retirement.
 * Color pending prefixes/incoming occurrence/metadata retire only after the
 * strict font gate. Actual class colorizer statics are never reset by an App. */
mc_client_startup_status mc_client_application_startup_retire(mc_client_application_startup **,
                                                              mc_client_startup_error *);

#endif
