#include "recipes_internal.h"

#include "../../block/registry/registry_internal.h"
#include "../../platform/text/stdout.h"

#include <limits.h>

static mc_registry_status diagnostic(mc_registry_error *error, mc_registry_status status,
                                     const char *message) {
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

void mc_registry_crafting_stage(mc_crafting_manager *manager, int32_t pc) {
    manager->constructor_pc = pc;
}

mc_registry_status mc_registry_crafting_error(mc_crafting_manager *manager) {
    mc_registry_error *error = &manager->classes->last_error;
    error->constructor_class = manager->classes->options.side == MC_REGISTRY_CLIENT ? "hk" : "ey";
    error->constructor_pc = manager->constructor_pc;
    return error->status;
}

static mc_registry_status crafting_fail(mc_crafting_manager *manager, mc_registry_status status,
                                        const char *message) {
    (void)mc_registry_fail(manager->classes, status, message, NULL);
    return mc_registry_crafting_error(manager);
}

static size_t decimal_ascii(int32_t value, char *out) {
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

static mc_registry_status print_count(mc_crafting_manager *manager) {
    mc_registry_classes *classes = manager->classes;
    mc_registry_crafting_stage(manager, 3204);
    mc_text_stdout output;
    mc_text_stdout_status text_status = mc_text_stdout_capture(&output);
    if (text_status != MC_TEXT_STDOUT_OK)
        return crafting_fail(manager, MC_REGISTRY_IO_FAILED, "Crafting stdout capture");
    mc_registry_crafting_stage(manager, 3207);
    mc_registry_crafting_stage(manager, 3211);
    mc_registry_crafting_stage(manager, 3215);
    const mc_registry_list *list = manager->b;
    mc_registry_crafting_stage(manager, 3218);
    if (list == NULL)
        return crafting_fail(manager, MC_REGISTRY_NULL_REFERENCE,
                             "Crafting List.size null receiver");
    if (list->count > (size_t)INT32_MAX)
        return crafting_fail(manager, MC_REGISTRY_LIMIT, "Crafting native List.size bound");
    const int32_t count = (int32_t)list->count;
    mc_registry_crafting_stage(manager, 3223);
    char line[64];
    size_t length = decimal_ascii(count, line);
    mc_registry_crafting_stage(manager, 3226);
    const mc_registry_string *suffix = NULL;
    mc_registry_status status = mc_registry_literal_ascii(classes, " recipes", &suffix);
    if (status != MC_REGISTRY_OK)
        return mc_registry_crafting_error(manager);
    mc_registry_crafting_stage(manager, 3228);
    if (suffix == NULL || (suffix->length != 0 && suffix->units == NULL))
        return crafting_fail(manager, MC_REGISTRY_INVALID_ARGUMENT,
                             "Crafting stdout literal storage");
    if (suffix->length > sizeof(line) - length)
        return crafting_fail(manager, MC_REGISTRY_LIMIT, "Crafting stdout native ASCII bound");
    for (size_t i = 0; i < suffix->length; ++i) {
        if (suffix->units[i] > UINT16_C(127))
            return crafting_fail(manager, MC_REGISTRY_INVALID_ARGUMENT,
                                 "Crafting stdout ASCII literal");
        line[length++] = (char)suffix->units[i];
    }
    mc_registry_crafting_stage(manager, 3231);
    mc_registry_crafting_stage(manager, 3234);
    bool io_failed = false;
    text_status = mc_text_stdout_println(&output, line, length, &io_failed);
    if (text_status != MC_TEXT_STDOUT_OK)
        return crafting_fail(manager,
                             text_status == MC_TEXT_STDOUT_INVALID_ARGUMENT
                                 ? MC_REGISTRY_INVALID_ARGUMENT
                                 : MC_REGISTRY_IO_FAILED,
                             "Crafting stdout native operation");
    manager->stdout_io_failed = manager->stdout_io_failed || io_failed;
    return MC_REGISTRY_OK;
}

static mc_registry_status construct_crafting(mc_crafting_manager *manager) {
    mc_registry_crafting_stage(manager, 1);
    mc_registry_crafting_stage(manager, 5);
    mc_registry_crafting_stage(manager, 9);
    mc_registry_list *list = mc_registry_allocate(manager->classes, sizeof(*list));
    if (list == NULL)
        return mc_registry_crafting_error(manager);
    *list = (mc_registry_list){0};
    mc_registry_crafting_stage(manager, 12);
    manager->b = list;
    mc_registry_status status = mc_registry_recipe_families(manager->classes, manager);
    if (status != MC_REGISTRY_OK)
        return status;
    status = mc_registry_recipe_explicit(manager->classes, manager);
    if (status != MC_REGISTRY_OK)
        return status;
    mc_registry_crafting_stage(manager, 3190);
    status = mc_registry_recipe_sort(manager);
    if (status != MC_REGISTRY_OK)
        return status;
    status = print_count(manager);
    if (status != MC_REGISTRY_OK)
        return status;
    mc_registry_crafting_stage(manager, 3237);
    return MC_REGISTRY_OK;
}

static mc_registry_status active_use(mc_registry_classes *classes, bool smelting) {
    if (classes == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    if (classes->entered == 0)
        return mc_registry_fail(classes, MC_REGISTRY_INVALID_ARGUMENT,
                                "Recipe class requires admitted initialization", NULL);
    mc_registry_class_state *state = smelting ? &classes->smelting_state : &classes->crafting_state;
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
    mc_registry_initializer_enter(classes, &frame, smelting ? "ey" : "hk", smelting ? "de" : "ey",
                                  &state->pc);
    mc_registry_status status;
    if (smelting) {
        mc_smelting_manager *manager = mc_registry_allocate(classes, sizeof(*manager));
        if (manager == NULL)
            status = classes->last_error.status;
        else {
            *manager = (mc_smelting_manager){.classes = classes, .constructor_pc = -1};
            mc_registry_initializer_stage(classes, 4);
            status = mc_registry_smelting_construct(manager);
            if (status == MC_REGISTRY_OK) {
                mc_registry_initializer_stage(classes, 7);
                classes->recipes.smelting = manager;
            }
        }
    } else {
        mc_crafting_manager *manager = mc_registry_allocate(classes, sizeof(*manager));
        if (manager == NULL)
            status = classes->last_error.status;
        else {
            *manager = (mc_crafting_manager){.classes = classes, .constructor_pc = -1};
            mc_registry_initializer_stage(classes, 4);
            status = construct_crafting(manager);
            if (status == MC_REGISTRY_OK) {
                mc_registry_initializer_stage(classes, 7);
                classes->recipes.crafting = manager;
            }
        }
    }
    if (status == MC_REGISTRY_OK)
        mc_registry_initializer_stage(classes, 10);
    state->phase = status == MC_REGISTRY_OK ? MC_REGISTRY_READY : MC_REGISTRY_FAILED;
    if (status != MC_REGISTRY_OK)
        state->failure = classes->last_error;
    mc_registry_initializer_leave(classes, &frame);
    return status;
}

mc_registry_status mc_registry_crafting_active_use(mc_registry_classes *classes) {
    return active_use(classes, false);
}

mc_registry_status mc_registry_smelting_active_use(mc_registry_classes *classes) {
    return active_use(classes, true);
}

static mc_registry_status initialize(mc_registry_classes *classes, bool smelting,
                                     mc_registry_error *error) {
    if (classes == NULL)
        return diagnostic(error, MC_REGISTRY_INVALID_ARGUMENT, "Recipe registry owner");
    if (classes->entered != 0)
        return diagnostic(error, MC_REGISTRY_BUSY, "Registry initialization entered");
    mc_registry_class_state *state = smelting ? &classes->smelting_state : &classes->crafting_state;
    if (state->phase == MC_REGISTRY_INITIALIZING)
        return diagnostic(error, MC_REGISTRY_BUSY, "Recipe initialization entered");
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
    const mc_registry_status status = active_use(classes, smelting);
    --classes->entered;
    if (status == MC_REGISTRY_OK)
        return diagnostic(error, MC_REGISTRY_OK, NULL);
    if (error != NULL)
        *error = classes->last_error;
    return status;
}

mc_registry_status mc_registry_classes_initialize_crafting(mc_registry_classes *classes,
                                                           mc_registry_error *error) {
    return initialize(classes, false, error);
}

mc_registry_status mc_registry_classes_initialize_smelting(mc_registry_classes *classes,
                                                           mc_registry_error *error) {
    return initialize(classes, true, error);
}

mc_registry_status mc_registry_classes_inspect_crafting(const mc_registry_classes *classes,
                                                        mc_registry_crafting_view *out,
                                                        mc_registry_error *error) {
    if (classes == NULL || out == NULL)
        return diagnostic(error, MC_REGISTRY_INVALID_ARGUMENT, "Crafting view arguments");
    *out = (mc_registry_crafting_view){classes->crafting_state.phase, classes->crafting_state.pc,
                                       classes->recipes.crafting};
    return diagnostic(error, MC_REGISTRY_OK, NULL);
}

mc_registry_status mc_registry_classes_inspect_smelting(const mc_registry_classes *classes,
                                                        mc_registry_smelting_view *out,
                                                        mc_registry_error *error) {
    if (classes == NULL || out == NULL)
        return diagnostic(error, MC_REGISTRY_INVALID_ARGUMENT, "Smelting view arguments");
    *out = (mc_registry_smelting_view){classes->smelting_state.phase, classes->smelting_state.pc,
                                       classes->recipes.smelting};
    return diagnostic(error, MC_REGISTRY_OK, NULL);
}
