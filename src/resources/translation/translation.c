#include "../../block/registry/registry_internal.h"

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

/* Allocation already recorded the native first cause and active initializer.
 * The constructor is nested beneath nh/ii.<clinit> PC4, not a new initializer. */
static mc_registry_status constructor_error(mc_translation *translation) {
    mc_registry_classes *classes = translation->classes;
    classes->last_error.constructor_class =
        classes->options.side == MC_REGISTRY_CLIENT ? "nh" : "ii";
    classes->last_error.constructor_pc = translation->constructor_pc;
    return classes->last_error.status;
}

static mc_registry_status constructor_fail(mc_translation *translation, mc_registry_status status,
                                           const char *message) {
    (void)mc_registry_fail(translation->classes, status, message, NULL);
    return constructor_error(translation);
}

static void *properties_allocate(void *context, size_t bytes) {
    mc_translation *translation = context;
    void *storage = mc_registry_allocate(translation->classes, bytes);
    if (storage == NULL)
        (void)constructor_error(translation);
    return storage;
}

static const mc_registry_string *bound_string(const mc_translation *translation,
                                              const mc_java_properties_string *source) {
    for (const mc_translation_string_binding *binding = translation->strings; binding != NULL;
         binding = binding->next)
        if (binding->source == source)
            return binding->string;
    return NULL;
}

static mc_registry_status bind_string(mc_translation *translation,
                                      const mc_java_properties_string *source) {
    if (bound_string(translation, source) != NULL)
        return MC_REGISTRY_OK;
    mc_java_properties_string_view view;
    if (!mc_java_properties_string_read(source, &view))
        return constructor_fail(translation, MC_REGISTRY_INVALID_ARGUMENT,
                                "Translation Properties String view");
    mc_registry_classes *classes = translation->classes;
    mc_translation_string_binding *binding = mc_registry_allocate(classes, sizeof(*binding));
    if (binding == NULL)
        return constructor_error(translation);
    mc_registry_string *string = mc_registry_allocate(classes, sizeof(*string));
    if (string == NULL)
        return constructor_error(translation);
    /* Both owners live in the same retained allocation ledger. Equal content
     * does not merge distinct Properties String identities. */
    string->units = view.units;
    string->length = view.length;
    binding->source = source;
    binding->string = string;
    binding->next = translation->strings;
    translation->strings = binding;
    return MC_REGISTRY_OK;
}

static mc_registry_status bind_entries(mc_translation *translation) {
    const size_t count = mc_java_properties_size(translation->b);
    for (size_t i = 0; i < count; ++i) {
        const mc_java_properties_string *key = NULL, *value = NULL;
        if (mc_java_properties_entry(translation->b, i, &key, &value) != MC_JAVA_PROPERTIES_OK)
            return constructor_fail(translation, MC_REGISTRY_INVALID_ARGUMENT,
                                    "Translation Properties entry view");
        mc_registry_status status = bind_string(translation, key);
        if (status != MC_REGISTRY_OK)
            return status;
        status = bind_string(translation, value);
        if (status != MC_REGISTRY_OK)
            return status;
    }
    return MC_REGISTRY_OK;
}

static mc_registry_status properties_failure(mc_translation *translation,
                                             mc_java_properties_status status) {
    switch (status) {
        case MC_JAVA_PROPERTIES_OK:
            return MC_REGISTRY_OK;
        case MC_JAVA_PROPERTIES_ALLOCATION_FAILED:
            /* The real arena callback distinguishes LIMIT from malloc failure. */
            return constructor_error(translation);
        case MC_JAVA_PROPERTIES_LIMIT:
            return constructor_fail(translation, MC_REGISTRY_LIMIT,
                                    "Translation Properties native size bound");
        case MC_JAVA_PROPERTIES_MALFORMED_UNICODE:
            return constructor_fail(translation, MC_REGISTRY_ILLEGAL_ARGUMENT,
                                    "Translation Properties malformed Unicode escape");
        case MC_JAVA_PROPERTIES_IO_ERROR:
            /* Only IOException is caught, over constructor PCs15..42. The
             * source prints it at48 before returning; that effect is unfinished. */
            translation->constructor_pc = 48;
            return constructor_fail(
                translation, MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY,
                "Translation caught IOException: printStackTrace at48 unfinished");
        default:
            return constructor_fail(translation, MC_REGISTRY_INVALID_ARGUMENT,
                                    "Translation Properties native operation");
    }
}

