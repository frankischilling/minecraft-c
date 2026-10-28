#ifndef MC_CORE_JAVA_LOGGING_INTERNAL_H
#define MC_CORE_JAVA_LOGGING_INTERNAL_H
#include "mc/core/java/logging.h"
#include <stdatomic.h>
#include <threads.h>

typedef union mc_java_log_alignment {
    long double number;
    void *pointer;
    uint64_t integer;
} mc_java_log_alignment;

typedef struct mc_java_log_allocation {
    struct mc_java_log_allocation *next;
    mc_java_log_alignment alignment;
} mc_java_log_allocation;

typedef struct mc_java_log_handlers {
    size_t count;
    /* The reached configuration owns a genuine empty collection. No Handler
     * object/list element or source emission is constructed in this slice. */
} mc_java_log_handlers;

typedef struct mc_java_log_configuration {
    struct mc_java_log_configuration *delegate;
    mc_java_log_handlers *handlers, *pending_handlers;
    bool ready, use_parent_handlers, declared_level_present, filter_present;
    int32_t level_value, declared_level, last_store;
} mc_java_log_configuration;

typedef struct mc_java_log_defaults {
    int32_t root_level, console_level;
    bool console_handler, simple_formatter, default_encoding, filter_absent;
} mc_java_log_defaults;

typedef struct mc_java_log_entry {
    struct mc_java_log_entry *next;
    mc_java_logger *logger;
} mc_java_log_entry;

struct mc_java_logger {
    mc_java_logging *storage, *manager;
    mc_java_logger *parent;
    mc_java_logger_kind kind;
    mc_java_logging_phase phase;
    atomic_size_t external_holds;
    uint16_t *name, *pending_name;
    size_t name_length;
    mc_java_log_configuration *configuration, *pending_configuration;
    bool system_logger, bundle_absent, registered;
    int32_t last_store;
};

struct mc_java_logging {
    mtx_t mutex;
    atomic_size_t service_holds, external_logger_holds, active_calls;
    atomic_bool mutex_quarantined;
    atomic_int unlock_failure;
    mc_java_logging_options options;
    mc_java_logging_phase phase, logger_class_phase, manager_phase;
    mc_java_logging_stage stage, last_completed;
    bool entered, manager_published;
    int root_handler_state;
    size_t native_bytes, allocations, raw_loggers, registry_count;
    mc_java_log_allocation *allocations_head;
    mc_java_logger *global, *root, *minecraft, *pending_logger;
    mc_java_log_defaults *configuration;
    mc_java_log_entry *registry_head, *registry_tail, *pending_registration;
    mc_java_logging_error failure;
};
#endif
