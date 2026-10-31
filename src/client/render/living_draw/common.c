#include "internal.h"

mc_client_graphics_site ld_site(const char *c, const char *m, const char *d, int32_t pc) {
    return (mc_client_graphics_site){c, m, d, pc};
}

void mc_client_living_draw_error_init(mc_client_living_draw_error *e) {
    static const mc_client_living_draw_error zero = {0};
    if (e == NULL)
        return;
    *e = zero;
    e->site.pc = e->child.pc = e->parent.pc = e->caller.pc = -1;
    e->state.pass = -1;
}

void ld_at(ld_frame *f, const char *c, const char *m, const char *d, int32_t pc) {
    f->error->site = ld_site(c, m, d, pc);
    if (pc >= 0)
        f->error->state.source_pc = (unsigned int)pc;
}

bool ld_fail(ld_frame *f, mc_client_living_draw_status status, const char *op) {
    f->error->status = status;
    f->error->operation = op;
    f->error->native_policy = f->error->throwable == MC_CLIENT_LIVING_DRAW_NO_THROWABLE ||
                              f->error->throwable == MC_CLIENT_LIVING_DRAW_UNCLASSIFIED_THROWABLE;
    return false;
}

bool ld_null(ld_frame *f, const char *op) {
    f->error->throwable = MC_CLIENT_LIVING_DRAW_EXCEPTION;
    f->error->source_exception = true;
    f->error->exception_class = "java/lang/NullPointerException";
    return ld_fail(f, MC_CLIENT_LIVING_DRAW_SOURCE_EXCEPTION, op);
}

bool ld_fp(ld_frame *f) {
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u ||
           ld_fail(f, MC_CLIENT_LIVING_DRAW_STATE, "Living draw strict floating controls");
}

bool ld_work(ld_frame *f) {
    if (f->scope->work == f->scope->max_work)
        return ld_fail(f, MC_CLIENT_LIVING_DRAW_LIMIT, "Living draw reached native work ceiling");
    ++f->scope->work;
    return true;
}

bool ld_display(ld_frame *f, bool ok) {
    if (ok)
        return true;
    f->error->selected_child = MC_CLIENT_LIVING_DRAW_CHILD_DISPLAY;
    return ld_fail(f, MC_CLIENT_LIVING_DRAW_CHILD_FAILED, f->error->detail.display.operation);
}

bool ld_graphics(ld_frame *f, bool ok) {
    if (ok)
        return true;
    mc_client_living_draw_error *e = f->error;
    e->selected_child = MC_CLIENT_LIVING_DRAW_CHILD_GRAPHICS;
    e->child = e->detail.graphics.site;
    if (!e->detail.graphics.native_policy) {
        const mc_client_graphics_status s = e->detail.graphics.status;
        const char *name =
            s == MC_CLIENT_GRAPHICS_NULL_REFERENCE ? "java/lang/NullPointerException"
            : s == MC_CLIENT_GRAPHICS_ARRAY_INDEX  ? "java/lang/ArrayIndexOutOfBoundsException"
            : s == MC_CLIENT_GRAPHICS_SOURCE_ILLEGAL_ARGUMENT ? "java/lang/IllegalArgumentException"
            : s == MC_CLIENT_GRAPHICS_SOURCE_ILLEGAL_STATE    ? "java/lang/IllegalStateException"
                                                              : NULL;
        if (name != NULL) {
            e->throwable = MC_CLIENT_LIVING_DRAW_EXCEPTION;
            e->source_exception = true;
            e->exception_class = name;
        } else
            e->throwable = MC_CLIENT_LIVING_DRAW_UNCLASSIFIED_THROWABLE;
    }
    return ld_fail(f, MC_CLIENT_LIVING_DRAW_CHILD_FAILED, e->detail.graphics.operation);
}

bool ld_capture(ld_frame *f, bool ok) {
    if (ok)
        return true;
    f->error->selected_child = MC_CLIENT_LIVING_DRAW_CHILD_CAPTURE;
    return ld_fail(f, MC_CLIENT_LIVING_DRAW_CHILD_FAILED, f->error->detail.capture.operation);
}

/* A native retained-owner failure cannot be caught as a Java Exception.
 * The actual earlier source child remains complete in its tagged detail. */
