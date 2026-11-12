#ifndef MC_CLIENT_STATISTICS_WRITER_H
#define MC_CLIENT_STATISTICS_WRITER_H

#include "mc/core/java/reader.h"
#include "mc/core/java/writer.h"
#include "mc/entity/player/player.h"
#include "mc/statistics/achievements/achievements.h"
#include "mc/world/storage/java_file.h"

typedef struct mc_client_stats_writer mc_client_stats_writer;
typedef struct mc_client_stats_classes mc_client_stats_classes;
typedef struct mc_client_stats_map mc_client_stats_map;
typedef struct mc_client_stats_text mc_client_stats_text;

typedef enum mc_client_stats_status {
    MC_CLIENT_STATS_OK,
    MC_CLIENT_STATS_INVALID_ARGUMENT,
    MC_CLIENT_STATS_INVALID_PHASE,
    MC_CLIENT_STATS_LIMIT,
    MC_CLIENT_STATS_ALLOCATION_FAILED,
    MC_CLIENT_STATS_REGISTRY_FAILED,
    MC_CLIENT_STATS_SESSION_FAILED,
    MC_CLIENT_STATS_IO_FAILED,
    MC_CLIENT_STATS_THREAD_FAILED,
    MC_CLIENT_STATS_BUSY,
    MC_CLIENT_STATS_UNSUPPORTED_PROFILE,
    MC_CLIENT_STATS_SOURCE_EXCEPTION,
    MC_CLIENT_STATS_DIAGNOSTIC_FAILED,
    MC_CLIENT_STATS_UNIMPLEMENTED_DEPENDENCY
} mc_client_stats_status;

typedef enum mc_client_stats_exception {
    MC_CLIENT_STATS_EXCEPTION_NONE,
    MC_CLIENT_STATS_NULL_REFERENCE,
    MC_CLIENT_STATS_NUMBER_FORMAT,
    MC_CLIENT_STATS_JSON_SYNTAX,
    MC_CLIENT_STATS_JSON_PATH,
    MC_CLIENT_STATS_ILLEGAL_STATE,
    MC_CLIENT_STATS_NO_SUCH_ELEMENT,
    MC_CLIENT_STATS_INDEX_OUT_OF_BOUNDS,
    MC_CLIENT_STATS_SOURCE_IO
} mc_client_stats_exception;

/* A stable nonrecursive child snapshot for a reached diagnostic frontier. */
typedef struct mc_client_stats_source_cause {
    mc_client_stats_status status;
    mc_client_stats_exception exception;
    const char *operation, *class_name, *method, *descriptor;
    unsigned int bytecode;
    int system_error;
    mc_java_reader_status reader;
    mc_java_file_error file;
} mc_client_stats_source_cause;

typedef struct mc_client_stats_site {
    const char *class_name, *method, *descriptor;
    unsigned int bytecode;
} mc_client_stats_site;

typedef struct mc_client_stats_error {
    mc_client_stats_status status;
    const char *operation;
    const char *class_name, *method, *descriptor;
    unsigned int bytecode;
    mc_client_stats_exception exception;
    size_t index, text_offset;
    int system_error;
    mc_registry_status registry;
    mc_player_status session;
    mc_java_reader_status reader;
    mc_java_writer_status writer;
} mc_client_stats_error;

typedef enum mc_client_stats_diagnostic_kind {
    MC_CLIENT_STATS_RELOCATING,
    MC_CLIENT_STATS_UNKNOWN_STATISTIC,
    MC_CLIENT_STATS_CHECKSUM_MISMATCH,
    MC_CLIENT_STATS_CAUGHT_EXCEPTION
} mc_client_stats_diagnostic_kind;

typedef struct mc_client_stats_diagnostic {
    mc_client_stats_diagnostic_kind kind;
    int32_t statistic_id;
    const char *path_utf8; /* Borrowed through this synchronous diagnostic only. */
    mc_client_stats_error cause;
} mc_client_stats_diagnostic;

/* Native diagnostic transport, not a stats/GUI/body replacement. A callback
 * may inspect this writer but must not mutate/reenter/retire it. It may run on
 * a real source worker. Keep its context live through joined retirement. NULL
 * emit selects concrete console diagnostics; source exception events have an
 * explicit native prefix instead of a fabricated Java Throwable stack. */
