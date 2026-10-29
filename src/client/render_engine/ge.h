#ifndef MC_CLIENT_RENDER_ENGINE_GE_H
#define MC_CLIENT_RENDER_ENGINE_GE_H
#include "arena.h"

typedef struct mc_client_direct_float_buffer mc_client_direct_float_buffer;
typedef struct mc_client_command_range mc_client_command_range;

typedef struct mc_client_graphics_limits {
    size_t max_source_list_entries, max_command_ranges, max_list_commands;
} mc_client_graphics_limits;

/* Actual shared class active use and direct buffers. Caller retains the real
 * class/constructor owner exclusively throughout; no Display is required.
 * Error caller and child sites are distinct. Negative/wrapped Java lengths
 * reach allocateDirect; a failed view retains its underlying arena allocation.
 * A returned buffer/view is borrowed until that exact arena owner is retired. */
bool mc_client_ge_initialize(mc_client_render_engine_classes *, mc_client_graphics_site caller,
                             mc_client_graphics_error *);
bool mc_client_ge_c(mc_client_render_engine_classes *, mc_client_render_arena *, int32_t bytes,
                    mc_client_graphics_site caller, mc_client_direct_byte_buffer **out,
                    mc_client_graphics_error *);
bool mc_client_ge_d(mc_client_render_engine_classes *, mc_client_render_arena *, int32_t count,
                    mc_client_graphics_site caller, mc_client_direct_int_buffer **out,
                    mc_client_graphics_error *);
bool mc_client_ge_e(mc_client_render_engine_classes *, mc_client_render_arena *, int32_t count,
                    mc_client_graphics_site caller, mc_client_direct_float_buffer **out,
                    mc_client_graphics_error *);
bool mc_client_ge_int_view(mc_client_render_arena *, mc_client_direct_byte_buffer *,
                           mc_client_graphics_site site, mc_client_direct_int_buffer **out,
                           mc_client_graphics_error *);
bool mc_client_ge_float_view(mc_client_render_arena *, mc_client_direct_byte_buffer *,
                             mc_client_graphics_site site, mc_client_direct_float_buffer **out,
                             mc_client_graphics_error *);

bool mc_client_ge_int_clear(mc_client_direct_int_buffer *, mc_client_graphics_site,
                            mc_client_graphics_error *);
bool mc_client_ge_int_flip(mc_client_direct_int_buffer *, mc_client_graphics_site,
                           mc_client_graphics_error *);
bool mc_client_ge_int_put(mc_client_direct_int_buffer *, int32_t, mc_client_graphics_site,
                          mc_client_graphics_error *);
bool mc_client_ge_int_get(const mc_client_direct_int_buffer *, int32_t index, int32_t *out,
                          mc_client_graphics_site, mc_client_graphics_error *);
size_t mc_client_ge_int_remaining(const mc_client_direct_int_buffer *);
/* Actual unmarked Buffer position/limit operations for newly constructed
 * direct IntBuffers. No clear/flip/relative-get is implied. The native span
 * view borrows actual position..limit storage without changing either cursor. */
bool mc_client_ge_int_position(mc_client_direct_int_buffer *, int32_t, mc_client_graphics_site,
                               mc_client_graphics_error *);
bool mc_client_ge_int_limit(mc_client_direct_int_buffer *, int32_t, mc_client_graphics_site,
                            mc_client_graphics_error *);
bool mc_client_ge_int_writable_remaining(mc_client_direct_int_buffer *, void **storage,
                                         size_t *remaining, mc_client_graphics_site,
                                         mc_client_graphics_error *);

/* Actual List.add kernel used on the same a/b List identities. Source valueOf
 * integers are represented by their exact int32 payload, as in the existing
 * ge texture ledger. Native growth is separately bounded and retains prefix. */
bool mc_client_ge_append(struct mc_client_render_list *, int32_t value, size_t max_entries,
                         mc_client_graphics_site, mc_client_graphics_error *);

/* Positive source ge.a(I)I: genuine context-wide allocation1, List.add12/25.
 * This complete operation owns class/context active guards. Failed generation
 * or either append retains every completed native/source effect. Output stays
 * unchanged on failure. Nonpositive/GL-zero/deletion/redefinition corners are
 * explicit reached dependencies, not success with an invented list base. */
bool mc_client_ge_generate_lists(mc_client_render_engine_classes *, mc_client_display *,
                                 int32_t count, mc_client_graphics_site caller, int32_t *out,
                                 mc_client_graphics_error *);
bool mc_client_ge_call_lists(mc_client_render_engine_classes *, mc_client_display *,
                             const mc_client_direct_int_buffer *, mc_client_graphics_site caller,
                             mc_client_graphics_error *);
#endif
