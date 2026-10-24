#include "recipes_internal.h"

#include "../../block/registry/registry_internal.h"
#include "../registry/stacks_internal.h"

/* Helper objects and their actual field arrays borrow the common class arena.
 * Fixed native capacities below are the exact private helper array extents,
 * not limits on the generic recipe methods. Transient Object[] operands are
 * synchronous typed spans; no Java allocation/OOME timing is asserted. */
typedef struct family_source {
    mc_registry_classes *classes;
    mc_crafting_manager *manager;
    const char *name;
    int32_t pc;
} family_source;

typedef struct pattern_row {
    size_t length;
    const mc_registry_string *values[3];
} pattern_row;

typedef struct pattern_array {
    size_t length;
    pattern_row *values[4];
} pattern_array;

typedef struct object_row {
    size_t length;
    mc_recipe_argument values[5];
} object_row;

typedef struct object_array {
    size_t length;
    object_row *values[5];
} object_array;

typedef struct grid_helper {
    family_source source;
    pattern_array *a;
    object_array *b;
} grid_helper;

typedef struct compression_helper {
    family_source source;
    object_array *a;
} compression_helper;

#define TRY(expression)                                                                            \
    do {                                                                                           \
        mc_registry_status family_status = (expression);                                           \
        if (family_status != MC_REGISTRY_OK)                                                       \
            return family_status;                                                                  \
    } while (0)

static void stage(family_source *source, int32_t pc) {
    source->pc = pc;
}

static mc_registry_status own_error(family_source *source) {
    source->classes->last_error.constructor_class = source->name;
    source->classes->last_error.constructor_pc = source->pc;
    return source->classes->last_error.status;
}

static mc_registry_status fail(family_source *source, mc_registry_status status,
                               const char *message) {
    (void)mc_registry_fail(source->classes, status, message, NULL);
    return own_error(source);
}

static family_source context(mc_registry_classes *classes, mc_crafting_manager *manager,
                             const char *client, const char *server) {
    return (family_source){classes, manager,
                           classes->options.side == MC_REGISTRY_CLIENT ? client : server, 0};
}

static void *new_helper(mc_crafting_manager *manager, size_t bytes, int32_t pc) {
    mc_registry_crafting_stage(manager, pc);
    return mc_registry_allocate(manager->classes, bytes);
}

static mc_recipe_argument character(uint16_t value) {
    return (mc_recipe_argument){.kind = MC_RECIPE_ARGUMENT_CHARACTER, .value.character = value};
}

static mc_recipe_argument stack_argument(mc_item_stack *stack) {
    return (mc_recipe_argument){.kind = MC_RECIPE_ARGUMENT_STACK, .value.stack = stack};
}

static mc_registry_status literal(family_source *source, const char *text, int32_t pc,
                                  mc_recipe_argument *out) {
    stage(source, pc);
    const mc_registry_string *string = NULL;
    mc_registry_status status = mc_registry_literal_ascii(source->classes, text, &string);
    if (status != MC_REGISTRY_OK)
        return own_error(source);
    *out = (mc_recipe_argument){.kind = MC_RECIPE_ARGUMENT_STRING, .value.string = string};
    return MC_REGISTRY_OK;
}

static mc_registry_status named(family_source *source, bool block, const char *name, int32_t pc,
                                mc_recipe_argument *out) {
    stage(source, pc);
    if (block) {
        mc_block *value = NULL;
        TRY(mc_registry_block_static_load(source->classes, name, &value));
        *out = (mc_recipe_argument){.kind = MC_RECIPE_ARGUMENT_BLOCK, .value.block = value};
    } else {
        mc_item *value = NULL;
        TRY(mc_registry_item_static_load(source->classes, name, &value));
        *out = (mc_recipe_argument){.kind = MC_RECIPE_ARGUMENT_ITEM, .value.item = value};
    }
    return MC_REGISTRY_OK;
}

