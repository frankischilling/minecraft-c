#ifndef MC_CLIENT_SOUND_FILE_REGISTRATION_H
#define MC_CLIENT_SOUND_FILE_REGISTRATION_H
#include "mc/client/sound/sound_manager.h"
#include "mc/core/java/url.h"
typedef struct mc_sound_pool_file_call mc_sound_pool_file_call;

typedef struct mc_sound_pool_file_error {
    mc_sound_status status;
    int32_t yo_pc, hr_pc;
    bool source_returned, print_stack_trace_pending;
    mc_sound_error pool;
    mc_sound_pool_report report;
    mc_java_url_error url;
} mc_sound_pool_file_error;

/* Real typed File to URI to URL inside the same hr pool. Caller App/manager/pool
 * storage remains live through call retirement. File/InfoName access stays
 * exclusive through the retained URI prefix; the pool monitor does not
 * serialize unrelated File API callers. One record owns all native
 * failed prefixes; releasing it never rolls back source pool occurrences. */
mc_sound_status mc_sound_manager_add_java_file(mc_sound_manager *, mc_sound_manager_pool_kind,
                                               mc_sound_text, mc_java_file *,
                                               mc_sound_pool_file_call **,
                                               const mc_sound_pool_entry **,
                                               mc_sound_pool_file_error *);
mc_sound_status mc_sound_pool_add_java_file(mc_sound_pool *, mc_sound_text, mc_java_file *,
                                            mc_sound_pool_file_call **,
                                            const mc_sound_pool_entry **,
                                            mc_sound_pool_file_error *);
mc_sound_status mc_sound_pool_file_call_release(mc_sound_pool_file_call **,
                                                mc_sound_pool_file_error *);
#endif
