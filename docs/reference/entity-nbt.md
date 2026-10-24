# Item Entity NBT evidence

The [native contract](../entity/nbt.md) follows the paired client/server base
Entity and ItemEntity methods. The authority is the untouched archives from the
[binary audit](binaries.md): client SHA-256
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`
and server SHA-256
`033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d`.

## Paired source anchors

| Operation | Client | Server | Reached order |
| --- | --- | --- | --- |
| Boolean save | `sn.c(nu)` | `lq.c(iq)` | Name1, dead6, id22, virtual base writer27 |
| Base write | `sn.d(nu)` | `lq.d(iq)` | Pos37, Motion71, Rotation98, Float108, Shorts119/130, bool140, Item hook145 |
| Base read | `sn.e(nu)` | `lq.e(iq)` | Lists3/10/17, motion stores, clamps, position/rotation stores, scalars, same bounds294, float remainder306, Item hook311 |
| Item write | `hl.b(nu)` | `ez.b(iq)` | Health byte-to-short9, Age20, captured Stack writer37, Item40 |
| Item read | `hl.a(nu)` | `ez.a(iq)` | Health unsigned mask11, Age21, Item getter27, Stack constructor37, reference replacement40 |
| NBT factory | `jc.a(nu,fd)` | `ga.a(iq,dj)` | id8, Class lookup11, world-only constructor45, semantic read66 outside catch |
| Class namespace | `jc` initializer | `ga` initializer | Four Map publications7/17/27/37, then 24 ordered registrations |
| World activation batch | `fd.a(List)` | `dj.a(List)` | Whole b.addAll5, live size15/get26, observer34 |
| World deferral batch | `fd.b(List)` | `dj.b(List)` | Whole D.addAll5, return11 |

Client `aj` and server `ag` are final name wrappers. Their static namespace call
at1 initializes EntityList before the called Class-to-name Map lookup7 can run.
That active use precedes the Boolean save's dead test. Registering a Class
literal does not construct or initialize the represented Entity subtype.

The factory's Java Exception handler covers constructor lookup/invocation,
ending before the semantic read at66. Unknown names and failed reflection use
the separate diagnostic path with a second id getter90 and println99. Native
unknown-name output has the limited default ASCII-stream contract described by
the API; arbitrary reflection exceptions and stack printing are unfinished.

## Native verification boundary

Constructed tests inspect actual Entity/Stack/World/Class owners. Source scalar
fixtures and deliberately wrong immutable input tags are labelled controls;
they are not natural gameplay observations. No native test count proves a full
Entity hierarchy, ticking, original Random-construction equivalence or complete
world persistence.

The leaf tests check signed narrowing, the widened saved-Y expression, ordered
partial reads, missing Item, old/new Stack identity, same bounds identity and
motion/rotation edge representations. Namespace tests exercise actual partial
Map allocations and cached first failure without initializing target Entities.
Class tests join all 13 Biome logical handles to the existing canonical cells,
exercise both WorldClasses binding orders and verify failed-join hold rollback.
Batch tests distinguish whole-addAll from interleaved single insertion through
a genuine late WorldAccess failure that retains all three appended occurrences.

The source methods do not preserve unknown Entity/Stack tags in a fresh record.
The native semantic writer follows that known-field schema. Separate retained
Store provenance does not supply missing live fields or subclass behavior.
Mutable supplied-compound partial writes, Java object/HashMap identity, Java
heap/OOME timing, full PrintStream state and original byte-order equivalence
remain outside these independent-result APIs.

## Captured original calls

The project-owned [observer](../../tests/reference/entity/EntityNbtObservation.java)
loads each untouched archive through an isolated loader and invokes the real
Boolean save wrapper, world-only Item constructor, NBT factory and semantic
read/write methods. The twenty prepared cases include saved narrowing, dead
save, factory calls, missing Item, motion boundaries/NaN/infinities, short or
wrong Lists, late wrong scalar tags, NULL compound and missing scalars. Prepared
scalar values are explicit controls. No constructor bypass, ID reset, Random
rewrite, static/final field replacement or native-generated expected state is
used.

Both original sides match native MSVC, GCC and Clang probes for all 2,422
comparison rows per side. Thirty-eight before/after snapshots contribute 1,976
rows over 52 selected fields. Other rows cover results, selected owner
relationships and semantic NBT tags. Raw numeric bits, including NaN bits and
signed zero, compare exactly. The controller independently decodes each actual
saved byte stream; only Compound tag order is sorted for semantic comparison.
Original and native serialization byte order is not equated. Complete original
Throwables and native diagnostic frames are retained separately; their text
and representations are not equated.

The local [capture controller](../../scripts/reference/entity/capture_nbt.py)
pins both archives, the observer and compiled class, native executable and
selected sources, and the recorded Windows Temurin 17.0.17+10 JDK profile. Each
of its five children has bounded output, a deadline, empty standard input and a
retained exit/reap receipt. Original calls use the interpreter with a bounded
heap. Linux native comparisons use actual GCC/Clang binaries under WSL while
the originals use that same pinned Windows JDK; wrapper receipts do not prove
complete backend process discovery. These observations do not generalize to
arbitrary JDKs or original-era VM scheduling.

With the supplied archives and that JDK installed, build `mc_probe_entity_nbt`
and run:

```powershell
python scripts/reference/entity/capture_nbt.py `
  --java '<jdk>/bin/java.exe' --javac '<jdk>/bin/javac.exe' `
  --client-jar '<reference>/b1.7.3.jar' `
  --server-jar '<reference>/b1.7.3_server.jar' `
  --native-probe '<build>/mc_probe_entity_nbt.exe' `
  --output-dir '<fresh ignored capture directory>'
```

Keep this raw capture private. The reviewed
[exporter](../../scripts/reference/entity/export_nbt.py) validates the complete
retained capture and exports both complete original stdout streams as bounded
zlib/base64 data in [the public fixture](../../tests/reference/entity/nbt_fixtures.json).
The fixture preserves all original rows, including raw saved bytes and encoded
Throwable text; it contains no archive, asset, private command, environment or
local path. Its public provenance records content hashes and scoped child
summaries. It deliberately has no tested-commit claim: the fixture records
original observations, while each native regression records its own inputs.

`entity.nbt_observation` validates the immutable complete original byte hashes,
bounded decompression and provenance, then compares fresh native runs to every
comparison row. Seven separate malformed-fixture/comparator controls check
truncation, trailing data, base64, byte pins, scope, provenance and a changed
native scalar. They are labelled controls, not original/native executions.
The regression needs no JAR or JDK and runs in public CI.

Private Random state, global profile state, hover, most remaining Entity fields,
ordinary World/Chunk ownership and disk reload are outside this observation.
