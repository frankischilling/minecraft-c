#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#if !defined(_WIN32) && !defined(__linux__)
#error "Timer hack native waits are implemented for Windows and Linux only"
#endif

#include "application_owner_internal.h"

#include "mc/entity/player/player.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>

static const char *const kg_constructor = "(Lnet/minecraft/client/Minecraft;Ljava/lang/String;)V";

int timer_hack_lock(mc_client_timer_hack *worker) {
#ifdef _WIN32
    AcquireSRWLockExclusive(&worker->mutex);
    return 0;
#else
    return pthread_mutex_lock(&worker->mutex);
#endif
}

int timer_hack_unlock(mc_client_timer_hack *worker) {
#ifdef _WIN32
    ReleaseSRWLockExclusive(&worker->mutex);
    return 0;
#else
    return pthread_mutex_unlock(&worker->mutex);
#endif
}

int timer_hack_signal(mc_client_timer_hack *worker) {
#ifdef _WIN32
    WakeConditionVariable(&worker->changed);
    return 0;
#else
    return pthread_cond_signal(&worker->changed);
#endif
}

static int native_wait_initialize(mc_client_timer_hack *worker) {
#ifdef _WIN32
    InitializeSRWLock(&worker->mutex);
    InitializeConditionVariable(&worker->changed);
    return 0;
#else
    int status = pthread_mutex_init(&worker->mutex, NULL);
    if (status != 0)
        return status;
    pthread_condattr_t attributes;
    status = pthread_condattr_init(&attributes);
    if (status != 0) {
        (void)pthread_mutex_destroy(&worker->mutex);
        return status;
    }
    status = pthread_condattr_setclock(&attributes, CLOCK_MONOTONIC);
    if (status == 0)
        status = pthread_cond_init(&worker->changed, &attributes);
    const int retired = pthread_condattr_destroy(&attributes);
    if (status == 0 && retired != 0) {
        (void)pthread_cond_destroy(&worker->changed);
        status = retired;
    }
    if (status != 0)
        (void)pthread_mutex_destroy(&worker->mutex);
    return status;
#endif
}

typedef struct sleep_deadline {
#ifdef _WIN32
    ULONGLONG start;
#else
    struct timespec end;
#endif
} sleep_deadline;

static int deadline_initialize(sleep_deadline *deadline) {
#ifdef _WIN32
    deadline->start = GetTickCount64();
    return 0;
#else
    _Static_assert(sizeof(time_t) >= sizeof(int64_t), "Timer hack requires native64 time_t");
    _Static_assert((time_t)-1 < 0, "Timer hack requires signed time_t");
    if (clock_gettime(CLOCK_MONOTONIC, &deadline->end) != 0)
        return errno;
    if (deadline->end.tv_sec < 0 || deadline->end.tv_sec > (time_t)(INT64_MAX - INT64_C(2147484)) ||
        deadline->end.tv_nsec < 0 || deadline->end.tv_nsec >= 1000000000L)
        return ERANGE;
    deadline->end.tv_sec += (time_t)(MC_CLIENT_TIMER_HACK_SLEEP_MILLIS / 1000u);
    deadline->end.tv_nsec += (long)(MC_CLIENT_TIMER_HACK_SLEEP_MILLIS % 1000u) * 1000000L;
    if (deadline->end.tv_nsec >= 1000000000L) {
        deadline->end.tv_nsec -= 1000000000L;
        ++deadline->end.tv_sec;
    }
    return 0;
#endif
}

/* Caller holds the native mutex. Return zero for a wake, ETIMEDOUT only after
 * the original relative sleep interval, or the concrete native failure. UTC
 * and the application's wall/nano clock providers are not consulted. */
static int native_wait(mc_client_timer_hack *worker, const sleep_deadline *deadline) {
#ifdef _WIN32
    const ULONGLONG elapsed = GetTickCount64() - deadline->start;
    if (elapsed >= MC_CLIENT_TIMER_HACK_SLEEP_MILLIS)
        return ETIMEDOUT;
    const DWORD remaining = (DWORD)((ULONGLONG)MC_CLIENT_TIMER_HACK_SLEEP_MILLIS - elapsed);
    if (SleepConditionVariableSRW(&worker->changed, &worker->mutex, remaining, 0) != 0)
        return 0;
    const DWORD failure = GetLastError();
    if (failure == ERROR_TIMEOUT) {
        if (GetTickCount64() - deadline->start >= MC_CLIENT_TIMER_HACK_SLEEP_MILLIS)
            return ETIMEDOUT;
        return 0;
    }
    return failure > (DWORD)INT_MAX ? EIO : (int)failure;
#else
    return pthread_cond_timedwait(&worker->changed, &worker->mutex, &deadline->end);
#endif
}

