#include "mc/world/chunk/nibbles.h"
#include "nibbles_reference_fixtures.h"

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

static uint8_t packed[MC_CHUNK_NIBBLE_BYTES + 3u];
static uint8_t expected_packed[sizeof(packed)];

_Static_assert(MC_CHUNK_BLOCKS == 32768u, "normal reference cell count");
_Static_assert(MC_CHUNK_NIBBLE_BYTES == 16384u, "normal reference packed length");
_Static_assert(sizeof(nibble_reference_writes) / sizeof(nibble_reference_writes[0]) == 16u,
               "complete original write sequence");
_Static_assert(sizeof(nibble_reference_deltas) / sizeof(nibble_reference_deltas[0]) == 15u,
               "complete observed byte deltas");

static int check_getter(size_t capacity, const nibble_reference_write *write, uint8_t expected)
{
    uint8_t value = 0xc3;
    CHECK(mc_chunk_nibbles_get(packed, capacity, write->x, write->y, write->z, &value));
    CHECK(value == expected);
    CHECK(memcmp(packed, expected_packed, sizeof(packed)) == 0);
    return 0;
}

static int test_original_sequence(size_t capacity)
{
    /* This is the observer's input recipe. Expected setter output comes only
     * from the captured deltas in nibbles_reference_fixtures.h. */
    for (size_t i = 0; i < MC_CHUNK_NIBBLE_BYTES; ++i) {
        packed[i] = (uint8_t)(i * 73u + (i >> 3u) * 19u + (i >> 8u) * 151u + 0xa7u);
    }
    /* Extra capacity is a C storage contract. The original packed prefix uses
     * the same fixture in both runs, while these sentinel bytes must survive. */
    packed[MC_CHUNK_NIBBLE_BYTES] = 0x36;
    packed[MC_CHUNK_NIBBLE_BYTES + 1u] = 0x94;
    packed[MC_CHUNK_NIBBLE_BYTES + 2u] = 0xe2;
    memcpy(expected_packed, packed, sizeof(packed));

    const size_t write_count = sizeof(nibble_reference_writes) / sizeof(nibble_reference_writes[0]);
    const size_t delta_count = sizeof(nibble_reference_deltas) / sizeof(nibble_reference_deltas[0]);
    for (size_t i = 0; i < write_count; ++i) {
        const nibble_reference_write *write = &nibble_reference_writes[i];
        if (check_getter(capacity, write, write->initial_getter))
            return 1;
    }

    for (size_t i = 0; i < write_count; ++i) {
        const nibble_reference_write *write = &nibble_reference_writes[i];
        CHECK(write->first_delta <= delta_count &&
              write->delta_count <= delta_count - write->first_delta);
        for (size_t change = 0; change < write->delta_count; ++change) {
            const nibble_reference_delta *delta =
                &nibble_reference_deltas[write->first_delta + change];
            CHECK(delta->offset < MC_CHUNK_NIBBLE_BYTES &&
                  expected_packed[delta->offset] == delta->before);
            expected_packed[delta->offset] = delta->after;
        }
        CHECK(mc_chunk_nibbles_set(packed, capacity, write->x, write->y, write->z, write->value));
        CHECK(memcmp(packed, expected_packed, sizeof(packed)) == 0);
        if (check_getter(capacity, write, write->observed_getter))
            return 1;
    }

    for (size_t i = 0; i < write_count; ++i) {
        const nibble_reference_write *write = &nibble_reference_writes[i];
        if (check_getter(capacity, write, write->final_getter))
            return 1;
    }
    return 0;
}

int main(void)
{
    if (test_original_sequence(MC_CHUNK_NIBBLE_BYTES) || test_original_sequence(sizeof(packed)))
        return 1;
    printf("packed nibble reference checks: %u (16 writes, 2 capacity variants)\n", checks);
    return 0;
}