static mc_registry_status stack_from_value(family_source *source, bool block, int32_t new_pc,
                                           int32_t constructor_pc, mc_recipe_argument value,
                                           int32_t count, int32_t damage, mc_item_stack **out) {
    stage(source, new_pc);
    mc_item_stack *raw = NULL;
    if (mc_registry_stack_allocate(source->classes, &raw) != MC_REGISTRY_OK)
        return own_error(source);
    stage(source, constructor_pc);
    if (block)
        TRY(mc_registry_stack_construct_block_values(source->classes, raw, value.value.block, count,
                                                     damage));
    else
        TRY(mc_registry_stack_construct_item_values(source->classes, raw, value.value.item, count,
                                                    damage));
    *out = raw;
    return MC_REGISTRY_OK;
}

static mc_registry_status stack_named(family_source *source, bool block, const char *name,
                                      int32_t new_pc, int32_t load_pc, int32_t constructor_pc,
                                      int32_t count, int32_t damage, mc_item_stack **out) {
    stage(source, new_pc);
    mc_item_stack *raw = NULL;
    if (mc_registry_stack_allocate(source->classes, &raw) != MC_REGISTRY_OK)
        return own_error(source);
    mc_recipe_argument value;
    TRY(named(source, block, name, load_pc, &value));
    stage(source, constructor_pc);
    if (block)
        TRY(mc_registry_stack_construct_block_values(source->classes, raw, value.value.block, count,
                                                     damage));
    else
        TRY(mc_registry_stack_construct_item_values(source->classes, raw, value.value.item, count,
                                                    damage));
    *out = raw;
    return MC_REGISTRY_OK;
}

static mc_registry_status shaped(family_source *source, mc_item_stack *output,
                                 const mc_recipe_argument *arguments, size_t count, int32_t pc) {
    stage(source, pc);
    return mc_registry_recipe_shaped(source->manager, output, arguments, count);
}

static mc_registry_status shapeless(family_source *source, mc_item_stack *output,
                                    const mc_recipe_argument *arguments, size_t count, int32_t pc) {
    stage(source, pc);
    return mc_registry_recipe_shapeless(source->manager, output, arguments, count);
}

typedef struct pattern_step {
    int32_t allocation_pc;
    size_t length;
    const char *text[3];
    int32_t literal_pc[3];
    int32_t publication_pc;
} pattern_step;

typedef struct grid_specification {
    const char *client, *server;
    size_t patterns;
    pattern_step pattern[4];
    int32_t pattern_publication_pc;
    size_t object_rows;
    int32_t object_allocation_pc, first_row_allocation_pc, object_publication_pc;
    const char *symbols[5][5];
    unsigned int first_row_blocks;
    bool armor;
} grid_specification;

/* These are literal constructor instructions, not material/ID snapshots.
 * Significant trailing spaces are from the pinned binary String constants. */
static const grid_specification tools = {
    .client = "ha",
    .server = "eq",
    .patterns = 4,
    .pattern = {{12, 3, {"XXX", " # ", " # "}, {17, 22, 27}, 30},
                {34, 3, {"X", "#", "#"}, {39, 44, 49}, 52},
                {56, 3, {"XX", "X#", " #"}, {61, 66, 71}, 74},
                {78, 3, {"XX", " #", " #"}, {83, 88, 93}, 96}},
    .pattern_publication_pc = 97,
    .object_rows = 5,
    .object_allocation_pc = 102,
    .first_row_allocation_pc = 108,
    .object_publication_pc = 290,
    .symbols = {{"y", "x", "m", "l", "n"},
                {"r", "v", "e", "z", "G"},
                {"q", "u", "d", "y", "F"},
                {"s", "w", "f", "A", "H"},
                {"L", "M", "N", "O", "P"}},
    .first_row_blocks = 3u};