static bool capture_failed(ld_frame *f) {
    f->error->throwable = MC_CLIENT_LIVING_DRAW_NO_THROWABLE;
    f->error->source_exception = false;
    f->error->exception_class = NULL;
    return ld_fail(f,
                   f->error->capture_failure.status == MC_CLIENT_MANAGER_LIMIT
                       ? MC_CLIENT_LIVING_DRAW_LIMIT
                       : MC_CLIENT_LIVING_DRAW_CHILD_FAILED,
                   f->error->capture_failure.operation);
}

bool ld_entity(ld_frame *f, bool ok) {
    mc_client_living_draw_error *e = f->error;
    mc_entity_render_error *v = &e->detail.entity;
    if (!mc_client_draw_entity_queries(f->scope, v, &e->capture_failure)) {
        e->selected_child = MC_CLIENT_LIVING_DRAW_CHILD_ENTITY;
        e->child = ld_site(v->site.class_name, v->site.method, v->site.descriptor, v->site.pc);
        return capture_failed(f);
    }
    bool adopted = true;
    for (size_t i = 0; i < 2; ++i)
        if (v->world_reads[i] != NULL &&
            !mc_client_draw_adopt_world(f->scope, &v->world_reads[i], &e->capture_failure)) {
            adopted = false;
            break;
        }
    if (ok && adopted)
        return true;
    e->selected_child = MC_CLIENT_LIVING_DRAW_CHILD_ENTITY;
    e->child = ld_site(v->site.class_name, v->site.method, v->site.descriptor, v->site.pc);
    if (!adopted)
        return capture_failed(f);
    if (v->throwable == MC_ENTITY_RENDER_SOURCE_EXCEPTION)
        e->throwable = MC_CLIENT_LIVING_DRAW_EXCEPTION;
    else if (v->throwable == MC_ENTITY_RENDER_SOURCE_ERROR)
        e->throwable = MC_CLIENT_LIVING_DRAW_ERROR;
    else if (v->throwable == MC_ENTITY_RENDER_UNCLASSIFIED_THROWABLE)
        e->throwable = MC_CLIENT_LIVING_DRAW_UNCLASSIFIED_THROWABLE;
    e->source_exception = v->source_exception;
    e->exception_class = v->exception_class;
    return ld_fail(f, MC_CLIENT_LIVING_DRAW_CHILD_FAILED,
                   ok ? e->capture_failure.operation : v->operation);
}

bool ld_model(ld_frame *f, bool ok) {
    if (ok)
        return true;
    mc_client_living_draw_error *e = f->error;
    mc_client_model_runtime_error *v = &e->detail.model;
    e->selected_child = MC_CLIENT_LIVING_DRAW_CHILD_MODEL;
    e->child = v->site;
    if (v->part.pending_list &&
        !mc_client_draw_pending_list(f->scope, &v->part, &e->capture_failure))
        return capture_failed(f);
    if (v->throwable == MC_CLIENT_MODEL_DRAW_SOURCE_EXCEPTION) {
        e->throwable = MC_CLIENT_LIVING_DRAW_EXCEPTION;
        e->source_exception = true;
        e->exception_class = v->exception_class;
    } else if (v->throwable == MC_CLIENT_MODEL_DRAW_UNCLASSIFIED_THROWABLE)
        e->throwable = MC_CLIENT_LIVING_DRAW_UNCLASSIFIED_THROWABLE;
    return ld_fail(f, MC_CLIENT_LIVING_DRAW_CHILD_FAILED, v->operation);
}

mc_client_render_manager *ld_manager(ld_frame *f, int32_t pc) {
    f->error->site.pc = pc;
    const void *source = f->renderer->manager;
    if (source == NULL) {
        ld_null(f, "Living draw null BW manager receiver");
        return NULL;
    }
    if (source != f->renderer->parent) {
        ld_fail(f, MC_CLIENT_LIVING_DRAW_UNIMPLEMENTED,
                "Living draw unrepresented BW manager occurrence");
        return NULL;
    }
    return f->renderer->parent;
}

static bool engine_capture(ld_frame *f, mc_client_render_engine *engine) {
    return ld_capture(f,
                      mc_client_draw_capture_engine(f->scope, engine, &f->error->detail.capture));
}

