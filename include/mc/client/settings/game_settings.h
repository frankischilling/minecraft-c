#ifndef MC_CLIENT_GAME_SETTINGS_H
#define MC_CLIENT_GAME_SETTINGS_H

#include "mc/core/java/writer.h"

typedef struct mc_game_settings mc_game_settings;
typedef struct mc_key_binding mc_key_binding;

#define MC_GAME_SETTINGS_NAMED_BINDINGS 10u
#define MC_GAME_SETTINGS_MAX_BINDINGS 1024u
#define MC_GAME_SETTINGS_MAX_BINDING_OWNERS 256u
#define MC_GAME_SETTINGS_MAX_TEXT_UNITS 1048576u
#define MC_GAME_SETTINGS_MAX_PATH_BYTES 1048576u
#define MC_GAME_SETTINGS_MAX_INPUT_BYTES 16777216u
#define MC_GAME_SETTINGS_OWNER_BYTES 16777216u
#define MC_GAME_SETTINGS_READER_BYTES 33554432u

typedef enum mc_game_settings_status {
    MC_GAME_SETTINGS_OK,
    MC_GAME_SETTINGS_INVALID_ARGUMENT,
    MC_GAME_SETTINGS_ALLOCATION_FAILED,
    MC_GAME_SETTINGS_LIMIT,
    MC_GAME_SETTINGS_UNSUPPORTED_CHARSET,
    MC_GAME_SETTINGS_BUSY,
    MC_GAME_SETTINGS_NATIVE_IO_ERROR,
    MC_GAME_SETTINGS_DIAGNOSTIC_FAILED
} mc_game_settings_status;

typedef struct mc_game_settings_error {
    mc_game_settings_status status;
    int system_error;
    const char *operation;
} mc_game_settings_error;

/* NULL String and nonnull empty String are distinct. State/KeyBinding UTF16
 * views remain valid until this settings owner is retired. Empty values use
 * the canonical empty String; arbitrary distinct externally created empty
 * Java String objects have no identity representation at this seam. A diagnostic's
 * original-line view is borrowed only during its callback. Inputs may contain
 * NUL, colon, newline and lone surrogates; there is no escaping. */
typedef struct mc_game_settings_text {
    const uint16_t *units;
    size_t length;
    bool is_null;
} mc_game_settings_text;

/* The 23 scalar/String roles, including the two nullable String roles.
 * D/G/H retain source letters because their complete gameplay roles have not
 * been established. No slider, enum, key-code or finite-float clamp is added. */
typedef struct mc_game_settings_values {
    float music_volume, sound_volume, mouse_sensitivity; /* a, b, c */
    bool invert_mouse;                                   /* d */
    int32_t render_distance;                             /* e */
    bool view_bobbing, anaglyph, advanced_opengl;        /* f, g, h */
    int32_t performance_mode;                            /* i */
    bool fancy_graphics, ambient_occlusion;              /* j, k */
    mc_game_settings_text texture_pack;                  /* l */
    int32_t difficulty;                                  /* y */
    bool hide_gui, third_person_view, show_debug_info;   /* z, A, B */
    mc_game_settings_text last_server;                   /* C */
    bool source_D, smooth_camera, debug_camera;          /* D, E, F */
    float source_G, source_H;                            /* G, H */
    int32_t gui_scale;                                   /* I */
} mc_game_settings_values;

typedef struct mc_game_settings_state {
    mc_game_settings_values values;
    /* m..v construction order: forward,left,back,right,jump,inventory,drop,
     * chat,fog,sneak. These references are independent of the current w array. */
    mc_key_binding *named[MC_GAME_SETTINGS_NAMED_BINDINGS];
    mc_key_binding *const *bindings;
    size_t binding_count;
    bool bindings_null;
    void *minecraft;           /* borrowed x; never dereferenced by persistence */
    const char *options_path;  /* owned File-role N; NULL in noarg construction */
    const void *file_identity; /* native File owner identity, not Java address */
} mc_game_settings_state;

typedef enum mc_game_settings_source_exception {
    MC_GAME_SETTINGS_SOURCE_NONE,
    MC_GAME_SETTINGS_SOURCE_NULL_FILE,
    MC_GAME_SETTINGS_SOURCE_FILE_IO,
    MC_GAME_SETTINGS_SOURCE_MISSING_SPLIT_VALUE,
    MC_GAME_SETTINGS_SOURCE_NUMBER_FORMAT,
    MC_GAME_SETTINGS_SOURCE_NULL_BINDING_ARRAY,
    MC_GAME_SETTINGS_SOURCE_NULL_BINDING
} mc_game_settings_source_exception;

typedef enum mc_game_settings_diagnostic_kind {
    MC_GAME_SETTINGS_BAD_OPTION,
    MC_GAME_SETTINGS_LOAD_FAILED,
    MC_GAME_SETTINGS_SAVE_FAILED
} mc_game_settings_diagnostic_kind;

typedef struct mc_game_settings_diagnostic {
    mc_game_settings_diagnostic_kind kind;
    const char *message; /* exact source message/prefix, without invented stack */
    mc_game_settings_source_exception exception;
    size_t line_number; /* 1 based for bad option; 0 for outer handler */
    mc_game_settings_text line;
    int system_error;
} mc_game_settings_diagnostic;

