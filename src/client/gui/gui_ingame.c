#include "../application_state.h"
#include "gui_ingame_internal.h"
#include "mc/entity/player/player.h"

#include <stdlib.h>

static mc_client_gui_ingame_status publish(const mc_client_gui_ingame_error *e,
                                           mc_client_gui_ingame_error *out) {
    if (out != NULL)
        *out = *e;
    return e->status;
}

static mc_client_gui_ingame_status fail(mc_client_gui_ingame_error *e,
                                        mc_client_gui_ingame_status status, const char *operation,
                                        const char *method, unsigned int pc) {
    e->status = status;
    e->operation = operation;
    e->site =
        (mc_client_gui_ingame_site){method == NULL ? NULL : "uq", method,
                                    method == NULL     ? NULL
                                    : method[1] == 'c' ? "()V"
                                                       : "(Lnet/minecraft/client/Minecraft;)V",
                                    pc};
    e->native_policy = true;
    return status;
}

static bool lock(mc_client_gui_ingame_classes *c, mc_client_gui_ingame_error *e) {
    if (c == NULL) {
        fail(e, MC_CLIENT_GUI_INGAME_INVALID_ARGUMENT, "GuiIngame native class receiver", NULL, 0);
        return false;
    }
    const int status = mtx_trylock(&c->mutex);
    if (status != thrd_success) {
        e->native_thread_error = status;
        fail(e,
             status == thrd_busy ? MC_CLIENT_GUI_INGAME_BUSY : MC_CLIENT_GUI_INGAME_THREAD_FAILED,
             "GuiIngame native class monitor", NULL, 0);
        return false;
    }
    return true;
}

static mc_client_gui_ingame_cause cause(const mc_client_gui_ingame_error *e) {
    return (mc_client_gui_ingame_cause){
        e->status, e->operation,          e->site, e->native_policy, e->model,
        e->random, e->native_thread_error};
}

static bool active_use_locked(mc_client_gui_ingame_classes *c, mc_client_gui_ingame_error *e) {
    if (c->retiring) {
        fail(e, MC_CLIENT_GUI_INGAME_BUSY, "GuiIngame class native retirement", NULL, 0);
        return false;
    }
    if (c->phase == MC_CLIENT_GUI_INGAME_CLASS_READY)
        return true;
    if (c->phase == MC_CLIENT_GUI_INGAME_CLASS_FAILED) {
        fail(e, MC_CLIENT_GUI_INGAME_CLASS_ERRONEOUS, "previous GuiIngame class activation failure",
             "<clinit>", c->failure.site.pc);
        e->first_class_failure = cause(&c->failure);
        return false;
    }
    if (c->phase == MC_CLIENT_GUI_INGAME_CLASS_INITIALIZING) {
        fail(e, MC_CLIENT_GUI_INGAME_BUSY, "GuiIngame class initializer active", NULL, 0);
        return false;
    }
    c->phase = MC_CLIENT_GUI_INGAME_CLASS_INITIALIZING;
    c->ub_initialized = true; /* Actual superclass has no <clinit>/static effects. */
    c->last_pc = 0;
    mc_client_model_status status =
        mc_client_item_renderer_allocate(c->models, c->static_item_bytes, &c->item);
    if (status == MC_CLIENT_MODEL_OK) {
        c->last_pc = 4;
        status = mc_client_item_renderer_construct(c->item, &e->model);
    }
    if (status != MC_CLIENT_MODEL_OK) {
        e->model.status = status;
        fail(e, MC_CLIENT_GUI_INGAME_MODEL_FAILED, "actual uq static ItemRenderer construction",
             "<clinit>", c->last_pc);
        c->failure = *e;
        c->phase = MC_CLIENT_GUI_INGAME_CLASS_FAILED;
        return false;
    }
    c->d = c->item; /* Actual putstatic7, never a notice/manager alias. */
    c->last_store_pc = 7;
    c->last_pc = 10;
    c->phase = MC_CLIENT_GUI_INGAME_CLASS_READY;
    return true;
}