typedef struct mc_client_stats_diagnostics {
    void *context;
    mc_client_stats_status (*emit)(void *context, const mc_client_stats_diagnostic *event);
} mc_client_stats_diagnostics;

typedef struct mc_client_stats_limits {
    size_t max_allocation_bytes;
    size_t max_map_entries;
    size_t max_text_units;
    size_t max_json_nodes;
    size_t max_json_depth;
} mc_client_stats_limits;

typedef enum mc_client_stats_name_case {
    /* Explicit normal local/account ASCII username profile with English case.
     * Other UTF16 names stop at the actual first CJ.toLowerCase site; no host
     * locale approximation, filtering or guessed Unicode mapping is used. */
    MC_CLIENT_STATS_NAME_ENGLISH_ASCII
} mc_client_stats_name_case;

typedef struct mc_client_stats_options {
    mc_client_stats_limits limits; /* Zero fields select bounded normal defaults. */
    mc_java_reader_charset charset;
    mc_client_stats_name_case name_case;
    mc_client_stats_diagnostics diagnostics;
} mc_client_stats_options;

typedef struct mc_client_stats_string {
    const uint16_t *units;
    size_t length;
    bool is_null;
} mc_client_stats_string;

typedef struct mc_client_stats_report {
    size_t diagnostics, unknown_ids;
    bool checksum_mismatch, syntax_caught;
    bool file_exception_caught, print_trouble;
    mc_client_stats_error caught;
} mc_client_stats_report;

/* Source-only native diagnostic snapshot, never embedded in the legacy
 * charged writer/worker error/report layout. Stored in stats_source backing. */
typedef struct mc_client_stats_file_error {
    mc_client_stats_error base;
    mc_java_file_error file;
    mc_client_stats_source_cause caught;
    mc_client_stats_site callers[4];
    size_t caller_count;
} mc_client_stats_file_error;

typedef enum mc_client_stats_phase {
    MC_CLIENT_STATS_RAW,
    MC_CLIENT_STATS_CONSTRUCTING,
    MC_CLIENT_STATS_READY,
    MC_CLIENT_STATS_FAILED_CONSTRUCTION,
    MC_CLIENT_STATS_RETIRED
} mc_client_stats_phase;

typedef struct mc_client_stats_state {
    mc_client_stats_phase phase;
    unsigned int writer_pc, synchronizer_pc;
    const void *total_identity, *unsent_identity, *synchronizer_identity;
    size_t total_count, unsent_count;
    bool dirty, busy;
    const void *pending_main_identity, *pending_other_identity;
    int32_t delay, other_delay;
    /* e/f/i/j/g/h constructor order. Legacy path mode exposes its native
     * wrappers/UTF8 paths; typed mode exposes actual File identities and leaves
     * UTF8 views NULL. Identity observation does not admit handed-File mutation. */
    const void *file_identities[6];
    const char *file_paths_utf8[6];
    const mc_player_session *session;
    size_t workers_started, workers_completed, workers_joined, allocation_bytes;
    mc_client_stats_error last_worker_error;
    mc_client_stats_report last_worker_report;
} mc_client_stats_state;

typedef struct mc_client_stats_classes_state {
    size_t holds;
    bool active, resolved, failed, retiring;
    const mc_world_info_name *stats_literal;
    const mc_client_stats_error *failure;
} mc_client_stats_classes_state;

/* Cold native xi constant-pool lifetime, not a fabricated xi clinit. The
 * outer application class/runtime owns ONE shared scope across its writers.
 * Only the reached xi ldc36 resolves its real counted "stats" String.
 * All class/name occurrence mutations are on the foreground thread. No parser,
 * Registry, worker, clock, output or device is initialized by create. */
mc_client_stats_status mc_client_stats_classes_create(size_t max_string_bytes,
                                                      mc_client_stats_classes **,
                                                      mc_client_stats_error *);
mc_client_stats_status mc_client_stats_classes_retain(mc_client_stats_classes *,
                                                      mc_client_stats_error *);
mc_client_stats_status mc_client_stats_classes_release(mc_client_stats_classes **,
                                                       mc_client_stats_error *);
/* Strict outer retirement: a live writer's retained scope causes BUSY. */
mc_client_stats_status mc_client_stats_classes_retire(mc_client_stats_classes **,
                                                      mc_client_stats_error *);
bool mc_client_stats_classes_read_state(const mc_client_stats_classes *,
                                        mc_client_stats_classes_state *);

