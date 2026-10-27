#ifndef MC_CLIENT_RENDER_ENGINE_TEXTURE_LOOKUP_H
#define MC_CLIENT_RENDER_ENGINE_TEXTURE_LOOKUP_H
#include "color_array.h"
#include "mc/client/texture_packs/texture_packs.h"
#include "texture_calls_internal.h"

typedef enum mc_client_texture_lookup_status {
    MC_CLIENT_LOOKUP_OK,
    MC_CLIENT_LOOKUP_INVALID_ARGUMENT,
    MC_CLIENT_LOOKUP_INVALID_PHASE,
    MC_CLIENT_LOOKUP_LIMIT,
    MC_CLIENT_LOOKUP_ALLOCATION_FAILED,
    MC_CLIENT_LOOKUP_BUSY,
    MC_CLIENT_LOOKUP_NULL_REFERENCE,
    MC_CLIENT_LOOKUP_SOURCE_ILLEGAL_ARGUMENT,
    MC_CLIENT_LOOKUP_PACK_FAILED,
    MC_CLIENT_LOOKUP_BUFFER_FAILED,
    MC_CLIENT_LOOKUP_TEXTURE_FAILED,
    MC_CLIENT_LOOKUP_UNIMPLEMENTED,
    MC_CLIENT_LOOKUP_CLEANUP_FAILED,
    MC_CLIENT_LOOKUP_SOURCE_NEGATIVE_SIZE,
    MC_CLIENT_LOOKUP_ARRAY_FAILED
} mc_client_texture_lookup_status;

typedef struct mc_client_texture_lookup_options {
    mc_client_render_texture_options textures;
    size_t max_cache_entries, max_key_units, max_prefix_bytes;
} mc_client_texture_lookup_options;

typedef struct mc_client_texture_lookup_error {
    mc_client_texture_lookup_status status;
    const char *operation;
    mc_client_graphics_site site, dependency;
    mc_client_graphics_site frames[3]; /* Actual known calls, outermost first. */
    size_t frame_count;
    bool native_policy, caught_source_io;
    mc_texture_pack_error pack;
    mc_texture_pack_error cleanup_pack;
    mc_texture_pack_error stream_retirement, pack_retirement, list_retirement;
    mc_client_texture_body_error texture;
    mc_client_graphics_error buffer, cleanup_context;
    mc_client_render_engine_status owner;
    mc_client_color_array_error array;
} mc_client_texture_lookup_error;

typedef struct mc_client_texture_lookup_state {
    bool pending, active, source_returned, cache_hit, clear_completed;
    bool generated, source_ledger_appended, source_closed, cache_published;
    bool image_io_returned, native_stream_consumed, color_lookup;
    mc_client_graphics_site last_site;
    mc_client_texture generated_id, returned_id;
    const mc_client_texture_pack_list *captured_list;
    const mc_client_texture_pack *captured_pack;
    const mc_client_texture_stream *stream;
    const mc_client_argb_image *image;
    const mc_client_color_array
        *array; /* May be an unpublished FAILED native candidate; inspect phase. */
    mc_texture_stream_state stream_before_retirement;
    const mc_client_texture_lookup_error *failure, *cleanup;
} mc_client_texture_lookup_state;

mc_client_texture_lookup_options mc_client_texture_lookup_default_options(void);
/* Actual ji.b(String)I over the real l/selected-Pack and b Map. Native options
 * fix once per Engine; NULL reuses them. Counted view is immutable/live through
 * the call; null is {NULL,0,true}, empty {NULL,0,false}. No encoding occurs before
 * the actual Pack operation. Byte/FP/provider failures are not IOException.
 * The normal constructed e has one direct slot. Resource/class/Pack/App/source/
 * Settings lifetimes remain exclusive and live, including retained failures.
 * Native pending prefix must be retired before another lookup/registration.
 * Return/cache IDs are raw numeric values, not owned resource occurrences.
 * Supported ordinary/default/selected-ZIP/missing/cache paths use actual bodies.
 * ## transform77, diagnostic221, GL_CLAMP/mipmap/other provider frontiers remain
 * explicit. No source-finally, extra ID, d publication or rollback is supplied.
 * Error/output/slots are valid, aligned and disjoint from owners and each other.
 * Large writable error storage should be caller-heap-owned. Out stays unchanged
 * on every failure, including native cleanup failure after source return. */
mc_client_texture_lookup_status mc_client_render_engine_texture(
    mc_client_render_engine *, mc_client_display *, const mc_client_texture_lookup_options *,
    mc_texture_pack_text nullable_key, mc_client_texture *out, mc_client_texture_lookup_error *);

/* Read-only borrowed actual pending identities. Views end when the record is
 * retired. No source call/clock/decode/bind is performed. */
bool mc_client_render_engine_lookup_inspect(const mc_client_render_engine *,
                                            mc_client_texture_lookup_state *);
/* Native-only allocation-free retirement of one bounded CPU prefix. Source
 * close6 is never credited here. BUSY preserves all remaining occurrences;
 * non-BUSY stream/Pack close errors can consume owners and are retained exactly
 * once. No resource ID/List/cache is deleted. Once remaining owners are gone,
 * pending clears even if that final native close returned an error. The last
 * cleanup diagnostic remains until Engine retirement. */
mc_client_texture_lookup_status
mc_client_render_engine_retire_lookup(mc_client_render_engine *, mc_client_texture_lookup_error *);

/* Inspect actual b Map only, not a substitute call of ji.b. */
bool mc_client_render_engine_cached_texture(const mc_client_render_engine *, mc_texture_pack_text,
                                            bool *found, mc_client_texture *out);

/* Native client-device final-owner hooks. Caller has stopped all operations.
 * Retire pending first, then free metadata before final CPU Engine release.
 * CPU release stays BUSY while opaque metadata exists; it never links Pack/
 * Display teardown or a callback into the independent CPU owner. */
bool mc_client_texture_lookup_pending(const mc_client_render_engine *);
bool mc_client_texture_lookup_free_metadata(mc_client_render_engine *);
#endif
