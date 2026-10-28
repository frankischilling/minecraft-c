#include "mc/core/java/logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

static unsigned checks;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            fprintf(stderr, "logging check%u line%d: %s\n", checks, __LINE__, #x);                 \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static bool name_is(mc_java_properties_string_view v, const char *ascii) {
    if (v.units == NULL || v.length != strlen(ascii))
        return false;
    for (size_t i = 0; i < v.length; ++i)
        if (v.units[i] != (uint16_t)(unsigned char)ascii[i])
            return false;
    return true;
}

static int cold(void) {
    mc_java_logging *s = NULL;
    mc_java_logging_error e;
    CHECK(mc_java_logging_create(NULL, &s, &e) == MC_JAVA_LOGGING_OK);
    mc_java_logging *unchanged = s;
    mc_java_logging_options o = mc_java_logging_default_options();
    o.max_native_bytes = 0;
    CHECK(mc_java_logging_create(&o, &unchanged, &e) == MC_JAVA_LOGGING_LIMIT && unchanged == s);
    o = mc_java_logging_default_options();
    o.profile = (mc_java_logging_profile)99;
    CHECK(mc_java_logging_create(&o, &unchanged, &e) == MC_JAVA_LOGGING_INVALID_ARGUMENT &&
          unchanged == s);
    CHECK(mc_java_logging_get_minecraft(s, NULL, &e) == MC_JAVA_LOGGING_INVALID_ARGUMENT);
    mc_java_logging_state st;
    CHECK(mc_java_logging_inspect(s, &st, &e) == MC_JAVA_LOGGING_OK);
    CHECK(st.phase == MC_JAVA_LOGGING_COLD && st.logger_class_phase == MC_JAVA_LOGGING_COLD &&
          st.manager_phase == MC_JAVA_LOGGING_COLD && st.stage == MC_JAVA_LOGGING_STAGE_COLD &&
          st.service_holds == 1 && st.external_logger_holds == 0 && st.active_calls == 0 &&
          st.native_allocations == 0 && st.raw_loggers == 0 && st.registry_count == 0 &&
          st.global == NULL && st.root == NULL && st.minecraft == NULL &&
          st.pending_logger == NULL && !st.manager_published && !st.configuration_published &&
          !st.console_configured && !st.console_constructed && st.root_handler_state == 0 &&
          !st.entered);
    mc_java_logging *alias = s;
    CHECK(mc_java_logging_retain(alias, &e) == MC_JAVA_LOGGING_OK);
    CHECK(mc_java_logging_retire(&s, &e) == MC_JAVA_LOGGING_BUSY && s == alias);
    CHECK(mc_java_logging_release(&alias, &e) == MC_JAVA_LOGGING_OK && alias == NULL);
    CHECK(mc_java_logging_retire(&s, &e) == MC_JAVA_LOGGING_OK && s == NULL);
    CHECK(mc_java_logging_retire(&s, &e) == MC_JAVA_LOGGING_OK);
    return 0;
}

