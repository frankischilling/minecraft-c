#ifndef MC_WORLD_DIMENSION_TEMPERATURE_H
#define MC_WORLD_DIMENSION_TEMPERATURE_H

#include "mc/world/biome/biome.h"
#include "mc/world/dimension/dimension.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct mc_temperature_owner mc_temperature_owner;
typedef struct mc_temperature_array mc_temperature_array;

typedef struct mc_temperature_limits {
    /* Independently bounded native requests. Direct includes the live owner
     * and all live array-handle objects; buffer includes the existing Biome
     * allocator headers and double payloads. Both are retained/peak limits,
     * not cumulative allocation counts or a combined World/manager budget.
     * Old plus new requests stay charged through replacement. Explicit zero
     * is valid. NULL limits selects 1MiB direct and 64MiB buffer storage. */
    size_t max_direct_allocation_bytes;
    size_t max_buffer_allocation_bytes;
} mc_temperature_limits;

typedef struct mc_temperature_owner_state {
    size_t holds, arrays;
    size_t direct_allocation_bytes, buffer_allocation_bytes;
    mc_temperature_limits limits;
} mc_temperature_owner_state;

typedef struct mc_temperature_array_view {
    const mc_temperature_owner *owner;
    /* Borrowed actual storage. Contents may be inspected/prefilled only with
     * exclusive access; changing descriptors/ownership is never permitted. */
    double *data;
    size_t length, holds;
} mc_temperature_array_view;

typedef enum mc_temperature_body {
    MC_TEMPERATURE_BODY_NONE = 0,
    MC_TEMPERATURE_BODY_NORMAL, /* xv/ph.a([DIIII)[D */
    MC_TEMPERATURE_BODY_FIXED   /* hd/et.a([DIIII)[D */
} mc_temperature_body;

typedef struct mc_temperature_error {
    mc_biome_status status;
    const char *operation;
    mc_temperature_body body;
    unsigned int bytecode;
    /* ug/mq long raw overload allocation35, when actually attempted. Other
     * native guards leave this zero; no source VM frame is manufactured. */
    unsigned int raw_bytecode;
    bool native_boundary;
    mc_world_dimension_status manager_status;
    bool reused_input, temperature_filled, auxiliary_published;
    size_t cells_completed;
} mc_temperature_error;

void mc_temperature_limits_default(mc_temperature_limits *out);
mc_biome_status mc_temperature_owner_create(const mc_temperature_limits *limits,
                                            mc_temperature_owner **out,
                                            mc_temperature_error *error);
mc_biome_status mc_temperature_owner_retain(mc_temperature_owner *owner);
void mc_temperature_owner_release(mc_temperature_owner *owner);
mc_biome_status mc_temperature_owner_get(const mc_temperature_owner *owner,
                                         mc_temperature_owner_state *out);

/* A live array owns one independent hold on its allocation ledger. Array
 * aliases retain this SAME handle, not a copied owning descriptor. Thus the
 * last provider/manager root can retire while genuine array holds keep the
 * output and its allocator alive. NULL release is harmless. NULL retain/get
 * is invalid; the source NULL-array value is represented by a NULL handle.
 * No reference count is a Java field or a Java GC/OOME equivalence claim. */
mc_biome_status mc_temperature_array_retain(mc_temperature_array *array);
void mc_temperature_array_release(mc_temperature_array *array);
mc_biome_status mc_temperature_array_get(const mc_temperature_array *array,
                                         mc_temperature_array_view *out);

/* Concrete actual normal/fixed manager temperature methods. receiver is a
 * genuine module-owned Dimension biome manager, not a fabricated descriptor;
 * it is retained through the whole call. The call chooses its actual normal
 * or fixed body. No Registry/World callback, Random, biome classification or
 * replacement manager is supplied by this API.
 *
 * input is the caller's current actual nullable array, borrowed on entry.
 * Success returns ONE owned reference at out: the SAME handle when capacity
 * suffices, otherwise a new exact-length array. An allocated zero-length array
 * is nonnull and distinct from source NULL. A different current manager does
 * not change an input's allocator membership or invalidate its identity.
 * allocation_owner is consulted only when a new output is actually needed at
 * source19, and may be NULL when reusing a sufficient input. Manager.c remains
 * allocated/charged to the actual current manager's existing memory owner.
 *
 * Normal: allocate output19 if needed, raw temperature fill44 (WHOLE retained
 * length cleared), then raw auxiliary74 and manager.c publication77, then
 * exact temperature processing. No a/b/d admission or mutation occurs. Failure
 * at c retains completed reused-input raw writes. A newly allocated output is
 * private until return209 and is released on failure. Fixed: allocate19 if
 * needed, read actual f30, fill only requested prefix33, return37; larger suffix
 * remains unchanged and a/b/c/d are untouched. No strict-FP gate is added to
 * this bit-copy-only fixed body.
 *
 * width/height/product must fit INT32_MAX and native spans. These checked
 * nonnegative dimensions, actual immutable embedded noise owners and the
 * captured x86-64 strict FP profile are a declared native domain, not every
 * original Java/reflection/aliasing/error case. Normal's FP guard is reached
 * at44 AFTER possible output allocation; no atomic numerical temperature API
 * is called. Successful completion is not original/native qualification.
 *
 * Failure preserves out and reached effects; error is fresh and may be NULL.
 * out/error/owners/inputs are exclusive, live and disjoint. A previously stored
 * out value must not own an unreleased reference (a borrowed sentinel is OK).
 * Call with a separate local result, NOT the provider.w field itself: only the
 * controller may transfer that result to w at its successful putfield2684.
 * Release the old w hold after that store. Keep a retained handle across any
 * later reentry; the next source getfield must reload the current field.
 * No same-buffer concurrent calls, descriptor surgery or reentry into this
 * pure native call is permitted. Synchronous World reentry happens outside
 * it, for example the later snow height query, and must observe real w state. */
mc_biome_status mc_temperature_sample(mc_world_dimension_biome *receiver,
                                      mc_temperature_owner *allocation_owner,
                                      mc_temperature_array *input, int32_t x, int32_t z,
                                      size_t width, size_t height, mc_temperature_array **out,
                                      mc_temperature_error *error);

#endif
