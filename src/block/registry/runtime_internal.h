#ifndef MC_BLOCK_REGISTRY_RUNTIME_INTERNAL_H
#define MC_BLOCK_REGISTRY_RUNTIME_INTERNAL_H

#include "mc/block/registry/registry.h"

/* Read-only admission for ordinary classes_release: reject NULL, entered
 * initialization, then zero runtime borrows, with fresh caller diagnostics.
 * No owner mutation, class use, allocation, release or callback occurs. The
 * error, if present, is disjoint from owner storage. Success reserves nothing;
 * keep the same owner exclusive and unchanged through the matching release. */
mc_registry_status mc_registry_runtime_borrow_release_admit(const mc_registry_classes *classes,
                                                            mc_registry_error *error);

/* Drop one independently owned native borrow, including inside an admitted
 * synchronous registry flow. No owner destruction/class use/callback occurs;
 * entered still prevents class destruction. Caller must own that exact live
 * borrow. Unlike ordinary public release, this internal retirement operation
 * does not reject entered, so an unrelated manager can retire safely during a
 * resource callback. False means a violated native ownership precondition. */
bool mc_registry_runtime_borrow_release(mc_registry_classes *classes);

/* Resolve a reached ASCII code literal in the actual shared String pool.
 * The caller keeps the same Registry live/exclusive; text is a programmer-owned
 * NUL-terminated literal, not external input. No class active use, phase gate,
 * borrow, resource callback or Random operation occurs. Checked allocation
 * retains its reached prefix and uses the existing Registry ledger. Failure
 * preserves out and reports fresh runtime diagnostics, without attributing an
 * earlier class initializer/Block constructor to this String allocation. */
mc_registry_status mc_registry_runtime_literal_ascii(mc_registry_classes *classes, const char *text,
                                                     const mc_registry_string **out,
                                                     mc_registry_error *error);

/* Nonowning actual q reference captured by an ordinary Block active use.
 * Existing final q has fixed extent256; NULL is a successful captured source
 * reference. Keep the same class owner live/exclusive through later iaload.
 * get preserves out on failure and retains the complete initializer cause.
 * at performs no active use, phase check, current-q comparison or reload;
 * captured NULL precedes signed bounds. Fresh element failures have no stale
 * initializer/constructor attribution. No allocation, borrow or Random call. */
typedef struct mc_registry_block_q_view {
    mc_registry_classes *classes;
    const int32_t *identity;
    size_t length;
} mc_registry_block_q_view;

mc_registry_status mc_registry_block_q_get(mc_registry_classes *classes,
                                           mc_registry_block_q_view *out, mc_registry_error *error);
mc_registry_status mc_registry_block_q_at(const mc_registry_block_q_view *array, int32_t index,
                                          int32_t *out, mc_registry_error *error);

/* Actual m captured before a later acquiring World getter. NULL is a valid
 * captured reference; at performs its null/bounds checks only when reached.
 * Elements may themselves be NULL. No active use or current-array reload at at.
 * Caller keeps the same Registry owner live/exclusive across intervening work. */
typedef struct mc_registry_block_m_view {
    mc_registry_classes *classes;
    mc_block *const *identity;
    size_t length;
} mc_registry_block_m_view;

mc_registry_status mc_registry_block_m_get(mc_registry_classes *classes,
                                           mc_registry_block_m_view *out, mc_registry_error *error);
mc_registry_status mc_registry_block_m_at(const mc_registry_block_m_view *array, int32_t index,
                                          const mc_block **out, mc_registry_error *error);

/* Actual random-tick n reference. get performs reached Block active use;
 * captured NULL succeeds. at checks that same reference before signed bounds,
 * with no class use, current-array reload, phase gate or failed-prefix fallback.
 * Keep its supplying Registry live/exclusive through every later baload. */
typedef struct mc_registry_block_n_view {
    mc_registry_classes *classes;
    const bool *identity;
    size_t length;
} mc_registry_block_n_view;

mc_registry_status mc_registry_block_n_get(mc_registry_classes *, mc_registry_block_n_view *,
                                           mc_registry_error *);
mc_registry_status mc_registry_block_n_at(const mc_registry_block_n_view *, int32_t index,
                                          bool *out, mc_registry_error *);

