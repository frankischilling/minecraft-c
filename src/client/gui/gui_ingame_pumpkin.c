#include "gui_ingame_pumpkin.h"

#include <string.h>

bool mc_client_gui_pumpkin_geometry_entered(mc_client_tessellator *nw, int32_t width,
                                            int32_t height, mc_client_gui_pumpkin_geometry *out) {
    if (out == NULL)
        return false;
    memset(out, 0, sizeof(*out));
    out->source_pc = 55;
    out->begin_attempts = 1;
    if (nw == NULL) {
        out->source_null_invocation = true;
        return false;
    }
    if (!mc_client_nw_begin_quads(nw, &out->graphics))
        return false;
    out->begin_returns = 1;
    static const unsigned int calls[] = {67, 80, 92, 103};
    for (size_t i = 0; i < 4; ++i) {
        out->source_pc = calls[i];
        /* Original i2d operands, full texture and fixed z=-90. No color,
         * normal, origin, matrix or inherited UB depth field is written. */
        if (!mc_client_nw_vertex_uv(
                nw, i == 1 || i == 2 ? (double)width : 0.0, i < 2 ? (double)height : 0.0, -90.0,
                i == 1 || i == 2 ? 1.0 : 0.0, i < 2 ? 1.0 : 0.0, &out->graphics))
            return false;
        ++out->vertices;
    }
    mc_client_nw_fields actual;
    if (!mc_client_nw_fields_get(nw, &actual) || actual.g == NULL || actual.o < 0 ||
        actual.o > (int32_t)(sizeof(out->words) / sizeof(*out->words))) {
        (void)mc_client_graphics_fail(&out->graphics, MC_CLIENT_GRAPHICS_INVALID_ARGUMENT,
                                      "Pumpkin actual pre-draw NW backing extent",
                                      (mc_client_graphics_site){"uq", "a", "(II)V", 103}, true);
        return false;
    }
    out->word_count = (size_t)actual.o;
    memcpy(out->words, actual.g, out->word_count * sizeof(*out->words));
    out->prefix_returned = true;
    return true;
}
