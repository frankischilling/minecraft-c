#include "effect_upload.h"
#include "../display/source_texture_update.h"
#include <string.h>

static bool failed(mc_client_effect_upload_error *e, mc_client_effect_upload_status status,
                   const char *operation, int32_t pc) {
    e->status = status;
    e->operation = operation;
    e->ji_pc = pc;
    return false;
}

static bool transfer(mc_client_render_engine *engine, const mc_texture_effect_fields *fields,
                     bool second, mc_client_effect_upload_error *e) {
    mc_client_direct_byte_buffer *buffer = (mc_client_direct_byte_buffer *)(void *)engine->fields.f;
    if (buffer == NULL)
        return failed(e, MC_CLIENT_EFFECT_UPLOAD_NULL_REFERENCE, "ji actual f clear receiver",
                      second ? 479 : 48);
    buffer->position = 0;
    buffer->limit = buffer->capacity; /* clear, before put's argument checks */
    if (fields->a == NULL)
        return failed(e, MC_CLIENT_EFFECT_UPLOAD_NULL_REFERENCE, "ji actual aw.a byte array",
                      second ? 491 : 60);
    if (buffer->data == NULL || fields->a_length > buffer->capacity)
        return failed(e, MC_CLIENT_EFFECT_UPLOAD_BUFFER_FAILED, "ji actual f put capacity",
                      second ? 491 : 60);
    memcpy(buffer->data, fields->a, fields->a_length);
    buffer->position = fields->a_length;
    buffer->position = 0;
    buffer->limit = fields->a_length;
    return true;
}

static bool bind_atlas(mc_client_render_engine *engine, mc_client_display *display,
                       const mc_texture_effect_fields *fields, mc_client_effect_upload_error *e) {
    static const uint16_t terrain[] = {'/', 't', 'e', 'r', 'r', 'a', 'i', 'n', '.', 'p', 'n', 'g'};
    static const uint16_t items[] = {'/', 'g', 'u', 'i', '/', 'i', 't',
                                     'e', 'm', 's', '.', 'p', 'n', 'g'};
    if (fields->f != 0 && fields->f != 1)
        return true; /* Actual aw42: neither source branch binds. */
    const mc_texture_pack_text key =
        fields->f == 0 ? (mc_texture_pack_text){terrain, sizeof(terrain) / sizeof(*terrain), false}
                       : (mc_texture_pack_text){items, sizeof(items) / sizeof(*items), false};
    mc_client_texture id = 0;
    /* Transfer the synchronous call guards to the existing ji.b owner. It
     * takes those same guards before any provider callback and releases them
     * before returning. The enclosing Engine occurrence remains held. */
    --engine->active_resource_calls;
    --engine->classes->active_resource_calls;
    const mc_client_texture_lookup_status status =
        mc_client_render_engine_texture(engine, display, NULL, key, &id, &e->lookup);
    ++engine->classes->active_resource_calls;
    ++engine->active_resource_calls;
    e->aw_pc = fields->f == 0 ? 13 : 36;
    if (status != MC_CLIENT_LOOKUP_OK)
        return failed(e, MC_CLIENT_EFFECT_UPLOAD_LOOKUP_FAILED, "aw actual ji.b atlas child", 83);
    e->aw_pc = fields->f == 0 ? 16 : 39;
    if (!mc_client_display_source_texture_bind(display, id, &e->display))
        return failed(e, MC_CLIENT_EFFECT_UPLOAD_DISPLAY_FAILED, "aw actual atlas glBindTexture",
                      83);
    e->aw_pc = 42;
    return true;
}

static bool sub_image(mc_client_render_engine *engine, mc_client_display *display, int32_t level,
                      int32_t x, int32_t y, int32_t size, int32_t pc,
                      mc_client_effect_upload_error *e) {
    const mc_client_direct_byte_buffer *buffer = engine->fields.f;
    if (buffer == NULL)
        return failed(e, MC_CLIENT_EFFECT_UPLOAD_NULL_REFERENCE, "ji actual upload f receiver", pc);
    if (buffer->data == NULL || buffer->position > buffer->limit ||
        buffer->limit > buffer->capacity)
        return failed(e, MC_CLIENT_EFFECT_UPLOAD_BUFFER_FAILED, "ji direct upload buffer extent",
                      pc);
    if (!mc_client_display_source_texture_sub_image(display, level, x, y, size, size,
                                                    buffer->data + buffer->position,
                                                    buffer->limit - buffer->position, &e->display))
        return failed(e, MC_CLIENT_EFFECT_UPLOAD_DISPLAY_FAILED, "ji actual glTexSubImage2D", pc);
    if (e->second_pass)
        ++e->separate_uploads;
    else
        ++e->atlas_uploads;
    return true;
}

