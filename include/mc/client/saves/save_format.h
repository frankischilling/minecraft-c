#ifndef MC_CLIENT_SAVES_SAVE_FORMAT_H
#define MC_CLIENT_SAVES_SAVE_FORMAT_H
#include "mc/core/math/classes.h"
#include "mc/world/storage/gzip_input.h"
#include "mc/world/storage/java_file.h"
#include "mc/world/storage/source_handler.h"
#include "mc/world/storage/store.h"

typedef struct mc_client_save_formats mc_client_save_formats;
typedef struct mc_client_source_nl mc_client_source_nl;
typedef mc_client_source_nl mc_client_save_format;
typedef struct mc_client_save_selection mc_client_save_selection;
typedef struct mc_client_save_list mc_client_save_list;

typedef enum mc_client_save_status {
    MC_CLIENT_SAVE_OK,
    MC_CLIENT_SAVE_INVALID_ARGUMENT,
    MC_CLIENT_SAVE_STATE_ERROR,
    MC_CLIENT_SAVE_BUSY,
    MC_CLIENT_SAVE_LIMIT,
    MC_CLIENT_SAVE_ALLOCATION_FAILED,
    MC_CLIENT_SAVE_SOURCE_NULL_REFERENCE,
    MC_CLIENT_SAVE_SOURCE_EXCEPTION,
    MC_CLIENT_SAVE_FILE_ERROR,
    MC_CLIENT_SAVE_INFO_ERROR,
    MC_CLIENT_SAVE_MATH_ERROR,
    MC_CLIENT_SAVE_UNIMPLEMENTED_DEPENDENCY,
    MC_CLIENT_SAVE_HANDLER_ERROR
} mc_client_save_status;

typedef struct mc_client_save_site {
    const char *class_name, *descriptor;
    int pc;
} mc_client_save_site;

typedef struct mc_client_save_error {
    mc_client_save_status status;
    const char *operation;
    mc_client_save_site frames[4]; /* native source-context metadata, NOT Java stack frames */
    size_t frame_count;
    bool caught_exception;
    mc_java_file_error file;
    mc_world_info_error info;
    mc_math_class_error math;
    mc_gzip_error gzip, gzip_close;
    mc_nbt_stream_error stream, close;
    mc_client_save_site cause_frames[4];
    size_t cause_frame_count;
    const mc_world_source_error *handler; /* Borrowed complete native prefix from format. */
} mc_client_save_error;

typedef enum mc_client_save_phase {
    MC_CLIENT_SAVE_RAW,
    MC_CLIENT_SAVE_CONSTRUCTING,
    MC_CLIENT_SAVE_READY,
    MC_CLIENT_SAVE_FAILED,
    MC_CLIENT_SAVE_RETIRING
} mc_client_save_phase;

typedef enum mc_client_save_method {
    MC_CLIENT_SAVE_FORMAT_NAME,
    MC_CLIENT_SAVE_OPEN_HANDLER,
    MC_CLIENT_SAVE_LIST,
    MC_CLIENT_SAVE_FLUSH,
    MC_CLIENT_SAVE_READ_INFO,
    MC_CLIENT_SAVE_DELETE,
    MC_CLIENT_SAVE_RENAME,
    MC_CLIENT_SAVE_NEEDS_CONVERSION,
    MC_CLIENT_SAVE_CONVERT,
    MC_CLIENT_SAVE_METHOD_COUNT
} mc_client_save_method;

typedef struct mc_client_save_method_state {
    const char *descriptor, *owner;
    bool implemented_complete;
    const char *first_dependency;
    int first_dependency_pc;
} mc_client_save_method_state;

typedef struct mc_client_save_options {
    size_t max_owner_bytes, max_entries;
} mc_client_save_options;

typedef struct mc_client_save_format_state {
    mc_client_save_phase phase;
    size_t holds;
    unsigned int last_store_pc;
    bool active, retiring;
    mc_java_file *source_a; /* exact supplied saves-child File, never parent Z */
    mc_java_file *pending_argument, *pending_world, *pending_metadata, *pending_candidate;
    mc_java_file_stream *pending_stream;
    mc_gzip_input *pending_gzip;
    mc_nbt_stream *pending_reader;
    mc_nbt_document *pending_document;
    mc_world_info_data *pending_data;
    mc_java_file_array *pending_files;
    mc_client_save_list *pending_list;
    mc_client_save_selection *pending_selection;
    mc_world_info *pending_info;
    mc_world_info_name *pending_name;
    mc_world_store *pending_handler;
    const mc_world_source_error *handler_error; /* Read-state fills from the same owner. */
} mc_client_save_format_state;

typedef struct mc_client_save_selection_fields {
    mc_world_info_name *filename, *display_name; /* nullable, borrowed exact identities */
    int64_t last_played, size_on_disk;
    bool requires_conversion;
    unsigned int last_store_pc;
    mc_client_save_phase phase;
} mc_client_save_selection_fields;

mc_client_save_options mc_client_save_default_options(void);
/* Native shared lifetime/literal scope. No mkdir, clock, class fill or stream.
 * files is the actual common File namespace. math may be NULL: its absence is
 * checked only at reached in.a(String)101. No independent table is created;
 * future App/World adoption uses this SAME retained MathHelper class/table. */
mc_client_save_status mc_client_save_formats_create(mc_java_files *files, mc_math_classes *math,
                                                    const mc_client_save_options *options,
                                                    mc_client_save_formats **out,
                                                    mc_client_save_error *error);
/* Explicit cold native service binding. SAME File namespace; no class
 * activation, source clock or filesystem effect. Bind once before hi.open;
 * existing unbound callers retain their actual new0 frontier. No fallback.
 * Every format error.handler borrows the format's retained full child error
 * until its next mutation/cleanup/retirement; copy it while this owner is live. */