mc_client_gui_ingame_status mc_client_gui_ingame_classes_create(mc_client_render_classes *models,
                                                                mc_java_random_profile *profile,
                                                                size_t static_item_bytes,
                                                                mc_client_gui_ingame_classes **out,
                                                                mc_client_gui_ingame_error *error) {
    mc_client_gui_ingame_error e = {0};
    if (models == NULL || profile == NULL || out == NULL)
        fail(&e, MC_CLIENT_GUI_INGAME_INVALID_ARGUMENT, "GuiIngame class dependencies/output", NULL,
             0);
    else {
        mc_client_gui_ingame_classes *c = calloc(1, sizeof(*c));
        if (c == NULL)
            fail(&e, MC_CLIENT_GUI_INGAME_ALLOCATION_FAILED, "GuiIngame cold namespace", NULL, 0);
        else {
            const int status = mtx_init(&c->mutex, mtx_plain);
            if (status != thrd_success) {
                e.native_thread_error = status;
                fail(&e, MC_CLIENT_GUI_INGAME_THREAD_FAILED, "GuiIngame native monitor", NULL, 0);
                free(c);
            } else {
                e.model.status = mc_client_render_classes_retain(models, profile);
                if (e.model.status != MC_CLIENT_MODEL_OK) {
                    fail(&e, MC_CLIENT_GUI_INGAME_MODEL_FAILED, "GuiIngame same model/profile hold",
                         NULL, 0);
                    mtx_destroy(&c->mutex);
                    free(c);
                } else {
                    c->holds = 1;
                    c->models = models;
                    c->profile = profile;
                    c->models_held = true;
                    c->static_item_bytes = static_item_bytes;
                    *out = c;
                }
            }
        }
    }
    return publish(&e, error);
}

mc_client_gui_ingame_status
mc_client_gui_ingame_classes_active_use(mc_client_gui_ingame_classes *c,
                                        mc_client_gui_ingame_error *error) {
    mc_client_gui_ingame_error e = {0};
    if (lock(c, &e)) {
        (void)active_use_locked(c, &e);
        (void)mtx_unlock(&c->mutex);
    }
    return publish(&e, error);
}

mc_client_gui_ingame_status mc_client_gui_ingame_classes_retain(mc_client_gui_ingame_classes *c,
                                                                mc_client_gui_ingame_error *error) {
    mc_client_gui_ingame_error e = {0};
    if (lock(c, &e)) {
        if (c->retiring)
            fail(&e, MC_CLIENT_GUI_INGAME_BUSY, "GuiIngame retiring class hold", NULL, 0);
        else if (c->holds == SIZE_MAX)
            fail(&e, MC_CLIENT_GUI_INGAME_LIMIT, "GuiIngame class hold bound", NULL, 0);
        else
            ++c->holds;
        (void)mtx_unlock(&c->mutex);
    }
    return publish(&e, error);
}

/* Called only for the actual final namespace occurrence, before consuming any
 * caller instance. A failed canonical child cleanup keeps its native slot. */
static bool final_prepare(mc_client_gui_ingame_classes *c, mc_client_gui_ingame_error *e) {
    c->retiring = true;
    e->model.status = mc_client_item_renderer_free(&c->item);
    if (e->model.status != MC_CLIENT_MODEL_OK) {
        fail(e,
             e->model.status == MC_CLIENT_MODEL_BUSY ? MC_CLIENT_GUI_INGAME_BUSY
                                                     : MC_CLIENT_GUI_INGAME_MODEL_FAILED,
             "GuiIngame static/pending ItemRenderer retirement", NULL, 0);
        return false;
    }
    c->static_item_retired = true;
    if (c->models_held) {
        e->model.status = mc_client_render_classes_release(c->models);
        if (e->model.status != MC_CLIENT_MODEL_OK) {
            fail(e, MC_CLIENT_GUI_INGAME_MODEL_FAILED, "GuiIngame model-class occurrence release",
                 NULL, 0);
            return false;
        }
        c->models_held = false;
    }
    return true;
}

static void drop_locked(mc_client_gui_ingame_classes *c) {
    const bool final = --c->holds == 0;
    (void)mtx_unlock(&c->mutex);
    if (final) {
        mtx_destroy(&c->mutex);
        free(c);
    }
}

mc_client_gui_ingame_status
mc_client_gui_ingame_classes_release(mc_client_gui_ingame_classes **slot,
                                     mc_client_gui_ingame_error *error) {
    mc_client_gui_ingame_error e = {0};
    if (slot == NULL)
        fail(&e, MC_CLIENT_GUI_INGAME_INVALID_ARGUMENT, "GuiIngame class release slot", NULL, 0);
    else if (*slot != NULL && lock(*slot, &e)) {
        mc_client_gui_ingame_classes *c = *slot;
        if (c->holds == 1 && !final_prepare(c, &e))
            (void)mtx_unlock(&c->mutex);
        else {
            *slot = NULL;
            drop_locked(c);
        }
    }
    return publish(&e, error);
}

