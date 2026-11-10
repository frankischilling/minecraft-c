# World block access and quiet metadata writes

`mc_world_runtime_block_get`, `mc_world_runtime_metadata_get` and
`mc_world_runtime_metadata_set_quiet` use the GAME World's actual acquisition
coordinator. Coordinates admit x in [-32000000,32000000), z in
[-32000000,32000000] and y in [0,128). Outside requests return zero or ordinary
false before chunk acquisition. Native status and the setter's ordinary
boolean result are separate outputs.

In-domain requests shift signed x/z by four and use their low four bits as
local coordinates. The selected identity comes from the actual packed-key
cache lookup, including aliases whose stored chunk coordinates differ from
the request. Missing generation or population resources fail explicitly and
retain completed acquisition effects. Native failure preserves the ordinary
output. These calls require exclusive access and the runtime's existing
lifetime and disjoint-output rules.

Every in-domain request checks the actual current provider receiver before
looking up a cached Chunk. A null World.v therefore fails even on a cache hit;
out-of-domain reads still return zero before that receiver access. The shared
block/metadata acquisition path preserves this order.

A normal metadata getter reads the Chunk's current `e` NibbleArray and its
current byte backing. Even y selects the low nibble; odd y selects the high
nibble. A quiet setter marks the selected normal Chunk dirty before reading
that same current `e`, writes the supplied integer's
low four bits and preserves the other nibble. An equal write still marks
dirty and returns ordinary true. The actual blank chunk returns metadata
zero and accepts writes with ordinary true while retaining its payload and
dirty flag.

The setter previously wrote the fixed stored metadata plane even when the
actual Chunk's `e` named another array. It now follows the current object and
checks only the selected byte index. A short backing is valid when that index
is in bounds. Null `e`, null backing and an out-of-bounds selected byte fail after the
dirty store; the ordinary Boolean remains unpublished. The internal source
diagnostic records World 70, Chunk 14 and the first backing read at 42 for even
Y or 70 for odd Y. Blank and out-of-domain calls skip those accesses.

This quiet metadata operation performs no block handler, WorldAccess or neighbor
notification, height/light update, or lighting submission. [Block-ID setters
and notifying metadata](mutations.md) have separate synchronous behavior and
unfinished concrete descendants. Existing top-block lookup starts at y63 and
scans upward; the separate [downward solid/liquid query](queries.md) acquires one
Chunk and scans Y127 through Y1.

The [shared-registry material getter](registry.md) performs the actual block
query before choosing shared Material.a for air or reading Block.m/bA for
nonair. It is separate from that downward scan. Registry activation
and nullable material results retain their actual ordering and identities.

The separate solid-support query captures the live Registry block array before
the acquiring getter. It then indexes that captured reference, returns false for
a NULL Block, calls material `h`, and conditionally invokes the selected Block's
support predicate. Slabs read their live subtype flag. This order differs from
the material getter and opacity lookup; see [fluid dependencies](fluids.md).

## Reference basis

The unchanged archives have SHA-256 client
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`
and server `033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d`.
The paired bytecode provides these entry points:

| Operation | Client | Server | Relevant bytecode |
| --- | --- | --- | --- |
| World metadata get | `fd.e(III)I` | `dj.c(III)I` | Bounds precede acquisition at PC48; chunk getter at PC68 |
| World quiet metadata set | `fd.e(IIII)Z` | `dj.d(IIII)Z` | Acquisition at PC48; chunk setter at PC70; literal true at PC73 |
| Normal Chunk metadata get | `lm.b(III)I` | `hi.b(III)I` | Reads actual metadata owner `e` |
| Normal Chunk metadata set | `lm.b(IIII)V` | `hi.b(IIII)V` | Dirty assignment at PC2; packed write at PC14 |
| Blank Chunk overrides | `li.b(III)I`, `li.b(IIII)V` | `hg.b(III)I`, `hg.b(IIII)V` | Getter returns zero; setter returns without mutation |

The earlier qualification used static source observations and native controls.
The [quiet-setter collector](../../tests/reference/world/metadata_set/README.md)
now executes fifteen unchanged-original client calls through the real World,
provider and cached normal/Blank Chunks. Native records match typed Boolean or
exception outcomes, dirty prefixes, nullable backing state, selected bytes,
unchanged fixed/unselected bytes, cache membership and World Random progress.
Replacement even/odd writes retain the same array across both calls. This
finite client comparison does not qualify server mutations or complete World
notification behavior. Native cache validation rejects
a normal NULL data owner before selection; the later defensive owner guard
is not a reached native test scenario or Java exception-parity claim.

## Explicit execution callers

The private `blocks_execution_internal.h` entries execute these same scalar
bodies with a stable caller frame and an explicitly supplied execution/code
bundle. The caller captures its evaluated World before later arguments. These
entries neither begin nor close that scope. Only the reached acquisition uses
the existing owned kernel; successful domain bypasses do not acquire a Chunk.

The returned continuation Boolean is separate from raw storage status. Native
completion can refuse after an actual successful GET. That path retains the
real pending attempt and stops before reading scalar payload, setting dirty,
writing a nibble or publishing an output. Native-only drain never resumes the
source body. Re-entry preserves the original selection, pending attempts and
close capsules. Metadata Source faults retain their separate typed sites.

These entries supply scalar descendants for retained callers. Cold import,
generation/population and foreign provider/Store graphs retain their explicit
external lifetime contract. Complete notifying mutation, Player wake and Arrow
impact composition remain separate joins.

## Native validation

Six selected cases pass on Windows MSVC and Linux GCC/Clang, with Linux
ASan/UBSan and leak checks: `world.construction`, `world.game_construction`,
`world.overworld_provider_construction`, `world.chunk_acquire`,
`world.chunk_nibbles` and `world.stored_chunk`. The GAME case now reports
4,397 checks, up from 4,318.

The added scenarios use real store-owned chunks and the provider's same blank
identity. They cover all six rejected boundaries, admitted asymmetric edges,
negative packed-key aliases, full-int masking, even/odd/equal writes,
preserved nonmetadata arrays/state, failed acquisition outputs and retained
pending removal. An in-memory typed NBT build/write/decode round trip retains
every edited metadata byte. This is separate from file saving, full registry
bootstrap, population callbacks and complete persistence acceptance.

See [World construction](construction.md), [packed chunk data](chunk.md) and
[test reproduction](../testing.md).
