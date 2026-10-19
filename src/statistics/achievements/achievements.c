#include "achievements_internal.h"

#include "../../block/registry/registry_internal.h"
#include "../../item/registry/stacks_internal.h"
#include "../../platform/text/stdout.h"

#include "mc/core/java/java.h"

#include <limits.h>

static mc_registry_status diagnostic(mc_registry_error *error, mc_registry_status status,
                                     const char *message)
{
    if (error != NULL)
        *error = (mc_registry_error){.status = status,
                                     .message = message,
                                     .initializer_pc = -1,
                                     .block_id = -1,
                                     .constructor_pc = -1,
                                     .profile_status = MC_JAVA_RANDOM_PROFILE_OK,
                                     .active_initializer_pc = -1,
                                     .item_argument = -1};
    return status;
}

static void stage(mc_statistic *statistic, int32_t pc)
{
    statistic->source_class = statistic->classes->options.side == MC_REGISTRY_CLIENT ? "ny" : "it";
    statistic->source_pc = pc;
}

static mc_registry_status own_error(mc_statistic *statistic)
{
    mc_registry_error *error = &statistic->classes->last_error;
    error->constructor_class = statistic->source_class;
    error->constructor_pc = statistic->source_pc;
    return error->status;
}

static mc_registry_status fail(mc_statistic *statistic, mc_registry_status status,
                               const char *message)
{
    (void)mc_registry_fail(statistic->classes, status, message, NULL);
    return own_error(statistic);
}

