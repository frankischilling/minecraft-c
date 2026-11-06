/* Exact production TU once. Link-map must prove archive print_stream.o absent. */
#include "print_stream_rebind_observer.h"
#include "../../../src/core/java/print_stream.c"

#include <time.h>

static struct {
    mc_java_system_output *system;
    mc_java_print_stream *next;
    unsigned int writes, trigger;
    void (*observe)(void *);
    void *context;
    bool rebound;
} observed;

static mc_java_writer_status observing_write(void *context, const mc_text_stdout *stream,
                                             const uint8_t *bytes, size_t length, size_t *physical,
                                             bool *known, mc_java_writer_error *e) {
    (void)context;
    const mc_java_writer_status status =
        mc_ps_native_stdout_write(NULL, stream, bytes, length, physical, known, e);
    if (status != MC_JAVA_WRITER_OK)
        return status;
    if (++observed.writes == observed.trigger) {
        if (observed.observe != NULL)
            observed.observe(observed.context);
        mc_java_print_error error;
        observed.rebound =
            mc_java_system_output_bind(observed.system, observed.next, &error) == MC_JAVA_PRINT_OK;
        if (!observed.rebound) {
            *e = (mc_java_writer_error){.status = MC_JAVA_WRITER_INVALID_ARGUMENT,
                                        .operation =
                                            "qualification actual System.out rebind failed"};
            return e->status;
        }
    }
    return status;
}

void print_observer_install(mc_java_print_stream *s, mc_java_system_output *system,
                            mc_java_print_stream *next, unsigned int trigger,
                            void (*observe)(void *), void *context) {
    observed.system = system;
    observed.next = next;
    observed.writes = 0;
    observed.trigger = trigger;
    observed.observe = observe;
    observed.context = context;
    observed.rebound = false;
    s->native_write = observing_write;
    s->native_context = NULL;
}

bool print_observer_rebound(void) {
    return observed.rebound;
}

void print_observer_clear(mc_java_print_stream *s) {
    s->native_write = mc_ps_native_stdout_write;
    s->native_context = NULL;
}

struct print_observer_monitor_hold {
    thrd_t thread;
    mtx_t gate;
    cnd_t changed;
    mtx_t *actual;
    bool ready, locked, release;
};

static int hold_real_monitor(void *context) {
    print_observer_monitor_hold *hold = context;
    const bool locked = mtx_lock(hold->actual) == thrd_success;
    if (mtx_lock(&hold->gate) != thrd_success) {
        if (locked)
            mtx_unlock(hold->actual);
        return 1;
    }
    hold->locked = locked;
    hold->ready = true;
    cnd_signal(&hold->changed);
    int wait = thrd_success;
    while (locked && !hold->release && wait == thrd_success)
        wait = cnd_wait(&hold->changed, &hold->gate);
    mtx_unlock(&hold->gate);
    const bool unlocked = !locked || mtx_unlock(hold->actual) == thrd_success;
    return locked && unlocked && wait == thrd_success ? 0 : 1;
}

bool print_observer_release_monitor(print_observer_monitor_hold **slot) {
    if (slot == NULL)
        return false;
    print_observer_monitor_hold *hold = *slot;
    if (hold == NULL)
        return true;
    if (mtx_lock(&hold->gate) != thrd_success)
        return false;
    hold->release = true;
    cnd_signal(&hold->changed);
    mtx_unlock(&hold->gate);
    int result = 1;
    if (thrd_join(hold->thread, &result) != thrd_success)
        return false; /* Keep bookkeeping and the borrowed owner alive. */
    cnd_destroy(&hold->changed);
    mtx_destroy(&hold->gate);
    free(hold);
    *slot = NULL;
    return result == 0;
}

static bool start_monitor_hold(mtx_t *actual, print_observer_monitor_hold **out) {
    if (actual == NULL || out == NULL || *out != NULL)
        return false;
    print_observer_monitor_hold *hold = calloc(1, sizeof(*hold));
    if (hold == NULL)
        return false;
    if (mtx_init(&hold->gate, mtx_plain) != thrd_success) {
        free(hold);
        return false;
    }
    if (cnd_init(&hold->changed) != thrd_success) {
        mtx_destroy(&hold->gate);
        free(hold);
        return false;
    }
    hold->actual = actual;
    if (mtx_lock(&hold->gate) != thrd_success) {
        cnd_destroy(&hold->changed);
        mtx_destroy(&hold->gate);
        free(hold);
        return false;
    }
    if (thrd_create(&hold->thread, hold_real_monitor, hold) != thrd_success) {
        mtx_unlock(&hold->gate);
        cnd_destroy(&hold->changed);
        mtx_destroy(&hold->gate);
        free(hold);
        return false;
    }
    *out = hold;
    struct timespec deadline;
    bool ready = timespec_get(&deadline, TIME_UTC) == TIME_UTC;
    if (ready) {
        deadline.tv_sec += 5;
        int waited = thrd_success;
        while (!hold->ready && waited == thrd_success)
            waited = cnd_timedwait(&hold->changed, &hold->gate, &deadline);
        ready = hold->ready && hold->locked;
    }
    mtx_unlock(&hold->gate);
    if (!ready) {
        (void)print_observer_release_monitor(out);
        return false;
    }
    return true;
}

bool print_observer_hold_stream(mc_java_print_stream *stream, print_observer_monitor_hold **out) {
    return stream != NULL && start_monitor_hold(&stream->monitor, out);
}

bool print_observer_hold_system(mc_java_system_output *system, print_observer_monitor_hold **out) {
    return system != NULL && start_monitor_hold(&system->monitor, out);
}
