# Player sleep, wake and result objects

The client implements original `GS.b(III):CW` sleep entry and `GS.a(ZZZ):void`
wake on the actual LocalPlayer. The [retained interface](../../include/mc/entity/player/sleep.h)
owns reached children until checked retirement. Native status reports execution
or refusal separately from the actual nullable CW result. The older finite
prefix interface remains available for its unchanged 48-profile comparison.
Both entries share that prefix implementation. Neither supplies a GUI.

GS first reads the Player's actual World remote field. For a local World it
calls the real sleeping predicate at 11, then the inherited alive predicate
at 18. Sleeping, removed or nonpositive-health Players return `CW.e`. It
freshly reads the Player's World and provider for the provider's `c` field at
35; true returns `CW.b`. A further fresh World read checks the source `f < 4`
daytime field and returns `CW.c`.

At night the source checks X, Y and Z distances in order against 3, 2 and 3.
A rejection returns `CW.d`. The comparisons retain Java's `dcmpl` behavior:
a NaN distance continues to the next axis, while a later finite out-of-range
axis can still reject. Remote Worlds bypass these local checks. The finite
prefix interface stops immediately before resize119 without sleep, pose, timer
or saved-spawn stores. It does not initialize CW at that frontier. The retained
entry continues through the actual body below.

## Retained entry and wake

`mc_player_try_sleep_begin` first executes that shared prefix. A continuation
calls inherited LS size wrapper119 and the actual SN two-store size body,
then changes the offset. Size alone does not rebuild bounds. The current
World's presence query135 selects loaded metadata148 or the unavailable-Chunk
position fallback275. Loaded air with orientation metadata follows the same
route: GS adds no Bed-ID test. The actual GS defining Registry is supplied
explicitly and first used at VE155; it is independent of the Player's World
and the activating Block's Registry.

The body preserves source binary32 position arguments, the same permanent
bounds receiver, four facing offsets and the reached null-bounds failure after
XYZ stores. It then sets sleeping, resets the timer, publishes a fresh bed
coordinate, zeros motion in Y/Z/X order, invokes local World.y and reads CW.a
late. Sleep does not set the saved spawn.

`mc_player_wake_begin` has no sleeping-entry guard. It restores standing size
and offset before reading the two coordinate locals. A matching Bed reaches
the shared notifying occupancy clear and existing ordered exit search. A
blocked search allocates the original coordinate fallback. A missing Bed or
null coordinate skips reposition and preserves the previous small bounds.
The body sets sleeping=false before its fresh World.B read, optionally invokes
World.y, stores timer0 or100, and optionally copies the late current bed
coordinate into a distinct saved-spawn object. It does not clear the bed field.

An allocated call remains reachable after a source failure, including when the
optional error output is null. Error copies borrow typed child views; they do
not own the call. Exact World occurrences, defining Registry, coordinate locals,
query producers, occupancy and exit-search work stay live through checked
retirement. Refused cleanup preserves remaining ownership and the first source
failure. Consumed child views are cleared before another cleanup can refuse.
No source retry, rollback or Player-global retry latch is used.

## World and Display callers

The [World tick](../../src/world/runtime/world_tick.c) sleeping predicate uses
the actual J field, local flag and each Player's sleeping/timer predicate.
After the threshold, the existing default-difficulty hostile branch still
stops at CQ33/FW7. A source route that bypasses that branch aligns the next
day's time, stores J=false, captures the Player iterator once and invokes each
reached wake with flags(false,false,true). Only iterator exhaustion reaches
the four weather resets and the ordinary tick tail.

Canonical Player-list mutations maintain a wrapping Java int structural
counter. The captured iterator compares cursor with the current size in
hasNext, then checks the captured counter only in next. A changed count can
therefore end iteration without a next call; an add/remove pair restoring
the old size still reaches ConcurrentModificationException. Replacing the
World.d field does not replace the captured iterator. Six private controls
against the actual JDK17 bootstrap classes and eight Code fingerprints check
this library behavior; they are not additional original-game observations.

The [HUD](../../src/client/gui/gui_ingame_draw.c) reads the timer twice at
the original sites. A positive timer disables depth and alpha tests, computes
the original binary32 fade, draws the shared filled rectangle, and reenables
alpha then depth. The source integer subtraction, float-to-int conversion
and alpha shift remain unchanged even for authored out-of-range timers.
The [Player renderer](../../src/client/render/living_draw/player.c) reads the
actual bed-coordinate metadata through GS.M and applies Y orientation, Z90,
then Y270. Query producers remain owned by its genuine Manager call.