static mc_registry_status load_resource(mc_translation *translation, const char *name,
                                        int32_t literal_pc, int32_t open_pc, int32_t load_pc) {
    mc_registry_classes *classes = translation->classes;
    /* ldc Class is a passive literal, not an initialization of another class.
     * The actual resource String belongs to the owner's shared literal pool. */
    translation->constructor_pc = literal_pc;
    const mc_registry_string *literal = NULL;
    mc_registry_status status = mc_registry_literal_ascii(classes, name, &literal);
    if (status != MC_REGISTRY_OK)
        return constructor_error(translation);
    translation->constructor_pc = open_pc;
    const mc_registry_resources *resources = classes->options.resources;
    if (resources == NULL || resources->open == NULL)
        return constructor_fail(translation, MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY,
                                "Translation Class.getResourceAsStream source unavailable");
    mc_java_properties_input input = {0};
    status = resources->open(resources->context, name, &input);
    if (status != MC_REGISTRY_OK && status != MC_REGISTRY_NULL_REFERENCE)
        return constructor_fail(translation, status, "Translation resource source failed");
    translation->constructor_pc = load_pc;
    if (status == MC_REGISTRY_NULL_REFERENCE)
        return constructor_fail(translation, MC_REGISTRY_NULL_REFERENCE,
                                "Translation Properties.load null InputStream");
    if (input.read == NULL)
        return constructor_fail(translation, MC_REGISTRY_INVALID_ARGUMENT,
                                "Translation source supplied no byte reader");
    mc_java_properties_status loaded = mc_java_properties_load(translation->b, &input);
    status = properties_failure(translation, loaded);
    const mc_registry_error primary = classes->last_error;
    /* Eager adapters have no Java publication of their own. Keep old bindings
     * and the real decoded prefix even when the load exits abruptly. A native
     * binding failure must not replace the earlier load exception/cause. */
    mc_registry_status bound = bind_entries(translation);
    if (status != MC_REGISTRY_OK) {
        classes->last_error = primary;
        return status;
    }
    return bound;
}

static mc_registry_status construct(mc_translation *translation) {
    translation->constructor_pc = 1; /* Object.<init> has no reached game fields. */
    translation->constructor_pc = 5;
    mc_java_properties_allocator allocator = {translation, properties_allocate};
    mc_java_properties *properties = NULL;
    mc_java_properties_status created = mc_java_properties_create(&allocator, &properties);
    if (created != MC_JAVA_PROPERTIES_OK)
        return properties_failure(translation, created);
    translation->constructor_pc = 9;
    translation->constructor_pc = 12;
    translation->b = properties;
    mc_registry_status status = load_resource(translation, "/lang/en_US.lang", 21, 23, 26);
    if (status != MC_REGISTRY_OK)
        return status;
    status = load_resource(translation, "/lang/stats_US.lang", 35, 37, 40);
    if (status != MC_REGISTRY_OK)
        return status;
    translation->constructor_pc = 51;
    return MC_REGISTRY_OK;
}

static mc_registry_status cached_state(mc_registry_classes *classes,
                                       const mc_registry_class_state *state) {
    if (state->phase == MC_REGISTRY_FAILED) {
        classes->last_error = state->failure;
        classes->last_error.status = MC_REGISTRY_FAILED_INITIALIZATION;
        return MC_REGISTRY_FAILED_INITIALIZATION;
    }
    /* An INITIALIZING class on this already admitted synchronous flow is not
     * restarted. Its actual static fields may still have default null values. */
    return MC_REGISTRY_OK;
}

static void finish_state(mc_registry_classes *classes, mc_registry_class_state *state,
                         mc_registry_initializer *frame, mc_registry_status status) {
    state->phase = status == MC_REGISTRY_OK ? MC_REGISTRY_READY : MC_REGISTRY_FAILED;
    if (status != MC_REGISTRY_OK)
        state->failure = classes->last_error;
    mc_registry_initializer_leave(classes, frame);
}

static mc_registry_status singleton_active_use(mc_registry_classes *classes) {
    mc_registry_class_state *state = &classes->translation_state;
    if (state->phase != MC_REGISTRY_COLD)
        return cached_state(classes, state);
    state->phase = MC_REGISTRY_INITIALIZING;
    state->pc = 0;
    mc_registry_initializer frame;
    mc_registry_initializer_enter(classes, &frame, "nh", "ii", &state->pc);
    mc_registry_status status = MC_REGISTRY_OK;
    mc_translation *translation = mc_registry_allocate(classes, sizeof(*translation));
    if (translation == NULL) {
        status = classes->last_error.status;
        goto done;
    }
    translation->classes = classes;
    translation->constructor_pc = -1;
    classes->translations.pending = translation;
    state->pc = 4;
    status = construct(translation);
    if (status != MC_REGISTRY_OK)
        goto done;
    state->pc = 7;
    classes->translations.singleton_a = translation;
    state->pc = 10;
done:
    finish_state(classes, state, &frame, status);
    return status;
}

