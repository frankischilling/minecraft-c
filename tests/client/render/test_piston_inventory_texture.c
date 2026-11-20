/* Actual registered JQ receivers and genuine lazy WJ array ownership. */
#include "block/registry/registry_internal.h"
#include "block/registry/runtime_internal.h"
#include "mc/block/registry/rendering.h"
#include "mc/block/registry/runtime.h"
#include "mc/entity/classes/classes.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int checks;
#define CHECK(value)                                                                               \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(value)) {                                                                            \
            fprintf(stderr, "JQ texture check%u at%d: %s\n", checks, __LINE__, #value);            \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)

static const int32_t metadata_values[] = {
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15,  16, 17, 18,        19,
    20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, -1, -2, -9, -17, 32, 40, INT32_MIN, INT32_MAX};
static const int32_t sides[] = {INT32_MIN, -1, 0, 1, 2, 3, 4, 5, 6, 7, INT32_MAX};
static const int32_t mutations[] = {INT32_MIN, -1, 42, INT32_MAX};

typedef struct stream {
    const uint8_t *data;
    size_t length, position;
    bool closed;
} stream;

typedef struct fixture {
    mc_materials *materials;
    mc_java_random_profile *profile;
    mc_entity_classes *entities;
    mc_registry_classes *registry;
    stream streams[3];
    size_t opens, closes;
    mc_registry_resources resources;
} fixture;

static mc_java_properties_status resource_read(void *context, uint8_t *out, size_t capacity,
                                               size_t *count) {
    stream *input = context;
    if (input == NULL || out == NULL || capacity == 0 || count == NULL || input->closed ||
        input->position > input->length)
        return MC_JAVA_PROPERTIES_IO_ERROR;
    size_t take = input->length - input->position;
    if (take > capacity)
        take = capacity;
    if (take != 0)
        memcpy(out, input->data + input->position, take);
    input->position += take;
    *count = take;
    return MC_JAVA_PROPERTIES_OK;
}

static mc_registry_status resource_open(void *context, const char *name,
                                        mc_java_properties_input *out) {
    static const char *const names[] = {"/lang/en_US.lang", "/lang/stats_US.lang",
                                        "/achievement/map.txt"};
    fixture *f = context;
    CHECK(f->opens < 3 && strcmp(name, names[f->opens]) == 0 && out != NULL);
    *out = (mc_java_properties_input){&f->streams[f->opens++], resource_read};
    return MC_REGISTRY_OK;
}

static mc_registry_status resource_close(void *context, const mc_java_properties_input *input) {
    fixture *f = context;
    CHECK(input != NULL && input->context == &f->streams[2] && input->read == resource_read);
    f->streams[2].closed = true;
    ++f->closes;
    return MC_REGISTRY_OK;
}

static bool resource_ready(void *context, const mc_java_properties_input *input) {
    (void)context;
    CHECK(input != NULL && input->read == resource_read);
    return false;
}

static void open_fixture(fixture *f, size_t budget) {
    /* The source PrintStream writes its actual descriptor. Publish complete
     * native rows before this initializer emits its ordinary source lines. */
    CHECK(fflush(stdout) == 0);
    static const uint8_t language[] = "stat.mineBlock=Mine %1$s\nstat.useItem=Use %1$s\n"
                                      "stat.breakItem=Break %1$s\nstat.craftItem=Craft %1$s\n";
    f->streams[0] = (stream){language, sizeof(language) - 1, 0, false};
    f->resources = (mc_registry_resources){f, resource_open, resource_close, resource_ready};
    CHECK(mc_materials_create(&f->materials) &&
          mc_java_random_profile_create(SIZE_MAX, &f->profile) == MC_JAVA_RANDOM_PROFILE_OK &&
          mc_entity_classes_create(MC_ENTITY_CLIENT, f->profile, NULL, &f->entities) ==
              MC_ENTITY_OK);
    const mc_registry_options options = {.side = MC_REGISTRY_CLIENT,
                                         .random_profile = f->profile,
                                         .materials = f->materials,
                                         .max_allocation_bytes = budget,
                                         .resources = &f->resources};
    CHECK(mc_registry_classes_allocate(&options, &f->registry, NULL) == MC_REGISTRY_OK &&
          mc_registry_classes_bind_entities(f->registry, f->entities, NULL) == MC_REGISTRY_OK &&
          mc_registry_classes_initialize_blocks(f->registry, NULL) == MC_REGISTRY_OK &&
          f->registry->direction_state.phase == MC_REGISTRY_COLD &&
          f->registry->direction_a == NULL && f->opens == 3 && f->closes == 1);
}

static void close_fixture(fixture *f) {
    CHECK(f->registry->entered == 0 && f->registry->initializer == NULL &&
          mc_registry_classes_free(f->registry, NULL) == MC_REGISTRY_OK &&
          mc_entity_classes_free(f->entities) == MC_ENTITY_OK);
    mc_java_random_profile_free(f->profile);
    mc_materials_destroy(f->materials);
}

static mc_block *block(fixture *f, int32_t id) {
    const mc_block *result = NULL;
    CHECK(mc_registry_block_get(f->registry, id, &result, NULL) == MC_REGISTRY_OK &&
          result != NULL && result->kind == MC_BLOCK_PISTON_BASE);
    return (mc_block *)(void *)result;
}

static uint64_t double_bits(double value) {
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static float from_bits(uint32_t bits) {
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static void shape(mc_block *b, int scene) {
    float values[6] = {0, 0, 0, 1, 1, 1};
    static const float partial[] = {.125f, .25f, .375f, .75f, .875f, 1};
    static const float outside[] = {-.125f, -.25f, -.375f, 1.125f, 1.25f, 1.375f};
    if (scene == 1)
        memcpy(values, partial, sizeof(values));
    if (scene == 2)
        memcpy(values, outside, sizeof(values));
    if (scene == 3)
        values[0] = values[2] = from_bits(UINT32_C(0x80000000));
    if (scene >= 4 && scene < 10)
        values[scene - 4] = from_bits(UINT32_C(0x7fc12345));
    if (scene == 10)
        for (size_t i = 0; i < 6; ++i)
            values[i] = from_bits(UINT32_C(0x7fc12345));
    if (scene == 11 || scene == 12)
        for (size_t i = 0; i < 6; ++i)
            values[i] =
                from_bits((i < 3) == (scene == 11) ? UINT32_C(0xff800000) : UINT32_C(0x7f800000));
    CHECK(mc_block_render_bounds(b, values[0], values[1], values[2], values[3], values[4],
                                 values[5]) == MC_REGISTRY_OK);
}

static void bounds_row(const mc_block *b) {
    const double values[] = {b->fields.bs, b->fields.bt, b->fields.bu,
                             b->fields.bv, b->fields.bw, b->fields.bx};
    for (size_t i = 0; i < 6; ++i)
        printf("%s%016" PRIx64, i == 0 ? "" : ",", double_bits(values[i]));
}

static int32_t texture(mc_block *b, int32_t side, int32_t metadata) {
    const mc_block before = *b;
    int32_t value = INT32_C(0x12345678);
    CHECK(mc_block_render_texture_metadata(b, side, metadata, &value) == MC_REGISTRY_OK &&
          memcmp(b, &before, sizeof(before)) == 0);
    return value;
}

static void native_failures(fixture *f, size_t base_bytes) {
    mc_block *b = block(f, 29);
    const mc_block before = *b;
    int32_t *saved = f->registry->direction_a;
    f->registry->direction_a = NULL;
    int32_t value = -91;
    const mc_registry_status failed = mc_block_render_texture_metadata(b, 0, 1, &value);
    const mc_registry_status bypass = mc_block_render_texture_metadata(b, 1, 1, &value);
    f->registry->direction_a = saved;
    CHECK(failed == MC_REGISTRY_NULL_REFERENCE && bypass == MC_REGISTRY_OK &&
          value == b->fields.bm && memcmp(b, &before, sizeof(before)) == 0);
    f->registry->direction_a = NULL;
    value = -91;
    const mc_registry_status still_null = mc_block_render_texture_metadata(b, 0, 1, &value);
    f->registry->direction_a = saved;
    CHECK(still_null == MC_REGISTRY_NULL_REFERENCE && value == -91 &&
          f->registry->last_error.status == MC_REGISTRY_NULL_REFERENCE);
    fixture limited = {0};
    open_fixture(&limited, base_bytes);
    b = block(&limited, 29);
    const size_t before_bytes = limited.registry->allocation_bytes;
    CHECK(before_bytes == base_bytes && texture(b, 0, 6) == b->fields.bm &&
          texture(b, 1, 1) == b->fields.bm && texture(b, 1, 9) == 110 &&
          limited.registry->direction_state.phase == MC_REGISTRY_COLD);
    value = -91;
    CHECK(mc_block_render_texture_metadata(b, 0, 1, &value) == MC_REGISTRY_LIMIT && value == -91 &&
          limited.registry->direction_state.phase == MC_REGISTRY_FAILED &&
          limited.registry->direction_state.pc == 2 && limited.registry->direction_a == NULL &&
          limited.registry->last_error.active_initializer_pc == 2 &&
          strcmp(limited.registry->last_error.active_initializer_class, "wj") == 0 &&
          limited.registry->allocation_bytes == before_bytes);
    CHECK(texture(b, 1, 1) == b->fields.bm && texture(b, 1, 9) == 110 &&
          texture(b, 0, 7) == b->fields.bm);
    value = -91;
    CHECK(mc_block_render_texture_metadata(b, 0, 1, &value) == MC_REGISTRY_FAILED_INITIALIZATION &&
          value == -91 && limited.registry->allocation_bytes == before_bytes);
    close_fixture(&limited);
}

int main(void) {
    fixture f = {0};
    open_fixture(&f, 0);
    const size_t base_bytes = f.registry->allocation_bytes;
    mc_block *first = block(&f, 29);
    static const int32_t prefix_sides[] = {0, 1, 1, 0}, prefix_metadata[] = {6, 1, 9, 1};
    for (size_t p = 0; p < 4; ++p) {
        printf("JQ_PREFIX_BEFORE %zu\n", p);
        const int32_t value = texture(first, prefix_sides[p], prefix_metadata[p]);
        CHECK(f.registry->direction_state.phase == (p == 3 ? MC_REGISTRY_READY : MC_REGISTRY_COLD));
        printf("JQ_PREFIX_AFTER %zu %08" PRIx32 "\n", p, (uint32_t)value);
    }
    const size_t ready_bytes = f.registry->allocation_bytes;
    size_t index = 0;
    for (int id_index = 0; id_index < 2; ++id_index) {
        const int32_t id = id_index == 0 ? 29 : 33;
        mc_block *b = block(&f, id);
        const int32_t original = b->fields.bm;
        for (int scene = 0; scene < 13; ++scene)
            for (int bm = 0; bm < 3; ++bm) {
                shape(b, scene);
                b->fields.bm = bm == 0 ? original : bm == 1 ? -3 : 197;
                printf("JQ_INPUT\t%" PRId32 "\t%d\t%d\t%08" PRIx32 "\t", id, scene, bm,
                       (uint32_t)b->fields.bm);
                bounds_row(b);
                putchar('\n');
                for (size_t m = 0; m < sizeof(metadata_values) / sizeof(*metadata_values); ++m)
                    for (size_t s = 0; s < sizeof(sides) / sizeof(*sides); ++s)
                        printf("JQ_TEX\t%zu\t%" PRId32 "\t%d\t%d\t%" PRId32 "\t%" PRId32
                               "\t%08" PRIx32 "\t1\n",
                               index++, id, scene, bm, sides[s], metadata_values[m],
                               (uint32_t)texture(b, sides[s], metadata_values[m]));
            }
        b->fields.bm = original;
        for (int direction = 0; direction < 6; ++direction)
            for (int extended = 0; extended < 2; ++extended)
                for (size_t changed = 0; changed < sizeof(mutations) / sizeof(*mutations);
                     ++changed) {
                    int32_t *array = f.registry->direction_a;
                    const int32_t saved = array[direction];
                    array[direction] = mutations[changed];
                    shape(b, 0);
                    for (int probe = 0; probe < 2; ++probe) {
                        const int32_t side = probe == 0 ? mutations[changed]
                                                        : mc_java_i32_add(mutations[changed], 1);
                        const int32_t value = texture(b, side, direction | (extended << 3));
                        printf("JQ_MUTATE\t%" PRId32 "\t%d\t%d\t%" PRId32 "\t%d\t%" PRId32
                               "\t%08" PRIx32 "\n",
                               id, direction, extended, mutations[changed], probe, side,
                               (uint32_t)value);
                    }
                    array[direction] = saved;
                }
        shape(b, 1);
        CHECK(mc_block_render_inventory_shape(b) == MC_REGISTRY_OK);
        printf("JQ_INVENTORY_BOUNDS\t%" PRId32 "\t", id);
        bounds_row(b);
        putchar('\n');
        for (int32_t side = 0; side < 6; ++side)
            printf("JQ_INVENTORY\t%" PRId32 "\t%" PRId32 "\t%08" PRIx32 "\n", id, side,
                   (uint32_t)texture(b, side, 1));
    }
    CHECK(index == 34320 && f.registry->allocation_bytes == ready_bytes);
    native_failures(&f, base_bytes);
    close_fixture(&f);
    printf("PASS: 34320 original PistonBase textures, %u native checks\n", checks);
    return 0;
}
