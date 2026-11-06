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
