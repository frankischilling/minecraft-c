#include "mc/block/registry/step_sound.h"
#include "managed_internal.h"
#include "registry_internal.h"
#include "runtime_internal.h"
#include <string.h>

struct mc_step_sound_name_owner {
    mc_registry_string value; /* Initial subobject: exact String identity. */
    uint64_t marker;
    mc_registry_classes *classes;
    size_t holds;
};

#define NAME_MARKER UINT64_C(0x53544550534e414d)

static mc_registry_status result(mc_registry_error *e, mc_registry_status s, const char *message) {
    if (e != NULL)
        *e = (mc_registry_error){.status = s,
                                 .message = message,
                                 .initializer_pc = -1,
                                 .block_id = -1,
                                 .constructor_pc = -1,
                                 .active_initializer_pc = -1,
                                 .item_argument = -1};
    return s;
}

static mc_registry_status borrow(mc_registry_classes *c, mc_registry_error *e) {
    if (c == NULL)
        return result(e, MC_REGISTRY_INVALID_ARGUMENT, "StepSound code owner");
    return mc_registry_classes_retain(c, c->options.side, c->options.random_profile, e);
}

mc_registry_status mc_registry_step_sound_volume(const mc_block_sound *s, float *out,
                                                 mc_registry_error *e) {
    if (out == NULL)
        return result(e, MC_REGISTRY_INVALID_ARGUMENT, "StepSound volume output");
    if (s == NULL)
        return result(e, MC_REGISTRY_NULL_REFERENCE, "StepSound b() null receiver");
    *out = s->fields.b;
    return result(e, MC_REGISTRY_OK, NULL);
}

mc_registry_status mc_registry_step_sound_pitch(const mc_block_sound *s, float *out,
                                                mc_registry_error *e) {
    if (out == NULL)
        return result(e, MC_REGISTRY_INVALID_ARGUMENT, "StepSound pitch output");
    if (s == NULL)
        return result(e, MC_REGISTRY_NULL_REFERENCE, "StepSound c() null receiver");
    *out = s->fields.c;
    return result(e, MC_REGISTRY_OK, NULL);
}

static bool equal_ascii(const mc_registry_string *s, const char *text) {
    const size_t n = strlen(text);
    if (s == NULL || s->length != n)
        return false;
    for (size_t i = 0; i < n; ++i)
        if (s->units[i] != (unsigned char)text[i])
            return false;
    return true;
}

static mc_step_sound_name_owner *managed_name(mc_registry_classes *c,
                                              const mc_registry_string *name) {
    size_t extent;
    if (!mc_registry_managed_extent(c, name, &extent) || extent < sizeof(mc_step_sound_name_owner))
        return NULL;
    mc_step_sound_name_owner *owner = (mc_step_sound_name_owner *)(void *)name;
    if (owner->marker != NAME_MARKER || owner->classes != c || owner->holds == 0)
        return NULL;
    return owner;
}

