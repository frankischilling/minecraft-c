#include "mc/core/java/java.h"
#include "source_sign_screen_internal.h"
#include "world/runtime/construction_internal.h"

#include <stdlib.h>

bool mc_screen_sign_fail(mc_client_source_yc *o, mc_client_screen_error *e,
                         mc_client_screen_status status, const char *operation, const char *method,
                         const char *descriptor, unsigned int pc, bool native) {
    mc_screen_fail(e, status, operation, "yc", method, descriptor, pc);
    e->native_policy = native;
    e->source_exception = !native && (status == MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE ||
                                      status == MC_CLIENT_SCREEN_SOURCE_INDEX);
    e->exception_class = !e->source_exception ? NULL
                         : status == MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE
                             ? "java/lang/NullPointerException"
                             : "java/lang/ArrayIndexOutOfBoundsException";
    if (o != NULL) {
        o->last_pc = pc;
        o->method = method;
        e->sign = &o->detail;
        o->base.failure = *e;
    }
    return false;
}

static bool mapping_cleanup(mc_client_screen_classes *c, mc_client_screen_error *e) {
    if (c->sign_mapping != NULL) {
        const mc_client_font_mapping_status status =
            mc_client_font_mapping_release(c->sign_mapping);
        if (status != MC_CLIENT_FONT_MAPPING_OK) {
            mc_screen_fail(e,
                           status == MC_CLIENT_FONT_MAPPING_BUSY ? MC_CLIENT_SCREEN_BUSY
                                                                 : MC_CLIENT_SCREEN_STRING_FAILED,
                           "YC actual FP native hold retirement", NULL, NULL, NULL, 0);
            return false;
        }
        c->sign_mapping = NULL;
    }
    c->sign_mapping_cleanup = NULL;
    return true;
}

bool mc_screen_sign_active_locked(mc_client_screen_classes *c, mc_client_font_mapping *mapping,
                                  mc_client_screen_error *e) {
    if (c->retiring || c->sign_admission_ended) {
        mc_screen_fail(e, MC_CLIENT_SCREEN_BUSY, "YC class native admission ended", NULL, NULL,
                       NULL, 0);
        return false;
    }
    if (c->sign_phase == MC_CLIENT_SCREEN_CLASS_READY)
        return true; /* Already initialized YC: no fresh FP read or operand requirement. */
    if (c->sign_phase == MC_CLIENT_SCREEN_CLASS_INITIALIZING) {
        mc_screen_fail(e, MC_CLIENT_SCREEN_BUSY, "YC actual initializer active", NULL, NULL, NULL,
                       0);
        return false;
    }
    if (c->sign_phase == MC_CLIENT_SCREEN_CLASS_FAILED) {
        *e = c->sign_failure;
        e->first_class_status = e->status;
        e->first_class_site = e->site;
        e->status = MC_CLIENT_SCREEN_CLASS_ERRONEOUS;
        return false;
    }
    const mc_client_font_mapping_status held = mc_client_font_mapping_retain(mapping);
    if (held != MC_CLIENT_FONT_MAPPING_OK) {
        mc_screen_fail(e,
                       held == MC_CLIENT_FONT_MAPPING_LIMIT ? MC_CLIENT_SCREEN_LIMIT
                                                            : MC_CLIENT_SCREEN_INVALID_ARGUMENT,
                       "YC explicit FP namespace native hold", NULL, NULL, NULL, 0);
        return false;
    }
    c->sign_mapping = mapping;
    c->sign_mapping_cleanup = mapping_cleanup;
    c->sign_phase = MC_CLIENT_SCREEN_CLASS_INITIALIZING;
    c->sign_pc = 0;
    const mc_player_string *accepted = NULL;
    mc_client_font_mapping_status status =
        mc_client_font_mapping_active_use(mapping, &accepted, &c->sign_mapping_failure);
    if (status != MC_CLIENT_FONT_MAPPING_OK) {
        mc_screen_fail(e,
                       status == MC_CLIENT_FONT_MAPPING_LIMIT ? MC_CLIENT_SCREEN_LIMIT
                       : status == MC_CLIENT_FONT_MAPPING_ALLOCATION_FAILED
                           ? MC_CLIENT_SCREEN_ALLOCATION_FAILED
                           : MC_CLIENT_SCREEN_STRING_FAILED,
                       "YC actual FP.a getstatic child", "yc", "<clinit>", "()V", 0);
        c->sign_failure = *e;
        c->sign_phase = MC_CLIENT_SCREEN_CLASS_FAILED;
        return false;
    }
    c->sign_pc = 3;
    e->string = mc_player_string_retain((mc_player_string *)accepted);
    if (e->string != MC_PLAYER_OK) {
        mc_screen_fail(e, MC_CLIENT_SCREEN_STRING_FAILED, "YC native captured m occurrence", "yc",
                       "<clinit>", "()V", 3);
        c->sign_failure = *e;
        c->sign_phase = MC_CLIENT_SCREEN_CLASS_FAILED;
        return false;
    }
    c->sign_accepted = (mc_player_string *)accepted;
    c->sign_store_pc = 3;
    c->sign_pc = 6;
    c->sign_phase = MC_CLIENT_SCREEN_CLASS_READY;
    return true;
}

