#include "loading_internal.h"

#include <stdlib.h>
#include <string.h>

mc_client_loading_call *mc_client_loading_call_create(void) {
    return calloc(1, sizeof(mc_client_loading_call));
}

static bool capture_failure(mc_client_loading_screen *p, mc_client_loading_status status,
                            const char *operation, int32_t pc) {
    mc_client_loading_fail(&p->call->error, status, operation, "a", "(I)V", pc);
    return false;
}

bool mc_client_loading_call_engine(mc_client_loading_screen *p, mc_client_render_engine *engine,
                                   int32_t pc) {
    if (engine != NULL) {
        const mc_client_render_engine_status status = mc_client_render_engine_retain(engine);
        p->call->error.captured_engine = status;
        if (status != MC_CLIENT_RENDER_ENGINE_OK)
            return capture_failure(p,
                                   status == MC_CLIENT_RENDER_ENGINE_BUSY ? MC_CLIENT_LOADING_BUSY
                                   : status == MC_CLIENT_RENDER_ENGINE_INVALID_PHASE
                                       ? MC_CLIENT_LOADING_INVALID_PHASE
                                       : MC_CLIENT_LOADING_LIMIT,
                                   "mk captured Engine native occurrence", pc);
    }
    p->call->engine = engine;
    return true;
}

bool mc_client_loading_call_font(mc_client_loading_screen *p, size_t slot,
                                 mc_client_bitmap_font *font, int32_t pc) {
    if (slot >= 4 || p->call->fonts[slot] != NULL)
        return capture_failure(p, MC_CLIENT_LOADING_INVALID_ARGUMENT,
                               "mk bounded Font occurrence slot", pc);
    if (font != NULL) {
        const mc_client_bitmap_font_status status = mc_client_bitmap_font_retain(font);
        p->call->error.captured_font = status;
        if (status != MC_CLIENT_BITMAP_FONT_OK)
            return capture_failure(p,
                                   status == MC_CLIENT_BITMAP_FONT_BUSY ? MC_CLIENT_LOADING_BUSY
                                   : status == MC_CLIENT_BITMAP_FONT_INVALID_PHASE
                                       ? MC_CLIENT_LOADING_INVALID_PHASE
                                       : MC_CLIENT_LOADING_LIMIT,
                                   "mk captured Font native occurrence", pc);
    }
    p->call->fonts[slot] = font;
    return true;
}

bool mc_client_loading_call_string(mc_client_loading_screen *p, size_t slot,
                                   const mc_player_string *text, int32_t pc) {
    if (slot >= 5 || p->call->strings[slot] != NULL)
        return capture_failure(p, MC_CLIENT_LOADING_INVALID_ARGUMENT,
                               "mk bounded String occurrence slot", pc);
    if (text != NULL) {
        p->call->error.captured_string = mc_player_string_retain((mc_player_string *)text);
        if (p->call->error.captured_string != MC_PLAYER_OK)
            return capture_failure(p, MC_CLIENT_LOADING_LIMIT,
                                   "mk captured String native occurrence", pc);
    }
    p->call->strings[slot] = (mc_player_string *)text;
    return true;
}

bool mc_client_loading_background_literal(mc_client_loading_screen *p,
                                          const mc_player_string **out) {
    static const uint16_t units[] = {'/', 'g', 'u', 'i', '/', 'b', 'a', 'c', 'k', 'g',
                                     'r', 'o', 'u', 'n', 'd', '.', 'p', 'n', 'g'};
    if (p->classes->background_literal == NULL) {
        mc_player_string *created = NULL;
        p->call->error.literal =
            mc_player_string_create(units, sizeof(units) / sizeof(units[0]), &created);
        if (p->call->error.literal != MC_PLAYER_OK)
            return capture_failure(p,
                                   p->call->error.literal == MC_PLAYER_ALLOCATION_FAILED
                                       ? MC_CLIENT_LOADING_ALLOCATION_FAILED
                                       : MC_CLIENT_LOADING_LIMIT,
                                   "mk actual background literal native resolution", 163);
        p->classes->background_literal = created;
    }
    if (!mc_client_loading_call_string(p, 0, p->classes->background_literal, 163))
        return false;
    *out = p->classes->background_literal;
    return true;
}