static const grid_specification weapons = {
    .client = "xc",
    .server = "ot",
    .patterns = 1,
    .pattern = {{12, 3, {"X", "X", "#"}, {17, 22, 27}, 30}},
    .pattern_publication_pc = 31,
    .object_rows = 2,
    .object_allocation_pc = 36,
    .first_row_allocation_pc = 42,
    .object_publication_pc = 113,
    .symbols = {{"y", "x", "m", "l", "n"}, {"p", "t", "o", "x", "E"}},
    .first_row_blocks = 3u};

static const grid_specification armor = {
    .client = "z",
    .server = "n",
    .patterns = 4,
    .pattern = {{12, 2, {"XXX", "X X"}, {17, 22}, 25},
                {29, 3, {"X X", "XXX", "XXX"}, {34, 39, 44}, 47},
                {51, 3, {"XXX", "X X", "X X"}, {56, 61, 66}, 69},
                {73, 2, {"X X", "X X"}, {78, 83}, 86}},
    .pattern_publication_pc = 87,
    .object_rows = 5,
    .object_allocation_pc = 92,
    .first_row_allocation_pc = 98,
    .object_publication_pc = 280,
    .symbols = {{"aD", "as", "m", "l", "n"},
                {"T", "X", "ab", "af", "aj"},
                {"U", "Y", "ac", "ag", "ak"},
                {"V", "Z", "ad", "ah", "al"},
                {"W", "aa", "ae", "ai", "am"}},
    .first_row_blocks = 2u,
    .armor = true};

static mc_registry_status construct_grid(grid_helper *helper, const grid_specification *spec) {
    family_source *source = &helper->source;
    stage(source, 1); /* Actual Object constructor. */
    stage(source, 6);
    pattern_array *patterns = mc_registry_allocate(source->classes, sizeof(*patterns));
    if (patterns == NULL)
        return own_error(source);
    *patterns = (pattern_array){.length = spec->patterns};
    for (size_t i = 0; i < spec->patterns; ++i) {
        const pattern_step *step = &spec->pattern[i];
        stage(source, step->allocation_pc);
        pattern_row *row = mc_registry_allocate(source->classes, sizeof(*row));
        if (row == NULL)
            return own_error(source);
        *row = (pattern_row){.length = step->length};
        for (size_t j = 0; j < step->length; ++j) {
            mc_recipe_argument value;
            TRY(literal(source, step->text[j], step->literal_pc[j], &value));
            row->values[j] = value.value.string;
        }
        stage(source, step->publication_pc);
        patterns->values[i] = row;
    }
    stage(source, spec->pattern_publication_pc);
    helper->a = patterns;
    stage(source, spec->object_allocation_pc);
    object_array *objects = mc_registry_allocate(source->classes, sizeof(*objects));
    if (objects == NULL)
        return own_error(source);
    *objects = (object_array){.length = spec->object_rows};
    for (size_t i = 0; i < spec->object_rows; ++i) {
        const int32_t allocation_pc = spec->first_row_allocation_pc + (int32_t)i * 37;
        stage(source, allocation_pc);
        object_row *row = mc_registry_allocate(source->classes, sizeof(*row));
        if (row == NULL)
            return own_error(source);
        *row = (object_row){.length = 5};
        for (size_t j = 0; j < row->length; ++j) {
            const bool block = i == 0 && (spec->first_row_blocks & (1u << j)) != 0;
            TRY(named(source, block, spec->symbols[i][j], allocation_pc + 5 + (int32_t)j * 6,
                      &row->values[j]));
        }
        stage(source, allocation_pc + 33);
        objects->values[i] = row;
    }
    stage(source, spec->object_publication_pc);
    helper->b = objects;
    return MC_REGISTRY_OK;
}

