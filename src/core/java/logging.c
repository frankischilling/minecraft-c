#include "logging_internal.h"
#include <stdlib.h>
#include <string.h>

_Static_assert(sizeof(mc_java_logging_error) <= 512u, "passive logging error stack bound");
static const uint16_t global_name[] = {'g', 'l', 'o', 'b', 'a', 'l'};
static const uint16_t minecraft_name[] = {'M', 'i', 'n', 'e', 'c', 'r', 'a', 'f', 't'};
static const char *const logger_ctor =
    "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/Module;Ljava/util/logging/LogManager;Z)V";
static const char *const global_ctor = "(Ljava/lang/String;)V";
static const char *const register_desc = "(Ljava/util/logging/Logger;Z)Z";

static mc_java_logging_site site(const char *cl, const char *method, const char *desc, int32_t pc) {
    return (mc_java_logging_site){cl, method, desc, pc};
}

static mc_java_logging_site native_site(void) {
    return site(NULL, NULL, NULL, -1);
}

void mc_java_logging_error_init(mc_java_logging_error *e) {
    if (e != NULL) {
        memset(e, 0, sizeof(*e));
        e->site = e->game_caller = e->cause_site = native_site();
    }
}

static mc_java_logging_status fail(mc_java_logging_error *e, mc_java_logging_status status,
                                   const char *op, mc_java_logging_site where) {
    e->status = status;
    e->operation = op;
    e->site = where;
    e->native_policy = true;
    return status;
}

static void prefix(const mc_java_logging *s, mc_java_logging_error *e) {
    e->stage = s->stage;
    e->last_completed = s->last_completed;
    e->native_bytes = s->native_bytes;
    e->native_allocations = s->allocations;
    e->raw_loggers = s->raw_loggers;
    e->registry_count = s->registry_count;
}

static bool increment(atomic_size_t *count) {
    size_t value = atomic_load_explicit(count, memory_order_relaxed);
    while (value != SIZE_MAX) {
        if (atomic_compare_exchange_weak_explicit(count, &value, value + 1u, memory_order_acq_rel,
                                                  memory_order_relaxed))
            return true;
    }
    return false;
}

/* A concurrent nonfinal release may make this the last occurrence before
 * our post-unlock commit. Preserve it and require a guarded final retry. */
static bool decrement_nonfinal(atomic_size_t *count) {
    size_t value = atomic_load_explicit(count, memory_order_acquire);
    while (value > 1u) {
        if (atomic_compare_exchange_weak_explicit(count, &value, value - 1u, memory_order_acq_rel,
                                                  memory_order_acquire))
            return true;
    }
    return false;
}

static void leave(mc_java_logging *s) {
    (void)atomic_fetch_sub_explicit(&s->active_calls, 1u, memory_order_acq_rel);
}

/* Do not commit output/count changes after a failed unlock. A genuinely
 * uncertain mutex stays quarantined/owned, never destroyed as if unlocked. */
static bool unlock(mc_java_logging *s, mc_java_logging_error *e, bool entered) {
    if (entered)
        s->entered = false;
    const int result = mtx_unlock(&s->mutex);
    if (result != thrd_success) {
        int expected = thrd_success;
        (void)atomic_compare_exchange_strong_explicit(&s->unlock_failure, &expected, result,
                                                      memory_order_release, memory_order_relaxed);
        atomic_store_explicit(&s->mutex_quarantined, true, memory_order_release);
    }
    if (atomic_load_explicit(&s->mutex_quarantined, memory_order_acquire)) {
        e->cause_status = e->status;
        e->cause_site = e->site;
        e->mutex_result = atomic_load_explicit(&s->unlock_failure, memory_order_acquire);
        fail(e, MC_JAVA_LOGGING_MUTEX_FAILED, "logging mutex unlock provider frontier",
             native_site());
        return false;
    }
    return true;
}

