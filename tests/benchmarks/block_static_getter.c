/* Fixed-work Windows READY Block getter benchmark. Public fixture inclusion is
 * a source projection of the measured private caller, not a new timing result. */
#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

#define main mc_block_static_existing_fixture_main
#include "../item/registry/test_runtime.c"
#undef main

/* Host timer only. The Windows path retains QPC; the unmeasured publication
 * path on POSIX uses CLOCK_MONOTONIC with a nanosecond frequency. */
static bool block_bench_frequency(int64_t *frequency) {
#ifdef _WIN32
    LARGE_INTEGER hz;
    if (!QueryPerformanceFrequency(&hz) || hz.QuadPart <= 0)
        return false;
    *frequency = (int64_t)hz.QuadPart;
#else
    *frequency = INT64_C(1000000000);
#endif
    return true;
}

static bool block_bench_counter(int64_t *ticks) {
#ifdef _WIN32
    LARGE_INTEGER counter;
    if (!QueryPerformanceCounter(&counter) || counter.QuadPart < 0)
        return false;
    *ticks = (int64_t)counter.QuadPart;
#else
    struct timespec counter;
    if (clock_gettime(CLOCK_MONOTONIC, &counter) != 0 || counter.tv_sec < 0 ||
        (uint64_t)counter.tv_sec > (uint64_t)INT64_MAX / UINT64_C(1000000000))
        return false;
    const uint64_t value =
        (uint64_t)counter.tv_sec * UINT64_C(1000000000) + (uint64_t)counter.tv_nsec;
    if (value > INT64_MAX)
        return false;
    *ticks = (int64_t)value;
#endif
    return true;
}

int main(int argc, char **argv) {
    int result = 1;
    mc_java_random_profile *profile = NULL;
    mc_materials *materials = NULL;
    fixture f = {0};
    size_t rounds = 10000;
    if (argc == 2) {
        char *end = NULL;
        errno = 0;
        const unsigned long long parsed = strtoull(argv[1], &end, 10);
        if (errno != 0 || end == argv[1] || *end != 0 || argv[1][0] == '-' || parsed == 0 ||
            parsed > SIZE_MAX || parsed > UINT64_MAX / 4656u) {
            fputs("Use a positive round count with a representable work/checksum total\n", stderr);
            return 1;
        }
        rounds = (size_t)parsed;
    } else if (argc != 1) {
        fputs("usage: block_static_getter_bench [rounds]\n", stderr);
        return 1;
    }
    CHECK(mc_java_random_profile_create(SIZE_MAX, &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_materials_create(&materials));
    const mc_registry_options options = {.side = MC_REGISTRY_CLIENT,
                                         .random_profile = profile,
                                         .materials = materials,
                                         .max_allocation_bytes = SIZE_MAX};
    /* Cold/failed/RAW/rebound/NULL/invalid-name controls are OUTSIDE timing,
     * and exercise the same unchanged public functions in both variants. */
    CHECK(block_static_semantics(options, false) == 0);
    CHECK(block_static_semantics(options, true) == 0);
    CHECK(allocate_fixture(&f, options) == MC_REGISTRY_OK);
    mc_registry_error error = poison_error();
    CHECK(mc_registry_classes_initialize_blocks(f.owner, &error) == MC_REGISTRY_OK &&
          f.owner->blocks_phase == MC_REGISTRY_READY && !f.bad && f.opens == 3 && f.closes == 1);
    for (size_t i = 0; i < COUNT(block_static_names); ++i) {
        const mc_block *block = NULL;
        CHECK(mc_registry_block_static_get(f.owner, block_static_names[i], &block, &error) ==
                  MC_REGISTRY_OK &&
              block == f.owner->blocks.named[i] && block != NULL &&
              block->fields.bn == (int32_t)i + 1 && fresh(error, MC_REGISTRY_OK));
    }
    const size_t bytes = f.owner->allocation_bytes;
    const uint64_t random_before = uniquifier(profile);
    int64_t frequency, first, last;
    CHECK(block_bench_frequency(&frequency) && frequency > 0);
    uint64_t checksum = 0;
    bool valid = true;
    CHECK(block_bench_counter(&first));
    for (size_t round = 0; round < rounds; ++round) {
        for (size_t i = 0; i < COUNT(block_static_names); ++i) {
            const mc_block *block = NULL;
            /* The complete actual public READY getter, including its normal
             * error output, active use and fresh named-cell load, is timed.
             * A returned ID contributes to an observable integer checksum. */
            const mc_registry_status status =
                mc_registry_block_static_get(f.owner, block_static_names[i], &block, &error);
            if (status != MC_REGISTRY_OK || block == NULL) {
                valid = false;
                break;
            }
            checksum += (uint64_t)(uint32_t)block->fields.bn;
        }
        if (!valid)
            break;
    }
    CHECK(block_bench_counter(&last));
    CHECK(valid && last >= first && checksum == (uint64_t)rounds * 4656u &&
          fresh(error, MC_REGISTRY_OK));
    CHECK(f.owner->blocks_phase == MC_REGISTRY_READY && f.owner->entered == 0 &&
          f.owner->initializer == NULL && f.owner->allocation_bytes == bytes &&
          uniquifier(profile) == random_before && f.opens == 3 && f.closes == 1 && !f.bad);
    printf("BLOCK_STATIC_GETTER_BENCH side=client phase=READY rounds=%zu fields=96 calls=%" PRIu64
           " ticks=%" PRId64 " frequency=%" PRId64 " checksum=%" PRIu64
           " controls=%u live-cells=1 source-order=1\n",
           rounds, (uint64_t)rounds * 96u, last - first, frequency, checksum, checks);
    result = 0;
cleanup:
    if (f.owner != NULL && !retire(&f))
        result = 1;
    if (!retirement_failed) {
        mc_materials_destroy(materials);
        mc_java_random_profile_free(profile);
    }
    return result;
}
