#include "effects.h"
#include <stdlib.h>

static bool fail(mc_client_effect_registration_error *e, mc_texture_effect_status status,
                 const char *operation, int32_t pc) {
    e->status = status;
    e->operation = operation;
    e->ji_pc = pc;
    return false;
}

bool mc_client_render_engine_register_effect(mc_client_render_engine *engine,
                                             mc_client_texture_effect *effect, size_t maximum,
                                             mc_client_effect_registration_error *out) {
    if (out == NULL)
        return false;
    *out = (mc_client_effect_registration_error){.ji_pc = -1};
    if (engine == NULL)
        return fail(out, MC_TEXTURE_EFFECT_NULL_REFERENCE, "ji.a actual receiver", -1);
    if (engine->phase != MC_CLIENT_RENDER_ENGINE_READY || engine->effects_retiring ||
        engine->effects_retired)
        return fail(out, MC_TEXTURE_EFFECT_INVALID_PHASE, "ji native effect-container admission",
                    -1);
    if (engine->active_resource_calls != 0 || engine->classes->active_source_calls != 0)
        return fail(out, MC_TEXTURE_EFFECT_BUSY, "ji entered source/resource call", -1);
    out->engine = mc_client_render_engine_retain(engine);
    if (out->engine != MC_CLIENT_RENDER_ENGINE_OK)
        return fail(out, MC_TEXTURE_EFFECT_BUSY, "ji native registration call hold", -1);
    ++engine->active_resource_calls;
    mc_client_render_list *list = (mc_client_render_list *)engine->fields.g; /* actual ji g1 */
    bool ok = false, held = false;
    if (list == NULL) {
        fail(out, MC_TEXTURE_EFFECT_NULL_REFERENCE, "ji actual g List.add receiver", 5);
        goto leave;
    }
    if (maximum == 0)
        maximum = 4096;
    if (list->count >= maximum || list->count == SIZE_MAX) {
        fail(out, MC_TEXTURE_EFFECT_LIMIT, "actual g List native bound", 5);
        goto leave;
    }
    if (effect != NULL) {
        if (mc_client_texture_effect_retain(effect, &out->child) != MC_TEXTURE_EFFECT_OK) {
            fail(out, out->child.status, "actual g occurrence native hold", 5);
            goto leave;
        }
        held = true;
    }
    if (list->count == list->capacity) {
        size_t capacity = list->capacity == 0 ? 10 : list->capacity + list->capacity / 2 + 1;
        if (capacity < list->capacity || capacity > maximum)
            capacity = maximum;
        if (capacity <= list->count || capacity > SIZE_MAX / sizeof(*list->entries)) {
            fail(out, MC_TEXTURE_EFFECT_LIMIT, "actual g List allocation size", 5);
            goto leave;
        }
        void **entries = realloc(list->entries, capacity * sizeof(*entries));
        if (entries == NULL) {
            fail(out, MC_TEXTURE_EFFECT_ALLOCATION_FAILED, "actual g List.add storage", 5);
            goto leave;
        }
        list->entries = entries;
        list->capacity = capacity;
    }
    const size_t at = list->count;
    list->entries[at] = effect;
    list->count = at + 1;
    out->appended = true;
    held = false;
    out->ji_pc = 12;
    if (mc_client_texture_effect_tick(effect, &out->child) != MC_TEXTURE_EFFECT_OK) {
        fail(out, out->child.status, "actual appended aw.a child", 12);
        goto leave;
    }
    out->child_returned = true;
    out->source_returned = true;
    out->ji_pc = 15;
    ok = true;
leave:
    if (held) {
        /* Nonfinal, already-held occurrence; no source callback/Registry
         * retirement can occur while the caller's input occurrence remains. */
        mc_client_texture_effect *temporary = effect;
        const mc_texture_effect_status dropped =
            mc_client_texture_effect_release(&temporary, &out->child);
        if (dropped != MC_TEXTURE_EFFECT_OK)
            ok = fail(out, dropped, "unappended native occurrence cleanup", -1);
    }
    --engine->active_resource_calls;
    out->engine = mc_client_render_engine_release(engine);
    if (out->engine != MC_CLIENT_RENDER_ENGINE_OK)
        ok = fail(out, MC_TEXTURE_EFFECT_BUSY, "ji registration native finalization", -1);
    return ok;
}

bool mc_client_render_engine_retire_effects(mc_client_render_engine *engine,
                                            mc_client_effect_registration_error *out) {
    if (out == NULL)
        return false;
    *out = (mc_client_effect_registration_error){.ji_pc = -1};
    if (engine == NULL)
        return fail(out, MC_TEXTURE_EFFECT_INVALID_ARGUMENT, "Engine effect retirement receiver",
                    -1);
    if (engine->phase == MC_CLIENT_RENDER_ENGINE_CONSTRUCTING ||
        engine->active_resource_calls != 0 || engine->classes->active_source_calls != 0)
        return fail(out, MC_TEXTURE_EFFECT_BUSY, "Engine entered effect retirement", -1);
    if (engine->effects_retired)
        return true;
    engine->effects_retiring = true;
    mc_client_render_list *list = (mc_client_render_list *)engine->fields.g;
    ++engine->active_resource_calls;
    if (list != NULL) {
        while (engine->effect_release_index < list->count) {
            mc_client_texture_effect *occurrence = list->entries[engine->effect_release_index];
            if (mc_client_texture_effect_release(&occurrence, &out->child) !=
                MC_TEXTURE_EFFECT_OK) {
                --engine->active_resource_calls;
                return fail(out, out->child.status, "Engine retained effect occurrence retirement",
                            -1);
            }
            ++engine->effect_release_index;
        }
    }
    engine->effects_retired = true;
    --engine->active_resource_calls;
    return true;
}

bool mc_client_render_engine_effect_at(const mc_client_render_engine *engine, size_t index,
                                       const mc_client_texture_effect **out) {
    if (engine == NULL || out == NULL || engine->effects_retiring || engine->effects_retired)
        return false;
    const mc_client_render_list *list = engine->fields.g;
    if (list == NULL || index >= list->count)
        return false;
    *out = list->entries[index];
    return true;
}