static bool begin(mc_java_logging *s, mc_java_logging_error *e) {
    if (s == NULL) {
        fail(e, MC_JAVA_LOGGING_INVALID_ARGUMENT, "logging live service receiver", native_site());
        return false;
    }
    if (atomic_load_explicit(&s->mutex_quarantined, memory_order_acquire)) {
        e->mutex_result = atomic_load_explicit(&s->unlock_failure, memory_order_acquire);
        fail(e, MC_JAVA_LOGGING_MUTEX_FAILED, "logging quarantined mutex", native_site());
        return false;
    }
    if (!increment(&s->active_calls)) {
        fail(e, MC_JAVA_LOGGING_LIMIT, "logging active call count", native_site());
        return false;
    }
    const int result = mtx_lock(&s->mutex);
    if (result != thrd_success) {
        leave(s);
        e->mutex_result = result;
        fail(e, MC_JAVA_LOGGING_MUTEX_FAILED, "logging mutex acquisition", native_site());
        return false;
    }
    prefix(s, e);
    if (s->entered || s->phase == MC_JAVA_LOGGING_RETIRING ||
        atomic_load_explicit(&s->mutex_quarantined, memory_order_acquire)) {
        fail(e, s->entered ? MC_JAVA_LOGGING_BUSY : MC_JAVA_LOGGING_INVALID_PHASE,
             "logging active/terminal admission", native_site());
        (void)unlock(s, e, false); /* Preserve the outer entered flag on recursive entry. */
        leave(s);
        return false;
    }
    s->entered = true;
    return true;
}

static mc_java_logging_status done(mc_java_logging *s, mc_java_logging_error *e,
                                   mc_java_logging_error *out) {
    prefix(s, e);
    (void)unlock(s, e, true);
    leave(s);
    if (out != NULL)
        *out = *e;
    return e->status;
}

mc_java_logging_options mc_java_logging_default_options(void) {
    return (mc_java_logging_options){MC_JAVA_LOGGING_NORMAL_DEFAULT, 1048576u, SIZE_MAX, 3u};
}

mc_java_logging_status mc_java_logging_create(const mc_java_logging_options *requested,
                                              mc_java_logging **out, mc_java_logging_error *error) {
    mc_java_logging_error e;
    mc_java_logging_error_init(&e);
    const mc_java_logging_options options =
        requested == NULL ? mc_java_logging_default_options() : *requested;
    if (out == NULL || (options.profile != MC_JAVA_LOGGING_NORMAL_DEFAULT &&
                        options.profile != MC_JAVA_LOGGING_CUSTOM_CONFIGURATION_UNIMPLEMENTED)) {
        fail(&e, MC_JAVA_LOGGING_INVALID_ARGUMENT, "logging native options/output", native_site());
    } else if (options.max_native_bytes < sizeof(mc_java_logging)) {
        fail(&e, MC_JAVA_LOGGING_LIMIT, "logging cold owner byte bound", native_site());
    } else {
        mc_java_logging *s = calloc(1, sizeof(*s));
        if (s == NULL) {
            fail(&e, MC_JAVA_LOGGING_ALLOCATION_FAILED, "logging cold owner allocation",
                 native_site());
        } else {
            const int result = mtx_init(&s->mutex, mtx_recursive);
            if (result != thrd_success) {
                free(s);
                e.mutex_result = result;
                fail(&e, MC_JAVA_LOGGING_MUTEX_FAILED, "logging native recursive mutex",
                     native_site());
            } else {
                s->options = options;
                s->native_bytes = sizeof(*s);
                atomic_init(&s->service_holds, 1u);
                atomic_init(&s->external_logger_holds, 0u);
                atomic_init(&s->active_calls, 0u);
                atomic_init(&s->mutex_quarantined, false);
                atomic_init(&s->unlock_failure, thrd_success);
                mc_java_logging_error_init(&s->failure);
                *out = s;
            }
        }
    }
    if (error != NULL)
        *error = e;
    return e.status;
}

