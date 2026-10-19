# Chunk arrays, heightmaps and direct skylight

The [C interface](../world/chunk.md) covers local voxel data. Whole chunk
lifecycle, neighbor-height queries, lighting queues and propagation have
separate acceptance requirements.

## Original bindings

See [activation and detach](#activation-and-detach) for ordinary GAME
acquisition's concrete known-empty World collection operations.

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

## Concrete constructor and finalizer

The same verified archives also define the concrete owner now represented by
the [generated Chunk interface](../world/chunk-objects.md). The base constructor
`lm.<init>(Lfd;II)V` / `hi.<init>(Ldj;II)V` publishes its fresh tile Map at12,
List[8] at21, flags n/o/q and time r at26/31/36/41, actual World d at46, final
coordinates j/k at51/56, and zero height byte[256] at65. Eight distinct empty
ArrayLists are constructed91 and stored94 in order. The byte-array overload
calls that base constructor5, stores the supplied blocks10 and distinct Nibble
owners e/f/g at23/36/49. Each Nibble creates its zero packed array8 and stores
its final a10. The native combined-backing allocation policy is separate from
this logical field order and from Java allocation-failure timing.

Full finishing captures the actual static q at39/113 before block bytes53/125;
element reads occur58/130. Height h80 precedes current World.t/Dimension.e97.
Wrapped subtraction131 precedes conditional Nibble setter149. Minimum i179
precedes neighbor helper201 and final dirty218. Its column helper calls the
four interval helpers at37/48/59/70. Each interval reads World height6; only a
difference reaches Sky21 or52, queue31 or62, then dirty36 or67. These are paired
bytecode findings, without a new original runtime observation. Existing actual
finishing captures below remain qualified for their own recorded contexts.

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

## Recorded acceptance

Runtime and reference source `67c928db7c0d87e599f16228833b0234fba4bc24`
passes all 30 configured CTests on native Windows/MSVC and WSL GCC13/Clang18,
without skips. Each build runs 27 public checks, the standard Java numerical
oracle and both chunk reference comparisons. GCC and Clang use ASan/UBSan.
Fresh original comparisons use Temurin17 and the pinned supplied JARs.

All five chunk cases match independently in C/client, C/server and client/server
comparisons. They cover air-to-stone, stone-to-air, mixed columns with a roof edit,
and two frozen natural post-cave buffers with roof edits. Each participant retains
four complete local snapshots around the constructor and two finishing calls.
The nibble comparison checks both 32768-value getter sweeps, all 16 setter calls
and every retained packed byte in each of its three pairings.

The public chunk fixture comes from the successful original-only capture at
`.local/reference/chunk-data-budget-audit/manifest.json`, SHA-256
`204e2e2da796ac96b4d47b3ee8326900c21cdc956564d8c98eb11833021f7220`.
Its historical source fingerprints and paired record hashes match the fixture
header. A separate byte-expansion audit decoded all 18 buffers from 16509 RLE
runs and matched 800 complete arrays, 160 minimum heights and 160 complete
opacity tables across that capture and the three fresh original runs. This
audit computes no height or lighting rule.

The nibble fixture's original manifest SHA-256 is
`874e6cfb8c4ceb319d0547ca225b87e1db55165a9e1f6162fbdbd642347557a7`.
Its manifest and both complete record hashes match the header. Expanding all
15 captured byte deltas reproduces every packed snapshot on both sides: 32
complete setter snapshots, four full getter sweeps and all 96 sampled getter
values match the fixture's attribution and values.

All six fresh manifests pass with empty error/unavailable lists. A separate
audit verifies 660 recorded source, input, executable, runtime, class, world-file
and artifact fingerprints. The recorded runtime and reference sources also
match the tested source snapshot.

| Local build | Comparison | Manifest SHA-256 |
| --- | --- | --- |
| `build/current-windows` | Chunk data | `5d2ab4c0c7f9b286ad2b0815e2a99ca772894e13a6bd8953cfc68ab1740cfd7f` |
| `build/current-windows` | Nibbles | `c1ceec1dd48cadf86ea42655d94e136f5edf8caba39679622d1a6fd079e0b801` |
| `build/current-gcc` | Chunk data | `10c2ef202d647942e507cadeaa225e73f5777671ff86abb9d025ba92546299c0` |
| `build/current-gcc` | Nibbles | `95fc94b426d7f2e152e0075ef854c0bb09639a5ac4499862173d64fc27bf559b` |
| `build/current-clang` | Chunk data | `858419227ffb6e725f994a421df43244ec79c483032f6cba3ee1e262ecd00f65` |
| `build/current-clang` | Nibbles | `7dcb821b42e29cb9b1e4541fc0e56fb7ca893cee0923ab647e683e58df3653ad` |

Chunk manifests are retained beneath `reference-chunk-data`, including a copy
inside each owned capture directory. Nibble manifests live inside owned
`reference-nibbles/capture-*` directories. Select historical reports by source
and manifest hash; a later run can replace the top-level chunk manifest.

These results establish the declared local arrays and packed access. Genuine
world cache/queue observations remain diagnostic evidence for later work.
They do not establish C world-height queries, queued-light behavior, propagation,
complete chunk generation, other dimensions, persistence or playable operation.

## Activation and detach

The same pinned client/server binaries establish these paired methods:

| Operation | Client | Server | Ordered bytecode |
| --- | --- | --- | --- |
| Preactivation | `lm.d()V` | `hi.c()V` | Return0 |
| Activation | `lm.e()V` | `hi.d()V` | active2; d6; l10/values13; World Collection18; each current m slice41/World List42; return51 |
| World tile registration | `fd.a(Collection)V` | `dj.a(Collection)V` | L1; true selects G8/addAll12, false selects c22/addAll26; return32 |
| World Entity registration | `fd.a(List)V` | `dj.a(List)V` | b1/addAll5; incoming size15; per-Entity hook34 only for nonempty input |
| Detach | `lm.f()V` | `hi.e()V` | inactive2; l6/values9; iterator14/hasNext21; Tile invalidation40 only for nonempty input; each current slice66/World List67; return76 |
| World Entity deferral | `fd.b(List)V` | `dj.b(List)V` | D1/addAll5; return11 |

The base cache invokes generation before map95/list107 publication,
preactivation120, activation125 and population gates128 onward. Registration
does not precede active or cache publication. Detach does not remove objects
from World b, clear Chunk collections or save/unpublish the Chunk itself.
Client Tile invalidation is `ow.i()V`; the paired server target is `jh.h()V`.
Their primitive h=true store is unreached for the known-empty domain.

The region decoder invokes `to.a(World,Level)` / `mg.a(World,Level)`, which
reads Level xPos3/zPos10 and calls the base Chunk constructor21 before
assigning Blocks34, Data52, SkyLight70, BlockLight88, HeightMap99 and
population110. The constructor creates actual d, Map and eight List identities.
Region relocation writes xPos246/zPos254 to the outer root before decoding
root.Level again265; this does not rewrite Level coordinates. The final
`lm.i()` / `hi.h()` normalization uses the class-shared first-use Block map.
The C [loaded binding](../world/chunk-activation.md) retains its existing atomic,
fixed-array decoder and explicit frozen-map policy; it does not claim the
source's allocation failures, missing-array handling or nonempty factories.

These are paired bytecode findings. Empty collection call order is reviewed
against that source; final empty lists alone cannot measure the number of
calls, Java Values identities, allocation or modCount. No new executed-original
Chunk activation or detach comparison is claimed.

## Local reproduction

Build the committed C source with both local JAR paths and the full commit ID
recorded in `MC_REFERENCE_TESTED_COMMIT`. Enable
`MC_ENABLE_NIBBLES_REFERENCE_TESTS` and `MC_ENABLE_CHUNK_REFERENCE_TESTS`.
Both require JDK11+ with `java` and `javac` from the same installation. Set
`Java_JAVA_EXECUTABLE` and `Java_JAVAC_EXECUTABLE` explicitly when several JDKs
are installed. Use the native paths for the process running CMake; Windows
paths and WSL paths are different.

After configuring and building, run:

```sh
ctest --test-dir build -R '^reference\.(nibbles|chunk_data)$' --output-on-failure
```

Add `-C Debug` for a Visual Studio build. Nibble comparisons need only the
committed sources, probe, JDK and supplied JARs. The full chunk corpus also
uses the earlier frozen original cave capture under
`.local/reference/caves-setup/capture-ea3e2caa3e134ed998a7394406a633e5/`.
Its manifest SHA-256 is
`bab5f6aad4890ba7bbdcc74384f9274a60855bcbd47230199e2b6188e79d14f8`.
Keep its manifest and paired `client-case-002.json`, `server-case-002.json`,
`client-case-006.json` and `server-case-006.json` together. The driver validates
their full fingerprints and exact 32768-byte post-cave prefixes. Its
`--natural-root` option accepts another location containing those same files.
Missing frozen inputs produce an unavailable result. They are not downloaded
by a build or supplied by public CI.

`scripts/reference/chunk/compare_chunk.py --capture-only` records fresh original
observations without running C. `--smoke` limits the corpus to air-to-stone and
does not need the frozen cave capture; a smoke result does not satisfy the full
five-case corpus. Reports and complete output records live under a newly owned
capture directory beneath the requested work directory. Fresh original world
roots remain under `.local/reference/chunk-data-setup/`.

The original server constructor initializes `dj.r` with `Random()` at bytecode
offsets 169 through 176, and spawn selection `dj.c()V` uses that generator.
A fixed world seed therefore does not fix the setup cache or pending-light
queue. Setup is recorded before the measured constructor. The observer admits
up to 512 cached entries and 16384 setup light regions; it then allows at most
1024 additional regions per finishing call, measured from the preceding actual
queue size. The intervening raw block edit must preserve the complete queue.
No queue is drained, truncated or replaced to fit those limits.

Each JVM has a 512 MiB heap limit and a 30-second process limit. A complete
UTF-8 JSON record, including its final newline, is bounded to 32 MiB. Aggregate
capture artifacts are bounded to 512 MiB. Setup beyond the observation budget
is unavailable; an over-budget measured stage or completed mismatch fails.
The parser continues to reject missing terminators, duplicate records and
incomplete arrays. Bounds are independent of exact local-byte comparisons.

The driver rejects renamed entry points and injected Java runtime, compiler or
classpath options before creating output. It records the canonical driver and
the exact bytes loaded for shared helpers. The commit field is a caller-supplied
label; acceptance also checks source and executable fingerprints against the
tested checkout.