static uint32_t average(uint32_t a, uint32_t b) {
    return ((((a >> 24) + (b >> 24)) >> 1) << 24) +
           (((a & UINT32_C(0xfefefe)) + (b & UINT32_C(0xfefefe))) >> 1);
}

static bool integer_at(mc_client_direct_byte_buffer *buffer, size_t at, uint32_t *value, int32_t pc,
                       mc_client_effect_upload_error *e) {
    if (at > buffer->limit || 4 > buffer->limit - at)
        return failed(e, MC_CLIENT_EFFECT_UPLOAD_BUFFER_FAILED, "ji actual f getInt index", pc);
    memcpy(value, buffer->data + at, 4); /* ge.c nativeOrder; x86-64 baseline. */
    return true;
}

static bool mipmaps(mc_client_render_engine *engine, mc_client_display *display,
                    const mc_texture_effect_fields *fields, mc_client_effect_upload_error *e) {
    mc_client_direct_byte_buffer *buffer = (mc_client_direct_byte_buffer *)(void *)engine->fields.f;
    for (int32_t level = 1; level <= 4; ++level) {
        const int32_t previous = 16 >> (level - 1), size = 16 >> level;
        for (int32_t x = 0; x < size; ++x) {
            for (int32_t y = 0; y < size; ++y) {
                uint32_t a, b, c, d;
                const int32_t offset = e->second_pass ? 381 : 0;
                if (!integer_at(buffer, (size_t)(x * 2 + y * 2 * previous) * 4, &a, 233 + offset,
                                e) ||
                    !integer_at(buffer, (size_t)(x * 2 + 1 + y * 2 * previous) * 4, &b,
                                260 + offset, e) ||
                    !integer_at(buffer, (size_t)(x * 2 + 1 + (y * 2 + 1) * previous) * 4, &c,
                                287 + offset, e) ||
                    !integer_at(buffer, (size_t)(x * 2 + (y * 2 + 1) * previous) * 4, &d,
                                314 + offset, e))
                    return false;
                const uint32_t value = average(average(a, b), average(c, d));
                const size_t at = (size_t)(x + y * size) * 4;
                if (at > buffer->limit || 4 > buffer->limit - at)
                    return failed(e, MC_CLIENT_EFFECT_UPLOAD_BUFFER_FAILED,
                                  "ji actual f putInt index", 357 + offset);
                memcpy(buffer->data + at, &value, 4);
            }
        }
        const int32_t x = e->second_pass ? 0 : mc_java_i32_mul(fields->b % 16, size);
        const int32_t y = e->second_pass ? 0 : mc_java_i32_mul(fields->b / 16, size);
        if (!sub_image(engine, display, level, x, y, size, e->second_pass ? 774 : 412, e))
            return false;
    }
    return true;
}

