#ifndef MC_CORE_MATH_CLASSES_H
#define MC_CORE_MATH_CLASSES_H
#include "mc/core/math/math.h"
#include <stddef.h>

typedef struct mc_math_classes mc_math_classes;

typedef enum mc_math_class_phase {
    MC_MATH_CLASS_COLD,
    MC_MATH_CLASS_CONSTRUCTING,
    MC_MATH_CLASS_READY,
    MC_MATH_CLASS_FAILED,
    MC_MATH_CLASS_RETIRING
} mc_math_class_phase;

typedef enum mc_math_class_status {
    MC_MATH_CLASS_OK,
    MC_MATH_CLASS_INVALID_ARGUMENT,
    MC_MATH_CLASS_STATE_ERROR,
    MC_MATH_CLASS_BUSY,
    MC_MATH_CLASS_LIMIT,
    MC_MATH_CLASS_ALLOCATION_FAILED,
    MC_MATH_CLASS_UNSUPPORTED_FP,
    MC_MATH_CLASS_INITIALIZATION_FAILED
} mc_math_class_status;

typedef struct mc_math_class_error {
    mc_math_class_status status;
    const char *operation;
    int source_pc; /* in.<clinit>; -1 is native ownership admission. */
    mc_math_class_status initial_cause;
} mc_math_class_error;

typedef struct mc_math_class_state {
    mc_math_class_phase phase;
    size_t holds, maximum_bytes;
    unsigned last_store_pc;
    const mc_math_trig *source_a; /* actual heap array identity, also on failure */
} mc_math_class_state;

/* One concrete MathHelper class namespace. Creating it is native ownership,
 * with no array, fill, Random or source active use. maximum_bytes includes this
 * descriptor and its eventual table. All operations require exclusive access.
 * No callbacks, process globals or detached caller-supplied table are used. */
mc_math_class_status mc_math_classes_create(size_t maximum_bytes, mc_math_classes **out,
                                            mc_math_class_error *error);
mc_math_class_status mc_math_classes_retain(mc_math_classes *classes, mc_math_class_error *error);
/* Success consumes/nulls exactly this occurrence, including remaining holds.
 * BUSY/error preserves it. Final retirement cannot admit a new hold. */
mc_math_class_status mc_math_classes_release(mc_math_classes **occurrence,
                                             mc_math_class_error *error);
mc_math_class_status mc_math_classes_state(const mc_math_classes *classes, mc_math_class_state *out,
                                           mc_math_class_error *error);
/* Real first active use: calloc zero65536, publish in.a4, then admit FP20 and
 * call the existing sole fill algorithm. FAILED latches its original cause;
 * retry cannot allocate or refill. Sticky exception flags are allowed, control
 * changes are rejected without resetting them. Published zeros survive failure. */
mc_math_class_status mc_math_classes_ensure(mc_math_classes *classes, mc_math_class_error *error);
/* READY-only borrowed actual table. Future App/World consumers adopt this same
 * owner/table and keep a class hold; they must not copy or separately free it. */
const mc_math_trig *mc_math_classes_table(const mc_math_classes *classes);
#endif
