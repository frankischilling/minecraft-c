/* Compare complete EW metadata failures on genuine World and Store owners. */
static bool ew_metadata_text_equal(const char *a, const char *b) {
    return a == b || (a != NULL && b != NULL && strcmp(a, b) == 0);
}

#define ew_text_equal ew_metadata_text_equal
#define ew_error_equal ew_metadata_error_equal
#include "../../benchmarks/chunk_cache_error_equal.inc"
#undef ew_error_equal
#undef ew_text_equal

static bool ew_metadata_failure_pair(mc_client_chunk_cache *cache, int32_t x, int32_t y, int32_t z,
                                     bool output) {
    mc_client_chunk_cache_error detailed, failure;
    unsigned char prior[sizeof(failure)];
    memset(&failure, 0xa5, sizeof(failure));
    memcpy(prior, &failure, sizeof(prior));
    mc_client_chunk_cache before;
    mc_client_chunk_cache_occurrence occurrences[4];
    store_chunk *entries[4] = {0};
    size_t leases[4] = {0};
    const size_t count = cache == NULL ? 0 : cache->cell_count;
    CHECK(count <= 4);
    unsigned char *world_before = NULL;
    if (cache != NULL) {
        memcpy(&before, cache, sizeof(before));
        memcpy(occurrences, cache->occurrences, count * sizeof(*occurrences));
        for (size_t i = 0; i < count; ++i) {
            if (!occurrences[i].leased)
                continue;
            entries[i] = store_find_chunk(occurrences[i].store, occurrences[i].state);
            CHECK(entries[i] != NULL);
            leases[i] = entries[i]->leases;
        }
        if (cache->state.d != NULL) {
            world_before = malloc(sizeof(*cache->state.d));
            CHECK(world_before != NULL);
            memcpy(world_before, cache->state.d, sizeof(*cache->state.d));
        }
    }
    int32_t expected = -179, actual = expected;
    const mc_client_chunk_cache_status status =
        mc_client_chunk_cache_metadata(cache, x, y, z, output ? &expected : NULL, &detailed);
    CHECK(cache == NULL ||
          (memcmp(cache, &before, sizeof(before)) == 0 &&
           memcmp(cache->occurrences, occurrences, count * sizeof(*occurrences)) == 0));
    for (size_t i = 0; i < count; ++i)
        CHECK(entries[i] == NULL || entries[i]->leases == leases[i]);
    CHECK(world_before == NULL ||
          memcmp(world_before, cache->state.d, sizeof(*cache->state.d)) == 0);
    CHECK(mc_client_chunk_cache_metadata_failure(cache, x, y, z, output ? &actual : NULL,
                                                 &failure) == status &&
          actual == expected);
    CHECK(cache == NULL ||
          (memcmp(cache, &before, sizeof(before)) == 0 &&
           memcmp(cache->occurrences, occurrences, count * sizeof(*occurrences)) == 0));
    for (size_t i = 0; i < count; ++i)
        CHECK(entries[i] == NULL || entries[i]->leases == leases[i]);
    CHECK(world_before == NULL ||
          memcmp(world_before, cache->state.d, sizeof(*cache->state.d)) == 0);
    if (status == MC_CLIENT_CHUNK_CACHE_OK) {
        CHECK(memcmp(prior, &failure, sizeof(prior)) == 0 &&
              detailed.status == MC_CLIENT_CHUNK_CACHE_OK && strcmp(detailed.method, "e") == 0 &&
              strcmp(detailed.descriptor, "(III)I") == 0);
    } else {
        CHECK(actual == -179 && ew_metadata_error_equal(&failure, &detailed));
    }
    free(world_before);
    return true;
}

