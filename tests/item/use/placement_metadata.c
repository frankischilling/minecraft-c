/* Reuse real Registry resource streams and successful initialization helpers.
 * The imported control executable is never run by this scalar observer. */
#define main mc_placement_metadata_registry_fixture_main
#include "../registry/test_runtime.c"
#undef main

#include "../../../src/item/use/metadata_internal.h"

#include <inttypes.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

static const int32_t metadata_operands[] = {
    INT32_MIN, INT32_MIN + 1, -65536, -17,           -16,      -1, 0, 1, 2, 3, 4, 7, 8, 15, 16,
    17,        255,           65535,  INT32_MAX - 1, INT32_MAX};

enum { METADATA_ITEMS = 202, METADATA_CLASSES = 38 };

/* kind is the allocated receiver identity. constructor_class records the
 * latest construction/mutator stage and can name an ancestor after CONTAINER. */
static const char *const metadata_runtime_classes[MC_ITEM_KIND_COUNT] = {
    [MC_ITEM_BASE] = "gm",
    [MC_ITEM_SHOVEL] = "wc",
    [MC_ITEM_PICKAXE] = "au",
    [MC_ITEM_AXE] = "ta",
    [MC_ITEM_FLINT_AND_STEEL] = "ye",
    [MC_ITEM_FOOD] = "yw",
    [MC_ITEM_BOW] = "qz",
    [MC_ITEM_COAL] = "rx",
    [MC_ITEM_SWORD] = "qd",
    [MC_ITEM_HOE] = "la",
    [MC_ITEM_SEEDS] = "rk",
    [MC_ITEM_ARMOR] = "wa",
    [MC_ITEM_PAINTING] = "ym",
    [MC_ITEM_SIGN] = "vc",
    [MC_ITEM_DOOR] = "hu",
    [MC_ITEM_BUCKET] = "bc",
    [MC_ITEM_MINECART] = "rl",
    [MC_ITEM_SADDLE] = "sa",
    [MC_ITEM_REDSTONE] = "ie",
    [MC_ITEM_SNOWBALL] = "dm",
    [MC_ITEM_BOAT] = "vd",
    [MC_ITEM_BLOCK_PLACER] = "lv",
    [MC_ITEM_EGG] = "mo",
    [MC_ITEM_FISHING_ROD] = "cs",
    [MC_ITEM_DYE] = "km",
    [MC_ITEM_BED] = "lp",
    [MC_ITEM_SHEARS] = "bl",
    [MC_ITEM_RECORD] = "tr",
    [MC_ITEM_SOUP] = "bz",
    [MC_ITEM_COOKIE] = "qk",
    [MC_ITEM_MAP] = "wr",
    [MC_ITEM_BLOCK] = "ck",
    [MC_ITEM_WOOL_BLOCK] = "bi",
    [MC_ITEM_LOG_BLOCK] = "go",
    [MC_ITEM_SLAB_BLOCK] = "en",
    [MC_ITEM_SAPLING_BLOCK] = "mr",
    [MC_ITEM_LEAVES_BLOCK] = "uy",
    [MC_ITEM_PISTON_BLOCK] = "qr"};

typedef struct metadata_observation {
    int32_t id;
    char runtime_class[16];
    int32_t values[COUNT(metadata_operands)];
} metadata_observation;

static const char *metadata_owner(const char *runtime_class) {
    static const char *const overrides[] = {"bi", "en", "go", "mr", "qr", "uy"};
    for (size_t i = 0; i < COUNT(overrides); ++i)
        if (strcmp(runtime_class, overrides[i]) == 0)
            return runtime_class;
    return "gm";
}