static void *allocate(mc_java_logging *s, size_t bytes, mc_java_logging_site where,
                      mc_java_logging_error *e) {
    if (bytes > SIZE_MAX - sizeof(mc_java_log_allocation) ||
        s->allocations >= s->options.max_allocations ||
        bytes + sizeof(mc_java_log_allocation) > s->options.max_native_bytes - s->native_bytes) {
        fail(e, MC_JAVA_LOGGING_LIMIT, "logging native child storage bound", where);
        return NULL;
    }
    mc_java_log_allocation *a = calloc(1, sizeof(*a) + bytes);
    if (a == NULL) {
        fail(e, MC_JAVA_LOGGING_ALLOCATION_FAILED, "logging native child allocation", where);
        return NULL;
    }
    a->next = s->allocations_head;
    s->allocations_head = a;
    s->native_bytes += sizeof(*a) + bytes;
    ++s->allocations;
    return a + 1;
}

static bool configuration(mc_java_logging *s, mc_java_logger *n, mc_java_logging_error *e) {
    const bool global = n->kind == MC_JAVA_LOGGER_GLOBAL;
    n->pending_configuration = allocate(s, sizeof(*n->pending_configuration),
                                        site("java/util/logging/Logger", "<init>",
                                             global ? global_ctor : logger_ctor, global ? 22 : 24),
                                        e);
    if (n->pending_configuration == NULL)
        return false;
    mc_java_log_configuration *c = n->pending_configuration;
    c->last_store = -1;
    c->pending_handlers =
        allocate(s, sizeof(*c->pending_handlers),
                 site("java/util/logging/Logger$ConfigurationData", "<init>", "()V", 5), e);
    if (c->pending_handlers == NULL)
        return false;
    c->handlers = c->pending_handlers; /* Genuine empty collection, source12. */
    c->last_store = 12;
    c->delegate = c;
    c->last_store = 17;
    c->use_parent_handlers = true;
    c->last_store = 22;
    c->level_value = 800;
    c->last_store = 32;
    c->ready = true;
    n->configuration = c;
    n->last_store = global ? 29 : 31;
    return true;
}

static bool name_value(mc_java_logging *s, mc_java_logger *n, const uint16_t *units, size_t length,
                       mc_java_logging_error *e) {
    const bool global = n->kind == MC_JAVA_LOGGER_GLOBAL;
    n->pending_name = allocate(s, (length == 0 ? 1u : length) * sizeof(uint16_t),
                               site("java/util/logging/Logger", "<init>",
                                    global ? global_ctor : logger_ctor, global ? 13 : 36),
                               e);
    if (n->pending_name == NULL)
        return false;
    if (length != 0)
        memcpy(n->pending_name, units, length * sizeof(uint16_t));
    n->name = n->pending_name;
    n->name_length = length;
    n->last_store = global ? 13 : 36;
    return true;
}

