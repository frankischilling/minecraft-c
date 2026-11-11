# Damage wakes a sleeping Player

Client `gs.a(sn,int):boolean` wakes a living, sleeping Player in a local World
before processing attacker type, difficulty or zero damage. Its wake call at
35 uses `(true,true,false)`: restore the standing size, clear Bed occupancy,
find the exit, clear sleeping and the timer, and recompute the World sleeping
flag without replacing the saved spawn. The existing
[sleep/wake implementation](player-sleep.md) supplies that body.

The earlier `av=0` store remains visible on every entered call. Nonpositive
health returns false before the sleeping predicate. A sleeping Player reads
its current World's remote flag at 25; a null World fails there. A remote
World skips wake. Damage zero returns false only after a reached local wake
has completed. A nonzero remote call still adds the damage statistic before
the inherited Living body returns false.

The damaged Player owns an interrupted native wake, including calls whose
optional error output is null. Its effect receipt borrows the retained child.
Pending detection and checked effect cleanup keep the real Player, World,
Registry, coordinates and occupancy work live until retirement succeeds.
Successful wake retires before later damage work. Cleanup retries retirement;
it does not repeat wake, damage statistics or health changes.

## Wake during an Arrow hit

The explicit damage execution used by Arrow enters the same wake body as
direct Player damage. The Player retains the wake invocation and its actual
World operands. GS loads the World for the block check at 30, for occupancy
at 61, for the exit search at 84, and for the sleeping List update at 198.
The two static Bed methods activate before checking their nullable World
argument at their own metadata call. An observer can change Player.World
during occupancy, and the exit search uses the later value.

Clearing the occupied bit calls the notifying metadata setter. It reads the
changed Block ID and the actual Block.t entry, notifies the live WorldAccess
List when required, then visits neighbors in the original order. A nearby
orphan Bed head reads its own direction and checks its foot. It removes itself
through the notifying ID setter. The acquired Chunk keeps its own lease
through the write, lighting, height and column work. The outer World operand
remains the same throughout these recursive calls.

In the prepared direction-zero case, the first cell observer sees the original
head at metadata eight and the orphan still present. The next observer sees the
orphan already removed. The Player remains asleep at both callbacks. A normal
return then finds the exit, moves the Player, clears sleeping and the timer,
and updates the live Player List. A callback exception preserves both cell
changes and stops before exit search. A callback that clears Player.World and
returns normally permits the recursive write to finish, then fails at the
actual exit search.

The retained wake exposes the actual failed child result for native cleanup.
Arrow saves the exception information while the producing receipt is still
live, services recorded acquisitions, and closes only the World tokens the wake
or its children opened. A callback can leave a separate scope open. Cleanup
refuses until that scope's owner closes it, then releases the recorded wake
work without running another cell callback.

Owned occupancy diagnostics borrow the execution's protected callback owner.
Their retirement does not enter the legacy diagnostic capture or acquire a new
producer hold. The receipt-limit control forces the count to `SIZE_MAX` during
cleanup, requires successful retirement with the count unchanged, then restores
the actual count before retiring the execution. The preceding failing runs
reached an unnecessary legacy retain after the block changes had already
committed; the Source prefix and callback count stay unchanged by the fix.

## Orphan foot drop

An orphan foot at `(7,64,8)`, beside the occupied head at `(8,64,8)`,
reaches the actual Bed drop call at `ve.b:129`. The owned entry uses the
existing [Block probability and spawn bodies](../../src/world/runtime/mutation/drops.c),
with [retained scopes](../../src/world/runtime/mutation/drops/execution.c).
It does not substitute the
Entity drop method, whose coordinate arithmetic and Random reads differ.
The World Random supplies the chance draw and three offsets. Item construction
uses its defining Entity classes and independent constructor entropy.

The retained drop records the actual constructor World load at `uu.a:85`
and publication load at `uu.a:116`. A failed publication keeps its real
insertion frame, Item local, callback receipt and raw return status. Native
retirement closes only those recorded occurrences after canonical acquisition
work has drained. A separate callback scope must be closed by its owner first.
Already consumed tokens are never popped again. Cleanup performs no new
constructor, insertion, Random read or callback.

The [foot packet](../../tests/reference/entity/bed_foot_wake/README.md) retains
two unchanged-original runs of normal return and an Item observer exception.
The native projection compares 44 values per case: Player pose and sleep
state, both committed cells, ordered cell callbacks, World and Player seeds,
and Item position, Stack, delay and publication flags. Item motion, yaw, phase,
Random Gaussian caches and Java stacks remain outside this native projection.
The original packet keeps those selected observations and thirty class pins.

Native Arrow cases enter sleep through the real sleep call, then cross the
sleeping bounds above the Bed. The successful hit clears occupancy, removes
the orphan foot, publishes one Bed Item, wakes the Player and applies four
damage. The observer-exception case preserves both cell changes and the inserted
Item while leaving health and sleep unchanged. It checks each native unwind
phase through diagnostic capture, hurt close, cleanup and quiescence without
replaying work. These are prepared CPU tests, separate from graphical evidence.

