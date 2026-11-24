#ifndef MC_CLIENT_TEXTURE_EFFECTS_BASE_H
#define MC_CLIENT_TEXTURE_EFFECTS_BASE_H
#include "mc/client/texture_effects/effect.h"
#include <stdlib.h>

/* First native subobject in every concrete owner. Source aw fields stay in
 * their actual existing field slots; this header is not a source mirror. */
struct mc_client_texture_effect {
    mc_texture_effect_kind kind;
    size_t holds;
    bool active;
};

typedef struct mc_aw_slots {
    const uint8_t **a;
    size_t *a_length;
    int32_t *b;
    bool *c;
    int32_t *d, *e, *f;
} mc_aw_slots;

typedef struct mc_aw_error {
    bool limit;
    int32_t pc;
} mc_aw_error;

/* One concrete shared aw.<init>(I)V allocation/store body, no callbacks or
 * activation. Same max/charged cells and actual subtype field addresses. */
static inline bool mc_texture_aw_construct(mc_aw_slots slots, size_t maximum, size_t *charged,
                                           int32_t texture, mc_aw_error *error) {
    error->pc = 8;
    if (*charged > maximum || 1024 > maximum - *charged) {
        error->limit = true;
        return false;
    }
    uint8_t *bytes = calloc(1, 1024);
    if (bytes == NULL)
        return false;
    *charged += 1024;
    *slots.a = bytes; /* aw10 */
    *slots.a_length = 1024;
    *slots.c = false;   /* aw15 */
    *slots.d = 0;       /* aw20 */
    *slots.e = 1;       /* aw25 */
    *slots.f = 0;       /* aw30 */
    *slots.b = texture; /* aw35 */
    return true;
}
#endif
