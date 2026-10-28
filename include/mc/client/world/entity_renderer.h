#ifndef MC_CLIENT_WORLD_ENTITY_RENDERER_H
#define MC_CLIENT_WORLD_ENTITY_RENDERER_H
#include "client/gui/bitmap_font.h"
#include "client/render_engine/color_array.h"
#include "client/render_engine/textures.h"
#include "mc/client/render/model.h"
#include "mc/client/render_engine/render_engine.h"
#include "mc/entity/player/player.h"
#include "millis.h"

typedef struct client_application client_application;
typedef struct mc_client_source_px mc_client_source_px;
typedef struct mc_client_source_ra mc_client_source_ra;
typedef struct mc_client_held_item mc_client_held_item;
typedef struct mc_client_source_iy mc_client_source_iy;

typedef enum mc_client_entity_renderer_status {
    MC_CLIENT_ENTITY_RENDERER_OK,
    MC_CLIENT_ENTITY_RENDERER_INVALID_ARGUMENT,
    MC_CLIENT_ENTITY_RENDERER_INVALID_PHASE,
    MC_CLIENT_ENTITY_RENDERER_BUSY,
    MC_CLIENT_ENTITY_RENDERER_LIMIT,
    MC_CLIENT_ENTITY_RENDERER_ALLOCATION_FAILED,
    MC_CLIENT_ENTITY_RENDERER_NULL_REFERENCE,
    MC_CLIENT_ENTITY_RENDERER_CLOCK_FAILED,
    MC_CLIENT_ENTITY_RENDERER_RANDOM_FAILED,
    MC_CLIENT_ENTITY_RENDERER_MODEL_FAILED,
    MC_CLIENT_ENTITY_RENDERER_ARRAY_FAILED,
    MC_CLIENT_ENTITY_RENDERER_IMAGE_FAILED,
    MC_CLIENT_ENTITY_RENDERER_GRAPHICS_FAILED
} mc_client_entity_renderer_status;

typedef enum mc_client_entity_renderer_phase {
    MC_CLIENT_ENTITY_RENDERER_RAW,
    MC_CLIENT_ENTITY_RENDERER_CONSTRUCTING,
    MC_CLIENT_ENTITY_RENDERER_READY,
    MC_CLIENT_ENTITY_RENDERER_FAILED,
    MC_CLIENT_ENTITY_RENDERER_RETIRING
} mc_client_entity_renderer_phase;

typedef struct mc_client_entity_renderer_error {
    mc_client_entity_renderer_status status;
    const char *operation;
    mc_client_graphics_site site;
    int32_t px_pc, ra_pc, iy_pc;
    bool native_policy; /* Local native admission/provider/frontier versus source NULL.
                         * Complete lower typed causes retain their own classification. */
    mc_platform_millis_status clock;
    mc_java_random_profile_status random;
    mc_client_model_error model;
    mc_client_color_array_error array;
    mc_client_render_engine_status image, owner;
    mc_client_bitmap_font_status font;
    mc_client_bitmap_font_error font_cleanup; /* Complete typed native release cause. */
    mc_client_graphics_error graphics;
    mc_client_render_texture_error texture;
} mc_client_entity_renderer_error;

typedef struct mc_client_entity_renderer_services {
    mc_client_render_classes *models;
    mc_client_render_engine_classes *graphics;
    mc_java_random_profile *profile; /* Borrowed through final owner retirement. */
    mc_client_display *display;      /* Native context binding, first used at iy39. */
} mc_client_entity_renderer_services;

typedef struct mc_client_entity_renderer_options {
    mc_client_render_engine_options arena;
    size_t model_bytes, image_bytes;
    mc_client_color_array_limits map_array;
} mc_client_entity_renderer_options;

typedef struct mc_client_entity_renderer_fields {
    client_application *j;
    float k;
    mc_client_source_ra *c;
    int32_t l;
    mc_entity *m;
    mc_player_mouse_filter_fields *filters[6]; /* n/o/p/q/r/s: six actual cu. */
    float t, u, v_to_C[8];
    bool D;
    double E, F, G;
    int64_t H, I;
    mc_java_random *J;
    int32_t K;
    struct mc_client_direct_float_buffer *f;
    float g, h, i, L, M;
} mc_client_entity_renderer_fields;

typedef struct mc_client_entity_renderer_state {
    mc_client_entity_renderer_phase phase, ra_phase, iy_phase;
    size_t holds, arena_bytes, arena_allocations, map_zero_stores;
    bool active, px_class_ready, px_a;
    int32_t px_b, d, e, last_store_pc, ra_last_store_pc, iy_last_store_pc;
    mc_client_entity_renderer_fields fields;
    const mc_java_random *pending_random; /* Native failed constructor candidate, not source J. */
    const mc_client_source_ra *pending_ra;
    mc_client_held_item *ra_occurrence; /* Borrowed native owner, actual RA is pending_ra. */
    const mc_client_source_iy *pending_iy;
    const mc_client_render_blocks *ra_blocks;
    client_application *ra_app;
    const void *ra_b;
    float ra_c, ra_d;
    int32_t ra_g;
    const mc_client_color_array *map_array, *pending_array;
    mc_game_settings *map_settings;
    mc_client_bitmap_font *map_font;
    mc_client_render_engine *map_engine;
    const mc_client_argb_image *map_image;
    int32_t map_texture;
    bool map_lease;
    const mc_client_entity_renderer_error *failure, *retirement;
} mc_client_entity_renderer_state;

