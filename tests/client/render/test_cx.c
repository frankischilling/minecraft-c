/* The exact owner TU is included once to arrange the actual private static
 * flag for its false branch and hold its real monitor. No Display provider,
 * capability result, allocator or source method body is replaced. */
#include "../../../src/client/render/cx.c"

#include <stdio.h>

static unsigned int checks;
#define CHECK(value)                                                                               \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(value)) {                                                                            \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #value);                            \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

int main(void) {
    int result = 1;
    bool monitor_owned = false, value = true;
    mc_client_cx_classes *classes = NULL, *other = NULL, *class_slot = NULL;
    mc_client_source_cx *raw = NULL, *second = NULL, *slot = NULL;
    mc_client_cx_error error = {0};
    mc_client_cx_class_state state = {0};
    mc_client_cx_state instance = {0};
    CHECK(mc_client_cx_classes_create(&classes, &error) == MC_CLIENT_CX_OK);
    CHECK(mc_client_cx_classes_inspect(classes, &state, &error) == MC_CLIENT_CX_OK);
    CHECK(state.phase == MC_CLIENT_CX_CLASS_COLD && state.holds == 1 && !state.enabled &&
          state.last_store == 0);
    CHECK(mc_client_cx_classes_create(&other, &error) == MC_CLIENT_CX_OK);
    CHECK(mc_client_cx_allocate(classes, &raw, &error) == MC_CLIENT_CX_OK);
    CHECK(mc_client_cx_classes_inspect(classes, &state, &error) == MC_CLIENT_CX_OK);
    CHECK(state.phase == MC_CLIENT_CX_CLASS_READY && state.enabled && state.last_store == 1 &&
          state.holds == 2);
    CHECK(mc_client_cx_classes_inspect(other, &state, &error) == MC_CLIENT_CX_OK);
    CHECK(state.phase == MC_CLIENT_CX_CLASS_COLD && !state.enabled && state.holds == 1);
    CHECK(mc_client_cx_inspect(raw, &instance, &error) == MC_CLIENT_CX_OK);
    CHECK(instance.phase == MC_CLIENT_CX_RAW && instance.constructor_pc == 0 &&
          instance.holds == 1);
    CHECK(mc_client_cx_supported(raw, NULL, &value, &error) == MC_CLIENT_CX_INVALID_PHASE);
    CHECK(value && !error.source_returned && !error.capability_read);
    CHECK(mc_client_cx_construct(raw, &error) == MC_CLIENT_CX_OK);
    CHECK(error.source_returned && error.source_pc == 4);
    CHECK(mc_client_cx_inspect(raw, &instance, &error) == MC_CLIENT_CX_OK);
    CHECK(instance.phase == MC_CLIENT_CX_READY && instance.constructor_pc == 4);
    CHECK(mc_client_cx_construct(raw, &error) == MC_CLIENT_CX_INVALID_PHASE);

    CHECK(mtx_lock(&classes->mutex) == thrd_success);
    monitor_owned = true;
    classes->enabled = false; /* Actual private field fixture, no public setter. */
    CHECK(mtx_unlock(&classes->mutex) == thrd_success);
    monitor_owned = false;
    CHECK(mc_client_cx_classes_active_use(classes, &error) == MC_CLIENT_CX_OK);
    CHECK(mc_client_cx_supported(raw, NULL, &value, &error) == MC_CLIENT_CX_OK);
    CHECK(!value && error.source_returned && !error.capability_read && error.source_pc == 20);
    CHECK(mc_client_cx_classes_inspect(classes, &state, &error) == MC_CLIENT_CX_OK);
    CHECK(!state.enabled && state.last_store == 1); /* Active use did not replay <clinit>. */

    CHECK(mtx_lock(&classes->mutex) == thrd_success);
    monitor_owned = true;
    classes->enabled = true;
    slot = raw;
    CHECK(mc_client_cx_release(&slot, &error) == MC_CLIENT_CX_BUSY && slot == raw);
    class_slot = classes;
    CHECK(mc_client_cx_classes_release(&class_slot, &error) == MC_CLIENT_CX_BUSY &&
          class_slot == classes);
    CHECK(mc_client_cx_retain(raw, &error) == MC_CLIENT_CX_BUSY);
    CHECK(mtx_unlock(&classes->mutex) == thrd_success);
    monitor_owned = false;
    slot = NULL;
    class_slot = NULL;
    value = true;
    CHECK(mc_client_cx_supported(raw, NULL, &value, &error) == MC_CLIENT_CX_DISPLAY_FAILED);
    CHECK(value && !error.source_returned && !error.capability_read && error.source_pc == 6 &&
          error.display.status == MC_CLIENT_DISPLAY_INVALID_ARGUMENT);
    CHECK(mc_client_cx_retain(raw, &error) == MC_CLIENT_CX_OK);
    slot = raw;
    CHECK(mc_client_cx_release(&slot, &error) == MC_CLIENT_CX_OK && slot == NULL);
    CHECK(mc_client_cx_inspect(raw, &instance, &error) == MC_CLIENT_CX_OK && instance.holds == 1);
    CHECK(mc_client_cx_release(&raw, &error) == MC_CLIENT_CX_OK && raw == NULL);
    CHECK(mc_client_cx_classes_release(&classes, &error) == MC_CLIENT_CX_OK && classes == NULL);

    CHECK(mc_client_cx_allocate(other, &second, &error) == MC_CLIENT_CX_OK);
    CHECK(mc_client_cx_construct(second, &error) == MC_CLIENT_CX_OK);
    CHECK(mc_client_cx_classes_release(&other, &error) == MC_CLIENT_CX_OK && other == NULL);
    value = false;
    CHECK(mc_client_cx_supported(second, NULL, &value, &error) == MC_CLIENT_CX_DISPLAY_FAILED);
    CHECK(!value && error.source_pc == 6); /* Instance retained its namespace. */
    CHECK(mc_client_cx_release(&second, &error) == MC_CLIENT_CX_OK && second == NULL);
    CHECK(mc_client_cx_release(&second, &error) == MC_CLIENT_CX_OK);
    printf("cx CPU: %u checks PASS\n", checks);
    result = 0;
cleanup:
    if (monitor_owned)
        (void)mtx_unlock(&classes->mutex);
    /* Unconsumed BUSY aliases above are not independently owned occurrences. */
    (void)mc_client_cx_release(&second, NULL);
    (void)mc_client_cx_release(&raw, NULL);
    (void)mc_client_cx_classes_release(&other, NULL);
    (void)mc_client_cx_classes_release(&classes, NULL);
    return result;
}
