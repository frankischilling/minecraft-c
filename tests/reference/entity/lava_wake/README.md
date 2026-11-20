# Lava mixing during Player wake

This finite original packet records 29 prepared calls to the unchanged Beta
1.7.3 Player damage/wake path. The sleeping Player clears the occupied Bed bit,
then its real World neighbor dispatch reaches Still Lava. It covers metadata 0
producing Obsidian, metadata 1–4 producing Cobblestone, metadata 5 and 15
retaining hot Lava, all four horizontal water neighbors and water above, no
contact, and reached StillWater conversion. All cells, seeds and observer
interruption ordinals are declared before execution.

The original receiver is `nx`, with inherited `rp` mixing and fizz bodies.
Hot Lava with contact fizzes twice: once before quiet conversion and once in
Flowing Lava's Added callback. Each call retains its actual sound, eight
particles, World Random pitch draws and Math Random positions. No-contact
conversion makes no presentation callbacks. StillWater reached after opaque
Lava conversion changes 9 to 8 through the five-argument Chunk setter and
retains its light, Added, range and scheduling effects.

Genuine observer exceptions interrupt cell, sound, first/fourth/third/ninth
particle and early/late range callbacks. They preserve the first Throwable,
sleeping Player, committed cells, consumed draws, light stores and queue
prefixes. Height range notification precedes height and sky stores. A later
range failure retains `World.o=true` and the first allocated scheduled record;
the second schedule and flag clear have not run. The nested StillWater failure
also prevents the later Lava fizz.

`finite.json` retains actual virtual declarations, full selected Player state,
all five complete Chunk-plane hashes, ordered Cell/Range/Sound/Particle
callbacks, real Java stacks, complete Player/World/Math Random state, passive
scheduled-class initialization/absolute sequence and both World scheduled sets.
The actual light-request list and static nesting depth are recorded, including
original region merging. All prepared queues are empty without post-call
resetting. With no surrounding Chunk radius, scheduling still allocates records
while both membership sets remain empty. The actual Chunk sky-seen flag stays
false; no light or sky getters are replaced or invoked by the probe.
North-water changes the real first available Bed exit.

`packet.py` is Java-free. Its independent interpreter derives the fixed
neighbor/mixing/conversion order, Java LCG draws, binary32 pitch rounding,
binary64 particle positions, exception prefixes, height/sky stores, complete
plane bytes and the original last-five light-region merge rule from declared
inputs and pinned bodies. It never selects expected effects from original rows
or native output. Shared Player setup and its unchanged state authority come
from the sealed Player-damage and plant-wake packets.

The original capture uses the locally supplied client archive with SHA-256
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`,
46 original class inventories, pinned Code identities and ten authored helper
products. It does not replace any original class. Two fresh JDK17 VMs run with
`-Xint -Xverify:all`; complete unfiltered stdout and stderr must match exactly.
Every tool process is bounded, with original runs limited to 70 seconds and
complete pipe capture required. Before/after identities bind supplied binaries,
authored sources, shared helpers, class names and class inventories. A guard
rejects network, process, native-library and out-of-directory write effects.
Private outputs, original bytecode and assets stay under `.local`.

Reproduce with a local unchanged client and the required libraries:

```powershell
python -B tests/reference/entity/lava_wake/capture_reference.py `
  --client-jar b1.7.3.jar `
  --library-root "$env:APPDATA/.minecraft/libraries" `
  --java-bin "C:/Program Files/Eclipse Adoptium/jdk-17.0.17.10-hotspot/bin" `
  --output .local/reference/lava-wake-fresh
```

The output directory must be new. After checking the private report, publish
only its `finite.json` and `provenance.json` here. Run `python -B packet.py` and
`python -B -m unittest test_packet.py` from this directory; repeat the latter
with `-O`. Corruption controls reject altered operands, receiver declarations,
partial effects, callback parameters/order/Random checkpoints, planes, stacks,
light/scheduled queues, method inventories and capture receipts.

[run_native.py](run_native.py) pairs the exposed native projection and records
separate refusal/ownership controls. Prepared native scheduled-class state is
supplied from the corresponding original before-state; natural class
construction remains a separate claim. See the
[Player-wake testing scope](../../../../docs/testing.md#lava-mixing-during-player-wake).
This packet qualifies no scheduled tick, light propagation, natural encounter,
rendering, frame rate or whole-game parity.
