#include "mesher.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static uint32_t bits(float f) {
    uint32_t u;
    memcpy(&u, &f, 4);
    return u;
}

int main(void) {
    double cases[3][6] = {{0, 0, 0, 1, 1, 1},
                          {.125, .25, .375, .75, .875, 1},
                          {-.125, -.25, -.375, 1.125, 1.25, 1.375}};
    uint32_t colors[4] = {0xff1f7fe9, 0xff1f7fe9, 0xff1f7fe9, 0xff1f7fe9};
    for (unsigned t = 0; t < 3; ++t) {
        mc_block_fields b = {.bs = cases[t][0], .bt = cases[t][1], .bu = cases[t][2]};
        b.bv = cases[t][3];
        b.bw = cases[t][4];
        b.bx = cases[t][5];
        for (int side = 0; side < 6; ++side) {
            mc_client_world_vertex v[6];
            if (!mc_client_world_mesher_face(&b, -16, 48, 32, -3, 50, 39, side, 83, colors, v))
                return 2;
            for (unsigned i = 0; i < 6; ++i)
                printf("%u %d %u %08" PRIx32 " %08" PRIx32 " %08" PRIx32 " %08" PRIx32 " %08" PRIx32
                       " %08" PRIx32 "\n",
                       t, side, i, bits(v[i].x), bits(v[i].y), bits(v[i].z), bits(v[i].u),
                       bits(v[i].v), v[i].argb);
        }
    }
    return 0;
}