Eight authored World caller controls pass on Windows and sanitized Linux,
with 15,054 native checks per host. The complete stored-air routes return at
198 after 361 genuine Chunk acquisitions and all 28,880 random samples. Other
cases preserve source failures, iterator effects and checked cleanup refusals.
The final World selections take 4.30 Windows CTest seconds and 18.91 Linux
seconds. Actual range counts are recorded and may be zero; this fixture
supplies no positive range-callback coverage or elapsed-sleep qualification.

Native Display controls also pass on both hosts: eleven timer inputs check
fade colors and raw pixels, and four actual sleep entries feed fresh protected
DS calls whose three returned Display rotations match the cardinal matrices.
These controls do not compare original GL output or qualify loaded-night
gameplay. Local B=false
does not allocate the remote GH sleeping screen. No replacement screen or
sleep-completion callback is supplied.

## Lazy CW namespace

[sleep_result.c](../../src/entity/player/sleep_result.c) owns the actual five
enum objects, their immutable names and ordinals, static cells and values
array in the defining Entity class namespace. A reached getstatic initializes
them in source allocation/publication order. Partial publication and a failed
initializer stay latched; increasing a native allocation budget does not retry
the original initializer. Passive inspection does not initialize the class.

Borrowed object, name and array views require their exact defining namespace
to remain live. A changed or null static cell remains observable. The Bed
activation caller compares results against its own code namespace's CW cells;
the Player's namespace is independent. The World implementation acquires that
second namespace only when its actual class/cell operation is reached.

## Evidence and limits

The [original packet](../../tests/reference/entity/player_sleep/README.md)
retains 48 prepared profiles: 31 rejection returns, four null-receiver
exceptions and 13 observations stopped before 119. Two fresh original VMs
agree, including selected reads, CW publications and reached Code hashes.
Its later original completions remain separate and receive no qualification
from the new packet. Native replay passes all 48 prefix projections on
Windows MSVC Release and sanitized Linux. Authored controls additionally
exercise each CW allocation cut, class failure, replaced static cells and
owner lifetime.

The separate [76-profile packet](../../tests/reference/entity/player_sleep_wake/README.md)
uses two fresh original VMs with genuinely published Players and normal Chunks.
Each observes 16 successful sleeps, 56 successful wakes and four actual null
failures; 1,313 selected invocations and 3,979 ordered events are retained per
VM. It covers four directions, all eight wake flag combinations, an awake
second Player, declared remote state, loaded air metadata, absent Beds,
null coordinates, blocked exits and the null World/bounds partial prefixes.
Twenty-five selected state fields include complete five-plane Chunk hashes,
identity relations and distinct saved-coordinate copies. Random and unselected
state preservation are checked within each call; constructor entropy is not
compared across VMs. This packet does not execute ticks or graphics.

Complete-body native replay passes all 76 profiles and 9,784 scalar comparisons
on Windows MSVC Release and Clang Debug Linux with AddressSanitizer,
UndefinedBehaviorSanitizer and leak detection. The corrected four-case follow-up
takes 4.82 Windows CTest seconds and 26.31 Linux seconds for this replay, with
1,292,724 Windows native checks. All 5,498 public inputs remain unchanged.
It compares the four actual fault leaves but does not claim equality of a
native execution journal with the original JDI journal.

The first Windows cohort fails this fixture before comparison: aggregate
memcmp reads native structure padding, and direct initializer output can
interrupt a buffered JSON row. Explicit scalar/reference comparisons retain
all 63 Base, 53 Living, 32 Player, 43 World and eight Provider fields, with
raw-bit floating comparisons. Each completed output row is flushed. Neither
the original packet nor expected values changes. The original failed capture
remains retained, separately from the successful replay.

```sh
ctest --test-dir build --output-on-failure -R '^(entity[.]player_sleep|tools[.]player_sleep_(packet(_optimized)?|native_(normal|optimized)))$'
ctest --test-dir build --output-on-failure -R '^(entity[.]player_sleep_wake|world[.](bed_state|sleep_tick)|tools[.]player_sleep_wake_(packet|controls)_(normal|optimized))$'
```

[Bed activation](../world/bed-interaction.md) now owns the retained sleep child
and compares its actual CW identity before setting occupancy. Daytime still
routes its message through the actual client. Full-night gameplay, damage/NBT
parent wake dispatch, default-hostile CQ/FW, remote sleep-screen behavior,
original pixels/audio and sustained FPS require separate qualification.