static void lookup_failure(ld_frame *f, const mc_client_texture_lookup_error *v) {
    mc_client_living_draw_error *e = f->error;
    e->child = v->site;
    if (!v->native_policy) {
        const char *name = v->status == MC_CLIENT_LOOKUP_NULL_REFERENCE
                               ? "java/lang/NullPointerException"
                           : v->status == MC_CLIENT_LOOKUP_ARRAY_FAILED
                               ? "java/lang/ArrayIndexOutOfBoundsException"
                           : v->status == MC_CLIENT_LOOKUP_SOURCE_NEGATIVE_SIZE
                               ? "java/lang/NegativeArraySizeException"
                           : v->status == MC_CLIENT_LOOKUP_SOURCE_ILLEGAL_ARGUMENT
                               ? "java/lang/IllegalArgumentException"
                               : NULL;
        if (name != NULL) {
            e->throwable = MC_CLIENT_LIVING_DRAW_EXCEPTION;
            e->source_exception = true;
            e->exception_class = name;
        } else
            e->throwable = MC_CLIENT_LIVING_DRAW_UNCLASSIFIED_THROWABLE;
    }
}

static bool texture_engine(ld_frame *f, mc_client_render_engine *engine,
                           const mc_player_string *name, mc_client_graphics_site lookup_site,
                           mc_client_graphics_site bind_site,
                           mc_client_living_draw_texture_call *receipt) {
    mc_client_living_draw_error *e = f->error;
    if (!engine_capture(f, engine))
        return false;
    mc_player_string_view text;
    const bool nonnull = mc_player_string_read(name, &text);
    const mc_texture_pack_text key = {nonnull ? text.units : NULL, nonnull ? text.length : 0,
                                      !nonnull};
    const bool pending = engine != NULL && mc_client_texture_lookup_pending(engine);
    ld_at(f, lookup_site.class_name, lookup_site.method, lookup_site.descriptor, lookup_site.pc);
    receipt->attempted = true;
    if (engine == NULL)
        return ld_null(f, "Living draw null texture Engine receiver");
    mc_client_texture texture;
    receipt->lookup_entered = true;
    const mc_client_texture_lookup_status status = mc_client_render_engine_texture(
        engine, f->scope->model.display, NULL, key, &texture, &e->detail.lookup);
    if (status != MC_CLIENT_LOOKUP_OK) {
        e->selected_child = MC_CLIENT_LIVING_DRAW_CHILD_LOOKUP;
        e->child = e->detail.lookup.site;
        if (!pending && mc_client_texture_lookup_pending(engine) &&
            !mc_client_draw_pending_lookup(f->scope, engine, pending, &e->capture_failure))
            return capture_failed(f);
        lookup_failure(f, &e->detail.lookup);
        return ld_fail(f, MC_CLIENT_LIVING_DRAW_CHILD_FAILED, e->detail.lookup.operation);
    }
    receipt->lookup_returned = true;
    receipt->texture_id = (int32_t)texture;
    ld_at(f, bind_site.class_name, bind_site.method, bind_site.descriptor, bind_site.pc);
    receipt->bind_attempted = true;
    /* Actual JI.b(I) returns4 before any GL/Display access for a negative ID. */
    if ((int32_t)texture < 0) {
        receipt->bind_returned = true;
        return true;
    }
    receipt->bind_returned = ld_display(
        f, mc_client_display_source_bind_id(f->scope->model.display, texture, &e->detail.display));
    return receipt->bind_returned;
}

