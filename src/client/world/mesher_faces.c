#include "mesher.h"

static const unsigned int triangle[6] = {0, 1, 2, 0, 2, 3};

bool mc_client_world_mesher_face(const mc_block_fields *b, int32_t ox, int32_t oy, int32_t oz,
                                 double x, double y, double z, int32_t side, int32_t texture,
                                 const uint32_t colors[4], mc_client_world_vertex out[6]) {
    if (b == NULL || colors == NULL || out == NULL || side < 0 || side > 5 || texture < 0 ||
        texture > 255)
        return false;
    double x0 = x + b->bs, x1 = x + b->bv, y0 = y + b->bt, y1 = y + b->bw;
    double z0 = z + b->bu, z1 = z + b->bx;
    int32_t tu = (texture & 15) * 16, tv = texture & 240;
    double lo = side <= 3 ? b->bs : b->bu;
    double hi = side <= 3 ? b->bv : b->bx;
    double u0 = ((double)tu + lo * 16.0) / 256.0;
    double u1 = ((double)tu + hi * 16.0 - .01) / 256.0;
    double v0, v1;
    if (lo < 0 || hi > 1) {
        u0 = ((float)tu + 0.0f) / 256.0f;
        u1 = ((float)tu + 15.99f) / 256.0f;
    }
    if (side <= 1) {
        v0 = ((double)tv + b->bu * 16.0) / 256.0;
        v1 = ((double)tv + b->bx * 16.0 - .01) / 256.0;
        if (b->bu < 0 || b->bx > 1) {
            v0 = ((float)tv + 0.0f) / 256.0f;
            v1 = ((float)tv + 15.99f) / 256.0f;
        }
    } else {
        v0 = ((double)(tv + 16) - b->bw * 16.0) / 256.0;
        v1 = ((double)(tv + 16) - b->bt * 16.0 - .01) / 256.0;
        if (b->bt < 0 || b->bw > 1) {
            v0 = ((float)tv + 0.0f) / 256.0f;
            v1 = ((float)tv + 15.99f) / 256.0f;
        }
    }
    double p[4][5];
#define P(i, xx, yy, zz, uu, vv)                                                                   \
    do {                                                                                           \
        p[i][0] = (xx);                                                                            \
        p[i][1] = (yy);                                                                            \
        p[i][2] = (zz);                                                                            \
        p[i][3] = (uu);                                                                            \
        p[i][4] = (vv);                                                                            \
    } while (0)
    switch (side) {
        case 0:
            P(0, x0, y0, z1, u0, v1);
            P(1, x0, y0, z0, u0, v0);
            P(2, x1, y0, z0, u1, v0);
            P(3, x1, y0, z1, u1, v1);
            break;
        case 1:
            P(0, x1, y1, z1, u1, v1);
            P(1, x1, y1, z0, u1, v0);
            P(2, x0, y1, z0, u0, v0);
            P(3, x0, y1, z1, u0, v1);
            break;
        case 2:
            P(0, x0, y1, z0, u1, v0);
            P(1, x1, y1, z0, u0, v0);
            P(2, x1, y0, z0, u0, v1);
            P(3, x0, y0, z0, u1, v1);
            break;
        case 3:
            P(0, x0, y1, z1, u0, v0);
            P(1, x0, y0, z1, u0, v1);
            P(2, x1, y0, z1, u1, v1);
            P(3, x1, y1, z1, u1, v0);
            break;
        case 4:
            P(0, x0, y1, z1, u1, v0);
            P(1, x0, y1, z0, u0, v0);
            P(2, x0, y0, z0, u0, v1);
            P(3, x0, y0, z1, u1, v1);
            break;
        default:
            P(0, x1, y0, z1, u0, v1);
            P(1, x1, y0, z0, u1, v1);
            P(2, x1, y1, z0, u1, v0);
            P(3, x1, y1, z1, u0, v0);
            break;
    }
#undef P
    for (size_t i = 0; i < 6; ++i) {
        unsigned int j = triangle[i];
        out[i] = (mc_client_world_vertex){(float)(p[j][0] + -(double)ox),
                                          (float)(p[j][1] + -(double)oy),
                                          (float)(p[j][2] + -(double)oz),
                                          (float)p[j][3],
                                          (float)p[j][4],
                                          colors[j]};
    }
    return true;
}