static int ready(size_t *complete_bytes) {
    mc_java_logging *s = NULL;
    mc_java_logger *a = NULL, *b = NULL;
    mc_java_logging_error e;
    CHECK(mc_java_logging_create(NULL, &s, &e) == MC_JAVA_LOGGING_OK);
    CHECK(mc_java_logging_get_minecraft(s, &a, &e) == MC_JAVA_LOGGING_OK);
    CHECK(mc_java_logging_get_minecraft(s, &b, &e) == MC_JAVA_LOGGING_OK && a == b);
    mc_java_logging_state st;
    CHECK(mc_java_logging_inspect(s, &st, &e) == MC_JAVA_LOGGING_OK);
    CHECK(st.phase == MC_JAVA_LOGGING_READY && st.logger_class_phase == MC_JAVA_LOGGING_READY &&
          st.manager_phase == MC_JAVA_LOGGING_READY &&
          st.last_completed == MC_JAVA_LOGGING_STAGE_READY && st.native_allocations == 16 &&
          st.raw_loggers == 3 && st.registry_count == 3 && st.external_logger_holds == 2 &&
          st.manager_published && st.configuration_published && st.root_handler_state == 3 &&
          st.console_configured && !st.console_constructed && st.default_level == 800 &&
          st.configured_console_level == 800 && st.pending_logger == NULL &&
          st.pending_registration == NULL && st.registry[0] == st.root &&
          st.registry[1] == st.global && st.registry[2] == a && a == st.minecraft &&
          st.global != st.root && st.root != st.minecraft && st.global != st.minecraft &&
          st.failure->status == MC_JAVA_LOGGING_OK);
    const mc_java_logger *nodes[3] = {st.root, st.global, st.minecraft};
    const char *names[3] = {"", "global", "Minecraft"};
    for (size_t i = 0; i < 3; ++i) {
        mc_java_logger_state n;
        CHECK(mc_java_logger_inspect(nodes[i], &n, &e) == MC_JAVA_LOGGING_OK);
        CHECK(n.identity == nodes[i] && n.storage == s && n.manager == s &&
              n.phase == MC_JAVA_LOGGING_READY && name_is(n.name, names[i]) && n.name_stored &&
              n.configuration_stored && n.configuration == n.pending_configuration &&
              n.delegate == n.configuration && n.handlers == n.pending_handlers &&
              n.handlers != NULL && n.handler_count == 0 && n.use_parent_handlers &&
              n.effective_level == 800 && !n.filter_present && n.bundle_absent && n.registered &&
              n.configuration_constructor_last_store_pc == 32);
        CHECK(n.parent == (i == 0 ? NULL : st.root) && n.system_logger == (i != 2));
        CHECK(n.declared_level_present == (i == 0) && n.declared_level == (i == 0 ? 800 : 0));
        CHECK(n.constructor_last_store_pc == (i == 1 ? 29 : 36));
    }
    *complete_bytes = st.native_bytes;
    mc_java_logging *separate = NULL;
    mc_java_logger *separate_logger = NULL;
    CHECK(mc_java_logging_create(NULL, &separate, &e) == MC_JAVA_LOGGING_OK);
    CHECK(mc_java_logging_get_minecraft(separate, &separate_logger, &e) == MC_JAVA_LOGGING_OK &&
          separate_logger != a);
    CHECK(mc_java_logger_release(&separate_logger, &e) == MC_JAVA_LOGGING_OK);
    CHECK(mc_java_logging_retire(&separate, &e) == MC_JAVA_LOGGING_OK);
    mc_java_logger *extra = a;
    CHECK(mc_java_logger_retain(extra, &e) == MC_JAVA_LOGGING_OK);
    CHECK(mc_java_logging_retire(&s, &e) == MC_JAVA_LOGGING_BUSY && s != NULL);
    CHECK(mc_java_logger_release(&a, &e) == MC_JAVA_LOGGING_OK && a == NULL);
    CHECK(mc_java_logger_release(&extra, &e) == MC_JAVA_LOGGING_OK && extra == NULL);
    mc_java_logger_state n;
    CHECK(mc_java_logger_inspect(b, &n, &e) == MC_JAVA_LOGGING_OK && n.external_holds == 1);
    CHECK(mc_java_logging_release(&s, &e) == MC_JAVA_LOGGING_BUSY && s != NULL);
    CHECK(mc_java_logger_release(&b, &e) == MC_JAVA_LOGGING_OK && b == NULL);
    CHECK(mc_java_logging_get_minecraft(s, &a, &e) == MC_JAVA_LOGGING_OK && a == st.minecraft);
    CHECK(mc_java_logging_inspect(s, &st, &e) == MC_JAVA_LOGGING_OK &&
          st.native_bytes == *complete_bytes && st.native_allocations == 16);
    CHECK(mc_java_logger_release(&a, &e) == MC_JAVA_LOGGING_OK && a == NULL);
    CHECK(mc_java_logging_retire(&s, &e) == MC_JAVA_LOGGING_OK && s == NULL);
    return 0;
}

