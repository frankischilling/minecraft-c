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
bool mc_client_display_lists_begin(mc_client_display *, int32_t id, mc_client_display_error *);
bool mc_client_display_lists_end(mc_client_display *, mc_client_display_error *);
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
