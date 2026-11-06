#ifndef MC_CLIENT_TEXTURE_PACKS_TEXTURE_PACKS_H
#define MC_CLIENT_TEXTURE_PACKS_TEXTURE_PACKS_H
#include "mc/client/render_engine/render_engine.h"
#include "mc/core/java/reader.h"
#include "mc/resources/image/image.h"
#include "mc/resources/translation/translation.h"

typedef struct client_application client_application;
typedef struct mc_client_texture_pack_list mc_client_texture_pack_list;
typedef struct mc_client_texture_pack mc_client_texture_pack;
typedef struct mc_client_texture_stream mc_client_texture_stream;
typedef struct mc_client_texture_string mc_client_texture_string;

typedef enum mc_texture_pack_status {
    MC_TEXTURE_PACK_OK,
    MC_TEXTURE_PACK_INVALID_ARGUMENT,
    MC_TEXTURE_PACK_INVALID_PHASE,
    MC_TEXTURE_PACK_LIMIT,
    MC_TEXTURE_PACK_ALLOCATION_FAILED,
    MC_TEXTURE_PACK_NULL_REFERENCE,
    MC_TEXTURE_PACK_SOURCE_IO,
    MC_TEXTURE_PACK_SOURCE_ILLEGAL_ARGUMENT,
    MC_TEXTURE_PACK_PROVIDER_FAILED,
    MC_TEXTURE_PACK_UNSUPPORTED_NATIVE,
    MC_TEXTURE_PACK_DIAGNOSTIC_DEPENDENCY,
    MC_TEXTURE_PACK_RENDER_DEPENDENCY,
    MC_TEXTURE_PACK_BUSY
} mc_texture_pack_status;

typedef enum mc_texture_pack_phase {
    MC_TEXTURE_PACK_RAW,
    MC_TEXTURE_PACK_CONSTRUCTING,
    MC_TEXTURE_PACK_READY,
    MC_TEXTURE_PACK_FAILED
} mc_texture_pack_phase;

typedef enum mc_texture_pack_kind {
    MC_TEXTURE_PACK_DEFAULT,
    MC_TEXTURE_PACK_ZIP
} mc_texture_pack_kind;

typedef struct mc_texture_pack_text {
    const uint16_t *units;
    size_t length;
    bool is_null;
} mc_texture_pack_text;

typedef struct mc_texture_pack_error {
    mc_texture_pack_status status, cause_status;
    const char *operation, *source_class, *source_method, *source_descriptor;
    unsigned int source_pc, ik_pc, constructor_pc;
    int system_error, zlib_error, native_close_error;
    mc_registry_status resource_status, native_close_status;
    mc_java_properties_status read_status;
    mc_java_reader_status reader_status;
    mc_image_status image_status;
    bool source_diagnostic_reached;
} mc_texture_pack_error;

typedef struct mc_texture_pack_options {
    size_t max_packs, max_directory_entries, max_path_bytes, max_name_units, max_streams;
    size_t max_archive_bytes, max_zip_entries, max_encoded_image_bytes;
    size_t max_decoded_image_bytes, max_stream_allocation_bytes;
    mc_java_reader_charset metadata_charset;
} mc_texture_pack_options;

typedef struct mc_texture_pack_list_state {
    mc_texture_pack_phase phase;
    size_t native_holds, listed_count, cached_count, active_calls;
    unsigned int last_store_pc;
    unsigned int rescan_store_pc;
    client_application *application;
    const char *directory;
    const void *list_identity, *cache_identity;
    const mc_client_texture_pack *default_pack, *selected_pack;
    const mc_client_texture_pack *failed_default_pack;
    mc_texture_pack_text selected_name;
    bool exists_reached, mkdirs_reached, mkdirs_result;
    mc_texture_pack_error failure;
} mc_texture_pack_list_state;

