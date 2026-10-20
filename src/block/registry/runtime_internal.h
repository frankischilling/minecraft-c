#ifndef MC_BLOCK_REGISTRY_RUNTIME_INTERNAL_H
#define MC_BLOCK_REGISTRY_RUNTIME_INTERNAL_H

#include "mc/block/registry/registry.h"

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

#endif
