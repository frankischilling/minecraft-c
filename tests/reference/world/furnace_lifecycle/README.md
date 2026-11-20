# Original Furnace World lifecycle

This is a separate saved-World packet from the 207-case Furnace component
packet. Four fresh original-client processes agree on seven ordered snapshots:
two ordinary VMs and two passive-JDI VMs, each using interpreter/default mode.
Both debugger journals agree literally on 196 selected original entry/exit
phases, at most 60 in a step. All 23 entered live Code hashes equal the verified
unchanged Beta 1.7.3 client archive.

`fixture.py` authors four genuine saved Chunks and metadata before execution.
The unchanged `FD(MX,"",23)` loads those files and the actual Furnace NBT factory
constructs the original SK. The actual Chunk/cache publication supplies its
initial World binding, Map and World.c occurrence. No manual Tile Map/List
insertion or fake World implementation is used. Original Blocks initialize
before Items as an explicit startup precondition. A genuine circular-init
failure from loading fueled SK before that precondition remains retained in
failed capture-01; it is not relabeled or counted in the final cohort.

The fixture contains 61 at `(8,64,8)`, metadata3, ore15×2 and coal263×2. All
other Block/Data/Light/HeightMap bytes are zero. Four canonical Chunks open in
order `(0,0),(0,1),(1,0),(1,1)`. `setup.json` records the actual normal RH
provider, its XA primitive fields, OK cache, WorldInfo primitives and Chunk
flags. The prepared World is authoritative, seed23/time0, provider noSky=false;
all four Chunks are active, lowest-height0, TerrainPopulated=true and otherwise
unmodified flags. Use those recorded preconditions when claiming native replay.
Initial duration1600 is observed from real saved-NBT fuel recomputation, not
authored onto a newly constructed native SK with duration0.

The steps are initial snapshot, actual `fd.g()` ignition, warm `fd.g()`, a cook
boundary, exhaustion and two subsequent `fd.g()` calls. `cases.json` explicitly
authors cook199 before the boundary and burn1/fuel-null before exhaustion.
Those two inputs isolate the branches; this packet does not claim that 200 or
1,600 complete gameplay ticks elapsed. No other Tile field, List/Map, Random,
queue or Block array is repaired between calls. The audit checks exact state
continuity, allowing only these declared between-step inputs.

The original has a material ownership quirk:

- Ignition retains original T0 and its contents, restores Map identity T0 and
  metadata3, but leaves a second **valid**, empty SK T1 in World.c.
- The next warm call and smelt boundary retain both valid objects. The boundary
  consumes one ore and creates actual iron265×1.
- Exhaustion restores61 and retains a third valid empty SK T2 in World.c.
  Both later calls retain all three valid objects; the Map still contains T0.

The phase journal observes how this happens. During iteration, removal marks
T0 invalid. The factory creates a fresh SK, and World.a enqueues it in G before
TC revives/enqueues T0. Deferred publication adds the fresh SK to c, maps it,
then remaps T0. The Map put does not invalidate the temporary object. The fresh
SK's source World field is null while pending and is assigned only by LM.a.
These are actual observed phases, not counts/order calculated from bytecode.

`finite.json` retains every before/after Tile/Stack identity and raw integer
field, c/G order, each Chunk Map's values, Block/metadata, World.L and TC.c,
all twenty Chunk-array hashes and actual light queues. Detached Stack roots
remain observed. `traces.json` retains selected entry/exit events, actual
arguments, nearest selected source caller/PC and direct JDI field snapshots.
Method-entry phases include not-yet-initialized fresh objects. The observer
sets breakpoints only in its authored begin/end methods; it never invokes a
target method, stores a target field or sets an original-game breakpoint.

All four actual World Random seed/Gaussian streams are preserved separately in
`random.json`, with sequential before/after joins. Constructor entropy is not
normalized or asserted equal across processes. Targets record zero game
network, native-link, process and outside-write attempts. Only the private
eight-byte session lock may change on disk; other saved inputs are hash-identical.
The local debugger transport starts before the target guard and is distinct
from game networking.

`native-observations.h` copies all seven before/after projections plus declared
between-step inputs. It supplies expected data, not native acceptance. A native
post-step replay must compare real identities and retain probe roots for detached
Stacks, then release them before checked terminal cleanup. Matching this header
does not establish parity with the full 196-phase original trace. Actual Player,
Container/GUI, full outer World ticks, rendering, naturally elapsed cook/burn,
save round trips after mutations, removal drops, callback failures, sustained
play and FPS remain separate requirements.

Capture pins JAR, SDK/JDK/JVM/module files, helper/declaration inputs and compiled
observer products before/after. Publication executes no game code and finishes
all hash/status/limit/count checks and reparses both ordinary streams, both
debugger target streams and both journals/live-Code files before public writes.
It rederives archive Code identities and checks all four actual saved inventories.
Preparations remain separately retained by hash. Later publication helpers are
labeled postexecution readback/generation work.

```powershell
python -B tests/reference/world/furnace_lifecycle/capture.py --jar b1.7.3.jar --libraries "$env:APPDATA/.minecraft/libraries" --output .local/furnace-world-original-new
python -B tests/reference/world/furnace_lifecycle/publish.py --capture .local/furnace-world-original-new
python -B tests/reference/world/furnace_lifecycle/audit.py
python -B tests/reference/world/furnace_lifecycle/test_packet.py
```

Public controls require Python only. Synthetic transport/files and corruption
mutations test decoders; they are not additional original/native executions.
JAR/classes, bytecode bodies, javap output, game assets and helper binaries stay
ignored. Format a regenerated header with clang-format18, then republish to
preserve its matching token stream.