/* Actual o reference captured at its reached Block active use. Trees retain
 * this exact array across a later World getter; substrate predicates index it
 * immediately. at never reloads current o or performs another active use. The
 * same lifetime, nullable-reference and signed-index rules as m/q/t apply. */
typedef struct mc_registry_block_o_view {
    mc_registry_classes *classes;
    const bool *identity;
    size_t length;
} mc_registry_block_o_view;

mc_registry_status mc_registry_block_o_get(mc_registry_classes *classes,
                                           mc_registry_block_o_view *out, mc_registry_error *error);
mc_registry_status mc_registry_block_o_at(const mc_registry_block_o_view *array, int32_t index,
                                          bool *out, mc_registry_error *error);

/* Actual TileEntity-bearing Block p reference captured before later Chunk lookup
 * work. A false element returns before the separate m capture. NULL capture
 * succeeds; at checks that retained reference before signed bounds, without
 * reloading p or invoking class initialization. Caller keeps the same Registry
 * owner and captured array alive/exclusive through the later baload. */
typedef struct mc_registry_block_p_view {
    mc_registry_classes *classes;
    const bool *identity;
    size_t length;
} mc_registry_block_p_view;

mc_registry_status mc_registry_block_p_get(mc_registry_classes *classes,
                                           mc_registry_block_p_view *out, mc_registry_error *error);
mc_registry_status mc_registry_block_p_at(const mc_registry_block_p_view *array, int32_t index,
                                          bool *out, mc_registry_error *error);

/* Actual live metadata-notification t reference, with the same get/at timing
 * contract. Index masking belongs to the World bytecode, never this accessor. */
typedef struct mc_registry_block_t_view {
    mc_registry_classes *classes;
    const bool *identity;
    size_t length;
} mc_registry_block_t_view;

mc_registry_status mc_registry_block_t_get(mc_registry_classes *classes,
                                           mc_registry_block_t_view *out, mc_registry_error *error);
mc_registry_status mc_registry_block_t_at(const mc_registry_block_t_view *array, int32_t index,
                                          bool *out, mc_registry_error *error);

/* Actual wj/od fields a..d. Active use initializes only this independent class:
 * four distinct six-int arrays, with retained earlier publications and a
 * first-failure latch. No Block/TileEntity/Random/bed-table initialization occurs.
 * Nested exclusive Registry initializer flows are admitted and restored.
 * A successful capture may be NULL; no array element is inspected here. */
typedef enum mc_registry_direction_field {
    MC_REGISTRY_DIRECTION_A = 0,
    MC_REGISTRY_DIRECTION_B,
    MC_REGISTRY_DIRECTION_C,
    MC_REGISTRY_DIRECTION_D
} mc_registry_direction_field;

typedef struct mc_registry_direction_view {
    mc_registry_classes *classes;
    const int32_t *identity;
    size_t length;
} mc_registry_direction_view;

mc_registry_status mc_registry_direction_get(mc_registry_classes *classes,
                                             mc_registry_direction_field field,
                                             mc_registry_direction_view *out,
                                             mc_registry_error *error);

/* Reached iaload on the captured identity, with NULL before signed bounds.
 * Never reloads current fields, initializes a class or consults its later
 * phase. Nonnull arrays must be the supplying owner's actual retained
 * six-int allocations; unknown projections are not adopted or dereferenced.
 * The view is nonowning: keep that owner live/exclusive through every use,
 * including field replacement. No hold/allocation/callback occurs. Failure
 * preserves out, records a fresh runtime cause and never changes the class's
 * first initializer failure. Fresh native admission leaves last_error alone. */
mc_registry_status mc_registry_direction_at(const mc_registry_direction_view *array, int32_t index,
                                            int32_t *out, mc_registry_error *error);

/* Actual nullable captured emission array. No reload or active use at at. */
typedef struct mc_registry_block_s_view {
    mc_registry_classes *classes;
    const int32_t *identity;
    size_t length;
} mc_registry_block_s_view;

mc_registry_status mc_registry_block_s_get(mc_registry_classes *classes,
                                           mc_registry_block_s_view *out, mc_registry_error *error);
mc_registry_status mc_registry_block_s_at(const mc_registry_block_s_view *array, int32_t index,
                                          int32_t *out, mc_registry_error *error);

#endif
