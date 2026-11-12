#include "mesher_lighting.h"
#include <inttypes.h>
#include <stdio.h>
static int scenario;

static float light(void *o, int x, int y, int z) {
    (void)o;
    return (float)((x + 1) * 9 + (y + 1) * 3 + (z + 1) + 1) / 32.0f;
}

static bool flag(void *o, int x, int y, int z) {
    (void)o;
    return scenario == 1 || (scenario == 2 && ((x + 2 * y + 4 * z) & 2) != 0);
}

int main(void) {
    static const float shade[6] = {.5f, 1, .8f, .8f, .6f, .6f};
    static const unsigned index[6] = {0, 1, 2, 0, 2, 3};
    for (scenario = 0; scenario < 3; ++scenario) {
        mc_client_world_ao a;
        mc_client_world_ao_prepare(&a, NULL, light, flag);
        for (unsigned side = 0; side < 6; ++side) {
            float corner[4];
            mc_client_world_ao_face(&a, side, corner);
            for (unsigned i = 0; i < 6; ++i) {
                float k = corner[index[i]];
                unsigned r = (unsigned)((.3f * shade[side]) * k * 255.0f),
                         g = (unsigned)((.6f * shade[side]) * k * 255.0f),
                         b = (unsigned)((.9f * shade[side]) * k * 255.0f);
                printf("%d %u %u %08" PRIx32 "\n", scenario, side, i,
                       UINT32_C(0xff000000) | (r << 16) | (g << 8) | b);
            }
        }
    }
    return 0;
}
