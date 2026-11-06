#ifndef MC_WORLD_RUNTIME_RAIL_POWER_SEARCH_INTERNAL_H
#define MC_WORLD_RUNTIME_RAIL_POWER_SEARCH_INTERNAL_H

/* Lifecycle and its private searches share one declaration of the complete
 * search error and the two held method interfaces. */
#include "rail_lifecycle_internal.h"

mc_rail_power_search_error mc_rail_power_search_empty_error(void);

/* mc_rail_power_search_held / mc_rail_power_verify_held are declared by the
 * included lifecycle header. Both are concrete pc/jj instance bodies. Caller
 * selects a genuine compatible receiver, keeps its Block/defining Registry
 * live/exclusive and holds/enters the supplied nonnull GAME World through any
 * reached source getter. World is otherwise nullable; search depth>=8 returns
 * ordinary false before a getter or class active use. A null instance receiver
 * still fails native invocation admission with its body unentered.
 *
 * out and error are required, disjoint from owners and input views. Failure
 * preserves out and completed source effects. Incoming diagnostics are ignored;
 * success returns a fresh neutral record. search/verify/cause are finite
 * current/deepest source sites, not an allocated stack or a work limit.
 * Actual raw metadata is passed to recursive search and only masked where
 * source does so. All iinc, coordinates and depth+1 wrap as Java ints. No
 * nonnegative-depth gate, kind dispatch, ID constant or native recursion cap
 * supplements the source's specific depth>=8 branch.
 *
 * Diagnostic context metadata in a verifier is neutral0 until its actual
 * metadata26 read completes; axis in a search becomes its actual masked/local
 * axis only after depth guard and source masking. PCs identify whether those
 * operations were reached; neutral context is not an extra source read.
 * Failure publication retains these sites and complete lower children in the
 * flat public mutation diagnostics. */

#endif
