#include "colorizer_internal.h"
#include "internal.h"

static const char *class_name(mc_client_colorizer_kind kind) {
    static const char *const names[MC_CLIENT_COLORIZER_COUNT] = {"hw", "ia", "jh"};
    return names[(size_t)kind];
}

static mc_client_colorizer_status fail(mc_client_colorizer_error *e,
                                       mc_client_colorizer_status status, const char *operation,
                                       mc_client_colorizer_kind kind, const char *method,
                                       const char *descriptor, int32_t pc, bool native) {
    e->status = status;
    e->operation = operation;
    e->native_policy = native;
    e->site = (mc_client_graphics_site){
        (unsigned)kind < MC_CLIENT_COLORIZER_COUNT ? class_name(kind) : NULL, method, descriptor,
        pc};
    return status;
}

mc_client_colorizer_options mc_client_colorizer_default_options(void) {
    mc_client_colorizer_options result;
    for (size_t i = 0; i < MC_CLIENT_COLORIZER_COUNT; ++i)
        result.initial_array[i] = mc_client_color_array_default_limits();
    return result;
}

static bool options_equal(const mc_client_colorizer_options *a,
                          const mc_client_colorizer_options *b) {
    for (size_t i = 0; i < MC_CLIENT_COLORIZER_COUNT; ++i)
        if (a->initial_array[i].max_native_bytes != b->initial_array[i].max_native_bytes ||
            a->initial_array[i].max_allocations != b->initial_array[i].max_allocations)
            return false;
    return true;
}

static mc_client_colorizer_status begin(mc_client_render_engine_classes *c,
                                        mc_client_colorizer_kind kind,
                                        const mc_client_colorizer_options *requested,
                                        mc_client_colorizer_error *e) {
    if (c == NULL || (unsigned)kind >= MC_CLIENT_COLORIZER_COUNT)
        return fail(e, MC_CLIENT_COLORIZER_INVALID_ARGUMENT, "colorizer native receiver/kind",
                    MC_CLIENT_COLORIZER_COUNT, NULL, NULL, -1, true);
    if (c->resource_retirement_started)
        return fail(e, MC_CLIENT_COLORIZER_INVALID_PHASE, "colorizer native namespace retiring",
                    kind, NULL, NULL, -1, true);
    if (c->active_source_calls != 0 || c->active_resource_calls != 0 ||
        c->nw.phase == MC_CLIENT_RENDER_ENGINE_CONSTRUCTING)
        return fail(e, MC_CLIENT_COLORIZER_BUSY, "colorizer namespace native call active", kind,
                    NULL, NULL, -1, true);
    const mc_client_colorizer_options options =
        requested == NULL ? (c->colorizers.options_set ? c->colorizers.options
                                                       : mc_client_colorizer_default_options())
                          : *requested;
    if (c->colorizers.options_set && !options_equal(&options, &c->colorizers.options))
        return fail(e, MC_CLIENT_COLORIZER_INVALID_ARGUMENT, "colorizer fixed native policy", kind,
                    NULL, NULL, -1, true);
    e->owner = mc_client_render_engine_classes_retain(c);
    if (e->owner != MC_CLIENT_RENDER_ENGINE_OK)
        return fail(e, MC_CLIENT_COLORIZER_LIMIT, "colorizer namespace native call hold", kind,
                    NULL, NULL, -1, true);
    if (!c->colorizers.options_set) {
        c->colorizers.options = options;
        c->colorizers.options_set = true;
    }
    ++c->active_source_calls;
    return MC_CLIENT_COLORIZER_OK;
}

static mc_client_colorizer_status initialize(mc_client_render_engine_classes *c,
                                             mc_client_colorizer_kind kind,
                                             mc_client_colorizer_error *e) {
    mc_client_colorizer_class *owner = &c->colorizers.classes[(size_t)kind];
    if (owner->phase == MC_CLIENT_COLORIZER_READY)
        return MC_CLIENT_COLORIZER_OK;
    if (owner->phase == MC_CLIENT_COLORIZER_FAILED) {
        *e = owner->failure;
        e->status = MC_CLIENT_COLORIZER_CLASS_FAILED;
        e->operation = "previous actual colorizer class initialization failed";
        return e->status;
    }
    if (owner->phase == MC_CLIENT_COLORIZER_CONSTRUCTING)
        return fail(e, MC_CLIENT_COLORIZER_BUSY, "colorizer initializer native active", kind,
                    "<clinit>", "()V", -1, true);
    owner->phase = MC_CLIENT_COLORIZER_CONSTRUCTING;
    const mc_client_graphics_site allocation = {class_name(kind), "<clinit>", "()V", 2};
    if (mc_client_color_array_create(65536, &c->colorizers.options.initial_array[(size_t)kind],
                                     allocation, &owner->candidate,
                                     &e->array) != MC_CLIENT_COLOR_ARRAY_OK) {
        fail(e, MC_CLIENT_COLORIZER_ARRAY_FAILED, "actual colorizer zero65536 array", kind,
             "<clinit>", "()V", 2, e->array.native_policy);
        owner->failure = *e;
        owner->phase = MC_CLIENT_COLORIZER_FAILED;
        return e->status;
    }
    owner->a = owner->candidate;
    owner->candidate = NULL; /* Actual putstatic4. */
    owner->last_store_pc = 4;
    owner->phase = MC_CLIENT_COLORIZER_READY;
    return MC_CLIENT_COLORIZER_OK;
}