static mc_registry_status populate_grid(grid_helper *helper, bool is_armor) {
    family_source *source = &helper->source;
    for (size_t column = 0; column < helper->b->values[0]->length; ++column) {
        stage(source, 20);
        const mc_recipe_argument material = helper->b->values[0]->values[column];
        for (size_t row = 0; row < helper->b->length - 1; ++row) {
            stage(source, 48);
            const mc_recipe_argument item = helper->b->values[row + 1]->values[column];
            if (item.kind != MC_RECIPE_ARGUMENT_ITEM)
                return fail(source, MC_REGISTRY_INVALID_ARGUMENT, "Recipe helper Item cast48");
            mc_item_stack *output = NULL;
            TRY(stack_from_value(source, false, 54, 60, item, 1, 0, &output));
            mc_recipe_argument arguments[5] = {{0}};
            stage(source, 75);
            const pattern_row *pattern = helper->a->values[row];
            arguments[0] =
                (mc_recipe_argument){.kind = MC_RECIPE_ARGUMENT_STRINGS,
                                     .value.strings = {pattern->values, pattern->length}};
            if (is_armor) {
                arguments[1] = character('X');
                arguments[2] = material;
                TRY(shaped(source, output, arguments, 3, 89));
            } else {
                arguments[1] = character('#');
                TRY(named(source, false, "B", 87, &arguments[2]));
                arguments[3] = character('X');
                arguments[4] = material;
                TRY(shaped(source, output, arguments, 5, 103));
            }
        }
    }
    return MC_REGISTRY_OK;
}

static mc_registry_status tools_tail(grid_helper *helper) {
    family_source *source = &helper->source;
    mc_item_stack *output = NULL;
    TRY(stack_named(source, false, "bc", 119, 123, 126, 1, 0, &output));
    mc_recipe_argument arguments[4] = {{0}};
    TRY(literal(source, " #", 135, &arguments[0]));
    TRY(literal(source, "# ", 140, &arguments[1]));
    arguments[2] = character('#');
    TRY(named(source, false, "m", 153, &arguments[3]));
    TRY(shaped(source, output, arguments, 4, 157));
    stage(source, 160);
    return MC_REGISTRY_OK;
}

static mc_registry_status weapons_tail(grid_helper *helper) {
    family_source *source = &helper->source;
    mc_item_stack *output = NULL;
    TRY(stack_named(source, false, "i", 119, 123, 127, 1, 0, &output));
    mc_recipe_argument bow[7] = {{0}};
    TRY(literal(source, " #X", 137, &bow[0]));
    TRY(literal(source, "# X", 142, &bow[1]));
    TRY(literal(source, " #X", 147, &bow[2]));
    bow[3] = character('X');
    TRY(named(source, false, "I", 160, &bow[4]));
    bow[5] = character('#');
    TRY(named(source, false, "B", 175, &bow[6]));
    TRY(shaped(source, output, bow, 7, 179));
    TRY(stack_named(source, false, "j", 183, 187, 191, 4, 0, &output));
    mc_recipe_argument arrow[9] = {{0}};
    TRY(literal(source, "X", 201, &arrow[0]));
    TRY(literal(source, "#", 206, &arrow[1]));
    TRY(literal(source, "Y", 211, &arrow[2]));
    arrow[3] = character('Y');
    TRY(named(source, false, "J", 224, &arrow[4]));
    arrow[5] = character('X');
    TRY(named(source, false, "an", 239, &arrow[6]));
    arrow[7] = character('#');
    TRY(named(source, false, "B", 255, &arrow[8]));
    TRY(shaped(source, output, arrow, 9, 259));
    stage(source, 262);
    return MC_REGISTRY_OK;
}

static mc_registry_status grid_family(mc_registry_classes *classes, mc_crafting_manager *manager,
                                      const grid_specification *spec, int32_t new_pc,
                                      int32_t constructor_pc, int32_t populate_pc) {
    grid_helper *helper = new_helper(manager, sizeof(*helper), new_pc);
    if (helper == NULL)
        return mc_registry_crafting_error(manager);
    *helper = (grid_helper){.source = context(classes, manager, spec->client, spec->server)};
    mc_registry_crafting_stage(manager, constructor_pc);
    TRY(construct_grid(helper, spec));
    mc_registry_crafting_stage(manager, populate_pc);
    TRY(populate_grid(helper, spec->armor));
    if (spec == &tools)
        return tools_tail(helper);
    if (spec == &weapons)
        return weapons_tail(helper);
    stage(&helper->source, 104);
    return MC_REGISTRY_OK;
}