mc_client_screen_status mc_client_screen_sign_active_use(mc_client_screen_classes *c,
                                                         mc_client_font_mapping *mapping,
                                                         mc_client_screen_error *error) {
    mc_client_screen_error e = {0};
    if (mc_screen_lock(c, &e)) {
        if (mc_screen_sign_active_locked(c, mapping, &e))
            e.source_returned = true;
        mtx_unlock(&c->mutex);
    }
    return mc_screen_publish(&e, error);
}

mc_client_screen_status
mc_client_screen_sign_class_terminal_prepare(mc_client_screen_classes *c,
                                             mc_client_screen_error *error) {
    mc_client_screen_error e = {0};
    if (mc_screen_lock(c, &e)) {
        if (c->sign_phase == MC_CLIENT_SCREEN_CLASS_INITIALIZING)
            mc_screen_fail(&e, MC_CLIENT_SCREEN_BUSY, "YC initializer active at native class end",
                           NULL, NULL, NULL, 0);
        else {
            c->sign_admission_ended = true;
            (void)mapping_cleanup(c, &e);
        }
        mtx_unlock(&c->mutex);
    }
    return mc_screen_publish(&e, error);
}

mc_client_screen_status mc_client_screen_sign_allocate(mc_client_screen_classes *c,
                                                       const mc_client_sign_screen_dependencies *d,
                                                       const mc_client_screen_options *options,
                                                       mc_client_source_yc **out,
                                                       mc_client_screen_error *error) {
    mc_client_screen_error e = {0};
    if (out == NULL || d == NULL)
        mc_screen_fail(&e, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "YC native output/dependencies", NULL,
                       NULL, NULL, 0);
    else if (mc_screen_lock(c, &e)) {
        if (mc_screen_sign_active_locked(c, d->mapping, &e)) {
            if (c->holds == SIZE_MAX ||
                (options != NULL && options->maximum_bytes < sizeof(mc_client_source_yc)))
                mc_screen_fail(&e, MC_CLIENT_SCREEN_LIMIT, "YC native owner budget", NULL, NULL,
                               NULL, 0);
            else {
                mc_client_source_yc *o = calloc(1, sizeof(*o));
                if (o == NULL)
                    mc_screen_fail(&e, MC_CLIENT_SCREEN_ALLOCATION_FAILED, "YC RAW native owner",
                                   NULL, NULL, NULL, 0);
                else {
                    o->base.classes = c;
                    o->base.holds = 1;
                    o->base.kind = MC_CLIENT_SCREEN_SIGN;
                    mc_screen_memory_init(&o->base.memory, options, sizeof(*o));
                    if (d->literal_classes != NULL) {
                        o->detail.tile = d->literal_classes->side != MC_WORLD_INFO_CLIENT ||
                                                 d->literal_classes->profile != c->profile ||
                                                 d->literal_classes->registry != c->registry
                                             ? MC_TILE_ENTITY_INVALID_ARGUMENT
                                             : mc_tile_entity_classes_borrow(d->literal_classes,
                                                                             &o->literal_classes);
                        if (o->detail.tile != MC_TILE_ENTITY_OK)
                            mc_screen_sign_fail(o, &e, MC_CLIENT_SCREEN_APP_FAILED,
                                                "YC defining Tile namespace native borrow",
                                                "<init>", "(Lyk;)V", 0, true);
                    }
                    if (e.status == MC_CLIENT_SCREEN_OK && d->input != NULL &&
                        mc_client_sdk_input_code_borrow(d->input, &o->input, &o->detail.input) !=
                            MC_CLIENT_SDK_INPUT_OK)
                        mc_screen_sign_fail(o, &e, MC_CLIENT_SCREEN_INPUT_FAILED,
                                            "YC explicit SDK code native borrow", "<init>",
                                            "(Lyk;)V", 0, true);
                    if (e.status == MC_CLIENT_SCREEN_OK) {
                        ++c->holds;
                        *out = o;
                    } else {
                        (void)mc_tile_entity_classes_release(&o->literal_classes);
                        e.sign = NULL; /* Unpublished native owner has no exported child view. */
                        free(o);
                    }
                }
            }
        }
        mtx_unlock(&c->mutex);
    }
    return mc_screen_publish(&e, error);
}

