/* Independent java.util.Random-only schedules for constructed C inputs. */
static const uint64_t controlled_air_after[2][4] = {
    {UINT64_C(0x83c36945d52b66f8), UINT64_C(0xb1dd41377e251735), UINT64_C(0xbf5eb2d2351f19b3),
     UINT64_C(0xd811cbd923a82576)},
    {UINT64_C(0xe258185a33517638), UINT64_C(0xae37cce7922b20e3), UINT64_C(0x2f38e5e1877d46a8),
     UINT64_C(0xe64f603f04e7192e)},
};
static const uint64_t controlled_sand_after[2][4] = {
    {UINT64_C(0x30bb662eb19dc046), UINT64_C(0x252c7725ebdcacdc), UINT64_C(0x50ed84a2cd6ae4fa),
     UINT64_C(0x99a5606171612d17)},
    {UINT64_C(0x2936305349318685), UINT64_C(0x8e9733c85e5f2df4), UINT64_C(0x7529b507a0fab18d),
     UINT64_C(0x6221b83e56753f3e)},
};
static const int32_t controlled_first_sandstone[] = {1, 0};
typedef struct chunk_seed_fixture {
    int32_t x;
    int32_t z;
    uint64_t bits;
} chunk_seed_fixture;

/* Integer-arithmetic boundary controls, derived with unbounded integers then
 * reduced modulo 2^64; these are separate from original runtime captures. */
static const chunk_seed_fixture controlled_chunk_seeds[] = {
    {INT32_MIN, INT32_MAX, UINT64_C(0xac0e33478ea9a42b)},
    {INT32_MAX, INT32_MIN, UINT64_C(0x53f1cc49e6c60af8)},
    {INT32_MAX, INT32_MAX, UINT64_C(0x454827fff56faf23)},
    {INT32_MIN, INT32_MIN, UINT64_C(0xbab7d79180000000)},
    {-1, 1, UINT64_C(0xffffffcf581c66cd)},
    {134217728, -134217729, UINT64_C(0x853f1caaa6a9a42b)},
};