typedef struct mc_texture_pack_state {
    mc_texture_pack_kind kind;
    size_t native_holds, open_streams;
    const mc_client_texture_string *name, *description, *description2, *cache_key;
    const char *file;
    const mc_client_argb_image *icon;
    int32_t icon_id;
    bool zip_open;
} mc_texture_pack_state;

typedef struct mc_texture_stream_state {
    size_t bytes_read;
    bool source_closed;
    size_t source_close_calls, native_close_calls;
    const mc_client_texture_pack *pack;
} mc_texture_stream_state;

/* Actual ik/i/pa/vp owners. Creation performs no class/resource/filesystem use.
 * Construct preserves pa icon loading before directory and App.z/kv.l reads.
 * App/resources and its stored Settings String arena are borrowed and must
 * outlive every retained list/pack/stream, including failed prefixes. Native
 * holds do not imply source load/unload or GL operations. Use is exclusive;
 * outputs/errors must be disjoint from live owners and dependencies. */
mc_texture_pack_options mc_client_texture_pack_default_options(void);
mc_texture_pack_status mc_client_texture_pack_list_allocate(const mc_registry_resources *,
                                                            const mc_texture_pack_options *,
                                                            mc_client_texture_pack_list **);
mc_texture_pack_status mc_client_texture_pack_list_construct(mc_client_texture_pack_list *,
                                                             client_application *,
                                                             const char *nullable_parent_directory,
                                                             mc_texture_pack_error *);
mc_texture_pack_status mc_client_texture_pack_list_rescan(mc_client_texture_pack_list *,
                                                          mc_texture_pack_error *);
mc_texture_pack_status mc_client_texture_pack_list_retain(mc_client_texture_pack_list *);
mc_texture_pack_status mc_client_texture_pack_list_release(mc_client_texture_pack_list *,
                                                           mc_texture_pack_error *);
bool mc_client_texture_pack_list_inspect(const mc_client_texture_pack_list *,
                                         mc_texture_pack_list_state *);
/* Borrowed current field. Capture/retain it before an effectful operation;
 * this deliberately does not reject a source-null selected field. */
bool mc_client_texture_pack_selected(const mc_client_texture_pack_list *,
                                     mc_client_texture_pack **);
bool mc_client_texture_pack_at(const mc_client_texture_pack_list *, size_t,
                               mc_client_texture_pack **);
bool mc_client_texture_pack_cache_at(const mc_client_texture_pack_list *, size_t,
                                     const mc_client_texture_string **, mc_client_texture_pack **);
mc_texture_pack_status mc_client_texture_pack_retain(mc_client_texture_pack *);
mc_texture_pack_status mc_client_texture_pack_release(mc_client_texture_pack *,
                                                      mc_texture_pack_error *);
bool mc_client_texture_pack_inspect(const mc_client_texture_pack *, mc_texture_pack_state *);
bool mc_client_texture_string_read(const mc_client_texture_string *, mc_texture_pack_text *);

/* Actual i/vp resource operations. Missing succeeds with NULL; source read/close
 * IOException, native provider/encoding/ZIP/image dependency and native bounds
 * are distinct results. Streams retain the genuine pack. A failed source read
 * does not invent source close; native final release separately retires any
 * still-open host resources and preserves that distinction in inspection. */
mc_texture_pack_status mc_client_texture_pack_open(mc_client_texture_pack *, mc_texture_pack_text,
                                                   mc_client_texture_stream **,
                                                   mc_texture_pack_error *);
mc_texture_pack_status mc_client_texture_stream_read(mc_client_texture_stream *, uint8_t *, size_t,
                                                     size_t *, mc_texture_pack_error *);
mc_texture_pack_status mc_client_texture_stream_close(mc_client_texture_stream *,
                                                      mc_texture_pack_error *);
mc_texture_pack_status mc_client_texture_stream_release(mc_client_texture_stream *,
                                                        mc_texture_pack_error *);
bool mc_client_texture_stream_inspect(const mc_client_texture_stream *, mc_texture_stream_state *);
#endif