Additional native controls compare legacy and owned entries to the same Block
bodies for Bed, double slab, clay and mature crops. They check multiple Item
publications, crop-specific argument evaluation, selected Item fields and
World Random consumption. They are ownership regressions, not independent
original observations. Missing Math ownership and an outstanding callback
scope have separate controls. Moving-piston TileEntity lookup and other
unjoined neighbor hooks remain explicit frontiers.

## Ladder drop

An unsupported metadata-two Ladder beside the occupied head uses the same
[Ladder neighbor body](../world/ladder.md#owned-wake-and-recursive-support).
The head occupancy observer returns first. Item publication then observes the
Ladder still present and the Player asleep. Only after publication returns does
the notifying air setter remove the Ladder and wake continue.

The [unchanged-original packet](../../tests/reference/entity/ladder_wake/README.md)
pairs 50 normal-return values and 42 publication-exception values with native
execution. A publication exception keeps the Ladder, inserted Item and sleep;
a later cell-observer exception keeps the Item and already removed Ladder.
Separate native cases cover an attached Ladder, missing Math ownership and a
callback's outstanding independent scope. Real Arrow cases check the successful
hit and both exception prefixes through checked native unwind. No cleanup path
replays publication, removal or Random consumption.

## Torch drop

An ordinary Torch beside the occupied head reaches its existing
[neighbor and support bodies](../world/torch-placement.md#owned-neighbor-during-player-wake)
through the retained execution. A Torch with missing west attachment and a
Stone floor publishes its Item after occupancy clears, while the Torch still
exists and the Player remains asleep. The notifying air setter follows.
The separate no-support route removes it during general survival instead.

The [original packet](../../tests/reference/entity/torch_wake/README.md) compares
54 selected values for normal wake and 46 for an Item observer exception.
The native projection includes actual west/floor cells. Native early/late
removal cases, supported attachment, independent scope and Math failures keep
their own controls. Real Arrow cases preserve health, sleep and each committed
prefix through checked cleanup when publication or a later cell observer
throws. These CPU cases do not qualify graphical sleeping-Player damage.

## Flower survival and drop

A Flower beside the occupied head now enters its existing neighbor/survival
body through the retained wake execution. A supported Flower survives without
an Item or Random draw. An unsupported Flower publishes Item37 while the
Player remains asleep and the Flower still exists, then removes the Flower
through the notifying air setter before wake finishes.

The [original packet](../../tests/reference/entity/flower_wake/README.md) selects
34 supported, 57 unsupported and 49 publication-exception values for native
comparison. The exception preserves the Flower, inserted Item, sleeping flag,
timer 37 and health 20; idle has already changed to zero. Actual light and
height inputs, the separately recorded World sky subtraction and the defining
Chunk class's sky-seen flag remain explicit. Real Arrow cases check both
exceptions through five native unwind phases, retaining the committed prefix
and complete World, Player and Arrow Random fields without replay. Later
cell-observer failure and the native ownership controls preserve their own
committed prefixes.
See the [Flower contract](../world/plants.md#owned-flower-neighbor-during-player-wake).

## Original evidence

The [component packet](../../tests/reference/entity/player_damage_wake/README.md)
contains two fresh unchanged-original VMs, each with 32 actual damage calls:
25 returns and seven first null exceptions. The Player is genuinely published
and enters sleep before declared component mutations. Four directions cover
both zero damage and a nonfatal hit. Other profiles cover dead Players,
remote bypass, null World/bounds/bed coordinates/App/Inventory, missing Beds,
and awake controls. All attackers are null.

The selected original state agrees exactly across both VMs: 4,794 scalars per
VM, including raw floating-point words, coordinate/bounds identity relations,
the five complete central Chunk-plane hashes, Random state, statistics and
sound arguments. The native comparison selects 4,748 shared scalars. The 46
unpaired positions retain the Java row schema and actual Java stack names;
the native API exposes no Java exception stack. This is not a claim of
complete World/App constructor or object-graph equality.

The null-bounds profiles preserve the occupancy clear and position stores
before the actual original `sn.e` failure. The sleeping flag and timer remain
unchanged at that failure. Missing Bed or null bed-coordinate profiles skip
repositioning, complete wake and retain their previous small bounds. A null
App fails only after wake when nonzero damage reaches the statistic callback.
A null Inventory fails later, after the completed wake and damage statistic,
inside Player armor calculation.

The Java-free reader and comparison controls reject 307 authored corruptions
in ordinary and optimized Python. Native execution results are recorded
separately in [testing](../testing.md). These controls are not
additional original observations.

Windows MSVC Release and Linux Clang Debug with ASan/UBSan replay all 32
profiles, 4,748 shared scalar comparisons and 537,253 fixture checks per host.
Both bounded processes exit zero with empty stderr and unchanged replay inputs.
The Linux replay initially rejects its large sanitized executable before launch
because it uses the packet reader's eight-MiB hashing admission. Build-product
hashing now streams independently; the packet bound remains unchanged and has
an explicit large-file rejection control. The fresh three-case follow-up passes.

## Limits

These are prepared CPU component calls. They execute no tick, frame, natural
night progression or original audio-device output. Hostile-attacker behavior,
Arrow resolution, broader fatal damage, default-difficulty nighttime spawning,
loaded gameplay and sustained frame times retain their separate requirements.
