/* Real resource-backed Registry initialization; the imported controls are not
 * executed by this scalar observer. */
#define main mc_appearance_registry_fixture_main
#include "../registry/test_runtime.c"
#undef main

#include "../../../src/client/render_engine/colorizer.h"
#include "../../../src/item/render/internal.h"

#include <inttypes.h>
#include <stdlib.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

static const int32_t appearance_operands[] = {
    INT32_MIN, INT32_MIN + 1, -65536, -17,           -16,      -1, 0, 1, 2, 3, 4, 7, 8, 15, 16,
    17,        255,           65535,  INT32_MAX - 1, INT32_MAX};
static const int32_t appearance_bm_operands[] = {
    INT32_MIN, -81, -80, -1, 0, INT32_C(2147483567), INT32_C(2147483568), INT32_MAX};

enum { APPEARANCE_ITEMS = 202, APPEARANCE_CLASSES = 38, APPEARANCE_BM_ROWS = 64 };

/* Allocated receiver identity, independent of mutable constructor diagnostics. */
static const char *const appearance_runtime_classes[MC_ITEM_KIND_COUNT] = {
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

typedef struct appearance_values {
    int32_t direct, wrapped, color;
    bool full_3d, rotate;
} appearance_values;

typedef struct appearance_item {
    const mc_item *item;
    int32_t id, bh;
    const char *runtime_class;
    char owners[4][16];
    appearance_values rows[COUNT(appearance_operands)];
} appearance_item;

typedef struct appearance_bm_row {
    int32_t id, bm, damage, direct, wrapped;
} appearance_bm_row;

static bool appearance_owner(const mc_item_render_error *error, char owner[16], bool first) {
    if (error->site.class_name == NULL)
        return false;
    const size_t length = strlen(error->site.class_name);
    if (length == 0 || length >= 16)
        return false;
    if (first)
        memcpy(owner, error->site.class_name, length + 1);
    return strcmp(owner, error->site.class_name) == 0;
}

static bool appearance_observe(mc_registry_classes *registry,
                               const mc_item_render_color_context *context, appearance_item *entry,
                               size_t operand) {
    bool result = false;
    mc_item_stack *stack = NULL;
    mc_item_render_error error = {0};
    appearance_values *row = &entry->rows[operand];
    const int32_t damage = appearance_operands[operand];
    const bool first = operand == 0;
    CHECK(mc_item_render_icon(registry, entry->item, damage, &row->direct, &error) &&
          error.returned && appearance_owner(&error, entry->owners[0], first));
    CHECK(mc_item_stack_allocate(&stack) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(stack, entry->id, 2, damage) == MC_ITEM_STACK_OK);
    CHECK(mc_item_stack_render_icon(registry, stack, &row->wrapped, &error) && error.returned);
    CHECK(mc_item_render_color_with_context(registry, entry->item, damage, context, &row->color,
                                            &error) &&
          error.returned && appearance_owner(&error, entry->owners[1], first));
    CHECK(error.colorizer_entered == (entry->item->kind == MC_ITEM_LEAVES_BLOCK) &&
          error.colorizer_returned == error.colorizer_entered);
    CHECK(mc_item_render_full_3d(registry, entry->item, &row->full_3d, &error) && error.returned &&
          appearance_owner(&error, entry->owners[2], first));
    CHECK(mc_item_render_rotate(registry, entry->item, &row->rotate, &error) && error.returned &&
          appearance_owner(&error, entry->owners[3], first));
    mc_item_stack_fields fields;
    CHECK(mc_item_stack_fields_get(stack, &fields) == MC_ITEM_STACK_OK && fields.id == entry->id &&
          fields.count == 2 && fields.damage == damage && fields.animation == 0);
    result = true;
cleanup:
    mc_item_stack_release(stack);
    if (!result)
        fprintf(stderr,
                "appearance id=%" PRId32 " damage=%" PRId32 " class=%s pc=%" PRId32 " status=%d\n",
                entry->id, damage, error.site.class_name == NULL ? "-" : error.site.class_name,
                error.site.pc, (int)error.status);
    return result;
}

static bool appearance_bm_observe(mc_registry_classes *registry, appearance_bm_row *row) {
    bool result = false;
    mc_item_stack *stack = NULL;
    const mc_block *captured = NULL;
    const mc_item *item = NULL;
    mc_registry_error registry_error = poison_error();
    mc_item_render_error direct_error = {0}, wrapped_error = {0};
    CHECK(mc_registry_block_static_get(registry, row->id == 6 ? "z" : "L", &captured,
                                       &registry_error) == MC_REGISTRY_OK &&
          captured != NULL);
    CHECK(mc_registry_item_c_at(registry, registry->items.c, row->id, &item, &registry_error) ==
              MC_REGISTRY_OK &&
          item != NULL);
    mc_block *block = (mc_block *)(void *)captured;
    const int32_t saved = block->fields.bm;
    block->fields.bm = row->bm;
    const bool direct =
        mc_item_render_icon(registry, item, row->damage, &row->direct, &direct_error);
    const bool constructed =
        direct && mc_item_stack_allocate(&stack) == MC_ITEM_STACK_OK &&
        mc_item_stack_construct(stack, row->id, 2, row->damage) == MC_ITEM_STACK_OK;
    const bool wrapped =
        constructed && mc_item_stack_render_icon(registry, stack, &row->wrapped, &wrapped_error);
    /* The declared instance input is restored on success AND failure before
     * any assertion transfers control out of the mutation interval. */
    block->fields.bm = saved;
    CHECK(block->fields.bm == saved && direct && direct_error.returned && constructed && wrapped &&
          wrapped_error.returned);
    result = true;
cleanup:
    mc_item_stack_release(stack);
    if (!result)
        fprintf(stderr,
                "appearance bm id=%" PRId32 " bm=%" PRId32 " damage=%" PRId32
                " direct-status=%d wrapped-status=%d\n",
                row->id, row->bm, row->damage, (int)direct_error.status, (int)wrapped_error.status);
    return result;
}

int main(void) {
    int result = 1;
    mc_java_random_profile *profile = NULL;
    mc_materials *materials = NULL;
    mc_client_render_engine_classes *graphics = NULL;
    const mc_item **slots = NULL;
    fixture f = {0};
    appearance_item observed[APPEARANCE_ITEMS] = {0};
    appearance_bm_row bm_rows[APPEARANCE_BM_ROWS] = {0};
    const char *class_names[APPEARANCE_CLASSES] = {0};
    bool seen_kinds[MC_ITEM_KIND_COUNT] = {0};
    size_t item_count = 0, class_count = 0, bm_count = 0;
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
          captured != NULL && captured->length == 32000 && captured->elements != NULL);
    slots = calloc(captured->length, sizeof(*slots));
    CHECK(slots != NULL);
    for (int32_t id = 0; id < (int32_t)captured->length; ++id) {
        const mc_item *item = NULL;
        CHECK(mc_registry_item_c_at(f.owner, captured, id, &item, &error) == MC_REGISTRY_OK);
        slots[id] = item;
        if (item == NULL)
            continue;
        CHECK(item_count < COUNT(observed) && item->classes == f.owner &&
              item->phase == MC_ITEM_CONSTRUCTED && item->fields.bf == id &&
              (unsigned int)item->kind < MC_ITEM_KIND_COUNT &&
              appearance_runtime_classes[item->kind] != NULL);
        appearance_item *entry = &observed[item_count++];
        entry->item = item;
        entry->id = id;
        entry->bh = item->fields.bh;
        entry->runtime_class = appearance_runtime_classes[item->kind];
        seen_kinds[item->kind] = true;
        size_t class_index = 0;
        while (class_index < class_count &&
               strcmp(class_names[class_index], entry->runtime_class) != 0)
            ++class_index;
        if (class_index == class_count) {
            CHECK(class_count < COUNT(class_names));
            class_names[class_count++] = entry->runtime_class;
        }
    }
    CHECK(item_count == APPEARANCE_ITEMS && class_count == APPEARANCE_CLASSES);
    for (size_t kind = 0; kind < COUNT(seen_kinds); ++kind)
        CHECK(seen_kinds[kind]);
    CHECK(mc_client_render_engine_classes_create(NULL, &graphics) == MC_CLIENT_RENDER_ENGINE_OK);
    mc_client_colorizer_state foliage;
    CHECK(mc_client_colorizer_inspect(graphics, MC_CLIENT_COLORIZER_FOLIAGE, &foliage) &&
          foliage.phase == MC_CLIENT_COLORIZER_RAW && foliage.a == NULL);
    const mc_item_render_color_context context = {graphics, mc_client_colorizer_read};
    const uint64_t ready_random = uniquifier(profile);
    const size_t registry_bytes = f.owner->allocation_bytes;
    for (size_t i = 0; i < item_count; ++i)
        for (size_t operand = 0; operand < COUNT(appearance_operands); ++operand)
            CHECK(appearance_observe(f.owner, &context, &observed[i], operand));
    const mc_block *sapling = NULL, *leaves = NULL;
    CHECK(mc_registry_block_static_get(f.owner, "z", &sapling, &error) == MC_REGISTRY_OK &&
          sapling != NULL);
    CHECK(mc_registry_block_static_get(f.owner, "L", &leaves, &error) == MC_REGISTRY_OK &&
          leaves != NULL);
    const int32_t initial_sapling = sapling->fields.bm, initial_leaves = leaves->fields.bm;
    static const int32_t bm_ids[] = {6, 18};
    for (size_t id = 0; id < COUNT(bm_ids); ++id)
        for (size_t bm = 0; bm < COUNT(appearance_bm_operands); ++bm)
            for (int32_t damage = 0; damage < 4; ++damage) {
                CHECK(bm_count < COUNT(bm_rows));
                appearance_bm_row *row = &bm_rows[bm_count++];
                *row = (appearance_bm_row){
                    .id = bm_ids[id], .bm = appearance_bm_operands[bm], .damage = damage};
                CHECK(appearance_bm_observe(f.owner, row));
            }
    CHECK(bm_count == APPEARANCE_BM_ROWS && sapling->fields.bm == initial_sapling &&
          leaves->fields.bm == initial_leaves);
    CHECK(f.owner->items.c == captured);
    for (size_t id = 0; id < captured->length; ++id)
        CHECK(captured->elements[id] == slots[id]);
    for (size_t i = 0; i < item_count; ++i)
        CHECK(observed[i].item->fields.bh == observed[i].bh);
    CHECK(mc_client_colorizer_inspect(graphics, MC_CLIENT_COLORIZER_FOLIAGE, &foliage) &&
          foliage.phase == MC_CLIENT_COLORIZER_READY && foliage.a != NULL &&
          foliage.failed_candidate == NULL && foliage.last_store_pc == 4);
    mc_client_color_array_state color_array;
    CHECK(mc_client_color_array_inspect(foliage.a, &color_array) &&
          color_array.phase == MC_CLIENT_COLOR_ARRAY_READY && color_array.length == 65536);
    CHECK(!f.bad && f.opens == 3 && f.closes == 1 && uniquifier(profile) == ready_random &&
          f.owner->allocation_bytes == registry_bytes && f.owner->entered == 0 &&
          f.owner->initializer == NULL);
    CHECK(mc_client_render_engine_classes_release(graphics) == MC_CLIENT_RENDER_ENGINE_OK);
    graphics = NULL;
    CHECK(retire(&f));
    mc_materials_destroy(materials);
    materials = NULL;
    mc_java_random_profile_free(profile);
    profile = NULL;

    for (size_t i = 0; i < item_count; ++i) {
        const appearance_item *entry = &observed[i];
        CHECK(printf("CLASS\t%" PRId32 "\t%s\t%s\t%s\t%s\t%s\t%" PRId32 "\n", entry->id,
                     entry->runtime_class, entry->owners[0], entry->owners[1], entry->owners[2],
                     entry->owners[3], entry->bh) > 0);
    }
    for (size_t i = 0; i < item_count; ++i)
        for (size_t operand = 0; operand < COUNT(appearance_operands); ++operand) {
            const appearance_values *row = &observed[i].rows[operand];
            CHECK(printf("APPEARANCE\t%" PRId32 "\t%" PRId32 "\t%" PRId32 "\t%" PRId32 "\t%" PRId32
                         "\t%d\t%d\n",
                         observed[i].id, appearance_operands[operand], row->direct, row->wrapped,
                         row->color, (int)row->full_3d, (int)row->rotate) > 0);
        }
    for (size_t i = 0; i < bm_count; ++i) {
        const appearance_bm_row *row = &bm_rows[i];
        CHECK(printf("BM\t%" PRId32 "\t%" PRId32 "\t%" PRId32 "\t%" PRId32 "\t%" PRId32 "\n",
                     row->id, row->bm, row->damage, row->direct, row->wrapped) > 0);
    }
    CHECK(printf("COMPLETE\t%zu\t%zu\t%zu\t%zu\n", item_count, class_count,
                 item_count * COUNT(appearance_operands), bm_count) > 0);
    CHECK(fflush(stdout) == 0 && !ferror(stdout));
    result = 0;
cleanup:
    free(slots);
    if (graphics != NULL &&
        mc_client_render_engine_classes_release(graphics) != MC_CLIENT_RENDER_ENGINE_OK)
        result = 1;
    if (!retirement_failed && f.owner != NULL && !retire(&f))
        result = 1;
    if (!retirement_failed) {
        mc_materials_destroy(materials);
        mc_java_random_profile_free(profile);
    }
    return result;
}
