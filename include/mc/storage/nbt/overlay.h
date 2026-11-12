#ifndef MC_STORAGE_NBT_OVERLAY_H
#define MC_STORAGE_NBT_OVERLAY_H

#include "mc/storage/nbt/nbt.h"

/* Native borrowed composition of already generated Compound fields. source
 * may be NULL; fields must be a Compound. Each input Compound has unique child
 * names. Decoded/owned NBT already has that invariant; duplicate-key resolution
 * is not performed here. Supply finite caller-bounded, readable immutable root
 * tags, direct child-pointer spans, child headers and UTF16 name spans for the
 * duration of both calls and subsequent borrowed-result use. No recursion or
 * allocation occurs: descendant payload validity belongs to the outer codec.
 *
 * Shallow admission checks Compound types, pointer-count byte overflow, nonnull
 * spans/children, value-tag types (not End), and valid name spans/lengths. A name
 * may contain any UTF16 units, including NUL and unpaired surrogates, within
 * UINT16_MAX units. Matching compares length and every unit; no ASCII, encoding,
 * normalization, pointer identity or NUL-terminated comparison is substituted.
 * Duplicate-key freedom and actual span readability are caller preconditions;
 * this API cannot probe stale pointers or discover unreadable backing storage.
 *
 * Preserve every baseline child's position, replacing matching names with the
 * actual generated child pointer. Keep unknown children and all descendant/raw
 * payload pointers untouched. Append fields absent from source in fields order.
 * Preserve source's exact root name if present, otherwise fields' root name.
 * The caller may rename a composed result to its actual nested field context.
 * No schema, Stack/Entity phase/owner, callback, hold or gameplay state exists
 * in this facility. Time is O(n+m+n*m), auxiliary C storage is bounded constant.
 *
 * count publishes the exact required child-pointer count, including checked
 * final pointer-byte extent, only on success. out is required. The builder
 * requires scratch_count>=that count; NULL scratch is valid only with count0
 * and scratch_count0. Nonnull scratch with excess capacity is accepted. All
 * fallible admission completes before any scratch/output write. On failure,
 * out and every scratch slot remain unchanged. Success writes only the first
 * required slots; trailing slots remain unchanged. Empty results may borrow
 * NULL scratch. The result borrows inputs/names and scratch until outer use
 * finishes; no source memory is copied or owned and no self-pointer is added.
 *
 * Output/error/scratch storage must be mutually disjoint and must not overlap
 * any input tag/header/name/direct-child span read by the operation. Distinct
 * disjoint subspans of the same backing allocation are permitted: one result
 * may borrow an earlier scratch span while a later overlay writes another.
 * The two immutable inputs may share tags/spans or be the same Compound. No address-range or
 * overlap detection is supplied. out is unchanged on all errors; error may be
 * NULL and is freshly {returned_status,0}, including {OK,0} on success.
 * INVALID_ARGUMENT denotes required NULL output/fields or incoherent NULL
 * scratch capacity; ROOT_TYPE a noncompound root; INVALID_TREE shallow missing
 * span/child/name or End child; INVALID_TYPE an unknown child type;
 * INVALID_LENGTH an overlong name; LIMIT pointer/count byte overflow;
 * BUFFER_TOO_SMALL insufficient scratch. No encoded byte offset is implied.
 */
mc_nbt_status mc_nbt_compound_overlay_count(const mc_nbt_tag *source, const mc_nbt_tag *fields,
                                            size_t *out, mc_nbt_error *error);
mc_nbt_status mc_nbt_compound_overlay(const mc_nbt_tag *source, const mc_nbt_tag *fields,
                                      const mc_nbt_tag **scratch, size_t scratch_count,
                                      mc_nbt_tag *out, mc_nbt_error *error);

#endif
