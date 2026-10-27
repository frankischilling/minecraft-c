#ifndef MC_CLIENT_TEXTURE_PACKS_INTERNAL_H
#define MC_CLIENT_TEXTURE_PACKS_INTERNAL_H
#include "image_io.h"
#include "mc/client/texture_packs/texture_packs.h"
#include "zip.h"

struct mc_client_texture_string {
    uint16_t *units;
    size_t length, holds;
};

typedef struct texture_pack_vector {
    mc_client_texture_pack **entries;
    size_t count, capacity;
} texture_pack_vector;

typedef struct texture_pack_cache_entry {
    mc_client_texture_string *key;
    mc_client_texture_pack *pack;
} texture_pack_cache_entry;

typedef struct texture_pack_cache {
    texture_pack_cache_entry *entries;
    size_t count, capacity;
} texture_pack_cache;

typedef struct texture_file_info {
    bool exists, directory, regular, bytes_valid, modified_valid;
    int64_t bytes, modified;
} texture_file_info;

typedef struct texture_directory_entry {
    char *path;
    mc_client_texture_string *name;
} texture_directory_entry;

typedef struct texture_directory {
    texture_directory_entry *entries;
    size_t count;
} texture_directory;

struct mc_client_texture_pack {
    size_t holds, streams;
    mc_texture_pack_kind kind;
    const mc_registry_resources *resources;
    mc_texture_pack_options options;
    mc_client_texture_string *name, *description, *description2, *cache_key;
    char *file;
    mc_client_argb_image *icon;
    int32_t icon_id;
    texture_zip *zip;
};

struct mc_client_texture_pack_list {
    size_t holds, active_calls;
    mc_texture_pack_phase phase;
    mc_texture_pack_options options;
    const mc_registry_resources *resources;
    client_application *app;
    texture_pack_vector *listed;
    texture_pack_cache *cached;
    mc_client_texture_pack *default_pack, *selected, *failed_default;
    mc_texture_pack_text selected_name;
    mc_client_texture_string *selected_name_owned;
    char *directory;
    unsigned int last_store_pc;
    unsigned int rescan_store_pc;
    bool exists_reached, mkdirs_reached, mkdirs_result;
    mc_texture_pack_error failure;
};

struct mc_client_texture_stream {
    mc_client_texture_pack *pack;
    mc_java_properties_input resource;
    texture_zip_stream *zip;
    size_t bytes_read, source_close_calls, native_close_calls;
    bool source_closed, resource_open, active;
};

mc_texture_pack_status texture_pack_fail(mc_texture_pack_error *, mc_texture_pack_status,
                                         const char *, const char *, const char *, const char *,
                                         unsigned int, unsigned int);
mc_texture_pack_status texture_pack_zip_error(mc_texture_pack_error *, const texture_zip_error *);
mc_client_texture_string *texture_string_copy(mc_texture_pack_text, size_t,
                                              mc_texture_pack_error *);
mc_client_texture_string *texture_string_utf8(const char *, size_t, mc_texture_pack_error *);
char *texture_text_utf8(mc_texture_pack_text, size_t, bool, mc_texture_pack_error *);
bool texture_text_equal(mc_texture_pack_text, mc_texture_pack_text);
void texture_string_free(mc_client_texture_string *);
mc_texture_pack_status texture_string_retain(mc_client_texture_string *);
char *texture_path_join(const char *, const char *, size_t, mc_texture_pack_error *);
mc_texture_pack_status texture_file_stat(const char *, texture_file_info *,
                                         mc_texture_pack_error *);
bool texture_file_mkdirs(const char *, mc_texture_pack_error *);
mc_texture_pack_status texture_directory_read(const char *, const mc_texture_pack_options *,
                                              texture_directory *, mc_texture_pack_error *);
void texture_directory_free(texture_directory *);
FILE *texture_file_open(const char *, mc_texture_pack_error *);
mc_texture_pack_status texture_name_zip(mc_texture_pack_text, bool *, mc_texture_pack_error *);
mc_texture_pack_status texture_pack_new(mc_texture_pack_kind, const mc_registry_resources *,
                                        const mc_texture_pack_options *, mc_client_texture_pack **,
                                        mc_texture_pack_error *);
mc_texture_pack_status texture_default_construct(mc_client_texture_pack *, mc_texture_pack_error *);
mc_texture_pack_status texture_pack_rescan_body(mc_client_texture_pack_list *,
                                                mc_texture_pack_error *);
mc_texture_pack_status texture_zip_construct(mc_client_texture_pack *, const char *,
                                             mc_texture_pack_text, mc_texture_pack_error *);
mc_texture_pack_status texture_zip_metadata(mc_client_texture_pack *, mc_texture_pack_error *);
mc_texture_pack_status texture_pack_load(mc_client_texture_pack *, mc_texture_pack_error *);
mc_texture_pack_status texture_pack_unload(mc_client_texture_pack *, client_application *,
                                           mc_texture_pack_error *);
mc_texture_pack_status texture_pack_base_open(mc_client_texture_pack *, mc_texture_pack_text,
                                              mc_client_texture_stream **, mc_texture_pack_error *);
mc_texture_pack_status texture_stream_from_zip(mc_client_texture_pack *, texture_zip_stream *,
                                               mc_client_texture_stream **,
                                               mc_texture_pack_error *);
#endif
