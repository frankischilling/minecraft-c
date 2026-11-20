# Cactus and Reeds neighbors during Player wake

This packet records twenty-one predeclared prepared calls to unchanged original
Player damage `(null,0)` after actual sleep entry. It covers Cactus81 and
Reeds83 beside a Bed, three-cell recursive columns and interruptions at first
or second Item publication and lower or middle removal. Native replay is a
separate controller. No tick, growth, natural encounter, graphical or complete
game correspondence is established here.

The Bed head is `(8,64,8)`, its foot is `(8,64,7)`, and the bottom plant is
`(7,64,8)`. Shared setup `[0,0,20,0,0]` constructs the actual cached Chunk and
Player, enters sleep, then prepares timer37, idle17, Player Random38 and
Math Random0. The declared cells are assigned after that setup. World Random1
is supplied to every profile. Neither the light planes nor a light/sky getter
is overridden. The defining Chunk's sky-seen static is observed without being
reset; these bodies make no light query and it stays false throughout both VMs.

`profiles.json` declares every mode and operand before capture. Cactus has sand
below; the neighboring actual Bed material rejects the bottom Cactus. Removing
that bottom cell notifies the next Cactus and can remove a complete three-cell
column. Reeds cover Air, sand, dry Grass, every adjacent still-Water direction,
flowing Water beside Dirt, and a same-Reed substrate. This version accepts
Grass/Dirt beside water or another Reed below; the sand scenario is deliberate.
The water neighbors sit at Y63 and are assigned directly, without fluid ticks.

The east Water cell lies directly below the Bed. Its reached Still Fluid
neighbor converts Water9 to flowing Water8, briefly sets `World.o`, emits one
range callback and requests two scheduled records. Sand beneath a removed
bottom plant requests one record when notification reaches it. Missing radius
Chunks leave both World scheduling sets empty, but the defining `qy` sequence
still advances. The observer passively reads the actual class initialization
state and sequence through `Unsafe` without initializing or resetting that
class. Both fresh VMs start at uninitialized/sequence0 and end at initialized/
sequence8. Source class-state inputs need separate alignment in native replay;
these prepared calls do not qualify natural constructor ordering.

The north Water cell rejects the first Bed exit support candidate. The Reed
blocks the next Air check, so that profile exits at Z9.5 rather than Z7.5.
The reader evaluates the declared candidate cells and preserves the changed
position and both actual bounding boxes. No pose normalization is applied.

Both original classes extend `uu`. Cactus `or.b` directly invokes virtual
survival6, metadata23, drop26 and notifying air35. Reeds `ri.b` calls its final
helper `ri.h` at6; that helper has the same survival/drop/removal sequence.
Reed survival delegates to its placement body, preserving below-cell and water
identity short circuits. Cactus checks west/east/north/south Material solidity
before its below-cell Cactus/sand test. World material `fd.f` reads the actual
Block at4, then uses air Material14 or the live Block array18/23 and material24.
The packet binds the complete selected original class inventories and method
Code hashes. No original class is patched, recompiled or replaced.

The selected inventory contains 43 classes, including the reached Sand,
Still/Flowing Fluid, Material and scheduling declarations. Callback observations
retain actual `World.o` and the water cell alongside the column. The range
callback occurs after the quiet conversion while `World.o` is true; normal
completion restores false. The final east-Water range profile throws a genuine
observer exception after the quiet conversion and first schedule request.
It retains `World.o=true`, the sleeping Player and sequence8, and never reaches
the second schedule or flag restoration. All preceding profiles declare the
range-exception operand false. Broader fluid ticks, Lava, scheduler drainage and
nonempty scheduling-set scenarios retain their own qualification requirements.

Each removed cell consumes the inherited chance float, then three position
floats before creating its Item. Cactus drops its own Block Item; Reeds select
live `Item.aH`338. Four Math doubles establish phase, yaw and horizontal motion
for each Item. The reader independently checks these arithmetic operations,
complete World/Player/Math Random continuations, all five raw Chunk-plane hashes,
the complete three-cell column, ordered callbacks and original call stacks.
Source observer exceptions retain the committed prefix without repairing it.
Original Item constructor entropy remains outside the observation format.

Capture requires two fresh JDK17 interpreters with identical complete streams,
bounded tool/process receipts and unchanged helper/archive/library identities.
Seven authored helper classes are the only compiled products. Full commands,
streams, classes and isolated Worlds remain under the private capture directory;
public proof contains hashes and bounded receipts rather than private paths.

Run the Java-free reader and corruption controls:

```text
python -B tests/reference/entity/upright_plants_wake/packet.py
python -B tests/reference/entity/upright_plants_wake/test_packet.py
python -B -O tests/reference/entity/upright_plants_wake/test_packet.py
```

Repeat original capture with a fresh output directory under `.local`:

```powershell
python -B tests/reference/entity/upright_plants_wake/capture_reference.py `
  --client-jar PATH_TO_SUPPLIED_CLIENT.jar `
  --library-root "$env:APPDATA/.minecraft/libraries" `
  --java-bin PATH_TO_JDK17/bin `
  --output .local/reference/upright-plants-wake-original
```

`read_reference(directory=HERE)` validates the closed finite and provenance
envelopes before returning observations. Duplicate JSON keys, nonfinite numbers,
unknown fields, Boolean integers, altered profiles, changed methods/planes,
wrong Item or recursive callback order, incomplete captures and changed bindings
are refused. A native projection remains responsible for its own retained
execution and cleanup observations.
