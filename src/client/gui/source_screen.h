#ifndef MC_CLIENT_GUI_SOURCE_SCREEN_H
#define MC_CLIENT_GUI_SOURCE_SCREEN_H

#include "../images/class_stream.h"
#include "mc/block/registry/runtime.h"
#include "mc/core/java/reader.h"

typedef struct client_application client_application;
typedef struct mc_client_bitmap_font mc_client_bitmap_font;
typedef struct mc_client_source_da mc_client_source_da;
typedef struct mc_client_source_fu mc_client_source_fu;
typedef struct mc_client_source_du mc_client_source_du;
typedef struct mc_client_source_ke mc_client_source_ke;
typedef struct mc_client_screen_classes mc_client_screen_classes;
typedef struct mc_client_screen_list mc_client_screen_list;
typedef struct mc_client_screen_initialization mc_client_screen_initialization;
typedef struct mc_client_screen_init_error mc_client_screen_init_error;

typedef enum mc_client_screen_status {
    MC_CLIENT_SCREEN_OK,
    MC_CLIENT_SCREEN_INVALID_ARGUMENT,
    MC_CLIENT_SCREEN_INVALID_PHASE,
    MC_CLIENT_SCREEN_BUSY,
    MC_CLIENT_SCREEN_LIMIT,
    MC_CLIENT_SCREEN_ALLOCATION_FAILED,
    MC_CLIENT_SCREEN_THREAD_FAILED,
    MC_CLIENT_SCREEN_RANDOM_FAILED,
    MC_CLIENT_SCREEN_CLASS_ERRONEOUS,
    MC_CLIENT_SCREEN_STRING_FAILED,
    MC_CLIENT_SCREEN_REGISTRY_FAILED,
    MC_CLIENT_SCREEN_STREAM_FAILED,
    MC_CLIENT_SCREEN_READER_FAILED,
    MC_CLIENT_SCREEN_APP_FAILED,
    MC_CLIENT_SCREEN_FONT_FAILED,
    MC_CLIENT_SCREEN_CALENDAR_FAILED,
    MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE,
    MC_CLIENT_SCREEN_UNIMPLEMENTED_DEPENDENCY
} mc_client_screen_status;

typedef enum mc_client_screen_phase {
    MC_CLIENT_SCREEN_RAW,
    MC_CLIENT_SCREEN_CONSTRUCTING,
    MC_CLIENT_SCREEN_READY,
    MC_CLIENT_SCREEN_FAILED,
    MC_CLIENT_SCREEN_RETIRING
} mc_client_screen_phase;

typedef enum mc_client_screen_class_phase {
    MC_CLIENT_SCREEN_CLASS_COLD,
    MC_CLIENT_SCREEN_CLASS_INITIALIZING,
    MC_CLIENT_SCREEN_CLASS_READY,
    MC_CLIENT_SCREEN_CLASS_FAILED
} mc_client_screen_class_phase;

typedef enum mc_client_screen_exception {
    MC_CLIENT_SCREEN_NO_EXCEPTION,
    MC_CLIENT_SCREEN_NULL_STREAM_EXCEPTION,
    MC_CLIENT_SCREEN_READ_IO_EXCEPTION,
    MC_CLIENT_SCREEN_EMPTY_SPLASH_EXCEPTION
} mc_client_screen_exception;

typedef struct mc_client_screen_site {
    const char *class_name, *method, *descriptor;
    unsigned int pc;
} mc_client_screen_site;

typedef struct mc_client_screen_error {
    mc_client_screen_status status;
    const char *operation;
    mc_client_screen_site site;
    bool native_policy, source_returned;
    mc_java_random_profile_status random;
    mc_player_status string;
    mc_registry_error registry;
    mc_client_class_stream_error stream;
    mc_client_image_io_error input;
    mc_java_reader_status reader;
    int native_thread_error;
    mc_client_screen_status first_class_status;
    mc_client_screen_site parent_site;
    mc_client_screen_site first_class_site;
    mc_java_random_profile_status first_class_random;
    const mc_client_screen_init_error *initialization;
} mc_client_screen_error;

/* A String reference, not a copied label. NULL is kind NULL; Registry String
 * views borrow the exact configured Registry held by this screen namespace.
 * Player references retain the same canonical String. A Registry reference
 * supplied to a constructor must belong to that configured Registry owner. */
typedef enum mc_client_screen_text_kind {
    MC_CLIENT_SCREEN_TEXT_NULL,
    MC_CLIENT_SCREEN_TEXT_PLAYER,
    MC_CLIENT_SCREEN_TEXT_REGISTRY
} mc_client_screen_text_kind;