mc_client_gui_ingame_status
mc_client_gui_ingame_classes_inspect(mc_client_gui_ingame_classes *c,
                                     mc_client_gui_ingame_class_state *out,
                                     mc_client_gui_ingame_error *error) {
    mc_client_gui_ingame_error e = {0};
    if (out == NULL)
        fail(&e, MC_CLIENT_GUI_INGAME_INVALID_ARGUMENT, "GuiIngame class inspection output", NULL,
             0);
    else if (lock(c, &e)) {
        const mc_client_item_renderer *live_d = c->static_item_retired ? NULL : c->d;
        *out = (mc_client_gui_ingame_class_state){
            c->phase,
            c->holds,
            c->ub_initialized,
            c->retiring,
            live_d != NULL,
            c->last_pc,
            c->last_store_pc,
            live_d,
            c->static_item_retired || live_d != NULL ? NULL : c->item,
            c->failure};
        (void)mtx_unlock(&c->mutex);
    }
    return publish(&e, error);
}

mc_client_gui_ingame_status
mc_client_gui_ingame_allocate(mc_client_gui_ingame_classes *c,
                              const mc_client_gui_ingame_options *options,
                              mc_client_source_uq **out, mc_client_gui_ingame_error *error) {
    mc_client_gui_ingame_error e = {0};
    if (out == NULL)
        fail(&e, MC_CLIENT_GUI_INGAME_INVALID_ARGUMENT, "GuiIngame RAW instance output", NULL, 0);
    else if (lock(c, &e)) {
        const mc_client_gui_ingame_options selected =
            options == NULL ? (mc_client_gui_ingame_options){1024u * 1024u, SIZE_MAX} : *options;
        if (active_use_locked(c, &e)) {
            if (c->holds == SIZE_MAX || selected.maximum_bytes < sizeof(mc_client_source_uq))
                fail(&e, MC_CLIENT_GUI_INGAME_LIMIT, "GuiIngame RAW native allocation/hold bound",
                     NULL, 0);
            else {
                mc_client_source_uq *o = calloc(1, sizeof(*o));
                if (o == NULL)
                    fail(&e, MC_CLIENT_GUI_INGAME_ALLOCATION_FAILED, "GuiIngame RAW instance", NULL,
                         0);
                else {
                    o->classes = c;
                    o->options = selected;
                    o->holds = 1;
                    o->bytes = sizeof(*o);
                    ++c->holds;
                    *out = o;
                }
            }
        }
        (void)mtx_unlock(&c->mutex);
    }
    return publish(&e, error);
}

static void store(mc_client_source_uq *o, const char *name, unsigned int pc) {
    o->stores[o->store_count++] = (mc_client_gui_ingame_site){
        name, "<init>",
        name[0] == 'u' && name[1] == 'b' ? "()V" : "(Lnet/minecraft/client/Minecraft;)V", pc};
    o->last_pc = pc;
}

static void *allocate_child(mc_client_source_uq *o, size_t bytes, unsigned int pc) {
    o->last_pc = pc;
    ++o->attempts;
    if (o->allocations >= o->options.maximum_allocations || o->bytes > o->options.maximum_bytes ||
        bytes > o->options.maximum_bytes - o->bytes) {
        fail(&o->failure, MC_CLIENT_GUI_INGAME_LIMIT, "GuiIngame native child quota", "<init>", pc);
        return NULL;
    }
    void *result = calloc(1, bytes);
    if (result == NULL) {
        fail(&o->failure, MC_CLIENT_GUI_INGAME_ALLOCATION_FAILED,
             "GuiIngame native child allocation", "<init>", pc);
        return NULL;
    }
    ++o->allocations;
    o->bytes += bytes;
    return result;
}

