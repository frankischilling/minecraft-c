#include "mc/server/runtime/construction.h"

#include <stdio.h>

static unsigned int checks;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

/* Native construction/lifetime contracts, including each real allocation
 * boundary. Original constructor graph comparison is a separate reference
 * check; these checks do not establish startup, networking or tick parity. */
static int check_server(mc_server_runtime *server, bool ready) {
    int result = 1;
    mc_server_runtime_fields f;
    mc_server_construction state;
    const mc_server_reference_list *backing = NULL;
    const void *const *items;
    mc_server_tracker *const *trackers;
    size_t count;
    CHECK(mc_server_runtime_inspect(server, &f, &state) == MC_SERVER_OK);
    CHECK(f.c == NULL && f.d == NULL && f.e == NULL && f.f == NULL && f.p == NULL && f.i == NULL);
    CHECK(!f.g && f.h == 0 && f.j == 0 && !f.l && !f.m && !f.n && !f.o);
    CHECK(f.q == (state.phase != MC_SERVER_RAW));
    CHECK((f.r != NULL) == (state.last_store_pc >= 27));
    CHECK((f.s != NULL) == (state.last_store_pc >= 41));
    CHECK((f.k != NULL) == (state.last_store_pc >= 49));
    CHECK(state.references == 1);
    CHECK(state.direct_allocation_bytes <= state.max_direct_allocation_bytes);
    if (f.r != NULL) {
        CHECK(mc_server_reference_list_view(f.r, &items, &count) == MC_SERVER_OK);
        CHECK(items == NULL && count == 0);
    }
    if (f.s != NULL) {
        CHECK(mc_server_synchronized_list_view(f.s, &backing) == MC_SERVER_OK);
        CHECK(backing != NULL && backing != f.r && (const void *)backing != (const void *)f.s);
        CHECK(mc_server_reference_list_view(backing, &items, &count) == MC_SERVER_OK);
        CHECK(items == NULL && count == 0);
    }
    if (f.k != NULL) {
        CHECK(mc_server_tracker_array_view(f.k, &trackers, &count) == MC_SERVER_OK);
        CHECK(trackers != NULL && count == 2 && trackers[0] == NULL && trackers[1] == NULL);
    }
    CHECK(state.phase == (ready ? MC_SERVER_READY : MC_SERVER_FAILED_CONSTRUCTION));
    CHECK(state.lifetime_started == ready);
    CHECK(ready ? state.failed_call_pc == 0 : state.failed_call_pc != 0);
    result = 0;
cleanup:
    return result;
}

int main(void) {
    int result = 1;
    mc_server_runtime *server = NULL, *candidate = NULL;
    mc_server_runtime_fields f;
    mc_server_construction state;
    mc_server_error error;
    CHECK(mc_server_runtime_release(NULL) == MC_SERVER_OK);
    CHECK(mc_server_runtime_retain(NULL) == MC_SERVER_INVALID_ARGUMENT);
    CHECK(mc_server_runtime_allocate(0, &candidate, NULL) == MC_SERVER_LIMIT && candidate == NULL);
    CHECK(mc_server_runtime_allocate(SIZE_MAX, NULL, NULL) == MC_SERVER_INVALID_ARGUMENT);
    CHECK(mc_server_runtime_construct(NULL, NULL) == MC_SERVER_INVALID_ARGUMENT);
    CHECK(mc_server_runtime_allocate(SIZE_MAX, &server, &error) == MC_SERVER_OK);
    CHECK(mc_server_runtime_inspect(server, &f, &state) == MC_SERVER_OK);
    CHECK(state.phase == MC_SERVER_RAW && !f.q && f.r == NULL && f.s == NULL && f.k == NULL);
    const size_t raw_bytes = state.direct_allocation_bytes;
    CHECK(mc_server_runtime_construct(server, &error) == MC_SERVER_OK);
    CHECK(check_server(server, true) == 0);
    CHECK(mc_server_runtime_inspect(server, NULL, &state) == MC_SERVER_OK);
    const size_t full_bytes = state.direct_allocation_bytes;
    CHECK(full_bytes > raw_bytes);
    CHECK(mc_server_runtime_retain(server) == MC_SERVER_OK);
    CHECK(mc_server_runtime_inspect(server, NULL, &state) == MC_SERVER_OK && state.references == 2);
    CHECK(mc_server_runtime_release(server) == MC_SERVER_OK);
    CHECK(mc_server_runtime_construct(server, &error) == MC_SERVER_UNSUPPORTED_STATE);
    CHECK(check_server(server, true) == 0);
    CHECK(mc_server_runtime_release(server) == MC_SERVER_OK);
    server = NULL;
    /* A tight owner can fail after either backing List or the synchronized
     * wrapper has been reached. Each failed attempt remains inspectable and
     * releasable; the null-error path must return the SAME failure category. */
    for (size_t bound = raw_bytes; bound < full_bytes; ++bound) {
        CHECK(mc_server_runtime_allocate(bound, &server, &error) == MC_SERVER_OK);
        CHECK(mc_server_runtime_construct(server, bound % 2u == 0 ? &error : NULL) ==
              MC_SERVER_LIMIT);
        CHECK(check_server(server, false) == 0);
        CHECK(mc_server_runtime_inspect(server, &f, &state) == MC_SERVER_OK);
        CHECK(mc_server_runtime_construct(server, NULL) == MC_SERVER_UNSUPPORTED_STATE);
        CHECK(mc_server_runtime_release(server) == MC_SERVER_OK);
        server = NULL;
    }
    CHECK(mc_server_runtime_allocate(full_bytes, &server, NULL) == MC_SERVER_OK);
    CHECK(mc_server_runtime_construct(server, NULL) == MC_SERVER_OK);
    CHECK(check_server(server, true) == 0);
    result = 0;
cleanup:
    if (mc_server_runtime_release(server) != MC_SERVER_OK)
        result = 1;
    if (result == 0)
        printf("Server constructor: %u native checks passed\n", checks);
    return result;
}