static mc_java_logger *new_logger(mc_java_logging *s, mc_java_logger_kind kind,
                                  mc_java_logging_error *e) {
    mc_java_logging_site where;
    if (kind == MC_JAVA_LOGGER_GLOBAL)
        where = site("java/util/logging/Logger", "<clinit>", "()V", 51);
    else if (kind == MC_JAVA_LOGGER_ROOT)
        where = site("java/util/logging/LogManager$2", "run", "()Ljava/lang/Object;", 62);
    else
        where = site(
            "java/util/logging/LogManager", "demandLogger",
            "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/Module;)Ljava/util/logging/Logger;",
            12);
    if (s->raw_loggers >= s->options.max_loggers) {
        fail(e, MC_JAVA_LOGGING_LIMIT, "logging raw Logger count bound", where);
        return NULL;
    }
    mc_java_logger *n = allocate(s, sizeof(*n), where, e);
    if (n == NULL)
        return NULL;
    ++s->raw_loggers;
    s->pending_logger = n;
    n->kind = kind;
    n->storage = s; /* Native backing, separate from source manager. */
    n->phase = MC_JAVA_LOGGING_CONSTRUCTING;
    n->last_store = -1;
    atomic_init(&n->external_holds, 0u);
    n->bundle_absent = true; /* Real NO_RESOURCE_BUNDLE policy, not a null bundle token. */
    n->last_store = 8;
    if (kind == MC_JAVA_LOGGER_GLOBAL) {
        if (!name_value(s, n, global_name, 6, e))
            goto failed;
        n->system_logger = true;
        n->last_store = 18;
        if (!configuration(s, n, e))
            goto failed;
    } else {
        n->manager = s;
        n->last_store = 14;
        n->system_logger = kind == MC_JAVA_LOGGER_ROOT;
        n->last_store = 20;
        if (!configuration(s, n, e))
            goto failed;
        if (!name_value(s, n, kind == MC_JAVA_LOGGER_ROOT ? NULL : minecraft_name,
                        kind == MC_JAVA_LOGGER_ROOT ? 0u : 9u, e))
            goto failed;
        /* setupResourceInfo(null, normal caller)42 completes its absent-bundle
         * contract; JVM caller Module/WeakReference machinery is not replicated. */
    }
    n->phase = MC_JAVA_LOGGING_READY;
    return n;
failed:
    n->phase = MC_JAVA_LOGGING_FAILED;
    return NULL;
}

static mc_java_logger *find(mc_java_logging *s, const uint16_t *name, size_t length) {
    for (mc_java_log_entry *i = s->registry_head; i != NULL; i = i->next) {
        mc_java_logger *n = i->logger;
        if (n->name_length == length &&
            (length == 0 || memcmp(n->name, name, length * sizeof(uint16_t)) == 0))
            return n;
    }
    return NULL;
}

static bool register_logger(mc_java_logging *s, mc_java_logger *n, mc_java_logging_error *e) {
    if (find(s, n->name, n->name_length) != NULL) {
        fail(
            e, MC_JAVA_LOGGING_INVALID_PHASE, "logging duplicate internal registration",
            site("java/util/logging/LogManager$LoggerContext", "addLocalLogger", register_desc, 0));
        return false;
    }
    n->manager = s; /* Context.setLogManager72 precedes WeakRef allocation75. */
    s->pending_registration = allocate(
        s, sizeof(*s->pending_registration),
        site("java/util/logging/LogManager$LoggerContext", "addLocalLogger", register_desc, 75), e);
    if (s->pending_registration == NULL)
        return false;
    mc_java_log_entry *entry = s->pending_registration;
    entry->logger = n;
    if (n->kind == MC_JAVA_LOGGER_ROOT) {
        n->configuration->declared_level_present = true;
        n->configuration->declared_level = s->configuration->root_level;
        n->configuration->level_value = s->configuration->root_level;
    } else {
        n->parent = s->root; /* Actual parent update before named-map visibility. */
        n->configuration->level_value = s->root->configuration->level_value;
    }
    if (s->registry_tail == NULL)
        s->registry_head = entry;
    else
        s->registry_tail->next = entry;
    s->registry_tail = entry;
    ++s->registry_count;
    n->registered = true;
    s->pending_registration = NULL;
    return true;
}

