#ifndef MC_CORE_JAVA_LOGGING_H
#define MC_CORE_JAVA_LOGGING_H
#include "mc/core/java/properties.h"

typedef struct mc_java_logging mc_java_logging;
typedef struct mc_java_logger mc_java_logger;

typedef enum mc_java_logging_status {
    MC_JAVA_LOGGING_OK,
    MC_JAVA_LOGGING_INVALID_ARGUMENT,
    MC_JAVA_LOGGING_INVALID_PHASE,
    MC_JAVA_LOGGING_BUSY,
    MC_JAVA_LOGGING_LIMIT,
    MC_JAVA_LOGGING_ALLOCATION_FAILED,
    MC_JAVA_LOGGING_MUTEX_FAILED,
    MC_JAVA_LOGGING_UNSUPPORTED_CONFIGURATION,
    MC_JAVA_LOGGING_LATCHED_FAILURE
} mc_java_logging_status;

typedef enum mc_java_logging_profile {
    MC_JAVA_LOGGING_NORMAL_DEFAULT,
    MC_JAVA_LOGGING_CUSTOM_CONFIGURATION_UNIMPLEMENTED
} mc_java_logging_profile;

typedef enum mc_java_logging_phase {
    MC_JAVA_LOGGING_COLD,
    MC_JAVA_LOGGING_CONSTRUCTING,
    MC_JAVA_LOGGING_READY,
    MC_JAVA_LOGGING_FAILED,
    MC_JAVA_LOGGING_RETIRING
} mc_java_logging_phase;

typedef enum mc_java_logging_stage {
    MC_JAVA_LOGGING_STAGE_COLD,
    MC_JAVA_LOGGING_STAGE_GLOBAL,
    MC_JAVA_LOGGING_STAGE_MANAGER,
    MC_JAVA_LOGGING_STAGE_ROOT,
    MC_JAVA_LOGGING_STAGE_CONFIGURATION,
    MC_JAVA_LOGGING_STAGE_REGISTER_ROOT,
    MC_JAVA_LOGGING_STAGE_REGISTER_GLOBAL,
    MC_JAVA_LOGGING_STAGE_MINECRAFT,
    MC_JAVA_LOGGING_STAGE_REGISTER_MINECRAFT,
    MC_JAVA_LOGGING_STAGE_READY
} mc_java_logging_stage;

typedef enum mc_java_logger_kind {
    MC_JAVA_LOGGER_GLOBAL,
    MC_JAVA_LOGGER_ROOT,
    MC_JAVA_LOGGER_MINECRAFT
} mc_java_logger_kind;

typedef struct mc_java_logging_site {
    const char *class_name, *method, *descriptor;
    int32_t pc;
} mc_java_logging_site;

typedef struct mc_java_logging_error {
    mc_java_logging_status status, cause_status;
    const char *operation; /* Static; no borrowed node/name/Throwable views. */
    mc_java_logging_site game_caller, site, cause_site;
    mc_java_logging_stage stage, last_completed;
    size_t native_bytes, native_allocations, raw_loggers, registry_count;
    int mutex_result;
    bool native_policy;
} mc_java_logging_error;

typedef struct mc_java_logging_options {
    mc_java_logging_profile profile;
    /* Owner sizeof is charged to bytes; allocation count covers later actual
     * child ledger allocations, not the cold owner. Zero admits none. */
    size_t max_native_bytes, max_allocations, max_loggers;
} mc_java_logging_options;

typedef struct mc_java_logger_state {
    mc_java_logger_kind kind;
    mc_java_logging_phase phase;
    const mc_java_logging *storage, *manager;
    const mc_java_logger *identity, *parent;
    mc_java_properties_string_view name; /* Native owned value, not Java getName. */
    const void *pending_name, *configuration, *pending_configuration;
    const void *handlers, *pending_handlers, *delegate;
    size_t external_holds, handler_count;
    bool name_stored, configuration_stored, system_logger, bundle_absent;
    bool use_parent_handlers, declared_level_present, registered, filter_present;
    int32_t effective_level, declared_level, constructor_last_store_pc,
        configuration_constructor_last_store_pc;
} mc_java_logger_state;

