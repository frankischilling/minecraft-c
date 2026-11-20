#ifndef MC_CLIENT_RENDER_ENGINE_H
#define MC_CLIENT_RENDER_ENGINE_H

#include "mc/client/fonts/default_font.h"
#include "mc/client/settings/game_settings.h"

typedef struct mc_client_render_engine mc_client_render_engine;
typedef struct mc_client_render_engine_classes mc_client_render_engine_classes;
typedef struct mc_client_texture_pack_list mc_client_texture_pack_list; /* actual ik, not Assets */
typedef struct mc_client_render_map mc_client_render_map;
typedef struct mc_client_render_list mc_client_render_list;
typedef struct mc_client_direct_byte_buffer mc_client_direct_byte_buffer;
typedef struct mc_client_direct_int_buffer mc_client_direct_int_buffer;
typedef struct mc_client_argb_image mc_client_argb_image;

typedef enum mc_client_render_engine_status {
    MC_CLIENT_RENDER_ENGINE_OK,
    MC_CLIENT_RENDER_ENGINE_INVALID_ARGUMENT,
    MC_CLIENT_RENDER_ENGINE_INVALID_PHASE,
    MC_CLIENT_RENDER_ENGINE_LIMIT,
    MC_CLIENT_RENDER_ENGINE_ALLOCATION_FAILED,
    MC_CLIENT_RENDER_ENGINE_CLASS_FAILED,
    MC_CLIENT_RENDER_ENGINE_FONT_FAILED,
    MC_CLIENT_RENDER_ENGINE_BUSY
} mc_client_render_engine_status;

typedef enum mc_client_render_engine_phase {
    MC_CLIENT_RENDER_ENGINE_RAW,
    MC_CLIENT_RENDER_ENGINE_CONSTRUCTING,
    MC_CLIENT_RENDER_ENGINE_READY,
    MC_CLIENT_RENDER_ENGINE_FAILED
} mc_client_render_engine_phase;

typedef struct mc_client_render_engine_error {
    mc_client_render_engine_status status, cause_status;
    const char *operation, *source_class, *ge_method;
    unsigned int source_pc, ji_pc, ge_pc;
    mc_client_font_error font;
} mc_client_render_engine_error;

typedef struct mc_client_render_engine_options {
    size_t max_constructor_bytes, max_constructor_allocations;
} mc_client_render_engine_options;

typedef struct mc_client_render_engine_class_state {
    size_t native_holds;
    bool ji_ready, ji_a;
    mc_client_render_engine_phase ge_phase;
    const mc_client_render_list *ge_a, *ge_b;
    mc_client_render_engine_error ge_failure;
    size_t native_resource_count, active_resource_calls;
    bool resource_retirement_started;
} mc_client_render_engine_class_state;

typedef struct mc_client_render_engine_fields {
    const mc_client_render_map *b, *c, *d, *h;
    const mc_client_render_list *g;
    const mc_client_direct_int_buffer *e;
    const mc_client_direct_byte_buffer *f;
    mc_game_settings *i;
    bool j, k;
    mc_client_texture_pack_list *l;
    mc_client_argb_image *m;
} mc_client_render_engine_fields;

typedef struct mc_client_render_engine_state {
    mc_client_render_engine_phase phase;
    size_t native_holds, constructor_bytes, constructor_allocations;
    unsigned int last_store_pc;
    bool graphics_created, graphics_disposed;
    uint32_t graphics_color;
    const mc_client_argb_image *failed_image;
    mc_client_render_engine_fields fields;
    mc_client_render_engine_error failure;
} mc_client_render_engine_state;

typedef struct mc_client_direct_buffer_state {
    const void *identity, *storage, *underlying_byte_buffer;
    size_t position, limit, capacity, storage_bytes;
    bool direct, native_order;
} mc_client_direct_buffer_state;

typedef struct mc_client_argb_image_state {
    size_t native_holds;
    uint32_t width, height, type;
    const uint32_t *pixels;
    size_t pixel_count;
    bool constructed;
} mc_client_argb_image_state;

/* Separate concrete ji/ge static namespace. No graphics, font discovery or
 * class initializer runs in create. ge first initializes its two actual Lists
 * at ji's first ge.d39, preserving/latching native failures and earlier stores.
 * These are unrelated to the existing bq/cv RenderClasses. Constructor bounds
 * count this implementation's child storage, distinct from Java OOME timing.
 * SIZE_MAX allocation count admits all; zero admits none. */
mc_client_render_engine_options mc_client_render_engine_default_options(void);
mc_client_render_engine_status
mc_client_render_engine_classes_create(const mc_client_render_engine_options *ge_options,
                                       mc_client_render_engine_classes **out);