static bool initialize(mc_java_logging *s, mc_java_logging_error *e) {
    s->phase = MC_JAVA_LOGGING_CONSTRUCTING;
    s->logger_class_phase = MC_JAVA_LOGGING_CONSTRUCTING;
    s->stage = MC_JAVA_LOGGING_STAGE_GLOBAL;
    s->global = new_logger(s, MC_JAVA_LOGGER_GLOBAL, e);
    if (s->global == NULL) {
        s->logger_class_phase = MC_JAVA_LOGGING_FAILED;
        goto failed;
    }
    s->pending_logger = NULL; /* Actual Logger.global61 publication. */
    s->logger_class_phase = MC_JAVA_LOGGING_READY;
    s->last_completed = MC_JAVA_LOGGING_STAGE_GLOBAL;
    s->stage = MC_JAVA_LOGGING_STAGE_MANAGER;
    s->manager_phase = MC_JAVA_LOGGING_CONSTRUCTING;
    /* The cold C owner is also the concrete manager representation. Its real
     * native map/configuration storage is not source-published until here35. */
    s->manager_published = true;
    s->last_completed = MC_JAVA_LOGGING_STAGE_MANAGER;
    s->stage = MC_JAVA_LOGGING_STAGE_ROOT;
    s->root = new_logger(s, MC_JAVA_LOGGER_ROOT, e);
    if (s->root == NULL)
        goto manager_failed;
    s->pending_logger = NULL; /* Actual LogManager$2 rootLogger79. */
    s->last_completed = MC_JAVA_LOGGING_STAGE_ROOT;
    s->stage = MC_JAVA_LOGGING_STAGE_CONFIGURATION;
    if (s->options.profile != MC_JAVA_LOGGING_NORMAL_DEFAULT) {
        fail(e, MC_JAVA_LOGGING_UNSUPPORTED_CONFIGURATION,
             "logging nondefault configuration dependency",
             site("java/util/logging/LogManager$2", "run", "()Ljava/lang/Object;", 87));
        goto manager_failed;
    }
    s->configuration =
        allocate(s, sizeof(*s->configuration),
                 site("java/util/logging/LogManager$2", "run", "()Ljava/lang/Object;", 87), e);
    if (s->configuration == NULL)
        goto manager_failed;
    *s->configuration = (mc_java_log_defaults){800, 800, true, true, true, true};
    s->root_handler_state = 3; /* STATE_UNINITIALIZED: no actual ConsoleHandler. */
    s->last_completed = MC_JAVA_LOGGING_STAGE_CONFIGURATION;
    s->stage = MC_JAVA_LOGGING_STAGE_REGISTER_ROOT;
    if (!register_logger(s, s->root, e))
        goto manager_failed;
    s->last_completed = MC_JAVA_LOGGING_STAGE_REGISTER_ROOT;
    s->stage = MC_JAVA_LOGGING_STAGE_REGISTER_GLOBAL;
    if (!register_logger(s, s->global, e))
        goto manager_failed;
    s->last_completed = MC_JAVA_LOGGING_STAGE_REGISTER_GLOBAL;
    s->manager_phase = MC_JAVA_LOGGING_READY;
    s->stage = MC_JAVA_LOGGING_STAGE_MINECRAFT;
    mc_java_logger *minecraft = new_logger(s, MC_JAVA_LOGGER_MINECRAFT, e);
    if (minecraft == NULL)
        goto failed;
    s->last_completed = MC_JAVA_LOGGING_STAGE_MINECRAFT;
    s->stage = MC_JAVA_LOGGING_STAGE_REGISTER_MINECRAFT;
    if (!register_logger(s, minecraft, e))
        goto failed;
    s->minecraft = minecraft;
    s->pending_logger = NULL;
    s->last_completed = MC_JAVA_LOGGING_STAGE_REGISTER_MINECRAFT;
    s->stage = s->last_completed = MC_JAVA_LOGGING_STAGE_READY;
    s->phase = MC_JAVA_LOGGING_READY;
    return true;
manager_failed:
    s->manager_phase = MC_JAVA_LOGGING_FAILED;
failed:
    s->phase = MC_JAVA_LOGGING_FAILED;
    prefix(s, e);
    s->failure = *e;
    return false;
}