static mc_registry_status admitted(mc_statistic *statistic)
{
    if (statistic == NULL || statistic->classes == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    if (statistic->classes->entered == 0 || statistic->kind != MC_STATISTIC_ACHIEVEMENT)
        return mc_registry_fail(statistic->classes, MC_REGISTRY_INVALID_ARGUMENT,
                                "Achievement operation requires its admitted owner", NULL);
    return MC_REGISTRY_OK;
}

/* These spans represent only the reached immutable String append inputs. The
 * actual native result owner and units are allocated at toString;
 * there are no dummy Java builder objects or Java OOME timing claims. */
typedef struct key_builder {
    const uint16_t *units[3];
    size_t lengths[3];
    size_t count, length;
} key_builder;

static mc_registry_status append_key(mc_statistic *statistic, key_builder *builder,
                                     const mc_registry_string *part)
{
    static const uint16_t null_units[] = {'n', 'u', 'l', 'l'};
    const uint16_t *units = part == NULL ? null_units : part->units;
    const size_t length = part == NULL ? sizeof(null_units) / sizeof(*null_units) : part->length;
    if (length != 0 && units == NULL)
        return fail(statistic, MC_REGISTRY_INVALID_ARGUMENT, "Achievement String append storage");
    if (builder->count >= 3 || length > (size_t)INT32_MAX - builder->length)
        return fail(statistic, MC_REGISTRY_LIMIT, "Achievement String native length bound");
    builder->units[builder->count] = units;
    builder->lengths[builder->count] = length;
    ++builder->count;
    builder->length += length;
    return MC_REGISTRY_OK;
}

static mc_registry_status finish_key(mc_statistic *statistic, const key_builder *builder,
                                     const mc_registry_string **out)
{
    if (builder->length > SIZE_MAX / sizeof(uint16_t))
        return fail(statistic, MC_REGISTRY_LIMIT, "Achievement String native allocation bound");
    mc_registry_classes *classes = statistic->classes;
    mc_registry_string *string = mc_registry_allocate(classes, sizeof(*string));
    if (string == NULL)
        return own_error(statistic);
    uint16_t *units = NULL;
    if (builder->length != 0) {
        units = mc_registry_allocate(classes, builder->length * sizeof(*units));
        if (units == NULL)
            return own_error(statistic);
    }
    size_t offset = 0;
    for (size_t i = 0; i < builder->count; ++i)
        for (size_t j = 0; j < builder->lengths[i]; ++j)
            units[offset++] = builder->units[i][j];
    *string = (mc_registry_string){.units = units, .length = builder->length};
    *out = string;
    return MC_REGISTRY_OK;
}

static mc_registry_status achievement_key(mc_statistic *statistic, const mc_registry_string *suffix,
                                          bool description, const mc_registry_string **out)
{
    mc_registry_classes *classes = statistic->classes;
    key_builder builder = {0};
    stage(statistic, description ? 37 : 5);
    stage(statistic, description ? 41 : 9);
    stage(statistic, description ? 44 : 12);
    const mc_registry_string *prefix = NULL;
    mc_registry_status status = mc_registry_literal_ascii(classes, "achievement.", &prefix);
    if (status != MC_REGISTRY_OK)
        return own_error(statistic);
    stage(statistic, description ? 46 : 14);
    status = append_key(statistic, &builder, prefix);
    if (status != MC_REGISTRY_OK)
        return status;
    stage(statistic, description ? 49 : 17);
    const mc_registry_string *saved_suffix = suffix;
    stage(statistic, description ? 50 : 18);
    status = append_key(statistic, &builder, saved_suffix);
    if (status != MC_REGISTRY_OK)
        return status;
    if (description) {
        stage(statistic, 53);
        const mc_registry_string *tail = NULL;
        status = mc_registry_literal_ascii(classes, ".desc", &tail);
        if (status != MC_REGISTRY_OK)
            return own_error(statistic);
        stage(statistic, 55);
        status = append_key(statistic, &builder, tail);
        if (status != MC_REGISTRY_OK)
            return status;
    }
    stage(statistic, description ? 58 : 21);
    return finish_key(statistic, &builder, out);
}

mc_registry_status mc_registry_achievement_construct_stack(mc_statistic *statistic, int32_t raw_id,
                                                           const mc_registry_string *suffix,
                                                           int32_t x, int32_t y,
                                                           mc_item_stack *icon,
                                                           const mc_statistic *parent)
{
    mc_registry_status status = admitted(statistic);
    if (status != MC_REGISTRY_OK)
        return status;
    mc_registry_classes *classes = statistic->classes;
    stage(statistic, 4);
    const int32_t id = mc_java_i32_add(INT32_C(5242880), raw_id);
    const mc_registry_string *key = NULL;
    status = achievement_key(statistic, suffix, false, &key);
    if (status != MC_REGISTRY_OK)
        return status;
    stage(statistic, 24);
    const mc_registry_string *name = NULL;
    status = mc_registry_translate(classes, key, &name);
    if (status != MC_REGISTRY_OK)
        return status;
    stage(statistic, 27);
    /* The reached two-argument vr/ns constructor forwards its actual i. */
    const mc_statistic_formatter *formatter = classes->statistic_base.i;
    status = mc_registry_statistic_construct_base(statistic, id, name, formatter);
    if (status != MC_REGISTRY_OK)
        return status;
    stage(statistic, 33);
    statistic->achievement.d = icon;
    const mc_registry_string *description_key = NULL;
    status = achievement_key(statistic, suffix, true, &description_key);
    if (status != MC_REGISTRY_OK)
        return status;
    stage(statistic, 61);
    const mc_registry_string *description = NULL;
    status = mc_registry_translate(classes, description_key, &description);
    if (status != MC_REGISTRY_OK)
        return status;
    stage(statistic, 64);
    statistic->achievement.l = description;
    stage(statistic, 69);
    statistic->achievement.a = x;
    stage(statistic, 75);
    statistic->achievement.b = y;

    stage(statistic, 79);
    status = mc_registry_achievements_active_use(classes);
    if (status != MC_REGISTRY_OK)
        return status;
    if (x < classes->achievements.a) {
        stage(statistic, 86);
        classes->achievements.a = x;
    }
    stage(statistic, 91);
    status = mc_registry_achievements_active_use(classes);
    if (status != MC_REGISTRY_OK)
        return status;
    if (y < classes->achievements.b) {
        stage(statistic, 99);
        classes->achievements.b = y;
    }
    stage(statistic, 103);
    status = mc_registry_achievements_active_use(classes);
    if (status != MC_REGISTRY_OK)
        return status;
    if (x > classes->achievements.c) {
        stage(statistic, 110);
        classes->achievements.c = x;
    }
    stage(statistic, 115);
    status = mc_registry_achievements_active_use(classes);
    if (status != MC_REGISTRY_OK)
        return status;
    if (y > classes->achievements.d) {
        stage(statistic, 123);
        classes->achievements.d = y;
    }
    stage(statistic, 129);
    statistic->achievement.c = parent;
    stage(statistic, 132);
    return MC_REGISTRY_OK;
}

mc_registry_status mc_registry_achievement_construct_item(mc_statistic *statistic, int32_t raw_id,
                                                          const mc_registry_string *suffix,
                                                          int32_t x, int32_t y, const mc_item *icon,
                                                          const mc_statistic *parent)
{
    mc_registry_status status = admitted(statistic);
    if (status != MC_REGISTRY_OK)
        return status;
    stage(statistic, 6);
    mc_item_stack *stack = NULL;
    status = mc_registry_stack_allocate(statistic->classes, &stack);
    if (status != MC_REGISTRY_OK)
        return own_error(statistic);
    stage(statistic, 12);
    status = mc_registry_stack_construct_item(statistic->classes, stack, icon);
    if (status != MC_REGISTRY_OK)
        return status;
    stage(statistic, 17);
    status =
        mc_registry_achievement_construct_stack(statistic, raw_id, suffix, x, y, stack, parent);
    if (status != MC_REGISTRY_OK)
        return status;
    stage(statistic, 20);
    return MC_REGISTRY_OK;
}

mc_registry_status mc_registry_achievement_construct_block(mc_statistic *statistic, int32_t raw_id,
                                                           const mc_registry_string *suffix,
                                                           int32_t x, int32_t y,
                                                           const mc_block *icon,
                                                           const mc_statistic *parent)
{
    mc_registry_status status = admitted(statistic);
    if (status != MC_REGISTRY_OK)
        return status;
    stage(statistic, 6);
    mc_item_stack *stack = NULL;
    status = mc_registry_stack_allocate(statistic->classes, &stack);
    if (status != MC_REGISTRY_OK)
        return own_error(statistic);
    stage(statistic, 12);
    status = mc_registry_stack_construct_block(statistic->classes, stack, icon);
    if (status != MC_REGISTRY_OK)
        return status;
    stage(statistic, 17);
    status =
        mc_registry_achievement_construct_stack(statistic, raw_id, suffix, x, y, stack, parent);
    if (status != MC_REGISTRY_OK)
        return status;
    stage(statistic, 20);
    return MC_REGISTRY_OK;
}

mc_registry_status mc_registry_achievement_flag(mc_statistic *statistic)
{
    mc_registry_status status = admitted(statistic);
    if (status != MC_REGISTRY_OK)
        return status;
    stage(statistic, 2);
    statistic->fields.g = true;
    stage(statistic, 6);
    return MC_REGISTRY_OK;
}

mc_registry_status mc_registry_achievement_special(mc_statistic *statistic)
{
    mc_registry_status status = admitted(statistic);
    if (status != MC_REGISTRY_OK)
        return status;
    stage(statistic, 2);
    statistic->achievement.special = true;
    stage(statistic, 6);
    return MC_REGISTRY_OK;
}

mc_registry_status mc_registry_achievement_register(mc_statistic *statistic)
{
    mc_registry_status status = admitted(statistic);
    if (status != MC_REGISTRY_OK)
        return status;
    stage(statistic, 1);
    status = mc_registry_statistic_register_base(statistic);
    if (status != MC_REGISTRY_OK)
        return status;
    stage(statistic, 5);
    status = mc_registry_achievements_active_use(statistic->classes);
    if (status != MC_REGISTRY_OK)
        return status;
    mc_registry_list *list = statistic->classes->achievements.e;
    stage(statistic, 9);
    status = mc_registry_statistic_list_add(statistic->classes, list, statistic);
    if (status != MC_REGISTRY_OK)
        return own_error(statistic);
    stage(statistic, 16);
    return MC_REGISTRY_OK;
}

typedef struct achievement_step {
    int32_t allocation_pc, raw_id, id_pc;
    const char *suffix;
    int32_t suffix_pc, x, x_pc, y, y_pc;
    bool block_icon;
    const char *icon;
    int32_t icon_pc, parent_index, parent_pc, constructor_pc;
    int32_t independent_pc, special_pc, registration_pc, publication_pc;
} achievement_step;

/* Actual ep/cy instructions. These rows schedule complete construction chains;
 * they are not initialized-property snapshots or detached icon/parent tables. */
static const achievement_step steps[] = {
    {10, 0, 14, "openInventory", 15, 0, 17, 0, 18, false, "aJ", 19, -1, 22, 23, 26, -1, 29, 32},
    {35, 1, 39, "mineWood", 40, 2, 42, 1, 43, true, "K", 44, 0, 47, 50, -1, -1, 53, 56},
    {59, 2, 63, "buildWorkBench", 64, 4, 66, -1, 67, true, "az", 68, 1, 71, 74, -1, -1, 77, 80},
    {83, 3, 87, "buildPickaxe", 88, 4, 90, 2, 91, false, "r", 92, 2, 95, 98, -1, -1, 101, 104},
    {107, 4, 111, "buildFurnace", 112, 3, 114, 4, 115, true, "aD", 116, 3, 119, 122, -1, -1, 125,
     128},
    {131, 5, 135, "acquireIron", 136, 1, 138, 4, 139, false, "m", 140, 4, 143, 146, -1, -1, 149,
     152},
    {155, 6, 159, "buildHoe", 161, 2, 163, -3, 164, false, "L", 166, 2, 169, 172, -1, -1, 175, 178},
    {181, 7, 185, "makeBread", 187, -1, 189, -3, 190, false, "S", 192, 6, 195, 198, -1, -1, 201,
     204},
    {207, 8, 211, "bakeCake", 213, 0, 215, -5, 216, false, "aX", 218, 6, 221, 224, -1, -1, 227,
     230},
    {233, 9, 237, "buildBetterPickaxe", 239, 6, 241, 2, 243, false, "v", 244, 3, 247, 250, -1, -1,
     253, 256},
    {259, 10, 263, "cookFish", 265, 2, 267, 6, 268, false, "aT", 270, 4, 273, 276, -1, -1, 279,
     282},
    {285, 11, 289, "onARail", 291, 2, 293, 3, 294, true, "aH", 295, 5, 298, 301, -1, 304, 307, 310},
    {313, 12, 317, "buildSword", 319, 6, 321, -1, 323, false, "p", 324, 2, 327, 330, -1, -1, 333,
     336},
    {339, 13, 343, "killEnemy", 345, 8, 347, -1, 349, false, "aV", 350, 12, 353, 356, -1, -1, 359,
     362},
    {365, 14, 369, "killCow", 371, 7, 373, -3, 375, false, "aD", 377, 12, 380, 383, -1, -1, 386,
     389},
    {392, 15, 396, "flyPig", 398, 8, 400, -4, 402, false, "ay", 404, 14, 407, 410, -1, 413, 416,
     419}};

_Static_assert(sizeof(steps) / sizeof(*steps) == 16, "ep/cy have sixteen named chains");

static mc_registry_status initialize_step(mc_registry_classes *classes,
                                          const achievement_step *step, size_t index)
{
    mc_registry_initializer_stage(classes, step->allocation_pc);
    mc_statistic *statistic = NULL;
    mc_registry_status status =
        mc_registry_statistic_allocate(classes, MC_STATISTIC_ACHIEVEMENT, &statistic);
    if (status != MC_REGISTRY_OK)
        return status;
    mc_registry_initializer_stage(classes, step->id_pc);
    const int32_t raw_id = step->raw_id;
    mc_registry_initializer_stage(classes, step->suffix_pc);
    const mc_registry_string *suffix = NULL;
    status = mc_registry_literal_ascii(classes, step->suffix, &suffix);
    if (status != MC_REGISTRY_OK)
        return status;
    mc_registry_initializer_stage(classes, step->x_pc);
    const int32_t x = step->x;
    mc_registry_initializer_stage(classes, step->y_pc);
    const int32_t y = step->y;
    mc_registry_initializer_stage(classes, step->icon_pc);
    mc_item *item = NULL;
    mc_block *block = NULL;
    if (step->block_icon)
        status = mc_registry_block_static_load(classes, step->icon, &block);
    else
        status = mc_registry_item_static_load(classes, step->icon, &item);
    if (status != MC_REGISTRY_OK)
        return status;
    mc_registry_initializer_stage(classes, step->parent_pc);
    const mc_statistic *parent = NULL;
    if (step->parent_index >= 0) {
        status = mc_registry_achievements_active_use(classes);
        if (status != MC_REGISTRY_OK)
            return status;
        parent = classes->achievements.named[(size_t)step->parent_index];
    }
    mc_registry_initializer_stage(classes, step->constructor_pc);
    if (step->block_icon)
        status =
            mc_registry_achievement_construct_block(statistic, raw_id, suffix, x, y, block, parent);
    else
        status =
            mc_registry_achievement_construct_item(statistic, raw_id, suffix, x, y, item, parent);
    if (status != MC_REGISTRY_OK)
        return status;
    if (step->independent_pc >= 0) {
        mc_registry_initializer_stage(classes, step->independent_pc);
        status = mc_registry_achievement_flag(statistic);
        if (status != MC_REGISTRY_OK)
            return status;
    }
    if (step->special_pc >= 0) {
        mc_registry_initializer_stage(classes, step->special_pc);
        status = mc_registry_achievement_special(statistic);
        if (status != MC_REGISTRY_OK)
            return status;
    }
    mc_registry_initializer_stage(classes, step->registration_pc);
    status = mc_registry_achievement_register(statistic);
    if (status != MC_REGISTRY_OK)
        return status;
    mc_registry_initializer_stage(classes, step->publication_pc);
    classes->achievements.named[index] = statistic;
    return MC_REGISTRY_OK;
}

static size_t decimal_ascii(int32_t value, char *out)
{
    char reversed[10];
    size_t count = 0, length = 0;
    uint32_t magnitude = value < 0 ? UINT32_C(0) - (uint32_t)value : (uint32_t)value;
    do {
        reversed[count++] = (char)('0' + magnitude % UINT32_C(10));
        magnitude /= UINT32_C(10);
    } while (magnitude != 0);
    if (value < 0)
        out[length++] = '-';
    while (count != 0)
        out[length++] = reversed[--count];
    return length;
}

static mc_registry_status stdout_status(mc_registry_classes *classes, mc_text_stdout_status status)
{
    return mc_registry_fail(classes,
                            status == MC_TEXT_STDOUT_INVALID_ARGUMENT ? MC_REGISTRY_INVALID_ARGUMENT
                                                                      : MC_REGISTRY_IO_FAILED,
                            "Achievement process stdout native operation", NULL);
}

static mc_registry_status print_count(mc_registry_classes *classes)
{
    mc_registry_initializer_stage(classes, 422);
    mc_text_stdout output;
    mc_text_stdout_status text_status = mc_text_stdout_capture(&output);
    if (text_status != MC_TEXT_STDOUT_OK)
        return stdout_status(classes, text_status);
    /* The admitted ASCII println profile needs no dummy builder/String heap
     * owner; preserve its actual operand evaluation and real output attempt. */
    mc_registry_initializer_stage(classes, 425);
    mc_registry_initializer_stage(classes, 429);
    mc_registry_initializer_stage(classes, 432);
    mc_registry_status status = mc_registry_achievements_active_use(classes);
    if (status != MC_REGISTRY_OK)
        return status;
    const mc_registry_list *list = classes->achievements.e;
    mc_registry_initializer_stage(classes, 435);
    if (list == NULL)
        return mc_registry_fail(classes, MC_REGISTRY_NULL_REFERENCE,
                                "Achievement List.size435 null receiver", NULL);
    if (list->count > (size_t)INT32_MAX)
        return mc_registry_fail(classes, MC_REGISTRY_LIMIT, "Achievement native List.size bound",
                                NULL);
    const int32_t count = (int32_t)list->count;
    mc_registry_initializer_stage(classes, 440);
    char line[64];
    size_t length = decimal_ascii(count, line);
    mc_registry_initializer_stage(classes, 443);
    const mc_registry_string *suffix = NULL;
    status = mc_registry_literal_ascii(classes, " achievements", &suffix);
    if (status != MC_REGISTRY_OK)
        return status;
    mc_registry_initializer_stage(classes, 445);
    if (suffix == NULL || (suffix->length != 0 && suffix->units == NULL))
        return mc_registry_fail(classes, MC_REGISTRY_INVALID_ARGUMENT,
                                "Achievement stdout literal storage", NULL);
    if (suffix->length > sizeof(line) - length)
        return mc_registry_fail(classes, MC_REGISTRY_LIMIT, "Achievement stdout ASCII native bound",
                                NULL);
    for (size_t i = 0; i < suffix->length; ++i) {
        if (suffix->units[i] > UINT16_C(127))
            return mc_registry_fail(classes, MC_REGISTRY_INVALID_ARGUMENT,
                                    "Achievement stdout ASCII literal", NULL);
        line[length++] = (char)suffix->units[i];
    }
    mc_registry_initializer_stage(classes, 448);
    mc_registry_initializer_stage(classes, 451);
    bool io_failed = false;
    text_status = mc_text_stdout_println(&output, line, length, &io_failed);
    if (text_status != MC_TEXT_STDOUT_OK)
        return stdout_status(classes, text_status);
    classes->achievements.stdout_io_failed = classes->achievements.stdout_io_failed || io_failed;
    return MC_REGISTRY_OK;
}

static mc_registry_status initialize(mc_registry_classes *classes)
{
    mc_registry_initializer_stage(classes, 0);
    /* This native list combines storage and initialization at its actual
     * constructor4, before e publication7. */
    mc_registry_initializer_stage(classes, 4);
    mc_registry_list *list = mc_registry_allocate(classes, sizeof(*list));
    if (list == NULL)
        return classes->last_error.status;
    *list = (mc_registry_list){0};
    mc_registry_initializer_stage(classes, 7);
    classes->achievements.e = list;
    for (size_t i = 0; i < sizeof(steps) / sizeof(*steps); ++i) {
        mc_registry_status status = initialize_step(classes, &steps[i], i);
        if (status != MC_REGISTRY_OK)
            return status;
    }
    mc_registry_status status = print_count(classes);
    if (status != MC_REGISTRY_OK)
        return status;
    mc_registry_initializer_stage(classes, 454);
    return MC_REGISTRY_OK;
}

mc_registry_status mc_registry_achievements_active_use(mc_registry_classes *classes)
{
    if (classes == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    if (classes->entered == 0)
        return mc_registry_fail(classes, MC_REGISTRY_INVALID_ARGUMENT,
                                "Achievements require admitted initialization", NULL);
    mc_registry_class_state *state = &classes->achievements_state;
    if (state->phase == MC_REGISTRY_FAILED) {
        classes->last_error = state->failure;
        classes->last_error.status = MC_REGISTRY_FAILED_INITIALIZATION;
        return MC_REGISTRY_FAILED_INITIALIZATION;
    }
    if (state->phase != MC_REGISTRY_COLD)
        return MC_REGISTRY_OK;
    state->phase = MC_REGISTRY_INITIALIZING;
    state->pc = 0;
    mc_registry_initializer frame;
    mc_registry_initializer_enter(classes, &frame, "ep", "cy", &state->pc);
    const mc_registry_status status = initialize(classes);
    state->phase = status == MC_REGISTRY_OK ? MC_REGISTRY_READY : MC_REGISTRY_FAILED;
    if (status != MC_REGISTRY_OK)
        state->failure = classes->last_error;
    mc_registry_initializer_leave(classes, &frame);
    return status;
}

mc_registry_status mc_registry_classes_initialize_achievements(mc_registry_classes *classes,
                                                               mc_registry_error *error)
{
    if (classes == NULL)
        return diagnostic(error, MC_REGISTRY_INVALID_ARGUMENT, "Achievement registry owner");
    if (classes->entered != 0 || classes->achievements_state.phase == MC_REGISTRY_INITIALIZING)
        return diagnostic(error, MC_REGISTRY_BUSY, "registry initialization entered");
    const mc_registry_class_state *state = &classes->achievements_state;
    if (state->phase == MC_REGISTRY_READY)
        return diagnostic(error, MC_REGISTRY_OK, NULL);
    if (state->phase == MC_REGISTRY_FAILED) {
        if (error != NULL) {
            *error = state->failure;
            error->status = MC_REGISTRY_FAILED_INITIALIZATION;
        }
        return MC_REGISTRY_FAILED_INITIALIZATION;
    }
    ++classes->entered;
    const mc_registry_status status = mc_registry_achievements_active_use(classes);
    --classes->entered;
    if (status == MC_REGISTRY_OK)
        return diagnostic(error, MC_REGISTRY_OK, NULL);
    if (error != NULL)
        *error = classes->last_error;
    return status;
}

mc_registry_status mc_registry_classes_inspect_achievements(const mc_registry_classes *classes,
                                                            mc_registry_achievement_view *out,
                                                            mc_registry_error *error)
{
    if (classes == NULL || out == NULL)
        return diagnostic(error, MC_REGISTRY_INVALID_ARGUMENT, "Achievement view arguments");
    *out = (mc_registry_achievement_view){.phase = classes->achievements_state.phase,
                                          .initializer_pc = classes->achievements_state.pc,
                                          .a = classes->achievements.a,
                                          .b = classes->achievements.b,
                                          .c = classes->achievements.c,
                                          .d = classes->achievements.d,
                                          .e = classes->achievements.e,
                                          .named = classes->achievements.named};
    return diagnostic(error, MC_REGISTRY_OK, NULL);
}

const mc_achievement_fields *mc_statistic_achievement_fields_get(const mc_statistic *statistic)
{
    return statistic == NULL || statistic->kind != MC_STATISTIC_ACHIEVEMENT
               ? NULL
               : &statistic->achievement;
}
