#ifndef MC_CLIENT_STATISTICS_INTERNAL_H
#define MC_CLIENT_STATISTICS_INTERNAL_H

#include "../../block/registry/registry_internal.h"
#include "../../block/registry/runtime_internal.h"
#include "../../entity/player/player_internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/client/statistics/writer.h"
#include "mc/core/java/integer_parse.h"
#include "mc/world/storage/java_file_writer.h"
#include <stdio.h>
#include <threads.h>

typedef struct stats_source stats_source;

/* Stack-only typed error dispatch around existing canonical error signatures.
 * No pointer to this scope survives into source backing; worker/foreground
 * scopes are independent native TLS call chains, with no allocation/callback. */
typedef struct stats_source_error_scope {
    struct stats_source_error_scope *previous;
    mc_client_stats_error *base;
    mc_client_stats_file_error value;
} stats_source_error_scope;

void stats_source_scope_open(stats_source_error_scope *, mc_client_stats_error *);
void stats_source_scope_close(stats_source_error_scope *);
mc_client_stats_file_error stats_source_snapshot(const mc_client_stats_error *);
void stats_source_child(mc_client_stats_error *, mc_java_file_error);
void stats_source_cause_set(mc_client_stats_error *, const mc_client_stats_error *);
void stats_source_publish_failure(mc_client_stats_writer *, const mc_client_stats_error *);

struct mc_client_stats_classes {
    size_t holds, max_string_bytes;
    bool active, failed, retiring;
    mc_world_info_name *stats_literal;
    mc_client_stats_error failure;
};

typedef struct stats_memory {
    mtx_t mutex;
    size_t holds, bytes, maximum;
    mc_registry_classes *registry; /* One main-admitted hold for the shared graph. */
    stats_source *source; /* Source-only backing; owner itself is uncharged legacy storage. */
} stats_memory;

typedef struct stats_entry {
    const mc_statistic *key;
    int32_t value;
    struct stats_entry *next;
} stats_entry;

struct mc_client_stats_map {
    mtx_t mutex;
    size_t holds, readers, count;
    stats_memory *memory;
    mc_registry_classes *registry;
    mc_client_stats_options options;
    stats_entry *head, *tail;
};

struct mc_client_stats_text {
    stats_memory *memory;
    uint16_t *units;
    size_t length;
};

typedef struct stats_builder {
    stats_memory *memory;
    uint16_t *units;
    size_t length, capacity, maximum;
} stats_builder;

typedef union stats_arena_block stats_arena_block;

union stats_arena_block {
    struct {
        stats_arena_block *next;
    } data;

    long double alignment;
    void *pointer_alignment;
    uint64_t integer_alignment;
};

typedef struct stats_arena {
    stats_memory *memory;
    stats_arena_block *blocks;
    mc_client_stats_status failure;
} stats_arena;

/* Concrete byte transport state, never a completed Reader/map callback. */
typedef struct stats_read_stream {
    FILE *stream;
    int system_error;
    bool source;
    mc_java_file_stream *file;
    mc_java_file_status file_status;
    mc_java_file_error file_error;
} stats_read_stream;

typedef struct stats_source_reader {
    struct stats_source_reader *next;
    stats_read_stream input;
    stats_arena arena;
    mc_java_reader *reader;
    stats_builder text;
    mc_client_stats_map *result;
    bool source_local_published;
} stats_source_reader;

struct stats_source {
    mc_client_stats_classes *classes;
    mc_java_files *files;
    mc_java_file *parent, *directory, *children[6], *pending_file;
    mc_java_file_array *listing;
    size_t listing_index;
    mc_world_info_name *pending_name;
    stats_builder *builder;
    stats_source_reader *readers, *current_reader;
    mc_client_stats_map *parse_candidate;
    mc_java_typed_file_writer *write_output;
    mc_java_typed_file_writer_error write_error, write_retirement;
    mc_client_stats_text *write_text;
    unsigned int write_pc;
    bool write_active, write_complete;
    bool retiring;
    mc_client_stats_file_error failure, cleanup_failure;
};

typedef struct stats_file {
    char *path;
} stats_file;