static void end(mc_client_render_engine_classes *c) {
    --c->active_source_calls;
    (void)mc_client_render_engine_classes_release(c); /* Nonfinal balanced call hold. */
}

mc_client_colorizer_status
mc_client_colorizer_initialize(mc_client_render_engine_classes *c, mc_client_colorizer_kind kind,
                               const mc_client_colorizer_options *options,
                               mc_client_colorizer_error *error) {
    mc_client_colorizer_error e = {0};
    mc_client_colorizer_status status = begin(c, kind, options, &e);
    if (status == MC_CLIENT_COLORIZER_OK) {
        status = initialize(c, kind, &e);
        end(c);
    }
    if (error != NULL)
        *error = e;
    return status;
}

mc_client_colorizer_status mc_client_colorizer_set(mc_client_render_engine_classes *c,
                                                   mc_client_colorizer_kind kind,
                                                   mc_client_color_array *array,
                                                   const mc_client_colorizer_options *options,
                                                   mc_client_colorizer_error *error) {
    mc_client_colorizer_error e = {0};
    mc_client_colorizer_status status = begin(c, kind, options, &e);
    if (status == MC_CLIENT_COLORIZER_OK) {
        status = initialize(c, kind, &e); /* Active use precedes actual setter body. */
        if (status == MC_CLIENT_COLORIZER_OK) {
            mc_client_colorizer_class *owner = &c->colorizers.classes[(size_t)kind];
            mc_client_color_array_state state;
            if (array != NULL && (!mc_client_color_array_inspect(array, &state) ||
                                  state.phase != MC_CLIENT_COLOR_ARRAY_READY)) {
                status = fail(&e, MC_CLIENT_COLORIZER_INVALID_PHASE, "incoming array native phase",
                              kind, "a", "([I)V", -1, true);
            } else if (array != NULL &&
                       mc_client_color_array_retain(array) != MC_CLIENT_COLOR_ARRAY_OK) {
                e.array =
                    (mc_client_color_array_error){.status = MC_CLIENT_COLOR_ARRAY_LIMIT,
                                                  .operation = "incoming static array native hold",
                                                  .site = {class_name(kind), "a", "([I)V", -1},
                                                  .native_policy = true};
                status = fail(&e, MC_CLIENT_COLORIZER_LIMIT, "incoming static array native hold",
                              kind, "a", "([I)V", -1, true);
            } else {
                mc_client_color_array *incoming = array;
                if (!mc_client_color_array_can_release(owner->a)) {
                    (void)mc_client_color_array_release(&incoming,
                                                        NULL); /* Unpublished reserved hold. */
                    status = fail(&e, MC_CLIENT_COLORIZER_BUSY,
                                  "displaced final array native call active", kind, "a", "([I)V",
                                  -1, true);
                } else {
                    mc_client_color_array *previous = owner->a;
                    owner->a = incoming;
                    owner->last_store_pc = 1; /* Actual same-reference putstatic1. */
                    (void)mc_client_color_array_release(&previous,
                                                        NULL); /* Preflight + no callbacks. */
                }
            }
        }
        end(c);
    }
    if (error != NULL)
        *error = e;
    return status;
}

bool mc_client_colorizer_inspect(const mc_client_render_engine_classes *c,
                                 mc_client_colorizer_kind kind, mc_client_colorizer_state *out) {
    if (c == NULL || out == NULL || (unsigned)kind >= MC_CLIENT_COLORIZER_COUNT)
        return false;
    const mc_client_colorizer_class *owner = &c->colorizers.classes[(size_t)kind];
    *out = (mc_client_colorizer_state){owner->phase, owner->a, owner->candidate,
                                       owner->last_store_pc, owner->failure};
    return true;
}

size_t mc_client_colorizer_occurrences(const mc_client_render_engine_classes *c,
                                       const mc_client_color_array *array) {
    size_t count = 0;
    if (array == NULL)
        return 0;
    for (size_t i = 0; i < MC_CLIENT_COLORIZER_COUNT; ++i) {
        count += c->colorizers.classes[i].a == array ? 1u : 0u;
        count += c->colorizers.classes[i].candidate == array ? 1u : 0u;
    }
    return count;
}

bool mc_client_colorizer_can_retire(const mc_client_render_engine_classes *c) {
    for (size_t i = 0; i < MC_CLIENT_COLORIZER_COUNT; ++i) {
        const mc_client_colorizer_class *owner = &c->colorizers.classes[i];
        if (owner->phase == MC_CLIENT_COLORIZER_CONSTRUCTING ||
            !mc_client_color_array_can_release_occurrences(
                owner->a, mc_client_colorizer_occurrences(c, owner->a)) ||
            !mc_client_color_array_can_release_occurrences(
                owner->candidate, mc_client_colorizer_occurrences(c, owner->candidate)))
            return false;
    }
    return true;
}

void mc_client_colorizer_free_namespace(mc_client_render_engine_classes *c) {
    /* Caller proved final alias counts and holds the exclusive namespace. */
    for (size_t i = 0; i < MC_CLIENT_COLORIZER_COUNT; ++i) {
        (void)mc_client_color_array_release(&c->colorizers.classes[i].a, NULL);
        (void)mc_client_color_array_release(&c->colorizers.classes[i].candidate, NULL);
    }
}
