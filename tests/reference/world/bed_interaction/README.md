# Original Bed interaction components

This packet retains 85 prepared observations from the unchanged Beta 1.7.3 client
JAR, SHA256 `af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`.
Two fresh headless JDK17 interpreter VMs agree. A third unchanged VM agrees under
read-only JDI and records 408 selected calls with ordered entry/return ticks and
20 live Code fingerprints: 19 game methods and JDK `Random.nextFloat`. Exact
class/Code pins cover 24 original classes. Raw pipes, compiled observers, the JAR
and original resources remain private.

The component matrix is:

| Cases | Prepared call |
|---|---|
| 0–63 | `VE.b(World,III,neighborID)` for metadata 0–15, partner absent/present, local/remote |
| 64–79 | `VE.a(World,III,Player)` for metadata 0–15, healthy pair, local Overworld day |
| 80–83 | Activation from four foot orientations with an absent head |
| 84 | Remote activation with an absent partner and null Player |

The eight local orphan-foot cases remove themselves and publish a genuine Bed
Item. Orphan heads remove themselves without an Item; remote direct neighbor
calls still remove orphan halves. Healthy pairs survive. The sixteen local
daytime activations return actual `CW.c`, invoke the actual DC message child and
leave the observed Player and Inventory unchanged. Eight stale occupied heads
clear bit4 through the genuine notifying metadata setter before calling Player
sleep. Absent-head and remote activation return before those Player children.

The original provider probe separately observes Overworld true and Nether false.
It never invokes the Nether explosion branch. No nighttime sleep, wake, saved
spawn, Item355 placement, startup, drawing or loaded gameplay is established by
these rows.

## Prepared original graph and retained observations

The observer constructs genuine FD Worlds, Overworld providers, the normal Map
ChunkProvider and normal Chunks. It clears the one selected Chunk's Block and
metadata planes and writes only the declared Bed cells. It resets the actual
World Random to2468 immediately before each selected call. Original Block,
Chunk, mutation, neighbor, drop, Stack, ItemEntity and publication bodies execute
unchanged. The selected Item observation includes its exact Stack, position,
pickup delay, World occurrence and one actual Chunk occurrence. Constructor
Math/default-Random motion and rotation are outside these selected outputs.

The daytime receiver is a genuinely constructed, healthy, awake DC published
through the actual World insertion method. Its App and Font are explicit
prepared original objects allocated without their constructors; the Font has
authored widths7, space4. Original UQ, DC, NH translation and UQ chat insertion
bodies execute. The resulting original notice UTF16/age and Player constructor
snapshot remain in `finite.json`. This is a bounded prepared component graph,
not an original initialized client or a Font/rendering qualification.

The source SecurityManager rejects network, native-library, subprocess, file
write and delete attempts; every original completion reports zero in those
counters. JDI transport is established before the source-effect guard. The
debugger only reads method bytes, arguments, return values and the observer's
active-case watchpoint. It never invokes target methods, writes target fields,
redefines classes or replaces a target body. Source, tool, SDK and generated
helper hashes are unchanged across the capture. The portable recapture also
checks that all finite values equal the reviewed original packet.

`finite.json` retains the original rows, all408 selected calls and all20 live
Code records. The call journal includes genuine notifying setter recursion,
the four ordered World Random float draws for each drop, publication, stale
occupancy clearing, GS daytime return and the DC→UQ→NH→UQ message sequence.
Its ticks record actual entry/return nesting. The actual Player message key is
the UTF16 argument observed at DC.b; it is not inferred from the translated
English notice.

## Native World comparison boundary

`run_native.py` compares 85 World projections containing 2,569 scalar values.
It compares complete Block/metadata planes, selected Item fields and publication
occurrences, World Random state, activation results, observed Player preservation,
actual CW result identity and the reached Player/message-key boundary. The
native World test callback checks the actual Player/key/order and records that
boundary. Its ordinary callback return supplies this component seam.

Original notice contents, original Player constructor values, and the full JDI
journal remain retained and are excluded from native comparison. This packet
does not claim that the World callback implements or completes native DC/App/UQ,
translation, Font, chat insertion or drawing. The actual client message route
needs its separate integration evidence. Native constructor entropy is not
compared against original constructor snapshots; preservation is checked within
each actual call.

The native transport represents every nonzero Block ID **or** metadata nibble
as `[x,y,z,id,metadata]`, sorted by x,z,y. Air with nonzero metadata is retained.
The reader reconstructs all 32,768 Block bytes and 16,384 metadata bytes, checks
their SHA256 against the original input/output plane hashes, then compares the
lossless sparse cells. Expected output reconstruction uses the captured scalar
results and must reproduce the original full-plane hash. It cannot silently
ignore an extra changed cell.

The native executable receives `bed-interaction HEX_ROOT HEX_RESOURCES`, prints
85 ordered `BED_INTERACTION` JSON rows and a final `Bed interaction: N checks`.
The optional initializer lines are `16 achievements` and `151 recipes`. The
native runner creates authored text resources and uses no JVM or original assets.
Native owner/refusal/retirement controls in the C executable are separate from
these finite original observations. Its separate Registry/CW namespace joins
and checked cleanup controls do not establish original cross-loader equivalence.

## Reproduce and validate

`profiles.json` and `profiles.tsv` declare the fixed85-row input inventory:
case, kind (neighbor0/activate1), metadata, partner0/1, remote0/1. The original
observer and native driver both execute that fixed matrix; the readers verify
their actual output inventory against these declarations.

To reproduce the original packet with a matching JDK17 installation and the
pinned SDK dependency archives:

```text
python tests/reference/world/bed_interaction/capture_reference.py --client-jar /path/b1.7.3.jar --library-root /path/libraries --java /path/jdk17/bin/java --javac /path/jdk17/bin/javac --output /ignored/new-capture
```

The output directory must be new. It receives fresh guarded receipts, raw pipes,
generated helpers, `finite.json`, `provenance.json` and `capture.json`; the command
does not overwrite this checked-in packet or earlier captures. All game classes
must originate in the pinned original archive. The capture code uses the shared
bounded subprocess reader and classfile parser; no JVM is required for ordinary
packet validation or native comparison.

```text
python tests/reference/world/bed_interaction/test_packet.py
python -O tests/reference/world/bed_interaction/test_packet.py
python tests/reference/world/bed_interaction/run_native.py --executable /path/mc_test_world_bed_interaction --work-dir /ignored/results
```

Strict readers reject unknown/missing/duplicate keys, bool/int substitutions,
wrong extents/order, invalid raw words, altered class/Code pins, malformed call
intervals, missing source completions, bad provenance, truncated process receipts
and extra or changed plane cells. Corruption controls use explicit checks that
remain active under `python -O`; both modes reject 1,167 corruptions and validate
all 2,569 projection scalars.