typedef struct mc_java_logging_state {
    mc_java_logging_phase phase, logger_class_phase, manager_phase;
    mc_java_logging_stage stage, last_completed;
    size_t service_holds, external_logger_holds, active_calls;
    size_t native_bytes, native_allocations, raw_loggers, registry_count;
    bool entered, manager_published, configuration_published, mutex_quarantined;
    int root_handler_state; /* Actual default3 means lazy/uninstantiated. */
    bool console_configured, console_constructed;
    int32_t default_level, configured_console_level;
    const mc_java_logger *global, *root, *minecraft, *pending_logger;
    const void *configuration, *pending_registration;
    const mc_java_logger *registry[3]; /* Actual publication order; borrowed. */
    const mc_java_logging_error *failure;
} mc_java_logging_state;

mc_java_logging_options mc_java_logging_default_options(void);
void mc_java_logging_error_init(mc_java_logging_error *);
/* Cold native storage only. No JavaHome/config file, classes/JVM, clock,
 * handler/formatter/stderr, worker or source initialization. Out is unchanged
 * on failure. All inputs/outputs/errors/slots are live and disjoint. */
mc_java_logging_status mc_java_logging_create(const mc_java_logging_options *,
                                              mc_java_logging **out, mc_java_logging_error *);
/* Each concurrent operation owns its own service occurrence, or a live Logger
 * occurrence for logger operations. No occurrence may be released concurrently
 * with its use. Namespace/data initialization is serialized by a real recursive
 * C17 mutex, not an application callback. Same-thread reentry returns BUSY. */
mc_java_logging_status mc_java_logging_retain(mc_java_logging *, mc_java_logging_error *);
mc_java_logging_status mc_java_logging_release(mc_java_logging **, mc_java_logging_error *);
mc_java_logging_status mc_java_logging_retire(mc_java_logging **, mc_java_logging_error *);
/* The sole reached game operation: fm.<clinit> getLogger("Minecraft")2.
 * First real initialization publishes distinct global/root/Minecraft nodes,
 * complete parent/config/registry in order, then one counted return occurrence.
 * Repeated calls share the same node. No generic successful emission API.
 * A failed initialization latches and retains pending/publication prefixes;
 * no constructor retry. A later native handle-count refusal keeps READY data.
 * Ordinary failure preserves out. Native control/limit failures are not Java
 * Throwable/OOME categories and do not store fm.a on the caller's behalf. */
mc_java_logging_status mc_java_logging_get_minecraft(mc_java_logging *, mc_java_logger **out,
                                                     mc_java_logging_error *);
mc_java_logging_status mc_java_logger_retain(mc_java_logger *, mc_java_logging_error *);
mc_java_logging_status mc_java_logger_release(mc_java_logger **, mc_java_logging_error *);
/* Quiescent, borrowed views. Require a live service/Logger occurrence; returned
 * pointers expire on mutation/retirement. No retain of a borrowed const node is
 * implied. No concurrent initialization/retirement may overlap inspection. */
mc_java_logging_status mc_java_logging_inspect(mc_java_logging *, mc_java_logging_state *,
                                               mc_java_logging_error *);
mc_java_logging_status mc_java_logger_inspect(const mc_java_logger *, mc_java_logger_state *,
                                              mc_java_logging_error *);
/* Successful occurrence release always consumes/nulls its slot. Strict retire
 * requires the final service occurrence, zero Logger holds and no other active
 * calls/waiters. BUSY/failure preserves it. No source reset/close/output occurs.
 * Actual mutex-unlock provider failure quarantines retained native storage;
 * it is an explicit unrecoverable provider frontier, never uncertain free. */
#endif
