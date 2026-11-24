#ifndef MC_SERVER_RUNTIME_CONSTRUCTION_H
#define MC_SERVER_RUNTIME_CONSTRUCTION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct mc_server_runtime mc_server_runtime;
typedef struct mc_server_reference_list mc_server_reference_list;
typedef struct mc_server_synchronized_list mc_server_synchronized_list;
typedef struct mc_server_tracker_array mc_server_tracker_array;
typedef struct mc_server_listener mc_server_listener;
typedef struct mc_server_properties mc_server_properties;
typedef struct mc_server_world_array mc_server_world_array;
typedef struct mc_server_player_manager mc_server_player_manager;
typedef struct mc_server_console mc_server_console;
typedef struct mc_server_tracker mc_server_tracker;
typedef struct mc_world_info_name mc_world_info_name;

typedef enum mc_server_status {
    MC_SERVER_OK = 0,
    MC_SERVER_INVALID_ARGUMENT,
    MC_SERVER_LIMIT,
    MC_SERVER_ALLOCATION_FAILED,
    MC_SERVER_THREAD_FAILED,
    MC_SERVER_UNSUPPORTED_STATE,
    MC_SERVER_BUSY
} mc_server_status;

typedef enum mc_server_phase {
    MC_SERVER_RAW = 0,
    MC_SERVER_CONSTRUCTING,
    MC_SERVER_READY,
    MC_SERVER_FAILED_CONSTRUCTION
} mc_server_phase;

typedef struct mc_server_error {
    mc_server_status status;
    const char *operation;
    unsigned int bytecode;
    int thread_status;
} mc_server_error;

/* All instance fields declared by the original MinecraftServer. The separate
 * default constructor does not enter its private startup method: connection,
 * properties, Worlds, player manager and console remain actual null fields.
 * These borrowed views expire with the owner. No gameplay mutation is exposed. */
typedef struct mc_server_runtime_fields {
    mc_server_listener *c;
    mc_server_properties *d;
    mc_server_world_array *e;
    mc_server_player_manager *f;
    mc_server_console *p;
    bool q, g;
    int32_t h;
    mc_world_info_name *i;
    int32_t j;
    mc_server_reference_list *r;
    mc_server_synchronized_list *s;
    mc_server_tracker_array *k;
    bool l, m, n, o;
} mc_server_runtime_fields;

typedef struct mc_server_construction {
    mc_server_phase phase;
    unsigned int last_store_pc, failed_call_pc;
    size_t direct_allocation_bytes, max_direct_allocation_bytes, references;
    bool lifetime_started;
} mc_server_construction;

/* Allocate a RAW identity, then make one constructor attempt on that identity.
 * max_direct_bytes bounds the owner and actual children, excluding allocator
 * and host thread-stack overhead. Failure preserves out. Constructor failure
 * preserves published fields and cannot be retried. No files, socket, Random,
 * clock sample, command loop or gameplay tick is reached by this entry.
 *
 * The original bt daemon has no game work: its run body only sleeps forever.
 * The native counterpart waits on an owned condition until explicit native
 * destruction. It never reads or writes game fields. This is a host-lifetime
 * adaptation, not Java Thread/bootstrap/interrupt timing equivalence. */
mc_server_status mc_server_runtime_allocate(size_t max_direct_bytes, mc_server_runtime **out,
                                            mc_server_error *error);
mc_server_status mc_server_runtime_construct(mc_server_runtime *server, mc_server_error *error);
mc_server_status mc_server_runtime_inspect(const mc_server_runtime *server,
                                           mc_server_runtime_fields *fields,
                                           mc_server_construction *construction);

/* One typed native hold per owning reference. External lifecycle access is
 * exclusive; a World takes its hold only at the reached F store. Releasing the
 * final hold retires the inert lifetime thread and all constructed children,
 * without startup, gameplay callbacks or a source save/shutdown operation.
 * NULL release succeeds. A failed admission never changes the hold count. */
mc_server_status mc_server_runtime_retain(mc_server_runtime *server);
mc_server_status mc_server_runtime_release(mc_server_runtime *server);

mc_server_status mc_server_reference_list_view(const mc_server_reference_list *list,
                                               const void *const **items, size_t *count);
mc_server_status mc_server_synchronized_list_view(const mc_server_synchronized_list *list,
                                                  const mc_server_reference_list **backing);
mc_server_status mc_server_tracker_array_view(const mc_server_tracker_array *array,
                                              mc_server_tracker *const **items, size_t *count);

#endif
