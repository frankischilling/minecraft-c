#ifndef MC_BLOCK_REGISTRY_REGISTRY_H
#define MC_BLOCK_REGISTRY_REGISTRY_H

#include "mc/block/material/material.h"
#include "mc/core/java/random_profile.h"

#include <stddef.h>

typedef struct mc_registry_classes mc_registry_classes;
typedef struct mc_block mc_block;
typedef struct mc_block_sound mc_block_sound;
typedef struct mc_registry_string mc_registry_string;

typedef enum mc_registry_side { MC_REGISTRY_CLIENT, MC_REGISTRY_SERVER } mc_registry_side;

typedef enum mc_registry_status {
    MC_REGISTRY_OK = 0,
    MC_REGISTRY_INVALID_ARGUMENT,
    MC_REGISTRY_LIMIT,
    MC_REGISTRY_ALLOCATION_FAILED,
    MC_REGISTRY_PROFILE_FAILED,
    MC_REGISTRY_NULL_REFERENCE,
    MC_REGISTRY_INDEX_OUT_OF_RANGE,
    MC_REGISTRY_DUPLICATE_BLOCK,
    MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY,
    MC_REGISTRY_FAILED_INITIALIZATION,
    MC_REGISTRY_BUSY
} mc_registry_status;

typedef enum mc_registry_phase {
    MC_REGISTRY_COLD,
    MC_REGISTRY_INITIALIZING,
    MC_REGISTRY_READY,
    MC_REGISTRY_FAILED
} mc_registry_phase;

typedef struct mc_registry_error {
    mc_registry_status status;
    const char *message;
    int32_t initializer_pc;
    int32_t block_id;
    const char *constructor_class;
    int32_t constructor_pc;
    mc_java_random_profile_status profile_status;
} mc_registry_error;

typedef struct mc_registry_options {
    mc_registry_side side;
    mc_java_random_profile *random_profile;
    mc_materials *materials;
    /* Zero selects 4 MiB. Includes native metadata, allocation records and
     * every retained object/array/string, including failed prefixes. */
    size_t max_allocation_bytes;
} mc_registry_options;

/* The actual seventeen declaring-class fields of uu/na. Same-letter subtype
 * fields have separate private storage. References and field storage are
 * borrowed from the class owner and remain live through failed initialization.
 * No field is a detached final-property lookup. */
typedef struct mc_block_fields {
    int32_t bm;
    int32_t bn;
    float bo;
    float bp;
    bool bq;
    bool br;
    double bs, bt, bu, bv, bw, bx;
    const mc_block_sound *by;
    float bz;
    mc_material *bA;
    float bB;
    const mc_registry_string *a;
} mc_block_fields;

typedef struct mc_block_sound_fields {
    const mc_registry_string *a;
    float b;
    float c;
} mc_block_sound_fields;

typedef struct mc_registry_string_view {
    const uint16_t *units;
    size_t length;
} mc_registry_string_view;

/* Diagnostic views do not perform a Java active use or turn an erroneous
 * class into a successfully initialized one. NULL cells describe the reached
 * prefix. Array references are distinct and elements remain owned/live. */
typedef struct mc_registry_block_view {
    mc_registry_phase phase;
    int32_t initializer_pc;
    size_t allocation_bytes;
    size_t named_blocks;
    const mc_block_sound *d, *e, *f, *g, *h, *i, *j, *k, *l;
    mc_block *const *m;
    const bool *n, *o, *p;
    const int32_t *q;
    const bool *r;
    const int32_t *s;
    const bool *t;
} mc_registry_block_view;

/* One shared class owner belongs above all Worlds using this registry.
 * Profile/material dependencies must be the same live application owners and
 * outlive every operation/view. Allocate creates only COLD native metadata:
 * no initializer, Random, clock, resource read or block prefill occurs.
 * All outputs are disjoint from owner/dependency storage and are unchanged
 * on native failure. Calls, view use and destruction require exclusive access.
 * Initialization requires the captured x86-64 strict FP environment throughout
 * the call: round-to-nearest, gradual underflow with FTZ/DAZ off, and no enabled
 * FP traps. Constructor arithmetic precedes the first default Random call;
 * that call's later FP admission is not an earlier constructor-prefix guard. */
mc_registry_status mc_registry_classes_allocate(const mc_registry_options *options,
                                                mc_registry_classes **out,
                                                mc_registry_error *error);

/* Execute the real Block construction prefix. The current implementation
 * stops explicitly at its unfinished Item/stat/resource dependency; it cannot
 * report READY or provide a successful full bootstrap. Failure latches and
 * retains every completed global/object/profile effect. The same owner cannot
 * retry. This explicit entry does not qualify ordinary client/server startup
 * or prescribe a universal class-initialization order. */
mc_registry_status mc_registry_classes_initialize_blocks(mc_registry_classes *classes,
                                                         mc_registry_error *error);

/* Inspect actual reached storage without invoking initialization. Native
 * diagnostic access after failure differs from a successful Java active use.
 * Static names are case-sensitive uu/na fields u..bl; unknown names fail. */
mc_registry_status mc_registry_classes_inspect_blocks(const mc_registry_classes *classes,
                                                      mc_registry_block_view *out,
                                                      mc_registry_error *error);
mc_registry_status mc_registry_classes_inspect_block_static(const mc_registry_classes *classes,
                                                            const char *name, const mc_block **out,
                                                            mc_registry_error *error);
const mc_block_fields *mc_block_fields_get(const mc_block *block);
const mc_block_sound_fields *mc_block_sound_fields_get(const mc_block_sound *sound);
bool mc_registry_string_read(const mc_registry_string *string, mc_registry_string_view *out);

/* Quiescent destruction releases this owner's native storage only, including
 * globally retained failed objects. No caller may retain a borrowed view.
 * The borrowed material/profile owners and their reached effects are retained.
 * free(NULL) succeeds; an entered initializer cannot be destroyed. */
mc_registry_status mc_registry_classes_free(mc_registry_classes *classes, mc_registry_error *error);

#endif