static bool remaining(const mc_client_loading_call *call) {
    if (call->engine != NULL)
        return true;
    for (size_t i = 0; i < 4; ++i)
        if (call->fonts[i] != NULL)
            return true;
    for (size_t i = 0; i < 5; ++i)
        if (call->strings[i] != NULL)
            return true;
    return false;
}

/* Source unwinding drops only shared occurrences. Last occurrences are kept
 * for explicit native retirement, so no GPU deletion is hidden in a source
 * failure/return. Every terminal child call below runs under mk's active guard. */
static mc_client_loading_status release_occurrences(mc_client_loading_call *call, bool terminal) {
    mc_client_loading_status status = MC_CLIENT_LOADING_OK;
    for (size_t i = 0; i < 4; ++i) {
        if (call->fonts[i] == NULL)
            continue;
        if (!terminal && (!mc_client_bitmap_font_inspect(call->fonts[i], &call->font_state) ||
                          call->font_state.holds <= 1)) {
            call->cleanup.font_retained[i] = true;
            if (status == MC_CLIENT_LOADING_OK)
                status = MC_CLIENT_LOADING_BUSY;
            continue;
        }
        const mc_client_bitmap_font_status child =
            mc_client_bitmap_font_release(&call->fonts[i], &call->font_cleanup_work);
        call->cleanup.font_retained[i] = call->fonts[i] != NULL;
        if (child != MC_CLIENT_BITMAP_FONT_OK)
            call->cleanup.fonts[i] = call->font_cleanup_work;
        if (child != MC_CLIENT_BITMAP_FONT_OK && status == MC_CLIENT_LOADING_OK)
            status = child == MC_CLIENT_BITMAP_FONT_BUSY ? MC_CLIENT_LOADING_BUSY
                                                         : MC_CLIENT_LOADING_CLEANUP_FAILED;
    }
    for (size_t i = 0; i < 5; ++i) {
        mc_player_string_release(call->strings[i]);
        call->strings[i] = NULL;
    }
    bool fonts_remaining = false;
    for (size_t i = 0; i < 4; ++i)
        fonts_remaining = fonts_remaining || call->fonts[i] != NULL;
    /* A retained Font's genuine generation lease can itself hold this Engine.
     * Keep the directly inspectable Engine occurrence through that cleanup;
     * otherwise a last lease could hit metadata BUSY after losing this slot. */
    if (call->engine != NULL && fonts_remaining) {
        call->cleanup.engine_retained = true;
        if (status == MC_CLIENT_LOADING_OK)
            status = MC_CLIENT_LOADING_BUSY;
    } else if (call->engine != NULL && (!call->lookup_failed || call->lookup_retired)) {
        if (!terminal && (!mc_client_render_engine_inspect(call->engine, &call->engine_state) ||
                          call->engine_state.native_holds <= 1)) {
            call->cleanup.engine_retained = true;
            if (status == MC_CLIENT_LOADING_OK)
                status = MC_CLIENT_LOADING_BUSY;
        } else {
            const mc_client_render_engine_status child =
                mc_client_render_engine_release(call->engine);
            if (child == MC_CLIENT_RENDER_ENGINE_OK)
                call->engine = NULL;
            else {
                call->cleanup.engine = child;
                if (status == MC_CLIENT_LOADING_OK)
                    status = child == MC_CLIENT_RENDER_ENGINE_BUSY
                                 ? MC_CLIENT_LOADING_BUSY
                                 : MC_CLIENT_LOADING_CLEANUP_FAILED;
            }
            call->cleanup.engine_retained = call->engine != NULL;
        }
    } else
        call->cleanup.engine_retained = call->engine != NULL;
    call->cleanup.status = status;
    return status;
}