static bool increment(size_t *counter, mc_client_timer_hack *worker) {
    if (*counter == SIZE_MAX) {
        application_owner_fail(&worker->failure, MC_CLIENT_APPLICATION_OWNER_LIMIT,
                               "Timer hack native observation count", "kg", "run", "()V",
                               worker->last_run_pc, 0);
        return false;
    }
    ++*counter;
    return true;
}

static int run_worker(void *context) {
    mc_client_timer_hack *worker = context;
    if (timer_hack_lock(worker) != 0)
        return 1;
    for (;;) {
        if (worker->native_cancel_requested) {
            worker->native_cancel_observed = true;
            break;
        }
        worker->last_run_pc = 4;
        if (!increment(&worker->J_reads, worker))
            break;
        if (!atomic_load_explicit(&worker->a->J, memory_order_seq_cst)) {
            worker->source_J_false_observed = true;
            worker->last_run_pc = 23;
            break;
        }
        worker->last_run_pc = 13;
        if (!increment(&worker->sleep_calls, worker))
            break;
        sleep_deadline deadline;
        int native_error = deadline_initialize(&deadline);
        if (native_error != 0) {
            application_owner_fail(&worker->failure, MC_CLIENT_APPLICATION_OWNER_CLOCK_FAILED,
                                   "Timer hack native monotonic deadline", "kg", "run", "()V", 13,
                                   native_error);
            break;
        }
        worker->sleeping = true;
        for (;;) {
            if (worker->native_cancel_requested) {
                worker->native_cancel_observed = true;
                break;
            }
            if (worker->interrupted) {
                worker->interrupted = false;
                worker->last_run_pc = 19;
                (void)increment(&worker->interrupted_exceptions, worker);
                break;
            }
            native_error = native_wait(worker, &deadline);
            if (native_error == ETIMEDOUT)
                break;
            if (native_error != 0) {
                application_owner_fail(&worker->failure, MC_CLIENT_APPLICATION_OWNER_THREAD_FAILED,
                                       "Timer hack native sleep", "kg", "run", "()V", 13,
                                       native_error);
                break;
            }
            if (!worker->interrupted && !worker->native_cancel_requested &&
                !increment(&worker->native_spurious_wakes, worker))
                break;
        }
        worker->sleeping = false;
        if (worker->native_cancel_observed ||
            worker->failure.status != MC_CLIENT_APPLICATION_OWNER_OK)
            break;
    }
    worker->completed = true;
    (void)timer_hack_unlock(worker);
    return 0;
}

mc_client_application_owner_status
mc_client_timer_hack_allocate(mc_client_timer_hack **out,
                              mc_client_application_owner_error *error) {
    if (error != NULL)
        *error = (mc_client_application_owner_error){0};
    if (out == NULL)
        return application_owner_fail(error, MC_CLIENT_APPLICATION_OWNER_INVALID_ARGUMENT,
                                      "Timer hack native allocation output", NULL, NULL, NULL, 0,
                                      0);
    mc_client_timer_hack *worker = calloc(1, sizeof(*worker));
    if (worker == NULL)
        return application_owner_fail(error, MC_CLIENT_APPLICATION_OWNER_ALLOCATION_FAILED,
                                      "Timer hack native allocation", NULL, NULL, NULL, 0, 0);
    const int created = native_wait_initialize(worker);
    if (created != 0) {
        free(worker);
        return application_owner_fail(error, MC_CLIENT_APPLICATION_OWNER_THREAD_FAILED,
                                      "Timer hack native wait setup", NULL, NULL, NULL, 0, created);
    }
    *out = worker;
    return MC_CLIENT_APPLICATION_OWNER_OK;
}