static int prefixes(size_t bytes) {
    bool completed[10] = {false};

    static const struct {
        const char *cl, *method;
        int32_t pc;
    } sites[16] = {{"java/util/logging/Logger", "<clinit>", 51},
                   {"java/util/logging/Logger", "<init>", 13},
                   {"java/util/logging/Logger", "<init>", 22},
                   {"java/util/logging/Logger$ConfigurationData", "<init>", 5},
                   {"java/util/logging/LogManager$2", "run", 62},
                   {"java/util/logging/Logger", "<init>", 24},
                   {"java/util/logging/Logger$ConfigurationData", "<init>", 5},
                   {"java/util/logging/Logger", "<init>", 36},
                   {"java/util/logging/LogManager$2", "run", 87},
                   {"java/util/logging/LogManager$LoggerContext", "addLocalLogger", 75},
                   {"java/util/logging/LogManager$LoggerContext", "addLocalLogger", 75},
                   {"java/util/logging/LogManager", "demandLogger", 12},
                   {"java/util/logging/Logger", "<init>", 24},
                   {"java/util/logging/Logger$ConfigurationData", "<init>", 5},
                   {"java/util/logging/Logger", "<init>", 36},
                   {"java/util/logging/LogManager$LoggerContext", "addLocalLogger", 75}};

    for (size_t quota = 0; quota <= 16; ++quota) {
        mc_java_logging_options o = mc_java_logging_default_options();
        o.max_allocations = quota;
        mc_java_logging *s = NULL;
        mc_java_logger *n = NULL;
        mc_java_logging_error e;
        CHECK(mc_java_logging_create(&o, &s, &e) == MC_JAVA_LOGGING_OK);
        const mc_java_logging_status result = mc_java_logging_get_minecraft(s, &n, &e);
        mc_java_logging_state st;
        CHECK(mc_java_logging_inspect(s, &st, NULL) == MC_JAVA_LOGGING_OK);
        completed[st.last_completed] = true;
        CHECK(st.native_allocations == quota && st.registry_count == (quota < 10   ? 0
                                                                      : quota < 11 ? 1
                                                                      : quota < 16 ? 2
                                                                                   : 3));
        CHECK((st.global != NULL) == (quota >= 4) && (st.root != NULL) == (quota >= 8) &&
              (st.configuration != NULL) == (quota >= 9) &&
              (st.minecraft != NULL) == (quota == 16));
        if (quota == 16) {
            CHECK(result == MC_JAVA_LOGGING_OK && n == st.minecraft);
            CHECK(mc_java_logger_release(&n, &e) == MC_JAVA_LOGGING_OK && n == NULL);
        } else {
            CHECK(result == MC_JAVA_LOGGING_LIMIT && n == NULL &&
                  st.phase == MC_JAVA_LOGGING_FAILED && st.failure->status == result &&
                  st.failure->game_caller.pc == 2 &&
                  strcmp(st.failure->game_caller.class_name, "fm") == 0 &&
                  st.failure->native_allocations == quota);
            CHECK(st.failure->site.pc == sites[quota].pc &&
                  strcmp(st.failure->site.class_name, sites[quota].cl) == 0 &&
                  strcmp(st.failure->site.method, sites[quota].method) == 0);
            const mc_java_logging_error first = *st.failure;
            CHECK(mc_java_logging_get_minecraft(s, &n, &e) == MC_JAVA_LOGGING_LATCHED_FAILURE &&
                  e.cause_status == MC_JAVA_LOGGING_LIMIT && e.site.pc == first.site.pc &&
                  e.native_allocations == quota && n == NULL);
            if (st.pending_logger != NULL) {
                mc_java_logger_state pending;
                CHECK(mc_java_logger_inspect(st.pending_logger, &pending, &e) ==
                      MC_JAVA_LOGGING_OK);
                CHECK(!pending.registered && pending.external_holds == 0);
                if (pending.phase == MC_JAVA_LOGGING_FAILED)
                    CHECK(mc_java_logger_retain((mc_java_logger *)st.pending_logger, &e) ==
                          MC_JAVA_LOGGING_INVALID_PHASE);
            }
        }
        CHECK(mc_java_logging_retire(&s, &e) == MC_JAVA_LOGGING_OK && s == NULL);
    }
    CHECK(completed[MC_JAVA_LOGGING_STAGE_COLD] && completed[MC_JAVA_LOGGING_STAGE_MANAGER] &&
          completed[MC_JAVA_LOGGING_STAGE_ROOT] && completed[MC_JAVA_LOGGING_STAGE_CONFIGURATION] &&
          completed[MC_JAVA_LOGGING_STAGE_REGISTER_ROOT] &&
          completed[MC_JAVA_LOGGING_STAGE_REGISTER_GLOBAL] &&
          completed[MC_JAVA_LOGGING_STAGE_MINECRAFT] && completed[MC_JAVA_LOGGING_STAGE_READY]);
    /* Global publication is immediately followed by manager publication with
     * no allocation/callback between them; do not invent a cut for that stage. */
    for (size_t nodes = 0; nodes <= 3; ++nodes) {
        mc_java_logging_options o = mc_java_logging_default_options();
        o.max_loggers = nodes;
        mc_java_logging *s = NULL;
        mc_java_logger *n = NULL;
        mc_java_logging_error e;
        CHECK(mc_java_logging_create(&o, &s, &e) == MC_JAVA_LOGGING_OK);
        CHECK(mc_java_logging_get_minecraft(s, &n, &e) ==
              (nodes == 3 ? MC_JAVA_LOGGING_OK : MC_JAVA_LOGGING_LIMIT));
        mc_java_logging_state st;
        CHECK(mc_java_logging_inspect(s, &st, NULL) == MC_JAVA_LOGGING_OK &&
              st.raw_loggers == nodes);
        CHECK(mc_java_logger_release(&n, &e) == MC_JAVA_LOGGING_OK);
        CHECK(mc_java_logging_retire(&s, &e) == MC_JAVA_LOGGING_OK);
    }
    for (size_t limit = 2; limit < 4; ++limit) {
        mc_java_logging_options o = mc_java_logging_default_options();
        o.max_native_bytes = bytes - (limit == 2 ? 1u : 0u);
        mc_java_logging *s = NULL;
        mc_java_logger *n = NULL;
        mc_java_logging_error e;
        CHECK(mc_java_logging_create(&o, &s, &e) == MC_JAVA_LOGGING_OK);
        CHECK(mc_java_logging_get_minecraft(s, &n, &e) ==
              (limit == 2 ? MC_JAVA_LOGGING_LIMIT : MC_JAVA_LOGGING_OK));
        CHECK(mc_java_logger_release(&n, &e) == MC_JAVA_LOGGING_OK);
        CHECK(mc_java_logging_retire(&s, &e) == MC_JAVA_LOGGING_OK);
    }
    return 0;
}