typedef struct stats_syncher {
    mc_client_stats_map *pending_main, *pending_other;
    stats_file *files[6]; /* e/f/i/j/g/h, actual constructor order. */
    mc_player_session *session;
    int32_t delay, other_delay;
    bool busy;
    unsigned int pc;
} stats_syncher;

typedef struct stats_worker {
    struct stats_worker *next;
    mc_client_stats_writer *owner;
    mc_client_stats_map *map;
    thrd_t thread;
    bool started, completed, joined, write;
    mc_client_stats_error error;
    mc_client_stats_report report;
} stats_worker;

struct mc_client_stats_writer {
    stats_memory *memory;
    mc_registry_classes *registry;
    mc_client_stats_options options;
    mtx_t mutex;
    bool entered, retiring;
    size_t diagnostic_depth;
    mc_client_stats_phase phase;
    unsigned int pc;
    mc_client_stats_map *total, *unsent;
    bool dirty;
    stats_syncher *syncher, *retained_syncher;
    stats_worker *workers;
    size_t workers_started, workers_completed, workers_joined;
    mc_client_stats_error last_worker_error;
    mc_client_stats_report last_worker_report;
};

void stats_error_clear(mc_client_stats_error *error);
mc_client_stats_status stats_fail(mc_client_stats_error *error, mc_client_stats_status status,
                                  const char *operation, const char *class_name, const char *method,
                                  const char *descriptor, unsigned int pc);
mc_client_stats_status stats_exception(mc_client_stats_error *error,
                                       mc_client_stats_exception exception, const char *operation,
                                       const char *class_name, const char *method,
                                       const char *descriptor, unsigned int pc);
mc_client_stats_options stats_options(const mc_client_stats_options *options);
stats_memory *stats_memory_create(size_t maximum);
bool stats_memory_retain(stats_memory *memory);
void stats_memory_release(stats_memory *memory);
void *stats_alloc(stats_memory *memory, size_t bytes, mc_client_stats_error *error);
void stats_free(void *storage);
size_t stats_memory_bytes(stats_memory *memory);
void *stats_arena_allocate(void *context, size_t bytes);
void stats_arena_clear(stats_arena *arena);
mc_client_stats_status stats_append(stats_builder *builder, const uint16_t *units, size_t length,
                                    mc_client_stats_error *error);
mc_client_stats_status stats_ascii(stats_builder *builder, const char *text,
                                   mc_client_stats_error *error);
mc_client_stats_status stats_string(stats_builder *builder, mc_client_stats_string text,
                                    mc_client_stats_error *error);
mc_client_stats_status stats_decimal(stats_builder *builder, int32_t value,
                                     mc_client_stats_error *error);
mc_client_stats_status stats_text_finish(stats_builder *builder, mc_client_stats_text **out,
                                         mc_client_stats_error *error);
void stats_builder_clear(stats_builder *builder);
mc_client_stats_status stats_map_make(mc_registry_classes *registry, stats_memory *memory,
                                      const mc_client_stats_options *options,
                                      mc_client_stats_map **out, mc_client_stats_error *error);
mc_client_stats_status stats_map_clone(const mc_client_stats_map *map, mc_client_stats_map **out,
                                       mc_client_stats_error *error);
mc_client_stats_status stats_map_add(mc_client_stats_map *map, const mc_statistic *key,
                                     int32_t value, mc_client_stats_error *error);
mc_client_stats_status stats_map_lease(const mc_client_stats_map *map,
                                       mc_client_stats_error *error);
void stats_map_unlease(const mc_client_stats_map *map);
bool stats_statistic_valid(mc_registry_classes *registry, const mc_statistic *statistic);
mc_client_stats_status stats_emit(mc_client_stats_writer *writer,
                                  const mc_client_stats_options *options,
                                  const mc_client_stats_diagnostic *event,
                                  mc_client_stats_report *report, mc_client_stats_error *error);
mc_client_stats_status stats_catch(mc_client_stats_writer *writer, mc_client_stats_report *report,
                                   mc_client_stats_error *error);