typedef struct mc_client_screen_text {
    mc_client_screen_text_kind kind;
    const mc_player_string *player;
    const mc_registry_string *registry;
} mc_client_screen_text;

bool mc_client_screen_text_read(mc_client_screen_text, mc_player_string_view *);

typedef struct mc_client_screen_options {
    /* Native retained-arena limit, not JVM heap/OOME timing. Includes this
     * object's raw owner and each arena record+block; canonical String/stream
     * child owners keep their own limits. NULL selects 1MiB/unlimited. */
    size_t maximum_bytes, maximum_allocations;
    size_t maximum_read_bytes, maximum_lines, maximum_name_bytes;
} mc_client_screen_options;

typedef struct mc_client_screen_da_fields {
    float k; /* inherited ub.k */
    client_application *b;
    int32_t c, d;
    const mc_client_screen_list *e;
    bool f;
    mc_client_bitmap_font *g;
    mc_client_source_du *h;
    mc_client_source_ke *a;
} mc_client_screen_da_fields;

typedef struct mc_client_screen_fu_fields {
    float i;
    const mc_player_string *j;
    mc_client_source_ke *l;
} mc_client_screen_fu_fields;

typedef struct mc_client_screen_state {
    mc_client_screen_phase phase;
    size_t holds, arena_bytes, allocations, allocation_attempts;
    bool is_menu;
    mc_client_screen_da_fields da;
    mc_client_screen_fu_fields fu;
    unsigned int last_pc, store_count;
    mc_client_screen_site stores[16];
    mc_client_screen_exception caught_exception;
    unsigned int caught_pc;
    mc_client_screen_error caught_cause;
    size_t splash_count, lines_read;
    const mc_player_string *last_line, *last_trimmed;
    const mc_client_class_stream *open_stream;
    bool input_reader_constructed, buffered_reader_constructed;
    mc_client_screen_error failure;
} mc_client_screen_state;

typedef struct mc_client_screen_class_state {
    mc_client_screen_class_phase phase;
    size_t holds;
    bool retiring;
    const mc_java_random *a, *pending_random;
    const mc_player_string *missingno, *splash_path;
    unsigned int last_pc, last_store_pc;
    mc_client_screen_error failure;
} mc_client_screen_class_state;

typedef struct mc_client_screen_du_state {
    mc_client_screen_phase phase;
    size_t holds;
    float k;
    const mc_client_screen_list *a;
    client_application *b;
    unsigned int last_pc;
    mc_client_screen_error failure;
} mc_client_screen_du_state;

typedef struct mc_client_screen_ke_fields {
    float k;
    int32_t a, b, c, d, f;
    mc_client_screen_text e;
    bool g, h;
} mc_client_screen_ke_fields;

typedef struct mc_client_screen_ke_state {
    mc_client_screen_phase phase;
    size_t holds;
    mc_client_screen_ke_fields fields;
    unsigned int last_pc, store_count, stores[11];
    mc_client_screen_error failure;
} mc_client_screen_ke_state;

/* Cold CPU namespace. profile/resources and their real underlying dependencies
 * outlive it and all views. Optional Registry is retained with CLIENT/same
 * profile admission, without active use. No Random/resource read is performed.
 * The CPU namespace TU has no Display/Font/stream method linkage. Active fu use
 * constructs one actual default Random0/4 then publishes static a7, retaining
 * its first failed prefix. No per-menu replacement Random or seed callback. */
mc_client_screen_status mc_client_screen_classes_create(mc_java_random_profile *,
                                                        const mc_registry_resources *,
                                                        mc_registry_classes *nullable_registry,
                                                        mc_client_screen_classes **,
                                                        mc_client_screen_error *);
mc_client_screen_status mc_client_screen_menu_active_use(mc_client_screen_classes *,
                                                         mc_client_screen_error *);
mc_client_screen_status mc_client_screen_classes_retain(mc_client_screen_classes *,
                                                        mc_client_screen_error *);
mc_client_screen_status mc_client_screen_classes_release(mc_client_screen_classes **,
                                                         mc_client_screen_error *);
mc_client_screen_status mc_client_screen_classes_inspect(mc_client_screen_classes *,
                                                         mc_client_screen_class_state *,
                                                         mc_client_screen_error *);

/* Named actual classes, no caller-selected virtual callback. fu_base returns
 * the SAME physical da identity suitable for the existing App.r field. FU new
 * performs class active use before RAW allocation. Outputs stay unchanged on
 * failure; constructors are one-shot and retain complete failed prefixes. */