static int unsupported(void) {
    mc_java_logging_options o = mc_java_logging_default_options();
    o.profile = MC_JAVA_LOGGING_CUSTOM_CONFIGURATION_UNIMPLEMENTED;
    mc_java_logging *s = NULL;
    mc_java_logger *n = NULL;
    mc_java_logging_error e;
    CHECK(mc_java_logging_create(&o, &s, &e) == MC_JAVA_LOGGING_OK);
    CHECK(mc_java_logging_get_minecraft(s, &n, &e) == MC_JAVA_LOGGING_UNSUPPORTED_CONFIGURATION &&
          e.site.pc == 87 && n == NULL);
    mc_java_logging_state st;
    CHECK(mc_java_logging_inspect(s, &st, &e) == MC_JAVA_LOGGING_OK);
    CHECK(st.phase == MC_JAVA_LOGGING_FAILED && st.global != NULL && st.root != NULL &&
          st.minecraft == NULL && st.configuration == NULL && st.registry_count == 0 &&
          st.native_allocations == 8 && st.root_handler_state == 0 &&
          st.last_completed == MC_JAVA_LOGGING_STAGE_ROOT && !st.console_configured);
    CHECK(mc_java_logging_retire(&s, &e) == MC_JAVA_LOGGING_OK);
    return 0;
}

typedef struct gate {
    mtx_t mutex;
    cnd_t condition;
    bool open;
} gate;

typedef struct work {
    gate *start;
    mc_java_logging *service;
    mc_java_logger *result;
    mc_java_logging_status status;
} work;

