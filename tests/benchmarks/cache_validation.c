/* Fixed-work cache validation CPU/contract benchmark. Publication formatting
 * preserves the measured caller tokens; prior binary timings remain distinct. */
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#ifndef _WIN32
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#endif
#include "mc/core/java/java.h"
#include "mc/world/cache/cache.h"
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

enum { CAPACITY = 4097, SAMPLES = 3, WARMUP_CALLS = 4 };

typedef struct fixture {
    mc_chunk_cache cache;
    mc_chunk_cache_entry entries[CAPACITY];
    mc_chunk_state *insertions[CAPACITY];
    mc_chunk_data data;
    mc_chunk_state normal, blank, invalid;
} fixture;

static fixture input, saved;
static unsigned int controls;
static volatile uint64_t sink;

static uint32_t hash_key(uint32_t hash) {
    hash ^= hash >> 16u;
    hash *= UINT32_C(0x7feb352d);
    hash ^= hash >> 15u;
    hash *= UINT32_C(0x846ca68b);
    return hash ^ (hash >> 16u);
}

static int fill(size_t count, unsigned int pattern) {
    memset(&input, 0, sizeof(input));
    input.normal.data = &input.data;
    input.blank.blank = true;
    uint32_t collision = 0;
    for (size_t i = 0; i < CAPACITY; ++i) {
        uint32_t bits;
        if (pattern == 3u) {
            while ((hash_key(collision) & 1023u) != 0u) {
                if (collision == UINT32_MAX)
                    return 1;
                ++collision;
            }
            bits = collision;
            if (collision == UINT32_MAX)
                return 1;
            ++collision;
        } else {
            bits = pattern == 0u   ? (uint32_t)i * UINT32_C(2654435761)
                   : pattern == 1u ? (uint32_t)i << 16u
                                   : UINT32_MAX - (uint32_t)i;
        }
        input.entries[i].key = mc_java_i32_narrow((int64_t)bits);
        input.entries[i].chunk = (i & 1u) == 0u ? &input.normal : &input.blank;
        input.insertions[i] = (i & 1u) == 0u ? &input.blank : &input.normal;
    }
    input.cache =
        (mc_chunk_cache){input.entries, CAPACITY, count, input.insertions, CAPACITY, count};
    return 0;
}

static int check(const char *name, const mc_chunk_cache *cache, bool expected) {
    memcpy(&saved, &input, sizeof(input));
    const bool actual = mc_chunk_cache_validate(cache);
    ++controls;
    if (actual != expected || memcmp(&saved, &input, sizeof(input)) != 0) {
        fprintf(stderr, "CACHE_CONTROL_FAILED name=%s count=%zu expected=%d actual=%d mutated=%d\n",
                name, cache == NULL ? 0u : cache->count, expected, actual,
                memcmp(&saved, &input, sizeof(input)) != 0);
        return 1;
    }
    return 0;
}

#define CHECK_CASE(name, cache, expected)                                                          \
    do {                                                                                           \
        if (check(name, cache, expected) != 0)                                                     \
            return 1;                                                                              \
    } while (0)

static int contract_controls(void) {
    const size_t counts[] = {0u, 1u, 8u, 9u, 511u, 512u, 513u, 1023u, 1024u, 1025u, 4096u};
    CHECK_CASE("null-cache", NULL, false);
    for (unsigned int pattern = 0; pattern < 4u; ++pattern) {
        for (size_t c = 0; c < sizeof(counts) / sizeof(counts[0]); ++c) {
            const size_t count = counts[c];
            if (fill(count, pattern) != 0)
                return 1;
            CHECK_CASE("valid-live", &input.cache, true);
            if (count > 1u) {
                const size_t pairs[][2] = {{0u, count - 1u}, {511u, 512u},  {512u, 1023u},
                                           {1023u, 1024u},   {512u, 4095u}, {1024u, 4095u}};
                for (size_t p = 0; p < sizeof(pairs) / sizeof(pairs[0]); ++p) {
                    if (pairs[p][1] >= count)
                        continue;
                    const int32_t prior = input.entries[pairs[p][1]].key;
                    input.entries[pairs[p][1]].key = input.entries[pairs[p][0]].key;
                    CHECK_CASE("duplicate-boundary-or-later-batch", &input.cache, false);
                    input.entries[pairs[p][1]].key = prior;
                    CHECK_CASE("mutated-key-restored", &input.cache, true);
                }
            }
            if (count != 0u) {
                const size_t positions[] = {0u, count - 1u, count > 512u ? 512u : 0u};
                for (size_t p = 0; p < sizeof(positions) / sizeof(positions[0]); ++p) {
                    const size_t at = positions[p];
                    mc_chunk_state *prior = input.entries[at].chunk;
                    input.entries[at].chunk = NULL;
                    CHECK_CASE("null-map-value", &input.cache, false);
                    input.entries[at].chunk = &input.invalid;
                    CHECK_CASE("normal-null-data", &input.cache, false);
                    input.entries[at].chunk = &input.blank;
                    CHECK_CASE("blank-null-data-valid", &input.cache, true);
                    input.entries[at].chunk = prior;
                }
                mc_chunk_state *prior = input.insertions[count - 1u];
                input.insertions[count - 1u] = NULL;
                CHECK_CASE("null-insertion", &input.cache, false);
                input.insertions[count - 1u] = &input.invalid;
                CHECK_CASE("invalid-insertion", &input.cache, false);
                input.insertions[count - 1u] = prior;
                CHECK_CASE("repeated-insertions-restored", &input.cache, true);
                input.normal.data = NULL;
                CHECK_CASE("live-normal-data-mutated", &input.cache, false);
                input.normal.data = &input.data;
                CHECK_CASE("live-normal-data-restored", &input.cache, true);
            }
        }
    }
    if (fill(513u, 2u) != 0)
        return 1;
    input.entries[0].key = INT32_MIN;
    input.entries[512].key = INT32_MAX;
    CHECK_CASE("all-int-edge-keys", &input.cache, true);
    input.entries[512].key = INT32_MIN;
    CHECK_CASE("edge-key-cross-batch-duplicate", &input.cache, false);
    if (fill(513u, 0u) != 0)
        return 1;
    input.cache.capacity = 512u;
    CHECK_CASE("count-exceeds-capacity", &input.cache, false);
    input.cache.capacity = CAPACITY;
    input.cache.entries = NULL;
    CHECK_CASE("null-entry-storage", &input.cache, false);
    input.cache.entries = input.entries;
    input.cache.insertions = NULL;
    CHECK_CASE("null-insertion-storage", &input.cache, false);
    input.cache.insertions = input.insertions;
    input.cache.insertion_count = CAPACITY + 1u;
    CHECK_CASE("insertion-count-exceeds-capacity", &input.cache, false);
    input.cache = (mc_chunk_cache){input.entries, SIZE_MAX, 0, input.insertions, SIZE_MAX, 0};
    CHECK_CASE("huge-unused-capacity-baseline-contract", &input.cache, true);
    printf("CACHE_CONTROLS checks=%u immutable=1 sizes-through=4096 patterns=4 collision-keys=1\n",
           controls);
    return 0;
}