typedef struct mc_client_stats_file_state {
    const mc_client_stats_classes *classes;
    const mc_java_files *files;
    const mc_java_file *parent, *directory, *children[6], *pending_file;
    const mc_java_file_array *listing;
    size_t listing_index;
    const mc_world_info_name *pending_name;
    const mc_java_file_stream *pending_stream;
    bool pending_builder, pending_reader, retiring;
    const mc_client_stats_file_error *failure, *cleanup_failure;
} mc_client_stats_file_state;

/* The same canonical constructor schedule with concrete File transport.
 * files/classes are existing shared cold services, never created per writer.
 * Parent and Session retain actual null/source identity. All partial candidates
 * stay privately owned. The real constructor worker starts without a join.
 * Source diagnostic dependencies stop at their real call. Typed write failures
 * retain their actual File/Writer prefixes.
 * This additive mode preserves the legacy path API below and its allocation
 * ledger; source backing is separately charged. */
mc_client_stats_status
mc_client_stats_writer_construct_file(mc_client_stats_writer *, mc_client_stats_classes *,
                                      mc_player_session *, mc_java_files *, mc_java_file *,
                                      mc_client_stats_report *, mc_client_stats_error *);
/* Borrowed identities only, after worker quiescence/join; does not force a join.
 * No borrowed view authorizes concurrent mutation/inspection of a handed File. */
bool mc_client_stats_writer_read_file_state(mc_client_stats_writer *, mc_client_stats_file_state *);

/* Same canonical native retirement with an optional stable typed child copy.
 * It remains valid when successful cleanup consumes the failed writer. Legacy
 * retire below keeps its exact old result/slot behavior and old error layout. */
mc_client_stats_status mc_client_stats_writer_retire_file(mc_client_stats_writer **,
                                                          mc_client_stats_file_error *);

mc_client_stats_limits mc_client_stats_default_limits(void);
mc_client_stats_options mc_client_stats_default_options(void);

/* Actual client xi allocation/constructor. The exact Registry is retained
 * without class initialization. Construct builds a/b, dirty, stats-directory
 * migration and the actual CJ, including its real constructor worker. It does
 * not join/service that worker before returning. Failed prefixes stay owned
 * and inspectable; no retry. directory_utf8 is the game directory, not a World
 * save directory. NULL represents the source null File, with its reached
 * relative-stats/mkdir prefix before listFiles55 fails. Native UTF8 filesystem
 * paths follow the existing file adapters' bounded Windows/Linux profile.
 * The actual nullable Session remains borrowed during the call and is retained
 * at CJ's stored reference; it is not replaced with a username-only object.
 * No Minecraft, notification, network/session-check, Player or World tick runs. */
/* Registry initialization/retirement remains exclusive to this client cohort.
 * A cold jl is genuinely initialized at its first parsed ID lookup, possibly
 * on the real read worker; keep the Registry quiescent until that worker joins.
 * The normal READY catalog's immutable ID/GUID fields can be read while main
 * startup installs the separate achievement-description callback. Session
 * holds are admitted/retired on the calling client thread; workers read only
 * the retained Session's immutable username, never its native hold count. */
mc_client_stats_status mc_client_stats_writer_allocate(mc_registry_classes *registry,
                                                       const mc_client_stats_options *options,
                                                       mc_client_stats_writer **out,
                                                       mc_client_stats_error *error);
mc_client_stats_status mc_client_stats_writer_construct(mc_client_stats_writer *writer,
                                                        mc_player_session *session,
                                                        const char *directory_utf8,
                                                        mc_client_stats_report *report,
                                                        mc_client_stats_error *error);
bool mc_client_stats_writer_read_state(mc_client_stats_writer *writer, mc_client_stats_state *out);

/* Actual xi.a(ny): presence, including zero/negative values and a null key.
 * No dirty store, prerequisite check, notification, input or value test. */
mc_client_stats_status mc_client_stats_writer_contains(mc_client_stats_writer *writer,
                                                       const mc_statistic *achievement, bool *out,
                                                       mc_client_stats_error *error);
mc_client_stats_status mc_client_stats_writer_prerequisite(mc_client_stats_writer *writer,
                                                           const mc_statistic *achievement,
                                                           bool *out, mc_client_stats_error *error);
mc_client_stats_status mc_client_stats_writer_value(mc_client_stats_writer *writer,
                                                    const mc_statistic *statistic, int32_t *out,
                                                    mc_client_stats_error *error);
