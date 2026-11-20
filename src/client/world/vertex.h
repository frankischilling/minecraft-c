#ifndef MC_CLIENT_WORLD_VERTEX_H
#define MC_CLIENT_WORLD_VERTEX_H

#include <stdint.h>

typedef struct mc_client_world_vertex {
    float x, y, z, u, v;
    uint32_t argb;
} mc_client_world_vertex;

#endif