static bool construct(mc_client_source_uq *o, client_application *app) {
    o->last_pc = 1; /* uq invokes ub.<init>; its Object1 has no source field effects. */
    o->fields.k = 0.0f;
    store(o, "ub", 6); /* Then actual ub return9, before uq new ArrayList5. */
    mc_client_gui_ingame_list *list = allocate_child(o, sizeof(*list), 5);
    if (list == NULL)
        return false;
    o->last_pc = 9; /* Actual empty ArrayList constructor; its allocation is retained. */
    o->fields.e = list;
    store(o, "uq", 12);
    o->pending_random = allocate_child(o, sizeof(*o->pending_random), 16);
    if (o->pending_random == NULL)
        return false;
    o->last_pc = 20;
    o->failure.random = mc_java_random_construct_default(o->classes->profile, o->pending_random);
    if (o->failure.random != MC_JAVA_RANDOM_PROFILE_OK) {
        fail(&o->failure, MC_CLIENT_GUI_INGAME_RANDOM_FAILED,
             "actual GuiIngame default Random constructor", "<init>", 20);
        return false;
    }
    o->fields.f = o->pending_random;
    o->pending_random = NULL;
    store(o, "uq", 23);
    o->fields.a = NULL;
    store(o, "uq", 28);
    o->fields.h = 0;
    store(o, "uq", 33);
    o->last_pc = 37;
    const mc_player_string *empty = mc_player_empty_literal();
    o->fields.i = empty;
    store(o, "uq", 39);
    o->fields.j = 0;
    store(o, "uq", 44);
    o->fields.l = false;
    store(o, "uq", 49);
    o->fields.c = 1.0f;
    store(o, "uq", 54);
    o->last_pc = 59;
    if (app != NULL && (app->constructor_phase == MC_CLIENT_APP_RETIRING ||
                        app->native_lifetime_phase == MC_CLIENT_APPLICATION_LIFETIME_RETIRED)) {
        fail(&o->failure, MC_CLIENT_GUI_INGAME_INVALID_PHASE,
             "GuiIngame native App borrow at g publication", "<init>", 59);
        return false;
    }
    o->fields.g = app;
    store(o, "uq", 59);
    o->last_pc = 62;
    o->failure.site =
        (mc_client_gui_ingame_site){"uq", "<init>", "(Lnet/minecraft/client/Minecraft;)V", 62};
    o->failure.source_returned = true;
    return true;
}

mc_client_gui_ingame_status mc_client_gui_ingame_construct(mc_client_source_uq *o,
                                                           client_application *app,
                                                           mc_client_gui_ingame_error *error) {
    mc_client_gui_ingame_error e = {0};
    if (o == NULL)
        fail(&e, MC_CLIENT_GUI_INGAME_INVALID_ARGUMENT, "GuiIngame native constructor receiver",
             NULL, 0);
    else if (lock(o->classes, &e)) {
        if (o->classes->retiring || o->phase != MC_CLIENT_GUI_INGAME_RAW)
            fail(&e, MC_CLIENT_GUI_INGAME_INVALID_PHASE, "GuiIngame one-shot constructor admission",
                 NULL, 0);
        else {
            o->phase = MC_CLIENT_GUI_INGAME_CONSTRUCTING;
            const bool returned = construct(o, app);
            o->phase = returned ? MC_CLIENT_GUI_INGAME_READY : MC_CLIENT_GUI_INGAME_FAILED;
            e = o->failure;
        }
        (void)mtx_unlock(&o->classes->mutex);
    }
    return publish(&e, error);
}

mc_client_gui_ingame_status mc_client_gui_ingame_retain(mc_client_source_uq *o,
                                                        mc_client_gui_ingame_error *error) {
    mc_client_gui_ingame_error e = {0};
    if (o == NULL)
        fail(&e, MC_CLIENT_GUI_INGAME_INVALID_ARGUMENT, "GuiIngame native retain receiver", NULL,
             0);
    else if (lock(o->classes, &e)) {
        if (o->classes->retiring || o->phase == MC_CLIENT_GUI_INGAME_RETIRING ||
            o->phase == MC_CLIENT_GUI_INGAME_CONSTRUCTING)
            fail(&e, MC_CLIENT_GUI_INGAME_BUSY, "GuiIngame entered/retiring occurrence", NULL, 0);
        else if (o->holds == SIZE_MAX)
            fail(&e, MC_CLIENT_GUI_INGAME_LIMIT, "GuiIngame instance hold bound", NULL, 0);
        else
            ++o->holds;
        (void)mtx_unlock(&o->classes->mutex);
    }
    return publish(&e, error);
}

