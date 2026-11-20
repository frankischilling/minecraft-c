#ifndef MC_CLIENT_DISPLAY_MATRIX_SOURCE_H
#define MC_CLIENT_DISPLAY_MATRIX_SOURCE_H
#include "display.h"

/* Exact existing rounded arithmetic; caller owns source state/order. */
static inline float mc_client_source_product(float left, float right) {
    volatile float rounded = left * right;
    return rounded;
}

static inline float mc_client_source_sum(float left, float right) {
    volatile float rounded = left + right;
    return rounded;
}

static inline void mc_client_source_translate(mc_client_modelview *matrix, float x, float y,
                                              float z) {
    for (size_t row = 0; row < 4; ++row) {
        const float px = mc_client_source_product(matrix->column_major[row], x);
        const float py = mc_client_source_product(matrix->column_major[4 + row], y);
        const float pz = mc_client_source_product(matrix->column_major[8 + row], z);
        const float xy = mc_client_source_sum(px, py);
        const float xyz = mc_client_source_sum(xy, pz);
        matrix->column_major[12 + row] = mc_client_source_sum(xyz, matrix->column_major[12 + row]);
    }
}

#endif
