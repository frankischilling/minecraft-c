# Chest placement predicates

This packet observes unchanged original `e.a(FD,III)Z` and private
`e.h(FD,III)Z` on a genuine saved `FD` World and four canonical `LM` Chunks.
It records 55 public predicate calls and nine separate private-helper calls.
Two ordinary fresh VMs and two read-only JDI fresh VMs agree on every logical
row: each pair runs once with the interpreter and once with the default VM.

`cases.json` freezes arguments and sparse Chest cells before execution. Public
cases include all sixteen horizontal neighbor masks, twelve extensions of
existing double Chests, occupied targets, four-Chunk boundaries, height and
signed-coordinate bounds, and NULL World. Private cases cover empty, isolated,
each adjacent Chest, multiple neighbors, a non-Chest center and NULL World.
An occupied non-Chest target can pass the direct override. This is not evidence
that the enclosing World or Item placement operation accepts an occupied cell.

The seed-23 saved inputs come from the established picking fixture. There is
no App, Player, GUI or original class replacement. Before each call, the helper
sets only declared inputs: four Chunks' Blocks, Data, BlockLight and HeightMap
arrays to zero; SkyLight bytes to `0x0f`; `i=0` and `n=true`; then the target
and sparse Chest cells. These direct array stores do not call a World setter,
construct TileEntities, clear queues, reseed a Random or place a block through
gameplay. Out-of-array target stores are skipped explicitly in the bounds cases.

Every row retains its original return or exception, observed target/cell values,
changed-array count, World and Chunk direct-field equality, light queues and
WorldAccess callbacks. All declared arrays/direct fields and queues remain
unchanged, and there are no callbacks. These are specific snapshots, not a claim
that every provider/cache object in the heap is unchanged. Each original World
uses its genuine Random owner. All four raw seed/Gaussian streams are retained;
their before/after and sequential joins agree within each process. No equality
of constructor entropy across processes is asserted.

The JDI journal records 482 actual `FD.a(III)I` entries and returns per traced
cohort, at most twelve in a case. It retains coordinates, results, entered E
methods and actual E caller code locations, including the outer `E.a` invocation
when a private `h` calls World. Both traced journals agree literally. Three live
method Code hashes agree with the original archive: E's public and private
predicates and FD's block getter. These query counts/order are runtime
observations, not values computed from bytecode. A NULL receiver throws before
entering FD, so those cases have zero FD entries; a native attempted-call record
must not be confused with an entered-method count.

The debugger sets breakpoints only in the authored begin/end boundaries. It
does not invoke a target method, write a target field or set an original-game
breakpoint. Its observer process launches a separate target through local JDWP;
the debugger transport is distinct from game networking. Target guards record
zero game network, native-link, process and outside-write attempts. Three allowed
private-World write permission checks belong to ordinary session-lock creation.
Independent file inventories show only that eight-byte lock may change; other
saved inputs remain byte-identical.

The source packet contains 13 selected class inventories and the three live
Code identities. Loaded class resources and their common source-JAR hash agree
with the verified original. JAR, libraries, JDK/JVM/module files, execution
helpers, declared arguments and compiled products match before and after the
cohort. Raw stdout, stderr, live journals, genuine World inputs/outputs and
compiled helpers stay local by hash. No original class, bytecode body, asset or
decompiled source is published.

`native-observations.h` contains separate public and private outcome arrays.
`query-observations.h` contains all 55 public cases' actual JDI query sequences
and helper-entry counts. Their presence supplies expected data, not a native
acceptance claim. Generation copies observations and arguments; it never solves
the placement predicate. The public audit regenerates both C token streams.

Publication executes no game code. Before writing public files it checks recorded
transport completion, limits, byte counts and hashes; reparses both ordinary
streams and both debugger target/journal streams; compares every result to the
capture report; rederives archive Code identities; and checks the actual private
World inventories. A failed early parser preparation is retained by hash. Its
registry preamble was rejected before the parser explicitly declared the two
ordinary initialization lines. No failed preparation is counted as a pass.

```powershell
python -B tests/reference/world/chest_placement/capture_reference.py --jar b1.7.3.jar --libraries "$env:APPDATA/.minecraft/libraries" --output .local/chest-placement-original-new
python -B tests/reference/world/chest_placement/publish.py --capture .local/chest-placement-original-new
python -B tests/reference/world/chest_placement/packet.py
python -B -m unittest discover -s tests/reference/world/chest_placement -p test_packet.py
```

The captured toolchain is JDK17. Format newly generated C headers with the
repository's clang-format18 setup and run the same publication command again;
matching formatted token streams are preserved. Four exact JDK17 guard warnings
and the two registry initialization lines are required explicitly.

Actual Chest placement, Item use/consumption, TileEntity construction, sound,
Player collision, saved-world gameplay, GUI behavior, native callback refusals,
original-created round trips and sustained play/FPS require separate controls.