static mc_client_gui_ingame_status release(mc_client_source_uq **slot, bool strict,
                                           mc_client_gui_ingame_error *error) {
    mc_client_gui_ingame_error e = {0};
    if (slot == NULL)
        fail(&e, MC_CLIENT_GUI_INGAME_INVALID_ARGUMENT, "GuiIngame native release slot", NULL, 0);
    else if (*slot != NULL && lock((*slot)->classes, &e)) {
        mc_client_source_uq *o = *slot;
        mc_client_gui_ingame_classes *c = o->classes;
        if (o->phase == MC_CLIENT_GUI_INGAME_CONSTRUCTING || (strict && o->holds != 1)) {
            fail(&e, MC_CLIENT_GUI_INGAME_BUSY, "GuiIngame active/external occurrence", NULL, 0);
            (void)mtx_unlock(&c->mutex);
        } else if (o->holds > 1) {
            --o->holds;
            *slot = NULL;
            (void)mtx_unlock(&c->mutex);
        } else {
            o->phase = MC_CLIENT_GUI_INGAME_RETIRING;
            if (c->holds == 1 && !final_prepare(c, &e))
                (void)mtx_unlock(&c->mutex);
            else {
                if (o->fields.e != NULL)
                    free(o->fields.e->entries); /* native array storage, never referents */
                free(o->fields.e);
                free(o->fields.f);
                free(o->pending_random);
                free(o);
                *slot = NULL;
                drop_locked(c);
            }
        }
    }
    return publish(&e, error);
}

mc_client_gui_ingame_status mc_client_gui_ingame_release(mc_client_source_uq **slot,
                                                         mc_client_gui_ingame_error *error) {
    return release(slot, false, error);
}

mc_client_gui_ingame_status mc_client_gui_ingame_retire(mc_client_source_uq **slot,
                                                        mc_client_gui_ingame_error *error) {
    return release(slot, true, error);
}

mc_client_gui_ingame_status mc_client_gui_ingame_inspect(mc_client_source_uq *o,
                                                         mc_client_gui_ingame_state *out,
                                                         mc_client_gui_ingame_error *error) {
    mc_client_gui_ingame_error e = {0};
    if (o == NULL || out == NULL)
        fail(&e, MC_CLIENT_GUI_INGAME_INVALID_ARGUMENT, "GuiIngame inspection receiver/output",
             NULL, 0);
    else if (lock(o->classes, &e)) {
        *out = (mc_client_gui_ingame_state){
            o->phase,   o->holds,          o->bytes,   o->allocations, o->attempts, o->fields,
            o->classes, o->pending_random, o->last_pc, o->store_count, {{0}},       o->failure};
        for (unsigned int i = 0; i < o->store_count; ++i)
            out->stores[i] = o->stores[i];
        (void)mtx_unlock(&o->classes->mutex);
    }
    return publish(&e, error);
}

bool mc_client_gui_ingame_list_size(const mc_client_gui_ingame_list *list, size_t *out) {
    if (list == NULL || out == NULL)
        return false;
    *out = list->size;
    return true;
}

/* b has its OWN descriptor/site handling; the constructor fail helper above
 * intentionally remains unchanged for existing constructor diagnostics. */
static mc_client_gui_ingame_status clear_failure(mc_client_gui_ingame_error *e,
                                                 mc_client_gui_ingame_status status,
                                                 const char *operation, unsigned int pc,
                                                 bool native) {
    e->status = status;
    e->operation = operation;
    e->site = (mc_client_gui_ingame_site){"uq", "b", "()V", pc};
    e->native_policy = native;
    return status;
}

mc_client_gui_ingame_status mc_client_gui_ingame_clear_messages(mc_client_source_uq *o,
                                                                mc_client_gui_ingame_error *error) {
    mc_client_gui_ingame_error e = {0};
    if (o == NULL || o->classes == NULL)
        clear_failure(&e, MC_CLIENT_GUI_INGAME_INVALID_ARGUMENT, "GuiIngame clear native receiver",
                      0, true);
    else {
        const int locked = mtx_trylock(&o->classes->mutex);
        if (locked != thrd_success) {
            e.native_thread_error = locked;
            clear_failure(&e,
                          locked == thrd_busy ? MC_CLIENT_GUI_INGAME_BUSY
                                              : MC_CLIENT_GUI_INGAME_THREAD_FAILED,
                          "GuiIngame clear native class monitor", 0, true);
        } else {
            if (o->classes->retiring ||
                o->classes->phase == MC_CLIENT_GUI_INGAME_CLASS_INITIALIZING ||
                o->phase == MC_CLIENT_GUI_INGAME_CONSTRUCTING ||
                o->phase == MC_CLIENT_GUI_INGAME_RETIRING)
                clear_failure(&e, MC_CLIENT_GUI_INGAME_BUSY,
                              "GuiIngame clear native active/retiring owner", 0, true);
            else {
                mc_client_gui_ingame_list *list = o->fields.e; /* actual getfield1 */
                if (list == NULL)
                    clear_failure(&e, MC_CLIENT_GUI_INGAME_SOURCE_NULL_REFERENCE,
                                  "uq actual List.clear receiver", 4, false);
                else if (list->size > list->capacity || (list->size != 0 && list->entries == NULL))
                    clear_failure(&e, MC_CLIENT_GUI_INGAME_INVALID_PHASE,
                                  "GuiIngame native List storage invariant", 4, true);
                else {
                    list->mod_count =
                        mc_java_i32_add(list->mod_count, 1); /* JDK putfield7, even empty */
                    void **array = list->entries;            /* JDK elementData11 */
                    const size_t old_size = list->size;      /* JDK size16 */
                    list->size = 0; /* JDK size0 putfield23, before null slots35 */
                    for (size_t i = 0; i < old_size; ++i)
                        array[i] = NULL;
                    e.site = (mc_client_gui_ingame_site){"uq", "b", "()V", 9};
                    e.source_returned = true;
                }
            }
            (void)mtx_unlock(&o->classes->mutex);
        }
    }
    return publish(&e, error);
}

