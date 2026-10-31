#include "internal.h"
#include "nw.h"
#include <stdlib.h>

mc_client_render_engine_status mc_client_render_engine_classes_allocate_context(
    const mc_client_render_engine_options *ge, const mc_client_render_engine_options *nw,
    const mc_client_graphics_limits *limits, mc_client_render_engine_classes **out) {
    if (out == NULL)
        return MC_CLIENT_RENDER_ENGINE_INVALID_ARGUMENT;
    const mc_client_graphics_limits selected =
        limits == NULL ? (mc_client_graphics_limits){4096, 128, 65536} : *limits;
    if (selected.max_command_ranges > 128 || selected.max_list_commands > 65536)
        return MC_CLIENT_RENDER_ENGINE_INVALID_ARGUMENT;
    mc_client_render_engine_classes *c = calloc(1, sizeof(*c));
    if (c == NULL)
        return MC_CLIENT_RENDER_ENGINE_ALLOCATION_FAILED;
    atomic_init(&c->holds, 1);
    c->arena.options = ge == NULL ? mc_client_render_engine_default_options() : *ge;
    c->nw.options =
        nw == NULL ? (mc_client_render_engine_options){20u * 1024u * 1024u, SIZE_MAX} : *nw;
    c->limits = selected;
    *out = c;
    return MC_CLIENT_RENDER_ENGINE_OK;
}
