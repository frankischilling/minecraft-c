#ifndef MC_TEST_WORLD_CHUNK_NIBBLES_REFERENCE_FIXTURES_H
#define MC_TEST_WORLD_CHUNK_NIBBLES_REFERENCE_FIXTURES_H

#include <stddef.h>
#include <stdint.h>

/* Values come from paired untouched client wi/server ob observations.
 * Constructor (I)V, getter a(III)I, setter a(IIII)V, field a:[B.
 * Original capture manifest SHA256:
 * 874e6cfb8c4ceb319d0547ca225b87e1db55165a9e1f6162fbdbd642347557a7
 * Client JAR SHA256:
 * af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c
 * Server JAR SHA256:
 * 033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d
 * Observer source SHA256:
 * 0dfe9f1dfb1449d9a864e3924fbe62321addf1890ae652e8c676715f574ddf2a
 * Client complete record SHA256:
 * d937ad335015213629a7ecc7f0a6e35a1972b6b7d889a53bee83192a58066204
 * Server complete record SHA256:
 * a2bf046e3dce90121e5dae99ecd8a40e668495c5255d4781b4aac3febd9a1cec
 * Prepared packed array SHA256:
 * 30eb6f7783747080ab9e65a563fd630625dac96e7c70641232b758309dba9f7d
 * Initial complete getter sweep SHA256:
 * 0175ebf465f7d8b1a9d235ca28ba3d4ce1c9d25b7fcd7030cb2c810a4d657128
 * Final complete getter sweep SHA256:
 * bccd2c72cb8e39de3039d50357cbf073cd14444775078db358e1e1385e01c754
 * Each row below retains the full packed snapshot SHA256. The deltas
 * reconstruct those observed snapshots from the explicit prepared input.
 */

typedef struct nibble_reference_delta {
    size_t offset;
    uint8_t before;
    uint8_t after;
} nibble_reference_delta;

typedef struct nibble_reference_write {
    int32_t x;
    int32_t y;
    int32_t z;
    int32_t value;
    uint8_t initial_getter;
    uint8_t observed_getter;
    uint8_t final_getter;
    size_t first_delta;
    size_t delta_count;
} nibble_reference_write;

static const nibble_reference_delta nibble_reference_deltas[] = {
    {0u, 0xa7, 0xa0},    {0u, 0xa0, 0xf0},     {16383u, 0x74, 0x70}, {16383u, 0x70, 0xf0},
    {1023u, 0x90, 0x9f}, {1023u, 0x9f, 0x0f},  {15360u, 0x8b, 0x80}, {15360u, 0x80, 0xf0},
    {3776u, 0xb1, 0xbf}, {3776u, 0xbf, 0x0f},  {11519u, 0x1f, 0x0f}, {1952u, 0x84, 0x80},
    {1952u, 0x80, 0xf0}, {14431u, 0x97, 0x9f}, {14431u, 0x9f, 0x0f},
};

static const nibble_reference_write nibble_reference_writes[] = {
    /* 5a49725ec474cba7a4d28db071e934fc1b83d583a473b91db3e10da7b4385527 */
    {0, 0, 0, 0, 7, 0, 0, 0u, 1u},
    /* 8441129dce34f13ae85d36e5874fce5be542f432fa9bc4f73abf2e330231bcc1 */
    {0, 1, 0, 15, 10, 15, 15, 1u, 1u},
    /* 3c86f5be9b32b80c22df1af3d2c164f319ba2abef06766411a11e36103b074a4 */
    {15, 126, 15, 16, 4, 0, 0, 2u, 1u},
    /* 76dea3ba15d27d792edd8a1c86b2b907f556079a5042af25b88e50e591b93919 */
    {15, 127, 15, 31, 7, 15, 15, 3u, 1u},
    /* 22ff995fe040ea46adb10291d2d4ae95554fce4c7f21947bf4430d23ce45bfe2 */
    {0, 126, 15, -1, 0, 15, 15, 4u, 1u},
    /* d2433b808c2a788d3d1aeb3fd150ecc1b9739c06a5d8d85848722bc91ff034cc */
    {0, 127, 15, -16, 9, 0, 0, 5u, 1u},
    /* f5aea621829aef2eea2dba23dbc151216b0b26900a2c303e656831c694ffb705 */
    {15, 0, 0, INT32_MIN, 11, 0, 0, 6u, 1u},
    /* 82bc6e8b2030c1fe8dfc997af9250bef42d051dc601e8f2249b949cc602c2ad2 */
    {15, 1, 0, INT32_MAX, 8, 15, 15, 7u, 1u},
    /* 252db58801449b78dfd8ef64d22ab98411a85912e1ab10e24967f224f4418617 */
    {3, 0, 11, 15, 1, 15, 15, 8u, 1u},
    /* 127828bc854a144bd39b47811701c292fef0952e25f36cf9f7aa3a8e3ad50929 */
    {3, 1, 11, 0, 11, 0, 0, 9u, 1u},
    /* 127828bc854a144bd39b47811701c292fef0952e25f36cf9f7aa3a8e3ad50929 */
    {11, 126, 3, 31, 15, 15, 15, 10u, 0u},
    /* 41221aa21b34e6658240a81f5931a7f4e5990ec3c3c1571964e921db6c01996b */
    {11, 127, 3, 16, 1, 0, 0, 10u, 1u},
    /* 92487de466824d20b2c4ab10611373db18db77973dae0ca8e770e7907c9dcc97 */
    {1, 64, 14, -16, 4, 0, 0, 11u, 1u},
    /* 6d54cc0e284959e36c15588caed9ea46e85f0a34d0dd4b89801680c7f3260982 */
    {1, 65, 14, -1, 8, 15, 15, 12u, 1u},
    /* 3507a8fb578a87928372f2ac21c5cfd5251cf0024b67f3bc2d07b3b2f8794812 */
    {14, 62, 1, INT32_MAX, 7, 15, 15, 13u, 1u},
    /* f202c5c89bce54aa9eb522f6c7ac59feff3c271c381c28e73a94b29af6f37c56 */
    {14, 63, 1, INT32_MIN, 9, 0, 0, 14u, 1u},
};

#endif
