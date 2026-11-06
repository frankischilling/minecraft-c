# Player sleep rejection and result objects

The client implements the rejection prefix of original `GS.b(III):CW` on the
actual LocalPlayer. The public [sleep API](../../include/mc/entity/player/sleep.h)
returns the actual nullable CW object identity. Its native status reports
execution or refusal separately from that source result. This module depends
on Entity and World owners and introduces no GUI dependency.

GS first reads the Player's actual World remote field. For a local World it
calls the real sleeping predicate at 11, then the inherited alive predicate
at 18. Sleeping, removed or nonpositive-health Players return `CW.e`. It
freshly reads the Player's World and provider for the provider's `c` field at
35; true returns `CW.b`. A further fresh World read checks the source `f < 4`
daytime field and returns `CW.c`.

At night the source checks X, Y and Z distances in order against 3, 2 and 3.
A rejection returns `CW.d`. The comparisons retain Java's `dcmpl` behavior:
a NaN distance continues to the next axis, while a later finite out-of-range
axis can still reject. Remote Worlds bypass these local checks. A route that
would proceed into sleeping stops immediately before resize at bytecode 119.
It performs no sleep, pose, timer or saved-spawn stores and does not initialize
CW merely because it reached that frontier.

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
Any later original completion is retained separately and receives no native
sleep-success credit. Native replay passes all 48 prefix projections on
Windows MSVC Release and sanitized Linux. Authored controls additionally
exercise each CW allocation cut, class failure, replaced static cells and
owner lifetime.

```sh
ctest --test-dir build --output-on-failure -R '^(entity[.]player_sleep|tools[.]player_sleep_(packet(_optimized)?|native_(normal|optimized)))$'
```

[Daytime Bed activation](../world/bed-interaction.md) calls this prefix and
routes the reached message through the actual client. Successful sleep,
waking, tick/camera changes, saved spawn from sleeping, hostile-Mob checks
and full-night gameplay remain unfinished.
