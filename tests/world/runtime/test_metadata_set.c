/* Finite quiet metadata calls over the path fixture's real World/Chunk owners. */
#define MC_PATH_ENTRY mc_metadata_set_unused_path_main
#include "test_path.c"
#undef MC_PATH_ENTRY

typedef struct metadata_fixture {
    path_fixture path;
    mc_world_runtime_acquisition_error *acquisition;
    size_t events;
    uint8_t fixed_before[MC_CHUNK_NIBBLE_BYTES];
    uint8_t backing_before[MC_CHUNK_NIBBLE_BYTES];
} metadata_fixture;

static mc_world_storage_status metadata_event(void *context, mc_world_runtime *world,
                                              mc_world_storage_error *error) {
    metadata_fixture *f = context;
    ++f->events;
    *error = (mc_world_storage_error){MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                      world == f->path.base.world
                                          ? "Unexpected quiet metadata WorldAccess event"
                                          : "Unexpected quiet metadata World identity",
                                      0};
    return error->status;
}

static mc_world_storage_status metadata_cell(void *c, mc_world_runtime *w, int32_t x, int32_t y,
                                             int32_t z, mc_world_storage_error *e) {
    (void)x;
    (void)y;
    (void)z;
    return metadata_event(c, w, e);
}

static mc_world_storage_status metadata_range(void *c, mc_world_runtime *w, int32_t x, int32_t y,
                                              int32_t z, int32_t a, int32_t b, int32_t d,
                                              mc_world_storage_error *e) {
    (void)a;
    (void)b;
    (void)d;
    return metadata_cell(c, w, x, y, z, e);
}

static mc_world_storage_status metadata_sound(void *c, mc_world_runtime *w,
                                              const mc_registry_string *n, double x, double y,
                                              double z, float volume, float pitch,
                                              mc_world_storage_error *e) {
    (void)n;
    (void)x;
    (void)y;
    (void)z;
    (void)volume;
    (void)pitch;
    return metadata_event(c, w, e);
}

static mc_world_storage_status metadata_particle(void *c, mc_world_runtime *w,
                                                 const mc_registry_string *n, double x, double y,
                                                 double z, double a, double b, double d,
                                                 mc_world_storage_error *e) {
    (void)n;
    (void)x;
    (void)y;
    (void)z;
    (void)a;
    (void)b;
    (void)d;
    return metadata_event(c, w, e);
}

static mc_world_storage_status metadata_entity(void *c, mc_world_runtime *w, mc_entity *entity,
                                               mc_world_storage_error *e) {
    (void)entity;
    return metadata_event(c, w, e);
}

static mc_world_storage_status metadata_tile(void *c, mc_world_runtime *w, int32_t x, int32_t y,
                                             int32_t z, mc_tile_entity *tile,
                                             mc_world_storage_error *e) {
    (void)tile;
    return metadata_cell(c, w, x, y, z, e);
}

static mc_world_storage_status metadata_auxiliary(void *c, mc_world_runtime *w, mc_entity *p,
                                                  int32_t effect, int32_t x, int32_t y, int32_t z,
                                                  int32_t data, mc_world_storage_error *e) {
    (void)p;
    (void)effect;
    (void)data;
    return metadata_cell(c, w, x, y, z, e);
}

static const char *boolean(bool value) {
    return value ? "true" : "false";
}

static void nullable_number(bool present, size_t value) {
    if (present)
        printf("%zu", value);
    else
        fputs("null", stdout);
}

static bool selected_membership(metadata_fixture *f, mc_chunk_state *from, mc_chunk_state *to) {
    mc_chunk_cache *cache = f->path.base.provider.cache;
    size_t entries = 0, insertions = 0;
    for (size_t i = 0; i < cache->count; ++i)
        if (cache->entries[i].key == mc_chunk_cache_key(0, 0)) {
            CHECK(cache->entries[i].chunk == from);
            cache->entries[i].chunk = to;
            ++entries;
        }
    for (size_t i = 0; i < cache->insertion_count; ++i)
        if (cache->insertions[i] == from) {
            cache->insertions[i] = to;
            ++insertions;
        }
    CHECK(entries == 1 && insertions == 1 && mc_chunk_cache_validate(cache));
    return true;
}