typedef struct stamp {
    uint64_t cpu_ns, wall, frequency;
} stamp;

static bool sample_clock(stamp *out) {
#ifdef _WIN32
    FILETIME created, ended, kernel, user;
    LARGE_INTEGER wall, frequency;
    if (!GetProcessTimes(GetCurrentProcess(), &created, &ended, &kernel, &user) ||
        !QueryPerformanceCounter(&wall) || !QueryPerformanceFrequency(&frequency))
        return false;
    const uint64_t kernel_ticks = ((uint64_t)kernel.dwHighDateTime << 32u) | kernel.dwLowDateTime;
    const uint64_t user_ticks = ((uint64_t)user.dwHighDateTime << 32u) | user.dwLowDateTime;
    *out = (stamp){(kernel_ticks + user_ticks) * 100u, (uint64_t)wall.QuadPart,
                   (uint64_t)frequency.QuadPart};
#else
    struct timespec cpu, wall;
    if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &cpu) != 0 ||
        clock_gettime(CLOCK_MONOTONIC, &wall) != 0)
        return false;
    *out = (stamp){(uint64_t)cpu.tv_sec * UINT64_C(1000000000) + (uint64_t)cpu.tv_nsec,
                   (uint64_t)wall.tv_sec * UINT64_C(1000000000) + (uint64_t)wall.tv_nsec,
                   UINT64_C(1000000000)};
#endif
    return true;
}

static int measure(void) {
    const size_t counts[] = {513u, 1024u, 4096u};
    for (unsigned int pattern = 0; pattern <= 3u; pattern += 3u) {
        for (size_t c = 0; c < sizeof(counts) / sizeof(counts[0]); ++c) {
            const size_t count = counts[c];
            if (fill(count, pattern) != 0)
                return 1;
            const uint64_t pairs = (uint64_t)count * (count - 1u) / 2u;
            const unsigned int calls = (unsigned int)(UINT64_C(50000000) / pairs) + 1u;
            uint64_t keys = UINT64_C(1469598103934665603);
            for (size_t i = 0; i < count; ++i)
                keys = (keys ^ (uint32_t)input.entries[i].key) * UINT64_C(1099511628211);
            memcpy(&saved, &input, sizeof(input));
            for (unsigned int warmup = 0; warmup < WARMUP_CALLS; ++warmup)
                if (!mc_chunk_cache_validate(&input.cache))
                    return 1;
            if (memcmp(&input, &saved, sizeof(input)) != 0)
                return 1;
            printf("CACHE_WARMUP count=%zu pattern=%s calls=%u immutable=1\n", count,
                   pattern == 0u ? "mixed" : "one-bucket", (unsigned int)WARMUP_CALLS);
            for (unsigned int sample = 0; sample < SAMPLES; ++sample) {
                memcpy(&saved, &input, sizeof(input));
                stamp before, after;
                if (!sample_clock(&before))
                    return 1;
                unsigned int completed = 0;
                for (; completed < calls; ++completed) {
                    if (!mc_chunk_cache_validate(&input.cache))
                        break;
                    sink = sink * UINT64_C(1099511628211) + 1u;
                }
                if (!sample_clock(&after) || completed != calls || after.cpu_ns < before.cpu_ns ||
                    after.wall <= before.wall || after.frequency != before.frequency ||
                    memcmp(&input, &saved, sizeof(input)) != 0)
                    return 1;
                printf("CACHE_CPU count=%zu pattern=%s sample=%u calls=%u keys=%" PRIu64
                       " baseline-pairs-per-call=%" PRIu64 " cpu-ns=%" PRIu64 " wall-ticks=%" PRIu64
                       " frequency=%" PRIu64 " immutable=1 sink=%" PRIu64 "\n",
                       count, pattern == 0u ? "mixed" : "one-bucket", sample, calls, keys, pairs,
                       after.cpu_ns - before.cpu_ns, after.wall - before.wall, after.frequency,
                       sink);
            }
        }
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 2 || (strcmp(argv[1], "controls") != 0 && strcmp(argv[1], "measure") != 0))
        return 2;
    if (contract_controls() != 0)
        return 1;
    return strcmp(argv[1], "measure") == 0 ? measure() : 0;
}
