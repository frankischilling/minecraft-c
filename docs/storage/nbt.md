# Raw NBT storage

[`tree.h`](../../include/mc/storage/nbt/tree.h) supplies independent immutable
source copies for owners such as [ItemStack](../item/stack.md). It preserves raw
payload bits and meaningful names without inspecting decoded-document layouts.
List-child names, which the format ignores, become empty. The allocation ceiling
covers either validation scratch or the new complete tree allocation; callers
deduct their other simultaneous live allocations before composing operations.

The `mc_storage` library exposes the original Beta tag format through
[`nbt.h`](../../include/mc/storage/nbt/nbt.h). It depends on `mc_core`, with no
graphics, audio, Java, assets, sockets, compression or file initialization.
The [original evidence](../reference/nbt.md) defines the
implemented codec scope. Complete world compatibility remains a separate task.

## Decode and ownership

`mc_nbt_read` reads one named tag. `mc_nbt_read_compound` additionally requires a
compound root, matching the original utility's separate validation boundary.
Both report consumed input bytes, so callers decide whether trailing data is
allowed. A successful document owns copies of every retained name, payload and
child and returns one caller hold. Input bytes can be released after decoding.
Keep that document hold while using borrowed tag views;
`mc_nbt_document_free` releases it.

[`array.h`](../../include/mc/storage/nbt/array.h) supplies shared mutable byte-array
handles. Acquiring a decoded handle checks the exact reachable ByteArray tag
pointer before inspecting it and retains its actual payload without copying.
Repeated acquisition returns the same handle while held. Different empty tags
have distinct nonnull handles even when their payload pointers are NULL; a
NULL handle represents a null array. Each live decoded handle keeps its
supplying document and payload alive after the caller releases the document.
The wrapper uses that document's remaining allocation budget and releases the
charge with its final hold.

Mutate bytes only under a live array hold with exclusive access. No const
reader or codec may use the same document, tags or bytes during mutation. Tag
descriptors, lengths and ownership stay unchanged. Immutable source trees from
`tree.h` retain their separate contract. [MapData](../world/saved-data.md#named-values-and-mapdata)
uses these shared handles for colors, including decoded arrays retained after
the caller releases its document hold.

```c
mc_nbt_document *document = NULL;
size_t consumed = 0;
mc_nbt_error error;
mc_nbt_status status = mc_nbt_read_compound(data, length, NULL, &document, &consumed, &error);
if (status != MC_NBT_OK) {
    /* Report mc_nbt_status_string(status) and error.offset. */
    return;
}
const mc_nbt_tag *root = mc_nbt_document_root(document);
const uint16_t level_name[] = {'L', 'e', 'v', 'e', 'l'};
const mc_nbt_tag *level = mc_nbt_compound_get(root, level_name, 5);
/* Inspect level if present; unknown application fields remain in the tree. */
(void)level;
mc_nbt_document_free(document);
```

Strings use explicit UTF-16 code units and lengths. Embedded NUL and lone
surrogates remain representable; they are not C strings or scalar Unicode text.
The [modified UTF-8 helper](../reference/mutf8.md) encodes payloads; NBT adds the
unsigned two-byte encoded-length prefix and its 65535-byte limit. Packet strings
have a different framing and must not use this NBT prefix.

Tag IDs 0 through 10 are supported. Later integer/long-array tags are rejected.
Unrecognized application field names using known types are retained with their
entire nested values. Duplicate compound names keep the last value, matching
the original map reader. No world schema is applied during parsing.

## Encode and validation

`mc_nbt_measure` validates an immutable borrowed tree and reports encoded size.
`mc_nbt_write` validates the complete tree before modifying the output buffer.
Caller-constructed trees are allowed; callers own their storage. Lists must be
homogeneous, with the declared element type matching every child. Compound
children must have distinct names and cannot be End tags. All result/count/error
storage must be mutually disjoint, including output buffers, and separate from
input bytes, limits and borrowed trees.

Normal serialization follows the original writer's behavior: empty lists emit
element type Byte (1), and floating NaNs emit the Java canonical bit pattern.
The parser retains input floating bits for inspection. Child names are omitted
inside lists. Compound children use the retained order; the codec does not
claim byte identity with a particular JVM's `HashMap` traversal. Compare compound
logical state separately from order, retaining complete wire output as evidence.
No field values, names, types, list order or array bytes may be normalized away.

## Bounds and failure behavior

Pass `NULL` limits for defaults or provide explicit `mc_nbt_limits`. Root depth
is 1; configured depth must be between 1 and 128. The defaults are 64 MiB input,
64 MiB output, 128 MiB cumulative document allocation, one million parsed value
tags, one million elements per container, and depth 64. The allocation budget
includes arena bookkeeping and replaced duplicate values. Compound End
sentinels are not value tags; physical compound entries still count toward the
element limit even when a name replaces an earlier value.

On failure, read document/count outputs and write buffer/count outputs remain
unchanged. The optional error reports a status and byte offset. All partial
document allocations are released, and freeing a null document is supported.
Limits are explicit resource restrictions, not a claim that every structurally
valid deep or large input is accepted.

Negative list lengths are rejected. The original reader treats them as an empty
loop; this is a documented malformed-input departure. Negative byte-array
lengths, invalid types, truncated data, malformed text and exhausted budgets
also return errors. Caller-constructed heterogeneous lists are rejected rather
than reproducing the original object's malformed serialization. These choices
do not change normal valid NBT data.

The codec performs no filesystem operations. [Compression](compression.md),
[region records](region.md), [World storage](../world/storage.md) and
[GAME saving](../world/saving.md) define the separate container, ownership and
write paths and their finite verification scopes. Complete World compatibility,
recovery and gameplay restart remain acceptance work.