static bool metadata_row(metadata_fixture *f, mc_chunk_state *selected, mc_world_chunk *object,
                         uint8_t *fixed, const char *profile, int32_t x, int32_t y, int32_t z,
                         int32_t metadata, const char *expected_exception, int expected_before,
                         int expected_after, bool expected_fixed_unchanged) {
    const size_t index = ((size_t)8 << 11 | (size_t)8 << 7 | 64u) >> 1;
    mc_world_runtime *world = f->path.base.world;
    mc_chunk_cache *cache = f->path.base.provider.cache;
    const bool outside =
        x < -32000000 || x >= 32000000 || z < -32000000 || z > 32000000 || y < 0 || y >= 128;
    /* Blank owns the provider-created literal override. Its declared unused
     * e=NULL operand needs no normal Chunk header or NibbleArray inspection. */
    CHECK(selected->blank == (object == NULL));
    mc_world_chunk_nibble *nibble = object == NULL ? NULL : object->e;
    uint8_t *backing = nibble == NULL ? NULL : nibble->a;
    const size_t length = backing == NULL ? 0 : nibble->length;
    const bool has_byte = backing != NULL && index < length;
    const unsigned int before_byte = has_byte ? backing[index] : 0;
    CHECK(length <= sizeof(f->backing_before) && cache->count == 9 && cache->insertion_count == 9);
    memcpy(f->fixed_before, fixed, sizeof(f->fixed_before));
    if (backing != NULL)
        memcpy(f->backing_before, backing, length);
    mc_chunk_cache_entry entries[9];
    mc_chunk_state *insertions[9];
    memcpy(entries, cache->entries, sizeof(entries));
    memcpy(insertions, cache->insertions, sizeof(insertions));
    const mc_java_random random_before = *world->fields.r;
    mc_chunk_provider_cache_observation before, after;
    CHECK(mc_chunk_provider_cache_observe(world->fields.v, &before));
    selected->dirty = false;
    const bool dirty_before = selected->dirty;
    f->events = 0;
    f->acquisition->status = MC_CHUNK_ACQUIRE_LIMIT;
    bool answer = false;
    mc_world_storage_error error;
    mc_world_metadata_source_error source;
    const mc_world_storage_status status = mc_world_runtime_metadata_set_quiet_source_failure(
        world, x, y, z, metadata, &answer, &error, f->acquisition, &source);
    const bool returned = status == MC_WORLD_STORAGE_OK;
    const char *exception = source.source_exception ? source.exception_class : NULL;
    CHECK((object == NULL || object->e == nibble) &&
          (nibble == NULL ||
           (nibble->a == backing && (backing == NULL || nibble->length == length))));
    const unsigned int after_byte = has_byte ? backing[index] : 0;
    const bool fixed_unchanged = memcmp(f->fixed_before, fixed, sizeof(f->fixed_before)) == 0;
    bool unselected_unchanged = true;
    for (size_t i = 0; i < length; ++i)
        if (i != index && f->backing_before[i] != backing[i])
            unselected_unchanged = false;
    const bool membership_unchanged =
        cache->count == 9 && cache->insertion_count == 9 &&
        memcmp(entries, cache->entries, sizeof(entries)) == 0 &&
        memcmp(insertions, cache->insertions, sizeof(insertions)) == 0;
    const bool random_unchanged =
        memcmp(&random_before, world->fields.r, sizeof(random_before)) == 0;
    CHECK(mc_chunk_provider_cache_observe(world->fields.v, &after) &&
          after.generic_gets == before.generic_gets + (outside ? 0u : 1u));
    CHECK(membership_unchanged && random_unchanged && unselected_unchanged && f->events == 0 &&
          fixed_unchanged == expected_fixed_unchanged &&
          selected->dirty == (!outside && !selected->blank));
    CHECK(has_byte == (expected_before >= 0) &&
          (!has_byte || (before_byte == (unsigned int)expected_before &&
                         after_byte == (unsigned int)expected_after)));
    if (expected_exception == NULL) {
        CHECK(returned && !source.source_exception && source.exception_class == NULL &&
              answer == !outside);
        if (outside)
            CHECK(f->acquisition->status == MC_CHUNK_ACQUIRE_LIMIT);
    } else {
        CHECK(!returned && status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && !answer &&
              exception != NULL && strcmp(exception, expected_exception) == 0 &&
              source.world_pc == 70 && source.chunk_pc == 14 &&
              source.nibble_pc == (nibble == NULL ? 0u : (y == 64 ? 42u : 70u)) &&
              f->acquisition->status == MC_CHUNK_ACQUIRE_OK &&
              f->acquisition->source_chunk == NULL &&
              f->acquisition->cause.status == MC_WORLD_STORAGE_OK);
    }
    printf("METADATA_SET {\"profile\":\"%s\",\"x\":%" PRId32 ",\"y\":%" PRId32 ",\"z\":%" PRId32
           ",\"metadata\":%" PRId32 ",\"returned\":%s,\"result\":%s,\"exception\":",
           profile, x, y, z, metadata, boolean(returned), returned ? boolean(answer) : "null");
    if (exception == NULL)
        fputs("null", stdout);
    else
        printf("\"%s\"", exception);
    printf(",\"dirty_before\":%s,\"dirty_after\":%s,\"nibble_null\":%s,\"backing_null\":%s,"
           "\"backing_length\":",
           boolean(dirty_before), boolean(selected->dirty), boolean(nibble == NULL),
           boolean(backing == NULL));
    nullable_number(backing != NULL, length);
    fputs(",\"before_byte\":", stdout);
    nullable_number(has_byte, before_byte);
    fputs(",\"after_byte\":", stdout);
    nullable_number(has_byte, after_byte);
    printf(",\"fixed_before\":%u,\"fixed_after\":%u,\"fixed_unchanged\":%s,\"unselected_"
           "unchanged\":%s,"
           "\"cache_membership_unchanged\":%s,\"world_random_unchanged\":%s,\"events\":%zu}\n",
           (unsigned int)f->fixed_before[index], (unsigned int)fixed[index],
           boolean(fixed_unchanged), boolean(unselected_unchanged), boolean(membership_unchanged),
           boolean(random_unchanged), f->events);
    return true;
}

