/* Actual yo/hr constructor ownership and read-only retained-prefix views. */
#ifndef MC_CLIENT_SOUND_CONSTRUCTION_H
#define MC_CLIENT_SOUND_CONSTRUCTION_H
#include "mc/client/sound/sound_manager.h"

typedef enum mc_sound_manager_phase {
    MC_SOUND_MANAGER_RAW,
    MC_SOUND_MANAGER_CONSTRUCTING,
    MC_SOUND_MANAGER_READY,
    MC_SOUND_MANAGER_FAILED,
    MC_SOUND_MANAGER_RETIRING
} mc_sound_manager_phase;

typedef struct mc_sound_constructor_site {
    const char *class_name, *method, *descriptor;
    int32_t pc;
} mc_sound_constructor_site;

typedef struct mc_sound_manager_constructor_error {
    mc_sound_status status;
    const char *operation;
    bool native_policy;
    int32_t pool_index; /* -1 unless the failure belongs to one of the hr children. */
    size_t frame_count;
    mc_sound_constructor_site frames[2]; /* yo outermost, then reached hr child. */
    mc_sound_error cause;                /* Complete old cause, including actual random_status. */
} mc_sound_manager_constructor_error;

/* These flags mark actual source stores over the existing native embedded
 * collection representation. They do not invent Java object-header allocation
 * or expose raw zero storage as a published Map/List/Random. */
typedef struct mc_sound_pool_constructor_prefix {
    int32_t last_store_pc;
    bool random_published, map_published, list_published;
    bool count_published, strip_digits_published;
} mc_sound_pool_constructor_prefix;

typedef struct mc_sound_manager_constructor_state {
    mc_sound_manager_phase phase;
    bool active;
    int32_t last_store_pc;
    unsigned published_pool_mask; /* b12/c23/d34, not native pending candidates. */
    const mc_sound_pool *pools[MC_SOUND_MANAGER_POOL_COUNT];
    const mc_sound_pool *pending_pool; /* Native retained uncompleted hr, read-only. */
    int32_t pending_pool_index;
    mc_sound_pool_constructor_prefix pool_prefixes[MC_SOUND_MANAGER_POOL_COUNT], pending_prefix;
    bool counter_published, random_published, delay_published;
    mc_sound_manager_state values; /* Native observations, not fabricated Random identities. */
    mc_sound_manager_constructor_error failure;
} mc_sound_manager_constructor_state;

/* All storage/outputs must be live, disjoint from owners and exclusive for the
 * complete call. Read-only inspection is permitted while active.
 * Unique native owner, shared lifetime reserved immediately. No source child,
 * Random, settings, backend call/device/worker or I/O is entered. */
mc_sound_status mc_sound_manager_allocate(mc_sound_manager_shared *, mc_sound_manager **out,
                                          mc_sound_error *);

/* One actual source constructor attempt after native admission. Success alone
 * produces READY; an entered failure retains this same FAILED owner/prefix.
 * profile is borrowed only during this call. No caller-provided seed/clock. */
mc_sound_status mc_sound_manager_construct(mc_sound_manager *, mc_java_random_profile *,
                                           size_t pool_allocation_bytes,
                                           mc_sound_manager_constructor_error *);

/* Quiescent/read-only inspection, including RAW/CONSTRUCTING/FAILED. Storage
 * views remain borrowed. Pending/failed pool may not be used as a source pool.
 * Publication flags are source-store history; during RETIRING a successfully
 * retired child pointer is NULL while its historical flag remains set. */
bool mc_sound_manager_read_construction(const mc_sound_manager *,
                                        mc_sound_manager_constructor_state *);

/* Existing mc_sound_manager_retire(&slot,error) consumes/nulls a successful
 * RAW/FAILED/READY occurrence. BUSY/failure preserves the same owning slot.
 * No new public mutable raw-pool or manager retain operation is proposed. */
#endif
