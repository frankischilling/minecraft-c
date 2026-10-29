#include "render_global_internal.h"

bool mc_render_batch_construct(mc_client_render_global *o, mc_client_render_batch *batch,
                               unsigned int enclosing_pc) {
    batch->state.phase = MC_CLIENT_RENDER_GLOBAL_CONSTRUCTING;
    /* Actual Object constructor1, then ge.d(65536)7 on the SAME graph arena. */
    mc_client_direct_int_buffer *buffer = NULL;
    if (!mc_client_ge_d(o->classes, &o->arena, 65536,
                        (mc_client_graphics_site){"tg", "<init>", "()V", 7}, &buffer,
                        &o->failure.graphics)) {
        batch->state.phase = MC_CLIENT_RENDER_GLOBAL_FAILED;
        return mc_render_global_graphics(o, enclosing_pc);
    }
    batch->state.g = buffer;
    batch->state.last_store_pc = 10;
    batch->state.h = false;
    batch->state.last_store_pc = 15;
    batch->state.i = false;
    batch->state.last_store_pc = 20;
    batch->state.phase = MC_CLIENT_RENDER_GLOBAL_READY; /* Actual return23. */
    return true;
}