static bool metadata_cases(const char *resources, const char *directory) {
    metadata_fixture *f = calloc(1, sizeof(*f));
    CHECK(f != NULL && setup(&f->path.base, resources, directory));
    for (int32_t x = -1; x <= 1; ++x)
        for (int32_t z = -1; z <= 1; ++z) {
            mc_chunk_state *state = NULL;
            CHECK(load_chunk(&f->path.base, x, z, &state) &&
                  mc_chunk_cache_publish(f->path.base.provider.cache, x, z, state));
        }
    mc_chunk_state *normal = NULL;
    CHECK(mc_chunk_cache_lookup(f->path.base.provider.cache, 0, 0, &normal) && normal != NULL);
    mc_world_chunk *object = mc_world_store_chunk_object(f->path.base.store, normal);
    CHECK(object != NULL && object->e != NULL && object->e->a != NULL &&
          object->e->length == MC_CHUNK_NIBBLE_BYTES);
    mc_world_chunk_nibble *original = object->e;
    uint8_t *fixed = original->a;
    const size_t index = ((size_t)8 << 11 | (size_t)8 << 7 | 64u) >> 1;
    CHECK(fixed[index] == 0);
    uint8_t *replacement = calloc(index + 1, 1);
    f->acquisition = calloc(1, sizeof(*f->acquisition));
    CHECK(replacement != NULL && f->acquisition != NULL);
    const mc_world_access_callbacks observer = {.cell = metadata_cell,
                                                .range = metadata_range,
                                                .sound = metadata_sound,
                                                .particle = metadata_particle,
                                                .entity_added = metadata_entity,
                                                .tile_entity_dirty = metadata_tile,
                                                .entity_removed = metadata_entity,
                                                .sky_changed = metadata_event,
                                                .auxiliary = metadata_auxiliary};
    CHECK(mc_world_access_create(&observer, f, &f->path.base.access, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(f->path.base.world, f->path.base.access, NULL) ==
              MC_WORLD_STORAGE_OK);
    mc_java_random_reseed(f->path.base.world->fields.r, 2468);
    /* These are authored operands between complete source invocations. No
     * assignment below repairs a pending call or supplies its cleanup. */
    mc_world_chunk_nibble alternate = {replacement, index + 1};
    replacement[index] = 0xa3;
    object->e = &alternate;
    CHECK(metadata_row(f, normal, object, fixed, "replacement-even18", 8, 64, 8, 18, NULL, 0xa3,
                       0xa2, true));
    CHECK(metadata_row(f, normal, object, fixed, "replacement-odd-minus1", 8, 65, 8, -1, NULL, 0xa2,
                       0xf2, true));
    const char *npe = "java/lang/NullPointerException";
    const char *bounds = "java/lang/ArrayIndexOutOfBoundsException";
    object->e = NULL;
    CHECK(metadata_row(f, normal, object, fixed, "e-null-even", 8, 64, 8, 18, npe, -1, -1, true));
    CHECK(metadata_row(f, normal, object, fixed, "e-null-odd", 8, 65, 8, 18, npe, -1, -1, true));
    object->e = original;
    original->a = NULL;
    CHECK(metadata_row(f, normal, object, fixed, "a-null-even", 8, 64, 8, 18, npe, -1, -1, true));
    CHECK(metadata_row(f, normal, object, fixed, "a-null-odd", 8, 65, 8, 18, npe, -1, -1, true));
    original->a = replacement;
    original->length = 0;
    CHECK(metadata_row(f, normal, object, fixed, "short-even", 8, 64, 8, 18, bounds, -1, -1, true));
    CHECK(metadata_row(f, normal, object, fixed, "short-odd", 8, 65, 8, 18, bounds, -1, -1, true));
    original->a = fixed;
    original->length = MC_CHUNK_NIBBLE_BYTES;
    mc_chunk_state *blank = &f->path.base.world->fallback;
    CHECK(blank->blank && selected_membership(f, normal, blank));
    CHECK(metadata_row(f, blank, NULL, fixed, "blank-null-e", 8, 64, 8, 18, NULL, -1, -1, true));
    CHECK(selected_membership(f, blank, normal));
    object->e = NULL;
    CHECK(
        metadata_row(f, normal, object, fixed, "outside-y128", 8, 128, 8, 18, NULL, -1, -1, true));
    CHECK(metadata_row(f, normal, object, fixed, "outside-y-minus1", 8, -1, 8, 18, NULL, -1, -1,
                       true));
    CHECK(metadata_row(f, normal, object, fixed, "outside-x-upper", 32000000, 64, 8, 18, NULL, -1,
                       -1, true));
    CHECK(metadata_row(f, normal, object, fixed, "outside-z-upper", 8, 64, 32000001, 18, NULL, -1,
                       -1, true));
    object->e = original;
    fixed[index] = 0xa3;
    CHECK(metadata_row(f, normal, object, fixed, "normal-even18", 8, 64, 8, 18, NULL, 0xa3, 0xa2,
                       false));
    CHECK(metadata_row(f, normal, object, fixed, "normal-odd-minus1", 8, 65, 8, -1, NULL, 0xa2,
                       0xf2, false));
    CHECK(teardown(&f->path.base));
    free(replacement);
    free(f->acquisition);
    free(f);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3)
        return 2;
    if (!metadata_cases(argv[1], argv[2]))
        return 1;
    printf("PASS: %u Metadata set source-order/ownership checks\n", checks);
    return 0;
}