mc_client_application_owner_status
mc_client_timer_hack_construct(mc_client_timer_hack *worker, client_application *app,
                               mc_player_string *name, mc_client_application_owner_error *error) {
    if (error != NULL)
        *error = (mc_client_application_owner_error){0};
    if (worker == NULL || app == NULL)
        return application_owner_fail(error, MC_CLIENT_APPLICATION_OWNER_INVALID_ARGUMENT,
                                      "Timer hack normal constructor native arguments", NULL, NULL,
                                      NULL, 0, 0);
    int native_error = timer_hack_lock(worker);
    if (native_error != 0)
        return application_owner_fail(error, MC_CLIENT_APPLICATION_OWNER_THREAD_FAILED,
                                      "Timer hack native constructor lock", NULL, NULL, NULL, 0,
                                      native_error);
    if (worker->phase != MC_CLIENT_TIMER_HACK_RAW) {
        (void)timer_hack_unlock(worker);
        return application_owner_fail(error, MC_CLIENT_APPLICATION_OWNER_INVALID_PHASE,
                                      "Timer hack native one-shot constructor", NULL, NULL, NULL, 0,
                                      0);
    }
    mc_client_application_owner_error failure = {0};
    if (application_worker_hold(app, &failure) != MC_CLIENT_APPLICATION_OWNER_OK) {
        (void)timer_hack_unlock(worker);
        if (error != NULL)
            *error = failure;
        return failure.status;
    }
    worker->phase = MC_CLIENT_TIMER_HACK_CONSTRUCTING;
    worker->a = app;
    worker->last_constructor_pc = 2;
    if (name == NULL) {
        application_owner_fail(&failure, MC_CLIENT_APPLICATION_OWNER_NULL_THREAD_NAME,
                               "Thread(String) null name boundary", "kg", "<init>", kg_constructor,
                               7, 0);
        goto failed;
    }
    const mc_player_status retained = mc_player_string_retain(name);
    if (retained != MC_PLAYER_OK) {
        application_owner_fail(&failure, MC_CLIENT_APPLICATION_OWNER_STRING_FAILED,
                               "Thread(String) native String hold", "kg", "<init>", kg_constructor,
                               7, (int)retained);
        goto failed;
    }
    worker->name = name;
    worker->last_constructor_pc = 7;
    worker->daemon = true;
    worker->last_constructor_pc = 12;
    native_error = thrd_create(&worker->thread, run_worker, worker);
    if (native_error != thrd_success) {
        application_owner_fail(&failure, MC_CLIENT_APPLICATION_OWNER_THREAD_FAILED,
                               "Timer hack native Thread.start", "kg", "<init>", kg_constructor, 16,
                               native_error);
        goto failed;
    }
    worker->started = true;
    worker->last_constructor_pc = 16;
    worker->phase = MC_CLIENT_TIMER_HACK_READY;
    (void)timer_hack_unlock(worker);
    return MC_CLIENT_APPLICATION_OWNER_OK;
failed:
    worker->phase = MC_CLIENT_TIMER_HACK_FAILED;
    worker->failure = failure;
    (void)timer_hack_unlock(worker);
    if (error != NULL)
        *error = failure;
    return failure.status;
}

mc_client_application_owner_status
mc_client_timer_hack_interrupt(mc_client_timer_hack *worker,
                               mc_client_application_owner_error *error) {
    if (error != NULL)
        *error = (mc_client_application_owner_error){0};
    if (worker == NULL)
        return application_owner_fail(error, MC_CLIENT_APPLICATION_OWNER_INVALID_ARGUMENT,
                                      "Timer hack native interrupt owner", NULL, NULL, NULL, 0, 0);
    const int locked = timer_hack_lock(worker);
    if (locked != 0)
        return application_owner_fail(error, MC_CLIENT_APPLICATION_OWNER_THREAD_FAILED,
                                      "Timer hack native interrupt lock", NULL, NULL, NULL, 0,
                                      locked);
    if (worker->phase != MC_CLIENT_TIMER_HACK_READY || !worker->started || worker->completed ||
        worker->retiring) {
        (void)timer_hack_unlock(worker);
        return application_owner_fail(error, MC_CLIENT_APPLICATION_OWNER_INVALID_PHASE,
                                      "Timer hack interrupt requires live started owner", NULL,
                                      NULL, NULL, 0, 0);
    }
    worker->interrupted = true;
    const int signaled = timer_hack_signal(worker);
    (void)timer_hack_unlock(worker);
    if (signaled != 0)
        return application_owner_fail(error, MC_CLIENT_APPLICATION_OWNER_THREAD_FAILED,
                                      "Timer hack native interrupt signal", NULL, NULL, NULL, 0,
                                      signaled);
    return MC_CLIENT_APPLICATION_OWNER_OK;
}

bool mc_client_timer_hack_inspect(mc_client_timer_hack *worker, mc_client_timer_hack_state *out) {
    if (worker == NULL || out == NULL || timer_hack_lock(worker) != 0)
        return false;
    *out = (mc_client_timer_hack_state){.phase = worker->phase,
                                        .a = worker->a,
                                        .name = worker->name,
                                        .daemon = worker->daemon,
                                        .started = worker->started,
                                        .sleeping = worker->sleeping,
                                        .completed = worker->completed,
                                        .joined = worker->joined,
                                        .interrupted = worker->interrupted,
                                        .native_cancel_requested = worker->native_cancel_requested,
                                        .native_cancel_observed = worker->native_cancel_observed,
                                        .source_J_false_observed = worker->source_J_false_observed,
                                        .J_reads = worker->J_reads,
                                        .sleep_calls = worker->sleep_calls,
                                        .interrupted_exceptions = worker->interrupted_exceptions,
                                        .native_spurious_wakes = worker->native_spurious_wakes,
                                        .last_constructor_pc = worker->last_constructor_pc,
                                        .last_run_pc = worker->last_run_pc,
                                        .sleep_millis = MC_CLIENT_TIMER_HACK_SLEEP_MILLIS,
                                        .failure = worker->failure};
    (void)timer_hack_unlock(worker);
    return true;
}

