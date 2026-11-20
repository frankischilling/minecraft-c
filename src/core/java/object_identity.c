#include "object_identity_internal.h"

#include <stddef.h>

void mc_java_identity_profile_initialize(mc_java_identity_profile *profile) {
    const uint32_t first = (uint32_t)((UINT64_C(1234567) * UINT64_C(16807)) % UINT64_C(2147483647));
    *profile = (mc_java_identity_profile){
        .startup_random = first,
        .main_thread = {.x = first,
                        .y = UINT32_C(842502087),
                        .z = UINT32_C(0x8767),
                        .w = UINT32_C(273326509)},
    };
}

uint32_t mc_java_object_identity_hash(mc_java_identity_thread *thread,
                                      mc_java_identity_cell *cell) {
    if (cell == NULL)
        return 0;
    if (cell->hash != 0)
        return cell->hash;
    uint32_t t = thread->x ^ (thread->x << 11u);
    uint32_t v = (thread->w ^ (thread->w >> 19u)) ^ (t ^ (t >> 8u));
    thread->x = thread->y;
    thread->y = thread->z;
    thread->z = thread->w;
    thread->w = v;
    uint32_t hash = v & UINT32_C(0x7fffffff);
    cell->hash = hash == 0 ? UINT32_C(0xbad) : hash;
    return cell->hash;
}