/* Independent RA/IY inspection has no fabricated PX fields/class state. */
typedef struct mc_client_held_item_state {
    mc_client_entity_renderer_phase ra_phase, iy_phase;
    size_t holds, arena_bytes, arena_allocations, map_zero_stores;
    int32_t ra_last_store_pc, iy_last_store_pc;
    const mc_client_source_ra *pending_ra;
    const mc_client_source_iy *pending_iy;
    const mc_client_render_blocks *ra_blocks;
    client_application *ra_app;
    const void *ra_b;
    float ra_c, ra_d;
    int32_t ra_g;
    const mc_client_color_array *map_array, *pending_array;
    mc_game_settings *map_settings;
    mc_client_bitmap_font *map_font;
    mc_client_render_engine *map_engine;
    const mc_client_argb_image *map_image;
    int32_t map_texture;
    bool map_lease;
    const mc_client_entity_renderer_error *failure, *retirement;
} mc_client_held_item_state;

/* Actual px constructor only, not a render loop/method implementation.
 * EntityRenderer is the inferred project domain label for binary px.
 * Native class creation stays cold; actual new299 calls class_active_use first.
 * RAW allocate retains the SAME model/graphics namespaces and validates the
 * shared profile, with no source effect, clock, Random or device activation.
 * App is NULL or a genuine live App borrowed through final retirement; q/z/p
 * fields are read only at ra46/50/54. Settings and profile remain externally
 * live. No general App transfer/thread-safety or Java OOME equivalence claimed.
 * All native/source namespace access here is exclusive, except the two
 * source volatile cells use the existing seq_cst C17 policy. Class active-use
 * requires an externally live namespace. Inspection views expire at any owner
 * mutation/retirement; errors own only static text and by-value child causes.
 * Writable outputs/errors/slots must be valid, live and disjoint from every
 * owner/view being mutated or retired. Failed candidates retain their whole
 * prefix. No failed constructor retry.
 */
mc_client_entity_renderer_options mc_client_entity_renderer_default_options(void);
void mc_client_entity_renderer_error_init(mc_client_entity_renderer_error *);
mc_client_entity_renderer_status
mc_client_entity_renderer_class_active_use(mc_client_render_classes *,
                                           mc_client_entity_renderer_error *);
mc_client_entity_renderer_status
mc_client_entity_renderer_allocate(const mc_client_entity_renderer_services *,
                                   const mc_client_entity_renderer_options *,
                                   mc_client_source_px **out, mc_client_entity_renderer_error *);
mc_client_entity_renderer_status
mc_client_entity_renderer_construct(mc_client_source_px *, client_application *nullable_actual_app,
                                    mc_client_entity_renderer_error *);
mc_client_entity_renderer_status
mc_client_entity_renderer_retain(mc_client_source_px *, mc_client_entity_renderer_error *);
/* Successful release consumes/nulls this occurrence with remaining holds too.
 * Strict retire requires its final inactive hold; BUSY/failure preserves it.
 * One final body covers RAW/FAILED/READY and retained retry. No source delete,
 * J/o store, global namespace resource retirement or static reset is implied.
 * Release this owner BEFORE its actual sj/Engine/Display dependencies.
 */
mc_client_entity_renderer_status
mc_client_entity_renderer_release(mc_client_source_px **, mc_client_entity_renderer_error *);
mc_client_entity_renderer_status
mc_client_entity_renderer_retire(mc_client_source_px **, mc_client_entity_renderer_error *);
bool mc_client_entity_renderer_inspect(const mc_client_source_px *,
                                       mc_client_entity_renderer_state *);
/* One genuine RA/IY constructor, used by PX.c and independently by TH.f.
 * The independent owner retains the SAME model/graphics services. Its App and
 * Settings are borrowed through final retirement; successful IY retains the
 * actual captured Font/Engine. Native metadata has its own fixed sizeof bound,
 * outside the unchanged PX source arena quota. Source object identity is the
 * returned read-only RA identity, never this native occurrence handle.
 * All operations are exclusive; retain cannot cross active/RETIRING state.
 * Strict final retirement refuses external occurrences before child cleanup. */
mc_client_entity_renderer_status
mc_client_held_item_allocate(const mc_client_entity_renderer_services *,
                             const mc_client_entity_renderer_options *, mc_client_held_item **,
                             mc_client_entity_renderer_error *);
mc_client_entity_renderer_status mc_client_held_item_construct(mc_client_held_item *,
                                                               client_application *,
                                                               mc_client_entity_renderer_error *);
mc_client_entity_renderer_status mc_client_held_item_retain(mc_client_held_item *,
                                                            mc_client_entity_renderer_error *);
mc_client_entity_renderer_status mc_client_held_item_release(mc_client_held_item **,
                                                             mc_client_entity_renderer_error *);
mc_client_entity_renderer_status mc_client_held_item_retire(mc_client_held_item **,
                                                            mc_client_entity_renderer_error *);
const mc_client_source_ra *mc_client_held_item_identity(const mc_client_held_item *);
bool mc_client_held_item_is_final_inactive(const mc_client_held_item *);
bool mc_client_held_item_inspect(const mc_client_held_item *, mc_client_held_item_state *);
#endif