/* Actual ji literal <clinit>: a=false at1, once. Call before actual new ji
 * allocation; ge/nw/font/device activity remains lazy. Native accounting is
 * protected separately; all non-atomic source namespace fields stay exclusive. */
mc_client_render_engine_status
mc_client_render_engine_classes_active_use(mc_client_render_engine_classes *,
                                           mc_client_render_engine_error *);
mc_client_render_engine_status
mc_client_render_engine_classes_retain(mc_client_render_engine_classes *);
mc_client_render_engine_status
mc_client_render_engine_classes_release(mc_client_render_engine_classes *);
bool mc_client_render_engine_classes_inspect(const mc_client_render_engine_classes *,
                                             mc_client_render_engine_class_state *);

/* Allocate the real RAW owner separately from ji.<init>(ik,kv). Retain the
 * genuine class/font owners, without source active use. Construct executes all
 * reached collections, direct buffers, ARGB image/Graphics and default-font
 * draw in source order. It performs no GL call, upload, startup publication or
 * texture-pack lookup. Nullable l/i identities are stored unchanged. Nonnull
 * ik requires its eventual genuine owner; Assets is never an ik substitute.
 * Settings/pack identities are borrowed and must outlive even a stored FAILED
 * prefix. No retry of a failed constructor. Explicit native retirement frees
 * retained failed resources without inventing source dispose/rollback effects.
 * Views expire on final release. Keep owners exclusive and error/output disjoint. */
mc_client_render_engine_status
mc_client_render_engine_allocate(mc_client_render_engine_classes *, mc_client_default_font *,
                                 const mc_client_render_engine_options *,
                                 mc_client_render_engine **out);
mc_client_render_engine_status mc_client_render_engine_construct(
    mc_client_render_engine *, mc_client_texture_pack_list *nullable_packs,
    mc_game_settings *nullable_settings, mc_client_render_engine_error *);
mc_client_render_engine_status mc_client_render_engine_retain(mc_client_render_engine *);
mc_client_render_engine_status mc_client_render_engine_release(mc_client_render_engine *);
/* Strict final native occurrence for a containing owner. Another Engine hold,
 * its constructing/resource/pending calls, or its actual final dependencies
 * yield BUSY without consuming this slot. Nonfinal class occurrence drop uses
 * the SAME atomic accounting and never retires unrelated shared class state.
 * Actual final class storage remains guarded by every namespace child/call.
 * No GL call, Pack/Settings release, source dispose or source field reset.
 * Keep this Engine and native font provider exclusive; source namespace fields
 * are not made generally thread safe by the class occurrence counter. */
mc_client_render_engine_status mc_client_render_engine_retire(mc_client_render_engine **occurrence,
                                                              mc_client_render_engine_error *);
bool mc_client_render_engine_inspect(const mc_client_render_engine *,
                                     mc_client_render_engine_state *);

bool mc_client_render_map_count(const mc_client_render_map *, size_t *);
bool mc_client_render_list_count(const mc_client_render_list *, size_t *);
bool mc_client_direct_int_buffer_inspect(const mc_client_direct_int_buffer *,
                                         mc_client_direct_buffer_state *);
bool mc_client_direct_byte_buffer_inspect(const mc_client_direct_byte_buffer *,
                                          mc_client_direct_buffer_state *);
bool mc_client_argb_image_inspect(const mc_client_argb_image *, mc_client_argb_image_state *);
/* Concrete TYPE_INT_ARGB BufferedImage storage for actual generated images.
 * No decoding, palette, captured raster or color conversion is substituted.
 * Positive dimensions, checked native byte quota, fixed8192-axis/64MiB plane
 * bounds; out preserved on failure. These are native limits, not full AWT.
 * set_pixel is the selected exact ARGB32 field write; invalid coordinates do
 * not change the image. Image retain/release and the occurrence snapshot in
 * inspect are atomic. Keep an existing live image occurrence while retaining,
 * inspecting or releasing another; final release cannot overlap a borrowed
 * view. Shape/pixels require publication synchronization and exclusive pixel
 * mutation; atomic occurrences do not make set_pixel concurrent-safe. Retain
 * before independent lifetime beyond its owner. */
mc_client_render_engine_status mc_client_argb_image_create(int32_t width, int32_t height,
                                                           size_t max_bytes,
                                                           mc_client_argb_image **out);
mc_client_render_engine_status mc_client_argb_image_set_pixel(mc_client_argb_image *, int32_t x,
                                                              int32_t y, uint32_t argb);
mc_client_render_engine_status mc_client_argb_image_retain(mc_client_argb_image *);
void mc_client_argb_image_release(mc_client_argb_image *);

#endif