mc_registry_status mc_registry_translation_active_use(mc_registry_classes *classes) {
    if (classes == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    if (classes->entered == 0)
        return mc_registry_fail(classes, MC_REGISTRY_INVALID_ARGUMENT,
                                "Translation active use requires an admitted initialization flow",
                                NULL);
    mc_registry_class_state *state = &classes->translation_wrapper_state;
    if (state->phase != MC_REGISTRY_COLD)
        return cached_state(classes, state);
    state->phase = MC_REGISTRY_INITIALIZING;
    state->pc = 0;
    mc_registry_initializer frame;
    mc_registry_initializer_enter(classes, &frame, "do", "ch", &state->pc);
    mc_registry_status status = singleton_active_use(classes);
    if (status == MC_REGISTRY_OK) {
        /* nh/ii.a() is the actual getstatic at0, including same-flow defaults. */
        mc_translation *translation = classes->translations.singleton_a;
        state->pc = 3;
        classes->translations.wrapper_a = translation;
        state->pc = 6;
    }
    finish_state(classes, state, &frame, status);
    return status;
}

mc_registry_status mc_registry_classes_initialize_translation(mc_registry_classes *classes,
                                                              mc_registry_error *error) {
    if (classes == NULL)
        return diagnostic(error, MC_REGISTRY_INVALID_ARGUMENT, "Translation registry owner");
    if (classes->entered != 0)
        return diagnostic(error, MC_REGISTRY_BUSY, "registry initialization entered");
    const mc_registry_class_state *state = &classes->translation_wrapper_state;
    if (state->phase == MC_REGISTRY_READY)
        return diagnostic(error, MC_REGISTRY_OK, NULL);
    if (state->phase == MC_REGISTRY_INITIALIZING)
        return diagnostic(error, MC_REGISTRY_BUSY, "registry initialization entered");
    if (state->phase == MC_REGISTRY_FAILED) {
        if (error != NULL) {
            *error = state->failure;
            error->status = MC_REGISTRY_FAILED_INITIALIZATION;
        }
        return MC_REGISTRY_FAILED_INITIALIZATION;
    }
    ++classes->entered;
    mc_registry_status status = mc_registry_translation_active_use(classes);
    --classes->entered;
    if (status == MC_REGISTRY_OK)
        return diagnostic(error, MC_REGISTRY_OK, NULL);
    if (error != NULL)
        *error = classes->last_error;
    return status;
}

mc_registry_status mc_registry_classes_inspect_translation(const mc_registry_classes *classes,
                                                           mc_registry_translation_view *out,
                                                           mc_registry_error *error) {
    if (classes == NULL || out == NULL)
        return diagnostic(error, MC_REGISTRY_INVALID_ARGUMENT, "Translation view arguments");
    *out = (mc_registry_translation_view){.singleton_phase = classes->translation_state.phase,
                                          .wrapper_phase = classes->translation_wrapper_state.phase,
                                          .singleton_initializer_pc = classes->translation_state.pc,
                                          .wrapper_initializer_pc =
                                              classes->translation_wrapper_state.pc,
                                          .singleton_a = classes->translations.singleton_a,
                                          .wrapper_a = classes->translations.wrapper_a};
    return diagnostic(error, MC_REGISTRY_OK, NULL);
}

static bool published(const mc_translation *translation) {
    return translation != NULL && translation->classes != NULL && translation->b != NULL &&
           translation->classes->translations.singleton_a == translation &&
           translation->classes->translation_state.phase == MC_REGISTRY_READY;
}

mc_registry_status mc_translation_property(const mc_translation *translation, const uint16_t *key,
                                           size_t length, const mc_registry_string **out,
                                           mc_registry_error *error) {
    if (!published(translation) || out == NULL || (key == NULL && length != 0))
        return diagnostic(error, MC_REGISTRY_INVALID_ARGUMENT,
                          "published Translation property arguments");
    const mc_java_properties_string *source = NULL;
    mc_java_properties_status status = mc_java_properties_get(translation->b, key, length, &source);
    if (status != MC_JAVA_PROPERTIES_OK)
        return diagnostic(error,
                          status == MC_JAVA_PROPERTIES_LIMIT ? MC_REGISTRY_LIMIT
                                                             : MC_REGISTRY_INVALID_ARGUMENT,
                          "Translation property key");
    const mc_registry_string *string = NULL;
    if (source != NULL) {
        string = bound_string(translation, source);
        if (string == NULL)
            return diagnostic(error, MC_REGISTRY_INVALID_ARGUMENT,
                              "Translation property missing retained String binding");
    }
    *out = string;
    return diagnostic(error, MC_REGISTRY_OK, NULL);
}

size_t mc_translation_property_count(const mc_translation *translation) {
    return published(translation) ? mc_java_properties_size(translation->b) : 0;
}

mc_registry_status mc_registry_translate(mc_registry_classes *classes,
                                         const mc_registry_string *key,
                                         const mc_registry_string **out) {
    if (classes == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    if (out == NULL)
        return mc_registry_fail(classes, MC_REGISTRY_INVALID_ARGUMENT, "Translation output", NULL);
    mc_registry_status status = mc_registry_translation_active_use(classes);
    if (status != MC_REGISTRY_OK)
        return status;
    const mc_translation *translation = classes->translations.wrapper_a;
    if (translation == NULL)
        return mc_registry_fail(classes, MC_REGISTRY_NULL_REFERENCE,
                                "Translation wrapper invokevirtual3 null receiver", NULL);
    if (key == NULL)
        return mc_registry_fail(classes, MC_REGISTRY_NULL_REFERENCE,
                                "Translation getProperty6 null key", NULL);
    const mc_registry_string *value = NULL;
    mc_registry_error error;
    status = mc_translation_property(translation, key->units, key->length, &value, &error);
    if (status != MC_REGISTRY_OK)
        return mc_registry_fail(classes, status, error.message, NULL);
    *out = value == NULL ? key : value;
    return MC_REGISTRY_OK;
}