mc_client_stats_status stats_parse(mc_registry_classes *registry, stats_memory *memory,
                                   const mc_client_stats_options *options,
                                   mc_client_stats_writer *writer, mc_client_stats_string json,
                                   mc_client_stats_map **out, mc_client_stats_report *report,
                                   mc_client_stats_error *error);
mc_client_stats_status stats_checksum(stats_memory *memory, size_t maximum,
                                      mc_java_reader_charset charset, mc_client_stats_string salt,
                                      mc_client_stats_string payload, char out[33],
                                      mc_client_stats_error *error);

mc_client_stats_status stats_directory(mc_client_stats_writer *writer, const char *directory,
                                       char **stats_directory, mc_client_stats_report *report,
                                       mc_client_stats_error *error);
mc_client_stats_status stats_make_files(mc_client_stats_writer *writer, mc_player_session *session,
                                        const char *directory, mc_client_stats_error *error);
void stats_files_clear(stats_syncher *syncher);
bool stats_file_exists(const stats_file *file, mc_client_stats_error *error);
mc_client_stats_status stats_read_file_set(mc_client_stats_writer *writer, size_t dat_index,
                                           mc_client_stats_map **out,
                                           mc_client_stats_report *report,
                                           mc_client_stats_error *error);
mc_client_stats_status stats_write_file_set(mc_client_stats_writer *writer,
                                            const mc_client_stats_map *map, size_t dat_index,
                                            mc_client_stats_report *report,
                                            mc_client_stats_error *error);
mc_client_stats_status stats_start_worker(mc_client_stats_writer *writer, bool write,
                                          mc_client_stats_map *map, mc_client_stats_error *error);
mc_client_stats_status stats_join_workers(mc_client_stats_writer *writer, bool completed_only,
                                          mc_client_stats_error *error);
bool stats_enter(mc_client_stats_writer *writer, mc_client_stats_error *error);
void stats_leave(mc_client_stats_writer *writer);
mc_client_stats_status stats_apply_map(mc_client_stats_writer *writer,
                                       const mc_client_stats_map *map, int operation,
                                       mc_client_stats_error *error);

void stats_source_frame(mc_client_stats_error *, const char *, const char *, const char *,
                        unsigned int);
mc_client_stats_status stats_source_begin(mc_client_stats_writer *, mc_client_stats_classes *,
                                          mc_java_files *, mc_java_file *, mc_client_stats_error *);
mc_client_stats_status stats_source_directory(mc_client_stats_writer *, mc_client_stats_report *,
                                              mc_client_stats_error *);
mc_client_stats_status stats_source_make_files(mc_client_stats_writer *, mc_player_session *,
                                               mc_client_stats_error *);
mc_client_stats_status stats_source_file_error(mc_client_stats_error *, mc_java_file_status,
                                               mc_java_file_error, const char *, const char *,
                                               const char *, unsigned int);
bool stats_owner_file_exists(mc_client_stats_writer *, size_t, mc_client_stats_error *);
mc_client_stats_status stats_source_retire(mc_client_stats_writer *, mc_client_stats_error *);
mc_client_stats_status stats_source_write_file_set(mc_client_stats_writer *,
                                                   const mc_client_stats_map *, size_t,
                                                   mc_client_stats_report *,
                                                   mc_client_stats_error *);
mc_client_stats_status stats_source_save_caught(mc_client_stats_report *, mc_client_stats_error *);
mc_client_stats_status stats_source_diagnostic(mc_client_stats_writer *,
                                               const mc_client_stats_diagnostic *,
                                               mc_client_stats_report *, mc_client_stats_error *);
mc_client_stats_status stats_source_worker_caught(mc_client_stats_writer *,
                                                  mc_client_stats_report *,
                                                  mc_client_stats_error *);
mc_client_stats_status stats_source_caught(mc_client_stats_writer *, mc_client_stats_report *,
                                           mc_client_stats_error *, unsigned int);
bool stats_enter_retirement(mc_client_stats_writer *, mc_client_stats_error *);
mc_client_stats_status stats_classes_literal(mc_client_stats_classes *, mc_world_info_name **,
                                             mc_client_stats_error *);
#endif
