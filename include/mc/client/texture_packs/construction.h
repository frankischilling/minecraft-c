#ifndef MC_CLIENT_TEXTURE_PACKS_CONSTRUCTION_H
#define MC_CLIENT_TEXTURE_PACKS_CONSTRUCTION_H
#include "mc/client/texture_packs/texture_packs.h"
#include "mc/world/storage/java_file.h"

typedef struct mc_texture_pack_source_error {
    mc_texture_pack_error native;
    mc_java_file_error file;
} mc_texture_pack_source_error;

typedef struct mc_texture_pack_source_state {
    const mc_java_files *files;
    const mc_java_file *f, *pending_file;
    const mc_java_file_array *scan_files;
    const mc_java_file *scan_file;
    const mc_client_texture_pack *pending_pack;
    const mc_client_texture_string *pending_key;
    const void *pending_list;
    size_t scan_index, retained_pack_count;
    bool retiring, failed_scan;
    const mc_texture_pack_source_error *failure, *retirement;
} mc_texture_pack_source_state;

typedef struct mc_texture_pack_file_state {
    const mc_java_file *h;
    const mc_world_info_name *name_identity;
    mc_java_file_error file_error;
    mc_texture_pack_error retirement;
} mc_texture_pack_file_state;

/* SAME ik/i/pa/vp concrete owners and control flow. Native storage/lifetime
 * admission only at allocation; no source clinit exists in these four classes.
 * Resources is borrowed unchanged through final list/pack/stream retirement.
 * The actual App and its Settings arena must outlive the entire list; startup
 * enforces this by retaining its attachment until source_retire succeeds.
 * Null parent File is valid. Out is unchanged on native allocation refusal.
 * Typed paths use the canonical File namespace path/string/enumeration bounds;
 * max_path_bytes remains the unchanged legacy char-route transport bound.
 * max_packs additionally bounds this typed attempt's native keepalive vector:
 * removed packs remain retained until terminal cleanup so external borrowers
 * cannot outlive resources. This is native policy, never Java source failure.
 * Native literal occurrences are local to the retained attempt; cross-module
 * or whole-JVM String interning is not supplied by these APIs. */
mc_texture_pack_status mc_client_texture_pack_list_source_allocate(
    mc_java_files *, const mc_registry_resources *, const mc_texture_pack_options *,
    mc_client_texture_pack_list **out, mc_texture_pack_source_error *);
mc_texture_pack_status mc_client_texture_pack_list_source_construct(mc_client_texture_pack_list *,
                                                                    client_application *,
                                                                    mc_java_file *nullable_parent,
                                                                    mc_texture_pack_source_error *);
bool mc_client_texture_pack_list_source_inspect(const mc_client_texture_pack_list *,
                                                mc_texture_pack_source_state *);
bool mc_client_texture_pack_source_inspect(const mc_client_texture_pack *,
                                           mc_texture_pack_file_state *);
/* Terminal native cleanup. Other list/pack/stream occurrences yield BUSY and
 * preserve this occurrence and every borrowed dependency. Retryable child
 * failures preserve the list. A consumed FILE-close failure is recorded, never
 * retried against stale rights; a later retirement call continues cleanup.
 * Borrowed error/state views expire at the next mutating owner operation.
 * No source load/unload/close or J/o/D store is implied by this operation. */
mc_texture_pack_status
mc_client_texture_pack_list_source_retire(mc_client_texture_pack_list **occurrence,
                                          mc_texture_pack_source_error *);
#endif