bool ld_texture_engine(ld_frame *f, mc_client_render_engine *engine, const mc_player_string *name,
                       mc_client_graphics_site caller, mc_client_graphics_site lookup_site,
                       mc_client_graphics_site bind_site) {
    mc_client_living_draw_error *e = f->error;
    if (e->state.texture_call_count == MC_CLIENT_LIVING_DRAW_TEXTURE_CALLS)
        return ld_fail(f, MC_CLIENT_LIVING_DRAW_LIMIT,
                       "Living draw actual texture receipt ceiling");
    mc_client_living_draw_texture_call *receipt =
        &e->state.texture_calls[e->state.texture_call_count++];
    *receipt = (mc_client_living_draw_texture_call){.engine_identity = (uintptr_t)engine,
                                                    .key_identity = (uintptr_t)name,
                                                    .caller = caller,
                                                    .lookup_site = lookup_site,
                                                    .bind_site = bind_site,
                                                    .failure_site = {.pc = -1},
                                                    .key_null = name == NULL};
    mc_player_string_view text;
    if (mc_player_string_read(name, &text)) {
        receipt->key_length = text.length;
        receipt->key_stored = text.length < MC_CLIENT_LIVING_DRAW_TEXTURE_KEY_UNITS
                                  ? text.length
                                  : MC_CLIENT_LIVING_DRAW_TEXTURE_KEY_UNITS;
        receipt->key_copied = receipt->key_stored == text.length;
        if (receipt->key_stored != 0)
            memcpy(receipt->key_units, text.units,
                   receipt->key_stored * sizeof(*receipt->key_units));
    }
    const bool ok = texture_engine(f, engine, name, lookup_site, bind_site, receipt);
    receipt->returned = ok;
    receipt->failed = !ok;
    if (!ok) {
        receipt->native_policy = e->native_policy;
        receipt->source_exception = e->source_exception;
        receipt->exception_class = e->exception_class;
        receipt->failure_site = e->child.class_name == NULL ? e->site : e->child;
    }
    return ok;
}

bool ld_texture(ld_frame *f, const mc_player_string *name, int32_t caller_pc) {
    mc_client_living_draw_error *e = f->error;
    const mc_client_graphics_site caller = e->site;
    const mc_client_graphics_site parent = e->parent;
    e->parent = caller;
    ld_at(f, "bw", "a", "(Ljava/lang/String;)V", 4);
    mc_client_render_manager *m = ld_manager(f, 4);
    if (m == NULL)
        return false;
    if (!ld_texture_engine(f, m->fields.e, name, caller,
                           ld_site("bw", "a", "(Ljava/lang/String;)V", 11),
                           ld_site("bw", "a", "(Ljava/lang/String;)V", 14)))
        return false;
    e->site = caller;
    e->site.pc = caller_pc;
    e->parent = parent;
    return true;
}

bool ld_skin(ld_frame *f) {
    mc_client_living_draw_error *e = f->error;
    const mc_player_string *url = (const mc_player_string *)f->actor->base.client_only.bA;
    if (!ld_capture(f, mc_client_draw_capture_string(f->scope, url, &e->detail.capture)))
        return false;
    const mc_player_string *fallback = NULL;
    ld_at(f, "gv", "a", "(Lls;DDDFF)V", 251);
    if (!ld_entity(f, mc_entity_render_texture(f->actor, &fallback, &e->detail.entity)))
        return false;
    if (!ld_capture(f, mc_client_draw_capture_string(f->scope, fallback, &e->detail.capture)))
        return false;
    const mc_client_graphics_site caller = ld_site("gv", "a", "(Lls;DDDFF)V", 254);
    const mc_client_graphics_site parent = e->parent;
    e->parent = caller;
    ld_at(f, "bw", "a", "(Ljava/lang/String;Ljava/lang/String;)Z", 4);
    mc_client_render_manager *m = ld_manager(f, 4);
    if (m == NULL)
        return false;
    mc_client_render_engine *engine = m->fields.e;
    if (!engine_capture(f, engine))
        return false;
    const bool pending = engine != NULL && mc_client_texture_lookup_pending(engine);
    ld_at(f, "bw", "a", "(Ljava/lang/String;Ljava/lang/String;)Z", 11);
    if (engine == NULL)
        return ld_null(f, "Living draw null downloaded texture Engine");
    mc_client_texture texture;
    const mc_client_image_download_status status = mc_client_render_download_texture(
        engine, f->scope->model.display, url, fallback, &texture, &e->detail.download);
    if (status != MC_CLIENT_IMAGE_DOWNLOAD_OK) {
        e->selected_child = MC_CLIENT_LIVING_DRAW_CHILD_DOWNLOAD;
        if (!pending && mc_client_texture_lookup_pending(engine) &&
            !mc_client_draw_pending_lookup(f->scope, engine, pending, &e->capture_failure))
            return capture_failed(f);
        const mc_client_download_texture_error *v = &e->detail.download;
        e->child = ld_site(v->download.site.class_name, v->download.site.method,
                           v->download.site.descriptor, (int32_t)v->download.site.pc);
        if (v->download.status == MC_CLIENT_IMAGE_DOWNLOAD_SOURCE_NULL) {
            e->throwable = MC_CLIENT_LIVING_DRAW_EXCEPTION;
            e->source_exception = true;
            e->exception_class = "java/lang/NullPointerException";
        } else if (v->download.source_exception) {
            e->source_exception = true;
            e->exception_class = v->download.exception_class;
            e->throwable = e->exception_class != NULL
                               ? MC_CLIENT_LIVING_DRAW_EXCEPTION
                               : MC_CLIENT_LIVING_DRAW_UNCLASSIFIED_THROWABLE;
        } else if (v->fallback)
            lookup_failure(f, &v->lookup);
        return ld_fail(f, MC_CLIENT_LIVING_DRAW_CHILD_FAILED,
                       "Living draw actual downloaded texture child");
    }
    if ((int32_t)texture >= 0) {
        ld_at(f, "bw", "a", "(Ljava/lang/String;Ljava/lang/String;)Z", 24);
        if (!ld_display(f, mc_client_display_source_bind_id(f->scope->model.display, texture,
                                                            &e->detail.display)))
            return false;
    }
    e->site = caller;
    e->parent = parent;
    return true;
}

