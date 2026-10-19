#include "../../block/registry/registry_internal.h"
#include "../achievements/achievements_internal.h"
#include "registry_internal.h"

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

static mc_registry_status cached_state(mc_registry_classes *classes,
                                       const mc_registry_class_state *state)
{
    if (state->phase == MC_REGISTRY_FAILED) {
        classes->last_error = state->failure;
        classes->last_error.status = MC_REGISTRY_FAILED_INITIALIZATION;
        return MC_REGISTRY_FAILED_INITIALIZATION;
    }
    return MC_REGISTRY_OK;
}

static mc_statistic_formatter *formatter_allocate(mc_registry_classes *classes)
{
    return mc_registry_allocate(classes, sizeof(mc_statistic_formatter));
}

static void formatter_construct(mc_statistic_formatter *formatter, mc_registry_classes *classes,
                                mc_statistic_formatter_kind kind)
{
    /* bq/ax, bo/av and bn/at constructors call only Object.<init>. The kind
     * and owner are native type information, not additional game fields. */
    *formatter = (mc_statistic_formatter){.classes = classes, .kind = kind};
}

mc_registry_status mc_registry_statistic_base_active_use(mc_registry_classes *classes)
{
    if (classes == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    if (classes->entered == 0)
        return mc_registry_fail(classes, MC_REGISTRY_INVALID_ARGUMENT,
                                "Statistic base active use requires admitted initialization", NULL);
    mc_registry_class_state *state = &classes->statistic_base_state;
    if (state->phase != MC_REGISTRY_COLD)
        return cached_state(classes, state);
    state->phase = MC_REGISTRY_INITIALIZING;
    state->pc = 0;
    mc_registry_initializer frame;
    mc_registry_initializer_enter(classes, &frame, "vr", "ns", &state->pc);
    mc_registry_status status = MC_REGISTRY_OK;
    mc_statistic_number_format *number = NULL;
    mc_statistic_formatter *formatter = NULL;
    const mc_registry_string *pattern = NULL;

    state->pc = 3;
    status = mc_registry_number_format_create(classes, true, &number);
    if (status != MC_REGISTRY_OK)
        goto done;
    state->pc = 6;
    classes->statistic_base.b = number;

    state->pc = 9;
    formatter = formatter_allocate(classes);
    if (formatter == NULL) {
        status = classes->last_error.status;
        goto done;
    }
    state->pc = 13;
    formatter_construct(formatter, classes, MC_STATISTIC_FORMAT_INTEGER);
    state->pc = 16;
    classes->statistic_base.i = formatter;

    /* The native numeric constructor combines storage and initialization.
     * Its real allocation occurs at25, after actual literal evaluation23;
     * the source new19 remains operation metadata, not a dummy allocation. */
    state->pc = 19;
    state->pc = 23;
    status = mc_registry_literal_ascii(classes, "########0.00", &pattern);
    if (status != MC_REGISTRY_OK)
        goto done;
    (void)pattern;
    state->pc = 25;
    status = mc_registry_number_format_create(classes, false, &number);
    if (status != MC_REGISTRY_OK)
        goto done;
    state->pc = 28;
    classes->statistic_base.c = number;

    state->pc = 31;
    formatter = formatter_allocate(classes);
    if (formatter == NULL) {
        status = classes->last_error.status;
        goto done;
    }
    state->pc = 35;
    formatter_construct(formatter, classes, MC_STATISTIC_FORMAT_TIME);
    state->pc = 38;
    classes->statistic_base.j = formatter;

    state->pc = 41;
    formatter = formatter_allocate(classes);
    if (formatter == NULL) {
        status = classes->last_error.status;
        goto done;
    }
    state->pc = 45;
    formatter_construct(formatter, classes, MC_STATISTIC_FORMAT_DISTANCE);
    state->pc = 48;
    classes->statistic_base.k = formatter;
    state->pc = 51;
done:
    state->phase = status == MC_REGISTRY_OK ? MC_REGISTRY_READY : MC_REGISTRY_FAILED;
    if (status != MC_REGISTRY_OK)
        state->failure = classes->last_error;
    mc_registry_initializer_leave(classes, &frame);
    return status;
}

mc_registry_status mc_registry_classes_initialize_statistic_base(mc_registry_classes *classes,
                                                                 mc_registry_error *error)
{
    if (classes == NULL)
        return diagnostic(error, MC_REGISTRY_INVALID_ARGUMENT, "Statistic base registry owner");
    if (classes->entered != 0 || classes->statistic_base_state.phase == MC_REGISTRY_INITIALIZING)
        return diagnostic(error, MC_REGISTRY_BUSY, "registry initialization entered");
    const mc_registry_class_state *state = &classes->statistic_base_state;
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
    const mc_registry_status status = mc_registry_statistic_base_active_use(classes);
    --classes->entered;
    if (status == MC_REGISTRY_OK)
        return diagnostic(error, MC_REGISTRY_OK, NULL);
    if (error != NULL)
        *error = classes->last_error;
    return status;
}

mc_registry_status mc_registry_classes_inspect_statistic_base(const mc_registry_classes *classes,
                                                              mc_registry_statistic_base_view *out,
                                                              mc_registry_error *error)
{
    if (classes == NULL || out == NULL)
        return diagnostic(error, MC_REGISTRY_INVALID_ARGUMENT, "Statistic base view arguments");
    *out = (mc_registry_statistic_base_view){.phase = classes->statistic_base_state.phase,
                                             .initializer_pc = classes->statistic_base_state.pc,
                                             .b = classes->statistic_base.b,
                                             .c = classes->statistic_base.c,
                                             .i = classes->statistic_base.i,
                                             .j = classes->statistic_base.j,
                                             .k = classes->statistic_base.k};
    return diagnostic(error, MC_REGISTRY_OK, NULL);
}

mc_registry_status mc_statistic_formatter_kind_get(const mc_statistic_formatter *formatter,
                                                   mc_statistic_formatter_kind *out)
{
    if (formatter == NULL || out == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    *out = formatter->kind;
    return MC_REGISTRY_OK;
}

static const char *source_class(mc_registry_classes *classes, mc_statistic_kind kind)
{
    if (kind == MC_STATISTIC_GENERAL)
        return classes->options.side == MC_REGISTRY_CLIENT ? "yd" : "pm";
    if (kind == MC_STATISTIC_ITEM)
        return classes->options.side == MC_REGISTRY_CLIENT ? "tw" : "mm";
    if (kind == MC_STATISTIC_ACHIEVEMENT)
        return classes->options.side == MC_REGISTRY_CLIENT ? "ny" : "it";
    return classes->options.side == MC_REGISTRY_CLIENT ? "vr" : "ns";
}

mc_registry_status mc_registry_statistic_allocate(mc_registry_classes *classes,
                                                  mc_statistic_kind kind, mc_statistic **out)
{
    if (classes == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    if (out == NULL || (unsigned int)kind >= (unsigned int)MC_STATISTIC_KIND_COUNT)
        return mc_registry_fail(classes, MC_REGISTRY_INVALID_ARGUMENT,
                                "Statistic allocation arguments", NULL);
    mc_registry_status status = mc_registry_statistic_base_active_use(classes);
    if (status != MC_REGISTRY_OK)
        return status;
    mc_statistic *statistic = mc_registry_allocate(classes, sizeof(*statistic));
    if (statistic == NULL)
        return classes->last_error.status;
    *statistic = (mc_statistic){.classes = classes,
                                .kind = kind,
                                .source_class = source_class(classes, kind),
                                .source_pc = -1};
    *out = statistic;
    return MC_REGISTRY_OK;
}

mc_registry_status mc_registry_statistic_construct_base(mc_statistic *statistic, int32_t id,
                                                        const mc_registry_string *name,
                                                        const mc_statistic_formatter *formatter)
{
    if (statistic == NULL || statistic->classes == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    mc_registry_classes *classes = statistic->classes;
    if (classes->entered == 0)
        return mc_registry_fail(classes, MC_REGISTRY_INVALID_ARGUMENT,
                                "Statistic construction requires admitted initialization", NULL);
    statistic->source_class = source_class(classes, MC_STATISTIC_BASE);
    statistic->source_pc = 1;
    statistic->source_pc = 6;
    statistic->fields.g = false;
    statistic->source_pc = 11;
    statistic->fields.e = id;
    statistic->source_pc = 16;
    statistic->fields.f = name;
    statistic->source_pc = 21;
    /* NULL is a genuine explicit formatter reference. Two-argument source
     * constructors are supplied their actual already evaluated static i. */
    statistic->fields.a = formatter;
    statistic->source_pc = 24;
    return MC_REGISTRY_OK;
}

mc_registry_status mc_registry_statistic_construct(mc_statistic *statistic, int32_t id,
                                                   const mc_registry_string *name,
                                                   const mc_statistic_formatter *formatter,
                                                   int32_t associated_id)
{
    if (statistic == NULL || statistic->classes == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    if (statistic->kind == MC_STATISTIC_ACHIEVEMENT)
        return mc_registry_fail(statistic->classes, MC_REGISTRY_INVALID_ARGUMENT,
                                "Use the typed Achievement constructor", NULL);
    mc_registry_status status =
        mc_registry_statistic_construct_base(statistic, id, name, formatter);
    if (status != MC_REGISTRY_OK)
        return status;
    mc_registry_classes *classes = statistic->classes;
    if (statistic->kind == MC_STATISTIC_ITEM) {
        statistic->source_class = source_class(classes, MC_STATISTIC_ITEM);
        statistic->source_pc = 8;
        statistic->associated_id = associated_id;
        statistic->source_pc = 11;
    }
    return MC_REGISTRY_OK;
}

mc_registry_status mc_registry_statistic_base_flag(mc_statistic *statistic)
{
    if (statistic == NULL || statistic->classes == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    mc_registry_classes *classes = statistic->classes;
    if (classes->entered == 0)
        return mc_registry_fail(classes, MC_REGISTRY_INVALID_ARGUMENT,
                                "Statistic flag requires admitted initialization", NULL);
    statistic->source_class = source_class(classes, MC_STATISTIC_BASE);
    statistic->source_pc = 2;
    statistic->fields.g = true;
    statistic->source_pc = 6;
    return MC_REGISTRY_OK;
}

mc_registry_status mc_registry_statistic_flag(mc_statistic *statistic)
{
    if (statistic != NULL && statistic->kind == MC_STATISTIC_ACHIEVEMENT)
        return mc_registry_achievement_flag(statistic);
    return mc_registry_statistic_base_flag(statistic);
}

const mc_statistic_fields *mc_statistic_fields_get(const mc_statistic *statistic)
{
    return statistic == NULL ? NULL : &statistic->fields;
}

mc_statistic_kind mc_statistic_kind_get(const mc_statistic *statistic)
{
    return statistic == NULL ? MC_STATISTIC_KIND_COUNT : statistic->kind;
}

mc_registry_status mc_statistic_item_id(const mc_statistic *statistic, int32_t *out)
{
    if (statistic == NULL || out == NULL || statistic->kind != MC_STATISTIC_ITEM)
        return MC_REGISTRY_INVALID_ARGUMENT;
    *out = statistic->associated_id;
    return MC_REGISTRY_OK;
}
