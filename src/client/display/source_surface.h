#ifndef MC_CLIENT_DISPLAY_SOURCE_SURFACE_H
#define MC_CLIENT_DISPLAY_SOURCE_SURFACE_H
#include "display.h"

typedef struct mc_client_display_context_lease mc_client_display_context_lease;
typedef struct mc_client_display_list_range mc_client_display_list_range;
typedef struct mc_client_display_texture_lease mc_client_display_texture_lease;

/* Actual context/GL boundaries may change FP control bits. Reached native
 * conversion/matrix work checks the existing strict x86-64 policy again and
 * reports UNSUPPORTED_FP before that work. Prior GL, color, list and matrix
 * publications remain; no automatic FP repair or source rollback occurs.
 * Sticky exception flags alone remain admitted. Command compilation that only
 * copies arguments performs no matrix arithmetic or new FP admission. */

typedef enum mc_client_source_array {
    MC_CLIENT_SOURCE_VERTEX_ARRAY,
    MC_CLIENT_SOURCE_UV_ARRAY,
    MC_CLIENT_SOURCE_COLOR_ARRAY,
    MC_CLIENT_SOURCE_NORMAL_ARRAY
} mc_client_source_array;

/* Exactly one source GL11.glGetError query on the canonical live Display.
 * A nonzero GL enum is returned as source data, not a native GL failure.
 * No gl_check, pre-drain, post-drain, frame reset, swap or pump is added.
 * Ordinary native context/active/function admission can fail before querying;
 * failure preserves value. Keep owner/value/error storage live and disjoint. */
bool mc_client_display_source_get_error(mc_client_display *, uint32_t *value,
                                        mc_client_display_error *);
/* Exactly the reached glViewport, with signed source dimensions and sticky GL
 * errors left to the source. No ClearColor, GetError, clear, swap or pump. */
bool mc_client_display_source_viewport(mc_client_display *, int32_t width, int32_t height,
                                       mc_client_display_error *);

bool mc_client_display_context_acquire(mc_client_display *, mc_client_display_context_lease **,
                                       mc_client_display_error *);
bool mc_client_display_context_release(mc_client_display_context_lease **,
                                       mc_client_display_error *);
bool mc_client_display_source_pointer(mc_client_display_context_lease *, mc_client_source_array,
                                      const void *underlying_buffer, size_t byte_offset,
                                      mc_client_display_error *);
bool mc_client_display_source_array(mc_client_display *, mc_client_source_array, bool enabled,
                                    mc_client_display_error *);
/* Exact 32-byte source nw interleaved storage. The caller retains its real
 * byte-buffer owner through this call. In compilation this appends an actual
 * immutable draw command; in execution it issues the real GL3.3 draw. */
bool mc_client_display_source_batch(mc_client_display_context_lease *,
                                    const void *underlying_buffer, const uint8_t *bytes,
                                    size_t byte_count, int32_t count, int32_t mode,
                                    mc_client_display_error *);

bool mc_client_display_lists_generate(mc_client_display *, int32_t count, size_t max_commands,
                                      mc_client_display_list_range **, int32_t *base,
                                      mc_client_display_error *);
/* Selected GL_COMPILE bridge. Begin keeps the committed definition associated
 * with the SAME reserved name; only End publishes the complete pending chain.
 * Both definitions count against the existing native byte/command limits.
 * Native admission/allocation failures retain the exact pending prefix; they
 * neither publish it nor fabricate a source GL_OUT_OF_MEMORY. */
bool mc_client_display_lists_begin(mc_client_display *, int32_t id, mc_client_display_error *);
bool mc_client_display_lists_end(mc_client_display *, mc_client_display_error *);
/* Native terminal abort of exactly the owned pending definition. Does not
 * acknowledge a source glEndList, erase completed commands or restore matrices.
 * A foreign pending list is rejected and remains untouched. */
bool mc_client_display_lists_abort(mc_client_display *, int32_t owned_id,
                                   mc_client_display_error *);

typedef struct mc_client_source_list_state {
    bool defined, building;
    size_t commands, draws, vertices;
    size_t pending_commands, pending_bytes;
} mc_client_source_list_state;