bool ld_brightness(ld_frame *f, float partial, float *out) {
    return ld_entity(f, mc_entity_render_brightness(f->actor, f->scope->model.math, partial, out,
                                                    &f->error->detail.entity));
}

static const uint16_t fur_units[] = {'/', 'm', 'o', 'b', '/', 's', 'h', 'e', 'e',
                                     'p', '_', 'f', 'u', 'r', '.', 'p', 'n', 'g'};
static const uint16_t shadow_units[] = {'%', 'c', 'l', 'a', 'm', 'p', '%', '/', 'm', 'i', 's', 'c',
                                        '/', 's', 'h', 'a', 'd', 'o', 'w', '.', 'p', 'n', 'g'};
static const mc_player_string fur = {SIZE_MAX, sizeof(fur_units) / sizeof(fur_units[0]), fur_units};
static const mc_player_string shadow = {SIZE_MAX, sizeof(shadow_units) / sizeof(shadow_units[0]),
                                        shadow_units};

const mc_player_string *ld_fur_literal(void) {
    return &fur;
}

const mc_player_string *ld_shadow_literal(void) {
    return &shadow;
}

void ld_registry_classify(ld_frame *f, mc_registry_classes *classes, const mc_registry_error *r) {
    mc_entity_render_throwable kind;
    const char *name;
    mc_entity_render_registry_classification(classes, r, &kind, &name);
    if (kind == MC_ENTITY_RENDER_NO_THROWABLE)
        return;
    f->error->throwable = kind == MC_ENTITY_RENDER_SOURCE_ERROR ? MC_CLIENT_LIVING_DRAW_ERROR
                                                                : MC_CLIENT_LIVING_DRAW_EXCEPTION;
    f->error->source_exception = true;
    f->error->exception_class = name;
}

void ld_access_classify(ld_frame *f, const mc_world_access_failure *failure) {
    const char *name = NULL;
    if (!mc_world_access_failure_source_exception(failure, &name))
        return;
    f->error->source_exception = true;
    f->error->exception_class = name;
    if (name != NULL && (!strcmp(name, "java/lang/ExceptionInInitializerError") ||
                         !strcmp(name, "java/lang/NoClassDefFoundError")))
        f->error->throwable = MC_CLIENT_LIVING_DRAW_ERROR;
    else if (name != NULL && (!strcmp(name, "java/lang/NullPointerException") ||
                              !strcmp(name, "java/lang/ClassCastException") ||
                              !strcmp(name, "java/lang/ArrayIndexOutOfBoundsException") ||
                              !strcmp(name, "java/lang/IllegalArgumentException") ||
                              !strcmp(name, "java/lang/IllegalStateException") ||
                              !strcmp(name, "java/lang/NegativeArraySizeException")))
        f->error->throwable = MC_CLIENT_LIVING_DRAW_EXCEPTION;
    else
        f->error->throwable = MC_CLIENT_LIVING_DRAW_UNCLASSIFIED_THROWABLE;
}