mc_client_application_owner_status
mc_client_timer_hack_stop(mc_client_timer_hack *worker, mc_client_application_owner_error *error) {
    if (error != NULL)
        *error = (mc_client_application_owner_error){0};
    if (worker == NULL)
        return application_owner_fail(error, MC_CLIENT_APPLICATION_OWNER_INVALID_ARGUMENT,
                                      "Timer hack native stop owner", NULL, NULL, NULL, 0, 0);
    int native_error = timer_hack_lock(worker);
    if (native_error != 0)
        return application_owner_fail(error, MC_CLIENT_APPLICATION_OWNER_THREAD_FAILED,
                                      "Timer hack native retirement lock", NULL, NULL, NULL, 0,
                                      native_error);
    if (worker->retiring) {
        (void)timer_hack_unlock(worker);
        return application_owner_fail(error, MC_CLIENT_APPLICATION_OWNER_BUSY,
                                      "Timer hack native retirement entered", NULL, NULL, NULL, 0,
                                      0);
    }
    worker->retiring = true;
    worker->phase = MC_CLIENT_TIMER_HACK_RETIRING;
    worker->native_cancel_requested = true;
    native_error = worker->condition_destroyed ? 0 : timer_hack_signal(worker);
    if (native_error != 0)
        goto retained;
    if (worker->started && !worker->joined) {
        (void)timer_hack_unlock(worker);
        int result = 0;
        const int joined = thrd_join(worker->thread, &result);
        if (joined == thrd_success)
            worker->joined = true;
        native_error = timer_hack_lock(worker);
        if (native_error != 0) {
            worker->retiring = false;
            return application_owner_fail(error, MC_CLIENT_APPLICATION_OWNER_THREAD_FAILED,
                                          "Timer hack native post-join lock", NULL, NULL, NULL, 0,
                                          native_error);
        }
        if (joined != thrd_success) {
            native_error = joined;
            goto retained;
        }
        if (result != 0) {
            native_error = result;
            goto retained;
        }
    }
    worker->retiring = false;
    (void)timer_hack_unlock(worker);
    return MC_CLIENT_APPLICATION_OWNER_OK;
retained:
    worker->retiring = false;
    (void)timer_hack_unlock(worker);
    return application_owner_fail(error, MC_CLIENT_APPLICATION_OWNER_THREAD_FAILED,
                                  "Timer hack native stop retained owner", NULL, NULL, NULL, 0,
                                  native_error);
}

mc_client_application_owner_status
mc_client_timer_hack_retire(mc_client_timer_hack **owner,
                            mc_client_application_owner_error *error) {
    if (error != NULL)
        *error = (mc_client_application_owner_error){0};
    if (owner == NULL)
        return application_owner_fail(error, MC_CLIENT_APPLICATION_OWNER_INVALID_ARGUMENT,
                                      "Timer hack native retirement output", NULL, NULL, NULL, 0,
                                      0);
    mc_client_timer_hack *worker = *owner;
    if (worker == NULL)
        return MC_CLIENT_APPLICATION_OWNER_OK;
    const mc_client_application_owner_status stopped = mc_client_timer_hack_stop(worker, error);
    if (stopped != MC_CLIENT_APPLICATION_OWNER_OK)
        return stopped;
    worker->retiring = true;
#ifndef _WIN32
    int native_error = 0;
    if (!worker->condition_destroyed) {
        native_error = pthread_cond_destroy(&worker->changed);
        if (native_error != 0)
            goto unlocked_retained;
        worker->condition_destroyed = true;
    }
    native_error = pthread_mutex_destroy(&worker->mutex);
    if (native_error != 0)
        goto unlocked_retained;
#endif
    mc_player_string_release(worker->name);
    if (worker->a != NULL)
        application_worker_release(worker->a);
    free(worker);
    *owner = NULL;
    return MC_CLIENT_APPLICATION_OWNER_OK;
#ifndef _WIN32
unlocked_retained:
    worker->retiring = false;
    return application_owner_fail(error, MC_CLIENT_APPLICATION_OWNER_THREAD_FAILED,
                                  "Timer hack native wait retirement retained owner", NULL, NULL,
                                  NULL, 0, native_error);
#endif
}
