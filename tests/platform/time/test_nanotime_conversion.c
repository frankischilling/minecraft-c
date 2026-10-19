#include "nanotime.h"
#include "nanotime_conversion_fixtures.h"

#include <emmintrin.h>
#include <stdio.h>
#include <string.h>

static unsigned int checks;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

typedef struct guarded_output {
    uint64_t before;
    int64_t value;
    uint64_t after;
} guarded_output;

static int test_vectors(const mc_nanotime_conversion_vector *vectors, size_t count, bool counter)
{
    for (size_t index = 0; index < count; ++index) {
        const mc_nanotime_conversion_vector *vector = &vectors[index];
        guarded_output out;
        unsigned char saved[sizeof(out)];
        memset(&out, 0xa5, sizeof(out));
        out.before = UINT64_C(0x0123456789abcdef);
        out.value = INT64_C(1234567890123456789);
        out.after = UINT64_C(0xfedcba9876543210);
        memcpy(saved, &out, sizeof(saved));
        CHECK(vector->status == MC_PLATFORM_NANOTIME_OK ||
              vector->status == MC_PLATFORM_NANOTIME_RANGE);

        mc_platform_nanotime_status status;
        if (counter) {
            status = mc_platform_nanotime_from_counter(vector->first, vector->second, &out.value);
        } else {
            status = mc_platform_nanotime_from_timespec(vector->first, vector->second, &out.value);
        }
        if (status != vector->status ||
            (status == MC_PLATFORM_NANOTIME_OK && out.value != vector->expected)) {
            fprintf(stderr, "%s vector %zu differs from captured Java scalar result\n",
                    counter ? "counter" : "timespec", index);
        }
        CHECK(status == vector->status);
        if (status == MC_PLATFORM_NANOTIME_OK) {
            CHECK(out.value == vector->expected);
            CHECK(out.before == UINT64_C(0x0123456789abcdef) &&
                  out.after == UINT64_C(0xfedcba9876543210));
        } else {
            CHECK(memcmp(saved, &out, sizeof(saved)) == 0);
        }
    }
    return 0;
}

int main(void)
{
    const size_t counter_count = sizeof(counter_vectors) / sizeof(counter_vectors[0]);
    const size_t timespec_count = sizeof(timespec_vectors) / sizeof(timespec_vectors[0]);
    const unsigned int initial_csr = _mm_getcsr();
    CHECK((initial_csr & 0xffc0u) == 0x1f80u);
    CHECK(counter_count > 0 && timespec_count > 0);
    CHECK(counter_count < 20000 && timespec_count < 20000 - counter_count);
    if (test_vectors(counter_vectors, counter_count, true) != 0 ||
        test_vectors(timespec_vectors, timespec_count, false) != 0) {
        return 1;
    }
    CHECK((_mm_getcsr() & 0xffc0u) == (initial_csr & 0xffc0u));
    printf("PASS: %u nanotime conversion checks (%zu counter, %zu timespec vectors)\n", checks,
           counter_count, timespec_count);
    return 0;
}
