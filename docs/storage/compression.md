# Gzip and zlib memory compression

`mc_storage` exposes [compression.h](../../include/mc/storage/compression/compression.h).
The module accepts borrowed byte spans and returns an owned buffer. Select gzip
or zlib explicitly; neither operation opens files, parses NBT, or closes streams.
All zlib state stays private. See [dependency setup](../dependencies.md) for the
pinned static C library and offline builds.

`mc_compression_encode` produces one checksummed stream using default deflate
level 6. Gzip has MTIME 0, OS 255, and no optional fields. Output is intended for
interoperable readers. Deflate blocks need not be byte-identical to a particular
Java runtime or another compressor.

`mc_compression_decode` validates a complete stream, including its checksum and
trailer. Gzip concatenates adjacent members whose first two bytes are `1f8b`;
every recognized member must be valid. It leaves nonmagic trailing bytes
unconsumed. Zlib stops after its first stream. Optional `consumed` reports the
exact compressed member-end offset, excluding that tail. It does not represent
transport read-ahead or the compressed position of an NBT root. Raw deflate and
preset dictionaries are unsupported.

Defaults are 64 MiB input, 64 MiB output, 128 MiB peak live allocation and 1024
members. Allocation accounting includes the module's allocation headers, zlib
state, output capacity and both old/new buffers during growth. System allocator
overhead is outside this accounting. Caller limits can be smaller; zero byte
limits are valid, while the member limit must be positive. Exact output limits
must still allow trailer validation without requiring a fictitious extra byte.
Input limits apply to the entire supplied span, including any tail.

On success, release the result with `mc_compression_buffer_free`. That function
accepts NULL and resets a non-NULL result to `{NULL, 0}`. On failure, the result
and optional consumed count remain unchanged. Release an earlier owned result
before reusing its result variable. The header documents disjoint storage
requirements. Statuses distinguish invalid arguments/streams, truncation,
resource limits, allocation failure and unexpected backend failures.

To read a compressed compound, decode with explicit limits, call
`mc_nbt_read_compound` on the resulting span, and then release the decompressed
buffer. A successful NBT document owns its retained data. Check compressed and
raw consumed counts separately if the caller requires exact file framing. A
valid container can contain invalid NBT, and a valid raw root can occur before
invalid compressed data.

The original helper reads one compound and closes its wrapper before necessarily
checking the gzip trailer. The C memory module validates all recognized members
and rejects malformed trailers that the original can leave unread. This is a
documented malformed-input departure, separate from valid save behavior. The
[reference notes](../reference/compression.md) record the observed cases and
modern-JDK scope.

This module does not implement region addressing, world schemas, atomic save
replacement, copied-world round trips or writable-world compatibility.
