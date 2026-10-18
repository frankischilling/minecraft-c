# Owned region image writer

[`writer.h`](../../include/mc/storage/region/writer.h) owns one mutable McRegion
image. The existing [read-only reader](region.md) remains an immutable diagnostic
view. The writer does not open files, compress data, read clocks or apply NBT.

## Allocation and bytes

Fresh creation produces two zeroed 4096-byte header sectors and 8192 bytes of
unconsumed growth. Import copies an existing aligned image with growth zero.
Each write accepts local coordinates in 0..31, opaque zlib bytes and explicit
32-bit timestamp bits. It writes selector 2 and the payload length plus one.

Required sectors are `(compressed_length + 5) / 4096 + 1`. An exact sector
boundary still allocates the extra sector. The maximum accepted compressed
length is 1,044,474 bytes; requests needing 256 sectors fail distinctly.

Same-size records keep their run. Changed-size records release the old run
before first-fit search, so that run can be reused. If no complete free run
fits, the writer appends the entire required run; it does not combine a short
free tail with growth. Files never shrink. New sectors are zeroed, while reused
suffixes, abandoned payloads and unrelated records retain their exact bytes.
Growth can be consumed and reset independently of timestamps or world time.

## Ownership and bounded failures

The opaque owner must not be copied. Borrowed image spans expire after a
successful write or free. A failed write retains bytes, allocation state,
growth, capacity and span addresses. Payload input must be disjoint from the
owner's storage. All operations require exclusive access.

Import rejects incomplete headers, unaligned files and unsafe allocation
tables: header-sector references, zero counts, out-of-bounds extents and
overlaps. Invalid payload lengths or compression selectors are retained for
the separate reader to diagnose. Original malformed-file constructor writes
are not reproduced.

Defaults are 64 MiB of image and 128 MiB of peak writer allocation. The latter
includes the owner, image/free-map capacities and old/new buffers during
growth. Images cannot exceed `0x7ffff000`, keeping sector operations inside the
originals' nonoverflowing signed file-length domain.

All memory is reserved before mutation. This atomic in-memory failure policy
does not reproduce partial original filesystem writes or establish durability.
[Native region transport](../world/storage.md) owns those separate operations.

## Original comparison scope

Untouched client `qj` and server `jx` supply the allocation observations;
`rj`/`kq` supply global region routing and cache observations. Public fixtures
retain four complete fresh/import replay profiles with raw compressed inputs,
all expected image bytes and growth frames. Original timestamps become explicit
C clock inputs after their raw values pass independently recorded clock bounds.
No other image bytes are normalized.

`storage.region_writer` checks constructed contracts. The two
`tools.region_write_*` tests validate provenance/guards and replay all profiles
without Java. `reference.region_write` performs fresh original comparisons
when `MC_ENABLE_STORAGE_REFERENCE_TESTS` is enabled. The image comparison does
not establish C filesystem or SoftReference behavior.
