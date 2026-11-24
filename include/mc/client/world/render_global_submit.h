#ifndef MC_CLIENT_WORLD_RENDER_GLOBAL_SUBMIT_H
#define MC_CLIENT_WORLD_RENDER_GLOBAL_SUBMIT_H

#include "mc/client/world/source_chunk_renderer.h"
#include "mc/core/math/classes.h"
#include "mc/entity/entity.h"

typedef struct mc_client_render_global_submit_call mc_client_render_global_submit_call;
typedef struct mc_client_graphics_site mc_client_graphics_site;

typedef struct mc_client_render_global_submit_state {
    mc_client_render_global *owner;
    mc_entity *camera, *batch_camera;
    mc_math_classes *math;
    bool active, returned, retiring, occlusion;
    int32_t pass, result, index, start, end;
    double partial, render_x, render_y, render_z;
    size_t dirty_polls, queued_chunks, subsets, selected_chunks;
    size_t configured_batches, appended_lists, drawn_batches;
    size_t query_polls, queries_issued;
    unsigned int source_pc, last_store_pc;
    int32_t interval;
    float distance, translate_x, translate_y, translate_z;
    bool matrix_pushed, query_active;
    const mc_client_graphics_site *frames;
    size_t frame_count;
    const mc_client_render_global_world_state *rebuild;
    const mc_client_render_global_error *failure, *cleanup;
} mc_client_render_global_submit_state;

/* Actual n.a(Lls;ID)I on the SAME render graph. Source int result and native
 * failure are distinct. The current App.i is captured separately when the
 * grouping child reaches it; it need not equal the argument camera. Rebuild,
 * recenter, sorting, counters, direct-buffer cursors and GL effects retain
 * their reached prefix. No failed call is replayed to resume execution.
 *
 * The actual App/World/Display and native dependencies stay live/exclusive on
 * the video thread through checked retirement. out starts NULL. An admitted
 * call is published even on source or native failure. Ordinary borrowed views
 * expire on source mutation or retirement, including partial retirement. */
mc_client_render_global_status mc_client_render_global_submit_begin(
    mc_client_render_global *, mc_entity *nullable_camera, int32_t pass, double partial,
    mc_math_classes *, mc_client_render_global_submit_call **out, mc_client_render_global_error *);
/* Actual n.a(ID)V over the SAME current T array and TG objects. The source
 * body does not use pass/partial, select/reset batches, read a camera/App/World
 * or activate Math. Only reached TG.a() bodies admit their actual dependencies
 * and retain their buffer/GL effects. Success is return29; result is not a
 * source int result for this void entry. Uses the same native N call gates,
 * retained receipt, inspection/lifetime contract and checked retire below. */
mc_client_render_global_status
mc_client_render_global_replay_begin(mc_client_render_global *, int32_t pass, double partial,
                                     mc_client_render_global_submit_call **out,
                                     mc_client_render_global_error *);
bool mc_client_render_global_submit_inspect(const mc_client_render_global_submit_call *,
                                            mc_client_render_global_submit_state *);
mc_client_render_global_status
mc_client_render_global_submit_retire(mc_client_render_global_submit_call **,
                                      mc_client_render_global_error *);

#endif
