# Remaining flower-family neighbors during Player wake

This packet records 25 predeclared prepared calls to unchanged original Player
damage `(null,0)` after actual sleep entry. It covers Dead Bush32, Crops59 and
both Mushrooms39/40. Existing Sapling/Tall Grass and Flower packets remain sealed
dependencies. This reader validates original observations; it runs no native
comparison and establishes no natural encounter, tick/growth or whole-game parity.

The Bed head is `(8,64,8)`, its foot is `(8,64,7)`, and the plant is `(7,64,8)`.
Shared setup `[0,0,20,0,0]` creates the actual cached Chunk, enters sleep, then
prepares timer37, idle17 and the selected Random streams. The declared plant
and below-cell assignments occur after setup. Only the actual SkyLight nibble
at the plant changes to the profile's selected value. The remainder of the
shared SkyLight plane stays zero below Y64 and 15 at/above Y64; BlockLight stays
zero and the complete height plane stays64. No light or sky getter is overridden.
World field `f=5` remains separately observed; actual `fd.m` passes literal zero
subtraction to the original Chunk light body.

## Declared profiles

- Dead Bush: metadata0, sand12 support and grass2 support loss, light15, seed1.
- Crops: farmland60 support at metadata0/7; Air0 support loss at each stage with
  seeds1, `0x1234` and `0x7fffffff`; metadata7/seed1 exceptions at first and second
  Item publication, plus a later plant-cell observer exception.
- Each Mushroom ID: grass2 support with target SkyLight0 and12; Air0 support
  loss with light12; grass2/light13 rejection; light12/Air0 Item publication
  and later plant-cell observer exceptions. Metadata0 and seed1 apply throughout.

Every profile and seed is declared in the helper and reader before capture.
The callback target is a fixed ordinal; no Item selector is invoked to choose
inputs or force a drop. An unreached exception ordinal would remain a normal
outcome. Source effects are never repaired after failure. The observer records
an Item only after the actual World ledger contains it, then throws the genuine
declared `IllegalStateException`. Cell exceptions occur after the real air store
and cell notification, while the Player is still asleep.

## Source and observation scope

Dead Bush `jb`, Crops `ni` and Mushroom `tl` extend `wb`; all inherit `wb.b`
and final helper `wb.h`. The helper invokes virtual survival6, metadata23,
drop26 and notifying air35. Dead Bush and Crops inherit `wb.g`; their virtual
substrates are `jb.d` (live sand) and `ni.d` (live farmland). Mushroom overrides
survival with `tl.g`: Y0..127, real light18 below13, below-cell34 and substrate37.
Its `tl.d` captures the actual `Block.o` array before element4.

Dead Bush's item selector returns -1, but the inherited chance float still
runs. Mushroom inherits count1, its own Block ID and damage0. Crop probability
first calls the base body at10. Exact metadata7 selects live Wheat `Item.R`;
then three `nextInt(15)` calls at36 independently select live Seed `Item.Q` when
the result is at most metadata. Extra Seed positions use binary32 arithmetic
through the coordinate addition before widening. Entity allocation111 precedes
Stack allocation135, live `Item.Q`139 and constructor142; Item constructor145,
pickup delay154 and World publication160 follow. Complete original method and
class inventories bind `jb`, `ni`, `tl` and `gm`, plus all34 shared original
classes. No original class is patched, recompiled or replaced.

Each row retains actual receiver method resolution, full selected Player/World
state, all five raw Chunk plane hashes, complete World/Player/Math Random
observations, every published Item and ordered callbacks with complete stacks.
Method resolution is not proof that every resolved method ran. Actual callback
and first Throwable stacks retain executed source order separately. Item
constructor entropy and unobserved fields remain outside this packet.

The closed reader uses an independent unsigned Java LCG and explicit binary32
rounding to check reached draw counts, Item coordinates, phase/yaw/motion and
Math Random progress. It checks raw planes against the declared shared setup
and completed cell stores. Shared prepared Player fields use the already bound
original setup packet; no native answer or C execution supplies expectations.
The original defining Chunk's sky-seen static is never reset: the first Dead
Bush light15 query changes false to true; later profiles inherit true, including
zero-light Mushrooms. Both before and after values remain recorded.

Capture requires two fresh JDK17 interpreters with identical complete streams,
bounded tool/process receipts, exact supplied archive/library identities and
unchanged input/helper/class inventories. Seven authored helper classes are
the only compiled products. Complete commands, streams, original metadata,
class files and isolated Worlds stay under the private capture directory.
The public proof contains hashes and bounded process receipts, not local paths.

Run the Java-free reader and corruption controls:

```text
python -B tests/reference/entity/remaining_plants_wake/packet.py
python -B tests/reference/entity/remaining_plants_wake/test_packet.py
python -B -O tests/reference/entity/remaining_plants_wake/test_packet.py
```

Repeat capture with a fresh output directory under `.local`:

```powershell
python -B tests/reference/entity/remaining_plants_wake/capture_reference.py `
  --client-jar PATH_TO_SUPPLIED_CLIENT.jar `
  --library-root "$env:APPDATA/.minecraft/libraries" `
  --java-bin PATH_TO_JDK17/bin `
  --output .local/reference/remaining-plants-wake-original
```

`read_reference(directory=HERE)` validates the finite/provenance envelopes and
returns the original rows. `validate_finite` and `validate_provenance` expose
the same strict checks independently for corruption controls. Duplicate JSON
keys, nonfinite values, unknown fields, Boolean integers, changed source order,
altered profiles/methods/light planes, wrong Item sequences, incomplete captures
and changed bindings are refused. Native pairing remains a separate controller.
