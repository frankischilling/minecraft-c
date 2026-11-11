# Flower survival and removal during Player wake

Two fresh JDK17 interpreters execute three prepared calls to the unchanged
original Player damage method with `(null,0)`, after actual sleep entry. The
occupied Bed head is `(8,64,8)`, its foot is `(8,64,7)`, and Flower37 with
metadata0 is `(7,64,8)`. The profiles are supported Grass2 below the Flower,
unsupported Air below it, and the same unsupported input with a declared Item
observer `IllegalStateException`.

The supported call retains the Flower and completes wake without an Item. The
unsupported call clears Bed occupancy, publishes Item37 while the Flower and
sleeping flag remain present, notifies the Flower's air cell, then completes
wake. The exception call retains the published Item and Flower, interrupts
wake with timer37 and World sleeping true, and leaves health20. Idle has already
changed from17 to0 and the Player has standing dimensions.

The callback stacks contain `wb.h` and `wb.b`. The unchanged neighbor body
invokes empty base8 then final survival helper17. The helper calls virtual
survival6, reads metadata23, drops26, and only then invokes notifying air
setter35. An observer exception interrupts that real drop before the air write.

Flower survival calls **`fd.m`**, not growth light `fd.n`. The prepared Chunk has
SkyLight15, BlockLight0 and a uniform height plane64. `fd.m` passes literal
subtraction0 to `lm.c`; the separately recorded World.f value5 is sky
subtraction used by other methods. The original Chunk shared sky-seen field
changes false to true during the first supported profile and remains true.
No light or sky getter is overridden or sampled outside the actual wake call.
The conditional sky-visible call is not reached under these bright inputs;
its branch is established from original Code, not a debugger query trace.

The helper declares its shared setup operands and both post-setup cell
assignments in every row. Shared setup fills the real cached original Chunk's
planes and enters sleep through the original Player method, then prepares
timer37, selected fields and seeded Random streams. Assigning Air below the
unsupported Flower deliberately retains the prepared light/height arrays.
There is no state repair, forced method result or natural-tick claim.

The finite packet keeps full selected Player state, Chunk plane hashes,
World/Player/Math Random observations, actual Item fields, callbacks and full
exception stacks. Thirty-two original classes are bound to the verified
archive. `flower-pins.json` adds complete `wb` and `wi` structures and eight
selected Flower/light Code authorities; shared pins bind the remaining
World, Chunk, Bed, Player and Item bodies. Only authored helper classes are
compiled. Original game classes are neither patched nor recompiled.

The separate native controller's `compare_flower_original` function in
`tests/tools/entity/test_player_damage_wake.py` selects34 scalars for supported
wake,57 for unsupported removal and49 for the publication exception:140 total.
It compares Player pose/dimensions, sleep/timer/health/idle, World sleeping,
head/Flower cells, actual floor/sky/block-light/height/World.f/Chunk sky-seen
state, World/Player seeds, selected Item fields, ordered cell callbacks and
publication state. The selected Item fields are position, delay, Stack,
inserted flag and World identity.

The packet reader validates original observations and provenance bindings;
it does not execute that native comparison or automatically compare every
retained source field. Wider Item motion/yaw/phase/dimensions/bounds/age/health,
Random Gaussian caches and Math Random, other Player fields, Chunk plane
hashes and Java stacks remain unpaired. The dark/sky-visible branch and
native-only refusal/observer controls have no original counterpart among
these three bright prepared profiles. Host qualification is recorded outside
this packet.

These prepared original calls execute no ticks, GUI, native library, network
or process launch. They do not establish natural sleep, the low-light branch,
sustained gameplay or whole-game parity.

Run the Java-free reader and corruption checks:

```text
python -B tests/reference/entity/flower_wake/packet.py
python -B -m unittest discover -s tests/reference/entity/flower_wake -p test_packet.py
python -B -O -m unittest discover -s tests/reference/entity/flower_wake -p test_packet.py
```

Repeat the original capture with the supplied archive and Launcher libraries.
The output directory must be new and inside the checkout's `.local`:

```powershell
python -B tests/reference/entity/flower_wake/capture_reference.py `
  --client-jar PATH_TO_SUPPLIED_CLIENT.jar `
  --library-root "$env:APPDATA/.minecraft/libraries" `
  --java-bin "$env:JAVA_HOME/bin" `
  --output .local/reference/flower-wake-original
```

Raw streams, commands, bounded receipts, tool/library hashes, compiled helpers
and isolated work directories stay private. Public provenance binds the
executed authored/shared sources and both complete VM stream extents/hashes.
Private commands are represented publicly by their hashes.

`read_reference(directory=HERE)` validates the closed finite/provenance packet
and returns the finite dictionary. `check()` does the same and notes the
separate140-scalar projection and unpaired observations. `P` exposes shared
exact-type helpers. Readers reject
duplicate JSON keys, nonfinite numbers, unknown fields, Boolean integers,
wrong light inputs, changed ordering and incomplete process receipts.