static mc_registry_status construct_compression(compression_helper *helper) {
    family_source *source = &helper->source;
    static const char *const blocks[] = {"ai", "aj", "ay", "P"};
    static const char *const items[] = {"n", "m", "l", "aU"};
    stage(source, 1);
    stage(source, 6);
    object_array *array = mc_registry_allocate(source->classes, sizeof(*array));
    if (array == NULL)
        return own_error(source);
    *array = (object_array){.length = 4};
    for (size_t i = 0; i < array->length; ++i) {
        const int32_t offset = (int32_t)i * 28;
        stage(source, 12 + offset);
        object_row *row = mc_registry_allocate(source->classes, sizeof(*row));
        if (row == NULL)
            return own_error(source);
        *row = (object_row){.length = 2};
        TRY(named(source, true, blocks[i], 17 + offset, &row->values[0]));
        mc_item_stack *stack = NULL;
        TRY(stack_named(source, false, items[i], 23 + offset, 27 + offset,
                        i == 3 ? 117 : 32 + offset, 9, i == 3 ? 4 : 0, &stack));
        row->values[1] = stack_argument(stack);
        stage(source, i == 3 ? 121 : 36 + offset);
        array->values[i] = row;
    }
    stage(source, 122);
    helper->a = array;
    stage(source, 125);
    return MC_REGISTRY_OK;
}

static mc_registry_status populate_compression(compression_helper *helper) {
    family_source *source = &helper->source;
    for (size_t i = 0; i < helper->a->length; ++i) {
        stage(source, 19);
        const mc_recipe_argument block = helper->a->values[i]->values[0];
        stage(source, 31);
        const mc_recipe_argument stack = helper->a->values[i]->values[1];
        if (block.kind != MC_RECIPE_ARGUMENT_BLOCK || stack.kind != MC_RECIPE_ARGUMENT_STACK)
            return fail(source, MC_REGISTRY_INVALID_ARGUMENT, "Compression helper operand cast");
        mc_item_stack *output = NULL;
        TRY(stack_from_value(source, true, 37, 42, block, 1, 0, &output));
        mc_recipe_argument compress[5] = {{0}};
        TRY(literal(source, "###", 51, &compress[0]));
        TRY(literal(source, "###", 56, &compress[1]));
        TRY(literal(source, "###", 61, &compress[2]));
        compress[3] = character('#');
        compress[4] = stack;
        TRY(shaped(source, output, compress, 5, 77));
        /* The stored nine-Item Stack itself is this recipe's output. */
        mc_recipe_argument expand[3] = {{0}};
        TRY(literal(source, "#", 89, &expand[0]));
        expand[1] = character('#');
        expand[2] = block;
        TRY(shaped(source, stack.value.stack, expand, 3, 104));
    }
    stage(source, 113);
    return MC_REGISTRY_OK;
}

static mc_registry_status compression_family(mc_registry_classes *classes,
                                             mc_crafting_manager *manager) {
    compression_helper *helper = new_helper(manager, sizeof(*helper), 37);
    if (helper == NULL)
        return mc_registry_crafting_error(manager);
    *helper = (compression_helper){.source = context(classes, manager, "tp", "mh")};
    mc_registry_crafting_stage(manager, 41);
    TRY(construct_compression(helper));
    mc_registry_crafting_stage(manager, 45);
    return populate_compression(helper);
}

