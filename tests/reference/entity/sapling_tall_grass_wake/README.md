# Sapling and TallGrass survival during Player wake

Two fresh JDK17 interpreters execute eight fixed prepared calls to unchanged
original Player damage `(null,0)` after actual sleep entry. The Bed head is
`(8,64,8)`, its foot is `(8,64,7)`, and the plant is `(7,64,8)`. Sapling6 uses
metadata2; TallGrass31 uses metadata1. The substrate at `(7,63,8)` is Grass2
for supported profiles and Air0 for unsupported profiles.

| Mode | World Random seed | Observed outcome |
| --- | --- | --- |
| `sapling-supported` | 1 | Sapling remains; no Item; wake completes |
| `sapling-unsupported` | 1 | Item6/count1/metadata2; Sapling removed; wake completes |
| `sapling-publication-error` | 1 | Same Item published; real observer exception; Sapling remains |
| `tallgrass-supported` | 1 | TallGrass remains; no Item; wake completes |
| `tallgrass-unsupported` | 1 | Seed295/count1/metadata0; TallGrass removed; wake completes |
| `tallgrass-publication-error` | 1 | Same Seed published; real observer exception; TallGrass remains |
| `tallgrass-unsupported-seed1234` | 0x1234 | Seed295/count1/metadata0; TallGrass removed; wake completes |
| `tallgrass-unsupported-seed7fffffff` | 0x7fffffff | No Item; TallGrass removed; wake completes |

All eight inputs were sealed before the first private capture. Every outcome
is retained, including the predeclared no-Item branch. The helper does not
invoke a selector to predict or force a drop, choose seeds after observing
results, or repair state after an interrupted call. The two declared error
profiles naturally reach the Item observer, which throws a genuine
`IllegalStateException` after the Item is inserted in the original World.

The ordinary return is false, with health20 and idle0 after wake. The error
profiles retain sleeping=true, timer37, World sleeping=true and the original
sleeping position; standing dimensions and idle0 are already committed. Bed
occupancy has cleared in every profile. Callback order is head-cell,
Item-added while the plant and sleeping flag remain present, then plant-air
cell. The exception interrupts after Item-added. With no Item, the order is
head-cell then plant-air cell.

Actual receiver classes are `he` for Sapling and `ru` for TallGrass. Reflection
records the declaring class, name and descriptor of each inherited or
overridden neighbor, survival, substrate and drop method. These declarations
describe Java method resolution; they are not evidence that every resolved
method ran in the supported profiles. Actual callback and first Throwable
stacks are retained separately.

Both receivers inherit `wb.b` (neighbor), `wb.g` (survival) and `wb.d`
(substrate). Neighbor invokes the base body8 then final helper17. `wb.h`
calls virtual survival6, reads metadata23, drops26, then notifies air35.
Sapling overrides metadata through `he.b_(I)I`, preserving metadata2 with
mask3. TallGrass overrides `ru.a(ILjava/util/Random;)I`: `nextInt(8)` at3
chooses its live Seed Item reference at9/12 or returns -1 at16. Its metadata
and count declarations are inherited from `uu`. No game class is patched,
recompiled or replaced with reconstructed behavior.

The real cached original Chunk has SkyLight15, BlockLight0 and a uniform
height plane64. Survival queries `fd.m`, which passes literal subtraction0
to `lm.c`. Separately recorded World.f=5 is sky subtraction used by other
methods. `fd.m` reaches the real original Chunk/nibble bodies; no light getter
is overridden or sampled separately. The bright inputs avoid the conditional
sky-visible branch. The original shared Chunk sky-seen static changes false
to true in the first supported profile and remains true through subsequent
profiles. It is not reset between calls.

Each row declares shared setup `[0,0,20,0,0]`, both post-setup cell assignments
and the actual World Random reseed. Shared setup prepares real cached Chunk
planes, calls actual sleep entry, then prepares timer37 and selected fields
and Random streams. Assigning Air below the plant retains those prepared
light/height arrays. These are finite prepared operands, not natural terrain
or a ticked sleep cycle.

The finite packet retains selected Player state, complete World/Player/Math
Random observations, live plane hashes and light inputs, actual Item fields,
ordered callbacks and complete first exception stacks. Thirty-four original
classes are bound to the supplied archive. `plant-family-pins.json` adds full
`he` and `ru` structures and exact virtual-drop Code authorities to unchanged
shared Flower/World/Chunk/Bed/Player/Item pins. The archive hash also covers
the live Seed reference; `gm` has no separate class record in this34-class
inventory. Only seven authored helper classes are compiled.

The native wake controller compares all eight selected profiles: 34 values
for each supported plant, 57 for each ordinary Item drop and the second
TallGrass Item seed, 49 for each publication exception, and 42 for no Item.
That is 379 selected values per native host. It includes actual light inputs,
World/Player Random states, Item position and Stack, wake pose and ordered
callbacks. Wider original Random/Gaussian observations, Item constructor
entropy, complete Chunk planes and exception stacks remain unpaired.

Fifteen native modes also check independent scope refusal, missing Math,
later removal-observer exceptions and no-Item continuation without Math.
These use the real retained Player damage/wake route with prepared operands;
they do not establish a graphical sleeping-Player encounter. The Java-free
reader validates original observations and bindings without running a native
comparison. Natural sleep, low-light Sapling/TallGrass, growth/tick,
additional metadata, server and sustained-gameplay parity remain separate.

Run the reader and closed-schema corruption checks without Java:

```text
python -B tests/reference/entity/sapling_tall_grass_wake/packet.py
python -B -m unittest discover -s tests/reference/entity/sapling_tall_grass_wake -p test_packet.py
python -B -O -m unittest discover -s tests/reference/entity/sapling_tall_grass_wake -p test_packet.py
```

Repeat the original capture using the supplied archive and Launcher libraries.
The output directory must be new and within the checkout's `.local`:

```powershell
python -B tests/reference/entity/sapling_tall_grass_wake/capture_reference.py `
  --client-jar PATH_TO_SUPPLIED_CLIENT.jar `
  --library-root "$env:APPDATA/.minecraft/libraries" `
  --java-bin "$env:JAVA_HOME/bin" `
  --output .local/reference/sapling-tall-grass-wake-original
```

Raw commands, streams, bounded process receipts, original class metadata,
compiled helpers and isolated work directories stay private. Public proof
binds all executed authored/shared sources, tools/libraries, seven complete
process receipts and both identical VM stdout/stderr extents/hashes. Commands
are represented publicly by hashes. Before/after checks cover input files,
helper products, class names and original class pins. No ticks, GUI, native
library, network or external process launches occur inside the original VMs.

`read_reference(directory=HERE)` validates the closed finite/provenance packet
and returns its dictionary. `check(directory=HERE)` does the same and prints
the original-only status. `P` exposes shared exact-type helpers. Readers reject
duplicate JSON keys, nonfinite values, unknown fields, Boolean integers,
changed profiles/seeds/method declarations, wrong Item outcomes, altered
source order, changed light inputs and incomplete process receipts. The proof
binds retained fields; it does not automatically compare them with native code.