mc_client_save_status mc_client_save_formats_bind_handlers(mc_client_save_formats *,
                                                           mc_world_store_source_classes *,
                                                           mc_client_save_error *);
mc_client_save_status mc_client_save_formats_retain(mc_client_save_formats *classes,
                                                    mc_client_save_error *error);
mc_client_save_status mc_client_save_formats_release(mc_client_save_formats **occurrence,
                                                     mc_client_save_error *error);
mc_client_save_status mc_client_save_format_allocate(mc_client_save_formats *classes,
                                                     mc_client_save_format **out,
                                                     mc_client_save_error *error);
/* Actual hi(File)->tq(File). exists5; optional mkdirs12 boolean ignored; then
 * SAME File published at18. NULL fails at5; RAW attempt latches FAILED. Native
 * unpublished arguments/children remain separate from source_a. */
mc_client_save_status mc_client_save_format_construct(mc_client_save_format *raw,
                                                      mc_java_file *actual_saves_child,
                                                      mc_client_save_error *error);
mc_client_save_status mc_client_save_format_retain(mc_client_save_format *format,
                                                   mc_client_save_error *error);
mc_client_save_status mc_client_save_format_release(mc_client_save_format **occurrence,
                                                    mc_client_save_error *error);
mc_client_save_status mc_client_save_format_clear_failure(mc_client_save_format *format,
                                                          mc_client_save_error *error);
mc_client_save_status mc_client_save_format_read_state(const mc_client_save_format *format,
                                                       mc_client_save_format_state *out,
                                                       mc_client_save_error *error);
mc_client_save_status mc_client_save_format_name(mc_client_save_format *format,
                                                 mc_world_info_name **owned_literal,
                                                 mc_client_save_error *error);
/* Real standalone as/FIS/GZIP/DataInput/NBT route; NEVER opens a Store or
 * session.lock. Incremental reads preserve actual grouping/CRC/finally order.
 * Source exceptions run real close29 before the caught printStackTrace76/133
 * frontier; native policy/provider stops preserve their unfinished Source
 * prefix and use native retirement instead. Normal parse closes22, then actual
 * decoded Data is adopted and the canonical ei constructor runs. Missing
 * world/primary/old returns nullable Info. No whole-file decoder or clone is
 * substituted, and print/old-on-exception remains an explicit next body. */
mc_client_save_status mc_client_save_format_read_info(mc_client_save_format *format,
                                                      mc_world_info_name *folder,
                                                      mc_world_info **nullable_info,
                                                      mc_client_save_error *error);
mc_client_save_status mc_client_save_format_list(mc_client_save_format *format,
                                                 mc_client_save_list **out,
                                                 mc_client_save_error *error);
mc_client_save_status mc_client_save_format_needs_conversion(mc_client_save_format *format,
                                                             mc_world_info_name *folder, bool *out,
                                                             mc_client_save_error *error);
/* Actual hi new0 -> fm class activation -> SAME mx/fm Store constructor.
 * A bound class context is a native prerequisite. Success alone transfers at13;
 * failure preserves out and retains the candidate/full cause until cleanup.
 * Source IOException printing remains an honest57 frontier. No generic open. */
mc_client_save_status mc_client_save_format_open(mc_client_save_format *format,
                                                 mc_world_info_name *folder, bool create_players,
                                                 mc_world_store **out, mc_client_save_error *error);
mc_client_save_method_state mc_client_save_format_method_state(mc_client_save_method method);

/* Actual vb raw identity/constructor, shared by the reached list row body.
 * Scalars are copied and nullable String identities retained in6/11/16/22/28
 * order; no trimming, version predicate, sort or timestamp is invented. */
mc_client_save_status mc_client_save_selection_allocate(size_t max_bytes,
                                                        mc_client_save_selection **out,
                                                        mc_client_save_error *error);
mc_client_save_status mc_client_save_selection_construct(mc_client_save_selection *raw,
                                                         mc_world_info_name *filename,
                                                         mc_world_info_name *display_name,
                                                         int64_t last_played, int64_t size,
                                                         bool convert, mc_client_save_error *error);
mc_client_save_status mc_client_save_selection_retain(mc_client_save_selection *selection,
                                                      mc_client_save_error *error);
mc_client_save_status mc_client_save_selection_release(mc_client_save_selection **occurrence,
                                                       mc_client_save_error *error);
mc_client_save_status mc_client_save_selection_read(const mc_client_save_selection *selection,
                                                    mc_client_save_selection_fields *out,
                                                    mc_client_save_error *error);
mc_client_save_status mc_client_save_selection_compare(const mc_client_save_selection *a,
                                                       const mc_client_save_selection *b,
                                                       int32_t *out, mc_client_save_error *error);
/* Stable natural ordering of the SAME canonical List and vb row identities:
 * lastPlayed descending, then Java UTF16 filename comparison. Equal comparisons
 * retain insertion order. Shared List aliases remain valid. Comparison uses a
 * bounded temporary pointer array before committing the permutation, so child
 * failure preserves the original List order. No metadata, String, timestamp or
 * row is copied/reconstructed. Requires exclusive access; no filesystem work. */
mc_client_save_status mc_client_save_list_sort(mc_client_save_list *, mc_client_save_error *);
size_t mc_client_save_list_size(const mc_client_save_list *list);
mc_client_save_selection *mc_client_save_list_at(const mc_client_save_list *list, size_t index);
mc_client_save_status mc_client_save_list_retain(mc_client_save_list *list,
                                                 mc_client_save_error *error);
mc_client_save_status mc_client_save_list_release(mc_client_save_list **occurrence,
                                                  mc_client_save_error *error);
#endif