static mc_registry_status populate_food(family_source *source) {
    for (int32_t variant = 0; variant < 2; ++variant) {
        const int32_t offset = variant * 79;
        mc_item_stack *output = NULL;
        TRY(stack_named(source, false, "D", 1 + offset, 5 + offset, 8 + offset, 1, 0, &output));
        mc_recipe_argument arguments[9] = {{0}};
        TRY(literal(source, "Y", 18 + offset, &arguments[0]));
        TRY(literal(source, "X", 23 + offset, &arguments[1]));
        TRY(literal(source, "#", 28 + offset, &arguments[2]));
        arguments[3] = character('X');
        TRY(named(source, true, variant == 0 ? "ag" : "ah", 41 + offset, &arguments[4]));
        arguments[5] = character('Y');
        TRY(named(source, true, variant == 0 ? "ah" : "ag", 56 + offset, &arguments[6]));
        arguments[7] = character('#');
        TRY(named(source, false, "C", 72 + offset, &arguments[8]));
        TRY(shaped(source, output, arguments, 9, 76 + offset));
    }
    mc_item_stack *output = NULL, *cocoa = NULL;
    TRY(stack_named(source, false, "ba", 159, 163, 168, 8, 0, &output));
    mc_recipe_argument arguments[5] = {{0}};
    TRY(literal(source, "#X#", 177, &arguments[0]));
    arguments[1] = character('X');
    TRY(stack_named(source, false, "aU", 190, 194, 199, 1, 3, &cocoa));
    arguments[2] = stack_argument(cocoa);
    arguments[3] = character('#');
    TRY(named(source, false, "R", 213, &arguments[4]));
    TRY(shaped(source, output, arguments, 5, 217));
    stage(source, 220);
    return MC_REGISTRY_OK;
}

typedef struct structure_step {
    const char *output, *ingredient;
    int32_t new_pc, load_pc, constructor_pc;
    size_t rows;
    const char *pattern[3];
    int32_t literal_pc[3];
    int32_t ingredient_pc, call_pc;
} structure_step;

static mc_registry_status populate_structures(family_source *source) {
    static const structure_step steps[] = {
        {"av", "y", 1, 5, 8, 3, {"###", "# #", "###"}, {17, 22, 27}, 40, 44},
        {"aC", "x", 48, 52, 55, 3, {"###", "# #", "###"}, {64, 69, 74}, 87, 91},
        {"az", "y", 95, 99, 102, 2, {"##", "##"}, {111, 116}, 129, 133},
        {"R", "F", 137, 141, 144, 2, {"##", "##"}, {153, 158}, 171, 175}};
    for (size_t i = 0; i < sizeof(steps) / sizeof(*steps); ++i) {
        const structure_step *step = &steps[i];
        mc_item_stack *output = NULL;
        TRY(stack_named(source, true, step->output, step->new_pc, step->load_pc,
                        step->constructor_pc, 1, 0, &output));
        mc_recipe_argument arguments[5] = {{0}};
        for (size_t row = 0; row < step->rows; ++row)
            TRY(literal(source, step->pattern[row], step->literal_pc[row], &arguments[row]));
        arguments[step->rows] = character('#');
        TRY(named(source, true, step->ingredient, step->ingredient_pc, &arguments[step->rows + 1]));
        TRY(shaped(source, output, arguments, step->rows + 2, step->call_pc));
    }
    stage(source, 178);
    return MC_REGISTRY_OK;
}

static mc_registry_status empty_family(mc_registry_classes *classes, mc_crafting_manager *manager,
                                       const char *client, const char *server, int32_t new_pc,
                                       int32_t constructor_pc, int32_t populate_pc,
                                       mc_registry_status (*populate)(family_source *)) {
    /* These actual helper classes have no instance fields. The retained native
     * context carries identity/source metadata without inventing game state. */
    family_source *source = new_helper(manager, sizeof(*source), new_pc);
    if (source == NULL)
        return mc_registry_crafting_error(manager);
    *source = context(classes, manager, client, server);
    mc_registry_crafting_stage(manager, constructor_pc);
    stage(source, 1); /* Object.<init>. */
    stage(source, 4); /* Empty helper constructor return. */
    mc_registry_crafting_stage(manager, populate_pc);
    return populate(source);
}