mc_java_logging_status mc_java_logging_get_minecraft(mc_java_logging *s, mc_java_logger **out,
                                                     mc_java_logging_error *error) {
    mc_java_logging_error e;
    mc_java_logging_error_init(&e);
    e.game_caller = site("fm", "<clinit>", "()V", 2);
    if (out == NULL) {
        fail(&e, MC_JAVA_LOGGING_INVALID_ARGUMENT, "logging Logger occurrence output",
             native_site());
    } else if (begin(s, &e)) {
        mc_java_logger *n = NULL;
        if (s->phase == MC_JAVA_LOGGING_FAILED) {
            e = s->failure;
            e.cause_status = e.status;
            e.cause_site = e.site;
            e.status = MC_JAVA_LOGGING_LATCHED_FAILURE;
        } else if (s->phase == MC_JAVA_LOGGING_COLD && !initialize(s, &e)) {
            /* Retain the complete failed native attempt, not a source return. */
        } else {
            n = find(s, minecraft_name, 9);
            if (n == NULL || n != s->minecraft)
                fail(&e, MC_JAVA_LOGGING_INVALID_PHASE, "logging actual named registry identity",
                     native_site());
        }
        prefix(s, &e);
        if (unlock(s, &e, true) && e.status == MC_JAVA_LOGGING_OK) {
            if (!increment(&s->external_logger_holds)) {
                fail(&e, MC_JAVA_LOGGING_LIMIT, "logging external Logger count", native_site());
            } else if (!increment(&n->external_holds)) {
                (void)atomic_fetch_sub_explicit(&s->external_logger_holds, 1u,
                                                memory_order_acq_rel);
                fail(&e, MC_JAVA_LOGGING_LIMIT, "logging Logger hold count", native_site());
            } else {
                *out = n;
            }
        }
        leave(s);
    }
    if (error != NULL)
        *error = e;
    return e.status;
}

mc_java_logging_status mc_java_logging_retain(mc_java_logging *s, mc_java_logging_error *error) {
    mc_java_logging_error e;
    mc_java_logging_error_init(&e);
    if (begin(s, &e)) {
        prefix(s, &e);
        if (unlock(s, &e, true) && !increment(&s->service_holds))
            fail(&e, MC_JAVA_LOGGING_LIMIT, "logging service hold count", native_site());
        leave(s);
    }
    if (error != NULL)
        *error = e;
    return e.status;
}

mc_java_logging_status mc_java_logger_retain(mc_java_logger *n, mc_java_logging_error *error) {
    mc_java_logging_error e;
    mc_java_logging_error_init(&e);
    if (n == NULL) {
        fail(&e, MC_JAVA_LOGGING_INVALID_ARGUMENT, "logging live Logger receiver", native_site());
    } else if (begin(n->storage, &e)) {
        mc_java_logging *s = n->storage;
        if (n->phase != MC_JAVA_LOGGING_READY)
            fail(&e, MC_JAVA_LOGGING_INVALID_PHASE, "logging incomplete Logger retain",
                 native_site());
        prefix(s, &e);
        if (unlock(s, &e, true) && e.status == MC_JAVA_LOGGING_OK) {
            if (!increment(&s->external_logger_holds))
                fail(&e, MC_JAVA_LOGGING_LIMIT, "logging external Logger count", native_site());
            else if (!increment(&n->external_holds)) {
                (void)atomic_fetch_sub_explicit(&s->external_logger_holds, 1u,
                                                memory_order_acq_rel);
                fail(&e, MC_JAVA_LOGGING_LIMIT, "logging Logger hold count", native_site());
            }
        }
        leave(s);
    }
    if (error != NULL)
        *error = e;
    return e.status;
}

mc_java_logging_status mc_java_logger_release(mc_java_logger **slot, mc_java_logging_error *error) {
    mc_java_logging_error e;
    mc_java_logging_error_init(&e);
    if (slot == NULL) {
        fail(&e, MC_JAVA_LOGGING_INVALID_ARGUMENT, "logging Logger release slot", native_site());
    } else if (*slot != NULL) {
        mc_java_logger *n = *slot;
        mc_java_logging *s = n->storage;
        if (begin(s, &e)) {
            const size_t holds = atomic_load_explicit(&n->external_holds, memory_order_acquire);
            if (holds == 0)
                fail(&e, MC_JAVA_LOGGING_INVALID_PHASE, "logging Logger occurrence already absent",
                     native_site());
            prefix(s, &e);
            if (unlock(s, &e, true) && e.status == MC_JAVA_LOGGING_OK) {
                (void)atomic_fetch_sub_explicit(&n->external_holds, 1u, memory_order_acq_rel);
                (void)atomic_fetch_sub_explicit(&s->external_logger_holds, 1u,
                                                memory_order_acq_rel);
                *slot = NULL;
            }
            leave(s);
        }
    }
    if (error != NULL)
        *error = e;
    return e.status;
}