/* Optional native diagnostic delivery; not an emulation of System.out or a
 * fabricated Java stack trace. A callback may inspect this owner but must not
 * mutate/reenter/retire it. Callback failure is native, not a caught source IO.
 * Context remains borrowed through the call. An operation string returned in
 * an error must remain live while the caller uses that error. */
typedef struct mc_game_settings_diagnostics {
    void *context;
    mc_game_settings_status (*emit)(void *context, const mc_game_settings_diagnostic *event,
                                    mc_game_settings_error *error);
} mc_game_settings_diagnostics;

typedef struct mc_game_settings_profile {
    mc_java_reader_charset charset;
    mc_java_writer_lines lines;
} mc_game_settings_profile;

typedef struct mc_game_settings_report {
    size_t lines_read, bad_options, bytes_read, println_attempts;
    bool file_absent, source_opened, source_close_reached;
    bool outer_exception_caught;
    mc_game_settings_source_exception outer_exception;
    int source_system_error;
    bool writer_state_present;
    mc_java_writer_state writer_state; /* passive; never calls checkError */
    bool native_input_admission_close;
    bool native_additional_close, native_cleanup_failed;
    int native_cleanup_system_error;
} mc_game_settings_report;

/* Defaults reproduce kv() without I/O. A NULL profile selects native charset
 * discovery and the host line separator; discovery is deferred until I/O.
 * The file constructor reproduces new File(directory,"options.txt") then
 * load. NULL directory means the relative child, as File(File,null-parent)
 * does; it is distinct from the noarg constructor's null File. Paths are
 * UTF8 native filenames, strict on Windows, at most MAX_PATH_BYTES bytes.
 * No directory creation occurs. A source-caught load error still publishes
 * the partially loaded owner with native OK. Native failure preserves out.
 * Report is populated for reached work even on native failure. */
mc_game_settings_status mc_game_settings_create_defaults(const mc_game_settings_profile *profile,
                                                         mc_game_settings **out,
                                                         mc_game_settings_error *error);
mc_game_settings_status mc_game_settings_create_file(
    void *minecraft, const char *directory_utf8, const mc_game_settings_profile *profile,
    const mc_game_settings_diagnostics *diagnostics, mc_game_settings **out,
    mc_game_settings_report *report, mc_game_settings_error *error);

bool mc_game_settings_read_state(const mc_game_settings *settings, mc_game_settings_state *out);
bool mc_key_binding_read(const mc_key_binding *binding, mc_game_settings_text *name, int32_t *code);

/* Direct public-field assignments, not kv's effectful option setters. Copy
 * external strings into retained owner storage before committing the values;
 * existing owner/literal String views preserve their identity. Two identical
 * input views in one values assignment retain their common String identity.
 * No save, SoundManager, RenderEngine or RenderGlobal effect is invented. */
mc_game_settings_status mc_game_settings_set_values(mc_game_settings *settings,
                                                    const mc_game_settings_values *values,
                                                    mc_game_settings_error *error);
mc_game_settings_status mc_game_settings_binding_create(mc_game_settings *settings,
                                                        mc_game_settings_text name, int32_t code,
                                                        mc_key_binding **out,
                                                        mc_game_settings_error *error);
mc_game_settings_status mc_game_settings_binding_set(mc_game_settings *settings,
                                                     mc_key_binding *binding,
                                                     mc_game_settings_text name, int32_t code,
                                                     mc_game_settings_error *error);
mc_game_settings_status mc_game_settings_set_named_binding(mc_game_settings *settings, size_t index,
                                                           mc_key_binding *binding,
                                                           mc_game_settings_error *error);
/* Replace w with a distinct retained array of the same binding identities;
 * duplicates/NULL entries and a NULL array are allowed. A nonnull empty array
 * remains distinct from NULL. Bindings must belong to this owner. Named
 * fields are not changed. Earlier array views remain valid until retirement. */
mc_game_settings_status mc_game_settings_set_binding_array(mc_game_settings *settings,
                                                           mc_key_binding *const *bindings,
                                                           size_t count, bool is_null,
                                                           mc_game_settings_error *error);

/* Source Exception handlers produce native OK plus explicit report/diagnostic.
 * Native limits/allocations/unsupported charsets/diagnostic failures propagate
 * separately. Earlier field and filesystem effects are never rolled back.
 * Report/error must not alias owner storage. All operations require exclusive
 * access; mutating reentry returns BUSY. Output snapshots are borrowed views. */
mc_game_settings_status mc_game_settings_load(mc_game_settings *settings,
                                              const mc_game_settings_diagnostics *diagnostics,
                                              mc_game_settings_report *report,
                                              mc_game_settings_error *error);
mc_game_settings_status mc_game_settings_save(mc_game_settings *settings,
                                              const mc_game_settings_diagnostics *diagnostics,
                                              mc_game_settings_report *report,
                                              mc_game_settings_error *error);
/* I/O calls retire their native streams before returning, separately from
 * source close. This releases only memory; no implicit save/flush/source close.
 * NULL *settings is harmless; BUSY preserves the owner. */
mc_game_settings_status mc_game_settings_retire(mc_game_settings **settings,
                                                mc_game_settings_error *error);

#endif
