#if defined(__linux__)
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#endif

#include "client/application_access.h"
#include "client/startup/application_construction.h"
#include "client/startup/application_owner_internal.h"
#include "client/startup/application_services.h"

#include <stdio.h>
#include <stdlib.h>

static unsigned int checks;
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "Resource service check%u line%d: %s\n", checks, __LINE__,             \
                    #condition);                                                                   \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

static bool actual_borrows(void) {
    client_application *app = calloc(1, sizeof(*app));
    CHECK(app != NULL);
    bool counted = true, second = false;
    CHECK(mc_client_application_resource_thread_borrow(NULL, &counted) && !counted);
    mc_client_application_resource_thread_release(NULL, false);
    CHECK(!mc_client_application_resource_thread_borrow(app, NULL));
    CHECK(mc_client_application_resource_thread_borrow(app, &counted) && !counted);
    CHECK(!mc_client_application_resource_thread_borrows_pending(app));
    app->constructor_phase = MC_CLIENT_APP_RETIRING;
    CHECK(!mc_client_application_resource_thread_borrow(app, &counted) && !counted);
    app->constructor_phase = MC_CLIENT_APP_RAW;
    mc_client_application_owner_error error = {0};
    CHECK(mc_client_application_lifetime_register(app, &error) == MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(mc_client_application_resource_thread_borrow(app, &counted) && counted);
    CHECK(mc_client_application_resource_thread_borrow(app, &second) && second);
    mc_client_application_volatile_state state;
    CHECK(mc_client_application_volatile_inspect(app, &state));
    CHECK(state.native_resource_thread_borrows == 2 && state.native_worker_holds == 0 &&
          state.native_callback_holds == 0 && state.native_effect_borrows == 0 && !state.J &&
          !state.o);
    CHECK(mc_client_application_lifetime_retire(app, &error) == MC_CLIENT_APPLICATION_OWNER_BUSY);
    client_application *slot = app;
    CHECK(mc_client_application_constructor_retire(&slot, NULL) == MC_CLIENT_APP_BUSY &&
          slot == app);
    CHECK(app->constructor_phase == MC_CLIENT_APP_RAW &&
          app->native_lifetime_phase == MC_CLIENT_APPLICATION_LIFETIME_REGISTERED);
    mc_client_application_resource_thread_release(app, second);
    CHECK(mc_client_application_resource_thread_borrows_pending(app));
    mc_client_application_resource_thread_release(app, counted);
    CHECK(!mc_client_application_resource_thread_borrows_pending(app));
    atomic_store_explicit(&app->resource_thread_borrows, SIZE_MAX, memory_order_seq_cst);
    CHECK(!mc_client_application_resource_thread_borrow(app, &counted) && !counted);
    CHECK(atomic_load_explicit(&app->resource_thread_borrows, memory_order_seq_cst) == SIZE_MAX);
    atomic_store_explicit(&app->resource_thread_borrows, 0, memory_order_seq_cst);
    bool screen_one = false, screen_two = false, rejected = true;
    CHECK(mc_client_application_screen_release(NULL, false));
    CHECK(!mc_client_application_screen_release(NULL, true));
    CHECK(mc_client_application_screen_borrow(app, &screen_one) && screen_one);
    CHECK(mc_client_application_screen_borrow(app, &screen_two) && screen_two);
    CHECK(mc_client_application_volatile_inspect(app, &state) && state.native_screen_borrows == 2 &&
          state.native_resource_thread_borrows == 0 && state.native_effect_borrows == 0);
    CHECK(mc_client_application_lifetime_retire(app, &error) == MC_CLIENT_APPLICATION_OWNER_BUSY);
    CHECK(mc_client_application_constructor_retire(&slot, NULL) == MC_CLIENT_APP_BUSY &&
          slot == app);
    CHECK(mc_client_application_screen_release(app, screen_one));
    CHECK(mc_client_application_screen_borrows_pending(app));
    app->constructor_phase = MC_CLIENT_APP_RETIRING;
    CHECK(!mc_client_application_screen_borrow(app, &rejected) && !rejected);
    CHECK(mc_client_application_screen_release(app, screen_two));
    CHECK(!mc_client_application_screen_borrows_pending(app));
    CHECK(!mc_client_application_screen_release(app, true));
    CHECK(mc_client_application_volatile_inspect(app, &state) && state.native_screen_borrows == 0);
    app->constructor_phase = MC_CLIENT_APP_RAW;
    atomic_store_explicit(&app->screen_borrows, SIZE_MAX, memory_order_seq_cst);
    CHECK(!mc_client_application_screen_borrow(app, &rejected) && !rejected);
    CHECK(atomic_load_explicit(&app->screen_borrows, memory_order_seq_cst) == SIZE_MAX);
    atomic_store_explicit(&app->screen_borrows, 0, memory_order_seq_cst);
    app->constructor_phase = MC_CLIENT_APP_RETIRING;
    CHECK(!mc_client_application_resource_thread_borrow(app, &counted) && !counted);
    app->constructor_phase = MC_CLIENT_APP_RAW;
    CHECK(mc_client_application_lifetime_retire(app, &error) == MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(!mc_client_application_resource_thread_borrow(app, &counted) && !counted);
    CHECK(!mc_client_application_screen_borrow(app, &rejected) && !rejected);
    CHECK(!mc_client_application_screen_release(app, true));
    free(app);
    return true;
}

static bool shared_output(void) {
    mc_client_application_classes *classes = NULL;
    mc_client_application_owner_error owner = {0};
    mc_java_print_error error = {0};
    mc_java_system_output *first = NULL, *second = NULL;
    CHECK(mc_client_application_classes_create(NULL, &classes, &owner) ==
          MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(mc_client_application_classes_output_service(classes, &first, &error) ==
              MC_JAVA_PRINT_INVALID_PHASE &&
          first == NULL && !classes->system_output_attempted);
    CHECK(mc_client_application_classes_active_use(classes, &owner) ==
          MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(mc_client_application_classes_output_service(classes, &first, &error) ==
              MC_JAVA_PRINT_INVALID_PHASE &&
          first == NULL && !classes->system_output_attempted && classes->system_output == NULL);
    /* The actual native runtime association, as used by startup, bootstraps
     * once. The accessor under test only retains this SAME real service. */
    classes->system_output_attempted = true;
    CHECK(mc_java_system_output_allocate(&classes->system_output,
                                         &classes->system_output_failure) == MC_JAVA_PRINT_OK);
    CHECK(mc_java_system_output_construct(classes->system_output,
                                          &classes->system_output_failure) == MC_JAVA_PRINT_OK);
    CHECK(mc_client_application_classes_output_service(classes, &first, &error) ==
          MC_JAVA_PRINT_OK);
    CHECK(mc_client_application_classes_output_service(classes, &second, &error) ==
          MC_JAVA_PRINT_OK);
    CHECK(first == classes->system_output && second == first);
    mc_java_system_output_state state;
    CHECK(mc_java_system_output_inspect(first, &state) && state.holds == 3);
    mc_java_print_stream *receiver = NULL;
    CHECK(mc_java_system_output_get(first, &receiver, &error) == MC_JAVA_PRINT_OK &&
          receiver != NULL);
    CHECK(mc_java_print_stream_release(&receiver, &error) == MC_JAVA_PRINT_OK && receiver == NULL);
    classes->native_retiring = true; /* Native admission control, not a source field. */
    mc_java_system_output *unchanged = first;
    CHECK(mc_client_application_classes_output_service(classes, &unchanged, &error) ==
              MC_JAVA_PRINT_INVALID_PHASE &&
          unchanged == first);
    classes->native_retiring = false;
    CHECK(mc_client_application_classes_output_service(classes, &unchanged, NULL) ==
              MC_JAVA_PRINT_INVALID_ARGUMENT &&
          unchanged == first);
    CHECK(mc_java_system_output_release(&first, &error) == MC_JAVA_PRINT_OK && first == NULL);
    CHECK(mc_java_system_output_release(&second, &error) == MC_JAVA_PRINT_OK && second == NULL);
    CHECK(mc_java_system_output_inspect(classes->system_output, &state) && state.holds == 1);
    CHECK(mc_client_application_classes_release(&classes, &owner) ==
              MC_CLIENT_APPLICATION_OWNER_OK &&
          classes == NULL);
    return true;
}

int main(void) {
    if (!actual_borrows() || !shared_output())
        return 1;
    printf("Resource services: %u checks\n", checks);
    return 0;
}
