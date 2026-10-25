#ifndef MC_CLIENT_WORLD_MESHER_LIGHTING_H
#define MC_CLIENT_WORLD_MESHER_LIGHTING_H

#include <stdbool.h>

/* cv's exact default O=1 AO sample order. Offsets are relative to the Block.
 * This CPU seam also accepts authored light/Material grids without a World. */
typedef struct mc_client_world_ao {
    void *owner;
    float (*brightness)(void *owner, int x, int y, int z);
    float n, o, p, q, r, s, t;
    bool ab, ac, ad, ae, af, ag, ah, ai, aj, ak, al, am;
} mc_client_world_ao;

void mc_client_world_ao_prepare(mc_client_world_ao *ao, void *owner,
                                float (*brightness)(void *, int, int, int),
                                bool (*transmit)(void *, int, int, int));
void mc_client_world_ao_face(const mc_client_world_ao *ao, unsigned int side, float out[4]);

#endif
