# Original Furnace components

This packet records 207 declared calls against genuine unchanged Beta 1.7.3
client objects: SK, IZ, IX, EY, NU/SP and BM/GP/VQ. Two ordinary fresh VMs
(interpreter and default) and two separate passive-JDI fresh VMs agree on every
logical outcome. Both journals also agree literally on 2,754 selected original
method entries, with at most 161 entries per case. Their 74 entered live Code
hashes equal the verified original archive. These are runtime entry/order and
typed-argument observations, not traces reconstructed from bytecode.

`declare.py` produces only `cases.json` inputs. Before each invocation the helper
constructs genuine SK/IZ/IX objects and authors the declared three SK cells,
36 Player-inventory cells, integer fields and coordinates. Equal labels mean the
same actual Stack. A/B are supplied objects; N labels identify new objects in
observation order. Snapshots retain detached supplied objects and returned
clones. `tracked_stacks` in the native header includes every observed object,
including returned clones; join `result_text` to its identity label for the
actual result payload. Animation fields are retained separately.

The World and IX.Player references are explicitly null. No concrete Player,
saved World, Tile Map/List, TC replacement, ordinary tick completion, GUI,
network, original pixels, sustained play or FPS is qualified. Blocks initialize
before Items; the original ten-entry EY catalog is warmed and snapshotted
before the cases. Its fields remain unchanged after the cohort. Cold class
initialization and recipe-bootstrap failure prefixes require separate evidence.
All registry/catalog objects retain original implementations; no original
class is rewritten or replaced, and no decompiler implementation is compiled.

Cases cover:

- Actual constructor, size/name/limit, signed indexing, nullable physical array,
  set clamp without JS dirty behavior, whole/split removals, negative/zero
  counts and amounts, and shared aliases. The actual set-clamp path calls SK.d
  twice. Set/remove have no OW.y_ entry; BM's later GP changed path is distinct.
- Cook/burn scale multiplication overflow, signed Java division, burning, and
  the real `b=200` store when the duration was zero.
- All fuel selector branches: wood Blocks, nonwood Blocks/Items, stick, coal
  including charcoal metadata, lava bucket, sapling, null, negative IDs and
  registered-table holes. Invalid IDs retain the genuine exception and prefix.
- All ten vanilla smelts, output identity/damage/capacity checks, negative input
  and output counts, existing versus cloned output, and input/fuel aliases.
- Actual OW/SK/IZ NBT read/write, short/byte narrowing, ignored and duplicate
  slots, fresh Stack construction, invalid-fuel and wrong-tag partial prefixes,
  alias loss on round trip, and duration recomputation. Empty fuel can preserve
  positive remaining burn while recomputing duration to zero.
- Three deliberate `n_()` calls with null World: positive burn decrements
  before the actual null dereference. These are failures, not completed ticks.
- 27 BM cases: actual 39-Slot layout, input/fuel to Player, main/hotbar routing,
  no-change null return, aliases, progress stores and empty-list detection.
  Changed output transfers with IX.Player null move/clear real Stacks before
  VQ throws at its Player World-field access. These failures remain literal;
  no successful pickup, stat, achievement or concrete-Player claim is made.

`finite.json` contains raw returns/exceptions and before/after fields. Original
NBT wire bytes remain private by size/SHA-256; an independent Python NBT reader
publishes exact tag widths and values. `traces.json` records selected original
method entries, typed arguments/receivers, per-case identities and the nearest
selected original parent method/PC. It does not record method exits or every
heap access. Breakpoints exist only at authored begin/end boundaries. The
debugger never invokes target methods, stores target fields or sets an original
game breakpoint. All four targets report zero network, native-link, process and
write attempts. The local JDWP debugger transport is separate from game network
operations and starts before the target guard.

`scan_callers.py` scans all 747 supplied class constant pools for DW/BM.a()V
references. Only BM and DW are candidates. Original javap confirms BM.a()V:1
invokespecial DW.a()V and DW.a(Llw;)V:1 invokevirtual DW.a()V. This archive
inventory is source evidence, not a runtime scheduling claim. It supplies no
ordinary Player-tick detection call. OV's shared-field drawing and actual
gameplay scheduling require their own qualification.

`native-observations.h` copies all 207 outcomes and arguments; the separate
`mc_furnace_bm_references` pointer array indexes all 27 BM rows. Generation does
not implement a Furnace predicate, fuel rule or transfer algorithm. The table
includes unsupported/native-policy input shapes explicitly; native acceptance
must identify which real API inputs it replays. Array-null cases are authored
Java object-state inputs, not a claim that native public APIs expose that state.

Capture uses JDK17, verification and bounded complete pipe capture. Original
JAR, SDK/JDK/JVM/module files, declarations, helpers and compiled observer
products are pinned before and after execution. Publication executes no game
code. Before any public write it rechecks retained input/product hashes,
complete child status/limits/counts/hashes, reparses both ordinary and both
debugger target streams and both journals/live-Code files, compares every
reported value, and rederives archive Code and caller inventories. Earlier
ordinary-only preparation remains retained separately and is not counted in
the final four-target cohort. Later publication-helper changes are labeled as
postexecution readback/generation work.

```powershell
python -B tests/reference/inventory/furnace/declare.py
python -B tests/reference/inventory/furnace/capture.py --jar b1.7.3.jar --libraries "$env:APPDATA/.minecraft/libraries" --output .local/furnace-original-new
python -B tests/reference/inventory/furnace/publish.py --capture .local/furnace-original-new
python -B tests/reference/inventory/furnace/audit.py
python -B tests/reference/inventory/furnace/test_packet.py
```

Format newly generated headers with clang-format18 and republish; matching C
token streams preserve formatter output. Public controls need Python only and
use published original values as decoder inputs. Synthetic transport mutations
and synthetic files prove rejection behavior, not additional original or native
executions. Original JAR/classes, javap output, binaries and assets stay ignored.