static bool ew_metadata_failure_controls(picking_fixture *f) {
    mc_client_chunk_cache *cache = NULL;
    mc_client_chunk_cache_error error;
    CHECK(mc_client_chunk_cache_create(f->base.world, 0, 0, 0, 31, 127, 31, &cache, &error) ==
              MC_CLIENT_CHUNK_CACHE_OK &&
          cache != NULL && cache->state.leased == 4);
    static const int32_t positions[][3] = {
        {8, 64, 8},         {0, 0, 0},          {31, 127, 31},      {8, -1, 8},
        {8, 128, 8},        {8, INT32_MIN, 8},  {8, INT32_MAX, 8},  {INT32_MIN, 64, 8},
        {INT32_MAX, 64, 8}, {8, 64, INT32_MIN}, {8, 64, INT32_MAX}, {-1, 64, 8},
        {32, 64, 8},        {8, 64, -1},        {8, 64, 32},        {15, 63, 15},
        {16, 65, 16}};
    for (size_t i = 0; i < sizeof(positions) / sizeof(*positions); ++i)
        CHECK(ew_metadata_failure_pair(cache, positions[i][0], positions[i][1], positions[i][2],
                                       true));
    CHECK(ew_metadata_failure_pair(NULL, 8, 64, 8, true) &&
          ew_metadata_failure_pair(NULL, 8, -1, 8, false) &&
          ew_metadata_failure_pair(cache, 8, 64, 8, false) &&
          ew_metadata_failure_pair(cache, 8, -1, 8, false));
    cache->state.active = true;
    CHECK(ew_metadata_failure_pair(cache, 8, 64, 8, true) &&
          ew_metadata_failure_pair(cache, 8, -1, 8, true));
    cache->state.active = false;
    cache->state.retiring = true;
    CHECK(ew_metadata_failure_pair(cache, 8, 64, 8, true));
    cache->state.retiring = false;
    mc_chunk_state *const *const *rows = cache->state.c;
    cache->state.c = NULL;
    CHECK(ew_metadata_failure_pair(cache, 8, 64, 8, true) &&
          ew_metadata_failure_pair(cache, -1, 64, 8, true) &&
          ew_metadata_failure_pair(cache, 8, -1, 8, true));
    cache->state.c = rows;
    mc_chunk_state *state = cache->occurrences[0].state;
    const bool leased = cache->occurrences[0].leased;
    /* Temporarily detach only this accessor projection; do not forge a lease
     * on NULL. Restore the genuine occurrence before retirement. */
    cache->occurrences[0].state = NULL;
    cache->occurrences[0].leased = false;
    CHECK(ew_metadata_failure_pair(cache, 8, 64, 8, true));
    cache->occurrences[0].state = state;
    cache->occurrences[0].leased = leased;
    const bool blank = state->blank;
    state->blank = true;
    CHECK(ew_metadata_failure_pair(cache, 8, 64, 8, true));
    state->blank = blank;
    mc_world_chunk *object = cache->occurrences[0].object;
    CHECK(object != NULL && object->e != NULL);
    mc_chunk_data *data = object->data;
    object->data = NULL;
    CHECK(ew_metadata_failure_pair(cache, 8, 64, 8, true));
    object->data = data; /* Metadata reads the live e descriptor, not data. */
    mc_world_chunk_nibble *nibble = object->e;
    object->e = NULL;
    CHECK(ew_metadata_failure_pair(cache, 8, 64, 8, true));
    state->blank = true;
    CHECK(ew_metadata_failure_pair(cache, 8, 64, 8, true));
    state->blank = blank;
    object->e = nibble;
    uint8_t *backing = nibble->a;
    nibble->a = NULL;
    CHECK(ew_metadata_failure_pair(cache, 8, 64, 8, true));
    nibble->a = backing;
    const size_t length = nibble->length;
    const size_t index = ((8u << 11u) | (8u << 7u) | 64u) / 2u;
    nibble->length = index;
    CHECK(ew_metadata_failure_pair(cache, 8, 64, 8, true));
    int32_t untouched = 79;
    CHECK(mc_client_chunk_cache_metadata_failure(cache, 8, 64, 8, &untouched, NULL) ==
              MC_CLIENT_CHUNK_CACHE_SOURCE_INDEX &&
          untouched == 79);
    nibble->length = index + 1u;
    CHECK(ew_metadata_failure_pair(cache, 8, 64, 8, true) &&
          ew_metadata_failure_pair(cache, 8, 65, 8, true));
    nibble->length = length;
    CHECK(mc_client_chunk_cache_retire(&cache, &error) == MC_CLIENT_CHUNK_CACHE_OK &&
          cache == NULL);
    CHECK(mc_client_chunk_cache_create(NULL, 0, 0, 0, 15, 127, 15, &cache, &error) ==
              MC_CLIENT_CHUNK_CACHE_SOURCE_NULL &&
          cache != NULL && ew_metadata_failure_pair(cache, 8, 64, 8, true) &&
          ew_metadata_failure_pair(cache, 8, -1, 8, true) &&
          mc_client_chunk_cache_retire(&cache, &error) == MC_CLIENT_CHUNK_CACHE_OK);
    return true;
}