typedef struct dye_stack_step {
    int32_t new_pc, load_pc, constructor_pc, count, damage;
} dye_stack_step;

static mc_registry_status dye_stack(family_source *source, const dye_stack_step *step,
                                    mc_item_stack **out) {
    return stack_named(source, false, "aU", step->new_pc, step->load_pc, step->constructor_pc,
                       step->count, step->damage, out);
}

typedef struct dye_mix_step {
    dye_stack_step output;
    size_t ingredients;
    dye_stack_step input[4];
    int32_t call_pc;
} dye_mix_step;

/* Full ng/ih method suffix, in actual call order; every entry causes new
 * evaluated Stack objects. These are source counts/metadata, never final IDs. */
static const dye_mix_step dye_mixes[] = {
    {{155, 159, 165, 2, 9}, 2, {{174, 178, 183, 1, 1}, {189, 193, 199, 1, 15}}, 203},
    {{207, 211, 217, 2, 14}, 2, {{226, 230, 235, 1, 1}, {241, 245, 251, 1, 11}}, 255},
    {{259, 263, 269, 2, 10}, 2, {{278, 282, 287, 1, 2}, {293, 297, 303, 1, 15}}, 307},
    {{311, 315, 321, 2, 8}, 2, {{330, 334, 339, 1, 0}, {345, 349, 355, 1, 15}}, 359},
    {{363, 367, 373, 2, 7}, 2, {{382, 386, 392, 1, 8}, {398, 402, 408, 1, 15}}, 412},
    {{416, 420, 426, 3, 7},
     3,
     {{435, 439, 444, 1, 0}, {450, 454, 460, 1, 15}, {466, 470, 476, 1, 15}},
     480},
    {{484, 488, 494, 2, 12}, 2, {{503, 507, 512, 1, 4}, {518, 522, 528, 1, 15}}, 532},
    {{536, 540, 546, 2, 6}, 2, {{555, 559, 564, 1, 4}, {570, 574, 579, 1, 2}}, 583},
    {{587, 591, 596, 2, 5}, 2, {{605, 609, 614, 1, 4}, {620, 624, 629, 1, 1}}, 633},
    {{637, 641, 647, 2, 13}, 2, {{656, 660, 665, 1, 5}, {671, 675, 681, 1, 9}}, 685},
    {{689, 693, 699, 3, 13},
     3,
     {{708, 712, 717, 1, 4}, {723, 727, 732, 1, 1}, {738, 742, 748, 1, 9}},
     752},
    {{756, 760, 766, 4, 13},
     4,
     {{775, 779, 784, 1, 4}, {790, 794, 799, 1, 1}, {805, 809, 814, 1, 1}, {820, 824, 830, 1, 15}},
     834}};

