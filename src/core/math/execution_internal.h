#ifndef MC_CORE_MATH_EXECUTION_INTERNAL_H
#define MC_CORE_MATH_EXECUTION_INTERNAL_H

#include "mc/core/math/classes.h"

typedef struct mc_math_execution_site {
    const char *class_name, *method, *descriptor;
    int pc;
} mc_math_execution_site;

typedef struct mc_math_execution_failure {
    mc_math_execution_site caller, child;
    mc_math_class_error cause;
} mc_math_execution_failure;

typedef enum mc_math_execution_phase {
    MC_MATH_EXECUTION_OPEN,
    MC_MATH_EXECUTION_RETIRING,
    MC_MATH_EXECUTION_RETIRED
} mc_math_execution_phase;

/* Private one-shot caller storage, disjoint from every live owner/error.
 * declared is borrowed/live/exclusive until the first reached use. Only owned
 * is an owning occurrence; unused retirement never inspects declared. NULL
 * execution selects legacy arithmetic, while a nonnull execution with NULL
 * declared fails at the actual IN invocation. No source work is replayed. */
typedef struct mc_math_execution {
    mc_math_classes *declared, *owned;
    mc_math_execution_phase phase;
    bool selected, failed;
    mc_math_execution_failure current, first;
    mc_math_class_error admission, cleanup;
} mc_math_execution;

static inline mc_math_class_error mc_math_execution_empty_error(void) {
    return (mc_math_class_error){MC_MATH_CLASS_OK, NULL, -1, MC_MATH_CLASS_OK};
}

static inline void mc_math_execution_clear_current(mc_math_execution *execution) {
    if (execution == NULL)
        return;
    execution->selected = false;
    execution->current = (mc_math_execution_failure){0};
    execution->current.caller.pc = execution->current.child.pc = -1;
    execution->current.cause = mc_math_execution_empty_error();
}

/* Fresh storage only; never overwrite a live owning occurrence. No retain,
 * phase query, initialization, Random operation or source getter occurs. */
static inline void mc_math_execution_init(mc_math_execution *execution, mc_math_classes *declared) {
    *execution = (mc_math_execution){0};
    execution->declared = declared;
    execution->admission = execution->cleanup = mc_math_execution_empty_error();
    execution->first.caller.pc = execution->first.child.pc = -1;
    execution->first.cause = mc_math_execution_empty_error();
    mc_math_execution_clear_current(execution);
}

static inline const mc_math_class_error *
mc_math_execution_cause(const mc_math_execution *execution) {
    if (execution == NULL)
        return NULL;
    if (execution->admission.status != MC_MATH_CLASS_OK)
        return &execution->admission;
    return execution->failed ? &execution->first.cause : &execution->current.cause;
}

/* Caller has evaluated the actual IN argument first. Retain is lazy and its
 * slot is published only on success. Every reached use ensures the same owner.
 * A failed invocation latches its first cause and cannot resume/retry source.
 * Source-exception attribution is not inferred from native Math status. */
static inline mc_math_class_status mc_math_execution_require(mc_math_execution *execution,
                                                             mc_math_execution_site caller,
                                                             const char *method,
                                                             const char *descriptor) {
    if (execution == NULL)
        return MC_MATH_CLASS_OK;
    if (execution->phase != MC_MATH_EXECUTION_OPEN) {
        execution->admission = (mc_math_class_error){
            MC_MATH_CLASS_STATE_ERROR, "Math execution is not open", -1, MC_MATH_CLASS_OK};
        return execution->admission.status;
    }
    execution->admission = mc_math_execution_empty_error();
    if (execution->failed)
        return execution->first.cause.status;
    mc_math_execution_clear_current(execution);
    execution->selected = true;
    execution->current.caller = caller;
    execution->current.child = (mc_math_execution_site){"in", method, descriptor, -1};
    mc_math_class_status status;
    if (execution->owned == NULL) {
        status = mc_math_classes_retain(execution->declared, &execution->current.cause);
        if (status != MC_MATH_CLASS_OK)
            goto failed;
        execution->owned = execution->declared;
    } else if (execution->declared != execution->owned) {
        status = MC_MATH_CLASS_STATE_ERROR;
        execution->current.cause = (mc_math_class_error){
            status, "Math execution defining namespace changed", -1, MC_MATH_CLASS_OK};
        goto failed;
    }
    status = mc_math_classes_ensure(execution->owned, &execution->current.cause);
    if (status == MC_MATH_CLASS_OK) {
        mc_math_execution_clear_current(execution);
        return status;
    }
failed:
    if (execution->current.cause.source_pc >= 0)
        execution->current.child =
            (mc_math_execution_site){"in", "<clinit>", "()V", execution->current.cause.source_pc};
    execution->first = execution->current;
    execution->failed = true;
    return status;
}

/* Checked native cleanup only. Consult/release owned, never the unused borrowed
 * declared object. Failure retains the exact slot and first source cause.
 * After retirement begins require rejects before touching borrowed fields. */
static inline mc_math_class_status mc_math_execution_retire(mc_math_execution *execution) {
    if (execution == NULL || execution->phase == MC_MATH_EXECUTION_RETIRED)
        return MC_MATH_CLASS_OK;
    execution->phase = MC_MATH_EXECUTION_RETIRING;
    execution->cleanup = mc_math_execution_empty_error();
    if (execution->owned != NULL) {
        const mc_math_class_status status =
            mc_math_classes_release(&execution->owned, &execution->cleanup);
        if (status != MC_MATH_CLASS_OK)
            return status;
    }
    execution->declared = NULL;
    execution->phase = MC_MATH_EXECUTION_RETIRED;
    mc_math_execution_clear_current(execution);
    return MC_MATH_CLASS_OK;
}

#endif
