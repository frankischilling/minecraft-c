#include "capture_alloc.h"

#include <stdbool.h>

/* One native allocator failure, on the main/control thread. It never supplies
 * a fabricated object/result; every other call delegates to the real malloc. */
static bool fail_next;

void mc_test_settings_capture_fail_next(void) {
    fail_next = true;
}

void *mc_test_settings_capture_malloc(size_t bytes) {
    if (fail_next) {
        fail_next = false;
        return NULL;
    }
    return malloc(bytes);
}