mc_client_screen_status mc_client_screen_sign_construct(mc_client_source_yc *o,
                                                        mc_tile_entity *tile,
                                                        mc_client_screen_error *error) {
    mc_client_screen_error e = {0};
    if (o == NULL)
        mc_screen_fail(&e, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "YC constructor native receiver",
                       NULL, NULL, NULL, 0);
    else if (mc_screen_lock(o->base.classes, &e)) {
        if (o->base.phase != MC_CLIENT_SCREEN_RAW || o->base.classes->retiring)
            mc_screen_fail(&e, MC_CLIENT_SCREEN_INVALID_PHASE, "YC one-shot constructor", "yc",
                           "<init>", "(Lyk;)V", 0);
        else {
            o->base.phase = MC_CLIENT_SCREEN_CONSTRUCTING;
            bool ok = mc_screen_base_construct_locked(&o->base, &e);
            if (!ok)
                e.parent_site = (mc_client_screen_site){"yc", "<init>", "(Lyk;)V", 1};
            if (ok) {
                ok = mc_screen_literal_locked(o->base.classes, "Edit sign message:", &o->title, &e);
                if (!ok)
                    e.parent_site = (mc_client_screen_site){"yc", "<init>", "(Lyk;)V", 5};
                if (ok) {
                    o->base.stores[o->base.store_count++] =
                        (mc_client_screen_site){"yc", "<init>", "(Lyk;)V", 7};
                    o->row = 0;
                    o->base.stores[o->base.store_count++] =
                        (mc_client_screen_site){"yc", "<init>", "(Lyk;)V", 12};
                    if (tile != NULL) {
                        const mc_tile_entity_class_literal *literal = NULL;
                        mc_tile_entity_class_literal_view view = {0};
                        o->detail.tile = mc_tile_entity_get_class(tile, &literal);
                        if (o->detail.tile == MC_TILE_ENTITY_OK)
                            o->detail.tile = mc_tile_entity_class_literal_inspect(literal, &view);
                        if (o->detail.tile == MC_TILE_ENTITY_OK &&
                            (view.kind != MC_TILE_ENTITY_CLASS_KIND_SIGN ||
                             (o->literal_classes != NULL && view.owner != o->literal_classes)))
                            o->detail.tile = MC_TILE_ENTITY_INVALID_ARGUMENT;
                        if (o->detail.tile == MC_TILE_ENTITY_OK)
                            o->detail.tile = mc_tile_entity_retain(tile);
                        ok = o->detail.tile == MC_TILE_ENTITY_OK;
                        if (ok)
                            o->held_tile = tile;
                        else
                            mc_screen_sign_fail(o, &e, MC_CLIENT_SCREEN_APP_FAILED,
                                                "YC actual nullable Sign native occurrence",
                                                "<init>", "(Lyk;)V", 17, true);
                    }
                    if (ok) {
                        o->tile = tile;
                        o->base.stores[o->base.store_count++] =
                            (mc_client_screen_site){"yc", "<init>", "(Lyk;)V", 17};
                        o->last_pc = 20;
                        o->method = "<init>";
                    }
                }
            }
            o->base.phase = ok ? MC_CLIENT_SCREEN_READY : MC_CLIENT_SCREEN_FAILED;
            if (!ok)
                o->base.failure = e;
            else
                e.source_returned = true;
        }
        mtx_unlock(&o->base.classes->mutex);
    }
    return mc_screen_publish(&e, error);
}

mc_client_source_da *mc_client_screen_sign_base(mc_client_source_yc *o) {
    return o == NULL ? NULL : &o->base;
}

bool mc_screen_sign_cleanup_locked(mc_client_source_yc *o, mc_client_screen_error *e) {
    for (mc_sign_initialization *r = o->initializations; r != NULL; r = r->next)
        if (!mc_screen_button_release_locked(&r->button, e))
            return false;
    if (o->input != NULL && mc_client_sdk_input_code_release(&o->input, &o->detail.cleanup_input) !=
                                MC_CLIENT_SDK_INPUT_OK) {
        mc_screen_fail(e, MC_CLIENT_SCREEN_INPUT_FAILED, "YC SDK code native release", NULL, NULL,
                       NULL, 0);
        o->cleanup_failure = *e;
        return false;
    }
    /* Expire only producer borrows before any captured receipt is released.
     * Partial cleanup may refuse a later release; no earlier released producer
     * may remain exposed then. The failed Source frame and value-only null
     * witness remain inspectable, and the Tile's World binding stays held. */
    mc_world_access_failure *failures[] = {&o->detail.dirty.world_access_failure,
                                           &o->detail.dirty.publication.world_access_failure,
                                           &o->detail.dirty.batch.publication.world_access_failure};
    for (size_t i = 0; i < sizeof(failures) / sizeof(failures[0]); ++i) {
        failures[i]->receiver = NULL;
        failures[i]->render_global = NULL;
        failures[i]->receipt = NULL;
    }
    for (mc_sign_dirty_call *call = o->dirty_calls; call != NULL; call = call->next)
        for (size_t i = 0; i < call->count; ++i) {
            mc_world_access_failure_release(&call->captures[i]);
            if (call->captures[i].receipt != NULL) {
                mc_screen_fail(e, MC_CLIENT_SCREEN_BUSY,
                               "YC dirty invocation native receipt release", NULL, NULL, NULL, 0);
                o->cleanup_failure = *e;
                return false;
            }
        }
    mc_tile_entity_release(o->held_tile);
    o->held_tile = NULL;
    o->detail.cleanup_tile = mc_tile_entity_classes_release(&o->literal_classes);
    if (o->detail.cleanup_tile != MC_TILE_ENTITY_OK) {
        mc_screen_fail(e, MC_CLIENT_SCREEN_APP_FAILED, "YC Tile namespace native release", NULL,
                       NULL, NULL, 0);
        o->cleanup_failure = *e;
        return false;
    }
    o->terminal_prepared = true;
    return true;
}