mc_client_screen_status mc_client_screen_allocate(mc_client_screen_classes *,
                                                  const mc_client_screen_options *,
                                                  mc_client_source_da **, mc_client_screen_error *);
mc_client_screen_status mc_client_screen_construct(mc_client_source_da *, mc_client_screen_error *);
mc_client_screen_status mc_client_screen_menu_allocate(mc_client_screen_classes *,
                                                       const mc_client_screen_options *,
                                                       mc_client_source_fu **,
                                                       mc_client_screen_error *);
mc_client_screen_status mc_client_screen_menu_construct(mc_client_source_fu *,
                                                        mc_client_screen_error *);
mc_client_source_da *mc_client_screen_menu_base(mc_client_source_fu *);
mc_client_screen_status mc_client_screen_retain(mc_client_source_da *, mc_client_screen_error *);
mc_client_screen_status mc_client_screen_release(mc_client_source_da **, mc_client_screen_error *);
mc_client_screen_status mc_client_screen_retire(mc_client_source_da **, mc_client_screen_error *);
mc_client_screen_status mc_client_screen_inspect(mc_client_source_da *, mc_client_screen_state *,
                                                 mc_client_screen_error *);
mc_client_screen_status mc_client_screen_menu_release(mc_client_source_fu **,
                                                      mc_client_screen_error *);
mc_client_screen_status mc_client_screen_menu_retire(mc_client_source_fu **,
                                                     mc_client_screen_error *);

/* du has no Random constructor. At b17 it acquires its own screen-domain
 * borrow for a registered App. NULL/RAW prepared operands keep the explicit
 * stable caller contract through strict retirement and every exported view.
 * No App owning cycle, independent source-field publication or fake App. */
mc_client_screen_status mc_client_screen_particles_allocate(mc_client_screen_classes *,
                                                            const mc_client_screen_options *,
                                                            mc_client_source_du **,
                                                            mc_client_screen_error *);
mc_client_screen_status mc_client_screen_particles_construct(mc_client_source_du *,
                                                             client_application *,
                                                             mc_client_screen_error *);
mc_client_screen_status mc_client_screen_particles_retain(mc_client_source_du *,
                                                          mc_client_screen_error *);
mc_client_screen_status mc_client_screen_particles_release(mc_client_source_du **,
                                                           mc_client_screen_error *);
mc_client_screen_status mc_client_screen_particles_retire(mc_client_source_du **,
                                                          mc_client_screen_error *);
mc_client_screen_status mc_client_screen_particles_inspect(mc_client_source_du *,
                                                           mc_client_screen_du_state *,
                                                           mc_client_screen_error *);
mc_client_screen_status mc_client_screen_button_allocate(mc_client_screen_classes *,
                                                         mc_client_source_ke **,
                                                         mc_client_screen_error *);
mc_client_screen_status mc_client_screen_button_construct(mc_client_source_ke *, int32_t id,
                                                          int32_t x, int32_t y, int32_t width,
                                                          int32_t height, mc_client_screen_text,
                                                          mc_client_screen_error *);
mc_client_screen_status mc_client_screen_button_construct_default(mc_client_source_ke *, int32_t id,
                                                                  int32_t x, int32_t y,
                                                                  mc_client_screen_text,
                                                                  mc_client_screen_error *);
mc_client_screen_status mc_client_screen_button_retain(mc_client_source_ke *,
                                                       mc_client_screen_error *);
mc_client_screen_status mc_client_screen_button_release(mc_client_source_ke **,
                                                        mc_client_screen_error *);
mc_client_screen_status mc_client_screen_button_retire(mc_client_source_ke **,
                                                       mc_client_screen_error *);
mc_client_screen_status mc_client_screen_button_inspect(mc_client_source_ke *,
                                                        mc_client_screen_ke_state *,
                                                        mc_client_screen_error *);
bool mc_client_screen_list_size(const mc_client_screen_list *, size_t *);

/* All calls and views require exclusive external serialization of the shared
 * resource/profile/Registry graph. A native try-monitor additionally refuses
 * callback reentry/contention as BUSY. Strict retire refuses extra holds before
 * any revocation. BUSY/fallible child retirement preserves the same slot for
 * retry. Successful consumption invalidates views, not source field stores.
 * No source close occurs in fu.<init>; native retirement closes its retained
 * ClassStream then frees reader storage. A caught source Exception is reported
 * by inspection while the constructor normally returns with prior j intact.
 * da.a(App,II)/fu.b declarations are in source_screen_initialize.h.
 * Input/draw/actions and MC.a(da) adoption remain separate methods.
 * READY here means constructor only; initialization has separate retained state. */
#endif