static void tick_failure(mc_client_gui_ingame_error *error, mc_client_gui_ingame_status status,
                         const char *operation, unsigned int pc, bool native) {
    error->status = status;
    error->operation = operation;
    error->site = (mc_client_gui_ingame_site){"uq", "a", "()V", pc};
    error->native_policy = native;
}

mc_client_gui_ingame_status mc_client_gui_ingame_tick(mc_client_source_uq *o,
                                                      mc_client_gui_ingame_error *error) {
    mc_client_gui_ingame_error e = {0};
    if (o == NULL || o->classes == NULL) {
        tick_failure(&e, MC_CLIENT_GUI_INGAME_INVALID_ARGUMENT, "GuiIngame tick native receiver", 0,
                     true);
        return publish(&e, error);
    }
    const int locked = mtx_trylock(&o->classes->mutex);
    if (locked != thrd_success) {
        e.native_thread_error = locked;
        tick_failure(&e,
                     locked == thrd_busy ? MC_CLIENT_GUI_INGAME_BUSY
                                         : MC_CLIENT_GUI_INGAME_THREAD_FAILED,
                     "GuiIngame tick native class monitor", 0, true);
        return publish(&e, error);
    }
    if (o->classes->retiring || o->classes->phase == MC_CLIENT_GUI_INGAME_CLASS_INITIALIZING ||
        o->phase == MC_CLIENT_GUI_INGAME_CONSTRUCTING ||
        o->phase == MC_CLIENT_GUI_INGAME_RETIRING) {
        tick_failure(&e, MC_CLIENT_GUI_INGAME_BUSY, "GuiIngame tick native active/retiring owner",
                     0, true);
    } else {
        if (o->fields.j > 0)
            o->fields.j = mc_java_i32_sub(o->fields.j, 1); /* Actual putfield14. */
        o->fields.h = mc_java_i32_add(o->fields.h, 1);     /* Actual putfield24. */
        mc_client_gui_ingame_list *list = o->fields.e;     /* Actual e31 before List.size34. */
        if (list == NULL)
            tick_failure(&e, MC_CLIENT_GUI_INGAME_SOURCE_NULL_REFERENCE,
                         "uq tick actual List.size null receiver", 34, false);
        else if (list->size > list->capacity || list->size > INT32_MAX ||
                 (list->size != 0 && list->entries == NULL))
            tick_failure(&e, MC_CLIENT_GUI_INGAME_INVALID_PHASE,
                         "GuiIngame tick native List storage invariant", 34, true);
        else if (list->size != 0) {
            /* No source callback occurs between size34 and get47. This is the
             * same current List/first element, never a diagnostic substitute. */
            void *entry = list->entries[0];
            if (entry == NULL)
                tick_failure(&e, MC_CLIENT_GUI_INGAME_SOURCE_NULL_REFERENCE,
                             "uq tick actual sw.b null receiver", 56, false);
            else
                tick_failure(&e, MC_CLIENT_GUI_INGAME_UNIMPLEMENTED,
                             "uq tick canonical sw message owner/type dependency", 52, true);
        } else {
            e.site = (mc_client_gui_ingame_site){"uq", "a", "()V", 70};
            e.source_returned = true;
        }
    }
    (void)mtx_unlock(&o->classes->mutex);
    return publish(&e, error);
}
