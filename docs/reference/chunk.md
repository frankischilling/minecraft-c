# Chunk arrays, heightmaps and direct skylight

The [C interface](../world/chunk.md) covers local voxel data. Whole chunk
lifecycle, neighbor-height queries, lighting queues and propagation have
separate acceptance requirements.

## Original bindings

Both supplied references use the hashes recorded in the
[binary audit](binaries.md): client SHA-256
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`
and server SHA-256
`033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d`.
Class names belong to their respective archives.

| Behavior | Client | Server | Relevant bytecode |
| --- | --- | --- | --- |
| Fresh normal block-array constructor | `lm.<init>(Lfd;[BII)V` | `hi.<init>(Ldj;[BII)V` | Retains the supplied blocks and creates three independent packed arrays |
| Packed constructor | `wi.<init>(I)V` | `ob.<init>(I)V` | Allocates the packed byte array for the supplied cell count |
| Packed getter | `wi.a(III)I` | `ob.a(III)I` | Offsets 0 through 53, X/Z/Y indexing and low/high nibble selection |
| Packed setter | `wi.a(IIII)V` | `ob.a(IIII)V` | Offsets 0 through 84, low-four-bit replacement and opposite-nibble preservation |
| Local heights and direct sky | `lm.c()V` | `hi.b()V` | Offsets 0 through 179 |
| Remaining finishing effects | Same method | Same method | Offsets 182 through 221, world-height checks, queued sky intervals and dirty state |
| Initialized opacity table | `uu.q:[I` | `na.q:[I` | Actual registry initialization, retained before and after measurement |

The normal generators invoke these finishing methods after the cave stage.
Fresh normal arrays hold 32768 blocks, three packed arrays of 16384 bytes
each, and 256 height bytes. The world/data distinction matters: the newly
generated chunk is not yet inserted into the normal cache during finishing.
Replacing every world-height query with the fresh local heightmap would
change the original behavior.

The local search excludes Y127 when computing heights. Direct sky begins
at Y127, subtracts the actual opacity without an invented minimum loss,
stores only positive light and never writes Y0. Positions skipped by a later
pass retain prior sky values. Blocks, metadata and block light are preserved
by this local stage.

## Opacity input authority

The existing paired original-only opacity capture is retained locally at
`.local/reference/chunk-opacity/capture-98a9ced45f364452bea700dc75fa9751/`.
Its manifest SHA-256 is
`f30101b344d5352d92f060e078691854df7bb8f22b099a32498a69d4de08e33b`.
Both full 256-entry tables have big-endian signed-integer SHA-256
`f15de154e2937ae549a09e588e04ae8b4e5c21a99b3649cbf45aa2e067fe63ce`.
The capture records 96 registered block IDs and table counts of 201 zeros,
two ones, three threes and 50 values of 255.

These values are captured inputs. They do not establish height/skylight
outputs, block-registry completeness or gameplay behavior. The C component
receives its opacity table explicitly and contains no hidden vanilla registry.

## Verification boundary

Original comparisons must execute the untouched constructors and methods.
Complete measured blocks, metadata, sky, block light and heights are retained;
comparing a checksum alone is not the local-array acceptance rule. C/client,
C/server and client/server comparisons are separate.

Full finishing observations use genuine independently owned original world
contexts. The actual server constructor may generate chunks while selecting
spawn before the measured chunk exists. Cache membership, retained objects and
complete queued-light region bounds remain separate diagnostic records.
Each observation owns its world directory and leaves lighting undrained.

Public C tests cover explicit ownership and invalid-argument behavior,
including unchanged outputs, capacity suffixes and defined extreme-opacity
arithmetic. Those C contract cases are distinct from original gameplay claims.
Current exact-source results belong in the [development status](../status.md)
and [parity matrix](../parity.md).