static mc_java_logging_status release_service(mc_java_logging **slot, mc_java_logging_error *error,
                                              bool strict) {
    mc_java_logging_error e;
    mc_java_logging_error_init(&e);
    if (slot == NULL) {
        fail(&e, MC_JAVA_LOGGING_INVALID_ARGUMENT, "logging service retirement slot",
             native_site());
    } else if (*slot != NULL) {
        mc_java_logging *s = *slot;
        if (begin(s, &e)) {
            const size_t holds = atomic_load_explicit(&s->service_holds, memory_order_acquire);
            const size_t handles =
                atomic_load_explicit(&s->external_logger_holds, memory_order_acquire);
            const size_t calls = atomic_load_explicit(&s->active_calls, memory_order_acquire);
            if ((strict && holds != 1) || (holds == 1 && (handles != 0 || calls != 1))) {
                fail(&e, MC_JAVA_LOGGING_BUSY, "logging live service/Logger/call occurrences",
                     native_site());
                return done(s, &e, error);
            }
            const bool final = holds == 1;
            if (final)
                s->phase = MC_JAVA_LOGGING_RETIRING;
            prefix(s, &e);
            if (!unlock(s, &e, true)) {
                leave(s);
            } else if (!final) {
                if (decrement_nonfinal(&s->service_holds))
                    *slot = NULL;
                else
                    fail(&e, MC_JAVA_LOGGING_BUSY,
                         "logging concurrent final release requires retry", native_site());
                leave(s);
            } else {
                /* Caller owns the final service occurrence, no Logger handle
                 * or waiter exists, and every valid concurrent caller would
                 * need another occurrence. Source fields/statics are not reset. */
                leave(s);
                mtx_destroy(&s->mutex);
                mc_java_log_allocation *a = s->allocations_head;
                while (a != NULL) {
                    mc_java_log_allocation *next = a->next;
                    free(a);
                    a = next;
                }
                free(s);
                *slot = NULL;
            }
        }
    }
    if (error != NULL)
        *error = e;
    return e.status;
}

mc_java_logging_status mc_java_logging_release(mc_java_logging **s, mc_java_logging_error *e) {
    return release_service(s, e, false);
}

mc_java_logging_status mc_java_logging_retire(mc_java_logging **s, mc_java_logging_error *e) {
    return release_service(s, e, true);
}

static mc_java_logger_state logger_state(const mc_java_logger *n) {
    mc_java_log_configuration *c = n->pending_configuration;
    const size_t holds = atomic_load_explicit(&n->external_holds, memory_order_acquire);
    return (mc_java_logger_state){
        .kind = n->kind,
        .phase = n->phase,
        .identity = n,
        .storage = n->storage,
        .manager = n->manager,
        .parent = n->parent,
        .name = {n->name, n->name_length},
        .pending_name = n->pending_name,
        .configuration = n->configuration,
        .pending_configuration = c,
        .handlers = c == NULL ? NULL : c->handlers,
        .pending_handlers = c == NULL ? NULL : c->pending_handlers,
        .delegate = c == NULL ? NULL : c->delegate,
        .external_holds = holds,
        .handler_count = c == NULL || c->handlers == NULL ? 0 : c->handlers->count,
        .name_stored = n->name != NULL,
        .configuration_stored = n->configuration != NULL,
        .system_logger = n->system_logger,
        .bundle_absent = n->bundle_absent,
        .use_parent_handlers = c != NULL && c->use_parent_handlers,
        .declared_level_present = c != NULL && c->declared_level_present,
        .registered = n->registered,
        .filter_present = c != NULL && c->filter_present,
        .effective_level = c == NULL ? 0 : c->level_value,
        .declared_level = c == NULL ? 0 : c->declared_level,
        .constructor_last_store_pc = n->last_store,
        .configuration_constructor_last_store_pc = c == NULL ? -1 : c->last_store};
}