/* xi.a(vr,int): b += value, then a += value, then sticky dirty=true. */
mc_client_stats_status mc_client_stats_writer_increment(mc_client_stats_writer *writer,
                                                        const mc_statistic *statistic,
                                                        int32_t value,
                                                        mc_client_stats_error *error);
mc_client_stats_status mc_client_stats_writer_copy_unsent(mc_client_stats_writer *writer,
                                                          mc_client_stats_map **out,
                                                          mc_client_stats_error *error);
/* Actual xi.d at Minecraft.k18: gate async save before CJ counter/pending-map
 * service. This is one reached operation, not an implementation of k/ticks. */
mc_client_stats_status mc_client_stats_writer_service(mc_client_stats_writer *writer,
                                                      mc_client_stats_report *report,
                                                      mc_client_stats_error *error);
/* Actual xi.c: clone b and execute CJ's bounded-wait synchronous local save.
 * Source caught file errors are explicit report/diagnostic outcomes. No map
 * or dirty flag is cleared and no upload acknowledgment is invented. */
mc_client_stats_status mc_client_stats_writer_save(mc_client_stats_writer *writer,
                                                   mc_client_stats_report *report,
                                                   mc_client_stats_error *error);
/* Direct actual CJ.a refresh for finite source tests and real callers. */
mc_client_stats_status mc_client_stats_writer_start_read(mc_client_stats_writer *writer,
                                                         mc_client_stats_error *error);

/* Native joins do not service maps, change source counters, save or clear
 * fields. Retirement joins before freeing all retained graph/file/map/Session
 * ownership. An unjoined handle on native failure keeps the owner live. */
mc_client_stats_status mc_client_stats_writer_join(mc_client_stats_writer *writer,
                                                   mc_client_stats_error *error);
mc_client_stats_status mc_client_stats_writer_retire(mc_client_stats_writer **writer,
                                                     mc_client_stats_error *error);

/* Concrete identity/int Map and source static codec seam. Maps retain their
 * exact Registry through their shared native graph hold and may outlive a
 * writer. Worker map admission changes only that thread-safe shared lifetime;
 * it never changes the Registry's main-thread retain count. NULL keys are real map keys. Entry
 * inspection uses native first-insertion traversal, not a promised JVM
 * identity-hash iteration. Serialization uses that actual traversal and its
 * matching checksum; no sorting or captured JVM identity hashes are supplied.
 * Keep map views exclusive/live; no mutation while a worker/codec reads them.
 * Native failures preserve output slots and never emulate Java OOM timing. */
mc_client_stats_status mc_client_stats_map_create(mc_registry_classes *registry,
                                                  const mc_client_stats_options *options,
                                                  mc_client_stats_map **out,
                                                  mc_client_stats_error *error);
mc_client_stats_status mc_client_stats_map_retain(mc_client_stats_map *map,
                                                  mc_client_stats_error *error);
void mc_client_stats_map_retire(mc_client_stats_map **map);
size_t mc_client_stats_map_size(const mc_client_stats_map *map);
mc_client_stats_status mc_client_stats_map_put(mc_client_stats_map *map, const mc_statistic *key,
                                               int32_t value, mc_client_stats_error *error);
mc_client_stats_status mc_client_stats_map_get(const mc_client_stats_map *map,
                                               const mc_statistic *key, bool *present,
                                               int32_t *value, mc_client_stats_error *error);
mc_client_stats_status mc_client_stats_map_entry(const mc_client_stats_map *map, size_t index,
                                                 const mc_statistic **key, int32_t *value,
                                                 mc_client_stats_error *error);
mc_client_stats_status mc_client_stats_parse(mc_registry_classes *registry,
                                             const mc_client_stats_options *options,
                                             mc_client_stats_string json, mc_client_stats_map **out,
                                             mc_client_stats_report *report,
                                             mc_client_stats_error *error);
mc_client_stats_status mc_client_stats_serialize(const mc_client_stats_map *map,
                                                 mc_client_stats_string username,
                                                 mc_client_stats_string salt,
                                                 mc_client_stats_text **out,
                                                 mc_client_stats_error *error);
bool mc_client_stats_text_read(const mc_client_stats_text *text, mc_client_stats_string *out);
void mc_client_stats_text_retire(mc_client_stats_text **text);

#endif