static mc_registry_status dye_wool(family_source *source, int32_t color) {
    stage(source, 9);
    mc_item_stack *output = NULL;
    if (mc_registry_stack_allocate(source->classes, &output) != MC_REGISTRY_OK)
        return own_error(source);
    mc_recipe_argument wool;
    TRY(named(source, true, "ac", 13, &wool));
    stage(source, 18);
    TRY(mc_registry_block_class_initialize(source->classes, MC_BLOCK_WOOL));
    /* Actual ee.e/cq.d is (value ^ -1) & 15. */
    const int32_t damage = (int32_t)((~(uint32_t)color) & UINT32_C(15));
    stage(source, 21);
    TRY(mc_registry_stack_construct_block_values(source->classes, output, wool.value.block, 1,
                                                 damage));
    mc_recipe_argument arguments[2] = {{0}};
    mc_item_stack *dye = NULL;
    TRY(stack_named(source, false, "aU", 30, 34, 39, 1, color, &dye));
    arguments[0] = stack_argument(dye);

    stage(source, 45);
    mc_item_stack *white = NULL;
    if (mc_registry_stack_allocate(source->classes, &white) != MC_REGISTRY_OK)
        return own_error(source);
    stage(source, 49);
    TRY(mc_registry_items_active_use(source->classes));
    /* Capture the array before evaluating Block.ac and its bn. An active use
     * must not cause this evaluated operand to be silently reloaded. */
    mc_registry_item_array *array = source->classes->items.c;
    TRY(named(source, true, "ac", 52, &wool));
    stage(source, 55);
    if (wool.value.block == NULL)
        return fail(source, MC_REGISTRY_NULL_REFERENCE, "Dye Block.bn55 null receiver");
    const int32_t id = wool.value.block->fields.bn;
    stage(source, 58);
    mc_item *item = NULL;
    if (mc_registry_item_array_load(source->classes, array, id, &item) != MC_REGISTRY_OK)
        return own_error(source);
    stage(source, 61);
    TRY(mc_registry_stack_construct_item_values(source->classes, white, item, 1, 0));
    arguments[1] = stack_argument(white);
    return shapeless(source, output, arguments, 2, 65);
}

static mc_registry_status populate_dyes(family_source *source) {
    for (int32_t color = 0; color < 16; ++color)
        TRY(dye_wool(source, color));

    static const struct {
        dye_stack_step output;
        bool block;
        const char *ingredient;
        int32_t ingredient_pc, call_pc;
    } extracts[] = {{{75, 79, 85, 2, 11}, true, "ae", 94, 98},
                    {{102, 106, 111, 2, 1}, true, "af", 120, 124},
                    {{128, 132, 138, 3, 15}, false, "aV", 147, 151}};

    for (size_t i = 0; i < sizeof(extracts) / sizeof(*extracts); ++i) {
        mc_item_stack *output = NULL;
        TRY(dye_stack(source, &extracts[i].output, &output));
        mc_recipe_argument argument;
        TRY(named(source, extracts[i].block, extracts[i].ingredient, extracts[i].ingredient_pc,
                  &argument));
        TRY(shapeless(source, output, &argument, 1, extracts[i].call_pc));
    }
    for (size_t i = 0; i < sizeof(dye_mixes) / sizeof(*dye_mixes); ++i) {
        const dye_mix_step *step = &dye_mixes[i];
        mc_item_stack *output = NULL;
        TRY(dye_stack(source, &step->output, &output));
        mc_recipe_argument arguments[4] = {{0}};
        for (size_t j = 0; j < step->ingredients; ++j) {
            mc_item_stack *input = NULL;
            TRY(dye_stack(source, &step->input[j], &input));
            arguments[j] = stack_argument(input);
        }
        TRY(shapeless(source, output, arguments, step->ingredients, step->call_pc));
    }
    stage(source, 837);
    return MC_REGISTRY_OK;
}

mc_registry_status mc_registry_recipe_families(mc_registry_classes *classes,
                                               mc_crafting_manager *manager) {
    if (classes == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    if (manager == NULL || manager->classes != classes || classes->entered == 0)
        return mc_registry_fail(classes, MC_REGISTRY_INVALID_ARGUMENT,
                                "Recipe helper families require their admitted manager", NULL);
    TRY(grid_family(classes, manager, &tools, 15, 19, 23));
    TRY(grid_family(classes, manager, &weapons, 26, 30, 34));
    TRY(compression_family(classes, manager));
    TRY(empty_family(classes, manager, "od", "iw", 48, 52, 56, populate_food));
    TRY(empty_family(classes, manager, "jy", "gp", 59, 63, 67, populate_structures));
    TRY(grid_family(classes, manager, &armor, 70, 74, 78));
    TRY(empty_family(classes, manager, "ng", "ih", 81, 85, 89, populate_dyes));
    return MC_REGISTRY_OK;
}

#undef TRY