mc_java_logging_status mc_java_logger_inspect(const mc_java_logger *n, mc_java_logger_state *out,
                                              mc_java_logging_error *error) {
    mc_java_logging_error e;
    mc_java_logging_error_init(&e);
    if (n == NULL || out == NULL) {
        fail(&e, MC_JAVA_LOGGING_INVALID_ARGUMENT, "logging inspection receiver/output",
             native_site());
    } else if (begin(n->storage, &e)) {
        mc_java_logging *s = n->storage;
        const size_t calls = atomic_load_explicit(&s->active_calls, memory_order_acquire);
        if (calls != 1)
            fail(&e, MC_JAVA_LOGGING_BUSY, "logging inspection requires quiescence", native_site());
        const mc_java_logger_state state = logger_state(n);
        prefix(s, &e);
        if (unlock(s, &e, true) && e.status == MC_JAVA_LOGGING_OK)
            *out = state;
        leave(s);
    }
    if (error != NULL)
        *error = e;
    return e.status;
}

mc_java_logging_status mc_java_logging_inspect(mc_java_logging *s, mc_java_logging_state *out,
                                               mc_java_logging_error *error) {
    mc_java_logging_error e;
    mc_java_logging_error_init(&e);
    if (out == NULL) {
        fail(&e, MC_JAVA_LOGGING_INVALID_ARGUMENT, "logging service inspection output",
             native_site());
    } else if (begin(s, &e)) {
        const size_t calls = atomic_load_explicit(&s->active_calls, memory_order_acquire);
        const size_t holds = atomic_load_explicit(&s->service_holds, memory_order_acquire);
        const size_t handles =
            atomic_load_explicit(&s->external_logger_holds, memory_order_acquire);
        if (calls != 1)
            fail(&e, MC_JAVA_LOGGING_BUSY, "logging inspection requires quiescence", native_site());
        mc_java_logging_state state = {
            .phase = s->phase,
            .logger_class_phase = s->logger_class_phase,
            .manager_phase = s->manager_phase,
            .stage = s->stage,
            .last_completed = s->last_completed,
            .service_holds = holds,
            .external_logger_holds = handles,
            .active_calls = calls - 1u,
            .native_bytes = s->native_bytes,
            .native_allocations = s->allocations,
            .raw_loggers = s->raw_loggers,
            .registry_count = s->registry_count,
            .entered = false,
            .manager_published = s->manager_published,
            .configuration_published = s->configuration != NULL,
            .mutex_quarantined = false,
            .root_handler_state = s->root_handler_state,
            .console_configured = s->configuration != NULL && s->configuration->console_handler,
            .console_constructed = false,
            .default_level = s->configuration == NULL ? 0 : s->configuration->root_level,
            .configured_console_level =
                s->configuration == NULL ? 0 : s->configuration->console_level,
            .global = s->global,
            .root = s->root,
            .minecraft = s->minecraft,
            .pending_logger = s->pending_logger,
            .configuration = s->configuration,
            .pending_registration = s->pending_registration,
            .failure = &s->failure};
        size_t i = 0;
        for (mc_java_log_entry *entry = s->registry_head; entry != NULL && i < 3;
             entry = entry->next)
            state.registry[i++] = entry->logger;
        prefix(s, &e);
        if (unlock(s, &e, true) && e.status == MC_JAVA_LOGGING_OK)
            *out = state;
        leave(s);
    }
    if (error != NULL)
        *error = e;
    return e.status;
}
