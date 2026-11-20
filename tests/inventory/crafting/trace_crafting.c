#include "mc/inventory/container.h"
#include "mc/item/recipes/matching.h"
#include "mc/resources/files/files.h"
#include "mc/statistics/registry/registry.h"

#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REQUIRE(value)                                                                             \
    do {                                                                                           \
        if (!(value)) {                                                                            \
            fprintf(stderr, "trace:%d: %s\n", __LINE__, #value);                                   \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

static const char *boolean(bool value) {
    return value ? "true" : "false";
}

static const char *exception(mc_inventory_status status) {
    switch (status) {
        case MC_INVENTORY_OK:
            return "OK";
        case MC_INVENTORY_NULL_REFERENCE:
            return "NullPointerException";
        case MC_INVENTORY_INDEX_OUT_OF_BOUNDS:
            return "ArrayIndexOutOfBoundsException";
        case MC_INVENTORY_NEGATIVE_ARRAY_SIZE:
            return "NegativeArraySizeException";
        default:
            return "NATIVE_FAILURE";
    }
}

static mc_item_stack *new_stack(int32_t id, int32_t count, int32_t damage) {
    mc_item_stack *value = NULL;
    if (mc_item_stack_allocate(&value) != MC_ITEM_STACK_OK)
        return NULL;
    if (mc_item_stack_construct(value, id, count, damage) != MC_ITEM_STACK_OK) {
        mc_item_stack_release(value);
        return NULL;
    }
    return value;
}

static bool print_stack(const mc_item_stack *stack) {
    if (stack == NULL)
        return printf("-") >= 0;
    mc_item_stack_fields f;
    return mc_item_stack_fields_get(stack, &f) == MC_ITEM_STACK_OK &&
           printf("%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId32, f.id, f.count, f.damage,
                  f.animation) >= 0;
}

static bool inventory_trace(mc_registry_side side) {
    bool ok = false;
    mc_inventory_crafting *grid = NULL, *negative = NULL;
    mc_inventory_result *result = NULL;
    mc_inventory_player *player = NULL;
    mc_item_stack *input = new_stack(17, 10, 2), *removed = NULL, *split = NULL;
    const mc_item_stack *borrowed = NULL, *a = NULL, *b = NULL;
    mc_inventory_status status;
    int32_t size, limit;
    const char *name;
    REQUIRE(input != NULL &&
            mc_inventory_crafting_create(side, NULL, 2, 2, NULL, &grid, NULL) == MC_INVENTORY_OK);
    REQUIRE(mc_inventory_crafting_size(grid, &size, NULL) == MC_INVENTORY_OK &&
            mc_inventory_crafting_name(grid, &name, NULL) == MC_INVENTORY_OK &&
            mc_inventory_crafting_limit(grid, &limit, NULL) == MC_INVENTORY_OK);
    printf("I\tgrid\t%d\t%s\t%d\n", (int)size, name, (int)limit);
    status = mc_inventory_crafting_set(grid, 0, input, NULL);
    REQUIRE(mc_inventory_crafting_borrow_slot(grid, 0, &borrowed, NULL) == MC_INVENTORY_OK);
    printf("I\tset\t%s\t%s\t", exception(status), boolean(borrowed == input));
    REQUIRE(print_stack(input));
    putchar('\n');
    REQUIRE(mc_inventory_crafting_borrow_slot(grid, 4, &borrowed, NULL) == MC_INVENTORY_OK &&
            mc_inventory_crafting_borrow_xy(grid, 2, 0, &a, NULL) == MC_INVENTORY_OK &&
            mc_inventory_crafting_borrow_xy(grid, 0, 2, &b, NULL) == MC_INVENTORY_OK);
    printf("I\thigh\t");
    REQUIRE(print_stack(borrowed));
    putchar('\t');
    REQUIRE(print_stack(a));
    putchar('\t');
    REQUIRE(print_stack(b));
    putchar('\n');
    status = mc_inventory_crafting_borrow_xy(grid, 0, -1, &borrowed, NULL);
    REQUIRE(mc_inventory_crafting_borrow_xy(grid, 0, INT32_MIN, &borrowed, NULL) ==
            MC_INVENTORY_OK);
    printf("I\tnegative\t%s\t%s\n", exception(status), boolean(borrowed == input));
    status = mc_inventory_crafting_split(grid, 0, -3, &removed, NULL);
    REQUIRE(mc_inventory_crafting_split(grid, 1, 1, &split, NULL) == MC_INVENTORY_OK);
    printf("I\tsplit-negative\t%s\t", exception(status));
    REQUIRE(print_stack(input));
    putchar('\t');
    REQUIRE(print_stack(split));
    putchar('\n');
    REQUIRE(mc_inventory_result_create(side, NULL, &result, NULL) == MC_INVENTORY_OK &&
            mc_inventory_result_set(result, 0, input, NULL) == MC_INVENTORY_OK &&
            mc_inventory_result_split(result, 0, 0, &removed, NULL) == MC_INVENTORY_OK);
    REQUIRE(mc_inventory_result_size(result, &size, NULL) == MC_INVENTORY_OK &&
            mc_inventory_result_name(result, &name, NULL) == MC_INVENTORY_OK &&
            mc_inventory_result_limit(result, &limit, NULL) == MC_INVENTORY_OK);
    status = mc_inventory_result_borrow(result, 1, &borrowed, NULL);
    printf("I\tresult\t%d\t%s\t%d\t%s\t", (int)size, name, (int)limit, boolean(removed == input));
    REQUIRE(print_stack(removed));
    printf("\t%s\n", exception(status));
    mc_item_stack_release(removed);
    removed = NULL;
    REQUIRE(mc_inventory_player_create(side, NULL, NULL, &player, NULL) == MC_INVENTORY_OK);
    mc_inventory_player_state state;
    REQUIRE(mc_inventory_player_read_state(player, &state));
    printf("I\tplayer\t36\t4\t%d\t%s\t%s\t%s\n", (int)state.selected, boolean(state.changed),
           boolean(state.cursor == NULL), boolean(state.player == NULL));
    REQUIRE(mc_inventory_player_set(player, 39, input, NULL) == MC_INVENTORY_OK &&
            mc_inventory_player_split(player, 39, 4, &split, NULL) == MC_INVENTORY_OK &&
            mc_inventory_player_changed(player, NULL) == MC_INVENTORY_OK &&
            mc_inventory_player_read_state(player, &state));
    status = mc_inventory_player_borrow(player, 40, &borrowed, NULL);
    printf("I\tplayer-split\t");
    REQUIRE(print_stack(input));
    putchar('\t');
    REQUIRE(print_stack(split));
    printf("\t%s\t%s\n", boolean(state.changed), exception(status));
    status = mc_inventory_crafting_create(side, NULL, INT32_MAX, 2, NULL, &negative, NULL);
    printf("I\tnegative-array\t%s\n", exception(status));
    ok = true;
cleanup:
    mc_item_stack_release(input);
    mc_item_stack_release(removed);
    mc_item_stack_release(split);
    mc_inventory_crafting_retire(&grid, NULL);
    mc_inventory_crafting_retire(&negative, NULL);
    mc_inventory_result_retire(&result, NULL);
    mc_inventory_player_retire(&player, NULL);
    return ok;
}

static bool graph(const char *name, mc_container *container) {
    bool ok = false;
    mc_container_state state;
    REQUIRE(mc_container_read_state(container, &state));
    const bool distinct = state.slot_list != state.snapshot_list &&
                          state.snapshot_list != state.listener_list &&
                          state.listener_list != state.blocked_player_set;
    printf("G\t%s\t%zu\t%zu\t%zu\t%zu\t%d\t%d\t%s\n", name, state.slots, state.snapshots,
           state.listeners, state.blocked_players, (int)state.window_id, (int)state.transaction,
           boolean(distinct));
    for (size_t i = 0; i < state.slots; ++i) {
        mc_container_slot_state slot;
        REQUIRE(mc_container_slot_read(container, (int32_t)i, &slot, NULL) == MC_INVENTORY_OK);
        const char *kind = slot.kind == MC_SLOT_CRAFTING ? "crafting"
                           : slot.kind == MC_SLOT_ARMOR  ? "armor"
                                                         : "normal";
        const char *inventory = slot.inventory_kind == MC_SLOT_INVENTORY_CRAFTING ? "grid"
                                : slot.inventory_kind == MC_SLOT_INVENTORY_RESULT ? "result"
                                                                                  : "player";
        const bool relation = slot.kind == MC_SLOT_CRAFTING ? slot.crafting_input == state.crafting
                              : slot.kind == MC_SLOT_ARMOR  ? slot.armor_container == container
                                                            : true;
        printf("S\t%s\t%d\t%s\t%s\t%d\t%d\t%d\t%d\t%s\n", name, (int)slot.container_index, kind,
               inventory, (int)slot.inventory_index, (int)slot.x, (int)slot.y, (int)slot.armor_type,
               boolean(relation));
    }
    ok = true;
cleanup:
    return ok;
}

static size_t columns(char *text, char delimiter, char **out, size_t capacity) {
    size_t count = 1;
    if (capacity == 0)
        return 0;
    out[0] = text;
    for (char *p = text; *p != '\0'; ++p)
        if (*p == delimiter) {
            if (count == capacity)
                return 0;
            *p = '\0';
            out[count++] = p + 1;
        }
    return count;
}

static bool integer(const char *text, int32_t *out) {
    char *end = NULL;
    const long long v = strtoll(text, &end, 10);
    if (end == text || *end != '\0' || v < INT32_MIN || v > INT32_MAX)
        return false;
    *out = (int32_t)v;
    return true;
}

static bool fresh(const mc_crafting_manager *manager, const mc_item_stack *output) {
    if (output == NULL)
        return true;
    for (size_t i = 0; i < mc_crafting_manager_size(manager); ++i) {
        const mc_recipe *recipe = NULL;
        if (mc_crafting_manager_get(manager, i, &recipe) != MC_REGISTRY_OK ||
            mc_recipe_output(recipe) == output)
            return false;
    }
    return true;
}

static bool trace_case(mc_registry_side side, const mc_crafting_manager *manager,
                       mc_container *player, mc_container *workbench, char *line) {
    bool ok = false;
    char *parts[12];
    const size_t count = columns(line, '\t', parts, 12u);
    int32_t width, index;
    mc_item_stack *aliases[10] = {0}, *direct = NULL, *found = NULL, *preview = NULL;
    const mc_item_stack *inputs[9] = {0}, *repeated = NULL;
    mc_item_stack_fields before[9] = {{0}};
    REQUIRE(count >= 3u && integer(parts[1], &width) && integer(parts[2], &index) &&
            (width == 2 || width == 3) && index >= 0);
    REQUIRE(count == (size_t)(3 + width * width));
    mc_container_state state;
    REQUIRE(mc_container_read_state(width == 2 ? player : workbench, &state));
    for (int32_t i = 0; i < width * width; ++i)
        REQUIRE(mc_inventory_crafting_set(state.crafting, i, NULL, NULL) == MC_INVENTORY_OK);
    for (int32_t i = 0; i < width * width; ++i) {
        if (strcmp(parts[i + 3], "-") == 0)
            continue;
        char *values[4];
        int32_t id, amount, damage, group;
        REQUIRE(columns(parts[i + 3], ',', values, 4u) == 4u && integer(values[0], &id) &&
                integer(values[1], &amount) && integer(values[2], &damage) &&
                integer(values[3], &group) && group >= 1 && group <= 9);
        if (aliases[group] == NULL)
            aliases[group] = new_stack(id, amount, damage);
        REQUIRE(aliases[group] != NULL);
        inputs[i] = aliases[group];
        REQUIRE(mc_item_stack_fields_get(inputs[i], &before[i]) == MC_ITEM_STACK_OK &&
                before[i].id == id && before[i].count == amount && before[i].damage == damage);
        REQUIRE(mc_inventory_crafting_set(state.crafting, i, aliases[group], NULL) ==
                MC_INVENTORY_OK);
    }
    const mc_recipe *recipe = NULL;
    bool matches;
    REQUIRE(mc_crafting_manager_get(manager, (size_t)index, &recipe) == MC_REGISTRY_OK);
    REQUIRE(mc_recipe_matches(side, recipe, state.crafting, &matches, NULL) == MC_CRAFTING_OK &&
            mc_recipe_craft_result(side, recipe, state.crafting, &direct, NULL) == MC_CRAFTING_OK &&
            mc_crafting_manager_find_matching(side, manager, state.crafting, &found, NULL) ==
                MC_CRAFTING_OK &&
            mc_inventory_result_get(state.result, 0, &preview, NULL) == MC_INVENTORY_OK);
    const mc_item_stack *last = NULL;
    REQUIRE(mc_inventory_crafting_borrow_slot(state.crafting, width * width - 1, &last, NULL) ==
                MC_INVENTORY_OK &&
            mc_inventory_crafting_set(state.crafting, width * width - 1, (mc_item_stack *)last,
                                      NULL) == MC_INVENTORY_OK &&
            mc_inventory_result_borrow(state.result, 0, &repeated, NULL) == MC_INVENTORY_OK);
    bool unchanged = true;
    for (int32_t i = 0; i < width * width; ++i)
        if (inputs[i] != NULL) {
            mc_item_stack_fields after;
            REQUIRE(mc_item_stack_fields_get(inputs[i], &after) == MC_ITEM_STACK_OK);
            if (after.id != before[i].id || after.count != before[i].count ||
                after.damage != before[i].damage || after.animation != before[i].animation)
                unchanged = false;
        }
    printf("C\t%s\t%d\t%d\t%s\t", parts[0], (int)width, (int)index, boolean(matches));
    REQUIRE(print_stack(direct));
    putchar('\t');
    REQUIRE(print_stack(found));
    putchar('\t');
    REQUIRE(print_stack(preview));
    printf("\t%s\t%s\t%s\t%s\t%s\n", boolean(fresh(manager, direct)),
           boolean(fresh(manager, found)),
           boolean(preview == NULL || (preview != direct && preview != found)),
           boolean(preview == NULL ? repeated == NULL : repeated != preview), boolean(unchanged));
    ok = true;
cleanup:
    for (size_t i = 0; i < 10u; ++i)
        mc_item_stack_release(aliases[i]);
    mc_item_stack_release(direct);
    mc_item_stack_release(found);
    mc_item_stack_release(preview);
    return ok;
}

static bool read_plan_line(FILE *input, char *line, size_t capacity, bool *present) {
    size_t length = 0;
    *present = false;
    for (;;) {
        const int byte = fgetc(input);
        if (byte == EOF) {
            if (ferror(input))
                return false;
            if (length == 0)
                return true;
            break;
        }
        if (byte == '\n')
            break;
        if (byte == '\r') {
            if (fgetc(input) != '\n')
                return false;
            break;
        }
        if ((byte != '\t' && (byte < 32 || byte > 126)) || length + 1u >= capacity)
            return false;
        line[length++] = (char)byte;
    }
    if (length == 0)
        return false;
    line[length] = '\0';
    *present = true;
    return true;
}

int main(int argc, char **argv) {
    int result = 1;
    mc_resource_files *files = NULL;
    mc_materials *materials = NULL;
    mc_java_random_profile *profile = NULL;
    mc_registry_classes *classes = NULL;
    mc_inventory_player *inventory = NULL;
    mc_container *player = NULL, *workbench = NULL;
    FILE *input = NULL;
    REQUIRE(argc == 4 && (strcmp(argv[1], "client") == 0 || strcmp(argv[1], "server") == 0));
    const mc_registry_side side =
        strcmp(argv[1], "client") == 0 ? MC_REGISTRY_CLIENT : MC_REGISTRY_SERVER;
    REQUIRE(mc_resource_files_create(argv[2], &files) == MC_REGISTRY_OK &&
            mc_materials_create(&materials) &&
            mc_java_random_profile_create(SIZE_MAX, &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    const mc_registry_options options = {side, profile, materials, 0,
                                         mc_resource_files_source(files)};
    REQUIRE(mc_registry_classes_allocate(&options, &classes, NULL) == MC_REGISTRY_OK &&
            mc_registry_classes_initialize_statistics(classes, NULL) == MC_REGISTRY_OK);
    mc_registry_crafting_view view;
    REQUIRE(mc_registry_classes_inspect_crafting(classes, &view, NULL) == MC_REGISTRY_OK &&
            mc_crafting_manager_size(view.a) == 151);
    printf("CRAFTING_TRACE_V1\t%s\n", argv[1]);
    REQUIRE(inventory_trace(side));
    REQUIRE(mc_inventory_player_create(side, NULL, NULL, &inventory, NULL) == MC_INVENTORY_OK &&
            mc_container_player_create(side, classes, inventory, true, NULL, &player, NULL) ==
                MC_INVENTORY_OK &&
            mc_container_workbench_create(side, classes, inventory, NULL, 5, -3, 9, NULL,
                                          &workbench, NULL) == MC_INVENTORY_OK);
    REQUIRE(graph("player", player) && graph("workbench", workbench));
#ifdef _WIN32
    REQUIRE(fopen_s(&input, argv[3], "rb") == 0);
#else
    input = fopen(argv[3], "rb");
#endif
    REQUIRE(input != NULL);
    char line[1024];
    for (;;) {
        bool present;
        REQUIRE(read_plan_line(input, line, sizeof(line), &present));
        if (!present)
            break;
        REQUIRE(trace_case(side, view.a, player, workbench, line));
    }
    REQUIRE(!ferror(input));
    printf("END\n");
    REQUIRE(fflush(stdout) == 0 && !ferror(stdout));
    result = 0;
cleanup:
    if (input != NULL && fclose(input) != 0)
        result = 1;
    mc_container_retire(&player, NULL);
    mc_container_retire(&workbench, NULL);
    mc_inventory_player_retire(&inventory, NULL);
    if (mc_registry_classes_free(classes, NULL) != MC_REGISTRY_OK)
        return 1;
    if (files != NULL && mc_resource_files_free(files) != MC_REGISTRY_OK)
        result = 1;
    mc_materials_destroy(materials);
    mc_java_random_profile_free(profile);
    return result;
}