mc_registry_status mc_registry_step_sound_name(mc_registry_classes *c, const mc_block_sound *sound,
                                               bool method_a, mc_step_sound_name *out,
                                               mc_registry_error *e) {
    if (c == NULL || out == NULL)
        return result(e, MC_REGISTRY_INVALID_ARGUMENT, "StepSound name arguments");
    if (sound == NULL)
        return result(e, MC_REGISTRY_NULL_REFERENCE, "StepSound name null receiver");
    if (sound->kind > 2)
        return result(e, MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY, "StepSound unsupported subclass");
    mc_registry_status status = borrow(c, e);
    if (status != MC_REGISTRY_OK)
        return status;
    const mc_registry_string *literal = NULL;
    if (method_a && sound->kind != 0) {
        status = mc_registry_literal_ascii(c, sound->kind == 1 ? "random.glass" : "step.gravel",
                                           &literal);
        if (status != MC_REGISTRY_OK) {
            (void)mc_registry_runtime_borrow_release(c);
            if (e != NULL)
                *e = c->last_error;
            return status;
        }
        *out = (mc_step_sound_name){.string = literal, .classes = c};
        return result(e, MC_REGISTRY_OK, NULL);
    }
    const mc_registry_string *suffix = sound->fields.a;
    const size_t length = suffix == NULL ? 4 : suffix->length;
    if (length > MC_STEP_SOUND_MAX_TEXT_UNITS - 5u ||
        length + 5u > (SIZE_MAX - sizeof(mc_step_sound_name_owner)) / sizeof(uint16_t)) {
        (void)mc_registry_runtime_borrow_release(c);
        return result(e, MC_REGISTRY_LIMIT, "StepSound String length");
    }
    void *storage = NULL;
    status = mc_registry_managed_allocate(
        c, sizeof(mc_step_sound_name_owner) + (length + 5u) * sizeof(uint16_t), &storage);
    if (status != MC_REGISTRY_OK) {
        (void)mc_registry_runtime_borrow_release(c);
        return result(e, status, "StepSound managed String allocation");
    }
    mc_step_sound_name_owner *owner = storage;
    uint16_t *units = (uint16_t *)(owner + 1);
    static const uint16_t prefix[] = {'s', 't', 'e', 'p', '.'}, null_name[] = {'n', 'u', 'l', 'l'};
    memcpy(units, prefix, sizeof(prefix));
    if (length != 0)
        memcpy(units + 5, suffix == NULL ? null_name : suffix->units, length * sizeof(*units));
    owner->value = (mc_registry_string){.units = units, .length = length + 5u};
    owner->marker = NAME_MARKER;
    owner->classes = c;
    owner->holds = 1;
    *out = (mc_step_sound_name){.string = &owner->value, .classes = c, .owner = owner};
    return result(e, MC_REGISTRY_OK, NULL);
}

mc_registry_status mc_registry_step_sound_name_retain(mc_registry_classes *c,
                                                      const mc_registry_string *name,
                                                      mc_step_sound_name *out,
                                                      mc_registry_error *e) {
    if (c == NULL || name == NULL || out == NULL)
        return result(e, MC_REGISTRY_INVALID_ARGUMENT, "StepSound retain arguments");
    mc_step_sound_name_owner *owner = managed_name(c, name);
    if (owner != NULL && owner->holds == SIZE_MAX)
        return result(e, MC_REGISTRY_LIMIT, "StepSound String holds");
    if (owner == NULL) {
        const char *key = equal_ascii(name, "random.glass")  ? "random.glass"
                          : equal_ascii(name, "step.gravel") ? "step.gravel"
                                                             : NULL;
        if (key == NULL)
            return result(e, MC_REGISTRY_INVALID_ARGUMENT, "String is not a StepSound result");
        const mc_registry_string *actual = NULL;
        mc_registry_status status = mc_registry_literal_ascii(c, key, &actual);
        if (status != MC_REGISTRY_OK) {
            if (e != NULL)
                *e = c->last_error;
            return status;
        }
        if (actual != name)
            return result(e, MC_REGISTRY_INVALID_ARGUMENT, "foreign StepSound literal");
    }
    mc_registry_status status = borrow(c, e);
    if (status != MC_REGISTRY_OK)
        return status;
    if (owner != NULL)
        ++owner->holds;
    *out = (mc_step_sound_name){.string = name, .classes = c, .owner = owner};
    return result(e, MC_REGISTRY_OK, NULL);
}

mc_registry_status mc_registry_step_sound_name_release(mc_step_sound_name *name,
                                                       mc_registry_error *e) {
    if (name == NULL)
        return result(e, MC_REGISTRY_INVALID_ARGUMENT, "StepSound release result");
    if (name->classes == NULL) {
        if (name->string != NULL || name->owner != NULL)
            return result(e, MC_REGISTRY_INVALID_ARGUMENT, "StepSound release owner");
        return result(e, MC_REGISTRY_OK, NULL);
    }
    mc_registry_classes *c = name->classes;
    if (c->runtime_borrows == 0)
        return result(e, MC_REGISTRY_INVALID_ARGUMENT, "StepSound missing class hold");
    if (name->owner != NULL) {
        mc_step_sound_name_owner *owner = managed_name(c, name->string);
        if (owner != name->owner)
            return result(e, MC_REGISTRY_INVALID_ARGUMENT, "StepSound release identity");
        if (--owner->holds == 0)
            mc_registry_managed_release(c, owner);
    }
    (void)mc_registry_runtime_borrow_release(c);
    *name = (mc_step_sound_name){0};
    return result(e, MC_REGISTRY_OK, NULL);
}
