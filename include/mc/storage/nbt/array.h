#ifndef MC_STORAGE_NBT_ARRAY_H
#define MC_STORAGE_NBT_ARRAY_H

#include "mc/storage/nbt/nbt.h"

typedef struct mc_nbt_byte_array mc_nbt_byte_array;

/* A concrete shared mutable byte-array identity. NULL is a null source array;
 * a successful zero-length construction is a distinct nonnull identity.
 * Create copies length bytes from source, or zero-initializes when source is
 * NULL. Length must fit INT32_MAX. The exact handle plus payload allocation is
 * bounded by max_allocation_bytes; no allocator overhead is charged. Failure
 * preserves out. Output and input storage must be disjoint and exclusive. */
mc_nbt_status mc_nbt_byte_array_create(size_t length, const uint8_t *source,
                                       size_t max_allocation_bytes, mc_nbt_byte_array **out);

/* Each successful construction/acquisition supplies one owned hold. Retain
 * preserves the SAME handle and fails unchanged on overflow; NULL is invalid.
 * Release accepts NULL. No synchronization or Java GC/OOME timing is implied. */
mc_nbt_status mc_nbt_byte_array_retain(mc_nbt_byte_array *array);
void mc_nbt_byte_array_release(mc_nbt_byte_array *array);

/* Borrow actual mutable bytes under a live hold. Keep exclusive access through
 * mutation: no overlapping const document/tag/tree reader or codec use may be
 * active. The descriptor, length and ownership cannot be changed. A decoded
 * empty array may have NULL data despite its nonnull handle. NULL accessors
 * return NULL/zero and do not turn a null handle into an allocated empty array.
 * allocation_bytes counts this handle's exact allocation: header plus payload
 * for standalone arrays, header only for document-backed arrays. The retained
 * shared document/arena is a separate dependency, not charged a second time. */
uint8_t *mc_nbt_byte_array_data(const mc_nbt_byte_array *array);
size_t mc_nbt_byte_array_length(const mc_nbt_byte_array *array);
size_t mc_nbt_byte_array_allocation_bytes(const mc_nbt_byte_array *array);

/* Acquire the actual reachable decoded ByteArray tag's SAME payload without a
 * copy. Foreign, copied, interior, NULL and non-ByteArray tags are invalid; the
 * supplied tag is compared with actual reachable tag pointers before it is
 * inspected. Repeated acquisition of the same tag returns the same handle
 * while held. Distinct empty tags have distinct handles even with NULL payloads.
 * A weak per-document cache owns no hold. Each live handle holds its supplying
 * document until final release, so caller document retirement cannot invalidate
 * it. The wrapper is charged to that document's residual allocation budget and
 * uncharged on final release. Failure preserves out and all existing holds.
 * Keep document/tag/out live, exclusive and disjoint throughout this call. */
mc_nbt_status mc_nbt_document_byte_array(mc_nbt_document *document, const mc_nbt_tag *tag,
                                         mc_nbt_byte_array **out);

#endif