mc_client_loading_status mc_client_loading_call_finish(mc_client_loading_screen *p,
                                                       mc_client_loading_status status,
                                                       mc_client_loading_error *out) {
    mc_client_loading_call *call = p->call;
    call->error.source_returned = status == MC_CLIENT_LOADING_OK;
    const mc_client_loading_status cleanup = release_occurrences(call, false);
    if (status == MC_CLIENT_LOADING_OK && cleanup != MC_CLIENT_LOADING_OK)
        status = mc_client_loading_fail(&call->error, cleanup,
                                        "mk native temporary occurrence requires retirement", NULL,
                                        NULL, -1);
    const bool retained = call->lookup_failed || call->update_failed || remaining(call) ||
                          cleanup != MC_CLIENT_LOADING_OK;
    call->error.native_finalized = !retained;
    call->error.lookup = call->lookup_failed ? &call->lookup : NULL;
    call->error.cleanup = retained ? &call->cleanup : NULL;
    if (out != NULL)
        *out = call->error;
    if (!retained) {
        p->call = NULL;
        free(call);
    }
    return status;
}

bool mc_client_loading_screen_call_inspect(const mc_client_loading_screen *p,
                                           mc_client_loading_call_state *out) {
    if (p == NULL || out == NULL)
        return false;
    *out = (mc_client_loading_call_state){0};
    const mc_client_loading_call *call = p->call;
    if (call == NULL)
        return true;
    out->retained = !p->active;
    out->lookup_failed = call->lookup_failed;
    out->update_failed = call->update_failed;
    out->source_returned = call->error.source_returned;
    out->engine = call->engine;
    for (size_t i = 0; i < 4; ++i)
        out->fonts[i] = call->fonts[i];
    for (size_t i = 0; i < 5; ++i)
        out->strings[i] = call->strings[i];
    out->failure = &call->error;
    out->cleanup = &call->cleanup;
    return true;
}

mc_client_loading_status mc_client_loading_screen_retire_call(mc_client_loading_screen *p,
                                                              mc_client_loading_error *out) {
    if (out != NULL)
        memset(out, 0, sizeof(*out));
    if (p == NULL || p->active || p->phase == MC_CLIENT_LOADING_CONSTRUCTING) {
        const mc_client_loading_status status =
            p == NULL ? MC_CLIENT_LOADING_INVALID_ARGUMENT : MC_CLIENT_LOADING_BUSY;
        if (out != NULL)
            mc_client_loading_fail(out, status, "mk retained call native retirement admission",
                                   NULL, NULL, -1);
        return status;
    }
    mc_client_loading_call *call = p->call;
    if (call == NULL)
        return MC_CLIENT_LOADING_OK;
    p->active = true;
    mc_client_loading_status status = MC_CLIENT_LOADING_OK;
    if (call->lookup_failed && !call->lookup_retired && call->engine != NULL) {
        const mc_client_texture_lookup_status child =
            mc_client_render_engine_retire_lookup(call->engine, &call->lookup_cleanup_work);
        if (child == MC_CLIENT_LOOKUP_OK)
            call->lookup_retired = true;
        else {
            call->lookup_cleanup_last = call->lookup_cleanup_work;
            call->cleanup.lookup = &call->lookup_cleanup_last;
            status = child == MC_CLIENT_LOOKUP_BUSY ? MC_CLIENT_LOADING_BUSY
                                                    : MC_CLIENT_LOADING_CLEANUP_FAILED;
        }
    }
    const mc_client_loading_status released = release_occurrences(call, true);
    if (status == MC_CLIENT_LOADING_OK)
        status = released;
    call->cleanup.status = status;
    p->active = false;
    if (status == MC_CLIENT_LOADING_OK && !remaining(call)) {
        if (out != NULL) {
            out->source_returned = call->error.source_returned;
            out->native_finalized = true;
        }
        p->call = NULL;
        free(call);
        return MC_CLIENT_LOADING_OK;
    }
    if (status == MC_CLIENT_LOADING_OK)
        status = call->cleanup.status = MC_CLIENT_LOADING_BUSY;
    if (out != NULL) {
        mc_client_loading_fail(out, status, "mk retained call native retirement", NULL, NULL, -1);
        out->lookup = call->lookup_failed ? &call->lookup : NULL;
        out->update = call->update_failed ? &call->update : NULL;
        out->source_returned = call->error.source_returned;
        out->cleanup = &call->cleanup;
    }
    return status;
}