typedef struct mc_client_source_batch_view {
    const uint8_t *bytes;
    size_t byte_count;
    int32_t vertices, mode;
    unsigned int arrays;
} mc_client_source_batch_view;

/* Passive inspection of committed commands and the retained pending prefix.
 * Pending bytes include actual command charges and the pending record itself.
 * Views expire at successful replacement of the SAME name or range retirement;
 * a pending view also expires when End publishes it. No activation, allocation,
 * GL or buffer-cursor work. Native abort leaves committed commands callable and
 * retains the pending prefix until range cleanup; it does not permit replay. */
bool mc_client_display_list_inspect(mc_client_display *, int32_t, mc_client_source_list_state *);
bool mc_client_display_list_batch(mc_client_display *, int32_t, size_t draw_index,
                                  mc_client_source_batch_view *);
bool mc_client_display_list_pending_batch(mc_client_display *, int32_t, size_t draw_index,
                                          mc_client_source_batch_view *);
bool mc_client_display_lists_call(mc_client_display *, const int32_t *ids, size_t count,
                                  mc_client_display_error *);
bool mc_client_display_lists_retire(mc_client_display_list_range **, mc_client_display_error *);

bool mc_client_display_source_color(mc_client_display *, float red, float green, float blue,
                                    float alpha, mc_client_display_error *);
bool mc_client_display_source_translate(mc_client_display *, float x, float y, float z,
                                        mc_client_display_error *);
bool mc_client_display_source_push(mc_client_display *, mc_client_display_error *);
bool mc_client_display_source_pop(mc_client_display *, mc_client_display_error *);
bool mc_client_display_source_texture_enable(mc_client_display *, bool enabled,
                                             mc_client_display_error *);
bool mc_client_display_source_alpha_test(mc_client_display *, bool enabled,
                                         mc_client_display_error *);
/* Selected MC.a400..460 source profile on the same current Display. Admit only
 * SMOOTH, depth1, LEQUAL, GREATER/reference bits0x3dcccccd, BACK and the two
 * PROJECTION/MODELVIEW modes. Unsupported profiles and compiled setter paths
 * fail natively; they do not synthesize a Java/GL error or acknowledge success.
 * Direct GL operations do not query/drain errors, clear, pump, swap, repair FP,
 * or reset unrelated state. Smooth/alpha use the existing actual source shader.
 * Matrix identity affects only the selected matrix; each has its own stack.
 * Existing compiled translations use the mode selected at list execution. */
bool mc_client_display_source_shade_model(mc_client_display *, int32_t mode,
                                          mc_client_display_error *);
bool mc_client_display_source_clear_depth(mc_client_display *, double depth,
                                          mc_client_display_error *);
bool mc_client_display_source_depth_test(mc_client_display *, bool enabled,
                                         mc_client_display_error *);
bool mc_client_display_source_depth_function(mc_client_display *, int32_t function,
                                             mc_client_display_error *);
bool mc_client_display_source_alpha_function(mc_client_display *, int32_t function, float reference,
                                             mc_client_display_error *);
bool mc_client_display_source_cull_face(mc_client_display *, int32_t mode,
                                        mc_client_display_error *);
bool mc_client_display_source_matrix_mode(mc_client_display *, int32_t mode,
                                          mc_client_display_error *);
bool mc_client_display_source_load_identity(mc_client_display *, mc_client_display_error *);
bool mc_client_display_source_bind_id(mc_client_display *, mc_client_texture,
                                      mc_client_display_error *);

bool mc_client_display_texture_lease_acquire(mc_client_texture_resource *,
                                             mc_client_display_texture_lease **,
                                             mc_client_display_error *);
bool mc_client_display_texture_lease_bind(mc_client_display_texture_lease *,
                                          mc_client_display_error *);
bool mc_client_display_texture_lease_begin(mc_client_display_texture_lease *,
                                           mc_client_display_error *);
bool mc_client_display_texture_lease_end(mc_client_display_texture_lease *,
                                         mc_client_display_error *);
bool mc_client_display_texture_lease_release(mc_client_display_texture_lease **,
                                             mc_client_display_error *);
#endif
