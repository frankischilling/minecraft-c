# Fire source bodies

This packet records 71 declared calls through unchanged Beta 1.7.3 client
Fire bodies. It covers actual Fire placement, concrete neighbor/tick calls,
Still Lava ignition, scheduled dispatch through `World.l`, and one Player's
normal random work over 361 cached Chunks. Later TNT fuse ticks, explosions,
natural encounters, rendering, frame rate and whole-game parity remain separate.

The fixed profiles include all nine entries in Fire's real encouragement and
flammability arrays, each with successful removal and replacement; supported
and unsupported Fire; all six flammable support faces; age 0/3/4/15 behavior;
Netherrack; rain at the Fire and each horizontal neighbor; and real Plains,
Desert and Tundra precipitation predicates. Prepared biome managers are genuine
original `hd` instances with the corresponding original static Biome, assigned
to the actual Dimension before the call. No original getter, class or method
is replaced.

Fire caches its old age before its `nextInt(3)/2` metadata update and scheduling.
The six burn attempts retain their original order and bounds, including draws
against nonflammable cells. Spread retains the original X/Z/Y traversal,
integer probability and rain short circuits. Its west rain query uses the
original center Z. Unsupported tick removal does not immediately return.
Lava ignition uses Material flammability rather than Fire encouragement: a
Crafting Table can cause a real Fire write followed by its actual removal.

Both Portal orientations execute the real frame check and six Portal writes.
Broken and ambiguous frames retain the Fire path. A genuine callback exception
on the third Portal write leaves the committed cells and `World.o=true`.
Burning TNT executes actual replacement/removal, the original `qw` constructor,
World publication and `random.fuse` sound. Constructor direction, bounds,
velocity, fuse 80, Entity ID and Math entropy are observed. Publication, sound,
and preceding cell failures preserve their different reached prefixes. Two
additional scheduled World tick cases interrupt real TNT publication and entity
sound after the World time advance and scheduled-record removal. They retain
the committed TNT, both newly scheduled Fire records and the original Throwable
through the full World tick caller.

The height case crosses into Chunk X=1. Its actual height range notification
uses local X=0 while the stored block and light requests use World X=16.
Range and cell interruptions preserve the original ordering of block,
metadata, height, sky, lighting queue and notification stores. Real null
encouragement/flammability arrays and a null World provider test genuine
NullPointerException precedence after the already completed metadata/schedule
prefixes. These prepared source-field controls are recorded before invocation;
their values are restored only after both observations for the next fresh World.

Most cases prepare four genuine original Chunks with zero Block, metadata,
SkyLight, BlockLight and height planes, then apply literal cell/height operands.
The random integration case prepares the full 19x19 footprint and actually
constructs and publishes one LocalPlayer at (-.5,64,32.5). Its declared World
LCG input reaches Fire through the first candidate Chunk and first normal
sample; the actual loop completes all 28,880 samples and exactly one Fire tick.
All four or 361 complete five-plane hashes are retained before and after.
The observer also checks that the complete cache footprint still contains the
same original Chunk receivers, including during the provider-null control.

Selected state includes World/Math Random with Gaussian cache, absolute
scheduled-class initialization/sequence and both real scheduled sets, Entity
class initialization/next ID, ordered light regions and nesting depth, weather,
World time/flag, sky-seen flag, both complete Fire arrays, Player/TNT fields,
and actual random-work LCG/cooldown/Set traversal. Every callback retains its
parameters, current cells, published TNT fields, Random checkpoints and complete
Java stack. Default Entity Random initialization is unused by these calls and
is explicitly unpaired; World and Math streams are supplied before invocation.

`model.py` derives the complete finite source effects from declared inputs,
Java LCG draws and IEEE rounding. `hashset_order.py` independently implements
the reached JDK17 insertion, resize, tree balancing and traversal rules for the
distinct packed Chunk hashes. Neither imports a native runtime, selects expected
effects from original rows, nor resets an observed post-call difference.
`packet.py` closes every row against that model and binds original class/Code
identities. Source stacks are original evidence; native projection does not
claim to reproduce Java stack objects.

The bounded capture uses the locally supplied unchanged client archive with
SHA-256 `af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`,
58 original class inventories, pinned Code and ten authored helper products.
Two fresh JDK17 VMs run with `-Xint -Xverify:all`; complete unfiltered stdout and
stderr must match byte for byte. Original runs have a 70-second ceiling, tool
processes are bounded and complete pipe capture is required. Source, helper,
archive/library/tool, class-name and inventory identities are checked before
and after. Effect guards reject network, native-library/process launches and
writes outside the private work directory. Original binaries/assets and raw
outputs remain private under `.local`.

```powershell
python -B tests/reference/world/fire/capture_reference.py `
  --client-jar b1.7.3.jar `
  --library-root "$env:APPDATA/.minecraft/libraries" `
  --java-bin "C:/Program Files/Eclipse Adoptium/jdk-17.0.17.10-hotspot/bin" `
  --output .local/reference/fire-fresh
```

Use a new output directory. After checking its private report, publish only
`finite.json` and `provenance.json` here. From this directory, run
`python -B packet.py` and `python -B -m unittest test_packet.py`, then repeat
the latter with `-O`. Corruption controls reject changed operands, cells,
callback order/entropy, real exception prefixes, all planes, queue/class
state, JDK traversal, method inventories and bounded-capture receipts.

[run_native.py](run_native.py) pairs the exposed ordinary native projection.
Native class state is an explicit prepared input taken before the matching
original Player allocation/prequeue; original counters are never reset and
output ordinals/IDs are never normalized. See the
[Fire testing scope](../../../../docs/testing.md#fire-source-bodies).
