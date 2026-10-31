#include "mesher_lighting.h"

void mc_client_world_ao_prepare(mc_client_world_ao *a, void *owner,
                                float (*brightness)(void *, int, int, int),
                                bool (*transmit)(void *, int, int, int)) {
    a->owner = owner;
    a->brightness = brightness;
    a->n = brightness(owner, 0, 0, 0);
    a->o = brightness(owner, -1, 0, 0);
    a->p = brightness(owner, 0, -1, 0);
    a->q = brightness(owner, 0, 0, -1);
    a->r = brightness(owner, 1, 0, 0);
    a->s = brightness(owner, 0, 1, 0);
    a->t = brightness(owner, 0, 0, 1);
    a->ac = transmit(owner, 1, 1, 0);
    a->ak = transmit(owner, 1, -1, 0);
    a->ag = transmit(owner, 1, 0, 1);
    a->ai = transmit(owner, 1, 0, -1);
    a->ad = transmit(owner, -1, 1, 0);
    a->al = transmit(owner, -1, -1, 0);
    a->af = transmit(owner, -1, 0, -1);
    a->ah = transmit(owner, -1, 0, 1);
    a->ae = transmit(owner, 0, 1, 1);
    a->ab = transmit(owner, 0, 1, -1);
    a->am = transmit(owner, 0, -1, 1);
    a->aj = transmit(owner, 0, -1, -1);
}

void mc_client_world_ao_face(const mc_client_world_ao *a, unsigned int side, float out[4]) {
    float u, v, w, x, y, z, A, B, C, D, E, F, G, H, I, J, K, L, M, N;
#define LIGHT(xx, yy, zz) a->brightness(a->owner, (xx), (yy), (zz))
    switch (side) {
        case 0:
            v = LIGHT(-1, -1, 0);
            x = LIGHT(0, -1, -1);
            y = LIGHT(0, -1, 1);
            A = LIGHT(1, -1, 0);
            u = a->aj || a->al ? LIGHT(-1, -1, -1) : v;
            w = a->am || a->al ? LIGHT(-1, -1, 1) : v;
            z = a->aj || a->ak ? LIGHT(1, -1, -1) : A;
            B = a->am || a->ak ? LIGHT(1, -1, 1) : A;
            out[0] = (w + v + y + a->p) / 4.0f;
            out[3] = (y + a->p + B + A) / 4.0f;
            out[2] = (a->p + x + A + z) / 4.0f;
            out[1] = (v + u + a->p + x) / 4.0f;
            break;
        case 1:
            D = LIGHT(-1, 1, 0);
            H = LIGHT(1, 1, 0);
            F = LIGHT(0, 1, -1);
            I = LIGHT(0, 1, 1);
            C = a->ab || a->ad ? LIGHT(-1, 1, -1) : D;
            G = a->ab || a->ac ? LIGHT(1, 1, -1) : H;
            E = a->ae || a->ad ? LIGHT(-1, 1, 1) : D;
            J = a->ae || a->ac ? LIGHT(1, 1, 1) : H;
            out[3] = (E + D + I + a->s) / 4.0f;
            out[0] = (I + a->s + J + H) / 4.0f;
            out[1] = (a->s + F + H + G) / 4.0f;
            out[2] = (D + C + a->s + F) / 4.0f;
            break;
        case 2:
            K = LIGHT(-1, 0, -1);
            x = LIGHT(0, -1, -1);
            F = LIGHT(0, 1, -1);
            L = LIGHT(1, 0, -1);
            u = a->af || a->aj ? LIGHT(-1, -1, -1) : K;
            C = a->af || a->ab ? LIGHT(-1, 1, -1) : K;
            z = a->ai || a->aj ? LIGHT(1, -1, -1) : L;
            G = a->ai || a->ab ? LIGHT(1, 1, -1) : L;
            out[0] = (K + C + a->q + F) / 4.0f;
            out[1] = (a->q + F + L + G) / 4.0f;
            out[2] = (x + a->q + z + L) / 4.0f;
            out[3] = (u + K + x + a->q) / 4.0f;
            break;
        case 3:
            M = LIGHT(-1, 0, 1);
            N = LIGHT(1, 0, 1);
            y = LIGHT(0, -1, 1);
            I = LIGHT(0, 1, 1);
            w = a->ah || a->am ? LIGHT(-1, -1, 1) : M;
            E = a->ah || a->ae ? LIGHT(-1, 1, 1) : M;
            B = a->ag || a->am ? LIGHT(1, -1, 1) : N;
            J = a->ag || a->ae ? LIGHT(1, 1, 1) : N;
            out[0] = (M + E + a->t + I) / 4.0f;
            out[3] = (a->t + I + N + J) / 4.0f;
            out[2] = (y + a->t + B + N) / 4.0f;
            out[1] = (w + M + y + a->t) / 4.0f;
            break;
        case 4:
            v = LIGHT(-1, -1, 0);
            K = LIGHT(-1, 0, -1);
            M = LIGHT(-1, 0, 1);
            D = LIGHT(-1, 1, 0);
            u = a->af || a->al ? LIGHT(-1, -1, -1) : K;
            w = a->ah || a->al ? LIGHT(-1, -1, 1) : M;
            C = a->af || a->ad ? LIGHT(-1, 1, -1) : K;
            E = a->ah || a->ad ? LIGHT(-1, 1, 1) : M;
            out[3] = (v + w + a->o + M) / 4.0f;
            out[0] = (a->o + M + D + E) / 4.0f;
            out[1] = (K + a->o + C + D) / 4.0f;
            out[2] = (u + v + K + a->o) / 4.0f;
            break;
        default:
            A = LIGHT(1, -1, 0);
            L = LIGHT(1, 0, -1);
            N = LIGHT(1, 0, 1);
            H = LIGHT(1, 1, 0);
            z = a->ak || a->ai ? LIGHT(1, -1, -1) : L;
            B = a->ak || a->ag ? LIGHT(1, -1, 1) : N;
            G = a->ac || a->ai ? LIGHT(1, 1, -1) : L;
            J = a->ac || a->ag ? LIGHT(1, 1, 1) : N;
            out[0] = (A + B + a->r + N) / 4.0f;
            out[3] = (a->r + N + H + J) / 4.0f;
            out[2] = (L + a->r + G + H) / 4.0f;
            out[1] = (z + A + L + a->r) / 4.0f;
            break;
    }
#undef LIGHT
}
