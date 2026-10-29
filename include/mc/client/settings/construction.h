#ifndef MC_CLIENT_SETTINGS_CONSTRUCTION_H
#define MC_CLIENT_SETTINGS_CONSTRUCTION_H

#include "mc/client/settings/game_settings.h"
#include "mc/world/storage/java_file.h"

typedef struct mc_game_settings_classes mc_game_settings_classes;
typedef struct client_application client_application;

typedef struct mc_java_typed_file_writer mc_java_typed_file_writer;
typedef struct mc_java_typed_file_writer_error mc_java_typed_file_writer_error;

typedef enum mc_game_settings_source_phase {
    MC_GAME_SETTINGS_SOURCE_RAW,
    MC_GAME_SETTINGS_SOURCE_CONSTRUCTING,
    MC_GAME_SETTINGS_SOURCE_READY,
    MC_GAME_SETTINGS_SOURCE_FAILED,
    MC_GAME_SETTINGS_SOURCE_RETIRING
} mc_game_settings_source_phase;

typedef enum mc_game_settings_class_phase {
    MC_GAME_SETTINGS_CLASS_COLD,
    MC_GAME_SETTINGS_CLASS_INITIALIZING,
    MC_GAME_SETTINGS_CLASS_READY,
    MC_GAME_SETTINGS_CLASS_PHASE_ERRONEOUS,
    MC_GAME_SETTINGS_CLASS_RETIRING
} mc_game_settings_class_phase;

typedef struct mc_game_settings_source_site {
    const char *class_name, *method, *descriptor;
    int32_t pc;
} mc_game_settings_source_site;

typedef struct mc_game_settings_source_error {
    mc_game_settings_error native;
    int thread_error;
    mc_game_settings_source_site site, child, exception_site;
    mc_java_file_error file;
    mc_java_writer_error output;
    mc_java_writer_state output_state;
    /* Borrowed from this owner's last typed save attempt until its next save
     * or retirement mutation. The actual FOS/Writer prefix stays retained. */
    const mc_java_typed_file_writer_error *typed_output;
    mc_game_settings_source_exception source_exception;
    const char *exception_class;
    /* NULL means the actual Java Throwable message was not constructed/known.
     * diagnostic_message is a separate actual source literal, never a fake
     * exception message or JVM stack. Native operation/system_error survive. */
    const char *exception_message, *diagnostic_message;
    mc_game_settings_text line;
    size_t line_number;
    bool output_reached, output_completed, native_stdout_failed;
} mc_game_settings_source_error;

typedef struct mc_game_settings_class_options {
    size_t maximum_array_bytes, maximum_arrays, maximum_string_bytes;
} mc_game_settings_class_options;

typedef struct mc_game_settings_class_state {
    mc_game_settings_class_phase phase;
    size_t holds, array_bytes, arrays;
    mc_world_info_name *const *J, *const *K, *const *L, *const *M;
    const mc_world_info_name *options_literal;
    mc_world_info_name *const *pending_array;
    size_t pending_length;
    bool stdout_captured, stdout_borrowed;
    mc_java_writer_state stdout_state;
    unsigned int last_store_pc;
    const mc_game_settings_source_error *failure, *retirement;
} mc_game_settings_class_state;

typedef struct mc_game_settings_source_state {
    mc_game_settings_source_phase phase;
    mc_java_file *N, *pending_file;
    mc_java_file_stream *pending_stream;
    const mc_game_settings_classes *classes;
    const mc_game_settings_source_error *failure, *retirement;
    const mc_game_settings_report *load_report;
    size_t source_stores;
    const int32_t *stores;
    int32_t last_store_pc, last_load_pc;
    int32_t last_save_pc;
    const mc_java_typed_file_writer *save_writer; /* Same next-mutation lifetime. */
    bool active, reader_present;
} mc_game_settings_source_state;

mc_game_settings_class_options mc_game_settings_class_default_options(void);
/* Cold counted native class storage only. files is the SAME counted File
 * namespace used by App.Z. Output profile selects the real native default
 * stdout encoding/line profile, independently of FileReader input profile.
 * No class fill, charset discovery, stdout capture, file IO or device effects. */
mc_game_settings_status
mc_game_settings_classes_create(mc_java_files *files,
                                const mc_game_settings_profile *output_profile,
                                const mc_game_settings_class_options *, mc_game_settings_classes **,
                                mc_game_settings_source_error *);
mc_game_settings_status mc_game_settings_classes_active_use(mc_game_settings_classes *,
                                                            mc_game_settings_source_error *);
mc_game_settings_status mc_game_settings_classes_retain(mc_game_settings_classes *,
                                                        mc_game_settings_source_error *);
mc_game_settings_status mc_game_settings_classes_release(mc_game_settings_classes **,
                                                         mc_game_settings_source_error *);
bool mc_game_settings_classes_inspect(mc_game_settings_classes *, mc_game_settings_class_state *);

/* Actual new kv191: activate its four real class arrays before RAW instance
 * allocation. No instance constructor/default stores or read happens here.
 * A native failure leaves out unchanged; a returned RAW owner must be retired. */
mc_game_settings_status mc_game_settings_source_allocate(mc_game_settings_classes *,
                                                         const mc_game_settings_profile *,
                                                         mc_game_settings **,
                                                         mc_game_settings_source_error *);
/* One shot actual kv(App,File): defaults/qb/w in actual order, x357, distinct
 * real child File361/368/N371, then SAME parser/load375. Null parent File is
 * valid. Failed native/unfinished source prefix stays on this SAME owner;
 * caller publishes App.z203 only after OK/READY. Actual App x is borrowed and
 * must stay live through final Settings retirement and all later borrowers.
 * Diagnostics are passive native observers after real supported stdout work. */
mc_game_settings_status mc_game_settings_source_construct(mc_game_settings *, client_application *,
                                                          mc_java_file *nullable_directory,
                                                          const mc_game_settings_diagnostics *,
                                                          mc_game_settings_source_error *);
bool mc_game_settings_source_inspect(const mc_game_settings *, mc_game_settings_source_state *);
/* Use the existing mc_game_settings_retire for BOTH representations. It now
 * preserves typed stream/File/class cleanup failures for retry, without save
 * or invented source close/finally. Old path owners keep their old contract. */

#endif