bool mc_client_render_engine_update_effects(mc_client_render_engine *engine,
                                            mc_client_display *display,
                                            mc_client_effect_upload_error *e) {
    if (e == NULL)
        return false;
    *e = (mc_client_effect_upload_error){.ji_pc = -1, .aw_pc = -1};
    if (engine == NULL || display == NULL)
        return failed(e, MC_CLIENT_EFFECT_UPLOAD_INVALID_ARGUMENT,
                      "ji dynamic texture owner/context", -1);
    if (engine->effects_retiring || engine->effects_retired ||
        !mc_client_texture_body_admit(engine, display, NULL, &e->lookup.texture))
        return failed(e,
                      engine->effects_retiring || engine->effects_retired ||
                              e->lookup.texture.status == MC_CLIENT_RENDER_TEXTURE_INVALID_PHASE
                          ? MC_CLIENT_EFFECT_UPLOAD_INVALID_PHASE
                      : e->lookup.texture.status == MC_CLIENT_RENDER_TEXTURE_BUSY
                          ? MC_CLIENT_EFFECT_UPLOAD_BUSY
                          : MC_CLIENT_EFFECT_UPLOAD_INVALID_ARGUMENT,
                      "ji dynamic texture native admission", -1);
    e->owner = mc_client_render_engine_retain(engine);
    if (e->owner != MC_CLIENT_RENDER_ENGINE_OK)
        return failed(e, MC_CLIENT_EFFECT_UPLOAD_BUSY, "ji dynamic texture call occurrence", -1);
    ++engine->active_resource_calls;
    ++engine->classes->active_resource_calls;
    bool ok = false;
    const mc_client_render_list *list = engine->fields.g;
    if (list == NULL) {
        failed(e, MC_CLIENT_EFFECT_UPLOAD_NULL_REFERENCE, "ji actual g List.size receiver", 7);
        goto leave;
    }
    for (size_t n = 0; n < list->count; ++n) {
        e->occurrence = n;
        mc_client_texture_effect *effect = list->entries[n];
        mc_game_settings_state settings;
        if (engine->fields.i == NULL) {
            failed(e, MC_CLIENT_EFFECT_UPLOAD_NULL_REFERENCE, "ji actual i Settings.g receiver",
                   34);
            goto leave;
        }
        if (!mc_game_settings_read_state(engine->fields.i, &settings)) {
            failed(e, MC_CLIENT_EFFECT_UPLOAD_INVALID_PHASE, "ji actual Settings native owner", 34);
            goto leave;
        }
        if (mc_client_texture_effect_set_anaglyph(effect, settings.values.anaglyph, &e->child) !=
            MC_TEXTURE_EFFECT_OK) {
            failed(e,
                   effect == NULL ? MC_CLIENT_EFFECT_UPLOAD_NULL_REFERENCE
                                  : MC_CLIENT_EFFECT_UPLOAD_CHILD_FAILED,
                   "ji actual aw.c store", 37);
            goto leave;
        }
        if (mc_client_texture_effect_tick(effect, &e->child) != MC_TEXTURE_EFFECT_OK) {
            failed(e, MC_CLIENT_EFFECT_UPLOAD_CHILD_FAILED, "ji actual aw.a animation child", 41);
            goto leave;
        }
        ++e->updated_occurrences;
        mc_texture_effect_state state;
        if (!mc_client_texture_effect_inspect(effect, &state)) {
            failed(e, MC_CLIENT_EFFECT_UPLOAD_CHILD_FAILED, "ji actual aw fields", 57);
            goto leave;
        }
        if (!transfer(engine, &state.fields, false, e) ||
            !bind_atlas(engine, display, &state.fields, e))
            goto leave;
        for (int32_t x = 0; x < state.fields.e; ++x) {
            for (int32_t y = 0; y < state.fields.e; ++y) {
                const int32_t dx = mc_java_i32_add(mc_java_i32_mul(state.fields.b % 16, 16),
                                                   mc_java_i32_mul(x, 16));
                const int32_t dy = mc_java_i32_add(mc_java_i32_mul(state.fields.b / 16, 16),
                                                   mc_java_i32_mul(y, 16));
                if (!sub_image(engine, display, 0, dx, dy, 16, 157, e) ||
                    (engine->classes->ji_a && !mipmaps(engine, display, &state.fields, e)))
                    goto leave;
            }
        }
    }
    e->second_pass = true;
    for (size_t n = 0; n < list->count; ++n) {
        e->occurrence = n;
        mc_texture_effect_state state;
        if (!mc_client_texture_effect_inspect(list->entries[n], &state)) {
            failed(e, MC_CLIENT_EFFECT_UPLOAD_NULL_REFERENCE, "ji second-pass actual aw.d receiver",
                   469);
            goto leave;
        }
        if (state.fields.d <= 0)
            continue;
        if (!transfer(engine, &state.fields, true, e))
            goto leave;
        if (!mc_client_display_source_texture_bind(display, (uint32_t)state.fields.d,
                                                   &e->display)) {
            failed(e, MC_CLIENT_EFFECT_UPLOAD_DISPLAY_FAILED, "ji actual separate texture bind",
                   519);
            goto leave;
        }
        if (!sub_image(engine, display, 0, 0, 0, 16, 542, e) ||
            (engine->classes->ji_a && !mipmaps(engine, display, &state.fields, e)))
            goto leave;
    }
    e->ji_pc = 789;
    e->source_returned = true;
    ok = true;
leave:
    --engine->classes->active_resource_calls;
    --engine->active_resource_calls;
    e->owner = mc_client_render_engine_release(engine);
    if (e->owner != MC_CLIENT_RENDER_ENGINE_OK)
        ok = failed(e, MC_CLIENT_EFFECT_UPLOAD_BUSY, "ji dynamic texture native call release", -1);
    return ok;
}