int main(void) {
    int result = 1;
    mc_java_random_profile *profile = NULL;
    mc_materials *materials = NULL;
    fixture f = {0};
    metadata_observation observed[METADATA_ITEMS] = {0};
    const char *class_names[METADATA_CLASSES] = {0};
    bool seen_kinds[MC_ITEM_KIND_COUNT] = {0};
    size_t item_count = 0, class_count = 0;
    mc_registry_error error = poison_error();
    const mc_registry_item_array *captured = NULL;
#ifdef _WIN32
    CHECK(_setmode(_fileno(stdout), _O_BINARY) != -1);
#endif
    CHECK(mc_java_random_profile_create(SIZE_MAX, &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_materials_create(&materials));
    const mc_registry_options options = {.side = MC_REGISTRY_CLIENT,
                                         .random_profile = profile,
                                         .materials = materials,
                                         .max_allocation_bytes = SIZE_MAX};
    CHECK(allocate_fixture(&f, options) == MC_REGISTRY_OK);
    CHECK(mc_registry_classes_initialize_blocks(f.owner, &error) == MC_REGISTRY_OK);
    CHECK(!f.bad && f.opens == 3 && f.closes == 1 && f.busy_checks == 9 &&
          f.owner->blocks_phase == MC_REGISTRY_READY &&
          f.owner->item_states[MC_ITEM_BASE].phase == MC_REGISTRY_READY);
    CHECK(mc_registry_item_c_get(f.owner, &captured, &error) == MC_REGISTRY_OK &&
          captured != NULL && captured->length == 32000);
    const uint64_t ready_random = uniquifier(profile);
    const size_t registry_bytes = f.owner->allocation_bytes;
    int32_t invalid_output = INT32_C(123456789);
    CHECK(!mc_item_placement_metadata((mc_item_kind)-1, INT32_MIN, &invalid_output) &&
          invalid_output == INT32_C(123456789));
    CHECK(!mc_item_placement_metadata(MC_ITEM_KIND_COUNT, INT32_MAX, &invalid_output) &&
          invalid_output == INT32_C(123456789));
    for (int32_t id = 0; id < (int32_t)captured->length; ++id) {
        const mc_item *item = NULL;
        CHECK(mc_registry_item_c_at(f.owner, captured, id, &item, &error) == MC_REGISTRY_OK);
        if (item == NULL)
            continue;
        CHECK(item_count < COUNT(observed) && item->classes == f.owner &&
              item->phase == MC_ITEM_CONSTRUCTED && item->fields.bf == id &&
              (unsigned int)item->kind < (unsigned int)MC_ITEM_KIND_COUNT &&
              metadata_runtime_classes[item->kind] != NULL);
        metadata_observation *entry = &observed[item_count];
        const char *runtime_class = metadata_runtime_classes[item->kind];
        const size_t length = strlen(runtime_class);
        CHECK(length != 0 && length < sizeof(entry->runtime_class));
        entry->id = id;
        memcpy(entry->runtime_class, runtime_class, length + 1);
        seen_kinds[item->kind] = true;
        size_t class_index = 0;
        while (class_index < class_count &&
               strcmp(class_names[class_index], entry->runtime_class) != 0)
            ++class_index;
        if (class_index == class_count) {
            CHECK(class_count < COUNT(class_names));
            class_names[class_count++] = entry->runtime_class;
        }
        for (size_t operand = 0; operand < COUNT(metadata_operands); ++operand)
            CHECK(mc_item_placement_metadata(item->kind, metadata_operands[operand],
                                             &entry->values[operand]));
        ++item_count;
    }
    CHECK(item_count == METADATA_ITEMS && class_count == METADATA_CLASSES);
    for (size_t kind = 0; kind < COUNT(seen_kinds); ++kind)
        CHECK(seen_kinds[kind]);
    CHECK(!f.bad && f.opens == 3 && f.closes == 1 && uniquifier(profile) == ready_random &&
          f.owner->allocation_bytes == registry_bytes && f.owner->entered == 0 &&
          f.owner->initializer == NULL);
    CHECK(retire(&f));
    mc_materials_destroy(materials);
    materials = NULL;
    mc_java_random_profile_free(profile);
    profile = NULL;

    for (size_t i = 0; i < item_count; ++i)
        CHECK(printf("CLASS\t%" PRId32 "\t%s\t%s\n", observed[i].id, observed[i].runtime_class,
                     metadata_owner(observed[i].runtime_class)) > 0);
    for (size_t i = 0; i < item_count; ++i)
        for (size_t operand = 0; operand < COUNT(metadata_operands); ++operand)
            CHECK(printf("METADATA\t%" PRId32 "\t%" PRId32 "\t%" PRId32 "\n", observed[i].id,
                         metadata_operands[operand], observed[i].values[operand]) > 0);
    CHECK(printf("COMPLETE\t%zu\t%zu\t%zu\n", item_count, class_count,
                 item_count * COUNT(metadata_operands)) > 0);
    CHECK(fflush(stdout) == 0 && !ferror(stdout));
    result = 0;
cleanup:
    if (!retirement_failed && f.owner != NULL && !retire(&f))
        result = 1;
    if (!retirement_failed) {
        mc_materials_destroy(materials);
        mc_java_random_profile_free(profile);
    }
    return result;
}
