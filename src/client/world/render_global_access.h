#ifndef MC_CLIENT_WORLD_RENDER_GLOBAL_ACCESS_H
#define MC_CLIENT_WORLD_RENDER_GLOBAL_ACCESS_H

#include "mc/client/world/render_global.h"
#include "mc/world/runtime/world_access_error.h"

typedef struct mc_render_global_access mc_render_global_access;
typedef struct mc_client_image_buffer mc_client_image_buffer;
typedef struct mc_client_image_download_call mc_client_image_download_call;

typedef struct mc_render_global_access_state {
    size_t registrations, active_callbacks, retained_failures;
    const mc_client_render_global_error *failure;
} mc_render_global_access_state;

typedef struct mc_render_global_callback_state {
    const mc_client_image_buffer *processors[2];
    const mc_client_image_download_call *images[2];
    const mc_client_render_global_error *failure;
    bool source_returned;
} mc_render_global_callback_state;

/* All operations require exclusive serialized use of the actual receiver and
 * World graph. Error outputs are required, fresh and disjoint from owners. */

/* One native interface identity belongs to the actual n receiver. This owns no
 * counted self occurrence. The caller keeps n and all its actual dependencies
 * alive until checked retirement succeeds. Creation runs no source callback. */
/* The seven presently implemented World caller slots enter their actual n
 * methods. Reached particle/appearance descendants retain explicit missing
 * bodies. The other three pm slots remain separate World caller work. */
mc_client_render_global_status mc_render_global_access_create(mc_client_render_global *,
                                                              mc_render_global_access **,
                                                              mc_client_render_global_error *);

/* Actual fd.b(pm)/fd.a(pm), called at n.a(fd)12/66 respectively. Each successful
 * add records the exact occurrence and a native World hold; remove still uses
 * source first-identity semantics, consuming only the receipt actually removed.
 * These do not substitute the supplied World for source reads of n.k. */
mc_client_render_global_status
mc_render_global_access_source_remove(mc_render_global_access *, mc_world_runtime *,
                                      mc_client_render_global_error *);
mc_client_render_global_status mc_render_global_access_source_add(mc_render_global_access *,
                                                                  mc_world_runtime *,
                                                                  mc_client_render_global_error *);

/* Native-only terminal coordination BEFORE World container retirement. Requires
 * no entered callback and no external n occurrences. Foreign memberships stay
 * intact and keep final bridge retirement BUSY until their World releases them.
 * Never replays n.a(NULL), DK/source
 * clears, callbacks or save. Failure preserves remaining owners for retry. */
mc_client_render_global_status mc_render_global_access_detach_all(mc_render_global_access *,
                                                                  mc_client_render_global_error *);
mc_client_render_global_status mc_render_global_access_retire(mc_render_global_access **,
                                                              mc_client_render_global_error *);
bool mc_render_global_access_inspect(const mc_render_global_access *,
                                     mc_render_global_access_state *);
/* Exact retained invocation only; never the owner's latest failure. The failure
 * frame lasts through checked bridge retirement; child views expire at the next
 * cleanup which consumes them. Capture the receipt using the World interface
 * to block child cleanup and bridge retirement. No owner is created here. */
bool mc_render_global_access_callback_inspect(const mc_render_global_access *,
                                              const mc_world_access_failure *,
                                              mc_render_global_callback_state *);

#endif
