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

A normal metadata getter reads its packed array. Even y selects the low
nibble; odd y selects the high nibble. A quiet setter marks the selected
normal chunk dirty before accessing metadata, writes the supplied integer's
low four bits and preserves the other nibble. An equal write still marks
dirty and returns ordinary true. The actual blank chunk returns metadata
zero and accepts writes with ordinary true while retaining its payload and
dirty flag.

This quiet metadata operation performs no block handler, observer or neighbor
notification, height/light update, or lighting submission. [Block-ID setters
and notifying metadata](mutations.md) have separate synchronous behavior and
unfinished concrete descendants. Existing top-block lookup starts at y63 and scans upward; the
separate downward material query is also unfinished.

The [shared-registry material getter](registry.md) performs the actual block
query before choosing shared Material.a for air or reading Block.m/bA for
nonair. It is separate from that unfinished downward scan. Registry activation
and nullable material results retain their actual ordering and identities.

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

These are source observations. This addition supplies no new executed
original-World or original-Chunk comparison. Native cache validation rejects
a normal NULL data owner before selection; the later defensive owner guard
is not a reached native test scenario or Java exception-parity claim.

## Native validation

Six selected cases pass on Windows MSVC and Linux GCC/Clang, with Linux
ASan/UBSan and leak checks: `world.construction`, `world.game_construction`,
`world.normal_provider_construction`, `world.chunk_acquire`,
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
