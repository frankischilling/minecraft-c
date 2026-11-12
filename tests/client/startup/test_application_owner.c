#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "client/startup/application_owner_internal.h"

#include "mc/entity/player/player.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t checks;

#define CHECK(expression)                                                                          \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(expression)) {                                                                       \
            fprintf(stderr, "Application owner check failed at%d: %s\n", __LINE__, #expression);   \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)

static bool arrays_zero(const mc_client_application_class_state *state) {
    uint8_t reserve = 0;
    int64_t histories = 0;
    for (size_t i = 0; i < state->b_length; ++i)
        reserve |= state->b[i];
    for (size_t i = 0; i < state->E_length; ++i)
        histories |= state->E[i];
    for (size_t i = 0; i < state->F_length; ++i)
        histories |= state->F[i];
    return reserve == 0 && histories == 0;
}

static void class_prefixes(void) {
    static const unsigned int failed_pc[] = {2, 10, 18};
    static const unsigned int last_pc[] = {0, 4, 12, 32};
    for (size_t allocations = 0; allocations <= 3; ++allocations) {
        const mc_client_application_class_options options = {SIZE_MAX, allocations};
        mc_client_application_classes *classes = NULL;
        mc_client_application_owner_error error = {0};
        CHECK(mc_client_application_classes_create(&options, &classes, &error) ==
              MC_CLIENT_APPLICATION_OWNER_OK);
        mc_client_application_class_state state = {0};
        CHECK(mc_client_application_classes_inspect(classes, &state));
        CHECK(state.phase == MC_CLIENT_APPLICATION_CLASS_COLD);
        CHECK(state.b == NULL && state.E == NULL && state.F == NULL);
        CHECK(state.a == NULL && state.af == NULL && state.G == 0 && state.H == 0);
        const mc_client_application_owner_status status =
            mc_client_application_classes_active_use(classes, &error);
        CHECK(status == (allocations < 3 ? MC_CLIENT_APPLICATION_OWNER_LIMIT
                                         : MC_CLIENT_APPLICATION_OWNER_OK));
        CHECK(mc_client_application_classes_inspect(classes, &state));
        CHECK(state.last_store_pc == last_pc[allocations]);
        CHECK(state.array_allocations == allocations);
        CHECK(state.allocation_attempts == (allocations < 3 ? allocations + 1u : 3u));
        CHECK((state.b != NULL) == (allocations >= 1));
        CHECK((state.E != NULL) == (allocations >= 2));
        CHECK((state.F != NULL) == (allocations >= 3));
        CHECK(arrays_zero(&state));
        CHECK(state.G == 0 && state.H == 0 && state.a == NULL && state.af == NULL);
        if (allocations < 3) {
            CHECK(state.phase == MC_CLIENT_APPLICATION_CLASS_ERRONEOUS);
            CHECK(state.failure.site.pc == failed_pc[allocations]);
            CHECK(strcmp(state.failure.site.class_name, "net/minecraft/client/Minecraft") == 0);
            CHECK(strcmp(state.failure.site.method, "<clinit>") == 0);
            CHECK(mc_client_application_classes_active_use(classes, &error) ==
                  MC_CLIENT_APPLICATION_OWNER_CLASS_ERRONEOUS);
            CHECK(error.site.pc == failed_pc[allocations]);
            mc_client_application_class_state after = {0};
            CHECK(mc_client_application_classes_inspect(classes, &after));
            CHECK(after.allocation_attempts == state.allocation_attempts);
            CHECK(after.b == state.b && after.E == state.E && after.F == state.F);
        } else {
            CHECK(state.phase == MC_CLIENT_APPLICATION_CLASS_READY);
            CHECK(state.b_length == MC_CLIENT_APPLICATION_RESERVE_BYTES);
            CHECK(state.E_length == 512 && state.F_length == 512 && state.E != state.F);
            CHECK(state.array_bytes == 10493952u);
            CHECK(mc_client_application_classes_active_use(classes, &error) ==
                  MC_CLIENT_APPLICATION_OWNER_OK);
        }
        CHECK(mc_client_application_classes_release(&classes, &error) ==
              MC_CLIENT_APPLICATION_OWNER_OK);
        CHECK(classes == NULL);
    }
    for (size_t prefix = 0; prefix < 3; ++prefix) {
        const size_t budget = prefix == 0
                                  ? MC_CLIENT_APPLICATION_RESERVE_BYTES - 1u
                                  : MC_CLIENT_APPLICATION_RESERVE_BYTES + (prefix - 1u) * 4096u;
        const mc_client_application_class_options options = {budget, SIZE_MAX};
        mc_client_application_classes *classes = NULL;
        mc_client_application_owner_error error = {0};
        CHECK(mc_client_application_classes_create(&options, &classes, &error) ==
              MC_CLIENT_APPLICATION_OWNER_OK);
        CHECK(mc_client_application_classes_active_use(classes, &error) ==
              MC_CLIENT_APPLICATION_OWNER_LIMIT);
        CHECK(error.site.pc == failed_pc[prefix]);
        CHECK(mc_client_application_classes_release(&classes, &error) ==
              MC_CLIENT_APPLICATION_OWNER_OK);
        CHECK(classes == NULL);
    }
}

typedef struct class_call {
    mc_client_application_classes *classes;
    mc_client_application_owner_status status;
    mc_client_application_owner_error error;
} class_call;

static int activate_class(void *context) {
    class_call *call = context;
    call->status = mc_client_application_classes_active_use(call->classes, &call->error);
    return 0;
}

static void class_concurrency(void) {
    mc_client_application_classes *classes = NULL;
    mc_client_application_owner_error error = {0};
    CHECK(mc_client_application_classes_create(NULL, &classes, &error) ==
          MC_CLIENT_APPLICATION_OWNER_OK);
    thrd_t threads[4];
    class_call calls[4] = {{0}};
    for (size_t i = 0; i < 4; ++i) {
        calls[i].classes = classes;
        CHECK(thrd_create(&threads[i], activate_class, &calls[i]) == thrd_success);
    }
    for (size_t i = 0; i < 4; ++i) {
        int result = -1;
        CHECK(thrd_join(threads[i], &result) == thrd_success && result == 0);
        CHECK(calls[i].status == MC_CLIENT_APPLICATION_OWNER_OK);
    }
    mc_client_application_class_state state = {0};
    CHECK(mc_client_application_classes_inspect(classes, &state));
    CHECK(state.array_allocations == 3 && state.allocation_attempts == 3);
    CHECK(state.E != state.F && arrays_zero(&state));
    CHECK(mc_client_application_classes_retain(classes) == MC_CLIENT_APPLICATION_OWNER_OK);
    mc_client_application_classes *second = classes;
    CHECK(mc_client_application_classes_release(&classes, &error) ==
          MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(classes == NULL);
    CHECK(mc_client_application_classes_inspect(second, &state) && state.native_holds == 1);
    CHECK(mc_client_application_classes_release(&second, &error) == MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(second == NULL);
}

static bool await_worker(mc_client_timer_hack *worker, size_t sleeps, size_t interrupts,
                         size_t spurious, bool completed, mc_client_timer_hack_state *out) {
    const struct timespec delay = {0, 1000000L};
    for (size_t attempt = 0; attempt < 5000; ++attempt) {
        if (!mc_client_timer_hack_inspect(worker, out))
            return false;
        if (out->sleep_calls >= sleeps && out->interrupted_exceptions >= interrupts &&
            out->native_spurious_wakes >= spurious && out->completed == completed &&
            (completed || out->sleeping))
            return true;
        if (out->failure.status != MC_CLIENT_APPLICATION_OWNER_OK)
            return false;
        (void)thrd_sleep(&delay, NULL);
    }
    return false;
}

static mc_player_string *thread_name(void) {
    static const uint16_t units[] = {'T', 'i', 'm', 'e', 'r', ' ', 'h', 'a', 'c',
                                     'k', ' ', 't', 'h', 'r', 'e', 'a', 'd'};
    mc_player_string *name = NULL;
    CHECK(mc_player_string_create(units, sizeof(units) / sizeof(units[0]), &name) == MC_PLAYER_OK);
    return name;
}

static void lifetime_prefixes(void) {
    client_application app = {0};
    app.running = true;
    app.captured = true;
    mc_client_application_owner_error error = {0};
    mc_client_application_volatile_state fields = {0};
    CHECK(!mc_client_application_volatile_inspect(&app, &fields));
    mc_client_timer_hack *worker = NULL;
    CHECK(mc_client_timer_hack_allocate(&worker, &error) == MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(mc_client_timer_hack_construct(worker, &app, NULL, &error) ==
          MC_CLIENT_APPLICATION_OWNER_INVALID_PHASE);
    mc_client_timer_hack_state state = {0};
    CHECK(mc_client_timer_hack_inspect(worker, &state));
    CHECK(state.phase == MC_CLIENT_TIMER_HACK_RAW && state.a == NULL);
    CHECK(mc_client_application_lifetime_register(&app, &error) == MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(mc_client_application_volatile_inspect(&app, &fields));
    CHECK(!fields.J && !fields.o && fields.native_worker_holds == 0);
    CHECK(app.running && app.captured);
    CHECK(mc_client_application_lifetime_register(&app, &error) ==
          MC_CLIENT_APPLICATION_OWNER_INVALID_PHASE);
    atomic_store_explicit(&app.J, true, memory_order_seq_cst);
    atomic_store_explicit(&app.o, true, memory_order_seq_cst);
    CHECK(mc_client_timer_hack_construct(worker, &app, NULL, &error) ==
          MC_CLIENT_APPLICATION_OWNER_NULL_THREAD_NAME);
    CHECK(error.site.pc == 7 && strcmp(error.site.class_name, "kg") == 0);
    CHECK(mc_client_timer_hack_inspect(worker, &state));
    CHECK(state.phase == MC_CLIENT_TIMER_HACK_FAILED && state.a == &app);
    CHECK(state.last_constructor_pc == 2 && state.name == NULL && !state.daemon && !state.started);
    CHECK(mc_client_application_volatile_inspect(&app, &fields));
    CHECK(fields.J && fields.o && fields.native_worker_holds == 1);
    CHECK(mc_client_application_lifetime_retire(&app, &error) == MC_CLIENT_APPLICATION_OWNER_BUSY);
    CHECK(mc_client_timer_hack_construct(worker, &app, NULL, &error) ==
          MC_CLIENT_APPLICATION_OWNER_INVALID_PHASE);
    CHECK(mc_client_timer_hack_retire(&worker, &error) == MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(worker == NULL);
    CHECK(mc_client_application_lifetime_retire(&app, &error) == MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(mc_client_application_volatile_inspect(&app, &fields));
    CHECK(fields.J && fields.o && fields.native_worker_holds == 0);
    CHECK(mc_client_application_lifetime_register(&app, &error) ==
          MC_CLIENT_APPLICATION_OWNER_INVALID_PHASE);
}

static void worker_zero(void) {
    client_application app = {0};
    mc_client_application_owner_error error = {0};
    CHECK(mc_client_application_lifetime_register(&app, &error) == MC_CLIENT_APPLICATION_OWNER_OK);
    mc_player_string *name = thread_name();
    mc_client_timer_hack *worker = NULL;
    CHECK(mc_client_timer_hack_allocate(&worker, &error) == MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(mc_client_timer_hack_construct(worker, &app, name, &error) ==
          MC_CLIENT_APPLICATION_OWNER_OK);
    mc_client_timer_hack_state state = {0};
    CHECK(await_worker(worker, 0, 0, 0, true, &state));
    CHECK(state.name == name && state.daemon && state.started && state.completed);
    CHECK(state.J_reads == 1 && state.sleep_calls == 0 && state.source_J_false_observed);
    CHECK(!state.native_cancel_observed && !state.interrupted && state.last_run_pc == 23);
    CHECK(state.last_constructor_pc == 16);
    CHECK(mc_client_timer_hack_stop(worker, &error) == MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(mc_client_timer_hack_inspect(worker, &state) && state.joined);
    CHECK(!state.native_cancel_observed && state.source_J_false_observed);
    CHECK(mc_client_application_lifetime_retire(&app, &error) == MC_CLIENT_APPLICATION_OWNER_BUSY);
    CHECK(mc_client_timer_hack_retire(&worker, &error) == MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(worker == NULL);
    CHECK(mc_client_application_lifetime_retire(&app, &error) == MC_CLIENT_APPLICATION_OWNER_OK);
    mc_player_string_release(name);
}

static void worker_interrupts(void) {
    client_application app = {0};
    mc_client_application_owner_error error = {0};
    CHECK(mc_client_application_lifetime_register(&app, &error) == MC_CLIENT_APPLICATION_OWNER_OK);
    atomic_store_explicit(&app.J, true, memory_order_seq_cst);
    atomic_store_explicit(&app.o, true, memory_order_seq_cst);
    mc_player_string *name = thread_name();
    mc_client_timer_hack *worker = NULL;
    CHECK(mc_client_timer_hack_allocate(&worker, &error) == MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(mc_client_timer_hack_construct(worker, &app, name, &error) ==
          MC_CLIENT_APPLICATION_OWNER_OK);
    mc_player_string_release(name); /* The real lower Thread-name role now owns its occurrence. */
    mc_client_timer_hack_state state = {0};
    CHECK(await_worker(worker, 1, 0, 0, false, &state));
    CHECK(state.sleep_millis == UINT32_C(2147483647));
    mc_player_string_view view = {0};
    CHECK(mc_player_string_read(state.name, &view) && view.length == 17);
    CHECK(timer_hack_lock(worker) == 0);
    CHECK(timer_hack_signal(worker) == 0);
    CHECK(timer_hack_unlock(worker) == 0);
    CHECK(await_worker(worker, 1, 0, 1, false, &state));
    CHECK(state.sleep_calls == 1 && state.J_reads == 1 && state.interrupted_exceptions == 0);
    CHECK(mc_client_timer_hack_interrupt(worker, &error) == MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(await_worker(worker, 2, 1, 1, false, &state));
    CHECK(state.J_reads == 2 && state.interrupted_exceptions == 1 && !state.interrupted);
    atomic_store_explicit(&app.J, false, memory_order_seq_cst);
    const size_t before = state.native_spurious_wakes;
    CHECK(timer_hack_lock(worker) == 0);
    CHECK(timer_hack_signal(worker) == 0);
    CHECK(timer_hack_unlock(worker) == 0);
    CHECK(await_worker(worker, 2, 1, before + 1u, false, &state));
    CHECK(state.J_reads == 2 && !state.completed && !state.source_J_false_observed);
    CHECK(mc_client_timer_hack_interrupt(worker, &error) == MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(await_worker(worker, 2, 2, before + 1u, true, &state));
    CHECK(state.J_reads == 3 && state.source_J_false_observed && !state.native_cancel_observed);
    CHECK(mc_client_timer_hack_retire(&worker, &error) == MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(worker == NULL);
    mc_client_application_volatile_state fields = {0};
    CHECK(mc_client_application_volatile_inspect(&app, &fields));
    CHECK(!fields.J && fields.o && fields.native_worker_holds == 0);
    CHECK(mc_client_application_lifetime_retire(&app, &error) == MC_CLIENT_APPLICATION_OWNER_OK);
}

static void native_cancellation(void) {
    client_application app = {0};
    mc_client_application_owner_error error = {0};
    CHECK(mc_client_application_lifetime_register(&app, &error) == MC_CLIENT_APPLICATION_OWNER_OK);
    atomic_store_explicit(&app.J, true, memory_order_seq_cst);
    atomic_store_explicit(&app.o, true, memory_order_seq_cst);
    mc_player_string *name = thread_name();
    mc_client_timer_hack *worker = NULL;
    CHECK(mc_client_timer_hack_allocate(&worker, &error) == MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(mc_client_timer_hack_construct(worker, &app, name, &error) ==
          MC_CLIENT_APPLICATION_OWNER_OK);
    mc_client_timer_hack_state state = {0};
    CHECK(await_worker(worker, 1, 0, 0, false, &state));
    CHECK(mc_client_timer_hack_stop(worker, &error) == MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(mc_client_timer_hack_inspect(worker, &state));
    CHECK(state.phase == MC_CLIENT_TIMER_HACK_RETIRING && state.completed && state.joined);
    CHECK(state.native_cancel_requested && state.native_cancel_observed);
    CHECK(!state.source_J_false_observed && state.interrupted_exceptions == 0);
    CHECK(state.a == &app && state.name == name && state.daemon);
    mc_client_application_volatile_state fields = {0};
    CHECK(mc_client_application_volatile_inspect(&app, &fields));
    CHECK(fields.J && fields.o && fields.native_worker_holds == 1);
    CHECK(mc_client_application_lifetime_retire(&app, &error) == MC_CLIENT_APPLICATION_OWNER_BUSY);
    CHECK(mc_client_timer_hack_retire(&worker, &error) == MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(worker == NULL);
    CHECK(mc_client_application_volatile_inspect(&app, &fields));
    CHECK(fields.J && fields.o && fields.native_worker_holds == 0);
    CHECK(mc_client_application_lifetime_retire(&app, &error) == MC_CLIENT_APPLICATION_OWNER_OK);
    mc_player_string_release(name);
}

static bool byte_array_zero(const uint8_t *data, size_t length) {
    uint8_t combined = 0;
    for (size_t i = 0; i < length; ++i)
        combined |= data[i];
    return combined == 0;
}

static bool long_array_zero(const int64_t *data, size_t length) {
    int64_t combined = 0;
    for (size_t i = 0; i < length; ++i)
        combined |= data[i];
    return combined == 0;
}

static void original_records(void) {
    mc_client_application_classes *classes = NULL;
    mc_client_application_owner_error error = {0};
    CHECK(mc_client_application_classes_create(NULL, &classes, &error) ==
          MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(mc_client_application_classes_active_use(classes, &error) ==
          MC_CLIENT_APPLICATION_OWNER_OK);
    mc_client_application_class_state fields = {0};
    CHECK(mc_client_application_classes_inspect(classes, &fields));
    printf("CLASS arrays %zu %zu %zu %d %d %d %d\n", fields.b_length, fields.E_length,
           fields.F_length, byte_array_zero(fields.b, fields.b_length),
           long_array_zero(fields.E, fields.E_length), long_array_zero(fields.F, fields.F_length),
           fields.E != fields.F);
    printf("CLASS fields %d %lld %d %d\n", fields.G, (long long)fields.H, fields.a == NULL,
           fields.af == NULL);
    client_application app = {0};
    CHECK(mc_client_application_lifetime_register(&app, &error) == MC_CLIENT_APPLICATION_OWNER_OK);
    mc_player_string *name = thread_name();
    mc_client_timer_hack *worker = NULL;
    mc_client_timer_hack_state state = {0};
    CHECK(mc_client_timer_hack_allocate(&worker, &error) == MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(mc_client_timer_hack_construct(worker, &app, name, &error) ==
          MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(await_worker(worker, 0, 0, 0, true, &state));
    printf("THREAD zero %d %d %d %d\n", state.daemon, state.name == name, state.completed,
           atomic_load_explicit(&app.J, memory_order_seq_cst));
    CHECK(mc_client_timer_hack_retire(&worker, &error) == MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(worker == NULL);
    atomic_store_explicit(&app.J, true, memory_order_seq_cst);
    CHECK(mc_client_timer_hack_allocate(&worker, &error) == MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(mc_client_timer_hack_construct(worker, &app, name, &error) ==
          MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(await_worker(worker, 1, 0, 0, false, &state));
    printf("THREAD live %d %d %d %d\n", state.daemon, state.name == name, state.sleeping,
           atomic_load_explicit(&app.J, memory_order_seq_cst));
    atomic_store_explicit(&app.J, false, memory_order_seq_cst);
    CHECK(mc_client_timer_hack_interrupt(worker, &error) == MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(await_worker(worker, 1, 1, 0, true, &state));
    printf("THREAD source-stop %d %d\n", state.completed,
           atomic_load_explicit(&app.J, memory_order_seq_cst));
    CHECK(mc_client_timer_hack_retire(&worker, &error) == MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(worker == NULL);
    CHECK(mc_client_application_lifetime_retire(&app, &error) == MC_CLIENT_APPLICATION_OWNER_OK);
    mc_player_string_release(name);
    CHECK(mc_client_application_classes_release(&classes, &error) ==
          MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(classes == NULL);
}

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "original") == 0) {
        original_records();
        return 0;
    }
    if (argc != 1)
        return 2;
    class_prefixes();
    class_concurrency();
    lifetime_prefixes();
    worker_zero();
    worker_interrupts();
    native_cancellation();
    printf("Application class/Timer hack contracts: %zu checks\n", checks);
    return 0;
}
