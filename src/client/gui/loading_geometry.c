#include "loading_geometry.h"
#include "mc/core/java/java.h"

void mc_client_loading_background_vertices(int32_t width, int32_t height, mc_client_vertex v[4]) {
    /* mk.a(I)204/207 and223/226: i2f,fdiv,f2d, not binary64 division. */
    const float u = (float)width / 32.0f;
    const float t = (float)height / 32.0f;
    const uint32_t color = UINT32_C(0xff404040);
    v[0] = (mc_client_vertex){0, height, 0, t, color};
    v[1] = (mc_client_vertex){width, height, u, t, color};
    v[2] = (mc_client_vertex){width, 0, u, 0, color};
    v[3] = (mc_client_vertex){0, 0, 0, 0, color};
}

static void rectangle(int32_t x, int32_t y, int32_t width, uint32_t color, mc_client_vertex v[4]) {
    const int32_t right = mc_java_i32_add(x, width);
    const int32_t bottom = mc_java_i32_add(y, 2);
    v[0] = (mc_client_vertex){x, y, 0, 0, color};
    v[1] = (mc_client_vertex){x, bottom, 0, 0, color};
    v[2] = (mc_client_vertex){right, bottom, 0, 0, color};
    v[3] = (mc_client_vertex){right, y, 0, 0, color};
}

void mc_client_loading_bar_vertices(int32_t width, int32_t height, int32_t progress,
                                    mc_client_vertex gray[4], mc_client_vertex green[4]) {
    const int32_t x = mc_java_i32_sub(width / 2, 50);
    const int32_t y = mc_java_i32_add(height / 2, 16);
    rectangle(x, y, 100, UINT32_C(0xff808080), gray);
    /* Exact original literal8454016 at384. No progress clamp. */
    rectangle(x, y, progress, UINT32_C(0xff80ff80), green);
}

int32_t mc_client_loading_center_x(int32_t width, int32_t measured) {
    return mc_java_i32_sub(width, measured) / 2;
}