mc_client_screen_status mc_client_screen_sign_terminal_prepare(mc_client_source_yc *o,
                                                               mc_client_screen_error *error) {
    mc_client_screen_error e = {0};
    if (o == NULL)
        mc_screen_fail(&e, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "YC native terminal receiver", NULL,
                       NULL, NULL, 0);
    else if (mc_screen_lock(o->base.classes, &e)) {
        if ((o->base.active_calls != 0 && !(o->button_monitor_lost && o->base.active_calls == 1)) ||
            o->base.holds != 1 || o->base.phase == MC_CLIENT_SCREEN_CONSTRUCTING)
            mc_screen_fail(&e, MC_CLIENT_SCREEN_BUSY,
                           "YC native terminal active/external occurrence", NULL, NULL, NULL, 0);
        else {
            if (o->button_monitor_lost) {
                --o->base.active_calls; /* Recovered ended native call, not Source resumption. */
                o->button_monitor_lost = false;
            }
            o->base.phase = MC_CLIENT_SCREEN_RETIRING;
            (void)mc_screen_sign_cleanup_locked(o, &e);
        }
        mtx_unlock(&o->base.classes->mutex);
    }
    return mc_screen_publish(&e, error);
}

mc_client_screen_status mc_client_screen_sign_inspect(mc_client_source_yc *o,
                                                      mc_client_sign_screen_state *out,
                                                      mc_client_screen_error *error) {
    mc_client_screen_error e = {0};
    if (o == NULL || out == NULL)
        mc_screen_fail(&e, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "YC native inspect receiver/output",
                       NULL, NULL, NULL, 0);
    else if (mc_screen_lock(o->base.classes, &e)) {
        *out = (mc_client_sign_screen_state){
            .phase = o->base.phase,
            .holds = o->base.holds,
            .source_calls = o->source_calls,
            .initialization_count = o->initialization_count,
            .title = o->title,
            .tile = o->base.phase == MC_CLIENT_SCREEN_RETIRING ? NULL : o->tile,
            .tick = o->tick,
            .row = o->row,
            .tile_held = o->held_tile != NULL,
            .input_held = o->input != NULL,
            .literal_classes_held = o->literal_classes != NULL,
            .terminal_prepared = o->terminal_prepared,
            .last_pc = o->last_pc,
            .row_stores = o->row_stores,
            .line_stores = o->line_stores,
            .tick_stores = o->tick_stores,
            .method = o->method,
            .failure = &o->base.failure,
            .detail = &o->detail,
            .cleanup_failure = &o->cleanup_failure};
        mtx_unlock(&o->base.classes->mutex);
    }
    return mc_screen_publish(&e, error);
}

static mc_client_screen_status release(mc_client_source_yc **slot, bool strict,
                                       mc_client_screen_error *e) {
    if (slot == NULL)
        return strict ? mc_client_screen_retire(NULL, e) : mc_client_screen_release(NULL, e);
    mc_client_source_da *base = *slot == NULL ? NULL : &(*slot)->base;
    const mc_client_screen_status status =
        strict ? mc_client_screen_retire(&base, e) : mc_client_screen_release(&base, e);
    if (status == MC_CLIENT_SCREEN_OK)
        *slot = NULL;
    return status;
}

mc_client_screen_status mc_client_screen_sign_release(mc_client_source_yc **o,
                                                      mc_client_screen_error *e) {
    return release(o, false, e);
}

mc_client_screen_status mc_client_screen_sign_retire(mc_client_source_yc **o,
                                                     mc_client_screen_error *e) {
    return release(o, true, e);
}
