#include "../../src/entity/entity_internal.h"

#include <emmintrin.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#endif

_Static_assert(sizeof(double) == sizeof(uint64_t), "binary64 storage required");
_Static_assert(sizeof(float) == sizeof(uint32_t), "binary32 storage required");

static int fail(const char *message) {
    fprintf(stderr, "item_formula_probe: %s\n", message);
    return 1;
}

static bool parse_draw(const char *text, double *out) {
    if (strlen(text) != 16u) {
        return false;
    }
    uint64_t bits = 0;
    for (size_t index = 0; index < 16u; ++index) {
        const unsigned char character = (unsigned char)text[index];
        unsigned int digit;
        if (character >= '0' && character <= '9') {
            digit = (unsigned int)(character - '0');
        } else if (character >= 'a' && character <= 'f') {
            digit = (unsigned int)(character - 'a') + 10u;
        } else {
            return false;
        }
        bits = (bits << 4u) | (uint64_t)digit;
    }
    if ((bits & UINT64_C(0x7ff0000000000000)) == UINT64_C(0x7ff0000000000000)) {
        return false;
    }
    double value;
    memcpy(&value, &bits, sizeof(value));
    if (!(value >= 0.0 && value < 1.0)) {
        return false;
    }
    *out = value;
    return true;
}

static uint32_t float_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static uint64_t double_bits(double value) {
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

int main(int argc, char **argv) {
    bool full;
    if (argc == 3 && strcmp(argv[1], "WORLD") == 0) {
        full = false;
    } else if (argc == 6 && strcmp(argv[1], "FULL") == 0) {
        full = true;
    } else {
        return fail("expected WORLD draw or FULL phase yaw X Z draws");
    }
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u) {
        return fail("strict SSE environment required");
    }
    double draws[4] = {0.0, 0.0, 0.0, 0.0};
    for (size_t index = 0; index < (full ? 4u : 1u); ++index) {
        if (!parse_draw(argv[2u + index], &draws[index])) {
            return fail(
                "draw must be exactly 16 lowercase hex digits encoding a finite value in [0,1)");
        }
    }
    const float phase = mc_entity_item_phase_from_draw(draws[0]);
    const float yaw = full ? mc_entity_item_yaw_from_draw(draws[1]) : 0.0f;
    const double motion_x = full ? mc_entity_item_velocity_from_draw(draws[2]) : 0.0;
    const double motion_y = full ? 0.20000000298023224 : 0.0;
    const double motion_z = full ? mc_entity_item_velocity_from_draw(draws[3]) : 0.0;
#if defined(_WIN32)
    if (_setmode(_fileno(stdout), _O_BINARY) == -1) {
        return fail("cannot select binary stdout");
    }
#endif
    if (printf("ITEM_FORMULA_V1\t%08" PRIx32 "\t%08" PRIx32 "\t%016" PRIx64 "\t%016" PRIx64
               "\t%016" PRIx64 "\n",
               (uint32_t)float_bits(phase), (uint32_t)float_bits(yaw),
               (uint64_t)double_bits(motion_x), (uint64_t)double_bits(motion_y),
               (uint64_t)double_bits(motion_z)) < 0 ||
        fflush(stdout) != 0) {
        return fail("stdout write failed");
    }
    return 0;
}