static void thread_ok(int value) {
    if (value != thrd_success) {
        fputs("logging thread synchronization failure\n", stderr);
        abort(); /* Never free a still-live borrower or count this as rejection. */
    }
}

static int worker(void *p) {
    work *w = p;
    thread_ok(mtx_lock(&w->start->mutex));
    while (!w->start->open)
        thread_ok(cnd_wait(&w->start->condition, &w->start->mutex));
    thread_ok(mtx_unlock(&w->start->mutex));
    w->status = mc_java_logging_get_minecraft(w->service, &w->result, NULL);
    const mc_java_logging_status released = mc_java_logging_release(&w->service, NULL);
    if (released != MC_JAVA_LOGGING_OK)
        return 1;
    return w->status == MC_JAVA_LOGGING_OK ? 0 : 1;
}

static int concurrent(void) {
    enum { COUNT = 6 };

    mc_java_logging *s = NULL;
    CHECK(mc_java_logging_create(NULL, &s, NULL) == MC_JAVA_LOGGING_OK);
    gate start = {0};
    CHECK(mtx_init(&start.mutex, mtx_plain) == thrd_success);
    CHECK(cnd_init(&start.condition) == thrd_success);
    work jobs[COUNT] = {0};
    thrd_t threads[COUNT];
    size_t made = 0;
    bool creation_ok = true;
    for (size_t i = 0; i < COUNT; ++i) {
        if (mc_java_logging_retain(s, NULL) != MC_JAVA_LOGGING_OK) {
            creation_ok = false;
            break;
        }
        jobs[i].start = &start;
        jobs[i].service = s;
        if (thrd_create(&threads[i], worker, &jobs[i]) != thrd_success) {
            (void)mc_java_logging_release(&jobs[i].service, NULL);
            creation_ok = false;
            break;
        }
        ++made;
    }
    thread_ok(mtx_lock(&start.mutex));
    start.open = true;
    thread_ok(cnd_broadcast(&start.condition));
    thread_ok(mtx_unlock(&start.mutex));
    bool joined_ok = true;
    for (size_t i = 0; i < made; ++i) {
        int result = 1;
        thread_ok(thrd_join(threads[i], &result));
        if (result != 0)
            joined_ok = false;
    }
    cnd_destroy(&start.condition);
    mtx_destroy(&start.mutex);
    if (!creation_ok || made != COUNT || !joined_ok) {
        for (size_t i = 0; i < COUNT; ++i) {
            (void)mc_java_logger_release(&jobs[i].result, NULL);
            (void)mc_java_logging_release(&jobs[i].service, NULL);
        }
        (void)mc_java_logging_retire(&s, NULL);
        CHECK(creation_ok && made == COUNT && joined_ok);
    }
    mc_java_logging_state st;
    CHECK(mc_java_logging_inspect(s, &st, NULL) == MC_JAVA_LOGGING_OK &&
          st.native_allocations == 16 && st.registry_count == 3 && st.service_holds == 1 &&
          st.external_logger_holds == COUNT && st.active_calls == 0);
    for (size_t i = 0; i < COUNT; ++i) {
        CHECK(jobs[i].status == MC_JAVA_LOGGING_OK && jobs[i].result == st.minecraft &&
              jobs[i].service == NULL);
        CHECK(mc_java_logger_release(&jobs[i].result, NULL) == MC_JAVA_LOGGING_OK &&
              jobs[i].result == NULL);
    }
    CHECK(mc_java_logging_retire(&s, NULL) == MC_JAVA_LOGGING_OK && s == NULL);
    return 0;
}

int main(int argc, char **argv) {
    (void)argv;
    if (argc != 1) {
        fputs("Logging input: no arguments expected\n", stderr);
        return 2;
    }
    size_t bytes = 0;
    CHECK(cold() == 0);
    CHECK(ready(&bytes) == 0);
    CHECK(prefixes(bytes) == 0);
    CHECK(unsupported() == 0);
    CHECK(concurrent() == 0);
    printf("Core logging ownership contracts: %u checks\n", checks);
    return 0;
}
